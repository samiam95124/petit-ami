#
# Example makefile for the hello world program, built on Petit-Ami
#
# The same program builds on any of the three Petit-Ami models, against
# either form of the library:
#
#   MODEL=plain      no terminal handler: system, sound and network only
#   MODEL=term       the terminal model: output goes to the console
#   MODEL=graph      the graphical model: output goes to a window
#
#   LINK_TYPE=static     the archive ../lib/libami_<model>.a (the default)
#   LINK_TYPE=dynamic    the shared library ../lib/petit_ami_<model>.so
#
# Either is given on the command line, as in
#
#   make MODEL=graph LINK_TYPE=dynamic
#
# or set here. The example uses only the include and lib directories of the
# Petit-Ami tree, made by the build at the root, and in the dynamic form its
# stdio header in libc. Build at the root first; after changing an option
# here, make clean before making again.
#
MODEL ?= plain
LINK_TYPE ?= static

# a value outside the sets stops the make rather than building the wrong thing
ifeq ($(filter $(MODEL),plain term graph),)
    $(error MODEL is $(MODEL): it must be plain, term or graph)
endif
ifeq ($(filter $(LINK_TYPE),static dynamic),)
    $(error LINK_TYPE is $(LINK_TYPE): it must be static or dynamic)
endif

#
# Where Petit-Ami stands: AMIDIR, set by sourcing setpath at the root of the
# Petit-Ami tree.
#
ifndef AMIDIR
    $(error AMIDIR is not set: source setpath at the root of the Petit-Ami tree)
endif

#
# The system libraries every model wants.
#
# libc is not named at all: the program takes the system one. The sound and
# network parts of the library are drawn in only by a program that calls
# them, so a program like this one that calls neither pays for neither. The
# C++ binding is drawn in the same way, by a C++ program calling through
# it, so no C++ runtime is named here: g++ supplies it for such a program.
#
LIBS = -lasound -lfluidsynth -lssl -lcrypto -lm -lpthread

#
# The system libraries the graphical model draws with. They depend on the
# backend the graphical library was built with, which the root build
# records in lib/graph_backend: wayland, x11 or fb.
#
BACKLIBS_wayland = -lwayland-client -lwayland-cursor -lxkbcommon \
                   -lfreetype -lfontconfig
BACKLIBS_x11 = -lX11 -lXtst -lfreetype -lfontconfig
BACKLIBS_fb = -lfreetype -lfontconfig -lpng -lz

GRAPHLIBS = $(BACKLIBS_$(shell cat $(AMIDIR)/lib/graph_backend 2> /dev/null))

ifeq ($(MODEL),graph)
    LIBS += $(GRAPHLIBS)
endif

#
# Keeping the model in the link.
#
# The terminal and graphical models take over stdout as the program starts,
# so a program that calls nothing in them, like this one, still wants them
# linked. Named only by its calls, the library would be left out, so the
# link asks for one of the model's functions by name. The plain model has
# no such takeover, and needs nothing.
#
ifneq ($(MODEL),plain)
    KEEP = -Wl,-u,ami_maxx
endif

#
# The Petit-Ami library, in the form chosen.
#
# Sound cannot live in a shared library (an ALSA bug), so in the dynamic form
# its object links directly beside the library, and the program carries the
# full path of the lib directory so that it runs from anywhere. The sound object
# stands before the shared library: the linker keeps a shared library only
# for references seen before it, and the sound object's are the first.
#
# The two forms meet the program's own printf differently. The archive
# carries Petit-Ami's stdio under the standard names, so the program is
# compiled against the system header and its printf lands in the model at
# link. The shared library keeps Petit-Ami's stdio under its own names
# beside the system one, so the program is compiled against Petit-Ami's
# stdio header (../libc/stdio.h), which gives printf that name.
#
ifeq ($(LINK_TYPE),static)
    AMILIB = $(AMIDIR)/lib/libami_$(MODEL).a
    AMILINK = $(AMILIB)
    AMICFLAGS =
else
    AMILIB = $(AMIDIR)/lib/sound.o $(AMIDIR)/lib/petit_ami_$(MODEL).so
    AMILINK = $(AMILIB) -Wl,-rpath,$(AMIDIR)/lib
    AMICFLAGS = -I$(AMIDIR)/libc -DSTDIO_BYPASS
endif

hello: hello.c $(AMILIB)
	gcc -g3 -I$(AMIDIR)/include $(AMICFLAGS) hello.c $(KEEP) $(AMILINK) $(LIBS) -o hello

clean:
	rm -f hello
