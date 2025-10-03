
#include "kernel/mem/populate.h"
#include "kernel/mem/init.h"
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
	int *flags = (int* )udata;
 	uint64_t populate_flags = PAGE_PRESENT | PAGE_USER;
	if ((*flags & PROT_READ) && !(vma->vm_flags & PROT_READ)) {
		return -1;
	}
	if ((*flags & PROT_WRITE) && !(vma->vm_flags & PROT_WRITE)) {
		return -1;
	}
	if ((*flags & PROT_EXEC) && !(vma->vm_flags & PROT_EXEC)) {
		return -1;
	}
	if (vma->vm_flags & PROT_WRITE) {
		populate_flags |= PAGE_WRITE;
	}
	if (!(vma->vm_flags & PROT_EXEC)) {
		populate_flags |= PAGE_NO_EXEC;
	}

	if (!vma->vm_src) { 
        uintptr_t hbase = (uintptr_t)base & ~(HPAGE_SIZE - 1);
        uintptr_t hend  = hbase + HPAGE_SIZE;
        if (hbase >= (uintptr_t)vma->vm_base && hend <= (uintptr_t)vma->vm_end) {
            populate_region(task->task_pml4, (void*)hbase, HPAGE_SIZE, populate_flags | PAGE_HUGE);
			protect_region(task->task_pml4, base, size, populate_flags);
            return 0;
        }
    }
	
	populate_region(task->task_pml4, base, size, populate_flags);
	if (vma->vm_src) {
		size_t vma_offset = (uintptr_t)base - (uintptr_t)vma->vm_base;
		struct page_table *old_pml4 = KADDR(read_cr3());
		load_pml4((struct page_table *)PADDR(task->task_pml4));
		if (vma_offset < vma->vm_len) {
			memcpy(base, vma->vm_src + vma_offset, MIN(size, vma->vm_len - vma_offset));
		}
		load_pml4((struct page_table *)PADDR(old_pml4));
	}
	protect_region(task->task_pml4, base, size, populate_flags);

	return 0;
}

/* Populates the VMAs for the given address range [base, base + size) by
 * backing the VMAs with physical pages.
 */
int populate_vma_range(struct task *task, void *base, size_t size, int flags)
{
	return walk_vma_range(task, base, size, do_populate_vma, &flags);
}
