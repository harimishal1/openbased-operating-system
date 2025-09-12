#include <assert.h>
#include <stdio.h>

#include <kernel/mem.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>

static int run_test(struct probe_frame *frame) {
	struct task *task = (struct task *) frame->rdi;
	if (page_lookup(task->task_pml4, 0, NULL)) {
		panic("NULL should not be mapped!");
	}

	return __checksum__;
}

extern void task_run();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_run,
	.should_continue = false,
	.checksum = __checksum__,
};
