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


- gc


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
  unsigned char elements[];
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
  struct ByteArray * code;
  word_t sourceTree;
  word_t debugMap;
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
  unsigned char elements[];
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
  struct Object* delegation_stack[DELEGATION_STACK_SIZE];
  struct MethodCacheEntry methodCache[METHOD_CACHE_SIZE];
  struct Object* fixedObjects[MAX_FIXEDS];


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




void (*primitives[]) (struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts);

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
#define FUNCTION_FRAME_SIZE 4

#define TRUE 1
#define FALSE 0

#define TYPE_OBJECT 0
#define TYPE_OOP_ARRAY  1
#define TYPE_BYTE_ARRAY 2

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

byte_t byte_array_set_element(struct Object* o, word_t i, byte_t val) {
  return byte_array_elements((struct ByteArray*)o)[i] = val;
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


word_t interpreter_decode_immediate(struct Interpreter* i) {

  word_t code;
  word_t val;
  word_t n;
  
  n = i -> codePointer;
  code = (((i -> method) -> code) -> elements)[n];
  val = code & 127;
  while (code >= 128)
  {
    n = n + 1;
    code = (((i -> method) -> code) -> elements)[n];
    val = (val << 7) | (code & 127);
  }
  i -> codePointer = n + 1;
  
  return val;
}


short int interpreter_decode_short(struct Interpreter* i) {

  word_t n;
  
  n = i -> codePointer;
  i -> codePointer = n + 2;
  
  return (short int) (i->method->code->elements[n] | (i->method->code->elements[n + 1] << 8));
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


/***************** FILES *************************/
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



void method_flush_cache(struct object_heap* oh, struct Symbol* selector) {
  struct MethodCacheEntry* cacheEntry;
  if (selector == (struct Symbol*)oh->cached.nil || selector == NULL) {
    fill_bytes_with((byte_t*)oh->methodCache, METHOD_CACHE_SIZE*sizeof(struct MethodCacheEntry), 0);
  } else {
    word_t i;
    for (i = 0; i < METHOD_CACHE_SIZE; i++) {
      if ((cacheEntry = &oh->methodCache[i])->selector == selector) cacheEntry->selector = (struct Symbol*)oh->cached.nil;
    }
  }
}


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


bool object_is_free(struct object_heap* heap, struct Object* o) {

  return (object_markbit(o) != heap->mark_color || object_hash(o) >= ID_HASH_RESERVED);
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
  fill_words_with((word_t*)(obj+1), words-sizeof(struct Object), 0);
  return obj;
}

struct Object* heap_make_used_space(struct object_heap* oh, struct Object* obj, word_t words) {
  struct Object* x = heap_make_free_space(oh, obj, words);
  object_set_idhash(obj, heap_new_hash(oh));
  return x;
}


bool heap_initialize(struct object_heap* oh, word_t size, word_t limit, word_t young_limit, word_t next_hash, word_t special_oop, word_t cdid) {
  oh->memoryOldLimit = limit;
  oh->memoryYoungLimit = young_limit;

  oh->memoryOldSize = size;
  oh->memoryYoungSize = young_limit;

  oh->memoryOld = (byte_t*)mmap((void*)0x1000000, limit, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|0x20, -1, 0);
  oh->memoryYoung = (byte_t*)mmap((void*)0x2000000, young_limit, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|0x20, -1, 0);
  oh->pinnedYoungObjects = calloc(1, (oh->memoryYoungSize / PINNED_CARD_SIZE + 1) * sizeof(word_t));

  /*perror("err: ");*/
  if (oh->memoryOld == NULL || oh->memoryOld == (void*)-1
      || oh->memoryYoung == NULL || oh->memoryYoung == (void*)-1) {
    fprintf(stderr, "Initial GC allocation of memory failed.\n");
    return 0;
  }

  oh->nextFree = (struct Object*)oh->memoryYoung;
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
  assert(oh->markStack != NULL);
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
    if (object_is_free(oh, obj) && object_total_size(obj) >= bytes) return obj;
    obj = object_after(oh, obj);
  }
  return NULL;
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
      assert(0);
    }

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



void heap_free_object(struct object_heap* oh, struct Object* obj) {
#if 0
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
#if 0
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

  while (object_in_memory(oh, obj, memory, memorySize)) {
    if (object_markbit(obj) != oh->mark_color) {
      heap_free_object(oh, obj);
    }
    if (object_is_free(oh, obj) && object_is_free(oh, prev) && obj != prev) {
      heap_make_free_space(oh, prev, object_word_size(obj)+object_word_size(prev));
      obj = object_after(oh, prev);
    } else {
      prev = obj;
      obj = object_after(oh, obj);
    }
    
  }

}

void heap_unmark_all(struct object_heap* oh, byte_t* memory, word_t memorySize) {
  struct Object* obj = (struct Object*) memory;

  while (object_in_memory(oh, obj, memory, memorySize)) {
    object_unmark(oh, obj);
    obj = object_after(oh, obj);
  }

}

void heap_what_points_to_in(struct object_heap* oh, struct Object* x, byte_t* memory, word_t memorySize) {
  struct Object* obj = (struct Object*) memory;

  while (object_in_memory(oh, obj, memory, memorySize)) {
    word_t offset, limit;
    offset = object_first_slot_offset(obj);
    limit = object_last_oop_offset(obj) + sizeof(word_t);
    for (; offset != limit; offset += sizeof(word_t)) {
      struct Object* val = object_slot_value_at_offset(obj, offset);
      if (val == x) {
        print_object(obj);
        break;
      }
    }
    obj = object_after(oh, obj);
  }

}

void heap_what_points_to(struct object_heap* oh, struct Object* x) {
  
  heap_what_points_to_in(oh, x, oh->memoryOld, oh->memoryOldSize);
  heap_what_points_to_in(oh, x, oh->memoryYoung, oh->memoryYoungSize);

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
    } else if (object_is_free(oh, obj)) {
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
    if (object_is_free(oh, o)) goto next;
    while (object_hash((struct Object*)o->map) == ID_HASH_FORWARDED) {
      o->map = (struct Map*)((struct ForwardedObject*)(o->map))->target;
    }
    offset = object_first_slot_offset(o);
    limit = object_last_oop_offset(o) + sizeof(word_t);
    for (; offset != limit; offset += sizeof(word_t)) {
      struct Object* val = object_slot_value_at_offset(o, offset);
      while (!object_is_smallint(val) && object_hash(val) == ID_HASH_FORWARDED) {
        object_slot_value_at_offset_put(oh, o, offset, (val=((struct ForwardedObject*)val)->target));
      }
    }
  next:
    o = object_after(oh, o);
  }


}

void heap_tenure(struct object_heap* oh) {
  /*bring all marked young objects to old generation*/
  word_t tenure_count = 0, tenure_words = 0;
  struct Object* tenure_start = (struct Object*) (oh->memoryOld + oh->memoryOldSize);
  struct Object* obj = (struct Object*) oh->memoryYoung;
  struct Object* prev;
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {
    /*if it's still there in the young section, move it to the old section */
    if (!object_is_free(oh, obj) && !object_is_pinned(oh, obj)) {
      assert((word_t)tenure_start + object_total_size(obj) - (word_t) oh->memoryOld < oh->memoryOldLimit);
      tenure_count++;
      tenure_words += object_word_size(obj);
      copy_words_into((word_t*)obj, object_word_size(obj), (word_t*) tenure_start);
      ((struct ForwardedObject*) obj)->target = tenure_start;
      object_set_idhash(obj, ID_HASH_FORWARDED);
      tenure_start = object_after(oh, tenure_start);
    }
    obj = object_after(oh, obj);
  }
  oh->memoryOldSize += tenure_words * sizeof(word_t);
#ifdef PRINT_DEBUG_GC
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
    if (object_is_free(oh, obj) && object_is_free(oh, prev) && obj != prev) {
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
    if (object_is_free(oh, obj) && !object_is_pinned(oh, obj)) {
      young_count++;
      young_word_count += object_word_size(obj);
      if (object_hash(prev) == ID_HASH_FREE && obj != prev) {
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
#ifdef PRINT_DEBUG_GC
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
    if (!object_is_free(oh, o)) {
      object_forward_pointers_to(oh, o, x, y);
    }

    o = object_after(oh, o);
  }


}

void heap_forward(struct object_heap* oh, struct Object* x, struct Object* y) {

  heap_free_object(oh, x);
  heap_forward_from(oh, x, y, oh->memoryOld, oh->memoryOldSize);
  heap_forward_from(oh, x, y, oh->memoryYoung, oh->memoryYoungSize);
}

void heap_store_into(struct object_heap* oh, struct Object* src, struct Object* dest) {
  /*  print_object(dest);*/
  if (!object_is_smallint(dest)) {
    assert(object_hash(dest) < ID_HASH_RESERVED); /*catch gc bugs earlier*/
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
  assert(!object_is_free(oh, o));
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

void interpreter_grow_stack(struct object_heap* oh, struct Interpreter* i) {

  struct OopArray * newStack;
  
  /* i suppose this isn't treated as a SmallInt type*/
  i -> stackSize *= 2;
  newStack = (struct OopArray *) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), i->stackSize);
  copy_words_into((word_t*) i->stack->elements, i->stackPointer, (word_t*) newStack->elements);
  
  i -> stack = newStack;

}

void interpreter_stack_allocate(struct object_heap* oh, struct Interpreter* i, word_t n) {

  if (i->stackPointer + n > i->stackSize) {
    interpreter_grow_stack(oh, i);
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
    interpreter_grow_stack(oh, i);
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
  print_stack_types(oh, 200);
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
  map->roleTable = role_table_grow_excluding(oh, roles, matches, method);
  heap_store_into(oh, (struct Object*)map, (struct Object*)map->roleTable);
  object_represent(oh, obj, map);

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

void interpreter_apply_to_arity_with_optionals(struct object_heap* oh, struct Interpreter * i, struct Closure * closure,
                                               struct Object* args[], word_t n, struct OopArray* opts);


void interpreter_signal(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* args[], word_t n, struct OopArray* opts) {

  struct Closure* method;
  struct Symbol* selector = (struct Symbol*)signal;
  struct MethodDefinition* def = method_dispatch_on(oh, selector, args, n, NULL);
  if (def == NULL) {
    unhandled_signal(oh, selector, n, args);
  }
  /*take this out when the debugger is mature*/
  /*assert(0);*/
  method = (struct Closure*)def->method;
  interpreter_apply_to_arity_with_optionals(oh, i, method, args, n, opts);
}

void interpreter_signal_with(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* arg, struct OopArray* opts) {

  struct Object* args[1];
  args[0] = arg;
  interpreter_signal(oh, i, signal, args, 1, opts);
}


void interpreter_signal_with_with(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* arg, struct Object* arg2, struct OopArray* opts) {
  struct Object* args[2];
  args[0] = arg;
  args[1] = arg2;
  interpreter_signal(oh, i, signal, args, 2, opts);
}

void interpreter_signal_with_with_with(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* arg, struct Object* arg2, struct Object* arg3, struct OopArray* opts) {
  struct Object* args[3];
  args[0] = arg;
  args[1] = arg2;
  args[2] = arg3;
  interpreter_signal(oh, i, signal, args, 3, opts);
}


void prim_fixme(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {

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

void interpreter_push_nil(struct object_heap* oh, struct Interpreter * i) {
  interpreter_stack_push(oh, i, oh->cached.nil);
}
void interpreter_push_false(struct object_heap* oh, struct Interpreter * i) {
  interpreter_stack_push(oh, i, oh->cached.false);
}
void interpreter_push_true(struct object_heap* oh, struct Interpreter * i) {
  interpreter_stack_push(oh, i, oh->cached.true);
}

void interpreter_dispatch_optionals(struct object_heap* oh, struct Interpreter * i, struct OopArray* opts) {

  word_t index, size = array_size(opts);
  for (index = 0; index < size; index += 2) {
    interpreter_dispatch_optional_keyword(oh, i, opts->elements[index], opts->elements[index+1]);
  }

}


void interpreter_apply_to_arity_with_optionals(struct object_heap* oh, struct Interpreter * i, struct Closure * closure,
                                               struct Object* argsNotStack[], word_t n, struct OopArray* opts) {


  word_t inputs, framePointer;
  struct Object** vars;
  struct LexicalContext* lexicalContext;
  struct CompiledMethod* method;
  struct Object* args[16];
  

#ifdef PRINT_DEBUG
  printf("apply to arity %ld\n", n);
#endif

  method = closure->method;
  inputs = object_to_smallint(method->inputVariables);

  /*make sure they are pinned*/
  assert(n <= 16);
  copy_words_into((word_t*)argsNotStack, n, (word_t*)args);
  
  if (n < inputs || (n > inputs && method->restVariable != oh->cached.true)) {
    struct OopArray* argsArray = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
    copy_words_into((word_t*) args, n, (word_t*)argsArray->elements);
    interpreter_signal_with_with(oh, i, get_special(oh, SPECIAL_OOP_WRONG_INPUTS_TO), (struct Object*) argsArray, (struct Object*)method, NULL);
    return;
  }

  framePointer = i->stackPointer + FUNCTION_FRAME_SIZE;

  if (method->heapAllocate == oh->cached.true) {
    lexicalContext = (struct LexicalContext*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_LEXICAL_CONTEXT_PROTO), object_to_smallint(method->localVariables));
    lexicalContext->framePointer = smallint_to_object(framePointer);
    interpreter_stack_allocate(oh, i, FUNCTION_FRAME_SIZE);
    vars = &lexicalContext->variables[0];
  } else {
    lexicalContext = (struct LexicalContext*) oh->cached.nil;
    interpreter_stack_allocate(oh, i, FUNCTION_FRAME_SIZE /*frame size in words*/ + object_to_smallint(method->localVariables));
    vars = &i->stack->elements[framePointer];
  }


  copy_words_into((word_t*) args, inputs, (word_t*) vars);
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
  i->codeSize = object_byte_size((struct Object*)method->code) - sizeof(struct ByteArray);
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



void prim_write_to_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  struct Object *console=args[0], *n=args[1], *handle=args[2], *seq=args[3], *start=args[4];
  byte_t* bytes = &((struct ByteArray*)seq)->elements[0] + object_to_smallint(start);
  word_t size = object_to_smallint(n);

  assert(arity == 5 && console != NULL);

  interpreter_stack_push(oh, oh->cached.interpreter, 
                         smallint_to_object(fwrite(bytes, 1, size, (object_to_smallint(handle) == 0)? stdout : stderr)));

}


void prim_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle = object_to_smallint(args[1]);
  closeFile(oh, handle);
  interpreter_push_nil(oh, oh->cached.interpreter);

}

void prim_readConsole_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t /*handle = object_to_smallint(args[2]),*/ n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;

  retval = fread((char*)(byte_array_elements(bytes) + start), 1, n, stdin);
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(retval));

}

void prim_read_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle = object_to_smallint(args[2]), n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;

  retval = readFile(oh, handle, n, (char*)(byte_array_elements(bytes) + start));
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(retval));
}

void prim_write_to_from_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle = object_to_smallint(args[2]), n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;
  retval = writeFile(oh, handle, n, (char*)(byte_array_elements(bytes) + start));
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(retval));
}


void prim_reposition_to(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle = object_to_smallint(args[1]), n = object_to_smallint(args[2]);
  word_t retval = seekFile(oh, handle, n);
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(retval));
}

void prim_positionOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle = object_to_smallint(args[1]);
  word_t retval = tellFile(oh, handle);
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(retval));
}


void prim_atEndOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle = object_to_smallint(args[1]);
  if (endOfFile(oh, handle)) {
    interpreter_push_true(oh, oh->cached.interpreter);
  } else {
    interpreter_push_false(oh, oh->cached.interpreter);
  }
}

void prim_sizeOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle = object_to_smallint(args[1]);
  word_t retval = sizeOfFile(oh, handle);
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(retval));
}



void prim_flush_output(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  /*struct Object *console=args[0];*/
  fflush(stdout);
  fflush(stderr);
  interpreter_push_nil(oh, oh->cached.interpreter);
}

void prim_handle_for(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle;
  struct Object /**file=args[0],*/ *fname=args[1];

  handle = openFile(oh, (struct ByteArray*)fname, SF_READ|SF_WRITE);
  if (handle >= 0) {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(handle));
  } else {
    interpreter_push_nil(oh, oh->cached.interpreter);
  }

}

void prim_handleForNew(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle;
  struct Object /**file=args[0],*/ *fname=args[1];

  handle = openFile(oh, (struct ByteArray*)fname, SF_READ|SF_WRITE|SF_CLEAR|SF_CREATE);
  if (handle >= 0) {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(handle));
  } else {
    interpreter_push_nil(oh, oh->cached.interpreter);
  }

}


void prim_handle_for_input(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts) {
  word_t handle;
  struct Object /**file=args[0],*/ *fname=args[1];

  handle = openFile(oh, (struct ByteArray*)fname, SF_READ);
  if (handle >= 0) {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(handle));
  } else {
    interpreter_push_nil(oh, oh->cached.interpreter);
  }

}


void prim_smallint_at_slot_named(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj;
  struct Object* name;
  struct SlotEntry * se;
  struct Object * proto;
  
  obj = args[0];
  name = args[1];
  proto = get_special(oh, SPECIAL_OOP_SMALL_INT_PROTO);
  se = slot_table_entry_for_name(oh, proto->map->slotTable, (struct Symbol*)name);
  if (se == NULL) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL);
  } else {
    word_t offset = object_to_smallint(se->offset);
    interpreter_stack_push(oh, oh->cached.interpreter, object_slot_value_at_offset(proto, offset));
  }


}



void prim_bytesize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(payload_size(args[0])));
}

void prim_findon(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct MethodDefinition* def;
  struct Symbol* selector= (struct Symbol*) args[0];
  struct OopArray* arguments= (struct OopArray*) args[1];


  def = method_dispatch_on(oh, selector, arguments->elements, array_size(arguments), NULL);

  interpreter_stack_push(oh, oh->cached.interpreter, (def == NULL ? oh->cached.nil : (struct Object*) def->method));
}



void prim_ensure(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {

  struct Closure* body = (struct Closure*) args[0];
  struct Object* ensureHandler = args[1];

  interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, body, NULL, 0, NULL);
  interpreter_stack_push(oh, oh->cached.interpreter, oh->cached.interpreter->ensureHandlers);
  interpreter_stack_push(oh, oh->cached.interpreter, ensureHandler);
  oh->cached.interpreter->ensureHandlers = smallint_to_object(oh->cached.interpreter->stackPointer - 2);

}


void prim_frame_pointer_of(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {

  struct Interpreter* i = (struct Interpreter*)args[0];
  struct Symbol* selector = (struct Symbol*) args[1];
  struct CompiledMethod* method;
  word_t frame = i->framePointer;



  while (frame > FUNCTION_FRAME_SIZE) {
    method = (struct CompiledMethod*) i->stack->elements[frame-3];
    if (method->selector == selector) {
      interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(frame));
      return;
    }
    frame = object_to_smallint(i->stack->elements[frame-1]);
  }

  interpreter_push_nil(oh, oh->cached.interpreter);

}


void prim_bytearray_newsize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj, *i;
  obj = args[0];
  i = args[1];
  
  if (!object_is_smallint(i)) {
    interpreter_push_nil(oh, oh->cached.interpreter);
    return;
  }

  interpreter_stack_push(oh, oh->cached.interpreter,
                         (struct Object*)heap_clone_byte_array_sized(oh, obj, (object_to_smallint(i) < 0) ? 0 : object_to_smallint(i)));
}


void prim_byteat_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj= args[0], *i=args[1], *val = args[2];
  word_t index;

  index = object_to_smallint(i);

  if (object_is_immutable(obj)) {
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), obj, NULL);
    return;
  }
  
  if (index < object_byte_size(obj)) {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(byte_array_set_element(obj, index, object_to_smallint(val))));
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, obj, NULL);
  }

}


void prim_byteat(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj, *i;
  word_t index;

  obj = args[0];
  i = args[1];
  index = object_to_smallint(i);
  
  if (index < object_byte_size(obj)) {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(byte_array_get_element(obj, index)));
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, obj, NULL);
  }

}


void prim_map(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj;
  obj = args[0];
  
  if (object_is_smallint(obj)) {
    interpreter_push_nil(oh, oh->cached.interpreter);
  } else {
    interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)obj->map);
  }


}

void prim_set_map(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj;
  struct Map* map;
  obj = args[0];
  map = (struct Map*)args[1];
  
  if (object_is_smallint(obj) || object_is_immutable(obj)) {
    interpreter_push_nil(oh, oh->cached.interpreter);
  } else {
    object_change_map(oh, obj, map);
    heap_store_into(oh, args[0], args[1]);
    interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)map);
  }

}

void prim_exit(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  print_stack_types(oh, 128);
  print_backtrace(oh);
  exit(0);
}

void prim_identity_hash(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  /*fix*/
  /*  print_detail(oh, args[0]);
      print_backtrace(oh);*/
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(object_hash(args[0])));
}

void prim_identity_hash_univ(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  /*fix*/
  /*  print_detail(oh, args[0]);
      print_backtrace(oh);*/
  if (object_is_smallint(args[0])) {
    interpreter_stack_push(oh, oh->cached.interpreter, args[0]);
  } else {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(object_hash(args[0])));
  }
}


void prim_clone(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  interpreter_stack_push(oh, oh->cached.interpreter, heap_clone(oh, args[0]));
}

void prim_applyto(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals) {
  struct Closure *method = (struct Closure*)args[0];
  struct OopArray* argArray = (struct OopArray*) args[1];
  struct OopArray* opts = NULL;

  if (optionals != NULL && optionals->elements[1] != oh->cached.nil) {
    opts = (struct OopArray*) optionals->elements[1];
  }

  interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, method,
                                            argArray->elements, array_size(argArray), opts);


}


void prim_at(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* array;
  word_t i;

  array = args[0];
  i = object_to_smallint(args[1]);
  
  if (i < object_array_size(array)) {
    interpreter_stack_push(oh, oh->cached.interpreter, ((struct OopArray*)array)->elements[i]);
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[1], args[0], NULL);

  }


}


void prim_at_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object *array = args[0], *i = args[1], *val = args[2];
  word_t index = object_to_smallint(i);

  if (object_is_immutable(array)) {
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), array, NULL);
    return;
  }
  
  if (index < object_array_size(array)) {
    heap_store_into(oh, array, val);
    interpreter_stack_push(oh, oh->cached.interpreter, ((struct OopArray*)array)->elements[index] = val);
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, array, NULL);
  }
}


void prim_ooparray_newsize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  /*struct Object* array = args[0];*/
  struct Object* i = args[1];
  if (object_is_smallint(i)) {
    interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)
                           heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO),
                                                      object_to_smallint(i)));
  } else {
    interpreter_stack_push(oh, oh->cached.interpreter, oh->cached.nil);
  }
}


void prim_equals(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  interpreter_stack_push(oh, oh->cached.interpreter, (args[0] == args[1])?oh->cached.true:oh->cached.false);
}

void prim_less_than(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  interpreter_stack_push(oh, oh->cached.interpreter,
 (object_to_smallint(args[0])<object_to_smallint(args[1]))?oh->cached.true:oh->cached.false);
}


void prim_size(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(object_array_size(args[0])));
}

void prim_bitand(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)((word_t)args[0] & (word_t)args[1]));
}
void prim_bitor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)((word_t)args[0] | (word_t)args[1]));
}
void prim_bitxor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)(((word_t)args[0] ^ (word_t)args[1])|SMALLINT_MASK));
}
void prim_bitnot(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)(~((word_t)args[0]) | SMALLINT_MASK));
}


void prim_plus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  word_t z = object_to_smallint(x) + object_to_smallint(y);
  if (smallint_fits_object(z)) {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(z));
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_ADD_OVERFLOW), x, y, NULL);
  }
}

void prim_removefrom(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {

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
    interpreter_stack_push(oh, oh->cached.interpreter, method);
    return;
  }

  for (i = 0; i < array_size(roles); i++) {
    struct Object* role = array_elements(roles)[i];
    if (!object_is_smallint(role)) {
      object_remove_role(oh, role, selector, def);
    }
  }

  method_flush_cache(oh, selector);
  interpreter_stack_push(oh, oh->cached.interpreter, method);

}


void prim_exponent(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray* x = (struct ByteArray*)args[0];
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(((word_t)*float_part(x) >> FLOAT_EXPONENT_OFFSET) & 0xFF));

}

void prim_significand(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray* x = (struct ByteArray*)args[0];
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object((word_t)(*float_part(x)) & FLOAT_SIGNIFICAND));

}

void prim_getcwd(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray* buf = (struct ByteArray*)args[1];
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(getCurrentDirectory(buf)));
}
void prim_setcwd(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray* buf = (struct ByteArray*)args[1];
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(setCurrentDirectory(buf)));
}


void prim_float_equals(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  if (*float_part(x) == *float_part(y)) {
    interpreter_push_true(oh, oh->cached.interpreter);
  } else {
    interpreter_push_false(oh, oh->cached.interpreter);
  }
}

void prim_float_less_than(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  if (*float_part(x) < *float_part(y)) {
    interpreter_push_true(oh, oh->cached.interpreter);
  } else {
    interpreter_push_false(oh, oh->cached.interpreter);
  }
}

void prim_float_plus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) + *float_part(y);
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)z);
}

void prim_float_minus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) - *float_part(y);
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)z);
}


void prim_float_times(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) * *float_part(y);
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)z);
}

void prim_float_divide(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) / *float_part(y);
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)z);
}

void prim_float_raisedTo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = pow(*float_part(x), *float_part(y));
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)z);
}

void prim_float_ln(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = log(*float_part(x));
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)z);
}

void prim_float_exp(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = exp(*float_part(x));
  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)z);
}

void prim_withSignificand_exponent(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  /*this is really a bytearray*/
  word_t significand = object_to_smallint(args[1]), exponent = object_to_smallint(args[2]);
  struct ByteArray* f = heap_new_float(oh);
  *((word_t*)float_part(f)) = significand | exponent << FLOAT_EXPONENT_OFFSET;

  interpreter_stack_push(oh, oh->cached.interpreter, (struct Object*)f);

}

void prim_bitshift(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  word_t bits = object_to_smallint(args[0]);
  word_t shift = object_to_smallint(args[1]);
  word_t z;
  if (shift >= 0) {
    if (shift >= 32 && bits != 0) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_BIT_SHIFT_OVERFLOW), args[0], args[1], NULL);
      return;
    }

    z = bits << shift;

    if (!smallint_fits_object(z) || z >> shift != bits) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_BIT_SHIFT_OVERFLOW), args[0], args[1], NULL);
      return;
    }

  } else if (shift <= -32) {
    z = bits >> 31;
  } else {
    z = bits >> -shift;
  }
  
  interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(z));

}


void prim_minus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  word_t z = object_to_smallint(x) - object_to_smallint(y);
  if (smallint_fits_object(z)) {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(z));
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SUBTRACT_OVERFLOW), x, y, NULL);
  }
}


void prim_times(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  word_t z = object_to_smallint(x) * object_to_smallint(y);
  if (z > 1073741823 || z < -1073741824) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_MULTIPLY_OVERFLOW), x, y, NULL);
  } else {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(z));
  }
}

void prim_quo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  if (object_to_smallint(y) == 0) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_DIVIDE_BY_ZERO), x, y, NULL);
  } else {
    interpreter_stack_push(oh, oh->cached.interpreter, smallint_to_object(object_to_smallint(x) / object_to_smallint(y)));
  }
}


void prim_forward_to(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  interpreter_stack_push(oh, oh->cached.interpreter, y);

  if (!object_is_smallint(x) && !object_is_smallint(y) && x != y) {
    heap_forward(oh, x, y);
    /*heap_full_gc(oh); fix? needed?*/
  }

}


void prim_clone_setting_slots(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object *obj = args[0], *slotArray = args[1], *valueArray = args[2], *newObj;
  word_t i;

  if (object_is_smallint(obj)) {
    interpreter_stack_push(oh, oh->cached.interpreter, obj);
    return;
  }
  newObj = heap_clone(oh, obj);

  /*fix, check that arrays are same size, and signal errors*/

  for (i = 0; i < object_array_size(slotArray); i++) {
    struct Symbol* name = (struct Symbol*)object_array_get_element(slotArray, i);
    struct SlotEntry* se = slot_table_entry_for_name(oh, obj->map->slotTable, name);
    if (se == NULL) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL);
    } else {
      /*since the object was just cloned, we aren't expecting a tenured obj to point to a new one*/
      object_slot_value_at_offset_put(oh, newObj, object_to_smallint(se->offset), object_array_get_element(valueArray, i));
    }
  }
  
  interpreter_stack_push(oh, oh->cached.interpreter, newObj);
}



void prim_as_method_on(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
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

  interpreter_stack_push(oh, oh->cached.interpreter, method);
}


void prim_at_slot_named(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj;
  struct Object* name;
  struct SlotEntry * se;
  
  obj = args[0];
  name = args[1];
  
  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL);
  } else {
    se = slot_table_entry_for_name(oh, obj->map->slotTable, (struct Symbol*)name);
    if (se == NULL) {
      interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL);
    } else {
      word_t offset = object_to_smallint(se->offset);
      interpreter_stack_push(oh, oh->cached.interpreter, object_slot_value_at_offset(obj, offset));
    }
  }


}


void prim_at_slot_named_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj=args[0], *val=args[2];
  struct Object* name = args[1];
  struct SlotEntry * se;
  struct Map* map;
  
  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL);
    return;
  }

  if (object_is_immutable(obj)) {
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), obj, NULL);
    return;
  }

  map = obj->map;
  se = slot_table_entry_for_name(oh, map->slotTable, (struct Symbol*)name);

  if (se == NULL) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL);
  } else {
    word_t offset = object_to_smallint(se->offset);
    interpreter_stack_push(oh, oh->cached.interpreter, object_slot_value_at_offset_put(oh, obj, offset, val));
  }

 /*note: not supporting delegate slots*/

}




void prim_clone_with_slot_valued(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj = args[0], *value = args[2];
  struct Symbol* name = (struct Symbol*)args[1];

  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL);
  } else {
    interpreter_stack_push(oh, oh->cached.interpreter, object_add_slot_named(oh, obj, name, value));
  }
}

void prim_clone_without_slot(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj = args[0];
  struct Symbol* name = (struct Symbol*)args[1];

  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL);
  } else {
    interpreter_stack_push(oh, oh->cached.interpreter, object_remove_slot(oh, obj, name));
  }
}

/* 
 * Args are the actual arguments to the function. dispatchers are the
 * objects we find the dispatch function on. Usually they should be
 * the same.
 */

void send_to_through_arity_with_optionals(struct object_heap* oh,
                                                  struct Symbol* selector, struct Object* args[],
                                                  struct Object* dispatchers[], word_t arity, struct OopArray* opts) {
  struct OopArray* argsArray;
  struct Closure* method;
  struct Object* traitsWindow;
  struct Object* argsStack[16];
  struct Object* dispatchersStack[16];
  /*make sure they are pinned*/
  assert(arity <= 16);
  /*for gc*/
  copy_words_into((word_t*)args, arity, (word_t*)argsStack);
  copy_words_into((word_t*)dispatchers, arity, (word_t*)dispatchersStack);

  struct MethodDefinition* def = method_dispatch_on(oh, selector, dispatchers, arity, NULL);

  if (def == NULL) {
    argsArray = (struct OopArray*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), arity);
    copy_words_into((word_t*)dispatchers, arity, (word_t*)&argsArray->elements[0]);
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_NOT_FOUND_ON), (struct Object*)selector, (struct Object*)argsArray, NULL);
    return;
  }

  method = (struct Closure*)def->method;
  traitsWindow = method->base.map->delegates->elements[0]; /*fix should this location be hardcoded as the first element?*/
  if (traitsWindow == oh->cached.primitive_method_window) {
#ifdef PRINT_DEBUG
    printf("calling primitive: %ld\n", object_to_smallint(((struct PrimitiveMethod*)method)->index));
#endif
    primitives[object_to_smallint(((struct PrimitiveMethod*)method)->index)](oh, args, arity, opts);
  } else if (traitsWindow == oh->cached.compiled_method_window || traitsWindow == oh->cached.closure_method_window) {
    interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, method, args, arity, opts);
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
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_APPLY_TO), def->method, (struct Object*)argsArray, optsArray);
  }

}

void send_message_with_optionals(struct object_heap* oh, struct Interpreter* i, word_t n, struct OopArray* opts) {

  struct Object** args;
  struct Symbol* selector;

#ifdef PRINT_DEBUG_FUNCALL
      printf("send_message_with_optionals BEFORE\n");
      printf("stack pointer: %ld\n", i->stackPointer);
      printf("frame pointer: %ld\n", i->framePointer);
      print_stack_types(oh, 16);
#endif


  i->stackPointer = i->stackPointer - n;
  args = &i->stack->elements[i->stackPointer];
  selector = (struct Symbol*)interpreter_stack_pop(oh, i);
  send_to_through_arity_with_optionals(oh, selector, args, args, n, opts);

#ifdef PRINT_DEBUG_FUNCALL
      printf("send_message_with_optionals AFTER\n");
      printf("stack pointer: %ld\n", i->stackPointer);
      printf("frame pointer: %ld\n", i->framePointer);
      print_stack_types(oh, 16);
#endif


}



void prim_send_to(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals) {
  struct Symbol* selector = (struct Symbol*)args[0];
  struct OopArray* opts, *arguments = (struct OopArray*)args[1];

  if (optionals == NULL) {
    opts = NULL;
  } else {
    opts = (struct OopArray*)optionals->elements[1];
    if (opts == (struct OopArray*)oh->cached.nil) opts = NULL;
  }
  send_to_through_arity_with_optionals(oh, selector, array_elements(arguments), array_elements(arguments), array_size(arguments), opts); 
}

void prim_send_to_through(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals) {
  struct Symbol* selector = (struct Symbol*)args[0];
  struct OopArray* opts, * arguments = (struct OopArray*)args[1], *dispatchers = (struct OopArray*)args[2];

  if (optionals == NULL) {
    opts = NULL;
  } else {
    opts = (struct OopArray*)optionals->elements[1];
    if (opts == (struct OopArray*)oh->cached.nil) opts = NULL;
  }
  /*fix check array sizes are the same*/
  send_to_through_arity_with_optionals(oh, selector, array_elements(arguments), array_elements(dispatchers), array_size(arguments), opts); 
}




void prim_as_accessor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
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
  interpreter_stack_push(oh, oh->cached.interpreter, method);

}


void (*primitives[]) (struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) = {

 /*0-9*/ prim_as_method_on, prim_as_accessor, prim_map, prim_set_map, prim_fixme, prim_removefrom, prim_clone, prim_clone_setting_slots, prim_clone_with_slot_valued, prim_fixme, 
 /*10-9*/ prim_fixme, prim_fixme, prim_clone_without_slot, prim_at_slot_named, prim_smallint_at_slot_named, prim_at_slot_named_put, prim_forward_to, prim_bytearray_newsize, prim_bytesize, prim_byteat, 
 /*20-9*/ prim_byteat_put, prim_ooparray_newsize, prim_size, prim_at, prim_at_put, prim_ensure, prim_applyto, prim_send_to, prim_send_to_through, prim_findon, 
 /*30-9*/ prim_fixme, prim_fixme, prim_exit, prim_fixme, prim_identity_hash, prim_identity_hash_univ, prim_equals, prim_less_than, prim_bitor, prim_bitand, 
 /*40-9*/ prim_bitxor, prim_bitnot, prim_bitshift, prim_plus, prim_minus, prim_times, prim_quo, prim_fixme, prim_fixme, prim_frame_pointer_of, 
 /*50-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 
 /*60-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_readConsole_from_into_starting_at, prim_write_to_starting_at, prim_flush_output, prim_handle_for, prim_handle_for_input, prim_fixme, 
 /*70-9*/ prim_handleForNew, prim_close, prim_read_from_into_starting_at, prim_write_to_from_starting_at, prim_reposition_to, prim_positionOf, prim_atEndOf, prim_sizeOf, prim_fixme, prim_fixme, 
 /*80-9*/ prim_fixme, prim_fixme, prim_getcwd, prim_setcwd, prim_significand, prim_exponent, prim_withSignificand_exponent, prim_float_equals, prim_float_less_than, prim_float_plus, 
 /*90-9*/ prim_float_minus, prim_float_times, prim_float_divide, prim_float_raisedTo, prim_float_ln, prim_float_exp, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 
 /*10-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,
 /*20-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,
 /*30-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,

};



bool interpreter_return_result(struct object_heap* oh, struct Interpreter* i, word_t context_depth, bool hasResult) {
  /* Implements a non-local return with a value, specifying the block to return
     from via lexical offset. Returns a success Boolean. */
  struct Object* result = oh->cached.nil;
  word_t framePointer;
  word_t ensureHandlers;

#ifdef PRINT_DEBUG
  printf("return result depth: %ld, has result: %ld\n", context_depth, hasResult);
#endif
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
                                   (struct Object*)i->closure, (struct Object*) targetContext, NULL);
      return 1;
    }
  }
  ensureHandlers = object_to_smallint(i->ensureHandlers);
  if (framePointer <= ensureHandlers) {
    struct Object* ensureHandler = i->stack->elements[ensureHandlers+1];
    i->ensureHandlers = i->stack->elements[ensureHandlers];
    interpreter_stack_push(oh, i, smallint_to_object(i->codePointer));
    interpreter_stack_push(oh, i, get_special(oh, SPECIAL_OOP_ENSURE_MARKER));
    interpreter_stack_push(oh, i, oh->cached.nil);
    interpreter_stack_push(oh, i, smallint_to_object(i->framePointer));
    i->codePointer = 0;
    i->framePointer = i->stackPointer;
    interpreter_apply_to_arity_with_optionals(oh, i, (struct Closure*) ensureHandler, NULL, 0, NULL);
    return 1;
  }
  if (hasResult) {
    result = interpreter_stack_pop(oh, i);
  }
  i->stackPointer = framePointer - FUNCTION_FRAME_SIZE;
  i->framePointer = object_to_smallint(i->stack->elements[framePointer - 1]);
  if (i->framePointer < FUNCTION_FRAME_SIZE) {
    return 0;
  }
  i->codePointer = object_to_smallint(i->stack->elements[framePointer - 4]);
  i->lexicalContext = (struct LexicalContext*) i->stack->elements[i->framePointer - 2];
  i->closure = (struct Closure*) i->stack->elements[i->framePointer - 3];
  heap_store_into(oh, (struct Object*)i, (struct Object*)i->lexicalContext);
  heap_store_into(oh, (struct Object*)i, (struct Object*)i->closure);
  i->method = i->closure->method;
  heap_store_into(oh, (struct Object*)i, (struct Object*)i->closure->method);

  i->codeSize = object_byte_size((struct Object*)i->method->code) - sizeof(struct ByteArray);

#ifdef PRINT_DEBUG_CODE_POINTER
  printf("code pointer: %ld/%ld\n", i->codePointer, i->codeSize);
#endif


  if (hasResult) {
    interpreter_stack_push(oh, i, result);
  }

#ifdef PRINT_DEBUG_FUNCALL
      printf("interpreter_return_result AFTER\n");
      printf("stack pointer: %ld\n", i->stackPointer);
      printf("frame pointer: %ld\n", i->framePointer);
      print_stack_types(oh, 16);
#endif


  return 1;

}




void interpreter_load_variable(struct object_heap* oh, struct Interpreter* i, word_t n) {
  if (i->method->heapAllocate == oh->cached.true) {
    interpreter_stack_push(oh, i, i->lexicalContext->variables[n]);
  } else {
    interpreter_stack_push(oh, i, i->stack->elements[i->framePointer+n]);
  }
}


void interpreter_store_variable(struct object_heap* oh, struct Interpreter* i, word_t n) {
  if (i->method->heapAllocate == oh->cached.true) {
    heap_store_into(oh, (struct Object*)i->lexicalContext, i->stack->elements[i->stackPointer - 1]);
    i->lexicalContext->variables[n] = i->stack->elements[i->stackPointer - 1];
  } else {
    i->stack->elements[i->framePointer+n] = i->stack->elements[i->stackPointer - 1];
  }
}

void interpreter_store_free_variable(struct object_heap* oh, struct Interpreter * i, word_t n)
{
  heap_store_into(oh, (struct Object*)i->closure->lexicalWindow[n-1], i->stack->elements[i->stackPointer - 1]);
  i->closure->lexicalWindow[n-1]->variables[interpreter_decode_immediate(i)] = i->stack->elements[i->stackPointer - 1];
}

void interpreter_load_free_variable(struct object_heap* oh, struct Interpreter * i, word_t n)
{
  interpreter_stack_push(oh, i, i->closure->lexicalWindow[n-1]->variables[interpreter_decode_immediate(i)]);
}



void interpreter_load_literal(struct object_heap* oh, struct Interpreter * i, word_t n)
{
  interpreter_stack_push(oh, i, i->method->literals->elements[n]);
}

void interpreter_load_selector(struct object_heap* oh, struct Interpreter * i, word_t n)
{
  interpreter_stack_push(oh, i, i->method->selectors->elements[n]);
}


void interpreter_new_closure(struct object_heap* oh, struct Interpreter * i, word_t n)
{
  struct Closure * newClosure;
  
  if ((struct CompiledMethod *) i->closure == i->method) {
    newClosure = (struct Closure *) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_CLOSURE_PROTO), 1);
  } else {
    word_t inheritedSize;
    
    inheritedSize = object_array_size((struct Object *) i->closure);
    newClosure = (struct Closure *) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_CLOSURE_PROTO), inheritedSize+1);
    copy_words_into((word_t *) i->closure->lexicalWindow, inheritedSize, (word_t*) newClosure->lexicalWindow + 1);
  }
  newClosure->lexicalWindow[0] = i->lexicalContext;
  newClosure->method = (struct CompiledMethod *) i->method->literals->elements[n];
  heap_store_into(oh, (struct Object*)newClosure, (struct Object*)i->lexicalContext);
  heap_store_into(oh, (struct Object*)newClosure, i->method->literals->elements[n]);
  interpreter_stack_push(oh, i, (struct Object*)newClosure);
}

void interpreter_push_integer(struct object_heap* oh, struct Interpreter * i, word_t integer)
{
  interpreter_stack_push(oh, i, smallint_to_object(integer));

}

void interpreter_jump_to(struct object_heap* oh, struct Interpreter * i)
{
  short int offset;
  offset = interpreter_decode_short(i);
#ifdef PRINT_DEBUG
  printf("increment code pointer by %ld\n", (word_t)offset);
#endif
  i->codePointer = i->codePointer + offset;
}

void interpreter_branch_if_true(struct object_heap* oh, struct Interpreter * i)
{

  short int offset;
  struct Object* condition;
  
  offset = interpreter_decode_short(i);
  condition = interpreter_stack_pop(oh, i);
  if (condition == oh->cached.true) {
    i->codePointer = i->codePointer + offset;
  } else {
    if (!(condition == oh->cached.false)) {
      i->codePointer = i->codePointer - 3;
      interpreter_signal_with(oh, i, get_special(oh, SPECIAL_OOP_NOT_A_BOOLEAN), condition, NULL);
    }
  }

}

/*fix rewrite this so it doesn't copy+paste?*/
void interpreter_branch_if_false(struct object_heap* oh, struct Interpreter * i)
{

  short int offset;
  struct Object* condition;
  
  offset = interpreter_decode_short(i);
  condition = interpreter_stack_pop(oh, i);
  if (condition == oh->cached.false) {
    i->codePointer = i->codePointer + offset;
  } else {
    if (!(condition == oh->cached.true)) {
      i->codePointer = i->codePointer - 3;
      interpreter_signal_with(oh, i, get_special(oh, SPECIAL_OOP_NOT_A_BOOLEAN), condition, NULL);
    }
  }

}

void interpreter_resend_message(struct object_heap* oh, struct Interpreter* i, word_t n) {

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
    assert((struct Object*)lexicalContext != i->stack->elements[framePointer-2]);
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
                                      (struct Object*)selector, (struct Object*)argsArray, (struct Object*) resender, NULL);
    return;

  }

  method = (struct Closure*)def->method;
  traitsWindow = method->base.map->delegates->elements[0];
  if (traitsWindow == oh->cached.primitive_method_window) {
#ifdef PRINT_DEBUG
    printf("calling primitive: %ld\n", object_to_smallint(((struct PrimitiveMethod*)method)->index));
#endif
    primitives[object_to_smallint(((struct PrimitiveMethod*)method)->index)](oh, args, n, NULL);
    return;
  }

  if (traitsWindow == oh->cached.compiled_method_window || traitsWindow == oh->cached.closure_method_window) {
    struct OopArray* optKeys = resender->optionalKeywords;
    interpreter_apply_to_arity_with_optionals(oh, i, method, args, n, NULL);
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
                                 def->method, (struct Object*) argsArray, signalOpts);
    
  }


}



void interpreter_push_environment(struct object_heap* oh, struct Interpreter * i) {
  interpreter_stack_push(oh, i, i->method->environment);
}

void interpreter_is_identical_to(struct object_heap* oh, struct Interpreter * i) {
  if (interpreter_stack_pop(oh, i) == interpreter_stack_pop(oh, i))
    interpreter_stack_push(oh, i, oh->cached.true);
  else
    interpreter_stack_push(oh, i, oh->cached.false);
}

void interpreter_branch_keyed(struct object_heap* oh, struct Interpreter * i, word_t n) {

  word_t tableSize;
  word_t hash;
  struct OopArray * table;
  struct Object* oop;
  word_t index;
  
  table = (struct OopArray *) i->method->literals->elements[n];
  oop = interpreter_stack_pop(oh, i);
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

void interpreter_new_array(struct object_heap* oh, struct Interpreter * i, word_t n)
{
  struct OopArray * arr;
  word_t stackPointer;
  
  if (n == 0)
  {
    interpreter_stack_push(oh, i, get_special(oh, SPECIAL_OOP_ARRAY_PROTO));
    return;
  }
  arr = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
  stackPointer = i->stackPointer - n;
  i->stackPointer = stackPointer;
  /*fix: check elements[0] location*/
  copy_words_into((word_t *) (&((i->stack)->elements)[stackPointer]), n, (word_t*)&(arr->elements[0]));
  interpreter_stack_push(oh, i, (struct Object *) arr);
}


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
    

    while (oh->cached.interpreter->codePointer < oh->cached.interpreter->codeSize) {
      word_t op, val, prevPointer;
      struct Interpreter* i = oh->cached.interpreter; /*it won't move while we are in here */

      if (oh->interrupt_flag) {
        return;
      }
#ifdef PRINT_DEBUG_CODE_POINTER
      printf("code pointer: %ld/%ld\n", i->codePointer, i->codeSize);
#endif
      
      instruction_counter++;
      op = i->method->code->elements[i->codePointer];
      prevPointer = i->codePointer;
      i->codePointer++;
      val = op >> 4;
      if (val == 15) {
        val = interpreter_decode_immediate(i);
      }

#ifdef PRINT_DEBUG_INSTRUCTION_COUNT
      printf("instruction counter: %lu\n", instruction_counter);
#endif

#ifdef PRINT_DEBUG_STACK_POINTER
      printf("stack pointer: %ld\n", i->stackPointer);
      printf("frame pointer: %ld\n", i->framePointer);
      printf("opcode %d\n", (int)op&15);
      /*printf("-------------STACK--------------------\n");
      print_stack(oh);
      printf("-------------ENDSTACK--------------------\n");*/
#endif
      switch (op & 15) {

      case 0:
        PRINTOP("op: send message with optionals\n");
        send_message_with_optionals(oh, i, val, (struct OopArray *) 0);
        break;
      case 1:
        PRINTOP("op: load var\n");
        interpreter_load_variable(oh, i, val);
        break;
      case 2:
        PRINTOP("op: store var\n");
        interpreter_store_variable(oh, i, val);
        break;
      case 3:
        PRINTOP("op: load free var\n");
        interpreter_load_free_variable(oh, i, val);
        break;
      case 4:
        PRINTOP("op: store free var\n");
        interpreter_store_free_variable(oh, i, val);
        break;
      case 5:
        PRINTOP("op: load literal\n");
        interpreter_load_literal(oh, i, val);
        break;
      case 6:
        PRINTOP("op: load selector\n");
        interpreter_load_selector(oh, i, val);
        break;
      case 7:
        PRINTOP("op: stack pop\n");
        interpreter_stack_pop_amount(oh, i, val);
        break;
      case 8:
        PRINTOP("op: new array\n");
        interpreter_new_array(oh, i, val);
        break;
      case 9:
        PRINTOP("op: new closure\n");
        interpreter_new_closure(oh, i, val);
        break;
      case 10:
        PRINTOP("op: branch keyed\n");
        interpreter_branch_keyed(oh, i, val);
        break;
      case 11:
        PRINTOP("op: send message with optionals (stack pop)\n");
        send_message_with_optionals(oh, i, val, (struct OopArray*)interpreter_stack_pop(oh, i));
        break;
      case 12:
        
        {
          PRINTOP("op: return result\n");

          i->codePointer = prevPointer;
          interpreter_return_result(oh, i, val, TRUE);
        }
        break;
      case 13:
        PRINTOP("op: push int\n");
        interpreter_push_integer(oh, i, val);
        break;
      case 14:
        PRINTOP("op: resend message\n");
        interpreter_resend_message(oh, i, val);
        break;
      case 15:

#ifdef PRINT_DEBUG
      printf("op %d\n", (int)op);
#endif

        switch (op)
          {
          case 15:
            PRINTOP("op: jump to\n");
            interpreter_jump_to(oh, i);
            break;
          case 31:
            PRINTOP("op: branch if true\n");
            interpreter_branch_if_true(oh, i);
            break;
          case 47:
            PRINTOP("op: branch if false\n");
            interpreter_branch_if_false(oh, i);
            break;
          case 63:
            PRINTOP("op: push env\n");
            interpreter_push_environment(oh, i);
            break;
          case 79:
            PRINTOP("op: push nil\n");
            interpreter_push_nil(oh, i);
            break;
          case 95:
            PRINTOP("op: is identical to\n");
            interpreter_is_identical_to(oh, i);
            break;
          case 111:
            PRINTOP("op: push true\n");
            interpreter_push_true(oh, i);
            break;
          case 127:
            PRINTOP("op: push false\n");
            interpreter_push_false(oh, i);
            break;
          case 143:
            PRINTOP("op: return result\n");
            interpreter_return_result(oh, i, (word_t) 0, FALSE);
            break;
          }
        break;
      }
      

    }
  } while (interpreter_return_result(oh, oh->cached.interpreter, 0, TRUE));



}

void adjust_fields_by(struct object_heap* oh, struct Object* o, word_t shift_amount) {

  word_t offset, limit;
  o->map = (struct Map*) inc_ptr(o->map, shift_amount);
  offset = object_first_slot_offset(o);
  limit = object_last_oop_offset(o) + sizeof(word_t);
  for (; offset != limit; offset += sizeof(word_t)) {
    struct Object* val = object_slot_value_at_offset(o, offset);
    if (!object_is_smallint(val)) {
      object_slot_value_at_offset_put(oh, o, offset, (struct Object*)inc_ptr(val, shift_amount));
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
    if (!object_is_free(oh, o)) {
      adjust_fields_by(oh, o, shift_amount);
    }
    o = object_after(oh, o);
  }


}


int main(int argc, char** argv) {

  FILE* file;
  struct slate_image_header sih;
  struct object_heap* heap;
  word_t memory_limit = 400 * 1024 * 1024;
  word_t young_limit = 10 * 1024 * 1024;
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
  
  if (sih.magic != (word_t)0xABCDEF43) {
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
  interpret(heap);
  
  gc_close(heap);

  fclose(file);

  return 0;
}
