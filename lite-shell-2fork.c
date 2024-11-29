#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<dirent.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define MAX_COMMAND_LENGTH 512
#define MAX_NUM_ARGUMENTS 20

int add_cmd_his(char *command, char **history, int *history_line) {
    // Allocate memory for the new command and copy it to history
    history[*history_line] = malloc(strlen(command) + 1);
    if (history[*history_line] == NULL) {
        perror("Failed to allocate memory");
        return -1; // Return an error code
    }
    strcpy(history[*history_line], command);

    // Increment the history line counter
    (*history_line)++;
    if (*history_line >= 100) {
        *history_line = 0; // Wrap around if history is full
    }

    return 0; // Return success code
}

int remove_folder(char *path){
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        perror("Error getting directory status");
        return 1;
    }
    if (!S_ISDIR(path_stat.st_mode)){
        fprintf(stderr, "%s is not a directory\n", path);
        return 1;
    }
    DIR *dir=opendir(path);
    if (dir == NULL) {
        perror("Error opening directory");
        return 1;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip the special entries "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (entry->d_type==DT_DIR){
            remove_folder(full_path);
        }
        if (entry->d_type==DT_REG){
            if (remove(full_path)!=0){
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

int exec_extern_cmd(char **arguments){
    const char *path_env=getenv("PATH");
    char *path_copy = strdup(path_env);
    char *path_dir= strtok(path_copy,":");
    char full_path[MAX_COMMAND_LENGTH];
    //combine full path to program
    while (path_dir!=NULL){
        snprintf(full_path,sizeof(full_path),"%s/%s",path_dir,arguments[0]);
        if (access(full_path,X_OK)==0){
            pid_t pid = fork();
            if (pid==-1){
                perror("fork error");
                free(path_copy);
                return -1;
            }
            if (pid==0){
                execv(full_path, arguments);
                perror("execv");
                exit(1);
            }else{
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

int main(){
    char *history[MAX_COMMAND_LENGTH];
    char command[MAX_COMMAND_LENGTH];
    char *arguments[MAX_NUM_ARGUMENTS];
    char initial_directory[MAX_COMMAND_LENGTH];
    int history_line=0;
    int a;
    // save init dir
    if (getcwd(initial_directory, sizeof(initial_directory)) == NULL) {
        perror("getcwd failed");
        return 1;
    }
    char buf[512];
    printf("######## Welcome to Shelldemo! ########\n");
    while(1){
        printf("> ");

        //get&breakdown command to argument
        if (!fgets(command, sizeof(command), stdin)) {
            break; // Exit on EOF
        }
        command[strcspn(command, "\n")] = '\0';
        int arg_count=0;
        char *token = strtok(command, " ");
        while (token != NULL && arg_count < MAX_NUM_ARGUMENTS) {
            arguments[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        arguments[arg_count]=NULL;

        //execute&handle edge situation
        int handle=0;
        if (arg_count==0){
            perror("empty command.\n");
            continue;
        }

        if (strcmp(arguments[0], "quit") == 0){
            break;
        }

        if (strcmp(arguments[0], "pwd2") == 0){
            add_cmd_his(command, history, &history_line);
            if (getcwd(buf, sizeof(buf)) == NULL) {
                perror("getcwd failed");
                return 1;
            }
            printf("%s\n",buf);
            continue;
        }

        if (strcmp(arguments[0],"echo2") == 0){
            add_cmd_his(command, history, &history_line);
            printf("%s\n",arguments[1]);
            continue;
        }
        

        if (strcmp(arguments[0],"cd2")==0){
            add_cmd_his(command, history, &history_line);
            if(arg_count<2){
                arguments[1]=initial_directory;
            }
            if(strcmp(arguments[1],"-")==0){
                arguments[1]="..";
            }
            if(chdir(arguments[1]) != 0){
                perror("cd failed");
            }
            continue;
        }

        if (strcmp(arguments[0],"ls2")==0){
            add_cmd_his(command, history, &history_line);
            if (arg_count<2){
                arguments[1]=".";
            }
            DIR *directory = opendir(arguments[1]);
            if (directory == NULL){
                perror("Unable to read directory");
                continue;
            }
            struct dirent *entry;
            while ((entry = readdir(directory)) != NULL){
                if(strcmp(entry->d_name,".")!=0&&strcmp(entry->d_name, "..") != 0){
                    if (entry->d_type==DT_DIR){
                        printf("%s:%s\n", "Folder",entry->d_name);
                    }
                    if (entry->d_type==DT_REG){
                        printf("%s:%s\n", "File",entry->d_name);
                    }
                }
            }
            closedir(directory);
            continue;
        }

        if (strcmp(arguments[0],"touch2")==0){
            add_cmd_his(command, history, &history_line);
            if (arg_count<2){
                arguments[1]="filename";
            }
            FILE *fp;
            fp = fopen(arguments[1],"w+");
            if (fp == NULL){
                perror("fopen:");
                return 1;
            }
            fclose(fp);
            fp = NULL;
            continue;
        }
        
        if (strcmp(arguments[0],"cat2")==0){
            add_cmd_his(command, history, &history_line);
            if(arg_count<2){
                perror("not enough argument");
                continue;
            }
            FILE *fp = fopen(arguments[1], "rb");
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

        if (strcmp(arguments[0],"cp2")==0){
            add_cmd_his(command, history, &history_line);
            if(arg_count<3){
                perror("not enough argument");
                continue;
            }
            FILE *fp1 = fopen(arguments[1], "rb");
            if (fp1 == NULL) {
                perror("Error opening file1");
                return 1;
            }
            FILE *fp2 = fopen(arguments[2], "wb");
            if (fp2 == NULL) {
                perror("Error opening destination file");
                fclose(fp1); // Ensure the first file is closed before returning
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
        
        if (strcmp(arguments[0],"rename2")==0){
            add_cmd_his(command, history, &history_line);
            if(arg_count<3){
                perror("not enough argument");
                continue;
            }
            if (rename(arguments[1],arguments[2])!=0)
            {
                perror("Error renaming file");
            }
            continue;
        }
        
        if (strcmp(arguments[0],"rm2")==0){
            add_cmd_his(command, history, &history_line);
            if (strcmp(arguments[1],"-r")==0){
                if (arg_count<2){
                    perror("not enough argument");
                    continue;
                }
                if(remove_folder(arguments[2])!=0){
                    perror("Error delete folder");
                }
                continue;
            }

            if (arg_count<2){
                perror("not enough argument");
                continue;
            }
            if (remove(arguments[1]) != 0){
                perror("Error delete file");
            }
            continue;
        }
        
        if (strcmp(arguments[0],"history2")==0){
            
            for (int i = 0; i < history_line; i++){
                printf("%s\n",history[i]);
            }
            add_cmd_his(command, history, &history_line);
            continue;
        }

        //test if this ia a external command
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