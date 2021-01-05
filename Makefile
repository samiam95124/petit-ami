################################################################################
#                                                                              #
#               Makefile for Petit Ami and associated programs                 #
#                                                                              #
################################################################################
#
# Structure of makefile
#
# There are six sections to the makefile:
#
# 1. Establishing macros to make products according to OS and type of build.
#
# 2. Building individual components.
#
# 3. Build libraries.
#
# 4. Building target programs (tests, demos, utilities, etc.)
#
# 5. Define build targets.
#
# 6. Define clean targets.
#
# The macros that are constructed are:
#
# CC		Contains the compiler definition in use.
#
# CFLAGS	Contains the compiler/linker flags to build programs.
#
# PLIBS		Contains the libraries and flags required to build plain (no display
#           model) programs.
# 
# CLIBS		Contains the libraries and flags required to build console model
#           programs.
#
# GLIBS		Contains the libraries and flags required to build graphical model
#           programs.
#
# PLIBSD	Contains dependencies only for plain (no display model) programs.
#
# CLIBSD    Contains dependencies only for console model programs.
#
# GLIBSD    Contains dependencies only for graphical model programs.
#
# The depenencies contain the collected library in use (.so or .a) with the
# correct extension.
#
# The ending of the program name gives the display model the program was 
# constructed for, one of:
#
# <none>	No display model, ie., serial console.
#
# c			Console model.
#
# g			Graphical model.
#
# In all cases, the same source file is used to generate all products, if 
# multiple output products exist (indeed, that is the point of Petit-Ami).
# Not all products can be made from a given source file. For example, a 
# graphical program cannot be also ported to console or serial mode.
#
# Note that the "c" or "console" postfix is often dropped, since only the "g"
# or "graphical" ending is necessary to differentiate the products.
#
# Note that "g" or "graphical" may denote a console program that can be compiled
# in either console or graphical mode, or it may denote a program specifically
# designed for graphical mode.
#
# Note that most of the complexity of this make is to create the libraries for
# specific implementations. There are sample makefiles elsewhere that are much
# simpler, because they use prebuilt libraries. Please see those examples.
#

################################################################################
#
# Establish build macros
#
################################################################################

#
# Set OSTYPE according to operating system
#
# If the OS environment variable is set, we use it, otherwise we assume it is
# A Unix variant and has uname. This works for linux and Mac OS X.
#
ifeq ($(OS),Windows_NT)
    OSTYPE=Windows_NT
else
	OSTYPE=$(shell uname)
endif

#
# Where do stdio definitions and overrides come from?
#
ifndef STDIO_SOURCE
    ifeq ($(OSTYPE),Windows_NT)
        #
        # stdio is a local stdio implementation with overrides.
        # Note it is not a complete libc. It works because in dynamic linking a module
        # can effectively replace a later module of the same names. It does not work
        # for static linking or if other modules in chain bypass posix standards.
        # 
        # Windows builds must use stdio since they don't use glibc.
        #
        STDIO_SOURCE=stdio
    else ifeq ($(OSTYPE),Darwn)
        #
        # Mac OS x builds must use stdio since they don't use glibc.
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

    ifeq ($(OSTYPE),Windows_NT)
    
        #
        # Windows
        #
        # Windows is always static. .dlls are added during load time.
        #
        LINK_TYPE=static

    else ifeq ($(OSTYPE),Darwin)
    
        #
        # Mac OS X is static
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
CFLAGS=-g3 -Iinclude

#
# Add flags by OS
#
ifeq ($(OSTYPE),Windows_NT)

	# Windows, nothing
	
else ifeq ($(OSTYPE),Darwin)

    # Mac OS X, nothing
    
else

    #
    # Linux
    #
    CFLAGS+=-Wl,--rpath=bin
    
endif

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

    ifeq ($(OSTYPE),Windows_NT)

	    # Windows
	    CFLAGS += -static
	
    else ifeq ($(OSTYPE),Darwin)

        # Mac OS X, nothing
    
    else

        # Linux
        CFLAGS += -static
        
    endif
    
endif

#
# Specify object file for libc
#
ifeq ($(OSTYPE),Windows_NT)

	# Nothing, libc is linked in overall lib
	
else ifeq ($(OSTYPE),Darwin)

    # Nothing, libc is linked in overall lib
    
else

    #
    # Linux, use modified GLIBC
    #
    
    #
    # For modified GLIBC, we need to specify the libary first or it won't
    # link correctly.
    #
    ifeq ($(LINK_TYPE),static)
        PLIBS = -Wl,--whole-archive bin/libc.a -Wl,--no-whole-archive
        CLIBS = -Wl,--whole-archive bin/libc.a -Wl,--no-whole-archive
        GLIBS = -Wl,--whole-archive bin/libc.a -Wl,--no-whole-archive
        PLIBSD = bin/libc.a
        CLIBSD = bin/libc.a
        GLIBSD = bin/libc.a
    else
	    PLIBS = bin/libc.so.6
	    CLIBS = bin/libc.so.6
	    GLIBS = bin/libc.so.6
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
    PLIBS += -Wl,--whole-archive bin/petit_ami_plain.a -Wl,--no-whole-archive
else
    PLIBS += bin/petit_ami_plain.so
endif

#
# terminal handler libraries
#

#
# Terminal model API
#
ifeq ($(LINK_TYPE),static)
    CLIBS += -Wl,--whole-archive bin/petit_ami_term.a -Wl,--no-whole-archive
else
    CLIBS += bin/petit_ami_term.so
endif

#
# Graphical model API
#
ifeq ($(LINK_TYPE),static)
	GLIBS += -Wl,--whole-archive bin/petit_ami_graph.a -Wl,--no-whole-archive
else
    GLIBS += bin/petit_ami_graph.so
endif

#
# add external packages
#
ifeq ($(OSTYPE),Windows_NT)

    #
    # Windows
    #
    CLIBS += -lwinmm -lgdi32
    GLIBS += -lwinmm -lgdi32 -lcomdlg32

else ifeq ($(OSTYPE),Darwin)

    #
    # Nothing needed for Mac OS X
    #
    
else

    #
    # Linux
    #
	PLIBS += -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
    GLIBS += -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
    CLIBS += -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
    
endif

#
# Create dependency macros
#
PLIBSD = bin/petit_ami_plain$(LIBEXT)
CLIBSD = bin/petit_ami_term$(LIBEXT)
GLIBSD = bin/petit_ami_graph$(LIBEXT)

################################################################################
#
# Build individual components
#
################################################################################

#
# Linux library components
#
linux/services.o: linux/services.c include/services.h Makefile
	gcc -g3 -Iinclude -fPIC -c linux/services.c -o linux/services.o
	
linux/sound.o: linux/sound.c include/sound.h Makefile
	gcc -g3 -Iinclude -fPIC -c linux/sound.c -lasound -lm -pthread -o linux/sound.o
	
linux/network.o: linux/network.c include/network.h Makefile
	gcc -g3 -Iinclude -fPIC -c linux/network.c -o linux/network.o
	
linux/fluidsynthplug.o: linux/fluidsynthplug.c include/sound.h Makefile
	gcc -g3 -Iinclude -fPIC -c linux/fluidsynthplug.c -lasound -lm -pthread -o linux/fluidsynthplug.o
	
linux/dumpsynthplug.o: linux/dumpsynthplug.c include/sound.h Makefile
	gcc -g3 -Iinclude -fPIC -c linux/dumpsynthplug.c -lasound -lm -pthread -o linux/dumpsynthplug.o
	
linux/xterm.o: linux/xterm.c include/terminal.h Makefile
	gcc -g3 -Iinclude -fPIC -c linux/xterm.c -o linux/xterm.o
	
linux/graph_x.o: linux/graph_x.c include/graph.h Makefile
	gcc -g3 -Iinclude -fPIC -c linux/graph_x.c -o linux/graph_x.o
	
#
# Windows library components
#
# Note that stub sources are not yet implemented
#
windows/stdio.o: libc/stdio.c libc/stdio.h Makefile
	gcc -g3 -Ilibc -c libc/stdio.c -o windows/stdio.o
	
windows/services.o: windows/services.c include/services.h Makefile
	gcc -g3 -Ilibc -Iinclude -c windows/services.c -o windows/services.o
	
windows/sound.o: stub/sound.c include/sound.h Makefile
	gcc -g3 -Ilibc -Iinclude -c stub/sound.c -o windows/sound.o
	
windows/network.o: stub/network.c include/network.h Makefile
	gcc -g3 -Ilibc -Iinclude -c stub/network.c -o windows/network.o
	
windows/console.o: windows/console.c include/terminal.h Makefile
	gcc -g3 -Ilibc -Iinclude -c windows/console.c -o windows/console.o
	
windows/graph.o: windows/graph.c include/graph.h Makefile
	gcc -g3 -Ilibc -Iinclude -c windows/graph.c -o windows/graph.o

#
# Mac OS X library components
#
# Note that stub sources are not yet implemented.
#
# Mac OS X can use some of the same components as Linux.
#
macosx/stdio.o: libc/stdio.c libc/stdio.h Makefile
	gcc -g3 -Ilibc -c libc/stdio.c -o macosx/stdio.o
	
macosx/services.o: linux/services.c include/services.h Makefile
	gcc -g3 -Ilibc -Iinclude -c linux/services.c -o macosx/services.o
	
macosx/sound.o: stub/sound.c include/sound.h Makefile
	gcc -g3 -Ilibc -Iinclude -c stub/sound.c -o macosx/sound.o
	
macosx/network.o: stub/network.c include/network.h Makefile
	gcc -g3 -Ilibc -Iinclude -c stub/network.c -o macosx/network.o
	
macosx/xterm.o: linux/xterm.c include/terminal.h Makefile
	gcc -g3 -Ilibc -Iinclude -c linux/xterm.c -o macosx/xterm.o
	
macosx/graph.o: stub/graph.c include/graph.h Makefile
	gcc -g3 -Ilibc -Iinclude -c stub/graph.c -o macosx/graph.o
	
#
# Components in common to all systems
#
utils/config.o: utils/config.c include/localdefs.h include/services.h \
	            include/config.h Makefile
	gcc -g3 -Iinclude -c utils/config.c -o utils/config.o
	
utils/option.o: utils/option.c include/localdefs.h include/services.h \
	            include/option.h Makefile
	gcc -g3 -Iinclude -c utils/option.c -o utils/option.o
	
################################################################################
#
# Build libraries
#
################################################################################

#
# Create terminal mode and graphical mode libraries
#

ifeq ($(OSTYPE),Windows_NT)

#
# Windows
#
# Windows cannot use .so files, but rather uses statically linked files that
# reference .dlls at runtime.
#
bin/petit_ami_plain.a: windows/services.o windows/sound.o windows/network.o \
	utils/option.o utils/config.o windows/stdio.o
	ar rcs bin/petit_ami_plain.a windows/services.o windows/sound.o \
        windows/network.o utils/config.o utils/option.o windows/stdio.o
	
bin/petit_ami_term.a: windows/services.o windows/sound.o windows/network.o \
    windows/console.o utils/config.o utils/option.o windows/stdio.o
	ar rcs bin/petit_ami_term.a windows/services.o windows/sound.o \
	    windows/network.o windows/console.o utils/config.o utils/option.o \
	    windows/stdio.o
	
bin/petit_ami_graph.a: windows/services.o windows/sound.o windows/network.o \
    windows/graph.o utils/config.o utils/option.o windows/stdio.o
	ar rcs bin/petit_ami_graph.a windows/services.o windows/sound.o \
	    windows/network.o windows/graph.o utils/config.o utils/option.o \
	    windows/stdio.o
	
else ifeq ($(OSTYPE),Darwin)

#
# Mac OS X
#
# Mac OS X cannot use .so files, but rather uses statically linked files.
#
bin/petit_ami_plain.a: macosx/services.o macosx/sound.o macosx/network.o 
	utils/config.o utils/option.o macosx/stdio.o
	ar rcs bin/petit_ami_plain.a macosx/services.o macosx/sound.o \
        macosx/network.o utils/config.o utils/option.o macosx/stdio.o
	
bin/petit_ami_term.a: macosx/services.o macosx/sound.o macosx/network.o \
    macosx/xterm.o utils/config.o utils/option.o macosx/stdio.o
	ar rcs bin/petit_ami_term.a macosx/services.o macosx/sound.o \
	    macosx/network.o macosx/xterm.o utils/config.o utils/option.o \
	    macosx/stdio.o
	
petit_ami_graph.a: macosx/services.o macosx/sound.o macosx/network.o \
    macosx/graph.o utils/config.o utils/option.o macosx/stdio.o
	ar rcs bin/petit_ami_graph.a macosx/services.o macosx/sound.o \
	    macosx/network.o macosx/graph.o utils/config.o utils/option.o \
	    macosx/stdio.o
	    
else

#
# Linux
#

#
# Note that sound lib cannot be put into an .so, there is a bug in ALSA.
# Thus we leave it as a .o file.
#
# The linux build uses fluidsynth, and uses a series of runtime plug-ins
# to do things like midi to wave conversion.
#
bin/petit_ami_plain.so: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o utils/config.o utils/option.o
	gcc -shared linux/services.o linux/sound.o linux/fluidsynthplug.o \
	    linux/dumpsynthplug.o linux/network.o utils/config.o utils/option.o \
	    -o bin/petit_ami_plain.so
	
bin/petit_ami_plain.a: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o utils/config.o utils/option.o
	ar rcs bin/petit_ami_plain.a linux/services.o linux/sound.o \
	    linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
	    utils/config.o utils/option.o
	
bin/petit_ami_term.so: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/xterm.o utils/config.o \
    utils/option.o 
	gcc -shared linux/services.o linux/sound.o linux/fluidsynthplug.o \
	    linux/dumpsynthplug.o  linux/network.o linux/xterm.o utils/config.o \
	    utils/option.o -o bin/petit_ami_term.so 
	
bin/petit_ami_term.a: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/xterm.o utils/config.o \
    utils/option.o
	ar rcs bin/petit_ami_term.a linux/services.o linux/sound.o \
		linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
		linux/xterm.o utils/config.o utils/option.o 
	
petit_ami_graph.so: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/graph_x.o utils/config.o \
    utils/option.o  
	gcc linux/services.o linux/sound.o linux/fluidsynthplug.o \
		linux/dumpsynthplug.o  linux/network.o linux/xterm.o utils/config.o \
		utils/option.o -o bin/petit_ami_graph.so
	
petit_ami_graph.a: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/graph_x.o utils/config.o \
    utils/option.o  
	ar rcs bin/petit_ami_graph.a linux/services.o linux/sound.o \
		linux/fluidsynthplug.o linux/dumpsynthplug.o  linux/network.o \
		linux/xterm.o utils/config.o utils/option.o 
	
endif

################################################################################
#
# Build final programs
#
################################################################################

#
# Linux specific tools
#
# These will be removed as things settle down. These were/are used to model
# Linux specific code and solve problems specific to Linux.
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
# Cross system tools - These work on any Petit-Ami compliant environment
#	

#
# Dump midi file in readable format (not Petit-Ami dependent)
#
dumpmidi: utils/dumpmidi.c Makefile
	gcc utils/dumpmidi.c -o bin/dumpmidi

#
# General test program
#
# If the build is not appropriate to the test program, you can either ignore
# the errors or comment this out. The test target will move out of the makefile
# in time.
#
test: $(PLIBSD) test.c
	$(CC) $(CFLAGS) test.c $(PLIBS) -o test
	
testc: $(CLIBSD) test.c
	$(CC) $(CFLAGS) test.c $(CLIBS) -o testc
	
testg: $(GLIBSD) test.c
	$(CC) $(CFLAGS) test.c $(GLIBS) -o testg
	
#
# Target programs that use Petit-Ami, such as games, utilities, etc.
# "dazzler" programs is my term. It comes from the Cromemco Dazzler, a graphics
# that ran on early S100 computers, and was a popular display demonstration from
# those days.
#

#
# Play example songs in QBasic play format (uses console timers)
# 	
play: $(CLIBSD) sound_programs/play.c
	$(CC) $(CFLAGS) sound_programs/play.c $(CLIBS) -o bin/play

#
# Emulate a sound keyboard (uses console timers)
#	
keyboard: $(CLIBSD) sound_programs/keyboard.c
	$(CC) $(CFLAGS) sound_programs/keyboard.c $(CLIBS) -o bin/keyboard

#
# Play midi files
#	
playmidi: $(PLIBSD) sound_programs/playmidi.c
	$(CC) $(CFLAGS) sound_programs/playmidi.c $(PLIBS) -o bin/playmidi

#
# Play wave files
#
playwave: $(PLIBSD) sound_programs/playwave.c
	$(CC) $(CFLAGS) sound_programs/playwave.c $(PLIBS) -o bin/playwave

#
# Print a list of available sound devices
#	
printdev: $(PLIBSD) sound_programs/printdev.c
	$(CC) $(CFLAGS) sound_programs/printdev.c $(PLIBS) -o bin/printdev

#
# Connect Midi input port to Midi output port
#
connectmidi: $(PLIBSD) sound_programs/connectmidi.c
	$(CC) $(CFLAGS) sound_programs/connectmidi.c $(PLIBS) -o bin/connectmidi

#
# Connect wave input port to wave output port
#	
connectwave: $(PLIBSD) sound_programs/connectwave.c
	$(CC) $(CFLAGS) sound_programs/connectwave.c $(PLIBS) -o bin/connectwave

#	
# Play random notes
#
randomc: $(CLIBSD) sound_programs/random.c
	$(CC) $(CFLAGS) sound_programs/random.c $(CLIBS) -o bin/random

#
# Generate waveforms
#	
genwave: $(PLIBSD) sound_programs/genwave.c
	$(CC) $(CFLAGS) sound_programs/genwave.c $(PLIBS) -o bin/genwave

#
# Test console model compliant output
#	
scntst: $(CLIBSD) tests/scntst.c
	$(CC) $(CFLAGS) tests/scntst.c $(CLIBS) -o bin/scntst

scntstg: $(GLIBSD) tests/scntst.c
	$(CC) $(CFLAGS) tests/scntst.c $(GLIBS) -o bin/scntstg
	
#
# Test graph model compliant output
#
gratst: $(GLIBSD) tests/gratst.c
	$(CC) $(CFLAGS) tests/gratst.c $(GLIBS) -o bin/gratst 
	
#
# Test windows management model compliant output
#
mantst: $(GLIBSD) tests/mantst.c
	$(CC) $(CFLAGS) tests/mantst.c $(GLIBS) -o bin/mantst 

#
# Test sound model compliant input/output (uses console timers)
#	
sndtst: $(CLIBSD) tests/sndtst.c
	$(CC) $(CFLAGS) tests/sndtst.c $(CLIBS) -o bin/sndtst 
	
#
# Test services module
#
svstst: $(PLIBSD) tests/svstst.c
	$(CC) $(CFLAGS) tests/svstst.c utils/option.c $(PLIBS) -o bin/svstst
	$(CC) $(CFLAGS) tests/svstst1.c $(PLIBS) -o bin/svstst1

#
# Test event model (console and graph mode)
#	
event: $(CLIBSD) tests/event.c
	$(CC) $(CFLAGS) tests/event.c $(CLIBS) -o bin/event
	
eventg: $(GLIBSD) tests/event.c
	$(CC) $(CFLAGS) tests/event.c $(GLIBS) -o bin/eventg

#
# Test terminal characteristics (console and graph mode)
#	
term: $(GLIBSD) tests/term.c
	$(CC) $(CFLAGS) tests/term.c $(CLIBS) -o bin/term
	
termg: $(GLIBSD) tests/term.c
	$(CC) $(CFLAGS) tests/term.c $(GLIBS) -o bin/termg
	
#
# Snake game
#	
snake: $(CLIBSD) terminal_games/snake.c
	$(CC) $(CFLAGS) terminal_games/snake.c $(CLIBS) -o bin/snake
	
snakeg: $(GLIBSD) terminal_games/snake.c
	$(CC) $(CFLAGS) terminal_games/snake.c $(GLIBS) -o bin/snakeg

#
# Mine game
#	
mine: $(CLIBSD) terminal_games/mine.c
	$(CC) $(CFLAGS) terminal_games/mine.c $(CLIBS) -o bin/mine
	
mineg: $(GLIBSD) terminal_games/mine.c
	$(CC) $(CFLAGS) terminal_games/mine.c $(GLIBS) -o bin/mineg

#
# Wator game/dazzler
#	
wator: $(CLIBSD) terminal_programs/wator.c
	$(CC) $(CFLAGS) terminal_programs/wator.c $(CLIBS) -o bin/wator
	
watorg: $(GLIBSD) terminal_programs/wator.c
	$(CC) $(CFLAGS) terminal_programs/wator.c $(GLIBS) -o bin/watorg

#
# Pong game
#	
pong: $(CLIBSD) terminal_games/pong.c
	$(CC) $(CFLAGS) terminal_games/pong.c $(CLIBS) -o bin/pong
	
pongg: $(GLIBSD) terminal_games/pong.c
	$(CC) $(CFLAGS) terminal_games/pong.c $(GLIBS) -o bin/pongg

#
# Text editor
#	
editor: $(CLIBSD) terminal_programs/editor.c
	$(CC) $(CFLAGS) terminal_programs/editor.c $(CLIBS) -o bin/editor
	
editorg: $(GLIBSD) terminal_programs/editor.c
	$(CC) $(CFLAGS) terminal_programs/editor.c $(GLIBS) -o bin/editorg

#
# Get html/https page
#	
getpage: $(PLIBSD) network_programs/getpage.c
	$(CC) $(CFLAGS) network_programs/getpage.c $(PLIBS) -o bin/getpage

#
# Get remote email
#
getmail: $(PLIBSD) network_programs/getmail.c
	$(CC) $(CFLAGS) network_programs/getmail.c $(PLIBS) -o bin/getmail

#
# Gettysberg address server
#	
gettys: $(PLIBSD) network_programs/gettys.c
	$(CC) $(CFLAGS) network_programs/gettys.c $(PLIBS) -o bin/gettys

#
# Message based networking test client
#	
msgclient: $(PLIBSD) network_programs/msgclient.c
	$(CC) $(CFLAGS) network_programs/msgclient.c $(PLIBS) -o bin/msgclient

#
# Message based networking test server
#	
msgserver: $(PLIBSD) network_programs/msgserver.c
	$(CC) $(CFLAGS) network_programs/msgserver.c $(PLIBS) -o bin/msgserver
	
#
# Print TCPIP/TLS certificates
#
prtcertnet: $(PLIBSD) network_programs/prtcertnet.c
	$(CC) $(CFLAGS) network_programs/prtcertnet.c $(PLIBS) -o bin/prtcertnet

#
# Print message based certificates
#	
prtcertmsg: $(PLIBSD) network_programs/prtcertmsg.c
	$(CC) $(CFLAGS) network_programs/prtcertmsg.c $(PLIBS) -o bin/prtcertmsg

#
# This program is missing???? check the linux machine.
#	
listcertnet: $(PLIBSD) network_programs/listcertnet.c
	$(CC) $(CFLAGS) network_programs/listcertnet.c $(PLIBS) -o bin/listcertnet

#
# Pixel set/reset dazzler
#
pixel: $(GLIBSD) graph_programs/pixel.c
	$(CC) $(CFLAGS) graph_programs/pixel.c $(GLIBS) -o bin/pixel

#
# Moving balls dazzlers
#	
ball1: $(GLIBSD) graph_programs/ball1.c
	$(CC) $(CFLAGS) graph_programs/ball1.c $(GLIBS) -o bin/ball1
	
ball2: $(GLIBSD) graph_programs/ball2.c
	$(CC) $(CFLAGS) graph_programs/ball2.c $(GLIBS) -o bin/ball2
	
#
# Moving lines dazzlers
#
line1: $(GLIBSD) graph_programs/line1.c
	$(CC) $(CFLAGS) graph_programs/line1.c $(GLIBS) -o bin/line1

line2: $(GLIBSD) graph_programs/line2.c
	$(CC) $(CFLAGS) graph_programs/line2.c $(GLIBS) -o bin/line2
	
line4: $(GLIBSD) graph_programs/line4.c
	$(CC) $(CFLAGS) graph_programs/line4.c $(GLIBS) -o bin/line4
	
line5: $(GLIBSD) graph_programs/line5.c
	$(CC) $(CFLAGS) graph_programs/line5.c $(GLIBS) -o bin/line5
	
################################################################################
#
# Build targets
#
################################################################################

#
# Make all executables
#        
ifeq ($(OSTYPE),Windows_NT)

#
# Windows
#
all: dumpmidi test play keyboard playmidi playwave \
    printdev connectmidi connectwave random genwave scntst sndtst svstst \
    event term snake pong mine wator editor getpage getmail gettys gratst \
    mantst
    
else ifeq ($(OSTYPE),Darwin)

#
# Mac OS X
#
all: dumpmidi test play keyboard playmidi playwave \
    printdev connectmidi connectwave random genwave scntst sndtst svstst \
    event term snake pong mine wator editor getpage getmail gettys gratst \
    mantst
    
else

#
# Linux
#
all: lsalsadev alsaparms dumpmidi test play keyboard playmidi playwave \
    printdev connectmidi connectwave random genwave scntst sndtst svstst \
    event getkeys getmouse term snake mine pong wator editor getpage getmail \
    gettys gratst mantst
    
endif 

################################################################################
#
# Clean target
#
################################################################################

clean:
	rm -f bin/dumpmidi bin/lsalsadev bin/alsaparms test bin/play bin/keyboard 
	rm -f bin/playmidi bin/playwave bin/printdev bin/connectmidi bin/connectwave 
	rm -f bin/random bin/genwave bin/scntst bin/sndtst bin/event bin/getkeys 
	rm -f bin/getmouse bin/term bin/snake bin/mine bin/editor bin/getpage
	rm -f bin/getmail bin/gettys bin/msgclient bin/msgserver bin/prtcertnet
	rm -f bin/listcertnet bin/prtcertmsg bin/gratst bin/mantst
	find . -name "*.o" -type f -delete
	rm -f bin/*.a
	rm -f bin/*.so
	rm -f bin/*.exe
