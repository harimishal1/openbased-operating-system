#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

static int run_test() {
	// Call boot_alloc a bunch of times to try trigger an OOM
	// situation

	cprintf("[TEST] Allocating memory from boot allocator until exhaustion\n");

	char *current = boot_alloc(0);
	ptrdiff_t remaining = BOOT_MAP_LIM - PADDR(current);
	char *allocated = boot_alloc(remaining);

	assert(current == allocated);

	// Try to map another page that is too much
	cprintf("[TEST] Allocating memory beyond exhaustion, this should panic\n");
	boot_alloc(1);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = page_init,
	.should_continue = false,
	.checksum = __checksum__,
};
