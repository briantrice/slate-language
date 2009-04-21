

slateroot=.

include $(slateroot)/common.mk

all: vm

vm:
	$(MAKE) -C $(VMDIR) vm && cp $(VM) ./$(execname)

$(DEFAULT_IMAGE): vm
	$(SILENT) $(ECHO) "repl resetOnStartup. Image saveNamed: '$(DEFAULT_IMAGE)'." | $(VM) -q -i $(DEFAULT_KERNEL_IMAGE)
	$(SILENT) touch $@

installdirs:
	$(SILENT) $(INSTALL) -d $(exec_prefix) $(lispdir) $(includedir) $(datadir) $(man1dir)

install: vm installdirs
	$(SECHO) "Installing" # TODO: Plugins and documentation missing
	$(SILENT) $(INSTALL) $(VM) $(exec_prefix)/$(execname)
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/$(DEFAULT_IMAGE) $(datadir)/$(DEFAULT_IMAGE)
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(VMDIR)/slate.h $(includedir)/slate.h
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/etc/slate-mode.el $(lispdir)/
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/etc/slate.1 $(man1dir)/

install-strip: install
	$(SILENT) $(INSTALL) -s $(VM) $(exec_prefix)/$(execname)

installcheck: install
	$(SECHO) "Checking installation"
        # TODO: A few sanity checks should be run on the installed files
	$(SILENT) $(ECHO) "3 + 4." | $(exec_prefix)/$(execname)

uninstall:
	$(SECHO) "Uninstalling"
	$(SILENT) $(RM) -f $(exec_prefix)/$(execname)
	$(SILENT) $(RM) -f $(lispdir)/slate-mode.el
	$(SILENT) $(RM) -f $(includedir)/slate.h
	$(SILENT) $(RM) -fr $(datadir)

edit:
	$(SECHO) "Launching Slate in Emacs"
	$(SILENT) $(EMACS) -Q -l $(slateroot)/etc/slate-startup.el

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
