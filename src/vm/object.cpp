
/* slate object manipulation */

#include "slate.hpp"


SLATE_INLINE word_t object_to_smallint(struct Object* xxx)  {return ((((word_t)xxx)>>1)); }
SLATE_INLINE struct Object* smallint_to_object(word_t xxx) {return ((struct Object*)(((xxx)<<1)|1)); }


SLATE_INLINE bool_t oop_is_object(word_t xxx)   { return ((((word_t)xxx)&SMALLINT_MASK) == 0); }
SLATE_INLINE bool_t oop_is_smallint(word_t xxx) { return ((((word_t)xxx)&SMALLINT_MASK) == 1);}
SLATE_INLINE bool_t object_is_smallint(struct Object* xxx) { return ((((word_t)xxx)&SMALLINT_MASK) == 1);}
SLATE_INLINE word_t object_markbit(struct Object* xxx)      { return  (((xxx)->header)&MARK_MASK); }
SLATE_INLINE word_t object_hash(struct Object* xxx)       { return  ((((xxx)->header)>>1)&ID_HASH_MAX);}
SLATE_INLINE word_t object_size(struct Object* xxx)       {return   xxx->objectSize;}
SLATE_INLINE word_t payload_size(struct Object* xxx) {return xxx->payloadSize;}
SLATE_INLINE word_t object_type(struct Object* xxx)     {return     ((((xxx)->header)>>30)&0x3);}


void object_set_mark(struct object_heap* oh, struct Object* xxx) {
  xxx->header &= ~MARK_MASK;
  xxx->header|=oh->mark_color & MARK_MASK;
}
void object_unmark(struct object_heap* oh, struct Object* xxx)  {
  xxx->header &= ~MARK_MASK;
  xxx->header|= ((~oh->mark_color)&MARK_MASK);
}

void object_set_format(struct Object* xxx, word_t type) {
  xxx->header &= ~(3<<30);
  xxx->header |= (type&3) << 30;
}
void object_set_size(struct Object* xxx, word_t size) {
  assert((size_t)size >= HEADER_SIZE_WORDS+1);
  xxx->objectSize = size;
}
void object_set_idhash(struct Object* xxx, word_t hash) {
  xxx->header &= ~(ID_HASH_MAX<<1);
  xxx->header |= (hash&ID_HASH_MAX) << 1;
}
void payload_set_size(struct Object* xxx, word_t size) {
  xxx->payloadSize = size;
}


/*fix see if it can be post incremented*/
word_t heap_new_hash(struct object_heap* oh) {
  word_t hash;
  do {
    hash = (oh->lastHash = 13849 + (27181 * oh->lastHash)) & ID_HASH_MAX;
  } while (hash >= ID_HASH_RESERVED);

  return hash;
}



/* DETAILS: This trick is from Tim Rowledge. Use a shift and XOR to set the 
 * sign bit if and only if the top two bits of the given value are the same, 
 * then test the sign bit. Note that the top two bits are equal for exactly 
 * those integers in the range that can be represented in 31-bits.
*/
word_t smallint_fits_object(word_t i) {   return (i ^ (i << 1)) >= 0;}
/*fix i didn't understand the above*/

SLATE_INLINE struct Object* get_special(struct object_heap* oh, word_t special_index) {
  return oh->special_objects_oop->elements[special_index];
}

SLATE_INLINE struct Map* object_get_map(struct object_heap* oh, struct Object* o) {
  if (object_is_smallint(o)) return get_special(oh, SPECIAL_OOP_SMALL_INT_PROTO)->map;
  return o->map;
}



word_t object_is_immutable(struct Object* o) {return ((word_t)o->map->flags & MAP_FLAG_IMMUTABLE) != 0; }

bool_t object_is_special(struct object_heap* oh, struct Object* obj) {
  word_t i;
  for (i = 0; i < SPECIAL_OOP_COUNT; i++) {
    if (obj == oh->special_objects_oop->elements[i]) return 1;
  }
  return 0;
}



word_t object_word_size(struct Object* o) {

  if (object_type(o) == TYPE_OBJECT) {
    return object_size(o);
  } 
  return object_size(o) + (payload_size(o) + sizeof(word_t) - 1) / sizeof(word_t); 

}

word_t object_array_offset(struct Object* o) {
  return object_size((struct Object*)o) * sizeof(word_t);
}

byte_t* byte_array_elements(struct ByteArray* o) {
  byte_t* elements = (byte_t*)inc_ptr(o, object_array_offset((struct Object*)o));
  return elements;
}

byte_t byte_array_get_element(struct Object* o, word_t i) {
  return byte_array_elements((struct ByteArray*)o)[i];
}

byte_t byte_array_set_element(struct ByteArray* o, word_t i, byte_t val) {
  return byte_array_elements(o)[i] = val;
}


struct Object* object_array_get_element(struct Object* o, word_t i) {
  struct Object** elements = (struct Object**)inc_ptr(o, object_array_offset(o));
  return elements[i];
}

struct Object* object_array_set_element(struct object_heap* oh, struct Object* o, word_t i, struct Object* val) {
  struct Object** elements = (struct Object**)inc_ptr(o, object_array_offset(o));
  heap_store_into(oh, o, val);
  return elements[i] = val;
}

struct Object** object_array_elements(struct Object* o) {
  return (struct Object**)inc_ptr(o, object_array_offset(o));
}

struct Object** array_elements(struct OopArray* o) {
  return object_array_elements((struct Object*) o);
}

float_type* float_part(struct ByteArray* o) {
  return (float_type*)object_array_elements((struct Object*) o);
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

word_t slot_table_capacity(struct SlotTable* roles) {
  return object_array_size((struct Object*)roles) / ((sizeof(struct SlotEntry) + (sizeof(word_t) - 1)) / sizeof(word_t));
}

word_t role_table_capacity(struct RoleTable* roles) {
  return object_array_size((struct Object*)roles) / ((sizeof(struct RoleEntry) + (sizeof(word_t) - 1)) / sizeof(word_t));
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

word_t object_first_slot_offset(struct Object* o) {

  return sizeof(struct Object);

}

word_t object_last_slot_offset(struct Object* o) {
  return object_size(o) * sizeof(word_t) - sizeof(word_t);
}


word_t object_last_oop_offset(struct Object* o) {

  if (object_type(o) == TYPE_OOP_ARRAY) {
    return object_last_slot_offset(o) + payload_size(o);
  }
  return object_last_slot_offset(o);
}

struct Object* object_slot_value_at_offset(struct Object* o, word_t offset) {

  return (struct Object*)*((word_t*)inc_ptr(o, offset));

}


struct Object* object_slot_value_at_offset_put(struct object_heap* oh, struct Object* o, word_t offset, struct Object* value) {

  heap_store_into(oh, o, value);
  return (struct Object*)((*((word_t*)inc_ptr(o, offset))) = (word_t)value);

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
  heap_store_into(oh, (struct Object*)roles, (struct Object*)name);
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
  Pinned<struct RoleTable> newRoles(oh);
  Pinned<struct Symbol> roleName(oh);
  oldSize = role_table_capacity(roles);
  newSize = role_table_accommodate(roles, (oldSize / 3 - role_table_empty_space(oh, roles)) + n);
  newRoles = (struct RoleTable*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), newSize * ROLE_ENTRY_WORD_SIZE);

  for (i = 0; i < oldSize; i++) {
    roleName = (struct Symbol*)roles->roles[i].name;
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
  Pinned<struct SlotTable> newSlots(oh);

  oldSize = slot_table_capacity(slots);
  newSize = slot_table_accommodate(slots, (oldSize / 3 - slot_table_empty_space(oh, slots)) + n);
  newSlots = (struct SlotTable*) heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), newSize * SLOT_ENTRY_WORD_SIZE);

  for (i = 0; i < oldSize; i++) {
    struct Symbol* slotName = (struct Symbol*)slots->slots[i].name;
    if (slotName != (struct Symbol*)oh->cached.nil && slotName != excluding) {
      struct SlotEntry * slot = slot_table_entry_for_inserting_name(oh, newSlots, slotName);
      *slot = slots->slots[i];
    }
    
  }

  return newSlots;

}

void slot_table_relocate_by(struct object_heap* oh, struct SlotTable* slots, word_t offset, word_t amount) {

  word_t i;

  for (i = 0; i < slot_table_capacity(slots); i++) {
    if (slots->slots[i].name != (struct Symbol*)oh->cached.nil
        && object_to_smallint(slots->slots[i].offset) >= offset) {
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
  heap_store_into(oh, obj, (struct Object*)map);
  obj->map = map;
}

void object_represent(struct object_heap* oh, struct Object* obj, struct Map* map) {
  object_change_map(oh, obj, map);
  heap_store_into(oh, (struct Object*)map, obj);
  map->representative = obj;
}

word_t object_add_role_at(struct object_heap* oh, struct Object* obj, struct Symbol* selector, word_t position, struct MethodDefinition* method) {

  Pinned<struct Map> map(oh);
  struct RoleEntry *entry, *chain;

  map = heap_clone_map(oh, obj->map);
  chain = role_table_entry_for_name(oh, obj->map->roleTable, selector);
  object_represent(oh, obj, map);
  
  while (chain != NULL) {

    if (chain->methodDefinition == method) {

      /*fix: do we want to copy the roletable*/
      map->roleTable = role_table_grow_excluding(oh, map->roleTable, 0, NULL);
      heap_store_into(oh, (struct Object*)map, (struct Object*)map->roleTable);

      /* roleTable is in young memory now so we don't have to store_into*/
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
  heap_store_into(oh, (struct Object*)map, (struct Object*)map->roleTable);

  entry = role_table_insert(oh, map->roleTable, selector);
  entry->name = selector;
  entry->nextRole = oh->cached.nil;
  entry->rolePositions = smallint_to_object(position);
  entry->methodDefinition = method;
  return TRUE;

}

word_t object_remove_role(struct object_heap* oh, struct Object* obj, struct Symbol* selector, struct MethodDefinition* method) {

  Pinned<struct Map> map(oh);
  Pinned<struct RoleTable> roles(oh);
  roles = obj->map->roleTable;
  word_t i, matches = 0;

  for (i = 0; i< role_table_capacity(roles); i++) {

    if (roles->roles[i].methodDefinition == method) {
      matches++;
    }

  }

  if (matches == 0) return FALSE;
  map = heap_clone_map(oh, obj->map);
  map->roleTable = role_table_grow_excluding(oh, roles, matches, method);
  heap_store_into(oh, (struct Object*)map, (struct Object*)map->roleTable);
  object_represent(oh, obj, map);
  return TRUE;

}





struct Object* object_add_slot_named_at(struct object_heap* oh, struct Object* obj, struct Symbol* name, struct Object* value, word_t offset) {

  Pinned<struct Map> map(oh);
  Pinned<struct Object> newObj(oh);
  Pinned<struct SlotEntry> entry(oh);

  entry = slot_table_entry_for_name(oh, obj->map->slotTable, name);
  if ((struct Object*)entry != NULL) return NULL;
  map = heap_clone_map(oh, obj->map);
  map->slotTable = slot_table_grow_excluding(oh, map->slotTable, 1, (struct Symbol*)oh->cached.nil);
  slot_table_relocate_by(oh, map->slotTable, offset, sizeof(word_t));
  entry = slot_table_entry_for_inserting_name(oh, map->slotTable, name);
  entry->name = name;
  entry->offset = smallint_to_object(offset);

  if (object_type(obj) == TYPE_OBJECT) {
    newObj = heap_allocate(oh, object_size(obj) + 1);
  } else {
    newObj = heap_allocate_with_payload(oh, object_size(obj) + 1, payload_size(obj));
  }
  object_set_format(newObj, object_type(obj));
  


  object_set_idhash(newObj, heap_new_hash(oh));
  copy_bytes_into((byte_t*)obj+ object_first_slot_offset(obj),
                  offset-object_first_slot_offset(obj),
                  (byte_t*)newObj + object_first_slot_offset(newObj));
  object_slot_value_at_offset_put(oh, newObj, offset, value);

  copy_bytes_into((byte_t*)obj+offset, object_total_size(obj) - offset, (byte_t*)newObj + offset + sizeof(word_t));
  newObj->map = map;
  map->representative = newObj;
  heap_store_into(oh, (struct Object*)map, (struct Object*)map->representative);

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

struct Object* object_remove_slot(struct object_heap* oh, struct Object* obj, struct Symbol* name) {
  word_t offset;
  Pinned<struct Object> newObj(oh);
  Pinned<struct Map> map(oh);
  Pinned<struct SlotEntry> se(oh);
  se = slot_table_entry_for_name(oh, obj->map->slotTable, name);
  if ((struct Object*)se == NULL) return obj;

  offset = object_to_smallint(se->offset);
  map = heap_clone_map(oh, obj->map);
  map->slotCount = smallint_to_object(object_to_smallint(map->slotCount) - 1);
  map->slotTable = slot_table_grow_excluding(oh, map->slotTable, -1, se->name);
  slot_table_relocate_by(oh, map->slotTable, offset, -sizeof(word_t));

  if (object_type(obj) == TYPE_OBJECT) {
    newObj = heap_allocate(oh, object_size(obj)-1);
    object_set_format(newObj, TYPE_OBJECT);
  } else {
    newObj = heap_allocate_with_payload(oh,  object_size(obj) -1 , payload_size(obj));
    object_set_format(newObj, object_type(obj));
  }

  /*fix we don't need to set the format again, right?*/
  object_set_idhash(newObj, heap_new_hash(oh));
  copy_bytes_into((byte_t*) obj + object_first_slot_offset(obj),
                  offset - object_first_slot_offset(obj), 
                  (byte_t*) newObj + object_first_slot_offset(newObj));

  copy_bytes_into((byte_t*) obj + object_first_slot_offset(obj) + sizeof(word_t), /*+one slot*/
                  object_total_size(obj) - offset - sizeof(word_t), 
                  (byte_t*) newObj + offset);
  newObj->map = map;
  map->representative = newObj;
  heap_store_into(oh, (struct Object*)map, map->representative);
  return newObj;
}



struct ForwardPointerEntry* forward_pointer_hash_get(struct ForwardPointerEntry* table,
                                                    word_t forwardPointerEntryCount,
                                                    struct Object* fromObj) {
  word_t index;
  word_t hash = (uword_t)fromObj % forwardPointerEntryCount;
  struct ForwardPointerEntry* entry;

  for (index = hash; index < forwardPointerEntryCount; index++) {
    entry = &table[index];
    if (entry->fromObj == fromObj || entry->fromObj == NULL) return entry;
  }
  for (index = 0; index < hash; index++) {
    entry = &table[index];
    if (entry->fromObj == fromObj || entry->fromObj == NULL) return entry;
  }

  return NULL;


}

struct ForwardPointerEntry* forward_pointer_hash_add(struct ForwardPointerEntry* table,
                                                    word_t forwardPointerEntryCount,
                                                    struct Object* fromObj, struct Object* toObj) {

  struct ForwardPointerEntry* entry = forward_pointer_hash_get(table, forwardPointerEntryCount, fromObj);
  if (NULL == entry) return NULL; /*full table*/
  assert(entry->fromObj == NULL || entry->fromObj == fromObj); /*assure we don't have inconsistancies*/
  entry->fromObj = fromObj;
  entry->toObj = toObj;
  return entry;

}


void copy_used_objects(struct object_heap* oh, struct Object** writeObject,  byte_t* memoryStart, word_t memorySize,
struct ForwardPointerEntry* table, word_t forwardPointerEntryCount) {
  struct Object* readObject = (struct Object*) memoryStart;
  while (object_in_memory(oh, readObject, memoryStart, memorySize)) {
    if (!object_is_free(readObject)) {
      assert(NULL != forward_pointer_hash_add(table, forwardPointerEntryCount, readObject, *writeObject));
      copy_words_into((word_t*)readObject, object_word_size(readObject), (word_t*)*writeObject);
      *writeObject = object_after(oh, *writeObject);
    }
    readObject = object_after(oh, readObject);
  }

}

void adjust_object_fields_with_table(struct object_heap* oh, byte_t* memory, word_t memorySize,
                                     struct ForwardPointerEntry* table, word_t forwardPointerEntryCount) {

  struct Object* o = (struct Object*) memory;
 
  while (object_in_memory(oh, o, memory, memorySize)) {
    word_t offset, limit;
    o->map = (struct Map*)forward_pointer_hash_get(table, forwardPointerEntryCount, (struct Object*)o->map)->toObj;
    offset = object_first_slot_offset(o);
    limit = object_last_oop_offset(o) + sizeof(word_t);
    for (; offset != limit; offset += sizeof(word_t)) {
      struct Object* val = object_slot_value_at_offset(o, offset);
      if (!object_is_smallint(val)) {
        object_slot_value_at_offset_put(oh, o, offset, 
                                        forward_pointer_hash_get(table, forwardPointerEntryCount, val)->toObj);
      }
    }
    o = object_after(oh, o);
  }

}


void adjust_fields_by(struct object_heap* oh, struct Object* o, word_t shift_amount) {

  word_t offset, limit;
  o->map = (struct Map*) inc_ptr(o->map, shift_amount);
  offset = object_first_slot_offset(o);
  limit = object_last_oop_offset(o) + sizeof(word_t);
  for (; offset != limit; offset += sizeof(word_t)) {
    struct Object* val = object_slot_value_at_offset(o, offset);
    if (!object_is_smallint(val)) {
      /*avoid calling the function because it will call heap_store_into*/
      (*((word_t*)inc_ptr(o, offset))) = (word_t)inc_ptr(val, shift_amount);
  /*      object_slot_value_at_offset_put(oh, o, offset, (struct Object*)inc_ptr(val, shift_amount));*/
    }
  }

}


void adjust_oop_pointers_from(struct object_heap* oh, word_t shift_amount, byte_t* memory, word_t memorySize) {

  struct Object* o = (struct Object*)memory;
#ifdef PRINT_DEBUG
  printf("First object: "); print_object(o);
#endif
  while (object_in_memory(oh, o, memory, memorySize)) {
    /*print_object(o);*/
    if (!object_is_free(o)) {
      adjust_fields_by(oh, o, shift_amount);
    }
    o = object_after(oh, o);
  }


}
