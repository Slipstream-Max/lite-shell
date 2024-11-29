#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_COMMAND_LENGTH 512
#define MAX_NUM_ARGUMENTS 20
#define MAX_PIPES 10

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

int redirect_io(char** arguments, int arg_count, int* is_input_redirect,
                int* insert_point) {
  int input_fd = -1, output_fd = -1;
  // search redirect symbol
  for (int i = 0; i < arg_count; i++) {
    if (strcmp(arguments[i], ">") == 0 && i + 1 < arg_count) {
      // open output file
      output_fd = open(arguments[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (output_fd < 0) {
        perror("Error opening file for output redirection");
        return -1;
      }
      arguments[i] = NULL;
      *insert_point = i;  // this is the command end
      break;
    } else if (strcmp(arguments[i], ">>") == 0 && i + 1 < arg_count) {
      // open output append file
      output_fd = open(arguments[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (output_fd < 0) {
        perror("Error opening file for append redirection");
        return -1;
      }
      arguments[i] = NULL;
      *insert_point = i;
      break;
    } else if (strcmp(arguments[i], "<") == 0 && i + 1 < arg_count) {
      // open input file
      input_fd = open(arguments[i + 1], O_RDONLY);
      if (input_fd < 0) {
        perror("Error opening file for input redirection");
        return -1;
      }
      arguments[i] = NULL;
      *insert_point = i;
      break;
    }
  }

  // if output symbol redirect std_output to file
  if (output_fd != -1) {
    dup2(output_fd, STDOUT_FILENO);
    close(output_fd);
  }

  // if input symbol redirect file as input
  if (input_fd != -1) {
    *is_input_redirect = 1;
    dup2(input_fd, STDIN_FILENO);
    close(input_fd);
  }

  return 0;  // binding success
}

void restore_io(int saved_stdin, int saved_stdout) {
  dup2(saved_stdin, STDIN_FILENO);    // restore to stdin
  dup2(saved_stdout, STDOUT_FILENO);  // restore to stdout
  close(saved_stdin);
  close(saved_stdout);
}

int remove_folder(char* path) {
  struct stat path_stat;
  if (stat(path, &path_stat) != 0) {
    perror("Error getting directory status");
    return 1;
  }
  if (!S_ISDIR(path_stat.st_mode)) {
    fprintf(stderr, "%s is not a directory\n", path);
    return 1;
  }
  DIR* dir = opendir(path);
  if (dir == NULL) {
    perror("Error opening directory");
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
      }
    }
  }
  closedir(dir);

  if (rmdir(path) != 0) {
    perror("Error removing directory");
  }
  return 0;
}

int exec_extern_cmd(char** arguments) {
  const char* path_env = getenv("PATH");
  char* path_copy = strdup(path_env);
  char* path_dir = strtok(path_copy, ":");
  char full_path[MAX_COMMAND_LENGTH];
  // combine full path to program
  while (path_dir != NULL) {
    snprintf(full_path, sizeof(full_path), "%s/%s", path_dir, arguments[0]);
    if (access(full_path, X_OK) == 0) {
      pid_t pid = fork();
      if (pid == -1) {
        perror("fork error");
        free(path_copy);
        return -1;
      }
      if (pid == 0) {
        execv(full_path, arguments);
        perror("execv");
        exit(1);
      } else {
        // 父进程
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
  return -1;
}

int exec_piped_commands(char* commands[], int num_commands) {
  int pipefds[2 * (num_commands - 1)];  // 为每个管道创建文件描述符
  pid_t pids[MAX_PIPES];
  int i, status;

  // 为所有的管道创建文件描述符
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
      // 子进程逻辑

      // 如果不是第一个命令，把前一个管道的读端作为标准输入
      if (i > 0) {
        dup2(pipefds[2 * (i - 1)], STDIN_FILENO);
      }

      // 如果不是最后一个命令，把当前管道的写端作为标准输出
      if (i < num_commands - 1) {
        dup2(pipefds[2 * i + 1], STDOUT_FILENO);
      }

      // 关闭所有管道文件描述符，因为它们已经被重定向
      for (int j = 0; j < 2 * (num_commands - 1); j++) {
        close(pipefds[j]);
      }

      // 执行命令
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

  // 父进程逻辑：关闭所有管道文件描述符并等待所有子进程退出
  for (i = 0; i < 2 * (num_commands - 1); i++) {
    close(pipefds[i]);
  }

  for (i = 0; i < num_commands; i++) {
    waitpid(pids[i], &status, 0);
  }

  return 0;
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
  // save init dir
  if (getcwd(initial_directory, sizeof(initial_directory)) == NULL) {
    perror("getcwd failed");
    return 1;
  }
  // save stdin/out for restore
  int saved_stdin = dup(STDIN_FILENO);
  int saved_stdout = dup(STDOUT_FILENO);
  char buf[512];
  printf("######## Welcome to Shelldemo! ########\n");
  while (1) {
    is_redirect_input = 0;
    restore_io(saved_stdin, saved_stdout);
    printf("> ");

    // get&breakdown command to argument
    if (!fgets(command, sizeof(command), stdin)) {
      break;  // Exit on EOF
    }
    command[strcspn(command, "\n")] = '\0';

    // 处理管道命令
    int pipe_count = 0;
    char* pipe_token = strtok(command, "|");
    while (pipe_token != NULL && pipe_count < MAX_PIPES) {
      piped_commands[pipe_count++] = pipe_token;
      pipe_token = strtok(NULL, "|");
    }

    // 如果存在管道符号，那么执行管道命令
    if (pipe_count > 1) {
      exec_piped_commands(piped_commands, pipe_count);
      continue;
    }

    int arg_count = 0;
    int insert_point = 0;
    char* token = strtok(command, " ");
    while (token != NULL && arg_count < MAX_NUM_ARGUMENTS) {
      arguments[arg_count++] = token;
      token = strtok(NULL, " ");
    }
    arguments[arg_count] = NULL;

    // execute&handle edge situation
    int handle = 0;
    if (arg_count == 0) {
      perror("empty command.\n");
      continue;
    }

    if (strcmp(arguments[0], "quit") == 0) {
      break;
    }

    // redirect io
    if (redirect_io(arguments, arg_count, &is_redirect_input, &insert_point) ==
        -1) {
      continue;
    }

    // if re-input then reformat command line
    if (is_redirect_input == 1) {
      if (!fgets(command_fromfile, sizeof(command_fromfile), stdin)) {
        break;  // Exit on EOF
      }
      command_fromfile[strcspn(command_fromfile, "\n")] = '\0';
      arguments[insert_point] = malloc(strlen(command_fromfile) + 1);
      strcpy(arguments[insert_point], command_fromfile);
    }
    arg_count = (insert_point + 1);

    // command
    if (strcmp(arguments[0], "pwd2") == 0) {
      add_cmd_his(command, history, &history_line);
      if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("getcwd failed");
        return 1;
      }
      printf("%s\n", buf);
      continue;
    }

    if (strcmp(arguments[0], "echo2") == 0) {
      add_cmd_his(command, history, &history_line);
      printf("%s\n", arguments[1]);
      continue;
    }

    if (strcmp(arguments[0], "cd2") == 0) {
      add_cmd_his(command, history, &history_line);
      if (arg_count < 2) {
        arguments[1] = initial_directory;
      }
      if (strcmp(arguments[1], "-") == 0) {
        arguments[1] = "..";
      }
      if (chdir(arguments[1]) != 0) {
        perror("cd failed");
      }
      continue;
    }

    if (strcmp(arguments[0], "ls2") == 0) {
      add_cmd_his(command, history, &history_line);
      if (arg_count < 2) {
        arguments[1] = ".";
      }
      DIR* directory = opendir(arguments[1]);
      if (directory == NULL) {
        perror("Unable to read directory");
        continue;
      }
      struct dirent* entry;
      while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0) {
          if (entry->d_type == DT_DIR) {
            printf("%s:%s\n", "Folder", entry->d_name);
          }
          if (entry->d_type == DT_REG) {
            printf("%s:%s\n", "File", entry->d_name);
          }
        }
      }
      closedir(directory);
      continue;
    }

    if (strcmp(arguments[0], "touch2") == 0) {
      add_cmd_his(command, history, &history_line);
      if (arg_count < 2) {
        arguments[1] = "filename";
      }
      FILE* fp;
      fp = fopen(arguments[1], "w+");
      if (fp == NULL) {
        perror("fopen:");
        return 1;
      }
      fclose(fp);
      fp = NULL;
      continue;
    }

    if (strcmp(arguments[0], "cat2") == 0) {
      add_cmd_his(command, history, &history_line);
      if (arg_count < 2) {
        perror("not enough argument");
        continue;
      }
      FILE* fp = fopen(arguments[1], "rb");
      if (fp == NULL) {
        perror("Error opening file");
        return 1;
      }
      char buffer[1024];
      size_t bytesRead;
      while ((bytesRead = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        fwrite(buffer, 1, bytesRead, stdout);
      }
      if (ferror(fp)) {
        perror("Error reading file");
      }
      printf("\n");
      fclose(fp);
      continue;
    }

    if (strcmp(arguments[0], "cp2") == 0) {
      add_cmd_his(command, history, &history_line);
      if (arg_count < 3) {
        perror("not enough argument");
        continue;
      }
      FILE* fp1 = fopen(arguments[1], "rb");
      if (fp1 == NULL) {
        perror("Error opening file1");
        return 1;
      }
      FILE* fp2 = fopen(arguments[2], "wb");
      if (fp2 == NULL) {
        perror("Error opening destination file");
        fclose(fp1);  // Ensure the first file is closed before returning
        return 1;
      }
      char buffer[1024];
      size_t bytesRead;
      while ((bytesRead = fread(buffer, 1, sizeof(buffer), fp1)) > 0) {
        if (fwrite(buffer, 1, bytesRead, fp2) != bytesRead) {
          perror("Error writing to destination file");
          fclose(fp1);
          fclose(fp2);
          return 1;
        }
      }
      if (ferror(fp1)) {
        perror("Error reading file");
      }
      fclose(fp1);
      fclose(fp2);
      continue;
    }

    if (strcmp(arguments[0], "rename2") == 0) {
      add_cmd_his(command, history, &history_line);
      if (arg_count < 3) {
        perror("not enough argument");
        continue;
      }
      if (rename(arguments[1], arguments[2]) != 0) {
        perror("Error renaming file");
      }
      continue;
    }

    if (strcmp(arguments[0], "rm2") == 0) {
      add_cmd_his(command, history, &history_line);
      if (strcmp(arguments[1], "-r") == 0) {
        if (arg_count < 2) {
          perror("not enough argument");
          continue;
        }
        if (remove_folder(arguments[2]) != 0) {
          perror("Error delete folder");
        }
        continue;
      }

      if (arg_count < 2) {
        perror("not enough argument");
        continue;
      }
      if (remove(arguments[1]) != 0) {
        perror("Error delete file");
      }
      continue;
    }

    if (strcmp(arguments[0], "history2") == 0) {
      for (int i = 0; i < history_line; i++) {
        printf("%s\n", history[i]);
      }
      add_cmd_his(command, history, &history_line);
      continue;
    }

    // test if this ia a external command
    int result = exec_extern_cmd(arguments);
    if (result == -1) {
      printf("Command not found or execution failed: %s\n", arguments[0]);
    } else {
      add_cmd_his(command, history, &history_line);
    }
  }
  printf("######## Quiting Shelldemo ########\n");
  return 0;
}