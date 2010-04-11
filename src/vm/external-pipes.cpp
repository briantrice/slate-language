#include "slate.hpp"


//todo fixme do another integer mapping to handles, this assumes that all platforms use ints for pipe handles
// .. like files. but you need to separate the read/write handles since you might want to close at different
// times

word_t pipe_open(struct object_heap* oh, struct ByteArray * name, struct Object* args, word_t pipes[2]) {
  byte_t nameString[SLATE_FILE_NAME_LENGTH];
  word_t nameLength;
  int pid;
  int pipe1[2], pipe2[2];

#ifdef WIN32
  return SLATE_ERROR_RETURN;
#else
  nameLength = extractCString(name, nameString, sizeof(nameString));


  if(pipe(pipe1) == -1 || pipe(pipe2) == -1) {
    fprintf(stderr, "pipe() failed.\n");
    return SLATE_ERROR_RETURN;
  }
  int readStdin = pipe1[0], writeStdin = pipe1[0],
    readStdout = pipe2[0], writeStdout = pipe2[1];

  if((pid = fork2()) == -1) {
    fprintf(stderr, "fork() failed.\n");
    exit(1);
  }
  
  /* If we're the child. */
  if(pid == 0) {
    
    /* Close what the child doesn't need */
    close(writeStdin);
    close(readStdout);
    
    dup2(readStdin, 0);
    close(readStdin);
    dup2(writeStdout, 1);
    close(writeStdout);
    
    /* Execute ps. */
    word_t argc = object_array_size(args);
    char** argv = (char**)malloc(sizeof(char*) * (argc + 1));
    argv[argc] = (char*) NULL;

    for (word_t i = 0; i < argc; i++) {
      byte_t argString[SLATE_FILE_NAME_LENGTH];
      word_t argStringLength;
      struct ByteArray* argvi = (struct ByteArray*)object_array_get_element(args, i); 
      if (object_type((struct Object*)argvi) != TYPE_BYTE_ARRAY) {
        fprintf(stderr, "Argv[%d] != bytearray\n", (int)i);
        argv[i] = 0;
        continue;
      }

      argStringLength = extractCString(argvi, argString, sizeof(argString));
      argv[i] = (char*)malloc(argStringLength+1);
      memcpy(argv[i], argString, argStringLength);
      argv[i][argStringLength] = '\0';
      fprintf(stderr, "Argv[%d] = %s\n", (int)i, (char*)argv[i]);

    }

    fprintf(stderr, "Exec: %s\n", nameString);
    execv((const char*)nameString, argv);
  }

  /* If we're the father. */
  if(pid > 0) {
    close(readStdin);
    close(writeStdout);
    pipes[0] = writeStdin;
    pipes[1] = readStdout;
    return 2;
  }
#endif
  return SLATE_ERROR_RETURN;
}

word_t pipe_write(struct object_heap* oh, word_t file, word_t n, char * bytes) {
#ifdef WIN32
  DWORD bytesWritten = 0;
  return (WriteFile((HANDLE)file, bytes, (DWORD)n, &bytesWritten, NULL)
	  ? bytesWritten : SLATE_ERROR_RETURN);
#else
  return write(file, bytes, n);
#endif
}

word_t pipe_read(struct object_heap* oh, word_t file, word_t n, char * bytes) {
#ifdef WIN32
  DWORD bytesRead = 0;
  return (ReadFile((HANDLE)file, bytes, (DWORD)n, &bytesRead, NULL)
	  ? bytesRead : SLATE_ERROR_RETURN);
#else
  return read(file, bytes, n);
#endif
}



void pipe_close(struct object_heap* oh, word_t pipe) {
#ifdef WIN32
  CloseHandle((HANDLE)pipe);
#else
  close(pipe);
#endif
}
