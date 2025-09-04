#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern struct list buddy_free_list[];

static int run_rest() {
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nviolations = 0;

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		list_foreach(buddy_free_list + order, node) {
			page = container_of(node, struct page_info, pp_node);

			if (page->pp_order != order)
				++nviolations;
		}
	}

	if (nviolations != 0) {
		panic("found %u order violations in free list", nviolations);
	}

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_rest,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
