#include <inc/lib.h>

static void
usage(void) {
    cprintf("usage: kill -SIGNAL -VALUE ENVID\n");
    exit();
}

void
umain(int argc, char **argv) {
    if (argc != 4)
        usage();

    char *end = NULL;
    long sig = strtol(argv[1], &end, 0);
    if (end == argv[1] || *end)
        usage();
    if (sig < 0)
        sig = -sig;

    long val = strtol(argv[2], &end, 0);
    if (end == argv[2] || *end)
        usage();

    long envid = strtol(argv[3], &end, 0);
    if (end == argv[3] || *end)
        usage();

    sigval_t sval = {.sival_int = (int)val};
    int res = sigqueue((envid_t)envid, (int)sig, sval);
    if (res < 0)
        cprintf("kill: %i\n", res);
}
