#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

static bool should_invalidate = false;
static bool has_invalidated = false;
static physaddr_t *invalidate_entry = NULL;
static physaddr_t entry_cache = 0;
static void tlb_invalidate_handler(struct probe_frame *frame) {
	if(!should_invalidate || !invalidate_entry)
		return;

	assert(*invalidate_entry != entry_cache);
	has_invalidated = true;
}

static int run_test() {
	struct page_info *page, *ret;
	physaddr_t *entry;
	size_t nfree;

	/* Remember the amount of free pages. */
	nfree = count_total_free_pages();

	/* Allocate a 4K page. */
	page = page_alloc(0);

	if (!page) {
		panic("cannot allocate 4K page!");
	}

	/* Insert the page. */
	assert(page_insert(kernel_pml4, page, 0, PAGE_PRESENT) == 0);
	
	assert(page->pp_ref == 1);
	assert(!page->pp_free);
	assert(kernel_pml4->entries[0] != 0);
	
	/* Look up the page. */
	ret = page_lookup(kernel_pml4, 0, &entry);
	assert((*entry & PAGE_MASK) == PAGE_PRESENT);
	assert(PAGE_ADDR(*entry) == page2pa(page));
	assert(ret == page);
	
	/* Re-insert the page at the same address */
	should_invalidate = true;
	invalidate_entry = entry;
	entry_cache = *entry;
	
	assert(page_insert(kernel_pml4, page, 0, PAGE_PRESENT) == 0);
	assert(page->pp_ref == 1);
	assert(!page->pp_free);
	assert(kernel_pml4->entries[0] != 0);
	
	assert(has_invalidated);
	
	/* Remove the page. */
	cprintf("---- Removing page ----\n");
	page_remove(kernel_pml4, 0);
	assert(page->pp_free);
	assert(!page_lookup(kernel_pml4, 0, NULL));
	
	/* Check if the page tables have been cleaned up. */
	assert(kernel_pml4->entries[0] == 0);
	
	/* Check if we leaked memory. */
	cprintf("Free pages: %u\n", 1);
	assert(nfree == count_total_free_pages());

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = page_init_ext,
	.should_continue = false,
	.checksum = __checksum__,

	.probe_count = 1,
	.probes = {
		{
			.target = tlb_invalidate,
			.callback = tlb_invalidate_handler,
		},
	},
};
