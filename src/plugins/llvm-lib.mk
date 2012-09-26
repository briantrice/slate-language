#Build llvm plugin

PACKAGES = Analysis Core BitWriter

lib/llvm-lib.so:
	g++ -shared -o lib/llvm-lib.so -Wl,-whole-archive `llvm-config --libfiles $(PACKAGES)` -Wl,-no-whole-archive

clean:
	rm lib/llvm-lib.so
