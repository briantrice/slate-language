#include "slate.h"
#ifdef SLATE_USE_MMAP
#include <sys/mman.h>
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
  oh->pinnedYoungObjects = calloc(1, (oh->memoryYoungSize / PINNED_CARD_SIZE + 1) * sizeof(word_t));
  oh->rememberedYoungObjects = calloc(1, (oh->memoryYoungSize / PINNED_CARD_SIZE + 1) * sizeof(word_t));

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
  oh->markStackSize = 4 * MB;
  oh->markStackPosition = 0;
  oh->markStack = malloc(oh->markStackSize * sizeof(struct Object*));
  
  oh->optimizedMethodsSize = 0;
  oh->optimizedMethodsLimit = 1024;
  oh->optimizedMethods = malloc(oh->optimizedMethodsLimit * sizeof(struct CompiledMethod*));

  oh->socketTicketCount = SLATE_NETTICKET_MAXIMUM;
  oh->socketTickets = calloc(oh->socketTicketCount * sizeof(struct slate_addrinfo_request), 1);
#if 0
  oh->socketTicketMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

  assert(oh->markStack != NULL);
  initMemoryModule(oh);
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
  if (object_is_young(oh, x)) {
    word_t diff = (byte_t*) x - oh->memoryYoung;
    return (oh->pinnedYoungObjects[diff / PINNED_CARD_SIZE] & (1 << (diff % PINNED_CARD_SIZE))) != 0;
  }
  return 0;
  
}

bool_t object_is_remembered(struct object_heap* oh, struct Object* x) {
  if (object_is_young(oh, x)) {
    word_t diff = (byte_t*) x - oh->memoryYoung;
    return (oh->rememberedYoungObjects[diff / PINNED_CARD_SIZE] & (1 << (diff % PINNED_CARD_SIZE))) != 0;
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
  int i, k;
#ifdef PRINT_DEBUG_GC_MARKINGS
  printf("freeing "); print_object(obj);
#endif
  /*fixme. there is a possibility that this is a method in oh->optimizedMethods.
   * we might want to optimize this
   */
  for (i = 0; i < oh->optimizedMethodsSize; i++) {
    if (obj == (struct Object*)oh->optimizedMethods[i]) {
      /*shift things down*/
      for (k = i; k < oh->optimizedMethodsSize - 1; k++) {
        oh->optimizedMethods[k] = oh->optimizedMethods[k+1];
      }
      oh->optimizedMethodsSize --;
      i--;
    }
  }

  heap_make_free_space(oh, obj, object_word_size(obj));

}


void heap_finish_gc(struct object_heap* oh) {
  method_flush_cache(oh, NULL);
  /*unpin the C stack*/
  fill_bytes_with((byte_t*)oh->pinnedYoungObjects, oh->memoryYoungSize / PINNED_CARD_SIZE * sizeof(word_t), 0);
  /*  cache_specials(oh);*/
}


void heap_finish_full_gc(struct object_heap* oh) {
  heap_finish_gc(oh);
  /*we can forget the remembered set after doing a full GC*/
  fill_bytes_with((byte_t*)oh->rememberedYoungObjects, oh->memoryYoungSize / PINNED_CARD_SIZE * sizeof(word_t), 0);
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

void heap_remember_young_object(struct object_heap* oh, struct Object* x) {
  if (object_is_young(oh, x) && !object_is_smallint(x)) {
    word_t diff = (byte_t*) x - oh->memoryYoung;
#if 0
    printf("remembering "); print_object(x);
#endif
    oh->rememberedYoungObjects[diff / PINNED_CARD_SIZE] |= 1 << (diff % PINNED_CARD_SIZE);
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
    printf("Growing mark stack to %" PRIdPTR "\n", oh->markStackSize);
#endif
    oh->markStack = realloc(oh->markStack, oh->markStackSize * sizeof(struct Object*));
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

void heap_mark_fixed(struct object_heap* oh, bool_t mark_old) {
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
  if (!oh->quiet) {
    printf("GC Freed %" PRIdPTR " words and coalesced %" PRIdPTR " times\n", freed_words, coalesce_count);
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
  if (!oh->quiet) {
    printf("GC tenured %" PRIdPTR " objects (%" PRIdPTR " words)\n", tenure_count, tenure_words);
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

void heap_mark_pinned_young(struct object_heap* oh) {
  struct Object* obj = (struct Object*) oh->memoryYoung;
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {
    if (object_hash(obj) < ID_HASH_RESERVED && object_is_pinned(oh, obj)) heap_mark(oh, obj);
    obj = object_after(oh, obj);
  }
}

void heap_mark_remembered_young(struct object_heap* oh) {
  struct Object* obj = (struct Object*) oh->memoryYoung;
  while (object_in_memory(oh, obj, oh->memoryYoung, oh->memoryYoungSize)) {
    if (object_hash(obj) < ID_HASH_RESERVED && object_is_remembered(oh, obj)) heap_mark(oh, obj);
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
#ifdef GC_BUG_CHECK
      assert(0 == heap_what_points_to_in(oh, obj, oh->memoryOld, oh->memoryOldSize, 0));
#endif
      young_count++;
      young_word_count += object_word_size(obj);
      if (object_is_free(prev) && obj != prev) {
        /*run hooks for free object before coalesce */
        heap_free_object(oh, obj);
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
  if (!oh->quiet) {
    printf("GC freed %" PRIdPTR " young objects (%" PRIdPTR " words)\n", young_count, young_word_count);
  }
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
  heap_mark_pinned_young(oh);
  heap_mark_recursively(oh, 1);
  /*  heap_print_marks(oh, oh->memoryYoung, oh->memoryYoungSize);*/
  heap_free_and_coalesce_unmarked(oh, oh->memoryOld, oh->memoryOldSize);
  heap_tenure(oh);
#ifdef GC_BUG_CHECK
  printf("GC integrity check...\n");
  heap_integrity_check(oh, oh->memoryOld, oh->memoryOldSize);
  heap_integrity_check(oh, oh->memoryYoung, oh->memoryYoungSize);
#endif
  heap_finish_full_gc(oh);
}

void heap_gc(struct object_heap* oh) {
#ifndef GC_BUG_CHECK
  heap_start_gc(oh);
  heap_unmark_all(oh, oh->memoryYoung, oh->memoryYoungSize);
  heap_pin_c_stack(oh);
  heap_mark_specials(oh, 0);
  heap_mark_interpreter_stack(oh, 0);
  heap_mark_fixed(oh, 0);
  heap_mark_pinned_young(oh);
  heap_mark_remembered_young(oh);
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
    heap_remember_young_object(oh, dest);
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


