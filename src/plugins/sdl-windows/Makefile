slateroot=../../..

include $(slateroot)/common.mk

LIBS+=`sdl-config --libs`
CFLAGS+=`sdl-config --cflags`
BASENAME=slate-windows
OBJS=sdl-windows.lo

default: all

all: $(BASENAME)$(LIB_SO_EXT)

clean:
	$(SILENT) $(RM) -fr $(OBJS) .libs *.a *.o
	$(SILENT) $(shell cd $(libdir) && $(RM) $(notdir $(BASENAME)$(LIB_SO_EXT)*))
	$(SILENT) $(shell cd $(libdir) && $(RM) $(notdir $(BASENAME).*$(LIB_SO_EXT)))
	$(SILENT) $(shell cd $(libdir) && $(RM) $(notdir $(BASENAME).la) *.a)

%.o: %.c
	$(SECHO) "Compiling src/plugins/$(BASENAME)/$<  ($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=compile $(CC) $(CFLAGS) -o $@ -c $<

$(BASENAME)$(LIB_SO_EXT): $(BASENAME).la
	$(SECHO) "Installing $(BASENAME) Plug-in  ($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=install install $(BASENAME).la `pwd`/$(libdir) 2> $(DEVNULL)

$(BASENAME).la: $(OBJS)
	$(SECHO) "Linking src/plugins/$(BASENAME)/$<  ($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=link $(CC) $(LIBS) -module -o $@ $< -rpath `pwd` $(LDFLAGS)

%.lo: %.c
	$(SECHO) "Compiling src/plugins/$(BASENAME)/$<  ($(MODE))"
	$(SILENT) $(LIBTOOL) $(LIBTOOL_FLAGS) --mode=compile $(CC) $(CFLAGS) -o $@ -c $<
