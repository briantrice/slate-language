

slateroot=.

include $(slateroot)/common.mk

all: vm

vm:
	$(MAKE) -C $(VMDIR) vm && cp $(VM) ./$(execname)

$(DEFAULT_IMAGE): vm
	$(SILENT) $(ECHO) "repl resetOnStartup. Image saveNamed: '$(DEFAULT_IMAGE)'." | $(VM) -q -i $(DEFAULT_KERNEL_IMAGE)
	$(SILENT) touch $@

install: vm
	$(SILENT) $(INSTALL) $(VM) $(exec_prefix)/$(execname)
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/$(DEFAULT_IMAGE) $(datadir)/$(DEFAULT_IMAGE)

backup: superclean
	cd .. && tar  '--exclude=*.git*' -jcvf cslatevm-backup.tar.bz2 cslatevm

plugins:
	$(MAKE) -C src/plugins

superclean: clean
	rm -f core vm vm.fast vm.prof

pluginsclean:
	$(MAKE) -C src/plugins clean
vmclean:
	$(MAKE) -C src/vm clean

clean: pluginsclean vmclean


.PHONY: clean superclean backup plugins pluginsclean vmclean
