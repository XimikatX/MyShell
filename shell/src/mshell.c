#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/wait.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "input.h"
#include "builtins.h"

#define MAX_ARGS (MAX_LINE_LENGTH / 2)

int exec_builtin(command* cmd, builtin_fun_t fun);
int exec_command(command* cmd);

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

            command* cmd = pickfirstcommand(seq);
            if (cmd == NULL) continue;

            builtin_fun_t fun = find_builtin_fun(cmd->args->arg);
            if (fun != NULL) {

                int code = exec_builtin(cmd, fun);
                if (code != 0) {
                    (void) dprintf(STDERR_FILENO, "Builtin %s error.\n", cmd->args->arg);
                }

            } else {

                pid_t pid = fork();
                if (pid > 0) {
                    waitpid(pid, NULL, 0);
                } else if (pid == 0) {

                    (void) exec_command(cmd); // does not return on success

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

                } else { // fork() error
                    break;
                }

            }

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

int exec_builtin(command* cmd, builtin_fun_t fun)
{
    char* argv[MAX_ARGS];
    vectorize(cmd, argv);

    return fun(argv);
}

int exec_command(command* cmd)
{
    char* argv[MAX_ARGS];
    vectorize(cmd, argv);

    return execvp(argv[0], argv);
}
