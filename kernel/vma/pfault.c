
#include "kernel/mem/populate.h"
#include "kernel/vma/find.h"
#include "kernel/vma/populate.h"
#include "stdio.h"
#include "x86-64/types.h"
#include <types.h>
#include "lib.h"

#include <kernel/mem.h>
#include <kernel/vma.h>

/* Handles the page fault for a given task. */
int task_page_fault_handler(struct task *task, void *va, int flags)
{
	/* LAB 4: your code here. */

struct vma *vma = task_find_vma(task, va);
	
	if (!vma || va < vma->vm_base) {
		return -EFAULT;
	}
	if ((flags & PROT_WRITE) && !(vma->vm_flags & PROT_WRITE)) {
		return -EFAULT;
	}
	if ((flags & PROT_EXEC) && !(vma->vm_flags & PROT_EXEC)) {
		return -EFAULT;
	}
	if (!(vma->vm_flags & PROT_READ)) {
		return -EFAULT;
	}

	int ret = populate_vma_range((struct task*) task, ROUNDDOWN(va, PAGE_SIZE), PAGE_SIZE, (int)flags);
	return ret;
}
