#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "buddy_consistency.h"

static int run_test() {
	struct page_info *page;
	physaddr_t addr;

	for (addr = 0;
	     addr < npages * PAGE_SIZE;
	     addr += (1 << (BUDDY_MAX_ORDER + 12 - 1))) {
		if (!page_lookup(kernel_pml4, pa2page(addr), NULL)) {
			continue;
		}

		check_buddy_consistency(addr, BUDDY_MAX_ORDER - 1, NULL);
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
