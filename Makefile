#
# Makefile for Petit Ami test
#

all: test event getkeys terminal
	
test: pa_ansi.c pa_terminal.h test.c stdio.c stdio.h Makefile
	gcc -g -o test stdio.c pa_ansi.c test.c
	
event: pa_ansi.c pa_terminal.h event.c stdio.c stdio.h Makefile
	gcc -g -o event stdio.c pa_ansi.c event.c
	
getkeys: getkeys.c Makefile
	gcc -g -o getkeys getkeys.c
	
terminal: pa_ansi.c pa_terminal.h terminal.c stdio.c stdio.h Makefile
	gcc -g -o terminal stdio.c pa_ansi.c terminal.c

test1: test1.c
	gcc -o test1 test1.c -lrt
	
clean:
	rm -f *.exe

