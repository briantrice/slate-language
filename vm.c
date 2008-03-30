/*
 * Copyright 2008 Timmy Douglas
 * New VM written in C (rather than pidgin/slang/etc) for slate
 * Based on original by Lee Salzman and Brian Rice
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
typedef word_t bool;

#define WORDT_MAX INTPTR_MAX

struct SlotTable;
struct Symbol;
struct CompiledMethod;
struct LexicalContext;
struct RoleTable;
struct OopArray;
struct ObjectPayload;
struct MethodDefinition;
struct Object;
struct ByteArray;
struct MethodCacheEntry;
struct Closure;
struct Root;
struct Interpreter;
struct ObjectHeader;
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

struct ObjectPayload
{
  word_t header;
};
struct MethodCacheEntry
{
  struct MethodDefinition * method;
  struct Object* selector;
  struct Map * maps[3];
};
struct ObjectHeader
{
  word_t header;
};
struct ForwardedObject
{
  struct ObjectHeader header;
  struct Object * target;
};
struct Map
{
  struct ObjectHeader header;
  struct Map * map;
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
  struct ObjectHeader header;
  struct Map * map;
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
  struct ObjectHeader header;
  struct Map * map;
  struct SlotEntry slots[];
};
struct Symbol
{
  struct ObjectHeader header;
  struct Map * map;
  struct Object* cacheMask;
  unsigned char elements[];
};
struct CompiledMethod
{
  struct ObjectHeader header;
  struct Map * map;
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
  struct ObjectHeader header;
  struct Map * map;
  struct Object* framePointer;
  struct Object* variables[];
};
struct RoleTable
{
  struct ObjectHeader header;
  struct Map * map;
  struct RoleEntry roles[];
};
struct OopArray
{
  struct ObjectHeader header;
  struct Map * map;
  struct Object* elements[];
};
struct MethodDefinition
{
  struct ObjectHeader header;
  struct Map * map;
  struct Object* method;
  struct Object* slotAccessor;
  word_t dispatchID;
  word_t dispatchPositions;
  word_t foundPositions;
  word_t dispatchRank;
};
struct Object
{
  struct ObjectHeader header;
  struct Map * map;
};
struct ByteArray
{
  struct ObjectHeader header;
  struct Map * map;
  unsigned char elements[];
};
struct Closure
{
  struct ObjectHeader header;
  struct Map * map;
  struct CompiledMethod * method;
  struct LexicalContext * lexicalWindow[];
};
struct Root
{
  struct ObjectHeader header;
  struct Map * map;
};
struct Interpreter /*note the bottom fields are treated as contents in a bytearray so they don't need to be converted from objects to get the actual smallint value*/
{
  struct ObjectHeader header;
  struct Map * map;
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

#define DELEGATION_STACK_SIZE 256
#define METHOD_CACHE_SIZE 1024

/*these things never exist in slate land (so word_t types are their actual value)*/
struct object_heap
{
  byte_t mark_color;
  byte_t * memory;
  word_t memory_size;
  word_t memory_limit;
  struct OopArray* special_objects_oop; /*root for gc*/
  word_t current_dispatch_id;
  bool interrupt_flag;
  word_t next_hash;
  struct Object* delegation_stack[DELEGATION_STACK_SIZE];
  struct MethodCacheEntry methodCache[METHOD_CACHE_SIZE];
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

#define OBJECT_SIZE_MASK 0x3F
bool oop_is_object(word_t xxx)   { return ((((word_t)xxx)&1) == 0); }
bool oop_is_smallint(word_t xxx) { return ((((word_t)xxx)&1) == 1);}
bool object_is_smallint(struct Object* xxx) { return ((((word_t)xxx)&1) == 1);}
word_t object_mark(struct Object* xxx)      { return  (((xxx)->header.header)&1); }
word_t object_hash(struct Object* xxx)       { return  ((((xxx)->header.header)>>1)&0x7FFFFF);}
word_t object_size(struct Object* xxx)       {return   ((((xxx)->header.header)>>24)&OBJECT_SIZE_MASK);}
word_t payload_size(struct Object* xxx) {return ((((xxx)->header.header))&0x3FFFFFFF);}
word_t object_type(struct Object* xxx)     {return     ((((xxx)->header.header)>>30)&0x3);}

void object_set_format(struct Object* xxx, word_t type) {
  xxx->header.header &= ~(3<<30);
  xxx->header.header |= (type&3) << 30;
}
void object_set_size(struct Object* xxx, word_t size) {
  xxx->header.header &= ~(0x3F<<24);
  xxx->header.header |= (size&0x3F) << 24;
}
void object_set_idhash(struct Object* xxx, word_t hash) {
  xxx->header.header &= ~(0x7FFFFF<<1);
  xxx->header.header |= (hash&0x7FFFFF) << 1;
}
void objectpayload_set_size(struct ObjectPayload* xxx, word_t size) {
  xxx->header &= ~(0x3FFFFFFF);
  xxx->header |= (size&0x3FFFFFFF);
}


/*fix see if it can be post incremented*/
word_t heap_new_hash(struct object_heap* oh) { return ++oh->next_hash;}



/* DETAILS: This trick is from Tim Rowledge. Use a shift and XOR to set the 
 * sign bit if and only if the top two bits of the given value are the same, 
 * then test the sign bit. Note that the top two bits are equal for exactly 
 * those integers in the range that can be represented in 31-bits.
*/
word_t smallint_fits_object(word_t i) {   return (i ^ (i << 1)) >= 0;}
/*fix i didn't understand the above*/

word_t object_to_smallint(struct Object* xxx)  {return ((((word_t)xxx)>>1)); }
struct Object* smallint_to_object(word_t xxx) {return ((struct Object*)(((xxx)<<1)|1)); }

#define inc_ptr(xxx, yyy)     ((byte_t*)xxx + yyy)

#define ID_HASH_RESERVED 0x7FFFF0
#define ID_HASH_FORWARDED ID_HASH_RESERVED
#define ID_HASH_FREE 0x7FFFFF


/*obj map flags is a smallint oop, so we start after the smallint flag*/
#define MAP_FLAG_RESTRICT_DELEGATION 2
#define MAP_FLAG_IMMUTABLE 4


#define HEADER_SIZE (sizeof(word_t))
#define HEADER_SIZE_WORDS (HEADER_SIZE/sizeof(word_t))
#define WORD_BYTES_MINUS_ONE (sizeof(word_t)-1)
#define ROLE_ENTRY_WORD_SIZE ((sizeof(struct RoleEntry) + WORD_BYTES_MINUS_ONE) / sizeof(word_t))
#define SLOT_ENTRY_WORD_SIZE ((sizeof(struct SlotEntry) + WORD_BYTES_MINUS_ONE) / sizeof(word_t))
#define FUNCTION_FRAME_SIZE 4

#define TRUE 1
#define FALSE 0

#define TYPE_OBJECT 0
#define TYPE_OOP_ARRAY  1
#define TYPE_BYTE_ARRAY 2
#define TYPE_PAYLOAD 3

#define METHOD_CACHE_ARITY 3

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


#ifdef PRINT_DEBUG_OPCODES
#define PRINTOP(X) printf(X)
#else
#define PRINTOP(X)
#endif



void print_object(struct Object* oop) {
  if (oop == NULL) {
    printf("<object NULL>\n");
    return;
  }
  if (oop_is_smallint((word_t)oop)) {
    printf("<object int_value: %ld>\n", object_to_smallint(oop));
  } else {
    char* typestr;
    switch (object_type(oop)) {
    case TYPE_OBJECT: typestr = "normal"; break;
    case TYPE_OOP_ARRAY: typestr = "oop array"; break;
    case TYPE_BYTE_ARRAY: typestr = "byte array"; break;
    case TYPE_PAYLOAD: typestr = "payload"; break;
    }
    printf("<object at %p, hash: 0x%lX, size: %ld, type: %s>\n", (void*)oop, object_hash(oop), object_size(oop), typestr);
  }

}


struct Object* get_special(struct object_heap* oh, word_t special_index) {
  return oh->special_objects_oop->elements[special_index];
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

word_t object_payload_size(struct Object* o) {

  /*in memory there is a payload header before the object*/
  o = (struct Object*) inc_ptr(o, -HEADER_SIZE);
  return payload_size(o);

}

word_t object_is_immutable(struct Object* o) {return ((word_t)o->map->flags & MAP_FLAG_IMMUTABLE) != 0; }

bool object_in_memory(struct object_heap* heap, struct Object* oop) {

  return (heap->memory <= (byte_t*)oop && (word_t)heap->memory + heap->memory_size > (word_t)oop);

}

struct Object* drop_payload(struct Object* o) {
  if (object_type(o) == TYPE_PAYLOAD) {
    return (struct Object*)inc_ptr(o, HEADER_SIZE);
  }
  return o;
}

struct Object* first_object(struct object_heap* heap) {

  struct Object* o = (struct Object*)heap->memory;
  
  return drop_payload(o);
}


word_t object_word_size(struct Object* o) {

  if (object_type(o) == TYPE_OBJECT) {
    return object_size(o);
  } 
  return object_size(o) + (object_payload_size(o) + sizeof(word_t) - 1) / sizeof(word_t); 

}

word_t object_array_offset(struct Object* o) {
  return object_size((struct Object*)o) * sizeof(word_t);
}

word_t object_array_size(struct Object* o) {

  assert(object_type(o) != TYPE_OBJECT);
  return (object_payload_size(o) + sizeof(word_t) - 1) / sizeof(word_t);

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
  return object_array_offset(o) + object_payload_size(o);

}

word_t object_total_size(struct Object* o) {
  /*IMPORTANT: rounds up to word size*/

  return object_word_size(o)*sizeof(word_t);

}

word_t object_first_slot_offset(struct Object* o) {

  return HEADER_SIZE + sizeof(struct Map*);

}

word_t object_last_slot_offset(struct Object* o) {
  return object_size(o) * sizeof(word_t) - sizeof(word_t);
}


word_t object_last_oop_offset(struct Object* o) {

  if (object_type(o) == TYPE_OOP_ARRAY) {
    return object_last_slot_offset(o) + object_payload_size(o);
  }
  return object_last_slot_offset(o);
}

struct Object* object_slot_value_at_offset(struct Object* o, word_t offset) {

  return (struct Object*)*((word_t*)inc_ptr(o, offset));

}

void object_slot_value_at_offset_put(struct Object* o, word_t offset, struct Object* value) {

  (*((word_t*)inc_ptr(o, offset))) = (word_t)value;

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

void print_symbol(struct Symbol* name) {
  fwrite(&name->elements[0], 1, object_payload_size((struct Object*)name), stdout);
}

void indent(word_t amount) { word_t i; for (i=0; i<amount; i++) printf("    "); }


void print_byte_array(struct Object* o) {
  word_t i;
  char* elements = (char*)inc_ptr(o, object_array_offset(o));
  printf("'");
  for (i=0; i < object_payload_size(o); i++) {
    if (elements[i] >= 32 && elements[i] <= 126) {
      printf("%c", elements[i]);
    } else {
      printf("\\x%02lx", (word_t)elements[i]);
    }
    if (i > 10 && object_payload_size(o) - i > 100) {
      i = object_payload_size(o) - 20;
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

  if (object_type(o) != TYPE_PAYLOAD) {
    struct SlotTable* slotTable = map->slotTable;
    word_t limit = object_to_smallint(map->slotCount);
    indent(depth); printf("slot count: %ld\n", limit);
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

void print_type(struct object_heap* oh, struct Object* o) {
  word_t i;
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
  x = traitsWindow->map->delegates;
  if (!x || array_size(x) < 1) goto fail;

  traits = x->elements[array_size(x)-1];

  {
    struct Map* map = traits->map;
    struct SlotTable* slotTable = map->slotTable;
    word_t limit = object_to_smallint(map->slotCount);
    for (i=0; i < limit; i++) {
      if (slotTable->slots[i].name == (struct Symbol*)oh->cached.nil) continue;
      if (strncmp((char*)slotTable->slots[i].name->elements, "printName", 9) == 0)
      {
        struct Object* obj = object_slot_value_at_offset(traits, object_to_smallint(slotTable->slots[i].offset));
        if (object_is_smallint(obj) || object_type(obj) != TYPE_BYTE_ARRAY) {
          break;
        } else {
          print_byte_array(obj);
          printf("\n");
          return;
        }
      }
    }
  }  

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




bool heap_initialize(struct object_heap* oh, word_t size, word_t limit, word_t next_hash, word_t special_oop, word_t cdid) {
  oh->memory_limit = limit;
  /*oh->memory = malloc(limit);*/
  oh->memory = (byte_t*)mmap((void*)0x1000000, 30*1024*1024, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|0x20, -1, 0);
  perror("err: ");
  if (oh->memory == NULL || oh->memory == (void*)-1) {
    fprintf(stderr, "Initial GC allocation of memory failed.\n");
    return 0;
  }
  oh->special_objects_oop = (struct OopArray*)((byte_t*)oh->memory + special_oop);
  oh->next_hash = next_hash;
  oh->memory_size = size;
  oh->current_dispatch_id = cdid;
  oh->interrupt_flag = 0;
  oh->mark_color = 1;

  return 1;
}

void gc_close(struct object_heap* oh) {

  free(oh->memory);
}

word_t* gc_allocate(struct object_heap* oh, word_t bytes) {

  /*FIX!!!!!!!!*/
#if 1
  assert(bytes % sizeof(word_t) == 0);
  assert(oh->memory_size + bytes < oh->memory_limit);
  word_t* res = (word_t*)((byte_t*)oh->memory + oh->memory_size);
  oh->memory_size += bytes;
  return res;
#else
  return malloc(bytes);
#endif
}

void heap_forward(struct object_heap* oh, struct Object* x, struct Object* y) {


}

void heap_force_gc(struct object_heap* oh) {


}



struct Object* heap_allocate(struct object_heap* oh, word_t words) {

  return (struct Object*)gc_allocate(oh, words * sizeof(word_t));

}


struct Object* heap_allocate_with_payload(struct object_heap* oh, word_t words, word_t payload_size) {

  struct Object* o;
  word_t size = words*sizeof(word_t) + sizeof(struct ObjectPayload) + \
    ((payload_size + sizeof(word_t) - 1) & ~(sizeof(word_t) - 1)); /*word aligned payload*/
  struct ObjectPayload* op = (struct ObjectPayload*)gc_allocate(oh, size);

  object_set_format((struct Object*)op, TYPE_PAYLOAD);
  objectpayload_set_size(op, payload_size);
  o = (struct Object*)(op + 1);
  /*gc_mark(oh, o); fix*/
  object_set_size(o, words);
  return o;
}


struct Object* heap_clone(struct object_heap* oh, struct Object* proto) {
  struct Object* newObj;
  
  if (object_type(proto) == TYPE_OBJECT) {
    newObj = heap_allocate(oh, object_size(proto));
  } else {
    newObj = heap_allocate_with_payload(oh, object_size(proto), object_payload_size(proto));
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
  printf("stack allocate, new stack pointer: %ld\n", i->stackPointer);

}

void interpreter_stack_push(struct object_heap* oh, struct Interpreter* i, struct Object* value) {

#ifdef PRINT_DEBUG_STACK_PUSH
  printf("Stack push at %ld (fp=%ld): ", i->stackPointer, i->framePointer); print_type(oh, value);
#endif
  if (i->stackPointer == i->stackSize) {
    interpreter_grow_stack(oh, i);
    /* important if the interpreter somehow moved?*/
    i = oh->cached.interpreter;
  }

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

void interpreter_signal(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* args[], word_t arity, struct OopArray* opts) {

  assert(0);
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



void method_save_cache(struct object_heap* oh, struct MethodDefinition* md, struct Symbol* name, struct Object* arguments[], word_t arity) {
  /*fix*/

}

void method_flush_cache(struct object_heap* oh, struct Symbol* name) {
  /*fix*/

}

struct MethodDefinition* method_check_cache(struct object_heap* oh, struct Symbol* name, struct Object* arguments[], word_t arity) {
  /*fix*/
  return NULL;
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
    if (slotName != (struct Symbol*)oh->cached.nil && slotName == excluding) {
      struct SlotEntry * slot = slot_table_entry_for_inserting_name(oh, newSlots, slotName);
      *slot = slots->slots[i];
    }
    
  }

  return newSlots;

}

void slot_table_relocate_by(struct object_heap* oh, struct SlotTable* slots, word_t offset, word_t amount) {

  word_t i;

  for (i = 0; i < slot_table_capacity(slots); i++) {
    if (slots->slots[i].name != (struct Symbol*)oh->cached.nil) {
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
  obj->map = map;
}

void object_represent(struct object_heap* oh, struct Object* obj, struct Map* map) {
  object_change_map(oh, obj, map);
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
#if 0
   role_table_entry_for_inserting_name(oh, map->roleTable, name);
  chain = role_table_entry_for_name(oh, map->roleTable, name);
  if (chain != NULL) {
    while (chain->nextRole != oh->cached.nil) {
      chain = & map->roleTable->roles[object_to_smallint(chain->nextRole)];
    }
    chain->nextRole = smallint_as_object((word_t)entry - (word_t)map->roleTable->roles);
  }
#endif
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
  /*fix root stack push map*/
  map->slotTable = slot_table_grow_excluding(oh, map->slotTable, 1, (struct Symbol*)oh->cached.nil);
  slot_table_relocate_by(oh, map->slotTable, offset, sizeof(word_t));
  entry = slot_table_entry_for_inserting_name(oh, map->slotTable, name);
  entry->name = name;
  entry->offset = smallint_to_object(offset);
  /*if it's not big enough or the max size, we have to turn it into an oop array*/
  if (object_size(obj) == OBJECT_SIZE_MASK) {
    newObj = heap_allocate_with_payload(oh, OBJECT_SIZE_MASK, 
                                        (object_type(obj) == TYPE_OBJECT? sizeof(word_t): object_payload_size(obj) + sizeof(word_t)));
    object_set_format(newObj, TYPE_OOP_ARRAY);

  } else {

    if (object_type(obj) == TYPE_OBJECT) {
      newObj = heap_allocate(oh, object_size(obj) + 1);
    } else {
      newObj = heap_allocate_with_payload(oh, object_size(obj) + 1, object_payload_size(obj));
    }
    object_set_format(newObj, object_type(obj));
  }

  /*root pop*/
  object_set_idhash(newObj, heap_new_hash(oh));
  copy_bytes_into((byte_t*)obj+ object_first_slot_offset(obj),
                  offset-object_first_slot_offset(obj),
                  (byte_t*)newObj + object_first_slot_offset(newObj));
  object_slot_value_at_offset_put(newObj, offset, value);

  copy_bytes_into((byte_t*)obj+offset, object_total_size(obj) - offset, (byte_t*)newObj + offset + sizeof(word_t));
  newObj->map = map;
  map->representative = newObj;

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
    printf("arguments[%ld] = ", i); print_type(oh, arguments[i]);
  }
  /*  printf("resend: "); print_object(resendMethod);*/
#endif

  dispatch = NULL;
  slotLocation = NULL;

  if (resendMethod == NULL) {
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

#ifdef PRINT_DEBUG
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
  copy_words_into((word_t*)args, n, (word_t*)argBuffer);
  oldDef = method_dispatch_on(oh, selector, argBuffer, n, NULL);
  if (oldDef == NULL || oldDef->dispatchPositions != positions || oldDef != method_is_on_arity(oh, oldDef->method, selector, args, n)) {
    oldDef = NULL;
  }
  /*fix: push root: <def> */
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
  /*root stack pop*/

  return def;
    
}



void interpreter_dispatch_optional_keyword(struct object_heap* oh, struct Interpreter * i, struct Object* key, struct Object* value) {

  struct OopArray* optKeys = i->method->optionalKeywords;

  word_t optKey, size = array_size(optKeys);

  for (optKey = 0; optKey < size; i++) {
    if (optKeys->elements[optKey] == key) {
      if (i->method->heapAllocate == oh->cached.true) {
        i->lexicalContext->variables[object_to_smallint(i->method->inputVariables) + optKey] = value;
      } else {
        i->stack->elements[i->framePointer + object_to_smallint(i->method->inputVariables) + optKey] = value;
      }
      return;
    }
  }

  /*fix*/
  assert(0);
}


void interpreter_dispatch_optionals(struct object_heap* oh, struct Interpreter * i, struct OopArray* opts) {

  word_t index, size = array_size(opts);
  for (index = 0; index < size; index += 2) {
    interpreter_dispatch_optional_keyword(oh, i, opts->elements[index], opts->elements[index+1]);
  }

}


void interpreter_apply_to_arity_with_optionals(struct object_heap* oh, struct Interpreter * i, struct Closure * closure,
                                               struct Object* args[], word_t n, struct OopArray* opts) {


  word_t inputs, framePointer;
  struct Object** vars;
  struct LexicalContext* lexicalContext;
  struct CompiledMethod* method;
  
#ifdef PRINT_DEBUG
  printf("apply to arity %ld\n", n);
#endif

  method = closure->method;
  inputs = object_to_smallint(method->inputVariables);
  
  if (n < inputs || (n > inputs && method->restVariable != oh->cached.nil)) {
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


#ifdef PRINT_DEBUG_FUNCALL
  print_stack_types(oh, 16);
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

void interpreter_push_nil(struct object_heap* oh, struct Interpreter * i) {
  interpreter_stack_push(oh, i, oh->cached.nil);
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
    interpreter_stack_push(oh, oh->cached.interpreter, ((struct OopArray*)array)->elements[index] = val);
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, array, NULL);
  }
}


void prim_newsize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
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
  interpreter_stack_push(oh, oh->cached.interpreter, (args[0]==args[1])?oh->cached.true:oh->cached.false);
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
    heap_force_gc(oh);
  }

}


void prim_as_method_on(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct MethodDefinition* def;
  struct Object *method = args[0], *roles=args[2];
  struct Symbol *selector = (struct Symbol*)args[1];
  struct Object* traitsWindow = method->map->delegates->elements[0];

   
  if (traitsWindow == get_special(oh, SPECIAL_OOP_CLOSURE_WINDOW)) {
    struct Closure* closure = (struct Closure*)heap_clone(oh, method);
    closure->method = (struct CompiledMethod*)heap_clone(oh, (struct Object*)closure->method);
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
#ifdef PRINT_DEBUG
  printf("Defining function '"); print_symbol(selector);
  printf("' on:\n");
  {
    word_t i;
    for (i = 0; i < object_array_size(roles); i++) {
      printf("arg[%ld] = ", i); print_type(oh, ((struct OopArray*)roles)->elements[i]);
    }
  }
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


void prim_clone_with_slot_valued(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) {
  struct Object* obj = args[0], *value = args[2];
  struct Symbol* name = (struct Symbol*)args[1];

  
  if (object_is_smallint(obj)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL);
  } else {
    interpreter_stack_push(oh, oh->cached.interpreter, object_add_slot_named(oh, obj, name, value));
  }


}


void (*primitives[]) (struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts) = {

 /*0-9*/ prim_as_method_on, prim_fixme, prim_map, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_clone_with_slot_valued, prim_fixme, 
 /*10-9*/ prim_fixme, prim_fixme, prim_fixme, prim_at_slot_named, prim_smallint_at_slot_named, prim_fixme, prim_forward_to, prim_fixme, prim_fixme, prim_fixme, 
 /*20-9*/ prim_fixme, prim_newsize, prim_size, prim_at, prim_at_put, prim_fixme, prim_applyto, prim_fixme, prim_fixme, prim_fixme, 
 /*30-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_equals, prim_fixme, prim_bitor, prim_bitand, 
 /*40-9*/ prim_fixme, prim_fixme, prim_fixme, prim_plus, prim_minus, prim_times, prim_quo, prim_fixme, prim_fixme, prim_fixme, 
 /*50-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 
 /*60-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_write_to_starting_at, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 
 /*70-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 
 /*80-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 

};

/* 
 * Args are the actual arguments to the function. dispatchers are the
 * objects we find the dispatch function on. Usually they should be
 * the same.
 */

void send_to_through_arity_with_optionals(struct object_heap* oh,
                                                  struct Object* selector, struct Object* args[],
                                                  struct Object* dispatchers[], word_t arity, struct OopArray* opts) {
  struct OopArray* argsArray;
  struct Closure* method;
  struct Object* traitsWindow;

  /*fix*/
  struct MethodDefinition* def = method_dispatch_on(oh, (struct Symbol*)selector, dispatchers, arity, NULL);

  if (def == NULL) {
    argsArray = (struct OopArray*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), arity);
    copy_words_into((word_t*)dispatchers, arity, (word_t*)&argsArray->elements[0]);
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_NOT_FOUND_ON), selector, (struct Object*)argsArray, NULL);
    return;
  }

  method = (struct Closure*)def->method;
  traitsWindow = method->map->delegates->elements[0]; /*fix should this location be hardcoded as the first element?*/
  if (traitsWindow == oh->cached.primitive_method_window) {
#ifdef PRINT_DEBUG
    printf("calling primitive: %ld\n", object_to_smallint(((struct PrimitiveMethod*)method)->index));
#endif
    primitives[object_to_smallint(((struct PrimitiveMethod*)method)->index)](oh, args, arity, opts);
  } else if (traitsWindow == oh->cached.compiled_method_window || traitsWindow == oh->cached.closure_method_window) {
    interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, method, args, arity, opts);
  } else {
    struct OopArray* optsArray = NULL;
    if (opts != NULL) {
      optsArray = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 2);
      optsArray->elements[0] = get_special(oh, SPECIAL_OOP_OPTIONALS);
      optsArray->elements[1] = (struct Object*)opts;
    }
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_APPLY_TO), def->method, (struct Object*)argsArray, optsArray);
  }

}

void send_message_with_optionals(struct object_heap* oh, struct Interpreter* i, word_t n, struct OopArray* opts) {

  struct Object** args;
  struct Object* selector;

#ifdef PRINT_DEBUG_FUNCALL
      printf("send_message_with_optionals BEFORE\n");
      printf("stack pointer: %ld\n", i->stackPointer);
      printf("frame pointer: %ld\n", i->framePointer);
      print_stack_types(oh, 16);
#endif


  i->stackPointer = i->stackPointer - n;
  args = &i->stack->elements[i->stackPointer];
  selector = interpreter_stack_pop(oh, i);
  send_to_through_arity_with_optionals(oh, selector, args, args, n, opts);

#ifdef PRINT_DEBUG_FUNCALL
      printf("send_message_with_optionals AFTER\n");
      printf("stack pointer: %ld\n", i->stackPointer);
      printf("frame pointer: %ld\n", i->framePointer);
      print_stack_types(oh, 16);
#endif


}



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
  i->method = i->closure->method;
  i->codeSize = object_byte_size((struct Object*)i->method->code) - sizeof(struct ByteArray);

#ifdef PRINT_DEBUG
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
    i->lexicalContext->variables[n] = i->stack->elements[i->stackPointer - 1];
  } else {
    i->stack->elements[i->framePointer+n] = i->stack->elements[i->stackPointer - 1];
  }
}

void interpreter_store_free_variable(struct object_heap* oh, struct Interpreter * i, word_t n)
{
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
  def = method_dispatch_on(oh, selector, args, n, barrier);
  if (def == NULL) {
    argsArray = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
    copy_words_into((word_t*)args, n, (word_t*)argsArray->elements);
    interpreter_signal_with_with_with(oh, i, get_special(oh, SPECIAL_OOP_NOT_FOUND_ON),
                                      (struct Object*)selector, (struct Object*)argsArray, (struct Object*) resender, NULL);
    return;

  }

  method = (struct Closure*)def->method;
  traitsWindow = method->map->delegates->elements[0];
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
    /* fix check that i is still in the same place in memory?*/
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
    argsArray = (struct OopArray*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
    copy_words_into((word_t*) args, n, (word_t*) argsArray->elements);
    
    interpreter_signal_with_with(oh, i, get_special(oh, SPECIAL_OOP_APPLY_TO),
                                 def->method, (struct Object*) argsArray, signalOpts);
    
  }


}



void interpreter_push_environment(struct object_heap* oh, struct Interpreter * i) {
  interpreter_stack_push(oh, i, i->method->environment);
}
void interpreter_push_false(struct object_heap* oh, struct Interpreter * i) {
  interpreter_stack_push(oh, i, oh->cached.false);
}
void interpreter_push_true(struct object_heap* oh, struct Interpreter * i) {
  interpreter_stack_push(oh, i, oh->cached.true);
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

#ifdef PRINT_DEBUG
  printf("Interpret: img:0x%p size:%ld spec:%p next:%ld\n",
         (void*)oh->memory, oh->memory_size, (void*)oh->special_objects_oop, oh->next_hash);
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
#ifdef PRINT_DEBUG
      printf("code pointer: %ld/%ld\n", i->codePointer, i->codeSize);
#endif
      
      op = i->method->code->elements[i->codePointer];
      prevPointer = i->codePointer;
      i->codePointer++;
      val = op >> 4;
      if (val == 15) {
        val = interpreter_decode_immediate(i);
      }
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
        interpreter_stack_pop(oh, i);
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


struct Object* object_after(struct object_heap* heap, struct Object* o) {
  /*print_object(o);*/
  assert(object_in_memory(heap, o) && object_total_size(o) != 0);

  o = (struct Object*)inc_ptr(o, object_total_size(o));
  if (!object_in_memory(heap, o)) return NULL;
  /*fix last allocated and next live*/
  o = drop_payload(o);
  return o;
}

bool object_is_free(struct object_heap* heap, struct Object* o) {

  return object_mark(o) != heap->mark_color || object_hash(o) >= ID_HASH_RESERVED;
}

void adjust_fields_by(struct object_heap* oh, struct Object* o, word_t shift_amount) {

  word_t offset, limit;
  o->map = (struct Map*) inc_ptr(o->map, shift_amount);
  /*fix do slots*/
  offset = object_first_slot_offset(o);
  limit = object_last_oop_offset(o) + sizeof(word_t);
  for (; offset != limit; offset += sizeof(word_t)) {
    struct Object* val = object_slot_value_at_offset(o, offset);
    if (!object_is_smallint(val)) {
      object_slot_value_at_offset_put(o, offset, (struct Object*)inc_ptr(val, shift_amount));
    }
  }

}

void adjust_oop_pointers(struct object_heap* heap, word_t shift_amount) {

  struct Object* o = first_object(heap);
#ifdef PRINT_DEBUG
  printf("First object: "); print_object(o);
#endif
  while (object_in_memory(heap, o)) {
    /*print_object(o);*/
    if (!object_is_free(heap, o)) {
      adjust_fields_by(heap, o, shift_amount);
    }
      o = object_after(heap, o);
  }


}


int main(int argc, char** argv) {

  FILE* file;
  struct slate_image_header sih;
  struct object_heap heap;
  word_t memory_limit = 30 * 1024 * 1024;

  memset(&heap, 0, sizeof(heap));

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
  

  if (!heap_initialize(&heap, sih.size, memory_limit, sih.next_hash, sih.special_objects_oop, sih.current_dispatch_id)) return 1;

  printf("Image size: %ld bytes\n", sih.size);
  if (fread(heap.memory, 1, sih.size, file) != sih.size) {
    fprintf(stderr, "Error fread()ing image\n");
    return 1;
  }

  adjust_oop_pointers(&heap, (word_t)heap.memory);
  interpret(&heap);
  
  gc_close(&heap);

  fclose(file);

  return 0;
}
