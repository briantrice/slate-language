Information for the Slate Distribution
--------------------------------------

Copyright/License Information
-----------------------------

See the LICENSE file.

What is Slate?
--------------

Slate is a prototype-based, multi-dispatch object-oriented language that
runs from a highly-customizable live environment. The implementation is
highly portable and relatively lightweight.

Where do I learn more?
----------------------

The reference site for this Distribution is at <http://www.slatelanguage.org>
and the Google Code project at: <http://code.google.com/p/slate-language/>

Bug reports and requests for information can be made via the Slate mailing
list, described at: <http://groups.google.com/group/slate-language>

See the wiki for more detailed documentation:
 <http://code.google.com/p/slate-language/w/list>

See our bug-tracker for issues:
 <http://code.google.com/p/slate-language/issues/list>

Obtaining Slate
---------------

To get a slate repository on your computer to play with, run:
    git clone git://repo.or.cz/cslatevm.git

Setup
-----

'make' builds the VM.
'make edit' launches the VM and standard image in Emacs with a scratch area.
'(sudo) make install' installs the VM and images in global directories (/usr/local/ by default) so that the VM can be invoked as just "slate".
'make plugins' builds the plugins that you need for the GUI and FFI-related stuff.

Read common.mk for more options.

Command Line
------------

    ./slate -i <image>

This starts slate using <image>.

Run `slate -h` for more details.

Learning Slate
--------------

Read the online tutorials (from newest to oldest):

* <http://slatelanguage.org/tutorial/>
* <http://code.google.com/p/slate-language/wiki/GettingStarted>

Bootstrapping
-------------

If you make changes to core slate files (like adding a new field to
CompiledMethods), sometimes the only easy way to implement those changes
throughout the system is to rebuild Slate completely. Here is how you
generate a new kernel image:

From the shell:

    make bootstrap WORD_SIZE=64 && make slate.image WORD_SIZE=64
or
    make bootstrap WORD_SIZE=32 && make slate.image WORD_SIZE=32

From within Slate:

At the Slate REPL, execute:
    load: 'src/mobius/init.slate'.
then:
    Image bootstrap &littleEndian: True &bitSize: 32.
or
    Image bootstrap &littleEndian: True &bitSize: 64.

Then you will load the resulting kernel image like a regular image:

    ./slate -i kernel.new.<endian>.<wordsize>.<timestamp>.image

After the image is loaded, you will want to save it so you
don't have to go through loading the kernel again:

    Image saveNamed: 'slate.image'.

The same steps can also be accomplished with:
    make slate.image

Debugging the VM
----------------

    make vmclean && make DEBUG=1
    gdb slate
    r -i <image-file>
    (on crash or Ctrl-c)
    bt
    f <n> (change frame to one with an 'oh' object (struct object_heap*))

See the slate backtrace -> `print print_backtrace(oh)`
Inspect an object       -> `print print_detail(oh, struct Object*)`
See the stack           -> `print print_stack_types(oh, 200)`


Build flags (e.g.  make vmclean && make DEBUG=1
EXTRACFLAGS="-DGC_BUG_CHECK -DSLATE_DISABLE_METHOD_OPTIMIZATION
-DSLATE_DISABLE_PIC_LOOKUP"):

GC_BUG_CHECK: extra features for debugging the GC

GC_INTEGRITY_CHECK: check all the object memory to make sure it
looks good after a GC

SLATE_DISABLE_METHOD_OPTIMIZATION: Don't optimize methods after
calling them a lot. Right now it sets method->oldCode to the current
code. In the future it should do inlining.

SLATE_DISABLE_PIC_LOOKUP: At the call site in the current method there
is a cache of called functions which is checked after the global cache
when doing method dispatch.

SLATE_DISABLE_METHOD_CACHE: Disable the global cache when doing method
dispatch.

SLATE_USE_MMAP: Use mmap to allocate the object memory. You should be
able to get constant pointers between runnings so that you can debug
by learning memory addresses from previous runs.

PRINT_DEBUG and the many PRINT_DEBUG_*: Print more verbose output to
the console.

ALWAYS_FULL_GC: Never do a new-generation garbage collection.

GC_MARK_FREED_MEMORY: Mark freed memory with 0xFE in case someone
tries to use it again. (Slow)

See the source for more.

Source directory structure
--------------------------

src/vm
: The C files for the VM. Interprets bytecode and provides necessary facilities for primitives, gc, etc.
src/mobius -> The slate files for the compiler, lexer, bootstrap, and close-to-vm? facilities etc.
src/core
: The core libraries for the default slate system.
src/lib
: The standard but optional libraries.
src/plugins
: C code for slate FFI calls. `make plugins' to build
src/ui
: The slate UI code that probably calls plugins to draw the basics. load: 'src/ui/init.slate'.
src/ui/gtk
: The slate GTK interface. load: 'src/ui/gtk/demo.slate'.
src/net
: The slate networking code. load: 'src/net/init.slate'.

Finding source code
-------------------

Besides using grep, there are a few facilities:

 #as: implementations do: [|:each| each definitionLocation ifNotNilDo: [|:l| inform: (l as: String) ]].

 (#parseExpression findOn: {Syntax Parser}) definitionLocation

See the slate manual for more details.

