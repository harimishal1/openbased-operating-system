
#include <task.h>
#include <vma.h>

#include <kernel/vma.h>

#include "kernel/mem/kmem.h"

/* Given a task and a VMA, this function splits the VMA at the given address
 * by setting the end address of original VMA to the given address and by
 * adding a new VMA with the given address as base.
 */
struct vma *split_vma(struct task *task, struct vma *lhs, void *addr)
{
	/* LAB 4: your code here. */
	if (addr <= lhs->vm_base || addr >= lhs->vm_end) {
		return lhs;
	}
	
	if (!page_aligned((uintptr_t)addr)) {
    	return NULL;
	}

	struct vma *rhs = kmalloc(sizeof(*lhs));
	if (!rhs) {
		return NULL;
	}

	rhs->vm_base = addr;
	rhs->vm_end = lhs->vm_end;
	rhs->vm_flags = lhs->vm_flags;
	rhs->vm_name = lhs->vm_name;
	rhs->vm_src = lhs->vm_src;

	rhs->vm_len = rhs->vm_end - rhs->vm_base;
	lhs->vm_end = addr;
	lhs->vm_len = lhs->vm_end - lhs->vm_base;

	rb_node_init(&rhs->vm_rb);
	rhs->vm_rb.child[0] = NULL;
	rhs->vm_rb.child[1] = NULL;
	list_init(&rhs->vm_mmap);
	if (insert_vma(task, rhs) < 0) {
		kfree(rhs);
		return NULL;
	}

	return rhs;
}

/* Given a task and a VMA, this function first splits the VMA into a left-hand
 * and right-hand side at address base. Then this function splits the
 * right-hand side or the original VMA, if no split happened, into a left-hand
 * and a right-hand side. This function finally returns the right-hand side of
 * the first split or the original VMA.
 */
struct vma *split_vmas(struct task *task, struct vma *vma, void *base, size_t size)
{
	/* LAB 4: your code here. */
    struct vma *middle = vma;

    if (middle->vm_base < base) {
        middle = split_vma(task, middle, base);
        if (!middle) return NULL;
    }

    if (middle->vm_end > base + size) {
        struct vma *rhs = split_vma(task, middle, base + size);
        if (!rhs) return NULL;
    }

    return middle;
}

