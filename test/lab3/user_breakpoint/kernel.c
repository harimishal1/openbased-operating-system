#include <assert.h>
#include <types.h>

#include <x86-64/idt.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

bool hit_breakpoint = false;

static void intercept_interrupt(struct probe_frame *frame) {
	struct int_frame *interrupt = (struct int_frame *) frame->rdi;
	if(interrupt->int_no == INT_BREAK) {
		hit_breakpoint = true;
	}
}

static int run_test(struct probe_frame *frame) {
	// Intercept call to monitor(); at this point we should
	// have seen a breakpoint interrupt.
	assert(hit_breakpoint == true);

	return __checksum__;
}

extern void int_dispatch();
extern void monitor();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = monitor,
	.should_continue = false,
	.checksum = __checksum__,

	.probe_count = 1,
	.probes = {
		{
			.target = int_dispatch,
			.callback = intercept_interrupt,
		}
	},
};
