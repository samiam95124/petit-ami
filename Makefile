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
        # Linux
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
CPP=g++

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
    CLIBS += stub/keeper.o bin/petit_ami_term.so 
endif

#
# Graphical model API
#
ifeq ($(LINK_TYPE),static)
	GLIBS += -Wl,--whole-archive bin/petit_ami_graph.a -Wl,--no-whole-archive
else
    GLIBS += stub/keeper.o bin/petit_ami_graph.so
endif

#
# add external packages
#
ifeq ($(OSTYPE),Windows_NT)

    #
    # Windows
    #
    PLIBS += -lwinmm -lwsock32
    CLIBS += -lwinmm -lgdi32 -lwsock32
    GLIBS += -lwinmm -lgdi32 -lcomdlg32 -lwsock32

else ifeq ($(OSTYPE),Darwin)

    #
    # Nothing needed for Mac OS X
    #
    
else

    #
    # Linux
    #
	PLIBS += linux/sound.o -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
	CLIBS += linux/sound.o -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
	GLIBS += linux/sound.o -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto -lX11
    
endif

#
# Create dependency macros
#
PLIBSD = bin/petit_ami_plain$(LIBEXT)
CLIBSD = bin/petit_ami_term$(LIBEXT) stub/keeper.o
GLIBSD = bin/petit_ami_graph$(LIBEXT) stub/keeper.o

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
all: dumpmidi play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testg \
     graphics_test management_test widget_test \
     sound_test sound_testg services_test event eventg term termg snake snakeg mine mineg \
     wator watorg pong pongg breakout editor editorg getpage getpageg getmail \
     getmailg gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 line2 \
     line4 line5 clock
    
else ifeq ($(OSTYPE),Darwin)

#
# Mac OS X
#
all: dumpmidi play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testg \
     graphics_test management_test widget_test \
     sound_test sound_testg services_test event eventg term termg snake snakeg mine mineg \
     wator watorg pong pongg breakout editor editorg getpage getpageg getmail \
     getmailg gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg listcertnet listcertnetg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 \
     line2 line4 line5 clock
    
else

#
# Linux
#
all: dumpmidi play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testg \
     graphics_test management_test widget_test \
     sound_test sound_testg services_test event eventg term termg snake snakeg mine mineg \
     wator watorg pong pongg breakout editor editorg getpage getpageg getmail \
     getmailg gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg listcertnet listcertnetg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 \
     line2 line4 line5 clock
    
endif 

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
	
linux/terminal.o: linux/terminal.c include/terminal.h Makefile
	gcc -g3 -Iinclude -fPIC -c linux/terminal.c -o linux/terminal.o
	
linux/graphics.o: linux/graphics.c include/graphics.h Makefile
	gcc -g3 -Iinclude -fPIC -c linux/graphics.c -o linux/graphics.o
	
#
# Windows library components
#
# Note that stub sources are not yet implemented
#
windows/stdio.o: libc/stdio.c libc/stdio.h Makefile
	gcc -g3 -Ilibc -c libc/stdio.c -o windows/stdio.o
	
windows/services.o: windows/services.c include/services.h Makefile
	gcc -g3 -Ilibc -Iinclude -c windows/services.c -o windows/services.o
	
windows/sound.o: windows/sound.c include/sound.h Makefile
	gcc -g3 -Ilibc -Iinclude -c windows/sound.c -o windows/sound.o
	
windows/network.o: windows/network.c include/network.h Makefile
	gcc -g3 -Ilibc -Iinclude -c windows/network.c -o windows/network.o
	
windows/terminal.o: windows/terminal.c include/terminal.h Makefile
	gcc -g3 -Ilibc -Iinclude -c windows/terminal.c -o windows/terminal.o
	
windows/graphics.o: windows/graphics.c include/graphics.h Makefile
	gcc -g3 -Ilibc -Iinclude -c windows/graphics.c -o windows/graphics.o

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
	
macosx/terminal.o: linux/terminal.c include/terminal.h Makefile
	gcc -g3 -Ilibc -Iinclude -c linux/terminal.c -o macosx/terminal.o
	
macosx/graphics.o: stub/graphics.c include/graphics.h Makefile
	gcc -g3 -Ilibc -Iinclude -c stub/graphics.c -o macosx/graphics.o
	
#
# Components in common to all systems
#
utils/config.o: utils/config.c include/localdefs.h include/services.h \
	            include/config.h Makefile
	gcc -g3 -fPIC -Ilibc -Iinclude -c utils/config.c -o utils/config.o
	
utils/option.o: utils/option.c include/localdefs.h include/services.h \
	            include/option.h Makefile
	gcc -g3 -fPIC -Ilibc -Iinclude -c utils/option.c -o utils/option.o
	
stub/keeper.o: stub/keeper.c
	gcc -g3 -fPIC -Iinclude -c stub/keeper.c -o stub/keeper.o

cpp/terminal.o: cpp/terminal.cpp
	g++ -g3 -fPIC -Iinclude -Ihpp -c cpp/terminal.cpp -o cpp/terminal.o
	
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
    windows/terminal.o utils/config.o utils/option.o windows/stdio.o
	ar rcs bin/petit_ami_term.a windows/services.o windows/sound.o \
	    windows/network.o windows/terminal.o utils/config.o utils/option.o \
	    windows/stdio.o
	
bin/petit_ami_graph.a: windows/services.o windows/sound.o windows/network.o \
    windows/graphics.o utils/config.o utils/option.o windows/stdio.o
	ar rcs bin/petit_ami_graph.a windows/services.o windows/sound.o \
	    windows/network.o windows/graphics.o utils/config.o utils/option.o \
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
    macosx/terminal.o utils/config.o utils/option.o macosx/stdio.o
	ar rcs bin/petit_ami_term.a macosx/services.o macosx/sound.o \
	    macosx/network.o macosx/terminal.o utils/config.o utils/option.o \
	    macosx/stdio.o
	
petit_ami_graph.a: macosx/services.o macosx/sound.o macosx/network.o \
    macosx/graphics.o utils/config.o utils/option.o macosx/stdio.o
	ar rcs bin/petit_ami_graph.a macosx/services.o macosx/sound.o \
	    macosx/network.o macosx/graphics.o utils/config.o utils/option.o \
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
	gcc -shared linux/services.o linux/fluidsynthplug.o \
	    linux/dumpsynthplug.o linux/network.o utils/config.o utils/option.o \
	    -o bin/petit_ami_plain.so
	
bin/petit_ami_plain.a: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o utils/config.o utils/option.o
	ar rcs bin/petit_ami_plain.a linux/services.o linux/sound.o \
	    linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
	    utils/config.o utils/option.o
	
bin/petit_ami_term.so: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/terminal.o utils/config.o \
    utils/option.o cpp/terminal.o
	gcc -shared linux/services.o linux/fluidsynthplug.o \
	    linux/dumpsynthplug.o  linux/network.o linux/terminal.o utils/config.o \
	    utils/option.o cpp/terminal.o -o bin/petit_ami_term.so 
	
bin/petit_ami_term.a: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/terminal.o utils/config.o \
    utils/option.o cpp/terminal.o
	ar rcs bin/petit_ami_term.a linux/services.o linux/sound.o \
		linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
		linux/terminal.o utils/config.o utils/option.o cpp/terminal.o
	
bin/petit_ami_graph.so: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/graphics.o utils/config.o \
    utils/option.o cpp/terminal.o
	gcc -shared linux/services.o linux/fluidsynthplug.o \
		linux/dumpsynthplug.o  linux/network.o linux/graphics.o utils/config.o \
		utils/option.o cpp/terminal.o -o bin/petit_ami_graph.so
	
bin/petit_ami_graph.a: linux/services.o linux/sound.o linux/fluidsynthplug.o \
    linux/dumpsynthplug.o linux/network.o linux/graphics.o utils/config.o \
    utils/option.o cpp/terminal.o
	ar rcs bin/petit_ami_graph.a linux/services.o linux/sound.o \
		linux/fluidsynthplug.o linux/dumpsynthplug.o  linux/network.o \
		linux/terminal.o utils/config.o utils/option.o cpp/terminal.o
	
endif

################################################################################
#
# Build final programs
#
################################################################################

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
	
test++: $(PLIBSD) test.cpp
	$(CPP) $(CFLAGS) test.cpp $(PLIBS) -o test
	
testc++: $(CLIBSD) test.cpp
	$(CPP) $(CFLAGS) test.cpp $(CLIBS) -o testc
	
testg++: $(GLIBSD) test.cpp
	$(CPP) $(CFLAGS) test.cpp $(GLIBS) -o testg
	
#
# Target programs that use Petit-Ami, such as games, utilities, etc.
# "dazzler" programs is my term. It comes from the Cromemco Dazzler, a graphics
# that ran on early S100 computers, and was a popular display demonstration from
# those days.
#
# Note serial output model programs can be ported to each of the console or
# graphical models, but generally the console model is skipped since the action
# such a program would be identical to the serial model.
#
# The use case for graphical versions of serial model programs is not clear.
# They serve as a test of the promotion model, and might be useful if only a
# graphical model system is in use, like the file manager.
#

#
# Play example songs in QBasic play format (uses console timers)
# 	
play: $(CLIBSD) sound_programs/play.c
	$(CC) $(CFLAGS) sound_programs/play.c $(CLIBS) -o bin/play
	
playg: $(GLIBSD) sound_programs/play.c
	$(CC) $(CFLAGS) sound_programs/play.c $(GLIBS) -o bin/playg

#
# Emulate a sound keyboard (uses console timers)
#	
keyboard: $(CLIBSD) sound_programs/keyboard.c
	$(CC) $(CFLAGS) sound_programs/keyboard.c $(CLIBS) -o bin/keyboard
	
keyboardg: $(GLIBSD) sound_programs/keyboard.c
	$(CC) $(CFLAGS) sound_programs/keyboard.c $(GLIBS) -o bin/keyboardg

#
# Play midi files
#	
playmidi: $(PLIBSD) sound_programs/playmidi.c
	$(CC) $(CFLAGS) sound_programs/playmidi.c $(PLIBS) -o bin/playmidi
	
playmidig: $(GLIBSD) sound_programs/playmidi.c
	$(CC) $(CFLAGS) sound_programs/playmidi.c $(GLIBS) -o bin/playmidig

#
# Play wave files
#
playwave: $(PLIBSD) sound_programs/playwave.c
	$(CC) $(CFLAGS) sound_programs/playwave.c $(PLIBS) -o bin/playwave
	
playwaveg: $(GLIBSD) sound_programs/playwave.c
	$(CC) $(CFLAGS) sound_programs/playwave.c $(GLIBS) -o bin/playwaveg

#
# Print a list of available sound devices
#	
printdev: $(PLIBSD) sound_programs/printdev.c
	$(CC) $(CFLAGS) sound_programs/printdev.c $(PLIBS) -o bin/printdev
	
printdevg: $(GLIBSD) sound_programs/printdev.c
	$(CC) $(CFLAGS) sound_programs/printdev.c $(GLIBS) -o bin/printdevg

#
# Connect Midi input port to Midi output port
#
connectmidi: $(PLIBSD) sound_programs/connectmidi.c
	$(CC) $(CFLAGS) sound_programs/connectmidi.c $(PLIBS) -o bin/connectmidi
	
connectmidig: $(GLIBSD) sound_programs/connectmidi.c
	$(CC) $(CFLAGS) sound_programs/connectmidi.c $(GLIBS) -o bin/connectmidig

#
# Connect wave input port to wave output port
#	
connectwave: $(PLIBSD) sound_programs/connectwave.c
	$(CC) $(CFLAGS) sound_programs/connectwave.c $(PLIBS) -o bin/connectwave
	
connectwaveg: $(GLIBSD) sound_programs/connectwave.c
	$(CC) $(CFLAGS) sound_programs/connectwave.c $(GLIBS) -o bin/connectwaveg

#	
# Play random notes
#
random: $(CLIBSD) sound_programs/random.c
	$(CC) $(CFLAGS) sound_programs/random.c $(CLIBS) -o bin/random
	
randomg: $(CLIBSD) sound_programs/random.c
	$(CC) $(CFLAGS) sound_programs/random.c $(CLIBS) -o bin/randomg

#
# Generate waveforms
#	
genwave: $(PLIBSD) sound_programs/genwave.c
	$(CC) $(CFLAGS) sound_programs/genwave.c $(PLIBS) -o bin/genwave
	
genwaveg: $(GLIBSD) sound_programs/genwave.c
	$(CC) $(CFLAGS) sound_programs/genwave.c $(GLIBS) -o bin/genwaveg

#
# Test console model compliant output
#	
terminal_test: $(CLIBSD) tests/terminal_test.c
	$(CC) $(CFLAGS) tests/terminal_test.c $(CLIBS) -o bin/terminal_test

terminal_testg: $(GLIBSD) tests/terminal_test.c
	$(CC) $(CFLAGS) tests/terminal_test.c $(GLIBS) -o bin/terminal_testg
	
#
# Test graph model compliant output
#
graphics_test: $(GLIBSD) tests/graphics_test.c
	$(CC) $(CFLAGS) tests/graphics_test.c $(GLIBS) -o bin/graphics_test 
	
#
# Test windows management model compliant output
#
management_test: $(GLIBSD) tests/management_test.c
	$(CC) $(CFLAGS) tests/management_test.c $(GLIBS) -o bin/management_test 

#
# Test windows widget compliant output
#
widget_test: $(GLIBSD) tests/widget_test.c
	$(CC) $(CFLAGS) tests/widget_test.c $(GLIBS) -o bin/widget_test 
	
#
# Test sound model compliant input/output (uses console timers)
#	
sound_test: $(CLIBSD) tests/sound_test.c
	$(CC) $(CFLAGS) tests/sound_test.c $(CLIBS) -o bin/sound_test 
	
sound_testg: $(GLIBSD) tests/sound_test.c
	$(CC) $(CFLAGS) tests/sound_test.c $(GLIBS) -o bin/sound_testg 
	
#
# Test services module
#
# Note services test uses a separate program, services_test1, that tests the ability to
# execute a separate program.
#
services_test: $(PLIBSD) tests/services_test.c
	$(CC) $(CFLAGS) tests/services_test.c $(PLIBS) -o bin/services_test
	$(CC) $(CFLAGS) tests/services_test1.c $(PLIBS) -o bin/services_test1

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
term: $(CLIBSD) tests/term.c
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
# Note pong is a different program in console vs. graphical mode, the graphical
# version takes full advantage of the graphical model.
#	
pong: $(CLIBSD) terminal_games/pong.c
	$(CC) $(CFLAGS) terminal_games/pong.c $(CLIBS) -o bin/pong
	
pongg: $(GLIBSD) graph_games/pong.c
	$(CC) $(CFLAGS) graph_games/pong.c $(GLIBS) -o bin/pongg

#
# Breakout game for graphics
#
breakout: $(GLIBSD) graph_games/breakout.c
	$(CC) $(CFLAGS) graph_games/breakout.c $(GLIBS) -o bin/breakout

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
	
getpageg: $(GLIBSD) network_programs/getpage.c
	$(CC) $(CFLAGS) network_programs/getpage.c $(GLIBS) -o bin/getpageg

#
# Get remote email
#
getmail: $(PLIBSD) network_programs/getmail.c
	$(CC) $(CFLAGS) network_programs/getmail.c $(PLIBS) -o bin/getmail

getmailg: $(GLIBSD) network_programs/getmail.c
	$(CC) $(CFLAGS) network_programs/getmail.c $(GLIBS) -o bin/getmailg

#
# Gettysberg address server
#	
gettys: $(PLIBSD) network_programs/gettys.c
	$(CC) $(CFLAGS) network_programs/gettys.c $(PLIBS) -o bin/gettys
	
gettysg: $(GLIBSD) network_programs/gettys.c
	$(CC) $(CFLAGS) network_programs/gettys.c $(GLIBS) -o bin/gettysg

#
# Message based networking test client
#	
msgclient: $(PLIBSD) network_programs/msgclient.c
	$(CC) $(CFLAGS) network_programs/msgclient.c $(PLIBS) -o bin/msgclient
	
msgclientg: $(GLIBSD) network_programs/msgclient.c
	$(CC) $(CFLAGS) network_programs/msgclient.c $(GLIBS) -o bin/msgclientg

#
# Message based networking test server
#	
msgserver: $(PLIBSD) network_programs/msgserver.c
	$(CC) $(CFLAGS) network_programs/msgserver.c $(PLIBS) -o bin/msgserver
	
msgserverg: $(GLIBSD) network_programs/msgserver.c
	$(CC) $(CFLAGS) network_programs/msgserver.c $(GLIBS) -o bin/msgserverg
	
#
# Print TCPIP/TLS certificates
#
prtcertnet: $(PLIBSD) network_programs/prtcertnet.c
	$(CC) $(CFLAGS) network_programs/prtcertnet.c $(PLIBS) -o bin/prtcertnet

prtcertnetg: $(GLIBSD) network_programs/prtcertnet.c
	$(CC) $(CFLAGS) network_programs/prtcertnet.c $(GLIBS) -o bin/prtcertnetg

#
# Print message based certificates
#	
prtcertmsg: $(PLIBSD) network_programs/prtcertmsg.c
	$(CC) $(CFLAGS) network_programs/prtcertmsg.c $(PLIBS) -o bin/prtcertmsg
	
prtcertmsgg: $(GLIBSD) network_programs/prtcertmsg.c
	$(CC) $(CFLAGS) network_programs/prtcertmsg.c $(GLIBS) -o bin/prtcertmsgg

#
# This program is missing???? check the linux machine.
#	
listcertnet: $(PLIBSD) network_programs/listcertnet.c
	$(CC) $(CFLAGS) network_programs/listcertnet.c $(PLIBS) -o bin/listcertnet
	
listcertnetg: $(GLIBSD) network_programs/listcertnet.c
	$(CC) $(CFLAGS) network_programs/listcertnet.c $(GLIBS) -o bin/listcertnetg

#
# Print Petit-Ami configuration tree
#	
prtconfig: $(PLIBSD) utils/prtconfig.c
	$(CC) $(CFLAGS) utils/prtconfig.c $(PLIBS) -o bin/prtconfig
	
prtconfigg: $(GLIBSD) utils/prtconfig.c
	$(CC) $(CFLAGS) utils/prtconfig.c $(GLIBS) -o bin/prtconfigg
	
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
	
ball3: $(GLIBSD) graph_programs/ball3.c
	$(CC) $(CFLAGS) graph_programs/ball3.c $(GLIBS) -o bin/ball3
	
ball4: $(GLIBSD) graph_programs/ball4.c
	$(CC) $(CFLAGS) graph_programs/ball4.c $(GLIBS) -o bin/ball4
	
ball5: $(GLIBSD) graph_programs/ball5.c
	$(CC) $(CFLAGS) graph_programs/ball5.c $(GLIBS) -o bin/ball5
	
ball6: $(GLIBSD) graph_programs/ball6.c
	$(CC) $(CFLAGS) graph_programs/ball6.c $(GLIBS) -o bin/ball6
	
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
	
#
# Resizable clock
#
clock: $(GLIBSD) graph_programs/clock.c
	$(CC) $(CFLAGS) graph_programs/clock.c $(GLIBS) -o bin/clock
	
################################################################################
#
# Clean target
#
################################################################################

clean:
	rm -f lsalsadev alsaparms
	rm -f bin/dumpmidi bin/test bin/play bin/playg bin/keyboard bin/keyboardg
	rm -f bin/playmidi bin/playmidig bin/playwave bin/playwaveg bin/printdev
	rm -f bin/printdevg bin/connectmidi bin/connectmidig bin/connectwave
	rm -f bin/connectwaveg bin/random bin/randomg bin/genwave bin/genwaveg 
	rm -f bin/terminal_test bin/terminal_testg bin/graphics_test 
	rm -f bin/management_test bin/widget_test bin/sound_test bin/sound_testg
	rm -f bin/services_test bin/event bin/eventg bin/term bin/termg bin/snake 
	rm -f bin/snakeg bin/mine bin/mineg bin/wator bin/watorg bin/pong bin/pongg
	rm -f bin/breakout bin/editor bin/editorg bin/getpage bin/getpageg 
	rm -f bin/getmail bin/getmailg bin/gettys bin/gettysg bin/msgclient 
	rm -f bin/msgclientg bin/msgserver bin/msgserverg bin/prtcertnet
	rm -f bin/prtcertnetg bin/prtcertmsg bin/prtcertmsgg bin/listcertnet 
	rm -f bin/listcertnetg bin/prtconfig bin/prtconfigg bin/pixel bin/ball1
	rm -f bin/ball2 bin/ball3 bin/ball4 bin/ball5 bin/line1 bin/line2 \
	rm -f bin/line4 bin/line5
	find . -name "*.o" -type f -delete
	rm -f bin/*.a
	rm -f bin/*.so
	rm -f bin/*.exe
