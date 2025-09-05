#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

static int check_flags(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	uint64_t flags;

	(void)walker;

	if (!(*entry & PAGE_PRESENT) || (*entry & PAGE_HUGE)) {
		return 0;
	}

	flags = *entry & PAGE_MASK;

	if (flags != (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)) {
		panic("%p points to a page table with the wrong "
		    "permissions!\n", base);
	}

	return 0;
}

static int run_test() {
	struct page_walker walker = {
		.pde_callback = check_flags,
		.pdpte_callback = check_flags,
		.pml4e_callback = check_flags,
	};

	walk_all_pages(kernel_pml4, &walker);

	return __checksum__;
}

extern void validate_pml4();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = validate_pml4,
	.should_continue = false,
	.checksum = __checksum__,
};
