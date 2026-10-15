/** ****************************************************************************
 *
 * Stdio library C++ stream binding
 *
 * Puts C++ stream I/O on Petit-Ami's stdio, the way libc/stdio.c puts C's
 * stdio on Petit-Ami's I/O vectors. The C++ library's own streams never
 * reach those vectors: libstdc++ builds std::cout on the system's fwrite,
 * and the file streams on the raw read and write calls, both inside the
 * shared C++ runtime, which binds the system's stdio. A C++ program linked
 * with Petit-Ami had its printf in the terminal or the window and its
 * std::cout on the console behind them.
 *
 * C++ streams were made to be redirected. A stream is a formatting layer
 * over a std::streambuf, and rdbuf() replaces the streambuf. The formatting
 * layer is not touched here: operator<<, the manipulators and the locales
 * stay the C++ library's. What this module supplies is:
 *
 * 1. stdio::filebuf, a streambuf whose every operation is a call on
 * Petit-Ami's stdio: fputc, fwrite, fgetc, fread, ungetc, fflush, fseek. It
 * holds no buffer of its own, so stream output and printf output on the same
 * FILE interleave in program order, as the standard streams require.
 *
 * 2. An installer, run as the program starts, that puts filebufs over
 * Petit-Ami's stdin, stdout and stderr into std::cin, std::cout, std::cerr
 * and std::clog, and puts the original streambufs back as the program ends.
 * The installer runs at init priority 101, the first a program may use, so
 * it precedes the program's own static constructors: a static object that
 * writes to std::cout writes to the window.
 *
 * 3. stdio::ifstream, ofstream and fstream, the file streams of Petit-Ami's
 * stdio, opened through its fopen, and constructible on a FILE* already
 * open, so a window from openwin or a connection from opennet is driven
 * with operator<<.
 *
 * The standard streams are redirected by linking the module, nothing is
 * called. A C++ program that uses a Petit-Ami class pulls it with the
 * class; one that does not asks the linker for it by name, -u ami_iostream,
 * as the makefiles do (see hellocpp/Makefile). The module is a member of
 * every model's library, so that a program's streams go where its model
 * puts standard output, and in the plain model to standard output in step
 * with printf.
 *
 * Two stdios, two modes
 *
 * This module never includes Petit-Ami's stdio.h. It includes <iostream>,
 * and the C++ library's headers take the system's stdio.h with it, which
 * does not coexist with Petit-Ami's in one translation unit (both define
 * FILE). The calls are reached as a program's are, per the mode described
 * at the head of libc/stdio.c. In override mode the standard names are
 * used, and the static link resolves them to Petit-Ami's stdio, as it does
 * for the program's own printf. In coined mode (STDIO_BYPASS) the stdio_
 * names are declared here directly, on an opaque FILE, and called. Either
 * way a FILE is only a pointer to this module, never looked into.
 *
 * Not done here
 *
 * The wide streams (std::wcout and the rest) are left on the C++ library's
 * own buffers. std::ios_base::sync_with_stdio(false) would replace the
 * installed streambufs with the C++ library's unsynchronized ones, which
 * write to the console directly; a program using this binding does not call
 * it (there is nothing to gain, the streams are on the C stdio either way).
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

#include <iostream>
#include <cstdio>

#include "stdio.hpp"

/*******************************************************************************

The stdio reached

In coined mode Petit-Ami's stdio lives under stdio_ names beside the system's.
The system's stdio.h is the one in this unit, so Petit-Ami's are declared
here on their own, with an opaque FILE: this module hands pointers through
and never looks at what they point to. In override mode the standard names
are Petit-Ami's at link.

*******************************************************************************/

#ifdef STDIO_BYPASS

extern "C" {

struct _stdio_file;
extern struct _stdio_file* stdio_stdin;
extern struct _stdio_file* stdio_stdout;
extern struct _stdio_file* stdio_err;
struct _stdio_file* stdio_fopen(const char* filename, const char* mode);
int    stdio_fclose(struct _stdio_file* f);
int    stdio_fflush(struct _stdio_file* f);
int    stdio_fgetc(struct _stdio_file* f);
int    stdio_fputc(int c, struct _stdio_file* f);
int    stdio_ungetc(int c, struct _stdio_file* f);
size_t stdio_fread(void* p, size_t size, size_t nobj, struct _stdio_file* f);
size_t stdio_fwrite(const void* p, size_t size, size_t nobj, struct _stdio_file* f);
int    stdio_fseek(struct _stdio_file* f, long offset, int origin);
long   stdio_ftell(struct _stdio_file* f);

}

namespace {

typedef struct _stdio_file amifile;
inline amifile* af(std::FILE* f) { return reinterpret_cast<amifile*>(f); }
inline std::FILE* sf(amifile* f) { return reinterpret_cast<std::FILE*>(f); }

inline std::FILE* ami_stdin()  { return sf(stdio_stdin); }
inline std::FILE* ami_stdout() { return sf(stdio_stdout); }
inline std::FILE* ami_stderr() { return sf(stdio_err); }
inline std::FILE* ami_fopen(const char* n, const char* m)
    { return sf(stdio_fopen(n, m)); }
inline int    ami_fclose(std::FILE* f) { return stdio_fclose(af(f)); }
inline int    ami_fflush(std::FILE* f) { return stdio_fflush(af(f)); }
inline int    ami_fgetc(std::FILE* f) { return stdio_fgetc(af(f)); }
inline int    ami_fputc(int c, std::FILE* f) { return stdio_fputc(c, af(f)); }
inline int    ami_ungetc(int c, std::FILE* f) { return stdio_ungetc(c, af(f)); }
inline size_t ami_fread(void* p, size_t n, std::FILE* f)
    { return stdio_fread(p, 1, n, af(f)); }
inline size_t ami_fwrite(const void* p, size_t n, std::FILE* f)
    { return stdio_fwrite(p, 1, n, af(f)); }
inline int    ami_fseek(std::FILE* f, long off, int whence)
    { return stdio_fseek(af(f), off, whence); }
inline long   ami_ftell(std::FILE* f) { return stdio_ftell(af(f)); }

}

#else

namespace {

inline std::FILE* ami_stdin()  { return stdin; }
inline std::FILE* ami_stdout() { return stdout; }
inline std::FILE* ami_stderr() { return stderr; }
inline std::FILE* ami_fopen(const char* n, const char* m)
    { return std::fopen(n, m); }
inline int    ami_fclose(std::FILE* f) { return std::fclose(f); }
inline int    ami_fflush(std::FILE* f) { return std::fflush(f); }
inline int    ami_fgetc(std::FILE* f) { return std::fgetc(f); }
inline int    ami_fputc(int c, std::FILE* f) { return std::fputc(c, f); }
inline int    ami_ungetc(int c, std::FILE* f) { return std::ungetc(c, f); }
inline size_t ami_fread(void* p, size_t n, std::FILE* f)
    { return std::fread(p, 1, n, f); }
inline size_t ami_fwrite(const void* p, size_t n, std::FILE* f)
    { return std::fwrite(p, 1, n, f); }
inline int    ami_fseek(std::FILE* f, long off, int whence)
    { return std::fseek(f, off, whence); }
inline long   ami_ftell(std::FILE* f) { return std::ftell(f); }

}

#endif

namespace stdio {

/*******************************************************************************

Translate a C++ open mode to an fopen mode

The table is the standard's (filebuf::open), with the binary flag adding a
"b". A combination the table does not have gives 0, and the open fails.

*******************************************************************************/

static const char* fopenmode(std::ios_base::openmode mode)

{

    using std::ios_base;

    bool bin = (mode & ios_base::binary) != 0;
    ios_base::openmode m = mode & ~(ios_base::binary | ios_base::ate);
    const char* s = 0;

    if (m == ios_base::out || m == (ios_base::out | ios_base::trunc))
        s = bin ? "wb" : "w";
    else if (m == ios_base::app || m == (ios_base::out | ios_base::app))
        s = bin ? "ab" : "a";
    else if (m == ios_base::in)
        s = bin ? "rb" : "r";
    else if (m == (ios_base::in | ios_base::out))
        s = bin ? "r+b" : "r+";
    else if (m == (ios_base::in | ios_base::out | ios_base::trunc))
        s = bin ? "w+b" : "w+";
    else if (m == (ios_base::in | ios_base::app) ||
             m == (ios_base::in | ios_base::out | ios_base::app))
        s = bin ? "a+b" : "a+";

    return s;

}

/*******************************************************************************

filebuf

*******************************************************************************/

filebuf::filebuf(): file_(0), owned_(false), last_(traits_type::eof()) { }

filebuf::filebuf(std::FILE* f): file_(f), owned_(false), last_(traits_type::eof()) { }

filebuf::~filebuf() { close(); }

filebuf* filebuf::open(const char* name, const char* mode)

{

    if (file_) return 0; /* already open */
    std::FILE* f = ami_fopen(name, mode);
    if (!f) return 0;
    file_ = f;
    owned_ = true;
    last_ = traits_type::eof();

    return this;

}

filebuf* filebuf::open(const char* name, std::ios_base::openmode mode)

{

    const char* m = fopenmode(mode);

    if (!m) return 0;
    if (!open(name, m)) return 0;
    /* at end: the standard's "ate" seeks to the end after the open */
    if ((mode & std::ios_base::ate) && ami_fseek(file_, 0, SEEK_END)) {

        close();
        return 0;

    }

    return this;

}

filebuf* filebuf::attach(std::FILE* f)

{

    close();
    file_ = f;
    owned_ = false;
    last_ = traits_type::eof();

    return this;

}

filebuf* filebuf::close()

{

    if (!file_) return 0;
    std::FILE* f = file_;
    bool owned = owned_;
    file_ = 0;
    owned_ = false;
    if (owned) { if (ami_fclose(f)) return 0; }
    else if (ami_fflush(f)) return 0;

    return this;

}

filebuf::int_type filebuf::overflow(int_type c)

{

    if (!file_) return traits_type::eof();
    if (traits_type::eq_int_type(c, traits_type::eof()))
        return ami_fflush(file_) ? traits_type::eof()
                                 : traits_type::not_eof(c);
    int r = ami_fputc(traits_type::to_char_type(c), file_);

    return r == EOF ? traits_type::eof() : traits_type::to_int_type((char)r);

}

std::streamsize filebuf::xsputn(const char_type* s, std::streamsize n)

{

    if (!file_ || n <= 0) return 0;

    return (std::streamsize)ami_fwrite(s, (size_t)n, file_);

}

filebuf::int_type filebuf::underflow()

{

    /* peek: take the character and give it back */
    if (!file_) return traits_type::eof();
    int c = ami_fgetc(file_);
    if (c == EOF) return traits_type::eof();
    ami_ungetc(c, file_);

    return traits_type::to_int_type((char)c);

}

filebuf::int_type filebuf::uflow()

{

    if (!file_) return traits_type::eof();
    int c = ami_fgetc(file_);
    if (c == EOF) return traits_type::eof();
    last_ = traits_type::to_int_type((char)c);

    return last_;

}

filebuf::int_type filebuf::pbackfail(int_type c)

{

    /* put back the character given, or the last one read when none is */
    if (!file_) return traits_type::eof();
    int_type b = traits_type::eq_int_type(c, traits_type::eof()) ? last_ : c;
    if (traits_type::eq_int_type(b, traits_type::eof())) return b;
    int r = ami_ungetc(traits_type::to_char_type(b), file_);
    last_ = traits_type::eof(); /* one push back is all the stdio keeps */

    return r == EOF ? traits_type::eof() : traits_type::to_int_type((char)r);

}

std::streamsize filebuf::xsgetn(char_type* s, std::streamsize n)

{

    if (!file_ || n <= 0) return 0;
    std::streamsize r = (std::streamsize)ami_fread(s, (size_t)n, file_);
    if (r > 0) last_ = traits_type::to_int_type(s[r-1]);

    return r;

}

int filebuf::sync()

{

    if (!file_) return -1;

    return ami_fflush(file_) ? -1 : 0;

}

filebuf::pos_type filebuf::seekoff(off_type off, std::ios_base::seekdir dir,
                                   std::ios_base::openmode)

{

    if (!file_) return pos_type(off_type(-1));
    int whence = dir == std::ios_base::beg ? SEEK_SET :
                 dir == std::ios_base::end ? SEEK_END : SEEK_CUR;
    if (ami_fseek(file_, (long)off, whence)) return pos_type(off_type(-1));

    return pos_type(ami_ftell(file_));

}

filebuf::pos_type filebuf::seekpos(pos_type pos, std::ios_base::openmode which)

{

    return seekoff(off_type(pos), std::ios_base::beg, which);

}

/*******************************************************************************

File streams

Each holds its filebuf and is a standard stream over it. A failed open sets
failbit, as the standard file streams do.

*******************************************************************************/

ifstream::ifstream(): std::istream(0) { init(&buf_); }
ifstream::ifstream(const char* name, std::ios_base::openmode mode): std::istream(0)
    { init(&buf_); open(name, mode); }
ifstream::ifstream(std::FILE* f): std::istream(0), buf_(f) { init(&buf_); }
ifstream::~ifstream() { }
void ifstream::open(const char* name, std::ios_base::openmode mode)
    { if (!buf_.open(name, mode | std::ios_base::in)) setstate(failbit); else clear(); }
void ifstream::attach(std::FILE* f) { buf_.attach(f); clear(); }
void ifstream::close() { if (!buf_.close()) setstate(failbit); }

ofstream::ofstream(): std::ostream(0) { init(&buf_); }
ofstream::ofstream(const char* name, std::ios_base::openmode mode): std::ostream(0)
    { init(&buf_); open(name, mode); }
ofstream::ofstream(std::FILE* f): std::ostream(0), buf_(f) { init(&buf_); }
ofstream::~ofstream() { }
void ofstream::open(const char* name, std::ios_base::openmode mode)
    { if (!buf_.open(name, mode | std::ios_base::out)) setstate(failbit); else clear(); }
void ofstream::attach(std::FILE* f) { buf_.attach(f); clear(); }
void ofstream::close() { if (!buf_.close()) setstate(failbit); }

fstream::fstream(): std::iostream(0) { init(&buf_); }
fstream::fstream(const char* name, std::ios_base::openmode mode): std::iostream(0)
    { init(&buf_); open(name, mode); }
fstream::fstream(std::FILE* f): std::iostream(0), buf_(f) { init(&buf_); }
fstream::~fstream() { }
void fstream::open(const char* name, std::ios_base::openmode mode)
    { if (!buf_.open(name, mode)) setstate(failbit); else clear(); }
void fstream::attach(std::FILE* f) { buf_.attach(f); clear(); }
void fstream::close() { if (!buf_.close()) setstate(failbit); }

} /* namespace stdio */

/*******************************************************************************

The standard streams

One object, constructed at init priority 101, puts filebufs on Petit-Ami's
stdin, stdout and stderr into the four standard streams and keeps the
streambufs they had. Its destructor, run after the program's own static
destructors, flushes the output streams and puts the originals back, so the
C++ library's own shutdown finds the streams as it made them.

The ios_base::Init member comes first: on a C++ library that makes the
standard streams from the program's units rather than within itself, it
makes them before they are touched here. The streambufs are members of the
same object, so they live exactly as long as they are installed.

*******************************************************************************/

namespace {

struct installer {

    std::ios_base::Init ioinit;    /* the standard streams exist */
    stdio::filebuf      inbuf;     /* Petit-Ami stdin */
    stdio::filebuf      outbuf;    /* Petit-Ami stdout */
    stdio::filebuf      errbuf;    /* Petit-Ami stderr */
    std::streambuf*     oldin;     /* what the streams had */
    std::streambuf*     oldout;
    std::streambuf*     olderr;
    std::streambuf*     oldlog;

    installer():
        inbuf(ami_stdin()), outbuf(ami_stdout()), errbuf(ami_stderr())
    {

        oldin  = std::cin.rdbuf(&inbuf);
        oldout = std::cout.rdbuf(&outbuf);
        olderr = std::cerr.rdbuf(&errbuf);
        oldlog = std::clog.rdbuf(&errbuf);

    }

    ~installer()
    {

        std::cout.flush();
        std::cerr.flush();
        std::clog.flush();
        std::cin.rdbuf(oldin);
        std::cout.rdbuf(oldout);
        std::cerr.rdbuf(olderr);
        std::clog.rdbuf(oldlog);

    }

};

installer streams __attribute__((init_priority(101)));

}

extern "C" int ami_iostream(void)

{

    return std::cout.rdbuf() == &streams.outbuf;

}
