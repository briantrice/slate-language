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
#include <signal.h>

word_t memory_string_to_bytes(char* str) {

  word_t res = atoi(str);

  if (strstr(str, "GB")) {
    return res * GB;

  } else if (strstr(str, "MB")) {
    return res * MB;

  } else if (strstr(str, "KB")) {
    return res * KB;

  } else {
    return res;
  }

}

int globalInterrupt = 0;

void slate_interrupt_handler(int sig) {
  globalInterrupt = 1;
}

void print_usage (char *progName) {
  fprintf(stderr, "Usage: %s <image>? [options]\n", progName);
  //fprintf(stderr, "usage: %s [option] [<image> [<argument>...]]\n", progName);
  fprintf(stderr, "  -h, --help            Print this help message, then exit\n");
  fprintf(stderr, "  -v, --version         Print the VM version, then exit\n");
  fprintf(stderr, "  -i, --image <image>   Specify a non-default image file to start with\n");
  fprintf(stderr, "  -mo <bytes>(GB|MB|KB) Old memory for tenured/old objects (Default 400MB)\n");
  fprintf(stderr, "  -mn <bytes>(GB|MB|KB) New memory for young/new objects (Default 10MB)\n");
  fprintf(stderr, "  --image-help          Print the help message for the image\n");
  //fprintf(stderr, "\nNotes:\n");
  //fprintf(stderr, "<image> defaults: `./%s', then `%s/%s'.\n", xstr (SLATE_IMGNAME), xstr (SLATE_DATADIR), xstr (SLATE_IMGNAME));
}

int main(int argc, char** argv) {

  char* image_name = NULL;
  FILE* image_file;
  struct slate_image_header sih;
  struct object_heap* heap;
  word_t memory_limit = 400 * MB;
  word_t young_limit = 10 * MB;
  size_t res;
  word_t le_test_ = 1;
  char* le_test = (char*)&le_test_;
  int i;
#ifdef WIN32
  // TODO WIN32 port set up signal handlers for interrupts.
#else
  struct sigaction interrupt_action, pipe_ignore_action;
#endif

  /* skip argv[0] and image name if given first */
  for (i = 1; i < argc; i++) {
	if ((i == 1) && (strncmp(argv[i], "-", 1) != 0)) {
	  image_name = argv[i];
	} else if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
	  print_usage(argv[0]);
	  return 0;
	} else if ((strcmp(argv[i], "-i") == 0) || (strcmp(argv[i], "--image") == 0)) {
      if (++i < argc)
		image_name = argv[i++];
      else
		error("You must specify an image filename after -i/--image.");
	} else if ((strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "--version") == 0)) {
	  //fprintf(stderr, "Slate version: %s\n", VERSION);
	  fprintf(stderr, "Slate build type: %s\n", SLATE_BUILD_TYPE);
	  fprintf(stderr, "Platform word-size: %d bits; byte-order: %s endian\n", (int)sizeof(word_t)*8, (le_test[0] == 1)? "little" : "big");
	  return 0;
	} else if (strcmp(argv[i], "-mo") == 0) {
      memory_limit = memory_string_to_bytes(argv[i+1]);
      i++;
    } else if (strcmp(argv[i], "-mn") == 0) {
      young_limit = memory_string_to_bytes(argv[i+1]);
      i++;
	} else if (strcmp(argv[i], "--") == 0) {
	  /* GNU convention to ignore all arguments past a --, allowing the image to process anything beyond that. */
	  break;
    } else {
      /* The VM does not process this argument. The warning is disabled, because the image may know how to process it. */
      /*fprintf(stderr, "Illegal argument: %s\n", argv[i]);*/
    }
  }

  if (!image_name) {
    fprintf(stderr, "You must supply an image file as an argument.\n");
	print_usage(argv[0]);
    return 1;
  }

  image_file = fopen(image_name, "rb");

  if (!image_file) {fprintf(stderr, "Open file failed (%d)\n", errno); return 1;}

  // Read in the elements of the Slate Image Header from the image file:
  assert(fread(&sih.magic, sizeof(sih.magic), 1, image_file) == 1);
  assert(fread(&sih.size, sizeof(sih.size), 1, image_file) == 1);
  assert(fread(&sih.next_hash, sizeof(sih.next_hash), 1, image_file) == 1);
  assert(fread(&sih.special_objects_oop, sizeof(sih.special_objects_oop), 1, image_file) == 1);
  assert(fread(&sih.current_dispatch_id, sizeof(sih.current_dispatch_id), 1, image_file) == 1);

  if (sih.size == 0) {
    fprintf(stderr, "Image size is zero. You have probably tried to load a file that isn't an image file or a file that is the wrong WORD_SIZE. Run slate without any options to see your build configuration.\n");
    return 1;
  }

  if (sih.magic != SLATE_IMAGE_MAGIC) {
    fprintf(stderr, "Magic number (0x%" PRIxPTR ") doesn't match (word_t)0xABCDEF43. Make sure you have a valid slate image and it is the correct endianess. Run slate without arguments to see more info.\n", sih.magic);
    return 1;
  }

  if (memory_limit < sih.size) {
    fprintf(stderr, "Slate image cannot fit into base allocated memory size. Use -mo with a greater value\n");
    return 1;
  }

  heap = calloc(1, sizeof(struct object_heap));

  if (!heap_initialize(heap, sih.size, memory_limit, young_limit, sih.next_hash, sih.special_objects_oop, sih.current_dispatch_id)) return 1;

  printf("Old Memory size: %" PRIdPTR " bytes\n", memory_limit);
  printf("New Memory size: %" PRIdPTR " bytes\n", young_limit);
  printf("Image size: %" PRIdPTR " bytes\n", sih.size);

  if ((res = fread(heap->memoryOld, 1, sih.size, image_file)) != sih.size) {
    fprintf(stderr, "Error fread()ing image. Got %" PRIuPTR "u, expected %" PRIuPTR "u.\n", res, sih.size);
    return 1;
  }

  adjust_oop_pointers_from(heap, (word_t)heap->memoryOld, heap->memoryOld, heap->memoryOldSize);
  heap->stackBottom = &heap;
  heap->argcSaved = argc;
  heap->argvSaved = argv;

#ifdef WIN32
  signal(SIGINT, slate_interrupt_handler);
#else
  interrupt_action.sa_handler = slate_interrupt_handler;
  interrupt_action.sa_flags = 0;
  sigemptyset(&interrupt_action.sa_mask);
  sigaction(SIGINT, &interrupt_action, NULL);


  pipe_ignore_action.sa_handler = SIG_IGN;
  pipe_ignore_action.sa_flags = 0;
  sigemptyset(&pipe_ignore_action.sa_mask);
  sigaction(SIGPIPE, &pipe_ignore_action, NULL);
#endif

  interpret(heap);
  
  heap_close(heap);

  fclose(image_file);

  return 0;
}
