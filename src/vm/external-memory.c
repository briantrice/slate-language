#include "slate.h"


/***************** MEMORY *************************/

void initMemoryModule (struct object_heap* oh) {
  memset (oh->memory_areas, 0, sizeof (oh->memory_areas));
  memset (oh->memory_sizes, 0, sizeof (oh->memory_sizes));
  memset (oh->memory_num_refs, 0, sizeof (oh->memory_num_refs));
}

int validMemoryHandle (struct object_heap* oh, int memory) {
  return (oh->memory_areas [memory] != NULL);
}

int allocateMemory(struct object_heap* oh)
{
  int memory;
  for (memory = 0; memory < SLATE_MEMS_MAXIMUM; ++ memory) {
    if (!(validMemoryHandle(oh, memory)))
      return memory;
  }
  return SLATE_ERROR_RETURN;
}

void closeMemory (struct object_heap* oh, int memory) {
  if (validMemoryHandle(oh, memory)) {
    if (!--oh->memory_num_refs [memory]) {
      free (oh->memory_areas [memory]);
      oh->memory_areas [memory] = NULL;
      oh->memory_sizes [memory] = 0;
    }
  }
}

void addRefMemory (struct object_heap* oh, int memory) {
  if (validMemoryHandle(oh, memory))
      ++oh->memory_num_refs [memory];
}
      
int openMemory (struct object_heap* oh, int size) {
  void* area;
  int memory;
  memory = allocateMemory (oh);
  if (memory < 0)
    return SLATE_ERROR_RETURN;
  area = malloc (size);
  if (area == NULL) {
    closeMemory (oh, memory);
    return SLATE_ERROR_RETURN;
  } else {
    oh->memory_areas [memory] = area;
    oh->memory_sizes [memory] = size;
    oh->memory_num_refs [memory] = 1;
    return memory;
  }
}

int writeMemory (struct object_heap* oh, int memory, int memStart, int n, byte_t* bytes) {
  void* area;
  int nDelimited;
  if (!(validMemoryHandle(oh, memory)))
    return SLATE_ERROR_RETURN;
  area = oh->memory_areas [memory];
  nDelimited = oh->memory_sizes [memory] - memStart;
  if (nDelimited > n)
    nDelimited = n; /* We're just taking the max of N and the amount left.*/
  if (nDelimited > 0)
    memcpy (bytes, area, nDelimited);
  return nDelimited;
}

int readMemory (struct object_heap* oh, int memory, int memStart, int n, byte_t* bytes) {
  void* area;
  int nDelimited;
  if (!(validMemoryHandle(oh, memory)))
    return SLATE_ERROR_RETURN;
  area = oh->memory_areas [memory];
  nDelimited = oh->memory_sizes [memory] - memStart;
  if (nDelimited > n)
    nDelimited = n; /* We're just taking the max of N and the amount left.*/
  if (nDelimited > 0)
    memcpy (area, bytes, nDelimited);
  return nDelimited;
}

int sizeOfMemory (struct object_heap* oh, int memory) {
  return (validMemoryHandle(oh, memory) ? oh->memory_sizes [memory] : SLATE_ERROR_RETURN);
}

int resizeMemory (struct object_heap* oh, int memory, int size) {
  void* result;
  if (!(validMemoryHandle(oh, memory)))
    return SLATE_ERROR_RETURN;
  if (oh->memory_num_refs [memory] != 1)
    return SLATE_ERROR_RETURN;
  if (oh->memory_sizes [memory] == size)
    return size;
  result = realloc (oh->memory_areas [memory], size);
  if (result == NULL)
    return SLATE_ERROR_RETURN;
  oh->memory_areas [memory] = result;
  oh->memory_sizes [memory] = size;
  return size;
}

int addressOfMemory (struct object_heap* oh, int memory, int offset, byte_t* addressBuffer) {
  void* result;
  result = ((validMemoryHandle(oh, memory)
             && 0 <= offset && offset < oh->memory_sizes [memory])
            ? (char *) oh->memory_areas [memory] + offset : NULL);
  memcpy (addressBuffer, (char *) &result, sizeof (&result));
  return sizeof (result);
}
