/* Linux only: on a crash, print the call stack to stderr before dying, so a
 * crash in a recorded session (dw1-run/grabar.sh keeps the log) can be located
 * even when the replay does not reproduce it. Frames print as dw1(+0xOFFSET);
 * resolve them with `addr2line -f -e build/linux32/dw1 0xOFFSET`.
 *
 * Division by zero: DW1 was built without zero-division checks and the PS1's
 * R3000 does not trap; it returns quotient -1 (1 for a negative dividend; all
 * ones unsigned) and the dividend as remainder, and the game relies on it in a
 * few places (tickHealingParticles: 512 / (i * 13 / 4) with i = 0). On i386 the
 * SIGFPE handler gives that result for 32-bit DIV/IDIV, logs the site once and
 * resumes, so such a site does not stop a test run. The Vita's ARM returns 0
 * without trapping, so logged sites still want a fix in the source. */
#define _GNU_SOURCE
/* not dw1_native.h: psyz.h renames write() */
void dw1_crash_handler_install(void);

#if defined(__linux__) && !defined(__vita__)
#include <execinfo.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

static void say(const char *s)
{
    if (write(STDERR_FILENO, s, strlen(s)) < 0) {
        /* nothing more to do */
    }
}

static void on_crash(int sig)
{
    void *frames[64];
    int n;

    say("dw1: crashed, call stack:\n");
    n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    signal(sig, SIG_DFL);
    raise(sig);
}

#if defined(__i386__)
/* Emulate the R3000 for a faulting 32-bit DIV/IDIV (F7 /6, F7 /7). Returns 0
 * when the instruction is something else, so the caller treats it as a crash. */
static int emulate_mips_div(ucontext_t *uc)
{
    static uintptr_t logged[64];
    static int nlogged;
    greg_t *r = uc->uc_mcontext.gregs;
    const uint8_t *p = (const uint8_t *)r[REG_EIP];
    static const int reg_index[8] = { REG_EAX, REG_ECX, REG_EDX, REG_EBX,
                                      REG_ESP, REG_EBP, REG_ESI, REG_EDI };
    uint8_t modrm;
    int mod, op, rm, i;
    int32_t a = (int32_t)r[REG_EAX];
    int have_divisor = 0;
    int32_t divisor = 0;

    while (*p == 0x2E || *p == 0x3E || *p == 0x26 || *p == 0x36 || *p == 0x64 || *p == 0x65)
        p++;
    if (*p++ != 0xF7)
        return 0;
    modrm = *p++;
    mod = modrm >> 6;
    op = (modrm >> 3) & 7;
    rm = modrm & 7;
    if (op != 6 && op != 7)
        return 0;
    if (mod == 3) {
        divisor = (int32_t)r[reg_index[rm]];
        have_divisor = 1;
    } else {
        if (rm == 4) {
            uint8_t sib = *p++;
            if (mod == 0 && (sib & 7) == 5)
                p += 4;
        } else if (mod == 0 && rm == 5) {
            p += 4;
        }
        p += (mod == 1) ? 1 : (mod == 2) ? 4 : 0;
    }

    if (op == 7 && a == INT32_MIN && (!have_divisor || divisor == -1)) {
        r[REG_EAX] = (greg_t)INT32_MIN; /* overflow: the R3000 gives MIN, 0 */
        r[REG_EDX] = 0;
    } else {
        r[REG_EDX] = (greg_t)a;
        r[REG_EAX] = (op == 6) ? (greg_t)0xFFFFFFFF : (greg_t)(a >= 0 ? -1 : 1);
    }

    for (i = 0; i < nlogged && logged[i] != (uintptr_t)r[REG_EIP]; i++) {
    }
    if (i == nlogged && nlogged < 64) {
        void *site = (void *)r[REG_EIP];

        logged[nlogged++] = (uintptr_t)site;
        say("dw1: division by zero (PS1 result used) at ");
        backtrace_symbols_fd(&site, 1, STDERR_FILENO);
    }
    r[REG_EIP] = (greg_t)(uintptr_t)p;
    return 1;
}
#endif

static void on_fpe(int sig, siginfo_t *info, void *ctx)
{
#if defined(__i386__)
    if ((info->si_code == FPE_INTDIV || info->si_code == FPE_INTOVF) &&
        emulate_mips_div((ucontext_t *)ctx))
        return;
#else
    (void)info;
    (void)ctx;
#endif
    on_crash(sig);
}

void dw1_crash_handler_install(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_crash;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_fpe;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGFPE, &sa, NULL);
}
#else
void dw1_crash_handler_install(void)
{
}
#endif
