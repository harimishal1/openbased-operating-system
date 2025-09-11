#include <kernel/test/test.h>

static int run_test() {
	/* Trigger a fault by executing NULL. */
	void (* func)(void) = NULL;
	asm("call *%0" : : "r"(func));

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
