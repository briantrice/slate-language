

slateroot=../..

include $(slateroot)/common.mk

all: vm

OBJECTS=debug.o external-files.o external-library.o external-memory.o gc.o interpreter.o method.o misc.o object.o primitives.o socket.o vm.o

vm: $(OBJECTS)
	$(CC) $(LIBS) -o vm $(OBJECTS)

clean:
	rm -f *.o vm
