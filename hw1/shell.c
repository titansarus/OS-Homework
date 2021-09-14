#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"


int cmd_help(tok_t arg[]);

int cmd_pwd(tok_t arg[]);

int cmd_cd(tok_t arg[]);

int cmd_wait(tok_t arg[]);

int cmd_quit(tok_t arg[]);


/* Command Lookup table */
typedef int cmd_fun_t(
        tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
    cmd_fun_t *fun;
    char *cmd;
    char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
        {cmd_help, "?",    "show this help menu"},
        {cmd_pwd,  "pwd",  "print working directory"},
        {cmd_cd,   "cd",   "change working directory"},
        {cmd_quit, "quit", "quit the command shell"},
        {cmd_wait, "wait", "wait for background processes to complete"},
};

/**
 * Exit Shell
 */
int cmd_quit(tok_t arg[]) {
    printf("Bye\n");
    fflush(stdout);
    exit(0);
}

/**
 * Wait for background processes to end
 */
int cmd_wait(tok_t arg[]) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, 0)) > 0) {
        fprintf(stderr, "Wait ended for pid %d\n", pid);
    }
    return 1;
}

/**
 * Available built-in commands
 */
int cmd_help(tok_t arg[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++) {
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
        fflush(stdout);
    }
    return 1;
}

/**
 * Print working directory
 */
int cmd_pwd(tok_t arg[]) {
    long size;
    char *buffer;
    char *pointer_to_name;

    size = pathconf(".", _PC_PATH_MAX);
    buffer = (char *) malloc((size_t) size);
    if (buffer != NULL) {
        pointer_to_name = getcwd(buffer, (size_t) size);
        printf("%s\n", pointer_to_name);
        fflush(stdout);
    }
    return 1;
}

/**
 * Change working directory
 */
int cmd_cd(tok_t arg[]) {
    int operation_result = chdir(arg[0]);
    if (operation_result != 0) {
        printf("cd: %s: No such file or directory\n", arg[0]);
        fflush(stdout);
    }
    return 1;
}

/**
 * Check if cmd is a built-in command
 * @param cmd : command name
 * @return index : return index of command in cmd_table if found; else return -1
 */
int lookup(char cmd[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++) {
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
    }
    return -1;
}

/**
 * Ignore signals for shell.
 */
void ignore_signals() {
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

/**
 * Change signal behaviours to default. Used for child process
 */
void default_signals() {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}


/**
 * Make process boilerplate
 * @param pProcess : pointer to pointer to process that will be assigned with a new boilerplate process
 * @param pid : process id; if equal to -1, it will not be assigned
 * @param argv : if null, it will not be assigned
 */
void process_boilerplate(process **pProcess, pid_t pid, tok_t argv[]) {
    (*pProcess) = (process *) malloc(sizeof(process));
    (*pProcess)->stopped = 0;
    (*pProcess)->completed = 0;
    (*pProcess)->background = 0;
    (*pProcess)->in = STDIN_FILENO;
    (*pProcess)->out = STDOUT_FILENO;
    (*pProcess)->err = STDERR_FILENO;
    (*pProcess)->prev = NULL;
    (*pProcess)->next = NULL;
    if (pid != -1) {
        (*pProcess)->pid = getpid();
    }
    if (argv != NULL) {
        (*pProcess)->argv = argv;
    }
}

/**
 * Initialize shell. Make the shell the first process in process list. Disable signals for the shell itself.
 */
void init_shell() {
    /* Check if we are running interactively */
    shell_terminal = STDIN_FILENO;

    /** Note that we cannot take control of the terminal if the shell
        is not interactive */
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {
        /* force into foreground */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        shell_pgid = getpid();
        /* Put shell in its own process group */
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);
        tcgetattr(shell_terminal, &shell_tmodes);

        ignore_signals();
    }

    process_boilerplate(&first_process, getpid(), NULL);
}


void update_status(void) {
    int status;
    pid_t pid;
    do {
        //-1 is WAIT_ANY
        pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
    } while (!change_process_status(pid, status));
}



/**
 * redirect in of process to file indicated by <
 * @param proc : process
 * @return 0 if successful; -1 if it encountered any error.
 */
int input_redirect(process *proc) {
    tok_t *args = proc->argv;
    int input_redirect = isDirectTok(args, "<");
    int file;
    if (input_redirect != -1) {
        if (args[input_redirect + 1] == NULL) {
            perror("Invalid Syntax\n");
            return -1;
        }
        file = open(args[input_redirect + 1], O_RDONLY);
        if (file < 0) {
            perror("File Error!\n");
            return -1;
        }
        proc->in = file;
        int i;
        for (i = input_redirect; i < MAXTOKS - 2 && args[i + 2]; i++) {
            args[i] = args[i + 2];
        }
        args[i] = NULL;
        return 0;
    }
    return 0;
}


/**
 * redirect out of process to file indicated by >
 * @param proc : process
 * @return 0 if successful; -1 if it encountered any error.
 */
int output_redirect(process *proc) {
    tok_t *args = proc->argv;
    int output_redirect = isDirectTok(args, ">");
    int file;
    if (output_redirect != -1) {
        if (args[output_redirect + 1] == NULL) {
            perror("Invalid Syntax\n");
            return -1;
        }
        file = open(args[output_redirect + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (file < 0) {
            perror("File Error!\n");
            return -1;
        }
        proc->out = file;
        int i;
        for (i = output_redirect; i < MAXTOKS - 2 && args[i + 2]; i++) {
            args[i] = args[i + 2];
        }
        args[i] = NULL;
        return 0;

    }
    return 0;
}

/**
 * check if process should be put on background based on & sign in args
 * @param proc : process
 * @return 1 if it should, 0 if  not.
 */
int should_put_background(process *proc) {
    if (proc->argv != NULL) {
        if (strcmp(proc->argv[proc->argc - 1], "&") == 0) {
            proc->argv[proc->argc - 1] = NULL;
            proc->argc--;
            return 1;
        }
    }
    return 0;
}


/**
 * Creates a process given the args
 */
process *create_process(tok_t *arg) {
    if (arg == NULL || arg[0] == NULL) {
        return NULL;
    }
    process *proc;
    process_boilerplate(&proc, -1, arg);
    input_redirect(proc);
    output_redirect(proc);
    int tok_count = tokCount(proc->argv);
    proc->argc = tok_count;
    int spb = should_put_background(proc);
    proc->background = spb;
    return proc;
}

/**
 * Add a process to our process list
 */
void add_process(process *proc) {
    process *p = first_process;
    while (p->next) {
        p = p->next;
    }
    p->next = proc;
    proc->prev = p;
}


int shell(int argc, char *argv[]) {
    char *s = malloc(INPUT_STRING_SIZE + 1); /* user input string */

    tok_t *t;                                /* tokens parsed from input */
    int lineNum = 0; //Used for debugging.
    int fundex = -1;
    pid_t pid = getpid();   /* get current processes PID */
    pid_t ppid = getppid(); /* get parents PID */
    pid_t cpid, tcpid, cpgid;
    int status;

    init_shell();


    while ((s = freadln(stdin))) {
        t = getToks(s); /* break the line into tokens */
        fundex = lookup(t[0]);
        /* Is first token a shell literal */
        if (fundex >= 0) {
            cmd_table[fundex].fun(&t[1]);
        } else {
            process *proc = create_process(t);
            if (proc != NULL) {
                add_process(proc);
                cpid = fork();
                if (cpid > 0) {// Parent
                    setpgid(pid, pid);
                    proc->pid = pid;
                    if (!(proc->background)) {
                        put_process_in_foreground(proc, 0);
                    }

                } else if (cpid == 0) {  // Child
                    if (proc != NULL) {
                        default_signals();
                        proc->pid = getpid();
                        launch_process(proc);
                    }
                }
            }
        }
        update_status();

    }
    return 0;
}

