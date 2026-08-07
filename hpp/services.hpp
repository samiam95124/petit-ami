/** ****************************************************************************
 *
 * Services library interface C++ wrapper declarations
 *
 * See cpp/services.cpp for the rationale. The types of the C header
 * appear here with the ami_ prefixes stripped, laid out identically to
 * their C forms: the wrapper casts between them.
 *
 ******************************************************************************/

#ifndef __SERVICES_HPP__
#define __SERVICES_HPP__

extern "C" {

#include <stdio.h>

}

namespace services {

/* length of character set */
const long csetlen = 32;

/* attributes */
typedef enum {

    atexec, /* is an executable file type */
    atarc,  /* has been archived since last modification */
    atsys,  /* is a system special file */
    atdir,  /* is a directory special file */
    atloop  /* contains heriarchy loop */

} attribute;
typedef long attrset; /* attributes in a set */

/* permissions */
typedef enum {

    pmread,  /* may be read */
    pmwrite, /* may be written */
    pmexec,  /* may be executed */
    pmdel,   /* may be deleted */
    pmvis,   /* may be seen in directory listings */
    pmcopy,  /* may be copied */
    pmren    /* may be renamed/moved */

} permission;
typedef long permset; /* permissions in a set */

/* standard directory format */
typedef struct filrec {

    char*          name;    /* name of file (zero terminated) */
    long           namel;   /* length of filename */
    long long      size;    /* size of file */
    long long      alloc;   /* allocation of file */
    attrset        attr;    /* attributes */
    long           create;  /* time of creation */
    long           modify;  /* time of last modification */
    long           access;  /* time of last access */
    long           backup;  /* time of last backup */
    permset        user;    /* user permissions */
    permset        group;   /* group permissions */
    permset        other;   /* other permissions */
    struct filrec* next;    /* next entry in list */

} filrec;
typedef filrec* filptr; /* pointer to file records */

/* environment strings */
typedef struct envrec {

    char* name;          /* name of string (zero terminated) */
    char* data;          /* data in string (zero terminated) */
    struct envrec *next; /* next entry in list */

} envrec;
typedef envrec* envptr; /* pointer to environment record */

/* character set */
typedef unsigned char chrset[csetlen];

/* the set operators of the C header, as inline functions */
inline long bit(long b) { return 1l<<b; }
inline long incset(const unsigned char* s, long b)
    { return !!(s[b>>3] & bit(b%8)); }
inline void addcset(unsigned char* s, long b) { s[b>>3] |= bit(b%8); }
inline void subcset(unsigned char* s, long b) { s[b>>3] &= ~bit(b%8); }
inline void clrcset(unsigned char* s)
    { for (long i = 0; i < csetlen; i++) s[i] = 0; }
inline long iniset(long s, long b) { return !!(s & bit(b)); }
inline long addiset(long s, long b) { return s | bit(b); }
inline long subiset(long s, long b) { return s & ~bit(b); }

/* procedures and functions */
void list(const char* f, filptr* lp);
void times(char* s, long sl, long t);
void dates(char* s, long sl, long t);
void writetime(FILE* f, long t);
void writedate(FILE* f, long t);
long time(void);
long local(long t);
long clock(void);
long elapsed(long r);
long validfile(const char* s);
long validpath(const char* s);
long wild(const char* s);
void getenv(const char* ls, char* ds, long dsl);
void setenv(const char* sn, const char* sd);
void allenv(envptr* el);
void remenv(const char* sn);
void exec(const char* cmd);
void exece(const char* cmd, envptr el);
void execw(const char* cmd, long* e);
void execew(const char* cmd, envptr el, long* e);
void getcur(char* fn, long l);
void setcur(const char* fn);
void brknam(const char* fn, char* p, long pl, char* n, long nl, char* e, long el);
void maknam(char* fn, long fnl, const char* p, const char* n, const char* e);
void fulnam(char* fn, long fnl);
void getpgm(char* p, long pl);
void getusr(char* fn, long fnl);
void setatr(const char* fn, attrset at);
void resatr(const char* fn, attrset at);
void bakupd(const char* fn);
void setuper(const char* fn, permset p);
void resuper(const char* fn, permset p);
void setgper(const char* fn, permset p);
void resgper(const char* fn, permset p);
void setoper(const char* fn, permset p);
void resoper(const char* fn, permset p);
void makpth(const char* fn);
void rempth(const char* fn);
void filchr(chrset fc);
char optchr(void);
char pthchr(void);
long latitude(void);
long longitude(void);
long altitude(void);
long country(void);
void countrys(char* s, long sl, long c);
long timezone(void);
long daysave(void);
long time24hour(void);
long language(void);
void languages(char* s, long sl, long l);
char decimal(void);
char numbersep(void);
long timeorder(void);
long dateorder(void);
char datesep(void);
char timesep(void);
char currchr(void);
long newthread(void (*threadmain)(void));
long initlock(void);
void deinitlock(long ln);
void lock(long ln);
void unlock(long ln);
long initsig(void);
void deinitsig(long sn);
void sendsig(long sn);
void sendsigone(long sn);
void waitsig(long ln, long sn);

/*******************************************************************************

The thread, mutex and signal objects

A thread starts on construction and runs the function it was given; the
services interface has no join or kill, so the object is the start and
the identifier. A mutex is made by its constructor and freed by its
destructor, and locks and unlocks by the C verbs: m.lock(), m.unlock().
The class is called mutex rather than lock only because C++ will not
let a class and its method share a name. A signal likewise lives from
constructor to destructor, sends with sendsig() or sendsigone(), and
waits on a mutex with waitsig(m).

These hold identifiers, not hooks: any number of each may exist.

*******************************************************************************/

class thread {

long tid; /* the thread identifier */

public:

/* constructor: the thread starts here */
thread(void (*threadmain)(void));

/* the identifier, as newthread returned it */
long id(void);

}; /* class thread */

class mutex {

friend class signal;

long ln; /* the lock identifier */

public:

/* constructor */
mutex();

/* destructor */
~mutex();

/* methods */
void lock(void);
void unlock(void);

}; /* class mutex */

class signal {

long sn; /* the signal identifier */

public:

/* constructor */
signal();

/* destructor */
~signal();

/* methods */
void sendsig(void);
void sendsigone(void);
void waitsig(mutex& m);

}; /* class signal */

} /* namespace services */

#endif /* __SERVICES_HPP__ */
