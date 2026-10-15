/** ****************************************************************************
 *
 * Stdio library C++ stream binding declarations
 *
 * See cpp/stdio.cpp for the rationale. The classes here put C++ stream I/O
 * on Petit-Ami's stdio, so that a C++ program's streams land where its
 * printf does: in the terminal, in the window, on the network connection.
 *
 * The binding is installed by linking it. Once linked, std::cin, std::cout,
 * std::cerr and std::clog run on Petit-Ami's stdin, stdout and stderr, and
 * nothing in the program needs to change. The classes declared here are
 * for streams on other files: a stdio::ofstream opens a file through
 * Petit-Ami's fopen, and a stdio::filebuf wraps any FILE* Petit-Ami hands
 * out, such as a window from openwin or a connection from opennet.
 *
 * The FILE* in these declarations is the pointer type the program's own
 * stdio.h gives it. The binding never looks inside the FILE, it only hands
 * the pointer back to Petit-Ami's stdio, so the layout behind it does not
 * matter here.
 *
 ******************************************************************************/

#ifndef __STDIO_HPP__
#define __STDIO_HPP__

#include <cstdio>
#include <ios>
#include <istream>
#include <ostream>
#include <streambuf>

namespace stdio {

/*******************************************************************************

Stream buffer on a Petit-Ami FILE

A filebuf forwards each operation straight to Petit-Ami's stdio: a put is a
fputc or fwrite, a get is a fgetc or fread, a sync is a fflush. It keeps no
buffer of its own, so output through a stream and output through printf on
the same FILE come out in the order the program made them, which is what the
standard streams need. Petit-Ami's own buffering, as set by setvbuf, applies
underneath as it does for the C calls.

A filebuf made on a FILE it did not open leaves the FILE open when it goes;
one that opened the file closes it.

*******************************************************************************/

class filebuf: public std::streambuf {

    public:

    filebuf();
    explicit filebuf(std::FILE* f);
    virtual ~filebuf();

    /* open a file by name and mode, as fopen does, or by C++ open mode */
    filebuf* open(const char* name, const char* mode);
    filebuf* open(const char* name, std::ios_base::openmode mode);
    /* take an open FILE; the caller keeps ownership */
    filebuf* attach(std::FILE* f);
    /* close the file if this filebuf opened it, otherwise just let it go */
    filebuf* close();
    bool is_open() const { return file_ != 0; }
    std::FILE* file() const { return file_; }

    protected:

    virtual int_type   overflow(int_type c = traits_type::eof());
    virtual std::streamsize xsputn(const char_type* s, std::streamsize n);
    virtual int_type   underflow();
    virtual int_type   uflow();
    virtual int_type   pbackfail(int_type c = traits_type::eof());
    virtual std::streamsize xsgetn(char_type* s, std::streamsize n);
    virtual int        sync();
    virtual pos_type   seekoff(off_type off, std::ios_base::seekdir dir,
                               std::ios_base::openmode which =
                                   std::ios_base::in | std::ios_base::out);
    virtual pos_type   seekpos(pos_type pos,
                               std::ios_base::openmode which =
                                   std::ios_base::in | std::ios_base::out);

    private:

    std::FILE* file_;  /* the file, or 0 when none */
    bool       owned_; /* this filebuf opened it and will close it */
    int_type   last_;  /* the last character read, for pbackfail(eof) */

    filebuf(const filebuf&);
    filebuf& operator=(const filebuf&);

};

/*******************************************************************************

File streams on Petit-Ami's stdio

These are the std::ifstream, std::ofstream and std::fstream of Petit-Ami: the
same interface, with the file opened through Petit-Ami's fopen and read and
written through its stdio. Each can also be made on a FILE* already open,
which is how a window or a network connection becomes a stream.

*******************************************************************************/

class ifstream: public std::istream {

    public:

    ifstream();
    explicit ifstream(const char* name,
                      std::ios_base::openmode mode = std::ios_base::in);
    explicit ifstream(std::FILE* f);
    virtual ~ifstream();

    void open(const char* name, std::ios_base::openmode mode = std::ios_base::in);
    void attach(std::FILE* f);
    void close();
    bool is_open() const { return buf_.is_open(); }
    filebuf* rdbuf() const { return const_cast<filebuf*>(&buf_); }

    private:

    filebuf buf_;

};

class ofstream: public std::ostream {

    public:

    ofstream();
    explicit ofstream(const char* name,
                      std::ios_base::openmode mode = std::ios_base::out);
    explicit ofstream(std::FILE* f);
    virtual ~ofstream();

    void open(const char* name, std::ios_base::openmode mode = std::ios_base::out);
    void attach(std::FILE* f);
    void close();
    bool is_open() const { return buf_.is_open(); }
    filebuf* rdbuf() const { return const_cast<filebuf*>(&buf_); }

    private:

    filebuf buf_;

};

class fstream: public std::iostream {

    public:

    fstream();
    explicit fstream(const char* name,
                     std::ios_base::openmode mode =
                         std::ios_base::in | std::ios_base::out);
    explicit fstream(std::FILE* f);
    virtual ~fstream();

    void open(const char* name,
              std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out);
    void attach(std::FILE* f);
    void close();
    bool is_open() const { return buf_.is_open(); }
    filebuf* rdbuf() const { return const_cast<filebuf*>(&buf_); }

    private:

    filebuf buf_;

};

} /* namespace stdio */

extern "C" {

/* The binding's presence. A program that makes no call into the binding
   names this symbol to the linker (-u ami_iostream) so that the member is
   linked and the standard streams are redirected. It returns 1 when the
   standard streams are on Petit-Ami's stdio. */
int ami_iostream(void);

}

#endif /* __STDIO_HPP__ */
