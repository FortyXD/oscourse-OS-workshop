#include <inc/lib.h>

static const char *fifo_path = "/test.fifo";
static const char *msg = "fifo message";

void
umain(int argc, char **argv) {
    USED(argc);
    USED(argv);

    remove(fifo_path);
    if (mkfifo(fifo_path) < 0)
        panic("mkfifo failed");

    envid_t child = fork();
    if (child < 0)
        panic("fork failed");

    if (child == 0) {
        int fd = open(fifo_path, O_RDONLY);
        if (fd < 0)
            panic("open fifo for read failed: %i", fd);
        char buf[32];
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n < 0)
            panic("read failed: %i", n);
        buf[n] = '\0';
        if (strcmp(buf, msg) != 0)
            panic("unexpected fifo data: %s", buf);
        close(fd);
        exit();
    }

    int fd = open(fifo_path, O_WRONLY);
    if (fd < 0)
        panic("open fifo for write failed: %i", fd);
    if (write(fd, msg, strlen(msg)) != (int)strlen(msg))
        panic("write failed");
    close(fd);

    wait(child);
    remove(fifo_path);
    cprintf("fifo: OK\n");
}
