# Individual Task Design (Signals + FIFO + Shell)

## Goals
- POSIX-like signals: `sigqueue`, `sigwait`, `sigaction`, with `SIGPIPE` support.
- Named FIFO files in the JOS filesystem (`mkfifo` + removal).
- Background jobs in shell (`&`) and a `kill` utility with integer value.

## Signals
### Data model
- Per-env fields in `struct Env`:
  - `env_sig_actions[NSIG]`, `env_sig_mask`, `env_sig_queue[]`.
  - `env_sigentry` trampoline pointer set once from user space.
  - `sigwait` state (`env_sig_waiting`, `env_sig_wait_set`, `env_sig_wait_dst`).
- Queue is a simple fixed-size array; signals are dequeued in arrival order.

### Delivery path
- `sys_sigqueue` enqueues a signal or wakes `sigwait`.
- `sig_deliver_pending()` is called before returning to user mode:
  - Pick first unblocked pending signal.
  - `SIG_DFL` => destroy env, `SIG_IGN` => drop.
  - Otherwise, build `Sigframe` on user stack and redirect `RIP` to user
    trampoline (`sig_entry`).
- `sig_entry` calls the handler and then `sys_sigreturn`, which restores the
  saved `Trapframe` and signal mask.

### `sigwait`
- If a pending signal in the set exists, it is returned immediately.
- Otherwise the env is marked not runnable and woken when a matching signal
  arrives.

## FIFO
### Persistent type
- New `FTYPE_FIFO` stored on disk (`mkfifo` sets the type).
- FIFO file persists across reboot, but in-memory FIFO buffer is runtime only.

### Runtime transport
- FS server maintains a shared one-page `struct Fifo` buffer per FIFO file.
- `serve_open` maps that page into the client’s `fd2data` address (passed in
  the open request).
- `devfifo` handles `read/write/close` using the shared buffer and counters.
- `SIGPIPE` is raised on write when no readers remain.

## Shell + Utilities
- `sh` recognizes `&` and skips waits for background jobs.
- `kill` syntax: `kill -SIGNAL -VALUE ENVID`, uses `sigqueue`.
- `mkfifo` utility calls the new API.

## Tests
- `user/testsig`: handler + `sigqueue`, `sigwait` behavior.
- `user/testfifo`: FIFO data flow across fork.
- `user/testsigpipe`: SIGPIPE + `-E_PIPE` on FIFO write.
- `user/testpipe`: updated to ignore SIGPIPE to preserve prior behavior.

Suggested manual run:
```
testsig
testfifo
testsigpipe
```
