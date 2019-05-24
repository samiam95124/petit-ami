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
        LINK_TYPE=static
#        LINK_TYPE=dynamic
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
	
#test: xterm.c terminal.h test.c stdio.c stdio.h services.c services.h Makefile
#	$(CC) $(CFLAGS) -o test stdio.c xterm.c test.c -lm  
	
scntst: xterm.c terminal.h scntst.c stdio.c stdio.h services.h services.c Makefile
	$(CC) $(CFLAGS) -o scntst xterm.c services.c scntst.c $(LIBS) -lm 
	
event: xterm.c terminal.h event.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o event stdio.c xterm.c event.c -lm 
	
getkeys: getkeys.c Makefile
	$(CC) $(CFLAGS) -o getkeys getkeys.c -lm 
	
getmouse: getmouse.c Makefile
	$(CC) $(CFLAGS) -o getmouse getmouse.c -lm 
	
term: xterm.c terminal.h term.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o term stdio.c xterm.c term.c -lm 
	
snake: term_game/snake.c xterm.c terminal.h term.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o term_game/snake xterm.c term_game/snake.c $(LIBS) -lm
	
mine: term_game/mine.c xterm.c terminal.h term.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o term_game/mine stdio.c xterm.c term_game/mine.c -lm
	
editor: term_prog/editor.c xterm.c terminal.h term.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o term_prog/editor stdio.c xterm.c term_prog/editor.c -lm

test1: test1.c
	$(CC) $(CFLAGS) -o test1 stdio.c xterm.c test1.c -lrt -lm
	
clean:
	rm -f test scmtst event getkeys getmouse term term_game/snake term_game/mine term_prog/editor test1

