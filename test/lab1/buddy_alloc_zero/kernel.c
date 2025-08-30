#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static struct page_info *alloc_assert_zero(bool huge) {
	int flags = ALLOC_ZERO | (huge ? ALLOC_HUGE : 0);
	struct page_info *page = page_alloc(flags);

	for(int i = 0; i < (huge ? HPAGE_SIZE : PAGE_SIZE); i++) {
		assert(*((char *) page2kva(page) + i) == 0);
	}

	return page;
}

static void write_pattern(struct page_info *page) {
	memset(page2kva(page), 0xAB, 1 << (page->pp_order + 12));
}

static void alloc_free_cycle(bool huge) {
	struct page_info *alloc = alloc_assert_zero(huge);
	write_pattern(alloc);
	page_free(alloc);
}

static int run_test() {
	// Perform allocations with ALLOC_ZERO and check that returned
	// memory has been zeroed out. We repeat this 30 times to weed
	// out any flukes w.r.t. not returning the same page properly.
	for(int i = 0; i < 30; i++) {
		alloc_free_cycle(false);
		alloc_free_cycle(true);
	}

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
