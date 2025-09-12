#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern int pml4_setup();

static int run_test() {
	int ret;

	cprintf("[TEST] Dumping all pages\n");
	ret = dump_page_tables((void *) read_cr3(), PAGE_HUGE);
	if (ret < 0)
		panic("[TEST] Page Table Dump exited with %d", ret);

	cprintf("[TEST] Dumping no pages\n");
	ret = dump_page_tables_range((void *) read_cr3(), PAGE_HUGE, (void *) KERNEL_VMA, (void *) KERNEL_VMA);
	if (ret < 0)
		panic("[TEST] Page Table Dump exited with %d", ret);

	cprintf("[TEST] Dumping a single page\n");
	ret = dump_page_tables_range((void *) read_cr3(), PAGE_HUGE, (void *) (KERNEL_VMA + HPAGE_SIZE), (void *) (KERNEL_VMA + HPAGE_SIZE + PAGE_SIZE));
	if (ret < 0)
		panic("[TEST] Page Table Dump exited with %d", ret);

	cprintf("[TEST] Dumping a single huge page\n");
	ret = dump_page_tables_range((void *) read_cr3(), PAGE_HUGE, (void *) (KERNEL_VMA + HPAGE_SIZE), (void *) (KERNEL_VMA + 2 * HPAGE_SIZE));
	if (ret < 0)
		panic("[TEST] Page Table Dump exited with %d", ret);

	cprintf("[TEST] Dumping two huge pages\n");
	ret = dump_page_tables_range((void *) read_cr3(), PAGE_HUGE, (void *) (KERNEL_VMA + 2 * HPAGE_SIZE), (void *) (KERNEL_VMA + 3 * HPAGE_SIZE + 1));
	if (ret < 0)
		panic("[TEST] Page Table Dump exited with %d", ret);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = pml4_setup,
	.should_continue = false,
	.checksum = __checksum__,
};
