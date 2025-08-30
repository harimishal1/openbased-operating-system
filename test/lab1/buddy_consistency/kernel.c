#include <list.h>
#include <paging.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "buddy_consistency.h"

extern void halt_kernel();

static int run_test() {
	struct page_info *page;
	physaddr_t addr;

	check_buddy_consistency_bootmap();

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
