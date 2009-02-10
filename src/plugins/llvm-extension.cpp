#include <llvm-c/Core.h>
#include <llvm/Bitcode/BitstreamWriter.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Module.h>
using namespace llvm;

extern "C" {
	int LLVMModuleGetBitcode(LLVMModuleRef M, unsigned char *c_buffer);
	void LLVMGetBitcodeBuffer( char *c_buffer );
};

std::vector<unsigned char> BitcodeBuffer;

int LLVMModuleCopyBitcodeToBuffer(LLVMModuleRef M) {
	BitcodeBuffer.clear();
	BitstreamWriter Stream(BitcodeBuffer);
	WriteBitcodeToStream(unwrap(M), Stream);
	return BitcodeBuffer.size();
}

/* buffer should be big enough to hold the bitcode */
void LLVMGetBitcodeBuffer( char *c_buffer ) {
	using namespace std;
	memcpy( c_buffer, &BitcodeBuffer[0], BitcodeBuffer.size() ); 
}

