/*******************************************************************************
*                                                                              *
*                         Services Library Header                              *
*                                                                              *
*                              Created 1996                                    *
*                                                                              *
*                               S. A. MOORE                                    *
*                                                                              *
*******************************************************************************/

#ifndef __SERVICES_H__
#define __SERVICES_H__

/*
 * Set manipulation operators for chrset.
 *
 * These are used to change the character set that defines what characters
 * are admissible in filenames.
 */
#define INSET(b, s) (s[b>>3] & 1<<b%8) /* test inclusion */
#define ADDSET(b, s) (s[b>>3] | 1<<b%8) /* add set member */
#define SUBSET(b, s) (s[b>>3] & ~(1<<b%8)) /* remove set member */
#define CLRSET(s) { int i; for (i = 0; i < 32; i++) s[i] = 0; } /* clear set */

/* attributes */
typedef enum {

    atexec, /* is an executable file type */
    atarc,  /* has been archived since last modification */
    atsys,  /* is a system special file */
    atdir,  /* is a directory special file */
    atloop  /* contains heriarchy loop */

} attribute;
typedef int attrset; /* attributes in a set */

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
typedef int permset; /* permissions in a set */

/* standard directory format */
typedef struct filrec {

    char    *name;   /* name of file */
    int     size;   /* size of file */
    int     alloc;   /* allocation of file */
    attrset attr;   /* attributes */
    int     create;   /* time of creation */
    int     modify;   /* time of last modification */
    int     access;   /* time of last access */
    int     backup;   /* time of last backup */
    permset user;   /* user permissions */
    permset group;   /* group permissions */
    permset other;   /* other permissions */
    struct filrec *next; /* next entry in list */

} filrec;
typedef filrec* filptr; /* pointer to file records */

/* environment strings */
typedef struct envrec {

    char* name; /* name of string */
    char* data; /* data in string */
    struct envrec *next; /* next entry in list */

} envrec;
typedef envrec* envptr; /* pointer to environment record */

/* character set */
typedef unsigned char chrset[32];

/*
 * Functions exposed in the services module
 */
extern void pa_list(char* f, filrec **l);
extern void pa_times(char* s, int t);
extern void pa_dates(char* s, int t);
extern void pa_writetime(FILE *f, int t);
extern void pa_writedate(FILE *f, int t);
extern int  pa_time(void);
extern int  pa_local(int t);
extern int  pa_clock(void);
extern int  pa_elapsed(int r);
extern int  pa_validfile(char* s);
extern int  pa_validpath(char* s);
extern int  pa_wild(char* s);
extern void pa_getenv(char* ls, char* ds);
extern void pa_setenv(char* sn, char* sd);
extern void pa_allenv(envrec **el);
extern void pa_remenv(char* sn);
extern void pa_exec(char* cmd);
extern void pa_exece(char* cmd, envrec *el);
extern void pa_execw(char* cmd, int *e);
extern void pa_execew(char* cmd, envrec *el, int *e);
extern void pa_getcur(char* fn);
extern void pa_setcur(char* fn);
extern void pa_brknam(char* fn, char* p, char* n, char* e);
extern void pa_maknam(char* fn, char* p, char* n, char* e);
extern void pa_fulnam(char* fn);
extern void pa_getpgm(char* p);
extern void pa_getusr(char* fn);
extern void pa_setatr(char* fn, attrset a);
extern void pa_resatr(char* fn, attrset a);
extern void pa_bakupd(char* fn);
extern void pa_setuper(char* fn, permset p);
extern void pa_resuper(char* fn, permset p);
extern void pa_setgper(char* fn, permset p);
extern void pa_resgper(char* fn, permset p);
extern void pa_setoper(char* fn, permset p);
extern void pa_resoper(char* fn, permset p);
extern void pa_makpth(char* fn);
extern void pa_rempth(char* fn);
extern void pa_filchr(chrset* fc);
extern char pa_optchr(void);
extern char pa_pthchr(void);
extern int  pa_latitude(void);
extern int  pa_longitude(void);
extern int  pa_altitude(void);
extern int  pa_country(void);
extern void pa_countrys(char* s, int c);
extern int  pa_timezone(void);
extern int  pa_daysave(void);
extern int  pa_time24hour(void);
extern int  pa_language(void);
extern void pa_languages(char* s, int l);
extern char pa_decimal(void);
extern char pa_numbersep(void);
extern int  pa_timeorder(void);
extern int  pa_dateorder(void);
extern char pa_datesep(void);
extern char pa_timesep(void);
extern char pa_currchr(void);

#endif
