#include <lib.h>

int main(int argc, char **argv)
{
	pid_t pid;
	unsigned cpuid;
	size_t i;

	/* Fork a bunch of processes. */
	for (i = 0; i < 1000; ++i) {
		if (fork() == 0) {
			break;
		}
	}

	pid = getpid();

	printf("[PID %5u] Running on CPU 0\n", pid);

	return 0;
}
