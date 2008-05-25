slateroot=../../..

include $(slateroot)/common.mk

LIBS+=`sdl-config --libs` -lSDL_gfx
CFLAGS+=`sdl-config --cflags`
BASENAME=sdl-ui
OBJS=$(BASENAME).lo

include $(pluginsdir)/common.mk
