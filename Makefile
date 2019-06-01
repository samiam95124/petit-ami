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
    CFLAGS +=-I.
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
# GTK definitions
#
GTK_LIBS=`pkg-config --cflags --libs gtk+-3.0`
        
all: test event getkeys terminal scntst
	
gtk_test: xterm.c terminal.h gtk_test.c graph_gtk.c services.c services.h Makefile
	$(CC) $(CFLAGS) -o gtk_test gtk_test.c graph_gtk.c `pkg-config --cflags --libs gtk+-3.0` $(LIBS) -lm
	
test: xterm.c terminal.h test.c services.c services.h Makefile
	$(CC) $(CFLAGS) -o test test.c xterm.c $(LIBS) -lm
	
testg: graph_x.c terminal.h testg.c services.c services.h Makefile
	$(CC) $(CFLAGS) graph_x.c testg.c -L/usr/X11R6/lib -lX11 -lxcb $(LIBS) -lm -o testg
#	$(CC) $(CFLAGS) -o testg testg.c graph_gtk.c $(GTK_LIBS) $(LIBS) -lm
	
scntst: xterm.c terminal.h scntst.c services.h services.c Makefile
	$(CC) $(CFLAGS) -o scntst xterm.c services.c scntst.c $(LIBS) -lm 
	
gtk_test_unbuffered: xterm.c terminal.h gtk_test_unbuffered.c graph_gtk.c services.c services.h Makefile
	$(CC) $(CFLAGS) -o gtk_test_unbuffered gtk_test_unbuffered.c graph_gtk.c $(GTK_LIBS) $(LIBS) -lm
	
gtk_test_buffered: xterm.c terminal.h gtk_test_buffered.c graph_gtk.c services.c services.h Makefile
	$(CC) $(CFLAGS) -o gtk_test_buffered gtk_test_buffered.c graph_gtk.c $(GTK_LIBS) $(LIBS) -lm
	
gtk_test2: xterm.c terminal.h gtk_test2.c graph_gtk.c services.c services.h Makefile
	$(CC) $(CFLAGS) -o gtk_test2 gtk_test2.c graph_gtk.c `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0` $(LIBS) -lm
	
event: xterm.c terminal.h event.c Makefile
	$(CC) $(CFLAGS) -o event xterm.c event.c $(LIBS) -lm 
	
getkeys: getkeys.c Makefile
	$(CC) $(CFLAGS) -o getkeys getkeys.c -lm 
	
getmouse: getmouse.c Makefile
	$(CC) $(CFLAGS) -o getmouse getmouse.c -lm 
	
term: xterm.c terminal.h term.c Makefile
	$(CC) $(CFLAGS) -o term xterm.c term.c $(LIBS) -lm 
	
snake: term_game/snake.c xterm.c terminal.h term.c Makefile
	$(CC) $(CFLAGS) -o term_game/snake xterm.c term_game/snake.c $(LIBS) -lm
	
mine: term_game/mine.c xterm.c terminal.h term.c Makefile
	$(CC) $(CFLAGS) -o term_game/mine xterm.c term_game/mine.c $(LIBS) -lm
	
editor: term_prog/editor.c xterm.c terminal.h term.c Makefile
	$(CC) $(CFLAGS) -o term_prog/editor xterm.c term_prog/editor.c $(LIBS) -lm
	
clean:
	rm -f test scmtst event getkeys getmouse term term_game/snake term_game/mine term_prog/editor test1
