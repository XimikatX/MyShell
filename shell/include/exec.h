#ifndef _EXEC_H_
#define _EXEC_H_

void exec_builtin(command* cmd, builtin_fun_t fun);

void exec_command(command* cmd);

#endif // !_EXEC_H_
