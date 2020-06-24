#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#define L_MAX 1024
#define A_MAX 64
#define M_MAX 256


//Parse environment directories **size=12
char** parse_directories(int* size){
    char* env = getenv("MYPATH");
    if(env == NULL){
        perror("Environment not found");
        return NULL;
    }
    char** env_dirs = calloc(M_MAX, sizeof(char*));
    env_dirs[0] = calloc(A_MAX, sizeof(char));
    int pos = 0; int dir_num = 0;
    for(int i = 0; i < strlen(env) - 1; ++i){
        if(env[i] == ':'){
            env_dirs[dir_num][pos] = '/';
            if(i != strlen(env) - 2){
                ++dir_num; pos = 0;
                env_dirs[dir_num] = calloc(A_MAX, sizeof(char));
            }
        }
        else{
            env_dirs[dir_num][pos] = env[i];
            ++pos;
        }
    }
    ++dir_num;
    *size = dir_num;
    return env_dirs;
}

//Pares the command line into individual arguments
int parse_arguments(char** args, char* command_line){
    if(command_line == NULL) return 0;
    if(command_line[0] == '\0') return 0;

    int pos = 0; int comm_num = 0;
    if(args[0] == NULL) args[0] = calloc(A_MAX, sizeof(char));
    for(int i = 0; i < strlen(command_line); ++i){
        if(command_line[i] == ' '){
            args[comm_num][pos] = '\0';
            ++comm_num; pos = 0;
            if(args[comm_num] == NULL)
             args[comm_num] = calloc(A_MAX, sizeof(char));
        }
        else{
            args[comm_num][pos] = command_line[i];
            ++pos;
        }
    }
    
    args[comm_num][pos] = '\0';
    int total = comm_num + 1;
    if(args[total] != NULL) free(args[total]);
    args[total] = NULL;
    return total;
}

//Populates dir with the directory string if valid and returns 1
//Returns 0 if invalid and dir is emptied.
int search_directories(char** directories, char* dir, char* command, int num_dirs){
   struct stat SB;
    dir[0] = '\0';
   for(int i = 0; i < num_dirs; ++i){
       strcat(dir, directories[i]);
       strcat(dir, command);
       int rc = lstat(dir, &SB);
        if(rc == -1){
            dir[0] = '\0';
        }
        else{
            return 1;
        }
       
   }

   return 0;
}

//If the command is cd or exit, handle that and return 1
//If not return 0
//If cd parameters are invalid, exit to stderr (and -1)
int handle_cd(char** args, int argc){
    if(argc != 2) return -1;
    return chdir(args[1]);
}

int next_bp_index(int* bp_list, int size){
    for(int i = 0; i < size; ++i){
        if(bp_list[i] == -1) return i;
    }
#if DEBUG_MODE
printf("We have a big fucking problem...");
#endif

    return 0;
}

int main(){

    //Memory allocation and variable assignment
    int num_dirs = 0;
    char** env_dirs = parse_directories(&num_dirs);
    if(env_dirs == NULL) return EXIT_FAILURE;
    char** args = calloc(M_MAX, sizeof(char*));
    char* input = calloc(L_MAX, sizeof(char));
    char* dir = calloc(A_MAX, sizeof(char));
    pid_t* bp_list = calloc(M_MAX, sizeof(pid_t));
    for(int i = 0; i < M_MAX; ++i){
        bp_list[i] = -1;
    }
    
    //The process loop
    int arg_c = 0;
    while(1){
        //Clear finished background processes
        for(int i = 0; i < M_MAX; ++i){
            int s;
            if(bp_list[i] != -1){
                pid_t done_pid = waitpid(bp_list[i], &s, WNOHANG);
                //Process done
                if(done_pid != 0){
                    bp_list[i] = -1;
                    if(WIFSIGNALED(s)){
                        printf("[pid %d terminated abnormally]", done_pid);
                    }
                    else{
                        int es = WEXITSTATUS(s);
                        printf("[pid %d terminated with exit status %d]",done_pid, es);
                    }
                }

            }
        }

        //----------------------------------

        int is_bp = 0;
        printf("%s$ ", getcwd(input, L_MAX));
        fgets(input, L_MAX, stdin);
        input[strlen(input) - 1] = '\0';
        //Determine background processing for op and remove it from input
        if(input[strlen(input) - 1] == '&'){
            is_bp = 1;
            input[strlen(input) - 2] = '\0'; //eliminate " &"
        }
        arg_c = parse_arguments(args, input);
        if(arg_c == 0){
            continue;
        } 

        if(strcmp(input, "exit") == 0) break;
        if (strcmp(args[0], "cd") == 0){
            int valid = handle_cd(args, arg_c);

            if(valid == 0){
                perror( "cd failed" );
            }
            continue;
        } 

        
        
        //lstat failure
        if(search_directories(env_dirs, dir, args[0], num_dirs) == 0){
            perror("lstat() failed");
        } 
        
        else{
            
            pid_t pid = fork();
            //Failed Fork
            if ( pid == -1 ){
                perror( "fork() failed" );
                return EXIT_FAILURE;
            }

            //Child Process: Execute command
            if(pid == 0){
                execv(dir, args);
                
            }

            //Foreground processing
            else if(is_bp == 0){
                int status;
                pid_t child_pid = waitpid(pid, &status, 0);
                
                if(WIFSIGNALED(status)){
                    printf("Child exited abnormally\n");
                }
                else{
                    int exit_status = WEXITSTATUS(status);
                    printf("Child (%d) exited successfully with code %d\n",child_pid, exit_status);
                }
                
                
            }
            
            //Run this process in background
            else{
                bp_list[next_bp_index(bp_list, M_MAX)] = pid;
            }
        } 
        
        
        
        //Determine working directory [Check!]
        //Take command input using fgets [Check!]
        //Parse command for validity [Check!]
        //Determine if a fork() is needed
        //If not, execute the command raw
            //Handle cd
            //Handle exit [Check!]
        //If so, fork() and waitpid() the parent
        //Unless stated otherwise by args


    }
    
    for(int i = 0; i < num_dirs; ++i){
        free(env_dirs[i]);
    }
    free(env_dirs);
    
    for(int i = 0; i < M_MAX; ++i){
        free(args[i]);
    }
    free(args);
    free(input);
    free(dir);
    free(bp_list);
    return EXIT_SUCCESS;
}