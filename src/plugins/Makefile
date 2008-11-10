
DLLEXT=.so
LIBS=time.so socket.so posix.so slate-cairo.so sdl-windows.so
NONPOSIX=pipe.so

LFLAGS_slate-cairo.o=-lcairo
LFLAGS_sdl-windows.o=-lSDL
#ARCH=-m32
ARCH=-m64
CFLAGS=-fPIC -I/usr/include/cairo -I/usr/include/SDL -std=c99 -Wall -pedantic -g $(ARCH)

all: $(LIBS)
	mkdir -p ../../lib
	cp $(LIBS) ../../lib/
	cd ../../lib && ln -s -f sdl-windows.so slate-windows.so

%$(DLLEXT): %.o
	gcc -shared $(CFLAGS) $(ARCH) $(LFLAGS_$<) -o $@ $<

clean:
	rm -f *.o *.so ../../lib/*.so



.PHONY: clean

