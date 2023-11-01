#ifndef _INPUT_H_
#define _INPUT_H_

#include "config.h"

#define BUF_SIZE (MAX_LINE_LENGTH * 2)
#define ALIGN_THRESHOLD (BUF_SIZE / 8)

ssize_t readline(char** line_ptr);

#endif /* !_INPUT_H_ */
