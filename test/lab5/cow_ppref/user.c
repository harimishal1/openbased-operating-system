#include <lib.h>

/* User program that triggers CoW behavior for testing */

int main(void)
{
    pid_t child;
    void *page;

    printf("[CoW User Test] Starting CoW test sequence\n");

    // Allocate a page that will be shared and then copied
    page = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (page == MAP_FAILED) {
        printf("Failed to allocate test page\n");
        return 1;
    }

    // Initialize the page with a pattern
    memset(page, 0x42, 4096);
    printf("[PID %5u] Allocated and initialized page at %p\n", getpid(), page);

    // Fork to create shared page scenario
    child = fork();

    if (child > 0) {
        // Parent process
        printf("[PID %5u] Parent after fork\n", getpid());
        
        // Trigger CoW by writing to the shared page
        printf("[PID %5u] Parent triggering CoW by writing to page\n", getpid());
        memset(page, 0x55, 4096);
        
        // Wait for child to complete
        waitpid(child, NULL, 0);
        printf("[PID %5u] Parent completed\n", getpid());
        
    } else if (child == 0) {
        // Child process
        printf("[PID %5u] Child after fork\n", getpid());
        
        // Trigger CoW by writing to the shared page
        printf("[PID %5u] Child triggering CoW by writing to page\n", getpid());
        memset(page, 0xaa, 4096);
        
        printf("[PID %5u] Child completed\n", getpid());
        return 0;
        
    } else {
        printf("Fork failed\n");
        return 1;
    }

    // Clean up
    munmap(page, 4096);
    printf("[PID %5u] Test sequence completed\n", getpid());

    return 0;
}
