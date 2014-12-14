#
# Makefile for Petit Ami test
#

all: test event getkeys terminal
	
test: ansiterm.c terminal.h test.c stdio.c stdio.h Makefile
	gcc -g -o test stdio.c ansiterm.c test.c
	
event: ansiterm.c terminal.h event.c stdio.c stdio.h Makefile
	gcc -g -o event stdio.c ansiterm.c event.c
	
getkeys: getkeys.c Makefile
	gcc -g -o getkeys getkeys.c
	
term: ansiterm.c terminal.h term.c stdio.c stdio.h Makefile
	gcc -g -o term stdio.c ansiterm.c term.c

test1: test1.c
	gcc -o test1 test1.c -lrt
	
clean:
	rm -f *.exe

