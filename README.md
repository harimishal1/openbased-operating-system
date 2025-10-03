## Scheduler (lab 5)

Added to the task struct:
	uint64_t task_time_budget;
	uint64_t last_time_stamp;
These are initialized in task_alloc.
Budget is initialized to 100 million ticks.

In idt.c we keep a bool reschedule, which is used to indicate whether we should switch to a new task.
At the end of int_handler, right before returning to the current task, we check if this is 
set, and if so, we call sched_yield.

In irq_handler, we decrease the task_time_budget of the current task using the task_tsc and the current
tsc (read_tsc()). Then if the budget goes below 0, we set reschedule to true.

In sched_yield, we again calculate the remaining time budget of the current task, and also of the next
task. If the budget of the next task being run is below 0, we reset it to TIMESLICE.

## Implemented bonus features (lab 1)

### Invalid Free Detection

Feature flag: INVALID_FREE_DETECTION

Freeing from the middle of a higher order chunk is considered to be an invalid free. As such, the protection for it is to mark all the pages that part of a higher order with an invalid order value. This is marked upon allocation and checked upon freeing. Thus we can ensure that higher order chunks are never freed. 

### Double Free Detection

Feature flag: DOUBLE_FREE_DETECTION

Two asserts inside the free function check along with a check whether the page currently being freed has the pp_free attribute set or not, ensures that double free doesn't occur. 

### Enabling The Tests

When the two define lines at the top of buddy.c are enabled, the bonus features are activated. When commented out, they fail. 
