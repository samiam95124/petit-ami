#
# Makefile for Petit Ami test
#

all: test event getkeys terminal scntst
	
test: ansiterm.c terminal.h test.c stdio.c stdio.h services.c services.h Makefile
	gcc -g -o test services.c test.c -lm 
#	gcc -g -o test stdio.c services.c ansiterm.c test.c -lm 
	
scntst: ansiterm.c terminal.h scntst.c stdio.c stdio.h services.h services.c Makefile
	gcc -g -o scntst stdio.c ansiterm.c services.c scntst.c -lm 
	
event: ansiterm.c terminal.h event.c stdio.c stdio.h Makefile
	gcc -g -o event stdio.c ansiterm.c event.c -lm 
	
getkeys: getkeys.c Makefile
	gcc -g -o getkeys getkeys.c -lm 
	
term: ansiterm.c terminal.h term.c stdio.c stdio.h Makefile
	gcc -g -o term stdio.c ansiterm.c term.c -lm 

test1: test1.c
	gcc -o test1 test1.c -lrt -lm 
	
clean:
	rm -f *.exe

