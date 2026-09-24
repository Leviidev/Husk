/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Code an iOS host cannot run as it is: a direct Linux system call, and a
 * write to the thread register. Built with -fstack-protector-all, so every
 * function also reads its stack guard from TPIDR_EL0 the Android way. */
long raw_getpid(void)
{
    register long x8 __asm__("x8") = 172;   /* __NR_getpid */
    register long x0 __asm__("x0");
    __asm__ volatile("svc #0" : "=r"(x0) : "r"(x8) : "memory");
    return x0;
}

void set_thread_pointer(void *p)
{
    __asm__ volatile("msr tpidr_el0, %0" : : "r"(p));
}

int sum(const char *s)
{
    char buf[32];
    int n = 0;
    for (int i = 0; i < 31 && s[i]; i++) { buf[i] = s[i]; n += buf[i]; }
    return n;
}
