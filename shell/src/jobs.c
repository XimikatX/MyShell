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

static void redirect_io(redir* r)
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
    } // else error

    if (fd != -1) {
        dup2(fd, IS_RIN(r->flags) ? STDIN_FILENO : STDOUT_FILENO);
        close(fd);
    } else {

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

        builtin_fun_t builtin = find_builtin_fun(cmd->args->arg);
        if (builtin != NULL) {
            exec_builtin(cmd, builtin);
            return;
        }

    }

    commandseq* cmd_ptr = commands;
    int cmd_count = 0;

    /* Count commands and check for empty */
    do {
        if (cmd_ptr->com == NULL) {
            (void) dprintf(STDERR_FILENO, SYNTAX_ERROR_STR "\n");
            return;
        }
        ++cmd_count;
    } while ((cmd_ptr = cmd_ptr->next) != commands);

    int pipe_des[2];
    int infile = dup(STDIN_FILENO);

    // TODO: refactor piping

    do {

        if (cmd_ptr->next != commands) {
            (void) pipe(pipe_des);
        } else {
            pipe_des[0] = dup(STDIN_FILENO);
            pipe_des[1] = dup(STDOUT_FILENO);
        }

        pid_t pid = fork();
        if (pid > 0) {
            close(infile);
            infile = pipe_des[0];
            close(pipe_des[1]);
        } else if (pid == 0) {

            close(pipe_des[0]);

            dup2(infile, STDIN_FILENO);
            close(infile);

            dup2(pipe_des[1], STDOUT_FILENO);
            close(pipe_des[1]);

            launch_command(cmd_ptr->com); // does not return

        } else { // fork() error
            exit(1);
        }

    } while ((cmd_ptr = cmd_ptr->next) != commands);

    close(infile);

    while (cmd_count > 0) {
        wait(NULL);
        --cmd_count;
    }

}