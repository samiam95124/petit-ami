# Ami !

**One program source, presented wherever you run it.** The same
"hello, world" -- the standard C program, unchanged -- on a command
line, on a terminal, and in a graphical window:

| Command line | Terminal | Graphical |
|:---:|:---:|:---:|
| ![hello on the command line](doc/images/hello_cmdline.png) | ![hello on a terminal](doc/images/hello_console.png) | ![hello in a window](doc/images/hello_graphical.png) |

And the same idea carried to a full application. This is one mail
client -- one core holding the store, the fetch engine and the
protocols -- with front ends that differ only in how they draw. In
pixels; in character cells on a stock terminal, windows and widgets
managed by Ami's own character-mode window manager; and the character
front end linked against the graphical library, which speaks the whole
terminal API onto a window of its own:

**mail** -- the graphical front end:

![the mail client, graphical](doc/images/mail_graphical.png)

**mailc** -- the same mail in character cells, running in a plain
terminal, with menus, windows and widgets from the character-mode
window manager:

![the mail client on a terminal](doc/images/mail_terminal.png)

**mailcg** -- the character front end, unchanged, compiled against the
graphical library:

![the character mail client on a graphical window](doc/images/mail_charwin.png)

---

The AMI tk is a toolkit based on the idea that carefully crafted display
paradigms can carry forward. Ami starts with the serial display paradigm
and carries that into a terminal oriented paradigm, then to a fully
graphical and windowed paradigm.

Web page for Ami !

https://samiam95124.github.io/amitk

Please see the following documents to get started in Petit-Ami

    doc/petit_ami.docx  The "grand unified" document of what it is, how to
                        install it, etc.
    INSTALL             The basic install procedure.

The project is presented as complete within a single tree. The format is
along the lines of GNU projects, IE, with a configure, Makefile, README, etc.
