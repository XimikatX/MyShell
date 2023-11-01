#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>

#include "builtins.h"

int exit_(char* []);

int echo(char* []);

int cd(char* []);

int kill_(char* []);

int ls(char* []);

int undefined(char* []);

builtin_pair builtins_table[] = {
        {"exit",  &exit_},
        {"lecho", &echo},
        {"lcd",   &cd},
        {"cd",    &cd},
        {"lkill", &kill_},
        {"lls",   &ls},
        {NULL, NULL}
};

int echo(char* argv[])
{
    int i = 1;
    if (argv[i]) printf("%s", argv[i++]);
    while (argv[i])
        printf(" %s", argv[i++]);

    printf("\n");
    fflush(stdout);
    return 0;
}

int undefined(char* argv[])
{
    fprintf(stderr, "Command %s undefined.\n", argv[0]);
    return BUILTIN_ERROR;
}

builtin_fun_t find_builtin_fun(char* name)
{
    builtin_pair* pair_ptr = builtins_table;
    while (pair_ptr->name != NULL && pair_ptr->fun != NULL) {
        if (strcmp(pair_ptr->name, name) == 0) return pair_ptr->fun;
        ++pair_ptr;
    }

    return NULL;

}

size_t arg_count(char* argv[])
{
    size_t argc = 0;
    while (argv[argc] != NULL) ++argc;
    return argc;
}

int exit_(char* argv[])
{
    if (arg_count(argv) != 1) return -1;
    exit(0);
}

int cd(char* argv[])
{
    char* path;
    switch (arg_count(argv)) {
        case 1:
            path = getenv("HOME");
            if (path == NULL) return -1;
            break;
        case 2:
            path = argv[1];
            break;
        default:
            return -1;
    }
    return chdir(path);
}

int kill_(char* argv[])
{
    char* sig_str = NULL;
    char* pid_str;

    switch (arg_count(argv)) {
        case 2:
            pid_str = argv[1];
            break;
        case 3:
            sig_str = argv[1];
            pid_str = argv[2];
            break;
        default:
            return -1;
    }

    char* end_ptr;

    errno = 0;
    pid_t pid = (pid_t) strtol(pid_str, &end_ptr, 10);
    if (errno != 0 || *end_ptr != '\0') return -1;

    int signal;
    if (sig_str != NULL) {
        if (*sig_str != '-') return -1;
        signal = (int) strtol(sig_str + 1, &end_ptr, 10);
        if (errno != 0 || *end_ptr != '\0') return -1;
    } else {
        signal = SIGTERM;
    }

    return kill(pid, signal);

}

int ls(char* argv[])
{
    if (arg_count(argv) != 1) return -1;

    DIR* dir = opendir(".");
    if (dir == NULL) return -1;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            dprintf(STDOUT_FILENO, "%s\n", entry->d_name);
        }
    }

    return 0;

}