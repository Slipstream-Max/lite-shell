#ifndef HISTORY_LOG_H
#define HISTORY_LOG_H

int add_cmd_his(char* command, char** history, int* history_line);
void log_error(const char* command, const char* error_message);

#endif
