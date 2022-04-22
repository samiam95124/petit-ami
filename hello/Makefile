#
# Example makefile for "plain" output hello world program
#

#
# Define libraries used.
#
LIBS=-lasound -lfluidsynth -lm -lpthread -lssl -lcrypto

#
# Define objects used
#
# Note that these have to be included here, and not in a library, because of ALSA bug.
#
OBJS=../linux/sound.o ../linux/fluidsynthplug.o ../linux/dumpsynthplug.o

#
# Archives/dlls/sos
#
ARCS=../bin/libc.so.6 ../bin/petit_ami_plain.so

hello: hello.c Makefile
	gcc -g3 -I../include -Wl,--rpath=bin hello.c $(ARCS) $(OBJS) $(LIBS) -o hello
	
clean:
	rm hello