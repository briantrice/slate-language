#include <llvm/Bitcode/BitstreamWriter.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Module.h>
using namespace llvm;

/// WriteBitcodeToBuffer - Write the specified module to the specified output
/// Buffer.
void WriteBitcodeToBuffer(const Module *M, std::vector<unsigned char> &Buffer) {
	BitstreamWriter Stream(Buffer);
	
	Buffer.reserve(256*1024);
  
	// Emit the module.
	WriteBitcodeToFile(M, Stream);
}
