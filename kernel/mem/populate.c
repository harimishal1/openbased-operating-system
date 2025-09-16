
#include "kernel/mem/buddy.h"
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
	struct page_info *page;
	struct populate_info *info = walker->udata;

	/* LAB 3: your code here. */
	//cprintf("gets to populate");
	if (*entry & PAGE_PRESENT) {
		return 0; 
	}
	page = page_alloc(ALLOC_ZERO);

	if (!page) {
		return -1;
	}

	page->pp_ref++;
	*entry = page2pa(page) | (info->flags) | PAGE_PRESENT;
	return 0;
}

static int populate_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct page_info *page;
	struct populate_info *info = walker->udata;

	/* LAB 3: your code here. */
	if (*entry & PAGE_PRESENT && *entry & PAGE_HUGE) { //fix this if block
		return 0; 
	}
	if(info->base <= base && info->end >= end) {
		//cprintf("huge pag
		page = page_alloc(ALLOC_ZERO | ALLOC_HUGE);
		if (!page) 
			return -1; 
		page->pp_ref++;
		*entry = page2pa(page) | info->flags | PAGE_PRESENT | PAGE_HUGE; 
	} else {
		return ptbl_split(entry, base, end, walker);
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
