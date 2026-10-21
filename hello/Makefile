#
# Example makefile for the hello world program, built on Petit-Ami
#
# The same program builds on any of the three Petit-Ami models, against
# either form of the library:
#
#   MODEL=plain      no terminal handler: system, sound and network only
#   MODEL=term       the terminal model: output goes to the console
#   MODEL=graph      the graphical model: output goes to a window
#
#   LINK_TYPE=static     the archive ../lib/libami_<model>.a (the default)
#   LINK_TYPE=dynamic    the shared library ../lib/petit_ami_<model>.so
#
# The Mac builds the static form only.
#
MODEL ?= plain
LINK_TYPE ?= static

OSTYPE := $(shell uname)

#
# Required libraries
#
ifeq ($(OSTYPE),Darwin)

    #
    # Mac OS X: OpenSSL from Homebrew, the rest is the system frameworks
    #
    SSL_PREFIX := $(shell /opt/homebrew/bin/brew --prefix openssl@3)
    LIBS = $(SSL_PREFIX)/lib/libssl.dylib $(SSL_PREFIX)/lib/libcrypto.dylib \
           -framework CoreFoundation -framework CoreGraphics \
           -framework ImageIO -framework CoreMIDI -framework AudioToolbox
    ifeq ($(MODEL),term)
        LIBS += -framework IOKit
    endif
    ifeq ($(MODEL),graph)
        LIBS += -framework Cocoa -framework CoreText -framework QuartzCore \
                -framework IOKit
    endif

else

    LIBS = -lasound -lfluidsynth -lssl -lcrypto -lm -lpthread

    #
    # On a graphical program, choose Wayland or X11
    #
    ifeq ($(MODEL),graph)
        ifneq ($(WAYLAND_DISPLAY),)
            LIBS += -lwayland-client -lwayland-cursor -lxkbcommon -lfreetype -lfontconfig
        else
            LIBS += -lX11 -lXtst -lfreetype -lfontconfig
        endif
    endif

endif

#
# Force use of the chosen model. Mach-O symbols carry a leading underscore.
#
ifneq ($(MODEL),plain)
    ifeq ($(OSTYPE),Darwin)
        KEEP = -Wl,-u,_ami_maxx
    else
        KEEP = -Wl,-u,ami_maxx
    endif
endif

#
# Set Ami specific libraries
#
# Sound cannot live in a shared library (an ALSA bug), so in the dynamic form
# its object links directly beside the library. Dynamic linking needs stdio
# bypass.
#
ifeq ($(LINK_TYPE),static)
    AMILIB = $(AMIDIR)/lib/libami_$(MODEL).a
    AMILINK = $(AMILIB)
    ifeq ($(OSTYPE),Darwin)
        ifeq ($(MODEL),graph)
            # the whole graphical model, entered at its own entry point
            AMILINK = -Wl,-force_load,$(AMILIB) -Wl,-e,_pa_entry
        endif
    endif
    AMICFLAGS =
else
    AMILIB = $(AMIDIR)/lib/sound.o $(AMIDIR)/lib/petit_ami_$(MODEL).so
    AMILINK = $(AMILIB) -Wl,-rpath,$(AMIDIR)/lib
    AMICFLAGS = -I$(AMIDIR)/libc -DSTDIO_BYPASS
endif

hello: hello.c $(AMILIB)
	gcc -g3 -I$(AMIDIR)/include $(AMICFLAGS) hello.c $(KEEP) $(AMILINK) $(LIBS) -o hello

clean:
	rm -f hello
