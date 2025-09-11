
/* System call stubs. */

#include <syscall.h>
#include <lib.h>

extern int64_t do_syscall(uint64_t a1, uint64_t a2,
	uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t num);

static inline int64_t syscall(int num, int check,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4,
	unsigned long a5, unsigned long a6)
{
	int64_t ret;

	/*
	 * In OpenLSD, system calls follow the same conventions as used by Linux. In
	 * other words, this is the normal System V AMD64 ABI calling conventions
	 * with some small modifications to deal with system call specifics.
	 * Concretely, our convention is as follows:
	 *  - The system call number is passed in RAX
	 *  - The six parameters of a system call are passed in
	 *      RDI, RSI, RDX, RCX, R8, and R9, in that order.
	 *  - The response of a system call is in RAX.
	 *  - For the SYSCALL bonus (non-interrupt route), we use R10 instead of RCX.
	 *
	 * The special rules for RAX and RCX/R10 are implemented in Assembly in
	 * do_syscall. Therefore, we can call do_syscall like a normal function with
	 * the syscall number as seventh parameter (on the stack).
	 */
	ret = do_syscall(a1, a2, a3, a4, a5, a6, num);

	if(check && ret < 0) {
		panic("syscall %d returned %d (> 0)", num, ret);
	}

	return ret;
}

void puts(const char *s, size_t len)
{
	syscall(SYS_cputs, 0, (uintptr_t)s, len, 0, 0, 0, 0);
}

int getc(void)
{
	return syscall(SYS_cgetc, 0, 0, 0, 0, 0, 0, 0);
}

int kill(pid_t pid)
{
	return syscall(SYS_kill, 1, pid, 0, 0, 0, 0, 0);
}

void exit(int exitcode)
{
	syscall(SYS_exit, 0, exitcode, 0, 0, 0, 0, 0);
}

pid_t getpid(void)
{
	 return syscall(SYS_getpid, 0, 0, 0, 0, 0, 0, 0);
}



