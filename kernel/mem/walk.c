#include "x86-64/paging.h"
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

/* Given an address addr, this function returns the sign extended address. */
static uintptr_t sign_extend(uintptr_t addr)
{
	return (addr < USER_LIM) ? addr : (0xffff000000000000ull | addr);
}

/* Given an addresss addr, this function returns the page boundary. */
static uintptr_t ptbl_end(uintptr_t addr)
{
	return addr | (PAGE_SIZE - 1);
}

static uintptr_t ptbl_start(uintptr_t addr)
{
	return addr & ~(PAGE_SIZE - 1);
}

/* Given an address addr, this function returns the page table boundary. */
static uintptr_t pdir_end(uintptr_t addr)
{
	return addr | (PAGE_TABLE_SPAN - 1);
}

static uintptr_t pdir_start(uintptr_t addr)
{
	return addr & ~(PAGE_TABLE_SPAN - 1);
}

/* Given an address addr, this function returns the page directory boundary. */
static uintptr_t pdpt_end(uintptr_t addr)
{
	return addr | (PAGE_DIR_SPAN - 1);
}

static uintptr_t pdpt_start(uintptr_t addr)
{
	return addr & ~(PAGE_DIR_SPAN - 1);
}

/* Given an address addr, this function returns the PDPT boundary. */
static uintptr_t pml4_end(uintptr_t addr)
{
	return addr | (PDPT_SPAN - 1);
}

static uintptr_t pml4_start(uintptr_t addr)
{
	return addr & ~(PDPT_SPAN - 1);
}

/* Walks over the page range from base to end iterating over the entries in the
 * given page table ptbl. The user may provide walker->pte_callback() that gets
 * called for every entry in the page table. In addition the user may provide
 * walker->pt_hole_callback() that gets called for every unmapped entry in the
 * page table.
 *
 * Hint: this function calls ptbl_end() and ptbl_start to get the boundaries of
 * the current page.
 * Hint: the next page is at ptbl_end() + 1.
 * Hint: the loop condition is next < end.
 */
static int ptbl_walk_range(struct page_table *ptbl, uintptr_t base,
    uintptr_t end, struct page_walker *walker)
{
	/* LAB 2: your code here. */
	base = ptbl_start(base);
	end = ptbl_end(end);	
	for(; base <= end;) {
		
		uintptr_t next = ptbl_end(base) + 1;
		physaddr_t *entry = &ptbl->entries[PAGE_TABLE_INDEX(base)];

		if (walker->pte_callback) {
			int r = walker->pte_callback(entry, base, ptbl_end(base), walker);
			if (r < 0)
				return r;
		}

		if (*entry & PAGE_PRESENT) {

		} else {
			if (walker->pt_hole_callback) {
				int r = walker->pt_hole_callback(base, MIN(ptbl_end(base), end), walker);
				if (r < 0)
					return r;
			}
			// if(walker->pte_unmap) {
			// 	int r = walker->pte_unmap(entry, base, end, walker);
			// 	if (r < 0)
			// 		return r;
			// }
		}
		base = next;
	}
	return 0;
}

/* Walks over the page range from base to end iterating over the entries in the
 * given page directory pdir. The user may provide walker->pde_callback() that
 * gets called for every entry in the page directory. In addition the user may
 * provide walker->pt_hole_callback() that gets called for every unmapped entry
 * in the page directory. If the PDE is present, but not a huge page, this
 * function calls ptbl_walk_range() to iterate over the entries in the page
 * table. The user may provide walker->pde_unmap() that gets called for every
 * present PDE after walking over the page table.
 *
 * Hint: see ptbl_walk_range().
 * Hint: think about what base/end values to pass to the various callbacks!
 */
static int pdir_walk_range(struct page_table *pdir, uintptr_t base,
    uintptr_t end, struct page_walker *walker)
{
	/* LAB 2: your code here. */
	base = pdir_start(base);

	for(; base <= end; ) {

		uintptr_t next = pdir_end(base) + 1;
		// if (next > end) next = end + 1;
		physaddr_t *entry = &pdir->entries[PAGE_DIR_INDEX(base)];

		if (walker->pde_callback) {
			int r = walker->pde_callback(entry, base, pdir_end(base), walker);
			if (r < 0)
				return r;
		}

		if (*entry & PAGE_PRESENT) {
			if (*entry & PAGE_SIZE) {
				if (walker->pde_unmap) {
					int r = walker->pde_unmap(entry, base, pdir_end(base), walker);
					if (r < 0)
						return r;
				}
			} else {
				struct page_table *ptbl = (struct page_table *)KADDR(PAGE_ADDR(*entry)); 
				int r = ptbl_walk_range(ptbl, base, MIN(pdir_end(base), end), walker);
				if (r < 0)
					return r;
				if (walker->pde_unmap) {
					r = walker->pde_unmap(entry, base, pdir_end(base), walker);
					if (r < 0)
						return r;
				}
			}
		} else {
			if (walker->pt_hole_callback) {
				// int r = walker->pt_hole_callback(base, next < end ? next : end, walker);
				int r = walker->pt_hole_callback(base, MIN(pdir_end(base), end), walker);
				if (r < 0)
					return r;
			}
		}
		base = next;
	}
	return 0;
}

/* Walks over the page range from base to end iterating over the entries in the
 * given PDPT pdpt. The user may provide walker->pdpte_callback() that gets
 * called for every entry in the PDPT. In addition the user may provide
 * walker->pt_hole_callback() that gets called for every unmapped entry in the
 * PDPT. If the PDPTE is present, this function calls pdir_walk_range() to
 * iterate over the entries in the page directory. The user may provide
 * walker->pdpte_unmap() that gets called for every present PDPTE after walking
 * over the page directory.
 *
 * Hint: see ptbl_walk_range().
 * Hint: think about what base/end values to pass to the various callbacks!
 */
static int pdpt_walk_range(struct page_table *pdpt, uintptr_t base,
    uintptr_t end, struct page_walker *walker)
{
	/* LAB 2: your code here. */
	base = pdpt_start(base);
	for(; base <= end; ) {
		uintptr_t next = pdpt_end(base) + 1;
		physaddr_t *entry = &pdpt->entries[PDPT_INDEX(base)];

		if (walker->pdpte_callback) {
			int r = walker->pdpte_callback(entry, base, pdpt_end(base), walker);
			if (r < 0)
				return r;
		}

		if (*entry & PAGE_PRESENT) {
			struct page_table *pdir = (struct page_table *)KADDR(PAGE_ADDR(*entry));
			int r = pdir_walk_range(pdir, base, MIN(pdpt_end(base), end), walker);
			if (r < 0)
				return r;
			if (walker->pdpte_unmap) {
				r = walker->pdpte_unmap(entry, base, pdpt_end(base), walker);
				if (r < 0)
					return r;
			}
		} else {
			if (walker->pt_hole_callback) {
				int r = walker->pt_hole_callback(base, MIN(pdpt_end(base), end), walker);
				if (r < 0)
					return r;
			}
		}
		base = next;
	}
	return 0;
}

/* Walks over the page range from base to end iterating over the entries in the
 * given PML4 pml4. The user may provide walker->pml4e_callback() that gets
 * called for every entry in the PML4. In addition the user may provide
 * walker->pt_hole_callback() that gets called for every unmapped entry in the
 * PML4. If the PML4E is present, this function calls pdpt_walk_range() to
 * iterate over the entries in the PDPT. The user may provide
 * walker->pml4e_unmap() that gets called for every present PML4E after walking
 * over the PDPT.
 *
 * Hint: see ptbl_walk_range().
 * Hint: think about what base/end values to pass to the various callbacks!
 */
static int pml4_walk_range(struct page_table *pml4, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	/* LAB 2: your code here. */
    base = pml4_start(base);
	for(; base <= end; ) {

		uintptr_t next = pml4_end(base) + 1;
		physaddr_t *entry = &pml4->entries[PML4_INDEX(base)];

		if (walker->pml4e_callback) {
			int r = walker->pml4e_callback(entry, base, pml4_end(base), walker);
			if (r < 0)
				return r;
		}

		if (*entry & PAGE_PRESENT) {
			struct page_table *pdpt = (struct page_table *)KADDR(PAGE_ADDR(*entry));
			int r = pdpt_walk_range(pdpt, base, MIN(pml4_end(base), end), walker);
			if (r < 0)
				return r;
			if (walker->pml4e_unmap) {
				r = walker->pml4e_unmap(entry, base, pml4_end(base), walker);
				if (r < 0)
					return r;
			}
		} else {
			if (walker->pt_hole_callback) {
				int r = walker->pt_hole_callback(base, MIN(pml4_end(base), end), walker);
				if (r < 0)
					return r;
			}
		}
		base = next;
	}
	return 0;
}

/* Helper function to walk over a page range starting at base and ending before
 * end.
 */
int walk_page_range(struct page_table *pml4, void *base, void *end,
	struct page_walker *walker)
{
	return pml4_walk_range(pml4, ROUNDDOWN((uintptr_t)base, PAGE_SIZE),
		ROUNDUP((uintptr_t)end, PAGE_SIZE) - 1, walker);
}

/* Helper function to walk over all pages. */
int walk_all_pages(struct page_table *pml4, struct page_walker *walker)
{
	return pml4_walk_range(pml4, 0, KERNEL_LIM, walker);
}

/* Helper function to walk over all user pages. */
int walk_user_pages(struct page_table *pml4, struct page_walker *walker)
{
	return pml4_walk_range(pml4, 0, USER_LIM, walker);
}

/* Helper function to walk over all kernel pages. */
int walk_kernel_pages(struct page_table *pml4, struct page_walker *walker)
{
	return pml4_walk_range(pml4, KERNEL_VMA, KERNEL_LIM, walker);
}
