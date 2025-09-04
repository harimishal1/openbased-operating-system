#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

static uintptr_t pte_last = 0, pde_last = 0, pdpte_last = 0, pml4e_last = 0;
static char pte_next = 0, pde_next = 0, pdpte_next = 0, pml4e_next = 0;

static void reset_state() {
	pte_last = pde_last = pdpte_last = pml4e_last = 0;
	pte_next = pde_next = pdpte_next = pml4e_next = 0;
}

static uintptr_t sign_extend(uintptr_t addr)
{
	return (addr < USER_LIM) ? addr : (0xffff000000000000ull | addr);
}

#define PTBL_CALLBACK 0
#define PTBL_UNMAP 1
#define PTBL_HOLE 2

static int pte_callback(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker) {
	assert(pte_next == PTBL_CALLBACK);
	assert(base == sign_extend(pte_last + 1) || pte_last == 0);
	assert(end - base == PAGE_SIZE - 1);

	if(!(*entry & PAGE_PRESENT)) {
		pte_next = PTBL_HOLE;
	} else {
		pte_last = end;
		pte_next = PTBL_CALLBACK;
	}

	return 0;
}

static int pde_callback(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker) {
	assert(pde_next == PTBL_CALLBACK);
	assert(base == sign_extend(pde_last + 1) || pde_last == 0);
	assert(end - base == PAGE_TABLE_SPAN - 1);

	if(!(*entry & PAGE_PRESENT)) {
		pde_next = PTBL_HOLE;
	} else {
		pde_next = PTBL_UNMAP;
	}

	return 0;
}

static int pdpte_callback(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker) {
	assert(pdpte_next == PTBL_CALLBACK);
	assert(base == sign_extend(pdpte_last + 1) || pdpte_last == 0);
	assert(end - base == PAGE_DIR_SPAN - 1);
	
	if(!(*entry & PAGE_PRESENT)) {
		pdpte_next = PTBL_HOLE;
	} else {
		pdpte_next = PTBL_UNMAP;
	}

	return 0;
}

static int pml4_callback(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker) {
	assert(pml4e_next == PTBL_CALLBACK);
	assert(base == sign_extend(pml4e_last + 1) || pml4e_last == 0);
	assert(end - base == PDPT_SPAN - 1);
	
	if(!(*entry & PAGE_PRESENT)) {
		pml4e_next = PTBL_HOLE;
	} else {
		pml4e_next = PTBL_UNMAP;
	}

	return 0;
}

static int pde_unmap(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker) {
	assert(pde_next == PTBL_UNMAP);
	assert(base > pde_last);
	assert(pte_last <= end);
	assert(end - base == PAGE_TABLE_SPAN - 1);

	if((*entry & PAGE_PRESENT) && !(*entry & PAGE_HUGE)) {
		assert(pte_last == end);
	}

	pde_next = PTBL_CALLBACK;
	pde_last = end;
	pte_last = end;

	return 0;
}

static int pdpte_unmap(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker) {
	assert(pdpte_next == PTBL_UNMAP);
	assert(base > pdpte_last);
	assert(pde_last <= end);
	assert(end - base == PAGE_DIR_SPAN - 1);

	if(*entry & PAGE_PRESENT) {
		assert(pde_last == end);
	}

	pdpte_next = PTBL_CALLBACK;
	pdpte_last = end;
	pde_last = end;
	pte_last = end;

	return 0;
}

static int pml4_unmap(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker) {
	assert(pml4e_next == PTBL_UNMAP);
	assert(base > pml4e_last);
	assert(pdpte_last <= end);
	assert(end - base == PDPT_SPAN - 1);

	if(*entry & PAGE_PRESENT) {
		assert(pdpte_last == end);
	}

	pml4e_next = PTBL_CALLBACK;
	pml4e_last = end;
	pdpte_last = end;
	pde_last = end;
	pte_last = end;

	return 0;
}

static int pt_hole_callback(uintptr_t base, uintptr_t end, struct page_walker *walker) {
	if(end - base == PAGE_SIZE - 1) {
		assert(pte_next == PTBL_HOLE);
		assert(base > pte_last);

		pte_last = end;
		pte_next = PTBL_CALLBACK;
	} else if(end - base == PAGE_TABLE_SPAN - 1) {
		assert(pde_next == PTBL_HOLE);
		assert(base > pde_last);

		pte_last = end;
		pde_last = end;
		pde_next = PTBL_CALLBACK;
	} else if(end - base == PAGE_DIR_SPAN - 1) {
		assert(pdpte_next == PTBL_HOLE);
		assert(base > pdpte_last);

		pte_last = end;
		pde_last = end;
		pdpte_last = end;
		pdpte_next = PTBL_CALLBACK;
	} else if(end - base == PDPT_SPAN - 1) {
		assert(pml4e_next == PTBL_HOLE);
		assert(base > pml4e_last || base == 0);

		pte_last = end;
		pde_last = end;
		pdpte_last = end;
		pml4e_last = end;
		pml4e_next = PTBL_CALLBACK;
	} else {
		panic("Invalid hole size!");
	}

	return 0;
}

static int run_test() {
	int ret;

	struct page_walker walker = {
		.pte_callback = pte_callback,
		.pde_callback = pde_callback,
		.pdpte_callback = pdpte_callback,
		.pml4e_callback = pml4_callback,
		.pde_unmap = pde_unmap,
		.pdpte_unmap = pdpte_unmap,
		.pml4e_unmap = pml4_unmap,
		.pt_hole_callback = pt_hole_callback,
	};

	reset_state();
	walk_all_pages(kernel_pml4, &walker);

	reset_state();
	walk_kernel_pages(kernel_pml4, &walker);

	reset_state();
	walk_page_range(kernel_pml4, (void *)KPAGES, (void *)KERNEL_LIM, &walker);

	reset_state();
	walk_page_range(kernel_pml4, (void *)(KERNEL_VMA + 5 * PAGE_SIZE), (void *)(KERNEL_LIM - 43 * HPAGE_SIZE), &walker);

	return __checksum__;
}

extern void halt_kernel();
struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
