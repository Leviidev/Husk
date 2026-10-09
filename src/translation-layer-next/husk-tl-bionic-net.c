/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Sockets and name resolution for Android code, over Darwin's.
 *
 * Linux and Darwin agree on the shape of a socket call and on very little of its
 * contents: AF_INET6 is 10 here and 30 there, SOL_SOCKET is 1 and 0xffff, a sockaddr
 * starts with a 16-bit family here and a length byte and an 8-bit family there, and
 * every SO_*, MSG_* and AI_* flag has its own number. Each call here converts what
 * goes in and what comes out and then calls Darwin.
 *
 * File descriptors need no translation: they are Darwin's, which is what the rest of
 * the shim already hands the guest.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-bionic.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

/* --------------------------------------------------------------- constants */

enum {
    G_AF_UNIX = 1, G_AF_INET = 2, G_AF_INET6 = 10,
    G_SOCK_NONBLOCK = 0x800, G_SOCK_CLOEXEC = 0x80000,
    G_SOL_SOCKET = 1,
    G_EAFNOSUPPORT = 97, G_ENOPROTOOPT = 92,
};

static int dom_to_darwin(int d)
{
    switch (d) {
    case G_AF_UNIX: return AF_UNIX;
    case G_AF_INET: return AF_INET;
    case G_AF_INET6: return AF_INET6;
    case 0: return AF_UNSPEC;
    default: return -1;
    }
}
static int dom_from_darwin(int d) { return d == AF_UNIX ? G_AF_UNIX : d == AF_INET ? G_AF_INET : d == AF_INET6 ? G_AF_INET6 : d; }

/* ---------------------------------------------------------------- sockaddr */

/* A Linux sockaddr into a Darwin one. Returns the Darwin length, or 0 if the family is not supported. */
static socklen_t sa_to_darwin(const void *g, socklen_t glen, struct sockaddr_storage *out)
{
    memset(out, 0, sizeof(*out));
    if (!g || glen < 2) return 0;
    const uint8_t *p = g;
    uint16_t fam = (uint16_t)(p[0] | (p[1] << 8));
    if (fam == G_AF_INET && glen >= 8) {
        struct sockaddr_in *s = (struct sockaddr_in *)out;
        s->sin_len = sizeof(*s); s->sin_family = AF_INET;
        memcpy(&s->sin_port, p + 2, 2); memcpy(&s->sin_addr, p + 4, 4);
        return sizeof(*s);
    }
    if (fam == G_AF_INET6 && glen >= 24) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)out;
        s->sin6_len = sizeof(*s); s->sin6_family = AF_INET6;
        memcpy(&s->sin6_port, p + 2, 2); memcpy(&s->sin6_flowinfo, p + 4, 4);
        memcpy(&s->sin6_addr, p + 8, 16);
        if (glen >= 28) memcpy(&s->sin6_scope_id, p + 24, 4);
        return sizeof(*s);
    }
    if (fam == G_AF_UNIX) {
        struct sockaddr_un *s = (struct sockaddr_un *)out;
        size_t n = glen > 2 ? glen - 2 : 0;
        if (n > sizeof(s->sun_path) - 1) n = sizeof(s->sun_path) - 1;
        memcpy(s->sun_path, p + 2, n);
        s->sun_family = AF_UNIX;
        s->sun_len = (uint8_t)(2 + n + 1);
        return s->sun_len;
    }
    return 0;
}

/* A Darwin sockaddr into the guest's buffer; returns the Linux length it would need, and fills as much as `cap` holds. */
static socklen_t sa_from_darwin(const struct sockaddr *d, void *g, socklen_t cap)
{
    uint8_t tmp[128];
    memset(tmp, 0, sizeof(tmp));
    socklen_t need = 0;
    if (d->sa_family == AF_INET) {
        const struct sockaddr_in *s = (const struct sockaddr_in *)d;
        tmp[0] = G_AF_INET;
        memcpy(tmp + 2, &s->sin_port, 2); memcpy(tmp + 4, &s->sin_addr, 4);
        need = 16;
    } else if (d->sa_family == AF_INET6) {
        const struct sockaddr_in6 *s = (const struct sockaddr_in6 *)d;
        tmp[0] = G_AF_INET6;
        memcpy(tmp + 2, &s->sin6_port, 2); memcpy(tmp + 4, &s->sin6_flowinfo, 4);
        memcpy(tmp + 8, &s->sin6_addr, 16); memcpy(tmp + 24, &s->sin6_scope_id, 4);
        need = 28;
    } else if (d->sa_family == AF_UNIX) {
        const struct sockaddr_un *s = (const struct sockaddr_un *)d;
        tmp[0] = G_AF_UNIX;
        size_t n = strnlen(s->sun_path, sizeof(s->sun_path));
        memcpy(tmp + 2, s->sun_path, n);
        need = (socklen_t)(2 + n + 1);
    } else {
        tmp[0] = (uint8_t)dom_from_darwin(d->sa_family);
        need = 2;
    }
    if (g && cap) memcpy(g, tmp, need < cap ? need : cap);
    return need;
}

/* ------------------------------------------------------------------ flags */

static int msg_flags_to_darwin(int f)
{
    int r = 0;
    if (f & 0x1) r |= MSG_OOB;
    if (f & 0x2) r |= MSG_PEEK;
    if (f & 0x4) r |= MSG_DONTROUTE;
    if (f & 0x40) r |= MSG_DONTWAIT;
    if (f & 0x100) r |= MSG_WAITALL;
    if (f & 0x20) r |= MSG_TRUNC;
    if (f & 0x80) r |= MSG_EOR;
    return r;                                   /* MSG_NOSIGNAL (0x4000) is SO_NOSIGPIPE on the socket */
}

/* ------------------------------------------------------------- socket calls */

static int g_net_trace = -1;
#define NTRACE(...) do { if (g_net_trace < 0) g_net_trace = getenv("TL_NET_TRACE") ? 1 : 0; if (g_net_trace) tl_log_line("net: " __VA_ARGS__); } while (0)

static void quiet_sigpipe(int fd)
{
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
}

static int b_socket(int domain, int type, int protocol)
{
    NTRACE("socket(domain %d, type %#x, proto %d)", domain, type, protocol);
    int d = dom_to_darwin(domain);
    if (d < 0) { tl_set_guest_errno(G_EAFNOSUPPORT); return -1; }
    int t = type & 0xF;
    TL_ERRNO_BEGIN();
    int fd = socket(d, t, protocol);
    if (fd >= 0) {
        if (type & G_SOCK_CLOEXEC) fcntl(fd, F_SETFD, FD_CLOEXEC);
        if (type & G_SOCK_NONBLOCK) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        quiet_sigpipe(fd);
    }
    TL_ERRNO_END();
    return fd;
}

static int b_socketpair(int domain, int type, int protocol, int *sv)
{
    int d = dom_to_darwin(domain);
    if (d < 0) { tl_set_guest_errno(G_EAFNOSUPPORT); return -1; }
    TL_ERRNO_BEGIN();
    int r = socketpair(d, type & 0xF, protocol, sv);
    if (r == 0) for (int i = 0; i < 2; i++) {
        if (type & G_SOCK_CLOEXEC) fcntl(sv[i], F_SETFD, FD_CLOEXEC);
        if (type & G_SOCK_NONBLOCK) fcntl(sv[i], F_SETFL, fcntl(sv[i], F_GETFL) | O_NONBLOCK);
        quiet_sigpipe(sv[i]);
    }
    TL_ERRNO_END();
    return r;
}

static int b_bind(int fd, const void *g, socklen_t glen)
{
    struct sockaddr_storage s;
    socklen_t n = sa_to_darwin(g, glen, &s);
    if (!n) { tl_set_guest_errno(G_EAFNOSUPPORT); return -1; }
    TL_ERRNO_BEGIN(); int r = bind(fd, (struct sockaddr *)&s, n); TL_ERRNO_END(); return r;
}

static int b_connect(int fd, const void *g, socklen_t glen)
{
    struct sockaddr_storage s;
    socklen_t n = sa_to_darwin(g, glen, &s);
    if (!n) { tl_set_guest_errno(G_EAFNOSUPPORT); return -1; }
    TL_ERRNO_BEGIN(); int r = connect(fd, (struct sockaddr *)&s, n); int e = errno; TL_ERRNO_END();
    { char ip[64] = "?"; if (s.ss_family == AF_INET) inet_ntop(AF_INET, &((struct sockaddr_in *)&s)->sin_addr, ip, sizeof(ip)); NTRACE("connect(fd %d, %s) -> %d errno %d", fd, ip, r, e); }
    return r;
}

static int b_listen(int fd, int backlog) { TL_ERRNO_BEGIN(); int r = listen(fd, backlog); TL_ERRNO_END(); return r; }

static int accept_common(int fd, void *g, socklen_t *glen, int flags)
{
    struct sockaddr_storage s; socklen_t sl = sizeof(s);
    TL_ERRNO_BEGIN();
    int c = accept(fd, (struct sockaddr *)&s, &sl);
    if (c >= 0) {
        if (flags & G_SOCK_CLOEXEC) fcntl(c, F_SETFD, FD_CLOEXEC);
        if (flags & G_SOCK_NONBLOCK) fcntl(c, F_SETFL, fcntl(c, F_GETFL) | O_NONBLOCK);
        quiet_sigpipe(c);
        if (g && glen) *glen = sa_from_darwin((struct sockaddr *)&s, g, *glen);
    }
    TL_ERRNO_END();
    return c;
}
static int b_accept(int fd, void *g, socklen_t *glen) { return accept_common(fd, g, glen, 0); }
static int b_accept4(int fd, void *g, socklen_t *glen, int flags) { return accept_common(fd, g, glen, flags); }

static long b_send(int fd, const void *p, size_t n, int flags)
{
    TL_ERRNO_BEGIN(); long r = send(fd, p, n, msg_flags_to_darwin(flags)); int e = errno; TL_ERRNO_END(); NTRACE("send(fd %d, %zu) -> %ld errno %d", fd, n, r, r < 0 ? e : 0); return r;
}
static long b_recv(int fd, void *p, size_t n, int flags)
{
    TL_ERRNO_BEGIN(); long r = recv(fd, p, n, msg_flags_to_darwin(flags)); int e = errno; TL_ERRNO_END(); NTRACE("recv(fd %d, %zu) -> %ld errno %d", fd, n, r, r < 0 ? e : 0); return r;
}
static long b_sendto(int fd, const void *p, size_t n, int flags, const void *g, socklen_t glen)
{
    struct sockaddr_storage s; socklen_t sl = 0;
    if (g) { sl = sa_to_darwin(g, glen, &s); if (!sl) { tl_set_guest_errno(G_EAFNOSUPPORT); return -1; } }
    TL_ERRNO_BEGIN(); long r = sendto(fd, p, n, msg_flags_to_darwin(flags), g ? (struct sockaddr *)&s : NULL, sl); TL_ERRNO_END(); return r;
}
static long b_recvfrom(int fd, void *p, size_t n, int flags, void *g, socklen_t *glen);
/* FORTIFY's recvfrom: the same, with the size of the buffer it fills. */
static long b___recvfrom_chk(int fd, void *p, size_t n, size_t buf_size, int flags, void *g, socklen_t *glen) { (void)buf_size; return b_recvfrom(fd, p, n, flags, g, glen); }
static long b_recvfrom(int fd, void *p, size_t n, int flags, void *g, socklen_t *glen)
{
    struct sockaddr_storage s; socklen_t sl = sizeof(s);
    TL_ERRNO_BEGIN();
    long r = recvfrom(fd, p, n, msg_flags_to_darwin(flags), g ? (struct sockaddr *)&s : NULL, g ? &sl : NULL);
    if (r >= 0 && g && glen) *glen = sa_from_darwin((struct sockaddr *)&s, g, *glen);
    int e = errno;
    TL_ERRNO_END();
    NTRACE("recvfrom(fd %d, %zu) -> %ld errno %d", fd, n, r, r < 0 ? e : 0);
    return r;
}

/* bionic's msghdr: 8-byte iovlen and controllen, and a pad after the 4-byte namelen. */
typedef struct { void *name; uint32_t namelen; struct iovec *iov; size_t iovlen; void *control; size_t controllen; int flags; } guest_msghdr;

static long b_sendmsg(int fd, const guest_msghdr *g, int flags)
{
    struct msghdr m; memset(&m, 0, sizeof(m));
    struct sockaddr_storage s;
    if (g->name) {
        m.msg_namelen = sa_to_darwin(g->name, g->namelen, &s);
        if (!m.msg_namelen) { tl_set_guest_errno(G_EAFNOSUPPORT); return -1; }
        m.msg_name = &s;
    }
    m.msg_iov = g->iov; m.msg_iovlen = (int)g->iovlen;
    TL_ERRNO_BEGIN(); long r = sendmsg(fd, &m, msg_flags_to_darwin(flags)); TL_ERRNO_END(); return r;
}
static long b_recvmsg(int fd, guest_msghdr *g, int flags)
{
    struct msghdr m; memset(&m, 0, sizeof(m));
    struct sockaddr_storage s;
    if (g->name) { m.msg_name = &s; m.msg_namelen = sizeof(s); }
    m.msg_iov = g->iov; m.msg_iovlen = (int)g->iovlen;
    TL_ERRNO_BEGIN();
    long r = recvmsg(fd, &m, msg_flags_to_darwin(flags));
    if (r >= 0) {
        if (g->name) g->namelen = sa_from_darwin((struct sockaddr *)&s, g->name, g->namelen);
        g->controllen = 0;
        g->flags = 0;
    }
    TL_ERRNO_END();
    return r;
}

static int b_shutdown(int fd, int how) { TL_ERRNO_BEGIN(); int r = shutdown(fd, how); TL_ERRNO_END(); return r; }

static int b_getsockname(int fd, void *g, socklen_t *glen)
{
    struct sockaddr_storage s; socklen_t sl = sizeof(s);
    TL_ERRNO_BEGIN();
    int r = getsockname(fd, (struct sockaddr *)&s, &sl);
    if (r == 0) *glen = sa_from_darwin((struct sockaddr *)&s, g, *glen);
    TL_ERRNO_END();
    return r;
}
static int b_getpeername(int fd, void *g, socklen_t *glen)
{
    struct sockaddr_storage s; socklen_t sl = sizeof(s);
    TL_ERRNO_BEGIN();
    int r = getpeername(fd, (struct sockaddr *)&s, &sl);
    int e = errno;
    if (r == 0) *glen = sa_from_darwin((struct sockaddr *)&s, g, *glen);
    TL_ERRNO_END();
    NTRACE("getpeername(fd %d) -> %d errno %d", fd, r, r < 0 ? e : 0);
    return r;
}

/* --------------------------------------------------------------- sockopts */

/* Linux (level, name) to Darwin's; false for an option Darwin has no counterpart for. */
static bool opt_to_darwin(int level, int name, int *dlevel, int *dname)
{
    static const struct { int g, d; } sol[] = {
        { 1, SO_DEBUG }, { 2, SO_REUSEADDR }, { 3, SO_TYPE }, { 4, SO_ERROR }, { 5, SO_DONTROUTE }, { 6, SO_BROADCAST },
        { 7, SO_SNDBUF }, { 8, SO_RCVBUF }, { 9, SO_KEEPALIVE }, { 10, SO_OOBINLINE }, { 13, SO_LINGER }, { 15, SO_REUSEPORT },
        { 20, SO_RCVTIMEO }, { 21, SO_SNDTIMEO }, { 30, SO_ACCEPTCONN },
    };
    static const struct { int g, d; } tcp[] = { { 1, TCP_NODELAY }, { 2, TCP_MAXSEG }, { 4, 0x10 /* TCP_KEEPALIVE */ }, { 5, 0x101 /* TCP_KEEPINTVL */ }, { 6, 0x102 /* TCP_KEEPCNT */ } };
    static const struct { int g, d; } ip[] = { { 1, IP_TOS }, { 2, IP_TTL }, { 32, IP_MULTICAST_IF }, { 33, IP_MULTICAST_TTL }, { 34, IP_MULTICAST_LOOP },
                                               { 35, IP_ADD_MEMBERSHIP }, { 36, IP_DROP_MEMBERSHIP } };
    static const struct { int g, d; } ip6[] = { { 26, IPV6_V6ONLY }, { 16, IPV6_UNICAST_HOPS }, { 17, IPV6_MULTICAST_IF }, { 18, IPV6_MULTICAST_HOPS },
                                                { 19, IPV6_MULTICAST_LOOP }, { 20, IPV6_JOIN_GROUP }, { 21, IPV6_LEAVE_GROUP } };
#define LOOK(tab, lvl) do { for (size_t i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) if (tab[i].g == name) { *dlevel = (lvl); *dname = tab[i].d; return true; } return false; } while (0)
    switch (level) {
    case G_SOL_SOCKET: LOOK(sol, SOL_SOCKET);
    case 6:  LOOK(tcp, IPPROTO_TCP);
    case 0:  LOOK(ip, IPPROTO_IP);
    case 41: LOOK(ip6, IPPROTO_IPV6);
    default: return false;
    }
#undef LOOK
}

static int b_setsockopt(int fd, int level, int name, const void *val, socklen_t len)
{
    int dl, dn;
    if (!opt_to_darwin(level, name, &dl, &dn)) return 0;            /* an option with no Darwin counterpart: accepted and ignored */
    TL_ERRNO_BEGIN(); int r = setsockopt(fd, dl, dn, val, len); TL_ERRNO_END(); return r;
}

static int b_getsockopt(int fd, int level, int name, void *val, socklen_t *len)
{
    int dl, dn;
    if (!opt_to_darwin(level, name, &dl, &dn)) { tl_set_guest_errno(G_ENOPROTOOPT); return -1; }
    TL_ERRNO_BEGIN();
    int r = getsockopt(fd, dl, dn, val, len);
    int e = errno;
    if (r == 0 && level == G_SOL_SOCKET && name == 4 && *len >= sizeof(int)) *(int *)val = tl_errno_to_guest(*(int *)val);   /* SO_ERROR holds an errno */
    TL_ERRNO_END();
    NTRACE("getsockopt(fd %d, level %d, opt %d) -> %d errno %d value %d", fd, level, name, r, r < 0 ? e : 0, r == 0 && val && *len >= sizeof(int) ? *(int *)val : -1);
    return r;
}

/* ------------------------------------------------------------ name lookup */

typedef struct guest_addrinfo {
    int ai_flags, ai_family, ai_socktype, ai_protocol;
    uint32_t ai_addrlen;
    char *ai_canonname;
    void *ai_addr;
    struct guest_addrinfo *ai_next;
} guest_addrinfo;

static int ai_flags_to_darwin(int f)
{
    int r = 0;
    if (f & 0x1) r |= AI_PASSIVE;
    if (f & 0x2) r |= AI_CANONNAME;
    if (f & 0x4) r |= AI_NUMERICHOST;
    if (f & 0x8) r |= AI_V4MAPPED;
    if (f & 0x10) r |= AI_ALL;
    if (f & 0x20) r |= AI_ADDRCONFIG;
    if (f & 0x400) r |= AI_NUMERICSERV;
    return r;
}

static int b_getaddrinfo(const char *node, const char *service, const guest_addrinfo *gh, guest_addrinfo **res)
{
    struct addrinfo hints, *dres = NULL, *hp = NULL;
    if (gh) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = ai_flags_to_darwin(gh->ai_flags);
        int fam = dom_to_darwin(gh->ai_family);
        if (fam < 0) return 5;                                      /* EAI_FAMILY */
        hints.ai_family = fam;
        hints.ai_socktype = gh->ai_socktype & 0xF;
        hints.ai_protocol = gh->ai_protocol;
        hp = &hints;
    }
    int r = getaddrinfo(node, service, hp, &dres);
    NTRACE("getaddrinfo(%s, %s) -> %d", node ? node : "(null)", service ? service : "(null)", r);
    if (r) return r;                                                /* EAI_* values agree with bionic's */
    guest_addrinfo *head = NULL, **tail = &head;
    for (struct addrinfo *a = dres; a; a = a->ai_next) {
        if (a->ai_family != AF_INET && a->ai_family != AF_INET6 && a->ai_family != AF_UNIX) continue;
        size_t canon = a->ai_canonname ? strlen(a->ai_canonname) + 1 : 0;
        guest_addrinfo *g = calloc(1, sizeof(*g) + 128 + canon);
        g->ai_flags = 0; g->ai_family = dom_from_darwin(a->ai_family); g->ai_socktype = a->ai_socktype; g->ai_protocol = a->ai_protocol;
        g->ai_addr = (uint8_t *)(g + 1);
        g->ai_addrlen = sa_from_darwin(a->ai_addr, g->ai_addr, 128);
        if (canon) { g->ai_canonname = (char *)(g + 1) + 128; memcpy(g->ai_canonname, a->ai_canonname, canon); }
        *tail = g; tail = &g->ai_next;
    }
    freeaddrinfo(dres);
    if (!head) return 8;                                            /* EAI_NONAME */
    *res = head;
    return 0;
}

static void b_freeaddrinfo(guest_addrinfo *a)
{
    while (a) { guest_addrinfo *n = a->ai_next; free(a); a = n; }
}

static const char *b_gai_strerror(int e) { return gai_strerror(e); }

static int b_getnameinfo(const void *g, socklen_t glen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags)
{
    struct sockaddr_storage s;
    socklen_t n = sa_to_darwin(g, glen, &s);
    if (!n) return 5;
    int f = 0;
    if (flags & 1) f |= NI_NUMERICHOST;
    if (flags & 2) f |= NI_NUMERICSERV;
    if (flags & 4) f |= NI_NOFQDN;
    if (flags & 8) f |= NI_NAMEREQD;
    if (flags & 16) f |= NI_DGRAM;
    return getnameinfo((struct sockaddr *)&s, n, host, hostlen, serv, servlen, f);
}

/* hostent: the same layout; only the address family number differs. */
typedef struct { char *h_name; char **h_aliases; int h_addrtype; int h_length; char **h_addr_list; } guest_hostent;
static __thread guest_hostent t_he;
static void *b_gethostbyname(const char *name)
{
    struct hostent *h = gethostbyname(name);
    if (!h) return NULL;
    t_he.h_name = h->h_name; t_he.h_aliases = h->h_aliases; t_he.h_addrtype = dom_from_darwin(h->h_addrtype);
    t_he.h_length = h->h_length; t_he.h_addr_list = h->h_addr_list;
    return &t_he;
}
static void *b_gethostbyaddr(const void *addr, socklen_t len, int type)
{
    int d = dom_to_darwin(type);
    if (d < 0) return NULL;
    struct hostent *h = gethostbyaddr(addr, len, d);
    if (!h) return NULL;
    t_he.h_name = h->h_name; t_he.h_aliases = h->h_aliases; t_he.h_addrtype = dom_from_darwin(h->h_addrtype);
    t_he.h_length = h->h_length; t_he.h_addr_list = h->h_addr_list;
    return &t_he;
}

static uint16_t b_htons(uint16_t x) { return __builtin_bswap16(x); }
static uint32_t b_htonl(uint32_t x) { return __builtin_bswap32(x); }
static unsigned b_if_nametoindex(const char *n) { return if_nametoindex(n); }
static int b_inet_pton(int af, const char *src, void *dst) { int d = dom_to_darwin(af); return d < 0 ? -1 : inet_pton(d, src, dst); }
static const char *b_inet_ntop(int af, const void *src, char *dst, socklen_t size) { int d = dom_to_darwin(af); return d < 0 ? NULL : inet_ntop(d, src, dst, size); }

const tl_bionic_entry tl_tab_net[] = {
    TL_WRAP("socket", b_socket), TL_WRAP("socketpair", b_socketpair), TL_WRAP("bind", b_bind), TL_WRAP("connect", b_connect),
    TL_WRAP("listen", b_listen), TL_WRAP("accept", b_accept), TL_WRAP("accept4", b_accept4), TL_WRAP("send", b_send),
    TL_WRAP("recv", b_recv), TL_WRAP("sendto", b_sendto), TL_WRAP("recvfrom", b_recvfrom), TL_WRAP("__recvfrom_chk", b___recvfrom_chk), TL_WRAP("sendmsg", b_sendmsg),
    TL_WRAP("recvmsg", b_recvmsg), TL_WRAP("shutdown", b_shutdown), TL_WRAP("getsockname", b_getsockname),
    TL_WRAP("getpeername", b_getpeername), TL_WRAP("setsockopt", b_setsockopt), TL_WRAP("getsockopt", b_getsockopt),
    TL_WRAP("getaddrinfo", b_getaddrinfo), TL_WRAP("freeaddrinfo", b_freeaddrinfo), TL_WRAP("gai_strerror", b_gai_strerror),
    TL_WRAP("getnameinfo", b_getnameinfo), TL_WRAP("gethostbyname", b_gethostbyname), TL_WRAP("gethostbyaddr", b_gethostbyaddr),
    TL_WRAP("if_nametoindex", b_if_nametoindex), TL_WRAP("inet_pton", b_inet_pton), TL_WRAP("inet_ntop", b_inet_ntop),
    TL_DIRECT(inet_addr), TL_DIRECT(inet_aton), TL_DIRECT(inet_ntoa), TL_WRAP("htons", b_htons), TL_WRAP("ntohs", b_htons), TL_WRAP("htonl", b_htonl), TL_WRAP("ntohl", b_htonl),
    TL_END
};
