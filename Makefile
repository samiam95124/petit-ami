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
        STDIO_SOURCE=glibc
        #STDIO_SOURCE=stdio
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
CFLAGS=-g3 -Wl,--rpath=bin -Iinclude

ifeq ($(STDIO_SOURCE),stdio)
    #
    # In local link, we need to get stdio.h from local directory
    #
    CFLAGS +=-Ilibc
endif

ifeq ($(LINK_TYPE),static)
    CFLAGS += -static
endif

#
# For modified GLIBC, we need to specify the libary first or it won't
# link correctly.
#
ifeq ($(LINK_TYPE),static)
    LIBS = bin/libc.a
    PLIBS = bin/libc.a
else
	LIBS = bin/libc.so.6
	PLIBS = bin/libc.so.6
endif

#
# Collected libraries
#

#
# Plain (no terminal handler)
# This option exists to drop the terminal handler, which should not be
# required for most code.
#
# Note there is no statically linked sound at the moment, since we don't have
# an absolute version of fluidsynth.
#
ifeq ($(LINK_TYPE),static)
    PLIBS += bin/petit_ami_plain.a
else
    PLIBS += linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o \
             bin/petit_ami_plain.so
endif

#
# terminal handler libraries
#
ifndef GRAPH
    #
    # Terminal model API
    #
    ifeq ($(LINK_TYPE),static)
        LIBS += bin/petit_ami_term.a
    else
        LIBS += linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o \
    	        bin/petit_ami_term.so
    endif
else
    #
    # Graphical model API
    #
    ifeq ($(LINK_TYPE),static)
	    LIBS += bin/petit_ami_graph.a
	else
	    LIBS += bin/petit_ami_graph.so
	endif
endif 
LIBS += -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
PLIBS += -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto

#
# Make all executables
#        
all: dumpmidi lsalsadev alsaparms test play keyboard playmidi playwave \
    printdev connectmidi connectwave random genwave scntst sndtst event \
    getkeys getmouse term snake mine editor getpage getmail gettys 

#
# Individual Petit-Ami library components
#
linux/services.o: linux/services.c include/services.h
	gcc -g3 -Iinclude -fPIC -c linux/services.c -o linux/services.o
	
linux/sound.o: linux/sound.c include/sound.h
	gcc -g3 -Iinclude -fPIC -c linux/sound.c -lasound -lm -pthread -o linux/sound.o
	
linux/network.o: linux/network.c include/network.h
	gcc -g3 -Iinclude -fPIC -c linux/network.c -o linux/network.o
	
linux/fluidsynthplug.o: linux/fluidsynthplug.c include/sound.h
	gcc -g3 -Iinclude -fPIC -c linux/fluidsynthplug.c -lasound -lm -pthread -o linux/fluidsynthplug.o
	
linux/dumpsynthplug.o: linux/dumpsynthplug.c include/sound.h
	gcc -g3 -Iinclude -fPIC -c linux/dumpsynthplug.c -lasound -lm -pthread -o linux/dumpsynthplug.o
	
linux/xterm.o: linux/xterm.c include/terminal.h
	gcc -g3 -Iinclude -fPIC -c linux/xterm.c -o linux/xterm.o
	
linux/graph_x.o: linux/graph_x.c include/graph.h
	gcc -g3 -Iinclude -fPIC -c linux/graph_x.c -o linux/graph_x.o

#
# Create terminal mode and graphical mode libraries
#

#
# Note that sound lib cannot be put into an .so, there is a bug in ALSA.
# Thus we leave it as a .o file.
#
bin/petit_ami_plain.so: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o
	gcc -shared linux/services.o linux/sound.o linux/fluidsynthplug.o \
	    linux/dumpsynthplug.o linux/network.o -o bin/petit_ami_plain.so
	
bin/petit_ami_plain.a: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o
	ar rcs bin/petit_ami_plain.a linux/services.o linux/sound.o \
	    linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o
	
bin/petit_ami_term.so: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/xterm.o
	gcc -shared linux/services.o linux/sound.o linux/fluidsynthplug.o \
	    linux/dumpsynthplug.o  linux/network.o linux/xterm.o -o bin/petit_ami_term.so
	
bin/petit_ami_term.a: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/xterm.o
	ar rcs bin/petit_ami_term.a linux/services.o linux/sound.o \
	    linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o linux/xterm.o
	
petit_ami_graph.so: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/graph_x.o
	gcc linux/services.o linux/sound.o linux/fluidsynthplug.o \
	linux/dumpsynthplug.o  linux/network.o linux/xterm.o -o bin/petit_ami_graph.so
	
petit_ami_graph.a: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/graph_x.o
	ar rcs bin/petit_ami_graph.a linux/services.o linux/sound.o \
	linux/fluidsynthplug.o linux/dumpsynthplug.o  linux/network.o linux/xterm.o

#
# Make individual executables
#	
dumpmidi: linux/dumpmidi.c Makefile
	gcc linux/dumpmidi.c -o bin/dumpmidi
	
lsalsadev: linux/lsalsadev.c Makefile
	gcc linux/lsalsadev.c -lasound -o bin/lsalsadev
	
alsaparms: linux/alsaparms.c Makefile
	gcc linux/alsaparms.c -lasound -o bin/alsaparms

test: bin/petit_ami_plain.so include/terminal.h test.c Makefile
	$(CC) $(CFLAGS) test.c $(PLIBS) -o test
	
play: bin/petit_ami_term.so include/terminal.h sound_programs/play.c Makefile
	$(CC) $(CFLAGS) sound_programs/play.c linux/option.c $(LIBS) -o bin/play
	
keyboard: bin/petit_ami_term.so include/terminal.h sound_programs/keyboard.c Makefile
	$(CC) $(CFLAGS) sound_programs/keyboard.c linux/option.c $(LIBS) -o bin/keyboard
	
playmidi: bin/petit_ami_term.so include/terminal.h sound_programs/playmidi.c linux/option.c Makefile
	$(CC) $(CFLAGS) sound_programs/playmidi.c linux/option.c $(PLIBS) -o bin/playmidi

playwave: bin/petit_ami_term.so include/terminal.h sound_programs/playwave.c Makefile
	$(CC) $(CFLAGS) sound_programs/playwave.c linux/option.c $(PLIBS) -o bin/playwave
	
printdev: bin/petit_ami_term.so include/terminal.h sound_programs/printdev.c Makefile
	$(CC) $(CFLAGS) sound_programs/printdev.c $(PLIBS) -o bin/printdev

connectmidi: bin/petit_ami_term.so include/terminal.h sound_programs/connectmidi.c Makefile
	$(CC) $(CFLAGS) sound_programs/connectmidi.c $(PLIBS) -o bin/connectmidi
	
connectwave: bin/petit_ami_term.so include/terminal.h sound_programs/connectwave.c Makefile
	$(CC) $(CFLAGS) sound_programs/connectwave.c $(PLIBS) -o bin/connectwave
	
random: bin/petit_ami_term.so include/terminal.h sound_programs/random.c Makefile
	$(CC) $(CFLAGS) sound_programs/random.c linux/option.c $(LIBS) -o bin/random
	
genwave: bin/petit_ami_term.so include/terminal.h sound_programs/genwave.c Makefile
	$(CC) $(CFLAGS) sound_programs/genwave.c linux/option.c $(PLIBS) -o bin/genwave
	
scntst: bin/petit_ami_term.so include/terminal.h tests/scntst.c include/services.h linux/services.c Makefile
	$(CC) $(CFLAGS) tests/scntst.c $(LIBS) -o bin/scntst 
	
sndtst: bin/petit_ami_term.so include/terminal.h tests/sndtst.c \
        include/services.h linux/services.c Makefile
	$(CC) $(CFLAGS) tests/sndtst.c linux/option.c $(LIBS) -o bin/sndtst 
	
event: include/terminal.h tests/event.c Makefile
	$(CC) $(CFLAGS) tests/event.c $(LIBS) -o bin/event
	
getkeys: linux/getkeys.c Makefile
	$(CC) $(CFLAGS) linux/getkeys.c -lm -o bin/getkeys
	
getmouse: linux/getmouse.c Makefile
	$(CC) $(CFLAGS) linux/getmouse.c -lm -o bin/getmouse
	
term: include/terminal.h tests/term.c Makefile
	$(CC) $(CFLAGS) tests/term.c $(LIBS) -o bin/term
	
snake: terminal_games/snake.c include/terminal.h Makefile
	$(CC) $(CFLAGS) terminal_games/snake.c $(LIBS) -o bin/snake
	
mine: terminal_games/mine.c include/terminal.h Makefile
	$(CC) $(CFLAGS) terminal_games/mine.c $(LIBS) -o bin/mine
	
editor: terminal_programs/editor.c include/terminal.h Makefile
	$(CC) $(CFLAGS) terminal_programs/editor.c $(LIBS) -o bin/editor
	
getpage: bin/petit_ami_plain.so network_programs/getpage.c Makefile
	$(CC) $(CFLAGS) network_programs/getpage.c linux/option.c $(PLIBS) -o bin/getpage

getmail: bin/petit_ami_plain.so network_programs/getmail.c Makefile
	$(CC) $(CFLAGS) network_programs/getmail.c linux/option.c $(PLIBS) -o bin/getmail
	
gettys: bin/petit_ami_plain.so network_programs/gettys.c Makefile
	$(CC) $(CFLAGS) network_programs/gettys.c linux/option.c $(PLIBS) -o bin/gettys
	
msgclient: bin/petit_ami_plain.so network_programs/msgclient.c Makefile
	$(CC) $(CFLAGS) network_programs/msgclient.c $(PLIBS) -o bin/msgclient
	
clean:
	rm -f bin/dumpmidi bin/lsalsadev bin/alsaparms test bin/play bin/keyboard 
	rm -f bin/playmidi bin/playwave bin/printdev bin/connectmidi bin/connectwave 
	rm -f bin/random bin/genwave bin/scntst bin/sndtst bin/event bin/getkeys 
	rm -f bin/getmouse bin/term bin/snake bin/mine bin/editor bin/getpage
	rm -f bin/getmail bin/gettys
	rm -f linux/*.o
	rm -f bin/petit_ami_term.a
	rm -f bin/petit_ami_term.so
	rm -f bin/petit_ami_plain.a
	rm -f bin/petit_ami_plain.so
