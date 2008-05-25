slateroot=../../..

include $(slateroot)/common.mk

CFLAGS+=$(shell pkg-config --cflags cairo)
CFLAGS+=-DHAVE_PTHREAD_H
CFLAGS+=-DHAVE_INTTYPES_H

LDFLAGS+=$(shell pkg-config --libs cairo)
BASENAME=slate-cairo
OBJS=slate-cairo.lo

include $(pluginsdir)/common.mk
