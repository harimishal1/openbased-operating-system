#include <assert.h>
#include <list.h>
#include <paging.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();
extern struct list buddy_free_list[];

static void do_split_merge(int flags) {
	struct list stolen_free_list[10];
	struct page_info *page;
	size_t order;
	size_t nfree_pages;

	/* Count the number of order 9 pages. */
	nfree_pages = count_free_pages(BUDDY_2M_PAGE);

	/* Allocate a order 9 chunk. */
	if (flags & ALLOC_HUGE)
		page = page_alloc(ALLOC_HUGE);
	else
		page = buddy_find(BUDDY_2M_PAGE);

	if (!page) {
		panic("can't allocate 2M page!");
	}

	/* Check against the count of huge pages. */
	assert(count_free_pages(BUDDY_2M_PAGE) + 1 == nfree_pages);

	/* Steal the lists of free pages. */
	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		stolen_free_list[order] = buddy_free_list[order];
		list_init(buddy_free_list + order);
	}

	/* Return the huge page. */
	page_free(page);

	/* Check if we have an order 9 chunk. */
	for (order = 0; order < BUDDY_2M_PAGE; ++order) {
		assert(count_free_pages(order) == 0);
	}

	assert(count_free_pages(BUDDY_2M_PAGE) == 1);

	/* Allocate a normal page. */
	page = page_alloc(0);

	if (!page) {
		panic("can't allocate 4K page!");
	}

	/* Check if we have a chunk of every order. */
	for (order = 0; order < BUDDY_2M_PAGE; ++order) {
		assert(count_free_pages(order) == 1);
	}

	assert(count_free_pages(BUDDY_2M_PAGE) == 0);

	/* Return the normal page. */
	page_free(page);

	/* Check if we have a huge page. */
	for (order = 0; order < BUDDY_2M_PAGE; ++order) {
		assert(count_free_pages(order) == 0);
	}

	assert(count_free_pages(BUDDY_2M_PAGE) == 1);

	/* Allocate an order 9 chunk again. */
	if (flags & ALLOC_HUGE)
		page = page_alloc(ALLOC_HUGE);
	else
		page = buddy_find(BUDDY_2M_PAGE);

	if (!page) {
		panic("can't allocate 2M page!");
	}

	/* Return the lists of free chunks. */
	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		buddy_free_list[order] = stolen_free_list[order];
	}

	/* Return the huge page. */
	page_free(page);

	if (flags & ALLOC_HUGE) {
		cprintf("[TESTS] Tested split and merge for huge pages\n");
	} else {
		cprintf("[TESTS] Tested split and merge for normal pages\n");
	}
}

static int run_test() {
	do_split_merge(0);
	do_split_merge(ALLOC_HUGE);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
