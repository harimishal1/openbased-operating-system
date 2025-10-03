
#include "kernel/mem/buddy.h"
#include "kernel/mem/insert.h"
#include "kernel/mem/tlb.h"
#include "kernel/sched/task.h"
#include "stdio.h"
#include "x86-64/paging.h"
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

struct populate_info {
	uint64_t flags;
	uintptr_t base, end;
};

static int populate_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct page_info *page = pa2page(PAGE_ADDR(*entry));
	struct populate_info *info = walker->udata;

	/* LAB 3: your code here. */
	if (*entry & PAGE_PRESENT && page->pp_ref > 1) {
		struct page_info *new_page = page_alloc(ALLOC_ZERO);
		if (!new_page) {
			return -1;
		}
		page_decref(page);
		new_page->pp_ref++;
		*entry = page2pa(new_page) | PAGE_PRESENT | info->flags;
		memcpy(page2kva(new_page), page2kva(page), PAGE_SIZE);
		tlb_invalidate(cur_task->task_pml4, (void*)base);
		return 0;
	} else if (*entry & PAGE_PRESENT && page->pp_ref == 1) {
		return 0;
	} else {
		page = page_alloc(ALLOC_ZERO);
		if (!page) {
			return -1;
		}

		page->pp_ref++;
		*entry = page2pa(page) | (info->flags) | PAGE_PRESENT;
		return 0;
	}
}

static int populate_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct page_info *page = pa2page(PAGE_ADDR(*entry));
	struct populate_info *info = walker->udata;

	/* LAB 3: your code here. */
	if ((*entry & PAGE_PRESENT) && (*entry & PAGE_HUGE && page->pp_ref > 1)) { 
		struct page_info *new_page = page_alloc(ALLOC_ZERO | ALLOC_HUGE);
		if (!new_page) {
			return -1;
		}
		*entry = page2pa(new_page) | PAGE_PRESENT | PAGE_HUGE | info->flags;
		page_decref(page);
		new_page->pp_ref++;
		memcpy(page2kva(new_page), page2kva(page), HPAGE_SIZE);
		tlb_invalidate(cur_task->task_pml4, (void*)base);
	} else if (*entry & PAGE_PRESENT && page->pp_ref == 1) {
		return 0;
	} else {
		if(info->base <= base && info->end >= end) {
		page = page_alloc(ALLOC_ZERO | ALLOC_HUGE);
		if (!page) 
			return -1; 
		page->pp_ref++;
		*entry = page2pa(page) | info->flags | PAGE_PRESENT | PAGE_HUGE; 
		} else {
			return ptbl_split(entry, base, end, walker);
		}
	}
	return 0;
}

/* Populates the region [va, va + size) with pages by allocating pages from the
 * frame allocator and mapping them.
 */
void populate_region(struct page_table *pml4, void *va, size_t size,
	uint64_t flags)
{
	/* LAB 3: your code here. */
	struct populate_info info = {
		.flags = flags,
		.base = ROUNDDOWN((uintptr_t)va, PAGE_SIZE),
		.end = ROUNDUP((uintptr_t)va + size, PAGE_SIZE) - 1,
	};
	struct page_walker walker = {
		.pte_callback = populate_pte,
		.pde_callback = populate_pde,
		.udata = &info,
		.pdpte_callback = ptbl_alloc,
		.pml4e_callback = ptbl_alloc,
		.pde_unmap = ptbl_merge,
	};

	if ((flags & PAGE_HUGE) && !hpage_aligned((uintptr_t)(va))) {
		return;
	}

	walk_page_range(pml4, va, (void *)((uintptr_t)va + size), &walker);
}
