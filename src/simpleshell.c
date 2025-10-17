#include "simpleshell.h"

// store latest exit status globally so we can access it lazy
static int last_exit_status = 0;

int io_redirection(char **io_redirect_info, int io_redirect_info_index);
int simpleshell();

int main(int argc, char *argv[]) {
    FILE* fptr = NULL;
    
    if (argc > 1) {
        // more than 1 argument implies our shell is being called through an interpreter
        if ((fptr = fopen(argv[1], "r")) == NULL) {
            fprintf(stderr, "Failed to fopen file: %s. %s\n", argv[1], strerror(errno));
            exit(1);
        }
    } else {
        // 1 or less arguments implies our shell is being called with no arguments
        if((fptr = fdopen(0, "r")) == NULL) {
            fprintf(stderr, "Failed to fdopen standard input. %s\n", strerror(errno));
            exit(1);
        }
    }
    
    char linebuf[BUFSIZ]; // line isn't expected to be longer than BUFSIZ characters
    char command[BUFSIZ]; // command is not expected to be greater than BUFSIZ bytes long
    char *token;
    
    for (;;) { 
        // (1) read line of input from stdin
        if (fptr == stdin) {
            printf("simpleshell: ");
            fflush(stdout);
        }
        
        if(fgets(linebuf, BUFSIZ, fptr) == NULL) { 
            // EOF reached
            if (fptr == stdin) {
                fprintf(stderr, "end of file read, exiting shell with exit code %d\n", last_exit_status);
            }
            break;
        }

        if(linebuf[0] == '#') { //if the line of input is a comment, just continue (2)
            continue;
        }
        
        // if the line is empty then also skip that
        if(linebuf[0] == '\n') {
            continue;
        }

        // tokenize arguments and separate io redireciton and arguments
        char *io_redirect_info[BUFSIZ];
        int io_redirect_info_index = 0;
        char *arguments[BUFSIZ];
        int arguments_index = 0;

        if ((token = strtok(linebuf, " \t\r\n\a")) == NULL) {
            continue;
        }
        
        // first token command in every case
        strcpy(command, token);
        arguments[arguments_index++] = command;
        
        token = strtok(NULL, " \t\r\n\a");

        //format: command {argument {argument...}} {redirection operation {redirection operation}} 

        // do io parsing and redirections
        while(token != NULL) {
            if((token[0]) == '>' || token[0] == '<' || token[0] == '2') { //look for IO redirection
                //need to store IO redirection here to separate later
                io_redirect_info[io_redirect_info_index++] = token;
            } else { //everything else lets put it into arguments 
                arguments[arguments_index++] = token; 
            }
            token = strtok(NULL, " \t\r\n\a");
        }
        arguments[arguments_index] = NULL;
        
        // built in cd
        if (strcmp(command, "cd") == 0) {
            char *dir = NULL;
            // if directory not specified then use just HOME
            if (arguments_index < 2) {
                dir = getenv("HOME");
                if (dir == NULL) {
                    fprintf(stderr, "Failed to cd: HOME not set\n");
                    last_exit_status = 1;
                    continue;
                }
            } else {
                dir = arguments[1];
            }
            
            if (chdir(dir) != 0) {
                fprintf(stderr, "Failed to cd: %s: %s\n", dir, strerror(errno));
                last_exit_status = 1;
            } else {
                last_exit_status = 0;
            }
            continue;
        }
        
        // built in pwd
        if (strcmp(command, "pwd") == 0) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
                last_exit_status = 0;
            } else {
                fprintf(stderr, "Failed to print current working directory. %s\n", strerror(errno));
                last_exit_status = 1;
            }
            continue;
        }
        
        if (strcmp(command, "exit") == 0) {
            int exit_code = last_exit_status;
            if (arguments_index > 1) {
                exit_code = atoi(arguments[1]);
            }
            exit(exit_code);
        }

        // fork to create new process to run command
        struct timeval start_time, end_time;
        gettimeofday(&start_time, NULL);
        
        int pid;
        int real_time_elapsed_begin = 0;
        int user_CPU_time = 0;
        int system_CPU_time = 0;
        int status;

        // handle child and parent and default exactly like the pset said
        switch(pid = fork()) { 
            case -1:
                fprintf(stderr, "Failed to fork: %s\n", strerror(errno));
                continue; // Don't exit, continue to next line

            case 0: // need to retokenize to separate the filename to the io redirection command 
                // do io redirection inside child
                if(io_redirection(io_redirect_info, io_redirect_info_index) != 0) {
                    _exit(1); //exit with status of 1 if redirection fails
                }

                // exec in child
                if(execvp(command, arguments) == -1) {
                    fprintf(stderr, "Failed to exec %s: %s\n", command, strerror(errno));
                    _exit(127); //if execvp fails, then exits with 127 status code
                }
                
                _exit(0); //execvp works, so exits with 0

            default: 
                // just wait for command to finish exec
                struct rusage usage;
                if (wait4(pid, &status, 0, &usage) == -1) { //wait for child process to exit
                    fprintf(stderr, "Failed to do wait4: %s\n", strerror(errno));
                    continue;
                }
                
                gettimeofday(&end_time, NULL);
                
                // long time parser because i ran out of time
                double real_time_elapsed = (end_time.tv_sec - start_time.tv_sec) + 
                                           (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
                double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0;
                double sys_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
                
                // when command exited, then report info about it
                if (WIFEXITED(status)) {
                    int exit_code = WEXITSTATUS(status);
                    if (exit_code == 0) {
                        fprintf(stderr, "Child process %d exited normally\n", pid);
                    } else {
                        fprintf(stderr, "Child process %d exited with return value %d\n", pid, exit_code);
                    }
                    last_exit_status = exit_code;
                } else if (WIFSIGNALED(status)) {
                    int sig = WTERMSIG(status);
                    fprintf(stderr, "Child process %d exited with signal %d (%s)\n", pid, sig, strsignal(sig));
                    last_exit_status = 128 + sig; 
                }
                
                fprintf(stderr, "Real: %.3fs User: %.3fs Sys: %.3fs\n", 
                        real_time_elapsed, user_time, sys_time);
                
                // go to next line
                break;
        }
    }
    
    if (fptr != NULL && fptr != stdin) {
        fclose(fptr);
    }
    
    return last_exit_status;
}


int io_redirection(char **io_redirect_info, int io_redirect_info_index) {
    for(int i = 0; i < io_redirect_info_index; i++) { 
        int fd;

        if(io_redirect_info[i][0] == '<') { //simplest case <filename
            //filename is extracted - just skip the < character
            char *filename = io_redirect_info[i] + 1;

            fd = open(filename, O_RDONLY); 
            if(fd == -1) {
                fprintf(stderr, "Error opening '%s' for input: %s\n", filename, strerror(errno));
                return -1;
            }

            dup2(fd, 0);
            close(fd);
        }
        else if(io_redirect_info[i][0] == '>') { 
            char *filename;

            if(io_redirect_info[i][1] == '>') { //case: >>filename
                //double > means appending, open file with O_APPEND
                filename = io_redirect_info[i] + 2;
                fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0666); 
            } else { //case: >filename
                filename = io_redirect_info[i] + 1;
                fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            }

            if(fd == -1) {
                fprintf(stderr, "Error opening '%s' for output: %s\n", filename, strerror(errno));
                return -1;
            }

            dup2(fd, 1);
            close(fd);
        }
        else { //this is for the case of redirecting stderr 2> or 2>> 
            char *filename;
     
            if(io_redirect_info[i][1] == '>' && io_redirect_info[i][2] == '>') { //case: 2>>filename
                filename = io_redirect_info[i] + 3;
                fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0666);
            } else { //case: 2>filename
                filename = io_redirect_info[i] + 2;
                fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            }

            if(fd == -1) {
                fprintf(stderr, "Error opening '%s' for stderr: %s\n", filename, strerror(errno));
                return -1;
            }

            dup2(fd, 2);
            close(fd);
        }
    }

    return 0;
}