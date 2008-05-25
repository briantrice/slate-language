##
# Shared plug-in Makefile rules.
##

default: all

all: $(OBJS) $(BASENAME)$(LIB_SO_EXT)

clean:
	$(SILENT) $(RM) -fr $(OBJS) .libs *.a *.o
	$(SILENT) $(shell cd $(libdir) && $(RM) $(notdir $(BASENAME)$(LIB_SO_EXT)*))
	$(SILENT) $(shell cd $(libdir) && $(RM) $(notdir $(BASENAME).*$(LIB_SO_EXT)))
	$(SILENT) $(shell cd $(libdir) && $(RM) $(notdir $(BASENAME).la) *.a)

%.o: %.c
	$(SECHO) "Compiling src/plugins/$(BASENAME)/$<  ($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=compile $(CC) $(CFLAGS) -o $@ -c $<

%$(LIB_SO_EXT): %.la
	$(SECHO) "Installing $(BASENAME) Plug-in  ($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=install install $< `pwd`/$(libdir) $(SILENT_ERRORS)

%.la: %.lo
	$(SECHO) "Linking src/plugins/$(BASENAME)/$<  ($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=link $(CC) $(LIBS) -module -o $@ $< -rpath `pwd` $(LDFLAGS)

%.lo: %.c
	$(SECHO) "Compiling src/plugins/$(BASENAME)/$<  ($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=compile $(CC) $(CFLAGS) -o $@ -c $<
