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
typedef byte_t bool;

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
  word_t flags;
  word_t representative;
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
  struct Object* name;
  struct Object* rolePositions; /*smallint?*/
  struct MethodDefinition * methodDefinition;
  struct Object* nextRole; /*smallint?*/
};
struct SlotEntry
{
  struct Object* name;
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
  word_t cacheMask;
  unsigned char elements[];
};
struct CompiledMethod
{
  struct ObjectHeader header;
  struct Map * map;
  struct CompiledMethod * method;
  struct Object* selector;
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
  word_t ensureHandlers;
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


bool oop_is_object(word_t xxx)   { return ((((word_t)xxx)&1) == 0); }
bool oop_is_smallint(word_t xxx) { return ((((word_t)xxx)&1) == 1);}
bool object_is_smallint(struct Object* xxx) { return ((((word_t)xxx)&1) == 1);}
word_t object_mark(struct Object* xxx)      { return  (((xxx)->header.header)&1); }
word_t object_hash(struct Object* xxx)       { return  ((((xxx)->header.header)>>1)&0x7FFFFF);}
word_t object_size(struct Object* xxx)       {return   ((((xxx)->header.header)>>24)&0x3F);}
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

word_t object_to_smallint(struct Object* xxx)  {return ((((word_t)xxx)>>1)); }
struct Object* smallint_to_object(word_t xxx) {return ((struct Object*)(((xxx)<<1)|1)); }

#define inc_ptr(xxx, yyy)     ((byte_t*)xxx + yyy)

#define ID_HASH_RESERVED 0x7FFFF0
#define ID_HASH_FORWARDED ID_HASH_RESERVED
#define ID_HASH_FREE 0x7FFFFF

#define MAP_FLAG_RESTRICT_DELEGATION 2
#define MAP_FLAG_IMMUTABLE 4

#define SLOT_OFFSET_MASK (~1)
#define SLOT_TYPE_MASK 1
#define SLOT_TYPE_DELEGATE 1
#define SLOT_TYPE_DATA 0

#define HEADER_SIZE sizeof(word_t)

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



void print_object(struct Object* oop) {
  if (oop == NULL) {
    printf("<object NULL>\n");
    return;
  }
  if (oop_is_smallint((word_t)oop)) {
    printf("<object int_value: %lu>\n", object_to_smallint(oop));
  } else {
    char* typestr;
    switch (object_type(oop)) {
    case TYPE_OBJECT: typestr = "normal"; break;
    case TYPE_OOP_ARRAY: typestr = "oop array"; break;
    case TYPE_BYTE_ARRAY: typestr = "byte array"; break;
    case TYPE_PAYLOAD: typestr = "payload"; break;
    }
    printf("<object at %p, hash: 0x%lX, size: %lu, type: %s>\n", (void*)oop, object_hash(oop), object_size(oop), typestr);
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


bool object_in_memory(struct object_heap* heap, struct Object* oop) {

  return (heap->memory <= (byte_t*)oop && heap->memory + heap->memory_size > (byte_t*)oop);

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

bool interpreter_return_result(struct object_heap* oh, struct Interpreter* i, word_t context_depth, bool has_result) {

  /*fixme*/
  assert(0);

  return 0;

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


word_t interpreter_decode_short(struct Interpreter* i) {

  unsigned long int n;
  
  n = i -> codePointer;
  i -> codePointer = n + 2;
  
  return (word_t) (i->method->code->elements[n] | (i->method->code->elements[n + 1] << 8));
}

void print_symbol(struct Object* name) {
  fwrite(&((struct Symbol*)name)->elements[0], 1, object_payload_size(name), stdout);
}


void indent(word_t amount) { word_t i; for (i=0; i<amount; i++) printf("    "); }

void print_object_with_depth(struct object_heap* oh, struct Object* o, word_t depth, word_t max_depth) {

  struct Map* map;
  word_t i;

  if (depth >= max_depth || object_is_smallint(o)) {
    print_object(o);
    return;
  }

  map = o->map;
  print_object(o);

  indent(depth); printf("{\n");

  if (object_type(o) == TYPE_BYTE_ARRAY) {
    char* elements = (char*)inc_ptr(o, object_array_offset(o));
    indent(depth); printf("bytes: '");
    for (i=0; i < object_payload_size(o); i++) {
      if (elements[i] >= 32 && elements[i] <= 126) {
        printf("%c", elements[i]);
      } else {
        printf("\\x%02lx", (word_t)elements[i]);
      }
    }
    printf("'\n");
  }
  if (object_type(o) == TYPE_OOP_ARRAY) {
    struct OopArray* array = (struct OopArray*)o;
    indent(depth); printf("size(array) = %lu\n", object_array_size(o));
    for (i=0; i < object_array_size(o); i++) {
      indent(depth); printf("array[%lu]: ", i); print_object_with_depth(oh, array->elements[i], depth+1, max_depth);
      if (object_array_size(o) - i > 10) {
        indent(depth); printf("...\n");
        i = object_array_size(o) - 2;
      }
    }
  }

  if ((map->flags & MAP_FLAG_RESTRICT_DELEGATION) == 0) {
    /*print if delegate*/
    
    struct OopArray* delegates = map->delegates;
    word_t offset = object_array_offset((struct Object*)delegates);
    word_t limit = object_total_size((struct Object*)delegates);
    for (i = 0; offset != limit; offset += sizeof(word_t), i++) {
      struct Object* delegate = object_slot_value_at_offset((struct Object*)delegates, offset);
      indent(depth); printf("delegate[%lu] = ", i); print_object_with_depth(oh, delegate, depth+1, max_depth);
    }
  }

  if (object_type(o) != TYPE_PAYLOAD) {
    struct SlotTable* slotTable = map->slotTable;
    word_t limit = object_to_smallint(map->slotCount);
    indent(depth); printf("slot count: %lu\n", limit);
    for (i=0; i < limit; i++) {
      bool delegate = 0;
      if (slotTable->slots[i].name == oh->cached.nil) continue;
      indent(depth);
      if ((object_to_smallint(slotTable->slots[i].offset) & SLOT_TYPE_MASK) == SLOT_TYPE_DELEGATE) {
        printf("delegate_");
        delegate = 1;
      }
      printf("slot[%lu]['", i); print_symbol(slotTable->slots[i].name); printf("'] = ");
      if (!delegate) {
        print_object(object_slot_value_at_offset(o, object_to_smallint(slotTable->slots[i].offset) & SLOT_OFFSET_MASK));
      } else {
        print_object_with_depth(oh, object_slot_value_at_offset(o, object_to_smallint(slotTable->slots[i].offset) & SLOT_OFFSET_MASK),
                                depth+1, max_depth);
      }
    }
  }


  if (depth < 2) {
    indent(depth); printf("map = "); print_object_with_depth(oh, (struct Object*)map, depth+1, max_depth);
  }
  
  /*roles */
  {
    struct RoleTable* roleTable = map->roleTable;
    word_t limit = role_table_capacity(roleTable);
    indent(depth); printf("role table capacity: %lu\n", limit);
    for (i = 0; i < limit; i++) {
      if (roleTable->roles[i].name == oh->cached.nil) {continue;}
      else {
        indent(depth); printf("role[%lu]['", i); print_symbol(roleTable->roles[i].name);
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

word_t* gc_allocate(struct object_heap* oh, word_t size) {

  /*FIX!!!!!!!!*/
  return malloc(size);

}


struct Object* heap_allocate_chunk_with_payload(struct object_heap* oh, word_t words, word_t payload_size) {

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


struct OopArray* heap_clone_oop_array_sized(struct object_heap* oh, struct Object* proto, word_t size) {

  struct Object* newObj = heap_allocate_chunk_with_payload(oh, object_size(proto), size * sizeof(word_t));
  object_set_format(newObj, TYPE_OOP_ARRAY);
  object_set_idhash(newObj, heap_new_hash(oh));
  /*copy everything but the header bits*/
  copy_words_into((word_t *) inc_ptr(proto, HEADER_SIZE), object_size(proto) - 1, (word_t*) inc_ptr(newObj, HEADER_SIZE));
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
}

void interpreter_stack_push(struct object_heap* oh, struct Interpreter* i, struct Object* value) {

#ifdef PRINT_DEBUG
  printf("Stack push: "); print_object(value);
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
  
  return i->stack->elements[i->stackPointer];

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



void method_save_cache(struct object_heap* oh, struct MethodDefinition* md, struct Object* name, struct Object* arguments[], word_t arity) {
  /*fix*/

}

struct MethodDefinition* method_check_cache(struct object_heap* oh, struct Object* name, struct Object* arguments[], word_t arity) {
  /*fix*/
  return NULL;
}


struct RoleEntry* role_table_entry_for_name(struct object_heap* oh, struct RoleTable* roles, struct Object* name) {

  word_t tableSize, hash, index;
  struct RoleEntry* role;

  tableSize = role_table_capacity(roles);
  if (tableSize == 0) return NULL;
  hash = object_hash(name) & (tableSize-1); /*fix base2 assumption*/
  
  
  for (index = hash; index < tableSize; index++) {

    role = &roles->roles[index];
    if (role->name == name) return role;
    if (role->name == oh->cached.nil) return NULL;
  }
  for (index = 0; index < hash; index++) {
    role = &roles->roles[index];
    if (role->name == name) return role;
    if (role->name == oh->cached.nil) return NULL;
  }

  return NULL;
}


/*
 * This is the main dispatch function
 *
 */
struct MethodDefinition* method_dispatch_on(struct object_heap* oh, struct Object* name,
                                            struct Object* arguments[], word_t arity, struct Object* resendMethod) {

  struct MethodDefinition *dispatch, *bestDef;
  struct Object* slotLocation;
  word_t bestRank, depth, delegationCount, resendRank, restricted, i;


#ifdef PRINT_DEBUG
  printf("dispatch to: '");
  print_symbol(name);
  printf("' (arity: %lu)\n", arity);
  for (i = 0; i < arity; i++) {
    printf("arguments[%lu] = \n", i); print_detail(oh, arguments[i]);
  }
  printf("resend: "); print_object(resendMethod);
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
    restricted = -1;
    
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
      /* Proceed unless we've visited the map at this index or the map marks
         an obj-meta transition... */
      if ((map->visitedPositions & (1 << i)) == 0 && 
          ((map->flags & MAP_FLAG_RESTRICT_DELEGATION) == 0 || restricted < 0)) {

        struct RoleEntry* role;

        /* If the map marks an obj-meta transition and the top of the stack
           is not the original argument, then mark the restriction point at
           the top of the delegation stack. */

        if ((map->flags & MAP_FLAG_RESTRICT_DELEGATION) && (arg != arguments[i])) {
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
              printf("found role <%p> for '%s' foundPos: %lx dispatchPos: %lx\n",(void*) role,
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

#ifdef PRINT_DEBUG
            printf("arguments[0] changed to slot location: \n");
            print_detail(oh, arguments[0]);
#endif
          }
          if (resendMethod == 0 && arity <= METHOD_CACHE_ARITY) method_save_cache(oh, dispatch, name, arguments, arity);
          return dispatch;
        }
        
        depth++;
      }
      if (object_array_size((struct Object*)map->delegates) > 0) {
        struct OopArray* delegates = map->delegates;
        word_t offset = object_array_offset((struct Object*)delegates);
        word_t limit = object_total_size((struct Object*)delegates);
        for (; offset != limit; offset += sizeof(word_t)) {
          struct Object* delegate = object_slot_value_at_offset((struct Object*)delegates, offset);
          if (delegate != oh->cached.nil) {
            oh->delegation_stack[delegationCount] = delegate;
            delegationCount++;
          }
          
        }
      }
      delegationCount--;
      if (delegationCount < DELEGATION_STACK_SIZE) {
        arg = oh->delegation_stack[delegationCount];
        if (delegationCount < restricted) {
          restricted = -1;
        }
      }

    } while (delegationCount < DELEGATION_STACK_SIZE);

  }


  if (dispatch != NULL && dispatch->slotAccessor != oh->cached.nil) {
    arguments[0] = slotLocation;
#ifdef PRINT_DEBUG
            printf("arguments[0] changed to slot location: \n");
            print_detail(oh, arguments[0]);
#endif

  }
  if (dispatch != NULL && resendMethod == 0 && arity < METHOD_CACHE_ARITY) {
    method_save_cache(oh, dispatch, name, arguments, arity);
  }

  return dispatch;
}

void call_primitive(struct object_heap* oh, word_t index,
                    struct Object* args[], word_t n, struct OopArray* opts) {

#ifdef PRINT_DEBUG
  printf("calling primitive %lu\n", index);
#endif

  assert(0);

}

void interpreter_dispatch_optional_keyword(struct object_heap* oh, struct Interpreter * i, struct Object* key, struct Object* value) {

  struct OopArray* optKeys = i->method->optionalKeywords;

  word_t optKey, size = object_array_size((struct Object*) optKeys);

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

  word_t index, size = object_array_size((struct Object*)opts);
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
  
  method = closure->method;
  inputs = object_to_smallint(method->inputVariables);
  
  if (n < inputs || (n > inputs && method->restVariable != oh->cached.nil)) {
    struct OopArray* argsArray = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
    copy_words_into((word_t*) args, n, (word_t*)argsArray->elements);
    interpreter_signal_with_with(oh, i, get_special(oh, SPECIAL_OOP_WRONG_INPUTS_TO), (struct Object*) argsArray, (struct Object*)method, NULL);
    return;
  }

  framePointer = i->stackPointer + 8;

  if (method->heapAllocate == oh->cached.true) {
    lexicalContext = (struct LexicalContext*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_LEXICAL_CONTEXT_PROTO), object_to_smallint(method->localVariables));
    lexicalContext->framePointer = smallint_to_object(framePointer);
    interpreter_stack_allocate(oh, i, 8);
    vars = &lexicalContext->variables[0];
  } else {
    lexicalContext = (struct LexicalContext*) oh->cached.nil;
    interpreter_stack_allocate(oh, i, sizeof(word_t) + object_to_smallint(method->localVariables));
    vars = &i->stack->elements[framePointer];
  }


  copy_words_into((word_t*) args, inputs, (word_t*) vars);
  i->stack->elements[framePointer-4] = smallint_to_object(i->codePointer);
  i->stack->elements[framePointer-3] = (struct Object*) closure;
  i->stack->elements[framePointer-2] = (struct Object*) lexicalContext;
  i->stack->elements[framePointer-1] = smallint_to_object(i->framePointer);

  
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
    vars[inputs+object_array_size((struct Object*)method->optionalKeywords)] = (struct Object*) restArgs;
  } else {
    if (method->restVariable == oh->cached.true) {
      vars[inputs+object_array_size((struct Object*)method->optionalKeywords)] = get_special(oh, SPECIAL_OOP_ARRAY_PROTO);
    }
  }
  if (opts != NULL) {
    interpreter_dispatch_optionals(oh, i, opts);
  }

}


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
  struct MethodDefinition* def = method_dispatch_on(oh, selector, dispatchers, arity, NULL);

  if (def == NULL) {
    argsArray = (struct OopArray*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), arity);
    copy_words_into((word_t*)dispatchers, arity, (word_t*)&argsArray->elements[0]);
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_NOT_FOUND_ON), selector, (struct Object*)argsArray, NULL);
    return;
  }

  method = (struct Closure*)def->method;
  traitsWindow = method->map->delegates->elements[0]; /*fix should this location be hardcoded as the first element?*/
  if (traitsWindow == oh->cached.primitive_method_window) {
    call_primitive(oh, object_to_smallint(((struct PrimitiveMethod*)method)->index), args, arity, opts);
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

  i->stackPointer = i->stackPointer - n;
  args = &i->stack->elements[i->stackPointer];
  selector = interpreter_stack_pop(oh, i);
  send_to_through_arity_with_optionals(oh, selector, args, args, n, opts);

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

void interpreter_pop_stack(struct object_heap* oh, struct Interpreter * i, word_t n)
{
  i->stackPointer = i->stackPointer - n;
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
  unsigned long int offset;
  offset = interpreter_decode_short(i);
#ifdef PRINT_DEBUG
  printf("increment code pointer by %lu", offset);
#endif
  i->codePointer = i->codePointer + offset;
}

void interpreter_branch_if_true(struct object_heap* oh, struct Interpreter * i)
{

  word_t offset;
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

  word_t offset;
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

void interpreter_resend_message(struct object_heap* oh, struct Interpreter* i, word_t val) {

  assert(0);

}



void interpreter_push_environment(struct object_heap* oh, struct Interpreter * i) {
  interpreter_stack_push(oh, i, i->method->environment);
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
  tableSize = object_array_size((struct Object *) table);
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
  printf("Interpret: img:0x%p size:%lu spec:%p next:%lu\n",
         (void*)oh->memory, oh->memory_size, (void*)oh->special_objects_oop, oh->next_hash);
  printf("Special oop: "); print_object((struct Object*)oh->special_objects_oop);
#endif

  cache_specials(oh);

#ifdef PRINT_DEBUG
  printf("Interpreter stack: "); print_object((struct Object*)oh->cached.interpreter);
  printf("Interpreter stack size: %lu\n", oh->cached.interpreter->stackSize);
#endif 

  do {
    

    while (oh->cached.interpreter->codePointer < oh->cached.interpreter->codeSize) {
      word_t op, val, prevPointer;
      struct Interpreter* i = oh->cached.interpreter; /*it won't move while we are in here */

      if (oh->interrupt_flag) {
        return;
      }
      
      op = i->method->code->elements[i->codePointer];
      prevPointer = i->codePointer;
      i->codePointer++;
      val = op >> 4;
      if (val == 15) {
        val = interpreter_decode_immediate(i);
      }
#ifdef PRINT_DEBUG
      printf("opcode %d\n", (int)op&15);
#endif
      switch (op & 15) {

      case 0:
        send_message_with_optionals(oh, i, val, (struct OopArray *) 0);
        break;
      case 1:
        interpreter_load_variable(oh, i, val);
        break;
      case 2:
        interpreter_store_variable(oh, i, val);
        break;
      case 3:
        interpreter_load_free_variable(oh, i, val);
        break;
      case 4:
        interpreter_store_free_variable(oh, i, val);
        break;
      case 5:
        interpreter_load_literal(oh, i, val);
        break;
      case 6:
        interpreter_load_selector(oh, i, val);
        break;
      case 7:
        interpreter_pop_stack(oh, i, val);
        break;
      case 8:
        interpreter_new_array(oh, i, val);
        break;
      case 9:
        interpreter_new_closure(oh, i, val);
        break;
      case 10:
        interpreter_branch_keyed(oh, i, val);
        break;
      case 11:
        send_message_with_optionals(oh, i, val, (struct OopArray*)interpreter_stack_pop(oh, i));
        break;
      case 12:
        
        {
          i->codePointer = prevPointer;
          interpreter_return_result(oh, i, val, TRUE);
        }
        break;
      case 13:
        interpreter_push_integer(oh, i, val);
        break;
      case 14:
        interpreter_resend_message(oh, i, val);
        break;
      case 15:

#ifdef PRINT_DEBUG
      printf("op %d\n", (int)op);
#endif

        switch (op)
          {
          case 15:
            interpreter_jump_to(oh, i);
            break;
          case 31:
            interpreter_branch_if_true(oh, i);
            break;
          case 47:
            interpreter_branch_if_false(oh, i);
            break;
          case 63:
            interpreter_push_environment(oh, i);
            break;
          case 79:
            interpreter_push_nil(oh, i);
            break;
          case 95:
            interpreter_is_identical_to(oh, i);
            break;
          case 111:
            interpreter_push_true(oh, i);
            break;
          case 127:
            interpreter_push_false(oh, i);
            break;
          case 143:
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
  byte_t* image_start;
  struct object_heap heap;
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
  
  image_start = malloc(sih.size);
  printf("Image size: %lu bytes\n", sih.size);
  if (fread(image_start, 1, sih.size, file) != sih.size) {
    fprintf(stderr, "Error fread()ing image\n");
    return 1;
  }


  heap.memory = image_start;
  heap.special_objects_oop = (struct OopArray*)(image_start + sih.special_objects_oop);
  heap.next_hash = sih.next_hash;
  heap.memory_size = sih.size;
  heap.current_dispatch_id = sih.current_dispatch_id;
  heap.interrupt_flag = 0;
  heap.mark_color = 1;

  adjust_oop_pointers(&heap, (word_t)heap.memory);
  interpret(&heap);
  

  free(image_start);
  fclose(file);

  return 0;
}
