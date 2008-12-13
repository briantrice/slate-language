

slateroot=.

include $(slateroot)/common.mk

all: vm

backup: superclean
	cd .. && tar  '--exclude=*.git*' -jcvf cslatevm-backup.tar.bz2 cslatevm

plugins:
	make -C src/plugins

vm.debug: vm.c $(OBJECTS)
	$(CC) $(DEBUGFLAGS) $(CFLAGS) $(LIBS) -g -o vm.debug vm.c $(OBJECTS)

#fix needs pg on every arg
vm.prof: vm.c $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBS) -O2 -pg -o vm.prof vm.c $(OBJECTS)


#fix cflags
vm: vm.c $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBS) -O2 -o vm vm.c $(OBJECTS)


superclean: clean
	rm -f core vm vm.fast vm.prof

pluginsclean:
	make -C src/plugins clean

clean: pluginsclean
	rm -f *.o


.PHONY: clean superclean backup plugins pluginsclean
