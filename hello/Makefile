#
# Example makefile for "plain" output hello world program
#

#
# Where Petit-Ami stands.
#
# Taken from this makefile's own path, not from the directory make was started
# in, so the example builds the same from here, from the root with
# make -C hello, or from anywhere with make -f.
#
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)

#
# Define libraries used.
#
# The C++ runtime is here because the Petit-Ami core carries the C++ binding
# with it, and libc is not named at all: the program takes the system one.
#
LIBS=-lasound -lfluidsynth -lssl -lcrypto -lstdc++ -lm -lpthread

#
# Archives
#
# The plain model in one archive. Sound and network sit inside it as members,
# and are drawn in only by a program that calls them, so a program like this
# one that calls neither pays for neither.
#
ARCS=$(ROOT)/lib/libami_plain.a

hello: hello.c Makefile $(ARCS)
	gcc -g3 -I$(ROOT)/include hello.c $(ARCS) $(LIBS) -o hello

#
# What the example borrows from the main build, made there when the tree has
# not been built yet.
#
$(ARCS):
	$(MAKE) -C $(ROOT) $(patsubst $(ROOT)/%,%,$@)

clean:
	rm -f hello
