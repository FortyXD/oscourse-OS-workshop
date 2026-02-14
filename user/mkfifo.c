#include <inc/lib.h>

static void
usage(void) {
    cprintf("usage: mkfifo PATH\n");
    exit();
}

void
umain(int argc, char **argv) {
    if (argc != 2)
        usage();

    int res = mkfifo(argv[1]);
    if (res < 0)
        cprintf("mkfifo: %i\n", res);
}
