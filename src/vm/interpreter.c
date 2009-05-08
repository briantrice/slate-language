#include "slate.h"


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
  printf("stack allocate, new stack pointer: %" PRIdPTR "\n", i->stackPointer);
#endif

}

void interpreter_stack_push(struct object_heap* oh, struct Interpreter* i, struct Object* value) {

#ifdef PRINT_DEBUG_STACK_PUSH
  printf("Stack push at %" PRIdPTR " (fp=%" PRIdPTR "): ", i->stackPointer, i->framePointer); print_type(oh, value);
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
    printf("popping from stack, new stack pointer: %" PRIdPTR "\n", i->stackPointer);
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
  printf("Unhandled signal: "); print_symbol(selector); printf(" with %" PRIdPTR " arguments: \n", n);
  for (i = 0; i<n; i++) {
    printf("arg[%" PRIdPTR "] = ", i);
    print_detail(oh, args[i]);
  }
  printf("partial stack: \n");
  /*print_stack_types(oh, 200);*/
  print_backtrace(oh);
  assert(0);
  exit(1);

}


void interpreter_signal(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) {
  
  struct Closure* method;
  struct Symbol* selector = (struct Symbol*)signal;
  struct MethodDefinition* def = method_dispatch_on(oh, selector, args, n, NULL);
  if (def == NULL) {
    unhandled_signal(oh, selector, n, args);
  }
  /*take this out when the debugger is mature*/
  /*  print_backtrace(oh);
      assert(0);*/
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

bool_t interpreter_dispatch_optional_keyword(struct object_heap* oh, struct Interpreter * i, struct Object* key, struct Object* value) {

  struct OopArray* optKeys = i->method->optionalKeywords;

  word_t optKey, size = array_size(optKeys);

  for (optKey = 0; optKey < size; optKey++) {
    if (optKeys->elements[optKey] == key) {
      if (i->method->heapAllocate == oh->cached.true_object) {
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


  word_t inputs, framePointer, j, beforeCallStackPointer;
  struct Object** vars;
  struct LexicalContext* lexicalContext;
  struct CompiledMethod* method;
  GC_VOLATILE struct Object* args[16];
  

#ifdef PRINT_DEBUG
  printf("apply to arity %" PRIdPTR "\n", n);
#endif

  method = closure->method;
  inputs = object_to_smallint(method->inputVariables);

  method->callCount = smallint_to_object(object_to_smallint(method->callCount) + 1);


  /*make sure they are pinned*/
  assert(n <= 16);
  copy_words_into(argsNotStack, n, args);


  /* optimize the callee function after a set number of calls*/
  if (method->callCount > (struct Object*)CALLEE_OPTIMIZE_AFTER && method->isInlined == oh->cached.false_object) {
    method_optimize(oh, method);
  }

  
  if (n < inputs || (n > inputs && method->restVariable != oh->cached.true_object)) {
    GC_VOLATILE struct OopArray* argsArray = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n);
    copy_words_into(args, n, argsArray->elements);
    interpreter_signal_with_with(oh, i, get_special(oh, SPECIAL_OOP_WRONG_INPUTS_TO), (struct Object*) argsArray, (struct Object*)method, NULL, resultStackPointer);
    return;
  }


  framePointer = i->stackPointer + FUNCTION_FRAME_SIZE;
  /* we save this so each function call doesn't leak the stack */
  beforeCallStackPointer = i->stackPointer;

  if (method->heapAllocate == oh->cached.true_object) {
    lexicalContext = (struct LexicalContext*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_LEXICAL_CONTEXT_PROTO), object_to_smallint(method->localVariables));
    lexicalContext->framePointer = smallint_to_object(framePointer);
    interpreter_stack_allocate(oh, i, FUNCTION_FRAME_SIZE + object_to_smallint(method->registerCount));
    vars = &lexicalContext->variables[0];
    for (j = 0; j < inputs; j++) {
      heap_store_into(oh, (struct Object*)lexicalContext, args[j]);
    }

  } else {
    lexicalContext = (struct LexicalContext*) oh->cached.nil;
    interpreter_stack_allocate(oh, i, FUNCTION_FRAME_SIZE /*frame size in words*/ + object_to_smallint(method->localVariables) + object_to_smallint(method->registerCount));
    vars = &i->stack->elements[framePointer];
    for (j = 0; j < inputs; j++) {
      heap_store_into(oh, (struct Object*)i->stack, args[j]);
    }
  }


  copy_words_into(args, inputs, vars);
  i->stack->elements[framePointer - 6] = smallint_to_object(beforeCallStackPointer);
  i->stack->elements[framePointer - 5] = smallint_to_object(resultStackPointer);
  i->stack->elements[framePointer - 4] = smallint_to_object(i->codePointer);
  i->stack->elements[framePointer - 3] = (struct Object*) closure;
  i->stack->elements[framePointer - 2] = (struct Object*) lexicalContext;
  i->stack->elements[framePointer - 1] = smallint_to_object(i->framePointer);

  heap_store_into(oh, (struct Object*)i->stack, (struct Object*)closure);
  heap_store_into(oh, (struct Object*)i->stack, (struct Object*)lexicalContext);

#ifdef PRINT_DEBUG_FUNCALL_VERBOSE
  printf("i->stack->elements[%" PRIdPTR "(fp-4)] = \n", framePointer - 4);
  print_detail(oh, i->stack->elements[framePointer - 4]);
  printf("i->stack->elements[%" PRIdPTR "(fp-3)] = \n", framePointer - 3);
  print_detail(oh, i->stack->elements[framePointer - 3]);
  printf("i->stack->elements[%" PRIdPTR "(fp-2)] = \n", framePointer - 2);
  print_detail(oh, i->stack->elements[framePointer - 2]);
  printf("i->stack->elements[%" PRIdPTR "(fp-1)] = \n", framePointer - 1);
  print_detail(oh, i->stack->elements[framePointer - 1]);
#endif

  profiler_leave_current(oh);
  profiler_enter_method(oh, (struct Object*)closure);

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
    GC_VOLATILE struct OopArray* restArgs = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), n - inputs);
    copy_words_into(args+inputs, n - inputs, restArgs->elements);
    vars[inputs+array_size(method->optionalKeywords)] = (struct Object*) restArgs;
    heap_store_into(oh, (struct Object*)lexicalContext, (struct Object*)restArgs);/*fix, not always right*/
  } else {
    if (method->restVariable == oh->cached.true_object) {
      vars[inputs+array_size(method->optionalKeywords)] = get_special(oh, SPECIAL_OOP_ARRAY_PROTO);
    }
  }
  if (opts != NULL) {
    interpreter_dispatch_optionals(oh, i, opts);
  }
}


void send_to_through_arity_with_optionals(struct object_heap* oh,
                                                  struct Symbol* selector, struct Object* args[],
                                                  struct Object* dispatchers[], word_t arity, struct OopArray* opts,
                                          word_t resultStackPointer/*where to put the return value in the stack*/) {
  GC_VOLATILE struct OopArray* argsArray;
  struct Closure* method;
  struct Object* traitsWindow;
  GC_VOLATILE struct Object* argsStack[16];
  GC_VOLATILE struct Object* dispatchersStack[16];
  struct MethodDefinition* def;
  struct CompiledMethod* callerMethod;
  word_t addToPic = FALSE;
  callerMethod = oh->cached.interpreter->method;

  /*make sure they are pinned*/
  assert(arity <= 16);
  /*for gc*/
  copy_words_into(args, arity, argsStack);
  copy_words_into(dispatchers, arity, dispatchersStack);

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
    printf("calling primitive: %" PRIdPTR "\n", object_to_smallint(((struct PrimitiveMethod*)method)->index));
#endif
    profiler_leave_current(oh);
    profiler_enter_method(oh, (struct Object*)method);
    primitives[object_to_smallint(((struct PrimitiveMethod*)method)->index)](oh, args, arity, opts, resultStackPointer);
    profiler_leave_current(oh);
    profiler_enter_method(oh, (struct Object*)oh->cached.interpreter->closure);
  } else if (traitsWindow == oh->cached.compiled_method_window || traitsWindow == oh->cached.closure_method_window) {
    interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, method, args, arity, opts, resultStackPointer);
  } else {
    GC_VOLATILE struct OopArray* optsArray = NULL;
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


bool_t interpreter_return_result(struct object_heap* oh, struct Interpreter* i, word_t context_depth, struct Object* result, word_t prevCodePointer) {
  /* Implements a non-local return with a value, specifying the block
   * to return from via lexical offset. Returns a success Boolean. The
   * prevCodePointer is the location of the return instruction that
   * returned. In the case that we have an ensure handler to run, we
   * will use that code pointer so the return is executed again.
   */
  word_t framePointer;
  word_t ensureHandlers;
  word_t resultStackPointer;

#ifdef PRINT_DEBUG_FUNCALL
      printf("interpreter_return_result BEFORE\n");
      printf("stack pointer: %" PRIdPTR "\n", i->stackPointer);
      printf("frame pointer: %" PRIdPTR "\n", i->framePointer);
      print_stack_types(oh, 16);
#endif


  if (context_depth == 0) {
    framePointer = i->framePointer;
  } else {
    struct LexicalContext* targetContext = i->closure->lexicalWindow[context_depth-1];
    framePointer = object_to_smallint(targetContext->framePointer);
    if (framePointer > i->stackPointer || (struct Object*)targetContext != i->stack->elements[framePointer-2]) {
      resultStackPointer = (word_t)i->stack->elements[framePointer - 5]>>1;
      interpreter_signal_with_with(oh, i, get_special(oh, SPECIAL_OOP_MAY_NOT_RETURN_TO),
                                   (struct Object*)i->closure, (struct Object*) targetContext, NULL, resultStackPointer);
      return 1;
    }
  }

  resultStackPointer = (word_t)i->stack->elements[framePointer - 5]>>1;

  /*store the result before we get interrupted for a possible finalizer... fixme i'm not sure
   if this is right*/
  if (result != NULL) {
#ifdef PRINT_DEBUG_STACK
    printf("setting stack[%" PRIdPTR "] = ", resultStackPointer); print_object(result);
#endif

    i->stack->elements[resultStackPointer] = result;
    heap_store_into(oh, (struct Object*)i->stack, (struct Object*)result);
  }

  ensureHandlers = object_to_smallint(i->ensureHandlers);
  if (framePointer <= ensureHandlers) {
    struct Object* ensureHandler = i->stack->elements[ensureHandlers+1];
#ifdef PRINT_DEBUG_ENSURE
  printf("current ensure handlers at %" PRIdPTR "\n", object_to_smallint(i->ensureHandlers));
#endif
    assert(object_to_smallint(i->stack->elements[ensureHandlers]) < 0x1000000); /*sanity check*/
    i->ensureHandlers = i->stack->elements[ensureHandlers];
#ifdef PRINT_DEBUG_ENSURE
  printf("reset ensure handlers at %" PRIdPTR "\n", object_to_smallint(i->ensureHandlers));
#endif

    interpreter_stack_push(oh, i, smallint_to_object(i->stackPointer));
    interpreter_stack_push(oh, i, smallint_to_object(resultStackPointer));
    interpreter_stack_push(oh, i, smallint_to_object(prevCodePointer));
    interpreter_stack_push(oh, i, get_special(oh, SPECIAL_OOP_ENSURE_MARKER));
    interpreter_stack_push(oh, i, oh->cached.nil);
    interpreter_stack_push(oh, i, smallint_to_object(i->framePointer));
    i->codePointer = 0;
    i->framePointer = i->stackPointer;
    /*assert(0); fixme not sure if this is totally the right way to set up the stack yet*/
    {
      interpreter_apply_to_arity_with_optionals(oh, i, (struct Closure*) ensureHandler, NULL, 0, NULL, resultStackPointer);
    }
    return 1;
  }
  i->stackPointer = object_to_smallint(i->stack->elements[framePointer - 6]);
  i->framePointer = object_to_smallint(i->stack->elements[framePointer - 1]);
  if (i->framePointer < FUNCTION_FRAME_SIZE) {
    /* returning from the last function on the stack seems to happen when the user presses Ctrl-D */
    exit(0);
    return 0;
  }

  profiler_leave_current(oh);
  profiler_enter_method(oh, (struct Object*)i->stack->elements[i->framePointer - 3]);


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
      printf("stack pointer: %" PRIdPTR "\n", i->stackPointer);
      printf("frame pointer: %" PRIdPTR "\n", i->framePointer);
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
  GC_VOLATILE struct OopArray* argsArray;
  struct Closure* method;
  struct CompiledMethod* resender;
  struct MethodDefinition* def;
  GC_VOLATILE struct Object* argsStack[16]; /*for pinning*/

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
  args = (resender->heapAllocate == oh->cached.true_object)? lexicalContext->variables : &i->stack->elements[framePointer];


  selector = resender->selector;
  n = object_to_smallint(resender->inputVariables);
  assert(n <= 16);
  copy_words_into(args, n, argsStack);

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
    printf("calling primitive: %" PRIdPTR "\n", object_to_smallint(((struct PrimitiveMethod*)method)->index));
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
    GC_VOLATILE struct OopArray* optsArray;
    GC_VOLATILE struct OopArray* optKeys;
    GC_VOLATILE struct OopArray* signalOpts;
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


void interpret(struct object_heap* oh) {

 /* we can set a conditional breakpoint if the vm crash is consistent */
  unsigned long int instruction_counter = 0;

#ifdef PRINT_DEBUG
  printf("Interpret: img:%p size:%" PRIdPTR " spec:%p next:%" PRIdPTR "\n",
         (void*)oh->memoryOld, oh->memoryOldSize, (void*)oh->special_objects_oop, oh->lastHash);
  printf("Special oop: "); print_object((struct Object*)oh->special_objects_oop);
#endif

  cache_specials(oh);

#ifdef PRINT_DEBUG
  printf("Interpreter stack: "); print_object((struct Object*)oh->cached.interpreter);
  printf("Interpreter stack size: %" PRIdPTR "\n", oh->cached.interpreter->stackSize);
  printf("Interpreter stack pointer: %" PRIdPTR "\n", oh->cached.interpreter->stackPointer);
  printf("Interpreter frame pointer: %" PRIdPTR "\n", oh->cached.interpreter->framePointer);
#endif 
  /*fixme this should only be called in the initial bootstrap because
    the stack doesn't have enough room for the registers */
  if (oh->cached.interpreter->framePointer == FUNCTION_FRAME_SIZE 
      && oh->cached.interpreter->stackPointer == FUNCTION_FRAME_SIZE
      && oh->cached.interpreter->stackSize == 16) {
    interpreter_stack_allocate(oh, oh->cached.interpreter, object_to_smallint(oh->cached.interpreter->method->registerCount));
  }

  for (;;) {
    word_t op, prevPointer;
    struct Interpreter* i = oh->cached.interpreter; /*it won't move while we are in here */

    /*while (oh->cached.interpreter->codePointer < oh->cached.interpreter->codeSize) {*/
    /*optimize and make sure every function has manual return opcodes*/
    for(;;) {
      
      if (oh->interrupt_flag) {
        return;
      }


      if (globalInterrupt) {
        printf("\nInterrupting...\n");
        interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_TYPE_ERROR_ON), oh->cached.nil, NULL, object_to_smallint(i->stack->elements[i->framePointer-5]));
        globalInterrupt = 0;
      }


      
      instruction_counter++;
      prevPointer = i->codePointer;
      op = (word_t)i->method->code->elements[i->codePointer];
#ifdef PRINT_DEBUG_CODE_POINTER
      printf("(%" PRIdPTR "/%" PRIdPTR ") %" PRIdPTR " ", i->codePointer, i->codeSize, op>>1);
#endif
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
          printf("send message fp: %" PRIdPTR ", result: %" PRIdPTR ", arity: %" PRIdPTR ", message: ", i->framePointer, result, arity);
          print_type(oh, selector);
#endif
          assert(arity <= 16);
          for (k=0; k<arity; k++) {
            word_t argReg = SSA_NEXT_PARAM_SMALLINT;
            args[k] = SSA_REGISTER(argReg);
#ifdef PRINT_DEBUG_OPCODES
            printf("args[%d@%" PRIdPTR "] = ", k, argReg);
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
          GC_VOLATILE struct Object* args[16];
          result = SSA_NEXT_PARAM_SMALLINT;
          selector = SSA_NEXT_PARAM_OBJECT;
          arity = SSA_NEXT_PARAM_SMALLINT;
          optsArrayReg = SSA_NEXT_PARAM_SMALLINT;

#ifdef PRINT_DEBUG_OPCODES
          printf("send message with opts fp: %" PRIdPTR ", result: %" PRIdPTR " arity: %" PRIdPTR ", opts: %" PRIdPTR ", message: ", i->framePointer, result, arity, optsArrayReg);
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
          GC_VOLATILE struct OopArray* array;
          result = SSA_NEXT_PARAM_SMALLINT;
          size = SSA_NEXT_PARAM_SMALLINT;

#ifdef PRINT_DEBUG_OPCODES
          printf("new array, result: %" PRIdPTR ", size: %" PRIdPTR "\n", result, size);
#endif
          array = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), size);
          for (k = 0; k < size; k++) {
            array->elements[k] = SSA_REGISTER(SSA_NEXT_PARAM_SMALLINT);
          }
          heap_store_into(oh, (struct Object*)i->stack, (struct Object*)array);
#ifdef PRINT_DEBUG_STACK
    printf("%" PRIuPTR "u: setting stack[%" PRIdPTR "] = ", instruction_counter, REG_STACK_POINTER(result)); print_object((struct Object*)array);
#endif
          ASSERT_VALID_REGISTER(result);
          SSA_REGISTER(result) = (struct Object*) array;
          break;
        }
      case OP_NEW_CLOSURE:
        {
          word_t result;
          GC_VOLATILE struct CompiledMethod* block;
          GC_VOLATILE struct Closure* newClosure;
          result = SSA_NEXT_PARAM_SMALLINT;
          block = (struct CompiledMethod*)SSA_NEXT_PARAM_OBJECT;
#ifdef PRINT_DEBUG_OPCODES
          printf("new closure, result: %" PRIdPTR ", block", result);
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
#if 1
          if (object_is_free((struct Object*)block)) {
            /*printf("%d\n", instruction_counter);*/
          }
#endif
          heap_store_into(oh, (struct Object*)newClosure, (struct Object*)block);
          heap_store_into(oh, (struct Object*)i->stack, (struct Object*)newClosure);
#ifdef PRINT_DEBUG_STACK
    printf("%" PRIuPTR "u: setting stack[%" PRIdPTR "] = ", instruction_counter, REG_STACK_POINTER(result)); print_object((struct Object*)newClosure);
#endif
          ASSERT_VALID_REGISTER(result);
          SSA_REGISTER(result) = (struct Object*) newClosure;
          break;
        }
      case OP_LOAD_LITERAL:
        {
          word_t destReg;
          GC_VOLATILE struct Object* literal;
          destReg = SSA_NEXT_PARAM_SMALLINT;
          literal = SSA_NEXT_PARAM_OBJECT;
#ifdef PRINT_DEBUG_OPCODES
          printf("load literal into reg %" PRIdPTR ", value: ", destReg);
          print_type(oh, literal);
#endif
          heap_store_into(oh, (struct Object*)i->stack, literal);
#ifdef PRINT_DEBUG_STACK
    printf("%" PRIuPTR "u: setting stack[%" PRIdPTR "] = ", instruction_counter, REG_STACK_POINTER(destReg)); print_object((struct Object*)literal);
#endif
          ASSERT_VALID_REGISTER(destReg);
          SSA_REGISTER(destReg) = literal;
          break;
        }
      case OP_RESEND_MESSAGE:
        {
          word_t resultRegister, lexicalOffset;
          resultRegister = SSA_NEXT_PARAM_SMALLINT;
          lexicalOffset = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("resend message reg %" PRIdPTR ", offset: %" PRIdPTR "\n", resultRegister, lexicalOffset);
#endif
          interpreter_resend_message(oh, i, lexicalOffset, i->framePointer + resultRegister);
          break;
        }
      case OP_LOAD_VARIABLE:
        {
          word_t var;
          var = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("load var %" PRIdPTR "\n", var);
#endif
          if (i->method->heapAllocate == oh->cached.true_object) {
            heap_store_into(oh, (struct Object*)i->stack, (struct Object*)i->lexicalContext);
#ifdef PRINT_DEBUG_STACK
            printf("%" PRIuPTR "u: setting stack[%" PRIdPTR "] = ", instruction_counter, REG_STACK_POINTER(var)); print_object(i->lexicalContext->variables[var]);
#endif
            ASSERT_VALID_REGISTER(var);
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
          printf("store var %" PRIdPTR "\n", var);
#endif
          if (i->method->heapAllocate == oh->cached.true_object) {
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
          printf("load free var to: %" PRIdPTR ", lexoffset: %" PRIdPTR ", index: %" PRIdPTR "\n", destReg, lexOffset, varIndex);
#endif
          heap_store_into(oh, (struct Object*)i->stack, (struct Object*)i->closure->lexicalWindow[lexOffset-1]);
#ifdef PRINT_DEBUG_STACK
          printf("%" PRIuPTR "u: setting stack[%" PRIdPTR "] = ", instruction_counter, REG_STACK_POINTER(destReg)); print_object(i->closure->lexicalWindow[lexOffset-1]->variables[varIndex]);
#endif
          ASSERT_VALID_REGISTER(destReg);
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
          printf("store free var from: %" PRIdPTR ", lexoffset: %" PRIdPTR ", index: %" PRIdPTR "\n", srcReg, lexOffset, varIndex);
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
          printf("move reg %" PRIdPTR ", %" PRIdPTR "\n", destReg, srcReg);
#endif
          heap_store_into(oh, (struct Object*)i->stack, SSA_REGISTER(srcReg));
#ifdef PRINT_DEBUG_STACK
          printf("%" PRIuPTR "u: setting stack[%" PRIdPTR "] = ", instruction_counter, REG_STACK_POINTER(destReg)); print_object(SSA_REGISTER(srcReg));
#endif
          ASSERT_VALID_REGISTER(destReg);
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
          printf("is identical %" PRIdPTR ", %" PRIdPTR "\n", destReg, srcReg);
#endif
          ASSERT_VALID_REGISTER(resultReg);
          
          SSA_REGISTER(resultReg) = (SSA_REGISTER(destReg) == SSA_REGISTER(srcReg)) ? oh->cached.true_object : oh->cached.false_object;
#ifdef PRINT_DEBUG_STACK
          printf("%" PRIuPTR "u: setting stack[%" PRIdPTR "] = ", instruction_counter, REG_STACK_POINTER(resultReg)); print_object(SSA_REGISTER(resultReg));
#endif
          break;
        }
      case OP_BRANCH_KEYED:
        {
          word_t tableReg, keyReg;
          keyReg = SSA_NEXT_PARAM_SMALLINT;
          tableReg = SSA_NEXT_PARAM_SMALLINT;
          /*assert(0);*/
#ifdef PRINT_DEBUG_OPCODES
          printf("branch keyed: %" PRIdPTR "/%" PRIdPTR "\n", tableReg, keyReg);
#endif
          interpreter_branch_keyed(oh, i, (struct OopArray*)SSA_REGISTER(tableReg), SSA_REGISTER(keyReg));
          break;
        }
      case OP_BRANCH_IF_TRUE:
        {
          word_t condReg, offset;
          GC_VOLATILE struct Object* val;
          condReg = SSA_NEXT_PARAM_SMALLINT;
          offset = SSA_NEXT_PARAM_SMALLINT - 1;

          val = SSA_REGISTER(condReg);

#ifdef PRINT_DEBUG_OPCODES
          printf("branch if true: %" PRIdPTR ", offset: %" PRIdPTR ", val: ", condReg, offset);
          print_type(oh, val);
#endif
          if (val == oh->cached.true_object) {
            i->codePointer = i->codePointer + offset;
          } else {
            if (val != oh->cached.false_object) {
              i->codePointer = i->codePointer - 3;
              interpreter_signal_with(oh, i, get_special(oh, SPECIAL_OOP_NOT_A_BOOLEAN), val, NULL, condReg /*fixme*/);
            }
          }
          break;
        }
      case OP_BRANCH_IF_FALSE:
        {
          word_t condReg, offset;
          GC_VOLATILE struct Object* val;
          condReg = SSA_NEXT_PARAM_SMALLINT;
          offset = SSA_NEXT_PARAM_SMALLINT - 1;

          val = SSA_REGISTER(condReg);

#ifdef PRINT_DEBUG_OPCODES
          printf("branch if false: %" PRIdPTR ", offset: %" PRIdPTR ", val: ", condReg, offset);
          print_type(oh, val);
#endif
          if (val == oh->cached.false_object) {
            i->codePointer = i->codePointer + offset;
          } else {
            if (val != oh->cached.true_object) {
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
          printf("jump to offset: %" PRIdPTR "\n", offset);
#endif
          i->codePointer = i->codePointer + offset;
          
          break;
        }
      case OP_LOAD_ENVIRONMENT:
        {
          word_t next_param;
          next_param = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("load environment into reg %" PRIdPTR ", value: ", next_param);
          print_type(oh, i->method->environment);
#endif
          heap_store_into(oh, (struct Object*)i->stack, (struct Object*)i->method->environment);
          ASSERT_VALID_REGISTER(next_param);
          SSA_REGISTER(next_param) = i->method->environment;
#ifdef PRINT_DEBUG_STACK
          printf("%" PRIuPTR "u: setting stack[%" PRIdPTR "] = ", instruction_counter, REG_STACK_POINTER(next_param)); print_object(SSA_REGISTER(next_param));
#endif
          break;
        }
      case OP_RETURN_REGISTER:
        {
          word_t reg;
          reg = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("return reg %" PRIdPTR ", value: ", reg);
          print_type(oh, SSA_REGISTER(reg));
#endif
#ifdef PRINT_DEBUG_STACK
          printf("%" PRIuPTR "u: ", instruction_counter);
#endif
          ASSERT_VALID_REGISTER(reg);
          interpreter_return_result(oh, i, 0, SSA_REGISTER(reg), prevPointer);
#ifdef PRINT_DEBUG_OPCODES
          printf("in function: \n");
          print_type(oh, (struct Object*)i->method);
#endif
          break;
        }
      case OP_RESUME: /*returning the result (or lack of) from the finalizer of a prim_ensure*/
        {
          PRINTOP("op: resume\n");
          /*one for the resume and one for the function that we
            interrupted the return from to run the finalizer*/
          /*interpreter_return_result(oh, i, 0, NULL);*/
          interpreter_return_result(oh, i, 0, NULL, prevPointer);
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
          printf("return result reg: %" PRIdPTR ", offset: %" PRIdPTR ", value: ", reg, offset);
          print_type(oh, SSA_REGISTER(reg));
#endif
#ifdef PRINT_DEBUG_STACK
          printf("%" PRIuPTR "u: ", instruction_counter);
#endif
          ASSERT_VALID_REGISTER(reg);
          interpreter_return_result(oh, i, offset, SSA_REGISTER(reg), prevPointer);
#ifdef PRINT_DEBUG_OPCODES
          printf("in function: \n");
          print_type(oh, (struct Object*)i->method);
#endif
          break;
        }
      case OP_RETURN_VALUE:
        {
          GC_VOLATILE struct Object* obj;
          PRINTOP("op: return obj\n");
          obj = SSA_NEXT_PARAM_OBJECT;
#ifdef PRINT_DEBUG_STACK
          printf("%" PRIuPTR "u: ", instruction_counter);
#endif
          interpreter_return_result(oh, i, 0, obj, prevPointer);
#ifdef PRINT_DEBUG_OPCODES
          printf("in function: \n");
          print_type(oh, (struct Object*)i->method);
#endif
          break;
        }
#if 0
      case OP_ALLOCATE_REGISTERS:
        {
          word_t reg;
          reg = SSA_NEXT_PARAM_SMALLINT;
#ifdef PRINT_DEBUG_OPCODES
          printf("allocate %" PRIdPTR " registers\n", reg);
#endif
          interpreter_stack_allocate(oh, i, reg);
#ifdef PRINT_DEBUG_OPCODES
          printf("new fp: %" PRIdPTR " sp: %" PRIdPTR "\n", i->framePointer, i->stackPointer);
#endif
          
          break;
        }
#endif
      default:
        printf("error bad opcode... %" PRIdPTR "\n", op>>1);
        assert(0);
        break;
      }
      

    }
  }/* while (interpreter_return_result(oh, oh->cached.interpreter, 0, NULL, prevPointer));*/



}
