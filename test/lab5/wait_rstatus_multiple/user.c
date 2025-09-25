#include <lib.h>

int main(void)
{
    pid_t child1, child2, exited_pid;
    int status;
    printf("[PID %5u] I'm testing rstatusmultiple!\n",getpid());

    // Fork the first child
    child1 = fork();
    
    if (child1 == 0) {

        printf("[PID %5u] I'm the first child! Will return 0x70.\n",getpid());
        return 0x70;
    }

    // Fork the second child
    child2 = fork();
    
    if (child2 == 0) {
        // This is child2
        printf("[PID %5u] I'm the second child! Will return 0x71.\n",getpid());
        return 0x71;
    }

    // This is the parent process
    printf("[Parent PID %5u] Waiting for child processes to exit...\n", getpid());

    // Wait for the first child to finish
    exited_pid = waitpid(child2, &status, 0);
    printf("[Parent PID %5u] Child with PID %d exited with status %p\n", getpid(), exited_pid, status);
    assert((exited_pid == child2 && status == 0x71));

    // Wait for the second child to finish
    exited_pid = waitpid(child1, &status, 0);
    printf("[Parent PID %5u] Child with PID %d exited with status %p\n", getpid(), exited_pid, status);
    assert((exited_pid == child1 && status == 0x70));


    return 0;
}
