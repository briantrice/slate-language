slateroot=.

include $(slateroot)/common.mk

all: vm

vm:
	$(SILENT) $(MAKE) -C $(VMDIR) vm
	$(SILENT) cp -f $(VMDIR)/$(VMNAME) ./slate

slate.%.image: kernel.new.%.image
	$(SILENT) $(ECHO) "repl resetOnStartup. Load DefaultSourceDir := Directory newNamed: '$(slatesrcdir)'. Image saveNamed: '$@'." | $(VM) $(QUIETNESS) -i $<
	$(SILENT) touch $@

$(DEFAULT_IMAGE): vm
	$(SILENT) $(ECHO) "repl resetOnStartup. Load DefaultSourceDir := Directory newNamed: '$(slatesrcdir)'. Image saveNamed: '$(DEFAULT_IMAGE)'." | $(VM) $(QUIETNESS) -i $(DEFAULT_KERNEL_IMAGE)
	$(SILENT) touch $(DEFAULT_IMAGE)

release_image: vm $(DEFAULT_IMAGE)

latest_release_image:
	$(SILENT) $(WGET) $(LATEST_SLATE_IMAGE_URL)
	-bunzip2 -q -k $(LATEST_SLATE_IMAGE_RELEASE_NAME).bz2
	$(INSTALL) $(LATEST_SLATE_IMAGE_RELEASE_NAME) $(DEFAULT_IMAGE)

installdirs:
	$(SILENT) $(INSTALL) -d $(exec_prefix) $(lispdir) $(includedir) $(datadir) $(man1dir)
	$(SILENT) $(INSTALL) -d $(slatesrcdir) $(slatesrcdir)/core $(slatesrcdir)/lib $(slatesrcdir)/syntax $(slatesrcdir)/mobius $(slatesrcdir)/demo

install: vm installdirs
	$(info Installing) # TODO: Plugins and documentation missing
	$(SILENT) $(INSTALL) $(VM) $(exec_prefix)/$(execname)
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/$(DEFAULT_IMAGE) $(datadir)/$(DEFAULT_IMAGE)
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(VMDIR)/slate.hpp $(includedir)/slate.hpp
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/etc/slate-mode.el $(lispdir)/
	$(SILENT) mkdir -p $(vimdir)
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/etc/slate.vim $(vimdir)/
	$(SILENT) cat $(slateroot)/etc/slate.1 | sed -e 's/$${prefix}/$(subst /,\/,$(prefix))/g' | $(GZIP) -c > $(slateroot)/etc/slate.1.gz
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/etc/slate.1.gz $(man1dir)
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/src/core/*.slate $(slatesrcdir)/core
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/src/lib/*.slate $(slatesrcdir)/lib
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/src/syntax/*.slate $(slatesrcdir)/syntax
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/src/mobius/*.slate $(slatesrcdir)/mobius
	$(SILENT) $(INSTALL) $(INSTALL_MODE) $(slateroot)/src/demo/*.slate $(slatesrcdir)/demo

install-strip: install
	$(SILENT) $(INSTALL) -s $(VM) $(exec_prefix)/$(execname)

installcheck:
	$(info Checking installation)
        # TODO: A few sanity checks should be run on the installed files
	$(SILENT) $(exec_prefix)/$(execname) --eval "3 + 4. quit."

uninstall:
	$(info Uninstalling)
	$(SILENT) $(RM) -f $(exec_prefix)/$(execname)
	$(SILENT) $(RM) -f $(lispdir)/slate-mode.el
	$(SILENT) $(RM) -f $(includedir)/slate.hpp
	$(SILENT) $(RM) -fr $(datadir)

edit:
	$(info Launching Slate in Emacs)
	$(SILENT) $(EMACS) -Q -l $(slateroot)/etc/slate-startup.el $(src)

slate-completions:
	$(info Creating completion file for evaluation)
	$(SILENT) $(VM) $(QUIETNESS) -i $(DEFAULT_IMAGE) --eval "(File newNamed: '~/.slate_completions' &mode: File CreateWrite) writer sessionDo: [| :x | Symbols keySet do: [| :name | x ; name ; '\n']]. quit."

readline-support: slate-completions

i18n-support:
	$(SILENT) $(VM) $(QUIETNESS) -i $(DEFAULT_IMAGE) --load src/i18n/init.slate --eval "Unicode buildTable" --eval "Image save. quit."

bootstrap: src/mobius/init.slate
	$(info Bootstrapping new images)
	$(SILENT) $(VM) $(QUIETNESS) -i $(DEFAULT_IMAGE) --load src/mobius/init.slate --eval "Bootstrap ImageDefinition bootstrap &bootstrapDirectory: '$(BOOTSTRAP_DIR)' &littleEndian: $(LITTLE_ENDIAN_SLATE) &wordSize: $(WORD_SIZE)." --eval "quit"

backup: superclean
	tar  '--exclude=*.git*' -jcvf ../slate-backup.tar.bz2 .

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
