#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "minishell.h"


char *homedir;
char *user;
char prompt[1024];

void hsi(int sig) {
    printf("\nCannot terminate shell with CTRL-C.\n");
}

void hsc(int sig){
    int olderrno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = olderrno;
}

void path(){
    struct passwd *pw = getpwuid(getuid());
    homedir = pw->pw_dir;
    user = pw->pw_name;
    
    if (getcwd(prompt, sizeof(prompt)) != NULL) {
        char *p = strstr(prompt, homedir);
        if (p != NULL) {
            memmove(p, "~", 1);
            memmove(p+1, p+strlen(homedir), strlen(p+strlen(homedir))+1);
        }
    }
}

void cd(char **args) {
    struct passwd *pw;
    char *home_dir;
   
    if (args[1] == NULL || strcmp(args[1], "~") == 0) {
        pw = getpwuid(getuid());
        home_dir = pw->pw_dir;
        if (chdir(home_dir) != 0) {
            fprintf(stderr, "Error: Cannot change directory to %s. %s.\n", home_dir, strerror(errno));
        }
        else{
            chdir("shared");
            path();
        }
    }  
    else  if (args[2] != NULL) {
        fprintf(stderr, "Error: Too many arguments to cd.\n");
    }
    else {
        if (chdir(args[1]) != 0) {
            fprintf(stderr, "Error: Cannot change directory to %s. %s.\n", args[1], strerror(errno));
        }
        else{
            path();
        }
    }
}



void execute_command(char **args) {
    pid_t pid;
   

    if ((pid = fork())< 0) {
        fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
        killpg(getpid(), SIGTERM);
        exit(1);
    }
    if(pid!=0){
        printf("pid: %d cmd: %s\n", pid, args[0]);
    }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
            killpg(getpid(), SIGTERM);
            exit(1);
        }
    } 
    else {
        int status;
        if (waitpid(pid, &status, WUNTRACED) == -1) {
            fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));
            killpg(getpid(), SIGTERM);
            exit(1);
        }
        printf("pid: %d done\n", pid);
    }
}


int main(){
    char input[1024];
    char *tokens[64];
    int num_tokens;

    path();

    if (signal(SIGINT, hsi) == SIG_ERR) {
        fprintf(stderr, "Error: Cannot register signal handler. %s.\n", strerror(errno));
        exit(1);
    }

    if (signal(SIGCHLD, hsc) == SIG_ERR) {
        fprintf(stderr, "Error: Cannot register signal handler. %s.\n", strerror(errno));
        exit(1);
    }

    int bg = 0;

    while (1) {
        bg = 0;
        printf("\033[0;32mminishell");
        printf("\x1B[37m:");
        printf("\033[0;35m%s", user);
        printf("\x1B[37m:");
        printf("\033[0;34m%s", prompt);
        printf("\x1B[37m$ ");
        if (!fgets(input, 1024, stdin)) {
            fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
            killpg(getpid(), SIGTERM);
            exit(1);
        }
        if(input[0]!='\n'){
            for(int i = 0; i<1024; i++){
                if(input[i] == '&'){
                    input[i] = '\0';
                    bg = 1;
                }
            }

            num_tokens = 0;
            tokens[num_tokens] = strtok(input, " \t\n");
            while (tokens[num_tokens] != NULL && num_tokens < 64) {
                num_tokens++;
                tokens[num_tokens] = strtok(NULL, " \t\n");
            }
            if(strcmp(tokens[0], "cd") == 0 && bg ==0){
                cd(tokens);
            }
            else if(strcmp(tokens[0], "exit")==0 && bg == 0){
                if(tokens[1]==NULL){
                    exit(0);
                }
                else{
                    fprintf(stderr, "Error: Too many arguments to exit.\n");
                    exit(1);
                }
            }
            else if (bg==1 && (strcmp(tokens[0],"exit") == 0) || (strcmp(tokens[0], "cd") == 0)) {
            printf("%s cannot be run in the background\n", tokens[0]);
            }
            
            else if(bg==1){
                execute_command(tokens);
            }
        }
    }
    killpg(getpid(), SIGTERM);
    exit(0);
}