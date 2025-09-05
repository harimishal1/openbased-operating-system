#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern void halt_kernel();
extern struct list buddy_free_list[];

static int run_test() {
    void *p = page_alloc(0);
    assert(p != NULL);
	p++;
    page_free(p);
	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
