
#include "kernel/sched/task.h"
#include "spinlock.h"
#include "task.h"
#include <types.h>
#include <cpu.h>
#include <list.h>
#include <stdio.h>
#include <x86-64/asm.h>
#include <x86-64/paging.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>

struct list runq;


#ifndef USE_BIG_KERNEL_LOCK
struct spinlock runq_lock = {
#ifdef DEBUG_SPINLOCK
	.name = "runq_lock",
#endif
};
#endif

extern size_t nuser_tasks;
extern struct spinlock kernel_lock;

void sched_init(void)
{
	list_init(&runq);
}

void sched_init_mp(void)
{
	/* LAB 6: your code here. */
    cur_task = NULL;
}

/* Runs the next runnable task. */
void sched_yield(void)
{
	/* LAB 5: your code here. */
    assert(big_spin_haslock(&kernel_lock));

    // If there's a running task, put it back on the run queue as runnable.
    if (cur_task && cur_task->task_status == TASK_RUNNING) {
        cur_task->task_status = TASK_RUNNABLE;
        list_add(&runq, &cur_task->task_node);
    }

    // Main scheduler loop: find a task or wait if none are available.
    struct task *next_task = NULL;
    while (1) {
        // Condition 1: If we are CPU 0 and all user tasks are gone, halt the system.
        if (this_cpu->cpu_id == 0 && nuser_tasks == 0) {
            cprintf("No user tasks running in the system! Halting.\n");
            cpus[0].cpu_status = CPU_HALTED;
            sched_halt(); // This function should not return
        }

        // Condition 2: If we are a secondary CPU and CPU 0 has halted, we should halt too.
        if (this_cpu->cpu_id != 0 && boot_cpu->cpu_status != CPU_STARTED) {
            cprintf("CPU %d halted as CPU 0 is not started\n", this_cpu->cpu_id);
            this_cpu->cpu_status = CPU_HALTED;
            sched_halt(); // This function should not return
        }

        // Condition 3: Try to find a task to run from the queue.
        if (!list_is_empty(&runq)) {
            next_task = container_of(list_pop_tail(&runq), struct task, task_node);
            break; 
        }

        // Condition 4 (Idle): The run queue is empty. Wait efficiently for an interrupt.
        spin_unlock(&kernel_lock);
        for (int i = 0; i < 50; i++) asm volatile("pause"); // Go to sleep until an interrupt wakes us.
        spin_lock(&kernel_lock);
        
        // After waking up, loop again to re-evaluate all conditions.
    }

    // We have found a task, so run it.
    if (next_task) {
        next_task->task_status = TASK_RUNNING;
        task_run(next_task);
    } else {
        // This should be unreachable if the logic above is correct.
        // It's a safety net.
        cprintf("Error: Scheduler exited without a task. Halting CPU %u.\n", this_cpu->cpu_id);
        sched_halt();
    }
}

   /*  if (cur_task && cur_task->task_status == TASK_RUNNING) {
        cur_task->task_status = TASK_RUNNABLE;
        list_add(&runq, &cur_task->task_node);
    }
    if (!list_is_empty(&runq)) {
        next_task = container_of(list_pop_tail(&runq), struct task, task_node);
    }

    if (next_task && next_task->task_status == TASK_RUNNABLE) {
        next_task->task_status = TASK_RUNNING;
        task_run(next_task);
    }
    cprintf("No runnable tasks in the system!\n");
    sched_halt(); */


/* For now jump into the kernel monitor. */
void sched_halt()
{
	halt_kernel();
}
