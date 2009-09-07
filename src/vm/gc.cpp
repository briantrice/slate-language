#include "slate.hpp"
#ifdef SLATE_USE_MMAP
#include <sys/mman.h>
#endif


#ifdef GC_BUG_CHECK
void assert_good_object(struct object_heap* oh, struct Object* obj) {
  assert(obj->payloadSize < 40 * MB);
  assert(obj->objectSize < 40 * MB);
  assert(object_is_young(oh, obj) || object_is_old(oh, obj));
  assert(object_is_young(oh, (struct Object*)obj->map) || object_is_old(oh, (struct Object*)obj->map));
  assert(!object_is_free(obj));
  assert(!object_is_free((struct Object*)obj->map));
}

void heap_integrity_check(struct object_heap* oh, byte_t* memory, word_t memorySize) {
  struct Object* o = (struct Object*)memory;
  printf("GC integrity check...\n");

  while (object_in_memory(oh, o, memory, memorySize)) {

    if (object_is_free(o)) {
      assert(heap_what_points_to(oh, o, 0) == 0);
    } else {
      assert_good_object(oh, o);
    }
    o = object_after(oh, o);
  }
}

#endif



bool_t object_is_old(struct object_heap* oh, struct Object* oop) {
  return (oh->memoryOld <= (byte_t*)oop && (word_t)oh->memoryOld + oh->memoryOldSize > (word_t)oop);

}

bool_t object_is_young(struct object_heap* oh, struct Object* obj) {
  return (oh->memoryYoung <= (byte_t*)obj && (byte_t*)oh->memoryYoung + oh->memoryYoungSize > (byte_t*)obj);
  
}

bool_t object_in_memory(struct object_heap* oh, struct Object* oop, byte_t* memory, word_t memorySize) {
  return (memory <= (byte_t*)oop && (byte_t*)memory + memorySize > (byte_t*)oop);

}

struct Object* object_after(struct object_heap* heap, struct Object* o) {

  assert(object_total_size(o) != 0);

  return (struct Object*)inc_ptr(o, object_total_size(o));
}

bool_t object_is_marked(struct object_heap* heap, struct Object* o) {

  return (object_markbit(o) == heap->mark_color);
}

bool_t object_is_free(struct Object* o) {

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
  printf("Making %" PRIdPTR " words of free space at: %p\n", words, (void*)start);
#endif


  object_set_format(obj, TYPE_OBJECT);
  object_set_size(obj, words);
  payload_set_size(obj, 0); /*zero it out or old memory will haunt us*/
  obj->map = NULL;
  /*fix should we mark this?*/
  object_set_idhash(obj, ID_HASH_FREE);
#ifdef GC_MARK_FREED_MEMORY
  fill_bytes_with((byte_t*)(obj+1), words*sizeof(word_t)-sizeof(struct Object), 0xFE);
#endif
  return obj;
}

struct Object* heap_make_used_space(struct object_heap* oh, struct Object* obj, word_t words) {
  struct Object* x = heap_make_free_space(oh, obj, words);
  object_set_idhash(obj, heap_new_hash(oh));
  return x;
}


void heap_zero_pin_counts_from(struct object_heap* oh, byte_t* memory, word_t memorySize) {

  struct Object* o = (struct Object*)memory;

  while (object_in_memory(oh, o, memory, memorySize)) {
    if (!object_is_free(o)) {
      object_zero_pin_count(o);
    }
    o = object_after(oh, o);
  }


}


bool_t heap_initialize(struct object_heap* oh, word_t size, word_t limit, word_t young_limit, word_t next_hash, word_t special_oop, word_t cdid) {
#ifdef SLATE_USE_MMAP
  void* oldStart = (void*)0x10000000;
  void* youngStart = (void*)0x80000000;
#endif
  oh->memoryOldLimit = limit;
  oh->memoryYoungLimit = young_limit;

  oh->memoryOldSize = size;
  oh->memoryYoungSize = young_limit;

#ifdef SLATE_USE_MMAP
  assert((byte_t*)oldStart + limit < (byte_t*) youngStart);
  oh->memoryOld = (byte_t*)mmap((void*)oldStart, limit, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|0x20, -1, 0);
  oh->memoryYoung = (byte_t*)mmap((void*)youngStart, young_limit, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|0x20, -1, 0);
#else
  oh->memoryOld = (byte_t*)malloc(limit);
  oh->memoryYoung = (byte_t*)malloc(young_limit);
#endif
  oh->rememberedOldObjects.clear();

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
  oh->markStackSize = 4 * MB;
  oh->markStackPosition = 0;
  oh->markStack = (struct Object**)malloc(oh->markStackSize * sizeof(struct Object*));
  
  assert(oh->markStack != NULL);

  return 1;
}

void heap_close(struct object_heap* oh) {
#ifdef SLATE_USE_MMAP
  munmap(oh->memoryOld, oh->memoryOldLimit);
  munmap(oh->memoryYoung, oh->memoryYoungLimit);
#else
  free(oh->memoryOld);
  free(oh->memoryYoung);
#endif
}
bool_t object_is_pinned(struct object_heap* oh, struct Object* x) {
  return object_pin_count(x) > 0;
}

bool_t object_is_remembered(struct object_heap* oh, struct Object* x) {
  if (object_is_old(oh, x)) {
    oh->rememberedOldObjects.find(x) != oh->rememberedOldObjects.end();
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
  bool_t already_scavenged = 0, already_full_gc = 0;
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
      printf("Couldn't allocate %" PRIdPTR " bytes\n", bytes + sizeof(struct Object));
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



void heap_free_object(struct object_heap* oh, struct Object* obj) {
#ifdef PRINT_DEBUG_GC_MARKINGS
  printf("freeing "); print_object(obj);
#endif
  //fixme
  //  oh->optimizedMethods.erase((struct CompiledMethod*)obj);

#ifdef GC_BUG_CHECK
  assert(!object_is_pinned(oh, obj));
#endif

  /*we also might want to optimize the removal if we are profiling*/
  if (oh->currentlyProfiling) {
    profiler_delete_method(oh, obj);

  }

  heap_make_free_space(oh, obj, object_word_size(obj));

}


void heap_finish_gc(struct object_heap* oh) {
  method_flush_cache(oh, NULL);
  /*  cache_specials(oh);*/
}


void heap_finish_full_gc(struct object_heap* oh) {
  heap_finish_gc(oh);
}



void heap_start_gc(struct object_heap* oh) {
  oh->mark_color ^= MARK_MASK;
  oh->markStackPosition = 0;
}

void heap_pin_object(struct object_heap* oh, struct Object* x) {
  //  printf("Pinning %p\n", x);

  assert(object_hash(x) < ID_HASH_RESERVED);

  object_increment_pin_count(x);
}

void heap_unpin_object(struct object_heap* oh, struct Object* x) {
  //  printf("Unpinning %p\n", x);
  // don't check the idhash because forwardTo: will free the object
  //assert(object_hash(x) < ID_HASH_RESERVED);
  object_decrement_pin_count(x);
}

void heap_remember_old_object(struct object_heap* oh, struct Object* x) {
  if (object_is_old(oh, x) && !object_is_smallint(x)) {
#if 0
    printf("remembering "); print_object(x);
#endif
    oh->rememberedOldObjects.insert(x);
  }
  
}



/*adds something to the mark stack and mark it if it isn't marked already*/
void heap_mark(struct object_heap* oh, struct Object* obj) {
#ifdef GC_BUG_CHECK
  assert_good_object(oh, obj);
#endif

  if (object_markbit(obj) == oh->mark_color) return;
#ifdef PRINT_DEBUG_GC_MARKINGS
  printf("marking "); print_object(obj);
#endif
  object_set_mark(oh, obj);
  
  if (oh->markStackPosition + 1 >=oh->markStackSize) {
    oh->markStackSize *= 2;
#ifdef PRINT_DEBUG
    printf("Growing mark stack to %" PRIdPTR "\n", oh->markStackSize);
#endif
    oh->markStack = (struct Object**)realloc(oh->markStack, oh->markStackSize * sizeof(struct Object*));
    assert(oh->markStack);
  }
  oh->markStack[oh->markStackPosition++] = obj;
}


void heap_mark_specials(struct object_heap* oh, bool_t mark_old) {
  word_t i;
  for (i = 0; i < array_size(oh->special_objects_oop); i++) {
    struct Object* obj = oh->special_objects_oop->elements[i];
    if (!mark_old && object_is_old(oh, obj)) continue;
    if (object_is_smallint(obj)) continue;
    heap_mark(oh, obj);
  }

}

void heap_mark_interpreter_stack(struct object_heap* oh, bool_t mark_old) {
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

void heap_mark_recursively(struct object_heap* oh, bool_t mark_old) {

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
  word_t freed_words = 0, coalesce_count = 0, free_count = 0, object_count = 0;
  while (object_in_memory(oh, obj, memory, memorySize)) {
    object_count++;
    if (!object_is_free(obj) && !object_is_marked(oh, obj)) {
      free_count++;
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
  if (!oh->quiet) {
    printf("GC freed %" PRIdPTR " of %" PRIdPTR " %s objects\n", 
           free_count, object_count, ((memory == oh->memoryOld)? "old":"new"));
    printf("GC coalesced %" PRIdPTR " times\n", coalesce_count);
  }
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
  word_t tenure_count = 0, pin_count = 0, object_count = 0, free_count = 0;
  struct Object* obj = (struct Object*) oh->memoryYoung;
  struct Object* prev;
  /* fixme.. can't do this right now */
  /*  if (oh->gcTenureCount++ % 20 == 0) {*/
  oh->nextOldFree = (struct Object*)oh->memoryOld;
  /*  }*/
  
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {
    /*if it's still there in the young section, move it to the old section */
    if (object_is_free(obj)) {
      obj = object_after(oh, obj);
      continue;
    }

    object_count++;
    if (!object_is_marked(oh, obj)) {
      free_count++;
      heap_free_object(oh, obj);
    }
    if (object_is_pinned(oh, obj)) {
      pin_count++;
    } else {
      if (!object_is_free(obj)) {
        struct Object* tenure_start = gc_allocate_old(oh, object_total_size(obj));
        tenure_count++;
        copy_words_into((word_t*)obj, object_word_size(obj), (word_t*) tenure_start);
#ifdef PRINT_DEBUG_GC_MARKINGS
        printf("tenuring from "); print_object(obj);
        printf("tenuring to "); print_object(tenure_start);
#endif
        
        ((struct ForwardedObject*) obj)->target = tenure_start;
        object_set_idhash(obj, ID_HASH_FORWARDED);
      }
    }
    obj = object_after(oh, obj);
  }
#ifdef PRINT_DEBUG_GC_1
  if (!oh->quiet) {
    printf("Full GC freed %" PRIdPTR " young objects and tenured %" PRIdPTR " of %" PRIdPTR " objects (%" PRIdPTR " pinned)\n", 
           free_count, tenure_count, object_count, pin_count);
  }
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


void heap_mark_remembered(struct object_heap* oh) {
  word_t object_count = 0, remember_count = 0;
  std::vector<struct Object*> badRemembered;

  for (std::set<struct Object*>::iterator i = oh->rememberedOldObjects.begin();
       i != oh->rememberedOldObjects.end();
       i++) {
    struct Object* o = *i;
    word_t offset, limit, foundSomething;
    object_count++;
    foundSomething = 0;

    if (object_is_young(oh, (struct Object*)o->map)) {
      remember_count++;
      foundSomething = 1;
      heap_mark(oh, (struct Object*)o->map);
    }

    offset = object_first_slot_offset(o);
    limit = object_last_oop_offset(o) + sizeof(word_t);
    for (; offset != limit; offset += sizeof(word_t)) {
      struct Object* val = object_slot_value_at_offset(o, offset);
      if (!object_is_smallint(val) && object_is_young(oh, val)) {
        remember_count++;
        foundSomething = 1;
        heap_mark(oh, val);
      }
    }
    if (foundSomething == 0 || object_is_young(oh, o)) {
      //oh->rememberedOldObjects.erase(i);
      badRemembered.push_back(o);
    }

  }

#ifdef PRINT_DEBUG_GC_2
  if (!oh->quiet) {
    printf("Removing %" PRIdPTR " entries from the remembered table\n", badRemembered.size());
  }
#endif

  for (size_t i = 0; i < badRemembered.size(); i++) {
    oh->rememberedOldObjects.erase(badRemembered[i]);
  }


#ifdef PRINT_DEBUG_GC_2
  if (!oh->quiet) {
    printf("Young GC found %" PRIdPTR " old objects pointing to %" PRIdPTR " young objects\n", 
           object_count, remember_count);
  }
#endif

}


void heap_mark_pinned_young(struct object_heap* oh) {
  struct Object* obj = (struct Object*) oh->memoryYoung;
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {
    if (object_hash(obj) < ID_HASH_RESERVED && object_pin_count(obj) > 0) heap_mark(oh, obj);
    obj = object_after(oh, obj);
  }
}

void heap_mark_pinned_old(struct object_heap* oh) {
  struct Object* obj = (struct Object*) oh->memoryOld;
  while (object_in_memory(oh, obj, oh->memoryOld, oh->memoryOldSize)) {
    if (object_hash(obj) < ID_HASH_RESERVED && object_pin_count(obj) > 0) heap_mark(oh, obj);
    obj = object_after(oh, obj);
  }

}


void heap_pin_c_stack_diff(struct object_heap* oh) {
  word_t stackTop;
  word_t** stack = (word_t**)oh->stackBottom;
  while ((void*)stack > (void*)&stackTop) {
    struct Object* obj = (struct Object*)*stack;
    if ((object_is_young(oh, obj) || object_is_old(oh, obj))
        && !object_is_smallint(obj)
        && !object_is_marked(oh, obj)) {
      print_object(obj);
    }
    stack--;
   }
 
 }


void heap_full_gc(struct object_heap* oh) {
  heap_start_gc(oh);
  heap_unmark_all(oh, oh->memoryOld, oh->memoryOldSize);
  heap_unmark_all(oh, oh->memoryYoung, oh->memoryYoungSize);
  heap_mark_pinned_young(oh);
  heap_mark_pinned_old(oh);
  heap_mark_specials(oh, 1);
  heap_mark_interpreter_stack(oh, 1);
  heap_mark_recursively(oh, 1);

#ifdef GC_BUG_CHECK
  heap_pin_c_stack_diff(oh);
#endif

  //this frees the old objects
  heap_free_and_coalesce_unmarked(oh, oh->memoryOld, oh->memoryOldSize);
  //this frees the young objects and moves them over
  heap_tenure(oh);
#ifdef GC_BUG_CHECK
  heap_integrity_check(oh, oh->memoryOld, oh->memoryOldSize);
  heap_integrity_check(oh, oh->memoryYoung, oh->memoryYoungSize);
#endif
  heap_finish_full_gc(oh);
}

void heap_gc(struct object_heap* oh) {
#ifndef ALWAYS_FULL_GC
  heap_start_gc(oh);
  heap_unmark_all(oh, oh->memoryYoung, oh->memoryYoungSize);
  heap_mark_pinned_young(oh);
  heap_mark_specials(oh, 0);
  heap_mark_interpreter_stack(oh, 0);
  heap_mark_remembered(oh);
  heap_mark_recursively(oh, 0);
#ifdef GC_BUG_CHECK
  heap_pin_c_stack_diff(oh);
#endif
  // this frees the young objects
  heap_free_and_coalesce_unmarked(oh, oh->memoryYoung, oh->memoryYoungSize);

  oh->nextFree = (struct Object*)oh->memoryYoung;
#ifdef GC_BUG_CHECK
  heap_integrity_check(oh, oh->memoryYoung, oh->memoryYoungSize);
#endif

  heap_finish_gc(oh);
#else
  heap_full_gc(oh);
#endif
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
#ifdef GC_BUG_CHECK
  assert(heap_what_points_to(oh, x, 1) == 0);
#endif
  heap_free_object(oh, x);
}

SLATE_INLINE void heap_store_into(struct object_heap* oh, struct Object* src, struct Object* dest) {
  /*  print_object(dest);*/
  if (!object_is_smallint(dest)) {
    assert(object_hash(dest) < ID_HASH_RESERVED); /*catch gc bugs earlier*/
    assert(object_hash(src) < ID_HASH_RESERVED); /*catch gc bugs earlier*/
  }

  if (object_is_young(oh, dest) && object_is_old(oh, src)) {
    heap_remember_old_object(oh, src);
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
  object_zero_pin_count(o);
  assert(!object_is_free(o));
  return o;
}

struct Object* heap_allocate(struct object_heap* oh, word_t words) {
  return heap_allocate_with_payload(oh, words, 0);
}

struct Object* heap_clone(struct object_heap* oh, struct Object* proto) {
  struct Object* newObj;
  //usually the caller should pin the objects but here we make an exception
  //if this got GC'd, then the copy_words_into would get garbage
  Pinned<struct Object> pinnedProto(oh);
  pinnedProto = proto;
  
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
  fill_bytes_with((byte_t*)((word_t*)newObj  + object_size(proto)), (bytes+ sizeof(word_t) - 1) / sizeof(word_t) * sizeof(word_t), 0);

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


