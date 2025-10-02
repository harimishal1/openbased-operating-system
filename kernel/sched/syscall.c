
#include "syscall.h"
#include "kernel/sched/task.h"
#include "task.h"
#include "types.h"
#include "x86-64/types.h"
#include <error.h>
#include <string.h>
#include <assert.h>
#include <lib.h>

#include <x86-64/asm.h>
#include <x86-64/gdt.h>

#include <kernel/console.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <kernel/vma/syscall.h>

extern void syscall64(void);

void syscall_init(void)
{
	/* LAB 3: your bonus code here. */
#ifdef BONUS_SYSCALL
	write_msr(MSR_STAR, ((uint64_t)GDT_KCODE << 32) | ((uint64_t)GDT_UCODE << 48));
	write_msr(MSR_LSTAR, (uint64_t)syscall64);
	write_msr(MSR_SFMASK, IF_RFLAGS);
	write_msr(MSR_EFER, read_msr(MSR_EFER) | MSR_EFER_SCE);
	write_msr(MSR_KERNEL_GS_BASE, (uint64_t)&this_cpu);
	#endif
}

/*
 * Print a string to the system console.
 * The string is exactly 'len' characters long.
 * Destroys the environment on memory errors.
 */
static void sys_cputs(const char *s, size_t len)
{
	/* Check that the user has permission to read memory [s, s+len).
	 * Destroy the environment if not. */
	/* LAB 3: your code here. */
	assert_user_mem(cur_task, (void *)s, len, PROT_READ);
	/* Print the string supplied by the user. */
	cprintf("%.*s", len, s);
}

/*
 * Read a character from the system console without blocking.
 * Returns the character, or 0 if there is no input waiting.
 */
static int sys_cgetc(void)
{
	return cons_getc();
}

/* Returns the PID of the current task. */
static pid_t sys_getpid(void)
{
	return cur_task->task_pid;
}

static int sys_kill(pid_t pid)
{
	struct task *task;
	/* LAB 5: your code here. */

	task = pid2task(pid, 1);

	if (!task) {
		return -1;
	}

	task->task_status = TASK_DYING;
	cprintf("[PID %5u] Marking task %d as DYING\n", cur_task->task_pid, task->task_pid);

	cprintf("[PID %5u] Exiting gracefully\n", task->task_pid);
	task_destroy(task);

	return 0;
}

static int sys_exit(int rcode)
{
	struct task *task;
	/* LAB 5: your code here. */

	/* LAB 3: your code here */
	task = cur_task;	

	if (!task) {
		return -1;
	}
	task->task_exit_status = rcode;

	task->task_status = TASK_DYING;

	cprintf("[PID %5u] Exiting gracefully with code %d\n", task->task_pid, rcode);

	task_destroy(task);

	return 0;
}



/* Dispatches to the correct kernel function, passing the arguments. */
int64_t syscall(uint64_t syscallno, uint64_t a1, uint64_t a2, uint64_t a3,
        uint64_t a4, uint64_t a5, uint64_t a6)
{
	/*
	 * Call the function corresponding to the 'syscallno' parameter.
	 * Return any appropriate return value.
	 */
	/* LAB 3: your code here. */
	if ((unsigned)syscallno >= NSYSCALLS) {
		return -ENOSYS;
	}

	switch (syscallno) {
		case SYS_cputs:
			sys_cputs((const char*)a1, (size_t)a2); 
			return 0;
		case SYS_cgetc: 
			return (int64_t)sys_cgetc();
		case SYS_getpid:
			return (int64_t)sys_getpid();
		case SYS_kill:
			return (int64_t)sys_kill( (pid_t)a1); 
		case SYS_exit: 
			return (int64_t)sys_exit((int)a1);
		case SYS_mquery:
			return (sys_mquery((struct vma_info*) a1, (void*)a2));
		case SYS_mmap:
			return (int64_t)sys_mmap((void*)a1, (size_t)a2, (int)a3, (int)a4, (int)a5, (uintptr_t)a6);
		case SYS_munmap:
			sys_munmap((void*)a1, (size_t) a2);
			return 0;
		case SYS_mprotect:
			return sys_mprotect((void*) a1, (size_t) a2, (int) a3);
		case SYS_madvise:
			return sys_madvise((void*) a1, (size_t) a2, (int) a3);
		case SYS_yield:
			sched_yield();
			return 0;
		case SYS_wait:
			return sys_wait((int *) a1);
		case SYS_waitpid:
			return sys_waitpid((pid_t) a1, (int *) a2, (int) a3);
		case SYS_fork:
			return sys_fork();
		default: 
			break;
	}
	return -ENOSYS;
}

void syscall_handler(uint64_t a1, uint64_t a2, uint64_t a3,
    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t syscallno)
{
	struct int_frame *frame;

	/* Syscall from user mode. */
	assert(cur_task);

	/* Avoid using the frame on the stack. */
	frame = &cur_task->task_frame;

	/* Issue the syscall. */
	frame->rax = syscall(syscallno, a1, a2, a3, a4, a5, a6);

	/* Return to the current task, which should be running. */
	task_run(cur_task);
}
