#include <assert.h>
#include <stdio.h>
#include <task.h>
#include <types.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>
#include <kernel/sched/task.h>
#include <kernel/mem/buddy.h>

static struct {
	int write_faults_seen;
	int cow_events_detected;
	bool fork_occurred;
} cow_state = {0};

static void pf_handle(struct probe_frame *frame) {
	int flags = (int)frame->rdx;

	/* Check if this is a write fault (potential CoW trigger) */
	if (flags & PAGE_WRITE)
		cow_state.write_faults_seen++;
}

static void fork_handle(struct probe_frame *frame) {
	cow_state.fork_occurred = true;
#ifdef DEBUG
	cprintf("[TESTS] Fork called\n");
#endif
}


static void pdec_handle(struct probe_frame *frame) {
	struct page_info *page = (struct page_info *)frame->rdi;

	if (page->pp_ref == 2 && cow_state.write_faults_seen > 0  
	                      && cow_state.fork_occurred)
		cow_state.cow_events_detected++;
}

static int run_test(struct probe_frame *frame) {

#ifdef DEBUG
	cprintf("\n[LAB 5] ========== CoW Test Results ==========\n");
	cprintf("  Write faults seen: %d\n", cow_state.write_faults_seen);
	cprintf("  CoW events detected: %d\n", cow_state.cow_events_detected);
	cprintf("===========================================\n");
#endif

	if (!cow_state.fork_occurred)
		panic("No fork syscall was observed during test execution");

	if (cow_state.write_faults_seen == 0)
		panic("Expected write faults to trigger CoW, but saw none");

	/* if we only see 1->0 decrements and no 2->1,
	 * it might mean CoW is not implemented (pages copied on fork instead of shared)
	 */
	if (cow_state.cow_events_detected == 0) {

		/* Check if we at least saw the basic fork + write pattern */
		if (cow_state.fork_occurred && cow_state.write_faults_seen > 0) {
			cprintf("[TESTS] Basic fork+write pattern observed, but no shared pages detected\n");
			cprintf("[TESTS] cowfork_ppref() test completed - CoW may not be implemented\n");
			panic("Incomplete test");
		} else {
			panic("Basic fork+write pattern not observed");
		}
	} else {
		return __checksum__;
	}
}

extern void halt_kernel();
extern void sys_fork();
extern void task_page_fault_handler();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,

	.probe_count = 3,
	.probes = {
		{
			.target = sys_fork,
			.callback = fork_handle
		},
		{
			.target = page_decref,
			.callback = pdec_handle
		},
		{
			.target = task_page_fault_handler,
			.callback = pf_handle
		},
	},
};
