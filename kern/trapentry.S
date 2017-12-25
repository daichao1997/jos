/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text
/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(do_divide, T_DIVIDE);
TRAPHANDLER_NOEC(do_debug, T_DEBUG);
TRAPHANDLER_NOEC(do_nmi, T_NMI);
TRAPHANDLER_NOEC(do_brkpt, T_BRKPT);
TRAPHANDLER_NOEC(do_oflow, T_OFLOW);
TRAPHANDLER_NOEC(do_bound, T_BOUND);
TRAPHANDLER_NOEC(do_illop, T_ILLOP);
TRAPHANDLER_NOEC(do_device, T_DEVICE);
TRAPHANDLER(do_dblflt, T_DBLFLT);

TRAPHANDLER(do_tss, T_TSS);
TRAPHANDLER(do_segnp, T_SEGNP);
TRAPHANDLER(do_stack, T_STACK);
TRAPHANDLER(do_gpflt, T_GPFLT);
TRAPHANDLER(do_pgflt, T_PGFLT);

TRAPHANDLER_NOEC(do_fperr, T_FPERR);
TRAPHANDLER(do_align, T_ALIGN);
TRAPHANDLER_NOEC(do_mchk, T_MCHK);
TRAPHANDLER_NOEC(do_simderr, T_SIMDERR);

TRAPHANDLER_NOEC(do_syscall, T_SYSCALL);

TRAPHANDLER_NOEC(do_timer, IRQ_OFFSET + IRQ_TIMER);
TRAPHANDLER_NOEC(do_irq1, IRQ_OFFSET + 1);
TRAPHANDLER_NOEC(do_irq2, IRQ_OFFSET + 2);
TRAPHANDLER_NOEC(do_irq3, IRQ_OFFSET + 3);
TRAPHANDLER_NOEC(do_irq4, IRQ_OFFSET + 4);
TRAPHANDLER_NOEC(do_irq5, IRQ_OFFSET + 5);
TRAPHANDLER_NOEC(do_irq6, IRQ_OFFSET + 6);
TRAPHANDLER_NOEC(do_irq7, IRQ_OFFSET + 7);
TRAPHANDLER_NOEC(do_irq8, IRQ_OFFSET + 8);
TRAPHANDLER_NOEC(do_irq9, IRQ_OFFSET + 9);
TRAPHANDLER_NOEC(do_irq10, IRQ_OFFSET + 10);
TRAPHANDLER_NOEC(do_irq11, IRQ_OFFSET + 11);
TRAPHANDLER_NOEC(do_irq12, IRQ_OFFSET + 12);
TRAPHANDLER_NOEC(do_irq13, IRQ_OFFSET + 13);
TRAPHANDLER_NOEC(do_irq14, IRQ_OFFSET + 14);
TRAPHANDLER_NOEC(do_irq15, IRQ_OFFSET + 15);
/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	pushl %ds
	pushl %es
	pushal
	
	movl $GD_KD, %eax
	movw %ax, %ds
	movw %ax, %es

	pushl %esp

	call trap
