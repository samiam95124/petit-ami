******************** Plain output hello world example compile *******************

Purpose:

This directory shows a minimal compile of the standard "hello, world" program
using plain output (not terminal or graphical enabled). The main purpose of this
is to give you a typical environment for your program, without having to account
for the full Petit-Ami environment.

Prerequistes:

The dependent libraries must be present in your system, OpenSSL, etc. The
Petit-Ami library this example links, ../lib/libami_plain.a, is made by the
build at the root; the makefile here makes it there if it is not already made,
so a plain "make" in this directory works in a tree that has never been built.

The makefile finds the root from its own path rather than from the directory
make was started in, so it builds the same however it is reached, and the
program it makes runs from any directory.

Why make a "plain" output version?

There are several components of the Petit-Ami system that don't rely on terminal
or graphical output, including:

* The system interface.
* The sound interface.
* The networking interface.

Thus there is good reason you might want to compile plain interface programs.
