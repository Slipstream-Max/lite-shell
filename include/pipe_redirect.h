#ifndef PIPE_REDIRECT_H
#define PIPE_REDIRECT_H

int redirect_io(char** arguments, int arg_count, int* is_input_redirect, int* insert_point);
void restore_io(int saved_stdin, int saved_stdout);
int exec_piped_commands(char* commands[], int num_commands);

#endif
