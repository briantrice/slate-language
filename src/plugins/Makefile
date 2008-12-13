
DLLEXT=.so
LIBS=slate-cairo.so sdl-windows.so
NONPOSIX=pipe.so posix.so

#LFLAGS_slate-cairo.o=-lcairo
CFLAGS_slate-cairo.o=`pkg-config --cflags cairo`
LFLAGS_slate-cairo.o=`pkg-config --libs cairo`
#LFLAGS_sdl-windows.o=-lSDL
CFLAGS_sdl-windows.o=`pkg-config --cflags sdl`
LFLAGS_sdl-windows.o=`pkg-config --libs sdl`
#ARCH=-m32
ARCH=-m64
CFLAGS=-fPIC -I/usr/include/cairo -I/usr/include/SDL -std=c99 -Wall -pedantic -c -g $(ARCH)

all: $(LIBS)
	mkdir -p ../../lib
	cp $(LIBS) ../../lib/
	cd ../../lib && ln -s -f sdl-windows.so slate-windows.so

%.o: %.c
	gcc $(CFLAGS) $(CFLAGS_$<) -o $@ $<

%$(DLLEXT): %.o
	gcc -shared -fPIC $(ARCH) $(LFLAGS_$<) -o $@ $<

clean:
	rm -f *.o *.so ../../lib/*.so



.PHONY: clean

