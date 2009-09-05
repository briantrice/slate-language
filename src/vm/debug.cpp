
#include "slate.hpp"

/* debugging routines to print objects etc from gdb*/


void print_object(struct Object* oop) {
  if (oop == NULL) {
    printf("<object NULL>\n");
    return;
  }
  if (oop_is_smallint((word_t)oop)) {
    printf("<object int_value: %" PRIdPTR ">\n", object_to_smallint(oop));
  } else {
    const char* typestr=0;
    switch (object_type(oop)) {
    case TYPE_OBJECT: typestr = "normal"; break;
    case TYPE_OOP_ARRAY: typestr = "oop array"; break;
    case TYPE_BYTE_ARRAY: typestr = "byte array"; break;
    }
    printf("<object at %p, hash: 0x%" PRIxPTR ", size: %" PRIdPTR ", payload size: %" PRIdPTR ", type: %s>\n", (void*)oop, object_hash(oop), object_size(oop), payload_size(oop), typestr);
  }

}


void print_symbol(struct Symbol* name) {
  if (fwrite(&name->elements[0], 1, payload_size((struct Object*)name), stdout) != (size_t)payload_size((struct Object*)name)) {
    /*handle error*/
  }
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
      printf("\\x%02" PRIxPTR "", (word_t)elements[i]);
    }
    if (i > 10 && payload_size(o) - i > 100) {
      i = payload_size(o) - 20;
      printf("{..snip..}");
    }
  }
  printf("'");

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
  if (object_hash(o) >= ID_HASH_RESERVED) {
    indent(depth); printf("<object is free/reserved>\n");
    return;
  }

  indent(depth); printf("{\n");

  if (object_type(o) == TYPE_BYTE_ARRAY) {
    indent(depth); printf("bytes: ");
    print_byte_array(o); printf("\n");
  }
  if (object_type(o) == TYPE_OOP_ARRAY) {
    struct OopArray* array = (struct OopArray*)o;
    indent(depth); printf("size(array) = %" PRIdPTR "\n", object_array_size(o));
    for (i=0; i < object_array_size(o); i++) {
      indent(depth); printf("array[%" PRIdPTR "]: ", i); print_object_with_depth(oh, array->elements[i], depth+1, max_depth);
      if (object_array_size(o) - i > 10) {
        indent(depth); printf("...\n");
        i = object_array_size(o) - 2;
      }
    }
  }
  indent(depth);
  printf("map flags: %" PRIdPTR " (%s)\n", 
         object_to_smallint(map->flags),
         ((((word_t)map->flags & MAP_FLAG_RESTRICT_DELEGATION)==0)? "delegation not restricted":"delegation restricted"));

  {
    /*print if delegate*/
    
    struct OopArray* delegates = map->delegates;
    word_t offset = object_array_offset((struct Object*)delegates);
    word_t limit = object_total_size((struct Object*)delegates);
    for (i = 0; offset != limit; offset += sizeof(word_t), i++) {
      struct Object* delegate = object_slot_value_at_offset((struct Object*)delegates, offset);
      indent(depth); printf("delegate[%" PRIdPTR "] = ", i);
      print_object_with_depth(oh, delegate, 
                              (((word_t)map->flags & MAP_FLAG_RESTRICT_DELEGATION) == 0)?  depth+1 : max(max_depth-1, depth+1), max_depth);
    }
  }

  {/*if?*/
    struct SlotTable* slotTable = map->slotTable;
    word_t limit = slot_table_capacity(map->slotTable);
    indent(depth); printf("slot count: %" PRIdPTR "\n", object_to_smallint(map->slotCount));
    for (i=0; i < limit; i++) {
      if (slotTable->slots[i].name == (struct Symbol*)oh->cached.nil) continue;
      indent(depth);
      printf("slot[%" PRIdPTR "]['", i); print_symbol(slotTable->slots[i].name); printf("'] = ");
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
    indent(depth); printf("role table capacity: %" PRIdPTR "\n", limit);
    for (i = 0; i < limit; i++) {
      if (roleTable->roles[i].name == (struct Symbol*)oh->cached.nil) {continue;}
      else {
        indent(depth); printf("role[%" PRIdPTR "]['", i); print_symbol(roleTable->roles[i].name);
        printf("'] @ 0x%04" PRIxPTR "\n", object_to_smallint(roleTable->roles[i].rolePositions));
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

bool_t print_printname(struct object_heap* oh, struct Object* o) {

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
    printf("<smallint value: %" PRIdPTR " (0x%" PRIuPTR "x)>\n", object_to_smallint(o), object_to_smallint(o));
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
    printf("stack[%" PRIdPTR "] = ", j);
    print_detail(oh, i->stack->elements[j]);
  }
}

void print_stack_types(struct object_heap* oh, word_t last_count) {
  struct Interpreter* i = oh->cached.interpreter;
  word_t size = i->stackPointer;
  word_t j;
  for (j=0; j < size; j++) {
    if (size - j > last_count) continue;
    printf("stack[%" PRIdPTR "] = ", j);
    print_type(oh, i->stack->elements[j]);
  }
}


void print_backtrace(struct object_heap* oh) {
  word_t depth = 0, detail_depth = -1 /*raise this to print verbose args in stack longer*/;
  struct Interpreter* i = oh->cached.interpreter;
  struct Closure* closure = (struct Closure*)i->method;
  word_t codePointer = i->codePointer;
  word_t fp = i->framePointer;
  word_t sp = i->stackPointer;
  word_t codeSize = i->codeSize;
  word_t resultStackPointer = object_to_smallint(i->stack->elements[fp-5]);
  struct LexicalContext* lc = (struct LexicalContext*)i->stack->elements[fp-2];;
  printf("printing backtrace from fp=%" PRIdPTR ", sp=%" PRIdPTR "\n", fp, i->stackPointer);
  do {
    word_t j;
    struct Object** vars;
    word_t input_count = object_to_smallint(closure->method->inputVariables);
    word_t local_count = object_to_smallint(closure->method->localVariables);
    vars = (closure->method->heapAllocate == oh->cached.true_object)? (&lc->variables[0]) : (&i->stack->elements[fp]);
    printf("------------------------------\n");
    printf("fp: %" PRIdPTR "\n", fp);
    printf("sp: %" PRIdPTR "\n", sp);
    printf("ip: %" PRIdPTR "/%" PRIdPTR "\n", codePointer, codeSize);
    printf("result: %" PRIdPTR "\n", resultStackPointer);
    printf("method: "); print_byte_array((struct Object*)(closure->method->selector)); printf("\n");
    printf("regs: %" PRIdPTR "\n", object_to_smallint(closure->method->registerCount));
    printf("heap alloc: %s\n", (closure->method->heapAllocate == oh->cached.true_object)? "true" : "false");

    for (j = 0; j < input_count; j++) {
      printf("arg[%" PRIdPTR "] (%p) = ", j, (void*)vars[j]);
      if (depth > detail_depth) {
        print_type(oh, vars[j]);
      } else {
        print_detail(oh, vars[j]);
      }
    }
    if (closure->method->heapAllocate == oh->cached.true_object) {
      for (j = 0; j < local_count; j++) {
        printf("var[%" PRIdPTR "] (%p)= ", j, (void*)lc->variables[j]);
        if (depth > detail_depth) {
          print_type(oh, lc->variables[j]);
        } else {
          print_detail(oh, lc->variables[j]);
        }
      }
    } else {
      for (j = input_count; j < input_count + local_count; j++) {
        printf("var[%" PRIdPTR "] (%p) = ", j - input_count, (void*)vars[j]);
        if (depth > detail_depth) {
          print_type(oh, vars[j]);
        } else {
          print_detail(oh, vars[j]);
        }
      }
    }


    /*order matters here*/
    codePointer = object_to_smallint(i->stack->elements[fp-4]);
    fp = object_to_smallint(i->stack->elements[fp-1]);
    if (fp < FUNCTION_FRAME_SIZE) break;
    sp = object_to_smallint(i->stack->elements[fp-6]);
    resultStackPointer = object_to_smallint(i->stack->elements[fp-5]);
    closure = (struct Closure*)i->stack->elements[fp-3];
    lc = (struct LexicalContext*)i->stack->elements[fp-2];
    codeSize = array_size(closure->method->code);
    depth++;
  } while (fp >= FUNCTION_FRAME_SIZE);

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
    printf("%" PRIdPTR "]\n", object_total_size(obj));

    obj = object_after(oh, obj);
  }
  printf("\n");
  printf("free: %" PRIdPTR ", used: %" PRIdPTR ", pinned: %" PRIdPTR "\n", free_count, used_count, pin_count);

}



word_t heap_what_points_to_in(struct object_heap* oh, struct Object* x, byte_t* memory, word_t memorySize, bool_t print) {
  struct Object* obj = (struct Object*) memory;
  word_t count = 0;
  while (object_in_memory(oh, obj, memory, memorySize)) {
    word_t offset, limit;
    offset = object_first_slot_offset(obj);
    limit = object_last_oop_offset(obj) + sizeof(word_t);
    for (; offset != limit; offset += sizeof(word_t)) {
      struct Object* val = object_slot_value_at_offset(obj, offset);
      if (val == x) {
        if (!object_is_free(obj)) count++;
        if (print) print_object(obj);
        break;
      }
    }
    obj = object_after(oh, obj);
  }
  return count;

}

word_t heap_what_points_to(struct object_heap* oh, struct Object* x, bool_t print) {
  
  return heap_what_points_to_in(oh, x, oh->memoryOld, oh->memoryOldSize, print) + heap_what_points_to_in(oh, x, oh->memoryYoung, oh->memoryYoungSize, print);

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
  printf("free: %" PRIdPTR ", used: %" PRIdPTR ", pinned: %" PRIdPTR "\n", free_count, used_count, pin_count);

}

word_t print_code_args(struct object_heap* oh, struct OopArray* code, word_t args, word_t start) {
  word_t i;
  for (i = start; i < start+args; i++) {
    //print_object(code->elements[i]);
    print_type(oh, code->elements[i]);
  }
  return args;
}

void print_code_disassembled(struct object_heap* oh, struct OopArray* code) {
  word_t i, size;
  size = array_size(code);
  i = 0;
  word_t op;

  while (i < size) {
    printf("[%" PRIdPTR "] ", i);
    op = object_to_smallint(code->elements[i++]);
    switch (op) {
    case 0: printf("direct send message "); i += print_code_args(oh, code, 3 + object_to_smallint(code->elements[i+2]), i); break;
    case 1: printf("indirect send message "); i += print_code_args(oh, code, 3 + object_to_smallint(code->elements[i+2]), i); break;
    case 3: printf("load literal "); i += print_code_args(oh, code, 2, i); break;
    case 4: printf("store literal "); i += print_code_args(oh, code, 2, i); break;
    case 5: printf("send message with opts "); i += print_code_args(oh, code, 4 + object_to_smallint(code->elements[i+2]), i); break;
    case 7: printf("new closure "); i += print_code_args(oh, code, 2, i); break;
    case 8: printf("new array with "); i += print_code_args(oh, code, 2 + object_to_smallint(code->elements[i+1]), i); break;
    case 9: printf("resend message "); i += print_code_args(oh, code, 2, i); break;
    case 10: printf("return from "); i += print_code_args(oh, code, 2, i); break;
    case 11: printf("load env "); i += print_code_args(oh, code, 1, i); break;
    case 12: printf("load var "); i += print_code_args(oh, code, 1, i); break;
    case 13: printf("store var "); i += print_code_args(oh, code, 1, i); break;
    case 14: printf("load free var "); i += print_code_args(oh, code, 3, i); break;
    case 15: printf("store free var "); i += print_code_args(oh, code, 3, i); break;
    case 16: printf("is identical to "); i += print_code_args(oh, code, 3, i); break;
    case 17: printf("branch keyed "); i += print_code_args(oh, code, 2, i); break;
    case 18: printf("jump to "); i += print_code_args(oh, code, 1, i); break;
    case 19: printf("move reg "); i += print_code_args(oh, code, 2, i); break;
    case 20: printf("branch true "); i += print_code_args(oh, code, 2, i); break;
    case 21: printf("branch false "); i += print_code_args(oh, code, 2, i); break;
    case 22: printf("return reg "); i += print_code_args(oh, code, 1, i); break;
    case 23: printf("return val "); i += print_code_args(oh, code, 1, i); break;
    case 24: printf("resume "); i += print_code_args(oh, code, 0, i); break;
    default: printf("error reading code %" PRIdPTR "... stopping\n", op); return;
    }
    printf("\n");
  }
}

void heap_integrity_check(struct object_heap* oh, byte_t* memory, word_t memorySize) {
  struct Object* o = (struct Object*)memory;

  while (object_in_memory(oh, o, memory, memorySize)) {
    if (object_is_free(o)) {
      assert(heap_what_points_to(oh, o, 0) == 0);
    }
    o = object_after(oh, o);
  }
}
