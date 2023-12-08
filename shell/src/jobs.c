#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "builtins.h"
#include "siparse.h"
#include "config.h"
#include "jobs.h"
#include "utils.h"

pid_t fg[MAX_FG_PROC_COUNT];
int fg_proc_count = 0;

bg_log_entry bg_log[MAX_BG_LOG_SIZE];
int bg_log_size = 0;

static void sigchld_block(sigset_t* old_set_ptr)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &set, old_set_ptr);
}

static void sigchld_unblock()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

void print_bg_log()
{
    bg_log_entry log[MAX_BG_LOG_SIZE];
    int log_size;

    sigchld_block(NULL);
    memcpy(log, bg_log, sizeof(bg_log_entry) * bg_log_size);
    log_size = bg_log_size;
    bg_log_size = 0;
    sigchld_unblock();

    for (int i = 0; i < log_size; ++i) {

        char* msg_fmt = NULL;
        int value;

        int status = log[i].status;
        if (WIFEXITED(status)) {
            msg_fmt = "Background process %ld terminated. (exited with status %d)\n";
            value = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            msg_fmt = "Background process %ld terminated. (killed by signal %d)\n";
            value = WTERMSIG(status);
        }

        if (msg_fmt != NULL) {
            (void) dprintf(STDOUT_FILENO, msg_fmt, (long) log[i].pid, value);
        }

    }

}

void sigchld_handler(int signal)
{
    pid_t pid;
    int status;
    while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) > 0) {

        int fg_index = -1;
        for (int i = 0; i < fg_proc_count; ++i) {
            if (fg[i] == pid) {
                fg_index = i;
                break;
            }
        }

        if (fg_index != -1) { // fg process
            --fg_proc_count;
            fg[fg_index] = fg[fg_proc_count];
        } else if (bg_log_size < MAX_BG_LOG_SIZE) { // bg process
            bg_log_entry entry = {pid, status};
            bg_log[bg_log_size++] = entry;
        }

    }
}

static void redirect_io(redir* r);

// TODO: refactor into smaller functions

void launch_command(command* cmd)
{
    if (cmd->redirs != NULL) {
        redirseq* seq = cmd->redirs;
        do {
            redirect_io(seq->r);
        } while ((seq = seq->next) != cmd->redirs);
    }

    char* argv[MAX_ARG_COUNT];
    vectorize(cmd, argv);

    (void) execvp(argv[0], argv);

    char* errmsg_fmt;
    switch (errno) {
        case ENOENT:
            errmsg_fmt = "%s: no such file or directory\n";
            break;
        case EACCES:
            errmsg_fmt = "%s: permission denied\n";
            break;
        default:
            errmsg_fmt = "%s: exec error\n";
    }

    (void) dprintf(STDERR_FILENO, errmsg_fmt, cmd->args->arg);
    exit(EXEC_FAILURE);

}

void launch_pipeline(pipeline* pipeline_ptr)
{
    if (pipeline_ptr == NULL || pipeline_ptr->commands == NULL) return;

    commandseq* commands = pipeline_ptr->commands;

    /* Process single commands separately */
    if (commands->next == commands) {

        command* cmd = commands->com;
        if (cmd == NULL) return; // allow empty line

        builtin_cmd_t builtin = find_builtin_fun(cmd->args->arg);
        if (builtin != NULL) {
            exec_builtin(cmd, builtin);
            return;
        }

    }

    commandseq* cmd_ptr = commands;
    int background = pipeline_ptr->flags & INBACKGROUND;

    /* Check for empty commands */
    do {
        if (cmd_ptr->com == NULL) {
            (void) dprintf(STDERR_FILENO, SYNTAX_ERROR_STR "\n");
            return;
        }
    } while ((cmd_ptr = cmd_ptr->next) != commands);

    pid_t job_pgid = 0;

    // TODO: refactor piping

    int pipe_des[2];
    int infile = dup(STDIN_FILENO);

    do {

        if (cmd_ptr->next != commands) {
            (void) pipe(pipe_des);
        } else {
            pipe_des[0] = dup(STDIN_FILENO);
            pipe_des[1] = dup(STDOUT_FILENO);
        }

        sigchld_block(NULL);
        pid_t pid = fork();
        if (!background && pid > 0) fg[fg_proc_count++] = pid;
        sigchld_unblock();

        if (pid > 0) {

            close(infile);
            infile = pipe_des[0];
            close(pipe_des[1]);

            if (interactive) {
                if (job_pgid == 0) job_pgid = pid;
                setpgid(pid, job_pgid);
            }

        } else if (pid == 0) {

            close(pipe_des[0]);

            dup2(infile, STDIN_FILENO);
            close(infile);

            dup2(pipe_des[1], STDOUT_FILENO);
            close(pipe_des[1]);

            if (interactive) {

                if (job_pgid == 0) job_pgid = getpid();
                setpgid(getpid(), job_pgid);
                if (!background) tcsetpgrp(STDIN_FILENO, job_pgid);

                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);

            }

            launch_command(cmd_ptr->com); // does not return

        } else { // fork() error
            exit(1);
        }

    } while ((cmd_ptr = cmd_ptr->next) != commands);

    close(infile);

    if (!background) {

        tcsetpgrp(STDIN_FILENO, job_pgid);

        sigset_t old_set;
        sigchld_block(&old_set);
        while (fg_proc_count > 0) {
            sigsuspend(&old_set);
        }
        sigchld_unblock();

        tcsetpgrp(STDIN_FILENO, getpgrp());

    }

}

void redirect_io(redir* r)
{
    int fd = -1;
    if (IS_RIN(r->flags)) {
        fd = open(r->filename, O_RDONLY);
    } else if (IS_ROUT(r->flags)) {
        fd = open(r->filename, O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    } else if (IS_RAPPEND(r->flags)) {
        fd = open(r->filename, O_WRONLY | O_CREAT | O_APPEND,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    }

    if (fd != -1) {
        dup2(fd, IS_RIN(r->flags) ? STDIN_FILENO : STDOUT_FILENO);
        close(fd);
    } else { // open() error

        char* errmsg_fmt;
        switch (errno) {
            case ENOENT:
                errmsg_fmt = "%s: no such file or directory\n";
                break;
            case EACCES:
                errmsg_fmt = "%s: permission denied\n";
                break;
            default:
                errmsg_fmt = "%s: redirection error\n";
        }

        (void) dprintf(STDERR_FILENO, errmsg_fmt, r->filename);
        exit(EXEC_FAILURE);

    }

}
