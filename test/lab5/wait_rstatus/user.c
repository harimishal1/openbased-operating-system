#include <lib.h>

int main(void)
{
    pid_t child, exited_pid;
    int status;
    printf("[PID %5u] I'm testing rstatus!\n",getpid());

    child = fork();
    
    if (child == 0) {
        // This is child1
        printf("[PID %5u] I'm the first child! Will return 0x70.\n",getpid());
        return 0x70;
    }

    printf("[Parent PID %5u] Waiting for child processes to exit...\n", getpid());

    exited_pid = waitpid(child, &status, 0);
    printf("[Parent PID %5u] Child with PID %d exited with status %p\n", getpid(), exited_pid, status);
    assert(status == 0x70);

    return 0;
}
