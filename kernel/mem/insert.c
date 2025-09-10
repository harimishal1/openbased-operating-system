
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

struct insert_info {
	struct page_table *pml4;
	struct page_info *page;
	uint64_t flags;
};

/* If the PTE already points to a present page, the reference count of the page
 * gets decremented and the TLB gets invalidated. Then this function increments
 * the reference count of the new page and sets the PTE to the new page with
 * the user-provided permissions.
 */
static int insert_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct insert_info *info = walker->udata;
	struct page_info *page;

	/* LAB 2: your code here. */
	if (*entry & PAGE_PRESENT) {
		page_decref(pa2page(PAGE_ADDR(*entry)));
		*entry = 0;
		tlb_invalidate(info->pml4, (void *)base);
	}
	page = info->page;
	page->pp_ref++;
	page->pp_free = 0;
	*entry = page2pa(page) | (info->flags);
	return 0;
}

/* If the PDE already points to a present huge page, the reference count of the
 * huge page gets decremented and the TLB gets invalidated. Then if the new
 * page is a 4K page, this function calls ptbl_alloc() to allocate a new page
 * table. If the new page is a 2M page, this function increments the reference
 * count of the new page and sets the PDE to the new huge page with the
 * user-provided permissions.
 */
static int insert_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct insert_info *info = walker->udata;
	struct page_info *page;

	/* LAB 2: your code here. */
	if (*entry & PAGE_PRESENT) {
		if (*entry & PAGE_HUGE) {
			page_decref(pa2page(PAGE_ADDR(*entry)));
			tlb_invalidate(info->pml4, (void *)base);

			if (info->flags & PAGE_HUGE) {
				page = info->page;
				page->pp_ref++;
				*entry = page2pa(page) | (info->flags);
			} else {
				int r = ptbl_alloc(entry, base, end, walker);
				if (r < 0) {
					return r;
				}
			}
		}
	} else {
		if (info->flags & PAGE_HUGE) {
			page = info->page;
			page->pp_ref++;
			*entry = page2pa(page) | (info->flags);
		} else {
			int r = ptbl_alloc(entry, base, end, walker);
			if (r < 0) {
				return r;
			}
		}
	}
	return 0;
}

static int insert_pdpte(physaddr_t *entry, uintptr_t base, uintptr_t end,
	struct page_walker *walker)
{
	struct insert_info *info = walker->udata;

	if (!(*entry & PAGE_PRESENT)) {
		int r = ptbl_alloc(entry, base, end, walker);
		if (r < 0) {
			return r;
		}
	}
	return 0;
}

static int insert_pml4e(physaddr_t *entry, uintptr_t base, uintptr_t end,
	struct page_walker *walker)
{
	struct insert_info *info = walker->udata;

	if (!(*entry & PAGE_PRESENT)) {
		int r = ptbl_alloc(entry, base, end, walker);
		if (r < 0) {
			return r;
		}
	}
	return 0;
}

// static int insert_hole(uintptr_t base, uintptr_t end,
// 	struct page_walker *walker)
// {
// 	struct insert_info *info = walker->udata;
// 	struct page_info *page;

// 	if(end - base == PAGE_SIZE - 1) { // case unmapped page
// 	} else if(end - base == PAGE_TABLE_SPAN - 1) { // case unmapped page table
// 	} else if(end - base == PAGE_DIR_SPAN - 1) { // case unmapped page directory
// 	} else if(end - base == PDPT_SPAN - 1) { // case unmapped PDPT
// 	} else {
// 		panic("Invalid hole size!");
// 	}

// 	return 0;
// }


/* Map the physical page page at virtual address va. The flags argument
 * contains the permission to set for the PTE. The PAGE_PRESENT flag should
 * always be set.
 *
 * Requirements:
 *  - If there is already a page mapped at va, it should be removed using
 *    page_decref().
 *  - If necessary, a page should be allocated and inserted into the page table
 *    on demand. This can be done by providing ptbl_alloc() to the page walker.
 *  - The reference count of the page should be incremented upon a successful
 *    insertion of the page.
 *  - The TLB must be invalidated if a page was previously present at va.
 *
 * Corner-case hint: make sure to consider what happens when the same page is
 * re-inserted at the same virtual address in the same page table. However, do
 * not try to distinguish this case in your code, as this frequently leads to
 * subtle bugs. There is another elegant way to handle everything in the same
 * code path.
 *
 * Hint: what should happen when the user inserts a 2M huge page at a
 * misaligned address?
 *
 * Hint: how do you deal with transparent huge paging in this method?
 *
 * Hint: this function calls walk_page_range(), hpage_aligned(), and page2pa().
 */
int page_insert(struct page_table *pml4, struct page_info *page, void *va,
    uint64_t flags)
{
	struct insert_info info;
	struct page_walker walker = {
		.pte_callback = insert_pte,
		.pde_callback = insert_pde,
		/* LAB 2: your code here. */
		.pdpte_callback = insert_pdpte,
		.pml4e_callback = insert_pml4e,
		// .pt_hole_callback = insert_hole,
		.udata = &info,
	};

	/* LAB 2: your code here. */
	info.pml4 = pml4;
	info.page = page;
	info.flags = flags | PAGE_PRESENT;
	int size = (flags & PAGE_SIZE) ? PAGE_DIR_SPAN : PAGE_SIZE;
	if (walk_page_range(pml4, va, (void *)((uintptr_t)va + size), &walker) < 0) {
		return -1;
	}
	return 0;

	// Hint: use the walker as follows
	// walk_page_range(pml4, va, (void *)((uintptr_t)va + PAGE_SIZE), &walker);
	// return -1;
}
