#ifndef JOS_INC_SIGNAL_H
#define JOS_INC_SIGNAL_H

#include <inc/types.h>
#include <inc/trap.h>

typedef uint64_t sigset_t;

typedef union sigval {
    int sival_int;
    void *sival_ptr;
} sigval_t;

typedef struct siginfo {
    int si_signo;
    sigval_t si_value;
} siginfo_t;

struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t *, void *);
    };
    sigset_t sa_mask;
    int sa_flags;
};

#define SA_SIGINFO 0x1

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

#define SIGHUP  1
#define SIGINT  2
#define SIGQUIT 3
#define SIGILL  4
#define SIGABRT 6
#define SIGFPE  8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15

#define NSIG 32
#define SIG_QUEUE_MAX 32

/* Проверяет корректность номера сигнала (1..NSIG-1). */
static inline int
sig_valid(int sig) {
    return sig > 0 && sig < NSIG;
}

/* Возвращает битовую маску для данного сигнала в sigset_t. */
static inline sigset_t
sig_bit(int sig) {
    return 1ULL << (sig - 1);
}

/* Очищает набор сигналов (делает его пустым). */
static inline void
sigemptyset(sigset_t *set) {
    *set = 0;
}

/* Заполняет набор сигналов (все биты в 1). */
static inline void
sigfillset(sigset_t *set) {
    *set = ~0ULL;
}

/* Добавляет сигнал в набор; возвращает 0 или -1 при неверном номере. */
static inline int
sigaddset(sigset_t *set, int sig) {
    if (!sig_valid(sig)) return -1;
    *set |= sig_bit(sig);
    return 0;
}

/* Удаляет сигнал из набора; возвращает 0 или -1 при неверном номере. */
static inline int
sigdelset(sigset_t *set, int sig) {
    if (!sig_valid(sig)) return -1;
    *set &= ~sig_bit(sig);
    return 0;
}

/* Проверяет, содержится ли сигнал в наборе (0/1). */
static inline int
sigismember(const sigset_t *set, int sig) {
    if (!sig_valid(sig)) return 0;
    return (*set & sig_bit(sig)) != 0;
}

struct Sigframe {
    struct Trapframe sf_tf;
    sigset_t sf_oldmask;
    siginfo_t sf_info;
    void *sf_handler;
    int sf_flags;
};

struct SigQueueEntry {
    int signo;
    sigval_t value;
};

#endif /* JOS_INC_SIGNAL_H */
