#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#define RSVD_BITS_MASK 0x000F000000000000
#define RSVD_BITS_MASK_CR3 0xFFFF000000000000

static int check_pte_wx(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	if (*entry & PAGE_PRESENT) {
		assert((*entry & RSVD_BITS_MASK) == 0);
	}

	return 0;
}

static int check_pde_wx(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	if (*entry & PAGE_PRESENT) {
		assert((*entry & RSVD_BITS_MASK) == 0);
	}

	return 0;
}

static int run_test() {
	struct page_walker walker = {
		.pte_callback = check_pte_wx,
		.pde_callback = check_pde_wx,
	};

	assert((read_cr3() & RSVD_BITS_MASK_CR3) == 0);

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
