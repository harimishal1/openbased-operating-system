
#include <types.h>
#include <error.h>
#include <lib.h>

#include <kernel/mem.h>
#include <kernel/sched.h>


pid_t sys_wait(int *rstatus)
{
	/* LAB 5: your code here. */
    struct task *child;
	struct list *node, *next;
	list_foreach_safe(&cur_task->task_children, node, next) {
		child = container_of(node, struct task, task_child);
		if (child->task_status == TASK_DYING) {
			if (child->task_exit_status) {
				*rstatus = child->task_exit_status;
			}
			pid_t child_pid = child->task_pid;
			task_destroy(child);
			return child_pid;
		}
	}

    cur_task->task_wait = NULL;
    cur_task->task_status = TASK_NOT_RUNNABLE;
    sched_yield();
    return sys_wait(rstatus);
	// return -ENOSYS;
}

pid_t sys_waitpid(pid_t pid, int *rstatus, int opts)
{
	/* LAB 5: your code here. */
	struct task *task;
	task = pid2task(pid, 1);
	if (!task) return -1;

	if (task->task_status == TASK_DYING) {
        if (task->task_exit_status) {
			*rstatus = task->task_exit_status;
			cprintf("In sys_waitpid: Task %d exit status: %5u\n", pid, *rstatus);
		}
        task_destroy(task);
        return pid;
	}

    cur_task->task_wait = task;
    cur_task->task_status = TASK_NOT_RUNNABLE;
    sched_yield();
    return sys_waitpid(pid, rstatus, opts);
}
