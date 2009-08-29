#include "slate.hpp"

#if defined(WIN32) || defined(__CYGWIN__)
#define DLL_FILE_NAME_EXTENSION ".dll"
#else
#define DLL_FILE_NAME_EXTENSION ".so"
#endif 

#ifdef WIN32
#define SLATE_LIB_HANDLE HMODULE
#else
#include <dlfcn.h>
#define SLATE_LIB_HANDLE void*
#endif

static char *safe_string(struct ByteArray *s, char const *suffix) {
  size_t len = byte_array_size(s);
  char *result = (char*)malloc(strlen(suffix) + len + 1);
  if (result == NULL)
    return NULL;
  memcpy(result, s->elements, len);
  strcpy(result + len, suffix);
  return result;
}

bool_t openExternalLibrary(struct object_heap* oh, struct ByteArray *libname, struct ByteArray *handle) {
  char *fullname;
  SLATE_LIB_HANDLE h;

  assert((unsigned)byte_array_size(handle) >= sizeof(h));

  fullname = safe_string(libname, DLL_FILE_NAME_EXTENSION);
  if (fullname == NULL)
    return FALSE;

#ifdef WIN32
  h = LoadLibrary(fullname);
#else
  h = dlopen(fullname, RTLD_NOW);

  char *message = dlerror();
  if (message != NULL) {
    fprintf (stderr, "openExternalLibrary '%s' error: %s\n", fullname, message);
  }
#endif
  free(fullname);

  if (h == NULL) {
    return FALSE;
  } else {
    memcpy(handle->elements, &h, sizeof(h));
    return TRUE;
  }
}

bool_t closeExternalLibrary(struct object_heap* oh, struct ByteArray *handle) {
  SLATE_LIB_HANDLE h;

  assert((unsigned)byte_array_size(handle) >= sizeof(h));
  memcpy(&h, handle->elements, sizeof(h));

#ifdef WIN32
  return FreeLibrary(h) ? TRUE : FALSE;
#else
  return (dlclose(h) == 0) ? TRUE : FALSE;
#endif
}

bool_t lookupExternalLibraryPrimitive(struct object_heap* oh, struct ByteArray *handle, struct ByteArray *symname, struct ByteArray *ptr) {
  SLATE_LIB_HANDLE h;
  void *fn;
  char *symbol;

  assert((unsigned)byte_array_size(handle) >= sizeof(h));
  assert((unsigned)byte_array_size(ptr) >= sizeof(fn));

  symbol = safe_string(symname, "");
  if (symbol == NULL)
    return FALSE;

  memcpy(&h, handle->elements, sizeof(h));
#ifdef WIN32
  fn = (void *) GetProcAddress(h, symbol);
#else
  fn = (void *) dlsym(h, symbol);
#endif
  free(symbol);

  if (fn == NULL) {
    return FALSE;
  } else {
    memcpy(ptr->elements, &fn, sizeof(fn));
    return TRUE;
  }
}

int readExternalLibraryError(struct ByteArray *messageBuffer) {
  char *message;
  int len;
#ifdef WIN32
  //TODO: do the equivalent of dlerror() on unix and write the string into
  // the buffer, returning the length.
  DWORD dw = GetLastError();
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &message,
		0, NULL);
#else
  message = dlerror();
#endif
  if (message == NULL)
    return 0;
  len = strlen(message);
  assert(byte_array_size(messageBuffer) >= len);
  memcpy(messageBuffer->elements, message, len);
#ifdef WIN32
  LocalFree(message);
#endif
  return len;
}




#ifndef WINDOWS
#  define __stdcall
#endif

enum ArgFormat
{
  ARG_FORMAT_VOID = (0 << 1) | 1,
  ARG_FORMAT_INT = (1 << 1) | 1,
  ARG_FORMAT_FLOAT = (2 << 1) | 1,
  ARG_FORMAT_POINTER = (3 << 1) | 1,
  ARG_FORMAT_BYTES = (4 << 1) | 1,
  ARG_FORMAT_BOOLEAN = (5 << 1) | 1,
  ARG_FORMAT_CSTRING = (6 << 1) | 1,
  ARG_FORMAT_C_STRUCT_VALUE = (7 << 1) | 1,
  ARG_FORMAT_DOUBLE = (8 << 1) | 1,
};

enum CallFormat
{
  CALL_FORMAT_C = (0 << 1) | 1,
  CALL_FORMAT_STD = (1 << 1) | 1,
};

typedef word_t (* ext_fn0_t) (void);
typedef word_t (* ext_fn1_t) (word_t);
typedef word_t (* ext_fn2_t) (word_t, word_t);
typedef word_t (* ext_fn3_t) (word_t, word_t, word_t);
typedef word_t (* ext_fn4_t) (word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn5_t) (word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn6_t) (word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn7_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn8_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn9_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn10_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn11_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn12_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn13_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn14_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn15_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (* ext_fn16_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);

typedef word_t (__stdcall * ext_std_fn0_t) (void);
typedef word_t (__stdcall * ext_std_fn1_t) (word_t);
typedef word_t (__stdcall * ext_std_fn2_t) (word_t, word_t);
typedef word_t (__stdcall * ext_std_fn3_t) (word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn4_t) (word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn5_t) (word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn6_t) (word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn7_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn8_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn9_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn10_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn11_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn12_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn13_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn14_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn15_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);
typedef word_t (__stdcall * ext_std_fn16_t) (word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t, word_t);

word_t extractBigInteger(struct ByteArray* bigInt) {
  byte_t *bytes = (byte_t *) byte_array_elements(bigInt);
#if __WORDSIZE == 64
  assert(payload_size((struct Object*)bigInt) >= 8);
  return ((word_t) bytes[0]) |
    (((word_t) bytes[1]) << 8) |
    (((word_t) bytes[2]) << 16) |
    (((word_t) bytes[3]) << 24) |
    (((word_t) bytes[4]) << 32) |
    (((word_t) bytes[5]) << 40) |
    (((word_t) bytes[6]) << 48) |
    (((word_t) bytes[7]) << 56);
#else
  assert(payload_size((struct Object*)bigInt) >= 4);
  return ((word_t) bytes[0]) |
    (((word_t) bytes[1]) << 8) |
    (((word_t) bytes[2]) << 16) |
    (((word_t) bytes[3]) << 24);
#endif

}

struct Object* injectBigInteger(struct object_heap* oh, word_t value) {
#if __WORDSIZE == 64
  byte_t bytes[8];
  bytes[0] = (byte_t) (value & 0xFF);
  bytes[1] = (byte_t) ((value >> 8) & 0xFF);
  bytes[2] = (byte_t) ((value >> 16) & 0xFF);
  bytes[3] = (byte_t) ((value >> 24) & 0xFF);
  bytes[4] = (byte_t) ((value >> 32) & 0xFF);
  bytes[5] = (byte_t) ((value >> 40) & 0xFF);
  bytes[6] = (byte_t) ((value >> 48) & 0xFF);
  bytes[7] = (byte_t) ((value >> 56) & 0xFF);
  return (struct Object*)heap_new_byte_array_with(oh, sizeof(bytes), bytes);
#else
  byte_t bytes[4];
  bytes[0] = (byte_t) (value & 0xFF);
  bytes[1] = (byte_t) ((value >> 8) & 0xFF);
  bytes[2] = (byte_t) ((value >> 16) & 0xFF);
  bytes[3] = (byte_t) ((value >> 24) & 0xFF);
  return (struct Object*)heap_new_byte_array_with(oh, sizeof(bytes), bytes);
#endif
}

#define MAX_ARG_COUNT 16

struct Object* heap_new_cstring(struct object_heap* oh, byte_t *input) {
  return (input ? (struct Object*)heap_new_string_with(oh, strlen((char*)input), (byte_t*) input) : oh->cached.nil);
}

struct Object* applyExternalLibraryPrimitive(struct object_heap* oh, struct ByteArray * fnHandle, struct OopArray * argsFormat, struct Object* callFormat, struct Object* resultFormat, struct OopArray * argsArr) {
  ext_fn0_t fn;
  GC_VOLATILE word_t args [MAX_ARG_COUNT];
  GC_VOLATILE struct Object* fixedArgs [MAX_ARG_COUNT]; /*don't gc*/
  word_t fixedArgsSize = 0;
  word_t result;
  word_t arg, argCount, outArgIndex = 0, outArgCount;

  assert ((unsigned)payload_size((struct Object *) fnHandle) >= sizeof (fn));
  memcpy (& fn, fnHandle -> elements, sizeof (fn));

  argCount = object_array_size((struct Object *) argsArr);
  outArgCount = argCount;
  if (argCount > MAX_ARG_COUNT || argCount != object_array_size((struct Object *) argsFormat))
    return oh->cached.nil;

  for (arg = 0; arg < argCount; ++arg) {
    struct Object* element = argsArr -> elements [arg];

    switch ((word_t)argsFormat->elements [arg]) { /*smallint conversion already done in enum values*/
    case ARG_FORMAT_INT:
      if (object_is_smallint(element))
        args[outArgIndex++] = object_to_smallint(element);
      else
        args[outArgIndex++] = extractBigInteger((struct ByteArray*) element);
      break;
    case ARG_FORMAT_BOOLEAN:
      if (element == oh->cached.true_object) {
        args[outArgIndex++] = TRUE;
      } else if (element == oh->cached.false_object) {
        args[outArgIndex++] = FALSE;
      } else {
        /*fix assert(0)?*/
        args[outArgIndex++] = -1;
      }
      break;
    case ARG_FORMAT_FLOAT:
      {
        union {
          float_type f;
          word_t u;
        } convert;

        if (object_is_smallint(element)) {
          convert.f = (float_type) object_to_smallint(element);
        } else {
          convert.f = * (float_type *) byte_array_elements((struct ByteArray*)element);
        }
        /*fixme this is broken probably*/
        /*assert(0);*/
        args[outArgIndex++] = convert.u;
      }

      break;
    case ARG_FORMAT_DOUBLE:
      {
	union {
	  double d;
	  word_t u[2];
	} convert;
	if (object_is_smallint(element)) {
	  convert.d = (double) object_to_smallint(element);
        } else {
          /*TODO, support for real doubles*/
	  convert.d = (double) * (float_type *) byte_array_elements((struct ByteArray*)element);
        }
	args[outArgIndex++] = convert.u[0];
	args[outArgIndex++] = convert.u[1];
	outArgCount++;
      }
      break;
    case ARG_FORMAT_POINTER:
      if (element == oh->cached.nil)
        args[outArgIndex++] = 0;
      else
        args[outArgIndex++] = * (word_t*) object_array_elements(element);
      break;
    case ARG_FORMAT_CSTRING:
      if (element == oh->cached.nil)
        args[outArgIndex++] = 0;
      else {
        word_t len = payload_size(element) + 1;
        struct ByteArray *bufferObject = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), len);
        char *buffer = (char *) byte_array_elements(bufferObject);
        memcpy(buffer, (char *) byte_array_elements((struct ByteArray*) element), len-1);
        buffer[len-1] = '\0';
        args[outArgIndex++] = (word_t) buffer;
        fixedArgs[fixedArgsSize++] = (struct Object*)bufferObject;
        heap_fixed_add(oh, (struct Object*)bufferObject);
      }
      break;
    case ARG_FORMAT_BYTES:
      if (element == oh->cached.nil) {
        args[outArgIndex++] = 0;
      } else {
        args[outArgIndex++] = (word_t) object_array_elements(element);
      }
      break;
    case ARG_FORMAT_C_STRUCT_VALUE:
      {
        word_t length = payload_size(element) / sizeof(word_t);
        word_t *source = (word_t *) object_array_elements(element);
        int i;
        /* make sure we have enough space */
        if (argCount - arg + length > MAX_ARG_COUNT)
          return oh->cached.nil;
        for(i = 0; i < length; ++i)
          args[outArgIndex++] = source[i];
        outArgCount += length - 1;
      }
      break;
    default:
      return oh->cached.nil;
    }
  }

  if (callFormat == (struct Object*)CALL_FORMAT_C) {
    switch(outArgCount) {
    case 0:
      result = (* (ext_fn0_t) fn) ();
      break;
    case 1:
      result = (* (ext_fn1_t) fn) (args [0]);
      break;
    case 2:
      result = (* (ext_fn2_t) fn) (args [0], args [1]);
      break;
    case 3:
      result = (* (ext_fn3_t) fn) (args [0], args [1], args [2]);
      break;
    case 4:
      result = (* (ext_fn4_t) fn) (args [0], args [1], args [2], args [3]);
      break;
    case 5:
      result = (* (ext_fn5_t) fn) (args [0], args [1], args [2], args [3], args [4]);
      break;
    case 6:
      result = (* (ext_fn6_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5]);
      break;
    case 7:
      result = (* (ext_fn7_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6]);
      break;
    case 8:
      result = (* (ext_fn8_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7]);
      break;
    case 9:
      result = (* (ext_fn9_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8]);
      break;
    case 10:
      result = (* (ext_fn10_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9]);
      break;
    case 11:
      result = (* (ext_fn11_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10]);
      break;
    case 12:
      result = (* (ext_fn12_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11]);
      break;
    case 13:
      result = (* (ext_fn13_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11], args [12]);
      break;
    case 14:
      result = (* (ext_fn14_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11], args [12], args [13]);
      break;
    case 15:
      result = (* (ext_fn15_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11], args [12], args [13], args [14]);
      break;
    case 16:
      result = (* (ext_fn16_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11], args [12], args [13], args [14], args [15]);
      break;
    default:
      return oh->cached.nil;
    }
  } else if (callFormat == (struct Object*)CALL_FORMAT_STD) {
    switch(outArgCount) {
    case 0:
      result = (* (ext_std_fn0_t) fn) ();
      break;
    case 1:
      result = (* (ext_std_fn1_t) fn) (args [0]);
      break;
    case 2:
      result = (* (ext_std_fn2_t) fn) (args [0], args [1]);
      break;
    case 3:
      result = (* (ext_std_fn3_t) fn) (args [0], args [1], args [2]);
      break;
    case 4:
      result = (* (ext_std_fn4_t) fn) (args [0], args [1], args [2], args [3]);
      break;
    case 5:
      result = (* (ext_std_fn5_t) fn) (args [0], args [1], args [2], args [3], args [4]);
      break;
    case 6:
      result = (* (ext_std_fn6_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5]);
      break;
    case 7:
      result = (* (ext_std_fn7_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6]);
      break;
    case 8:
      result = (* (ext_std_fn8_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7]);
      break;
    case 9:
      result = (* (ext_std_fn9_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8]);
      break;
    case 10:
      result = (* (ext_std_fn10_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9]);
      break;
    case 11:
      result = (* (ext_std_fn11_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10]);
      break;
    case 12:
      result = (* (ext_std_fn12_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11]);
      break;
    case 13:
      result = (* (ext_std_fn13_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11], args [12]);
    case 14:
      result = (* (ext_std_fn14_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11], args [12], args [13]);
    case 15:
      result = (* (ext_std_fn15_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11], args [12], args [13], args [14]);
    case 16:
      result = (* (ext_std_fn16_t) fn) (args [0], args [1], args [2], args [3], args [4], args [5], args [6], args [7], args [8], args [9], args [10], args [11], args [12], args [13], args [14], args [15]);
      break;
    default:
      return oh->cached.nil;
    }
  } else {
    return oh->cached.nil;
  }

  for (arg = 0; arg < fixedArgsSize; ++arg) {
    heap_fixed_remove(oh, fixedArgs[arg]);
  }


  switch ((word_t)resultFormat) { /*preconverted smallint*/
  case ARG_FORMAT_INT:
    if (smallint_fits_object(result))
      return smallint_to_object(result);
    else
      return injectBigInteger(oh, (word_t)result);
  case ARG_FORMAT_BOOLEAN:
    if (result)
      return oh->cached.true_object;
    else
      return oh->cached.false_object;
  case ARG_FORMAT_POINTER:
    if (result == 0)
      return oh->cached.nil;
    else
      return (struct Object *) heap_new_byte_array_with(oh, sizeof(result), (byte_t*) &result);
  case ARG_FORMAT_CSTRING:
    return heap_new_cstring(oh, (byte_t*) result);
  default:
    return oh->cached.nil;
  }
}

