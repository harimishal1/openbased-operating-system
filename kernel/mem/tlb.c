
#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

/* Invalidate a TLB entry, but only if the page tables being modified are the
 * ones currently in use by the processor.
 *
 * Hint: this function calls flush_page().
 */
void tlb_invalidate(struct page_table *pml4, void *va)
{
	/* Flush the entry only if we are modifying the current address space. */
	/* LAB 5: update your code here */
    if (pml4 == (struct page_table *)KADDR(read_cr3())) {
        flush_page(va);
    }
}
