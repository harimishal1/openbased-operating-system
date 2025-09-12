
#include "kernel/mem/buddy.h"
#include "kernel/mem/ptbl.h"
#include "kernel/mem/tlb.h"
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

struct remove_info {
	struct page_table *pml4;
	uintptr_t base_remove;
	uintptr_t end_remove;
};

/* Removes the page if present by decrementing the reference count, clearing the
 * PTE and invalidating the TLB.
 */
static int remove_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct remove_info *info = walker->udata;
	struct page_info *page;

	/* LAB 2: your code here. */
	if (*entry & PAGE_PRESENT) {
		page = pa2page(PAGE_ADDR(*entry));
		// page->pp_ref--;
		page_decref(page);
		*entry = 0;
		tlb_invalidate(info->pml4, (void *) base);
	}
	return 0;
}

/* Removes the page if present and if it is a huge page by decrementing the
 * reference count, clearing the PDE and invalidating the TLB.
 * If the region to remove does not span the entire PDE, perform a split.
 */
static int remove_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct remove_info *info = walker->udata;
	struct page_info *old_page;

	/* LAB 2: your code here. */
	if (*entry & PAGE_PRESENT) {
		if (*entry & PAGE_HUGE) {

			old_page = pa2page(PAGE_ADDR(*entry));
			if(info->base_remove > base && info->end_remove < end) {
				ptbl_split(entry, base & ~(PAGE_TABLE_SPAN - 1), end | (PAGE_TABLE_SPAN - 1), walker);
			} else {
				*entry = 0;
				page_decref(old_page);
			}
			tlb_invalidate(info->pml4, (void *) (base & ~(PAGE_TABLE_SPAN - 1)));
			return 0;
		}
	}
	return 0;
}


/* Unmaps the range of pages from [va, va + size). */
void unmap_page_range(struct page_table *pml4, void *va, size_t size)
{
	/* LAB 2: your code here. */
	struct remove_info info = {
		.pml4 = pml4,
		.base_remove = (uintptr_t) va,
		.end_remove = (uintptr_t) va + size,
	};
	struct page_walker walker = {
		.pte_callback = remove_pte,
		.pde_callback = remove_pde,
		/* LAB 2: your code here. */
		.pde_unmap = ptbl_free,
		.pdpte_unmap = ptbl_free,
		.pml4e_unmap = ptbl_free,
		.udata = &info,
	};

	walk_page_range(pml4, va, va + size, &walker);
}

/* Unmaps all user pages. */
void unmap_user_pages(struct page_table *pml4)
{
	unmap_page_range(pml4, 0, USER_LIM);
}

/* Unmaps the physical page at the virtual address va. */
void page_remove(struct page_table *pml4, void *va)
{
	/* LAB 2: your code here */
	struct page_info *page = page_lookup(pml4, va, NULL);
	if (!page) {
		return;
	}
	if (page->pp_order == BUDDY_2M_PAGE) {
		unmap_page_range(pml4, va, HPAGE_SIZE);
	} else {
		unmap_page_range(pml4, va, PAGE_SIZE);
	}
}
