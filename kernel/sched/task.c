
#include "kernel/sched/task.h"
#include "elf.h"
#include "kernel/mem/buddy.h"
#include "kernel/mem/init.h"
#include "kernel/mem/insert.h"
#include "kernel/mem/kmem.h"
#include "kernel/mem/protect.h"
#include "kernel/sched/idt.h"
#include "kernel/sched/sched.h"
#include "kernel/vma/insert.h"
#include "kernel/vma/protect.h"
#include "kernel/vma/remove.h"
#include "kernel/vma/user.h"
#include "lapic.h"
#include "list.h"
#include "spinlock.h"
#include "stdio.h"
#include "types.h"
#include "x86-64/asm.h"
#include "x86-64/idt.h"
#include "x86-64/memory.h"
#include "x86-64/paging.h"
#include "x86-64/types.h"
#include <error.h>
#include <string.h>
#include <paging.h>
#include <task.h>
#include <cpu.h>
#include <lib.h>

#include <kernel/monitor.h>
#include <kernel/mem.h>
#include <kernel/sched.h>


extern struct spinlock kernel_lock;
extern struct spinlock runq_lock;
extern struct list runq;
extern struct list zeroq;

struct spinlock zeroq_lock = {
#ifdef DEBUG_SPINLOCK
	.name = "zeroq_lock",
#endif
};

struct spinlock kthread_lock = {
#ifdef DEBUG_SPINLOCK
	.name = "kthread_lock",
#endif
};

extern int check_user_vma_range(uintptr_t *fault_va, struct task *task, void *base, size_t size, int flags);

pid_t pid_max = 1 << 16;
struct task **tasks = (struct task **)PIDMAP_BASE;
size_t nuser_tasks = 0;
size_t nkernel_tasks = 0;

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
	populate_region(kernel_pml4, (void *)PIDMAP_BASE, pid_max * sizeof(struct task *), 
	PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC);
	memset((void *)PIDMAP_BASE, 0, pid_max * sizeof(struct task *));
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
	task->task_pml4 = (struct page_table *)page2kva(page);
	
	size_t half = PAGE_TABLE_ENTRIES / 2;
	physaddr_t *dst = task->task_pml4->entries;
	physaddr_t *src = kernel_pml4->entries;
	
	memcpy(&dst[half], &src[half], PAGE_SIZE / 2);

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
	task->task_cpunum = lapic_cpunum();

	memset(&task->task_frame, 0, sizeof task->task_frame);

	task->task_frame.ds = GDT_UDATA | 3;
	task->task_frame.ss = GDT_UDATA | 3;
	task->task_frame.rsp = USTACK_TOP;
	task->task_frame.cs = GDT_UCODE | 3;
	task->task_frame.rflags = FLAGS_IF | 0x2;

	task->task_time_budget = TIMESLICE;
	task->last_time_stamp = read_tsc();



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
	load_pml4((void *) PADDR(task->task_pml4));
	struct elf *elf_binary = (struct elf *)binary;
	if (elf_binary->e_magic != ELF_MAGIC) {
		panic("magic number is wrong");
	}

	struct elf_proghdr *program_header = (struct elf_proghdr *)(binary + elf_binary->e_phoff);

	for (size_t i = 0; i < elf_binary->e_phnum; i++) {
		
		uint64_t va = program_header[i].p_va;
		uint64_t memsz = program_header[i].p_memsz;
		uint64_t filesz = program_header[i].p_filesz;

		if (program_header[i].p_type != ELF_PROG_LOAD) {
			continue;
		}
		if (memsz < filesz) {
			panic("p_memsz is smaller than p_filesz");
		}

		uint64_t flags = (PAGE_PRESENT | PAGE_USER);
		uint64_t prot_flags = (PROT_READ | MAP_POPULATE);
		char* task_name = ".rodata";

		if (program_header[i].p_flags & ELF_PROG_FLAG_WRITE){ 
			flags |= (PAGE_WRITE | PAGE_NO_EXEC);
			prot_flags |= PROT_WRITE;
			task_name = ".data";
		}

		if (!(program_header[i].p_flags & ELF_PROG_FLAG_EXEC)){ 
			flags |= PAGE_NO_EXEC;

		}

		if ((program_header[i].p_flags & ELF_PROG_FLAG_EXEC)){ 
			prot_flags |= PROT_EXEC;
			task_name = ".text";
		}

		size_t aligned_addr_diff = program_header[i].p_va - ROUNDDOWN(program_header[i].p_va, PAGE_SIZE);
		add_executable_vma(task, task_name, (void*)va - aligned_addr_diff, ROUNDUP(va + memsz, PAGE_SIZE) - (va - aligned_addr_diff),
			prot_flags, binary + program_header[i].p_offset - aligned_addr_diff, filesz + aligned_addr_diff);

	}

	/* Now map one page for the program's initial stack at virtual address
	 * USTACK_TOP - PAGE_SIZE.
	 */

	/* LAB 3: your code here. */

	add_anonymous_vma(task,"stack", (void *)(USTACK_TOP - PAGE_SIZE), PAGE_SIZE, PROT_READ | PROT_WRITE);
	load_pml4((void *) PADDR(kernel_pml4));
	task->task_frame.rip = elf_binary->e_entry;
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
	/* LAB 5: modify your code here. */
	/* LAB 3: your code here. */
	struct task *task = task_alloc(0);
	if (!task) {
		panic("couldnt allocate task");
	}
	rb_init(&task->task_rb);
	list_init(&task->task_mmap);
	list_init(&task->task_children);
	list_init(&task->task_zombies);
	list_init(&task->task_node);
	task->task_wait = NULL;

	task_load_elf (task, binary);
	task->task_type = type;

	if (type == TASK_TYPE_USER) {
		nuser_tasks++;
	}

	task->task_ppid = 0;
	tasks[task->task_pid] = task;

	// list_add(&runq, &task->task_node);
	list_add(&this_cpu->runq, &task->task_node);

	return;
}

void zero_page_daemon(void *arg)
{	
	lapic_timer_off();
	if(!big_spin_haslock(&kernel_lock)) {
		big_spin_lock(&kernel_lock);
	}
	int freed_pages = 0;
	fine_spin_lock(&zeroq_lock);
	while (!list_is_empty(&zeroq) && freed_pages < 10) {
		struct list *node = list_pop_tail(&zeroq);
		struct page_info *pp = container_of(node, struct page_info, pp_node);
		memset(page2kva(pp), 0, PAGE_SIZE);
		list_del(&pp->pp_node);
		page_free(pp);
		freed_pages++;
	}
	list_init(&zeroq);
	fine_spin_unlock(&zeroq_lock);

	cur_task->task_status = TASK_RUNNABLE;
	list_add(&this_cpu->runq, &cur_task->task_node);
	cur_task = NULL;
	sched_yield();
}

void kthread_create(void (*entry)(void *), void *arg)
{
	/* LAB 6: your code here. */
	fine_spin_lock(&kthread_lock);
	struct task *kthread = kmalloc(sizeof(struct task));
	if (!kthread) {
		panic("couldnt allocate task");
	}

	/* struct task *kthread = page2kva(page_alloc(ALLOC_ZERO));
	if (!kthread) {
		panic("couldnt allocate task");
	}  */
	
	kthread->task_type = TASK_TYPE_KERNEL;
	
	/*Add to PID map */
	pid_t pid;
    for (pid = 1; pid < pid_max; ++pid) {
		if (!tasks[pid]) { 
			tasks[pid] = kthread; kthread->task_pid = pid; 
			break; 
		}
    }
	
	/* We are out of PIDs. */
    if (pid == pid_max) 
	{ kfree(kthread); 
		panic("Max PIDs reached"); 
	}
	nkernel_tasks++;
	size_t nkernel_number = nkernel_tasks + 1;
	
	/*Initialize task struct */	
	kthread->task_pml4 = kernel_pml4;
    kthread->task_type = TASK_TYPE_KERNEL;
    kthread->task_status = TASK_RUNNABLE;
    kthread->task_runs = 0;
    kthread->task_ppid = 0;	
	kthread->task_cpunum = lapic_cpunum();
    kthread->task_exit_status = 0;

	/* Setting up the kernel thread's lists and rb tree */
	rb_init(&kthread->task_rb);
	list_init(&kthread->task_mmap);
	list_init(&kthread->task_children);
	list_init(&kthread->task_zombies);
	list_init(&kthread->task_node);
	list_init(&kthread->task_child);
	kthread->task_wait = NULL;
	kthread->task_wait_exit_status = NULL;

	/* Setting up the kernel_thread */
	uintptr_t stack_top = KSTACK_TOP + (nkernel_number) * (PAGE_SIZE);
    uintptr_t stack_bottom = stack_top - PAGE_SIZE;

	struct page_info *page = page_alloc(ALLOC_ZERO);
    if (!page) {
		panic("Couldnt allocate page for kthread stack");
	}
	page_insert(kernel_pml4, page, (void *)(stack_bottom), PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC);

	void *kthread_stack = page2kva(page);
	if (!kthread_stack) { 
		panic("Couldnt get kva for kthread stack");
	}

	memset(&kthread->task_frame, 0, sizeof(kthread->task_frame));
    kthread->task_frame.cs     = GDT_KCODE;
    kthread->task_frame.ss     = GDT_KDATA;
    kthread->task_frame.ds     = GDT_KDATA;
    kthread->task_frame.rflags = FLAGS_IF | 0x2;
	kthread->task_frame.rdi    = (uint64_t)arg;
	kthread->task_frame.rsp    = (uint64_t)stack_top;
	kthread->task_frame.rip    = (uint64_t)entry; 

	kthread->task_time_budget = TIMESLICE;
	kthread->last_time_stamp = read_tsc();



	/* fine_spin_lock(&runq_lock);
    list_add_tail(&runq, &task->task_node);
    fine_spin_unlock(&runq_lock); */
	//add to global runq
	/* fine_spin_lock(&runq_lock);
		list_add(&this_cpu->runq, &cur_task->task_node);
    fine_spin_unlock(&runq_lock); */
	
    list_add(&this_cpu->runq, &kthread->task_node);
	cprintf("[PID %5u] New kernel thread with PID %u\n",
            cur_task ? cur_task->task_pid : 0, kthread->task_pid);
			cprintf("Daemon [PID %u] created, task_node at %p\n", kthread->task_pid, &kthread->task_node);
	fine_spin_unlock(&kthread_lock);
	return;
}

/* Free the task and all of the memory that is used by it.
 */
void task_free(struct task *task)
{
	//assert(big_spin_haslock(&kernel_lock));
	struct task *waiting;

	/* LAB 5: your code here. */
	task->task_status = TASK_DYING;
	/* If we are freeing the current task, switch to the kernel_pml4
	 * before freeing the page tables, just in case the page gets re-used.
	 */
	if (task == cur_task) {
		load_pml4((struct page_table *)PADDR(kernel_pml4));
	}

	if (task->task_ppid != 0) {
		struct task *parent = pid2task(task->task_ppid, 0);
		if (task->task_pid == 2) {
			// print parent infoif it is readyor print it is null
			if (parent) {
				cprintf("Parent of 2 is %d and its status is %d\n", parent->task_pid, parent->task_status);
				// print task_wait
				if (parent->task_wait) {
					cprintf("Parent is waiting for pid %d\n", parent->task_wait->task_pid);
				} else {
					cprintf("Parent is not waiting for any child\n");
				}
			} else {
				cprintf("Parent of 2 is NULL\n");
			}
		}
		if (parent) {
			if ((parent->task_status == TASK_NOT_RUNNABLE) && // hari - maybe change parent->task_wait to set parent later
				((parent->task_wait == NULL )|| parent->task_wait == task)) {
				if (parent->task_wait_exit_status) {
					struct page_table *old_pml4 = KADDR(read_cr3());
					load_pml4((struct page_table *)PADDR(parent->task_pml4));
					*(parent->task_wait_exit_status) = task->task_exit_status;
					load_pml4((struct page_table *)PADDR(old_pml4));
				}
				parent->task_frame.rax = task->task_pid;
				parent->task_status = TASK_RUNNABLE;
				//cprintf("task_Free_1: adding frame with rip %p to runq\n", cur_task->task_frame.rip);
				list_del(&task->task_child);
				// list_add_tail(&runq, &parent->task_node);
				list_add(&this_cpu->runq, &parent->task_node);
			} else if (task == cur_task){

				list_del(&task->task_node);
				list_del(&task->task_child);
				list_add_tail(&parent->task_zombies, &task->task_node);
				return;
			}
		}
	}
	
	struct list *node, *next;
	struct task *child;
	list_foreach_safe(&task->task_children, node, next) {
		child = container_of(node, struct task, task_child);
		list_del(&child->task_child);
		list_init(&child->task_child);
        child->task_ppid = 0;
	}
	
	list_foreach_safe(&task->task_zombies, node, next) {
		child = container_of(node, struct task, task_node);
		cprintf("DEBUG: task_free: Parent %d about to clean up zombie %d\n", task->task_pid, child->task_pid);
	    list_del(&child->task_node);
		child->task_ppid = 0;
	    task_free(child);
	}
	list_del(&task->task_node);
	

	/* Unmap the task from the PID map. */
	tasks[task->task_pid] = NULL;
	nuser_tasks--;

	/* Free the VMA */
	free_vmas(task);

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
	if(task == cur_task) {
		cur_task = NULL;
	}
	sched_yield();
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
	// print saved rsp
	//cprintf("saved rsp is %p\n", frame->rsp);
	big_spin_unlock(&kernel_lock);
	switch (frame->int_no) {
#ifdef BONUS_SYSCALL
		case 0x80: sysret64(frame); break;
#endif
	default: 
		lapic_timer_on();
		iret64(frame);
		break;
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
	cur_task = task;
	cur_task->task_status = TASK_RUNNING;
	cur_task->task_runs++;

	load_pml4((void *)PADDR(task->task_pml4));

	task_pop_frame(&task->task_frame);
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
	if (check_user_vma_range(&fault_va, task, va, size, flags) < 0) {
		cprintf("[PID %5u] Access violation for va %p\n",
			task->task_pid, fault_va);
		task_destroy(task);
	}
}
