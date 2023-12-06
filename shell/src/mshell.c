#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>

#include "config.h"
#include "siparse.h"
#include "input.h"
#include "builtins.h"
#include "jobs.h"

int main(int argc, char* argv[])
{
    while (1) {

        if (isatty(STDIN_FILENO)) {
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
