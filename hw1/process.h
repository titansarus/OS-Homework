#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <signal.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

typedef struct process {
    char **argv;
    int argc;
    pid_t pid;
    int completed;
    char stopped;
    int background;
    int status;
    struct termios tmodes;
    int in, out, err;
    struct process *next;
    struct process *prev;
} process;

process *first_process; //pointer to the first process that is launched */

void launch_process(process *p);

void put_process_in_background(process *p, int cont);

void put_process_in_foreground(process *p, int cont);

int change_process_status(pid_t id, int status);


#endif
