/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_QOS_H
#define HUSK_TL_QOS_H

#include <stdbool.h>
#include <stddef.h>
#if defined(__APPLE__)
#include <pthread/qos.h>
#endif

static inline int tl_qos_tolower_ascii(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

static inline bool tl_qos_has_substr_ci(const char *hay, const char *needle)
{
    if (!hay || !needle || !*needle) return false;
    for (; *hay; hay++) {
        size_t i = 0;
        while (needle[i] && hay[i] && tl_qos_tolower_ascii((unsigned char)hay[i]) == (unsigned char)needle[i]) i++;
        if (!needle[i]) return true;
    }
    return false;
}

static inline void tl_qos_set_interactive(void)
{
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

static inline void tl_qos_set_initiated(void)
{
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
#endif
}

static inline void tl_qos_set_utility(void)
{
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif
}

/* Map Android/Linux nice priority (-20..19) onto Apple QoS so render/game threads stay on P-cores. */
static inline void tl_qos_apply_nice(int nice_prio)
{
#if defined(__APPLE__)
    if (nice_prio <= -4) pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    else if (nice_prio <= 6) pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
    else pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#else
    (void)nice_prio;
#endif
}

/* Map Java Thread priority (1..10) onto Apple QoS. */
static inline void tl_qos_apply_java_priority(int prio)
{
#if defined(__APPLE__)
    if (prio >= 7) pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    else if (prio >= 4) pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
    else pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#else
    (void)prio;
#endif
}

/* Promote game/render/audio loops by thread name while demoting known async background pools. */
static inline void tl_qos_apply_name(const char *name)
{
#if defined(__APPLE__)
    if (!name || !*name) return;
    if (tl_qos_has_substr_ci(name, "render") || tl_qos_has_substr_ci(name, "sdl")
        || tl_qos_has_substr_ci(name, "unity") || tl_qos_has_substr_ci(name, "game")
        || tl_qos_has_substr_ci(name, "main") || tl_qos_has_substr_ci(name, "rhi")
        || tl_qos_has_substr_ci(name, "audio") || tl_qos_has_substr_ci(name, "mixer")
        || tl_qos_has_substr_ci(name, "glthread") || (name[0] == 'G' && name[1] == 'L')) {
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    } else if (tl_qos_has_substr_ci(name, "async") || tl_qos_has_substr_ci(name, "worker")
               || tl_qos_has_substr_ci(name, "pool") || tl_qos_has_substr_ci(name, "http")
               || tl_qos_has_substr_ci(name, "download") || tl_qos_has_substr_ci(name, "log")
               || tl_qos_has_substr_ci(name, "background") || tl_qos_has_substr_ci(name, "finalizer")
               || tl_qos_has_substr_ci(name, "daemon") || tl_qos_has_substr_ci(name, "gc")) {
        pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
    }
#else
    (void)name;
#endif
}

#endif /* HUSK_TL_QOS_H */
