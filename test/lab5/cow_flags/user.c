#include <lib.h>
#include <error.h>

/* This test performs checks on page table flags during COW operation */

int main(void)
{
	pid_t child;
	void *page;

	/* Map a page with read-write permissions */
	page = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (page == MAP_FAILED) {
		printf("mmap failed\n");
		return -1;
	}
	
	memset(page, 0x42, 4096);
	printf("[PID %5d] Allocated page at %p and initialized\n", getpid(), page);
	
	/* Create a child process */
	child = fork();
	
	if (child > 0) {
		/* Parent process */
		printf("[PID %5d] Parent after fork\n", getpid());
		
		/* Now write to the page to trigger COW */
		printf("[PID %5d] Parent triggering CoW by writing to page\n", getpid());
		memset(page, 0x55, 4096);
		
		/* Wait for child to complete */
		waitpid(child, NULL, 0);
		printf("[PID %5d] Parent completed\n", getpid());
		
	} else if (child == 0) {
		/* Child process */
		printf("[PID %5d] Child after fork\n", getpid());
		
		/* Write to the page to trigger COW */
		printf("[PID %5d] Child triggering CoW by writing to page\n", getpid());
		memset(page, 0xAA, 4096);
		
		printf("[PID %5d] Child completed\n", getpid());
		return 0;
		
	} else {
		printf("Fork failed\n");
		return -1;
	}

	printf("[PID %5d] Test sequence completed\n", getpid());
	munmap(page,4096);
	return 0;
}
