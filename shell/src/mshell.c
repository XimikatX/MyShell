#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "config.h"
#include "siparse.h"
#include "input.h"
#include "jobs.h"

int interactive;

void init()
{
    struct sigaction act;
    act.sa_handler = sigchld_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGCHLD, &act, NULL);

    interactive = isatty(STDIN_FILENO);
    if (interactive) {

        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        pid_t pid = getpid();
        setpgid(pid, pid);

        tcsetpgrp(STDIN_FILENO, pid);

    }

}

int main(int argc, char* argv[])
{
    init();
    while (1) {

        if (interactive) {
            print_bg_log();
            (void) dprintf(STDOUT_FILENO, PROMPT_STR);
        }

        char* line;
        ssize_t line_len = readline(&line);

        if (line_len <= 0) return line_len != 0; // Error or EOF

        pipelineseq* seq = line_len <= MAX_LINE_LENGTH ? parseline(line) : NULL;
        if (seq != NULL) {

            pipelineseq* seq_ptr = seq;
            do {
                launch_pipeline(seq_ptr->pipeline);
            } while ((seq_ptr = seq_ptr->next) != seq);

        } else {
            (void) dprintf(STDERR_FILENO, SYNTAX_ERROR_STR "\n");
        }

    }

}
