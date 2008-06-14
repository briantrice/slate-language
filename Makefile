
CC=gcc
LD=ld
INCLUDES=
PRINT_DEBUG_Y=  -DPRINT_DEBUG_STACK_POINTER  -DPRINT_DEBUG_STACK_PUSH -DPRINT_DEBUG_FOUND_ROLE  -DPRINT_DEBUG_FUNCALL   
PRINT_DEBUG_X=-DPRINT_DEBUG  -DPRINT_DEBUG_OPCODES -DPRINT_DEBUG_INSTRUCTION_COUNT -DPRINT_DEBUG_CODE_POINTER -DPRINT_DEBUG_DISPATCH 
PRINT_DEBUG_1=-DPRINT_DEBUG_DEFUN -DPRINT_DEBUG_GC
PRINT_DEBUG_2=-DPRINT_DEBUG_DEFUN -DPRINT_DEBUG  -DPRINT_DEBUG_OPCODES -DPRINT_DEBUG_INSTRUCTION_COUNT -DPRINT_DEBUG_CODE_POINTER -DPRINT_DEBUG_DISPATCH  -DPRINT_DEBUG_GC_MARKINGS
PRINT_DEBUG=$(PRINT_DEBUG_1)
LIBRARIES_=-lefence -lm -ldl
LIBRARIES=-lm -ldl
#LIBRARIES=
CFLAGS=-std=c99 -Wall -pedantic-errors -pthread $(PRINT_DEBUG)  $(INCLUDES)
DEBUGFLAGS= -g  
OBJECTS=

all: vm

backup: superclean
	cd .. && tar  '--exclude=*.git*' -jcvf cslatevm-backup.tar.bz2 cslatevm

plugins:
	make -C src/plugins

vm: vm.c $(OBJECTS)
	gcc $(DEBUGFLAGS) $(CFLAGS) $(LIBRARIES) -o vm vm.c $(OBJECTS)

#fix needs pg on every arg
vm.prof: vm.c $(OBJECTS)
	gcc $(CFLAGS) $(LIBRARIES) -pg -o vm.prof vm.c $(OBJECTS)


#fix cflags
vm.fast: vm.c $(OBJECTS)
	gcc $(CFLAGS) $(LIBRARIES) -O2 -o vm.fast vm.c $(OBJECTS)


superclean: clean
	rm -f core vm vm.fast vm.prof

pluginsclean:
	make -C src/plugins clean
clean: pluginsclean
	rm -f *.o


.PHONY: clean superclean backup plugins pluginsclean
