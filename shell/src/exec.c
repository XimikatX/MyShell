#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>

#include "builtins.h"
#include "siparse.h"
#include "config.h"
#include "exec.h"

#define MAX_ARGS (MAX_LINE_LENGTH / 2)

void vectorize(command* cmd, char* argv[])
{
    int argc = 0;

    argseq* arg_iter = cmd->args;
    do {
        argv[argc++] = arg_iter->arg;
        arg_iter = arg_iter->next;
    } while (arg_iter != cmd->args);

    argv[argc] = NULL;

}

void exec_builtin(command* cmd, builtin_fun_t fun)
{
    char* argv[MAX_ARGS];
    vectorize(cmd, argv);

    if (fun(argv) != 0) {
        (void) dprintf(STDERR_FILENO, "Builtin %s error.\n", cmd->args->arg);
    }
}

int redirect_io(redir* r);

void exec_command(command* cmd)
{
    if (cmd->redirs != NULL) {
        redirseq* seq = cmd->redirs;
        do {
            if (redirect_io(seq->r) == -1) return;
            seq = seq->next;
        } while (seq != cmd->redirs);
    }

    char* argv[MAX_ARGS];
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

}

int redirect_io(redir* r)
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
    } // else error?

    if (fd != -1) {

        (void) dup2(fd, IS_RIN(r->flags) ? STDIN_FILENO : STDOUT_FILENO);
        close(fd);
        return fd;

    }

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
    return -1;

}