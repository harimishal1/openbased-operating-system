## Lab 6 kernel threads

The zero-page daemon is a background kernel thread that zeroes and frees physical pages to maintain clean and ready to use memory. 
The function is defined at task.c:342.

It runs non-preemptively by disabling the LAPIC timer, acquires the kernel lock for safety, and processes up to ten pages from the global zeroq list at a time. For each page, it clears its contents with memset, frees it back to the buddy allocator, and then yields control to the scheduler. 

The daemon itself is created using kthread_create (task.c:367), which allocates and initializes a kernel task thread, assigns it a PID, sets up a one-page stack, configures its CPU frame (entry point, stack pointer, argument, and segment selectors), and adds it to the per-CPU run queue. 

The daemon runs asynchronously and goes through the zeroq list when it is scheduled back in. The zeroq list itself is updated inside the page_decref function every time it is called. 

## Implemented bonus features

### Invalid Free Detection

Feature flag: INVALID_FREE_DETECTION

Freeing from the middle of a higher order chunk is considered to be an invalid free. As such, the protection for it is to mark all the pages that part of a higher order with an invalid order value. This is marked upon allocation and checked upon freeing. Thus we can ensure that higher order chunks are never freed. 

### Double Free Detection

Feature flag: DOUBLE_FREE_DETECTION

Two asserts inside the free function check along with a check whether the page currently being freed has the pp_free attribute set or not, ensures that double free doesn't occur. 

### Enabling The Tests

When the two define lines at the top of buddy.c are enabled, the bonus features are activated. When commented out, they fail. 
