
#include "kernel/mem/populate.h"
#include "kernel/vma/find.h"
#include "kernel/vma/populate.h"
#include "x86-64/types.h"
#include <types.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

/* Handles the page fault for a given task. */
int task_page_fault_handler(struct task *task, void *va, int flags)
{
	/* LAB 4: your code here. */
	int r = populate_vma_range(task, ROUNDDOWN(va, PAGE_SIZE), PAGE_SIZE, flags);
	if (r < 0) {
		return r;
	}
	return 0;
}
