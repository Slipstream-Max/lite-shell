#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../include/command_exec.h"
#include "../include/pipe_redirect.h"
#include "../include/history_log.h"


#define MAX_COMMAND_LENGTH 512
#define MAX_NUM_ARGUMENTS 20
#define MAX_PIPES 10

void print_prompt() {
    char cwd[MAX_COMMAND_LENGTH];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s [%02d:%02d:%02d] >>", cwd, t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        perror("getcwd() error");
    }
}

int main() {
    char* history[MAX_COMMAND_LENGTH];
    char command[MAX_COMMAND_LENGTH];
    char command_fromfile[MAX_COMMAND_LENGTH];
    char* piped_commands[MAX_PIPES];
    char* arguments[MAX_NUM_ARGUMENTS];
    char initial_directory[MAX_COMMAND_LENGTH];
    int history_line = 0;
    int is_redirect_input = 0;

    // Save initial directory
    if (getcwd(initial_directory, sizeof(initial_directory)) == NULL) {
        perror("getcwd failed");
        return 1;
    }

    // Save stdin/out for restore
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);

    printf("######## Welcome to Shelldemo! ########\n");

    while (1) {
        is_redirect_input = 0;
        restore_io(saved_stdin, saved_stdout);
        print_prompt();

        // Get & breakdown command to argument
        if (!fgets(command, sizeof(command), stdin)) {
            break;  // Exit on EOF
        }
        command[strcspn(command, "\n")] = '\0';

        // Handle piped commands
        int pipe_count = 0;
        char* pipe_token = strtok(command, "|");
        while (pipe_token != NULL && pipe_count < MAX_PIPES) {
            piped_commands[pipe_count++] = pipe_token;
            pipe_token = strtok(NULL, "|");
        }

        // Execute piped commands if present
        if (pipe_count > 1) {
            exec_piped_commands(piped_commands, pipe_count);
            continue;
        }

        int arg_count = 0;
        int insert_point = 0;
        char command_copy[256];  // 假设最大长度为 256
        strcpy(command_copy, command);
        char* token = strtok(command_copy, " ");
        while (token != NULL && arg_count < MAX_NUM_ARGUMENTS) {
            arguments[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        arguments[arg_count] = NULL;

        // Handle edge situation
        if (arg_count == 0) {
            perror("empty command.\n");
            log_error("empty command", "No command entered");
            continue;
        }

        if (strcmp(arguments[0], "quit") == 0) {
            break;
        }

        // Redirect IO
        if (redirect_io(arguments, arg_count, &is_redirect_input, &insert_point) == -1) {
            continue;
        }

        // Re-input if redirected
        if (is_redirect_input == 1) {
            if (!fgets(command_fromfile, sizeof(command_fromfile), stdin)) {
                break;  // Exit on EOF
            }
            command_fromfile[strcspn(command_fromfile, "\n")] = '\0';
            arguments[insert_point] = malloc(strlen(command_fromfile) + 1);
            strcpy(arguments[insert_point], command_fromfile);
        }
        arg_count = (insert_point + 1);

        // Command execution logic
        handle_command(command, arguments, arg_count, history, &history_line, initial_directory);
    }

    printf("######## Quitting Shelldemo ########\n");
    return 0;
}
