#include <inc/error.h>
#include <inc/string.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/sched.h>
#include <kern/signal.h>

/* Проверяет, заблокирован ли сигнал в текущей маске env.
 * SIGKILL никогда не блокируется и всегда считается разрешенным. */
static bool
sig_is_blocked(struct Env *env, int sig) {
    if (sig == SIGKILL)
        return 0;
    return (env->env_sig_mask & sig_bit(sig)) != 0;
}

/* Ищет в очереди первый сигнал, который входит в заданный набор (битовую маску).
 * Возвращает индекс в очереди или -1, если подходящего сигнала нет. */
static int
sig_queue_find_matching(struct Env *env, sigset_t set) {
    for (uint16_t i = 0; i < env->env_sig_queue_len; i++) {
        if (set & sig_bit(env->env_sig_queue[i].signo))
            return i;
    }
    return -1;
}

/* Ищет первый незаблокированный сигнал в очереди env.
 * Возвращает индекс или -1, если все сигналы заблокированы/очередь пуста. */
static int
sig_queue_find_unblocked(struct Env *env) {
    for (uint16_t i = 0; i < env->env_sig_queue_len; i++) {
        int sig = env->env_sig_queue[i].signo;
        if (!sig_is_blocked(env, sig))
            return i;
    }
    return -1;
}

/* Удаляет элемент очереди по индексу, при необходимости сдвигая хвост.
 * Если out != NULL, копирует удаленную запись наружу. */
static int
sig_queue_remove(struct Env *env, uint16_t idx, struct SigQueueEntry *out) {
    if (idx >= env->env_sig_queue_len)
        return -E_INVAL;

    if (out)
        *out = env->env_sig_queue[idx];

    if (idx + 1 < env->env_sig_queue_len) {
        memmove(&env->env_sig_queue[idx],
                &env->env_sig_queue[idx + 1],
                (env->env_sig_queue_len - idx - 1) * sizeof(env->env_sig_queue[0]));
    }
    env->env_sig_queue_len--;
    return 0;
}

/* Инициализирует подсистему сигналов для нового env:
 * сбрасывает трамплин, маски, ожидания и ставит SIG_DFL для всех сигналов. */
void
sig_init_env(struct Env *env) {
    env->env_sigentry = NULL;
    env->env_sig_mask = 0;
    env->env_sig_waiting = 0;
    env->env_sig_wait_set = 0;
    env->env_sig_wait_oldmask = 0;
    env->env_sig_wait_dst = 0;
    env->env_sig_queue_len = 0;

    for (int i = 0; i < NSIG; i++) {
        env->env_sig_actions[i].sa_handler = SIG_DFL;
        env->env_sig_actions[i].sa_mask = 0;
        env->env_sig_actions[i].sa_flags = 0;
    }
}

/* Ставит сигнал в очередь env или немедленно пробуждает sigwait,
 * если env ждет сигнал из соответствующего набора. */
int
sigqueue_env(struct Env *env, int sig, sigval_t value) {
    if (!sig_valid(sig))
        return -E_INVAL;

    if (env->env_sig_waiting && (env->env_sig_wait_set & sig_bit(sig))) {
        struct AddressSpace *old = switch_address_space(&env->address_space);
        user_mem_assert(env, (void *)env->env_sig_wait_dst, sizeof(int), PROT_W);
        nosan_memcpy((void *)env->env_sig_wait_dst, &sig, sizeof(sig));
        switch_address_space(old);

        env->env_sig_waiting = 0;
        env->env_sig_mask = env->env_sig_wait_oldmask;
        env->env_status = ENV_RUNNABLE;
        env->env_tf.tf_regs.reg_rax = 0;
        return 0;
    }

    if (env->env_sig_queue_len >= SIG_QUEUE_MAX)
        return -E_NO_MEM;

    env->env_sig_queue[env->env_sig_queue_len].signo = sig;
    env->env_sig_queue[env->env_sig_queue_len].value = value;
    env->env_sig_queue_len++;
    return 0;
}

/* Доставляет один ожидающий незаблокированный сигнал в user-mode:
 * формирует Sigframe на пользовательском стеке и перенастраивает RIP/RSP
 * на трамплин env_sigentry. SIG_IGN пропускается, SIG_DFL/SIGKILL убивает env. */
void
sig_deliver_pending(struct Env *env, struct Trapframe *tf) {
    if (!env || !(tf->tf_cs & 3))
        return;
    if (env->env_sig_waiting)
        return;

    if (!env->env_sigentry) {
        int kill_idx = sig_queue_find_matching(env, sig_bit(SIGKILL));
        if (kill_idx >= 0) {
            struct SigQueueEntry entry;
            if (sig_queue_remove(env, kill_idx, &entry) == 0)
                env_destroy(env);
        }
        return;
    }

    int idx = sig_queue_find_unblocked(env);
    if (idx < 0)
        return;

    struct SigQueueEntry entry;
    if (sig_queue_remove(env, idx, &entry) < 0)
        return;

    struct sigaction *act = &env->env_sig_actions[entry.signo];
    void *handler = (act->sa_flags & SA_SIGINFO) ? (void *)act->sa_sigaction : (void *)act->sa_handler;

    if (handler == SIG_IGN)
        return;

    if (entry.signo == SIGKILL || handler == SIG_DFL || !env->env_sigentry) {
        env_destroy(env);
        return;
    }

    uintptr_t sp = tf->tf_rsp;
    uintptr_t frame = (sp - sizeof(struct Sigframe)) & ~0xFULL;
    uintptr_t user_rsp = frame - 8;

    user_mem_assert(env, (void *)frame, sizeof(struct Sigframe), PROT_W);

    struct Sigframe sf;
    sf.sf_tf = *tf;
    sf.sf_oldmask = env->env_sig_mask;
    sf.sf_info.si_signo = entry.signo;
    sf.sf_info.si_value = entry.value;
    sf.sf_handler = handler;
    sf.sf_flags = act->sa_flags;

    env->env_sig_mask |= act->sa_mask | sig_bit(entry.signo);

    struct AddressSpace *old = switch_address_space(&env->address_space);
    nosan_memcpy((void *)frame, &sf, sizeof(sf));
    switch_address_space(old);

    tf->tf_regs.reg_rdi = frame;
    tf->tf_rip = (uintptr_t)env->env_sigentry;
    tf->tf_rsp = user_rsp;
}
