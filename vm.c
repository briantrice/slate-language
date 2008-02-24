/*
 * Copyright 2008 Timmy Douglas
 * New VM written in C (rather than pidgin/slang/etc) for slate
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef intptr_t word_t;
typedef uint8_t byte_t;
typedef byte_t bool;

word_t global_current_dispatch_id = 0;


struct slate_image_header {
  word_t magic;
  word_t size;
  word_t next_hash;
  word_t special_objects_oop;
  word_t current_dispatch_id;
};


struct map {
  int FIXME;
};

struct object {

  word_t header; /* ensure HEADER_SIZE */
  struct map* map; /*fix*/
};

struct object_heap
{
  word_t * rootStack[16];
  word_t rootStackPosition;
  struct object * markStack[4096];
  word_t markStackPosition;
  word_t markStackOverflow;
  byte_t mark_color;
  word_t * pinnedCards;
  word_t * stackBottom;
  /*struct BreakEntry breakTable[512];*/
  word_t breakTableSize;
  word_t totalAllocated;
  word_t lastAllocated;
  word_t nextLive;
  byte_t * memory;
  word_t memory_size;
  word_t memoryEnd;
  word_t memoryLimit;
  word_t totalObjectCount;
  word_t lowSpaceThreshold;
  word_t shrinkThreshold;
  word_t growthHeadroom;
  word_t next_hash;
  word_t TrueObject;
  word_t FalseObject;
  word_t NilObject;
  word_t ClosureWindow;
  word_t CompiledMethodWindow;
  word_t PrimitiveMethodWindow;
  word_t special_objects_oop;
};



bool oop_is_object(word_t xxx)   { return ((((word_t)xxx)&1) == 0); }
bool oop_is_smallint(word_t xxx) { return ((((word_t)xxx)&1) == 1);}
word_t object_mark(struct object* xxx)      { return  (((xxx)->header)&1); }
word_t object_hash(struct object* xxx)       { return  ((((xxx)->header)>>1)&0x7FFFFF);}
word_t object_size(struct object* xxx)       {return   ((((xxx)->header)>>24)&0x3F);}
word_t payload_size(struct object* xxx) {return ((((xxx)->header))&0x3FFFFFFF);}
word_t object_type(struct object* xxx)     {return     ((((xxx)->header)>>30)&0x3);}

#define oop_to_smallint(xxx)  ((((word_t)xxx)>>1)) 
#define oop_to_object(xxx)    ((struct object*)xxx)
#define inc_ptr(xxx, yyy)     ((byte_t*)xxx + yyy)

#define ID_HASH_RESERVED 0x7FFFF0

#define HEADER_SIZE sizeof(word_t)

#define TYPE_OBJECT 0
#define TYPE_OOP_ARRAY  1
#define TYPE_BYTE_ARRAY 2
#define TYPE_PAYLOAD 3


#if 0
struct ObjectHeader
{
  unsigned long int isMarked:1;
  unsigned long int idHash:23;
  unsigned long int objectSize:6;
  unsigned long int objectFormat:2;
};
#endif

void print_object(struct object* oop) {

  if (oop_is_smallint((word_t)oop)) {
    printf("<object int_value: %lu>\n", oop_to_smallint(oop));
  } else {
    printf("<object at %p, hash: 0x%lX, size: %lu, type: %lu>\n", (void*)oop, object_hash(oop), object_size(oop), object_type(oop));
  }

}

void interpret(struct object_heap* heap) {

  printf("Interpret: img:0x%p size:%lu spec:%p next:%lu\n",
         (void*)heap->memory, heap->memory_size, (void*)heap->special_objects_oop, heap->next_hash);
  printf("Special oop: "); print_object(oop_to_object(heap->special_objects_oop));

}

bool object_in_memory(struct object_heap* heap, struct object* oop) {

  return (heap->memory <= (byte_t*)oop && heap->memory + heap->memory_size > (byte_t*)oop);

}

struct object* drop_payload(struct object* o) {
  if (object_type(o) == TYPE_PAYLOAD) {
    return (struct object*)inc_ptr(o, HEADER_SIZE);
  }
  return o;
}

struct object* first_object(struct object_heap* heap) {

  struct object* o = (struct object*)heap->memory;
  
  return drop_payload(o);
}

word_t object_payload_size(struct object* o) {

  /*in memory there is a payload header before the object*/
  o = (struct object*) inc_ptr(o, -HEADER_SIZE);
  return payload_size(o);

}

word_t object_word_size(struct object* o) {

  if (object_type(o) == TYPE_OBJECT) {
    return object_size(o);
  } 
  return object_size(o) + (object_payload_size(o) + sizeof(word_t) - 1) / sizeof(word_t); 

}


word_t object_total_size(struct object* o) {
  /*IMPORTANT: rounds up to word size*/

  return object_word_size(o)*sizeof(word_t);

}

word_t object_first_slot_offset(struct object* o) {

  return HEADER_SIZE + sizeof(struct map*);

}

word_t object_last_slot_offset(struct object* o) {
  return object_size(o) * sizeof(word_t) - sizeof(word_t);
}

word_t object_last_oop_offset(struct object* o) {

  if (object_type(o) == TYPE_OOP_ARRAY) {
    return object_last_slot_offset(o) + object_payload_size(o);
  }
  return object_last_slot_offset(o);
}

word_t object_slot_value_at_offset(struct object* o, word_t offset) {

  return *((word_t*)inc_ptr(o, offset));

}

void object_slot_value_at_offset_put(struct object* o, word_t offset, word_t value) {

  *((word_t*)inc_ptr(o, offset)) = value;

}


struct object* object_after(struct object_heap* heap, struct object* o) {
  /*print_object(o);*/
  assert(object_in_memory(heap, o) && object_total_size(o) != 0);

  o = (struct object*)inc_ptr(o, object_total_size(o));
  /*fix last allocated and next live*/
  o = drop_payload(o);
  return o;
}

bool object_is_free(struct object_heap* heap, struct object* o) {

  return object_mark(o) != heap->mark_color || object_hash(o) > ID_HASH_RESERVED;
}

void adjust_fields_by(struct object_heap* heap, struct object* o, word_t shift_amount) {

  word_t offset, limit;
  o->map = (struct map*) inc_ptr(o->map, shift_amount);
  /*fix do slots*/
  offset = object_first_slot_offset(o);
  limit = object_last_oop_offset(o);
  for (; offset != limit; offset += sizeof(word_t)) {
    word_t val = object_slot_value_at_offset(o, offset);
    if (oop_is_object(val)) {
      object_slot_value_at_offset_put(o, offset, (word_t)inc_ptr(val, shift_amount));
    }
  }

}

void adjust_oop_pointers(struct object_heap* heap, word_t shift_amount) {

  struct object* o = first_object(heap);
  printf("First object: "); print_object(o);
  while (object_in_memory(heap, o)) {
    if (!object_is_free(heap, o)) {
      adjust_fields_by(heap, o, shift_amount);
    }
      o = object_after(heap, o);
  }


}


int main(int argc, char** argv) {

  FILE* file;
  struct slate_image_header sih;
  byte_t* image_start;
  struct object_heap heap;
  memset(&heap, 0, sizeof(heap));

  if (argc != 2) {
    fprintf(stderr, "You must supply an image file as an argument\n");
    return 1;
  }

  file = fopen(argv[1], "r");
  if (file == NULL) {fprintf(stderr, "Open file failed (%d)", errno); return 1;}

  fread(&sih.magic, sizeof(sih.magic), 1, file);
  fread(&sih.size, sizeof(sih.size), 1, file);
  fread(&sih.next_hash, sizeof(sih.next_hash), 1, file);
  fread(&sih.special_objects_oop, sizeof(sih.special_objects_oop), 1, file);
  fread(&sih.current_dispatch_id, sizeof(sih.current_dispatch_id), 1, file);

  if (sih.magic != (word_t)0xABCDEF42) {
    fprintf(stderr, "Magic number (0x%lX) doesn't match (word_t)0xABCDEF42\n", sih.magic);
    return 1;
  }
  
  image_start = malloc(sih.size);
  printf("Image size: %lu bytes\n", sih.size);
  printf("Special Objects Pointer: 0x%lX\n", sih.special_objects_oop);
  if (fread(image_start, 1, sih.size, file) != sih.size) {
    fprintf(stderr, "Error fread()ing image\n");
    return 1;
  }


  heap.memory = image_start;
  heap.special_objects_oop = (word_t)(image_start + sih.special_objects_oop);
  heap.next_hash = sih.next_hash;
  heap.memory_size = sih.size;
  global_current_dispatch_id = sih.current_dispatch_id;
  
  adjust_oop_pointers(&heap, (word_t)heap.memory);
  interpret(&heap);
  

  free(image_start);
  fclose(file);

  return 0;
}
