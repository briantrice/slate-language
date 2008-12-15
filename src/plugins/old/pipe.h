#ifndef __PIPE_H__
#define __PIPE_H__
#include <stdio.h>

void * pipe_CreatePipeForCommand(const char * command);
int pipe_ClosePipe(void * handle);
int pipe_ReadIntoByteArray(void * handle, int count, char* array, int start);
int pipe_WriteFromByteArray(void * handle, int count, char* array, int start);
int pipe_IsAtEnd(void * handle);

#endif

