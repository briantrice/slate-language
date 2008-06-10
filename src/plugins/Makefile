CFLAGS=-fPIC
DLLEXT=.so
LIBS=time.so socket.so pipe.so posix.so


all: $(LIBS)
	mkdir -p ../../lib && cp $(LIBS) ../../lib/

%$(DLLEXT): %.o
	gcc -shared $(CFLAGS) -o $@ $<


clean:
	rm -f *.o *.so ../../lib/*.so



.PHONY: clean

