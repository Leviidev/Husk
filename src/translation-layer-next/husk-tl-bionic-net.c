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
#include <sys/stat.h>
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
        const char *gp = (const char *)p + 2;
        char path[1100];
        bool abstract = n > 0 && gp[0] == 0;
        if (abstract) { size_t m = n - 1 < sizeof(path) - 1 ? n - 1 : sizeof(path) - 1; memcpy(path, gp + 1, m); path[m] = 0; }
        else { size_t m = strnlen(gp, n); char raw[1100]; if (m > sizeof(raw) - 1) m = sizeof(raw) - 1; memcpy(raw, gp, m); raw[m] = 0;
               const char *tl_path_resolve(const char *path, char *buf, size_t n); char rb[1100];
               snprintf(path, sizeof(path), "%s", m ? tl_path_resolve(raw, rb, sizeof(rb)) : ""); }
        /* Linux's abstract names, and paths longer than Darwin's 104 bytes (an app's files under an iOS container path), as a short
         * name in the temporary directory made from a hash of the address: a bind and a connect to the same address meet there */
        if (abstract || strlen(path) > sizeof(s->sun_path) - 1) {
            uint64_t h = 1469598103934665603ull;
            for (const char *q = path; *q; q++) h = (h ^ (uint8_t)*q) * 1099511628211ull;
            const char *tmp = getenv("TMPDIR"); if (!tmp || !*tmp || strlen(tmp) > sizeof(s->sun_path) - 22) tmp = "/tmp";
            char dir[600]; snprintf(dir, sizeof(dir), "%s%ss", tmp, tmp[strlen(tmp) - 1] == '/' ? "" : "/");
            mkdir(dir, 0700);
            snprintf(path, sizeof(path), "%s/%c%016llx", dir, abstract ? 'a' : 'p', (unsigned long long)h);
        }
        n = strlen(path);
        if (n > sizeof(s->sun_path) - 1) n = sizeof(s->sun_path) - 1;
        memcpy(s->sun_path, path, n); s->sun_path[n] = 0;
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
    /* a short name made for an address (see sa_to_darwin) outlives the app's own unlink of the long path: a socket left there by an
       earlier run goes first, as the app's unlink would have taken it */
    if (s.ss_family == AF_UNIX) {
        const char *sp = ((struct sockaddr_un *)&s)->sun_path, *base = strrchr(sp, '/');
        struct stat st;
        if (base && (base[1] == 'a' || base[1] == 'p') && strlen(base + 2) == 16 && base - sp >= 2 && !strncmp(base - 2, "/s", 2) && stat(sp, &st) == 0 && S_ISSOCK(st.st_mode)) unlink(sp);
    }
    TL_ERRNO_BEGIN(); int r = bind(fd, (struct sockaddr *)&s, n); int e = errno; TL_ERRNO_END();
    NTRACE("bind(fd %d, family %d%s%s) -> %d errno %d", fd, s.ss_family, s.ss_family == AF_UNIX ? " " : "", s.ss_family == AF_UNIX ? ((struct sockaddr_un *)&s)->sun_path : "", r, r < 0 ? e : 0);
    return r;
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
static long b_sendto(int fd, const void *p, size_t n, int flags, const void *g, socklen_t glen);
static long b___sendto_chk(int fd, const void *p, size_t n, size_t bl, int flags, const void *g, socklen_t glen) { (void)bl; return b_sendto(fd, p, n, flags, g, glen); }
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
/* sendmmsg / recvmmsg: Darwin has neither; one message at a time (QUIC and DNS libraries batch with them) */
typedef struct { guest_msghdr hdr; unsigned int len; } guest_mmsghdr;
static int b_sendmmsg(int fd, guest_mmsghdr *v, unsigned int n, int flags)
{
    unsigned int i = 0;
    for (; i < n; i++) { long r = b_sendmsg(fd, &v[i].hdr, flags); if (r < 0) return i ? (int)i : -1; v[i].len = (unsigned int)r; }
    return (int)i;
}
static int b_recvmmsg(int fd, guest_mmsghdr *v, unsigned int n, int flags, void *timeout)
{
    (void)timeout;
    unsigned int i = 0;
    for (; i < n; i++) {
        long r = b_recvmsg(fd, &v[i].hdr, i ? flags | 0x40 /* MSG_DONTWAIT */ : flags);
        if (r < 0) return i ? (int)i : -1;
        v[i].len = (unsigned int)r;
    }
    return (int)i;
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
    /* multicast options whose Linux structs Darwin lays out differently (Java's MulticastSocket, UPnP/SSDP: yaacc) */
    if ((level == 0 || level == 41) && (name == 42 || name == 45) && val && len >= 8 + 2) {
        /* MCAST_JOIN_GROUP / MCAST_LEAVE_GROUP: Linux struct group_req { u32 interface; (pad); sockaddr_storage group at 8 };
           Darwin's is packed, the group at 4, and its sockaddr has sa_len */
        uint8_t gr[4 + sizeof(struct sockaddr_storage)] = {0};
        memcpy(gr, val, 4);
        struct sockaddr_storage ss;
        socklen_t sl = sa_to_darwin((const uint8_t *)val + 8, len - 8, &ss);
        if (!sl) { tl_set_guest_errno(22 /* EINVAL */); return -1; }
        memcpy(gr + 4, &ss, sizeof(ss));
        TL_ERRNO_BEGIN(); int r = setsockopt(fd, level == 0 ? IPPROTO_IP : IPPROTO_IPV6, name == 42 ? 80 /* MCAST_JOIN_GROUP */ : 81, gr, sizeof(gr)); TL_ERRNO_END();
        return r;
    }
    if (level == 0 && name == 32 && val && len >= 12) {
        /* IP_MULTICAST_IF with Linux's struct ip_mreqn { multiaddr, address, int ifindex }: by index, else by address */
        int idx; memcpy(&idx, (const uint8_t *)val + 8, 4);
        struct sockaddr_storage ss; socklen_t sl = sizeof(ss);
        bool v6 = getsockname(fd, (struct sockaddr *)&ss, &sl) == 0 && ss.ss_family == AF_INET6;
        TL_ERRNO_BEGIN();
        int r = v6 ? (idx ? setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &idx, sizeof(idx)) : 0)   /* on an IPv6 socket, by index */
              : idx ? setsockopt(fd, IPPROTO_IP, 0x42 /* IP_MULTICAST_IFINDEX */, &idx, sizeof(idx))
                    : setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (const uint8_t *)val + 4, 4);
        int e = errno;
        TL_ERRNO_END();
        NTRACE("setsockopt(fd %d, IP_MULTICAST_IF index %d, %s socket) -> %d errno %d", fd, idx, v6 ? "v6" : "v4", r, r < 0 ? e : 0);
        return r;
    }
    if (level == 0 && (name == 33 || name == 34) && val && len >= 1) {
        /* IP_MULTICAST_TTL / LOOP, a byte (libcore's setsockoptByte) or an int. Java's sockets are IPv6 ones, where Linux takes the
           IPv4 options and Darwin refuses them: there they are the IPv6 options (hop limit, loop), which take an int. */
        int v = len >= 4 ? *(const int *)val : *(const uint8_t *)val;
        struct sockaddr_storage ss; socklen_t sl = sizeof(ss);
        bool v6 = getsockname(fd, (struct sockaddr *)&ss, &sl) == 0 && ss.ss_family == AF_INET6;
        TL_ERRNO_BEGIN();
        int r;
        if (v6) r = setsockopt(fd, IPPROTO_IPV6, name == 33 ? IPV6_MULTICAST_HOPS : IPV6_MULTICAST_LOOP, &v, sizeof(v));
        else { unsigned char c = (unsigned char)v; r = setsockopt(fd, IPPROTO_IP, name == 33 ? IP_MULTICAST_TTL : IP_MULTICAST_LOOP, &c, 1);
               if (r < 0) r = setsockopt(fd, IPPROTO_IP, name == 33 ? IP_MULTICAST_TTL : IP_MULTICAST_LOOP, &v, sizeof(v)); }
        int e = errno;
        TL_ERRNO_END();
        NTRACE("setsockopt(fd %d, IP multicast %d = %d, %s socket) -> %d errno %d", fd, name, v, v6 ? "v6" : "v4", r, r < 0 ? e : 0);
        return r;
    }
    if (level == 0 && (name == 35 || name == 36) && val && len >= 12) {
        /* IP_ADD/DROP_MEMBERSHIP with an ip_mreqn: Darwin takes the ip_mreq at its start */
        TL_ERRNO_BEGIN(); int r = setsockopt(fd, IPPROTO_IP, name == 35 ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, val, 8); TL_ERRNO_END();
        return r;
    }
    if (!opt_to_darwin(level, name, &dl, &dn)) return 0;            /* an option with no Darwin counterpart: accepted and ignored */
    TL_ERRNO_BEGIN(); int r = setsockopt(fd, dl, dn, val, len); int e = errno; TL_ERRNO_END();
    NTRACE("setsockopt(fd %d, level %d, opt %d, len %u) -> %d errno %d", fd, level, name, (unsigned)len, r, r < 0 ? e : 0);
    return r;
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
    NTRACE("getaddrinfo(%s, %s, family %d flags %#x socktype %d) -> %d", node ? node : "(null)", service ? service : "(null)",
           gh ? gh->ai_family : -1, gh ? gh->ai_flags : -1, gh ? gh->ai_socktype : -1, r);
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

/* bionic's own: getaddrinfo on a network (netid/mark are Android's network selection; there is one network here) */
static int b_android_getaddrinfofornet(const char *node, const char *service, const guest_addrinfo *gh, unsigned netid, unsigned mark, guest_addrinfo **res)
{
    (void)netid; (void)mark;
    return b_getaddrinfo(node, service, gh, res);
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

/* The reentrant forms (Python's socket module uses them): the lookup under a lock, its result copied into the caller's buffer.
 * 0 with *result NULL and *herr HOST_NOT_FOUND when there is no answer, ERANGE when buf is too small, as bionic does. */
static pthread_mutex_t he_lock = PTHREAD_MUTEX_INITIALIZER;
static int he_copy(const struct hostent *h, guest_hostent *out, char *buf, size_t buflen, guest_hostent **result, int *herr)
{
    size_t na = 0, nl = 0, need = strlen(h->h_name) + 1;
    while (h->h_aliases && h->h_aliases[na]) need += strlen(h->h_aliases[na++]) + 1;
    while (h->h_addr_list && h->h_addr_list[nl]) nl++;
    need += (na + 1 + nl + 1) * sizeof(char *) + nl * (size_t)h->h_length + sizeof(char *);
    if (need > buflen) { if (herr) *herr = -1; return ERANGE; }
    char *p = (char *)(((uintptr_t)buf + sizeof(char *) - 1) & ~(uintptr_t)(sizeof(char *) - 1));
    char **aliases = (char **)p; p += (na + 1) * sizeof(char *);
    char **addrs = (char **)p; p += (nl + 1) * sizeof(char *);
    for (size_t i = 0; i < nl; i++) { memcpy(p, h->h_addr_list[i], (size_t)h->h_length); addrs[i] = p; p += h->h_length; }
    addrs[nl] = NULL;
    for (size_t i = 0; i < na; i++) { size_t l = strlen(h->h_aliases[i]) + 1; memcpy(p, h->h_aliases[i], l); aliases[i] = p; p += l; }
    aliases[na] = NULL;
    size_t l = strlen(h->h_name) + 1; memcpy(p, h->h_name, l);
    out->h_name = p; out->h_aliases = aliases; out->h_addrtype = dom_from_darwin(h->h_addrtype);
    out->h_length = h->h_length; out->h_addr_list = addrs;
    *result = out;
    if (herr) *herr = 0;
    return 0;
}
static int he_finish(struct hostent *h, guest_hostent *out, char *buf, size_t buflen, guest_hostent **result, int *herr)
{
    int r = 0;
    *result = NULL;
    if (h) r = he_copy(h, out, buf, buflen, result, herr);
    else if (herr) *herr = h_errno;
    pthread_mutex_unlock(&he_lock);
    return r;
}
static int b_gethostbyname2_r(const char *name, int af, guest_hostent *out, char *buf, size_t buflen, guest_hostent **result, int *herr)
{
    int d = dom_to_darwin(af);
    if (d < 0) { *result = NULL; if (herr) *herr = 1; return 0; }
    pthread_mutex_lock(&he_lock);
    return he_finish(gethostbyname2(name, d), out, buf, buflen, result, herr);
}
static int b_gethostbyname_r(const char *name, guest_hostent *out, char *buf, size_t buflen, guest_hostent **result, int *herr)
{
    pthread_mutex_lock(&he_lock);
    return he_finish(gethostbyname(name), out, buf, buflen, result, herr);
}
static int b_gethostbyaddr_r(const void *addr, socklen_t len, int type, guest_hostent *out, char *buf, size_t buflen,
                             guest_hostent **result, int *herr)
{
    int d = dom_to_darwin(type);
    if (d < 0) { *result = NULL; if (herr) *herr = 1; return 0; }
    pthread_mutex_lock(&he_lock);
    return he_finish(gethostbyaddr(addr, len, d), out, buf, buflen, result, herr);
}

static uint16_t b_htons(uint16_t x) { return __builtin_bswap16(x); }
static uint32_t b_htonl(uint32_t x) { return __builtin_bswap32(x); }
static unsigned b_if_nametoindex(const char *n) { return if_nametoindex(n); }
static int b_inet_pton(int af, const char *src, void *dst) { int d = dom_to_darwin(af); return d < 0 ? -1 : inet_pton(d, src, dst); }
static const char *b_inet_ntop(int af, const void *src, char *dst, socklen_t size) { int d = dom_to_darwin(af); return d < 0 ? NULL : inet_ntop(d, src, dst, size); }

/*
 * getifaddrs: the host's interfaces with IPv4 or IPv6 addresses, in bionic's struct ifaddrs (the same fields, Linux sockaddrs). Each
 * entry is one allocation holding its name and addresses, so freeifaddrs frees the chain entry by entry. Flags keep their meaning;
 * IFF_MULTICAST is 0x1000 on Linux, 0x8000 here.
 */
#include <ifaddrs.h>
#include <net/if.h>
typedef struct guest_ifaddrs { struct guest_ifaddrs *next; char *name; uint32_t flags; void *addr, *netmask, *ifu, *data; } guest_ifaddrs;
static int b_getifaddrs(guest_ifaddrs **out)
{
    struct ifaddrs *list;
    if (getifaddrs(&list) != 0) { tl_set_guest_errno(tl_errno_to_guest(errno)); return -1; }
    guest_ifaddrs *head = NULL, **tail = &head;
    for (struct ifaddrs *i = list; i; i = i->ifa_next) {
        if (!i->ifa_addr || (i->ifa_addr->sa_family != AF_INET && i->ifa_addr->sa_family != AF_INET6)) continue;
        size_t nl = strlen(i->ifa_name) + 1;
        uint8_t *blk = calloc(1, sizeof(guest_ifaddrs) + 3 * 32 + nl);
        guest_ifaddrs *g = (guest_ifaddrs *)blk;
        uint8_t *sa = blk + sizeof(guest_ifaddrs);
        g->name = (char *)(sa + 96); memcpy(g->name, i->ifa_name, nl);
        uint32_t f = i->ifa_flags & 0x3FF & ~0x20u;
        if (i->ifa_flags & IFF_MULTICAST) f |= 0x1000;
        g->flags = f;
        sa_from_darwin(i->ifa_addr, sa, 32); g->addr = sa;
        if (i->ifa_netmask) { struct sockaddr_storage m; memcpy(&m, i->ifa_netmask, i->ifa_netmask->sa_len < sizeof(m) ? i->ifa_netmask->sa_len : sizeof(m));
                              m.ss_family = i->ifa_addr->sa_family; sa_from_darwin((struct sockaddr *)&m, sa + 32, 32); g->netmask = sa + 32; }
        if (i->ifa_dstaddr && (i->ifa_flags & (IFF_BROADCAST | IFF_POINTOPOINT))) { sa_from_darwin(i->ifa_dstaddr, sa + 64, 32); g->ifu = sa + 64; }
        *tail = g; tail = &g->next;
    }
    freeifaddrs(list);
    *out = head;
    return 0;
}
static void b_freeifaddrs(guest_ifaddrs *g) { while (g) { guest_ifaddrs *n = g->next; free(g); g = n; } }

/* The same 16 bytes on both sides */
static const struct in6_addr g_in6addr_any = IN6ADDR_ANY_INIT, g_in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

const tl_bionic_entry tl_tab_net[] = {
    TL_WRAP("getifaddrs", b_getifaddrs), TL_WRAP("freeifaddrs", b_freeifaddrs),
    TL_DATA("in6addr_any", &g_in6addr_any), TL_DATA("in6addr_loopback", &g_in6addr_loopback),
    TL_WRAP("socket", b_socket), TL_WRAP("socketpair", b_socketpair), TL_WRAP("bind", b_bind), TL_WRAP("connect", b_connect),
    TL_WRAP("listen", b_listen), TL_WRAP("accept", b_accept), TL_WRAP("accept4", b_accept4), TL_WRAP("send", b_send),
    TL_WRAP("recv", b_recv), TL_WRAP("sendto", b_sendto), TL_WRAP("__sendto_chk", b___sendto_chk), TL_WRAP("recvfrom", b_recvfrom), TL_WRAP("__recvfrom_chk", b___recvfrom_chk), TL_WRAP("sendmsg", b_sendmsg),
    TL_WRAP("recvmsg", b_recvmsg), TL_WRAP("sendmmsg", b_sendmmsg), TL_WRAP("recvmmsg", b_recvmmsg), TL_WRAP("shutdown", b_shutdown), TL_WRAP("getsockname", b_getsockname),
    TL_WRAP("getpeername", b_getpeername), TL_WRAP("setsockopt", b_setsockopt), TL_WRAP("getsockopt", b_getsockopt),
    TL_WRAP("getaddrinfo", b_getaddrinfo), TL_WRAP("android_getaddrinfofornet", b_android_getaddrinfofornet), TL_WRAP("freeaddrinfo", b_freeaddrinfo), TL_WRAP("gai_strerror", b_gai_strerror),
    TL_WRAP("getnameinfo", b_getnameinfo), TL_WRAP("gethostbyname", b_gethostbyname), TL_WRAP("gethostbyaddr", b_gethostbyaddr),
    TL_WRAP("gethostbyname_r", b_gethostbyname_r), TL_WRAP("gethostbyname2_r", b_gethostbyname2_r), TL_WRAP("gethostbyaddr_r", b_gethostbyaddr_r),
    TL_WRAP("if_nametoindex", b_if_nametoindex), TL_WRAP("inet_pton", b_inet_pton), TL_WRAP("inet_ntop", b_inet_ntop),
    TL_DIRECT(inet_addr), TL_DIRECT(inet_aton), TL_DIRECT(inet_ntoa), TL_WRAP("htons", b_htons), TL_WRAP("ntohs", b_htons), TL_WRAP("htonl", b_htonl), TL_WRAP("ntohl", b_htonl),
    TL_END
};
