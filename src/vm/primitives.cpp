#include "slate.hpp"


//Template for defining Slate primitive signatures. Not a macro because IDEs don't process it:
//#define SLATE_PRIM(prim_name) void prim_name(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer)

#ifdef SLATE_DAEMONIZE
#include <pwd.h>
#include <sys/stat.h>
#include <signal.h>
#endif

void prim_fixme(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  printf("UNIMPLEMENTED PRIMITIVE\n");
  interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_TYPE_ERROR_ON), x, NULL, resultStackPointer);
}

#pragma mark Root

void prim_isIdenticalTo(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	oh->cached.interpreter->stack->elements[resultStackPointer] = (args[0]==args[1])? oh->cached.true_object : oh->cached.false_object;
}

void prim_identity_hash(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	/*fix*/
	/*  print_detail(oh, args[0]);
	 print_backtrace(oh);*/
	oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_hash(args[0]));
}

void prim_identity_hash_univ(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	/*fix*/
	/*  print_detail(oh, args[0]);
	 print_backtrace(oh);*/
	if (object_is_smallint(args[0])) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = args[0];
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_hash(args[0]));
	}
}

/* Root forwardTo: anotherObject */
void prim_forward_to(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* x = args[0];
	struct Object* y = args[1];
	oh->cached.interpreter->stack->elements[resultStackPointer] = y;
	/* since some objects like roleTables store pointers to things like Nil in byte arrays rather than oop arrays,
	 * we must make sure that these special objects do not move.
	 */
	if (x == get_special(oh, SPECIAL_OOP_NIL) 
		|| x == get_special(oh, SPECIAL_OOP_TRUE)
		|| x == get_special(oh, SPECIAL_OOP_FALSE)) {
		printf("Error... you cannot call forwardTo on this special object (did you add a slot to Nil/True/False?)\n");
		interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_TYPE_ERROR_ON), x, NULL, resultStackPointer); \
		return;
	}
	
	if (!object_is_smallint(x) && !object_is_smallint(y) && x != y) {
          heap_unpin_object(oh, x);
          heap_forward(oh, x, y);
          /*heap_gc(oh);*/ /* unnecessary waste of time for one object? */
          //cache_specials(oh);
	}
	
}

/* Root atSlotNamed: symbol */
void prim_at_slot_named(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* obj;
	struct Object* name;
	struct SlotEntry * se;
	
	obj = args[0];
	name = args[1];
	
	if (object_is_smallint(obj)) {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
	} else {
		se = slot_table_entry_for_name(oh, obj->map->slotTable, (struct Symbol*)name);
		if (se == NULL) {
			interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
		} else {
			word_t offset = object_to_smallint(se->offset);
			oh->cached.interpreter->stack->elements[resultStackPointer] = object_slot_value_at_offset(obj, offset);
		}
	}
}

/* Root atSlotNamed: symbol put: value */
void prim_at_slot_named_put(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* obj=args[0], *val=args[2];
	struct Object* name = args[1];
	struct SlotEntry * se;
	struct Map* map;
	
	if (object_is_smallint(obj)) {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
		return;
	}
	
	if (object_is_immutable(obj)) {
		interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), obj, NULL, resultStackPointer);
		return;
	}
	
	map = obj->map;
	se = slot_table_entry_for_name(oh, map->slotTable, (struct Symbol*)name);
	
	if (se == NULL) {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
	} else {
		word_t offset = object_to_smallint(se->offset);
		oh->cached.interpreter->stack->elements[resultStackPointer] = object_slot_value_at_offset_put(oh, obj, offset, val);
	}
	
	/*note: not supporting delegate slots*/
	
}

void prim_clone(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	if (object_is_smallint(args[0])) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = args[0];
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = heap_clone(oh, args[0]);
	}
}

/* Cloneable cloneSettingSlots: slotNamesArray to: valuesArray */
void prim_clone_setting_slots(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  Pinned<struct Object> obj(oh, args[0]), slotArray(oh, args[1]), valueArray(oh, args[2]), newObj(oh);
  word_t i;
	
	if (object_is_smallint(obj)) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = obj;
		return;
	}
	newObj = heap_clone(oh, obj);
	
	/*fix, check that arrays are same size, and signal errors*/
	
	for (i = 0; i < object_array_size(slotArray); i++) {
		struct Symbol* name = (struct Symbol*)object_array_get_element(slotArray, i);
		struct SlotEntry* se = slot_table_entry_for_name(oh, obj->map->slotTable, name);
		if (se == NULL) {
			interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL, resultStackPointer);
		} else {
			/*since the object was just cloned, we aren't expecting a tenured obj to point to a new one*/
			object_slot_value_at_offset_put(oh, newObj, object_to_smallint(se->offset), object_array_get_element(valueArray, i));
		}
	}
	
	oh->cached.interpreter->stack->elements[resultStackPointer] = newObj;
}

void prim_clone_with_slot_valued(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  Pinned<struct Object> obj(oh, args[0]), value(oh, args[2]);
  Pinned<struct Symbol> name(oh, (struct Symbol*)args[1]);
	
	if (object_is_smallint(obj)) {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL, resultStackPointer);
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = object_add_slot_named(oh, obj, name, value);
	}
}

void prim_clone_without_slot(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  Pinned<struct Object> obj(oh, args[0]);
  Pinned<struct Symbol> name(oh, (struct Symbol*)args[1]);
	
	if (object_is_smallint(obj)) {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, (struct Object*)name, NULL, resultStackPointer);
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = object_remove_slot(oh, obj, name);
	}
}

#pragma mark Map

void prim_map(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* obj;
	obj = args[0];
	
	if (object_is_smallint(obj)) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)obj->map;
	}
	
	
}

void prim_set_map(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  Pinned<struct Object> obj(oh);
  Pinned<struct Map> map(oh);
  obj = args[0];
  map = (struct Map*)args[1];
  
  if (object_is_smallint(obj) || object_is_immutable(obj)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  } else {
    object_change_map(oh, obj, map);
    heap_store_into(oh, args[0], args[1]);
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)map;
  }
  
}

#pragma mark Method

void prim_applyto(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  Pinned<struct Closure> method(oh);
  Pinned<struct OopArray> argArray(oh);
  Pinned<struct OopArray> real_opts(oh);

  method = (struct Closure*)args[0];
  argArray = (struct OopArray*) args[1];
  
  if (opts != NULL && opts->elements[1] != oh->cached.nil) {
    real_opts = (struct OopArray*) opts->elements[1];
  }
  interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, method,
                                            argArray->elements, array_size(argArray), real_opts, resultStackPointer);
}

void prim_applytoNewStack(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  Pinned<struct Closure> method(oh);
  Pinned<struct OopArray> argArray(oh);
  Pinned<struct OopArray> real_opts(oh);
  word_t thisFrameSize, shiftOffset;
  struct Interpreter* i = oh->cached.interpreter;
  method = (struct Closure*)args[0];
  argArray = (struct OopArray*) args[1];
  
  if (opts != NULL && opts->elements[1] != oh->cached.nil) {
    real_opts = (struct OopArray*) opts->elements[1];
  }

  shiftOffset = i->framePointer - FUNCTION_FRAME_SIZE;
  thisFrameSize = FUNCTION_FRAME_SIZE + object_to_smallint(method->method->registerCount);
  copy_words_into(&i->stack->elements[shiftOffset],
                  thisFrameSize,
                  &i->stack->elements[0]);

  i->framePointer = FUNCTION_FRAME_SIZE;
  i->stackPointer = thisFrameSize;
  i->ensureHandlers = smallint_to_object(0);

  i->stack->elements[i->framePointer - 6] = smallint_to_object(0); // before call stack pointer
  i->stack->elements[i->framePointer - 5] = smallint_to_object(0); //resultStackPointer


  interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, method,
                                            argArray->elements, array_size(argArray), real_opts, resultStackPointer - shiftOffset);
}

void prim_findon(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct MethodDefinition* def;
  Pinned<struct Symbol> selector(oh);
  Pinned<struct OopArray> arguments(oh);
	
  selector = (struct Symbol*) args[0];
  arguments = (struct OopArray*) args[1];

  def = method_dispatch_on(oh, selector, arguments->elements, array_size(arguments), NULL);
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = (def == NULL ? oh->cached.nil : (struct Object*) def->method);
}

void prim_ensure(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	
  Pinned<struct Closure> body(oh);
  Pinned<struct Object> ensureHandler(oh);

  body = (struct Closure*) args[0];
  ensureHandler = args[1];

  interpreter_apply_to_arity_with_optionals(oh, oh->cached.interpreter, body, NULL, 0, NULL, resultStackPointer);
  /*the registers are already allocated on the stack so we don't worry about overwriting them*/
  interpreter_stack_push(oh, oh->cached.interpreter, oh->cached.interpreter->ensureHandlers);
  interpreter_stack_push(oh, oh->cached.interpreter, ensureHandler);
  oh->cached.interpreter->ensureHandlers = smallint_to_object(oh->cached.interpreter->stackPointer - 2);
#ifdef PRINT_DEBUG_ENSURE
	printf("ensure handlers at %" PRIdPTR "\n", oh->cached.interpreter->stackPointer - 2);
#endif
	
}

void prim_send_to(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer) {
  Pinned<struct Symbol> selector(oh,(struct Symbol*)args[0]);
  Pinned<struct OopArray> opts(oh), arguments(oh, (struct OopArray*)args[1]);
	
  if (optionals == NULL) {
    opts = NULL;
  } else {
    opts = (struct OopArray*)optionals->elements[1];
    if (opts == (struct OopArray*)oh->cached.nil) opts = NULL;
  }
  send_to_through_arity_with_optionals(oh, selector, array_elements(arguments), array_elements(arguments), array_size(arguments), opts, resultStackPointer); 
}

void prim_send_to_through(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer) {
  Pinned<struct Symbol> selector(oh, (struct Symbol*)args[0]);
  Pinned<struct OopArray> opts(oh), arguments(oh, (struct OopArray*)args[1]), dispatchers(oh, (struct OopArray*)args[2]);
	
  if (optionals == NULL) {
    opts = NULL;
  } else {
    opts = (struct OopArray*)optionals->elements[1];
    if (opts == (struct OopArray*)oh->cached.nil) opts = NULL;
  }
  /*fix check array sizes are the same*/
  send_to_through_arity_with_optionals(oh, selector, array_elements(arguments), array_elements(dispatchers), array_size(arguments), opts, resultStackPointer); 
}

/* Method asMethod: selector on: rolesArray */
void prim_as_method_on(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
        Pinned<struct MethodDefinition> def(oh);
	Pinned<struct Object> method(oh);
        Pinned<struct Object> roles(oh);
        method = args[0];
        roles = args[2];
	Pinned<struct Symbol> selector(oh);
        selector = (struct Symbol*)args[1];
	Pinned<struct Object> traitsWindow(oh);
        traitsWindow = method->map->delegates->elements[0];
	Pinned<struct Object> closure(oh);
        std::vector<Pinned<struct Object> > pinnedRoles(object_array_size(roles), Pinned<struct Object>(oh));
        for (int i = 0; i < object_array_size(roles); i++) {
          pinnedRoles[i] = ((struct OopArray*)roles)->elements[i];
        }
	
	if (traitsWindow == get_special(oh, SPECIAL_OOP_CLOSURE_WINDOW)) {
                closure = heap_clone(oh, method);
		((struct Closure*)closure)->method = (struct CompiledMethod*)heap_clone(oh, (struct Object*)((struct Closure*)closure)->method);
		heap_store_into(oh, (struct Object*)closure, (struct Object*)((struct Closure*)closure)->method);
		((struct Closure*)closure)->method->method = ((struct Closure*)closure)->method;
		((struct Closure*)closure)->method->selector = selector;
		method = (struct Object*)closure;
	} else {
                
                closure = heap_clone(oh, method);
		((struct CompiledMethod*)closure)->method = closure;
		((struct CompiledMethod*)closure)->selector = selector;
		method = (struct Object*) closure;
	}
	def = method_define(oh, method, (struct Symbol*)selector, ((struct OopArray*)roles)->elements, object_array_size(roles));
	def->slotAccessor = oh->cached.nil;
	method_flush_cache(oh, selector);
#ifdef PRINT_DEBUG_DEFUN
	if (!oh->quiet) {
		printf("Defining function '"); print_symbol(selector);
		printf("' on: ");
		if (!print_printname(oh, ((struct OopArray*)roles)->elements[0])) printf("NoRole");
		{
			word_t i;
			for (i = 1; i < object_array_size(roles); i++) {
				printf(", ");
				if (!print_printname(oh, ((struct OopArray*)roles)->elements[i])) printf("NoRole");
			}
		}
		printf("\n");
	}
#endif
	
	oh->cached.interpreter->stack->elements[resultStackPointer] = method;
}

/* Method removeFrom: rolesArray */
void prim_removefrom(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	
  Pinned<struct Object> method(oh, args[0]), traitsWindow(oh);
  Pinned<struct OopArray> roles(oh, (struct OopArray*)args[1]);
  Pinned<struct Symbol> selector(oh);
  selector = (struct Symbol*)oh->cached.nil;
  Pinned<struct MethodDefinition> def(oh);
  word_t i;
	
	traitsWindow = method->map->delegates->elements[0];
	
	if (traitsWindow == oh->cached.closure_method_window || traitsWindow == oh->cached.compiled_method_window) {
		selector = ((struct Closure*)method)->method->selector;
	} else {
		/*May only remove a CompiledMethod or Closure.*/
		assert(0);
	};
	
	def = method_is_on_arity(oh, method, selector, array_elements(roles), array_size(roles));
	if ((struct Object*)def == NULL) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = method;
		return;
	};
	
	for (i = 0; i < array_size(roles); i++) {
		struct Object* role = array_elements(roles)[i];
		if (!object_is_smallint(role)) {
			object_remove_role(oh, role, selector, def);
		}
	};
	method_flush_cache(oh, selector);
	oh->cached.interpreter->stack->elements[resultStackPointer] = method;
}

void prim_as_accessor(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  Pinned<struct Object> method(oh);
  method = args[0];
  Pinned<struct Object> slot(oh);
  slot = args[2];
  Pinned<struct OopArray> roles(oh);
  roles = (struct OopArray*)args[3];
  Pinned<struct Symbol> selector(oh);
  selector = (struct Symbol*)args[1];
  Pinned<struct Object> traitsWindow(oh, method->map->delegates->elements[0]);
  struct MethodDefinition* def;
  Pinned<struct Object> closure(oh);
  std::vector<Pinned<struct Object> > pinnedRoles(object_array_size(roles), Pinned<struct Object>(oh));
  for (int i = 0; i < object_array_size(roles); i++) {
    pinnedRoles[i] = roles->elements[i];
  }
	
	if (traitsWindow == oh->cached.closure_method_window) {
	        closure = heap_clone(oh, method);
		((struct Closure*)closure)->method = (struct CompiledMethod*)heap_clone(oh, (struct Object*)((struct Closure*)closure)->method);
		heap_store_into(oh, (struct Object*)closure, (struct Object*)((struct Closure*)closure)->method);
		((struct Closure*)closure)->method->method = ((struct Closure*)closure)->method;
		((struct Closure*)closure)->method->selector = selector;
		method = (struct Object*)closure;
	} else if (traitsWindow == oh->cached.compiled_method_window){
		closure = heap_clone(oh, method);
		((struct CompiledMethod*)closure)->method = closure;
		((struct CompiledMethod*)closure)->selector = selector;
		method = (struct Object*) closure;
	}

	def = method_define(oh, method, selector, roles->elements, array_size(roles));
	def->slotAccessor = slot;
	method_flush_cache(oh, selector);
	oh->cached.interpreter->stack->elements[resultStackPointer] = method;
	
#ifdef PRINT_DEBUG_DEFUN
	if (!oh->quiet) {
		printf("Defining accessor '"); print_symbol(selector);
		printf("' on: ");
		if (!print_printname(oh, ((struct OopArray*)roles)->elements[0])) printf("NoRole");
		{
			word_t i;
			for (i = 1; i < array_size(roles); i++) {
				printf(", ");
				if (!print_printname(oh, ((struct OopArray*)roles)->elements[i])) printf("NoRole");
			}
		}
		printf("\n");
	}
#endif
}

#pragma mark Array

void prim_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* array;
	word_t i;
	
	array = args[0];
	i = object_to_smallint(args[1]);
	ASSURE_SMALLINT_ARG(1);  
	if (i < object_array_size(array) && i >= 0) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = ((struct OopArray*)array)->elements[i];
	} else {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[1], args[0], NULL, resultStackPointer);
	}
}

void prim_at_put(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object *array = args[0], *i = args[1], *val = args[2];
	word_t index = object_to_smallint(i);
	
	ASSURE_SMALLINT_ARG(1);
	if (object_is_immutable(array)) {
		interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), array, NULL, resultStackPointer);
		return;
	}
	
	if (index < object_array_size(array)) {
		heap_store_into(oh, array, val);
		oh->cached.interpreter->stack->elements[resultStackPointer] =  ((struct OopArray*)array)->elements[index] = val;
	} else {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, array, NULL, resultStackPointer);
	}
}

void prim_ooparray_newsize(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	/*struct Object* array = args[0];*/
	struct Object* i = args[1];
	if (object_is_smallint(i)) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)
		heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO),
								   object_to_smallint(i));
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
	}
}

void prim_size(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_array_size(args[0]));
}

#pragma mark ByteArray

void prim_bytearray_newsize(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* obj, *i;
	obj = args[0];
	i = args[1];
	
	if (!object_is_smallint(i)) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
		return;
	}
	
	oh->cached.interpreter->stack->elements[resultStackPointer] = 
	(struct Object*)heap_clone_byte_array_sized(oh, obj, (object_to_smallint(i) < 0) ? 0 : object_to_smallint(i));
}

void prim_bytesize(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(payload_size(args[0]));
}

void prim_byteat_put(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* obj= args[0], *i=args[1], *val = args[2];
	word_t index;
	
	index = object_to_smallint(i);
	
	ASSURE_SMALLINT_ARG(1);
	ASSURE_SMALLINT_ARG(2);
	
	if (object_is_immutable(obj)) {
		interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_IMMUTABLE), obj, NULL, resultStackPointer);
		return;
	}
	
	if (index < byte_array_size((struct ByteArray*)obj)) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(byte_array_set_element((struct ByteArray*)obj, index, object_to_smallint(val)));
	} else {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, obj, NULL, resultStackPointer);
	}
	
}

void prim_byteat(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* obj, *i;
	word_t index;
	
	obj = args[0];
	i = args[1];
	index = object_to_smallint(i);
	
	ASSURE_SMALLINT_ARG(1);
	
	if (index < byte_array_size((struct ByteArray*)obj)) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(byte_array_get_element(obj, index));
	} else {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), i, obj, NULL, resultStackPointer);
	}
	
}

#pragma mark File

void prim_atEndOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	word_t handle = object_to_smallint(args[1]);
	ASSURE_SMALLINT_ARG(1);
	if (file_isatend(oh, handle)) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
	}
}

void prim_sizeOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	word_t handle = object_to_smallint(args[1]);
	word_t retval = file_sizeof(oh, handle);
	ASSURE_SMALLINT_ARG(1);
	oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_flush_output(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	/*struct Object *console=args[0];*/
	fflush(stdout);
	fflush(stderr);
	oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
}

void prim_handle_for(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	word_t handle;
	struct Object /**file=args[0],*/ *fname=args[1];
	
	handle = file_open(oh, (struct ByteArray*)fname, SF_READ|SF_WRITE);
	if (handle >= 0) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
	}
	
}

void prim_handleForNew(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	word_t handle;
	struct Object /**file=args[0],*/ *fname=args[1];
	
	handle = file_open(oh, (struct ByteArray*)fname, SF_READ|SF_WRITE|SF_CLEAR|SF_CREATE);
	if (handle >= 0) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
	}
	
}

void prim_handle_for_input(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	word_t handle;
	struct Object /**file=args[0],*/ *fname=args[1];
	
	handle = file_open(oh, (struct ByteArray*)fname, SF_READ);
	if (handle >= 0) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
	}
	
}

void prim_closePipe(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[0]);
  int retval;

  ASSURE_SMALLINT_ARG(0);
#ifdef WIN32
  retval = closesocket(handle);
#else
  retval = close(handle);
#endif
  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(retval);

}

void prim_readFromPipe(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* array = (struct ByteArray*) args[0];
  word_t handle = object_to_smallint(args[1]);
  word_t start = object_to_smallint(args[2]), end = object_to_smallint(args[3]);
  ssize_t retval;

  ASSURE_TYPE_ARG(0, TYPE_BYTE_ARRAY);
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(3);

  if (start < 0 || start >= byte_array_size(array)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[2], args[0], NULL, resultStackPointer);
    return;
  }

  if (end < start || end > byte_array_size(array)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[3], args[0], NULL, resultStackPointer);
    return;
  }

  retval = recv(handle, byte_array_elements(array)+start, end - start, 0);


  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(retval);


}

void prim_writeToPipe(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* array = (struct ByteArray*) args[0];
  word_t handle = object_to_smallint(args[1]);
  word_t start = object_to_smallint(args[2]), end = object_to_smallint(args[3]);
  ssize_t retval;

  ASSURE_TYPE_ARG(0, TYPE_BYTE_ARRAY);
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(3);

  if (start < 0 || start >= byte_array_size(array)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[2], args[0], NULL, resultStackPointer);
    return;
  }

  if (end < start || end > byte_array_size(array)) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_KEY_NOT_FOUND_ON), args[3], args[0], NULL, resultStackPointer);
    return;
  }

  retval = send(handle, byte_array_elements(array)+start, end - start, 0);

  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(retval);

}

/*FIXME this is a copy of the last function with only the select call changed*/
void prim_selectOnWritePipesFor(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  Pinned<struct OopArray> selectOn(oh);
  selectOn = (struct OopArray*) args[0];
  Pinned<struct OopArray> readyPipes(oh);
  word_t waitTime = object_to_smallint(args[1]);
  int retval, fdCount, maxFD;
  struct timeval tv;
  fd_set fdList;
  maxFD = 0;

  ASSURE_SMALLINT_ARG(1);

  if ((fdCount = socket_select_setup(selectOn, &fdList, &maxFD)) < 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }

  
  tv.tv_sec = waitTime / 1000000;
  tv.tv_usec = waitTime % 1000000;
  retval = select(maxFD+1, NULL, &fdList, NULL, &tv); 
  
  if (retval < 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }


  readyPipes = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), retval);
  socket_select_find_available(selectOn, &fdList, readyPipes, retval);

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)readyPipes;

}

#pragma mark Socket

void prim_socketCreate(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t domain = object_to_smallint(args[0]);
  word_t type = object_to_smallint(args[1]);
  word_t protocol = object_to_smallint(args[2]);
  word_t ret = socket(socket_lookup_domain(domain), socket_lookup_type(type), socket_lookup_protocol(protocol));
  int ret2 = 0;

  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  
  if (ret >= 0) {
    ret2 = socket_set_nonblocking(ret);
  } else {
    perror("socket create");
  };

  if (ret2 < 0) {
    perror("set nonblocking");
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(-1);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);
  }
}

void prim_socketListen(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  word_t size = object_to_smallint(args[1]);
  word_t ret;

  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);

  ret = listen(fd, size);
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);

}

void prim_socketAccept(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  word_t ret;
  struct sockaddr_storage addr;
  socklen_t len;
  Pinned<struct ByteArray> addrArray(oh);
  Pinned<struct OopArray> result(oh);

  ASSURE_SMALLINT_ARG(0);

  len = sizeof(addr);
  ret = accept(fd, (struct sockaddr*)&addr, &len);
  
  if (ret >= 0) {
    addrArray = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), sizeof(struct sockaddr_in));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);
    return;
  }
  
  result = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 2);

  object_array_set_element(oh, (struct Object*)result, 0, smallint_to_object(ret));
  object_array_set_element(oh, (struct Object*)result, 1, (struct Object*)addrArray);

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)result;

}

void prim_socketBind(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  struct ByteArray* address = (struct ByteArray*) args[1];
  word_t ret;

  ASSURE_SMALLINT_ARG(0);

  ret = bind(fd, (const struct sockaddr*)byte_array_elements(address), (socklen_t)byte_array_size(address));
  if (ret < 0) perror("bind");
  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);
}

void prim_socketConnect(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  struct ByteArray* address = (struct ByteArray*) args[1];
  word_t ret;

  ASSURE_SMALLINT_ARG(0);

  ret = connect(fd, (const struct sockaddr*)byte_array_elements(address), (socklen_t)byte_array_size(address));
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);

}

void prim_socketGetError(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t fd = object_to_smallint(args[0]);
  word_t ret;
  int optval;
  socklen_t optlen;
  optlen = 4;
  ASSURE_SMALLINT_ARG(0);

  ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, (socklen_t*)&optlen);

  if (ret == 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(optval);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);
  }

}

void prim_getAddrInfo(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* hostname = (struct ByteArray*)args[1];
  struct ByteArray* service = (struct ByteArray*)args[2];
  word_t family = object_to_smallint(args[3]);
  word_t type = object_to_smallint(args[4]);
  word_t protocol = object_to_smallint(args[5]);
  word_t flags = object_to_smallint(args[6]);
  word_t ret, serviceSize, hostnameSize;

  ASSURE_TYPE_ARG(1, TYPE_BYTE_ARRAY);
  ASSURE_SMALLINT_ARG(3);
  ASSURE_SMALLINT_ARG(4);
  ASSURE_SMALLINT_ARG(5);
  ASSURE_SMALLINT_ARG(6);

  if ((struct Object*)hostname == oh->cached.nil) {
    hostnameSize = 0;
  } else {
    hostnameSize = byte_array_size(hostname)+1;
  }

  if ((struct Object*)service == oh->cached.nil) {
    serviceSize = 0;
  } else {
    ASSURE_TYPE_ARG(2, TYPE_BYTE_ARRAY);
    serviceSize = byte_array_size(service)+1;
  }

  ret = socket_getaddrinfo(oh, hostname, hostnameSize, service, serviceSize, family, type, protocol, flags);

  oh->cached.interpreter->stack->elements[resultStackPointer] = SOCKET_RETURN(ret);

}

void prim_getAddrInfoResult(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t ticket = object_to_smallint(args[0]);
  if (ticket >= oh->socketTicketCount || ticket < 0
      || oh->socketTickets[ticket].inUse == 0 || oh->socketTickets[ticket].finished == 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }
  if (oh->socketTickets[ticket].result < 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(socket_return(oh->socketTickets[ticket].result));
  } else {
    word_t count, i;
    struct addrinfo* ai = oh->socketTickets[ticket].addrResult;
    struct addrinfo* current = ai;
    Pinned<struct OopArray> retval(oh);
    Pinned<struct OopArray> aResult(oh);
    count = 0;
    while (current != NULL) {
      current = current->ai_next;
      count++;
    }
    current = ai;
    retval = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), count);
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)retval;
    heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)retval);
    
    for (i = 0; i < count; i++) {
      aResult = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 6);
      struct ByteArray* aResultAddr;
      struct ByteArray* aResultCanonName;
      word_t canonNameLen = (current->ai_canonname == NULL)? 0 : strlen(current->ai_canonname);
      retval->elements[i] = (struct Object*)aResult;
      heap_store_into(oh, (struct Object*)retval, retval->elements[i]);
      aResult->elements[0] = smallint_to_object(current->ai_flags);
      aResult->elements[1] = smallint_to_object(socket_reverse_lookup_domain(current->ai_family));
      aResult->elements[2] = smallint_to_object(socket_reverse_lookup_type(current->ai_socktype));
      aResult->elements[3] = smallint_to_object(socket_reverse_lookup_protocol(current->ai_protocol));

      aResultAddr = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), current->ai_addrlen);
      aResult->elements[4] = (struct Object*)aResultAddr;
      heap_store_into(oh, (struct Object*)aResult, aResult->elements[4]);
      copy_bytes_into((byte_t*)current->ai_addr, current->ai_addrlen, aResultAddr->elements);
      if (canonNameLen == 0) {
        aResult->elements[5] = oh->cached.nil;
      } else {
        aResultCanonName = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), canonNameLen);
        aResult->elements[5] = (struct Object*)aResultCanonName;
        heap_store_into(oh, (struct Object*)aResult, aResult->elements[5]);
        copy_bytes_into((byte_t*)current->ai_canonname, canonNameLen, aResultCanonName->elements);
      }

      current = current->ai_next;
    }

  }
}

void prim_freeAddrInfoResult(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t ticket = object_to_smallint(args[0]);
  if (ticket >= oh->socketTicketCount || ticket < 0
      || oh->socketTickets[ticket].inUse == 0 || oh->socketTickets[ticket].finished == 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
    return;
  }
  free(oh->socketTickets[ticket].hostname);
  oh->socketTickets[ticket].hostname = 0;
  free(oh->socketTickets[ticket].service);
  oh->socketTickets[ticket].service = 0;
  freeaddrinfo(oh->socketTickets[ticket].addrResult);
  oh->socketTickets[ticket].addrResult = 0;
  
  oh->socketTickets[ticket].inUse = 0;
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
}


void prim_socketCreateIP(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t domain = object_to_smallint(args[0]);
  struct Object* address = args[1];
  word_t port = object_to_smallint(args[2]);
  /*  struct OopArray* options = (struct OopArray*) args[3];*/
  struct sockaddr_in* sin;
  struct sockaddr_in6* sin6;
  struct sockaddr_un* sun;
  Pinned<struct ByteArray> ret(oh);
  
  ASSURE_SMALLINT_ARG(0);

  switch (domain) {

  case SLATE_DOMAIN_LOCAL:
#ifdef WIN32
#else
    if (byte_array_size((struct ByteArray*)address) > 100) {
      ret = (struct ByteArray*)oh->cached.nil;
      break;
    }
    ret = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), sizeof(struct sockaddr_un));
    sun = (struct sockaddr_un*)byte_array_elements(ret);
    sun->sun_family = socket_lookup_domain(domain);
    ASSURE_TYPE_ARG(1, TYPE_BYTE_ARRAY);
    strncpy(sun->sun_path, (char*)byte_array_elements((struct ByteArray*)address), 100);
    sun->sun_path[byte_array_size((struct ByteArray*)address)] = '\0';
#endif
    break;

  case SLATE_DOMAIN_IPV4:
    ASSURE_SMALLINT_ARG(2);
    if (object_array_size(address) < 4) {
      ret = (struct ByteArray*)oh->cached.nil;
      break;
    }
    ret = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), sizeof(struct sockaddr_in));
    sin = (struct sockaddr_in*)byte_array_elements(ret);
    sin->sin_family = socket_lookup_domain(domain);
    sin->sin_port = htons((uint16_t)port);
    ASSURE_TYPE_ARG(1, TYPE_OOP_ARRAY);
    sin->sin_addr.s_addr = htonl(((object_to_smallint(object_array_get_element(address, 0)) & 0xFF) << 24)
      | ((object_to_smallint(object_array_get_element(address, 1)) & 0xFF) << 16)
      | ((object_to_smallint(object_array_get_element(address, 2)) & 0xFF) << 8)
      | (object_to_smallint(object_array_get_element(address, 3)) & 0xFF));
    break;

    /*fixme ipv6*/
  case SLATE_DOMAIN_IPV6:
    ASSURE_SMALLINT_ARG(2);
    if (object_array_size(address) < 16) {
      ret = (struct ByteArray*)oh->cached.nil;
      break;
    }
    ret = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), sizeof(struct sockaddr_in6));
    sin6 = (struct sockaddr_in6*)byte_array_elements(ret);
    sin6->sin6_family = socket_lookup_domain(domain);
    sin6->sin6_port = htons((uint16_t)port);
    ASSURE_TYPE_ARG(1, TYPE_OOP_ARRAY);
    {
      int i;
      for (i = 0; i < 16; i++)
        sin6->sin6_addr.s6_addr[i] = object_to_smallint(object_array_get_element(address, i)) & 0xFF;
    }
    break;
    
  default:
    ret = (struct ByteArray*)oh->cached.nil;
    break;
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)ret;

}

void prim_write_to_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *console=args[0], *n=args[1], *handle=args[2], *seq=args[3], *start=args[4];
  byte_t* bytes = &((struct ByteArray*)seq)->elements[0] + object_to_smallint(start);
  word_t size = object_to_smallint(n);


  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(4);

  assert(arity == 5 && console != NULL);

  oh->cached.interpreter->stack->elements[resultStackPointer] =
                         smallint_to_object(fwrite(bytes, 1, size, (object_to_smallint(handle) == 0)? stdout : stderr));

}

void prim_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  ASSURE_SMALLINT_ARG(1);

  file_close(oh, handle);
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

void prim_file_delete(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  char filename[SLATE_FILE_NAME_LENGTH];
  word_t len;
  len = extractCString((struct ByteArray*)args[1], (byte_t*)filename, sizeof(filename));
  oh->cached.interpreter->stack->elements[resultStackPointer] = ((file_delete(oh, filename)) ? oh->cached.true_object : oh->cached.false_object);
}

void prim_file_touch(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
}

void prim_file_rename_to(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  char src[SLATE_FILE_NAME_LENGTH], dest[SLATE_FILE_NAME_LENGTH];
  word_t srcLen, destLen;
  srcLen = extractCString((struct ByteArray*)args[1], (byte_t*)src, sizeof(src));
  destLen = extractCString((struct ByteArray*)args[2], (byte_t*)dest, sizeof(dest));
  oh->cached.interpreter->stack->elements[resultStackPointer] = ((file_rename_to(oh, src, dest)) ? oh->cached.true_object : oh->cached.false_object);
}

void prim_file_information(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  char filename[SLATE_FILE_NAME_LENGTH];
  word_t len;
  len = extractCString((struct ByteArray*)args[1], (byte_t*)filename, sizeof(filename));
  oh->cached.interpreter->stack->elements[resultStackPointer] = file_information(oh, filename);
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, oh->cached.interpreter->stack->elements[resultStackPointer]);
}

void prim_dir_make(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  char filename[SLATE_FILE_NAME_LENGTH];
  word_t len;
  len = extractCString((struct ByteArray*)args[1], (byte_t*)filename, sizeof(filename));
  oh->cached.interpreter->stack->elements[resultStackPointer] = ((dir_make(oh, filename)) ? oh->cached.true_object : oh->cached.false_object);
}

void prim_dir_rename_to(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  char src[SLATE_FILE_NAME_LENGTH], dest[SLATE_FILE_NAME_LENGTH];
  word_t srcLen, destLen;
  srcLen = extractCString((struct ByteArray*)args[1], (byte_t*)src, sizeof(src));
  destLen = extractCString((struct ByteArray*)args[2], (byte_t*)dest, sizeof(dest));
  oh->cached.interpreter->stack->elements[resultStackPointer] = ((dir_rename_to(oh, src, dest)) ? oh->cached.true_object : oh->cached.false_object);
}

void prim_dir_delete(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  char filename[SLATE_FILE_NAME_LENGTH];
  word_t len;
  len = extractCString((struct ByteArray*)args[1], (byte_t*)filename, sizeof(filename));
  oh->cached.interpreter->stack->elements[resultStackPointer] = ((dir_delete(oh, filename)) ? oh->cached.true_object : oh->cached.false_object);
}


void prim_readConsole_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t /*handle = object_to_smallint(args[2]),*/ n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;

  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(4);

  retval = fread((char*)(byte_array_elements(bytes) + start), 1, n, stdin);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);

}

void prim_read_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[2]), n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(4);
  retval = file_read(oh, handle, n, (char*)(byte_array_elements(bytes) + start));
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_write_to_from_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[2]), n = object_to_smallint(args[1]), start = object_to_smallint(args[4]);
  struct ByteArray* bytes = (struct ByteArray*)args[3];
  word_t retval;
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(4);
  retval = file_write(oh, handle, n, (char*)(byte_array_elements(bytes) + start));
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_reposition_to(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]), n = object_to_smallint(args[2]);
  word_t retval = file_seek(oh, handle, n);
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_positionOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  word_t retval = file_tell(oh, handle);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

#pragma mark Directory

void prim_dir_open(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* buf = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(dir_open(oh, buf));
}

void prim_dir_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  ASSURE_SMALLINT_ARG(1);

  dir_close(oh, handle);
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
}

void prim_dir_read(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t handle = object_to_smallint(args[1]);
  struct ByteArray* buf = (struct ByteArray*)args[2];
  word_t retval;

  ASSURE_SMALLINT_ARG(1);

  retval = dir_read(oh, handle, buf);

  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(retval);
}

void prim_dir_getcwd(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* buf = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(dir_getcwd(buf));
}

void prim_dir_setcwd(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* buf = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(dir_setcwd(buf));
}

#pragma mark Platform

void prim_bytesPerWord(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(sizeof(word_t));
}

int slate_refresh_systeminfo(struct object_heap* oh) {
  return !(uname(&oh->platform_info));
}

void prim_system_name(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* result;
  int resultLength;
  if (slate_refresh_systeminfo(oh)) {
    resultLength = strlen(oh->platform_info.nodename);
    result = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), strlen(oh->platform_info.nodename));
    copy_bytes_into((byte_t*)oh->platform_info.nodename, resultLength, result->elements);
  } else {
    result = (struct ByteArray*)oh->cached.nil;
  };
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)result;
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)result);
}

void prim_system_release(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *result;
  int resultLength;
  if (slate_refresh_systeminfo(oh)) {
    resultLength = strlen(oh->platform_info.release);
    result = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), strlen(oh->platform_info.release));
    copy_bytes_into((byte_t*)oh->platform_info.release, resultLength, result->elements);
  } else {
    result = (struct ByteArray*)oh->cached.nil;
  };
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)result;
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)result);
}

void prim_system_version(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *result;
  int resultLength;
  if (slate_refresh_systeminfo(oh)) {
    resultLength = strlen(oh->platform_info.version);
    result = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), strlen(oh->platform_info.version));
    copy_bytes_into((byte_t*)oh->platform_info.version, resultLength, result->elements);
  } else {
    result = (struct ByteArray*)oh->cached.nil;
  };
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)result;
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)result);
}

void prim_system_platform(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *result;
  int resultLength;
  if (slate_refresh_systeminfo(oh)) {
    resultLength = strlen(oh->platform_info.sysname);
    result = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), strlen(oh->platform_info.sysname));
    copy_bytes_into((byte_t*)oh->platform_info.sysname, resultLength, result->elements);
  } else {
    result = (struct ByteArray*)oh->cached.nil;
  };
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)result;
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)result);
}

void prim_system_machine(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *result;
  int resultLength;
  if (slate_refresh_systeminfo(oh)) {
    resultLength = strlen(oh->platform_info.machine);
    result = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), strlen(oh->platform_info.machine));
    copy_bytes_into((byte_t*)oh->platform_info.machine, resultLength, result->elements);
  } else {
    result = (struct ByteArray*)oh->cached.nil;
  };
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)result;
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)result);
}

void prim_environment_removekey(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *keyString = args[1];
  size_t keyLength = payload_size(keyString);
  char key[SLATE_FILE_NAME_LENGTH];
  memcpy(key, (char*)byte_array_elements((struct ByteArray*)keyString), keyLength);
  key[keyLength] = '\0';
#ifdef WIN32
  SetEnvironmentVariable(key, "");
#else
  unsetenv(key);
#endif
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
}

void prim_environment_atput(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *keyString = args[1];
  struct Object *valueString = args[2];
  size_t keyLength = payload_size(keyString);
  size_t valueLength = payload_size(valueString);
  char key[SLATE_FILE_NAME_LENGTH], value[SLATE_FILE_NAME_LENGTH];
  int success;
  memcpy(key, (char*)byte_array_elements((struct ByteArray*)keyString), keyLength);
  key[keyLength] = '\0';
  memcpy(value, (char*)byte_array_elements((struct ByteArray*)valueString), valueLength);
  value[valueLength] = '\0';
#ifdef WIN32
  success = SetEnvironmentVariable(key, value);
#else
  success = setenv(key, value, 1);
#endif
  oh->cached.interpreter->stack->elements[resultStackPointer] = (success ? oh->cached.false_object : oh->cached.true_object);
}

void prim_isLittleEndian(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  int x = 1;
  char little_endian = *(char*)&x;
  oh->cached.interpreter->stack->elements[resultStackPointer] = ((little_endian == 1) ? oh->cached.true_object : oh->cached.false_object);
}

void prim_system_execute(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *commandString = args[1];
  size_t commandLength = payload_size(commandString);
  char command[SLATE_FILE_NAME_LENGTH];
  memcpy(command, (char*)byte_array_elements((struct ByteArray*)commandString), commandLength);
  command[commandLength] = '\0';
  oh->cached.interpreter->stack->elements[resultStackPointer] = (system(command) ? oh->cached.false_object : oh->cached.true_object);
}

#pragma mark Time

#ifdef WIN32 // gettimeofday() ported to WIN32 for prim_timeSinceEpoch()

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

struct timezone 
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int tzflag = 0;

  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    tmpres /= 10;  /*convert into microseconds*/
    /*converting file time to unix epoch*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS; 
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }

  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }

  return 0;
}

#endif

void prim_timeSinceEpoch(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  int64_t time;
  int i;
  struct ByteArray* timeArray;
  const int arraySize = 8;

  timeArray = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), arraySize);

  time = getTickCount();

  for (i = 0; i < arraySize; i++) {
    timeArray->elements[i] = ((time >> (i * 8)) & 0xFF);
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)timeArray;
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)timeArray);
}

void prim_addressOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *handle=args[1], *offset=args[2];
  struct ByteArray* addressBuffer=(struct ByteArray*) args[3];
  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  if (object_is_smallint(handle) && object_is_smallint(offset) && (unsigned)byte_array_size(addressBuffer) >= sizeof(word_t)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] =
                           smallint_to_object(memarea_addressof(oh,
                                                              (int)object_to_smallint(handle), 
                                                              (int)object_to_smallint(offset),
                                                              byte_array_elements(addressBuffer)));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

#pragma mark ExternalLibrary

void prim_library_open(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *libname=args[1], *handle = args[2];

  if (openExternalLibrary(oh, (struct ByteArray*)libname, (struct ByteArray*)handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }

}

void prim_library_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *handle=args[1];

  if (closeExternalLibrary(oh, (struct ByteArray*) handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }

}

void prim_procAddressOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *handle=args[2], *symname=args[1];
  struct ByteArray* addressBuffer=(struct ByteArray*) args[3];

  if (lookupExternalLibraryPrimitive(oh, (struct ByteArray*) handle, (struct ByteArray *) symname, addressBuffer)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }

}

void prim_extlibError(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* messageBuffer=(struct ByteArray*) args[1];

  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(readExternalLibraryError(messageBuffer));
}

void prim_applyExternal(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  oh->cached.interpreter->stack->elements[resultStackPointer] =
                         applyExternalLibraryPrimitive(oh, (struct ByteArray*)args[1], 
                                                       (struct OopArray*)args[2],
                                                       args[3],
                                                       args[4],
                                                       (struct OopArray*)args[5]);

}

#pragma mark MemoryArea

void prim_memory_new(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object *size=args[1];
  word_t handle;

  if (!object_is_smallint(size)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }

  handle = (word_t)memarea_open(oh, object_to_smallint(size));
  if (handle >= 0) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(handle);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_memory_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1];
  if (object_is_smallint(handle)) {
    memarea_close(oh, object_to_smallint(handle));
  }
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

void prim_memory_size(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1];
  if (object_is_smallint(handle)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(memarea_sizeof(oh, object_to_smallint(handle)));
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_memory_addRef(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1];
  if (object_is_smallint(handle)) {
    memarea_addref(oh, object_to_smallint(handle));
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

void prim_memory_read(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct ByteArray* buf = (struct ByteArray*)args[0];
  word_t amount = object_to_smallint(args[1]), startingAt = object_to_smallint(args[3]),
    handle = object_to_smallint(args[2]);

  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(3);

  if (!memarea_handle_isvalid(oh, handle) 
      || byte_array_size(buf) < amount 
      || startingAt + amount >= oh->memory_sizes [handle]) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(-1);
    return;
  }
  
  oh->cached.interpreter->stack->elements[resultStackPointer] =
    smallint_to_object(memarea_write(oh, handle, startingAt, amount, byte_array_elements(buf)));

}

void prim_memory_write(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct ByteArray* buf = (struct ByteArray*)args[0];
  word_t amount = object_to_smallint(args[1]), startingAt = object_to_smallint(args[3]),
    handle = object_to_smallint(args[2]);

  ASSURE_SMALLINT_ARG(1);
  ASSURE_SMALLINT_ARG(2);
  ASSURE_SMALLINT_ARG(3);

  if (!memarea_handle_isvalid(oh, handle) 
      || byte_array_size(buf) < amount 
      || startingAt + amount >= oh->memory_sizes [handle]) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(-1);
    return;
  }
  
  oh->cached.interpreter->stack->elements[resultStackPointer] = 
    smallint_to_object(memarea_read(oh, handle, startingAt, amount, byte_array_elements(buf)));

}

void prim_memory_resizeTo(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Object* handle = args[1], *size = args[2];
  if (object_is_smallint(handle) && object_is_smallint(size)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = 
                           smallint_to_object(memarea_resize(oh, object_to_smallint(handle), object_to_smallint(size)));

  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }

}

void prim_smallint_at_slot_named(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* obj;
  struct Object* name;
  struct SlotEntry * se;
  struct Object * proto;
  
  obj = args[0];
  name = args[1];
  proto = get_special(oh, SPECIAL_OOP_SMALL_INT_PROTO);
  se = slot_table_entry_for_name(oh, proto->map->slotTable, (struct Symbol*)name);
  if (se == NULL) {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SLOT_NOT_FOUND_NAMED), obj, name, NULL, resultStackPointer);
  } else {
    word_t offset = object_to_smallint(se->offset);
    oh->cached.interpreter->stack->elements[resultStackPointer] =  object_slot_value_at_offset(proto, offset);
  }


}

void prim_frame_pointer_of(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  struct Interpreter* i = (struct Interpreter*)args[0];
  struct Symbol* selector = (struct Symbol*) args[1];
  struct CompiledMethod* method;
  word_t frame = i->framePointer;



  while (frame > FUNCTION_FRAME_SIZE) {
    method = (struct CompiledMethod*) i->stack->elements[frame-3];
    method = method->method; /*incase it's a closure and not a compiledmethod*/
    if (method->selector == selector) {
      oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(frame);
      return;
    }
    frame = object_to_smallint(i->stack->elements[frame-1]);
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;

}

#pragma mark Clone/Daemonize System

void prim_cloneSystem(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
#ifdef WIN32
#pragma message("TODO WIN32 port forking/cloning the system")
	return;
#else
	pid_t retval;
	int pipes[2];
	struct OopArray* array;
	
	/* make two pipes that we can use exclusively in each process to talk to the other */
	/*FIXME remap fds for safety*/
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipes) == -1) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
		return;
	}
	
	retval = fork2();
	
	if (retval == (pid_t)-1) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
		return;
	}
	
	array = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), 2);
	oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)array;
	
	if (!retval) {  /* child */
		array->elements[0] = oh->cached.false_object;
		array->elements[1] = smallint_to_object(pipes[0]);
	} else { /* parent */
		array->elements[0] = oh->cached.true_object;
		array->elements[1] = smallint_to_object(pipes[1]);
	}
	
#endif
}

/*
 void prim_cloneSystemInProcess(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
 #ifdef WIN32
 #pragma message("TODO WIN32 port cloning/threading a system")
 #else
 
 #endif
 }
 */

#ifdef SLATE_DAEMONIZE

/* Change this to whatever your daemon is called */
#define DAEMON_NAME "slatedaemon"

/* Change this to the user under which to run */
#define RUN_AS_USER "root"

static void child_handler(int signum) {
	switch(signum) {
		case SIGALRM: exit(EXIT_FAILURE); break;
		case SIGUSR1: exit(EXIT_SUCCESS); break;
		case SIGCHLD: exit(EXIT_FAILURE); break;
	}
}

void prim_daemonizeSystem(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	const char* lock_filename = (char*)byte_array_elements((struct ByteArray*) args[1]);
#ifdef WIN32
#pragma message("TODO WIN32 port daemonizing a system")
#else
	pid_t pid, sid, parent;
	int lfp = -1;
	
	/* already a daemon */
	if (getppid() == 1) return;
	
	/* Create the lock file as the current user */
	if (lock_filename && lock_filename[0]) {
		lfp = open(lock_filename,O_RDWR|O_CREAT,0640);
		if (lfp < 0) {
			printf("Unable to create lock file %s, code=%d (%s)",
				   lock_filename, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	
	/* Drop user if there is one, and we were run as root */
	if (getuid() == 0 || geteuid() == 0) {
		struct passwd *pw = getpwnam(RUN_AS_USER);
		if (pw) {
			if (!oh->quiet)
				printf("Setting user to " RUN_AS_USER);
			setuid(pw->pw_uid);
		}
	}
	
	/* Trap signals that we expect to receive */
	signal(SIGCHLD,child_handler);
	signal(SIGUSR1,child_handler);
	signal(SIGALRM,child_handler);
	
	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		printf("Unable to fork daemon, code=%d (%s)",
			   errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		
		/* Wait for confirmation from the child via SIGTERM or SIGCHLD, or
		 for two seconds to elapse (SIGALRM).  pause() should not return. */
		alarm(2);
		pause();
		
		exit(EXIT_FAILURE);
	}
	
	/* At this point we are executing as the child process */
	parent = getppid();
	
	/* Cancel certain signals */
	signal(SIGCHLD,SIG_DFL); /* A child process dies */
	signal(SIGTSTP,SIG_IGN); /* Various TTY signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGHUP, SIG_IGN); /* Ignore hangup signal */
	signal(SIGTERM,SIG_DFL); /* Die on SIGTERM */
	
	/* Change the file mode mask */
	umask(0);
	
	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		printf("Unable to create a new session, code %d (%s)",
			   errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	/* Change the current working directory.  This prevents the current
     directory from being locked; hence not being able to remove it. */
	if ((chdir("/")) < 0) {
		printf("Unable to change directory to %s, code %d (%s)",
			   "/", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	/* Redirect standard files to /dev/null */
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
	
	/* Tell the parent process that we are A-okay */
	kill(parent, SIGUSR1);
#endif
	oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
}

#endif //SLATE_DAEMONIZE

#pragma mark VM invocation arguments

/*TODO: obsolete*/
void prim_run_args_into(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* arguments = (struct ByteArray*)args[1];
  oh->cached.interpreter->stack->elements[resultStackPointer] = 
                         smallint_to_object(write_args_into(oh, (char*)byte_array_elements(arguments), byte_array_size(arguments)));

  
}

void prim_vmArgCount(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(oh->argcSaved);
}

void prim_vmArg(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  word_t i;
  int len;
  struct ByteArray* array;
  ASSURE_SMALLINT_ARG(1);  
  i = object_to_smallint(args[1]);

  if (i >= 0 && i < oh->argcSaved) {
    len = strlen(oh->argvSaved[i]);
    array = heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), len);
    copy_bytes_into((byte_t*)oh->argvSaved[i], len, array->elements);
    oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)array;
    heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)array);
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
  }
}

void prim_environmentVariables(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  int i, len;
  word_t lenstr;
  Pinned<struct OopArray> array(oh);

  len = 0;
  while (oh->envp[len]) len++;

  array = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), len);

  for (i = 0; i < len; i++) {
    lenstr = strlen(oh->envp[i]);
    array->elements[i] = (struct Object*) heap_clone_byte_array_sized(oh, get_special(oh, SPECIAL_OOP_BYTE_ARRAY_PROTO), lenstr);
    copy_bytes_into((byte_t*)oh->envp[i], lenstr, ((struct ByteArray*)array->elements[i])->elements);
    heap_store_into(oh, (struct Object*) array, (struct Object*) array->elements[i]);
  }

  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)array;
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)array);
}


void prim_startProfiling(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  if (oh->currentlyProfiling) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
    return;
  }

  profiler_start(oh);
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
}

void prim_stopProfiling(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  Pinned<struct OopArray> array(oh);
  std::vector<Pinned<struct OopArray> > pinnedArrays;
  std::vector<Pinned<struct Object> > pinnedMethods;
  word_t k;
  if (!oh->currentlyProfiling) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
    return;
  }

  /*we don't use heap_store_into below because everything is pinned and should be in the young obj area*/

  // gc before we allocate so we know how many profiledmethods to keep
  heap_full_gc(oh);

  /*method, callcount, selftime, childCounts, childTimes*/
  array = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO),
                                     oh->profiledMethods.size()*5);


#ifdef GC_BUG_CHECK
  for (std::set<struct Object*>::iterator i = oh->profiledMethods.begin();
       i != oh->profiledMethods.end();
       i++) {
    assert(object_hash(*i) < ID_HASH_RESERVED);
  }
#endif


  //pin all the methods so the next time we iterate over things aren't deleted from the list
  // while iterating
  for (std::set<struct Object*>::iterator i = oh->profiledMethods.begin();
       i != oh->profiledMethods.end();
       i++) {
    Pinned<struct Object> m(oh, *i);
    pinnedMethods.push_back(m);
  }

  // methods are pinned... we don't have to worry about redirecting things anymore after they're freed?
  profiler_stop(oh);


  k = 0;
  for (std::set<struct Object*>::iterator i = oh->profiledMethods.begin();
       i != oh->profiledMethods.end();
       i++) {
    struct Object* method = *i;
    int m = 0;
    Pinned<struct OopArray> childCounts(oh, heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO),
                                                                        oh->profilerChildCallCount[method].size()*2));
    Pinned<struct OopArray> childTimes(oh, heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO),
                                                                        oh->profilerChildCallTime[method].size()*2));
    
    m = 0;
    for (std::map<struct Object*,word_t>::iterator cci = oh->profilerChildCallCount[method].begin();
         cci != oh->profilerChildCallCount[method].end();
         cci++) {
#ifdef GC_BUG_CHECK
      assert_good_object(oh, (*cci).first);
#endif
      childCounts->elements[m++] = (*cci).first;
      childCounts->elements[m++] = smallint_to_object((*cci).second);
    }
    m = 0;
    for (std::map<struct Object*,word_t>::iterator cti = oh->profilerChildCallTime[method].begin();
         cti != oh->profilerChildCallTime[method].end();
         cti++) {
#ifdef GC_BUG_CHECK
      assert_good_object(oh, (*cti).first);
#endif
      childTimes->elements[m++] = (*cti).first;
      childTimes->elements[m++] = smallint_to_object((*cti).second);
    }

    pinnedArrays.push_back(childCounts);
    pinnedArrays.push_back(childTimes);

#ifdef GC_BUG_CHECK
      assert_good_object(oh, method);
      assert_good_object(oh, childCounts);
      assert_good_object(oh, childTimes);
#endif

    array->elements[k++] = method;
    array->elements[k++] = smallint_to_object(oh->profilerCallCounts[method]);
    array->elements[k++] = smallint_to_object(oh->profilerSelfTime[method]);
    array->elements[k++] = childCounts;
    array->elements[k++] = childTimes;
  }

  oh->profiledMethods.clear();
  pinnedArrays.clear();
  pinnedMethods.clear();

  oh->cached.interpreter->stack->elements[resultStackPointer] = array;
  heap_store_into(oh, (struct Object*)oh->cached.interpreter->stack, (struct Object*)array);
  //this is probably a mess, we should do a full gc so we don't crash
  heap_full_gc(oh);

}

void prim_profilerStatistics(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
}



void prim_heap_gc(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  if (!oh->quiet) {
    printf("Collecting garbage...\n");
  };
  heap_full_gc(oh);
}

void prim_save_image(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  char nameString [SLATE_FILE_NAME_LENGTH];
  struct slate_image_header sih;
  struct Object* name = args[1];
  size_t nameLength = payload_size(name);
  FILE * imageFile;
	
  word_t totalSize, forwardPointerEntryCount;
  byte_t* memoryStart;
  struct Object *writeObject;
  struct ForwardPointerEntry* forwardPointers;
  /* do a full gc, allocate a new chunk of memory the size of the young and old combined,
   * copy all the non-free objects to the new memory while keeping an array of the position changes,
   * go through the memory and fix up the pointers, adjust points to start from 0 instead of memoryStart,
   * and write the header and the memory out to disk
   */
	
  /*push true so if it resumes from the save image, it will do init code*/
  /*fixme*/
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
	
  if (nameLength >= sizeof(nameString)) {
    /*interpreter_stack_pop(oh, oh->cached.interpreter);*/
    /*push nil*/
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
    return;
  }
  memcpy(nameString, (char*)byte_array_elements((struct ByteArray*)name), nameLength);
  nameString[nameLength] = '\0';
	
  imageFile = fopen(nameString, "wb");
  if (!imageFile) {
    /*interpreter_stack_pop(oh, oh->cached.interpreter);*/
    /*push nil*/
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.nil;
		
    return;
  }
  printf("Saving image to %s\n", nameString);
  heap_full_gc(oh);
  totalSize = oh->memoryOldSize + oh->memoryYoungSize;
  forwardPointerEntryCount = ((totalSize / 4) + sizeof(struct ForwardPointerEntry) - 1) / sizeof(struct ForwardPointerEntry);
  memoryStart = (byte_t*)calloc(1, totalSize);
  writeObject = (struct Object*)memoryStart;
  forwardPointers = (struct ForwardPointerEntry*)calloc(1, forwardPointerEntryCount * sizeof(struct ForwardPointerEntry));
  assert(memoryStart != NULL);
  copy_used_objects(oh, &writeObject, oh->memoryOld, oh->memoryOldSize, forwardPointers, forwardPointerEntryCount);
  copy_used_objects(oh, &writeObject, oh->memoryYoung, oh->memoryYoungSize, forwardPointers, forwardPointerEntryCount);
  totalSize = (byte_t*)writeObject - memoryStart;
  adjust_object_fields_with_table(oh, memoryStart, totalSize, forwardPointers, forwardPointerEntryCount);
  adjust_oop_pointers_from(oh, 0-(word_t)memoryStart, memoryStart, totalSize);
  sih.magic = SLATE_IMAGE_MAGIC;
  sih.size = totalSize;
  sih.next_hash = heap_new_hash(oh);
  sih.special_objects_oop = (byte_t*) (forward_pointer_hash_get(forwardPointers, forwardPointerEntryCount, (struct Object*)oh->special_objects_oop)->toObj) - memoryStart;
  sih.current_dispatch_id = oh->current_dispatch_id;
	
  if (fwrite(&sih, sizeof(struct slate_image_header), 1, imageFile) != 1
      || fwrite(memoryStart, 1, totalSize, imageFile) != (size_t)totalSize) {
    fprintf(stderr, "Error writing image!\n");
  }
  fclose(imageFile);
  free(forwardPointers);
  free(memoryStart);
	
  oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  /*
    interpreter_stack_pop(oh, oh->cached.interpreter);
    interpreter_push_false(oh, oh->cached.interpreter);
  */
}

void prim_exit(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  /*  print_stack_types(oh, 128);*/
  /*  print_backtrace(oh);*/
  ASSURE_SMALLINT_ARG(1);
  if (!oh->quiet) {
    printf("Slate process %d exiting...\n", getpid());
  }
  exit(object_to_smallint(args[1]));
}

#pragma mark SmallInteger

void prim_equals(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (args[0] == args[1])?oh->cached.true_object:oh->cached.false_object;
}

void prim_less_than(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {

  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = 
    (object_to_smallint(args[0])<object_to_smallint(args[1]))?oh->cached.true_object:oh->cached.false_object;
}

void prim_bitand(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)((word_t)args[0] & (word_t)args[1]);
}
void prim_bitor(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)((word_t)args[0] | (word_t)args[1]);
}
void prim_bitxor(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)(((word_t)args[0] ^ (word_t)args[1])|SMALLINT_MASK);
}
void prim_bitnot(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  ASSURE_SMALLINT_ARG(0);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)(~((word_t)args[0]) | SMALLINT_MASK);
}

void prim_smallIntegerMinimum(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)(((word_t)1<< (sizeof(word_t)*8-1))|1); /*top and smallint bit set*/
}

void prim_smallIntegerMaximum(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)LONG_MAX; /*has all bits except the top set*/
}

void prim_plus(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct Object* x = args[0];
  struct Object* y = args[1];
  word_t z = object_to_smallint(x) + object_to_smallint(y);


  ASSURE_SMALLINT_ARG(0);
  ASSURE_SMALLINT_ARG(1);


  if (smallint_fits_object(z)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
  } else {
    interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_ADD_OVERFLOW), x, y, NULL, resultStackPointer);
  }
}

void prim_exponent(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* x = (struct ByteArray*)args[0];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object((*(word_t*)float_part(x) >> FLOAT_EXPONENT_OFFSET) & 0xFF);

}

void prim_significand(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray* x = (struct ByteArray*)args[0];
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(*(word_t*)float_part(x) & FLOAT_SIGNIFICAND);

}

void prim_withSignificand_exponent(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	/*this is really a bytearray*/
	word_t significand = object_to_smallint(args[1]), exponent = object_to_smallint(args[2]);
	struct ByteArray* f = heap_new_float(oh);
	*((word_t*)float_part(f)) = significand | exponent << FLOAT_EXPONENT_OFFSET;
	
	oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)f;
	
}

/* SmallInteger bitShift: SmallInteger */
void prim_bitshift(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	word_t bits = object_to_smallint(args[0]);
	word_t shift = object_to_smallint(args[1]);
	word_t z;
	
	ASSURE_SMALLINT_ARG(0);
	ASSURE_SMALLINT_ARG(1);
	
	if (shift >= 0) {
		if (shift >= __WORDSIZE && bits != 0) {
			interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_BIT_SHIFT_OVERFLOW), args[0], args[1], NULL, resultStackPointer);
			return;
		}
		
		z = bits << shift;
		
		if (!smallint_fits_object(z) || z >> shift != bits) {
			interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_BIT_SHIFT_OVERFLOW), args[0], args[1], NULL, resultStackPointer);
			return;
		}
		
	} else if (shift <= -__WORDSIZE) {
		z = bits >> (__WORDSIZE-1);
	} else {
		z = bits >> -shift;
	}
	
	oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
	
}

/* SmallInteger - SmallInteger */
void prim_minus(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* x = args[0];
	struct Object* y = args[1];
	word_t z = object_to_smallint(x) - object_to_smallint(y);
	
	ASSURE_SMALLINT_ARG(0);
	ASSURE_SMALLINT_ARG(1);
	
	if (smallint_fits_object(z)) {
		oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
	} else {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_SUBTRACT_OVERFLOW), x, y, NULL, resultStackPointer);
	}
}

/* SmallInteger * SmallInteger */
void prim_times(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	word_t x = object_to_smallint(args[0]);
	word_t y = object_to_smallint(args[1]);
	word_t z = x * y;
	
	
	ASSURE_SMALLINT_ARG(0);
	ASSURE_SMALLINT_ARG(1);
	
	
	if (y != 0 && (z / y != x || !smallint_fits_object(z))) { /*thanks slava*/
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_MULTIPLY_OVERFLOW), args[0], args[1], NULL, resultStackPointer);
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(z);
	}
}

/* SmallInteger quo: SmallInteger */
void prim_quo(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
	struct Object* x = args[0];
	struct Object* y = args[1];
	
	ASSURE_SMALLINT_ARG(0);
	ASSURE_SMALLINT_ARG(1);
	
	if (object_to_smallint(y) == 0) {
		interpreter_signal_with_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_DIVIDE_BY_ZERO), x, y, NULL, resultStackPointer);
	} else {
		oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object(object_to_smallint(x) / object_to_smallint(y));
	}
}

#pragma mark Float

void prim_float_equals(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  if (*float_part(x) == *float_part(y)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }
}

void prim_float_less_than(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  if (*float_part(x) < *float_part(y)) {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.true_object;
  } else {
    oh->cached.interpreter->stack->elements[resultStackPointer] = oh->cached.false_object;
  }
}

void prim_float_plus(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) + *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_minus(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) - *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}


void prim_float_times(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) * *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_divide(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = *float_part(x) / *float_part(y);
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_raisedTo(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0], *y = (struct ByteArray*)args[1];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = pow(*float_part(x), *float_part(y));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_ln(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = log(*float_part(x));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_exp(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = exp(*float_part(x));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_float_sin(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  struct ByteArray *x = (struct ByteArray*)args[0];
  struct ByteArray* z = heap_new_float(oh);
  *float_part(z) = sin(*float_part(x));
  oh->cached.interpreter->stack->elements[resultStackPointer] = (struct Object*)z;
}

void prim_objectPointerAddress(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer) {
  oh->cached.interpreter->stack->elements[resultStackPointer] = smallint_to_object((word_t)args[1]);
}

void (*primitives[]) (struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer) = {

 /*0-9*/ prim_as_method_on, prim_as_accessor, prim_map, prim_set_map, prim_fixme, prim_removefrom, prim_clone, prim_clone_setting_slots, prim_clone_with_slot_valued, prim_fixme, 
 /*10-9*/ prim_fixme, prim_fixme, prim_clone_without_slot, prim_at_slot_named, prim_smallint_at_slot_named, prim_at_slot_named_put, prim_forward_to, prim_bytearray_newsize, prim_bytesize, prim_byteat, 
 /*20-9*/ prim_byteat_put, prim_ooparray_newsize, prim_size, prim_at, prim_at_put, prim_ensure, prim_applyto, prim_send_to, prim_send_to_through, prim_findon, 
 /*30-9*/ prim_fixme, prim_run_args_into, prim_exit, prim_isIdenticalTo, prim_identity_hash, prim_identity_hash_univ, prim_equals, prim_less_than, prim_bitor, prim_bitand, 
 /*40-9*/ prim_bitxor, prim_bitnot, prim_bitshift, prim_plus, prim_minus, prim_times, prim_quo, prim_fixme, prim_fixme, prim_frame_pointer_of, 
 /*50-9*/ prim_fixme, prim_fixme, prim_fixme, prim_heap_gc, prim_bytesPerWord, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, 
 /*60-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_readConsole_from_into_starting_at, prim_write_to_starting_at, prim_flush_output, prim_handle_for, prim_handle_for_input, prim_fixme, 
 /*70-9*/ prim_handleForNew, prim_close, prim_read_from_into_starting_at, prim_write_to_from_starting_at, prim_reposition_to, prim_positionOf, prim_atEndOf, prim_sizeOf, prim_save_image, prim_dir_open, 
 /*80-9*/ prim_dir_close, prim_dir_read, prim_dir_getcwd, prim_dir_setcwd, prim_significand, prim_exponent, prim_withSignificand_exponent, prim_float_equals, prim_float_less_than, prim_float_plus, 
 /*90-9*/ prim_float_minus, prim_float_times, prim_float_divide, prim_float_raisedTo, prim_float_ln, prim_float_exp, prim_float_sin, prim_fixme, prim_fixme, prim_fixme, 
 /*00-9*/ prim_fixme, prim_fixme, prim_fixme, prim_memory_new, prim_memory_close, prim_memory_addRef, prim_memory_write, prim_memory_read, prim_memory_size, prim_memory_resizeTo,
 /*10-9*/ prim_addressOf, prim_library_open, prim_library_close, prim_procAddressOf, prim_extlibError, prim_applyExternal, prim_timeSinceEpoch, prim_cloneSystem, prim_readFromPipe, prim_writeToPipe,
 /*20-9*/ prim_selectOnReadPipesFor, prim_selectOnWritePipesFor, prim_closePipe, prim_socketCreate, prim_socketListen, prim_socketAccept, prim_socketBind, prim_socketConnect, prim_socketCreateIP, prim_smallIntegerMinimum,
 /*30-9*/ prim_smallIntegerMaximum, prim_socketGetError, prim_getAddrInfo, prim_getAddrInfoResult, prim_freeAddrInfoResult, prim_vmArgCount, prim_vmArg, prim_environmentVariables, prim_environment_atput, prim_environment_removekey,
 /*40-9*/ prim_isLittleEndian, prim_system_name, prim_system_release, prim_system_version, prim_system_platform, prim_system_machine, prim_system_execute, prim_startProfiling, prim_stopProfiling, prim_profilerStatistics,
 /*50-9*/ prim_file_delete, prim_file_touch, prim_file_rename_to, prim_file_information, prim_dir_make, prim_dir_rename_to, prim_dir_delete, prim_objectPointerAddress, prim_applytoNewStack, prim_fixme,
 /*60-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,
 /*70-9*/ prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme, prim_fixme,

};
