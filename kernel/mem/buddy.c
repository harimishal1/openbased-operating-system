#include "kernel/mem/buddy.h"
#include "assert.h"
#include "stdio.h"
#include <types.h>
#include <list.h>
#include <paging.h>
#include <string.h>

#include <kernel/mem.h>

// #define DOUBLE_FREE_DETECTION
// #define INVALID_FREE_DETECTION

/* Physical page metadata. */
size_t npages;
struct page_info *pages;

/*
 * List of free buddy chunks (often also referred to as buddy pages or simply
 * pages). Each order has a list containing all free buddy chunks of the
 * specific buddy order. Buddy orders go from 0 to BUDDY_MAX_ORDER - 1
 */
struct list buddy_free_list[BUDDY_MAX_ORDER];

/*detects invalid free by marking interior pages of a higher order chunk as unavailable
 */

#ifdef INVALID_FREE_DETECTION
#define ORDER_SENTINEL 0x3F  // invalid as a real order

static inline void mark_interior_on_alloc(struct page_info *head, size_t order) {
    if (order == 0) return;
    size_t idx = (size_t)(head - pages);
    size_t n   = (size_t)1 << order;
    for (size_t i = 1; i < n; i++) {
        struct page_info *p = &pages[idx + i];
        p->pp_order = ORDER_SENTINEL;   
        p->pp_free  = 0;                
    }
}
#endif

/* Counts the number of free pages for the given order.
 */
size_t count_free_pages(size_t order)
{
	struct list *node;
	size_t nfree_pages = 0;

	if (order >= BUDDY_MAX_ORDER) {
		return 0;
	}

	list_foreach(buddy_free_list + order, node) {
		++nfree_pages;
	}

	return nfree_pages;
}

/* Shows the number of free pages in the buddy allocator as well as the amount
 * of free memory in kiB.
 *
 * Use this function to diagnose your buddy allocator.
 */
void show_buddy_info(void)
{
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nfree_pages;
	size_t nfree = 0;

	cprintf("Buddy allocator:\n");

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		nfree_pages = count_free_pages(order);

		cprintf("  order #%u pages=%u\n", order, nfree_pages);

		nfree += nfree_pages * (1 << (order + 12));
	}

	cprintf("  free: %u kiB\n", nfree / 1024);
}

/* Gets the total amount of free pages. */
size_t count_total_free_pages(void)
{
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nfree_pages;
	size_t nfree = 0;

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		nfree_pages = count_free_pages(order);
		nfree += nfree_pages * (1 << order);
	}

	return nfree;
}

/* Splits lhs into free pages until the order of the page is the requested
 * order req_order.
 *
 * The algorithm to split pages is as follows:
 *  - Given the page of order k, locate the page and its buddy at order k - 1.
 *  - Decrement the order of both the page and its buddy.
 *  - Mark the buddy page as free and add it to the free list.
 *  - Repeat until the page is of the requested order.
 *
 * Returns a page of the requested order.
 */
struct page_info *buddy_split(struct page_info *lhs, size_t req_order)
{  	
	/* LAB 1: your code here. */
	while(lhs->pp_order > req_order) {

		struct page_info *buddy;
		size_t idx = lhs - pages, buddy_idx;

		buddy_idx = idx ^ (1ULL << (lhs->pp_order - 1));
		buddy = pages + buddy_idx;

		lhs->pp_order--;

		buddy->pp_order = lhs->pp_order;
		buddy->pp_free = 1;
		list_add_tail(&buddy_free_list[buddy->pp_order], &buddy->pp_node);
	}
	lhs->pp_free = 0;
	return lhs;
}

/* Merges the buddy of the page with the page if the buddy is free to form
 * larger and larger free pages until either the maximum order is reached or
 * no free buddy is found.
 *
 * The algorithm to merge pages is as follows:
 *  - Given the page of order k, locate the page with the lowest address
 *    and its buddy of order k.
 *  - Check if both the page and the buddy are free and whether the order
 *    matches.
 *  - Remove the page and its buddy from the free list.
 *  - Increment the order of the page.
 *  - Repeat until the maximum order has been reached or until the buddy is not
 *    free.
 *
 * Returns the largest merged free page possible.
 */
struct page_info *buddy_merge(struct page_info *page)
{	
	/* LAB 1: your code here. */
	while (page->pp_order < BUDDY_MAX_ORDER - 1) {
		struct page_info *buddy;
		struct page_info *tmp;

		size_t idx = page - pages, buddy_idx;
		buddy_idx = idx ^ (1ULL << (page->pp_order));
		buddy = pages + buddy_idx;

		if(buddy->pp_avail == 0 || buddy->pp_ref > 0) {
			break;
		}
		if (buddy->pp_free == 0 || buddy->pp_order != page->pp_order) {
			break;
		}
		list_del(&buddy->pp_node);
		if (page > buddy) {
			tmp = page;
			page = buddy;
			buddy = tmp;
		}

		buddy->pp_free = 0;
		page->pp_order++;
	}
	return page;
}

/* Given the order req_order, attempts to find a page of that order or a larger
 * order in the free list. In case the order of the free page is larger than the
 * requested order, the page is split down to the requested order using
 * buddy_split().
 *
 * Returns a page of the requested order or NULL if no such page can be found.
 */
struct page_info *buddy_find(size_t req_order)
{	
	/* LAB 1: your code here. */
	struct list *node;
	size_t order;

	for(order = req_order; order < BUDDY_MAX_ORDER; order++) {
		if(!list_is_empty(buddy_free_list + order)) {
			node = list_pop(buddy_free_list + order);
			struct page_info *page = container_of(node, struct page_info, pp_node);
			return buddy_split(page, req_order);
		}
	}
	return NULL;
}

/*
 * Allocates a physical page.
 *
 * if (alloc_flags & ALLOC_ZERO), fills the entire returned physical page with
 * '\0' bytes.
 * if (alloc_flags & ALLOC_HUGE), returns a huge physical 2M page.
 *
 * Beware: this function does NOT increment the reference count of the page -
 * this is the caller's responsibility.
 *
 * Returns NULL if out of free memory.
 *
 * Hint: use buddy_find() to find a free page of the right order.
 * Hint: use page2kva() and memset() to clear the page.
 */
struct page_info *page_alloc(int alloc_flags)
{
	/* LAB 1: your code here. */
	size_t req_order = 0;
	if (alloc_flags & ALLOC_HUGE) {
		req_order = BUDDY_2M_PAGE;
	}

    if (req_order >= BUDDY_MAX_ORDER)
        return NULL;

    struct page_info *page = buddy_find(req_order);
    if (!page) return NULL; 

	#ifdef INVALID_FREE_DETECTION
	mark_interior_on_alloc(page, req_order); //we check for higher order chunk free here
	#endif
    
	if (alloc_flags & ALLOC_ZERO) { 
        size_t bytes = (size_t)1ULL << (PAGE_TABLE_SHIFT + req_order);
        memset(page2kva(page), 0, bytes);
    }
    return page;
}

/*
 * Return a page to the free list.
 * This function should only be called when pp->pp_ref reaches 0.
 * Pages can only be freed when they are available.
 *
 * Hint: mark the page as free and use buddy_merge() to merge the free page
 * with its buddies before returning the page to the free list.
 */
void page_free(struct page_info *pp)
{	
	/* LAB 1: your code here. */
	//double free detection
	#ifdef DOUBLE_FREE_DETECTION
	assert(pp->pp_avail == 1);   
    assert(pp->pp_ref == 0); 
	if(pp->pp_free == 1){
		panic("Double free detected");
	}	
	pp->pp_free = 0;
	#endif
	//double free detection

	//invalid free detection
	#ifdef INVALID_FREE_DETECTION
	if (pp->pp_order == ORDER_SENTINEL){
		panic("invalid free: interior (non-head) page");
	}
	#endif
	//invalid free detection

	//cprintf("Freeing page %p of order %d\n", page2pa(pp), pp->pp_order);
   	pp = buddy_merge(pp);
	pp->pp_free = 1;
	list_add_tail(&buddy_free_list[pp->pp_order], &pp->pp_node);
}

/*
 * Decrement the reference count on a page, freeing it if there
 * are no more refs.
 */
void page_decref(struct page_info *pp)
{
	assert(pp->pp_ref > 0);
	if (--pp->pp_ref == 0) {
		page_free(pp);
	}
}
static int in_page_range(void *p)
{
	return ((uintptr_t)pages <= (uintptr_t)p &&
	        (uintptr_t)p < (uintptr_t)(pages + npages));
}

static void *update_ptr(void *p)
{
	if (!in_page_range(p))
		return p;

	return (void *)((uintptr_t)p + KPAGES - (uintptr_t)pages);
}

void buddy_migrate(void)
{
	struct page_info *page;
	struct list *node;
	size_t i;

	for (i = 0; i < npages; ++i) {
		page = pages + i;
		node = &page->pp_node;

		node->next = update_ptr(node->next);
		node->prev = update_ptr(node->prev);
	}

	for (i = 0; i < BUDDY_MAX_ORDER; ++i) {
		node = buddy_free_list + i;

		node->next = update_ptr(node->next);
		node->prev = update_ptr(node->prev);
	}

	pages = (struct page_info *)KPAGES;
}

/**
 * Increase the range of pages managed by our kernel in the pages[] array. This
 * method will increase npages by chunks of size <the number of pages in a
 * max_order page>.
 *
 * This method expects a size parameter representing the requested minimum size
 * of npages. npages is then increased to the first multiple of the chunk size
 * strictly larger than size, so that pages[size] is a valid page.
 */
int buddy_grow(struct page_table *pml4, size_t size)
{
	// We grow the page scope of the buddy allocator by an entire MAX_ORDER
	// worth of pages every time. Compute how many pages this corresponds to,
	// and how many pages are needed to store that many page_info structs.
	size_t increment_structs = (1 << (12 + BUDDY_MAX_ORDER - 1)) / PAGE_SIZE;
	size_t increment_pages = ROUNDUP(increment_structs * sizeof(struct page_info), PAGE_SIZE) / PAGE_SIZE;

	// Continuously grow the buddy allocator while the current size is less
	// than the requested size.
	while(npages <= size) {
		struct page_info *pages_end = pages + npages;

		// Allocate pages for the new structs
		for (size_t i = 0; i < increment_pages; i++) {
			struct page_info *page = page_alloc(ALLOC_ZERO);
			if (!page)
				return -1; // We ran out of memory
			
			// Ensure the page is mapped in the correct location: after the
			// existing pages array
			int ret = page_insert(pml4, page, (char *) pages_end + i * PAGE_SIZE,
			    PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC);
			if (ret < 0)
				return ret;
		}

		// Initialise newly allocated page_info structs. Crucially, we do NOT
		// hand these new pages to the buddy allocator through page_free, since
		// we cannot know whether they are actually free or available. This is
		// the job of page_init_ext().
		for(size_t i = 0; i < increment_structs; i++) {
			struct page_info *info = pages_end + i;
			list_init(&info->pp_node);
		}

		// Update npages size
		npages += increment_structs;
	}

	return 0;
}
