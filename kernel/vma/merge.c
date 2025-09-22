
#include <task.h>
#include <vma.h>

#include <kernel/vma.h>
#include <kernel/mem.h>

/* Given a task and two VMAs, checks if the VMAs are adjacent and compatible
 * for merging. If they are, then the VMAs are merged by removing the
 * right-hand side and extending the left-hand side by setting the end address
 * of the left-hand side to the end address of the right-hand side.
 */
struct vma *merge_vma(struct task *task, struct vma *lhs, struct vma *rhs)
{
	/* LAB 4: your code here. */

	if (lhs->vm_end != rhs->vm_base && rhs->vm_end != lhs->vm_base)
        return NULL;
    if (lhs->vm_flags != rhs->vm_flags) 
		return NULL;
    if (strcmp(lhs->vm_name, rhs->vm_name) != 0)
    	return NULL;
    if (lhs->vm_src != rhs->vm_src)   
		return NULL;
    if (lhs->vm_len != rhs->vm_len)   
		return NULL;

    if (lhs->vm_end == rhs->vm_base) {
        lhs->vm_end = rhs->vm_end;
    } else if (rhs->vm_end == lhs->vm_base) {
        lhs->vm_base = rhs->vm_base;
    }

    remove_vma(task, rhs);
    kfree(rhs);

    return lhs;
}

/* Given a task and a VMA, this function attempts to merge the given VMA with
 * the previous and the next VMA. Returns the merged VMA or the original VMA if
 * the VMAs could not be merged.
 */
struct vma *merge_vmas(struct task *task, struct vma *vma)
{
	/* LAB 4: your code here. */
	return vma;
}

