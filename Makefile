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
        #STDIO_SOURCE=glibc
        STDIO_SOURCE=stdio
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
CFLAGS=-g3

ifeq ($(STDIO_SOURCE),stdio)
    #
    # In local link, we need to get stdio.h from local directory
    #
    CFLAGS +=-Iinclude
endif

ifeq ($(LINK_TYPE),static)
    CFLAGS += -static
endif

ifeq ($(STDIO_SOURCE),glibc)
    ifeq ($(LINK_TYPE),static)
        LIBS = libc.a
    else
        LIBS = libc.so
    endif
else
    LIBS = stdio.c
endif

#
# Choose which model, console or graphics window.
#
ifdef GRAPH
    LIBS += -L/usr/X11R6/lib -lX11 -lxcb
    BASEMOD = linux/graph_x.c linux/sound.c
else
    BASEMOD = linux/xterm.c linux/sound.c
endif

#
# Individual Petit-Ami library components
#
linux/services.o: linux/services.c include/services.h
	gcc -g3 -Iinclude -fPIC -c linux/services.c -o linux/services.o
	
linux/sound.o: linux/sound.c include/sound.h
	gcc -g3 -Iinclude -fPIC -c linux/sound.c -o linux/sound.o
	
linux/xterm.o: linux/xterm.c include/terminal.h
	gcc -g3 -Iinclude -fPIC -c linux/xterm.c -o linux/xterm.o
	
linux/graph_x.o: linux/graph_x.c include/graph.h
	gcc -g3 -Iinclude -fPIC -c linux/graph_x.c -o linux/graph_x.o

#
# Create terminal mode and graphical mode libraries
#
bin/petit_ami_term.so: linux/services.o linux/sound.o linux/xterm.o
	gcc -shared linux/services.o linux/sound.o linux/xterm.o -o bin/petit_ami_term.so
	
bin/petit_ami_term.a: linux/services.o linux/sound.o linux/xterm.o
	ar rcs bin/petit_ami_term.a linux/services.o linux/sound.o linux/xterm.o
	
petit_ami_graph: linux/services.o linux/sound.o linux/graph_x.o
	gcc linux/services.o linux/sound.o linux/xterm.o -o bin/petit_ami_graph.so
	ar rcs bin/petit_ami_graph.a linux/services.o linux/sound.o linux/xterm.o
        
all: test event getkeys terminal scntst
	
test: bin/petit_ami_term.so include/terminal.h test.c Makefile
	$(CC) $(CFLAGS) -Wl,--rpath=bin -o test bin/libc.so test.c bin/petit_ami_term.so -lasound -lm -pthread
	
play: bin/petit_ami_term.so include/terminal.h sound_prog/play.c Makefile
	$(CC) $(CFLAGS) -Wl,--rpath=bin -o play bin/libc.so sound_prog/play.c bin/petit_ami_term.so -lasound -lm -pthread
	
testg: $(BASEMOD) terminal.h testg.c services.c services.h Makefile
	$(CC) $(CFLAGS) $(BASEMOD) testg.c $(LIBS) -lm -o testg
	
scntst: $(BASEMOD) terminal.h scntst.c services.h services.c Makefile
	$(CC) $(CFLAGS) -o scntst xterm.c services.c scntst.c $(LIBS) -lm 
	
event: $(BASEMOD) terminal.h event.c Makefile
	$(CC) $(CFLAGS) -o event $(BASEMOD) event.c $(LIBS) -lm 
	
getkeys: getkeys.c Makefile
	$(CC) $(CFLAGS) -o getkeys getkeys.c -lm 
	
getmouse: getmouse.c Makefile
	$(CC) $(CFLAGS) -o getmouse getmouse.c -lm 
	
term: $(BASEMOD) terminal.h term.c Makefile
	$(CC) $(CFLAGS) $(BASEMOD) term.c $(LIBS) -lm -o term
	
snake: term_game/snake.c $(BASEMOD) terminal.h Makefile
	$(CC) $(CFLAGS) -o term_game/snake $(BASEMOD) term_game/snake.c $(LIBS) -lm
	
mine: term_game/mine.c $(BASEMOD) terminal.h Makefile
	$(CC) $(CFLAGS) -o term_game/mine $(BASEMOD) term_game/mine.c $(LIBS) -lm
	
editor: term_prog/editor.c $(BASEMOD) terminal.h Makefile
	$(CC) $(CFLAGS) -o term_prog/editor $(BASEMOD) term_prog/editor.c $(LIBS) -lm
	
clean:
	rm -f test scmtst event getkeys getmouse term term_game/snake term_game/mine term_prog/editor test1
