#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

static int check_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	uint64_t flags;

	(void)walker;

	flags = *entry & (PAGE_WRITE | PAGE_NO_EXEC);

	if (flags == PAGE_WRITE) {
		panic("%p is mapped as write executable!\n", base);
	}

	return 0;
}

static int check_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	uint64_t flags;

	(void)walker;

	if (!(*entry & PAGE_HUGE)) {
		return 0;
	}

	flags = *entry & (PAGE_WRITE | PAGE_NO_EXEC);

	if (flags == PAGE_WRITE) {
		panic("%p is mapped as write executable!\n", base);
	}

	return 0;
}

static int run_test(struct probe_frame *frame) {
	struct task *task = (struct task *) frame->rdi;
	struct page_walker walker = {
		.pte_callback = check_pte,
		.pde_callback = check_pde,
	};

	walk_all_pages(task->task_pml4, &walker);

	return __checksum__;
}

extern void task_run();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_run,
	.should_continue = false,
	.checksum = __checksum__,
};
