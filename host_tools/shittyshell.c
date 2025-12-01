#define _POSIX_C_SOURCE 200809L

/*
 * Minimal teaching shell for experimenting with fork/exec/wait.
 * Type absolute paths (e.g., /bin/ls) or use the built-in "boot"
 * command to start the kernel image with QEMU after building rootfs.img.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 16

static int tokenize(char *line, char **argv) {
    int argc = 0;
    char *tok = strtok(line, " \t");
    while (tok && argc < MAX_ARGS - 1) {
        argv[argc++] = tok;
        tok = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    return argc;
}

static void run_command(char **argv) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        execv(argv[0], argv);
        perror("execv");
        _exit(EXIT_FAILURE);
    }

    int status = 0;
    (void)waitpid(pid, &status, 0);
}

int main(void) {
    char *line = NULL;
    size_t bufsize = 0;

    while (1) {
        printf("$ ");
        fflush(stdout);

        if (getline(&line, &bufsize, stdin) == -1) {
            putchar('\n');
            break;
        }

        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (strcmp(line, "exit") == 0) {
            break;
        }

        if (line[0] == '\0') {
            continue;
        }

        if (strcmp(line, "boot") == 0) {
            char *boot_argv[] = {"/usr/bin/qemu-system-i386", "-hda", "../rootfs.img", NULL};
            run_command(boot_argv);
            continue;
        }

        char *argv[MAX_ARGS];
        int argc = tokenize(line, argv);
        if (argc == 0) {
            continue;
        }

        if (argv[0][0] != '/') {
            fprintf(stderr, "Provide an absolute path (example: /bin/ls). Built-ins: boot, exit.\n");
            continue;
        }

        run_command(argv);
    }

    free(line);
    return 0;
}
