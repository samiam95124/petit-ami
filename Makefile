#
# Makefile for Petit Ami test
#

#
# Where do stdio definitions and overrides come from?
#
ifndef STDIO_SOURCE
    #
    # glibc assumes that this is a patched glibc with override calls.
    #
    STDIO_SOURCE=glibc
    #
    # stdio is a local stdio implementation with overrides.
    # Note it is not a complete libc. It works because in dynamic linking a module
    # can effectively replace a later module of the same names. It does not work
    # for static linking or if other modules in chain bypass posix standards.
    # 
#   STDIO_SOURCE=stdio
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

all: test event getkeys terminal scntst
	
test: xterm.c terminal.h test.c services.c services.h Makefile
	$(CC) $(CFLAGS) -o test test.c xterm.c $(LIBS) -lm
	
scntst: xterm.c terminal.h scntst.c services.h services.c Makefile
	$(CC) $(CFLAGS) -o scntst xterm.c services.c scntst.c $(LIBS) -lm 
	
gtk_test_unbuffered: xterm.c terminal.h gtk_test_unbuffered.c graph_gtk.c services.c services.h Makefile
	$(CC) $(CFLAGS) -o gtk_test_unbuffered gtk_test_unbuffered.c graph_gtk.c `pkg-config --cflags --libs gtk+-3.0` $(LIBS) -lm
	
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
