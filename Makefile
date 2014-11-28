#
# Makefile for Petit Ami test
#

all: test.exe
	
test.exe: pa_ansi.c pa_terminal.h test.c stdio.c stdio.h Makefile
	gcc -g -o test stdio.c pa_ansi.c test.c
clean:
	rm -f *.exe

