

slateroot=.

include $(slateroot)/common.mk

all: vm

vm:
	$(SILENT) $(MAKE) -C $(VMDIR) vm
	$(SILENT) cp $(VMDIR)/$(VMNAME) ./slate

$(DEFAULT_IMAGE): vm
	$(SILENT) $(ECHO) "repl resetOnStartup. Image saveNamed: '$(DEFAULT_IMAGE)'." | $(VM) $(QUIETNESS) -i $(DEFAULT_KERNEL_IMAGE)
	$(SILENT) touch $(DEFAULT_IMAGE)

release_image: vm $(DEFAULT_IMAGE)

installdirs:
	$(SILENT) $(INSTALL) -d $(exec_prefix) $(lispdir) $(includedir) $(datadir) $(man1dir)

install: vm installdirs
	$(info Installing) # TODO: Plugins and documentation missing
	$(SILENT) $(INSTALL) $(VM) $(exec_prefix)/$(execname)
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/$(DEFAULT_IMAGE) $(datadir)/$(DEFAULT_IMAGE)
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(VMDIR)/slate.h $(includedir)/slate.h
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/etc/slate-mode.el $(lispdir)/
	$(SILENT) cat $(slateroot)/etc/slate.1 | sed -e 's/$${prefix}/$(subst /,\/,$(prefix))/g' | $(GZIP) -c > $(slateroot)/etc/slate.1.gz
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/etc/slate.1.gz $(man1dir)

install-strip: install
	$(SILENT) $(INSTALL) -s $(VM) $(exec_prefix)/$(execname)

installcheck: install
	$(info Checking installation)
        # TODO: A few sanity checks should be run on the installed files
	$(SILENT) $(ECHO) "3 + 4." | $(exec_prefix)/$(execname)

uninstall:
	$(info Uninstalling)
	$(SILENT) $(RM) -f $(exec_prefix)/$(execname)
	$(SILENT) $(RM) -f $(lispdir)/slate-mode.el
	$(SILENT) $(RM) -f $(includedir)/slate.h
	$(SILENT) $(RM) -fr $(datadir)

edit:
	$(info Launching Slate in Emacs)
	$(SILENT) $(EMACS) -Q -l $(slateroot)/etc/slate-startup.el

bootstrap:
	$(info Bootstrapping new images)
	$(SILENT) $(ECHO) "load: 'src/mobius/init.slate'. Image littleEndian: True bitSize: $(WORD_SIZE)" | $(VM) $(QUIETNESS) -i $(DEFAULT_IMAGE)

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

tags: TAGS

TAGS: $(SOURCES) $(HEADERS)
	$(SECHO) "Creating $@ file"
	$(SILENT) $(ETAGS) $(SOURCES) $(HEADERS)

.PHONY: clean superclean backup plugins pluginsclean vmclean
