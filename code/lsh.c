/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file(s)
 * you will need to modify the CMakeLists.txt to compile
 * your additional file(s).
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Using assert statements in your code is a great way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>

#include "parse.h"

#include <signal.h>

#include <sys/wait.h>
#include <errno.h>

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);
int c;
__pid_t pid;
pid_t foreground_pid = -1; //no active foreground processes
void signal_handler(int);

void execute_command(Command *cmd);
void add_bg_process(pid_t pid);
void terminate_bg_processes();
#define MAX_BG_PROCESSES 100

static pid_t bg_processes[MAX_BG_PROCESSES];
static int bg_count = 0;



int main(void) {
    signal(SIGCHLD, signal_handler);
    signal(SIGINT, signal_handler);
    for (;;) {
        char *line = readline("> ");
        
        if (line == NULL) {  

                    printf("\n(EOF) Exiting...\n");
                    terminate_bg_processes(); //exit cleanly by forcefully terminating any potential background processes
                    exit(0); 

                   /* if (bg_count > 0) {
                        printf("\n(EOF) Cannot exit: background processes running\n");
                        
                    } else {
                        printf("\n(EOF) Exiting...\n");
                        exit(0); 
                    }
                    continue; */
                }

        // Remove leading and trailing whitespace from the line
        stripwhite(line);

        if (*line) {
            add_history(line);

            Command cmd;
            if (parse(line, &cmd) == 1) {
                print_cmd(&cmd);
                execute_command(&cmd);
            } else {
                printf("Parse ERROR\n");
            }
        }
        free(line);
    }

    while (waitpid(-1, NULL, WNOHANG) > 0);
    return 0;
}

void terminate_bg_processes() { //forcefully kills all bg processes
    for (int i = 0; i < bg_count; i++) {
        kill(bg_processes[i], SIGKILL);
    }
    bg_count = 0; //resets the count to 0
}



void add_bg_process(pid_t pid) { // used to keep track of background processes
    if (bg_count < MAX_BG_PROCESSES) { //a maximum limit of bg processes is not needed, but probably good just in case
        bg_processes[bg_count++] = pid; //adds the pid of bg process to the array at index bg_count, and updates bg_count
    } else {
        fprintf(stderr, "Maximum limit of background processes reached.\n"); //standard error msg if max limit bg processes reached
    }
}
void signal_handler(int signum) {
  if (signum == SIGINT) {
  if (foreground_pid > 0) {
    //  printf("Foreground process %d interrupted. Caught signal %d\n", foreground_pid, signum);
      kill(foreground_pid, SIGINT); //preferred handling
      sleep(1);
      if (waitpid(foreground_pid, NULL, WNOHANG) == 0) { //backup handling in case first failed
          //printf("Foreground process %d did not terminate. Force killing.\n", foreground_pid);
          kill(foreground_pid, SIGKILL);
      }
      waitpid(foreground_pid, NULL, 0); 
      foreground_pid = -1;
  }
  } 
  else if (signum == SIGCHLD) {
    int status;
    pid_t pid;

    //sigchld only handles background processes
    for (int i = 0; i < bg_count; ) {
      pid = bg_processes[i]; 
      //wait for each background process, non blocking
      if (waitpid(pid, &status, WNOHANG) > 0) {
          //remove terminated processes from the array
          bg_processes[i] = bg_processes[--bg_count]; //move last process to current spot, next process will now be at index i
      } else {
          //increment if process is still running
          i++;
      }
    }
  } 
}


/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 *
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}


/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}

void execute_command(Command *cmd) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);

    } else if (pid == 0) {
        //child process
        if (!cmd->background) {
            signal(SIGINT, SIG_DFL); //no bg process allowed, interrupt Ctrl-C default
            foreground_pid = getpid(); //store child PID to track as fg process
          //  printf("Started child process with foreground PID %d\n", foreground_pid);
        } else {
            signal(SIGINT, SIG_IGN); //ignore interrupts
             
        }
        execvp(cmd->pgm->pgmlist[0], cmd->pgm->pgmlist);
        perror("execvp error");
        exit(EXIT_FAILURE);
    } else {
        //parent process
        if (cmd->background) { //bg process
          //  printf("Started background process with PID %d\n", pid);
            add_bg_process(pid); //track all bg processes by adding PID to the array
        } else { //fg process
            int status;
            foreground_pid = pid; //update fg PID variable
            while ((waitpid(pid, &status, 0)) > 0); //wait for process to finish, blocking
            foreground_pid = -1; //reset fg PID variable
        }
    }
}
