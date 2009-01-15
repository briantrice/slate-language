

slateroot=.

include $(slateroot)/common.mk

all: vm

vm:
	make -C src/vm vm && cp src/vm/vm ./slate

backup: superclean
	cd .. && tar  '--exclude=*.git*' -jcvf cslatevm-backup.tar.bz2 cslatevm

plugins:
	make -C src/plugins

superclean: clean
	rm -f core vm vm.fast vm.prof

pluginsclean:
	make -C src/plugins clean
vmclean:
	make -C src/vm clean

clean: pluginsclean vmclean


.PHONY: clean superclean backup plugins pluginsclean vmclean
