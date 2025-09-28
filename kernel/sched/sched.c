
#include "kernel/sched/task.h"
#include "task.h"
#include <types.h>
#include <list.h>
#include <stdio.h>
#include <x86-64/asm.h>
#include <x86-64/paging.h>

#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>

struct list runq;


extern size_t nuser_tasks;

void sched_init(void)
{
	list_init(&runq);
}


/* Runs the next runnable task. */
void sched_yield(void)
{
	/* LAB 5: your code here. */
	if (list_is_empty(&runq)) {
		if (cur_task) {
			task_run(cur_task);
		} else {
			halt_kernel();
		}
	}

	struct task *next_task = container_of(list_pop_tail(&runq), struct task, task_node);
	task_run(next_task);
}

/* For now jump into the kernel monitor. */
void sched_halt()
{
	halt_kernel();
}
