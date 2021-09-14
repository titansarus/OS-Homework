#include "process.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>

#include "parse.h"
#include "shell.h"

/**
 * Tries to run the process proc using PATH Environment variable.
 */
int run_from_path(struct process const *proc) {
  tok_t *t = proc->argv;
  char *path_address = malloc(2048 * sizeof(char));
  char *path = getenv("PATH");
  tok_t *path_toks = getToks(path);
  int i = 0;
  while (path_toks[i]) {
    strcpy(path_address, path_toks[i]);
    strcat(path_address, "/");
    strcat(path_address, t[0]);
    if (access(path_address, F_OK) == 0) {
      execv(path_address, &t[0]);
      return 1;
    }

    i++;
  }
  return 0;
}

/**
 * Executes the process p.
 * If the shell is in interactive mode and the process is a foreground process,
 * then p should take control of the terminal.
 */
void launch_process(process *p) {
  dup2(p->in, STDIN_FILENO);
  dup2(p->out, STDOUT_FILENO);
  int execute_well = run_from_path(p);
  if (!execute_well) {
    if (access(p->argv[0], F_OK) == 0) {
      execv(p->argv[0], &(p->argv[0]));
      execute_well = 1;
    }
  }
  if (!execute_well) {
    printf("File not found!");
    //        exit(1);
  }
  //    exit(0);

  /** YOUR CODE HERE */
}

/* Put a process in the foreground. This function assumes that the shell
 * is in interactive mode. If the cont argument is true, send the process
 * group a SIGCONT signal to wake it up.
 */
void put_process_in_foreground(process *p, int cont) {
  int status;
  tcsetpgrp(STDIN_FILENO, p->pid);

  //-1 is WAIT_ANY. VSCode intellisense can't find WAIT_ANY. So I Hardcoded
  //that.
  waitpid(-1, &status, WUNTRACED);

  tcsetpgrp(STDIN_FILENO, first_process->pid);
}

/* Put a process in the background. If the cont argument is true, send
 * the process group a SIGCONT signal to wake it up. */
void put_process_in_background(process *p, int cont) {
  ;  // Do Nothing. If it is not in foreground, it is in background;
  /** YOUR CODE HERE */
}

int change_process_status(pid_t id, int status) {
  process *proc;
  if (id > 0) {
    for (proc = first_process; proc; proc = proc->next) {
      if (proc->pid == id) {
        proc->status = status;
        proc->completed = 1;
        return 0;
      }
    }
  }
  return -1;
}

int check_if_some_background_process_running() {
  process *proc;
  for (proc = first_process; proc; proc = proc->next) {
    if (proc->background == 1 && (proc->completed == 0)) {
      return 1;
    }
  }
  return 0;
}
