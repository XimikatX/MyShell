#ifndef _BUILTINS_H_
#define _BUILTINS_H_

#define BUILTIN_ERROR 2

typedef struct {
	char* name;
	int (*fun)(char**); 
} builtin_pair;

extern builtin_pair builtins_table[];

typedef int(* builtin_fun_t)(char**);

builtin_fun_t find_builtin_fun(char* name);

#endif /* !_BUILTINS_H_ */
