
/*********************************
 * INLINE STUFF                  *
 *********************************/


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

SLATE_INLINE bool_t object_is_smallint(struct Object* xxx) { return ((((word_t)xxx)&SMALLINT_MASK) == 1);}
SLATE_INLINE word_t object_to_smallint(struct Object* xxx)  {return ((((word_t)xxx)>>1)); }
SLATE_INLINE struct Object* smallint_to_object(word_t xxx) {return ((struct Object*)(((xxx)<<1)|1)); }


SLATE_INLINE bool_t oop_is_object(word_t xxx)   { return ((((word_t)xxx)&SMALLINT_MASK) == 0); }
SLATE_INLINE bool_t oop_is_smallint(word_t xxx) { return ((((word_t)xxx)&SMALLINT_MASK) == 1);}
SLATE_INLINE word_t object_markbit(struct Object* xxx)      { return  (((xxx)->header)&MARK_MASK); }
SLATE_INLINE word_t object_hash(struct Object* xxx)       { return  ((((xxx)->header)>>1)&ID_HASH_MAX);}
SLATE_INLINE word_t object_size(struct Object* xxx)       {return   xxx->objectSize;}
SLATE_INLINE word_t payload_size(struct Object* xxx) {return xxx->payloadSize;}
SLATE_INLINE word_t object_type(struct Object* xxx)     {return     ((((xxx)->header)>>30)&0x3);}
SLATE_INLINE word_t object_pin_count(struct Object* xxx)     {return     ((((xxx)->header)>>PINNED_OFFSET)&PINNED_MASK);}

SLATE_INLINE struct Object* get_special(struct object_heap* oh, word_t special_index) {
  return oh->special_objects_oop->elements[special_index];
}

SLATE_INLINE struct Map* object_get_map(struct object_heap* oh, struct Object* o) {
  if (object_is_smallint(o)) return get_special(oh, SPECIAL_OOP_SMALL_INT_PROTO)->map;
  return o->map;
}

SLATE_INLINE word_t object_array_size(struct Object* o) {

  assert(object_type(o) != TYPE_OBJECT);
  return (payload_size(o) + sizeof(word_t) - 1) / sizeof(word_t);

}

SLATE_INLINE word_t byte_array_size(struct ByteArray* o) {
  return payload_size((struct Object*)o);
}


SLATE_INLINE word_t array_size(struct OopArray* x) {
  return object_array_size((struct Object*) x);
}


SLATE_INLINE word_t object_byte_size(struct Object* o) {
  if (object_type(o) == TYPE_OBJECT) {
    return object_array_offset(o);
  } 
  return object_array_offset(o) + payload_size(o);

}

SLATE_INLINE word_t object_total_size(struct Object* o) {
  /*IMPORTANT: rounds up to word size*/

  return object_word_size(o)*sizeof(word_t);

}


SLATE_INLINE void heap_pin_object(struct object_heap* oh, struct Object* x) {
  //  printf("Pinning %p\n", x);

  if (object_is_smallint(x)) return;
  assert(object_hash(x) < ID_HASH_RESERVED);

  object_increment_pin_count(x);
}

SLATE_INLINE void heap_unpin_object(struct object_heap* oh, struct Object* x) {
  //  printf("Unpinning %p\n", x);
  // don't check the idhash because forwardTo: will free the object
  //assert(object_hash(x) < ID_HASH_RESERVED);
  if (object_is_smallint(x)) return;
  object_decrement_pin_count(x);
}


SLATE_INLINE void object_increment_pin_count(struct Object* xxx)     {
  word_t count = ((((xxx)->header)>>PINNED_OFFSET)&PINNED_MASK);
  assert (count != PINNED_MASK);
  count++;
#if GC_BUG_CHECK
  if (count > 10) {
    printf("inc %d ", (int)count); print_object(xxx);
  }
#endif
  if (count == PINNED_MASK) {
    assert(0);
  }
  xxx->header &= ~(PINNED_MASK << PINNED_OFFSET);
  xxx->header |= count << PINNED_OFFSET;
}


SLATE_INLINE void object_decrement_pin_count(struct Object* xxx)     {
  word_t count = ((((xxx)->header)>>PINNED_OFFSET)&PINNED_MASK);
  //this could happen for the forwardTo: call since we manually unpin it
  //assert (count > 0);
#if GC_BUG_CHECK
  if (count > 10) {
    printf("dec %d ", (int)count); print_object(xxx);
  }
#endif
  if (count == 0) return;
  count--;
  xxx->header &= ~(PINNED_MASK << PINNED_OFFSET);
  xxx->header |= count << PINNED_OFFSET;
}


SLATE_INLINE bool_t object_is_old(struct object_heap* oh, struct Object* oop) {
  return (oh->memoryOld <= (byte_t*)oop && (byte_t*)oh->memoryOld + oh->memoryOldSize > (byte_t*)oop);

}

SLATE_INLINE bool_t object_is_young(struct object_heap* oh, struct Object* obj) {
  return (oh->memoryYoung <= (byte_t*)obj && (byte_t*)oh->memoryYoung + oh->memoryYoungSize > (byte_t*)obj);
  
}

SLATE_INLINE bool_t object_in_memory(struct object_heap* oh, struct Object* oop, byte_t* memory, word_t memorySize) {
  return (memory <= (byte_t*)oop && (byte_t*)memory + memorySize > (byte_t*)oop);

}

SLATE_INLINE struct Object* object_after(struct object_heap* heap, struct Object* o) {

  assert(object_total_size(o) != 0);

  return (struct Object*)inc_ptr(o, object_total_size(o));
}

SLATE_INLINE bool_t object_is_free(struct Object* o) {

  return (object_hash(o) >= ID_HASH_RESERVED);
}

SLATE_INLINE byte_t* inc_ptr(struct Object* obj, word_t amt) {
  return ((byte_t*)obj + amt);
}

SLATE_INLINE word_t object_word_size(struct Object* o) {

  if (object_type(o) == TYPE_OBJECT) {
    return object_size(o);
  } 
  return object_size(o) + (payload_size(o) + sizeof(word_t) - 1) / sizeof(word_t); 

}

SLATE_INLINE struct Object* object_slot_value_at_offset(struct Object* o, word_t offset) {

  return (struct Object*)*((word_t*)inc_ptr(o, offset));

}

SLATE_INLINE word_t hash_selector(struct object_heap* oh, struct Symbol* name, struct Object* arguments[], word_t n) {
  word_t hash = (word_t) name;
  word_t i;
  for (i = 0; i < n; i++) {
    hash ^= object_hash((struct Object*)object_get_map(oh, arguments[i]));
  }
  return hash;
}


#ifdef SLATE_USE_RDTSC
SLATE_INLINE volatile int64_t getRealTimeClock() {
   register long long TSC asm("eax");
   asm volatile (".byte 15, 49" : : : "eax", "edx");
   return TSC;
} 
#else
SLATE_INLINE volatile int64_t getRealTimeClock() {
  return getTickCount();
} 
#endif
