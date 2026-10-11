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
MODEL ?= plain
LINK_TYPE ?= static

#
# Required libraries
#
LIBS = -lasound -lfluidsynth -lssl -lcrypto -lm -lpthread

#
# The system libraries the graphical model draws with: those of the display
# backend the graphical library was built with, wayland here. For x11 use
# -lX11 -lXtst -lfreetype -lfontconfig, for fb -lfreetype -lfontconfig -lpng -lz
#
ifeq ($(MODEL),graph)
    LIBS += -lwayland-client -lwayland-cursor -lxkbcommon -lfreetype -lfontconfig
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
