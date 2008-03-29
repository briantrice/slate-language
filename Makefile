
CC=gcc
LD=ld
INCLUDES=
PRINT_DEBUG_OFF=  -DPRINT_DEBUG_STACK_POINTER  -DPRINT_DEBUG_STACK_PUSH  -DPRINT_DEBUG_FUNCALL
PRINT_DEBUG=-DPRINT_DEBUG -DPRINT_DEBUG_DISPATCH -DPRINT_DEBUG_OPCODES
#LIBRARIES=-lefence
LIBRARIES=
CFLAGS=-std=c99 -Wall -pedantic-errors -g $(LIBRARIES) $(PRINT_DEBUG) $(INCLUDES)
OBJECTS=

all: vm

backup: superclean
	cd .. && tar  '--exclude=*.git*' -zcvf cslatevm-backup.tar.gz cslatevm

vm: vm.c $(OBJECTS)
	gcc $(CFLAGS) -o vm vm.c $(OBJECTS)

superclean: clean
	rm -f core vm

clean:
	rm -f *.o


.PHONY: clean superclean backup
