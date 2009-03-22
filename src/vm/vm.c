/*
 * Copyright 2008 Timmy Douglas
 * New VM written in C (rather than pidgin/slang/etc) for slate
 * Based on original by Lee Salzman and Brian Rice
 */


/***************************

TODO:

- replace(?) "->elements[i]" calls to function calls since
the array elements might not come directly after the header
on objects that have slots. use (byte|object)_array_(get|set)_element


- some gc bugs where not everything gets marked or pinned when
  interpreter_signal calls interpreter_apply_to_arity_with_optionals
  and the new lexicalcontext causes some args[] to be collected
  immaturely


- edit mobius/c/types.slate to get the correct wordsize

***************************/

#include "slate.h"

word_t memory_string_to_bytes(char* str) {

  word_t res = atoi(str);

  if (strstr(str, "GB")) {
    return res * 1024 * 1024 * 1024;

  } else if (strstr(str, "MB")) {
    return res * 1024 * 1024;

  } else if (strstr(str, "KB")) {
    return res * 1024;

  } else {
    return res;
  }

}

int globalInterrupt = 0;

void slate_interrupt_handler(int sig) {
  globalInterrupt = 1;
}



int main(int argc, char** argv) {

  FILE* file;
  struct slate_image_header sih;
  struct object_heap* heap;
  word_t memory_limit = 400 * 1024 * 1024;
  word_t young_limit = 10 * 1024 * 1024;
  size_t res;
  word_t le_test_ = 1;
  char* le_test = (char*)&le_test_;
  int i;
  struct sigaction interrupt_action, pipe_ignore_action;


  heap = calloc(1, sizeof(struct object_heap));


  if (argc < 2) {
    fprintf(stderr, "Slate build type: %s\n", SLATE_BUILD_TYPE);
    fprintf(stderr, "You must supply an image file as an argument\n");
    fprintf(stderr, "Your platform is %d bit, %s\n", (int)sizeof(word_t)*8, (le_test[0] == 1)? "little endian" : "big endian");
    fprintf(stderr, "Usage: ./vm <image> [-mo <bytes>(GB|MB|KB|)] [-mn <bytes>(GB|MB|KB|)]\n");
    fprintf(stderr, "VM Options:\n");
    fprintf(stderr, "  -mo Old memory for tenured/old objects (Default 400MB)\n");
    fprintf(stderr, "  -mn New memory for young/new objects (Default 10MB)\n");
    fprintf(stderr, "For image options: %s <image> --image-help\n", argv[0]);

    return 1;
  }

  file = fopen(argv[1], "r");

  if (file == NULL) {fprintf(stderr, "Open file failed (%d)\n", errno); return 1;}

  assert(fread(&sih.magic, sizeof(sih.magic), 1, file) == 1);
  assert(fread(&sih.size, sizeof(sih.size), 1, file) == 1);
  assert(fread(&sih.next_hash, sizeof(sih.next_hash), 1, file) == 1);
  assert(fread(&sih.special_objects_oop, sizeof(sih.special_objects_oop), 1, file) == 1);
  assert(fread(&sih.current_dispatch_id, sizeof(sih.current_dispatch_id), 1, file) == 1);

  if (sih.size == 0) {
    fprintf(stderr, "Image size is zero. You have probably tried to load a file that isn't an image file or a file that is the wrong WORD_SIZE. Run slate without any options to see your build configuration.\n");
    return 1;

  }

  /* skip argv[0] and image name */
  for (i = 2; i < argc - 1; i++) {
    if (strcmp(argv[i], "-mo") == 0) {
      memory_limit = memory_string_to_bytes(argv[i+1]);
      i++;
    } else if (strcmp(argv[i], "-mn") == 0) {
      young_limit = memory_string_to_bytes(argv[i+1]);
      i++;
    } else {
      /*it might be an image argument*/
      /*fprintf(stderr, "Illegal argument: %s\n", argv[i]);*/
    }
  }
  
  if (sih.magic != SLATE_IMAGE_MAGIC) {
    fprintf(stderr, "Magic number (0x%" PRIxPTR ") doesn't match (word_t)0xABCDEF43. Make sure you have a valid slate image and it is the correct endianess. Run slate without arguments to see more info.\n", sih.magic);
    return 1;
  }
  
  if (memory_limit < sih.size) {
    fprintf(stderr, "Slate image cannot fit into base allocated memory size. Use -mo with a greater value\n");
    return 1;
  }

  if (!heap_initialize(heap, sih.size, memory_limit, young_limit, sih.next_hash, sih.special_objects_oop, sih.current_dispatch_id)) return 1;

  printf("Old Memory size: %" PRIdPTR " bytes\n", memory_limit);
  printf("New Memory size: %" PRIdPTR " bytes\n", young_limit);
  printf("Image size: %" PRIdPTR " bytes\n", sih.size);
  if ((res = fread(heap->memoryOld, 1, sih.size, file)) != sih.size) {
    fprintf(stderr, "Error fread()ing image. Got %" PRIuPTR "u, expected %" PRIuPTR "u.\n", res, sih.size);
    return 1;
  }

  adjust_oop_pointers_from(heap, (word_t)heap->memoryOld, heap->memoryOld, heap->memoryOldSize);
  heap->stackBottom = &heap;
  heap->argcSaved = argc;
  heap->argvSaved = argv;

  interrupt_action.sa_handler = slate_interrupt_handler;
  interrupt_action.sa_flags = 0;
  sigemptyset(&interrupt_action.sa_mask);
  sigaction(SIGINT, &interrupt_action, NULL);


  pipe_ignore_action.sa_handler = SIG_IGN;
  pipe_ignore_action.sa_flags = 0;
  sigemptyset(&pipe_ignore_action.sa_mask);
  sigaction(SIGPIPE, &pipe_ignore_action, NULL);


  interpret(heap);
  
  gc_close(heap);

  fclose(file);

  return 0;
}
