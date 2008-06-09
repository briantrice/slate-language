CFLAGS=-fPIC
DLLEXT=.so
LIBS=libtime.so


all: $(LIBS)

lib%$(DLLEXT): %.o
	gcc -shared $(CFLAGS) -o $@ $<


clean:
	rm -f *.o *.so



.PHONY: clean

