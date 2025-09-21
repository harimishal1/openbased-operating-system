
#include <types.h>

#include <kernel/sched.h>
#include <kernel/vma.h>

struct user_info {
	uintptr_t last;
	int flags;
};

int do_check_user_vma(struct task *task, void *base, size_t size, struct vma *vma, void *udata)
{
	struct user_info *info = udata;

	if ((uintptr_t) vma->vm_base > info->last)
		return -1;

	if ((vma->vm_flags & info->flags) != info->flags)
		return -1;

	info->last = (uintptr_t)vma->vm_end;

	return 0;
}

/**
 * Checks that the given task has VMAs that give access to the range of memory
 * [va, va + size) with the permissions "flags".
 *
 * In case access is not allowed, the "failing" address is passed in fault_va
 * and this function will return -1. On success, returns 0.
 */
int check_user_vma_range(uintptr_t *fault_va, struct task *task, void *base,
	size_t size, int flags)
{
	struct user_info info = {
		.last = (uintptr_t) base,
		.flags = flags,
	};

	int ret = walk_vma_range(task, base, size, do_check_user_vma, &info);

	if(ret != 0 || info.last < (uintptr_t)base + size) {
		*fault_va = info.last;
		return -1;
	}

	return 0;
}

