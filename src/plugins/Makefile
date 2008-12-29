


slateroot=../..

include $(slateroot)/common.mk


all: $(PLUGINS)
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=finish $(libdir)

clean:
	$(SILENT) rm -f $(libdir)/* *.a *.o .libs/*


%.o: %.c
	$(SECHO) "Compiling src/plugins/$(BASENAME)/$<	($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=compile $(CC) $(CFLAGS) $(CFLAGS_$<) -o $@ -c $<

%.o: %.m
	$(SECHO) "Compiling src/plugins/$(BASENAME)/$<	($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=compile $(CC) $(CFLAGS) $(CFLAGS_$<) -o $@ -c $<

%$(LIB_SO_EXT): %.la
	$(SECHO) "Installing $(BASENAME) Plug-in  ($(MODE))"
	mkdir -p `pwd`/$(libdir)
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=install install $< `pwd`/$(libdir) $(SILENT_ERRORS)

%.la: %.lo
	$(SECHO) "Linking src/plugins/$(BASENAME)/$<  ($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=link $(CC) $(LIBS) $(LDFLAGS) $(LDFLAGS_$<) -module -o $@ $< -rpath `pwd` $(LDFLAGS)

%.lo: %.c
	$(SECHO) "Compiling src/plugins/$(BASENAME)/$<	($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=compile $(CC) $(CFLAGS) $(CFLAGS_$<) -o $@ -c $<

%.lo: %.m
	$(SECHO) "Compiling src/plugins/$(BASENAME)/$<	($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=compile $(CC) $(CFLAGS) $(CFLAGS_$<) -o $@ -c $<

#%.o: %.c
#	$(LIBTOOL) $(LIBTOOL_FLAGS) --mode=compile $(CC) $(CFLAGS) $(CFLAGS_$<) -fPIC -c -o $@ $<
#	$(CC) $(CFLAGS) $(CFLAGS_$<) -c -o $@ $<

#%$(LIB_SO_EXT): %.o
#	$(LIBTOOL) $(LIBTOOL_FLAGS) --mode=link $(CC) $(CFLAGS) -shared -fPIC $(LDFLAGS) $(LDFLAGS_$<) -o $@ $<
#
#clean:
#	$(LIBTOOL) $(LIBTOOL_FLAGS) --mode=clean rm -f *.o *.so ../../lib/*.$(LIB_SO_EXT)



.PHONY: clean

