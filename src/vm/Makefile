

slateroot=../..

include $(slateroot)/common.mk

all: vm

OBJECTS=debug.o external-files.o external-directory.o external-library.o external-memory.o gc.o interpreter.o method.o misc.o object.o primitives.o socket.o profiler.o optimizer.o vm.o

$(OBJECTS): slate.hpp

%.o: %.cpp
	$(SECHO) "Compiling src/vm/$(BASENAME)/$<	($(MODE))"
	$(CPP) $(CFLAGS) $(CFLAGS_$<) -o $@ -c $<

vm: $(OBJECTS)
	$(CPP) $(LIBS) $(CFLAGS) $(CFLAGS_$<) -o $(VMNAME) $(OBJECTS)

clean:
	rm -f *.o $(VMNAME)
