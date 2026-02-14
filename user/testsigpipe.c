#include <inc/lib.h>

static volatile int got_sigpipe;

static void
pipe_handler(int sig) {
    USED(sig);
    got_sigpipe = 1;
}

void
umain(int argc, char **argv) {
    USED(argc);
    USED(argv);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = pipe_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) < 0)
        panic("sigaction failed");

    const char *path = "/sigpipe.fifo";
    remove(path);
    if (mkfifo(path) < 0)
        panic("mkfifo failed");

    envid_t child = fork();
    if (child < 0)
        panic("fork failed");
    if (child == 0) {
        int rfd = open(path, O_RDONLY);
        if (rfd < 0)
            panic("open fifo for read failed: %i", rfd);

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGUSR1);
        int sig = 0;
        sigwait(&set, &sig);

        close(rfd);
        exit();
    }

    int fd = open(path, O_WRONLY);
    if (fd < 0)
        panic("open fifo failed: %i", fd);

    sigval_t v = {.sival_int = 0};
    sigqueue(child, SIGUSR1, v);
    wait(child);

    int res = write(fd, "x", 1);
    if (res != -E_PIPE)
        panic("expected -E_PIPE, got %i", res);

    for (int i = 0; i < 1000 && !got_sigpipe; i++)
        sys_yield();

    if (!got_sigpipe)
        panic("SIGPIPE handler not called");

    close(fd);
    remove(path);
    cprintf("sigpipe: OK\n");
}
