#include <assert.h>

#include <kernel/sched.h>
#include <kernel/test/test.h>

extern void syscall_handler();
static int syscall_counter = 0;
static void handle_syscall(struct probe_frame *frame) {
	syscall_counter++;
}

static int run_test() {
	// Two prints, one getpid(), and one exit()
	assert(syscall_counter == 4);

	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,

	.probe_count = 1,
	.probes = {
		{
			.target = syscall_handler,
			.callback = handle_syscall,
		}
	},
};
