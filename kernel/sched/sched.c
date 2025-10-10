
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

	list_init(&this_cpu->runq);
	list_init(&this_cpu->nextq);
	this_cpu->runq_len = 0;
}

void sched_init_mp(void)
{
	/* LAB 6: your code here. */
    cur_task = NULL;

	list_init(&this_cpu->runq);
	list_init(&this_cpu->nextq);
	this_cpu->runq_len = 0;
}

/* Runs the next runnable task. */
void sched_yield(void)
{
try_again:
	/* LAB 5: your code here. */
    if (cur_task && cur_task->task_status == TASK_RUNNING) {

		// lab 5 scheduler
        uint64_t current_time_stamp = read_tsc();
        uint64_t used_time = current_time_stamp - cur_task->last_time_stamp;
        cur_task->last_time_stamp = current_time_stamp;
        cur_task->task_time_budget -= (int64_t)used_time;

        cur_task->task_status = TASK_RUNNABLE;
		if (cur_task->task_cpunum == this_cpu->cpu_id && this_cpu->runq_len > 0) {
			this_cpu->runq_len++;
		}
		list_add(&this_cpu->nextq, &cur_task->task_node);
        cur_task = NULL;
    }

    struct task *next_task = NULL;

	// First try from own runq
    while (!list_is_empty(&this_cpu->runq)) {
        next_task = container_of(list_pop_tail(&this_cpu->runq), struct task, task_node);
        if (next_task->task_status != TASK_RUNNABLE) {
            next_task = NULL;
            continue;
        }

		next_task->task_cpunum = this_cpu->cpu_id;
		this_cpu->runq_len++;

		// lab 5 scheduler
        next_task->last_time_stamp = read_tsc();
        if (next_task->task_time_budget <= 0) {
            next_task->task_time_budget = TIMESLICE;
        }

        task_run(next_task);
    }

	if (fine_spin_haslock(&runq_lock)) {
		fine_spin_unlock(&runq_lock);
		// goto try_again;
	}
	// Now own runq is empty so try taking some from the global or migrate
    if (fine_spin_trylock(&runq_lock) == 0) {
        int local_size = this_cpu->runq_len;
        int high = nuser_tasks / ncpus + 2;

        if (local_size > high) {
            int migrate_count = local_size / 2;
            for (int i = 0; i < migrate_count && !list_is_empty(&this_cpu->nextq); i++) {
                struct list *node = list_pop_tail(&this_cpu->nextq);
                list_add(&runq, node);
                this_cpu->runq_len--;
            }
        }
        int take_from_global_runq = 4;
        for (int i = 0; i < take_from_global_runq && !list_is_empty(&runq); ++i) {
            struct list *node = list_pop_tail(&runq);
            list_add(&this_cpu->runq, node);
        }

		if (fine_spin_haslock(&runq_lock)) {
			fine_spin_unlock(&runq_lock);
		}
        goto try_again;

	} else if (!list_is_empty(&this_cpu->nextq)) { // If cant get lock, move from nextq
		while (!list_is_empty(&this_cpu->nextq)) {
            struct list *node = list_pop_tail(&this_cpu->nextq);
            list_add(&this_cpu->runq, node);
        }

        this_cpu->runq_len = 0;
        goto try_again;
	}

    if (this_cpu->cpu_id == 0 && nuser_tasks == 0) {
        cprintf("No user tasks running in the system! Halting.\n");
        cpus[0].cpu_status = CPU_HALTED;
        sched_halt();
    }

    if (this_cpu->cpu_id != 0 && cpus[0].cpu_status == CPU_HALTED) {
        cprintf("CPU %d halted as CPU 0 is not halted\n", this_cpu->cpu_id);
        this_cpu->cpu_status = CPU_HALTED;
        sched_halt(); // This function should not return
    }

    big_spin_unlock(&kernel_lock);
    for (int i = 0; i < 50; i++) {
        asm volatile("pause");
    }
    big_spin_lock(&kernel_lock);
    goto try_again;
}

/* For now jump into the kernel monitor. */
void sched_halt()
{
	halt_kernel();
}
