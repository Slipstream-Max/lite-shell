#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/history_log.h"

int add_cmd_his(char* command, char** history, int* history_line) {
    // Allocate memory for the new command and copy it to history
    history[*history_line] = malloc(strlen(command) + 1);
    if (history[*history_line] == NULL) {
        perror("Failed to allocate memory");
        return -1;  // Return an error code
    }
    strcpy(history[*history_line], command);

    // Increment the history line counter
    (*history_line)++;
    if (*history_line >= 100) {
        *history_line = 0;  // Wrap around if history is full
    }

    return 0;  // Return success code
}

void log_error(const char* command, const char* error_message) {
    // Open the log file in append mode
    FILE* log_file = fopen(".lite_shell_err_log", "a");
    if (log_file == NULL) {
        perror("Error opening log file");
        return;
    }

    // Log the command and the error message
    fprintf(log_file, "Command: %s\nError: %s\n\n", command, error_message);

    // Close the log file
    fclose(log_file);
}
    