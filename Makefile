
CC=gcc
LD=ld
INCLUDES=
PRINT_DEBUG_Y=  -DPRINT_DEBUG_STACK_POINTER  -DPRINT_DEBUG_STACK_PUSH -DPRINT_DEBUG_FOUND_ROLE  -DPRINT_DEBUG_FUNCALL   
PRINT_DEBUG_X=-DPRINT_DEBUG  -DPRINT_DEBUG_OPCODES -DPRINT_DEBUG_INSTRUCTION_COUNT -DPRINT_DEBUG_CODE_POINTER -DPRINT_DEBUG_DISPATCH 
PRINT_DEBUG_1=-DPRINT_DEBUG_DEFUN
PRINT_DEBUG_2=-DPRINT_DEBUG_DEFUN -DPRINT_DEBUG  -DPRINT_DEBUG_OPCODES -DPRINT_DEBUG_INSTRUCTION_COUNT -DPRINT_DEBUG_CODE_POINTER -DPRINT_DEBUG_DISPATCH 
PRINT_DEBUG=$(PRINT_DEBUG_1)
LIBRARIES=-lefence -lm
#LIBRARIES=
CFLAGS=-std=c99 -Wall -pedantic-errors -g  $(PRINT_DEBUG) $(INCLUDES)
OBJECTS=

all: vm

backup: superclean
	cd .. && tar  '--exclude=*.git*' -zcvf cslatevm-backup.tar.gz cslatevm

vm: vm.c $(OBJECTS)
	gcc $(CFLAGS) $(LIBRARIES) -o vm vm.c $(OBJECTS)

#fix needs pg on every arg
vm.prof: vm.c $(OBJECTS)
	gcc -std=c99 -Wall -lm -pedantic-errors -pg -o vm.prof vm.c


#fix cflags
vm.fast: vm.c $(OBJECTS)
	gcc -std=c99 -Wall -lm -pedantic-errors -DPRINT_DEBUG_DEFUN -O3 -o vm.fast vm.c

superclean: clean
	rm -f core vm vm.fast vm.prof

clean:
	rm -f *.o


.PHONY: clean superclean backup
