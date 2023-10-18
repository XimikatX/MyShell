#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#include <unistd.h>
#include <sys/wait.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"

size_t arg_count(argseq* arg_seq);

void vectorize(argseq* arg_seq, char* argv[]);

void exec_command(command* cmd);

int main(int argc, char* argv[])
{
    pipelineseq* line;
    command* cmd;

    char line_buf[MAX_LINE_LENGTH];

    while (1) {

        (void) write(STDOUT_FILENO, PROMPT_STR, strlen(PROMPT_STR));

        ssize_t read_count = read(STDIN_FILENO, line_buf, MAX_LINE_LENGTH);
        if (read_count > 0) {

            line_buf[read_count - 1] = '\0';

            line = parseline(line_buf);
            if (line == NULL) {
                char* error_str = SYNTAX_ERROR_STR "\n";
                (void) write(STDERR_FILENO, error_str, strlen(error_str));
            } else {
                cmd = pickfirstcommand(line);
                if (cmd != NULL) {
                    pid_t pid = fork();
                    if (pid > 0) {
                        waitpid(pid, NULL, 0);
                    } else if (pid == 0) {

                        exec_command(cmd);

                        char* msg_fmt;
                        switch (errno) {
                            case ENOENT:
                                msg_fmt = "%s: no such file or directory\n";
                                break;
                            case EACCES:
                                msg_fmt = "%s: permission denied\n";
                                break;
                            default:
                                printf("%d\n", errno);
                                msg_fmt = "%s: exec error\n";
                        }

                        // is there a more elegant way to do this?

                        char* exec_name = cmd->args->arg;
                        char* err_msg = malloc(strlen(exec_name) + strlen(msg_fmt) - 1);
                        int msg_sz = sprintf(err_msg, msg_fmt, exec_name);

                        (void) write(STDERR_FILENO, err_msg, msg_sz);
                        free(err_msg);
                        exit(EXEC_FAILURE);

                    } // else fork() error
                }
            }

        } else { // EOF or Error
            break;
        }

    }

    return 0;

}

size_t arg_count(argseq* arg_seq)
{
    size_t count = 0;
    argseq* arg_iter = arg_seq;
    do {
        ++count;
        arg_iter = arg_iter->next;
    } while (arg_iter != arg_seq);

    return count;

}

void vectorize(argseq* arg_seq, char** argv)
{
    argseq* arg_iter = arg_seq;
    do {
        *argv++ = arg_iter->arg;
        arg_iter = arg_iter->next;
    } while (arg_iter != arg_seq);

    *argv = NULL;

}

void exec_command(command* cmd)
{
    size_t argc = arg_count(cmd->args);

    /* Successful exec() call will reclaim this memory */
    char** argv = malloc((argc + 1) * sizeof(char*));
    vectorize(cmd->args, argv);

    (void) execvp(argv[0], argv);
    free(argv);

}
