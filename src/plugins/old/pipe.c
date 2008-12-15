#include "pipe.h"

void * pipe_CreatePipeForCommand(const char* command) {
  return popen(command, "r+");
}

int pipe_ClosePipe(void * handle) {
  return pclose((FILE *) handle);
}

int pipe_ReadIntoByteArray(void * handle, int count, char* array, int start) {
  return fread(&array[start], 1, count, (FILE *) handle);
}

int pipe_WriteFromByteArray(void * handle, int count, char* array, int start) {
  return fwrite(&array[start], 1, count, (FILE *) handle);
}

int pipe_IsAtEnd(void * handle) {
  return feof((FILE *) handle);
}
