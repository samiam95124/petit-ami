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
    endif
endif

#
# Link image statically or dynamically?
#
ifndef LINK_TYPE

    ifeq ($(OS),Windows_NT)
    
        #
        # Windows
        #
        # Windows is always static. .dlls are added during load time.
        #
        LINK_TYPE=static

    else
    
        #
        # Linux 
        #
        ifeq ($(STDIO_SOURCE),stdio)
            # Linking local stdio, must be dynamic
            LINK_TYPE=dynamic
        else
            # LINK_TYPE=static
            LINK_TYPE=dynamic
        endif
        
    endif
    
endif

CC=gcc
CFLAGS=-g3 -Wl,--rpath=bin -Iinclude

#
# Set library dependencies
#
ifeq ($(LINK_TYPE),static)
    LIBEXT = .a
else
    LIBEXT = .so
endif

#
# Select where stdio.h comes from
#
ifeq ($(STDIO_SOURCE),stdio)
    #
    # In local link, we need to get stdio.h from local directory
    #
    CFLAGS +=-Ilibc
endif

#
# modify compile flags for static operation
#
ifeq ($(LINK_TYPE),static)
    CFLAGS += -static
endif

#
# Specify object file for libc
#
ifneq ($(OS),Windows_NT)

    #
    # Linux, use modified GLIBC
    #
    
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
    
endif

#
# Collected libraries
#

#
# Plain (no terminal handler)
# This option exists to drop the terminal handler, which should not be
# required for most code.
#
# Note that this is more important for Linux than Windows, because Windows
# console is "transparent", or unchanging depending on mode.
#
# Note there is no statically linked sound at the moment, since we don't have
# an absolute version of fluidsynth.
#
# The libraries are wrapped as defines as:
#
# LIBS		Full Petit-Ami libraries including console or graphics.
# PLIBS     Petit-Ami libraries without console or graphics.
#
ifeq ($(LINK_TYPE),static)
    PLIBS += bin/petit_ami_plain.a
else
    PLIBS += bin/petit_ami_plain.so
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
        LIBS += bin/petit_ami_term.so
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

#
# add external packages
#
ifeq ($(OS),Windows_NT)

    #
    # Windows
    #
    LIBS += -lwinmm -lgdi32
    
else

    #
    # Linux
    #
    LIBS += -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
    PLIBS += -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
    
endif

#
# Make all executables
#        
ifeq ($(OS),Windows_NT)

#
# Windows
#
all: dumpmidi test play keyboard playmidi playwave \
    printdev connectmidi connectwave random genwave scntst sndtst svstst \
    event term snake pong mine wator editor getpage getmail gettys
    
else

#
# Linux
#
all: lsalsadev alsaparms dumpmidi test play keyboard playmidi playwave \
    printdev connectmidi connectwave random genwave scntst sndtst svstst \
    event getkeys getmouse term snake mine pong wator editor getpage getmail \
    gettys
    
endif 

#
# Individual Petit-Ami library components
#

#
# Linux target components
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
# Windows target components
#
# Note that stub sources are not yet implemented
#
libc/stdio.o: libc/stdio.c libc/stdio.h
	gcc -g3 -Ilibc -c libc/stdio.c -o libc/stdio.o
	
windows/services.o: windows/services.c include/services.h
	gcc -g3 -Iinclude -c windows/services.c -o windows/services.o
	
windows/sound.o: stub/sound.c include/sound.h
	gcc -g3 -Iinclude -c stub/sound.c -o windows/sound.o
	
windows/network.o: stub/network.c include/network.h
	gcc -g3 -Iinclude -c stub/network.c -o windows/network.o
	
windows/console.o: windows/console.c include/terminal.h
	gcc -g3 -Iinclude -c windows/console.c -o windows/console.o
	
windows/graph.o: stub/graph.c include/graph.h
	gcc -g3 -Iinclude -c stub/graph.c -o windows/graph.o

#
# Create terminal mode and graphical mode libraries
#

ifeq ($(OS),Windows_NT)

#
# Windows
#
# Windows cannot use .so files, but rather uses statically linked files that
# reference .dlls at runtime.
#
bin/petit_ami_plain.a: windows/services.o windows/sound.o windows/network.o libc/stdio.o
	ar rcs bin/petit_ami_plain.a windows/services.o windows/sound.o \
        windows/network.o libc/stdio.o
	
bin/petit_ami_term.a: windows/services.o windows/sound.o windows/network.o \
    windows/console.o libc/stdio.o
	ar rcs bin/petit_ami_term.a windows/services.o windows/sound.o \
	    windows/network.o windows/console.o libc/stdio.o
	
petit_ami_graph.a: windows/services.o windows/sound.o windows/network.o \
    windows/graph.o libc/stdio.o
	ar rcs bin/petit_ami_graph.a windows/services.o windows/sound.o \
	    windows/network.o windows/graph.o libc/stdio.o
	
else

#
# Linux
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
	
endif

#
# Make individual executables
#	

#
# Linux specific tools
#
# These will be removed as things settle down.
#	
lsalsadev: linux/lsalsadev.c Makefile
	gcc linux/lsalsadev.c -lasound -o bin/lsalsadev
	
alsaparms: linux/alsaparms.c Makefile
	gcc linux/alsaparms.c -lasound -o bin/alsaparms
	
getkeys: linux/getkeys.c Makefile
	$(CC) $(CFLAGS) linux/getkeys.c -lm -o bin/getkeys
	
getmouse: linux/getmouse.c Makefile
	$(CC) $(CFLAGS) linux/getmouse.c -lm -o bin/getmouse

#
# Cross system tools
#	
dumpmidi: utils/dumpmidi.c Makefile
	gcc utils/dumpmidi.c -o bin/dumpmidi

test: bin/petit_ami_term$(LIBEXT) include/terminal.h test.c Makefile
	$(CC) $(CFLAGS) test.c $(LIBS) -o test
	
play: bin/petit_ami_term$(LIBEXT) include/terminal.h sound_programs/play.c Makefile
	$(CC) $(CFLAGS) sound_programs/play.c utils/option.c $(LIBS) -o bin/play
	
keyboard: bin/petit_ami_term$(LIBEXT) include/terminal.h sound_programs/keyboard.c Makefile
	$(CC) $(CFLAGS) sound_programs/keyboard.c utils/option.c $(LIBS) -o bin/keyboard
	
playmidi: bin/petit_ami_plain$(LIBEXT) include/terminal.h sound_programs/playmidi.c utils/option.c Makefile
	$(CC) $(CFLAGS) sound_programs/playmidi.c utils/option.c $(PLIBS) -o bin/playmidi

playwave: bin/petit_ami_plain$(LIBEXT) include/terminal.h sound_programs/playwave.c Makefile
	$(CC) $(CFLAGS) sound_programs/playwave.c utils/option.c $(PLIBS) -o bin/playwave
	
printdev: bin/petit_ami_plain$(LIBEXT) include/terminal.h sound_programs/printdev.c Makefile
	$(CC) $(CFLAGS) sound_programs/printdev.c $(PLIBS) -o bin/printdev

connectmidi: bin/petit_ami_plain$(LIBEXT) include/terminal.h sound_programs/connectmidi.c Makefile
	$(CC) $(CFLAGS) sound_programs/connectmidi.c $(PLIBS) -o bin/connectmidi
	
connectwave: bin/petit_ami_plain$(LIBEXT) include/terminal.h sound_programs/connectwave.c Makefile
	$(CC) $(CFLAGS) sound_programs/connectwave.c $(PLIBS) -o bin/connectwave
	
random: bin/petit_ami_term$(LIBEXT) include/terminal.h sound_programs/random.c Makefile
	$(CC) $(CFLAGS) sound_programs/random.c utils/option.c $(LIBS) -o bin/random
	
genwave: bin/petit_ami_plain$(LIBEXT) include/terminal.h sound_programs/genwave.c Makefile
	$(CC) $(CFLAGS) sound_programs/genwave.c utils/option.c $(PLIBS) -o bin/genwave
	
scntst: bin/petit_ami_term$(LIBEXT) include/terminal.h tests/scntst.c include/services.h linux/services.c Makefile
	$(CC) $(CFLAGS) tests/scntst.c $(LIBS) -o bin/scntst 
	
sndtst: bin/petit_ami_term$(LIBEXT) include/terminal.h tests/sndtst.c \
        include/services.h linux/services.c Makefile
	$(CC) $(CFLAGS) tests/sndtst.c utils/option.c $(LIBS) -o bin/sndtst 
	
svstst: bin/petit_ami_plain$(LIBEXT) include/terminal.h tests/svstst.c \
        include/services.h linux/services.c Makefile
	$(CC) $(CFLAGS) tests/svstst.c utils/option.c $(PLIBS) -o bin/svstst
	$(CC) $(CFLAGS) tests/svstst1.c $(PLIBS) -o bin/svstst1
	
event: include/terminal.h tests/event.c Makefile
	$(CC) $(CFLAGS) tests/event.c $(LIBS) -o bin/event
	
term: bin/petit_ami_term$(LIBEXT) include/terminal.h tests/term.c Makefile
	$(CC) $(CFLAGS) tests/term.c $(LIBS) -o bin/term
	
snake: bin/petit_ami_term$(LIBEXT) terminal_games/snake.c include/terminal.h Makefile
	$(CC) $(CFLAGS) terminal_games/snake.c $(LIBS) -o bin/snake
	
mine: bin/petit_ami_term$(LIBEXT) terminal_games/mine.c include/terminal.h Makefile
	$(CC) $(CFLAGS) terminal_games/mine.c $(LIBS) -o bin/mine
	
wator: bin/petit_ami_term$(LIBEXT) terminal_programs/wator.c include/terminal.h Makefile
	$(CC) $(CFLAGS) terminal_programs/wator.c $(LIBS) -o bin/wator
	
pong: bin/petit_ami_term$(LIBEXT) terminal_games/pong.c include/terminal.h Makefile
	$(CC) $(CFLAGS) terminal_games/pong.c $(LIBS) -o bin/pong
	
editor: terminal_programs/editor.c include/terminal.h Makefile
	$(CC) $(CFLAGS) terminal_programs/editor.c $(LIBS) -o bin/editor
	
getpage: bin/petit_ami_plain$(LIBEXT) network_programs/getpage.c Makefile
	$(CC) $(CFLAGS) network_programs/getpage.c utils/option.c $(PLIBS) -o bin/getpage

getmail: bin/petit_ami_plain$(LIBEXT) network_programs/getmail.c Makefile
	$(CC) $(CFLAGS) network_programs/getmail.c utils/option.c $(PLIBS) -o bin/getmail
	
gettys: bin/petit_ami_plain$(LIBEXT) network_programs/gettys.c Makefile
	$(CC) $(CFLAGS) network_programs/gettys.c utils/option.c $(PLIBS) -o bin/gettys
	
msgclient: bin/petit_ami_plain$(LIBEXT) network_programs/msgclient.c Makefile
	$(CC) $(CFLAGS) network_programs/msgclient.c utils/option.c $(PLIBS) -o bin/msgclient
	
msgserver: bin/petit_ami_plain$(LIBEXT) network_programs/msgserver.c Makefile
	$(CC) $(CFLAGS) network_programs/msgserver.c utils/option.c $(PLIBS) -o bin/msgserver
	
prtcertnet: bin/petit_ami_plain$(LIBEXT) network_programs/prtcertnet.c Makefile
	$(CC) $(CFLAGS) network_programs/prtcertnet.c $(PLIBS) -o bin/prtcertnet
	
prtcertmsg: bin/petit_ami_plain$(LIBEXT) network_programs/prtcertmsg.c Makefile
	$(CC) $(CFLAGS) network_programs/prtcertmsg.c $(PLIBS) -o bin/prtcertmsg
	
listcertnet: bin/petit_ami_plain$(LIBEXT) network_programs/listcertnet.c Makefile
	$(CC) $(CFLAGS) network_programs/listcertnet.c $(PLIBS) -o bin/listcertnet
	
clean:
	rm -f bin/dumpmidi bin/lsalsadev bin/alsaparms test bin/play bin/keyboard 
	rm -f bin/playmidi bin/playwave bin/printdev bin/connectmidi bin/connectwave 
	rm -f bin/random bin/genwave bin/scntst bin/sndtst bin/event bin/getkeys 
	rm -f bin/getmouse bin/term bin/snake bin/mine bin/editor bin/getpage
	rm -f bin/getmail bin/gettys bin/msgclient bin/msgserver bin/prtcertnet
	rm -f bin/listcertnet
	rm -f bin/prtcertmsg
	find . -name "*.o" -type f -delete
	rm -f bin/*.a
	rm -f bin/*.so
	rm -f bin/*.exe
