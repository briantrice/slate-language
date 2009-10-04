#include "slate.hpp"

void error(const char* str) {
  fprintf(stderr, "%s", str);
  assert(0);
}

void fill_bytes_with(byte_t* dst, word_t n, byte_t value) {
#ifdef SLATE_NO_MEM_PRIMS
  while (n > 0)
  {
    *dst = value;
    dst++;
    n--;
  }
#else
  memset(dst, value, n);
#endif
}

void fill_words_with(word_t* dst, word_t n, word_t value) {
#ifdef SLATE_NO_MEM_PRIMS
  while (n > 0)
  {
    *dst = value;
    dst++;
    n--;
  }
#else
  /*fixme*/
  while (n > 0)
  {
    *dst = value;
    dst++;
    n--;
  }
#endif
}

void copy_words_into(void * src, word_t n, void * dst) {
#ifdef SLATE_NO_MEM_PRIMS
  if ((src < dst) && ((src + n) > dst))
  {
    dst = dst + n;
    src = src + n;
    
    {
      do
      {
        dst = dst - 1;
        src = src - 1;
        *dst = *src;
        n = n - 1;
      }
      while (n > 0);
    }
  }
  else
    while (n > 0)
    {
      *dst = *src;
      dst = dst + 1;
      src = src + 1;
      n = n - 1;
    }
#else
  memcpy(dst, src, n*sizeof(word_t));
#endif
}

void copy_bytes_into(byte_t * src, word_t n, byte_t * dst) {
#ifdef SLATE_NO_MEM_PRIMS
  if ((src < dst) && ((src + n) > dst))
  {
    dst = dst + n;
    src = src + n;
    
    {
      do
      {
        dst = dst - 1;
        src = src - 1;
        *dst = *src;
        n = n - 1;
      }
      while (n > 0);
    }
  }
  else
    while (n > 0)
    {
      *dst = *src;
      dst = dst + 1;
      src = src + 1;
      n = n - 1;
    }
#else
  memcpy(dst, src, n);
#endif
}

// For platform information:
#ifdef WIN32

int uname(struct utsname *un) {
  DWORD dwVersion = 0;
  DWORD dwMajorVersion = 0;
  DWORD dwMinorVersion = 0;
  DWORD dwBuild = 0;
#ifdef WIN32
#ifdef WIN64
  strcpy(un->sysname,"Win64");
#else
  strcpy(un->sysname,"Win32");
#endif
#endif
  dwVersion = GetVersion();
  if (dwVersion < 0x80000000) dwBuild = (DWORD)(HIWORD(dwVersion)); else dwBuild=0;
  sprintf(un->release, "%d", dwBuild);
  sprintf(un->version, "%d %d", (DWORD)(LOBYTE(LOWORD(dwVersion))), (DWORD)(HIBYTE(LOWORD(dwVersion))));
#ifdef WIN32
#ifdef WIN64
  strcpy(un->machine,"x64");
#else
  strcpy(un->machine,"x86");
#endif
#endif
  if(gethostname(un->nodename, 256)!=0) strcpy(un->nodename, "localhost");
  return 0;
};
#endif

#ifdef WIN32
int getpid() {
  return GetCurrentProcessId();
}
#endif

int64_t getTickCount() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}


void cache_specials(struct object_heap* heap) {

 heap->cached.interpreter = (struct Interpreter*) get_special(heap, SPECIAL_OOP_INTERPRETER);
 heap->cached.true_object = (struct Object*) get_special(heap, SPECIAL_OOP_TRUE);
 heap->cached.false_object = (struct Object*) get_special(heap, SPECIAL_OOP_FALSE);
 heap->cached.nil = (struct Object*) get_special(heap, SPECIAL_OOP_NIL);
 heap->cached.primitive_method_window = (struct Object*) get_special(heap, SPECIAL_OOP_PRIMITIVE_METHOD_WINDOW);
 heap->cached.compiled_method_window = (struct Object*) get_special(heap, SPECIAL_OOP_COMPILED_METHOD_WINDOW);
 heap->cached.closure_method_window = (struct Object*) get_special(heap, SPECIAL_OOP_CLOSURE_WINDOW);

}

#ifndef WIN32
word_t max(word_t x, word_t y) {
  if (x > y) return x;
  return y;
}
#endif

/*obsolete*/
word_t write_args_into(struct object_heap* oh, char* buffer, word_t limit) {

  word_t i, iLen, totalLen;

  totalLen = 0;
  for (i = 0; i < oh->argcSaved; i++) {
    iLen = strlen(oh->argvSaved[i]) + 1;
    if (totalLen + iLen >= limit) return totalLen;
    memcpy (buffer + totalLen, oh->argvSaved[i], iLen);
    totalLen += iLen;
  }
  return totalLen;

}

word_t byte_array_extract_into(struct ByteArray * fromArray, byte_t* targetBuffer, word_t bufferSize)
{
  word_t payloadSize = payload_size((struct Object *) fromArray);
  if (bufferSize < payloadSize)
    return SLATE_ERROR_RETURN;

  copy_bytes_into((byte_t*) fromArray -> elements, bufferSize, targetBuffer);
  
  return payloadSize;
}

word_t calculateMethodCallDepth(struct object_heap* oh) {
  struct Interpreter* i = oh->cached.interpreter;
  word_t fp = i->framePointer;
  word_t depth = 0;
  do {
    /*order matters here*/
    fp = object_to_smallint(i->stack->elements[fp-1]);
    if (fp < FUNCTION_FRAME_SIZE) break;
    depth++;
  } while (fp >= FUNCTION_FRAME_SIZE);

  return depth;
}


word_t extractCString(struct ByteArray * array, byte_t* buffer, word_t bufferSize) {
  word_t arrayLength = byte_array_extract_into(array, (byte_t*)buffer, bufferSize - 1);

  if (arrayLength < 0)
    return SLATE_ERROR_RETURN;

  buffer [arrayLength] = '\0';	
  return arrayLength;
}

#ifndef WIN32
/* this keeps us from having to handle sigchld and wait for processes.
   if you don't wait they become zombies */
int fork2()
{
  pid_t pid;
  int status;
  
  if (!(pid = fork()))
    {
      switch (fork())
        {
        case 0: return 0;
        case -1: _exit(errno); /* assumes all errnos are <256 */
        default: _exit(0);
        }
    }
  
  if (pid < 0 || waitpid(pid,&status,0) < 0)
    return -1;
  
  if (WIFEXITED(status))
    if (WEXITSTATUS(status) == 0)
      return 1;
    else
      errno = WEXITSTATUS(status);
  else
    errno = EINTR; /* well, sort of :-) */
  
  return -1;
}
#endif

