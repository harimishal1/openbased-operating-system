#include "kernel/mem/init.h"
#include "spinlock.h"
#include <types.h>
#include <assert.h>
#include <boot.h>
#include <error.h>
#include <stdio.h>
#include <string.h>

#include <cpu.h>

#include <kernel/acpi.h>
#include <kernel/console.h>
#include <kernel/fwcfg.h>
#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/mp.h>
#include <kernel/pic.h>
#include <kernel/sched.h>
#include <kernel/test/probe.h>
#include <kernel/test/test.h>
#include <kernel/symbols.h>

extern struct spinlock kernel_lock;

uint8_t *find_user_binary() {
	// Find the binary to run from the QEMU fw_cfg parameters
	char *user_binary_name;
	int ret = fwcfg_read_alloc("opt/openlsd.user", &user_binary_name);

	// No user program specified
	if(ret == 0 || ret == -EINVAL) {
		return NULL;
	}

	// Error handling
	if(ret < 0)
		panic("Could not load user program name: %e\n", ret);

	// Find the user binary symbol by its name
	char *user_binary_symbol_name = kmalloc(ret + 24); // 24 bytes for "_binary_obj_user_" and "_start" + \0
	snprintf(user_binary_symbol_name, ret + 24, "_binary_obj_user_%s_start", user_binary_name);

	uint8_t *binary = find_symbol(user_binary_symbol_name);
	if(binary == NULL)
		panic("Could not load user binary with name: %s\n", user_binary_name);

	return binary;
}

void kmain(struct boot_info *boot_info)
{
	extern char edata[], end[];
	struct rsdp *rsdp;
	
	/* Before doing anything else, complete the ELF loading process.
	* Clear the uninitialized global data (BSS) section of our program.
	* This ensures that all static/global variables start out zero.
	*/
	memset(edata, 0, end - edata);
	
	/* Initialize the console.
	* Can't call cprintf until after we do this! */
	cons_init();
	cprintf("\n");
	
	// Pass boot info to the monitor for debugging
	extern struct boot_info *monitor_boot_info;
	monitor_boot_info = boot_info;

	/* Prepare the fw_cfg interface for reading QEMU metadata */
	fwcfg_init();

	/* Register the current test to run */
	uint8_t *binary = tests_init(); // If test comes with a binary to run, we should run it

	/* Set up segmentation, interrupts and system calls. */
	gdt_init();
	idt_init();
	syscall_init();
	/* Lab 1 memory management initialization functions */
	mem_init(boot_info);

	/* Set up the slab allocator. */
	kmem_init();

	/* Set up the interrupt controller and timers */
	pic_init();
	rsdp = rsdp_find();

	madt_init(rsdp);
	lapic_init();
	hpet_init(rsdp);
		
	big_spin_lock(&kernel_lock);

	/* Set up the tasks. */
	task_init();
	sched_init();
	
	mem_init_mp();
	boot_cpus();

	/// If test does not come with a binary to run, try to find a user-specified one
	if(binary == NULL)
		binary = find_user_binary();

	// If still no binary is found, we drop into the kernel monitor
	if(binary == NULL) {
		/* Drop into the kernel monitor */
		cprintf("No user program was selected!\n");
		halt_kernel();
	}

	// big_spin_lock(&kernel_lock);
	task_create(binary, TASK_TYPE_USER);
	
	sched_yield();
}

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void _panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	/* Be extra sure that the machine is in as reasonable state */
	__asm __volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* Break into the kernel monitor */
	halt_kernel();
}

/* Like panic, but don't. */
void _warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
