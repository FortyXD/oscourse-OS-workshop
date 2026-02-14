#include <inc/fifo.h>
#include <inc/lib.h>
#include <inc/x86.h>

static ssize_t devfifo_read(struct Fd *fd, void *buf, size_t n);
static ssize_t devfifo_write(struct Fd *fd, const void *buf, size_t n);
static int devfifo_stat(struct Fd *fd, struct Stat *stat);
static int devfifo_close(struct Fd *fd);

struct Dev devfifo = {
        .dev_id = 'F',
        .dev_name = "fifo",
        .dev_read = devfifo_read,
        .dev_write = devfifo_write,
        .dev_close = devfifo_close,
        .dev_stat = devfifo_stat,
};

/* Захватывает спин-лок FIFO в общей памяти через атомарный xchg,
 * с активным ожиданием и уступкой CPU. */
static void
fifo_lock(struct Fifo *f) {
    while (xchg(&f->lock, 1))
        sys_yield();
}

/* Освобождает спин-лок FIFO с release-семантикой. */
static void
fifo_unlock(struct Fifo *f) {
    __atomic_store_n(&f->lock, 0, __ATOMIC_RELEASE);
}

/* Читает до n байт из FIFO в буфер.
 * Блокируется при пустом буфере, но возвращает EOF, если писателей нет. */
static ssize_t
devfifo_read(struct Fd *fd, void *vbuf, size_t n) {
    struct Fifo *f = (struct Fifo *)fd2data(fd);
    uint8_t *buf = vbuf;

    for (size_t i = 0; i < n; i++) {
        while (1) {
            fifo_lock(f);
            if (f->rpos != f->wpos) {
                buf[i] = f->buf[f->rpos % sizeof(f->buf)];
                f->rpos++;
                fifo_unlock(f);
                break;
            }

            if (f->writers == 0) {
                fifo_unlock(f);
                return i;
            }
            fifo_unlock(f);

            if (i > 0)
                return i;
            sys_yield();
        }
    }

    return n;
}

/* Пишет до n байт в FIFO.
 * Если читателей нет, посылает SIGPIPE и возвращает -E_PIPE (или число байт). */
static ssize_t
devfifo_write(struct Fd *fd, const void *vbuf, size_t n) {
    struct Fifo *f = (struct Fifo *)fd2data(fd);
    const uint8_t *buf = vbuf;

    for (size_t i = 0; i < n; i++) {
        while (1) {
            fifo_lock(f);
            if (f->readers == 0) {
                fifo_unlock(f);
                sigval_t val = {.sival_int = 0};
                sigqueue(0, SIGPIPE, val);
                return i ? (ssize_t)i : -E_PIPE;
            }
            if (f->wpos < f->rpos + sizeof(f->buf)) {
                f->buf[f->wpos % sizeof(f->buf)] = buf[i];
                f->wpos++;
                fifo_unlock(f);
                break;
            }
            fifo_unlock(f);

            if (i > 0)
                return i;
            sys_yield();
        }
    }

    return n;
}

/* Возвращает статическую информацию о FIFO и текущий размер очереди. */
static int
devfifo_stat(struct Fd *fd, struct Stat *stat) {
    struct Fifo *f = (struct Fifo *)fd2data(fd);
    strcpy(stat->st_name, "<fifo>");
    stat->st_size = f->wpos - f->rpos;
    stat->st_isdir = 0;
    stat->st_dev = &devfifo;
    return 0;
}

/* Закрывает FIFO: уменьшает счетчики читателей/писателей и размэпливает страницу данных. */
static int
devfifo_close(struct Fd *fd) {
    struct Fifo *f = (struct Fifo *)fd2data(fd);
    int omode = fd->fd_omode & O_ACCMODE;

    if (omode == O_RDONLY) {
        __atomic_fetch_sub(&f->readers, 1, __ATOMIC_ACQ_REL);
    } else if (omode == O_WRONLY) {
        __atomic_fetch_sub(&f->writers, 1, __ATOMIC_ACQ_REL);
    } else if (omode == O_RDWR) {
        __atomic_fetch_sub(&f->readers, 1, __ATOMIC_ACQ_REL);
        __atomic_fetch_sub(&f->writers, 1, __ATOMIC_ACQ_REL);
    }

    return sys_unmap_region(0, fd2data(fd), PAGE_SIZE);
}
