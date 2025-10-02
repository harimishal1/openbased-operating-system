
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
    struct task *next_task = NULL;

    if (cur_task && cur_task->task_status == TASK_RUNNING) {
        cur_task->task_status = TASK_RUNNABLE;
        list_add_tail(&runq, &cur_task->task_node);
    }

    if (!list_is_empty(&runq)) {
        next_task = container_of(list_pop_tail(&runq), struct task, task_node);
    }

    if (next_task && next_task->task_status == TASK_RUNNABLE) {
        next_task->task_status = TASK_RUNNING;
        task_run(next_task);
    }

    if (cur_task && cur_task->task_status == TASK_RUNNING) {
        task_run(cur_task); 
    }

    cprintf("No runnable tasks in the system!\n");
    sched_halt();
}

/* For now jump into the kernel monitor. */
void sched_halt()
{
	halt_kernel();
}
