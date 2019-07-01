#
# Makefile for Petit Ami test
#

#
# Where do stdio definitions and overrides come from?
#
ifndef STDIO_SOURCE
    ifeq ($(OS),Windows_NT)
        #
        # stdio is a local stdio implementation with overrides.
        # Note it is not a complete libc. It works because in dynamic linking a module
        # can effectively replace a later module of the same names. It does not work
        # for static linking or if other modules in chain bypass posix standards.
        # 
        # Windows builds must use stdio since they don't use glibc.
        #
        STDIO_SOURCE=stdio
    else
        #
        # glibc assumes that this is a patched glibc with override calls.
        #
        STDIO_SOURCE=glibc
        #STDIO_SOURCE=stdio
    endif
endif

#
# Link image statically or dynamically?
#
ifndef LINK_TYPE
    ifeq ($(STDIO_SOURCE),stdio)
        # Linking local stdio, must be dynamic
        LINK_TYPE=dynamic
    else
#        LINK_TYPE=static
        LINK_TYPE=dynamic
    endif
endif

CC=gcc
CFLAGS=-g3 -Wl,--rpath=bin -Iinclude

ifeq ($(STDIO_SOURCE),stdio)
    #
    # In local link, we need to get stdio.h from local directory
    #
    CFLAGS +=-Ilibc
endif

ifeq ($(LINK_TYPE),static)
    CFLAGS += -static
endif

#
# For modified GLIBC, we need to specify the libary first or it won't
# link correctly.
#
ifeq ($(LINK_TYPE),static)
    LIBS = bin/libc.a
else
	LIBS = bin/libc.so.6
endif

#
# Collected libraries
#
ifndef GRAPH
    #
    # Terminal model API
    #
    ifeq ($(LINK_TYPE),static)
        LIBS += bin/petit_ami_term.a
    else
    	LIBS += linux/sound.o bin/petit_ami_term.so
    endif
else
    #
    # Graphical model API
    #
    ifeq ($(LINK_TYPE),static)
	    LIBS += bin/petit_ami_graph.a
	else
	    LIBS += bin/petit_ami_graph.so
	endif
endif 
LIBS += -lasound -lm -pthread

#
# Make all executables
#        
all: test play scntst event getkeys getmouse term snake mine editor 

#
# Individual Petit-Ami library components
#
linux/services.o: linux/services.c include/services.h
	gcc -g3 -Iinclude -fPIC -c linux/services.c -o linux/services.o
	
linux/sound.o: linux/sound.c include/sound.h
	gcc -g3 -Iinclude -fPIC -c linux/sound.c -lasound -lm -pthread -o linux/sound.o
	
linux/xterm.o: linux/xterm.c include/terminal.h
	gcc -g3 -Iinclude -fPIC -c linux/xterm.c -o linux/xterm.o
	
linux/graph_x.o: linux/graph_x.c include/graph.h
	gcc -g3 -Iinclude -fPIC -c linux/graph_x.c -o linux/graph_x.o

#
# Create terminal mode and graphical mode libraries
#

#
# Note that sound lib cannot be put into an .so, there is a bug in ALSA.
# Thus we leave it as a .o file.
#
bin/petit_ami_term.so: linux/services.o linux/sound.o linux/xterm.o
	gcc -shared linux/services.o linux/xterm.o -o bin/petit_ami_term.so
	
bin/petit_ami_term.a: linux/services.o linux/sound.o linux/xterm.o
	ar rcs bin/petit_ami_term.a linux/services.o linux/sound.o linux/xterm.o
	
petit_ami_graph.so: linux/services.o linux/sound.o linux/graph_x.o
	gcc linux/services.o linux/sound.o linux/xterm.o -o bin/petit_ami_graph.so
	
petit_ami_graph.a: linux/services.o linux/sound.o linux/graph_x.o
	ar rcs bin/petit_ami_graph.a linux/services.o linux/sound.o linux/xterm.o

#
# Make individual executables
#	
dumpmidi: linux/dumpmidi.c Makefile
	gcc -o bin/dumpmidi linux/dumpmidi.c

test: bin/petit_ami_term.so include/terminal.h test.c Makefile
	$(CC) $(CFLAGS) test.c $(LIBS) -o test
	
play: bin/petit_ami_term.so include/terminal.h sound_prog/play.c Makefile
	$(CC) $(CFLAGS) sound_prog/play.c $(LIBS) -o bin/play
	
scntst:  include/terminal.h tests/scntst.c include/services.h linux/services.c Makefile
	$(CC) $(CFLAGS) tests/scntst.c $(LIBS) -o bin/scntst 
	
event: include/terminal.h tests/event.c Makefile
	$(CC) $(CFLAGS) tests/event.c $(LIBS) -o bin/event
	
getkeys: linux/getkeys.c Makefile
	$(CC) $(CFLAGS) linux/getkeys.c -lm -o bin/getkeys
	
getmouse: linux/getmouse.c Makefile
	$(CC) $(CFLAGS) linux/getmouse.c -lm -o bin/getmouse
	
term: include/terminal.h tests/term.c Makefile
	$(CC) $(CFLAGS) tests/term.c $(LIBS) -o bin/term
	
snake: term_game/snake.c include/terminal.h Makefile
	$(CC) $(CFLAGS) term_game/snake.c $(LIBS) -o bin/snake
	
mine: term_game/mine.c include/terminal.h Makefile
	$(CC) $(CFLAGS) term_game/mine.c $(LIBS) -o bin/mine
	
editor: term_prog/editor.c include/terminal.h Makefile
	$(CC) $(CFLAGS) term_prog/editor.c $(LIBS) -o bin/editor
	
clean:
	rm -f test bin/scntst bin/event bin/getkeys bin/getmouse bin/term bin/snake
	rm -f bin/mine bin/editor
	rm -f linux/*.o
	rm -f bin/petit_ami_term.a
	rm -f bin/petit_ami_term.so
