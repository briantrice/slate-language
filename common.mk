##
# Variables included by all Makefiles.
##

## Directory definitions

libdir       = $(slateroot)/lib
mobiusdir    = $(slateroot)/src/mobius
pluginsdir   = $(slateroot)/src/plugins
testdir      = $(slateroot)/tests

prefix      = /usr/local
datadir     = $(prefix)/share/slate
exec_prefix = $(prefix)/bin
execname    = slate
includedir  = $(prefix)/include
mandir      = $(prefix)/man
man1dir     = $(mandir)/man1
infodir     = $(prefix)/info
lispdir     = $(prefix)/share/emacs/site-lisp
DEVNULL     := /dev/null

## Build modes. Set on command line using QUIET=1, DEBUG=1 or PROFILE=1


ifdef QUIET
  VERBOSE := 0
  SILENT := @
  SILENT_ERRORS := 2> $(DEVNULL)
  LIBTOOL_FLAGS += --silent
endif

ifdef DEBUG
  MODE := debug
  COPTFLAGS += -g 
else
  MODE := optimized
#  COPTFLAGS += -O2
endif

ifdef PROFILE
  MODE += profiled
  COPTFLAGS += -pg -g
else
#  COPTFLAGS += -fomit-frame-pointer 
endif

## All required executables

CC          := gcc
CP          := cp -f
LIBTOOL     ?= libtool
ECHO        := echo
WGET        := wget -q --cache=off
SECHO       := @$(ECHO)
INSTALL     := install
TAR         := tar
GZIP        := gzip
ETAGS       := etags
EMACS       := emacs

## Platform independent definitions

INCLUDES    += 
CFLAGS      += -DSLATE_DATADIR=$(datadir)
CFLAGS      += $(COPTFLAGS) -std=c99 -Wall -pedantic -pthread $(PRINT_DEBUG)  $(INCLUDES)


## Default variables

PLATFORM    := unix
CCVERSION   := $(shell $(CC) -dumpversion)
WORD_SIZE   := 64
LDFLAGS     := # -avoid-version
LIBS        := #-lm -ldl
#PLUGINS     := platform.so posix.so pipe.so
PLUGINS     := sdl-windows.so slate-cairo.so
HOST_SYSTEM := $(shell uname -s)
LIB_SO_EXT  := .so
INSTALL_MODE := -m 644
CPU_TYPE    := `uname -m`
VM_LIBRARIES = -lm -ldl

CFLAGS_slate-cairo.c=`pkg-config --cflags cairo`
LDFLAGS_slate-cairo.lo=`pkg-config --libs cairo`
CFLAGS_sdl-windows.c=`pkg-config --cflags sdl`
LDFLAGS_sdl-windows.lo=`pkg-config --libs sdl`


PRINT_DEBUG_Y=  -DPRINT_DEBUG_STACK_POINTER  -DPRINT_DEBUG_STACK_PUSH -DPRINT_DEBUG_FOUND_ROLE  -DPRINT_DEBUG_FUNCALL   
PRINT_DEBUG_X=-DPRINT_DEBUG  -DPRINT_DEBUG_OPCODES -DPRINT_DEBUG_INSTRUCTION_COUNT -DPRINT_DEBUG_CODE_POINTER -DPRINT_DEBUG_DISPATCH 
PRINT_DEBUG_1=-DPRINT_DEBUG_DEFUN -DPRINT_DEBUG_GC_1
PRINT_DEBUG_4=-DPRINT_DEBUG_DEFUN -DPRINT_DEBUG_GC  -DPRINT_DEBUG_OPCODES  -DPRINT_DEBUG_CODE_POINTER  -DPRINT_DEBUG_ENSURE -DPRINT_DEBUG_STACK
PRINT_DEBUG_3=-DPRINT_DEBUG_DEFUN -DPRINT_DEBUG_GC -DPRINT_DEBUG_OPTIMIZER -DPRINT_DEBUG_PIC_HITS
PRINT_DEBUG_2=-DPRINT_DEBUG_DEFUN -DPRINT_DEBUG  -DPRINT_DEBUG_OPCODES -DPRINT_DEBUG_INSTRUCTION_COUNT -DPRINT_DEBUG_CODE_POINTER -DPRINT_DEBUG_DISPATCH  -DPRINT_DEBUG_GC_MARKINGS
PRINT_DEBUG=$(PRINT_DEBUG_1)


## Determine CPU type

#ifeq ($(HOST_SYSTEM), Darwin)
#  CPU_TYPE := powerpc
#endif

## TODO: Sparc detection for SunOS?
## TODO: Base CPU type on real information, not just generic OS variant

## CPU-type specific overrides. Any of the variables above may be changed here.

ifdef ARCH
  CFLAGS += -m$(ARCH)
endif


#ifeq ($(CPU_TYPE), i686)
#  CFLAGS += -m$(WORD_SIZE)
#endif

#ifeq ($(CPU_TYPE), x86_64)
#  CFLAGS += -m$(WORD_SIZE)
#endif

## Platform specific overrides. Any of the variables above may be changed here.

ifeq ($(HOST_SYSTEM), AIX)
  LIB_SO_EXT  := .a
  PLUGINS     := platform.so posix.so pipe.so
endif

ifeq ($(HOST_SYSTEM), BeOS)
  PLATFORM  := beos
endif

ifeq ($(findstring CYGWIN,$(HOST_SYSTEM)), CYGWIN)
  PLATFORM   := windows
  LDFLAGS    += -no-undefined
  LIBS       := -lm
  LIB_SO_EXT := .dll
# DEVNULL    := NUL # Required if using cmd.exe and not bash.
endif

ifeq ($(HOST_SYSTEM), Darwin)
#  LIBTOOL := MACOSX_DEPLOYMENT_TARGET=10.3 glibtool
  LIBTOOL := glibtool
  PLUGINS := $(subst sdl-windows,quartz-windows, $(PLUGINS))
endif

ifeq ($(HOST_SYSTEM), DragonFly)
  LIBS      := -lm
endif

ifeq ($(HOST_SYSTEM), FreeBSD)
  LIBS      := -lm -lc_r
  LIBTOOL  := libtool13
endif

ifeq ($(HOST_SYSTEM), HP-UX)
  LIBS       := -lm -ldld
  LIB_SO_EXT := .sl
  PLUGINS    := platform.so posix.so pipe.so
endif

ifeq ($(HOST_SYSTEM), Linux)
  LIBS       := -lm -ldl
endif

ifeq ($(findstring MINGW,$(HOST_SYSTEM)), MINGW)
  PLATFORM  := windows
  LIBS      := -lm
endif

ifeq ($(HOST_SYSTEM), NetBSD)
  LIBS      := -lm
endif

ifeq ($(HOST_SYSTEM), SunOS)
  PLUGINS   := platform.so posix.so pipe.so
  # Work around GCC Ultra SPARC alignment bug
  # TODO: Should be CPU specific.
  COPTFLAGS := $(subst -O2,-O0, $(COPTFLAGS))
endif

PLUGINS := $(subst .so,$(LIB_SO_EXT), $(PLUGINS))

