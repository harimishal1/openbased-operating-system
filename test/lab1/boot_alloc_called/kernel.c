#include <assert.h>

#include <kernel/test/test.h>

extern void boot_alloc();
extern void halt_kernel();

static int called_boot_alloc = 0;
static void handle_boot_alloc(struct probe_frame *frame) {
	called_boot_alloc = 1;
}

static int run_test() {
	assert(called_boot_alloc);

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,

	.probe_count = 1,
	.probes = {
		{
			.target = boot_alloc,
			.callback = handle_boot_alloc
		}
	}
};
