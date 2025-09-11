#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

static int ismemset(void *s, int c, size_t n)
{
	unsigned char *p = s;
	size_t i;

	for (i = 0; i < n; ++i, ++p) {
		if (*p != c) {
			return 0;
		}
	}

	return 1;
}

static int run_test() {
	struct page_info *page, *ret;
	physaddr_t *entry;
	char *addr, *data;
	size_t i, k;
	size_t nfree;

	/* Remember the amount of free pages. */
	nfree = count_total_free_pages();

	/* Allocate a 2M page. */
	page = page_alloc(ALLOC_HUGE);

	if (!page) {
		panic("cannot allocate 2M page!");
	}

	/* Fill up page. */
	data = page2kva(page);

	for (i = 0; i < 512; ++i) {
		memset(data, i & 0xff, PAGE_SIZE);
		data += PAGE_SIZE;
	}

	/* Misaligned insert should fail. */
	assert(page_insert(kernel_pml4, page, (void *)PAGE_SIZE, PAGE_PRESENT) != 0);

	/* Insert the page. */
	assert(page_insert(kernel_pml4, page, 0, PAGE_PRESENT) == 0);
	assert(page->pp_ref == 1);
	assert(page->pp_order == BUDDY_2M_PAGE);
	assert(!page->pp_free);
	assert(kernel_pml4->entries[0] != 0);

	/* Look up the page. */
	ret = page_lookup(kernel_pml4, 0, &entry);
	assert((*entry & PAGE_MASK) == (PAGE_PRESENT | PAGE_HUGE));
	assert(PAGE_ADDR(*entry) == page2pa(page));
	assert(ret == page);

	/* Change protections of the 4K page at 0x1000. */
	protect_region(kernel_pml4, (void *)PAGE_SIZE, PAGE_SIZE, PAGE_PRESENT | PAGE_WRITE);

	/* Look up the other 4K pages. */
	addr = NULL;

	for (i = 0; i < PAGE_TABLE_ENTRIES; ++i, addr += PAGE_SIZE) {
		entry = NULL;
		ret = page_lookup(kernel_pml4, addr, &entry);

		assert(ret);
		assert(entry);
		if (addr == (void *)PAGE_SIZE) {
			assert((*entry & PAGE_MASK) == (PAGE_PRESENT | PAGE_WRITE));
		} else {
			assert((*entry & PAGE_MASK) == PAGE_PRESENT);
		}
		page = pa2page(PAGE_ADDR(*entry));
		assert(page->pp_order == 0);
		assert(!page->pp_free);
		assert(page->pp_ref == 1);
		assert(list_is_empty(&page->pp_node));

		if (!ismemset(addr, (i & 0xff), PAGE_SIZE)) {
			panic("page %p is corrupt", addr);
		}
	}

	/* Remove the 4K page */
	unmap_page_range(kernel_pml4, (void *)PAGE_SIZE, PAGE_SIZE);

	/* Populate a page at 0x1000. */
	populate_region(kernel_pml4, (void *)PAGE_SIZE, PAGE_SIZE, PAGE_PRESENT);

	/* Look up the page. */
	page_lookup(kernel_pml4, 0, &entry);
	assert((*entry & PAGE_MASK) == (PAGE_PRESENT | PAGE_HUGE));

	/* Remove the page. */
	unmap_page_range(kernel_pml4, 0, HPAGE_SIZE);
	assert(!page_lookup(kernel_pml4, 0, NULL));

	/* Check if the page tables have been cleaned up. */
	assert(kernel_pml4->entries[0] == 0);

	/* Check if we leaked memory. */
	assert(nfree == count_total_free_pages());

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
