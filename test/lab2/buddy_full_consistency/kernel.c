#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "buddy_consistency.h"

static int run_test() {
	struct page_info *page;
	physaddr_t addr;

	for (addr = 0;
	     addr < npages * PAGE_SIZE;
	     addr += PAGE_SIZE) {

		struct page_info *page = pa2page(addr);
		
		// check that the buddy metadata page is mapped
		struct page_info *metadata = page_lookup(kernel_pml4, pa2page(addr), NULL);
		if (metadata == NULL) {
			panic("Page_info structure number %d is not mapped\n", pa2page(addr) - pages);
		}

		// Retrieve the page table entry
		physaddr_t *entry;
		struct page_info *mapped = page_lookup(kernel_pml4, KADDR(addr), &entry);

		// If the page is not available, the page tables should either
		// not contain it, or have it read-only
		if(!page->pp_avail) {
			if (!(mapped == NULL || !(*entry & PAGE_WRITE))) {
				panic("Kernel address %p should not be mapped, or only read-only\n", KADDR(addr));
			}
			continue;
		}

		// If the page is available, it should be mapped read-write
		if (mapped == NULL) {
			panic("Kernel address %llx should be mapped\n", KADDR(addr));
		}

		// check consistency of the buddy metadata page
		check_buddy_consistency(page2pa(metadata), metadata->pp_order, NULL);

		// check consistency of the actual page.
		check_buddy_consistency(addr, page->pp_order, NULL);
	}

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
