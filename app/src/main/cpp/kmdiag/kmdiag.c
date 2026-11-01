// Diagnostics for phones whose logcat apps can't read (some vendors block
// it). Writes to one file, which the launcher saves to Downloads after a
// crash:
//  - every __android_log_print message (the 2.11 engine logs only through
//    it), copied as it's written
//  - on a fatal signal, the faulting address, registers, a backtrace and
//    the engine return addresses found on the stack, each as library +
//    offset. Then the previous handler runs, so Android's crash report is
//    unchanged.
// Plain C, so it doesn't bring its own C++ runtime next to the game's.

#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <jni.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <unwind.h>

#include "shadowhook.h"

static int g_fd = -1;

static void out(const char *s, size_t n) {
    if (g_fd >= 0 && n) (void) write(g_fd, s, n);
}

static void outf(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) out(buf, n < (int) sizeof buf ? (size_t) n : sizeof buf - 1);
}

static void stamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    outf("%02d:%02d:%02d.%03ld ", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000);
}

// ---- log copy ----------------------------------------------------------------

typedef int (*log_buf_write_fn)(int, int, const char *, const char *);
static log_buf_write_fn g_buf_write_orig;
typedef int (*log_write_fn)(int, const char *, const char *);
static void *g_print_stub;
static log_write_fn g_log_write;

static int print_proxy(int prio, const char *tag, const char *fmt, ...) {
    char msg[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    static const char levels[] = "??VDIWEF";
    stamp();
    outf("%c/%s: %s\n", prio >= 0 && prio < 8 ? levels[prio] : '?', tag ? tag : "", msg);
    // Straight to the original buffered writer, so the line isn't copied twice.
    if (g_buf_write_orig) return g_buf_write_orig(0 /* LOG_ID_MAIN */, prio, tag, msg);
    return g_log_write ? g_log_write(prio, tag, msg) : 0;
}

// Java's android.util.Log writes through __android_log_buf_write.

static int buf_write_proxy(int buf, int prio, const char *tag, const char *text) {
    static const char levels[] = "??VDIWEF";
    stamp();
    outf("%c/%s: %s\n", prio >= 0 && prio < 8 ? levels[prio] : '?', tag ? tag : "", text ? text : "");
    return g_buf_write_orig ? g_buf_write_orig(buf, prio, tag, text) : 0;
}

// ---- crash report ------------------------------------------------------------

static void where(const char *label, uintptr_t addr) {
    Dl_info info;
    if (addr && dladdr((void *) addr, &info) && info.dli_fname) {
        const char *name = strrchr(info.dli_fname, '/');
        name = name ? name + 1 : info.dli_fname;
        outf("  %s %p  %s+0x%lx", label, (void *) addr, name, (unsigned long) (addr - (uintptr_t) info.dli_fbase));
        if (info.dli_sname) outf("  (%s+0x%lx)", info.dli_sname, (unsigned long) (addr - (uintptr_t) info.dli_saddr));
        out("\n", 1);
    } else {
        outf("  %s %p\n", label, (void *) addr);
    }
}

static int in_engine(uintptr_t addr) {
    Dl_info info;
    return addr && dladdr((void *) addr, &info) && info.dli_fname && strstr(info.dli_fname, "libGame.so");
}

struct trace { int n; };

static _Unwind_Reason_Code frame(struct _Unwind_Context *ctx, void *arg) {
    struct trace *t = arg;
    uintptr_t pc = _Unwind_GetIP(ctx);
    if (!pc || t->n >= 40) return _URC_END_OF_STACK;
    char label[16];
    snprintf(label, sizeof label, "#%02d", t->n++);
    where(label, pc);
    return _URC_NO_REASON;
}

static const int kSignals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP };
static struct sigaction g_old[32];

static void on_crash(int sig, siginfo_t *info, void *uc_void) {
    ucontext_t *uc = uc_void;
    stamp();
    outf("\n*** fatal signal %d (code %d) fault address %p, thread %d\n",
         sig, info->si_code, info->si_addr, (int) syscall(SYS_gettid));
#if defined(__aarch64__)
    uintptr_t pc = uc->uc_mcontext.pc, lr = uc->uc_mcontext.regs[30], sp = uc->uc_mcontext.sp;
    where("pc", pc);
    where("lr", lr);
    for (int i = 0; i < 29; i += 4) {
        outf("  x%-2d %016lx", i, (unsigned long) uc->uc_mcontext.regs[i]);
        for (int j = 1; j < 4 && i + j < 29; j++) outf(" x%-2d %016lx", i + j, (unsigned long) uc->uc_mcontext.regs[i + j]);
        out("\n", 1);
    }
    out("backtrace:\n", 11);
    struct trace t = { 0 };
    _Unwind_Backtrace(frame, &t);
    out("engine addresses on the stack:\n", 31);
    const uintptr_t *stack = (const uintptr_t *) sp;
    int found = 0;
    for (int i = 0; i < 2048 && found < 32; i++) {
        if (in_engine(stack[i])) {
            char label[16];
            snprintf(label, sizeof label, "sp+%03x", i * 8);
            where(label, stack[i]);
            found++;
        }
    }
#else
    (void) uc;
#endif
    out("*** end\n", 8);
    fsync(g_fd);

    // Let the previous handler (Android's crash reporter) have it.
    sigaction(sig, &g_old[sig], NULL);
    syscall(SYS_tgkill, getpid(), syscall(SYS_gettid), sig);
}

static void install_handlers(void) {
    static char alt[64 * 1024];
    stack_t ss = { .ss_sp = alt, .ss_size = sizeof alt, .ss_flags = 0 };
    sigaltstack(&ss, NULL);
    for (size_t i = 0; i < sizeof kSignals / sizeof kSignals[0]; i++) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = on_crash;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&sa.sa_mask);
        sigaction(kSignals[i], &sa, &g_old[kSignals[i]]);
    }
}

// Re-install from the game thread too, after the engine may have set its own.
JNIEXPORT void JNICALL Java_com_samp_mobile_game_Diag_installCrashHandler(JNIEnv *env, jclass cls) {
    (void) env; (void) cls;
    install_handlers();
}

JNIEXPORT void JNICALL Java_com_samp_mobile_game_Diag_note(JNIEnv *env, jclass cls, jstring text) {
    (void) cls;
    const char *s = (*env)->GetStringUTFChars(env, text, NULL);
    stamp();
    outf("%s\n", s);
    (*env)->ReleaseStringUTFChars(env, text, s);
}

JNIEXPORT void JNICALL Java_com_samp_mobile_game_Diag_start(JNIEnv *env, jclass cls, jstring path) {
    (void) cls;
    if (g_fd >= 0) return;
    const char *p = (*env)->GetStringUTFChars(env, path, NULL);
    g_fd = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC, 0644);
    (*env)->ReleaseStringUTFChars(env, path, p);
    if (g_fd < 0) return;
    outf("kmdiag started, pid %d\n", getpid());

    install_handlers();

    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) == 0) {
        void *write_fn = dlsym(RTLD_DEFAULT, "__android_log_write");
        g_log_write = (log_write_fn) write_fn;
        g_print_stub = shadowhook_hook_sym_name("liblog.so", "__android_log_print", (void *) print_proxy, NULL);
        if (!g_print_stub) outf("log copy unavailable: %s\n", shadowhook_to_errmsg(shadowhook_get_errno()));
        if (!shadowhook_hook_sym_name("liblog.so", "__android_log_buf_write", (void *) buf_write_proxy,
                                      (void **) &g_buf_write_orig)) {
            outf("Java log copy unavailable: %s\n", shadowhook_to_errmsg(shadowhook_get_errno()));
        }
    } else {
        outf("shadowhook init failed: %s\n", shadowhook_to_errmsg(shadowhook_get_errno()));
    }
}
