#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "rand.h"

#define MISALIGNED_PROB ((double) 0.3)
#define PAGE_REMOVAL_PROB ((double) 0.3)
#define PAGE_REGAIN_PROB ((double) 0.4)
#define PAGE_DIFF_FLAGS_PROB ((double) 0.1)
#define RANDOM_PATTERNS_COUNT 16

#define MERGE_FLAGS PAGE_UMASK
#define NO_MERGE_FLAGS PAGE_UMASK & (~PAGE_USER)

static uint8_t random_patterns[RANDOM_PATTERNS_COUNT];

/*
 * Assert that a pattern of a 4k page pointed to by 'va' has the expected
 * pattern! A pattern is a random byte written to all the bytes in the page.
 * These random bytes can be found in the 'random_patterns' array. A pattern is
 * assigned by taking the index of the normal page inside its parent huge page.
 * So if we have the huge page at adress 0x200000 then the 4k page at adress
 * 0x202000 is at index 2 in the huge page. Then the pattern byte of page
 * 0x202000 is random_patterns[2 % RANDOM_PATTERNS_COUNT].
*/
static void assert_page_matches_pattern(void* va){
	va = ROUNDDOWN(va, PAGE_SIZE); 

	int pindex = ((uintptr_t)va - ROUNDDOWN((uintptr_t) va, HPAGE_SIZE)) / PAGE_SIZE;
	uint8_t pattern = random_patterns[pindex % RANDOM_PATTERNS_COUNT];
	uint8_t* data = (uint8_t*) va;

	for (int i = 0; i < PAGE_SIZE; i++){
		if(data[i] != pattern){
			panic("pattern doesn't match for adress range [%p - %p], problematic adress is %p", va, va + PAGE_SIZE - 1, va + i);
		}
	}
}

struct valid_split_info {
	uintptr_t hpage_base;
	size_t unmapped_size;
	bool choose_flags;
};

static int assert_valid_split_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
	struct page_walker *walker){
	
	uint64_t flags = *entry & PAGE_UMASK;
	if (flags != (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)) {
		panic("pde entry at va:%p doesn't have the expected ptbl permissions!\n");
	}
	return 0;
}

static int assert_valid_split_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
	struct page_walker *walker){

	if (!(*entry & PAGE_PRESENT)){
		return 0;
	}

	struct valid_split_info *vsi = (struct valid_split_info*) walker->udata;

	uint64_t flags = MERGE_FLAGS;
	uint64_t en_flags = *entry & PAGE_UMASK;
	if (vsi->choose_flags && base < vsi->hpage_base + vsi->unmapped_size){
		flags = random_decide(1 - PAGE_DIFF_FLAGS_PROB) ? MERGE_FLAGS : NO_MERGE_FLAGS;
	}

	if (en_flags != flags){
		panic("flags after split for 4K page dont match th expected flags, expected: %lx, got: %lx", flags, en_flags);
	}

	assert_page_matches_pattern((void*) base);

	return 0;
}

/*
 * The following test has 2 phases.
 *
 * 1. Allocate as many huge pages as possible and map them. With probability
 *    MISALIGNED_PROB the insertion may be misaligned - the failed insert will
 *    then be retried with proper alignment
 *
 * 2. Go over the allocated huge pages and do the following:
 *
 *  2.1. With probability PAGE_REMOVAL_PROB, remove a random number of pages
 *  from the huge page to force a split of the huge page; then assert the split
 *  is proper.
 *
 *  2.2. With probability PAGE_REGAIN_PROB, for a huge page that has been split,
 *  give all the normal pages back to the huge page, and then for each of those
 *  pages:
 *
 *   2.2.1: With probability PAGE_DIFF_FLAGS_PROB, the normal page will get
 *   different flags than the original huge page, forcing the merge to be
 *   impossible
 *
 *  2.3. If a merge should happen after re-insertion (all flags are the same of
 *  all the 4k pages), assert that the merge took place properly - otherwise,
 *  assert that the huge page is still split.
 *
 * In addition, each 4k page is filled with a pattern that is checked to detect
 * corruption or improper mappings.
 */
static int run_test() {
	size_t huge_pages_count = count_free_pages(BUDDY_2M_PAGE) / 2;

	for (int i = 0; i < RANDOM_PATTERNS_COUNT; i++){
		random_patterns[i] = random_uint_bounded(0xFF + 1);
	}

	struct page_info *page;
	void* va = NULL;
	struct page_info* test_pages[huge_pages_count];

	// Allocate, map and setup huge_pages_count
	for (int i = 0; i < huge_pages_count; i++, va += HPAGE_SIZE){
		page = page_alloc(ALLOC_HUGE);
		assert(page != NULL);

		// Randomly misalign one of the pages at a random misalingment
		if (random_decide(MISALIGNED_PROB)){
			uint64_t misalginment = PAGE_SIZE * random_uint_bounded(512);
			misalginment = misalginment == 0 ? PAGE_SIZE : misalginment;

			assert(page_insert(kernel_pml4, page, va + misalginment, MERGE_FLAGS) != 0);
		}

		assert(page_insert(kernel_pml4, page, va, MERGE_FLAGS) == 0);

		physaddr_t *entry;
		assert(page_lookup(kernel_pml4, va, &entry) != NULL);
		assert(*entry & PAGE_HUGE);

		// Write the pattern to the test pages
		void* va_pg = va;
		for (int j = 0; j < HPAGE_SIZE / PAGE_SIZE; j++, va_pg += PAGE_SIZE){
			memset(va_pg, random_patterns[j % RANDOM_PATTERNS_COUNT], PAGE_SIZE);
		}

		test_pages[i] = page;
	}

	va = NULL;

	for(int i = 0; i < huge_pages_count; i++, va += HPAGE_SIZE){
		if(!random_decide(PAGE_REMOVAL_PROB)) continue;

		page = test_pages[i];

		physaddr_t *entry;
		assert(page_lookup(kernel_pml4, va, &entry) != NULL);

		struct valid_split_info vsi = {
			.choose_flags = false,
			.unmapped_size = (random_uint_bounded(513) + 1) * PAGE_SIZE,
			.hpage_base = (uintptr_t) va,
		};

		unmap_page_range(kernel_pml4, va, vsi.unmapped_size);
		
		struct page_walker walker = {
			.pte_callback = assert_valid_split_pte,
			.pde_callback = assert_valid_split_pde,
			.udata = &vsi,
		};

		walk_page_range(kernel_pml4, va, va + HPAGE_SIZE, &walker);

		bool regain_all_pages = random_decide(PAGE_REGAIN_PROB);
		bool should_merge = regain_all_pages;

		extern uint64_t random_seed_current;
		uint64_t prev_seed = random_seed_current;

		if (regain_all_pages){
			int j = 0;
			for (void* va_i = va; va_i < va + vsi.unmapped_size; va_i += PAGE_SIZE, j++){
				struct page_info *page = page_alloc(0);
				assert(page != NULL);

				bool keep_flags = random_decide(1 - PAGE_DIFF_FLAGS_PROB);
				should_merge &= keep_flags;
				uint64_t flags = keep_flags ? MERGE_FLAGS : NO_MERGE_FLAGS;

				assert(page_insert(kernel_pml4, page, va_i, flags) == 0);
				memset(va_i, random_patterns[j % RANDOM_PATTERNS_COUNT], PAGE_SIZE);
			}
		}

		if (should_merge){
			physaddr_t prev_entry = *entry;

			struct page_info *page = page_lookup(kernel_pml4, va, &entry);
			assert(page != NULL);
			assert(!page->pp_free && page->pp_ref > 0);
			assert(page->pp_order == BUDDY_2M_PAGE);

			if (prev_entry != *entry){
				panic("Flags of new huge page made from merging doesn't have the expected flags");
			}

			for (void* va_pg = va; va_pg < va + HPAGE_SIZE; va += HPAGE_SIZE){
				assert_page_matches_pattern(va_pg);
			}

		} else if(regain_all_pages){
			random_seed_current = prev_seed;
			vsi.choose_flags = true;
			walk_page_range(kernel_pml4, va, va + HPAGE_SIZE, &walker);
		}
	}

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
