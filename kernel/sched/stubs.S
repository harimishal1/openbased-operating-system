
.section .text

#include <x86-64/gdt.h>
#include <x86-64/idt.h>
#include <x86-64/memory.h>

#include <cpu.h>


.macro ISR_NOERRCODE int_no
.global isr\int_no
isr\int_no:
	pushq $0
	pushq $\int_no

	jmp isr_common_stub
.endm

.macro ISR_ERRCODE int_no
.global isr\int_no
isr\int_no:
	pushq $\int_no

	jmp isr_common_stub
.endm

/* LAB 3: your code here. */

// 7-2 Vol. 3A
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 16
ISR_ERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 30
ISR_NOERRCODE 128
ISR_NOERRCODE 127

isr_common_stub:
	/* LAB 3: your code here. */
	/* Save the register state on the stack. */

/* struct int_frame {
 	uint64_t ds;
 	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
 	uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
 	uint64_t int_no, err_code;
 	uint64_t rip, cs, rflags, rsp, ss;
 }; */

    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq $0, %rax
    movw %ds, %ax
    pushq %rax

	/* Recover the segments. */

	movq $GDT_KDATA, %rax
	movw %ax, %ds
	movw %ax, %es

	/* Jump to the generic interrupt handler. */
	movq %rsp, %rdi
	jmp int_handler

.global syscall64
syscall64:
	/* LAB 3: your bonus code here. */
#ifdef BONUS_SYSCALL
	swapgs

	/* Store the userspace stack pointer in the per-cpu struct. */
	movq %rsp, %gs:CPU_TSS_RSP3

	/* Load the kernel stack pointer from the per-cpu struct. */
	movq %gs:CPU_TSS_RSP0, %rsp

	/* Store the register state into the per-cpu struct. */
	movq    %rcx, %gs:CPU_TSS_RSP1
    movq    %r11, %gs:CPU_TSS_RSP2 

#endif
	/* Jump to the system call handler. This is implemented as a normal function
	 * that expects the syscall number as its seventh argument, so we push the
	 * syscall number to the stack again. Also restore the RCX register and add
	 * a dummy return address.
	 */
	movq %r10, %rcx
	pushq %rax
	pushq $0
	jmp syscall_handler

.global iret64
iret64:
	/* LAB 3: your code here. */
	/* Restore the register state. */

    movw    (%rdi), %ax
    movw    %ax, %ds
    movw    %ax, %es

	lea     8(%rdi), %rsp

    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rdi
    popq    %rsi
    popq    %rbp
    popq    %rbx
    popq    %rdx
    popq    %rcx
    popq    %rax

	/* Return from the interrupt. */
	addq $16, %rsp
	iretq

.global sysret64
sysret64:
	/* LAB 3: your bonus code here. */
#ifdef BONUS_SYSCALL
	/* Recover the register state. */
	movq    %gs:CPU_TSS_RSP3, %rsp
    movq    %gs:CPU_TSS_RSP1, %rcx 
    movq    %gs:CPU_TSS_RSP2, %r11
    swapgs
#endif

	/* Return from the system call. */
	sysretq
