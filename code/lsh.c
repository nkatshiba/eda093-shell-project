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

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);
int c;

void execute_command(char *);


int main(void)
{
  for (;;)
  {
    char *line;
    line = readline("> ");

    if (line == NULL) {
        printf("\n(EOF) Exiting...\n");
        break;
    }

    // Remove leading and trailing whitespace from the line
    stripwhite(line);


    // If stripped line not blank
    if (*line)
    {
      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        // Just prints cmd
        //print_cmd(&cmd);
        /*if (system(line) == -1) {*/
        /*    perror("system");*/
        /*}*/
        if (strchr(line, '|')) {
          execute_pipes(line);
        }
        else {
          execute_command(line);
        }
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }
    // Clear memory
    free(line);
  }
  return 0;
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

void execute_command(char *command) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // child
        char *argv[64]; // Assuming a maximum of 64 arguments
        int i = 0;

        // GPT
        char *token = strtok(command, " ");
        while (token != NULL && i < 63) { // Leave space for NULL terminator
            argv[i++] = token;
            token = strtok(NULL, " ");
        }
        argv[i] = NULL; // NULL-terminate the argument list
        // \\ GPT

        // exec
        if (execvp(argv[0], argv) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else {
        // parent
        int status;
        waitpid(pid, &status, 0);
    }
}


void execute_pipes(char *command) {
  int pipefd[2];    //array used to return two fd's: the read and write ends of the pipe

  pid_t pid;    

  int prev_input_fd = 0;    //fd used to store the input from previous command

  char *cmd = strtok(command, "|");    //separates command using the pipe sign as a separator, points at first command

  while (cmd != NULL) {    //iterate for every command
    if (pipe(pipefd) == -1) {  //create pipe
      perror("pipe");
      exit(EXIT_FAILURE);
    }

    pid = fork();    //fork
    if (pid == -1) {
      perror("fork");
      exit(EXIT_FAILURE);
    }

    if (pid == 0) {    //child 
      if (prev_input_fd != 0) {    //check if there is previous input from a pipe
        dup2(prev_input_fd, STDIN_FILENO);    //set read end of pipe to be standard input for the previous command input
        close(prev_input_fd);    //close read end of pipe
      }
      
      if (strtok(NULL, "|") != NULL) {    //check if this is the last command
        dup2(pipefd[1], STDOUT_FILENO);    //iset write end of pipe to be standard input for the next command
      }                                    //NOTE: if this is last command, it sends the output to terminal instead of redirecting the output to the pipe
      
      //avoid leaking file descriptors
      close(pipefd[0]);    //close read end
      close(pipefd[1]);    //close write end

      execute_command(cmd);    //execute command
      
      exit(EXIT_SUCCESS);
    }
    else {    //parent
      wait(NULL);    //wait for child

      close(pipefd[1]);    //close write end of pipe

      prev_input_fd = pipefd[0];    //save read end of pipe for next loop

      cmd = strtok(NULL,"|");    //cmd points at the next command for next loop
    }
  } 

if (prev_input_fd != 0) {
  close(prev_input_fd);    //close remaining fd to avoid leaks
}
}