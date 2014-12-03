#
# Makefile for Petit Ami test
#

all: test
	
test: pa_ansi.c pa_terminal.h test.c stdio.c stdio.h Makefile
	gcc -g -o test stdio.c pa_ansi.c test.c
	
event: pa_ansi.c pa_terminal.h event.c stdio.c stdio.h Makefile
	gcc -g -o event stdio.c pa_ansi.c event.c
	
getkeys: getkeys.c
	gcc -o getkeys getkeys.c
	
clean:
	rm -f *.exe

