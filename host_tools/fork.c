#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        char *argv[] = {"/usr/bin/ls", getenv("HOME") ? getenv("HOME") : ".", NULL};
        printf("I'm the child!\n");
        execv(argv[0], argv);
        perror("execv");
        _exit(EXIT_FAILURE);
    }

    printf("I'm the parent. child pid = %d\n", pid);
    wait(NULL);
    return EXIT_SUCCESS;
}
