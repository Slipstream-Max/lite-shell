#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include "../include/pipe_redirect.h"
#include "../include/history_log.h"

#define MAX_COMMAND_LENGTH 512
#define MAX_NUM_ARGUMENTS 20
#define MAX_PIPES 10

int redirect_io(char** arguments, int arg_count, int* is_input_redirect, int* insert_point) {
    int input_fd = -1, output_fd = -1;

    // Search redirect symbol
    for (int i = 0; i < arg_count; i++) {
        if (strcmp(arguments[i], ">") == 0 && i + 1 < arg_count) {
            // Open output file
            output_fd = open(arguments[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_fd < 0) {
                perror("Error opening file for output redirection");
                log_error(arguments[i], "Error opening file for output redirection");
                return -1;
            }
            arguments[i] = NULL;
            *insert_point = i;  // This is the command end
            break;
        } else if (strcmp(arguments[i], ">>") == 0 && i + 1 < arg_count) {
            // Open output append file
            output_fd = open(arguments[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (output_fd < 0) {
                perror("Error opening file for append redirection");
                log_error(arguments[i], "Error opening file for append redirection");
                return -1;
            }
            arguments[i] = NULL;
            *insert_point = i;
            break;
        } else if (strcmp(arguments[i], "<") == 0 && i + 1 < arg_count) {
            // Open input file
            input_fd = open(arguments[i + 1], O_RDONLY);
            if (input_fd < 0) {
                perror("Error opening file for input redirection");
                log_error(arguments[i], "Error opening file for input redirection");
                return -1;
            }
            arguments[i] = NULL;
            *insert_point = i;
            break;
        }
    }

    // Redirect stdout to file if output symbol is present
    if (output_fd != -1) {
        dup2(output_fd, STDOUT_FILENO);
        close(output_fd);
    }

    // Redirect file as input if input symbol is present
    if (input_fd != -1) {
        *is_input_redirect = 1;
        dup2(input_fd, STDIN_FILENO);
        close(input_fd);
    }

    if (output_fd==-1&&input_fd==-1){
        return 1;
    }

    return 0;  // Binding success
}

void restore_io(int saved_stdin, int saved_stdout) {
    dup2(saved_stdin, STDIN_FILENO);    // Restore to stdin
    dup2(saved_stdout, STDOUT_FILENO);  // Restore to stdout
    close(saved_stdin); 
    close(saved_stdout);
    dup(STDIN_FILENO);    //have no idea why add this but whitout it will crash.
    dup(STDOUT_FILENO); 
}

int exec_piped_commands(char* commands[], int num_commands) {
    int pipefds[2 * (num_commands - 1)];  // Create file descriptors for each pipe
    pid_t pids[MAX_PIPES];
    int i, status;

    // Create file descriptors for all pipes
    for (i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefds + 2 * i) == -1) {
            perror("pipe error");
            return -1;
        }
    }

    for (i = 0; i < num_commands; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("fork error");
            return -1;
        }

        if (pids[i] == 0) {
            // Child process logic

            // If not the first command, use the previous pipe's read end as standard input
            if (i > 0) {
                dup2(pipefds[2 * (i - 1)], STDIN_FILENO);
            }

            // If not the last command, use the current pipe's write end as standard output
            if (i < num_commands - 1) {
                dup2(pipefds[2 * i + 1], STDOUT_FILENO);
            }

            // Close all pipe file descriptors as they have been redirected
            for (int j = 0; j < 2 * (num_commands - 1); j++) {
                close(pipefds[j]);
            }

            // Execute command
            char* arguments[MAX_NUM_ARGUMENTS];
            int arg_count = 0;
            char* token = strtok(commands[i], " ");
            while (token != NULL && arg_count < MAX_NUM_ARGUMENTS) {
                arguments[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            arguments[arg_count] = NULL;

            execvp(arguments[0], arguments);
            perror("exec error");
            exit(1);
        }
    }

    // Parent process logic: Close all pipe file descriptors and wait for all child processes to exit
    for (i = 0; i < 2 * (num_commands - 1); i++) {
        close(pipefds[i]);
    }

    for (i = 0; i < num_commands; i++) {
        waitpid(pids[i], &status, 0);
    }

    return 0;
}
