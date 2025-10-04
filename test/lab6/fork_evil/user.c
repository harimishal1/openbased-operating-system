#include <lib.h>

int main(void)
{
	pid_t child, killed;
	size_t i;

	child = fork();

	if (child > 0) {
		printf("Child is %p\n", child);
		sched_yield();
		sched_yield();
		sched_yield();
		sched_yield();
		sched_yield();
		sched_yield();
		sched_yield();
		sched_yield();
		sched_yield(); // Create enough delay for another core to catch up
		kill(child);
		assert(waitpid(child, NULL, 0) == child);
		printf("[PID %5u] I am the parent!\n", getpid());
	} else {
		printf("[PID %5u] I am the child!\n", getpid());

		while (1);
	}

	return 0;
}
