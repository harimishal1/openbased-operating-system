#include <assert.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

static int run_test() {
	struct page_info *page;

	size_t nfree = count_total_free_pages();

	// Allocating many pages introduces new page table pages as well,
	// so we calculate how many pages are needed for that
	size_t table_count = (nfree + PAGE_TABLE_ENTRIES - 1) / PAGE_TABLE_ENTRIES;
	nfree -= table_count;

	// Allocate all pages in the system
	for(size_t i = 0; i < nfree; i++) {
		// Sometimes map a huge page; to test both direct HPage and THP
		bool alloc_huge = i % 1024 == 0 && nfree > 512;
		
		page = page_alloc(alloc_huge ? ALLOC_HUGE : 0);

		// If huge page fails due to fragmentation, retry small pages
		if(page == NULL && alloc_huge) {
			alloc_huge = false;
			page = page_alloc(0);
		}

		assert(page != NULL);
		assert(page_insert(kernel_pml4, page, (void *)(i * PAGE_SIZE), PAGE_PRESENT | PAGE_WRITE) == 0);
		
		// Find the current backing page again; THP merge might pull it our from under us
		page = page_lookup(kernel_pml4, (void *)(i * PAGE_SIZE), NULL);
		assert(page->pp_ref == 1);
		assert(!page->pp_free);

		if(alloc_huge)
			i += 511; // the 512th is in the for step
	}

	dump_page_tables_range(kernel_pml4, PAGE_HUGE, 0, (void *) USER_LIM);

	// Write a pattern to all these pages
	memset(0, 0xBF, nfree * PAGE_SIZE);

	// Check that the pattern holds up
	for(size_t i = 0; i < nfree * PAGE_SIZE; i++)
		assert(*((uint8_t *)i) == 0xBF);

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
