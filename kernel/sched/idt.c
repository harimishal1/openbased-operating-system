
#include "kernel/vma/pfault.h"
#include <assert.h>
#include <stdio.h>

#include <x86-64/asm.h>
#include <x86-64/gdt.h>
#include <x86-64/idt.h>

#include <kernel/sched/idt.h>
#include <kernel/monitor.h>
#include <kernel/sched/syscall.h>

#include <kernel/sched/task.h>

#include <lib.h>
#include <paging.h>
extern int task_page_fault_handler(struct task *task, void *va, int flags);

/* LAB 3: your code here. */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr30(void);
extern void isr128(void);
extern void isr127(void);

static const char *int_names[256] = {
	[INT_DIVIDE] = "Divide-by-Zero Error Exception (#DE)",
	[INT_DEBUG] = "Debug (#DB)",
	[INT_NMI] = "Non-Maskable Interrupt",
	[INT_BREAK] = "Breakpoint (#BP)",
	[INT_OVERFLOW] = "Overflow (#OF)",
	[INT_BOUND] = "Bound Range (#BR)",
	[INT_INVALID_OP] = "Invalid Opcode (#UD)",
	[INT_DEVICE] = "Device Not Available (#NM)",
	[INT_DOUBLE_FAULT] = "Double Fault (#DF)",
	[INT_TSS] = "Invalid TSS (#TS)",
	[INT_NO_SEG_PRESENT] = "Segment Not Present (#NP)",
	[INT_SS] = "Stack (#SS)",
	[INT_GPF] = "General Protection (#GP)",
	[INT_PAGE_FAULT] = "Page Fault (#PF)",
	[INT_FPU] = "x86 FPU Floating-Point (#MF)",
	[INT_ALIGNMENT] = "Alignment Check (#AC)",
	[INT_MCE] = "Machine Check (#MC)",
	[INT_SIMD] = "SIMD Floating-Point (#XF)",
	[INT_SECURITY] = "Security (#SX)",
	[INT_SYSCALL] = "System Call(#SC)",
	[INT_PANIC] = "Panic",
};

static struct idt_entry entries[256];
static struct idtr idtr = {
	.limit = sizeof(entries) - 1,
	.entries = entries,
};

static const char *get_int_name(unsigned int_no)
{
	if (!int_names[int_no])
		return "Unknown Interrupt";

	return int_names[int_no];
}

void print_int_frame(struct int_frame *frame)
{
	cprintf("INT frame at %p\n", frame);

	/* Print the interrupt number and the name. */
	cprintf(" INT %u: %s\n",
		frame->int_no,
		get_int_name(frame->int_no));

	/* Print the error code. */
	switch (frame->int_no) {
	case INT_PAGE_FAULT:
		cprintf(" CR2 %p\n", read_cr2());
		cprintf(" ERR 0x%016llx (%s, %s, %s)\n",
			frame->err_code,
			frame->err_code & 4 ? "user" : "kernel",
			frame->err_code & 2 ? "write" : "read",
			frame->err_code & 1 ? "protection" : "not present");
		break;
	default:
		cprintf(" ERR 0x%016llx\n", frame->err_code);
	}

	/* Print the general-purpose registers. */
	cprintf(" RAX 0x%016llx"
		" RCX 0x%016llx"
		" RDX 0x%016llx"
		" RBX 0x%016llx\n"
		" RSP 0x%016llx"
		" RBP 0x%016llx"
		" RSI 0x%016llx"
		" RDI 0x%016llx\n"
		" R8  0x%016llx"
		" R9  0x%016llx"
		" R10 0x%016llx"
		" R11 0x%016llx\n"
		" R12 0x%016llx"
		" R13 0x%016llx"
		" R14 0x%016llx"
		" R15 0x%016llx\n",
		frame->rax, frame->rcx, frame->rdx, frame->rbx,
		frame->rsp, frame->rbp, frame->rsi, frame->rdi,
		frame->r8,  frame->r9,  frame->r10, frame->r11,
		frame->r12, frame->r13, frame->r14, frame->r15);

	/* Print the IP, segment selectors and the RFLAGS register. */
	cprintf(" RIP 0x%016llx"
		" RFL 0x%016llx\n"
		" CS  0x%04x"
		"            "
		" DS  0x%04x"
		"            "
		" SS  0x%04x\n",
		frame->rip, frame->rflags,
		frame->cs, frame->ds, frame->ss);
}

/* Set up the interrupt handlers. */
void idt_init(void)
{
	/* LAB 3: your code here. */
	set_idt_entry(&entries[INT_DIVIDE], (void *)isr0, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE); //
	set_idt_entry(&entries[INT_DEBUG], (void *)isr1, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_NMI], (void *)isr2, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_BREAK], (void *)isr3, IDT_TRAP_GATE32 | IDT_PRESENT | IDT_PRIVL(3), GDT_KCODE);
	set_idt_entry(&entries[INT_OVERFLOW], (void *)isr4, IDT_TRAP_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_BOUND], (void *)isr5, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_INVALID_OP], (void *)isr6, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_DEVICE], (void *)isr7, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_DOUBLE_FAULT], (void *)isr8, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_TSS], (void *)isr10, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_NO_SEG_PRESENT], (void *)isr11, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_SS], (void *)isr12, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_GPF], (void *)isr13, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE); //
	set_idt_entry(&entries[INT_PAGE_FAULT], (void *)isr14, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_FPU], (void *)isr16, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_ALIGNMENT], (void *)isr17, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_MCE], (void *)isr18, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_SIMD], (void *)isr19, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_SECURITY], (void *)isr30, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(0), GDT_KCODE);
	set_idt_entry(&entries[INT_SYSCALL], (void *)isr128, IDT_TRAP_GATE32 | IDT_PRESENT | IDT_PRIVL(3), GDT_KCODE);
	set_idt_entry(&entries[INT_PANIC], (void *)isr127, IDT_INT_GATE32 | IDT_PRESENT | IDT_PRIVL(3), GDT_KCODE);
	load_idt(&idtr);
}

void gpf_handler(struct int_frame *frame)
{
	void *fault_va = (void *)read_cr2();
	cprintf("[PID %5u] gpf fault va %p ip %p\n",
		cur_task->task_pid, fault_va, frame->rip);
	print_int_frame(frame);
	task_destroy(cur_task);

}

void divide_handler(struct int_frame *frame)
{
	void *fault_va = (void *)read_cr2();
	cprintf("[PID %5u] gpf fault va %p ip %p\n",
		cur_task->task_pid, fault_va, frame->rip);
	print_int_frame(frame);
	task_destroy(cur_task);

}

void int_dispatch(struct int_frame *frame)
{
	/* Handle processor exceptions:
	 *  - Fall through to the kernel monitor() on a breakpoint.
	 *  - Halt the kernel upon user panic (halt_kernel())
	 *  - Dispatch page faults to page_fault_handler().
	 *  - Dispatch system calls to syscall().
	 */
	uint64_t ret;
	switch (frame->int_no) {
		/* LAB 3: your code here. */
		case INT_BREAK:
			monitor(frame);
			return;
		case INT_PANIC:
			halt_kernel();
			return;
		case INT_PAGE_FAULT:
			page_fault_handler(frame);
			return;
		case INT_SYSCALL:
			ret = syscall(frame->rax,frame->rdi,frame->rsi,frame->rdx,frame->rcx, frame->r8, frame->r9);
			frame->rax = ret;
			return;
		case INT_GPF:
			gpf_handler(frame);
			return;
		case INT_DIVIDE:
			divide_handler(frame);
			return;
		default: break;
	}

	/* Unexpected trap: The user process or the kernel has a bug. */
	print_int_frame(frame);

	if (frame->cs == GDT_KCODE) {
		panic("unhandled interrupt in kernel");
	} else {
		task_destroy(cur_task);
		return;
	}
}

void int_handler(struct int_frame *frame)
{
	/* The task may have set DF and some versions of GCC rely on DF being
	 * clear. */
	asm volatile("cld" ::: "cc");
	/* Check if interrupts are disabled.
	 * If this assertion fails, DO NOT be tempted to fix it by inserting a
	 * "cli" in the interrupt path.
	 */
	assert(!(read_rflags() & FLAGS_IF));
	/* cprintf("Incoming INT frame at %p\n", frame); */
	if ((frame->cs & 3) == 3) {
		/* Interrupt from user mode. */
		assert(cur_task);

		/* Copy interrupt frame (which is currently on the stack) into
		 * 'cur_task->task_frame', so that running the task will restart at
		 * the point of interrupt. */
		cur_task->task_frame = *frame;

		/* Avoid using the frame on the stack. */
		frame = &cur_task->task_frame;
	}

	/* Dispatch based on the type of interrupt that occurred. */
	int_dispatch(frame);

	/* Return to the current task, which should be running. */
	task_run(cur_task);
}

void page_fault_handler(struct int_frame *frame)
{
	void *fault_va;
	unsigned perm = 0;
	int ret;


	/* Read the CR2 register to find the faulting address. */
	fault_va = (void *)read_cr2();

	/* LAB 4: your code here */
	perm = PROT_READ;
	if (frame->err_code & PF_WRITE) perm |= PROT_WRITE;
	if (frame->err_code & PF_IFETCH) perm |= PROT_EXEC;
	ret = task_page_fault_handler(cur_task, fault_va, perm);
	if (ret == 0) return;

	/* Handle kernel-mode page faults. */
	/* LAB 3: your code here. */
	if (frame->cs == GDT_KCODE) {
	// if ((frame->cs & 0x3) == 0) {
        cprintf("Kernel page fault at va %p, ip %p\n", fault_va, frame->rip);
        print_int_frame(frame);
        panic("page fault in kernel mode");
    }
	/* We have already handled kernel-mode exceptions, so if we get here, the
	 * page fault has happened in user mode.
	 */

	/* Destroy the task that caused the fault. */
	cprintf("[PID %5u] user fault va %p ip %p\n",
		cur_task->task_pid, fault_va, frame->rip);
	print_int_frame(frame);
	task_destroy(cur_task);
}