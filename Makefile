#
# Makefile for Petit Ami test
#

all: test.exe
	
test.exe: pa_ansi.c pa_terminal.h test.c stdio.c stdio.h
	gcc -g -o test pa_ansi.c stdio.c test.c
clean:
	rm -f *.exe

