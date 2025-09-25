
#include "kernel/mem/buddy.h"
#include "kernel/mem/tlb.h"
#include "stdio.h"
#include "x86-64/asm.h"
#include "x86-64/paging.h"
#include <types.h>
#include <string.h>
#include <paging.h>

#include <kernel/mem.h>

/* Allocates a page table if none is present for the given entry.
 * If there is already something present in the PTE, then this function simply
 * returns. Otherwise, this function allocates a page using page_alloc(),
 * increments the reference count and stores the newly allocated page table
 * with the PAGE_PRESENT | PAGE_WRITE | PAGE_USER permissions.
 */
int ptbl_alloc(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	/* LAB 2: your code here. */
	if (*entry & PAGE_PRESENT) {
		return 0;
	}
	struct page_info *page = page_alloc(ALLOC_ZERO);
	if (!page) {
		return -1;
	}
	page->pp_ref++;
	*entry = page2pa(page); 
	*entry = *entry | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	return 0;
}

/**
 * Splits up a huge page by allocating a new page table and setting up the huge
 * page into smaller pages that consecutively make up the huge page.
 *
 * If no huge page was mapped at the entry, simply allocate a page table.
 *
 * Otherwise, if a huge page is present, we need to allocate a new page to serve
 * as our page table. Ensure the PDE points to this new page.
 *
 * We need to make sure we keep the memory maps and the buddy allocator in sync:
 * if we split a huge page here, we should make sure the buddy allocator sees it
 * as 512 order-0 allocations rather than 1 huge-page allocation. To do this
 * properly, allocate new pages for each entry in the new page table and copy
 * over the data. Release the huge page (decref) at the end.
 *
 * Note: if the page_info struct of the huge page is neither free nor has a
 * refcount, then this huge page refers to one of the statically-mapped memory
 * regions that are not managed by the buddy allocator (think stack, ELF). For
 * those, we should not touch the buddy allocator, and instead just fill the new
 * page table with entries pointing to the existing memory.
 *
 * Hint: it might seem appealing to reuse the existing huge page chunk from the
 * buddy allocator, and simply fill the new page table with entries pointing to
 * the existing memory. DO NOT DO THIS; this will lead to very subtle bugs when
 * later freeing individual pages in the split range (what if pp_ref == 2 before
 * the split?).
 *
 * Hint: this function calls ptbl_alloc(), page_alloc(), page2pa(), page2kva(),
 * and page_decref().
 */
int ptbl_split(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	/* LAB 2: your code here. */
	if (!(*entry & PAGE_PRESENT)) {
		return ptbl_alloc(entry, base, end, walker);
	}
	if (!(*entry & PAGE_HUGE)) {
		return ptbl_alloc(entry, base, end, walker);
	} else {
		struct page_info *huge_page = pa2page(PAGE_ADDR(*entry));
		struct page_info *new_page = page_alloc(ALLOC_ZERO);
		if (!new_page) {
			return -1;
		}
		new_page->pp_ref++;

		uint64_t flags = *entry & PAGE_UMASK;
		
		*entry = page2pa(new_page);
		*entry = *entry | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
		
		struct page_table *ptbl = (struct page_table *)page2kva(new_page);
		bool statically_mapped = (huge_page->pp_free == 0 && huge_page->pp_ref == 0);
		
		for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
			if (statically_mapped) {
				struct page_info *page = huge_page + i;	
				ptbl->entries[i] = page2pa(page) | PAGE_PRESENT | flags;
				// page->pp_free = 0;
			} else {
				struct page_info *page = page_alloc(ALLOC_ZERO);
				if (!page) {
					return -1;
				}
				
				uintptr_t page_kva = (uintptr_t)page2kva(huge_page) + i * PAGE_SIZE;
				memcpy(page2kva(page), (void *)page_kva, PAGE_SIZE);
				// page->pp_free = 0;
				page->pp_ref = 1;
				ptbl->entries[i] = page2pa(page) | PAGE_PRESENT | flags;
			}
		}
		if (!statically_mapped) {
			page_decref(huge_page);
		}
	}
	return 0;
}

/* Attempts to merge all consecutive pages in a page table into a huge page.
*
* First checks if the PDE points to a huge page. If the PDE points to a huge
* page there is nothing to do. Otherwise the PDE points to a page table.
* Then, this function checks all entries in the page table to check if they
* point to present and available pages and share the same flags. If not all
* pages are present or if not all flags are the same, this function simply
* returns.
* At this point the pages can be merged into a huge page. This function now
* allocates a huge page and copies over the data from the consecutive pages
 * over to the huge page.
 * Finally, it sets the PDE to point to the huge page with the flags shared
 * between the previous pages.
 *
 * Hint: don't forget to free the page table and the previously used pages.
 * Hint: be very careful about your order of operations. What happens when you
 *    are asked to merge pages from the buddy allocator (KPAGES) and you start
 *    freeing its old memory? This is prone to race conditions!
 */
 int ptbl_merge(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
	{
		/* LAB 2: your code here. */

		if( !(*entry & PAGE_PRESENT) || *entry & PAGE_HUGE) {
			return 0;
		}
		//return 0;
		struct page_table *ptbl = (struct page_table *)KADDR(PAGE_ADDR(*entry));
		struct page_info *pt = pa2page(PAGE_ADDR(*entry));
		for(size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
			if(!(ptbl->entries[i] & PAGE_PRESENT)) {
				return 0;
			}
			if( (ptbl->entries[i] & PAGE_UMASK) != (ptbl->entries[0] & PAGE_UMASK)) {
				return 0;
			}
		}
		struct page_info *huge_page = page_alloc(ALLOC_HUGE|ALLOC_ZERO);
		huge_page->pp_ref++;
		uint64_t flags = (ptbl->entries[0] & PAGE_UMASK);
		for(size_t i = 0; i < PAGE_TABLE_ENTRIES; i++){
			struct page_info *page = pa2page(PAGE_ADDR(ptbl->entries[i]));
			memcpy(page2kva(huge_page) + i * PAGE_SIZE, page2kva(page), PAGE_SIZE);
		}
		*entry = page2pa(huge_page) | flags | PAGE_HUGE| PAGE_PRESENT;
		for(size_t i = 0; i < PAGE_TABLE_ENTRIES; i++){
			tlb_invalidate((struct page_table*)read_cr3(), (void *)(base + i * PAGE_SIZE));
		}
		for(size_t i = 0; i < PAGE_TABLE_ENTRIES; i++){
			struct page_info *page = pa2page(PAGE_ADDR(ptbl->entries[i]));
			// page_free(page);
			page_decref(page);
		}
		page_decref(pt);
		return 0;
}

/* Frees up the page table by checking if all entries are clear. Returns if no
 * page table is present. Otherwise this function checks every entry in the
 * page table and frees the page table if no entry is set.
 *
 * Hint: this function calls pa2page(), page2kva() and page_decref().
 */
int ptbl_free(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	/* LAB 2: your code here. */
	if ((*entry & PAGE_HUGE) && !(*entry & PAGE_PRESENT)) {
		return 0;
	}

	struct page_info *page = pa2page(PAGE_ADDR(*entry));
	struct page_table *page_table = (struct page_table *)page2kva(page);

	for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		if (page_table->entries[i] & PAGE_PRESENT) {
			return 0;
		}
	}

	page_decref(pa2page(PAGE_ADDR(*entry)));
	*entry = 0;
	
	return 0;
}
