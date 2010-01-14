#include "slate.hpp"

static const char* opcode_names[] = {
  "direct send message ",
  "indirect send message ",
  "-",
  "load literal ",
  "store literal ",
  "send message with opts ",
  "-",
  "new closure ",
  "new array with ",
  "resend message ",
  "return from ",
  "load env ",
  "load var ",
  "store var ",
  "load free var ",
  "store free var ",
  "is identical to ",
  "branch keyed ",
  "jump to ",
  "move reg ",
  "branch true ",
  "branch false ",
  "return reg ",
  "return val ",
  "resume ",
  "primitiveDo ",
  "directApplyTo ",
  "is nil ",
  "inline primitive check jump ",
  "inline method check jump "
};

void print_opcode_args(struct object_heap* oh, std::vector<struct Object*>& code, size_t start, size_t args) {
  for (size_t i = start; i < start+args; i++) {
    print_type(oh, code[i]);
  }
}


// this should eventually obsolete print_code_disassembled
void print_code(struct object_heap* oh, std::vector<struct Object*> code) {

  for (size_t i = 0; i < code.size(); i += opcode_length(code, i)) {
    printf("[%" PRIuMAX "] ", i);
    word_t rawop = (word_t)code[i];
    if (rawop >= OP_INTERNAL_SEND) {
      printf("<internal>");
    } else {
      printf("%s", opcode_names[rawop>>1]);
    }
    print_opcode_args(oh, code, i+1, opcode_length(code, i)-1);
    printf("\n");
  }

  
}

//fixme redundant with debug.cpp
word_t opcode_length(std::vector<struct Object*>& code, word_t start){
  return 1 + opcode_base_length((word_t)code[start]) + opcode_arg_length(code, start);
}

// the opcode vararg length
word_t opcode_arg_length(std::vector<struct Object*>& code, word_t start) {
  word_t op = (word_t)code[start];
  word_t i = start+1;
  switch (op) {
  case OP_SEND: case OP_INTERNAL_SEND: return object_to_smallint(code[i+2]);
  case OP_SEND_MESSAGE_WITH_OPTS: return object_to_smallint(code[i+2]);
  case OP_NEW_ARRAY_WITH: return object_to_smallint(code[i+1]);
  case OP_PRIMITIVE_DO: return object_to_smallint(code[i+1]);
  case OP_APPLY_TO: return object_to_smallint(code[i+1]);
  case OP_INLINE_PRIMITIVE_CHECK: return object_to_smallint(code[i+3]);
  case OP_INLINE_METHOD_CHECK: return object_to_smallint(code[i+1]);


  case OP_LOAD_LITERAL:
  case OP_NEW_CLOSURE:
  case OP_RESEND_MESSAGE:
  case OP_RETURN_FROM:
  case OP_LOAD_ENVIRONMENT:
  case OP_LOAD_VARIABLE:
  case OP_STORE_VARIABLE:
  case OP_LOAD_FREE_VARIABLE:
  case OP_STORE_FREE_VARIABLE:
  case OP_IS_IDENTICAL_TO:
  case OP_BRANCH_KEYED:
  case OP_JUMP_TO:
  case OP_MOVE_REGISTER:
  case OP_BRANCH_IF_TRUE:
  case OP_BRANCH_IF_FALSE:
  case OP_RETURN_REGISTER:
  case OP_RETURN_VALUE:
  case OP_RESUME:
  case OP_IS_NIL:
    return 0;


  default: assert(0);
  }
  return 0;
}

// amount of arguments for an op not including varargs
word_t opcode_base_length(word_t rawop) {
  switch (rawop) {
  case OP_SEND: case OP_INTERNAL_SEND: return 3; 
  case OP_LOAD_LITERAL: return 2;
  case OP_SEND_MESSAGE_WITH_OPTS: return 4;
  case OP_NEW_CLOSURE: return 2;
  case OP_NEW_ARRAY_WITH: return 2;
  case OP_RESEND_MESSAGE: return 2;
  case OP_RETURN_FROM: return 2;
  case OP_LOAD_ENVIRONMENT: return 1;
  case OP_LOAD_VARIABLE: return 1;
  case OP_STORE_VARIABLE: return 1;
  case OP_LOAD_FREE_VARIABLE: return 3;
  case OP_STORE_FREE_VARIABLE: return 3;
  case OP_IS_IDENTICAL_TO: return 3;
  case OP_BRANCH_KEYED: return 2;
  case OP_JUMP_TO: return 1;
  case OP_MOVE_REGISTER: return 2;
  case OP_BRANCH_IF_TRUE: return 2;
  case OP_BRANCH_IF_FALSE: return 2;
  case OP_RETURN_REGISTER: return 1;
  case OP_RETURN_VALUE: return 1;
  case OP_RESUME: return 0;
  case OP_PRIMITIVE_DO: return 3;
  case OP_APPLY_TO: return 3;
  case OP_IS_NIL: return 2;
  case OP_INLINE_PRIMITIVE_CHECK: return 5;
  case OP_INLINE_METHOD_CHECK: return 3;
  default: assert(0);
  }
  return 0;

}

// what argument is the jump for the given op
word_t opcode_jump_offset(word_t rawop) {
  switch (rawop) {
  case OP_JUMP_TO: return 1;
  case OP_BRANCH_IF_FALSE: return 2;
  case OP_BRANCH_IF_TRUE: return 2;
  case OP_INLINE_PRIMITIVE_CHECK: return 5;
  case OP_INLINE_METHOD_CHECK: return 3;
  default: assert(0);
  }
  return 0;
}

// where the jump (offset) is relative from the end of the instruction
word_t opcode_jump_adjustment(word_t rawop) {
  switch (rawop) {
  case OP_JUMP_TO: return -1;
  case OP_BRANCH_IF_FALSE: return -1;
  case OP_BRANCH_IF_TRUE: return -1;
  case OP_INLINE_PRIMITIVE_CHECK: return 0;
  case OP_INLINE_METHOD_CHECK: return 0;
  default: assert(0);
  }
  return 0;
}

bool opcode_can_jump(word_t rawop) {
  switch (rawop) {
  case OP_JUMP_TO:
  case OP_BRANCH_IF_FALSE:
  case OP_BRANCH_IF_TRUE:
  case OP_INLINE_PRIMITIVE_CHECK:
  case OP_INLINE_METHOD_CHECK:
    return true;
  default: 
    // i just have this to throw an error of it's an invalid instruction
    assert(opcode_base_length(rawop) >= 0);
    return false;

  }
  return 0;
}


// bit or-ing with LSB being the first argument and MSB being the last argument
// all addition arguments (when opcode_arg_len > 0), are register arguments
word_t opcode_register_locations(word_t rawop) {
  switch (rawop) {
  case OP_SEND: case OP_INTERNAL_SEND: return 1;
  case OP_LOAD_LITERAL: return 1;
  case OP_SEND_MESSAGE_WITH_OPTS: return 1+8;
  case OP_NEW_CLOSURE: return 1;
  case OP_NEW_ARRAY_WITH: return 1;

  case OP_RESEND_MESSAGE: return 1;
  case OP_RETURN_FROM: return 1;
  case OP_LOAD_ENVIRONMENT: return 1;
  case OP_LOAD_VARIABLE: return 1;
  case OP_STORE_VARIABLE: return 1;
  case OP_LOAD_FREE_VARIABLE: return 1;
  case OP_STORE_FREE_VARIABLE: return 4;
  case OP_IS_IDENTICAL_TO: return 1+2+4;
  case OP_BRANCH_KEYED: return 1+2;
  case OP_JUMP_TO: return 0;
  case OP_MOVE_REGISTER: return 1+2;
  case OP_BRANCH_IF_TRUE: return 1;
  case OP_BRANCH_IF_FALSE: return 1;
  case OP_RETURN_REGISTER: return 1;
  case OP_RETURN_VALUE: return 0;
  case OP_RESUME: return 0;
  case OP_PRIMITIVE_DO: return 1+4;
  case OP_APPLY_TO: return 1+4;
  case OP_IS_NIL: return 1+2;
  case OP_INLINE_PRIMITIVE_CHECK: return 1;
  case OP_INLINE_METHOD_CHECK: return 0;
  default: assert(0);
  }

  return 0;
}

void optimizer_append_code_to_vector(struct OopArray* code, std::vector<struct Object*>& vector) {
  std::copy(code->elements, code->elements + array_size(code), std::back_inserter(vector));

}

void optimizer_offset_value(std::vector<struct Object*>& code, word_t pos, word_t offset) {
  struct Object* obj = code[pos];
  assert(object_is_smallint(obj));
  code[pos] = smallint_to_object(object_to_smallint(obj) + offset);
}

void optimizer_offset_registers(std::vector<struct Object*>& code, int offset) {
  word_t offsets, opcodeArgCount, baseLength;
  for (size_t i = 0; i < code.size(); i += opcode_length(code, i)) {
    offsets = opcode_register_locations((word_t)code[i]);
    baseLength = opcode_base_length((word_t)code[i]);
    opcodeArgCount = opcode_arg_length(code, i);

    // first we offset the base arguments
    assert (offsets < 16);
    for (word_t offsetBit = 0; offsetBit < 6; offsetBit++) {
      word_t offsetMask = 1 << offsetBit;
      if ((offsets & offsetMask) == offsetMask) {
        optimizer_offset_value(code, i + 1 + offsetBit, offset);
      }
    }

    //and if it has extra arguments (a variable number) we get those here
    for (size_t offsetArg = i + 1 + baseLength; offsetArg < i + 1 + baseLength + opcodeArgCount; offsetArg++) {
      optimizer_offset_value(code, offsetArg, offset);
    }

  }

}


void optimizer_insert_code(std::vector<struct Object*>& code, size_t offset, std::vector<struct Object*>& newCode) {
  assert(offset >= 0 && offset <= code.size());
  //fix all relative jumps before inserting code if they jump over where our code is to be inserted


  //cases before the offset jumping past it
  for (size_t i = 0; i < offset; i += opcode_length(code, i)) {
    if (opcode_can_jump((word_t)code[i])) {
      word_t codeJumpParameter = opcode_jump_offset((word_t)code[i]);
      word_t jumpOffset = object_to_smallint(code[i + codeJumpParameter]);
      jumpOffset += opcode_jump_adjustment((word_t)code[i]);
      if (i + opcode_length(code, i) + jumpOffset >= offset) {
        optimizer_offset_value(code, i + codeJumpParameter, newCode.size());
      }
    }

  }

  //cases after the offset jumping before it
  for (size_t i = offset; i < code.size(); i += opcode_length(code, i)) {
    if (opcode_can_jump((word_t)code[i])) {
      word_t codeJumpParameter = opcode_jump_offset((word_t)code[i]);
      word_t jumpOffset = object_to_smallint(code[i + codeJumpParameter]);
      jumpOffset += opcode_jump_adjustment((word_t)code[i]);
      if (i + opcode_length(code, i) + jumpOffset < offset) {
        optimizer_offset_value(code, i + codeJumpParameter, 0 - newCode.size());
      }
    }

  }

  code.insert(code.begin() + offset, newCode.begin(), newCode.end());

}


void optimizer_delete_code(std::vector<struct Object*>& code, size_t offset, word_t amount) {
  assert(offset >= 0 && offset < code.size() && amount > 0);
  //fix all relative jumps before inserting code if they jump over where our code is to be inserted


  //cases before the offset jumping past it
  for (size_t i = 0; i < offset; i += opcode_length(code, i)) {
    if (opcode_can_jump((word_t)code[i])) {
      word_t codeJumpParameter = opcode_jump_offset((word_t)code[i]);
      word_t jumpOffset = object_to_smallint(code[i + codeJumpParameter]);
      jumpOffset += opcode_jump_adjustment((word_t)code[i]);
      if (i + opcode_length(code, i) + jumpOffset > offset) {
        optimizer_offset_value(code, i + codeJumpParameter, 0 - amount);
      }
    }

  }

  //cases after the offset jumping before it
  for (size_t i = offset; i < code.size(); i += opcode_length(code, i)) {
    if (opcode_can_jump((word_t)code[i])) {
      word_t codeJumpParameter = opcode_jump_offset((word_t)code[i]);
      word_t jumpOffset = object_to_smallint(code[i + codeJumpParameter]);
      jumpOffset += opcode_jump_adjustment((word_t)code[i]);
      if (i + opcode_length(code, i) + jumpOffset <= offset) {
        optimizer_offset_value(code, i + codeJumpParameter, amount);
      }
    }

  }
         

  code.erase(code.begin() + offset, code.begin() + offset + amount);
}


bool optimizer_picEntry_compare(struct Object** entryA, struct Object** entryB) {
  return (word_t)entryA[PIC_CALLEE_COUNT] > (word_t)entryB[PIC_CALLEE_COUNT];
}


bool optimizer_method_can_be_inlined(struct object_heap* oh, struct CompiledMethod* method) {
  if (method->heapAllocate == oh->cached.true_object) return false;
  if (method->restVariable == oh->cached.true_object) return false;
  struct Object* traitsWindow = method->base.map->delegates->elements[0];
  if (traitsWindow == oh->cached.primitive_method_window) return true;
  std::vector<struct Object*> code;
  optimizer_append_code_to_vector(method->code, code);
  for (size_t i = 0; i < (size_t)array_size(method->code); i += opcode_length(code, i)) {
    switch ((word_t)code[i]) {
    case OP_RESEND_MESSAGE:
    case OP_RESUME:
    case OP_RETURN_FROM:
    case OP_BRANCH_KEYED: /*i don't want to figure this one out now*/
    case OP_STORE_FREE_VARIABLE: /*maybe these should be possible*/
    case OP_LOAD_FREE_VARIABLE:
      
      return false;

    default: break;
    }
  }

  return code.size() < INLINER_MAX_INLINE_SIZE;
}

// looks at the pic cache to get a list of commonly called methods given a selector and putting the result in the out vector
void optimizer_commonly_called_implementations(struct object_heap* oh, struct CompiledMethod* method, struct Symbol* selector, std::vector<struct Object**>& out) {

  if ((struct Object*)method->calleeCount == oh->cached.nil) return;
  std::vector<struct Object**> picEntries; // we sort by most called for each selector
  for (word_t i = 0; i < array_size(method->calleeCount); i += CALLER_PIC_ENTRY_SIZE) {
    struct MethodDefinition* def = (struct MethodDefinition*)method->calleeCount->elements[i+PIC_CALLEE];
    if ((struct Object*)def == oh->cached.nil) continue;
    struct CompiledMethod* picMethod = (struct CompiledMethod*)def->method;
    struct Object* traitsWindow = picMethod->base.map->delegates->elements[0];
    assert (traitsWindow == oh->cached.compiled_method_window || traitsWindow == oh->cached.primitive_method_window);
    struct Symbol* picSelector = picMethod->selector;
    if (selector != picSelector) continue;
    struct Object** picEntry = &method->calleeCount->elements[i];
    picEntries.push_back(picEntry);
  }
  if (picEntries.empty()) return;
  std::sort(picEntries.begin(), picEntries.end(), optimizer_picEntry_compare);
  for (size_t i = 0; i < picEntries.size(); i++) {
    struct CompiledMethod* method = (struct CompiledMethod*) ((struct MethodDefinition*)(picEntries[i])[PIC_CALLEE])->method;
    if (optimizer_method_can_be_inlined(oh, method)) {
      out.push_back(picEntries[i]);
    }
  }
}

void optimizer_change_returns_to_jumps(struct object_heap* oh, std::vector<struct Object*>& code, word_t resultReg, word_t jumpOutDistance) {


  for (size_t i = 0; i < code.size(); i += opcode_length(code, i)) {
    if (OP_RETURN_REGISTER == (word_t)code[i]) {
      struct Object* reg = code[i+1];
      std::vector<struct Object*> codeToInsert;
      codeToInsert.push_back((struct Object*)OP_MOVE_REGISTER);
      codeToInsert.push_back(smallint_to_object(resultReg));
      codeToInsert.push_back(reg);
      codeToInsert.push_back((struct Object*)OP_JUMP_TO);
      codeToInsert.push_back(smallint_to_object(code.size() - i - 2 + jumpOutDistance + 1 /*jump_to jumps subtract one*/));
      optimizer_delete_code(code, i, 2);
      optimizer_insert_code(code, i, codeToInsert);
    } else if (OP_RETURN_VALUE == (word_t)code[i]) {
      struct Object* literal = code[i+1];
      std::vector<struct Object*> codeToInsert;
      codeToInsert.push_back((struct Object*)OP_LOAD_LITERAL);
      codeToInsert.push_back(smallint_to_object(resultReg));
      codeToInsert.push_back(literal);
      codeToInsert.push_back((struct Object*)OP_JUMP_TO);
      codeToInsert.push_back(smallint_to_object(code.size() - i - 2 + jumpOutDistance + 1));
      optimizer_delete_code(code, i, 2);
      optimizer_insert_code(code, i, codeToInsert);

    }
  }


}


void optimizer_remove_redundant_ops(struct object_heap* oh, struct CompiledMethod* method, std::vector<struct Object*>& code) {

  if (method->heapAllocate == oh->cached.true_object) return;

  for (size_t i = 0; i < code.size(); ) {
    if (OP_LOAD_VARIABLE == (word_t)code[i]) {
      optimizer_delete_code(code, i, 2);
    } else if (OP_STORE_VARIABLE == (word_t)code[i]) {
      optimizer_delete_code(code, i, 2);
    } else {
      i += opcode_length(code, i);
    }

  }
}

void optimizer_remove_internal_ops(struct object_heap* oh, struct CompiledMethod* method, std::vector<struct Object*>& code) {
  for (size_t i = 0; i < code.size(); i += opcode_length(code, i)) {
    if (OP_INTERNAL_SEND >= (word_t)code[i]) {
      switch ((word_t)code[i]) {
      case OP_INTERNAL_SEND: code[i] = (struct Object*) OP_SEND; break;
      default: break;
      }
    }
  }
}


//fixme: remove double jumps (a jump that points to another jump), remove no-op load/store variables
// the main inliner function
void optimizer_inline_callees(struct object_heap* oh, struct CompiledMethod* method) {
  std::vector<struct Object*> code;
  if (!optimizer_method_can_be_inlined(oh, method)) return;
  optimizer_append_code_to_vector(method->code, code);
  std::vector<struct Object**> commonCalledImplementations; //picEntries for each selector
  word_t newRegisterCount = object_to_smallint(method->registerCount);


  for (size_t i = 0; i < code.size(); i += opcode_length(code, i)) {

    if (OP_INLINE_METHOD_CHECK == (word_t)code[i]
        || OP_INLINE_PRIMITIVE_CHECK == (word_t)code[i]) {
      while (OP_INLINE_METHOD_CHECK == (word_t)code[i]
             || OP_INLINE_PRIMITIVE_CHECK == (word_t)code[i]) {
        //jump past the send because this one coming up has already been inlined
        word_t jumpAmount = object_to_smallint(code[i + opcode_jump_offset((word_t)code[i])] + opcode_jump_adjustment((word_t)code[i]));
        i += opcode_length(code, i);
        i += jumpAmount;
        
      }
      continue; // this jumps us past the send that was inlined
    }
    //only inline plain sends
    if (OP_SEND != (word_t)code[i]) {
      continue;
    }

    //mark it so we don't infinite loop
    code[i] = (struct Object*) OP_INTERNAL_SEND;

    word_t resultReg = object_to_smallint(code[i+1]);
    struct Symbol* selector = (struct Symbol*)code[i+2];
    word_t arity = object_to_smallint(code[i+3]);
    commonCalledImplementations.clear();
    //print_pic_entries(oh, method);
    optimizer_commonly_called_implementations(oh, method, selector, commonCalledImplementations);

    for (size_t k = 0; k < commonCalledImplementations.size(); k++) {
      if (k > 2) break;     //don't bother inlining if there are a lot of different implementations
      struct Object** picEntry = commonCalledImplementations[k];
      struct MethodDefinition* def = (struct MethodDefinition*)picEntry[PIC_CALLEE];
      struct Object* traitsWindow = def->method->map->delegates->elements[0];
      if (traitsWindow == oh->cached.compiled_method_window) {
        struct CompiledMethod* cmethod = (struct CompiledMethod*)def->method;
        //fixme
        std::vector<struct Object*> inlineOpcodeCode, codeToInsert, preludeMoveRegisters;
        word_t requiredRegisters = object_to_smallint(cmethod->registerCount);

        inlineOpcodeCode.push_back((struct Object*)OP_INLINE_METHOD_CHECK);
        inlineOpcodeCode.push_back(picEntry[PIC_CALLEE_MAPS]);
        inlineOpcodeCode.push_back(smallint_to_object(arity));
        //jump offset if match fails... this should be just past the end of the inserted code
        inlineOpcodeCode.push_back(smallint_to_object(opcode_length(code, i)));
        for (word_t g = 0; g < arity; g++) {
          inlineOpcodeCode.push_back(code[i + OP_SEND_PARAMETER_0 + g]);
          preludeMoveRegisters.push_back((struct Object*)OP_MOVE_REGISTER);
          preludeMoveRegisters.push_back(smallint_to_object(g + newRegisterCount));
          preludeMoveRegisters.push_back(code[i + OP_SEND_PARAMETER_0 + g]);
        }

        // use its non-inlined code if available because the type feedback characteristics may be different
        if ((struct Object*)cmethod->oldCode != oh->cached.nil) {
          optimizer_append_code_to_vector(cmethod->oldCode, codeToInsert);
        } else {
          optimizer_append_code_to_vector(cmethod->code, codeToInsert);
        }

        // we could be smart about minimizing this in the future... but this is the only safe way now
        //remap registers
        optimizer_offset_registers(codeToInsert, newRegisterCount);
        newRegisterCount += requiredRegisters;
        
        optimizer_remove_redundant_ops(oh, cmethod, codeToInsert);

        // put this in after we do register moving because it's already been done for this
        optimizer_insert_code(codeToInsert, 0, preludeMoveRegisters);

        //change returns to jumps
        //remap return registers
        optimizer_change_returns_to_jumps(oh, codeToInsert, resultReg, opcode_length(code, i));
        inlineOpcodeCode[3] = smallint_to_object(codeToInsert.size()); // this is the jump
        std::copy(codeToInsert.begin(), codeToInsert.end(), std::back_inserter(inlineOpcodeCode));
        optimizer_insert_code(code, i, inlineOpcodeCode);
        
        //this next would skip over inlining further
        //which might protect us from infinite loops
        //if you comment this out, the jumps at the end of inserted code will be wrong
        //fixme... it would be ideal to optimize long term based on our calleeCounts
        //rather than taking the callee's possibly inlined code as-is
        i += inlineOpcodeCode.size();

      } else if (traitsWindow == oh->cached.primitive_method_window) {
        struct PrimitiveMethod* pmethod = (struct PrimitiveMethod*)def->method;
        std::vector<struct Object*> codeToInsert;
        codeToInsert.push_back((struct Object*)OP_INLINE_PRIMITIVE_CHECK);
        codeToInsert.push_back(smallint_to_object(resultReg));
        codeToInsert.push_back(picEntry[PIC_CALLEE_MAPS]);
        codeToInsert.push_back(pmethod->index);
        codeToInsert.push_back(smallint_to_object(arity));
        //jump offset if primitive is executed... this gets updated as more code is inserted
        codeToInsert.push_back(smallint_to_object(opcode_length(code, i)));
        for (word_t g = 0; g < arity; g++) {
          codeToInsert.push_back(code[i + OP_SEND_PARAMETER_0 + g]);
        }
        optimizer_insert_code(code, i, codeToInsert);

        i += codeToInsert.size();
      } else {
        assert(0);
      }
    }

  }

  //fixme we need to figure out what references there are in the code so that we can un-inline and re-inline
  //when things change
  //but for now....
  //  printf("before remove:\n");
  //  print_code(oh, code);
  optimizer_remove_redundant_ops(oh, method, code);
  optimizer_remove_internal_ops(oh, method, code);

  //  printf("after remove:\n");
  //  print_code(oh, code);

  struct OopArray* newCode = heap_clone_oop_array_sized(oh, get_special(oh, SPECIAL_OOP_ARRAY_PROTO), code.size());
  for (size_t i = 0; i < code.size(); i++) {
    newCode->elements[i] = code[i];
  }
  method->code = newCode;
  method->registerCount = smallint_to_object(newRegisterCount);
  heap_store_into(oh, (struct Object*) method, (struct Object*) method->code);



}
