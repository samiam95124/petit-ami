/** ****************************************************************************
 *
 * Services library interface C++ wrapper
 *
 * Wraps the calls in services with C++ conventions, the same way
 * cpp/terminal.cpp wraps the terminal calls. This brings:
 *
 * 1. The functions and types do not need an "ami_" prefix: the services
 * namespace does the isolation.
 *
 * 2. The threading pieces become objects. A thread starts on
 * construction; a mutex and a signal are made by their constructors,
 * freed by their destructors, and speak the C verbs -- m.lock(),
 * m.unlock(), s.sendsig(), s.waitsig(m) -- so a lock cannot leak past
 * an early return and an identifier never outlives what it names.
 *
 * The rest of services is stateless calls on the world -- files, paths,
 * environment, time, measurement -- and a namespace serves those
 * better than objects would.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

extern "C" {

#include <stdio.h>

#include <services.h>

}

#include "services.hpp"

namespace services {

/* procedures and functions */
void list(const char* f, filptr* lp) { ami_list((char*)f, (ami_filrec**)lp); }
void times(char* s, long sl, long t) { ami_times(s, sl, t); }
void dates(char* s, long sl, long t) { ami_dates(s, sl, t); }
void writetime(FILE* f, long t) { ami_writetime(f, t); }
void writedate(FILE* f, long t) { ami_writedate(f, t); }
long time(void) { return ami_time(); }
long local(long t) { return ami_local(t); }
long clock(void) { return ami_clock(); }
long elapsed(long r) { return ami_elapsed(r); }
long validfile(const char* s) { return ami_validfile((char*)s); }
long validpath(const char* s) { return ami_validpath((char*)s); }
long wild(const char* s) { return ami_wild((char*)s); }
void getenv(const char* ls, char* ds, long dsl) { ami_getenv((char*)ls, ds, dsl); }
void setenv(const char* sn, const char* sd) { ami_setenv((char*)sn, (char*)sd); }
void allenv(envptr* el) { ami_allenv((ami_envrec**)el); }
void remenv(const char* sn) { ami_remenv((char*)sn); }
void exec(const char* cmd) { ami_exec((char*)cmd); }
void exece(const char* cmd, envptr el) { ami_exece((char*)cmd, (ami_envrec*)el); }
void execw(const char* cmd, long* e) { ami_execw((char*)cmd, e); }
void execew(const char* cmd, envptr el, long* e) { ami_execew((char*)cmd, (ami_envrec*)el, e); }
void getcur(char* fn, long l) { ami_getcur(fn, l); }
void setcur(const char* fn) { ami_setcur((char*)fn); }
void brknam(const char* fn, char* p, long pl, char* n, long nl, char* e, long el) { ami_brknam((char*)fn, p, pl, n, nl, e, el); }
void maknam(char* fn, long fnl, const char* p, const char* n, const char* e) { ami_maknam(fn, fnl, (char*)p, (char*)n, (char*)e); }
void fulnam(char* fn, long fnl) { ami_fulnam(fn, fnl); }
void getpgm(char* p, long pl) { ami_getpgm(p, pl); }
void getusr(char* fn, long fnl) { ami_getusr(fn, fnl); }
void setatr(const char* fn, attrset at) { ami_setatr((char*)fn, at); }
void resatr(const char* fn, attrset at) { ami_resatr((char*)fn, at); }
void bakupd(const char* fn) { ami_bakupd((char*)fn); }
void setuper(const char* fn, permset p) { ami_setuper((char*)fn, p); }
void resuper(const char* fn, permset p) { ami_resuper((char*)fn, p); }
void setgper(const char* fn, permset p) { ami_setgper((char*)fn, p); }
void resgper(const char* fn, permset p) { ami_resgper((char*)fn, p); }
void setoper(const char* fn, permset p) { ami_setoper((char*)fn, p); }
void resoper(const char* fn, permset p) { ami_resoper((char*)fn, p); }
void makpth(const char* fn) { ami_makpth((char*)fn); }
void rempth(const char* fn) { ami_rempth((char*)fn); }
void filchr(chrset fc) { ami_filchr(fc); }
char optchr(void) { return ami_optchr(); }
char pthchr(void) { return ami_pthchr(); }
long latitude(void) { return ami_latitude(); }
long longitude(void) { return ami_longitude(); }
long altitude(void) { return ami_altitude(); }
long country(void) { return ami_country(); }
void countrys(char* s, long sl, long c) { ami_countrys(s, sl, c); }
long timezone(void) { return ami_timezone(); }
long daysave(void) { return ami_daysave(); }
long time24hour(void) { return ami_time24hour(); }
long language(void) { return ami_language(); }
void languages(char* s, long sl, long l) { ami_languages(s, sl, l); }
char decimal(void) { return ami_decimal(); }
char numbersep(void) { return ami_numbersep(); }
long timeorder(void) { return ami_timeorder(); }
long dateorder(void) { return ami_dateorder(); }
char datesep(void) { return ami_datesep(); }
char timesep(void) { return ami_timesep(); }
char currchr(void) { return ami_currchr(); }
long newthread(void (*threadmain)(void)) { return ami_newthread(threadmain); }
long initlock(void) { return ami_initlock(); }
void deinitlock(long ln) { ami_deinitlock(ln); }
void lock(long ln) { ami_lock(ln); }
void unlock(long ln) { ami_unlock(ln); }
long initsig(void) { return ami_initsig(); }
void deinitsig(long sn) { ami_deinitsig(sn); }
void sendsig(long sn) { ami_sendsig(sn); }
void sendsigone(long sn) { ami_sendsigone(sn); }
void waitsig(long ln, long sn) { ami_waitsig(ln, sn); }

/* methods */
thread::thread(void (*threadmain)(void))

{

    tid = ami_newthread(threadmain);

}


mutex::mutex(void)

{

    ln = ami_initlock();

}

mutex::~mutex(void)

{

    ami_deinitlock(ln);

}

void mutex::lock(void) { ami_lock(ln); }
void mutex::unlock(void) { ami_unlock(ln); }

signal::signal(void)

{

    sn = ami_initsig();

}

signal::~signal(void)

{

    ami_deinitsig(sn);

}

void signal::sendsig(void) { ami_sendsig(sn); }
void signal::sendsigone(void) { ami_sendsigone(sn); }
void signal::waitsig(mutex& m) { ami_waitsig(m.ln, sn); }

} /* namespace services */
