

slateroot=../..

include $(slateroot)/common.mk

all: vm

OBJECTS=debug.o external-files.o external-library.o external-memory.o gc.o interpreter.o method.o misc.o object.o primitives.o socket.o profiler.o vm.o

%.o: %.c
	$(SECHO) "Compiling src/vm/$(BASENAME)/$<	($(MODE))"
	$(CC) $(CFLAGS) $(CFLAGS_$<) -o $@ -c $<

vm: $(OBJECTS)
	$(CC) $(LIBS) $(CFLAGS) $(CFLAGS_$<) -o $(VMNAME) $(OBJECTS)

clean:
	rm -f *.o $(VMNAME)
