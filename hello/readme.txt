******************** Plain output hello world example compile *******************

Purpose:

This directory shows a minimal compile of the standard "hello, world" program
using plain output (not terminal or graphical enabled). The main purpose of this
is to give you a typical environment for your program, without having to account
for the full Petit-Ami environment.

Prerequistes:

You must have built Petit-Ami with the libraries present in the ./bin directory.
Further, the dependent libraries must be present in your system, OpenSSL, etc.

Why make a "plain" output version?

There are several components of the Petit-Ami system that don't rely on terminal
or graphical output, including:

* The system interface.
* The sound interface.
* The networking interface.

Thus there is good reason you might want to compile plain interface programs.
