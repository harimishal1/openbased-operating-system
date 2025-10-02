
#include "list.h"
#include "stdio.h"
#include <types.h>
#include <error.h>
#include <lib.h>

#include <kernel/mem.h>
#include <kernel/sched.h>


pid_t sys_wait(int *rstatus)
{
	/* LAB 5: your code here. */
    struct list *node;
    struct task *child;

   if (list_is_empty(&cur_task->task_children) && list_is_empty(&cur_task->task_zombies)) {
        return -ECHILD; 
    }

    list_foreach(&cur_task->task_zombies, node) {
        child = container_of(node, struct task, task_child);

        if (rstatus) {
            //*rstatus = child->task_exit_status;
            cprintf("DEBUG: Child PID %u exit status is %d\n", child->task_pid, child->task_exit_status);
            *rstatus = child->task_exit_status;
            cprintf("DEBUG: *rstatus set to %d (0x%x)\n", *rstatus, *rstatus);
        }
        list_del(&child->task_child);
        task_destroy(child);
        return child->task_pid;
    }

    cur_task->task_wait = NULL;
    cur_task->task_status = TASK_NOT_RUNNABLE;
    sched_yield();
    return sys_wait(rstatus);
}

pid_t sys_waitpid(pid_t pid, int *rstatus, int opts)
{
	/* LAB 5: your code here. */
    struct list *node;
    struct task *child;

    if (list_is_empty(&cur_task->task_children) && list_is_empty(&cur_task->task_zombies)) {
        return -ECHILD;
    }
    
    if (pid == -1 || pid == 0) {
        return sys_wait(rstatus);
    }

    list_foreach(&cur_task->task_zombies, node) {
        child = container_of(node, struct task, task_child);
        if (child->task_pid == pid) {
            if (rstatus) {
                cprintf("DEBUG: Child PID %u exit status is %d\n", child->task_pid, child->task_exit_status);
                *rstatus = child->task_exit_status;
                cprintf("DEBUG: *rstatus set to %d (0x%x)\n", *rstatus, *rstatus);
                //*rstatus = child->task_exit_status;
            }
            list_del(&child->task_child);
            task_destroy(child);
            return pid;
        }
    }

    child = pid2task(pid, 1);
    if (!child) {
        return -ECHILD;
    }

    if (child == cur_task) {
        return -ECHILD;
    }

    cur_task->task_wait = child;
    cur_task->task_status = TASK_NOT_RUNNABLE;
    sched_yield();
    return sys_waitpid(pid, rstatus, opts);
}
