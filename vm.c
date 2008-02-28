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
  word_t selector;
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
  word_t slotCount;
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
  word_t index;
  word_t selector;
  word_t inputVariables;
};
struct RoleEntry
{
  word_t name;
  word_t rolePositions;
  struct MethodDefinition * methodDefinition;
  word_t nextRole;
};
struct SlotEntry
{
  word_t name;
  word_t offset;
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
  word_t selector;
  word_t inputVariables;
  word_t localVariables;
  word_t restVariable;
  struct OopArray * optionalKeywords;
  struct Object* heapAllocate;
  word_t environment;
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
  word_t method;
  word_t slotAccessor;
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
struct Interpreter
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
  struct {
    struct Interpreter* interpreter;
    struct Object* true;
    struct Object* false;
  } cached;
};
/****************************************/


bool oop_is_object(word_t xxx)   { return ((((word_t)xxx)&1) == 0); }
bool oop_is_smallint(word_t xxx) { return ((((word_t)xxx)&1) == 1);}
word_t object_mark(struct Object* xxx)      { return  (((xxx)->header.header)&1); }
word_t object_hash(struct Object* xxx)       { return  ((((xxx)->header.header)>>1)&0x7FFFFF);}
word_t object_size(struct Object* xxx)       {return   ((((xxx)->header.header)>>24)&0x3F);}
word_t payload_size(struct Object* xxx) {return ((((xxx)->header.header))&0x3FFFFFFF);}
word_t object_type(struct Object* xxx)     {return     ((((xxx)->header.header)>>30)&0x3);}

word_t oop_to_smallint(struct Object* xxx)  {return ((((word_t)xxx)>>1)); }
struct Object* smallint_to_oop(word_t xxx) {return ((struct Object*)(((xxx)<<1)|1)); }

#define oop_to_object(xxx)    ((struct Object*)xxx)
#define inc_ptr(xxx, yyy)     ((byte_t*)xxx + yyy)

#define ID_HASH_RESERVED 0x7FFFF0
#define ID_HASH_FORWARDED ID_HASH_RESERVED
#define ID_HASH_FREE 0x7FFFFF

#define HEADER_SIZE sizeof(word_t)

#define TRUE 1
#define FALSE 0

#define TYPE_OBJECT 0
#define TYPE_OOP_ARRAY  1
#define TYPE_BYTE_ARRAY 2
#define TYPE_PAYLOAD 3


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

  if (oop_is_smallint((word_t)oop)) {
    printf("<object int_value: %lu>\n", oop_to_smallint(oop));
  } else {
    printf("<object at %p, hash: 0x%lX, size: %lu, type: %lu>\n", (void*)oop, object_hash(oop), object_size(oop), object_type(oop));
  }

}


struct Object* get_special(struct object_heap* oh, word_t special_index) {
  return oh->special_objects_oop->elements[special_index];
}

void cache_specials(struct object_heap* heap) {

 heap->cached.interpreter = (struct Interpreter*) get_special(heap, SPECIAL_OOP_INTERPRETER);
 heap->cached.true = (struct Object*) get_special(heap, SPECIAL_OOP_TRUE);
 heap->cached.false = (struct Object*) get_special(heap, SPECIAL_OOP_FALSE);

}

bool interpreter_return_result(struct object_heap* oh, struct Interpreter* i, word_t context_depth, bool has_result) {

  /*fixme*/
  assert(0);

  return 0;

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

word_t decode_immediate(struct Interpreter* i) {

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


word_t decode_short(struct Interpreter* i) {

  unsigned long int n;
  
  n = i -> codePointer;
  i -> codePointer = n + 2;
  
  return (word_t) (i->method->code->elements[n] | (i->method->code->elements[n + 1] << 8));
}


struct OopArray* heap_new_oop_array(struct object_heap* oh, struct Object* proto, word_t size) {

  assert(0);
}

void interpreter_stack_push(struct object_heap* oh, struct Interpreter* i, struct Object* value) {

  assert(0);

}

struct Object* interpreter_stack_pop(struct object_heap* oh, struct Interpreter* i) {

  if (i -> stackPointer == 0) {
    /*error("Attempted to pop empty interpreter stack.");*/
    assert(0);
  }

  i->stackPointer = i->stackPointer - 1;
  
  return i->stack->elements[i->stackPointer];

}

void send_message_with_optionals(struct object_heap* oh, word_t arity, struct OopArray* opts) {

  /*fix*/
  assert(0);

}

word_t object_payload_size(struct Object* o) {

  /*in memory there is a payload header before the object*/
  o = (struct Object*) inc_ptr(o, -HEADER_SIZE);
  return payload_size(o);

}

word_t object_array_size(struct Object* o) {

  assert(object_type(o) != TYPE_OBJECT);
  return (object_payload_size(o) + sizeof(word_t) - 1) / sizeof(word_t);

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
  i->closure->lexicalWindow[n-1]->variables[decode_immediate(i)] = i->stack->elements[i->stackPointer - 1];
}

void interpreter_load_free_variable(struct object_heap* oh, struct Interpreter * i, word_t n)
{
  interpreter_stack_push(oh, i, i->closure->lexicalWindow[n-1]->variables[decode_immediate(i)]);
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
  i -> stackPointer = i -> stackPointer - n;
}

void interpreter_new_closure(struct object_heap* oh, struct Interpreter * i, word_t n)
{
  struct Closure * newClosure;
  
  if ((struct CompiledMethod *) i -> closure == i -> method) {
    newClosure = (struct Closure *) heap_new_oop_array(oh, get_special(oh, SPECIAL_OOP_CLOSURE_PROTO), 1);
  } else {
    word_t inheritedSize;
    
    inheritedSize = object_array_size((struct Object *) i -> closure);
    newClosure = (struct Closure *) heap_new_oop_array(oh, get_special(oh, SPECIAL_OOP_CLOSURE_PROTO), inheritedSize+1);
    copy_words_into((word_t *) i->closure->lexicalWindow, inheritedSize, (word_t*) newClosure->lexicalWindow + 1);
  }
  newClosure->lexicalWindow[0] = i -> lexicalContext;
  newClosure->method = (struct CompiledMethod *) i->method->literals->elements[n];
  interpreter_stack_push(oh, i, (struct Object*)newClosure);
}

void interpreter_push_integer(struct object_heap* oh, struct Interpreter * i, word_t integer)
{
  interpreter_stack_push(oh, i, smallint_to_oop(integer));

}

void interpreter_jump_to(struct object_heap* oh, struct Interpreter * i)
{
  unsigned long int offset;
  offset = decode_short(i);
  i -> codePointer = i -> codePointer + offset;
}

void interpreter_branch_if_true(struct object_heap* oh, struct Interpreter * i)
{

  word_t offset;
  struct Object* condition;
  
  offset = decode_short(i);
  condition = interpreter_stack_pop(oh, i);
  if (condition == oh->cached.true) {
    i -> codePointer = i -> codePointer + offset;
  } else {
    if (!(condition == oh->cached.false)) {
      i->codePointer = i->codePointer - 3;
      interpreter_signal_with(oh, i, get_special(oh, SPECIAL_OOP_NOT_A_BOOLEAN), condition);
    }
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
  arr = heap_new_oop_array(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
  stackPointer = i -> stackPointer - n;
  i -> stackPointer = stackPointer;
  /*fix: check elements[0] location*/
  copy_words_into((word_t *) (&((i -> stack) -> elements)[stackPointer]), n, (word_t*)&(arr->elements[0]));
  interpreter_stack_push(oh, i, (struct Object *) arr);
}


void interpret(struct object_heap* oh) {

  printf("Interpret: img:0x%p size:%lu spec:%p next:%lu\n",
         (void*)oh->memory, oh->memory_size, (void*)oh->special_objects_oop, oh->next_hash);
  printf("Special oop: "); print_object(oop_to_object(oh->special_objects_oop));

  cache_specials(oh);
 
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
        val = decode_immediate(i);
      }

      printf("opcode %d\n", (int)op&15);
      
      switch (op & 15) {

      case 0:
        send_message_with_optionals(oh, val, (struct OopArray *) 0);
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
        interpreter_send_message_with_optionals(oh, i, val);
        break;
      case 12:
        
        {
          i -> codePointer = prevPointer;
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

word_t object_slot_value_at_offset(struct Object* o, word_t offset) {

  return *((word_t*)inc_ptr(o, offset));

}

void object_slot_value_at_offset_put(struct Object* o, word_t offset, word_t value) {

  *((word_t*)inc_ptr(o, offset)) = value;

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

void adjust_fields_by(struct object_heap* heap, struct Object* o, word_t shift_amount) {

  word_t offset, limit;
  o->map = (struct Map*) inc_ptr(o->map, shift_amount);
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

  struct Object* o = first_object(heap);
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

  adjust_oop_pointers(&heap, (word_t)heap.memory);
  interpret(&heap);
  

  free(image_start);
  fclose(file);

  return 0;
}
