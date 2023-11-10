#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "input.h"
#include "builtins.h"

#define MAX_ARGS (MAX_LINE_LENGTH / 2)

void exec_builtin(command* cmd, builtin_fun_t fun);

void exec_command(command* cmd);

int main(int argc, char* argv[])
{
    while (1) {

        if (isatty(STDIN_FILENO)) {
            (void) dprintf(STDOUT_FILENO, PROMPT_STR); // flush()?
        }

        char* line;
        ssize_t line_len = readline(&line);

        if (line_len <= 0) break; // EOF or Error

        pipelineseq* seq = line_len <= MAX_LINE_LENGTH ? parseline(line) : NULL;
        if (seq == NULL) {
            (void) dprintf(STDERR_FILENO, SYNTAX_ERROR_STR "\n");
        } else {

            pipelineseq* iter = seq;
            do {

                command* cmd = pickfirstcommand(iter);
                if (cmd == NULL) continue;

                builtin_fun_t fun = find_builtin_fun(cmd->args->arg);
                if (fun != NULL) {

                    exec_builtin(cmd, fun);

                } else {

                    pid_t pid = fork();
                    if (pid > 0) {
                        waitpid(pid, NULL, 0);
                    } else if (pid == 0) {
                        exec_command(cmd); // does not return on success
                        exit(EXIT_FAILURE);
                    } else { // fork() error
                        break;
                    }

                }

            } while ((iter = iter->next) != seq);

        }

    }

    return 0;

}

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

int redirect(redir* r);

void exec_command(command* cmd)
{
    if (cmd->redirs != NULL) {
        redirseq* seq = cmd->redirs;
        do {
            if (redirect(seq->r) == -1) return;
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

int redirect(redir* r)
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

        if (IS_RIN(r->flags)) {
            dup2(fd, STDIN_FILENO);
            close(fd);
        } else {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
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