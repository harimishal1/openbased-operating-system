#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

static int check_pte_wx(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	uint64_t flags;

	(void)walker;

	if (!(*entry & PAGE_PRESENT)) {
		return 0;
	}

	flags = *entry & (PAGE_WRITE | PAGE_NO_EXEC);

	if (flags == PAGE_WRITE) {
		panic("%p is mapped as write executable!\n", base);
	}

	return 0;
}

static int check_pde_wx(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	uint64_t flags;

	(void)walker;

	if (!(*entry & PAGE_PRESENT) || !(*entry & PAGE_HUGE)) {
		return 0;
	}

	flags = *entry & (PAGE_WRITE | PAGE_NO_EXEC);

	if (flags == PAGE_WRITE) {
		panic("%p is mapped as write executable!\n", base);
	}

	return 0;
}

static int run_test() {
	struct page_walker walker = {
		.pte_callback = check_pte_wx,
		.pde_callback = check_pde_wx,
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
