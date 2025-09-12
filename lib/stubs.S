
#include <x86-64/idt.h>

.section .text

.global do_syscall
do_syscall:
	# The syscall number is passed as the 7th argument, which in the SysV AMD64
	# ABI is passed on the stack. However, for syscalls, the number should be
	# passed in RAX, so we move it there.
	movq 0x8(%rsp), %rax

#ifdef BONUS_SYSCALL
	# When using SYSCALL instead of interrupts, we use R10 instead of RCX
	movq %rcx, %r10
	syscall
#else
	int $INT_SYSCALL
#endif

	retq
