
#include "kernel/sched/task.h"
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
extern struct spinlock kernel_lock;

#ifndef USE_BIG_KERNEL_LOCK
struct spinlock runq_lock = {
#ifdef DEBUG_SPINLOCK
	.name = "runq_lock",
#endif
};
#endif

extern size_t nuser_tasks;

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
    struct task *next_task = NULL;

    // if (cur_task) {
    //     uint64_t current_time_stamp = read_tsc();
    //     uint64_t used_time = current_time_stamp - cur_task->last_time_stamp;
    //     cur_task->last_time_stamp = current_time_stamp;
    //     cur_task->task_time_budget -= (int64_t)used_time;
    // }

    if (cur_task && cur_task->task_status == TASK_RUNNING) {
        cur_task->task_status = TASK_RUNNABLE;
        list_add(&runq, &cur_task->task_node);
    }
    if (!list_is_empty(&runq)) {
        next_task = container_of(list_pop_tail(&runq), struct task, task_node);
    }

    if (next_task && next_task->task_status == TASK_RUNNABLE) {
        next_task->task_status = TASK_RUNNING;
        // next_task->last_time_stamp = read_tsc();
        // if (next_task->task_time_budget <= 0) {
        //     next_task->task_time_budget = TIMESLICE;
        // }
        task_run(next_task);
    }

    cprintf("No runnable tasks in the system!\n");
    sched_halt();
}

bool all_cpus_halted(void)
{
    for (size_t i = 0; i < ncpus; i++) {
        if (cpus[i].cpu_status != CPU_HALTED)
            return false;
    }
    return true;
}

/* For now jump into the kernel monitor. */
void sched_halt()
{
	// halt_kernel();
    cur_task = NULL;

    xchg(&this_cpu->cpu_status, CPU_HALTED);
	
    if (all_cpus_halted()) {
        cprintf("[sched] All CPUs halted — all tasks complete.\n");
        halt_kernel();
    }

    if (big_spin_haslock(&kernel_lock)) {
        big_spin_unlock(&kernel_lock);
    }

    asm volatile("sti; hlt; cli");

    xchg(&this_cpu->cpu_status, CPU_STARTED);

    big_spin_lock(&kernel_lock);

    return;
}
