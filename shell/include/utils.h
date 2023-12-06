#ifndef _UTILS_H_
#define _UTILS_H_

#include "siparse.h"

void printcommand(command *, int);
void printpipeline(pipeline *, int);
void printparsedline(pipelineseq *);

command * pickfirstcommand(pipelineseq *);

void vectorize(command* cmd, char** argv);

#endif /* !_UTILS_H_ */
