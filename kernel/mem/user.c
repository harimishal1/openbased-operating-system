
#include "x86-64/paging.h"
#include <types.h>
#include <cpu.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/sched/task.h>

struct user_info {
	uintptr_t va;
	uint64_t flags;
};

static int check_user_hole(uintptr_t base, uintptr_t end,
	struct page_walker *walker)
{
	struct user_info *info = walker->udata;

	/* LAB 3: your code here. */
	info->va = base;
	return -1;
	// return 0;
}

static int check_user_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct user_info *info = walker->udata;

	/* LAB 3: your code here. */
	uint64_t flags = info->flags;
	if (!(*entry & PAGE_PRESENT) || !(*entry & PAGE_USER)) {
		info->va = base;
		return -1;
	}
	if ((flags & PAGE_WRITE) && !(*entry & PAGE_WRITE)) {
		info->va = base;
		return -1;
	}
	if (!(flags & PAGE_NO_EXEC) && (*entry & PAGE_NO_EXEC)) {
		info->va = base;
		return -1;
	}
	return 0;
}

static int check_user_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct user_info *info = walker->udata;

	/* LAB 3: your code here. */
	if (*entry & PAGE_HUGE) {
		uint64_t flags = info->flags;
		if (!(*entry & PAGE_PRESENT) || !(*entry & PAGE_USER)) {
			info->va = base;
			return -1;
		}
		if ((flags & PAGE_WRITE) && !(*entry & PAGE_WRITE)) {
			info->va = base;
			return -1;
		}
		if (!(flags & PAGE_NO_EXEC) && (*entry & PAGE_NO_EXEC)) {
			info->va = base;
			return -1;
		}
	}
	return 0;
}

/*
 * Checks that in the given PML4, access is allowed to the range of memory
 * [va, va + size) with the permissions flags | PAGE_PRESENT | PAGE_USER.
 * 
 * In case access is not allowed, the "failing" address is passed in fault_va
 * and this function will return -1. On success, returns 0.
 */
int check_user_mem(uintptr_t *fault_va, struct page_table *pml4, void *va,
	size_t size, uint64_t flags)
{
	struct user_info info = {
		.flags = flags | PAGE_PRESENT | PAGE_USER,
	};
	struct page_walker walker = {
		.pt_hole_callback = check_user_hole,
		.pte_callback = check_user_pte,
		.pde_callback = check_user_pde,
		.udata = &info,
	};
	int ret;

	ret = walk_page_range(pml4, va, (void *)((uintptr_t)va + size),
		&walker);

	*fault_va = info.va;

	return ret;
}
