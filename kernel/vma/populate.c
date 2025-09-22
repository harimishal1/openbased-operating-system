
#include "kernel/mem/populate.h"
#include "kernel/mem/protect.h"
#include "x86-64/paging.h"
#include <types.h>
#include <lib.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

/* Checks the flags in udata against the flags of the VMA to check appropriate
 * permissions. If the permissions are all right, this function populates the
 * address range [base, base + size) with physical pages. If the VMA is backed
 * by an executable, the data is copied over. Then the protection of the
 * physical pages is adjusted to match the permissions of the VMA.
 */
int do_populate_vma(struct task *task, void *base, size_t size,
	struct vma *vma, void *udata)
{
	/* LAB 4: your code here. */
	int *flags = udata;

	if ((*flags & PROT_READ) && !(vma->vm_flags & PROT_READ)) {
		return -1;
	}
	if ((*flags & PROT_WRITE) && !(vma->vm_flags & PROT_WRITE)) {
		return -1;
	}
	if ((*flags & PROT_EXEC) && !(vma->vm_flags & PROT_EXEC)) {
		return -1;
	}

	uint64_t populate_flags = PAGE_PRESENT | PAGE_USER;
	if (vma->vm_flags & PROT_WRITE) {
		populate_flags |= PAGE_WRITE;
	}
	if (!(vma->vm_flags & PROT_EXEC)) {
		populate_flags |= PAGE_NO_EXEC;
	}

	populate_region(task->task_pml4, base, size, populate_flags);

	if (vma->vm_src) {
		size_t vma_offset = (uintptr_t)base - (uintptr_t)vma->vm_base;
		
		if (vma_offset < vma->vm_len) {
			memcpy(base, vma->vm_src + vma_offset, MIN(size, vma->vm_len - vma_offset));
		}
	}

	// protect_region(task->task_pml4, base, size, populate_flags);

	return 0;
}

/* Populates the VMAs for the given address range [base, base + size) by
 * backing the VMAs with physical pages.
 */
int populate_vma_range(struct task *task, void *base, size_t size, int flags)
{
	return walk_vma_range(task, base, size, do_populate_vma, &flags);
}

