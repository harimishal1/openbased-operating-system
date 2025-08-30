#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();

static int run_test() {
	size_t free_pages = count_total_free_pages();

	// Allocate as many pages as are free
	for (size_t i = 0; i < free_pages; i++)
		assert(page_alloc(0) != NULL);

	// Allocate one more, should return NULL
	assert(page_alloc(0) == NULL);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
