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

enum {
    TL_QOS_ROLE_DEFAULT     = 0,
    TL_QOS_ROLE_INTERACTIVE = 1,
    TL_QOS_ROLE_UTILITY     = 2,
};

/* Classify thread names so game/render/gfx/audio worker pools stay on P-cores while
 * async background pools and thermal/profiler/battery samplers run on efficiency cores. */
static inline int tl_qos_role_for_name(const char *name)
{
    if (!name || !*name) return TL_QOS_ROLE_DEFAULT;
    static const char *const telemetry_or_bg_tokens[] = {
        "telemetry", "metrics", "thermal", "cpuspeed", "energystat", "sampler",
        "hitch", "battery", "http", "download", "log", "background",
        "finalizer", "daemon", "gc",
    };
    for (size_t i = 0; i < sizeof(telemetry_or_bg_tokens) / sizeof(telemetry_or_bg_tokens[0]); i++) {
        if (tl_qos_has_substr_ci(name, telemetry_or_bg_tokens[i])) return TL_QOS_ROLE_UTILITY;
    }
    static const char *const interactive_tokens[] = {
        "render", "sdl", "unity", "game", "main", "rhi",
        "bgfx", "audio", "mixer", "sound", "glthread", "gfx",
        "jobpool", "taskthread", "aaudio", "opensl",
    };
    for (size_t i = 0; i < sizeof(interactive_tokens) / sizeof(interactive_tokens[0]); i++) {
        if (tl_qos_has_substr_ci(name, interactive_tokens[i])) return TL_QOS_ROLE_INTERACTIVE;
    }
    if (name[0] == 'G' && name[1] == 'L') return TL_QOS_ROLE_INTERACTIVE;
    static const char *const generic_utility_tokens[] = {
        "async", "worker", "pool",
    };
    for (size_t i = 0; i < sizeof(generic_utility_tokens) / sizeof(generic_utility_tokens[0]); i++) {
        if (tl_qos_has_substr_ci(name, generic_utility_tokens[i])) return TL_QOS_ROLE_UTILITY;
    }
    return TL_QOS_ROLE_DEFAULT;
}

/* Promote game/render/audio loops by thread name while demoting known async background pools first. */
static inline void tl_qos_apply_name(const char *name)
{
#if defined(__APPLE__)
    int role = tl_qos_role_for_name(name);
    if (role == TL_QOS_ROLE_UTILITY) pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
    else if (role == TL_QOS_ROLE_INTERACTIVE) pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#else
    (void)name;
#endif
}

#endif /* HUSK_TL_QOS_H */
