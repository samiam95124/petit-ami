#
# Makefile for Petit Ami test
#

CC=gcc
CFLAGS=-I. -g3

all: test event getkeys terminal scntst
	
test: xterm.c terminal.h test.c stdio.c stdio.h services.c services.h Makefile
	$(CC) $(CFLAGS) -o test stdio.c xterm.c test.c -lm 
	
scntst: xterm.c terminal.h scntst.c stdio.c stdio.h services.h services.c Makefile
	$(CC) $(CFLAGS) -o scntst stdio.c xterm.c services.c scntst.c -lm 
	
event: xterm.c terminal.h event.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o event stdio.c xterm.c event.c -lm 
	
getkeys: getkeys.c Makefile
	$(CC) $(CFLAGS) -o getkeys getkeys.c -lm 
	
getmouse: getmouse.c Makefile
	$(CC) $(CFLAGS) -o getmouse getmouse.c -lm 
	
term: xterm.c terminal.h term.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o term stdio.c xterm.c term.c -lm 
	
snake: term_game/snake.c xterm.c terminal.h term.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o term_game/snake stdio.c xterm.c term_game/snake.c -lm
	
mine: term_game/mine.c xterm.c terminal.h term.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o term_game/mine stdio.c xterm.c term_game/mine.c -lm
	
editor: term_prog/editor.c xterm.c terminal.h term.c stdio.c stdio.h Makefile
	$(CC) $(CFLAGS) -o term_prog/editor stdio.c xterm.c term_prog/editor.c -lm

test1: test1.c
	$(CC) $(CFLAGS) -o test1 stdio.c xterm.c test1.c -lrt -lm
	
clean:
	rm -f test scmtst event getkeys getmouse term term_game/snake term_game/mine term_prog/editor test1

