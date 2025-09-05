#include <kernel/mem.h>
#include <kernel/test/test.h>

static int check_entry(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	if (PAGE_ADDR(*entry) > npages * PAGE_SIZE) {
		panic("region mapped at %p - %p points to non-existent physical memory!", base, end);
	}

	if (KERNEL_VMA <= base && end < KERNEL_VMA + 64ULL * 1024 * 1024 * 1024) {
		return 0;
	}

	if (KPAGES <= base) {
		return 0;
	}

	if (KSTACK_TOP - KSTACK_SIZE <= base && end < KSTACK_TOP) {
		return 0;
	}

	panic("region mapped at %p - %p is not the identity map, page info "
		"structs or the kernel stack!", base, end);

	return 0;
}

static int check_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	if ((end - base) != 0xfff) {
		panic("end [%llx] - base [%llx] should be 4kb - 1  bytes\n", end, base);
	}

	// Ignore readonly sections since that could be reserved areas from mmap
	if (!(*entry & PAGE_PRESENT) || !(*entry & PAGE_WRITE)) {
		return 0;
	}

	return check_entry(entry, base, end, walker);
}

static int check_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	if ((end - base) != 0x1FFFFF) {
		panic("end [%llx] - base [%llx] should be a 2m page - 1\n", end, base);
	}

	if (!(*entry & PAGE_PRESENT) || !(*entry & PAGE_HUGE) || !(*entry & PAGE_WRITE)) {
		return 0;
	}

	return check_entry(entry, base, end, walker);
}

static int run_test() {
	struct page_walker walker = {
		.pte_callback = check_pte,
		.pde_callback = check_pde,
	};

	walk_all_pages(kernel_pml4, &walker);

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
