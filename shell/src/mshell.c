#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>

#include "config.h"
#include "siparse.h"
#include "input.h"
#include "builtins.h"
#include "exec.h"

int process_pipeline(pipeline* pipeline_ptr);

int main(int argc, char* argv[])
{
    int err_flag = 0;
    while (!err_flag) {

        if (isatty(STDIN_FILENO)) {
            (void) dprintf(STDOUT_FILENO, PROMPT_STR); // flush()?
        }

        char* line;
        ssize_t line_len = readline(&line);

        if (line_len < 0) { // Error
            err_flag = 1;
            break;
        } else if (line_len == 0) { // EOF
            break;
        }

        pipelineseq* seq = line_len <= MAX_LINE_LENGTH ? parseline(line) : NULL;
        if (seq != NULL) {

            pipelineseq* seq_ptr = seq;
            do {
                if (process_pipeline(seq_ptr->pipeline) != 0) {
                    // something went terribly wrong - exit shell
                    err_flag = 1;
                    break;
                }
            } while ((seq_ptr = seq_ptr->next) != seq);

        } else {
            (void) dprintf(STDERR_FILENO, SYNTAX_ERROR_STR "\n");
        }

    }

    return err_flag;

}

// TODO: process dup() errors

int process_pipeline(pipeline* pipeline_ptr)
{

    if (pipeline_ptr == NULL || pipeline_ptr->commands == NULL) return 0;

    commandseq* commands = pipeline_ptr->commands;

    /* Process single commands separately */
    if (commands->next == commands) {

        command* cmd = commands->com;
        if (cmd == NULL) return 0; // allow empty line

        builtin_fun_t builtin = find_builtin_fun(cmd->args->arg);
        if (builtin != NULL) {
            exec_builtin(cmd, builtin);
            return 0;
        }

    }

    commandseq* cmd_ptr = commands;
    int cmd_count = 0;

    /* Count commands and check for empty */
    do {
        if (cmd_ptr->com == NULL) {
            (void) dprintf(STDERR_FILENO, SYNTAX_ERROR_STR "\n");
            return 0;
        }
        ++cmd_count;
    } while ((cmd_ptr = cmd_ptr->next) != commands);

    int pipe_fds[2];
    int pipe_read_fd = dup(STDIN_FILENO);

    do {

        if (cmd_ptr->next != commands) {
            pipe(pipe_fds);
        } else {
            pipe_fds[0] = dup(STDIN_FILENO);
            pipe_fds[1] = dup(STDOUT_FILENO);
        }

        pid_t pid = fork();
        if (pid > 0) {
            close(pipe_read_fd);
            pipe_read_fd = pipe_fds[0];
            close(pipe_fds[1]);
        } else if (pid == 0) {

            close(pipe_fds[0]);

            dup2(pipe_read_fd, STDIN_FILENO);
            close(pipe_read_fd);

            dup2(pipe_fds[1], STDOUT_FILENO);
            close(pipe_fds[1]);

            exec_command(cmd_ptr->com); // does not return on success
            exit(EXIT_FAILURE);

        } else { // fork() error
            return -1;
        }

    } while ((cmd_ptr = cmd_ptr->next) != commands);\

    close(pipe_read_fd);

    while (cmd_count > 0) {
        wait(NULL);
        --cmd_count;
    }

    return 0;

}
