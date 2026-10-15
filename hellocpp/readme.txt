**************** Hello world example compile for Petit-Ami, in C++ ****************

Purpose:

This directory is ../hello written in C++: the "hello, world" program on the
C++ standard streams, compiled against Petit-Ami. It shows the one thing a C++
program needs beyond what the C example shows, which is the stream binding.
The example uses only the include, hpp and lib directories of the tree.

The program writes std::cout, and the line lands where the model puts standard
output, as the C example's printf does. The same program builds on any of the
three Petit-Ami models, in either form of the library, chosen on the make
command line:

    make                                 plain output, the static archive
    make MODEL=plain LINK_TYPE=static    plain output, the static archive
    make MODEL=plain LINK_TYPE=dynamic   plain output, the shared library
    make MODEL=term  LINK_TYPE=static    the terminal model, the static archive
    make MODEL=term  LINK_TYPE=dynamic   the terminal model, the shared library
    make MODEL=graph LINK_TYPE=static    the graphical model, the static archive
    make MODEL=graph LINK_TYPE=dynamic   the graphical model, the shared library

The models look like this to the program:

* plain: no terminal handler. The line goes to standard output, in step with
  any printf the program also makes.
* term: the terminal model takes over the console as the program starts, and
  "hello, world" appears on a cleared screen.
* graph: the graphical model opens a window as the program starts, and
  "hello, world" appears in it. The window stays until it is closed.

As in the C example, the program does not remember the options it was built
with, so after changing either, make clean before making again, and the root
of the tree must have been built in the same LINK_TYPE.

What the stream binding is:

The C++ library's streams do not reach Petit-Ami by themselves. std::cout is
built on the system's stdio inside the C++ runtime, so a C++ program linked
with Petit-Ami alone has its printf in the window and its std::cout on the
console behind it. Petit-Ami's stream binding (cpp/stdio.cpp in the tree)
puts the four standard streams, std::cin, std::cout, std::cerr and std::clog,
on Petit-Ami's stdin, stdout and stderr as the program starts. Nothing in the
program changes: it includes <iostream> and writes std::cout.

The binding is a member of each model's library. A program that uses one of
Petit-Ami's C++ classes (terminal::term, graphics::graph) gets it with the
class. A program like this one calls nothing in it, so the makefile asks the
linker for it by name (-u ami_iostream), the same way it asks for the model
(-u ami_maxx). Leave it out and the program links and runs, with its output on
the console instead of in the model.

The binding also provides file streams on Petit-Ami's stdio, stdio::ifstream,
stdio::ofstream and stdio::fstream, declared in hpp/stdio.hpp, for files
opened through Petit-Ami and for the FILE* Petit-Ami hands out, such as a
window or a network connection:

    #include <graphics.hpp>
    #include <stdio.hpp>
    graphics::window w;      /* a window of its own; it converts to FILE* */
    stdio::ofstream win(w);
    win << "text in the window" << std::endl;

How the makefile is put together:

* It is the C example's makefile with g++ for gcc, the hpp directory on the
  include path, and -u ami_iostream added to the linker requests.
* A C++ program compiles against the system's headers in both forms of the
  library. The C example's dynamic form compiles against Petit-Ami's own
  stdio header, ../libc/stdio.h, to reach the shared library's stdio by its
  coined names; a C++ program cannot, because <iostream> brings the system's
  stdio.h with it, and the two headers do not coexist in one translation
  unit. The streams need no header to reach the model, since the binding in
  the library makes the calls, so in the dynamic form a C++ program's streams
  land in the model, and its C stdio calls, printf and the rest, land on the
  system's stdout. The static form has no such split: the archive supplies
  the stdio names to the whole program.
* The wide streams (std::wcout and the rest) are not redirected, and
  std::ios_base::sync_with_stdio(false) must not be called, since it would
  replace the redirected streams with the C++ library's own.
