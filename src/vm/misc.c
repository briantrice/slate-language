#include "slate.h"

void error(char* str) {
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

word_t extractCString(struct ByteArray * array, byte_t* buffer, word_t bufferSize) {
  word_t arrayLength = byte_array_extract_into(array, (byte_t*)buffer, bufferSize - 1);

  if (arrayLength < 0)
    return SLATE_ERROR_RETURN;

  buffer [arrayLength] = '\0';	
  return arrayLength;
}

word_t hash_selector(struct object_heap* oh, struct Symbol* name, struct Object* arguments[], word_t n) {
  word_t i;
  word_t hash = (word_t) name;
  for (i = 0; i < n; i++) {
    hash += object_hash((struct Object*)object_get_map(oh, arguments[i]));
  }
  return hash;
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
