#include "buddy_consistency.h"

// Helper method to check the consistency of the buddy allocator
// at any point in time
void check_buddy_consistency(physaddr_t addr, size_t order, struct page_info *parent) {
	struct page_info *page;

	page = pa2page(addr);

	if (order > BUDDY_MAX_ORDER) {
		panic("page %p has order %lu which is larger than the maximum of %lu!", order, BUDDY_MAX_ORDER);
	}

	if (parent && parent != page) {
		if (page->pp_free || !list_is_empty(&page->pp_node) ||
			page->pp_order < parent->pp_order) {
			panic("page %p of order %u is free, while parent page "
				"%p of order %u is already free",
				page2pa(page), page->pp_order,
				page2pa(parent), parent->pp_order);
		}
	}

	if (page->pp_free && list_is_empty(&page->pp_node)) {
		panic("page %p of order %u is free, but not on the free list",
			page2pa(page), page->pp_order);
	}

	if (!page->pp_free && !list_is_empty(&page->pp_node)) {
		panic("page %p of order %u is in use, but on the free list",
			page2pa(page), page->pp_order);
	}

	if (page->pp_free && page->pp_ref > 0) {
		panic("page %p of order %u is free, but has a non-zero refcount %u",
			page2pa(page), page->pp_order, page->pp_ref);
	}

	if (!page->pp_avail && (page->pp_free || page->pp_ref > 0)) {
		panic("page %p of order %u is free or has a non-zero refcount, but is not available",
			page2pa(page), page->pp_order);
	}

	if (page->pp_free && list_is_empty(&page->pp_node)) {
		parent = page;
	}

	if (order == 0) {
		return;
	}

	--order;
	check_buddy_consistency(addr, order, parent);
	check_buddy_consistency(addr | (1 << (order + 12)), order, parent);
}

void check_buddy_consistency_bootmap() {
	for (physaddr_t addr = 0; addr < BOOT_MAP_LIM; addr += (1 << (BUDDY_MAX_ORDER + 12 - 1))) {
		check_buddy_consistency(addr, BUDDY_MAX_ORDER - 1, NULL);
	}
}
