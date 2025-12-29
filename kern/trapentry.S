/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

#include <kern/picirq.h>

#ifdef CONFIG_KSPACE

.globl clock_thdlr
.type clock_thdlr, @function
clock_thdlr:
    call save_trapframe_trap
    # Set trap code for trapframe
    movl $(IRQ_OFFSET + IRQ_CLOCK), 136(%rsp)
    call trap
    jmp .

.globl timer_thdlr
.type timer_thdlr, @function
timer_thdlr:
    call save_trapframe_trap
    # Set trap code for trapframe
    movl $(IRQ_OFFSET + IRQ_TIMER), 136(%rsp)
    call trap
    jmp .

#else

# TRAPHANDLER defines a globally-visible function for handling a trap.
# It pushes a trap number onto the stack, then jumps to _alltraps.
# Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
#
# You shouldn't call a TRAPHANDLER function from C, but you may
# need to _declare_ one in C (for instance, to get a function pointer
# during IDT setup).  You can declare the function with
#   void NAME();
# where NAME is the argument passed to TRAPHANDLER.

#define TRAPHANDLER(name, num) \
.globl name;                   \
.type name, @function;         \
.align 2;                      \
name:                          \
    pushq $(num);              \
    jmp _alltraps

# Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
# It pushes a 0 in place of the error code, so the trap frame has the same
# format in either case.

#define TRAPHANDLER_NOEC(name, num) \
.globl name;                        \
.type name, @function;              \
.align 2;                           \
name:                               \
    pushq $0;                       \
    pushq $(num);                   \
    jmp _alltraps

.text

.globl _alltraps
.type _alltraps, @function;
.align 2
_alltraps:
    # LAB 8: Your code here
    # Complete `struct Trapframe' on stack
    # NOTE: Members after tf_paddind4 already on stack.
    #       For struct PushRegs you can use PUSHA macro.
    subq $16,%rsp
    movw %ds,8(%rsp)
    movw %es,(%rsp)
    PUSHA
    # LAB 8: Your code here
    # Invoke `trap' with a pointer to struct trap as an argument.
    movl $GD_KD,%eax
    movw %ax,%ds
    movw %ax,%es
    movq %rsp, %rdi
    call trap
    jmp .

# LAB 8: Your code here
# Use TRAPHANDLER or TRAPHANDLER_NOEC to setup
# all trap handlers' entry points
# (Look up their list in inc/trap.h)

TRAPHANDLER_NOEC(thdlr_0, 0)
TRAPHANDLER_NOEC(thdlr_1, 1)
TRAPHANDLER_NOEC(thdlr_2, 2)
TRAPHANDLER_NOEC(thdlr_3, 3)
TRAPHANDLER_NOEC(thdlr_4, 4)
TRAPHANDLER_NOEC(thdlr_5, 5)
TRAPHANDLER_NOEC(thdlr_6, 6)
TRAPHANDLER_NOEC(thdlr_7, 7)
TRAPHANDLER(thdlr_8, 8)
TRAPHANDLER_NOEC(thdlr_9, 9)
TRAPHANDLER(thdlr_10, 10)
TRAPHANDLER(thdlr_11, 11)
TRAPHANDLER(thdlr_12, 12)
TRAPHANDLER(thdlr_13, 13)
TRAPHANDLER(thdlr_14, 14)
TRAPHANDLER_NOEC(thdlr_15, 15)
TRAPHANDLER_NOEC(thdlr_16, 16)
TRAPHANDLER(thdlr_17, 17)
TRAPHANDLER_NOEC(thdlr_18, 18)
TRAPHANDLER_NOEC(thdlr_19, 19)
TRAPHANDLER_NOEC(thdlr_20, 20)
TRAPHANDLER(thdlr_21, 21)

TRAPHANDLER_NOEC(thdlr_32, 32)
TRAPHANDLER_NOEC(thdlr_40, 40)

TRAPHANDLER_NOEC(thdlr_48, 48)

#endif
