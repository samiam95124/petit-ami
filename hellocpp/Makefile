#
# Example makefile for the hello world program in C++, built on Petit-Ami
#
# The program writes std::cout, and the stream lands where the model puts
# standard output, as printf does in ../hello:
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
# Ask for the stream binding by name: it puts std::cout and the other standard
# streams on Petit-Ami's stdio, and a program that only writes std::cout calls
# nothing in it. Then force use of the chosen model.
#
KEEP = -Wl,-u,ami_iostream
ifneq ($(MODEL),plain)
    KEEP += -Wl,-u,ami_maxx
endif

#
# Set Ami specific libraries
#
# Sound cannot live in a shared library (an ALSA bug), so in the dynamic form
# its object links directly beside the library. A C++ program compiles against
# the system's headers in either form: the streams reach Petit-Ami through the
# binding in the library.
#
ifeq ($(LINK_TYPE),static)
    AMILIB = $(AMIDIR)/lib/libami_$(MODEL).a
    AMILINK = $(AMILIB)
else
    AMILIB = $(AMIDIR)/lib/sound.o $(AMIDIR)/lib/petit_ami_$(MODEL).so
    AMILINK = $(AMILIB) -Wl,-rpath,$(AMIDIR)/lib
endif

hello: hello.cpp $(AMILIB)
	g++ -g3 -I$(AMIDIR)/include -I$(AMIDIR)/hpp hello.cpp $(KEEP) $(AMILINK) $(LIBS) -o hello

clean:
	rm -f hello
