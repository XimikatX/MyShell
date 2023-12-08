#ifndef _EXEC_H_
#define _EXEC_H_

extern int interactive;

#define MAX_FG_PROC_COUNT (MAX_LINE_LENGTH / 2)

extern pid_t fg[];
extern int fg_proc_count;

#define MAX_BG_LOG_SIZE 64

typedef struct {
    pid_t pid;
    int status;
} bg_log_entry;

extern bg_log_entry bg_log[];
extern int bg_log_size;

void print_bg_log();

void sigchld_handler(int signal);

void launch_command(command* cmd);

void launch_pipeline(pipeline* pipeline_ptr);

#endif // !_EXEC_H_
