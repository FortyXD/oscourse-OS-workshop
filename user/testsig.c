#include <inc/lib.h>

static volatile int got_usr1;
static volatile int got_val;

static void
usr1_handler(int sig, siginfo_t *info, void *ctx) {
    USED(sig);
    USED(ctx);
    got_usr1 = 1;
    got_val = info->si_value.sival_int;
}

void
umain(int argc, char **argv) {
    USED(argc);
    USED(argv);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = usr1_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0)
        panic("sigaction failed");

    sigval_t v = {.sival_int = 42};
    if (sigqueue(0, SIGUSR1, v) < 0)
        panic("sigqueue failed");

    for (int i = 0; i < 1000 && !got_usr1; i++)
        sys_yield();

    if (!got_usr1 || got_val != 42)
        panic("SIGUSR1 handler did not run");

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);

    envid_t parent = sys_getenvid();
    envid_t child = fork();
    if (child < 0)
        panic("fork failed");
    if (child == 0) {
        sigval_t v2 = {.sival_int = 0};
        sigqueue(parent, SIGUSR2, v2);
        exit();
    }

    int sig = 0;
    if (sigwait(&set, &sig) < 0)
        panic("sigwait failed");
    if (sig != SIGUSR2)
        panic("sigwait returned wrong signal");

    wait(child);
    cprintf("signals: OK\n");
}
