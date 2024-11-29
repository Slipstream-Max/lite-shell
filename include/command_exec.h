#ifndef COMMAND_EXEC_H
#define COMMAND_EXEC_H

void handle_command(char* command, char** arguments, int arg_count, char** history, int* history_line, char* initial_directory);

#endif
