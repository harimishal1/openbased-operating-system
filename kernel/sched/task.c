
#include <error.h>
#include <string.h>
#include <paging.h>
#include <task.h>
#include <cpu.h>
#include <lib.h>

#include <kernel/monitor.h>
#include <kernel/mem.h>
#include <kernel/sched.h>


pid_t pid_max = 1 << 16;
struct task **tasks = (struct task **)PIDMAP_BASE;
size_t nuser_tasks = 0;

/* Looks up the respective task for a given PID.
 * If check_perm is non-zero, this function checks if the PID maps to the
 * current task or if the current task is the parent of the task that the PID
 * maps to.
 *
 * If pid is zero, this will return the current process
 */
struct task *pid2task(pid_t pid, int check_perm)
{
	struct task *task;

	/* PID 0 is the current task. */
	if (pid == 0) {
		return cur_task;
	}

	/* Limit the PID. */
	if (pid >= pid_max) {
		return NULL;
	}

	/* Look up the task in the PID map. */
	task = tasks[pid];

	/* No such mapping found. */
	if (!task) {
		return NULL;
	}

	/* If we don't have to do a permission check, we can simply return the
	 * task.
	 */
	if (!check_perm) {
		return task;
	}

	/* Check if the task is the current task or if the current task is the
	 * parent. If not, then the current task has insufficient permissions.
	 */
	if (task != cur_task && task->task_ppid != cur_task->task_pid) {
		return NULL;
	}

	return task;
}

void task_init(void)
{
	/* Allocate an array of pointers at PIDMAP_BASE to be able to map PIDs
	 * to tasks.
	 */
	/* LAB 3: your code here. */
}

/* Sets up the virtual address space for the task. */
static int task_setup_vas(struct task *task)
{
	struct page_info *page;

	/* Allocate a page for the page table. */
	page = page_alloc(ALLOC_ZERO);

	if (!page) {
		return -ENOMEM;
	}

	++page->pp_ref;

	/* Now set task->task_pml4 and initialize the page table.
	 * Can you use kernel_pml4 as a template?
	 */

	/* LAB 3: your code here. */
	return 0;
}

/* Allocates and initializes a new task.
 * On success, the new task is returned.
 */
struct task *task_alloc(pid_t ppid)
{
	struct task *task;
	pid_t pid;

	/* Allocate a new task struct. */
	task = kmalloc(sizeof *task);

	if (!task) {
		return NULL;
	}

	/* Set up the virtual address space for the task. */
	if (task_setup_vas(task) < 0) {
		kfree(task);
		return NULL;
	}

	/* Find a free PID for the task in the PID mapping and associate the
	 * task with that PID.
	 */
	for (pid = 1; pid < pid_max; ++pid) {
		if (!tasks[pid]) {
			tasks[pid] = task;
			task->task_pid = pid;
			break;
		}
	}
	/* We are out of PIDs. */
	if (pid == pid_max) {
		page_decref(pa2page(PADDR(task->task_pml4)));
		kfree(task);
		return NULL;
	}

	/* Set up the task. */
	task->task_ppid = ppid;
	task->task_type = TASK_TYPE_USER;
	task->task_status = TASK_RUNNABLE;
	task->task_runs = 0;

	memset(&task->task_frame, 0, sizeof task->task_frame);

	task->task_frame.ds = GDT_UDATA | 3;
	task->task_frame.ss = GDT_UDATA | 3;
	task->task_frame.rsp = USTACK_TOP;
	task->task_frame.cs = GDT_UCODE | 3;


	/* You will set task->task_frame.rip later. */
	cprintf("[PID %5u] New task with PID %u\n",
	        cur_task ? cur_task->task_pid : 0, task->task_pid);

	return task;
}

/* Sets up the initial program binary, stack and processor flags for a user
 * process.
 * This function is ONLY called during kernel initialization, before running
 * the first user-mode environment.
 *
 * This function loads all loadable segments from the ELF binary image into the
 * task's user memory, starting at the appropriate virtual addresses indicated
 * in the ELF program header.
 * At the same time it clears to zero any portions of these segments that are
 * marked in the program header as being mapped but not actually present in the
 * ELF file, i.e., the program's .bss section.
 *
 * Finally, this function maps one page for the program's initial stack.
 */
static void task_load_elf(struct task *task, uint8_t *binary)
{
	/* Hints:
	 * - Load each program segment into virtual memory at the address
	 *   specified in the ELF section header.
	 * - You should only load segments with type ELF_PROG_LOAD.
	 * - Each segment's virtual address can be found in p_va and its
	 *   size in memory can be found in p_memsz.
	 * - The p_filesz bytes from the ELF binary, starting at binary +
	 *   p_offset, should be copied to virtual address p_va.
	 * - Any remaining memory bytes should be zero.
	 * - Use populate_region() and protect_region().
	 * - Check for malicious input.
	 *
	 * Loading the segments is much simpler if you can move data directly
	 * into the virtual addresses stored in the ELF binary.
	 * So in which address space should we be operating during this
	 * function?
	 *
	 * You must also do something with the entry point of the program, to
	 * make sure that the task starts executing there.
	 */

	/* LAB 3: your code here. */

	/* Now map one page for the program's initial stack at virtual address
	 * USTACK_TOP - PAGE_SIZE.
	 */

	/* LAB 3: your code here. */


}

/* Allocates a new task with task_alloc(), loads the named ELF binary using
 * task_load_elf() and sets its task type.
 * If the task is a user task, increment the number of user tasks.
 * This function is ONLY called during kernel initialization, before running
 * the first user-mode task.
 * The new task's parent PID is set to 0.
 */
void task_create(uint8_t *binary, enum task_type type)
{
	/* LAB 3: your code here. */
}

/* Free the task and all of the memory that is used by it.
 */
void task_free(struct task *task)
{
	struct task *waiting;
	/* If we are freeing the current task, switch to the kernel_pml4
	 * before freeing the page tables, just in case the page gets re-used.
	 */
	if (task == cur_task) {
		load_pml4((struct page_table *)PADDR(kernel_pml4));
	}

	/* Unmap the task from the PID map. */
	tasks[task->task_pid] = NULL;

	/* Unmap the user pages. */
	unmap_user_pages(task->task_pml4);
	page_decref(pa2page(PADDR(task->task_pml4)));

	/* Note the task's demise. */
	cprintf("[PID %5u] Freed task with PID %u\n",
	    cur_task ? cur_task->task_pid : task->task_ppid,
 	    task->task_pid);


	/* Free the task. */
	kfree(task);
}

/* Frees the task. If the task is the currently running task, then this
 * function runs a new task (and does not return to the caller).
 */
void task_destroy(struct task *task)
{
	task_free(task);

	cprintf("Destroyed the only task - nothing more to do!\n");
	halt_kernel();
}

/*
 * Restores the register values in the trap frame with the iretq or sysretq
 * instruction. This exits the kernel and starts executing the code of some
 * task.
 *
 * This function does not return.
 */
void task_pop_frame(struct int_frame *frame)
{
	switch (frame->int_no) {
#ifdef BONUS_SYSCALL
		case 0x80: sysret64(frame); break;
#endif
		default: iret64(frame); break;
	}

	panic("We should have gone back to userspace!");
}

/* Context switch from the current task to the provided task.
 * Note: if this is the first call to task_run(), cur_task is NULL.
 *
 * This function does not return.
 */
void task_run(struct task *task)
{
	/*
	 * Step 1: If this is a context switch (a new task is running):
	 *     1. Set the current task (if any) back to
	 *        TASK_RUNNABLE if it is TASK_RUNNING (think about
	 *        what other states it can be in),
	 *     2. Set 'cur_task' to the new task,
	 *     3. Set its status to TASK_RUNNING,
	 *     4. Update its 'task_runs' counter,
	 *     5. Use load_pml4() to switch to its address space.
	 * Step 2: Use task_pop_frame() to restore the task's
	 *     registers and drop into user mode in the
	 *     task.
	 *
	 * Hint: This function loads the new task's state from
	 *  e->task_frame.  Go back through the code you wrote above
	 *  and make sure you have set the relevant parts of
	 *  e->task_frame to sensible values.
	 */

	/* LAB 3: Your code here. */
	panic("task_run() not yet implemented");



}

/*
 * Checks that the task is allowed to access the range of memory
 * [va, va + size). If it can, then the function simply returns.
 * If it cannot, the task gets killed and if the task is the current task,
 * this function will not return.
 *
 * Note: this function expects PROT_* flags, not PAGE_*! (see lib.h)
 */
void assert_user_mem(struct task *task, void *va, size_t size, int flags)
{
	uintptr_t fault_va;

	/* LAB 4: your code here */

	uint64_t page_flags = PAGE_USER;
	if (flags & PROT_READ)
		page_flags |= PAGE_PRESENT;
	if (flags & PROT_WRITE)
		page_flags |= PAGE_WRITE;
	if (!(flags & PROT_EXEC))
		page_flags |= PAGE_NO_EXEC;

	if (check_user_mem(&fault_va, task->task_pml4, va, size, page_flags) < 0) {
		cprintf("[PID %5u] Access violation for va %p\n",
			task->task_pid, fault_va);
		task_destroy(task);
	}
}
