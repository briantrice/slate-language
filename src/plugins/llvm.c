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

EXPORT LLVMModuleRef llvm_module_create_with_name(char *moduleID) {
	return LLVMModuleCreateWithName(moduleID);
}

EXPORT void llvm_dispose_module(LLVMModuleRef module) {
	LLVMDisposeModule(module);
}

EXPORT void llvm_dump_module(LLVMModuleRef module) {
	LLVMDumpModule(module);
}
