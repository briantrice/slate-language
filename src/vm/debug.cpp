
#include "slate.hpp"

/* debugging routines to print objects etc from gdb*/


void print_object(struct Object* oop) {
  if (oop == NULL) {
    fprintf(stderr, "<object NULL>\n");
    return;
  }
  if (oop_is_smallint((word_t)oop)) {
    fprintf(stderr, "<object int_value: %" PRIdPTR ">\n", object_to_smallint(oop));
  } else {
    const char* typestr=0;
    switch (object_type(oop)) {
    case TYPE_OBJECT: typestr = "normal"; break;
    case TYPE_OOP_ARRAY: typestr = "oop array"; break;
    case TYPE_BYTE_ARRAY: typestr = "byte array"; break;
    }
    fprintf(stderr, "<object at %p, hash: 0x%" PRIxPTR ", size: %" PRIdPTR ", payload size: %" PRIdPTR ", type: %s>\n", (void*)oop, object_hash(oop), object_size(oop), payload_size(oop), typestr);
  }

}


void print_symbol(struct Symbol* name) {
  if (fwrite(&name->elements[0], 1, payload_size((struct Object*)name), stdout) != (size_t)payload_size((struct Object*)name)) {
    /*handle error*/
  }
}

void indent(word_t amount) { word_t i; for (i=0; i<amount; i++) fprintf(stderr, "    "); }


void print_byte_array(struct Object* o) {
  word_t i;
  char* elements = (char*)inc_ptr(o, object_array_offset(o));
  fprintf(stderr, "'");
  for (i=0; i < payload_size(o); i++) {
    if (elements[i] >= 32 && elements[i] <= 126) {
      fprintf(stderr, "%c", elements[i]);
    } else {
      fprintf(stderr, "\\x%02" PRIxPTR "", (word_t)elements[i]);
    }
    if (i > 10 && payload_size(o) - i > 100) {
      i = payload_size(o) - 20;
      fprintf(stderr, "{..snip..}");
    }
  }
  fprintf(stderr, "'");

}



void print_object_with_depth(struct object_heap* oh, struct Object* o, word_t depth, word_t max_depth) {

  struct Map* map;
  word_t i;

  if (depth >= max_depth || object_is_smallint(o)) {
    fprintf(stderr, "(depth limit reached) "); print_object(o);
    return;
  }

  if (o == oh->cached.nil) {
    fprintf(stderr, "<object NilObject>\n");
    return;
  }

  map = o->map;
  print_object(o);
  if (object_hash(o) >= ID_HASH_RESERVED) {
    indent(depth); fprintf(stderr, "<object is free/reserved>\n");
    return;
  }

  indent(depth); fprintf(stderr, "{\n");

  if (object_type(o) == TYPE_BYTE_ARRAY) {
    indent(depth); fprintf(stderr, "bytes: ");
    print_byte_array(o); fprintf(stderr, "\n");
  }
  if (object_type(o) == TYPE_OOP_ARRAY) {
    struct OopArray* array = (struct OopArray*)o;
    indent(depth); fprintf(stderr, "size(array) = %" PRIdPTR "\n", object_array_size(o));
    for (i=0; i < object_array_size(o); i++) {
      indent(depth); fprintf(stderr, "array[%" PRIdPTR "]: ", i); print_object_with_depth(oh, array->elements[i], depth+1, max_depth);
      if (object_array_size(o) - i > 10) {
        indent(depth); fprintf(stderr, "...\n");
        i = object_array_size(o) - 2;
      }
    }
  }
  indent(depth);
  fprintf(stderr, "map flags: %" PRIdPTR " (%s)\n", 
         object_to_smallint(map->flags),
         ((((word_t)map->flags & MAP_FLAG_RESTRICT_DELEGATION)==0)? "delegation not restricted":"delegation restricted"));

  {
    /*print if delegate*/
    
    struct OopArray* delegates = map->delegates;
    word_t offset = object_array_offset((struct Object*)delegates);
    word_t limit = object_total_size((struct Object*)delegates);
    for (i = 0; offset != limit; offset += sizeof(word_t), i++) {
      struct Object* delegate = object_slot_value_at_offset((struct Object*)delegates, offset);
      indent(depth); fprintf(stderr, "delegate[%" PRIdPTR "] = ", i);
      print_object_with_depth(oh, delegate, 
                              (((word_t)map->flags & MAP_FLAG_RESTRICT_DELEGATION) == 0)?  depth+1 : max(max_depth-1, depth+1), max_depth);
    }
  }

  {/*if?*/
    struct SlotTable* slotTable = map->slotTable;
    word_t limit = slot_table_capacity(map->slotTable);
    indent(depth); fprintf(stderr, "slot count: %" PRIdPTR "\n", object_to_smallint(map->slotCount));
    for (i=0; i < limit; i++) {
      if (slotTable->slots[i].name == (struct Symbol*)oh->cached.nil) continue;
      indent(depth);
      fprintf(stderr, "slot[%" PRIdPTR "]['", i); print_symbol(slotTable->slots[i].name); fprintf(stderr, "'] = ");
      {
        struct Object* obj = object_slot_value_at_offset(o, object_to_smallint(slotTable->slots[i].offset));
        if (object_is_smallint(obj) || object_type(obj) != TYPE_BYTE_ARRAY) {
          print_object(obj);
        } else {
          print_byte_array(obj); fprintf(stderr, "\n");
        }
      }
    }
  }


  /*indent(depth); fprintf(stderr, "map = "); print_object_with_depth(oh, (struct Object*)map, depth+1, max_depth);*/
    
  /*roles */
  {
    struct RoleTable* roleTable = map->roleTable;
    word_t limit = role_table_capacity(roleTable);
    indent(depth); fprintf(stderr, "role table capacity: %" PRIdPTR "\n", limit);
    for (i = 0; i < limit; i++) {
      if (roleTable->roles[i].name == (struct Symbol*)oh->cached.nil) {continue;}
      else {
        indent(depth); fprintf(stderr, "role[%" PRIdPTR "]['", i); print_symbol(roleTable->roles[i].name);
        fprintf(stderr, "'] @ 0x%04" PRIxPTR "\n", object_to_smallint(roleTable->roles[i].rolePositions));
#if 0
        print_object_with_depth(oh, (struct Object*)roleTable->roles[i].methodDefinition, max_depth, max_depth);
#endif
        if (limit - i > 50 && depth >= 2) {
          indent(depth); fprintf(stderr, "...\n");
          i = limit - 3;
        }
      }
      
    }
    
  }


  indent(depth); fprintf(stderr, "}\n");

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
    fprintf(stderr, "<smallint value: %" PRIdPTR " (0x%" PRIuPTR "x)>\n", object_to_smallint(o), object_to_smallint(o));
    return;
  }

  if (object_type(o) == TYPE_BYTE_ARRAY) {
    fprintf(stderr, "(");
    print_byte_array(o);
    fprintf(stderr, ") ");
    /*return;*/
  }
  
  x = o->map->delegates;
  if (!x || array_size(x) < 1) goto fail;

  traitsWindow = x->elements[0];

  {
    if (traitsWindow == oh->cached.compiled_method_window) {
      fprintf(stderr, "(method: ");
      print_byte_array((struct Object*)(((struct CompiledMethod*)o)->method->selector));
      fprintf(stderr, ")");
    }
  }


  x = traitsWindow->map->delegates;
  if (!x || array_size(x) < 1) goto fail;

  traits = x->elements[array_size(x)-1];



  print_printname(oh, o);
  
  fprintf(stderr, "(");
  print_printname(oh, traits);
  fprintf(stderr, ")\n");


  return;
 fail:
  fprintf(stderr, "<unknown type>\n");
}


void print_stack(struct object_heap* oh) {
  struct Interpreter* i = oh->cached.interpreter;
  word_t size = i->stackPointer;
  word_t j;
  for (j=0; j < size; j++) {
    fprintf(stderr, "stack[%" PRIdPTR "] = ", j);
    print_detail(oh, i->stack->elements[j]);
  }
}

void print_stack_types(struct object_heap* oh, word_t last_count) {
  struct Interpreter* i = oh->cached.interpreter;
  word_t size = i->stackPointer;
  word_t j;
  for (j=0; j < size; j++) {
    if (size - j > last_count) continue;
    fprintf(stderr, "stack[%" PRIdPTR "] = ", j);
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
  word_t resultStackPointer = object_to_smallint(i->stack->elements[fp - FRAME_OFFSET_RESULT_STACK_POINTER]);
  struct LexicalContext* lc = (struct LexicalContext*)i->stack->elements[fp - FRAME_OFFSET_LEXICAL_CONTEXT];;
  fprintf(stderr, "printing backtrace from fp=%" PRIdPTR ", sp=%" PRIdPTR "\n", fp, i->stackPointer);
  do {
    word_t j;
    struct Object** vars;
    word_t input_count = object_to_smallint(closure->method->inputVariables);
    word_t local_count = object_to_smallint(closure->method->localVariables);
    vars = (closure->method->heapAllocate == oh->cached.true_object)? (&lc->variables[0]) : (&i->stack->elements[fp]);
    fprintf(stderr, "------------------------------\n");
    fprintf(stderr, "fp: %" PRIdPTR "\n", fp);
    fprintf(stderr, "sp: %" PRIdPTR "\n", sp);
    fprintf(stderr, "ip: %" PRIdPTR "/%" PRIdPTR "\n", codePointer, codeSize);
    fprintf(stderr, "result: %" PRIdPTR "\n", resultStackPointer);
    fprintf(stderr, "code: %p\n", closure->method->code);
    fprintf(stderr, "selector: "); print_byte_array((struct Object*)(closure->method->selector)); fprintf(stderr, "\n");
    fprintf(stderr, "regs: %" PRIdPTR "\n", object_to_smallint(closure->method->registerCount));
    fprintf(stderr, "heap alloc: %s\n", (closure->method->heapAllocate == oh->cached.true_object)? "true" : "false");

    for (j = 0; j < input_count; j++) {
      fprintf(stderr, "arg[%" PRIdPTR "] (%p) = ", j, (void*)vars[j]);
      if (depth > detail_depth) {
        print_type(oh, vars[j]);
      } else {
        print_detail(oh, vars[j]);
      }
    }
    if (closure->method->heapAllocate == oh->cached.true_object) {
      for (j = 0; j < local_count; j++) {
        fprintf(stderr, "var[%" PRIdPTR "] (%p)= ", j, (void*)lc->variables[j]);
        if (depth > detail_depth) {
          print_type(oh, lc->variables[j]);
        } else {
          print_detail(oh, lc->variables[j]);
        }
      }
    } else {
      for (j = input_count; j < input_count + local_count; j++) {
        fprintf(stderr, "var[%" PRIdPTR "] (%p) = ", j - input_count, (void*)vars[j]);
        if (depth > detail_depth) {
          print_type(oh, vars[j]);
        } else {
          print_detail(oh, vars[j]);
        }
      }
    }


    /*order matters here*/
    codePointer = object_to_smallint(i->stack->elements[fp - FRAME_OFFSET_CODE_POINTER]);
    fp = object_to_smallint(i->stack->elements[fp - FRAME_OFFSET_PREVIOUS_FRAME_POINTER]);
    if (fp < FUNCTION_FRAME_SIZE) break;
    sp = object_to_smallint(i->stack->elements[fp - FRAME_OFFSET_BEFORE_CALL_STACK_POINTER]);
    resultStackPointer = object_to_smallint(i->stack->elements[fp - FRAME_OFFSET_RESULT_STACK_POINTER]);
    closure = (struct Closure*)i->stack->elements[fp - FRAME_OFFSET_METHOD];
    lc = (struct LexicalContext*)i->stack->elements[fp - FRAME_OFFSET_LEXICAL_CONTEXT];
    codeSize = array_size(closure->method->code);
    depth++;
  } while (fp >= FUNCTION_FRAME_SIZE);

}

void heap_print_objects(struct object_heap* oh, byte_t* memory, word_t memorySize) {
  struct Object* obj = (struct Object*) memory;
  word_t pin_count = 0, used_count = 0, free_count = 0;
  fprintf(stderr, "\nmemory:\n");
  while (object_in_memory(oh, obj, memory, memorySize)) {
    if (object_is_pinned(oh, obj)) {
      fprintf(stderr, "[pinned ");
      pin_count++;
    } else if (object_is_free(obj)) {
      fprintf(stderr, "[free ");
      free_count++;
    } else {
      fprintf(stderr, "[used ");
      used_count++;
    }
    fprintf(stderr, "%" PRIdPTR "]\n", object_total_size(obj));

    obj = object_after(oh, obj);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "free: %" PRIdPTR ", used: %" PRIdPTR ", pinned: %" PRIdPTR "\n", free_count, used_count, pin_count);

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
  fprintf(stderr, "\nmemory:\n");
  while (object_in_memory(oh, obj, memory, memorySize)) {
    if (object_is_pinned(oh, obj)) {
      fprintf(stderr, "X");
      pin_count++;
    } else if (object_is_free(obj)) {
      fprintf(stderr, " ");
      free_count++;
    } else {
      fprintf(stderr, "*");
      used_count++;
    }
    count--;
    if (count == 0) {
      count = 80;
      fprintf(stderr, "\n");
    }

    obj = object_after(oh, obj);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "free: %" PRIdPTR ", used: %" PRIdPTR ", pinned: %" PRIdPTR "\n", free_count, used_count, pin_count);

}


void print_pic_entries(struct object_heap* oh, struct CompiledMethod* method) {
  if ((struct Object*)method->calleeCount == oh->cached.nil) return;

  for (word_t i = 0; i < array_size(method->calleeCount); i += CALLER_PIC_ENTRY_SIZE) {
    struct MethodDefinition* def = (struct MethodDefinition*)method->calleeCount->elements[i+PIC_CALLEE];
    if ((struct Object*)def == oh->cached.nil) continue;
    struct CompiledMethod* picMethod = (struct CompiledMethod*)def->method;
    struct Object* traitsWindow = picMethod->base.map->delegates->elements[0];
    assert (traitsWindow == oh->cached.compiled_method_window || traitsWindow == oh->cached.primitive_method_window);
    struct Symbol* picSelector = picMethod->selector;
    print_type(oh, (struct Object*)picSelector);
  }

}



void print_code_disassembled(struct object_heap* oh, struct OopArray* slatecode) {
  std::vector<struct Object*> code;
  optimizer_append_code_to_vector(slatecode, code);
  print_code(oh, code);
}

