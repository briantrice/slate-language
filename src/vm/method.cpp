#include "slate.hpp"


word_t method_pic_hash(struct object_heap* oh, struct CompiledMethod* callerMethod, word_t arity, struct Object* args[]) {
  word_t arraySize = array_size(callerMethod->calleeCount);
  word_t entryStart = (hash_selector(oh, NULL, args, arity) % (arraySize/CALLER_PIC_ENTRY_SIZE)) * CALLER_PIC_ENTRY_SIZE;
  return entryStart;
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
  printf("' (arity: %" PRIdPTR ")\n", arity);
  for (i = 0; i < arity; i++) {
    printf("arguments[%" PRIdPTR "] (%p) = ", i, (void*)arguments[i]); print_type(oh, arguments[i]);
  }
  /*  printf("resend: "); print_object(resendMethod);*/
#endif

  dispatch = NULL;
  slotLocation = NULL;

#ifndef SLATE_DISABLE_METHOD_CACHE
  if (resendMethod == NULL && arity <= METHOD_CACHE_ARITY) {
    dispatch = method_check_cache(oh, name, arguments, arity);
    if (dispatch != NULL) return dispatch;
  }
#endif

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
              printf("found role index %" PRIdPTR " <%p> for '%s' foundPos: %" PRIuPTR "x dispatchPos: %" PRIuPTR "x\n",
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

#ifndef SLATE_DISABLE_METHOD_CACHE
  if (dispatch != NULL && resendMethod == 0 && arity < METHOD_CACHE_ARITY) {
    method_save_cache(oh, dispatch, name, arguments, arity);
  }
#endif

  return dispatch;
}


bool method_on_call_stack(struct object_heap* oh, struct CompiledMethod* method) {

  struct Interpreter* i = oh->cached.interpreter;
  word_t fp = i->framePointer;
  struct Closure* closure = (struct Closure*)i->method;
  do {
    if (closure->method == method) return true;
    fp = object_to_smallint(i->stack->elements[fp - FRAME_OFFSET_PREVIOUS_FRAME_POINTER]);
    if (fp < FUNCTION_FRAME_SIZE) break;
    closure = (struct Closure*)i->stack->elements[fp - FRAME_OFFSET_METHOD];
  } while (fp >= FUNCTION_FRAME_SIZE);

  if (closure->method == method) return true;
  return false;
}


void method_unoptimize(struct object_heap* oh, struct CompiledMethod* method) {
#ifdef PRINT_DEBUG_UNOPTIMIZER
  printf("Unoptimizing '"); print_symbol(method->selector); printf("'\n");
#endif
  if (method_on_call_stack(oh, method)) {
    printf("Fixme cannot unoptimizing because on call stack: '"); print_symbol(method->selector); printf("'\n");
    return;
  }

  method->code = method->oldCode;
  heap_store_into(oh, (struct Object*) method->code, (struct Object*) method->oldCode);
  method->isInlined = oh->cached.false_object;
  method->calleeCount = (struct OopArray*)oh->cached.nil;
  method->callCount = smallint_to_object(0);
  oh->optimizedMethods.erase(method);

}


void method_remove_optimized_sending(struct object_heap* oh, struct Symbol* symbol) {
  if (oh->optimizedMethods.empty()) return;
  for (std::multiset<struct CompiledMethod*>::iterator i = oh->optimizedMethods.begin(); i != oh->optimizedMethods.end(); i++) {
    struct CompiledMethod* method = *i;
    /*resend?*/
    if (method->selector == symbol) {
      method_unoptimize(oh, method);
      continue;
    }
    for (int j = 0; j < array_size(method->selectors); j++) {
      if (array_elements(method->selectors)[j] == (struct Object*)symbol) {
        method_unoptimize(oh, method);
        break;
      }
    }
  }

}



void method_optimize(struct object_heap* oh, struct CompiledMethod* method) {

  /* only optimize old objects because they don't move in memory and
   * we don't want to have to update our method cache every gc */
  if (object_is_young(oh, (struct Object*)method)) return;


  // make sure we don't optimize something already on the stack
  // since a return will put us possibly at a different op code
  if (method_on_call_stack(oh, method)) {
    return;
  }

  if (!optimizer_method_can_be_optimized(oh, method)) {
    //don't bother trying anymore
    method->isInlined = oh->cached.true_object;
    method->oldCode = method->code;
    method->nextInlineAtCallCount = oh->cached.nil;
    return;
  }

#ifdef PRINT_DEBUG_OPTIMIZER
  printf("Optimizing '"); print_symbol(method->selector); printf("'\n");
#endif


#ifdef PRINT_DEBUG_OPTIMIZER2
  method_print_debug_info(oh, method);
  printf("This method is called by:\n");
  method_print_debug_info(oh, oh->cached.interpreter->method);
#endif

  if ((struct Object*)method->oldCode == oh->cached.nil) {
    method->oldCode = method->code;
    heap_store_into(oh, (struct Object*) method->oldCode, (struct Object*) method->code);

  }
  //whether to start with a fresh slate
  //not starting may give us another degree of inlining
  heap_store_into(oh, (struct Object*) method->oldCode, (struct Object*) method->code);

  method->isInlined = oh->cached.true_object;
  oh->optimizedMethods.insert(method);
#ifdef SLATE_SHOW_INLINER_CODE
  printf("before:\n");
  print_type(oh, (struct Object*)method->selector);
  print_code_disassembled(oh, method->code);
#endif
  optimizer_inline_callees(oh, method);
#ifdef SLATE_SHOW_INLINER_CODE
  printf("after:\n");
  print_code_disassembled(oh, method->code);
#endif
  
}

void method_pic_setup(struct object_heap* oh, struct CompiledMethod* caller) {

  caller->calleeCount = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), CALLER_PIC_SIZE*CALLER_PIC_ENTRY_SIZE);
  heap_store_into(oh, (struct Object*) caller, (struct Object*) caller->calleeCount);

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


void method_pic_insert(struct object_heap* oh, struct OopArray* calleeCount, 
                       struct Object* picEntry[], struct MethodDefinition* def,
                         word_t arity, struct Object* args[]) {

  word_t j;
  picEntry[PIC_CALLEE] = (struct Object*)def;
  picEntry[PIC_CALLEE_ARITY] = smallint_to_object(arity);
  picEntry[PIC_CALLEE_COUNT] = smallint_to_object(1);
  picEntry[PIC_CALLEE_MAPS] = 
    (struct Object*)heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), arity);

  heap_store_into(oh, (struct Object*) calleeCount, picEntry[PIC_CALLEE]);
  heap_store_into(oh, (struct Object*) calleeCount, picEntry[PIC_CALLEE_MAPS]);
  
  for (j = 0; j < arity; j++) {
    ((struct OopArray*)picEntry[PIC_CALLEE_MAPS])->elements[j] = (struct Object*)object_get_map(oh, args[j]);
    heap_store_into(oh, picEntry[PIC_CALLEE_MAPS], ((struct OopArray*)picEntry[PIC_CALLEE_MAPS])->elements[j]);
  }

}

void method_pic_flush_caller_pics(struct object_heap* oh, struct CompiledMethod* callee) {

  int i;
  if (callee->base.map->delegates->elements[0] == oh->cached.closure_method_window) callee = callee->method;
  assert (callee->base.map->delegates->elements[0] == oh->cached.compiled_method_window);
  if (!object_is_smallint(callee->cachedInCallersCount)) return;
  for (i = 0; i < object_to_smallint(callee->cachedInCallersCount); i++) {
    /*this should reset the pic*/
    method_pic_setup(oh, (struct CompiledMethod*)callee->cachedInCallers->elements[i]);
  }


}

/*when a function is redefined, we need to know what PICs to flush. Here each method will
keep a list of all the pics that it is in */
void method_pic_add_callee_backreference(struct object_heap* oh,
                                         struct CompiledMethod* caller, struct CompiledMethod* callee) {

  if (callee->base.map->delegates->elements[0] == oh->cached.closure_method_window) callee = callee->method;
  if (callee->base.map->delegates->elements[0] == oh->cached.primitive_method_window) return;

  assert (callee->base.map->delegates->elements[0] == oh->cached.compiled_method_window);

  if ((struct Object*)callee->cachedInCallers == oh->cached.nil) {
    callee->cachedInCallers = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 32);
    heap_store_into(oh, (struct Object*)callee, (struct Object*)callee->cachedInCallers);
    callee->cachedInCallersCount = smallint_to_object(0);
  }

  if (object_to_smallint(callee->cachedInCallersCount) >= array_size(callee->cachedInCallers)) {
    struct OopArray* newArray = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), array_size(callee->cachedInCallers) * 2);
    copy_words_into(callee->cachedInCallers->elements, array_size(callee->cachedInCallers), newArray->elements);
    callee->cachedInCallers = newArray;
    heap_store_into(oh, (struct Object*)callee, (struct Object*)callee->cachedInCallers);
  }

  callee->cachedInCallers->elements[object_to_smallint(callee->cachedInCallersCount)] = (struct Object*)caller;
  heap_store_into(oh, (struct Object*)callee->cachedInCallers, (struct Object*)caller);
  callee->cachedInCallersCount =  smallint_to_object(object_to_smallint(callee->cachedInCallersCount) + 1);

}

void method_pic_add_callee(struct object_heap* oh, struct CompiledMethod* callerMethod, struct MethodDefinition* def,
                           word_t arity, struct Object* args[]) {
  word_t i;
  word_t arraySize = array_size(callerMethod->calleeCount);
  word_t entryStart = method_pic_hash(oh, callerMethod, arity, args);

#if 0
  if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) {
    Pinned<struct OopArray> pinArray(oh);
    pinArray = callerMethod->calleeCount;
    method_pic_insert(oh, callerMethod->calleeCount, &callerMethod->calleeCount->elements[i], def, arity, args);
    Pinned<struct CompiledMethod> defMethod(oh);
    defMethod = (struct CompiledMethod*) def->method;
    method_pic_add_callee_backreference(oh, callerMethod, (struct CompiledMethod*) defMethod);
    return;
  }

 /*this old slow code will fill up the hash table instead of only looking at the entry where the hash lands us*/
#else
  for (i = entryStart; i < arraySize; i+= CALLER_PIC_ENTRY_SIZE) {
    /* if it's nil, we need to insert it*/
    if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) {
      Pinned<struct OopArray> pinArray(oh);
      pinArray = callerMethod->calleeCount;
      method_pic_insert(oh, callerMethod->calleeCount, &callerMethod->calleeCount->elements[i], def, arity, args);
      Pinned<struct CompiledMethod> defMethod(oh);
      defMethod = (struct CompiledMethod*) def->method;
      method_pic_add_callee_backreference(oh, callerMethod, (struct CompiledMethod*) defMethod);
      return;
    }
  }
  for (i = 0; i < entryStart; i+= CALLER_PIC_ENTRY_SIZE) {
    /*MUST be same as first loop*/
    if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) {
      method_pic_insert(oh, callerMethod->calleeCount, &callerMethod->calleeCount->elements[i], def, arity, args);
      Pinned<struct CompiledMethod> defMethod(oh);
      defMethod = (struct CompiledMethod*) def->method;
      method_pic_add_callee_backreference(oh, callerMethod, (struct CompiledMethod*)defMethod);
      return;
    }
  }
#endif
}



struct MethodDefinition* method_pic_find_callee(struct object_heap* oh, struct CompiledMethod* callerMethod,
                                              struct Symbol* selector, word_t arity, struct Object* args[]) {

#ifdef SLATE_DISABLE_PIC_LOOKUP
  return NULL;
#endif

  Pinned<struct MethodDefinition> retval(oh);
  word_t i;
  word_t arraySize = array_size(callerMethod->calleeCount);
  word_t entryStart = method_pic_hash(oh, callerMethod, arity, args);


#if 0
  if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) return NULL;
  retval = method_pic_match_selector(oh, &callerMethod->calleeCount->elements[i], selector, arity, args, TRUE);
  if ((struct MethodDefinition*)retval != NULL) return retval;
  return NULL; /*only look at first match*/

#else /*this old code goes through the whole hash table which will take a while in a bad case*/
  for (i = entryStart; i < arraySize; i+= CALLER_PIC_ENTRY_SIZE) {
   if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) return NULL;
   retval = method_pic_match_selector(oh, &callerMethod->calleeCount->elements[i], selector, arity, args, TRUE);
   if ((struct MethodDefinition*)retval != NULL) return retval;
  }
  for (i = 0; i < entryStart; i+= CALLER_PIC_ENTRY_SIZE) {
    /*MUST be same as first loop*/
   if (callerMethod->calleeCount->elements[i+PIC_CALLEE] == oh->cached.nil) return NULL;
   retval = method_pic_match_selector(oh, &callerMethod->calleeCount->elements[i], selector, arity, args, TRUE);
   if ((struct MethodDefinition*)retval != NULL) return retval;
  }
#endif
  return NULL;
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

  word_t positions, i;
  struct Object* argBuffer[16];
  Pinned<struct MethodDefinition> def(oh);
  Pinned<struct MethodDefinition> oldDef(oh);

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

  copy_words_into(args, n, argBuffer); /* method_dispatch_on modifies its arguments (first argument)*/
  oldDef = method_dispatch_on(oh, selector, argBuffer, n, NULL);
  if (oldDef == (struct Object*)NULL || oldDef->dispatchPositions != positions || oldDef != method_is_on_arity(oh, oldDef->method, selector, args, n)) {
    oldDef = NULL;
  }
  if (oldDef != (struct Object*)NULL) {
    Pinned<struct CompiledMethod> oldDefMethod(oh);
    oldDefMethod = (struct CompiledMethod*)oldDef->method;
    method_pic_flush_caller_pics(oh, (struct CompiledMethod*)oldDefMethod);
  }
  def->method = method;
  heap_store_into(oh, (struct Object*) def, (struct Object*) method);
  def->dispatchPositions = positions;


  for (i = 0; i < n; i++) {
    if (!object_is_smallint(args[i]) && (struct Object*)args[i] != get_special(oh, SPECIAL_OOP_NO_ROLE)) {
      if (oldDef != (struct Object*)NULL) {
        object_remove_role(oh, args[i], selector, oldDef);
      }
      object_add_role_at(oh, args[i], selector, 1<<i, def);
    }
  }
  return def;
    
}

