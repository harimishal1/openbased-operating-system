#include <assert.h>
#include <list.h>
#include <paging.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();
extern struct list buddy_free_list[];

static int run_test() {
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nfree_basemem = 0;
	size_t nfree_extmem = 0;

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		list_foreach(buddy_free_list + order, node) {
			page = container_of(node, struct page_info, pp_node);

			if (page2pa(page) < EXT_PHYS_MEM) {
				++nfree_basemem;
			} else {
				++nfree_extmem;
			}
		}
	}

	assert(nfree_basemem > 0);
	assert(nfree_extmem > 0);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
