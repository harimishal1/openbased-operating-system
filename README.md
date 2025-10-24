## Lab 7 Report for Swapping 
Swap-Out Integration and Memory Pressure Handling

The swap_out path would be triggered whenever the kernel detects that physical memory is exhausted, that is, when page_alloc() fails to return a free frame. Currently, this condition calls oom_kill_task() inside populate_region() as a last resort, which scans all running user tasks and kills the one with the largest resident set size (RSS) to reclaim memory.

Swapping helps the kernel attempt to swap out cold or inactive pages instead of killing a task. When page_alloc() fails, the kernel could call swap_out_daemon() or a helper like try_swap_out_page() before invoking the OOM killer. The daemon would scan the inactive_pages list, select pages that haven’t been recently accessed, and call swap_page_out() to write them to the swap disk. Each swapped-out page would have its PTE’s PAGE_PRESENT bit cleared and a custom flag (e.g., PAGE_PAGED_OUT) set, allowing the page to be faulted back in later.

On the other side, when a task tries to access a swapped-out page, the page-fault handler would detect the accessed flag in the PTE and call swap_page_in(). That function would look up the page’s swap slot, read the data back into a new physical page, restore the PTE to PAGE_PRESENT, and mark the page as active again.

In summary:

Memory pressure: populate_region() calls oom_kill_task() now, but could instead trigger swap_out() to reclaim pages.

Swap-out: swap_out_daemon writes inactive pages to disk, clears PTEs, and frees frames.

Swap-in: Future page faults on those pages would call swap_page_in() to restore them.

Fallback: If swapping cannot reclaim enough memory, the kernel still calls oom_kill_task() to terminate the largest process.

Together, these components form a complete virtual memory lifecycle — allocate → age → swap out → swap in → reclaim — allowing the system to handle memory pressure more gracefully than immediate process termination.

Due to time constraints we were unable to fully implement our features but this is the overview of what we attempted. The swapping in function does not work.

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
