
#include "kernel/mem/insert.h"
#include "kernel/mem/ptbl.h"
#include "kernel/mem/tlb.h"
#include "kernel/sched/task.h"
#include "lib.h"
#include "types.h"
#include "x86-64/memory.h"
#include "x86-64/paging.h"
#include <error.h>
#include <list.h>

#include <kernel/console.h>
#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>
#include <kernel/vma.h>

extern struct list runq;
extern struct task *task_alloc(pid_t ppid);
extern struct task **tasks;
extern size_t nuser_tasks;

/* Allocates a task struct for the child process and copies the register state,
 * the VMAs and the page tables. Once the child task has been set up, it is
 * added to the run queue.
 */
struct task *task_clone(struct task *task)
{
	/* LAB 5: your code here. */
	struct task *child_task = task_alloc(task->task_pid);
	if (!child_task) return NULL;

	// copy over register frame
	child_task->task_frame = task->task_frame;
	
	// allocate new page for child process pml4
	struct page_info *child_pml4_page = page_alloc(ALLOC_ZERO);
	if (!child_pml4_page) return NULL;
	child_pml4_page->pp_ref++;
	physaddr_t pa = page2pa(child_pml4_page);
	struct page_table *child_pml4 = (struct page_table *)KADDR(pa);
	child_task->task_pml4 = child_pml4;

	// loop over all vmas in parent task
	struct list *node;
	list_foreach(&task->task_mmap, node) {
		struct vma *current_vma = container_of(node, struct vma, vm_mmap);
		
		// add vma to child task
    	struct vma *child_vma = add_vma(
			child_task, 
			current_vma->vm_name,
			current_vma->vm_base,
			(uintptr_t)current_vma->vm_end - (uintptr_t)current_vma->vm_base,
			current_vma->vm_flags
    	);
		child_vma->vm_src = current_vma->vm_src;
		child_vma->vm_len = current_vma->vm_len;

		// loop over all pages of the vma
		struct page_info *page;
		physaddr_t *entry;
        for (uintptr_t va = (uintptr_t)current_vma->vm_base; va < (uintptr_t)current_vma->vm_end; va += PAGE_SIZE) {
			page = page_lookup(task->task_pml4, (void *)va, &entry);
			if (!page || !(*entry & PAGE_PRESENT)) continue;

			// mark page as read-only
			*entry &= ~PAGE_WRITE;
			tlb_invalidate(task->task_pml4, (void *)va);

			// add page to child page table
			page_insert(child_pml4, page, (void *)va, PAGE_PRESENT | PAGE_USER);
        }
    }

	// set task metadata
	child_task->task_ppid = task->task_pid;
	child_task->task_type = task->task_type;
	if (child_task->task_type == TASK_TYPE_USER) {
		nuser_tasks++;
	}
	child_task->task_frame.rax = 0;
	tasks[child_task->task_pid] = child_task;
	list_add(&runq, &child_task->task_node);

	return child_task;
}

pid_t sys_fork(void)
{
	/* LAB 5: your code here. */
	struct task *child_task = task_clone(cur_task);
	if (child_task) {
		return child_task->task_pid;
	}
	return -1;
	// return -ENOSYS;
}
