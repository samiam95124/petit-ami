/*******************************************************************************
*                                                                              *
*                            Services Library                                  *
*                                                                              *
*                              Created 1996                                    *
*                                                                              *
*                               S. A. MOORE                                    *
*                                                                              *
*******************************************************************************/

#ifndef __SERVICES_H__
#define __SERVICES_H__

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
typdef filrec* filptr; /* pointer to file records */

/* environment strings */
typedef struct envrec {

    char *name; /* name of string */
    char *data; /* data in string */
    struct envrec *next; /* next entry in list */

} envrec;
typedef envrec* envptr; /* pointer to environment record */

/* character set */
typedef unsigned char chrset[32];

/*
 * Functions exposed in the services module
 */
extern void list(char *f, filrec **l);
extern void times(char *s, int t);
extern void dates(char *s, int t);
extern void writetime(FILE *f, int t);
extern void writedate(FILE *f, int t);
extern int time(void);
extern int local(int t);
extern int clock(void);
extern int elapsed(int r);
extern int validfile(char *s);
extern int validpath(char *s);
extern int wild(char *s);
extern void getenv(char *ls, char *ds);
extern void setenv(char *sn, char *sd);
extern void allenv(envrec **el);
extern void remenv(char *sn);
extern void exec(char *cmd);
extern void exece(char *cmd, envrec *el);
extern void execw(char *cmd, int *e);
extern void execew(char *cmd, envrec *el, int *e);
extern void getcur(char *fn);
extern void setcur(char *fn);
extern void brknam(char *fn, char *p, char *n, char *e);
extern void maknam(char *fn, char *p, char *n, char *e);
extern void fulnam(char *fn);
extern void getpgm(char *p);
extern void getusr(char *fn);
extern void setatr(char *fn, attrset a);
extern void resatr(char *fn, attrset a);
extern void bakupd(char *fn);
extern void setuper(char *fn, permset p);
extern void resuper(char *fn, permset p);
extern void setgper(char *fn, permset p);
extern void resgper(char *fn, permset p);
extern void setoper(char *fn, permset p);
extern void resoper(char *fn, permset p);
extern void seterr(int c);
extern void makpth(char *fn);
extern void rempth(char *fn);
extern void filchr(chrset* fc);
extern char optchr(void);
extern char pthchr(void);

#endif