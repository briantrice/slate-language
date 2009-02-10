#ifdef WIN32
#  define EXPORT __declspec(dllexport)
#else
#  define EXPORT
#endif

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitReader.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include "llvm-c/Analysis.h"
#include "llvm-c/ExecutionEngine.h"

#include <stdio.h>
#include <stdlib.h>

EXPORT int wrapper_LLVMVerifyFunction(LLVMValueRef Fn) {
  	LLVMVerifyFunction(Fn, LLVMReturnStatusAction) == 0;
}

EXPORT LLVMExecutionEngineRef wrapper_LLVMCreateExecutionEngine(LLVMModuleProviderRef MP) {
	LLVMExecutionEngineRef Interp;
	char *Error;
	if( LLVMCreateExecutionEngine(&Interp, MP, &Error) ) {
		printf("LLVM: %s\n", Error);
		exit(-1);
	}
	return Interp;
}

