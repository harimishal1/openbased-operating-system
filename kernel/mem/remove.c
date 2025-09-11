
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

struct remove_info {
	struct page_table *pml4;
	// new
	size_t size_removed;
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
	struct page_info *page;

	/* LAB 2: your code here. */
	if (*entry & PAGE_PRESENT) {
		if (*entry & PAGE_HUGE) {

			page = pa2page(PAGE_ADDR(*entry));

			if (info->size_removed < HPAGE_SIZE - 1) {
				int r = ptbl_split(entry, base, end, walker);
				if (r < 0) {
					return r;
				}
			} else {
				page_decref(page);
			}

			// page_decref(page);
			*entry = 0;
			tlb_invalidate(info->pml4, (void *) base);
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
		.size_removed = size,
	};
	struct page_walker walker = {
		.pte_callback = remove_pte,
		.pde_callback = remove_pde,
		/* LAB 2: your code here. */
		// .pte_unmap = ptbl_free,
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
	unmap_page_range(pml4, va, PAGE_SIZE);
}
