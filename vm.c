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

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <dlfcn.h>

typedef intptr_t word_t;
typedef uint8_t byte_t;
typedef word_t bool;

#define WORDT_MAX INTPTR_MAX

struct SlotTable;
struct Symbol;
struct CompiledMethod;
struct LexicalContext;
struct RoleTable;
struct OopArray;
struct MethodDefinition;
struct Object;
struct ByteArray;
struct MethodCacheEntry;
struct Closure;
struct Interpreter;
struct ForwardedObject;
struct Map;
struct BreakEntry;
struct PrimitiveMethod;
struct RoleEntry;
struct SlotEntry;

struct slate_image_header {
  word_t magic;
  word_t size;
  word_t next_hash;
  word_t special_objects_oop;
  word_t current_dispatch_id;
};



struct Object
{
  word_t header;
  word_t objectSize; /*in words... used for slot storage*/
  word_t payloadSize; /*in bytes... used for ooparray and bytearray elements*/
  /*put the map after*/
  struct Map * map;

};
/* when we clone objects, make sure we don't overwrite the things within headersize. 
 * we might clone with a new size, and we don't want to set the size back.
 */
#define HEADER_SIZE (sizeof(struct Object) - sizeof(struct Map*))
#define HEADER_SIZE_WORDS (HEADER_SIZE/sizeof(word_t))
#define SLATE_IMAGE_MAGIC (word_t)0xABCDEF43

#define METHOD_CACHE_ARITY 6

struct MethodCacheEntry
{
  struct MethodDefinition * method;
  struct Symbol* selector;
  struct Map * maps[METHOD_CACHE_ARITY];
};
struct ForwardedObject
{
  word_t header;
  word_t objectSize; 
  word_t payloadSize; 
  struct Object * target; /*rather than the map*/
};
struct ForwardPointerEntry /*for saving images*/
{
  struct Object* fromObj;
  struct Object* toObj;
};
struct Map
{
  struct Object base;
  struct Object* flags;
  struct Object* representative;
  struct OopArray * delegates;
  struct Object* slotCount;
  struct SlotTable * slotTable;
  struct RoleTable * roleTable;
  word_t dispatchID;
  word_t visitedPositions;
};
struct BreakEntry
{
  word_t oldAddress;
  word_t newAddress;
};
struct PrimitiveMethod
{
  struct Object base;
  struct Object* index;
  struct Object* selector;
  struct Object* inputVariables;
};
struct RoleEntry
{
  struct Symbol* name;
  struct Object* rolePositions; /*smallint?*/
  struct MethodDefinition * methodDefinition;
  struct Object* nextRole; /*smallint?*/
};
struct SlotEntry
{
  struct Symbol* name;
  struct Object* offset; /*smallint?*/
};
struct SlotTable
{
  struct Object base;
  struct SlotEntry slots[];
};
struct Symbol
{
  struct Object base;
  struct Object* cacheMask;
  byte_t elements[];
};
struct CompiledMethod
{
  struct Object base;
  struct CompiledMethod * method;
  struct Symbol* selector;
  struct Object* inputVariables;
  struct Object* localVariables;
  struct Object* restVariable;
  struct OopArray * optionalKeywords;
  struct Object* heapAllocate;
  struct Object* environment;
  struct OopArray * literals;
  struct OopArray * selectors;
  struct OopArray * code;
  word_t sourceTree;
  word_t debugMap;
  struct Object* isInlined;
  struct OopArray* oldCode;
  struct Object* callCount;
  /* calleeCount is the polymorphic inline cache (PIC), see #defines/method_pic_add_callee below for details */
  struct OopArray* calleeCount;
  struct Object* reserved1;
  struct Object* reserved2;
  struct Object* reserved3;
  struct Object* reserved4;
  struct Object* reserved5;
  struct Object* reserved6;
};
struct LexicalContext
{
  struct Object base;
  struct Object* framePointer;
  struct Object* variables[];
};
struct RoleTable
{
  struct Object base;
  struct RoleEntry roles[];
};
struct OopArray
{
  struct Object base;
  struct Object* elements[];
};
struct MethodDefinition
{
  struct Object base;
  struct Object* method;
  struct Object* slotAccessor;
  word_t dispatchID;
  word_t dispatchPositions;
  word_t foundPositions;
  word_t dispatchRank;
};

struct ByteArray
{
  struct Object base;
  byte_t elements[];
};
struct Closure
{
  struct Object base;
  struct CompiledMethod * method;
  struct LexicalContext * lexicalWindow[];
};
struct Interpreter /*note the bottom fields are treated as contents in a bytearray so they don't need to be converted from objects to get the actual smallint value*/
{
  struct Object base;
  struct OopArray * stack;
  struct CompiledMethod * method;
  struct Closure * closure;
  struct LexicalContext * lexicalContext;
  struct Object* ensureHandlers;
  /*everything below here is in the interpreter's "byte array" so we don't need to translate from oop to int*/
  word_t framePointer;
  word_t codePointer;
  word_t codeSize;
  word_t stackPointer;
  word_t stackSize;
};

#define SLATE_ERROR_RETURN (-1)
#define SLATE_FILE_NAME_LENGTH 512
#define DELEGATION_STACK_SIZE 256
#define MAX_FIXEDS 64
#define MARK_MASK 1
#define METHOD_CACHE_SIZE 1024*64
#define PINNED_CARD_SIZE (sizeof(word_t) * 8)
#define SLATE_MEMS_MAXIMUM		1024

/*these things never exist in slate land (so word_t types are their actual value)*/
struct object_heap
{
  byte_t mark_color;
  byte_t * memoryOld;
  byte_t * memoryYoung;
  word_t memoryOldSize;
  word_t memoryYoungSize;
  word_t memoryOldLimit;
  word_t memoryYoungLimit;
  struct OopArray* special_objects_oop; /*root for gc*/
  word_t current_dispatch_id;
  bool interrupt_flag;
  word_t lastHash;
  word_t method_cache_hit, method_cache_access;
  FILE** file_index;
  word_t file_index_size;
  struct Object *nextFree;
  struct Object *nextOldFree;
  struct Object* delegation_stack[DELEGATION_STACK_SIZE];
  struct MethodCacheEntry methodCache[METHOD_CACHE_SIZE];
  struct Object* fixedObjects[MAX_FIXEDS];

  struct CompiledMethod** optimizedMethods;
  word_t optimizedMethodsSize;
  word_t optimizedMethodsLimit;


  void* memory_areas [SLATE_MEMS_MAXIMUM];
  int memory_sizes [SLATE_MEMS_MAXIMUM];
  int memory_num_refs [SLATE_MEMS_MAXIMUM];


  int argcSaved;
  char** argvSaved;

  struct Object** markStack;
  size_t markStackSize;
  size_t markStackPosition;

  word_t* pinnedYoungObjects; /* scan the C stack for things we can't move */
  void* stackBottom;

  struct {
    struct Interpreter* interpreter;
    struct Object* true;
    struct Object* false;
    struct Object* nil;
    struct Object* primitive_method_window;
    struct Object* compiled_method_window;
    struct Object* closure_method_window;
  } cached;
};



/****************************************/




void (*primitives[]) (struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);

#define SMALLINT_MASK 0x1
#define ID_HASH_RESERVED 0x7FFFF0
#define ID_HASH_FORWARDED ID_HASH_RESERVED
#define ID_HASH_FREE 0x7FFFFF
#define ID_HASH_MAX ID_HASH_FREE


#define FLOAT_SIGNIFICAND 0x7FFFFF
#define FLOAT_EXPONENT_OFFSET 23
typedef float float_t;


/*obj map flags is a smallint oop, so we start after the smallint flag*/
#define MAP_FLAG_RESTRICT_DELEGATION 2
#define MAP_FLAG_IMMUTABLE 4


#define WORD_BYTES_MINUS_ONE (sizeof(word_t)-1)
#define ROLE_ENTRY_WORD_SIZE ((sizeof(struct RoleEntry) + WORD_BYTES_MINUS_ONE) / sizeof(word_t))
#define SLOT_ENTRY_WORD_SIZE ((sizeof(struct SlotEntry) + WORD_BYTES_MINUS_ONE) / sizeof(word_t))
#define FUNCTION_FRAME_SIZE 5

#define TRUE 1
#define FALSE 0

#define TYPE_OBJECT 0
#define TYPE_OOP_ARRAY  1
#define TYPE_BYTE_ARRAY 2

#define CALLER_PIC_SETUP_AFTER 500
#define CALLEE_OPTIMIZE_AFTER 1000
#define CALLER_PIC_SIZE 64
#define CALLER_PIC_ENTRY_SIZE 4 /*calleeCompiledMethod, calleeArity, oopArrayOfMaps, count*/
#define PIC_CALLEE 0 
#define PIC_CALLEE_ARITY 1
#define PIC_CALLEE_MAPS 2
#define PIC_CALLEE_COUNT 3

word_t object_to_smallint(struct Object* xxx)  {return ((((word_t)xxx)>>1)); }
struct Object* smallint_to_object(word_t xxx) {return ((struct Object*)(((xxx)<<1)|1)); }


bool oop_is_object(word_t xxx)   { return ((((word_t)xxx)&SMALLINT_MASK) == 0); }
bool oop_is_smallint(word_t xxx) { return ((((word_t)xxx)&SMALLINT_MASK) == 1);}
bool object_is_smallint(struct Object* xxx) { return ((((word_t)xxx)&SMALLINT_MASK) == 1);}
word_t object_markbit(struct Object* xxx)      { return  (((xxx)->header)&MARK_MASK); }
word_t object_hash(struct Object* xxx)       { return  ((((xxx)->header)>>1)&ID_HASH_MAX);}
word_t object_size(struct Object* xxx)       {return   xxx->objectSize;}
word_t payload_size(struct Object* xxx) {return xxx->payloadSize;}
word_t object_type(struct Object* xxx)     {return     ((((xxx)->header)>>30)&0x3);}


void object_set_mark(struct object_heap* oh, struct Object* xxx) {
  xxx->header &= ~MARK_MASK;
  xxx->header|=oh->mark_color & MARK_MASK;
}
void object_unmark(struct object_heap* oh, struct Object* xxx)  {
  xxx->header &= ~MARK_MASK;
  xxx->header|= ((~oh->mark_color)&MARK_MASK);
}

void object_set_format(struct Object* xxx, word_t type) {
  xxx->header &= ~(3<<30);
  xxx->header |= (type&3) << 30;
}
void object_set_size(struct Object* xxx, word_t size) {
  assert(size >= HEADER_SIZE_WORDS+1);
  xxx->objectSize = size;
}
void object_set_idhash(struct Object* xxx, word_t hash) {
  xxx->header &= ~(ID_HASH_MAX<<1);
  xxx->header |= (hash&ID_HASH_MAX) << 1;
}
void payload_set_size(struct Object* xxx, word_t size) {
  xxx->payloadSize = size;
}


/*fix see if it can be post incremented*/
word_t heap_new_hash(struct object_heap* oh) {
  word_t hash;
  do {
    hash = (oh->lastHash = 13849 + (27181 * oh->lastHash)) & ID_HASH_MAX;
  } while (hash >= ID_HASH_RESERVED);

  return hash;
}



/* DETAILS: This trick is from Tim Rowledge. Use a shift and XOR to set the 
 * sign bit if and only if the top two bits of the given value are the same, 
 * then test the sign bit. Note that the top two bits are equal for exactly 
 * those integers in the range that can be represented in 31-bits.
*/
word_t smallint_fits_object(word_t i) {   return (i ^ (i << 1)) >= 0;}
/*fix i didn't understand the above*/


#define inc_ptr(xxx, yyy)     ((byte_t*)xxx + yyy)




#define SPECIAL_OOP_LOBBY 0
#define SPECIAL_OOP_NO_ROLE 1
#define SPECIAL_OOP_NIL 2
#define SPECIAL_OOP_TRUE 3
#define SPECIAL_OOP_FALSE 4
#define SPECIAL_OOP_ARRAY_PROTO 5
#define SPECIAL_OOP_BYTE_ARRAY_PROTO 6
#define SPECIAL_OOP_ASCII_PROTO 7
#define SPECIAL_OOP_MAP_PROTO 8
#define SPECIAL_OOP_METHOD_DEF_PROTO 9
#define SPECIAL_OOP_SMALL_INT_PROTO 10
#define SPECIAL_OOP_FLOAT_PROTO 11
#define SPECIAL_OOP_CLOSURE_WINDOW 12
#define SPECIAL_OOP_COMPILED_METHOD_WINDOW 13
#define SPECIAL_OOP_PRIMITIVE_METHOD_WINDOW 14
#define SPECIAL_OOP_CLOSURE_PROTO 15
#define SPECIAL_OOP_LEXICAL_CONTEXT_PROTO 16
#define SPECIAL_OOP_INTERPRETER 17
#define SPECIAL_OOP_ENSURE_MARKER 18
#define SPECIAL_OOP_NOT_FOUND_ON 19
#define SPECIAL_OOP_NOT_FOUND_ON_AFTER 20
#define SPECIAL_OOP_WRONG_INPUTS_TO 21
#define SPECIAL_OOP_MAY_NOT_RETURN_TO 22
#define SPECIAL_OOP_SLOT_NOT_FOUND_NAMED 23
#define SPECIAL_OOP_KEY_NOT_FOUND_ON 24
#define SPECIAL_OOP_IMMUTABLE 25
#define SPECIAL_OOP_BIT_SHIFT_OVERFLOW 26
#define SPECIAL_OOP_ADD_OVERFLOW 27 
#define SPECIAL_OOP_SUBTRACT_OVERFLOW 28
#define SPECIAL_OOP_MULTIPLY_OVERFLOW 29
#define SPECIAL_OOP_DIVIDE_BY_ZERO 30
#define SPECIAL_OOP_NOT_A_BOOLEAN 31
#define SPECIAL_OOP_APPLY_TO 32
#define SPECIAL_OOP_OPTIONALS 33

#define SF_READ				1
#define SF_WRITE			1 << 1
#define SF_CREATE			1 << 2
#define SF_CLEAR			1 << 3


#ifdef PRINT_DEBUG_OPCODES
#define PRINTOP(X) printf(X)
#else
#define PRINTOP(X)
#endif


void error(char* str) {
  fprintf(stderr, str);
  assert(0);
}

void fill_bytes_with(byte_t* dst, word_t n, byte_t value)
{
  while (n > 0)
  {
    *dst = value;
    dst++;
    n--;
  }
}

void fill_words_with(word_t* dst, word_t n, word_t value)
{
  while (n > 0)
  {
    *dst = value;
    dst++;
    n--;
  }
}

void copy_words_into(word_t * src, word_t n, word_t * dst)
{
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
}

void copy_bytes_into(byte_t * src, word_t n, byte_t * dst)
{
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
}



void print_object(struct Object* oop) {
  if (oop == NULL) {
    printf("<object NULL>\n");
    return;
  }
  if (oop_is_smallint((word_t)oop)) {
    printf("<object int_value: %ld>\n", object_to_smallint(oop));
  } else {
    char* typestr=0;
    switch (object_type(oop)) {
    case TYPE_OBJECT: typestr = "normal"; break;
    case TYPE_OOP_ARRAY: typestr = "oop array"; break;
    case TYPE_BYTE_ARRAY: typestr = "byte array"; break;
    }
    printf("<object at %p, hash: 0x%lX, size: %ld, payload size: %ld, type: %s>\n", (void*)oop, object_hash(oop), object_size(oop), payload_size(oop), typestr);
  }

}


struct Object* get_special(struct object_heap* oh, word_t special_index) {
  return oh->special_objects_oop->elements[special_index];
}

struct Map* object_get_map(struct object_heap* oh, struct Object* o) {
  if (object_is_smallint(o)) return get_special(oh, SPECIAL_OOP_SMALL_INT_PROTO)->map;
  return o->map;
}

void cache_specials(struct object_heap* heap) {

 heap->cached.interpreter = (struct Interpreter*) get_special(heap, SPECIAL_OOP_INTERPRETER);
 heap->cached.true = (struct Object*) get_special(heap, SPECIAL_OOP_TRUE);
 heap->cached.false = (struct Object*) get_special(heap, SPECIAL_OOP_FALSE);
 heap->cached.nil = (struct Object*) get_special(heap, SPECIAL_OOP_NIL);
 heap->cached.primitive_method_window = (struct Object*) get_special(heap, SPECIAL_OOP_PRIMITIVE_METHOD_WINDOW);
 heap->cached.compiled_method_window = (struct Object*) get_special(heap, SPECIAL_OOP_COMPILED_METHOD_WINDOW);
 heap->cached.closure_method_window = (struct Object*) get_special(heap, SPECIAL_OOP_CLOSURE_WINDOW);

}



/*************** BASIC OBJECT FIELDS ******************/


/* for any assignment where an old gen object points to a new gen
object.  call it before the next GC happens. !Before any stack pushes! */
void heap_store_into(struct object_heap* oh, struct Object* src, struct Object* dest);



word_t object_is_immutable(struct Object* o) {return ((word_t)o->map->flags & MAP_FLAG_IMMUTABLE) != 0; }



word_t object_word_size(struct Object* o) {

  if (object_type(o) == TYPE_OBJECT) {
    return object_size(o);
  } 
  return object_size(o) + (payload_size(o) + sizeof(word_t) - 1) / sizeof(word_t); 

}

word_t object_array_offset(struct Object* o) {
  return object_size((struct Object*)o) * sizeof(word_t);
}

byte_t* byte_array_elements(struct ByteArray* o) {
  byte_t* elements = (byte_t*)inc_ptr(o, object_array_offset((struct Object*)o));
  return elements;
}

byte_t byte_array_get_element(struct Object* o, word_t i) {
  return byte_array_elements((struct ByteArray*)o)[i];
}

byte_t byte_array_set_element(struct ByteArray* o, word_t i, byte_t val) {
  return byte_array_elements(o)[i] = val;
}


struct Object* object_array_get_element(struct Object* o, word_t i) {
  struct Object** elements = (struct Object**)inc_ptr(o, object_array_offset(o));
  return elements[i];
}

struct Object* object_array_set_element(struct object_heap* oh, struct Object* o, word_t i, struct Object* val) {
  struct Object** elements = (struct Object**)inc_ptr(o, object_array_offset(o));
  heap_store_into(oh, o, val);
  return elements[i] = val;
}

struct Object** object_array_elements(struct Object* o) {
  return (struct Object**)inc_ptr(o, object_array_offset(o));
}

struct Object** array_elements(struct OopArray* o) {
  return object_array_elements((struct Object*) o);
}

float_t* float_part(struct ByteArray* o) {
  return (float_t*)object_array_elements((struct Object*) o);
}

word_t object_array_size(struct Object* o) {

  assert(object_type(o) != TYPE_OBJECT);
  return (payload_size(o) + sizeof(word_t) - 1) / sizeof(word_t);

}

word_t byte_array_size(struct ByteArray* o) {
  return payload_size((struct Object*)o);
}


word_t array_size(struct OopArray* x) {
  return object_array_size((struct Object*) x);
}

word_t slot_table_capacity(struct SlotTable* roles) {
  return object_array_size((struct Object*)roles) / ((sizeof(struct SlotEntry) + (sizeof(word_t) - 1)) / sizeof(word_t));
}

word_t role_table_capacity(struct RoleTable* roles) {
  return object_array_size((struct Object*)roles) / ((sizeof(struct RoleEntry) + (sizeof(word_t) - 1)) / sizeof(word_t));
}



word_t object_byte_size(struct Object* o) {
  if (object_type(o) == TYPE_OBJECT) {
    return object_array_offset(o);
  } 
  return object_array_offset(o) + payload_size(o);

}

word_t object_total_size(struct Object* o) {
  /*IMPORTANT: rounds up to word size*/

  return object_word_size(o)*sizeof(word_t);

}

word_t object_first_slot_offset(struct Object* o) {

  return sizeof(struct Object);

}

word_t object_last_slot_offset(struct Object* o) {
  return object_size(o) * sizeof(word_t) - sizeof(word_t);
}


word_t object_last_oop_offset(struct Object* o) {

  if (object_type(o) == TYPE_OOP_ARRAY) {
    return object_last_slot_offset(o) + payload_size(o);
  }
  return object_last_slot_offset(o);
}

struct Object* object_slot_value_at_offset(struct Object* o, word_t offset) {

  return (struct Object*)*((word_t*)inc_ptr(o, offset));

}


struct Object* object_slot_value_at_offset_put(struct object_heap* oh, struct Object* o, word_t offset, struct Object* value) {

  heap_store_into(oh, o, value);
  return (struct Object*)((*((word_t*)inc_ptr(o, offset))) = (word_t)value);

}



/*************** DEBUG PRINTING ******************/


void print_symbol(struct Symbol* name) {
  fwrite(&name->elements[0], 1, payload_size((struct Object*)name), stdout);
}

void indent(word_t amount) { word_t i; for (i=0; i<amount; i++) printf("    "); }


void print_byte_array(struct Object* o) {
  word_t i;
  char* elements = (char*)inc_ptr(o, object_array_offset(o));
  printf("'");
  for (i=0; i < payload_size(o); i++) {
    if (elements[i] >= 32 && elements[i] <= 126) {
      printf("%c", elements[i]);
    } else {
      printf("\\x%02lx", (word_t)elements[i]);
    }
    if (i > 10 && payload_size(o) - i > 100) {
      i = payload_size(o) - 20;
      printf("{..snip..}");
    }
  }
  printf("'");

}

word_t max(word_t x, word_t y) {
  if (x > y) return x;
  return y;
}

void print_object_with_depth(struct object_heap* oh, struct Object* o, word_t depth, word_t max_depth) {

  struct Map* map;
  word_t i;

  if (depth >= max_depth || object_is_smallint(o)) {
    printf("(depth limit reached) "); print_object(o);
    return;
  }

  if (o == oh->cached.nil) {
    printf("<object NilObject>\n");
    return;
  }

  map = o->map;
  print_object(o);

  indent(depth); printf("{\n");

  if (object_type(o) == TYPE_BYTE_ARRAY) {
    indent(depth); printf("bytes: ");
    print_byte_array(o); printf("\n");
  }
  if (object_type(o) == TYPE_OOP_ARRAY) {
    struct OopArray* array = (struct OopArray*)o;
    indent(depth); printf("size(array) = %ld\n", object_array_size(o));
    for (i=0; i < object_array_size(o); i++) {
      indent(depth); printf("array[%ld]: ", i); print_object_with_depth(oh, array->elements[i], depth+1, max_depth);
      if (object_array_size(o) - i > 10) {
        indent(depth); printf("...\n");
        i = object_array_size(o) - 2;
      }
    }
  }
  indent(depth);
  printf("map flags: %ld (%s)\n", 
         object_to_smallint(map->flags),
         ((((word_t)map->flags & MAP_FLAG_RESTRICT_DELEGATION)==0)? "delegation not restricted":"delegation restricted"));

  {
    /*print if delegate*/
    
    struct OopArray* delegates = map->delegates;
    word_t offset = object_array_offset((struct Object*)delegates);
    word_t limit = object_total_size((struct Object*)delegates);
    for (i = 0; offset != limit; offset += sizeof(word_t), i++) {
      struct Object* delegate = object_slot_value_at_offset((struct Object*)delegates, offset);
      indent(depth); printf("delegate[%ld] = ", i);
      print_object_with_depth(oh, delegate, 
                              (((word_t)map->flags & MAP_FLAG_RESTRICT_DELEGATION) == 0)?  depth+1 : max(max_depth-1, depth+1), max_depth);
    }
  }

  {/*if?*/
    struct SlotTable* slotTable = map->slotTable;
    word_t limit = slot_table_capacity(map->slotTable);
    indent(depth); printf("slot count: %ld\n", object_to_smallint(map->slotCount));
    for (i=0; i < limit; i++) {
      if (slotTable->slots[i].name == (struct Symbol*)oh->cached.nil) continue;
      indent(depth);
      printf("slot[%ld]['", i); print_symbol(slotTable->slots[i].name); printf("'] = ");
      {
        struct Object* obj = object_slot_value_at_offset(o, object_to_smallint(slotTable->slots[i].offset));
        if (object_is_smallint(obj) || object_type(obj) != TYPE_BYTE_ARRAY) {
          print_object(obj);
        } else {
          print_byte_array(obj); printf("\n");
        }
      }
    }
  }


  /*indent(depth); printf("map = "); print_object_with_depth(oh, (struct Object*)map, depth+1, max_depth);*/
    
  /*roles */
  {
    struct RoleTable* roleTable = map->roleTable;
    word_t limit = role_table_capacity(roleTable);
    indent(depth); printf("role table capacity: %ld\n", limit);
    for (i = 0; i < limit; i++) {
      if (roleTable->roles[i].name == (struct Symbol*)oh->cached.nil) {continue;}
      else {
        indent(depth); printf("role[%ld]['", i); print_symbol(roleTable->roles[i].name);
        printf("'] @ 0x%04lx\n", object_to_smallint(roleTable->roles[i].rolePositions));
#if 0
        print_object_with_depth(oh, (struct Object*)roleTable->roles[i].methodDefinition, max_depth, max_depth);
#endif
        if (limit - i > 50 && depth >= 2) {
          indent(depth); printf("...\n");
          i = limit - 3;
        }
      }
      
    }
    
  }


  indent(depth); printf("}\n");

}

void print_detail(struct object_heap* oh, struct Object* o) {
  print_object_with_depth(oh, o, 1, 5);
}

bool print_printname(struct object_heap* oh, struct Object* o) {

  word_t i;
  struct Map* map = o->map;
  struct SlotTable* slotTable = map->slotTable;
  word_t limit = slot_table_capacity(slotTable);
  for (i=0; i < limit; i++) {
    if (slotTable->slots[i].name == (struct Symbol*)oh->cached.nil) continue;
    if (strncmp((char*)slotTable->slots[i].name->elements, "printName", 9) == 0) {
      struct Object* obj = object_slot_value_at_offset(o, object_to_smallint(slotTable->slots[i].offset));
      if (object_is_smallint(obj) || object_type(obj) != TYPE_BYTE_ARRAY) {
        return FALSE;
      } else {
        print_byte_array(obj);
        return TRUE;
      }
    }
  }
  return FALSE;
}

void print_type(struct object_heap* oh, struct Object* o) {
  struct Object* traits;
  struct Object* traitsWindow;
  struct OopArray* x;
  if (object_is_smallint(o)) {
    printf("<smallint value: %ld (0x%lx)>\n", object_to_smallint(o), object_to_smallint(o));
    return;
  }

  if (object_type(o) == TYPE_BYTE_ARRAY) {
    printf("(");
    print_byte_array(o);
    printf(") ");
    /*return;*/
  }
  
  x = o->map->delegates;
  if (!x || array_size(x) < 1) goto fail;

  traitsWindow = x->elements[0];

  {
    if (traitsWindow == oh->cached.compiled_method_window) {
      printf("(method: ");
      print_byte_array((struct Object*)(((struct CompiledMethod*)o)->method->selector));
      printf(")");
    }
  }


  x = traitsWindow->map->delegates;
  if (!x || array_size(x) < 1) goto fail;

  traits = x->elements[array_size(x)-1];



  print_printname(oh, o);
  
  printf("(");
  print_printname(oh, traits);
  printf(")\n");


  return;
 fail:
  printf("<unknown type>\n");
}


void print_stack(struct object_heap* oh) {
  struct Interpreter* i = oh->cached.interpreter;
  word_t size = i->stackPointer;
  word_t j;
  for (j=0; j < size; j++) {
    printf("stack[%ld] = ", j);
    print_detail(oh, i->stack->elements[j]);
  }
}

void print_stack_types(struct object_heap* oh, word_t last_count) {
  struct Interpreter* i = oh->cached.interpreter;
  word_t size = i->stackPointer;
  word_t j;
  for (j=0; j < size; j++) {
    if (size - j > last_count) continue;
    printf("stack[%ld] = ", j);
    print_type(oh, i->stack->elements[j]);
  }
}


void print_backtrace(struct object_heap* oh) {
  word_t depth = 0, detail_depth = -1 /*raise this to print verbose args in stack longer*/;
  struct Interpreter* i = oh->cached.interpreter;
  word_t fp = i->framePointer;
  printf("backtrace: fp=%ld, sp=%ld\n", fp, i->stackPointer);
  while (fp > FUNCTION_FRAME_SIZE) {
    word_t j;
    struct Object** vars;
    struct Closure* closure = (struct Closure*)i->stack->elements[fp-3];
    struct LexicalContext* lc = (struct LexicalContext*)i->stack->elements[fp-2];
    word_t input_count = object_to_smallint(closure->method->inputVariables);
    word_t local_count = object_to_smallint(closure->method->localVariables);

    vars = (closure->method->heapAllocate == oh->cached.true)? (&lc->variables[0]) : (&i->stack->elements[fp]);
    printf("------------------------------\n");
    printf("fp: %ld\n", fp);
    printf("method: "); print_byte_array((struct Object*)(closure->method->selector)); printf("\n");

    for (j = 0; j < input_count; j++) {
      printf("arg[%ld] (%p) = ", j, (void*)vars[j]);
      if (depth > detail_depth) {
        print_type(oh, vars[j]);
      } else {
        print_detail(oh, vars[j]);
      }
    }

    if (closure->method->heapAllocate == oh->cached.true) {
      for (j = 0; j < local_count; j++) {
        printf("var[%ld] (%p)= ", j, (void*)lc->variables[j]);
        if (depth > detail_depth) {
          print_type(oh, lc->variables[j]);
        } else {
          print_detail(oh, lc->variables[j]);
        }
      }
    } else {
      for (j = input_count; j < input_count + local_count; j++) {
        printf("var[%ld] (%p) = ", j - input_count, (void*)vars[j]);
        if (depth > detail_depth) {
          print_type(oh, vars[j]);
        } else {
          print_detail(oh, vars[j]);
        }
      }
    }



    fp = object_to_smallint(i->stack->elements[fp-1]);
    depth++;
  }

}

/***************** MEMORY *************************/

void initMemoryModule (struct object_heap* oh)
{
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

void closeMemory (struct object_heap* oh, int memory)
{
  if (validMemoryHandle(oh, memory)) {
    if (!--oh->memory_num_refs [memory]) {
      free (oh->memory_areas [memory]);
      oh->memory_areas [memory] = NULL;
      oh->memory_sizes [memory] = 0;
    }
  }
}

void addRefMemory (struct object_heap* oh, int memory)
{
  if (validMemoryHandle(oh, memory))
      ++oh->memory_num_refs [memory];
}
      
int openMemory (struct object_heap* oh, int size)
{
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

int writeMemory (struct object_heap* oh, int memory, int memStart, int n, char* bytes)
{
  void* area;
  int nDelimited;
  if (!(validMemoryHandle(oh, memory)))
    return SLATE_ERROR_RETURN;
  area = oh->memory_areas [memory];
  nDelimited = oh->memory_sizes [memory] - memStart;
  if (nDelimited > n)
    nDelimited = n; // We're just taking the max of N and the amount left.
  if (nDelimited > 0)
    memcpy (bytes, area, nDelimited);
  return nDelimited;
}

int readMemory (struct object_heap* oh, int memory, int memStart, int n, char* bytes)
{
  void* area;
  int nDelimited;
  if (!(validMemoryHandle(oh, memory)))
    return SLATE_ERROR_RETURN;
  area = oh->memory_areas [memory];
  nDelimited = oh->memory_sizes [memory] - memStart;
  if (nDelimited > n)
    nDelimited = n; // We're just taking the max of N and the amount left.
  if (nDelimited > 0)
    memcpy (area, bytes, nDelimited);
  return nDelimited;
}

int sizeOfMemory (struct object_heap* oh, int memory)
{
  return (validMemoryHandle(oh, memory) ? oh->memory_sizes [memory] : SLATE_ERROR_RETURN);
}

int resizeMemory (struct object_heap* oh, int memory, int size)
{
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

int addressOfMemory (struct object_heap* oh, int memory, int offset, byte_t* addressBuffer)
{
  void* result;
  result = ((validMemoryHandle(oh, memory)
             && 0 <= offset && offset < oh->memory_sizes [memory])
            ? (char *) oh->memory_areas [memory] + offset : NULL);
  memcpy (addressBuffer, (char *) &result, sizeof (&result));
  return sizeof (result);
}



/***************** EXTLIB *************************/

#if defined(__CYGWIN__)
#define DLL_FILE_NAME_EXTENSION ".dll"
#else
#define DLL_FILE_NAME_EXTENSION ".so"
#endif 



static char *safe_string(struct ByteArray *s, char const *suffix) {
  size_t len = byte_array_size(s);
  char *result = malloc(strlen(suffix) + len + 1);
  if (result == NULL)
    return NULL;
  memcpy(result, s->elements, len);
  strcpy(result + len, suffix);
  return result;
}

bool openExternalLibrary(struct object_heap* oh, struct ByteArray *libname, struct ByteArray *handle)
{
  char *fullname;
  void *h;

  assert(byte_array_size(handle) >= sizeof(h));

  fullname = safe_string(libname, DLL_FILE_NAME_EXTENSION);
  if (fullname == NULL)
    return FALSE;

  h = dlopen(fullname, RTLD_NOW);

  char *message = dlerror();
  if (message != NULL) {
    fprintf (stderr, "openExternalLibrary '%s' error: %s\n", fullname, message);
  }
  free(fullname);

  if (h == NULL) {
    return FALSE;
  } else {
    memcpy(handle->elements, &h, sizeof(h));
    return TRUE;
  }
}

bool closeExternalLibrary(struct object_heap* oh, struct ByteArray *handle) {
  void *h;

  assert(byte_array_size(handle) >= sizeof(h));
  memcpy(&h, handle->elements, sizeof(h));

  return (dlclose(h) == 0) ? TRUE : FALSE;
}

bool lookupExternalLibraryPrimitive(struct object_heap* oh, 
                                    struct ByteArray *handle,
				   struct ByteArray *symname,
				   struct ByteArray *ptr)
{
  void *h;
  void *fn;
  char *symbol;

  assert(byte_array_size(handle) >= sizeof(h));
  assert(byte_array_size(ptr) >= sizeof(fn));

  symbol = safe_string(symname, "");
  if (symbol == NULL)
    return FALSE;

  memcpy(&h, handle->elements, sizeof(h));
  fn = (void *) dlsym(h, symbol);
  free(symbol);

  if (fn == NULL) {
    return FALSE;
  } else {
    memcpy(ptr->elements, &fn, sizeof(fn));
    return TRUE;
  }
}

int readExternalLibraryError(struct ByteArray *messageBuffer) {
  char *message = dlerror();
  if (message == NULL)
    return 0;
  int len = strlen(message);
  assert(byte_array_size(messageBuffer) >= len);
  memcpy(messageBuffer->elements, message, len);
  return len;
}




/***************** FILES *************************/


word_t write_args_into(struct object_heap* oh, char* buffer, word_t limit) {

  word_t i, iLen, nRemaining, totalLen;

  nRemaining = limit;
  totalLen = 0;
  for (i=0; i<oh->argcSaved; i++) {
    iLen = strlen (oh->argvSaved [i]) + 1;
    memcpy (buffer + totalLen, oh->argvSaved [i], max(iLen, nRemaining));
    totalLen += iLen;

    nRemaining = max(nRemaining - iLen, 0);
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


word_t extractCString (struct ByteArray * array, byte_t* buffer, word_t bufferSize)
{
  word_t arrayLength = byte_array_extract_into(array, (byte_t*)buffer, bufferSize - 1);

  if (arrayLength < 0)
    return SLATE_ERROR_RETURN;

  buffer [arrayLength] = '\0';	
  return arrayLength;
}

bool valid_handle(struct object_heap* oh, word_t file) {
  return (0 <= file && file < oh->file_index_size && oh->file_index[file] != NULL);
}

word_t allocate_file(struct object_heap* oh)
{
  word_t file;
  word_t initial_size = oh->file_index_size;
  for (file = 0; file < initial_size; ++ file) {
    if (!(valid_handle(oh, file)))
      return file;
  }
  oh->file_index_size *= 2;
  oh->file_index = realloc(oh->file_index, oh->file_index_size * sizeof(FILE*));
  assert(oh->file_index);

  for (file = initial_size; file < oh->file_index_size; file++) {
    oh->file_index[file] = NULL;
  }
  
  return initial_size;
}

void closeFile(struct object_heap* oh, word_t file)
{
  if (valid_handle(oh, file)) {
    fclose (oh->file_index[file]);
    oh->file_index[file] = NULL;
  }
}

word_t openFile(struct object_heap* oh, struct ByteArray * name, word_t flags)
{
  byte_t nameString[SLATE_FILE_NAME_LENGTH];
  word_t nameLength;
  char mode[8];
  word_t modeIndex = 0;
  word_t file;

  nameLength = extractCString(name, nameString, sizeof(nameString));

  if (nameLength <= 0)
    return SLATE_ERROR_RETURN;

  file = allocate_file(oh);
  if (file < 0)
    return SLATE_ERROR_RETURN;

  // (CLEAR \/ CREATE) /\ !WRITE
  if ((flags & SF_CLEAR || flags & SF_CREATE) && (! (flags & SF_WRITE)))
    error("ANSI does not support clearing or creating files that are not opened for writing");

  // WRITE /\ !READ
  if (flags & SF_WRITE && (! (flags & SF_READ)))
    error("ANSI does not support opening files for writing only");

  if (flags & SF_WRITE) {
    if (flags & SF_CLEAR) {
      mode[modeIndex++] = 'w';
      if (flags & SF_READ)
	mode[modeIndex++] = '+';
    } else {
      mode[modeIndex++] = 'r';
      mode[modeIndex++] = '+';
    }
  } else if (flags & SF_READ)
    mode[modeIndex++] = 'r';
  else {
    fprintf(stderr, "Slate: Unexpected mode flags for ANSI file module: %ld, falling back to \"r\"", flags);
    mode[modeIndex++] = 'r';
  }

  mode[modeIndex++] = 'b';
  mode[modeIndex++] = 0;

  oh->file_index[file] = fopen((char*)nameString, mode);
  return (valid_handle(oh, file) ? file : SLATE_ERROR_RETURN);
}

word_t writeFile(struct object_heap* oh, word_t file, word_t n, char * bytes)
{
  return (valid_handle(oh, file) ? fwrite (bytes, 1, n, oh->file_index[file])
	  : SLATE_ERROR_RETURN);
}

word_t readFile(struct object_heap* oh, word_t file, word_t n, char * bytes)
{
  return (valid_handle(oh, file) ? fread (bytes, 1, n, oh->file_index[file])
	  : SLATE_ERROR_RETURN);
}

word_t sizeOfFile(struct object_heap* oh, word_t file)
{
  word_t pos, size;
  if (!(valid_handle(oh, file)))
    return SLATE_ERROR_RETURN;
  pos = ftell (oh->file_index[file]);
  fseek (oh->file_index[file], 0, SEEK_END);
  size = ftell (oh->file_index[file]);
  fseek (oh->file_index[file], pos, SEEK_SET);
  return size;
}

word_t seekFile(struct object_heap* oh, word_t file, word_t offset)
{
  return (valid_handle(oh, file) && fseek (oh->file_index[file], offset, SEEK_SET) == 0
	  ? ftell (oh->file_index[file]) : SLATE_ERROR_RETURN);
}

word_t tellFile(struct object_heap* oh, word_t file)
{
  return (valid_handle(oh, file) ?  ftell (oh->file_index[file]) : SLATE_ERROR_RETURN);
}

bool endOfFile(struct object_heap* oh, word_t file)
{
  word_t c;
  if (!(valid_handle(oh, file)))
    return TRUE;
  c = fgetc (oh->file_index[file]);
  if (c == EOF)
    return TRUE;
  else {
    ungetc (c, oh->file_index[file]);
    return FALSE;
  }
}



int getCurrentDirectory(struct ByteArray *wdBuffer) {
  return ((getcwd((char *)wdBuffer->elements, byte_array_size(wdBuffer)) == NULL)
	  ? -errno : strlen((const char *)wdBuffer->elements));
}

int setCurrentDirectory(struct ByteArray *newWd) {
  word_t wdLen = byte_array_size(newWd);
  char *wd = malloc(wdLen + 1);
  if (wd == NULL)
    return -errno;

  memcpy(wd, newWd->elements, wdLen);
  wd[wdLen] = '\0';
  if (chdir(wd) == -1) {
    int savedErrno = errno;
    free(wd);
    return -savedErrno;
  } else {
    free(wd);
    return 0;
  }
}


/*************** MEMORY ******************/

void heap_gc(struct object_heap* oh);
void heap_full_gc(struct object_heap* oh);





bool object_is_old(struct object_heap* oh, struct Object* oop) {
  return (oh->memoryOld <= (byte_t*)oop && (word_t)oh->memoryOld + oh->memoryOldSize > (word_t)oop);

}

bool object_is_young(struct object_heap* oh, struct Object* obj) {
  return (oh->memoryYoung <= (byte_t*)obj && (byte_t*)oh->memoryYoung + oh->memoryYoungSize > (byte_t*)obj);
  
}

bool object_in_memory(struct object_heap* oh, struct Object* oop, byte_t* memory, word_t memorySize) {
  return (memory <= (byte_t*)oop && (byte_t*)memory + memorySize > (byte_t*)oop);

}

struct Object* object_after(struct object_heap* heap, struct Object* o) {

  assert(object_total_size(o) != 0);

  return (struct Object*)inc_ptr(o, object_total_size(o));
}

bool object_is_marked(struct object_heap* heap, struct Object* o) {

  return (object_markbit(o) == heap->mark_color);
}

bool object_is_free(struct Object* o) {

  return (object_hash(o) >= ID_HASH_RESERVED);
}

void method_flush_cache(struct object_heap* oh, struct Symbol* selector) {
  struct MethodCacheEntry* cacheEntry;
  if (selector == (struct Symbol*)oh->cached.nil || selector == NULL) {
#if 1
    fill_bytes_with((byte_t*)oh->methodCache, METHOD_CACHE_SIZE*sizeof(struct MethodCacheEntry), 0);
#else
    /*flush only things that pointed to young memory*/
    word_t i, j;
    for (i = 0; i < METHOD_CACHE_SIZE; i++) {
      cacheEntry = &oh->methodCache[i];
      if (object_is_young(oh, (struct Object*)cacheEntry->selector) || object_is_young(oh, (struct Object*)cacheEntry->method)) goto zero;
      for (j = 0; j < METHOD_CACHE_ARITY; j++) {
        if (object_is_young(oh, (struct Object*)cacheEntry->maps[j])) goto zero;
      }
      /*it only points to old (same addressed) memory so leave it*/
      return;
    zero:
      fill_bytes_with((byte_t*)oh->methodCache, METHOD_CACHE_SIZE*sizeof(struct MethodCacheEntry), 0);
    }
#endif
  } else {
    word_t i;
    for (i = 0; i < METHOD_CACHE_SIZE; i++) {
      if ((cacheEntry = &oh->methodCache[i])->selector == selector) cacheEntry->selector = (struct Symbol*)oh->cached.nil;
    }
  }
}

struct Object* heap_make_free_space(struct object_heap* oh, struct Object* obj, word_t words) {

  assert(words > 0);

#ifdef PRINT_DEBUG
  printf("Making %ld words of free space at: %p\n", words, (void*)start);
#endif


  object_set_format(obj, TYPE_OBJECT);
  object_set_size(obj, words);
  payload_set_size(obj, 0); /*zero it out or old memory will haunt us*/
  obj->map = NULL;
  /*fix should we mark this?*/
  object_set_idhash(obj, ID_HASH_FREE);
#if 0
  fill_words_with((word_t*)(obj+1), words-sizeof(struct Object), 0);
#endif
  return obj;
}

struct Object* heap_make_used_space(struct object_heap* oh, struct Object* obj, word_t words) {
  struct Object* x = heap_make_free_space(oh, obj, words);
  object_set_idhash(obj, heap_new_hash(oh));
  return x;
}


bool heap_initialize(struct object_heap* oh, word_t size, word_t limit, word_t young_limit, word_t next_hash, word_t special_oop, word_t cdid) {
  void* oldStart = (void*)0x10000000;
  void* youngStart = (void*)0x80000000;
  oh->memoryOldLimit = limit;
  oh->memoryYoungLimit = young_limit;

  oh->memoryOldSize = size;
  oh->memoryYoungSize = young_limit;

  assert((byte_t*)oldStart + limit < (byte_t*) youngStart);
  oh->memoryOld = (byte_t*)mmap((void*)oldStart, limit, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|0x20, -1, 0);
  oh->memoryYoung = (byte_t*)mmap((void*)youngStart, young_limit, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|0x20, -1, 0);
  oh->pinnedYoungObjects = calloc(1, (oh->memoryYoungSize / PINNED_CARD_SIZE + 1) * sizeof(word_t));

  /*perror("err: ");*/
  if (oh->memoryOld == NULL || oh->memoryOld == (void*)-1
      || oh->memoryYoung == NULL || oh->memoryYoung == (void*)-1) {
    fprintf(stderr, "Initial GC allocation of memory failed.\n");
    return 0;
  }

  oh->nextFree = (struct Object*)oh->memoryYoung;
  oh->nextOldFree = (struct Object*)oh->memoryOld;
  heap_make_free_space(oh, oh->nextFree, oh->memoryYoungSize / sizeof(word_t));


  oh->special_objects_oop = (struct OopArray*)((byte_t*)oh->memoryOld + special_oop);
  oh->lastHash = next_hash;
  oh->current_dispatch_id = cdid;
  oh->interrupt_flag = 0;
  oh->mark_color = 1;
  oh->file_index_size = 256;
  oh->file_index = calloc(oh->file_index_size, sizeof(FILE*));
  oh->markStackSize = 1024*1024*4;
  oh->markStackPosition = 0;
  oh->markStack = malloc(oh->markStackSize * sizeof(struct Object*));
  
  oh->optimizedMethodsSize = 0;
  oh->optimizedMethodsLimit = 1024;
  oh->optimizedMethods = malloc(oh->optimizedMethodsLimit * sizeof(struct CompiledMethod*));

  assert(oh->markStack != NULL);
  initMemoryModule(oh);
  return 1;
}

void gc_close(struct object_heap* oh) {

  munmap(oh->memoryOld, oh->memoryOldLimit);
  munmap(oh->memoryYoung, oh->memoryYoungLimit);

}
bool object_is_pinned(struct object_heap* oh, struct Object* x) {
  if (object_is_young(oh, x)) {
    word_t diff = (byte_t*) x - oh->memoryYoung;
    return (oh->pinnedYoungObjects[diff / PINNED_CARD_SIZE] & (1 << (diff % PINNED_CARD_SIZE))) != 0;
  }
  return 0;
  
}


struct Object* heap_find_first_young_free(struct object_heap* oh, struct Object* obj, word_t bytes) {
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {
    if (object_is_free(obj) && object_total_size(obj) >= bytes) return obj;
    obj = object_after(oh, obj);
  }
  return NULL;
}

struct Object* heap_find_first_old_free(struct object_heap* oh, struct Object* obj, word_t bytes) {
  while (object_in_memory(oh, obj, oh->memoryOld, oh->memoryOldSize)) {
    if (object_is_free(obj) && object_total_size(obj) >= bytes) return obj;
    obj = object_after(oh, obj);
  }
  return NULL;
}

void heap_print_objects(struct object_heap* oh, byte_t* memory, word_t memorySize) {
  struct Object* obj = (struct Object*) memory;
  word_t pin_count = 0, used_count = 0, free_count = 0;
  printf("\nmemory:\n");
  while (object_in_memory(oh, obj, memory, memorySize)) {
    if (object_is_pinned(oh, obj)) {
      printf("[pinned ");
      pin_count++;
    } else if (object_is_free(obj)) {
      printf("[free ");
      free_count++;
    } else {
      printf("[used ");
      used_count++;
    }
    printf("%ld]\n", object_total_size(obj));

    obj = object_after(oh, obj);
  }
  printf("\n");
  printf("free: %ld, used: %ld, pinned: %ld\n", free_count, used_count, pin_count);

}

struct Object* gc_allocate_old(struct object_heap* oh, word_t bytes) {
  word_t words = bytes / sizeof(word_t);
  assert(bytes % sizeof(word_t) == 0);

  if (!object_in_memory(oh, oh->nextOldFree, oh->memoryOld, oh->memoryOldSize)) {
    oh->nextOldFree = (struct Object*)oh->memoryOld;
  }

  oh->nextOldFree = heap_find_first_old_free(oh, oh->nextOldFree, bytes + sizeof(struct Object));

  if (oh->nextOldFree == NULL) {
    assert(oh->memoryOldSize+bytes < oh->memoryOldLimit);
    oh->nextOldFree = (struct Object*)(oh->memoryOld + oh->memoryOldSize);
    heap_make_used_space(oh, oh->nextOldFree, words);
    oh->memoryOldSize += bytes;
    return oh->nextOldFree;

  } else {
    struct Object* next;
    word_t oldsize;
    oldsize = object_word_size(oh->nextOldFree);
    heap_make_used_space(oh, oh->nextOldFree, words);
    next = object_after(oh, oh->nextOldFree);
    heap_make_free_space(oh, next, oldsize - words);
    return oh->nextOldFree;
  }

}


struct Object* gc_allocate(struct object_heap* oh, word_t bytes) {
  bool already_scavenged = 0, already_full_gc = 0;
  struct Object* next;
  word_t oldsize;
  word_t words = bytes / sizeof(word_t);

  assert(bytes % sizeof(word_t) == 0);

 start:
  if (!object_in_memory(oh, oh->nextFree, oh->memoryYoung, oh->memoryYoungSize)) {
    oh->nextFree = (struct Object*)oh->memoryYoung;
  }
  /*fix also look for things the exact same size*/
  /*plus room to break in half*/
  oh->nextFree = heap_find_first_young_free(oh, oh->nextFree, bytes + sizeof(struct Object));
  if (oh->nextFree == NULL) {
    if (!already_scavenged) {
      heap_gc(oh);
      already_scavenged = 1;

    } else if (!already_full_gc) {
      already_full_gc = 1;
      heap_full_gc(oh);
    } else {
      heap_print_objects(oh, oh->memoryYoung, oh->memoryYoungSize);
      print_backtrace(oh);
      assert(0);
    }
    oh->nextFree = (struct Object*)oh->memoryYoung;
    goto start;
  }
  /*we break the space in half if we can*/
  oldsize = object_word_size(oh->nextFree);
  heap_make_used_space(oh, oh->nextFree, words);
  next = object_after(oh, oh->nextFree);
  heap_make_free_space(oh, next, oldsize - words);
  return oh->nextFree;
  
}

void object_forward_pointers_to(struct object_heap* oh, struct Object* o, struct Object* x, struct Object* y) {

  word_t offset, limit;
  if (o->map == (struct Map*)x) {
    heap_store_into(oh, o, y);
    o->map = (struct Map*)y;
  }
  offset = object_first_slot_offset(o);
  limit = object_last_oop_offset(o) + sizeof(word_t);
  for (; offset != limit; offset += sizeof(word_t)) {
    struct Object* val = object_slot_value_at_offset(o, offset);
    if (val == x) {
#ifdef PRINT_DEBUG
      printf("Forwarding pointer in "); print_type(oh, o); printf(" to "); print_type(oh, y);
#endif
      object_slot_value_at_offset_put(oh, o, offset, y);
    }
  }
}



word_t heap_what_points_to_in(struct object_heap* oh, struct Object* x, byte_t* memory, word_t memorySize) {
  struct Object* obj = (struct Object*) memory;
  word_t count = 0;
  while (object_in_memory(oh, obj, memory, memorySize)) {
    word_t offset, limit;
    offset = object_first_slot_offset(obj);
    limit = object_last_oop_offset(obj) + sizeof(word_t);
    for (; offset != limit; offset += sizeof(word_t)) {
      struct Object* val = object_slot_value_at_offset(obj, offset);
      if (val == x) {
        if (!object_is_free(x)) count++;
        print_object(obj);
        break;
      }
    }
    obj = object_after(oh, obj);
  }
  return count;

}

word_t heap_what_points_to(struct object_heap* oh, struct Object* x) {
  
  return heap_what_points_to_in(oh, x, oh->memoryOld, oh->memoryOldSize) + heap_what_points_to_in(oh, x, oh->memoryYoung, oh->memoryYoungSize);

}

void heap_free_object(struct object_heap* oh, struct Object* obj) {
#ifdef PRINT_DEBUG_GC_MARKINGS
  printf("freeing "); print_object(obj);
#endif
  heap_make_free_space(oh, obj, object_word_size(obj));

}


void heap_finish_gc(struct object_heap* oh) {
  method_flush_cache(oh, NULL);
  /*  cache_specials(oh);*/
  fill_words_with((word_t*)oh->pinnedYoungObjects, oh->memoryYoungSize / PINNED_CARD_SIZE, 0);
}
void heap_start_gc(struct object_heap* oh) {
  oh->mark_color ^= MARK_MASK;
  oh->markStackPosition = 0;
  
}


void heap_pin_young_object(struct object_heap* oh, struct Object* x) {
  if (object_is_young(oh, x) && !object_is_smallint(x)) {
    word_t diff = (byte_t*) x - oh->memoryYoung;
#if 0
    printf("pinning "); print_object(x);
#endif
    oh->pinnedYoungObjects[diff / PINNED_CARD_SIZE] |= 1 << (diff % PINNED_CARD_SIZE);
  }
  
}


/*adds something to the mark stack and mark it if it isn't marked already*/
void heap_mark(struct object_heap* oh, struct Object* obj) {
  if (object_markbit(obj) == oh->mark_color) return;
#ifdef PRINT_DEBUG_GC_MARKINGS
  printf("marking "); print_object(obj);
#endif
  object_set_mark(oh, obj);
  
  if (oh->markStackPosition + 1 >=oh->markStackSize) {
    oh->markStackSize *= 2;
#ifdef PRINT_DEBUG
    printf("Growing mark stack to %ld\n", oh->markStackSize);
#endif
    oh->markStack = realloc(oh->markStack, oh->markStackSize * sizeof(struct Object*));
    assert(oh->markStack);
  }
  oh->markStack[oh->markStackPosition++] = obj;
}


void heap_mark_specials(struct object_heap* oh, bool mark_old) {
  word_t i;
  for (i = 0; i < array_size(oh->special_objects_oop); i++) {
    struct Object* obj = oh->special_objects_oop->elements[i];
    if (!mark_old && object_is_old(oh, obj)) continue;
    if (object_is_smallint(obj)) continue;
    heap_mark(oh, obj);
  }

}

void heap_mark_fixed(struct object_heap* oh, bool mark_old) {
  word_t i;
  for (i = 0; i < MAX_FIXEDS; i++) {
    if (oh->fixedObjects[i] != NULL) {
      struct Object* obj = oh->fixedObjects[i];
      if (!mark_old && object_is_old(oh, obj)) continue;
      if (object_is_smallint(obj)) continue;
      heap_mark(oh, obj);
    }
  }
}

void heap_mark_interpreter_stack(struct object_heap* oh, bool mark_old) {
  word_t i;
  for (i = 0; i < oh->cached.interpreter->stackPointer; i++) {
    struct Object* obj = oh->cached.interpreter->stack->elements[i];
    if (object_is_smallint(obj)) continue;
    if (!mark_old && object_is_old(oh, obj)) continue;
    heap_mark(oh, obj);
  }

}

void heap_mark_fields(struct object_heap* oh, struct Object* o) {
  word_t offset, limit;
  heap_mark(oh, (struct Object*)o->map);
  offset = object_first_slot_offset(o);
  limit = object_last_oop_offset(o) + sizeof(word_t);
  for (; offset != limit; offset += sizeof(word_t)) {
    struct Object* val = object_slot_value_at_offset(o, offset);
    if (!object_is_smallint(val)) {
      heap_mark(oh, val);
    }
  }


}

void heap_mark_recursively(struct object_heap* oh, bool mark_old) {

  while (oh->markStackPosition > 0) {
    struct Object* obj;
    oh->markStackPosition--;
    obj = oh->markStack[oh->markStackPosition];
    if (!mark_old && object_is_old(oh, obj)) continue;
    heap_mark_fields(oh, obj);
  }

}

void heap_free_and_coalesce_unmarked(struct object_heap* oh, byte_t* memory, word_t memorySize) {
  struct Object* obj = (struct Object*) memory;
  struct Object* prev = obj;
  word_t freed_words = 0, coalesce_count = 0;
  while (object_in_memory(oh, obj, memory, memorySize)) {
    if (!object_is_marked(oh, obj)) {
      freed_words += object_word_size(obj);
      heap_free_object(oh, obj);
    }
    if (object_is_free(obj) && object_is_free(prev) && obj != prev) {
      heap_make_free_space(oh, prev, object_word_size(obj)+object_word_size(prev));
      coalesce_count++;
      obj = object_after(oh, prev);
    } else {
      prev = obj;
      obj = object_after(oh, obj);
    }
    
  }
#ifdef PRINT_DEBUG_GC_1
  printf("GC Freed %ld words and coalesced %ld times\n", freed_words, coalesce_count);
#endif
}

void heap_unmark_all(struct object_heap* oh, byte_t* memory, word_t memorySize) {
  struct Object* obj = (struct Object*) memory;

  while (object_in_memory(oh, obj, memory, memorySize)) {
#ifdef PRINT_DEBUG_GC_MARKINGS
    printf("unmarking "); print_object(obj);
#endif
    object_unmark(oh, obj);
    obj = object_after(oh, obj);
  }

}

void heap_print_marks(struct object_heap* oh, byte_t* memory, word_t memorySize) {
  struct Object* obj = (struct Object*) memory;
  word_t count = 80;
  word_t pin_count = 0, used_count = 0, free_count = 0;
  printf("\nmemory:\n");
  while (object_in_memory(oh, obj, memory, memorySize)) {
    if (object_is_pinned(oh, obj)) {
      printf("X");
      pin_count++;
    } else if (object_is_free(obj)) {
      printf(" ");
      free_count++;
    } else {
      printf("*");
      used_count++;
    }
    count--;
    if (count == 0) {
      count = 80;
      printf("\n");
    }

    obj = object_after(oh, obj);
  }
  printf("\n");
  printf("free: %ld, used: %ld, pinned: %ld\n", free_count, used_count, pin_count);

}



void heap_update_forwarded_pointers(struct object_heap* oh, byte_t* memory, word_t memorySize) {
  struct Object* o = (struct Object*) memory;
 
  while (object_in_memory(oh, o, memory, memorySize)) {
    word_t offset, limit;
    if (object_is_free(o)) goto next;
    while (object_hash((struct Object*)o->map) == ID_HASH_FORWARDED) {
      o->map = (struct Map*)((struct ForwardedObject*)(o->map))->target;
    }
    assert(!object_is_free((struct Object*)o->map));
    offset = object_first_slot_offset(o);
    limit = object_last_oop_offset(o) + sizeof(word_t);
    for (; offset != limit; offset += sizeof(word_t)) {
      struct Object* val = object_slot_value_at_offset(o, offset);
      while (!object_is_smallint(val) && object_hash(val) == ID_HASH_FORWARDED) {
        object_slot_value_at_offset_put(oh, o, offset, (val=((struct ForwardedObject*)val)->target));
      }
      assert(object_is_smallint(val) || !object_is_free(val));

    }
  next:
    o = object_after(oh, o);
  }


}

void heap_tenure(struct object_heap* oh) {
  /*bring all marked young objects to old generation*/
  word_t tenure_count = 0, tenure_words = 0;
  struct Object* obj = (struct Object*) oh->memoryYoung;
  struct Object* prev;
  oh->nextOldFree = (struct Object*)oh->memoryOld;
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {
    /*if it's still there in the young section, move it to the old section */
    if (!object_is_marked(oh, obj)) {
      heap_free_object(oh, obj);
    }
    if (!object_is_free(obj) && !object_is_pinned(oh, obj)) {
      struct Object* tenure_start = gc_allocate_old(oh, object_total_size(obj));
      tenure_count++;
      tenure_words += object_word_size(obj);
      copy_words_into((word_t*)obj, object_word_size(obj), (word_t*) tenure_start);
#ifdef PRINT_DEBUG_GC_MARKINGS
    printf("tenuring from "); print_object(obj);
    printf("tenuring to "); print_object(tenure_start);
#endif

      ((struct ForwardedObject*) obj)->target = tenure_start;
      object_set_idhash(obj, ID_HASH_FORWARDED);
    }
    obj = object_after(oh, obj);
  }
#ifdef PRINT_DEBUG_GC_1
  printf("GC tenured %ld objects (%ld words)\n", tenure_count, tenure_words);
#endif

  heap_update_forwarded_pointers(oh, oh->memoryOld, oh->memoryOldSize);
  /*fixed objects in the young space need to have their pointers updated also*/
  heap_update_forwarded_pointers(oh, oh->memoryYoung, oh->memoryYoungSize);


  /*we coalesce after it has been tenured so we don't erase forwarding pointers */
  /*todo do lazily*/
  obj = (struct Object*) oh->memoryYoung;
  prev = obj;
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {

    /*coalesce free spaces*/
    if (object_is_free(obj) && object_is_free(prev) && obj != prev) {
      heap_make_free_space(oh, prev, object_word_size(obj)+object_word_size(prev));
      obj = object_after(oh, prev);
    } else {
      prev = obj;
      obj = object_after(oh, obj);
    }

  }


  oh->nextFree = (struct Object*)oh->memoryYoung;

}

void heap_mark_pinned_young(struct object_heap* oh) {
  struct Object* obj = (struct Object*) oh->memoryYoung;
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {
    if (object_hash(obj) < ID_HASH_RESERVED && object_is_pinned(oh, obj)) heap_mark(oh, obj);
    obj = object_after(oh, obj);
  }
}

void heap_sweep_young(struct object_heap* oh) {
  word_t young_count = 0, young_word_count = 0;
  
  struct Object* obj = (struct Object*) oh->memoryYoung;
  struct Object* prev = obj;
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {
    /* the pinned objects are ones on the C stack or rememberedSet (from heap_store_into)*/
    if (object_is_free(obj) && !object_is_pinned(oh, obj)) {
      young_count++;
      young_word_count += object_word_size(obj);
      if (object_is_free(prev) && obj != prev) {
        heap_make_free_space(oh, prev, object_word_size(obj)+object_word_size(prev));
        obj = object_after(oh, prev);
        continue;
      } else {
        heap_free_object(oh, obj);
      }
    }
    prev = obj;
    obj = object_after(oh, obj);
  }
#ifdef PRINT_DEBUG_GC_2
  printf("GC freed %ld young objects (%ld words)\n", young_count, young_word_count);
#endif
  oh->nextFree = (struct Object*)oh->memoryYoung;

}

void heap_pin_c_stack(struct object_heap* oh) {
  /* right now we only pin things in memoryYoung because things in memoryOld never move */
  word_t stackTop;
  word_t** stack = oh->stackBottom;
  while ((void*)stack > (void*)&stackTop) {
    heap_pin_young_object(oh, (struct Object*)*stack);
    stack--;
  }


}



void heap_full_gc(struct object_heap* oh) {
  heap_start_gc(oh);
  heap_unmark_all(oh, oh->memoryOld, oh->memoryOldSize);
  heap_unmark_all(oh, oh->memoryYoung, oh->memoryYoungSize);
  heap_pin_c_stack(oh);
  heap_mark_specials(oh, 1);
  heap_mark_interpreter_stack(oh, 1);
  heap_mark_fixed(oh, 1);
  heap_mark_recursively(oh, 1);
  /*  heap_print_marks(oh, oh->memoryYoung, oh->memoryYoungSize);*/
  heap_free_and_coalesce_unmarked(oh, oh->memoryOld, oh->memoryOldSize);
  heap_tenure(oh);
  heap_finish_gc(oh);
}

void heap_gc(struct object_heap* oh) {
#if 1
  heap_start_gc(oh);
  heap_unmark_all(oh, oh->memoryYoung, oh->memoryYoungSize);
  heap_pin_c_stack(oh);
  heap_mark_specials(oh, 0);
  heap_mark_interpreter_stack(oh, 0);
  heap_mark_fixed(oh, 0);
  heap_mark_pinned_young(oh);
  heap_mark_recursively(oh, 0);
  /*heap_print_marks(oh, oh->memoryYoung, oh->memoryYoungSize);*/
  heap_sweep_young(oh);
  heap_finish_gc(oh);
#else
  heap_full_gc(oh);
#endif
}



void heap_fixed_add(struct object_heap* oh, struct Object* x) {
  word_t i;
  for (i = 0; i < MAX_FIXEDS; i++) {
    if (oh->fixedObjects[i] == NULL) {
      oh->fixedObjects[i] = x;
      return;
    }
  }

  assert(0);
  
}

void heap_fixed_remove(struct object_heap* oh, struct Object* x) {
  word_t i;
  for (i = 0; i < MAX_FIXEDS; i++) {
    if (oh->fixedObjects[i] == x) {
      oh->fixedObjects[i] = NULL;
      return;
    }
  }
  assert(0);
}



void heap_forward_from(struct object_heap* oh, struct Object* x, struct Object* y, byte_t* memory, word_t memorySize) {
  struct Object* o = (struct Object*)memory;

  while (object_in_memory(oh, o, memory, memorySize)) {
    /*print_object(o);*/
    if (!object_is_free(o)) {
      object_forward_pointers_to(oh, o, x, y);
    }

    o = object_after(oh, o);
  }


}

void heap_forward(struct object_heap* oh, struct Object* x, struct Object* y) {

  heap_forward_from(oh, x, y, oh->memoryOld, oh->memoryOldSize);
  heap_forward_from(oh, x, y, oh->memoryYoung, oh->memoryYoungSize);
  heap_free_object(oh, x);
}

void heap_store_into(struct object_heap* oh, struct Object* src, struct Object* dest) {
  /*  print_object(dest);*/
  if (!object_is_smallint(dest)) {
    assert(object_hash(dest) < ID_HASH_RESERVED); /*catch gc bugs earlier*/
    assert(object_hash(src) < ID_HASH_RESERVED); /*catch gc bugs earlier*/
  }

  if (/*object_is_young(oh, dest) && (implicit by following cmd)*/ object_is_old(oh, src)) {
    heap_pin_young_object(oh, dest);
  }
}

struct Object* heap_allocate_with_payload(struct object_heap* oh, word_t words, word_t payload_size) {

  struct Object* o;
  /*word aligned payload*/
  word_t size = words*sizeof(word_t) + ((payload_size + sizeof(word_t) - 1) & ~(sizeof(word_t) - 1));
  o = gc_allocate(oh, size);

  object_set_format(o, TYPE_BYTE_ARRAY);
  payload_set_size(o, payload_size);
  object_set_size(o, words);
  object_set_mark(oh, o);
  assert(!object_is_free(o));
  return o;
}

struct Object* heap_allocate(struct object_heap* oh, word_t words) {
  return heap_allocate_with_payload(oh, words, 0);
}

struct Object* heap_clone(struct object_heap* oh, struct Object* proto) {
  struct Object* newObj;
  
  if (object_type(proto) == TYPE_OBJECT) {
    newObj = heap_allocate(oh, object_size(proto));
  } else {
    newObj = heap_allocate_with_payload(oh, object_size(proto), payload_size(proto));
  }

  object_set_format(newObj, object_type(proto));

  object_set_idhash(newObj, heap_new_hash(oh));
  copy_words_into((word_t*)inc_ptr(proto, HEADER_SIZE),
                  object_word_size(proto) - HEADER_SIZE_WORDS,
                  (word_t*)inc_ptr(newObj, HEADER_SIZE));

  return newObj;
}


struct Object* heap_clone_special(struct object_heap* oh, word_t special_index) {
  return heap_clone(oh, get_special(oh, special_index));
}

struct Map* heap_clone_map(struct object_heap* oh, struct Map* map) {
  return (struct Map*)heap_clone(oh, (struct Object*)map);
}

struct ByteArray* heap_new_float(struct object_heap* oh) {
  return (struct ByteArray*)heap_clone_special(oh, SPECIAL_OOP_FLOAT_PROTO);
}

struct OopArray* heap_clone_oop_array_sized(struct object_heap* oh, struct Object* proto, word_t size) {

  struct Object* newObj = heap_allocate_with_payload(oh, object_size(proto), size * sizeof(struct Object*));
  object_set_format(newObj, TYPE_OOP_ARRAY);

  object_set_idhash(newObj, heap_new_hash(oh));
  /*copy everything but the header bits*/

  copy_words_into((word_t *) inc_ptr(proto, HEADER_SIZE),
                  object_size(proto) - HEADER_SIZE_WORDS,
                  (word_t*) inc_ptr(newObj, HEADER_SIZE));

  fill_words_with((word_t*)newObj  + object_size(proto), size, (word_t)oh->cached.nil);

  return (struct OopArray*) newObj;
}

struct ByteArray* heap_clone_byte_array_sized(struct object_heap* oh, struct Object* proto, word_t bytes) {

  struct Object* newObj = heap_allocate_with_payload(oh, object_size(proto), bytes);
  object_set_format(newObj, TYPE_BYTE_ARRAY);

  object_set_idhash(newObj, heap_new_hash(oh));
  /*copy everything but the header bits*/

  copy_words_into((word_t *) inc_ptr(proto, HEADER_SIZE),
                  object_size(proto) - HEADER_SIZE_WORDS,
                  (word_t*) inc_ptr(newObj, HEADER_SIZE));

  /*assumption that we are word aligned*/
  fill_words_with((word_t*)newObj  + object_size(proto), (bytes+ sizeof(word_t) - 1) / sizeof(word_t), 0);

  return (struct ByteArray*) newObj;
}


struct ByteArray* heap_new_byte_array_with(struct object_heap* oh, word_t byte_size, byte_t* bytes) {
  struct ByteArray* arr = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), byte_size);
  word_t i;
  for (i = 0; i < byte_size; i++) {
    byte_array_set_element(arr, i, bytes[i]);
  }
  return arr;
}


struct ByteArray* heap_new_string_with(struct object_heap* oh, word_t byte_size, byte_t* bytes) {
  struct ByteArray* arr = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_ASCII_PROTO), byte_size);
  word_t i;
  for (i = 0; i < byte_size; i++) {
    byte_array_set_element(arr, i, bytes[i]);
  }
  return arr;
}

void interpreter_grow_stack(struct object_heap* oh, struct Interpreter* i, word_t minimum) {

  struct OopArray * newStack;
  
  /* i suppose this isn't treated as a SmallInt type*/
  do {
    i->stackSize *= 2;
  } while (i->stackSize < minimum);
  newStack = (struct OopArray *) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), i->stackSize);
  copy_words_into((word_t*) i->stack->elements, i->stackPointer, (word_t*) newStack->elements);
  
  i -> stack = newStack;

}

void interpreter_stack_allocate(struct object_heap* oh, struct Interpreter* i, word_t n) {

  if (i->stackPointer + n > i->stackSize) {
    interpreter_grow_stack(oh, i, i->stackPointer + n);
    assert(i->stackPointer + n <= i->stackSize);
  }
  i->stackPointer += n;

#ifdef PRINT_DEBUG_STACK_PUSH
  printf("stack allocate, new stack pointer: %ld\n", i->stackPointer);
#endif

}

void interpreter_stack_push(struct object_heap* oh, struct Interpreter* i, struct Object* value) {

#ifdef PRINT_DEBUG_STACK_PUSH
  printf("Stack push at %ld (fp=%ld): ", i->stackPointer, i->framePointer); print_type(oh, value);
#endif
  if (!object_is_smallint(value)) {
    assert(object_hash(value) < ID_HASH_RESERVED); /*catch gc bugs earlier*/
  }
  if (i->stackPointer == i->stackSize) {
    interpreter_grow_stack(oh, i, 0);
  }
  heap_store_into(oh, (struct Object*)i->stack, value);
  i->stack->elements[i -> stackPointer] = value;
  i->stackPointer++;
}

struct Object* interpreter_stack_pop(struct object_heap* oh, struct Interpreter* i) {

  if (i -> stackPointer == 0) {
    /*error("Attempted to pop empty interpreter stack.");*/
    assert(0);
  }

  i->stackPointer = i->stackPointer - 1;

  {
    struct Object* o = i->stack->elements[i->stackPointer];
#ifdef PRINT_DEBUG_STACK_POINTER
    printf("popping from stack, new stack pointer: %ld\n", i->stackPointer);
    /*print_detail(oh, o);*/
#endif
    return o;
  }

}

void interpreter_stack_pop_amount(struct object_heap* oh, struct Interpreter* i, word_t amount) {

  if (i -> stackPointer - amount < 0) {
    /*error("Attempted to pop empty interpreter stack.");*/
    assert(0);
  }

  i->stackPointer = i->stackPointer - amount;

}


void unhandled_signal(struct object_heap* oh, struct Symbol* selector, word_t n, struct Object* args[]) {
  word_t i;
  printf("Unhandled signal: "); print_symbol(selector); printf(" with %ld arguments: \n", n);
  for (i = 0; i<n; i++) {
    printf("arg[%ld] = ", i);
    print_detail(oh, args[i]);
  }
  printf("partial stack: \n");
  /*print_stack_types(oh, 200);*/
  print_backtrace(oh);
  assert(0);
  exit(1);

}

word_t hash_selector(struct object_heap* oh, struct Symbol* name, struct Object* arguments[], word_t n) {
  word_t i;
  word_t hash = (word_t) name;
  for (i = 0; i < n; i++) {
    hash += object_hash((struct Object*)object_get_map(oh, arguments[i]));
  }
  return hash;
}

void method_save_cache(struct object_heap* oh, struct MethodDefinition* md, struct Symbol* name, struct Object* arguments[], word_t n) {
  struct MethodCacheEntry* cacheEntry;
  word_t i;
  word_t hash = hash_selector(oh, name, arguments, n);
  assert(n <= METHOD_CACHE_ARITY);
  cacheEntry = &oh->methodCache[hash & (METHOD_CACHE_SIZE-1)];
  cacheEntry->method = md;
  cacheEntry->selector = name;
  for (i = 0; i < n; i++) {
    cacheEntry->maps[i] = object_get_map(oh, arguments[i]);
  }
}


struct MethodDefinition* method_check_cache(struct object_heap* oh, struct Symbol* selector, struct Object* arguments[], word_t n) {
  struct MethodCacheEntry* cacheEntry;
  word_t hash = hash_selector(oh, selector, arguments, n);
  assert(n <= METHOD_CACHE_ARITY);
  oh->method_cache_access++;
  cacheEntry = &oh->methodCache[hash & (METHOD_CACHE_SIZE-1)];
  if (cacheEntry->selector != selector) return NULL;

  /*fix findOn: might not call us with enough elements*/
  if (object_to_smallint(selector->cacheMask) > (1 << n)) return NULL;

  switch (object_to_smallint(selector->cacheMask)) { /*the found positions for the function*/
  case 1: if (object_get_map(oh, arguments[0]) != cacheEntry->maps[0]) return NULL; break;
  case 2: if (object_get_map(oh, arguments[1]) != cacheEntry->maps[1]) return NULL; break;
  case 4: if (object_get_map(oh, arguments[2]) != cacheEntry->maps[2]) return NULL; break;

  case 3: if (object_get_map(oh, arguments[0]) != cacheEntry->maps[0] ||
              object_get_map(oh, arguments[1]) != cacheEntry->maps[1]) return NULL; break;
  case 5: if (object_get_map(oh, arguments[0]) != cacheEntry->maps[0] ||
              object_get_map(oh, arguments[2]) != cacheEntry->maps[2]) return NULL; break;
  case 6: if (object_get_map(oh, arguments[1]) != cacheEntry->maps[1] ||
              object_get_map(oh, arguments[2]) != cacheEntry->maps[2]) return NULL; break;

  case 7: if (object_get_map(oh, arguments[0]) != cacheEntry->maps[0] ||
              object_get_map(oh, arguments[1]) != cacheEntry->maps[1] ||
              object_get_map(oh, arguments[2]) != cacheEntry->maps[2]) return NULL; break;
  }
  oh->method_cache_hit++;
  return cacheEntry->method;

}


struct RoleEntry* role_table_entry_for_name(struct object_heap* oh, struct RoleTable* roles, struct Symbol* name) {

  word_t tableSize, hash, index;
  struct RoleEntry* role;

  tableSize = role_table_capacity(roles);
  if (tableSize == 0) return NULL;
  hash = object_hash((struct Object*)name) & (tableSize-1); /*fix base2 assumption*/
  
  
  for (index = hash; index < tableSize; index++) {

    role = &roles->roles[index];
    if (role->name == name) return role;
    if (role->name == (struct Symbol*)oh->cached.nil) return NULL;
  }
  for (index = 0; index < hash; index++) {
    role = &roles->roles[index];
    if (role->name == name) return role;
    if (role->name == (struct Symbol*)oh->cached.nil) return NULL;
  }

  return NULL;
}


struct RoleEntry* role_table_entry_for_inserting_name(struct object_heap* oh, struct RoleTable* roles, struct Symbol* name) {

  word_t tableSize, hash, index;
  struct RoleEntry* role;

  tableSize = role_table_capacity(roles);
  if (tableSize == 0) return NULL;
  hash = object_hash((struct Object*)name) & (tableSize-1); /*fix base2 assumption*/
  
  
  for (index = hash; index < tableSize; index++) {

    role = &roles->roles[index];
    if (role->name == (struct Symbol*)oh->cached.nil) return role;
  }
  for (index = 0; index < hash; index++) {
    role = &roles->roles[index];
    if (role->name == (struct Symbol*)oh->cached.nil) return role;
  }

  return NULL;
}


struct RoleEntry* role_table_insert(struct object_heap* oh, struct RoleTable* roles, struct Symbol* name) {
  struct RoleEntry* chain = role_table_entry_for_name(oh, roles, name);
  struct RoleEntry* role = role_table_entry_for_inserting_name(oh, roles, name);
  heap_store_into(oh, (struct Object*)roles, (struct Object*)name);
  if (chain != NULL) {
    while (chain->nextRole != oh->cached.nil) {
      chain = & roles->roles[object_to_smallint(chain->nextRole)];
    }
    chain->nextRole = smallint_to_object(role - roles->roles);
  }

  return role;
}


struct SlotEntry* slot_table_entry_for_name(struct object_heap* oh, struct SlotTable* slots, struct Symbol* name) {

  word_t tableSize, hash, index;
  struct SlotEntry* slot;

  tableSize = slot_table_capacity(slots);
  if (tableSize == 0) return NULL;
  hash = object_hash((struct Object*)name) & (tableSize-1); /*fix base2 assumption*/
  
  
  for (index = hash; index < tableSize; index++) {

    slot = &slots->slots[index];
    if (slot->name == name) return slot;
    if (slot->name == (struct Symbol*)oh->cached.nil) return NULL;
  }
  for (index = 0; index < hash; index++) {
    slot = &slots->slots[index];
    if (slot->name == name) return slot;
    if (slot->name == (struct Symbol*)oh->cached.nil) return NULL;
  }

  return NULL;
}


struct SlotEntry* slot_table_entry_for_inserting_name(struct object_heap* oh, struct SlotTable* slots, struct Symbol* name) {

  word_t tableSize, hash, index;
  struct SlotEntry* slot;

  tableSize = slot_table_capacity(slots);
  if (tableSize == 0) return NULL;
  hash = object_hash((struct Object*)name) & (tableSize-1); /*fix base2 assumption*/
  
  
  for (index = hash; index < tableSize; index++) {
    slot = &slots->slots[index];
    if (slot->name == (struct Symbol*)oh->cached.nil) return slot;
  }
  for (index = 0; index < hash; index++) {
    slot = &slots->slots[index];
    if (slot->name == (struct Symbol*)oh->cached.nil) return slot;
  }

  return NULL;
}



word_t role_table_accommodate(struct RoleTable* roles, word_t n) {

  word_t tableSize, requested;

  tableSize = role_table_capacity(roles);
  assert(tableSize + n >= 0);
  requested = tableSize + n;
  while (tableSize > requested)
    tableSize >>= 1;
  if (tableSize ==0 && requested > 1)
    tableSize = 1;
  while (tableSize < requested)
    tableSize <<= 1;

  return tableSize;
}

word_t slot_table_accommodate(struct SlotTable* roles, word_t n) {

  word_t tableSize, requested;

  tableSize = slot_table_capacity(roles);
  assert(tableSize + n >= 0);
  requested = tableSize + n;
  while (tableSize > requested)
    tableSize >>= 1;
  if (tableSize ==0 && requested > 1)
    tableSize = 1;
  while (tableSize < requested)
    tableSize <<= 1;

  return tableSize;
}


word_t role_table_empty_space(struct object_heap* oh, struct RoleTable* roles) {

  word_t space = 0, i;

  for (i = 0; i < role_table_capacity(roles); i++) {
    if (roles->roles[i].name == (struct Symbol*)oh->cached.nil) space++;
  }

  return space;
}

word_t slot_table_empty_space(struct object_heap* oh, struct SlotTable* slots) {

  word_t space = 0, i;

  for (i = 0; i < slot_table_capacity(slots); i++) {
    if (slots->slots[i].name == (struct Symbol*)oh->cached.nil) space++;
  }

  return space;
}


struct RoleTable* role_table_grow_excluding(struct object_heap* oh, struct RoleTable* roles, word_t n, struct MethodDefinition* method) {
  word_t oldSize, newSize, i;
  struct RoleTable* newRoles;

  oldSize = role_table_capacity(roles);
  newSize = role_table_accommodate(roles, (oldSize / 3 - role_table_empty_space(oh, roles)) + n);
  newRoles = (struct RoleTable*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), newSize * ROLE_ENTRY_WORD_SIZE);

  for (i = 0; i < oldSize; i++) {
    struct Symbol* roleName = (struct Symbol*)roles->roles[i].name;
    if (roleName != (struct Symbol*)oh->cached.nil && roles->roles[i].methodDefinition != method) {
      struct RoleEntry * role = role_table_insert(oh, newRoles, roleName);
      *role = roles->roles[i];
      role->nextRole = oh->cached.nil;
    }
    
  }

  return newRoles;

}


struct SlotTable* slot_table_grow_excluding(struct object_heap* oh, struct SlotTable* slots, word_t n, struct Symbol* excluding) {
  word_t oldSize, newSize, i;
  struct SlotTable* newSlots;

  oldSize = slot_table_capacity(slots);
  newSize = slot_table_accommodate(slots, (oldSize / 3 - slot_table_empty_space(oh, slots)) + n);
  newSlots = (struct SlotTable*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), newSize * SLOT_ENTRY_WORD_SIZE);

  for (i = 0; i < oldSize; i++) {
    struct Symbol* slotName = (struct Symbol*)slots->slots[i].name;
    if (slotName != (struct Symbol*)oh->cached.nil && slotName != excluding) {
      struct SlotEntry * slot = slot_table_entry_for_inserting_name(oh, newSlots, slotName);
      *slot = slots->slots[i];
    }
    
  }

  return newSlots;

}

void slot_table_relocate_by(struct object_heap* oh, struct SlotTable* slots, word_t offset, word_t amount) {

  word_t i;

  for (i = 0; i < slot_table_capacity(slots); i++) {
    if (slots->slots[i].name != (struct Symbol*)oh->cached.nil
        && object_to_smallint(slots->slots[i].offset) >= offset) {
      slots->slots[i].offset = smallint_to_object(object_to_smallint(slots->slots[i].offset) + amount);
    }
  }
}




struct MethodDefinition* object_has_role_named_at(struct Object* obj, struct Symbol* selector, word_t position, struct Object* method) {

  word_t i;

  for (i = 0; i < role_table_capacity(obj->map->roleTable); i++) {
    struct RoleEntry* role = &obj->map->roleTable->roles[i];
    if (role->name == selector
        && (object_to_smallint(role->rolePositions) & position) == position
        && role->methodDefinition->method == method) {
      return role->methodDefinition;
    }

  }

  return NULL;

}

void object_change_map(struct object_heap* oh, struct Object* obj, struct Map* map) {
  if (obj->map->representative == obj) obj->map->representative = oh->cached.nil;
  heap_store_into(oh, obj, (struct Object*)map);
  obj->map = map;
}

void object_represent(struct object_heap* oh, struct Object* obj, struct Map* map) {
  object_change_map(oh, obj, map);
  heap_store_into(oh, (struct Object*)map, obj);
  map->representative = obj;
}

word_t object_add_role_at(struct object_heap* oh, struct Object* obj, struct Symbol* selector, word_t position, struct MethodDefinition* method) {

  struct Map* map;
  struct RoleEntry *entry, *chain;

  map = heap_clone_map(oh, obj->map);
  chain = role_table_entry_for_name(oh, obj->map->roleTable, selector);
  object_represent(oh, obj, map);
  
  while (chain != NULL) {

    if (chain->methodDefinition == method) {

      /*fix: do we want to copy the roletable*/
      map->roleTable = role_table_grow_excluding(oh, map->roleTable, 0, NULL);
      /* roleTable is in young memory now so we don't have to store_into*/
      entry = role_table_entry_for_name(oh, map->roleTable, selector);
      while (entry != NULL) {
        if (entry->methodDefinition == method) {
          entry->rolePositions = smallint_to_object(object_to_smallint(entry->rolePositions) | position);
          return FALSE;
        }
        if (entry->nextRole == oh->cached.nil) {
          entry = NULL;
        } else {
          entry = &map->roleTable->roles[object_to_smallint(entry->nextRole)];
        }
      }
      
    }

    if (chain->nextRole == oh->cached.nil) {
      chain = NULL;
    } else {
      chain = &map->roleTable->roles[object_to_smallint(chain->nextRole)];
    }
  }
  
  /*not found, adding role*/
  map->roleTable = role_table_grow_excluding(oh, map->roleTable, 1, NULL);

  entry = role_table_insert(oh, map->roleTable, selector);
  entry->name = selector;
  entry->nextRole = oh->cached.nil;
  entry->rolePositions = smallint_to_object(position);
  entry->methodDefinition = method;
  return TRUE;

}

word_t object_remove_role(struct object_heap* oh, struct Object* obj, struct Symbol* selector, struct MethodDefinition* method) {

  struct Map* map;
  struct RoleTable* roles = obj->map->roleTable;
  word_t i, matches = 0;

  for (i = 0; i< role_table_capacity(roles); i++) {

    if (roles->roles[i].methodDefinition == method) {
      matches++;
    }

  }

  if (matches == 0) return FALSE;
  map = heap_clone_map(oh, obj->map);
  heap_fixed_add(oh, (struct Object*)map);
  map->roleTable = role_table_grow_excluding(oh, roles, matches, method);
  heap_store_into(oh, (struct Object*)map, (struct Object*)map->roleTable);
  object_represent(oh, obj, map);
  heap_fixed_remove(oh, (struct Object*)map);
  return TRUE;

}





struct Object* object_add_slot_named_at(struct object_heap* oh, struct Object* obj, struct Symbol* name, struct Object* value, word_t offset) {

  struct Map *map;
  struct Object* newObj;
  struct SlotEntry *entry;

  entry = slot_table_entry_for_name(oh, obj->map->slotTable, name);
  if (entry != NULL) return NULL;
  map = heap_clone_map(oh, obj->map);
  heap_fixed_add(oh, (struct Object*)map);
  map->slotTable = slot_table_grow_excluding(oh, map->slotTable, 1, (struct Symbol*)oh->cached.nil);
  slot_table_relocate_by(oh, map->slotTable, offset, sizeof(word_t));
  entry = slot_table_entry_for_inserting_name(oh, map->slotTable, name);
  entry->name = name;
  entry->offset = smallint_to_object(offset);

  if (object_type(obj) == TYPE_OBJECT) {
    newObj = heap_allocate(oh, object_size(obj) + 1);
  } else {
    newObj = heap_allocate_with_payload(oh, object_size(obj) + 1, payload_size(obj));
  }
  object_set_format(newObj, object_type(obj));
  


  heap_fixed_remove(oh, (struct Object*) map);
  object_set_idhash(newObj, heap_new_hash(oh));
  copy_bytes_into((byte_t*)obj+ object_first_slot_offset(obj),
                  offset-object_first_slot_offset(obj),
                  (byte_t*)newObj + object_first_slot_offset(newObj));
  object_slot_value_at_offset_put(oh, newObj, offset, value);

  copy_bytes_into((byte_t*)obj+offset, object_total_size(obj) - offset, (byte_t*)newObj + offset + sizeof(word_t));
  newObj->map = map;
  map->representative = newObj;
  heap_store_into(oh, (struct Object*)map, (struct Object*)map->representative);

  return newObj;
}


struct Object* object_add_slot_named(struct object_heap* oh, struct Object* obj, struct Symbol* name, struct Object* value) {

  
  struct Object* newObj = 
    object_add_slot_named_at(oh, obj, name, value,
                             object_first_slot_offset(obj) + object_to_smallint(obj->map->slotCount) * sizeof(word_t));

  if (newObj == NULL) return obj;

  newObj->map->slotCount = smallint_to_object(object_to_smallint(newObj->map->slotCount) + 1);

  return newObj;
}

struct Object* object_remove_slot(struct object_heap* oh, struct Object* obj, struct Symbol* name) {
  word_t offset;
  struct Object* newObj;
  struct Map* map;
  struct SlotEntry* se = slot_table_entry_for_name(oh, obj->map->slotTable, name);
  if (se == NULL) return obj;

  offset = object_to_smallint(se->offset);
  map = heap_clone_map(oh, obj->map);
  map->slotCount = smallint_to_object(object_to_smallint(map->slotCount) - 1);
  heap_fixed_add(oh, (struct Object*)map);
  map->slotTable = slot_table_grow_excluding(oh, map->slotTable, -1, se->name);
  slot_table_relocate_by(oh, map->slotTable, offset, -sizeof(word_t));

  if (object_type(obj) == TYPE_OBJECT) {
    newObj = heap_allocate(oh, object_size(obj)-1);
    object_set_format(newObj, TYPE_OBJECT);
  } else {
    newObj = heap_allocate_with_payload(oh,  object_size(obj) -1 , payload_size(obj));
    object_set_format(newObj, object_type(obj));
  }

  heap_fixed_remove(oh, (struct Object*) map);
  /*fix we don't need to set the format again, right?*/
  object_set_idhash(newObj, heap_new_hash(oh));
  copy_bytes_into((byte_t*) obj + object_first_slot_offset(obj),
                  offset - object_first_slot_offset(obj), 
                  (byte_t*) newObj + object_first_slot_offset(newObj));

  copy_bytes_into((byte_t*) obj + object_first_slot_offset(obj) + sizeof(word_t), /*+one slot*/
                  object_total_size(obj) - offset - sizeof(word_t), 
                  (byte_t*) newObj + offset);
  newObj->map = map;
  map->representative = newObj;
  heap_store_into(oh, (struct Object*)map, map->representative);
  return newObj;
}


/*
 * This is the main dispatch function
 *
 */
struct MethodDefinition* method_dispatch_on(struct object_heap* oh, struct Symbol* name,
                                            struct Object* arguments[], word_t arity, struct Object* resendMethod) {

  struct MethodDefinition *dispatch, *bestDef;
  struct Object* slotLocation;
  word_t bestRank, depth, delegationCount, resendRank, restricted, i;


#ifdef PRINT_DEBUG_DISPATCH
  printf("dispatch to: '");
  print_symbol(name);
  printf("' (arity: %ld)\n", arity);
  for (i = 0; i < arity; i++) {
    printf("arguments[%ld] (%p) = ", i, (void*)arguments[i]); print_type(oh, arguments[i]);
  }
  /*  printf("resend: "); print_object(resendMethod);*/
#endif

  dispatch = NULL;
  slotLocation = NULL;

  if (resendMethod == NULL && arity <= METHOD_CACHE_ARITY) {
    dispatch = method_check_cache(oh, name, arguments, arity);
    if (dispatch != NULL) return dispatch;
  }

  oh->current_dispatch_id++;
  bestRank = 0;
  bestDef = NULL;
  resendRank = ((resendMethod == NULL) ? WORDT_MAX : 0);

  for (i = 0; i < arity; i++) {
    struct Object *obj;
    struct Map* map;
    struct Object* arg = arguments[i];
    delegationCount = 0;
    depth = 0;
    restricted = WORDT_MAX; /*pointer in delegate_stack (with sp of delegateCount) to where we don't trace further*/
    
    do {
      /* Set up obj to be a pointer to the object, or SmallInteger if it's
         a direct SmallInt. */
      if (object_is_smallint(arg)) {
        obj = get_special(oh, SPECIAL_OOP_SMALL_INT_PROTO);
      } else {
        obj = arg;
      }
      /* Identify the map, and update its dispatchID and reset the visited mask
         if it hasn't been visited during this call already. */
      map = obj->map;

      if (map->dispatchID != oh->current_dispatch_id) {
        map->dispatchID = oh->current_dispatch_id;
        map->visitedPositions = 0;
      }

      /* we haven't been here before */
      if ((map->visitedPositions & (1 << i)) == 0) {

        struct RoleEntry* role;

        /* If the map marks an obj-meta transition and the top of the stack
           is not the original argument, then mark the restriction point at
           the top of the delegation stack. */

        if (((word_t)map->flags & MAP_FLAG_RESTRICT_DELEGATION) && (arg != arguments[i])) {
          restricted = delegationCount;
        }
        map->visitedPositions |= (1 << i);
        role = role_table_entry_for_name(oh, map->roleTable, name);
        while (role != NULL) {
          if ((object_to_smallint(role->rolePositions) & (1 << i)) != 0) {
            struct MethodDefinition* def = role->methodDefinition;
            /* If the method hasn't been visited this time, mark it
               so and clear the other dispatch marks.*/
            if (def->dispatchID != oh->current_dispatch_id) {
              def->dispatchID = oh->current_dispatch_id;
              def->foundPositions = 0;
              def->dispatchRank = 0;
            }
            /*If the method hasn't been found at this position...*/
            if ((def->foundPositions & (1 << i)) == 0) {
              /*fix*/

              def->dispatchRank |= ((31 - depth) << ((5 - i) * 5));
              def->foundPositions |= (1 << i);

#ifdef PRINT_DEBUG_FOUND_ROLE
              printf("found role index %ld <%p> for '%s' foundPos: %lx dispatchPos: %lx\n",
                     i,
                     (void*) role,
                     ((struct Symbol*)(role->name))->elements, def->foundPositions, def->dispatchPositions);
#endif

              if (def->method == resendMethod) {
                struct RoleEntry* rescan = role_table_entry_for_name(oh, map->roleTable, name);
                resendRank = def->dispatchRank;
                while (rescan != role) {
                  struct MethodDefinition* redef = rescan->methodDefinition;
                  if (redef->foundPositions == redef->dispatchPositions &&
                      (dispatch == NULL || redef->dispatchRank <= resendRank)) {
                    dispatch = redef;
                    slotLocation = obj;
                    if (redef->dispatchRank > bestRank) {
                      bestRank = redef->dispatchRank;
                      bestDef = redef;
                    }
                  }
                  if (rescan->nextRole == oh->cached.nil) {
                    rescan = NULL;
                  } else {
                    rescan = &map->roleTable->roles[object_to_smallint(rescan->nextRole)];
                  }
                  
                }

              } else /*not a resend*/ {
                if (def->foundPositions == def->dispatchPositions &&
                    (dispatch == NULL || def->dispatchRank > dispatch->dispatchRank) &&
                    def->dispatchRank <= resendRank) {
                  dispatch = def;
                  slotLocation = obj;
                  if (def->dispatchRank > bestRank) {
                    bestRank = def->dispatchRank;
                    bestDef = def;
                  }
                }
              }
              if (def->dispatchRank >= bestRank && def != bestDef) {
                bestRank = def->dispatchRank;
                bestDef = NULL;
              }
            }
            
          }
          role = ((role->nextRole == oh->cached.nil) ? NULL : &map->roleTable->roles[object_to_smallint(role->nextRole)]);
        } /*while role != NULL*/

        if (depth > 31) { /*fix wordsize*/
          assert(0);
        }
        if (dispatch != NULL && bestDef == dispatch) {
          if (dispatch->slotAccessor != oh->cached.nil) {
            arguments[0] = slotLocation;
            /*do we need to try to do a heap_store_into?*/
#ifdef PRINT_DEBUG_DISPATCH_SLOT_CHANGES
            printf("arguments[0] changed to slot location: \n");
            print_detail(oh, arguments[0]);
#endif
          }
          if (resendMethod == 0 && arity <= METHOD_CACHE_ARITY) method_save_cache(oh, dispatch, name, arguments, arity);
          return dispatch;
        }
        
        depth++;


        /* We add the delegates to the list when we didn't just finish checking a restricted object*/
        if (delegationCount <= restricted && array_size(map->delegates) > 0) {
          struct OopArray* delegates = map->delegates;
          word_t offset = object_array_offset((struct Object*)delegates);
          word_t limit = object_total_size((struct Object*)delegates);
          for (; offset != limit; offset += sizeof(word_t)) {
            struct Object* delegate = object_slot_value_at_offset((struct Object*)delegates, offset);
            if (delegate != oh->cached.nil) {
              oh->delegation_stack[delegationCount++] = delegate;
            }
          }
        }
        
      } /*end haven't been here before*/

      

      delegationCount--;
      if (delegationCount < restricted) restricted = WORDT_MAX; /*everything is unrestricted now*/

      if (delegationCount < 0 || delegationCount >= DELEGATION_STACK_SIZE) break;

      arg = oh->delegation_stack[delegationCount];


    } while (1);

  }


  if (dispatch != NULL && dispatch->slotAccessor != oh->cached.nil) {
    /*check heap store into?*/
    arguments[0] = slotLocation;
#ifdef PRINT_DEBUG_DISPATCH_SLOT_CHANGES
            printf("arguments[0] changed to slot location: \n");
            print_detail(oh, arguments[0]);
#endif

  }
  if (dispatch != NULL && resendMethod == 0 && arity < METHOD_CACHE_ARITY) {
    method_save_cache(oh, dispatch, name, arguments, arity);
  }

  return dispatch;
}

/********************** EXTLIB ****************************/

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

struct Object* applyExternalLibraryPrimitive(struct object_heap* oh,
   struct ByteArray * fnHandle, 
   struct OopArray * argsFormat, 
   struct Object* callFormat,
   struct Object* resultFormat, 
   struct OopArray * argsArr)
{
  ext_fn0_t fn;
  word_t args [MAX_ARG_COUNT];
  word_t result;
  word_t arg, argCount, outArgIndex = 0, outArgCount;

  assert (payload_size((struct Object *) fnHandle) >= sizeof (fn));
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
      if (element == oh->cached.true) {
        args[outArgIndex++] = TRUE;
      } else if (element == oh->cached.false) {
        args[outArgIndex++] = FALSE;
      } else {
        /*fix assert(0)?*/
        args[outArgIndex++] = -1;
      }
      break;
    case ARG_FORMAT_FLOAT:
      if (object_is_smallint(element)) {
        union {
          float f;
          word_t u;
        } convert;
        convert.f = (float) object_to_smallint(element);
        args[outArgIndex++] = convert.u;
      } else
        args[outArgIndex++] = * (word_t *) byte_array_elements((struct ByteArray*)element);
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
	  convert.d = (double) * (float *) byte_array_elements((struct ByteArray*)element);
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
        //make sure we have enough space
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

  switch ((word_t)resultFormat) { /*preconverted smallint*/
  case ARG_FORMAT_INT:
    if (smallint_fits_object(result))
      return smallint_to_object(result);
    else
      return injectBigInteger(oh, (word_t)result);
  case ARG_FORMAT_BOOLEAN:
    if (result)
      return oh->cached.true;
    else
      return oh->cached.false;
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


/********************** OPTIMIZER ****************************/



void method_add_optimized(struct object_heap* oh, struct CompiledMethod* method) {

  if (oh->optimizedMethodsSize + 1 >= oh->optimizedMethodsLimit) {
    oh->optimizedMethodsLimit *= 2;
    oh->optimizedMethods = realloc(oh->optimizedMethods, oh->optimizedMethodsLimit * sizeof(struct CompiledMethod*));
    assert(oh->optimizedMethods != NULL);

  }
  oh->optimizedMethods[oh->optimizedMethodsSize++] = method;

}

void method_unoptimize(struct object_heap* oh, struct CompiledMethod* method) {
#ifdef PRINT_DEBUG_OPTIMIZER
  printf("Unoptimizing '"); print_symbol(method->selector); printf("'\n");
#endif
  method->code = method->oldCode;
  method->isInlined = oh->cached.false;
  method->calleeCount = (struct OopArray*)oh->cached.nil;
  method->callCount = smallint_to_object(0);

}


void method_remove_optimized_sending(struct object_heap* oh, struct Symbol* symbol) {
  word_t i, j;
  for (i = 0; i < oh->optimizedMethodsSize; i++) {
    struct CompiledMethod* method = oh->optimizedMethods[i];
    /*resend?*/
    if (method->selector == symbol) {
      method_unoptimize(oh, method);
      continue;
    }
    for (j = 0; j < array_size(method->selectors); j++) {
      if (array_elements(method->selectors)[j] == (struct Object*)symbol) {
        method_unoptimize(oh, method);
        break;
      }
    }
  }

}


void method_optimize(struct object_heap* oh, struct CompiledMethod* method) {
#ifdef PRINT_DEBUG_OPTIMIZER
  printf("Optimizing '"); print_symbol(method->selector); printf("'\n");
#endif

  /* only optimize old objects because they don't move in memory and
   * we don't want to have to update our method cache every gc */
  if (object_is_young(oh, (struct Object*)method)) return;
#ifdef PRINT_DEBUG_OPTIMIZER
  method_print_debug_info(oh, method);
  printf("This method is called by:\n");
  method_print_debug_info(oh, oh->cached.interpreter->method);
#endif

  method->oldCode = method->code;
  method->isInlined = oh->cached.true;
  method_add_optimized(oh, method);
  
}

void method_pic_setup(struct object_heap* oh, struct CompiledMethod* caller) {

  caller->calleeCount = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), CALLER_PIC_SIZE*CALLER_PIC_ENTRY_SIZE);

}

struct MethodDefinition* method_pic_match_selector(struct object_heap* oh, struct Object* picEntry[],
                                                 struct Symbol* selector,
                        word_t arity, struct Object* args[], word_t incrementWhenFound) {
  struct Closure* closure = (struct Closure*) ((struct MethodDefinition*)picEntry[PIC_CALLEE])->method;
  struct Symbol* methodSelector;
  /*primitive methods store their selector in a different place*/
  if (closure->base.map->delegates->elements[0] == oh->cached.primitive_method_window) {
    methodSelector = (struct Symbol*)((struct PrimitiveMethod*)closure)->selector;
  } else {
    methodSelector = closure->method->selector;
  }

  if (methodSelector == selector 
      && object_to_smallint(picEntry[PIC_CALLEE_ARITY]) == arity) {
    /*callee method and arity work out, check the maps*/
    word_t j, success = 1;
    for (j = 0; j < arity; j++) {
      if ((struct Map*)((struct OopArray*)picEntry[PIC_CALLEE_MAPS])->elements[j] != object_get_map(oh, args[j])) {
        success = 0;
        break;
      }
    }
    if (success) {
      if (incrementWhenFound) {
        picEntry[PIC_CALLEE_COUNT] = smallint_to_object(object_to_smallint(picEntry[PIC_CALLEE_COUNT]) + 1);
      }
      return (struct MethodDefinition*)picEntry[PIC_CALLEE];
    }
  }
  return NULL;
}


void method_pic_insert(struct object_heap* oh, struct Object* picEntry[], struct MethodDefinition* def,
                         word_t arity, struct Object* args[]) {

  word_t j;
  picEntry[PIC_CALLEE] = (struct Object*)def;
  picEntry[PIC_CALLEE_ARITY] = smallint_to_object(arity);
  picEntry[PIC_CALLEE_COUNT] = smallint_to_object(1);
  picEntry[PIC_CALLEE_MAPS] = 
    (struct Object*)heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), arity);
  
  for (j = 0; j < arity; j++) {
    ((struct OopArray*)picEntry[PIC_CALLEE_MAPS])->elements[j] = (struct Object*)object_get_map(oh, args[j]);
  }

}

void method_pic_add_callee(struct object_heap* oh, struct CompiledMethod* callerMethod, struct MethodDefinition* def,
                           word_t arity, struct Object* args[]) {
  word_t i;
  word_t arraySize = array_size(callerMethod->calleeCount);
  word_t entryStart = (hash_selector(oh, NULL, args, arity) % (arraySize/CALLER_PIC_ENTRY_SIZE)) * CALLER_PIC_ENTRY_SIZE;
  for (i = entryStart; i < arraySize; i+= CALLER_PIC_ENTRY_SIZE) {
    /* if it's nil, we need to insert it*/
    if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) {
      method_pic_insert(oh, &callerMethod->calleeCount->elements[i], def, arity, args);
      return;
    }
  }
  for (i = 0; i < entryStart; i+= CALLER_PIC_ENTRY_SIZE) {
    /*MUST be same as first loop*/
    if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) {
      method_pic_insert(oh, &callerMethod->calleeCount->elements[i], def, arity, args);
      return;
    }
  }
}

struct MethodDefinition* method_pic_find_callee(struct object_heap* oh, struct CompiledMethod* callerMethod,
                                              struct Symbol* selector, word_t arity, struct Object* args[]) {
  word_t i;
  word_t arraySize = array_size(callerMethod->calleeCount);
  word_t entryStart = (hash_selector(oh, NULL, args, arity) % (arraySize/CALLER_PIC_ENTRY_SIZE)) * CALLER_PIC_ENTRY_SIZE;
  struct MethodDefinition* retval;
  for (i = entryStart; i < arraySize; i+= CALLER_PIC_ENTRY_SIZE) {
   if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) return NULL;
   if ((retval = method_pic_match_selector(oh, &callerMethod->calleeCount->elements[i], selector, arity, args, TRUE))) return retval;
  }
  for (i = 0; i < entryStart; i+= CALLER_PIC_ENTRY_SIZE) {
    /*MUST be same as first loop*/
   if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) return NULL;
   if ((retval = method_pic_match_selector(oh, &callerMethod->calleeCount->elements[i], selector, arity, args, TRUE))) return retval;

  }
  return NULL;
}

/********************** SIGNAL ****************************/

void interpreter_apply_to_arity_with_optionals(struct object_heap* oh, struct Interpreter * i, struct Closure * closure,
                                               struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);


void interpreter_signal(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  struct Closure* method;
  struct Symbol* selector = (struct Symbol*)signal;
  struct MethodDefinition* def = method_dispatch_on(oh, selector, args, n, NULL);
  if (def == NULL) {
    unhandled_signal(oh, selector, n, args);
  }
  /*take this out when the debugger is mature*/
  /*assert(0);*/
  method = (struct Closure*)def->method;
  interpreter_apply_to_arity_with_optionals(oh, i, method, args, n, opts, resultStackPointer);
}

void interpreter_signal_with(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* arg, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* args[1];
  args[0] = arg;
  interpreter_signal(oh, i, signal, args, 1, opts, resultStackPointer);
}


void interpreter_signal_with_with(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* arg, struct Object* arg2, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* args[2];
  args[0] = arg;
  args[1] = arg2;
  interpreter_signal(oh, i, signal, args, 2, opts, resultStackPointer);
}

void interpreter_signal_with_with_with(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* arg, struct Object* arg2, struct Object* arg3, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* args[3];
  args[0] = arg;
  args[1] = arg2;
  args[2] = arg3;
  interpreter_signal(oh, i, signal, args, 3, opts, resultStackPointer);
}


void prim_fixme(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  printf("unimplemented primitive... dying\n");
  assert(0);

}


struct MethodDefinition* method_is_on_arity(struct object_heap* oh, struct Object* method, struct Symbol* selector, struct Object* args[], word_t n) {

  word_t positions, i;
  struct MethodDefinition* def;

  positions = 0;
  def = NULL;

  for (i = 0; i < n; i++) {
    if (!object_is_smallint(args[i])) {

      struct MethodDefinition* roleDef = object_has_role_named_at(args[i], selector, 1<<i, method);
      if (roleDef != NULL) {
        positions |= 1<<i;
        def = roleDef;
      }

    }
  }

  return ((def != NULL && positions == def->dispatchPositions)? def : NULL);

}


struct MethodDefinition* method_define(struct object_heap* oh, struct Object* method, struct Symbol* selector, struct Object* args[], word_t n) {

  struct Object* argBuffer[16];
  word_t positions, i;
  struct MethodDefinition *def, *oldDef;

  def = (struct MethodDefinition*)heap_clone_special(oh, SPECIAL_OOP_METHOD_DEF_PROTO);
  positions = 0;
  for (i = 0; i < n; i++) {
    if (!object_is_smallint(args[i]) && args[i] != get_special(oh, SPECIAL_OOP_NO_ROLE)) {
      positions |= (1 << i);
    }
  }

  /* any methods that call the same symbol must be decompiled because they might call an old version */
  method_remove_optimized_sending(oh, selector);
  selector->cacheMask = smallint_to_object(object_to_smallint(selector->cacheMask) | positions);
  assert(n<=16);
  copy_words_into((word_t*)args, n, (word_t*)argBuffer); /*for pinning i presume */
  oldDef = method_dispatch_on(oh, selector, argBuffer, n, NULL);
  if (oldDef == NULL || oldDef->dispatchPositions != positions || oldDef != method_is_on_arity(oh, oldDef->method, selector, args, n)) {
    oldDef = NULL;
  }
  heap_fixed_add(oh, (struct Object*)def);
  def->method = method;
  def->dispatchPositions = positions;
  for (i = 0; i < n; i++) {
    if (!object_is_smallint(args[i]) && args[i] != get_special(oh, SPECIAL_OOP_NO_ROLE)) {
      if (oldDef != NULL) {
        object_remove_role(oh, args[i], selector, oldDef);
      }
      object_add_role_at(oh, args[i], selector, 1<<i, def);
    }
  }
  heap_fixed_remove(oh, (struct Object*)def);
  return def;
    
}



bool interpreter_dispatch_optional_keyword(struct object_heap* oh, struct Interpreter * i, struct Object* key, struct Object* value) {

  struct OopArray* optKeys = i->method->optionalKeywords;

  word_t optKey, size = array_size(optKeys);

  for (optKey = 0; optKey < size; optKey++) {
    if (optKeys->elements[optKey] == key) {
      if (i->method->heapAllocate == oh->cached.true) {
        heap_store_into(oh, (struct Object*)i->lexicalContext, value);
        i->lexicalContext->variables[object_to_smallint(i->method->inputVariables) + optKey] = value;
      } else {
        heap_store_into(oh, (struct Object*)i->stack, value);
        i->stack->elements[i->framePointer + object_to_smallint(i->method->inputVariables) + optKey] = value;
      }
      return TRUE;
    }
  }

  /*fix*/
  return FALSE;
}

void interpreter_dispatch_optionals(struct object_heap* oh, struct Interpreter * i, struct OopArray* opts) {

  word_t index, size = array_size(opts);
  for (index = 0; index < size; index += 2) {
    interpreter_dispatch_optional_keyword(oh, i, opts->elements[index], opts->elements[index+1]);
  }

}


void interpreter_apply_to_arity_with_optionals(struct object_heap* oh, struct Interpreter * i, struct Closure * closure,
                                               struct Object* argsNotStack[], word_t n, struct OopArray* opts,
                                               word_t resultStackPointer) {


  word_t inputs, framePointer, j;
  struct Object** vars;
  struct LexicalContext* lexicalContext;
  struct CompiledMethod* method;
  struct Object* args[16];
  

#ifdef PRINT_DEBUG
  printf("apply to arity %ld\n", n);
#endif

  method = closure->method;
  inputs = object_to_smallint(method->inputVariables);

  method->callCount = smallint_to_object(object_to_smallint(method->callCount) + 1);


  /*make sure they are pinned*/
  assert(n <= 16);
  copy_words_into((word_t*)argsNotStack, n, (word_t*)args);


  /* optimize the callee function after a set number of calls*/
  if (method->callCount > (struct Object*)CALLEE_OPTIMIZE_AFTER && method->isInlined == oh->cached.false) {
    method_optimize(oh, method);
  }

  
  if (n < inputs || (n > inputs && method->restVariable != oh->cached.true)) {
    struct OopArray* argsArray = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
    copy_words_into((word_t*) args, n, (word_t*)argsArray->elements);
    interpreter_signal_with_with(oh, i, get_special(oh, SPECIAL_OOP_WRONG_INPUTS_TO), (struct Object*) argsArray, (struct Object*)method, NULL, resultStackPointer);
    return;
  }

  framePointer = i->stackPointer + FUNCTION_FRAME_SIZE;

  if (method->heapAllocate == oh->cached.true) {
    lexicalContext = (struct LexicalContext*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_LEXICAL_CONTEXT_PROTO), object_to_smallint(method->localVariables));
    lexicalContext->framePointer = smallint_to_object(framePointer);
    interpreter_stack_allocate(oh, i, FUNCTION_FRAME_SIZE);
    vars = &lexicalContext->variables[0];
    for (j = 0; j < inputs; j++) {
      heap_store_into(oh, (struct Object*)lexicalContext, args[j]);
    }

  } else {
    lexicalContext = (struct LexicalContext*) oh->cached.nil;
    interpreter_stack_allocate(oh, i, FUNCTION_FRAME_SIZE /*frame size in words*/ + object_to_smallint(method->localVariables));
    vars = &i->stack->elements[framePointer];
    for (j = 0; j < inputs; j++) {
      heap_store_into(oh, (struct Object*)i->stack, args[j]);
    }
  }


  copy_words_into((word_t*) args, inputs, (word_t*) vars);
  i->stack->elements[framePointer - 5] = smallint_to_object(resultStackPointer);
  i->stack->elements[framePointer - 4] = smallint_to_object(i->codePointer);
  i->stack->elements[framePointer - 3] = (struct Object*) closure;
  i->stack->elements[framePointer - 2] = (struct Object*) lexicalContext;
  i->stack->elements[framePointer - 1] = smallint_to_object(i->framePointer);

  heap_store_into(oh, (struct Object*)i->stack, (struct Object*)closure);
  heap_store_into(oh, (struct Object*)i->stack, (struct Object*)lexicalContext);

#ifdef PRINT_DEBUG_FUNCALL_VERBOSE
  printf("i->stack->elements[%ld(fp-4)] = \n", framePointer - 4);
  print_detail(oh, i->stack->elements[framePointer - 4]);
  printf("i->stack->elements[%ld(fp-3)] = \n", framePointer - 3);
  print_detail(oh, i->stack->elements[framePointer - 3]);
  printf("i->stack->elements[%ld(fp-2)] = \n", framePointer - 2);
  print_detail(oh, i->stack->elements[framePointer - 2]);
  printf("i->stack->elements[%ld(fp-1)] = \n", framePointer - 1);
  print_detail(oh, i->stack->elements[framePointer - 1]);
#endif


  i->framePointer = framePointer;
  i->method = method;
  i->closure = closure;
  i->lexicalContext = lexicalContext;

  heap_store_into(oh, (struct Object*)i, (struct Object*)lexicalContext);
  heap_store_into(oh, (struct Object*)i, (struct Object*)method);
  heap_store_into(oh, (struct Object*)i, (struct Object*)closure);

  i->codeSize = array_size(method->code);
  i->codePointer = 0;
  fill_words_with(((word_t*)vars)+inputs, object_to_smallint(method->localVariables) - inputs, (word_t)oh->cached.nil);

  if (n > inputs) {
    struct OopArray* restArgs = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n - inputs);
    copy_words_into(((word_t*)args)+inputs, n - inputs, (word_t*)restArgs->elements);
    vars[inputs+array_size(method->optionalKeywords)] = (struct Object*) restArgs;
    heap_store_into(oh, (struct Object*)lexicalContext, (struct Object*)restArgs);/*fix, not always right*/
  } else {
    if (method->restVariable == oh->cached.true) {
      vars[inputs+array_size(method->optionalKeywords)] = get_special(oh, SPECIAL_OOP_ARRAY_PROTO);
    }
  }
  if (opts != NULL) {
    interpreter_dispatch_optionals(oh, i, opts);
  }

}



void prim_write_to_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *console=args[0], *n=args[1], *handle=args[2], *seq=args[3], *start=args[4];
  byte_t* bytes = &((struct ByteArray*)seq)->elements[0] + object_to_smallint(start);
  word_t size = object_to_smallint(n);

  assert(arity == 5 && console != NULL);

  oh->cached.interpreter->stack->elements[resultStackPointer] =
                         smallint_to_object(fwrite(bytes, 1, size, (object_to_smallint(handle) == 0)? stdout : stderr));

}


void prim_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  closeFile(oh, handle);
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

void prim_readConsole_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t /*handle = object_to_smallint(args[2]),*/ n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;

  retval = fread((char*)(byte_array_elements(bytes) + start), 1, n, stdin);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);

}

void prim_read_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[2]), n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;

  retval = readFile(oh, handle, n, (char*)(byte_array_elements(bytes) + start));
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_write_to_from_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[2]), n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;
  retval = writeFile(oh, handle, n, (char*)(byte_array_elements(bytes) + start));
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}


void prim_reposition_to(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]), n = object_to_smallint(args[2]);
  word_t retval = seekFile(oh, handle, n);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_positionOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  word_t retval = tellFile(oh, handle);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_bytesPerWord(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(sizeof(word_t));
}


void prim_atEndOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  if (endOfFile(oh, handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false;
  }
}

void prim_sizeOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  word_t retval = sizeOfFile(oh, handle);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}



void prim_flush_output(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  /*struct Object *console=args[0];*/
  fflush(stdout);
  fflush(stderr);
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
}

void prim_handle_for(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle;
  struct Object /**file=args[0],*/ *fname=args[1];

  handle = openFile(oh, (struct ByteArray*)fname, SF_READ|SF_WRITE);
  if (handle >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_handleForNew(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle;
  struct Object /**file=args[0],*/ *fname=args[1];

  handle = openFile(oh, (struct ByteArray*)fname, SF_READ|SF_WRITE|SF_CLEAR|SF_CREATE);
  if (handle >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}


void prim_handle_for_input(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle;
  struct Object /**file=args[0],*/ *fname=args[1];

  handle = openFile(oh, (struct ByteArray*)fname, SF_READ);
  if (handle >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}


void prim_addressOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *handle=args[1], *offset=args[2];
  struct ByteArray* addressBuffer=(struct ByteArray*) args[3];

  if (object_is_smallint(handle) && object_is_smallint(offset)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] =
                           smallint_to_object(addressOfMemory(oh,
                                                              object_to_smallint(handle), 
                                                              object_to_smallint(offset),
                                                              byte_array_elements(addressBuffer)));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_loadLibrary(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *libname=args[1], *handle = args[2];

  if (openExternalLibrary(oh, (struct ByteArray*)libname, (struct ByteArray*)handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false;
  }

}

void prim_closeLibrary(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *handle=args[1];

  if (closeExternalLibrary(oh, (struct ByteArray*) handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false;
  }

}


void prim_procAddressOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *handle=args[2], *symname=args[1];
  struct ByteArray* addressBuffer=(struct ByteArray*) args[3];

  if (lookupExternalLibraryPrimitive(oh, (struct ByteArray*) handle, (struct ByteArray *) symname, addressBuffer)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false;
  }

}


void prim_applyExternal(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  oh->cached.interpreter->stack->elements[resultStackPointer] =
                         applyExternalLibraryPrimitive(oh, (struct ByteArray*)args[1], 
                                                       (struct OopArray*)args[2],
                                                       args[3],
                                                       args[4],
                                                       (struct OopArray*)args[5]);

}

void prim_newFixedArea(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *size=args[1];
  word_t handle;

  if (!object_is_smallint(size)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }

  handle = openMemory(oh, object_to_smallint(size));
  if (handle >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_closeFixedArea(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1];
  if (object_is_smallint(handle)) {
    closeMemory(oh, object_to_smallint(handle));
  }
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

void prim_fixedAreaSize(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1];
  if (object_is_smallint(handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(sizeOfMemory(oh, object_to_smallint(handle)));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_fixedAreaAddRef(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1];
  if (object_is_smallint(handle)) {
    addRefMemory(oh, object_to_smallint(handle));
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

void prim_fixedAreaResize(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1], *size = args[2];
  if (object_is_smallint(handle) && object_is_smallint(size)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = 
                           smallint_to_object(resizeMemory(oh, object_to_smallint(handle), object_to_smallint(size)));

  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_smallint_at_slot_named(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj;
  struct Object* name;
  struct SlotEntry * se;
  struct Object * proto;
  
  obj = args[0];
  name = args[1];
  proto = get_special(oh, SPECIAL_OOP_SMALL_INT_PROTO);
  se = slot_table_entry_for_name(oh, proto->map->slotTable, (struct Symbol*)name);
  if (se == NULL) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
  } else {
    word_t offset = object_to_smallint(se->offset);
    oh->cached.interpreter->stack->elements[resultStackPointer] =  object_slot_value_at_offset(proto, offset);
  }


}



void prim_bytesize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(payload_size(args[0]));
}

void prim_findon(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct MethodDefinition* def;
  struct Symbol* selector= (struct Symbol*) args[0];
  struct OopArray* arguments= (struct OopArray*) args[1];


  def = method_dispatch_on(oh, selector, arguments->elements, array_size(arguments), NULL);

  oh->cached.interpreter->stack->elements[resultStackPointer] = (def == NULL ? oh->cached.nil : (struct Object*) def->method);
}



void prim_ensure(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  struct Closure* body = (struct Closure*) args[0];
  struct Object* ensureHandler = args[1];
  interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, body, NULL, 0, NULL, resultStackPointer);
  assert(0);
  interpreter_stack_push(oh, oh->cached.interpreter, oh->cached.interpreter->ensureHandlers);
  interpreter_stack_push(oh, oh->cached.interpreter, ensureHandler);
  oh->cached.interpreter->ensureHandlers = smallint_to_object(oh->cached.interpreter->stackPointer - 2);

}


void prim_frame_pointer_of(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  struct Interpreter* i = (struct Interpreter*)args[0];
  struct Symbol* selector = (struct Symbol*) args[1];
  struct CompiledMethod* method;
  word_t frame = i->framePointer;



  while (frame > FUNCTION_FRAME_SIZE) {
    method = (struct CompiledMethod*) i->stack->elements[frame-3];
    if (method->selector == selector) {
      oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(frame);
      return;
    }
    frame = object_to_smallint(i->stack->elements[frame-1]);
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}


void prim_bytearray_newsize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj, *i;
  obj = args[0];
  i = args[1];
  
  if (!object_is_smallint(i)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = 
                         (struct Object*)heap_clone_byte_array_sized(oh, obj, (object_to_smallint(i) < 0) ? 0 : object_to_smallint(i));
}


void prim_byteat_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj= args[0], *i=args[1], *val = args[2];
  word_t index;

  index = object_to_smallint(i);

  if (object_is_immutable(obj)) {
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), obj, NULL, resultStackPointer);
    return;
  }
  
  if (index < object_byte_size(obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(byte_array_set_element((struct ByteArray*)obj, index, object_to_smallint(val)));
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, obj, NULL, resultStackPointer);
  }

}


void prim_byteat(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj, *i;
  word_t index;

  obj = args[0];
  i = args[1];
  index = object_to_smallint(i);
  
  if (index < object_byte_size(obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(byte_array_get_element(obj, index));
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, obj, NULL, resultStackPointer);
  }

}


void prim_map(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj;
  obj = args[0];
  
  if (object_is_smallint(obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)obj->map;
  }


}

void prim_set_map(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj;
  struct Map* map;
  obj = args[0];
  map = (struct Map*)args[1];
  
  if (object_is_smallint(obj) || object_is_immutable(obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  } else {
    object_change_map(oh, obj, map);
    heap_store_into(oh, args[0], args[1]);
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)map;
  }

}

void prim_run_args_into(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* arguments = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = 
                         smallint_to_object(write_args_into(oh, (char*)byte_array_elements(arguments), byte_array_size(arguments)));

  
}

void prim_exit(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*  print_stack_types(oh, 128);*/
  print_backtrace(oh);
  exit(0);
}

void prim_isIdenticalTo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (args[0]==args[1])? oh->cached.true : oh->cached.false;
}

void prim_identity_hash(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*fix*/
  /*  print_detail(oh, args[0]);
      print_backtrace(oh);*/
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_hash(args[0]));
}

void prim_identity_hash_univ(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*fix*/
  /*  print_detail(oh, args[0]);
      print_backtrace(oh);*/
  if (object_is_smallint(args[0])) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = args[0];
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_hash(args[0]));
  }
}


void prim_clone(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = heap_clone(oh, args[0]);
}

void prim_applyto(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer) {
  struct Closure *method = (struct Closure*)args[0];
  struct OopArray* argArray = (struct OopArray*) args[1];
  struct OopArray* opts = NULL;

  if (optionals != NULL && optionals->elements[1] != oh->cached.nil) {
    opts = (struct OopArray*) optionals->elements[1];
  }
  interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, method,
                                            argArray->elements, array_size(argArray), opts, resultStackPointer);


}


void prim_at(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* array;
  word_t i;

  array = args[0];
  i = object_to_smallint(args[1]);
  
  if (i < object_array_size(array) && i >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = ((struct OopArray*)array)->elements[i];
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[1], args[0], NULL, resultStackPointer);

  }


}


void prim_at_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *array = args[0], *i = args[1], *val = args[2];
  word_t index = object_to_smallint(i);

  if (object_is_immutable(array)) {
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), array, NULL, resultStackPointer);
    return;
  }
  
  if (index < object_array_size(array)) {
    heap_store_into(oh, array, val);
    oh->cached.interpreter->stack->elements[resultStackPointer] =  ((struct OopArray*)array)->elements[index] = val;
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, array, NULL, resultStackPointer);
  }
}


void prim_ooparray_newsize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*struct Object* array = args[0];*/
  struct Object* i = args[1];
  if (object_is_smallint(i)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)
                           heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO),
                                                      object_to_smallint(i));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }
}


void prim_equals(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (args[0] == args[1])?oh->cached.true:oh->cached.false;
}

void prim_less_than(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = 
    (object_to_smallint(args[0])<object_to_smallint(args[1]))?oh->cached.true:oh->cached.false;
}


void prim_size(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_array_size(args[0]));
}

void prim_bitand(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)((word_t)args[0] & (word_t)args[1]);
}
void prim_bitor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)((word_t)args[0] | (word_t)args[1]);
}
void prim_bitxor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)(((word_t)args[0] ^ (word_t)args[1])|SMALLINT_MASK);
}
void prim_bitnot(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)(~((word_t)args[0]) | SMALLINT_MASK);
}


void prim_plus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  word_t z = object_to_smallint(x) + object_to_smallint(y);
  if (smallint_fits_object(z)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_ADD_OVERFLOW), x, y, NULL, resultStackPointer);
  }
}

void prim_removefrom(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {

  struct Object *method = args[0], *traitsWindow;
  struct OopArray* roles = (struct OopArray*)args[1];
  struct Symbol* selector = (struct Symbol*)oh->cached.nil;
  struct MethodDefinition* def;
  word_t i;

  traitsWindow = method->map->delegates->elements[0];

  if (traitsWindow == oh->cached.closure_method_window || traitsWindow == oh->cached.compiled_method_window) {
    selector = ((struct Closure*)method)->method->selector;
  } else {
    /*May only remove a CompiledMethod or Closure.*/
    assert(0);
  }

  def = method_is_on_arity(oh, method, selector, array_elements(roles), array_size(roles));
  if (def == NULL) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = method;
    return;
  }

  for (i = 0; i < array_size(roles); i++) {
    struct Object* role = array_elements(roles)[i];
    if (!object_is_smallint(role)) {
      object_remove_role(oh, role, selector, def);
    }
  }

  method_flush_cache(oh, selector);
  oh->cached.interpreter->stack->elements[resultStackPointer] = method;

}


void prim_exponent(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* x = (struct ByteArray*)args[0];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object((*(word_t*)float_part(x) >> FLOAT_EXPONENT_OFFSET) & 0xFF);

}

void prim_significand(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* x = (struct ByteArray*)args[0];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(*(word_t*)float_part(x) & FLOAT_SIGNIFICAND);

}

void prim_getcwd(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* buf = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(getCurrentDirectory(buf));
}
void prim_setcwd(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* buf = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(setCurrentDirectory(buf));
}


void prim_float_equals(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  if (*float_part(x) == *float_part(y)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false;
  }
}

void prim_float_less_than(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  if (*float_part(x) < *float_part(y)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false;
  }
}

void prim_float_plus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) + *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_minus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) - *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}


void prim_float_times(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) * *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_divide(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) / *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_raisedTo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = pow(*float_part(x), *float_part(y));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_ln(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = log(*float_part(x));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_exp(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = exp(*float_part(x));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_withSignificand_exponent(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  /*this is really a bytearray*/
  word_t significand = object_to_smallint(args[1]), exponent = object_to_smallint(args[2]);
  struct ByteArray* f = heap_new_float(oh);
  *((word_t*)float_part(f)) = significand | exponent << FLOAT_EXPONENT_OFFSET;

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)f;

}

void prim_bitshift(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  word_t bits = object_to_smallint(args[0]);
  word_t shift = object_to_smallint(args[1]);
  word_t z;
  if (shift >= 0) {
    if (shift >= 32 && bits != 0) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_BIT_SHIFT_OVERFLOW), args[0], args[1], NULL, resultStackPointer);
      return;
    }

    z = bits << shift;

    if (!smallint_fits_object(z) || z >> shift != bits) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_BIT_SHIFT_OVERFLOW), args[0], args[1], NULL, resultStackPointer);
      return;
    }

  } else if (shift <= -32) {
    z = bits >> 31;
  } else {
    z = bits >> -shift;
  }
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);

}


void prim_minus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  word_t z = object_to_smallint(x) - object_to_smallint(y);
  if (smallint_fits_object(z)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SUBTRACT_OVERFLOW), x, y, NULL, resultStackPointer);
  }
}


void prim_times(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  word_t z = object_to_smallint(x) * object_to_smallint(y);
  if (z > 1073741823 || z < -1073741824) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_MULTIPLY_OVERFLOW), x, y, NULL, resultStackPointer);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
  }
}

void prim_quo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  if (object_to_smallint(y) == 0) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_DIVIDE_BY_ZERO), x, y, NULL, resultStackPointer);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_to_smallint(x) / object_to_smallint(y));
  }
}


void prim_forward_to(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = y;

  if (!object_is_smallint(x) && !object_is_smallint(y) && x != y) {
    heap_forward(oh, x, y);
    heap_gc(oh);
  }

}


void prim_clone_setting_slots(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *obj = args[0], *slotArray = args[1], *valueArray = args[2], *newObj;
  word_t i;

  if (object_is_smallint(obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = obj;
    return;
  }
  newObj = heap_clone(oh, obj);

  /*fix, check that arrays are same size, and signal errors*/

  for (i = 0; i < object_array_size(slotArray); i++) {
    struct Symbol* name = (struct Symbol*)object_array_get_element(slotArray, i);
    struct SlotEntry* se = slot_table_entry_for_name(oh, obj->map->slotTable, name);
    if (se == NULL) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL, resultStackPointer);
    } else {
      /*since the object was just cloned, we aren't expecting a tenured obj to point to a new one*/
      object_slot_value_at_offset_put(oh, newObj, object_to_smallint(se->offset), object_array_get_element(valueArray, i));
    }
  }
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = newObj;
}



void prim_as_method_on(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct MethodDefinition* def;
  struct Object *method = args[0], *roles=args[2];
  struct Symbol *selector = (struct Symbol*)args[1];
  struct Object* traitsWindow = method->map->delegates->elements[0];

   
  if (traitsWindow == get_special(oh, SPECIAL_OOP_CLOSURE_WINDOW)) {
    struct Closure* closure = (struct Closure*)heap_clone(oh, method);
    closure->method = (struct CompiledMethod*)heap_clone(oh, (struct Object*)closure->method);
    heap_store_into(oh, (struct Object*)closure, (struct Object*)closure->method);
    closure->method->method = closure->method;
    closure->method->selector = selector;
    method = (struct Object*)closure;
  } else {
    struct CompiledMethod* closure= (struct CompiledMethod*)heap_clone(oh, method);
    closure->method = closure;
    closure->selector = selector;
    method = (struct Object*) closure;
  }
  def = method_define(oh, method, (struct Symbol*)selector, ((struct OopArray*)roles)->elements, object_array_size(roles));
  def->slotAccessor = oh->cached.nil;
  method_flush_cache(oh, selector);
#ifdef PRINT_DEBUG_DEFUN
  printf("Defining function '"); print_symbol(selector);
  printf("' on: ");
  if (!print_printname(oh, ((struct OopArray*)roles)->elements[0])) printf("NoRole");
  {
    word_t i;
    for (i = 1; i < object_array_size(roles); i++) {
      printf(", ");
      if (!print_printname(oh, ((struct OopArray*)roles)->elements[i])) printf("NoRole");
    }
  }
  printf("\n");
#endif

  oh->cached.interpreter->stack->elements[resultStackPointer] = method;
}


void prim_at_slot_named(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj;
  struct Object* name;
  struct SlotEntry * se;
  
  obj = args[0];
  name = args[1];
  
  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
  } else {
    se = slot_table_entry_for_name(oh, obj->map->slotTable, (struct Symbol*)name);
    if (se == NULL) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
    } else {
      word_t offset = object_to_smallint(se->offset);
      oh->cached.interpreter->stack->elements[resultStackPointer] = object_slot_value_at_offset(obj, offset);
    }
  }


}


void prim_at_slot_named_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj=args[0], *val=args[2];
  struct Object* name = args[1];
  struct SlotEntry * se;
  struct Map* map;
  
  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
    return;
  }

  if (object_is_immutable(obj)) {
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), obj, NULL, resultStackPointer);
    return;
  }

  map = obj->map;
  se = slot_table_entry_for_name(oh, map->slotTable, (struct Symbol*)name);

  if (se == NULL) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
  } else {
    word_t offset = object_to_smallint(se->offset);
    oh->cached.interpreter->stack->elements[resultStackPointer] = object_slot_value_at_offset_put(oh, obj, offset, val);
  }

 /*note: not supporting delegate slots*/

}




void prim_clone_with_slot_valued(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj = args[0], *value = args[2];
  struct Symbol* name = (struct Symbol*)args[1];

  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL, resultStackPointer);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = object_add_slot_named(oh, obj, name, value);
  }
}

void prim_clone_without_slot(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj = args[0];
  struct Symbol* name = (struct Symbol*)args[1];

  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL, resultStackPointer);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = object_remove_slot(oh, obj, name);
  }
}

/* 
 * Args are the actual arguments to the function. dispatchers are the
 * objects we find the dispatch function on. Usually they should be
 * the same.
 */

void send_to_through_arity_with_optionals(struct object_heap* oh,
                                                  struct Symbol* selector, struct Object* args[],
                                                  struct Object* dispatchers[], word_t arity, struct OopArray* opts,
                                          word_t resultStackPointer/*where to put the return value in the stack*/) {
  struct OopArray* argsArray;
  struct Closure* method;
  struct Object* traitsWindow;
  struct Object* argsStack[16];
  struct Object* dispatchersStack[16];
  struct MethodDefinition* def;
  struct CompiledMethod* callerMethod;
  word_t addToPic = FALSE;
  callerMethod = oh->cached.interpreter->method;

  /*make sure they are pinned*/
  assert(arity <= 16);
  /*for gc*/
  copy_words_into((word_t*)args, arity, (word_t*)argsStack);
  copy_words_into((word_t*)dispatchers, arity, (word_t*)dispatchersStack);

  def = NULL;

  /* set up a PIC for the caller if it has been called a lot */
  if (object_is_old(oh, (struct Object*)callerMethod)
      && callerMethod->callCount > (struct Object*)CALLER_PIC_SETUP_AFTER) {
    if ((struct Object*)callerMethod->calleeCount == oh->cached.nil) {
      method_pic_setup(oh, callerMethod);
      addToPic = TRUE;
    } else {
      def = method_pic_find_callee(oh, callerMethod, selector, arity, dispatchers);
      if (def==NULL) {
        addToPic = TRUE;
#ifdef PRINT_DEBUG_PIC_HITS
        printf("pic miss\n");
#endif

      }
      else {
#ifdef PRINT_DEBUG_PIC_HITS
        printf("pic hit\n");
#endif
      }
    }
    
  }

  if (def == NULL) {
    def = method_dispatch_on(oh, selector, dispatchers, arity, NULL);
  } else {
#ifdef PRINT_DEBUG_PIC_HITS
    printf("using pic over dispatch\n");
#endif
  }

  if (def == NULL) {
    argsArray = (struct OopArray*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), arity);
    copy_words_into((word_t*)dispatchers, arity, (word_t*)&argsArray->elements[0]);
    heap_fixed_add(oh, (struct Object*)argsArray);
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_NOT_FOUND_ON), (struct Object*)selector, (struct Object*)argsArray, NULL, resultStackPointer);
    heap_fixed_remove(oh, (struct Object*)argsArray);
    return;
  }
  /*PIC add here*/
  if (addToPic) method_pic_add_callee(oh, callerMethod, def, arity, dispatchers);
  method = (struct Closure*)def->method;
  traitsWindow = method->base.map->delegates->elements[0]; /*fix should this location be hardcoded as the first element?*/
  if (traitsWindow == oh->cached.primitive_method_window) {
#ifdef PRINT_DEBUG
    printf("calling primitive: %ld\n", object_to_smallint(((struct PrimitiveMethod*)method)->index));
#endif
    /*fix me to pass result register*/
    primitives[object_to_smallint(((struct PrimitiveMethod*)method)->index)](oh, args, arity, opts, resultStackPointer);
  } else if (traitsWindow == oh->cached.compiled_method_window || traitsWindow == oh->cached.closure_method_window) {
    interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, method, args, arity, opts, resultStackPointer);
  } else {
    struct OopArray* optsArray = NULL;
    argsArray = (struct OopArray*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), arity);
    copy_words_into((word_t*)dispatchers, arity, (word_t*)&argsArray->elements[0]);

    if (opts != NULL) {
      optsArray = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 2);
      optsArray->elements[0] = get_special(oh, SPECIAL_OOP_OPTIONALS);
      optsArray->elements[1] = (struct Object*)opts;
      heap_store_into(oh, (struct Object*)optsArray, (struct Object*)opts);
    }
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_APPLY_TO), def->method, (struct Object*)argsArray, optsArray, resultStackPointer);
  }

}


void prim_send_to(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer) {
  struct Symbol* selector = (struct Symbol*)args[0];
  struct OopArray* opts, *arguments = (struct OopArray*)args[1];

  if (optionals == NULL) {
    opts = NULL;
  } else {
    opts = (struct OopArray*)optionals->elements[1];
    if (opts == (struct OopArray*)oh->cached.nil) opts = NULL;
  }
  assert(0);
  send_to_through_arity_with_optionals(oh, selector, array_elements(arguments), array_elements(arguments), array_size(arguments), opts, 0/*fixme stack*/); 
}

void prim_send_to_through(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer) {
  struct Symbol* selector = (struct Symbol*)args[0];
  struct OopArray* opts, * arguments = (struct OopArray*)args[1], *dispatchers = (struct OopArray*)args[2];

  if (optionals == NULL) {
    opts = NULL;
  } else {
    opts = (struct OopArray*)optionals->elements[1];
    if (opts == (struct OopArray*)oh->cached.nil) opts = NULL;
  }
  /*fix check array sizes are the same*/
  assert(0);
  send_to_through_arity_with_optionals(oh, selector, array_elements(arguments), array_elements(dispatchers), array_size(arguments), opts, 0/*fixme stack*/); 
}




void prim_as_accessor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *method = args[0], *slot = args[2];
  struct OopArray *roles = (struct OopArray*)args[3];
  struct Symbol* selector = (struct Symbol*)args[1];
  struct Object* traitsWindow = method->map->delegates->elements[0];
  struct MethodDefinition* def;
  
  if (traitsWindow == oh->cached.closure_method_window) {
    struct Closure* closure = (struct Closure*)heap_clone(oh, method);
    closure->method = (struct CompiledMethod*)heap_clone(oh, (struct Object*)closure->method);
    heap_store_into(oh, (struct Object*)closure, (struct Object*)closure->method);
    closure->method->method = closure->method;
    closure->method->selector = selector;
    method = (struct Object*)closure;
  } else if (traitsWindow == oh->cached.compiled_method_window){
    struct CompiledMethod* closure = (struct CompiledMethod*)heap_clone(oh, method);
    closure->method = closure;
    closure->selector = selector;
    method = (struct Object*) closure;
  }

  def = method_define(oh, method, selector, roles->elements, array_size(roles));
  def->slotAccessor = slot;
  method_flush_cache(oh, selector);
  oh->cached.interpreter->stack->elements[resultStackPointer] = method;

#ifdef PRINT_DEBUG_DEFUN
  printf("Defining accessor '"); print_symbol(selector);
  printf("' on: ");
  if (!print_printname(oh, ((struct OopArray*)roles)->elements[0])) printf("NoRole");
  {
    word_t i;
    for (i = 1; i < array_size(roles); i++) {
      printf(", ");
      if (!print_printname(oh, ((struct OopArray*)roles)->elements[i])) printf("NoRole");
    }
  }
  printf("\n");
#endif

}

struct ForwardPointerEntry* forward_pointer_hash_get(struct ForwardPointerEntry* table,
                                                    word_t forwardPointerEntryCount,
                                                    struct Object* fromObj) {
  word_t index;
  word_t hash = (word_t)fromObj % forwardPointerEntryCount;
  struct ForwardPointerEntry* entry;

  for (index = hash; index < forwardPointerEntryCount; index++) {
    entry = &table[index];
    if (entry->fromObj == fromObj || entry->fromObj == NULL) return entry;
  }
  for (index = 0; index < hash; index++) {
    entry = &table[index];
    if (entry->fromObj == fromObj || entry->fromObj == NULL) return entry;
  }

  return NULL;


}

struct ForwardPointerEntry* forward_pointer_hash_add(struct ForwardPointerEntry* table,
                                                    word_t forwardPointerEntryCount,
                                                    struct Object* fromObj, struct Object* toObj) {

  struct ForwardPointerEntry* entry = forward_pointer_hash_get(table, forwardPointerEntryCount, fromObj);
  if (NULL == entry) return NULL; /*full table*/
  assert(entry->fromObj == NULL || entry->fromObj == fromObj); /*assure we don't have inconsistancies*/
  entry->fromObj = fromObj;
  entry->toObj = toObj;
  return entry;

}


void copy_used_objects(struct object_heap* oh, struct Object** writeObject,  byte_t* memoryStart, word_t memorySize,
struct ForwardPointerEntry* table, word_t forwardPointerEntryCount) {
  struct Object* readObject = (struct Object*) memoryStart;
  while (object_in_memory(oh, readObject, memoryStart, memorySize)) {
    if (!object_is_free(readObject)) {
      assert(NULL != forward_pointer_hash_add(table, forwardPointerEntryCount, readObject, *writeObject));
      copy_words_into((word_t*)readObject, object_word_size(readObject), (word_t*)*writeObject);
      *writeObject = object_after(oh, *writeObject);
    }
    readObject = object_after(oh, readObject);
  }

}

void adjust_object_fields_with_table(struct object_heap* oh, byte_t* memory, word_t memorySize,
                                     struct ForwardPointerEntry* table, word_t forwardPointerEntryCount) {

  struct Object* o = (struct Object*) memory;
 
  while (object_in_memory(oh, o, memory, memorySize)) {
    word_t offset, limit;
    o->map = (struct Map*)forward_pointer_hash_get(table, forwardPointerEntryCount, (struct Object*)o->map)->toObj;
    offset = object_first_slot_offset(o);
    limit = object_last_oop_offset(o) + sizeof(word_t);
    for (; offset != limit; offset += sizeof(word_t)) {
      struct Object* val = object_slot_value_at_offset(o, offset);
      if (!object_is_smallint(val)) {
        object_slot_value_at_offset_put(oh, o, offset, 
                                        forward_pointer_hash_get(table, forwardPointerEntryCount, val)->toObj);
      }
    }
    o = object_after(oh, o);
  }

}


void adjust_fields_by(struct object_heap* oh, struct Object* o, word_t shift_amount) {

  word_t offset, limit;
  o->map = (struct Map*) inc_ptr(o->map, shift_amount);
  offset = object_first_slot_offset(o);
  limit = object_last_oop_offset(o) + sizeof(word_t);
  for (; offset != limit; offset += sizeof(word_t)) {
    struct Object* val = object_slot_value_at_offset(o, offset);
    if (!object_is_smallint(val)) {
      /*avoid calling the function because it will call heap_store_into*/
      (*((word_t*)inc_ptr(o, offset))) = (word_t)inc_ptr(val, shift_amount);
  /*      object_slot_value_at_offset_put(oh, o, offset, (struct Object*)inc_ptr(val, shift_amount));*/
    }
  }

}


void adjust_oop_pointers_from(struct object_heap* oh, word_t shift_amount, byte_t* memory, word_t memorySize) {

  struct Object* o = (struct Object*)memory;
#ifdef PRINT_DEBUG
  printf("First object: "); print_object(o);
#endif
  while (object_in_memory(oh, o, memory, memorySize)) {
    /*print_object(o);*/
    if (!object_is_free(o)) {
      adjust_fields_by(oh, o, shift_amount);
    }
    o = object_after(oh, o);
  }


}


void prim_save_image(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  char nameString [SLATE_FILE_NAME_LENGTH];
  struct slate_image_header sih;
  struct Object* name = args[1];
  size_t nameLength = payload_size(name);
  FILE * imageFile;

  word_t totalSize, forwardPointerEntryCount;
  byte_t* memoryStart;
  struct Object *writeObject;
  struct ForwardPointerEntry* forwardPointers;
  /* do a full gc, allocate a new chunk of memory the size of the young and old combined,
   * copy all the non-free objects to the new memory while keeping an array of the position changes,
   * go through the memory and fix up the pointers, adjust points to start from 0 instead of memoryStart,
   * and write the header and the memory out to disk
   */

  /*push true so if it resumes from the save image, it will do init code*/
  /*fixme*/
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true;

  if (nameLength >= sizeof(nameString)) {
    /*interpreter_stack_pop(oh, oh->cached.interpreter);*/
    /*push nil*/
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }
  memcpy(nameString, (char*)byte_array_elements((struct ByteArray*)name), nameLength);
  nameString[nameLength] = '\0';

  imageFile = fopen(nameString, "wb");
  if (!imageFile) {
    /*interpreter_stack_pop(oh, oh->cached.interpreter);*/
    /*push nil*/
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

    return;
  }
  printf("Saving image to %s\n", nameString);
  heap_full_gc(oh);
  totalSize = oh->memoryOldSize + oh->memoryYoungSize;
  forwardPointerEntryCount = ((totalSize / 4) + sizeof(struct ForwardPointerEntry) - 1) / sizeof(struct ForwardPointerEntry);
  memoryStart = malloc(totalSize);
  writeObject = (struct Object*)memoryStart;
  forwardPointers = calloc(1, forwardPointerEntryCount * sizeof(struct ForwardPointerEntry));
  assert(memoryStart != NULL);
  copy_used_objects(oh, &writeObject, oh->memoryOld, oh->memoryOldSize, forwardPointers, forwardPointerEntryCount);
  copy_used_objects(oh, &writeObject, oh->memoryYoung, oh->memoryYoungSize, forwardPointers, forwardPointerEntryCount);
  totalSize = (byte_t*)writeObject - memoryStart;
  adjust_object_fields_with_table(oh, memoryStart, totalSize, forwardPointers, forwardPointerEntryCount);
  adjust_oop_pointers_from(oh, 0-(word_t)memoryStart, memoryStart, totalSize);
  sih.magic = SLATE_IMAGE_MAGIC;
  sih.size = totalSize;
  sih.next_hash = heap_new_hash(oh);
  sih.special_objects_oop = (byte_t*) (forward_pointer_hash_get(forwardPointers, forwardPointerEntryCount, (struct Object*)oh->special_objects_oop)->toObj) - memoryStart;
  sih.current_dispatch_id = oh->current_dispatch_id;

  fwrite(&sih, sizeof(struct slate_image_header), 1, imageFile);
  fwrite(memoryStart, 1, totalSize, imageFile);
  fclose(imageFile);
  free(forwardPointers);
  free(memoryStart);

  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false;
  /*
  interpreter_stack_pop(oh, oh->cached.interpreter);
  interpreter_push_false(oh, oh->cached.interpreter);
  */
  

}


void (*primitives[]) (struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) = {

 /*0-9*/ prim_as_method_on, prim_as_accessor, prim_map, prim_set_map, prim_fixme, prim_removefrom, prim_clone, prim_clone_setting_slots, prim_clone_with_slot_valued, prim_fixme, 
 /*10-9*/ prim_fixme, prim_fixme, prim_clone_without_slot, prim_at_slot_named, prim_smallint_at_slot_named, prim_at_slot_named_put, prim_forward_to, prim_bytearray_newsize, prim_bytesize, prim_byteat, 
 /*20-9*/ prim_byteat_put, prim_ooparray_newsize, prim_size, prim_at, prim_at_put, prim_ensure, prim_applyto, prim_send_to, prim_send_to_through, prim_findon, 
 /*30-9*/ prim_fixme, prim_run_args_into, prim_exit, prim_isIdenticalTo, prim_identity_hash, prim_identity_hash_univ, prim_equals, prim_less_than, prim_bitor, prim_bitand, 
 /*40-9*/ prim_bitxor, prim_bitnot, prim_bitshift, prim_plus, prim_minus, prim_times, prim_quo, prim_fixme, prim_fixme, prim_frame_pointer_of, 
 /*50-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_bytesPerWord, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 
 /*60-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_readConsole_from_into_starting_at, prim_write_to_starting_at, prim_flush_output, prim_handle_for, prim_handle_for_input, prim_fixme, 
 /*70-9*/ prim_handleForNew, prim_close, prim_read_from_into_starting_at, prim_write_to_from_starting_at, prim_reposition_to, prim_positionOf, prim_atEndOf, prim_sizeOf, prim_save_image, prim_fixme, 
 /*80-9*/ prim_fixme, prim_fixme, prim_getcwd, prim_setcwd, prim_significand, prim_exponent, prim_withSignificand_exponent, prim_float_equals, prim_float_less_than, prim_float_plus, 
 /*90-9*/ prim_float_minus, prim_float_times, prim_float_divide, prim_float_raisedTo, prim_float_ln, prim_float_exp, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 
 /*00-9*/ prim_fixme, prim_fixme, prim_fixme, prim_newFixedArea, prim_closeFixedArea, prim_fixedAreaAddRef, prim_fixme, prim_fixme, prim_fixedAreaSize, prim_fixedAreaResize,
 /*10-9*/ prim_addressOf, prim_loadLibrary, prim_closeLibrary, prim_procAddressOf, prim_fixme, prim_applyExternal, prim_fixme, prim_fixme, prim_fixme, prim_fixme,
 /*20-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,
 /*30-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,
 /*40-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,
 /*50-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,

};



bool interpreter_return_result(struct object_heap* oh, struct Interpreter* i, word_t context_depth, struct Object* result) {
  /* Implements a non-local return with a value, specifying the block to return
     from via lexical offset. Returns a success Boolean. */
  word_t framePointer;
  word_t ensureHandlers;
  word_t resultStackPointer;

#ifdef PRINT_DEBUG_FUNCALL
      printf("interpreter_return_result BEFORE\n");
      printf("stack pointer: %ld\n", i->stackPointer);
      printf("frame pointer: %ld\n", i->framePointer);
      print_stack_types(oh, 16);
#endif


  if (context_depth == 0) {
    framePointer = i->framePointer;
  } else {
    struct LexicalContext* targetContext = i->closure->lexicalWindow[context_depth-1];
    framePointer = object_to_smallint(targetContext->framePointer);
    if (framePointer > i->stackPointer || (struct Object*)targetContext != i->stack->elements[framePointer-2]) {

      interpreter_signal_with_with(oh, i, get_special(oh, SPECIAL_OOP_MAY_NOT_RETURN_TO),
                                   (struct Object*)i->closure, (struct Object*) targetContext, NULL, resultStackPointer);
      return 1;
    }
  }

  resultStackPointer = (word_t)i->stack->elements[framePointer - 5]>>1;

  ensureHandlers = object_to_smallint(i->ensureHandlers);
  if (framePointer <= ensureHandlers) {
    struct Object* ensureHandler = i->stack->elements[ensureHandlers+1];
    i->ensureHandlers = i->stack->elements[ensureHandlers];
    interpreter_stack_push(oh, i, smallint_to_object(resultStackPointer));
    interpreter_stack_push(oh, i, smallint_to_object(i->codePointer));
    interpreter_stack_push(oh, i, get_special(oh, SPECIAL_OOP_ENSURE_MARKER));
    interpreter_stack_push(oh, i, oh->cached.nil);
    interpreter_stack_push(oh, i, smallint_to_object(i->framePointer));
    i->codePointer = 0;
    i->framePointer = i->stackPointer;
    assert(0);
    {
      interpreter_apply_to_arity_with_optionals(oh, i, (struct Closure*) ensureHandler, NULL, 0, NULL, resultStackPointer);
    }
    return 1;
  }
  if (result != NULL) {
    i->stack->elements[resultStackPointer] = result;
    heap_store_into(oh, (struct Object*)i->stack, (struct Object*)result);
  }
  i->stackPointer = framePointer - FUNCTION_FRAME_SIZE;
  i->framePointer = object_to_smallint(i->stack->elements[framePointer - 1]);
  if (i->framePointer < FUNCTION_FRAME_SIZE) {
    assert(0);
    return 0;
  }
  i->codePointer = object_to_smallint(i->stack->elements[framePointer - 4]);
  i->lexicalContext = (struct LexicalContext*) i->stack->elements[i->framePointer - 2];
  i->closure = (struct Closure*) i->stack->elements[i->framePointer - 3];
  heap_store_into(oh, (struct Object*)i, (struct Object*)i->lexicalContext);
  heap_store_into(oh, (struct Object*)i, (struct Object*)i->closure);
  i->method = i->closure->method;
  heap_store_into(oh, (struct Object*)i, (struct Object*)i->closure->method);

  i->codeSize = array_size(i->method->code);

#ifdef PRINT_DEBUG_FUNCALL
      printf("interpreter_return_result AFTER\n");
      printf("stack pointer: %ld\n", i->stackPointer);
      printf("frame pointer: %ld\n", i->framePointer);
      print_stack_types(oh, 16);
#endif


  return 1;

}





void interpreter_resend_message(struct object_heap* oh, struct Interpreter* i, word_t n, word_t resultStackPointer) {

  word_t framePointer;
  struct LexicalContext* lexicalContext;
  struct Object *barrier, *traitsWindow;
  struct Object** args;
  struct Symbol* selector;
  struct OopArray* argsArray;
  struct Closure* method;
  struct CompiledMethod* resender;
  struct MethodDefinition* def;
  struct Object* argsStack[16]; /*for pinning*/

  if (n == 0) {
    framePointer = i->framePointer;
    lexicalContext = i->lexicalContext;
  } else {
    lexicalContext = i->closure->lexicalWindow[n-1];
    framePointer = object_to_smallint(lexicalContext->framePointer);
    /*Attempted to resend without enclosing method definition.*/
    assert((struct Object*)lexicalContext == i->stack->elements[framePointer-2]);
  }

  barrier = i->stack->elements[framePointer-3];
  resender = ((struct Closure*) barrier)->method;
  args = (resender->heapAllocate == oh->cached.true)? lexicalContext->variables : &i->stack->elements[framePointer];


  selector = resender->selector;
  n = object_to_smallint(resender->inputVariables);
  assert(n <= 16);
  copy_words_into((word_t*)args, n, (word_t*)argsStack);

  def = method_dispatch_on(oh, selector, args, n, barrier);
  if (def == NULL) {
    argsArray = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
    copy_words_into((word_t*)args, n, (word_t*)argsArray->elements);
    interpreter_signal_with_with_with(oh, i, get_special(oh, SPECIAL_OOP_NOT_FOUND_ON),
                                      (struct Object*)selector, (struct Object*)argsArray, (struct Object*) resender, NULL, resultStackPointer);
    return;

  }

  method = (struct Closure*)def->method;
  traitsWindow = method->base.map->delegates->elements[0];
  if (traitsWindow == oh->cached.primitive_method_window) {
#ifdef PRINT_DEBUG
    printf("calling primitive: %ld\n", object_to_smallint(((struct PrimitiveMethod*)method)->index));
#endif
    primitives[object_to_smallint(((struct PrimitiveMethod*)method)->index)](oh, args, n, NULL, resultStackPointer);
    return;
  }

  if (traitsWindow == oh->cached.compiled_method_window || traitsWindow == oh->cached.closure_method_window) {
    struct OopArray* optKeys = resender->optionalKeywords;
    interpreter_apply_to_arity_with_optionals(oh, i, method, args, n, NULL, resultStackPointer);
    if (i->closure == method) {
      word_t optKey;
      for (optKey = 0; optKey < array_size(optKeys); optKey++) {
        struct Object* optVal = args[n+optKey];
        if (optVal != oh->cached.nil) {
          interpreter_dispatch_optional_keyword(oh, i, optKeys->elements[optKey], optVal);
        }
      }
    }

  } else {
    struct OopArray* optsArray;
    struct OopArray* optKeys;
    struct OopArray* signalOpts;
    word_t optKey;
    
    optKeys = resender->optionalKeywords;
    optsArray = (struct OopArray*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO),
                                                              array_size(optKeys) * 2);
    
    for (optKey = 0; optKey < array_size(optKeys); optKey++) {
      struct Object* optVal = args[n+optKey];
      optsArray->elements[optKey*2] = optKeys->elements[optKey];
      optsArray->elements[optKey*2+1] = optVal;
    }
    
    signalOpts = (struct OopArray*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 2);
    signalOpts->elements[0] = get_special(oh, SPECIAL_OOP_OPTIONALS);
    signalOpts->elements[1] = (struct Object*) optsArray;
    heap_store_into(oh, (struct Object*)signalOpts, (struct Object*)optsArray);

    argsArray = (struct OopArray*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
    copy_words_into((word_t*) args, n, (word_t*) argsArray->elements);
    
    interpreter_signal_with_with(oh, i, get_special(oh, SPECIAL_OOP_APPLY_TO),
                                 def->method, (struct Object*) argsArray, signalOpts, resultStackPointer);
    
  }


}



void interpreter_branch_keyed(struct object_heap* oh, struct Interpreter * i, struct OopArray* table, struct Object* oop) {

  word_t tableSize;
  word_t hash;
  word_t index;
  
  if (oop_is_object((word_t)oop))
    hash = object_hash(oop);
  else
    hash = object_to_smallint(oop);
  tableSize = array_size(table);
  hash = hash & (tableSize - 2); /*fix: check this out... size is probably 2^n, so XX & 2^n-2 is like mod tableSize?*/
  index = hash;

  /*help: why isn't this just one loop from 0 to tableSize? how is this opcode used?*/
  while (index < tableSize)
  {
    struct Object* offset;
    struct Object* key;
    
    key = table->elements[index];
    offset = table->elements[index + 1];
    if (offset == oh->cached.nil)
    {
      return;
    }
    if (oop == key)
    {
      i->codePointer = i->codePointer + object_to_smallint(offset); /*fix check that offset is the right type*/
      
      return;
    }
    index = index + 2;
  }
  index = 0;
  while (index < hash)
  {
    struct Object* offset;
    struct Object* key;
    
    key = table->elements[index];
    offset = table->elements[index + 1];
    if (offset == oh->cached.nil)
    {
      return;
    }
    if (oop == key)
    {
      i->codePointer = i->codePointer + object_to_smallint(offset);
      
      return;
    }
    index = index + 2;
  }


}

#define OP_SEND                         ((0 << 1) | SMALLINT_MASK)
#define OP_INDIRECT_SEND                ((1 << 1) | SMALLINT_MASK) /*unused now*/
#define OP_ALLOCATE_REGISTERS           ((2 << 1) | SMALLINT_MASK)
#define OP_LOAD_LITERAL                 ((3 << 1) | SMALLINT_MASK)
#define OP_STORE_LITERAL                ((4 << 1) | SMALLINT_MASK)
#define OP_SEND_MESSAGE_WITH_OPTS       ((5 << 1) | SMALLINT_MASK)
/*?? profit??*/
#define OP_NEW_CLOSURE                  ((7 << 1) | SMALLINT_MASK)
#define OP_NEW_ARRAY_WITH               ((8 << 1) | SMALLINT_MASK)
#define OP_RESEND_MESSAGE               ((9 << 1) | SMALLINT_MASK)
#define OP_RETURN_FROM                  ((10 << 1) | SMALLINT_MASK)
#define OP_LOAD_ENVIRONMENT             ((11 << 1) | SMALLINT_MASK)
#define OP_LOAD_VARIABLE                ((12 << 1) | SMALLINT_MASK)
#define OP_STORE_VARIABLE               ((13 << 1) | SMALLINT_MASK)
#define OP_LOAD_FREE_VARIABLE           ((14 << 1) | SMALLINT_MASK)
#define OP_STORE_FREE_VARIABLE          ((15 << 1) | SMALLINT_MASK)
#define OP_IS_IDENTICAL_TO              ((16 << 1) | SMALLINT_MASK)
#define OP_BRANCH_KEYED                 ((17 << 1) | SMALLINT_MASK)
#define OP_JUMP_TO                      ((18 << 1) | SMALLINT_MASK)
#define OP_MOVE_REGISTER                ((19 << 1) | SMALLINT_MASK)
#define OP_BRANCH_IF_TRUE               ((20 << 1) | SMALLINT_MASK)
#define OP_BRANCH_IF_FALSE              ((21 << 1) | SMALLINT_MASK)
#define OP_RETURN_REGISTER              ((22 << 1) | SMALLINT_MASK)
#define OP_RETURN_VALUE                 ((23 << 1) | SMALLINT_MASK)
#define OP_RESUME                       ((24 << 1) | SMALLINT_MASK)
#define OP_                             ((25 << 1) | SMALLINT_MASK)

#define SSA_REGISTER(X)                 (i->stack->elements[i->framePointer + (X)])
#define SSA_NEXT_PARAM_SMALLINT         ((word_t)i->method->code->elements[i->codePointer++]>>1)
#define SSA_NEXT_PARAM_OBJECT           (i->method->code->elements[i->codePointer++])

void interpret(struct object_heap* oh) {

 /* we can set a conditional breakpoint if the vm crash is consistent */
  unsigned long int instruction_counter = 0;

#ifdef PRINT_DEBUG
  printf("Interpret: img:%p size:%ld spec:%p next:%ld\n",
         (void*)oh->memoryOld, oh->memoryOldSize, (void*)oh->special_objects_oop, oh->lastHash);
  printf("Special oop: "); print_object((struct Object*)oh->special_objects_oop);
#endif

  cache_specials(oh);

#ifdef PRINT_DEBUG
  printf("Interpreter stack: "); print_object((struct Object*)oh->cached.interpreter);
  printf("Interpreter stack size: %ld\n", oh->cached.interpreter->stackSize);
  printf("Interpreter stack pointer: %ld\n", oh->cached.interpreter->stackPointer);
  printf("Interpreter frame pointer: %ld\n", oh->cached.interpreter->framePointer);
#endif 

  do {
    word_t op, prevPointer;
    struct Interpreter* i = oh->cached.interpreter; /*it won't move while we are in here */

    /*while (oh->cached.interpreter->codePointer < oh->cached.interpreter->codeSize) {*/
    /*optimize and make sure every function has manual return opcodes*/
    for(;;) {
      
      if (oh->interrupt_flag) {
        return;
      }
      
      instruction_counter++;
      op = (word_t)i->method->code->elements[i->codePointer];
#ifdef PRINT_DEBUG_CODE_POINTER
      printf("(%ld/%ld) %ld ", i->codePointer, i->codeSize, op>>1);
#endif
      prevPointer = i->codePointer;
      i->codePointer++;

      switch (op) {

      case OP_SEND:
        {
          word_t result, arity;
          int k;
          struct Object *selector;
          struct Object* args[16];
          result = SSA_NEXT_PARAM_SMALLINT;
          selector = SSA_NEXT_PARAM_OBJECT;
          arity = SSA_NEXT_PARAM_SMALLINT;

#ifdef PRINT_DEBUG_OPCODES
          printf("send message fp: %ld, result: %ld, arity: %ld, message: ", i->framePointer, result, arity);
          print_type(oh, selector);
#endif
          assert(arity <= 16);
          for (k=0; k<arity; k++) {
            word_t argReg = SSA_NEXT_PARAM_SMALLINT;
            args[k] = SSA_REGISTER(argReg);
#ifdef PRINT_DEBUG_OPCODES
            printf("args[%d@%ld] = ", k, argReg);
            print_type(oh, args[k]);
#endif
          }
          send_to_through_arity_with_optionals(oh, (struct Symbol*)selector, args, args, arity, NULL, i->framePointer + result);
#ifdef PRINT_DEBUG_OPCODES
          printf("in function: \n");
          print_type(oh, (struct Object*)i->method);
#endif
          break;
        }
      case OP_SEND_MESSAGE_WITH_OPTS:
        {
          word_t result, arity, optsArrayReg;
          int k;
          struct Object *selector;
          struct Object* args[16];
          result = SSA_NEXT_PARAM_SMALLINT;
          selector = SSA_NEXT_PARAM_OBJECT;
          arity = SSA_NEXT_PARAM_SMALLINT;
          optsArrayReg = SSA_NEXT_PARAM_SMALLINT;

#ifdef PRINT_DEBUG_OPCODES
          printf("send message with opts fp: %ld, result: %ld arity: %ld, opts: %ld, message: ", i->framePointer, result, arity, optsArrayReg);
          print_type(oh, selector);
#endif
          assert(arity <= 16);
          for (k=0; k<arity; k++) {
            args[k] = SSA_REGISTER(SSA_NEXT_PARAM_SMALLINT);
#ifdef PRINT_DEBUG_OPCODES
            printf("args[%d] = ", k);
            print_type(oh, args[k]);
#endif
          }
          send_to_through_arity_with_optionals(oh, (struct Symbol*)selector, args, args,
                                               arity, (struct OopArray*)SSA_REGISTER(optsArrayReg),
                                               i->framePointer + result);
          break;
        }
      case OP_NEW_ARRAY_WITH:
        {
          word_t result, size, k;
          struct OopArray* array;
          result = SSA_NEXT_PARAM_SMALLINT;
          size = SSA_NEXT_PARAM_SMALLINT;

#ifdef PRINT_DEBUG_OPCODES
          printf("new array, result: %ld, size: %ld\n", result, size);
#endif
          array = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), size);
          for (k = 0; k < size; k++) {
            array->elements[k] = SSA_REGISTER(SSA_NEXT_PARAM_SMALLINT);
          }
          heap_store_into(oh, (struct Object*)i->stack, (struct Object*)array);
          SSA_REGISTER(result) = (struct Object*) array;
          break;
        }
      case OP_NEW_CLOSURE:
        {
          word_t result;
          struct CompiledMethod* block;
          struct Closure* newClosure;
          result = SSA_NEXT_PARAM_SMALLINT;
          block = (struct CompiledMethod*)SSA_NEXT_PARAM_OBJECT;
#ifdef PRINT_DEBUG_OPCODES
          printf("new closure, result: %ld, block", result);
          print_type(oh, (struct Object*)block);
#endif
          if ((struct CompiledMethod *) i->closure == i->method) {
            newClosure = (struct Closure *) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_CLOSURE_PROTO), 1);
          } else {
            word_t inheritedSize;
            
            inheritedSize = object_array_size((struct Object *) i->closure);
            newClosure = (struct Closure *) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_CLOSURE_PROTO), inheritedSize+1);
            copy_words_into((word_t *) i->closure->lexicalWindow, inheritedSize, (word_t*) newClosure->lexicalWindow + 1);
          }
          newClosure->lexicalWindow[0] = i->lexicalContext;
          newClosure->method = block;
          heap_store_into(oh, (struct Object*)newClosure, (struct Object*)i->lexicalContext);
          heap_store_into(oh, (struct Object*)newClosure, (struct Object*)block);
          heap_store_into(oh, (struct Object*)i->stack, (struct Object*)newClosure);
          SSA_REGISTER(result) = (struct Object*) newClosure;
          break;
        }
      case OP_LOAD_LITERAL:
        {
          word_t destReg;
          struct Object* literal;
          destReg = SSA_NEXT_PARAM_SMALLINT;
          literal = SSA_NEXT_PARAM_OBJECT;
#ifdef PRINT_DEBUG_OPCODES
          printf("load literal into reg %ld, value: ", destReg);
          print_type(oh, literal);
#endif
          heap_store_into(oh, (struct Object*)i->stack, literal);
          SSA_REGISTER(destReg) = literal;
          break;
        }
      case OP_RESEND_MESSAGE:
        {
          word_t resultRegister, lexicalOffset;
          resultRegister = SSA_NEXT_PARAM_SMALLINT;
          lexicalOffset = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("resend message reg %ld, offset: %ld\n", resultRegister, lexicalOffset);
#endif
          interpreter_resend_message(oh, i, lexicalOffset, i->framePointer + resultRegister);
          break;
        }
      case OP_LOAD_VARIABLE:
        {
          word_t var;
          var = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("load var %ld\n", var);
#endif
          if (i->method->heapAllocate == oh->cached.true) {
            heap_store_into(oh, (struct Object*)i->stack, (struct Object*)i->lexicalContext);
            SSA_REGISTER(var) = i->lexicalContext->variables[var];
          }
#ifdef PRINT_DEBUG_OPCODES
          printf("var val =");
          print_type(oh, SSA_REGISTER(var));
#endif
          /*if it's not heap allocated the register is already loaded*/
          break;
        }
      case OP_STORE_VARIABLE:
        {
          word_t var;
          var = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("store var %ld\n", var);
#endif
          if (i->method->heapAllocate == oh->cached.true) {
            heap_store_into(oh, (struct Object*)i->lexicalContext, (struct Object*)i->stack);
            i->lexicalContext->variables[var] = SSA_REGISTER(var);
          }
          /*if it's not heap allocated the register is already loaded*/
#ifdef PRINT_DEBUG_OPCODES
          printf("var val =");
          print_type(oh, SSA_REGISTER(var));
#endif
          break;
        }
      case OP_LOAD_FREE_VARIABLE:
        {
          word_t destReg, lexOffset, varIndex;
          destReg = SSA_NEXT_PARAM_SMALLINT;
          lexOffset = SSA_NEXT_PARAM_SMALLINT;
          varIndex = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("load free var to: %ld, lexoffset: %ld, index: %ld\n", destReg, lexOffset, varIndex);
#endif
          heap_store_into(oh, (struct Object*)i->stack, (struct Object*)i->closure->lexicalWindow[lexOffset-1]);
          SSA_REGISTER(destReg) = i->closure->lexicalWindow[lexOffset-1]->variables[varIndex];

#ifdef PRINT_DEBUG_OPCODES
          printf("var val =");
          print_type(oh, SSA_REGISTER(destReg));
#endif
          break;
        }
      case OP_STORE_FREE_VARIABLE:
        {
          word_t srcReg, lexOffset, varIndex;
          lexOffset = SSA_NEXT_PARAM_SMALLINT;
          varIndex = SSA_NEXT_PARAM_SMALLINT;
          srcReg = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("store free var from: %ld, lexoffset: %ld, index: %ld\n", srcReg, lexOffset, varIndex);
#endif
          heap_store_into(oh, (struct Object*)i->closure->lexicalWindow[lexOffset-1], (struct Object*)i->stack);
          i->closure->lexicalWindow[lexOffset-1]->variables[varIndex] = SSA_REGISTER(srcReg);

#ifdef PRINT_DEBUG_OPCODES
          printf("var val =");
          print_type(oh, SSA_REGISTER(srcReg));
#endif
          break;
        }
      case OP_MOVE_REGISTER:
        {
          word_t destReg, srcReg;
          destReg = SSA_NEXT_PARAM_SMALLINT;
          srcReg = SSA_NEXT_PARAM_SMALLINT;

#ifdef PRINT_DEBUG_OPCODES
          printf("move reg %ld, %ld\n", destReg, srcReg);
#endif
          heap_store_into(oh, (struct Object*)i->stack, SSA_REGISTER(srcReg));
          SSA_REGISTER(destReg) = SSA_REGISTER(srcReg);
          break;
        }
      case OP_IS_IDENTICAL_TO:
        {
          word_t destReg, srcReg, resultReg;
          resultReg = SSA_NEXT_PARAM_SMALLINT;
          destReg = SSA_NEXT_PARAM_SMALLINT;
          srcReg = SSA_NEXT_PARAM_SMALLINT;

#ifdef PRINT_DEBUG_OPCODES
          printf("is identical %ld, %ld\n", destReg, srcReg);
#endif
          
          SSA_REGISTER(resultReg) = (SSA_REGISTER(destReg) == SSA_REGISTER(srcReg)) ? oh->cached.true : oh->cached.false;
          break;
        }
      case OP_BRANCH_KEYED:
        {
          word_t tableReg, keyReg;
          keyReg = SSA_NEXT_PARAM_SMALLINT;
          tableReg = SSA_NEXT_PARAM_SMALLINT;
          assert(0);
#ifdef PRINT_DEBUG_OPCODES
          printf("branch keyed: %ld/%ld\n", tableReg, keyReg);
#endif
          interpreter_branch_keyed(oh, i, (struct OopArray*)SSA_REGISTER(tableReg), SSA_REGISTER(keyReg));
          break;
        }
      case OP_BRANCH_IF_TRUE:
        {
          word_t condReg, offset;
          struct Object* val;
          condReg = SSA_NEXT_PARAM_SMALLINT;
          offset = SSA_NEXT_PARAM_SMALLINT - 1;

          val = SSA_REGISTER(condReg);

#ifdef PRINT_DEBUG_OPCODES
          printf("branch if true: %ld, offset: %ld, val: ", condReg, offset);
          print_type(oh, val);
#endif
          if (val == oh->cached.true) {
            i->codePointer = i->codePointer + offset;
          } else {
            if (val != oh->cached.false) {
              i->codePointer = i->codePointer - 3;
              interpreter_signal_with(oh, i, get_special(oh, SPECIAL_OOP_NOT_A_BOOLEAN), val, NULL, condReg /*fixme*/);
            }
          }
          break;
        }
      case OP_BRANCH_IF_FALSE:
        {
          word_t condReg, offset;
          struct Object* val;
          condReg = SSA_NEXT_PARAM_SMALLINT;
          offset = SSA_NEXT_PARAM_SMALLINT - 1;

          val = SSA_REGISTER(condReg);

#ifdef PRINT_DEBUG_OPCODES
          printf("branch if false: %ld, offset: %ld, val: ", condReg, offset);
          print_type(oh, val);
#endif
          if (val == oh->cached.false) {
            i->codePointer = i->codePointer + offset;
          } else {
            if (val != oh->cached.true) {
              i->codePointer = i->codePointer - 3;
              interpreter_signal_with(oh, i, get_special(oh, SPECIAL_OOP_NOT_A_BOOLEAN), val, NULL, condReg /*fixme*/);
            }
          }
          break;
        }
      case OP_JUMP_TO:
        {
          word_t offset;
          offset = SSA_NEXT_PARAM_SMALLINT - 1;
          assert(offset < 20000 && offset > -20000);
#ifdef PRINT_DEBUG_OPCODES
          printf("jump to offset: %ld\n", offset);
#endif
          i->codePointer = i->codePointer + offset;
          
          break;
        }
      case OP_LOAD_ENVIRONMENT:
        {
          word_t next_param;
          next_param = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("load environment into reg %ld, value: ", next_param);
          print_type(oh, i->method->environment);
#endif
          heap_store_into(oh, (struct Object*)i->stack, (struct Object*)i->method->environment);
          SSA_REGISTER(next_param) = i->method->environment;
          break;
        }
      case OP_RETURN_REGISTER:
        {
          word_t reg;
          reg = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("return reg %ld, value: ", reg);
          print_type(oh, SSA_REGISTER(reg));
#endif
          interpreter_return_result(oh, i, 0, SSA_REGISTER(reg));
#ifdef PRINT_DEBUG_OPCODES
          printf("in function: \n");
          print_type(oh, (struct Object*)i->method);
#endif
          break;
        }
      case OP_RETURN_FROM:
        {
          word_t reg, offset;
          PRINTOP("op: return from\n");
          reg = SSA_NEXT_PARAM_SMALLINT;
          offset = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("return result reg: %ld, offset: %ld, value: ", reg, offset);
          print_type(oh, SSA_REGISTER(reg));
#endif
          interpreter_return_result(oh, i, offset, SSA_REGISTER(reg));
#ifdef PRINT_DEBUG_OPCODES
          printf("in function: \n");
          print_type(oh, (struct Object*)i->method);
#endif
          break;
        }
      case OP_RETURN_VALUE:
        {
          struct Object* obj;
          PRINTOP("op: return obj\n");
          obj = SSA_NEXT_PARAM_OBJECT;
          interpreter_return_result(oh, i, 0, obj);
#ifdef PRINT_DEBUG_OPCODES
          printf("in function: \n");
          print_type(oh, (struct Object*)i->method);
#endif
          break;
        }
      case OP_ALLOCATE_REGISTERS:
        {
          word_t reg;
          reg = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("allocate %ld registers\n", reg);
#endif
          interpreter_stack_allocate(oh, i, reg);
#ifdef PRINT_DEBUG_OPCODES
          printf("new fp: %ld sp: %ld\n", i->framePointer, i->stackPointer);
#endif
          
          break;
        }

      default:
        printf("error bad opcode... %ld\n", op>>1);
        assert(0);
        break;
      }
      

    }
  } while (interpreter_return_result(oh, oh->cached.interpreter, 0, NULL));



}


int main(int argc, char** argv) {

  FILE* file;
  struct slate_image_header sih;
  struct object_heap* heap;
  word_t memory_limit = 400 * 1024 * 1024;
  word_t young_limit = 5 * 1024 * 1024;
  size_t res;

  heap = calloc(1, sizeof(struct object_heap));

  if (argc > 2) {
    fprintf(stderr, "You must supply an image file as an argument\n");
    return 1;
  }
  if (argc == 1) {
    file = fopen("slate.image", "r");
  } else {
    file = fopen(argv[1], "r");
  }
  if (file == NULL) {fprintf(stderr, "Open file failed (%d)\n", errno); return 1;}

  fread(&sih.magic, sizeof(sih.magic), 1, file);
  fread(&sih.size, sizeof(sih.size), 1, file);
  fread(&sih.next_hash, sizeof(sih.next_hash), 1, file);
  fread(&sih.special_objects_oop, sizeof(sih.special_objects_oop), 1, file);
  fread(&sih.current_dispatch_id, sizeof(sih.current_dispatch_id), 1, file);
  
  if (sih.magic != SLATE_IMAGE_MAGIC) {
    fprintf(stderr, "Magic number (0x%lX) doesn't match (word_t)0xABCDEF43\n", sih.magic);
    return 1;
  }
  

  if (!heap_initialize(heap, sih.size, memory_limit, young_limit, sih.next_hash, sih.special_objects_oop, sih.current_dispatch_id)) return 1;

  printf("Image size: %ld bytes\n", sih.size);
  if ((res = fread(heap->memoryOld, 1, sih.size, file)) != sih.size) {
    fprintf(stderr, "Error fread()ing image. Got %lu, expected %lu.\n", res, sih.size);
    return 1;
  }

  adjust_oop_pointers_from(heap, (word_t)heap->memoryOld, heap->memoryOld, heap->memoryOldSize);
  heap->stackBottom = &heap;
  heap->argcSaved = argc;
  heap->argvSaved = argv;
  interpret(heap);
  
  gc_close(heap);

  fclose(file);

  return 0;
}
