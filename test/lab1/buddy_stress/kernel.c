#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "buddy_consistency.h"

extern void halt_kernel();

// We have a static list to keep track of all allocations
#define ALLOC_MAX 1024

static struct page_info *alloc_pages[ALLOC_MAX];
static size_t alloc_pages_index = 0;

// Helper method to perform the given number of allocations and
// frees, and check for buddy consistency after this has finished
static void alloc_and_free(size_t allocs, size_t frees) {
	size_t free_pages = count_total_free_pages();
	assert(free_pages > allocs);
	
	size_t end_alloc_pages = alloc_pages_index + allocs - frees;
	assert(end_alloc_pages >= 0 && end_alloc_pages < ALLOC_MAX);

	size_t expected_free_pages = free_pages - allocs + frees;

	for(size_t i = 0; i < allocs; i++) {
		struct page_info *alloc = page_alloc(0);
		assert(alloc != NULL);
		alloc_pages[alloc_pages_index++] = alloc;
	}

	for(size_t i = 0; i < frees; i++) {
		page_free(alloc_pages[--alloc_pages_index]);
	}

	free_pages = count_total_free_pages();
	assert(free_pages == expected_free_pages);

	check_buddy_consistency_bootmap();
}

static int run_test() {
	alloc_and_free(10, 6);
	alloc_and_free(890, 432);
	alloc_and_free(432, 234);
	alloc_and_free(100, 760);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
