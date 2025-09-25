#include <assert.h>
#include <stdio.h>
#include <types.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>
#include <kernel/sched/task.h>
#include <task.h>

// Test state tracking
static struct {
	int write_faults_seen;
	int memcpy_cow_detected;
	int flag_fixes_detected;
	bool fork_occurred;
	uintptr_t last_cow_va;
} cow_flags_state = {0};

static void pf_handle(struct probe_frame *frame) {
	struct task *task = (struct task *)frame->rdi;
	void *va = (void *)frame->rsi;
	int fault_flags = (int)frame->rdx;

	/* Only check write faults and only if we have a valid task */
	if (!(fault_flags & PAGE_WRITE) || !task || !task->task_pml4)
		return;

	cow_flags_state.write_faults_seen++;

	physaddr_t *entry = NULL;
	struct page_info *page = page_lookup(task->task_pml4, va, &entry);

	if (page && entry)
	{
		uint64_t flags = *entry & PAGE_MASK;
#ifdef DEBUG
		cprintf("[TESTS] Write fault #%d at %p: flags=0x%lx, ref_count=%d\n",
			   cow_flags_state.write_faults_seen, va, flags, page->pp_ref);
#endif
		if (page->pp_ref > 1)
			cow_flags_state.last_cow_va = (uintptr_t)va;
		
	}
}

static void memcpy_handle(struct probe_frame *frame) {
	void *dst = (void *)frame->rdi;
	void *src = (void *)frame->rsi;
	size_t size = (size_t)frame->rdx;

	/* Check if this looks like a CoW memcpy (PAGE_SIZE copy) */
	if (size != PAGE_SIZE || cow_flags_state.last_cow_va == 0)
		return;

	cow_flags_state.memcpy_cow_detected++;
#ifdef DEBUG
	cprintf("[TESTS] CoW memcpy detected: dst=%p, src=%p, size=%zu\n",
			dst, src, size);
#endif

	/* Check the page that triggered CoW after the copy */
	if (!cur_task || !cur_task->task_pml4)
		goto reset_cow_va;

	physaddr_t *entry;
	struct page_info *page = page_lookup(cur_task->task_pml4,
	                                     (void *)cow_flags_state.last_cow_va, &entry);

	if (!page || !entry)
		goto reset_cow_va;

	uint64_t flags = *entry & PAGE_MASK;
#ifdef DEBUG
	cprintf("[TESTS] After CoW copy: VA %p, flags=0x%lx, ref_count=%d\n",
			cow_flags_state.last_cow_va, flags, page->pp_ref);
#endif

	// Check if the new page is now writable and private
	if (page->pp_ref == 1 && (flags & PAGE_WRITE))
		cow_flags_state.flag_fixes_detected++;

reset_cow_va:
	/* Reset for next potential CoW */
	cow_flags_state.last_cow_va = 0;
}

static void fork_handle(struct probe_frame *frame) {
	cow_flags_state.fork_occurred = true;
}

static int run_test(struct probe_frame *frame) {
#ifdef DEBUG
	cprintf("\n[LAB 5] ========== CoW Flags Test Results ==========\n");
	cprintf("  Write faults seen: %d\n", cow_flags_state.write_faults_seen);
	cprintf("  CoW memcpy operations: %d\n", cow_flags_state.memcpy_cow_detected);
	cprintf("  Flag fixes detected: %d\n", cow_flags_state.flag_fixes_detected);
#endif
	if (!cow_flags_state.fork_occurred)
		panic("No fork syscall was observed during test execution");

	if (cow_flags_state.write_faults_seen == 0)
		panic("Expected write faults to trigger CoW, but saw none");

	/* after CoW memcpy, are pages properly made writable? */
	if (cow_flags_state.flag_fixes_detected > 0)
		return __checksum__;
	else if (cow_flags_state.memcpy_cow_detected > 0)
		panic("CoW is copying pages but not fixing flags!\n");

	return __checksum__;
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
			.target = task_page_fault_handler,
			.callback = pf_handle
		},
		{
			.target = memcpy,
			.callback = memcpy_handle
		},
	},
};
