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
# On a graphical program, choose Wayland or X11
#
ifeq ($(MODEL),graph)
    ifneq ($(WAYLAND_DISPLAY),)
        LIBS += -lwayland-client -lwayland-cursor -lxkbcommon -lfreetype -lfontconfig
    else
        LIBS += -lX11 -lXtst -lfreetype -lfontconfig
    endif
endif

#
# Force use of the chosen model
#
ifneq ($(MODEL),plain)
    KEEP = -Wl,-u,ami_maxx
endif

#
# Set Ami specific libraries
#
# Sound cannot live in a shared library (an ALSA bug), so in the dynamic form
# its object links directly beside the library. Dynamic linking needs stdio
# bypass.
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
