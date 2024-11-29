#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "../include/command_exec.h"
#include "../include/history_log.h"

#define MAX_COMMAND_LENGTH 512
#define MAX_NUM_ARGUMENTS 20

int exec_extern_cmd(char** arguments);
int remove_folder(char* path);

void handle_command(char* command, char** arguments, int arg_count, char** history, int* history_line, char* initial_directory) {
    char buf[512];
    if (strcmp(arguments[0], "pwd2") == 0) {
        add_cmd_his(command, history, history_line);
        if (getcwd(buf, sizeof(buf)) == NULL) {
            perror("getcwd failed");
            log_error(arguments[0], "getcwd failed");
        }
        printf("%s\n", buf);
        return;
    }

    if (strcmp(arguments[0], "echo2") == 0) {
        add_cmd_his(command, history, history_line);
        printf("%s\n", arguments[1]);
        return;
    }

    if (strcmp(arguments[0], "cd2") == 0) {
        add_cmd_his(command, history, history_line);
        if (arg_count < 2) {
            arguments[1] = initial_directory;
        }
        if (strcmp(arguments[1], "-") == 0) {
            arguments[1] = "..";
        }
        if (chdir(arguments[1]) != 0) {
            perror("cd failed");
            log_error(arguments[0], "cd failed");
        }
        return;
    }

    if (strcmp(arguments[0], "ls2") == 0) {
        add_cmd_his(command, history, history_line);
        if (arg_count < 2) {
            arguments[1] = ".";
        }
        DIR* directory = opendir(arguments[1]);
        if (directory == NULL) {
            perror("Unable to read directory");
            log_error(arguments[0], "Unable to read directory");
            return;
        }
        struct dirent* entry;
        while ((entry = readdir(directory)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                if (entry->d_type == DT_DIR) {
                    printf("%s:%s\n", "Folder", entry->d_name);
                }
                if (entry->d_type == DT_REG) {
                    printf("%s:%s\n", "File", entry->d_name);
                }
            }
        }
        closedir(directory);
        return;
    }

    if (strcmp(arguments[0], "touch2") == 0) {
        add_cmd_his(command, history, history_line);
        if (arg_count < 2) {
            arguments[1] = "filename";
        }
        FILE* fp;
        fp = fopen(arguments[1], "w+");
        if (fp == NULL) {
            perror("fopen:");
            log_error(arguments[0], "fopen failed");
            return;
        }
        fclose(fp);
        return;
    }

    if (strcmp(arguments[0], "cat2") == 0) {
        add_cmd_his(command, history, history_line);
        if (arg_count < 2) {
            perror("not enough argument");
            log_error(arguments[0], "not enough argument");
            return;
        }
        FILE* fp = fopen(arguments[1], "rb");
        if (fp == NULL) {
            perror("Error opening file");
            log_error(arguments[0], "Error opening file");
            return;
        }
        char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
            fwrite(buffer, 1, bytesRead, stdout);
        }
        if (ferror(fp)) {
            perror("Error reading file");
            log_error(arguments[0], "Error reading file");
        }
        printf("\n");
        fclose(fp);
        return;
    }

    if (strcmp(arguments[0], "cp2") == 0) {
        add_cmd_his(command, history, history_line);
        if (arg_count < 3) {
            perror("not enough argument");
            log_error(arguments[0], "not enough argument");
            return;
        }
        FILE* fp1 = fopen(arguments[1], "rb");
        if (fp1 == NULL) {
            perror("Error opening file1");
            log_error(arguments[0], "Error opening file");
            return;
        }
        FILE* fp2 = fopen(arguments[2], "wb");
        if (fp2 == NULL) {
            perror("Error opening destination file");
            log_error(arguments[0], "Error opening destination file");
            fclose(fp1);  // Ensure the first file is closed before returning
            return;
        }
        char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), fp1)) > 0) {
            if (fwrite(buffer, 1, bytesRead, fp2) != bytesRead) {
                perror("Error writing to destination file");
                log_error(arguments[0], "Error writing to destination file");
                fclose(fp1);
                fclose(fp2);
                return;
            }
        }
        if (ferror(fp1)) {
            perror("Error reading file");
            log_error(arguments[0], "Error reading file");
        }
        fclose(fp1);
        fclose(fp2);
        return;
    }

    if (strcmp(arguments[0], "rename2") == 0) {
        add_cmd_his(command, history, history_line);
        if (arg_count < 3) {
            perror("not enough argument");
            log_error(arguments[0], "not enough argument");
            return;
        }
        if (rename(arguments[1], arguments[2]) != 0) {
            perror("Error renaming file");
            log_error(arguments[0], "Error renaming file");
        }
        return;
    }
    
    if (strcmp(arguments[0], "rm2") == 0) {
        add_cmd_his(command, history, history_line);
        if (arg_count < 2) {
            perror("not enough argument");
            log_error(arguments[0], "not enough argument");
            return;
        }
        if (strcmp(arguments[1], "-r") == 0) {
            if (arg_count < 3) {
                //perror("not enough argument");
                log_error(arguments[0], "not enough argument");
                return;
            }
            if (remove_folder(arguments[2]) != 0) {
                perror("Error delete folder");
                log_error(arguments[0], "Error delete folder");
            }
            return;
        }
        if (remove(arguments[1]) != 0) {
            perror("Error delete file");
            log_error(arguments[0], "Error delete file");
        }
        return;
    }

    if (strcmp(arguments[0], "history2") == 0) {
        for (int i = 0; i < *history_line; i++) {
            printf("%s\n", history[i]);
        }
        add_cmd_his(command, history, history_line);
        return;
    }

    // Test if this is an external command
    int result = exec_extern_cmd(arguments);
    if (result == -1) {
        printf("Command not found or execution failed: %s\n", arguments[0]);
        log_error(arguments[0], "Command not found or execution failed");
    } else {
        add_cmd_his(command, history, history_line);
    }
}

int exec_extern_cmd(char** arguments) {
    
    if (strcmp(arguments[0], "cd") == 0) {
        const char* dir = arguments[1];
        if (dir == NULL) {
            // 如果没有提供目录参数，获取 HOME 环境变量
            dir = getenv("HOME");
            if (dir == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return -1;
            }
        }
        if (chdir(dir) != 0) {
            perror("cd");
            return -1;
        }
        return 0;
    }

    const char* path_env = getenv("PATH");
    char* path_copy = strdup(path_env);
    char* path_dir = strtok(path_copy, ":");
    char full_path[MAX_COMMAND_LENGTH];

    // Combine full path to program
    while (path_dir != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", path_dir, arguments[0]);
        if (access(full_path, X_OK) == 0) {
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork error");
                log_error(arguments[0], "fork failed");
                free(path_copy);
                return -1;
            }
            if (pid == 0) {
                execv(full_path, arguments);
                perror("execv");
                log_error(arguments[0], "execv failed");
                exit(1);
            } else {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                    return WEXITSTATUS(status);
                } else {
                    return -1;
                }
            }
        }
        path_dir = strtok(NULL, ":");
    }
    free(path_copy);
    log_error(arguments[0], "Command not found or not executable");
    return -1;
}

int remove_folder(char* path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        perror("Error getting directory status");
        log_error(path, "Error getting directory status");
        return 1;
    }
    if (!S_ISDIR(path_stat.st_mode)) {
        fprintf(stderr, "%s is not a directory\n", path);
        log_error(path, "Not a directory");
        return 1;
    }
    DIR* dir = opendir(path);
    if (dir == NULL) {
        perror("Error opening directory");
        log_error(path, "Error opening directory");
        return 1;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip the special entries "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (entry->d_type == DT_DIR) {
            remove_folder(full_path);
        }
        if (entry->d_type == DT_REG) {
            if (remove(full_path) != 0) {
                perror("Error deleting file");
                log_error(full_path, "Error deleting file");
            }
        }
    }
    closedir(dir);

    if (rmdir(path) != 0) {
        perror("Error removing directory");
        log_error(path, "Error removing directory");
    }
    return 0;
}
