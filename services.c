/********************************************************************************
*                                                                              *
*                       UNIX EXTENDED FUNCTION LIBRARY                         *
*                                                                              *
*                              Created 1996                                    *
*                                                                              *
*                               S. A. MOORE                                    *
*                                                                              *
* Contains various system oriented library functions, including files,         *
* directories, time, program execution, environment, and random numbers.       *
* This implementation is specific to the Unix system, but extlib tends to      *
* have processing elements that are universal.                                 *
*                                                                              *
********************************************************************************/

#include "errno".h"
#include "sys/types.h"
#include "sys/status.h"
#include "fcntl.h"
#include "dirend.h"

#include "services.h" /* the header for this file */

#define hoursec         3600   /* number of seconds in an hour */
#define daysec          (hoursec * 24)   /* number of seconds in a day */
#define yearsec         (daysec * 365)   /* number of seconds in year */
/* Unix time adjustment for 1970 */
#define unixadj         (yearsec * 30 + daysec * 7)

/* maximum size of holding buffers (I had to make this very large for large
   paths [sam]) */
#define maxstr          500

typedef char bufstr[maxstr]; /* standard string buffer */

static bufstr pthstr;   /* buffer for execution path */
static char **ep;   /* unix environment string table */
static int ei;   /* index for string table */
static int si;   /* index for strings */
static envrec *envlst;   /* our environment list */
static envrec *p;   /* environment entry pointer */

/* dummy routine placeholders */
static void cat(char *d, char *s1, char *s2) { }
static int index_(char *s, char *m) { }
static void clears(char *s) { }
static void copys(char *d, char *s) { }
static int comps(char *d, char *s) { }
static int words(char *s) { }
static void extword(char *d, char *s, int st, int ed) { }
static void extract(char *d, char *s, int st, int ed) { }
static int exists(char *fn) { }
static void trim(char *d, char *s) { }

/********************************************************************************

Process string library error

Outputs an error message using the special syslib function, then halts.

********************************************************************************/

static void error(char *s)
{

    fprintf(stderr, "\nError: Services: %s\n", s);

    exit(1);

}


/********************************************************************************

Handle Unix error

Looks up the given error number to find a message, then prints the message
as an error. This routine relys on errno being set, and so must be called
directly after encountering a Unix error.

Note that error numbers are usually passed back as negative numbers by Unix, so
they should be negated before calling this routine.

********************************************************************************/

static void unixerr(void)
{

    fprintf(stderr, "\nError: Services: %s\n", strerror(errno));

}


/********************************************************************************

Place character in string

Places the given character in the space padded string buffer, with full error
checking.

********************************************************************************/

static void plcstr(char *s, int *i, char c)
{
    /* check overflow */
    if (*i > strlen(s))   /* overflow */
    error("Name too long for buffer");
    s[*i - 1] = c;   /* place character */
    (*i)++;   /* next */

}


/* match with wildcards at the given a and b positions. we use shortest string
   first matching */

static int match(char *a, char *b, int ia, int ib)
{
    int m;


    m = 1;   /* default to matches */
    while (ia < strlen(a) && ib < strlen(b) && m) {  /* match characters */
    if (a[ia-1] == '*') {  /* multicharacter wildcard, go searching */
    /* skip all wildcards in match expression name. For each '*' or
       '?', we skip one character in the matched name. The idea being
       that '*' means 1 or more matched characters */
    while (ia < strlen(a) && (a[ia-1] == '?' || a[ia-1] == '*')) {
    ia++;   /* next character */
    ib++;

}

    /* recursively match to string until we find a match for the rest
       or run out of string */
    while (ib < strlen(b) && !match(a, b, ia, ib))
    ib++;
    if (ib >= strlen(b))   /* didn't match, set false */
    m = 0;
    ib = strlen(b);   /* terminate */

}

    else if (a[ia-1] != b[ib-1] && a[ia-1] != '?')
    m = 0;
    /* fail match */
    if (!m)
    break;


    ia++;   /* next character */
    ib++;

}

    return m;

}


/********************************************************************************

Create file list

Accepts a filename, that may include wildcards. All of the matching files are
found, and a list of file entries is returned. The file entries are in standard
directory format.

The entries are allocated from general storage, and both the entry and the
filename should be disposed of by the caller when they are no longer needed.
If no files are matched, the returned list is nil.

********************************************************************************/

void list(
    /** file to search for */ char *f,
    /** file list returned */ filrec **l
)

{

    struct dirent dr; /* Unix directory record */
    int           fd; /* directory file descriptor */
    int           r;  /* result code */
    int           rd;
    sc_sstat      sr; /* stat() record */
    filrec*       fp; /* file entry pointer */
    filrec*       lp; /* last entry pointer */
    int           i;  /* name index */
    bufstr        p;  /* filename components */
    bufstr        n;
    bufstr        e;
    bufstr        fn; /* holder for directory name */
    bufstr        dn; /* holder for directory entry */
    char STR1[256];

    *l = NULL;   /* clear destination list */
    lp = NULL;   /* clear last pointer */
    brknam(f, p, n, e);   /* break up filename */
    /* check wildcards in path */
    if (index_(p, "*") != 0 || index_(p, "?") != 0)
    error("Path cannot contain wildcards");
    /* construct name of containing directory */
    maknam(fn, p, ".", "");
    fd = sc_open(fn, O_RDONLY, 0);   /* open the directory */
    if (fd < 0)   /* process unix open error */
    unixerr();
    maknam(fn, "", n, e);   /* reform name without path */
    do {   /* read directory entries */

        rd = sc_readdir(fd, &dr, 1);
        if (rd < 0) unixerr(); /* process unix error */
        if (rd == 1) {  /* valid next */

            /* copy to standard string */
            clears(dn);   /* clear it */
            i = 1;   /* set 1st character */
            while (i < sc_dirlen && dr.d_name[i-1] != '\0') {

                dn[i-1] = dr.d_name[i-1];   /* copy character */
                i++;   /* next */

            }
            if (match(fn, dn, 1, 1)) {  /* matching filename, add to list */
            fp = Malloc(sizeof(filrec));   /* create a new file entry */
            sprintf(STR1, "%.*s", sc_dirlen, dr.d_name);
            r = sc_stat(STR1, &sr);   /* get stat structure on file */
            if (r < 0)   /* process unix error */
            unixerr();
            /* file information in stat record, translate to our format */
            copys(fp->name, dn);   /* place filename */
            fp->size = sr.st_size;   /* place size */
            /* there is actually a real unix allocation, but I haven't figgured out
               how to calculate it from block/blocksize */
            fp->alloc = sr.st_size;   /* place allocation */
            fp->attr = 0;   /* clear attributes */
            /* clear permissions to all is allowed */
            fp->user = (1 << ((int)pmread)) | (1 << ((int)pmwrite)) |
                (1 << ((int)pmexec)) | (1 << ((int)pmdel)) | (1 << ((int)pmvis)) |
                (1 << ((int)pmcopy)) | (1 << ((int)pmren));
            fp->other = (1 << ((int)pmread)) | (1 << ((int)pmwrite)) |
                (1 << ((int)pmexec)) | (1 << ((int)pmdel)) | (1 << ((int)pmvis)) |
                (1 << ((int)pmcopy)) | (1 << ((int)pmren));
            fp->group = (1 << ((int)pmread)) | (1 << ((int)pmwrite)) |
                (1 << ((int)pmexec)) | (1 << ((int)pmdel)) | (1 << ((int)pmvis)) |
                (1 << ((int)pmcopy)) | (1 << ((int)pmren));
            /* check and set directory attribute */
            if ((sr.st_mode & sc_s_ifdir) != 0)
            fp->attr |= 1 << ((int)atdir);
            /* check and set any system special file */
            if ((sr.st_mode & sc_s_ififo) != 0)
            fp->attr |= 1 << ((int)atsys);
            if ((sr.st_mode & sc_s_ifchr) != 0)
            fp->attr |= 1 << ((int)atsys);
            if ((sr.st_mode & sc_s_ifblk) != 0)
            fp->attr |= 1 << ((int)atsys);
            /* check hidden. in Unix, this is done with a leading '.'. We remove
               visiblity priveledges */
            if (dr.d_name[0] == '.') {
            fp->user &= ~(1 << ((int)pmvis));
            fp->group &= ~(1 << ((int)pmvis));
            fp->other &= ~(1 << ((int)pmvis));

        }

            /* check and set executable attribute. Unix has separate executable
               permissions for each permission type, we set executable if any of
               them are true */
            if ((sr.st_mode & sc_s_iexec) != 0)
            fp->attr |= 1 << ((int)atexec);
            /* set execute permissions to user */
            if ((sr.st_mode & sc_s_iexec) == 0)
            fp->user &= ~(1 << ((int)pmexec));
            /* set read permissions to user */
            if ((sr.st_mode & sc_s_iread) == 0)
            fp->user &= ~(1 << ((int)pmread));
            /* set write permissions to user */
            if ((sr.st_mode & sc_s_iwrite) == 0)
            fp->user &= ~(1 << ((int)pmwrite));
            /* set execute permissions to group */
            if ((sr.st_mode & sc_s_igexec) == 0)
            fp->group &= ~(1 << ((int)pmexec));
            /* set read permissions to group */
            if ((sr.st_mode & sc_s_igread) == 0)
            fp->group &= ~(1 << ((int)pmread));
            /* set write permissions to group */
            if ((sr.st_mode & sc_s_igwrite) == 0)
            fp->group &= ~(1 << ((int)pmwrite));
            /* set execute permissions to other */
            if ((sr.st_mode & sc_s_ioexec) == 0)
            fp->other = fp->group & (~(1 << ((int)pmexec)));
            /* set read permissions to other */
            if ((sr.st_mode & sc_s_ioread) == 0)
            fp->other = fp->group & (~(1 << ((int)pmread)));
            /* set write permissions to other */
            if ((sr.st_mode & sc_s_iowrite) == 0)
            fp->other = fp->group & (~(1 << ((int)pmwrite)));
            /* set times */
            fp->create = sr.st_ctime - unixadj;
            fp->modify = sr.st_mtime - unixadj;
            fp->access = sr.st_atime - unixadj;
            fp->backup = -INT_MAX;   /* no backup time for Unix */
            /* insert entry to list */
            if (*l == NULL)
            *l = fp;   /* insert new top */
            else
            lp->next = fp;
            /* insert next entry */
            lp = fp;   /* set new last */
            fp->next = NULL;   /* clear next */

        }


    }


} while (rd == 1);
    r = sc_close(fd);
    if (r < 0)   /* process unix error */
    unixerr();

}


/* Local variables for times_: */
struct LOC_times_ {
    char *s;
    int i;   /* index for string */
} ;

static void wrtzer(int v, struct LOC_times_ *LINK)
{
    LINK->s[LINK->i-1] = v / 10 + '0';
    LINK->i++;
    LINK->s[LINK->i-1] = v % 10 + '0';
/* p2c: services.pas, line 331:
 * Note: Using % for possibly-negative arguments [317] */
    LINK->i++;

}


/********************************************************************************

Get time string padded

Converts the given time into a padded string.

********************************************************************************/

static void times_(char *s_, int t)
{
    /* result string */
    /* time to convert */
    struct LOC_times_ V;
    char h;   /* hour */
    char m;   /* minute */
    char sec;   /* second */
    int pm;   /* am/pm flag */
    int FORLIM;




    V.s = s_;
    if (strlen(V.s) < 11)   /* string to small to hold result */
    error("*** String to small to hold time");
    FORLIM = strlen(V.s);
    for (V.i = 1; V.i <= FORLIM; V.i++)   /* clear result */
    V.s[V.i-1] = ' ';
    V.i = 1;   /* set 1st string place */
    /* because leap adjustments are made in terms of days, we just remove
       the days to find the time of day in seconds. this is completely
       independent of leap adjustments */
    t %= daysec;   /* find number of seconds in day */
/* p2c: services.pas, line 345:
 * Note: Using % for possibly-negative arguments [317] */
    /* if before 2000, find from remaining seconds */
    if (t < 0)
    t += daysec;
    h = t / hoursec;   /* find hours */
    t %= hoursec;   /* subtract hours */
/* p2c: services.pas, line 349:
 * Note: Using % for possibly-negative arguments [317] */
    m = t / 60;   /* find minutes */
    sec = t % 60;   /* find seconds */
/* p2c: services.pas, line 351:
 * Note: Using % for possibly-negative arguments [317] */
    pm = 0;   /* set am */
    if (h == 0)
    h = 12;   /* hour zero */
    else if (h > 12) {
    h -= 12;
    pm = 1;
}  /* 1 pm to 11 pm */
    wrtzer(h, &V);   /* place hour */
    if (V.s[0] == '0')   /* clear leading zero */
    V.s[0] = ' ';
    V.s[V.i-1] = ':';
    V.i++;
    wrtzer(m, &V);
    V.s[V.i-1] = ':';
    V.i++;
    wrtzer(sec, &V);
    V.i++;
    if (pm) {
    V.s[V.i-1] = 'p';
    V.i++;
    V.s[V.i-1] = 'm';
    V.i++;

    return;
}

    V.s[V.i-1] = 'a';
    V.i++;
    V.s[V.i-1] = 'm';
    V.i++;

}


/* Local variables for dates_: */
struct LOC_dates_ {
    char *s;
    int i;   /* index for string */
} ;

static void wrtzer_(int v, struct LOC_dates_ *LINK)
{
    LINK->s[LINK->i-1] = v / 10 % 10 + '0';
/* p2c: services.pas, line 411:
 * Note: Using % for possibly-negative arguments [317] */
    LINK->i++;
    LINK->s[LINK->i-1] = v % 10 + '0';
/* p2c: services.pas, line 413:
 * Note: Using % for possibly-negative arguments [317] */
    LINK->i++;

}

static int leapyear(int y, struct LOC_dates_ *LINK)
{
    return ((y & 3) == 0 && y % 100 != 0 || y % 400 == 0);
/* p2c: services.pas, line 422:
 * Note: Using % for possibly-negative arguments [317] */
/* p2c: services.pas, line 422:
 * Note: Using % for possibly-negative arguments [317] */

}


/********************************************************************************

Get date string padded

Converts the given date into a padded string.

********************************************************************************/

static void dates_(char *s_, int t)
{
    /* string to place date into */
    /* time record to write from */
    struct LOC_dates_ V;
    int y;   /* year holder */
    char d;   /* day holder */
    char m;   /* month holder */
    char dm;   /* temp days of month holder */
    char leap;   /* leap year adder */
    char di;   /* day counter array index */
    short yd;   /* years in day holder */
    int done;   /* loop complete flag */


    int days[12];   /* days in months */
    int FORLIM;


    V.s = s_;
    days[0] = 31;   /* january */
    days[1] = 28;   /* february-leap day */
    days[2] = 31;   /* march */
    days[3] = 30;   /* april */
    days[4] = 31;   /* may */
    days[5] = 30;   /* june */
    days[6] = 31;   /* july */
    days[7] = 31;   /* august */
    days[8] = 30;   /* september */
    days[9] = 31;   /* october */
    days[10] = 30;   /* november */
    days[11] = 31;   /* december */

    if (strlen(V.s) < 8)   /* string to small to hold result */
    error("*** String to small to hold time");
    FORLIM = strlen(V.s);
    for (V.i = 1; V.i <= FORLIM; V.i++)   /* clear result */
    V.s[V.i-1] = ' ';
    V.i = 1;   /* set 1st string place */
    if (t < 0)
    y = 1999;
    else
    y = 2000;
    /* set initial year */
    done = 0;   /* set no loop exit */
    t = abs(t);   /* find seconds magnitude */
    do {

    yd = 365;   /* set days in this year */
    if (leapyear(y, &V))
    yd = 366;   /* set leap year days */
    else
    yd = 365;
    /* set normal year days */
    if (t / daysec > yd) {  /* remove another year */
    if (y >= 2000)
    y++;
    else
    y--;
    /* find next year */
    t -= yd * daysec;   /* remove that many seconds */

}

    else {
    done = 1;

}
} while (!done);   /* until year found */
    leap = 0;   /* set no leap day */
    /* check leap year, and set leap day accordingly */
    if (leapyear(y, &V))
    leap = 1;
    t = t / daysec + 1;   /* find days into year */
    if (y < 2000)   /* adjust for negative years */
    t = leap - t + 366;
    di = 1;   /* set 1st month */
    while (di <= 12) {  /* fit months */
    dm = days[di-1];   /* get the days of month */
    if (di == 2)   /* february, add leap day */
    dm += leap;
    /* check remaining day falls within month */
    if (dm >= t) {
    m = di;
    d = t;
    di = 13;
} else {
    t -= dm;
    di++;
}
}


    wrtzer_(y, &V);   /* place year */
    V.s[V.i-1] = '/';
    V.i++;
    wrtzer_(m, &V);   /* place month */
    V.s[V.i-1] = '/';
    V.i++;   /* place day */

    wrtzer_(d, &V);
}


/********************************************************************************

Write time

Writes the time to a given file, from a time record.

********************************************************************************/

static void writetime_(FILE *f, int t)
{
    /* file to write to */
    /* time record to write from */
    char s[256];


    times_(s, t);   /* convert time to string form */
    fputs(s, f);   /* output */

}


/********************************************************************************

Write date

Writes the date to a given file, from a time record.
Note that this routine should check and obey the international format settings
used by windows.

********************************************************************************/

static void writedate_(FILE *f, int t)
{
    /* file to write to */
    /* time record to write from */
    char s[256];


    dates_(s, t);   /* convert date to string form */
    fputs(s, f);   /* output */

}


/********************************************************************************

Find current time

Finds the current time as an S2000 integer.

********************************************************************************/

static int time_(void)
{
    sc_timeval tv;   /* record to get time */
    sc_timezone tz;   /* record to get timezone */
    int r;   /* return value */


    r = sc_gettimeofday(&tv, &tz);   /* get time info */
    if (r < 0)   /* process unix error */
    unixerr();
    return (tv.tv_sec - unixadj);   /* return S2000 time */

}


/********************************************************************************

Convert to local time

Converts a GMT standard time to the local time.
Note: tried to use the system local time adjustment routine, it bombs.
Currently hard wired for PST.

********************************************************************************/

static int local_(int t)
{
    /* time to convert */
    /* localized time */
    sc_timeval tv;   /* record to get time */
    sc_timezone tz;   /* record to get timezone */
    int r;   /* return value */


    r = sc_gettimeofday(&tv, &tz);   /* get time info */
    if (r < 0)   /* process unix error */
    unixerr();
    return (t - tz.tz_minuteswest * 60);   /* adjust for minutes west */

}


/********************************************************************************

Find clock tick

Finds the time in terms of "ticks". Ticks are defined to occur at 0.1ms, or
100us intervals. The rules for this counter are:

   1. The counter will rollover as much as, but not more than, each 24 hours.
   2. The counter has no specific zero point (and cannot, for example, be used
      to determine the exact time of day).

The base time of 100us is designed specifically to fit these rules. The count
will rollover each 59 hours using 31 bits of precision (the sign bit is
unused).
Note that the rules are upward compatible such that at the 64 bit precision
level, the clock actually represents a real universal time, because it then
has more than enough precision to count from 0 AD to present.

********************************************************************************/

static int clock_(void)
{
    sc_timeval tv;   /* record to get time */
    sc_timezone tz;   /* record to get timezone */
    int r;   /* return value */


    r = sc_gettimeofday(&tv, &tz);   /* get time info */
    if (r < 0)   /* process unix error */
    unixerr();
    /* for Unix, the time is kept in microseconds since the start of the last
       second. we find the number of 100usecs, then add 48 hours worth of
       seconds from standard time */
    return (tv.tv_usec / 100 + tv.tv_sec % (daysec * 2) * 10000);
/* p2c: services.pas, line 608:
 * Note: Using % for possibly-negative arguments [317] */

}


/********************************************************************************

Find elapsed time

Finds the time elapsed since a reference time. The reference time should be
obtained from "clock". Rollover is properly handled, but the maximum elapsed
time that can be measured is 24 hours.

********************************************************************************/

static int elapsed_(int r)
{
    /* reference time */
    int t;


    t = clock_();   /* get the current time */
    if (t >= r)
    t -= r;   /* time has not wrapped */
    else
    t += INT_MAX - r;
    /* time has wrapped */
    return t;   /* return result */

}


/********************************************************************************

Validate filename

Finds if the given string contains a valid filename. Returns true if so,
otherwise false.
There is not much that is not valid in Unix. We only error on a filename that
is null or all blanks

********************************************************************************/

static int valid(char *s)
{
    /* string to validate */
    /* valid/invalid status */
    int r;   /* good/bad result */


    r = 1;   /* set result good by default */
    if (*s == '\0')   /* no filename exists */
    r = 0;
    return r;   /* return error status */

}


/********************************************************************************

Validate pathname

Finds if the given string contains a valid pathname. Returns true if so,
otherwise false.
There is not much that is not valid in Unix. We only error on a filename that
is null or all blanks

********************************************************************************/

static int validp(char *s)
{
    /* string to validate */
    /* valid/invalid status */
    int r;   /* good/bad result */


    r = 1;   /* set result good by default */
    if (*s == '\0')   /* no filename exists */
    r = 0;
    return r;   /* return error status */

}


/********************************************************************************

Check wildcarded filename

Checks if the given filename has a wildcard character, '*' or '?' imbedded.
Also checks if the filename ends in '/', which is an implied '*.*' wildcard
on that directory.

********************************************************************************/

static int wild_(char *s)
{
    /* filename */
    /* wildcard status */
    int r;   /* result flag */
    int i;   /* index for string */
    int ln;   /* length of string */


    ln = strlen(s);   /* find length */
    r = 0;   /* set no wildcard found */
    if (ln <= 0)  /* not null */
    return 0;

    /* search and flag wildcard characters */
    for (i = 0; i < ln; i++) {
    if (s[i] == '*' || s[i] == '?')
    r = 1;
}
    if (s[ln-1] == '/') {
    r = 1;   /* last was '/', it's wild */

}
    return r;

}


/********************************************************************************

Find environment string

Finds the environment string by name, and returns that. Returns nil if not
found.

********************************************************************************/

static void fndenv(char *esn, envrec **ep)
{
    /* string name */
    /* returns environment pointer */
    envrec *p;   /* pointer to environment entry */
    char STR1[256];


    p = envlst;   /* index top of environment list */
    *ep = NULL;   /* set no string found */
    while (p != NULL && *ep == NULL) {  /* traverse */
    sprintf(STR1, "%c", *p->name);
    if (comps(esn, STR1))
    *ep = p;   /* found */
    else {
    p = p->next;   /* next string */

}
}


}


/********************************************************************************

Get environment string padded

Returns an environment string by name.

********************************************************************************/

static void getenvp(char *ls, char *ds)
{
    /* string name */
    /* string buffer */
    envrec *p;   /* pointer to environment entry */
    char STR1[256];


    clears(ds);   /* clear result */
    fndenv(ls, &p);   /* find environment string */
    if (p != NULL) {   /* place string */
    sprintf(STR1, "%c", *p->data);
    copys(ds, STR1);
}

}


/********************************************************************************

Get environment string

Returns an environment string by name.

********************************************************************************/

static void getenv_(char *ls, char *ds)
{
    /* string name */
    /* string buffer */
    bufstr b;   /* string buffer */


    getenvp(ls, b);   /* get environment string */
    /* copy into result */

    copys(ds, b);
}


/********************************************************************************

Set environment string

Sets an environment string by name.

********************************************************************************/

static void setenv_(char *sn, char *sd)
{
    /* name of string */
    /* value of string */
    envrec *p;   /* pointer to environment entry */


    fndenv(sn, &p);   /* find environment string */
    if (p != NULL) {  /* found */
    Free(p->data);   /* release last contents */
    /* create new data string */

    copys(p->data, sd);
    return;
}

    p = Malloc(sizeof(envrec));   /* get a new environment entry */
    p->next = envlst;   /* push onto environment list */
    envlst = p;
    copys(p.name, sn);   /* set name */
    /* place data */

    copys(p.data, sd);
    /* create brand new entry */


}


/********************************************************************************

Remove environment string

Removes an environment string by name.

********************************************************************************/

static void remenv_(char *sn)
{
    /* name of string */
    envrec *p, *l;   /* pointer to environment entry */


    fndenv(sn, &p);   /* find environment string */
    if (p == NULL)  /* found */
    return;


    /* remove entry from list */
    if (envlst == p)
    envlst = p->next;   /* gap from list top */
    else {   /* search */
    /* find last entry that indexes this one */
    l = envlst;   /* index top of list */
    while (l->next != p && l != NULL)
    l = l->next;
    if (l == NULL)
    error("System error: bad environment list");
    l->next = p->next;   /* gap out of list */

}
    /* search */

    Free(p->name);   /* release name */
    Free(p->data);   /* release data */
    /* release entry */

    Free(p);
}


/********************************************************************************

Get environment strings all

Returns a table with the entire environment string set in it.

********************************************************************************/

static void allenv_(envrec **el)
{
    /* environment table */
    envrec *p, *lp;   /* environment pointers */


    /* copy current environment list */
    lp = envlst;   /* index top of environment list */
    *el = NULL;   /* clear destination */
    while (lp != NULL) {  /* copy entries */
    p = Malloc(sizeof(envrec));   /* create a new entry */
    p->next = *el;   /* push onto list */
    *el = p;
    copys(p.name, lp.name);   /* place name */
    copys(p.data, lp.data);   /* place data */
    lp = lp->next;   /* next entry */

}


}


/********************************************************************************

Execute program

Executes a program by name. Does not wait for the program to complete.

********************************************************************************/

static void exec_(char *cmd)
{
    /* program name to execute */
    int r;   /* result code */
    int pid;   /* task id for child process */
    bufstr cn;   /* buffer for command filename */
    bufstr cmds;   /* buffer for commands */
    int wc;   /* word count in command */
    bufstr p, n, e;   /* filename components */
    bufstr pc;   /* path copy */
    char **av, **ev;
    int i;


    wc = words(cmd);   /* find number of words in command */
    if (wc == 0)
    error("Command is empty");
    extword(cn, cmd, 1, 1);   /* get the command verb */
    if (!exists(cn)) {  /* does not exist in current form */
    /* perform pathing search */
    brknam(cn, p, n, e);   /* break down the name */
    if (p[0] == ' ' && pthstr[0] != ' ') {
    copys(pc, pthstr);   /* make a copy of the path */
    trim(pc, pc);   /* make sure left aligned */
    while (pc[0] != ' ') {  /* match path components */
    i = index_(pc, ":");   /* find next path separator */
    if (i == 0) {  /* none left, use entire remaining */
    copys(p, pc);   /* none left, use entire remaining */
    /* clear the rest */

    clears(pc);
}

    else {   /* remove from path */
    extract(p, pc, 1, i - 1);   /* get left side to path */
    extract(pc, pc, i + 1, strlen(pc));   /* make sure left aligned */

    trim(pc, pc);
}
    /* copy partial */

    maknam(cn, p, n, e);   /* create filename */
    if (exists(cn))   /* found, indicate stop */
    clears(pc);

}

    if (!exists(cn))
    error("Command does not exist");

}

    else
    error("Command does not exist");
}


    /* on fork, the child is going to see a zero return, and the parent will
       get the process id. Although this seems dangerous, forked processes
       are truly independent, and so don't care what language is running */
    pid = sc_fork();   /* start subprocess */
    if (pid != 0)  /* we are the child */
    return;


    r = sc_execve(cn, av, ev);   /* execute directory */
    if (r < 0)   /* process unix error */
    unixerr();

    error("Should not continue from execute");
}


/********************************************************************************

Execute program with wait

Executes a program by name. Waits for the program to complete.

********************************************************************************/

static void execw_(char *cmd, int *e)
{
    /* program name to execute */
    /* return error */
}


/********************************************************************************

Execute program with environment

Executes a program by name. Does not wait for the program to complete. Supplies
the program environment.

********************************************************************************/

static void exece_(char *cmd, envrec *el)
{
    /* program name to execute */
    /* environment */
}


/********************************************************************************

Execute program with environment and wait

Executes a program by name. Waits for the program to complete. Supplies the
program environment.

********************************************************************************/

static void execew_(char *cmd, envrec *el, int *e)
{
    /* program name to execute */
    /* environment */
    /* return error */
}


/********************************************************************************

Get current path

Returns the current path in the given padded string.

********************************************************************************/

static void getcur_(char *fn)
{
    /* buffer to get path */
    int r;   /* result code */


    r = sc_getcwd(fn);   /* get the current path */
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Set current path

Sets the current path rom the given string.

********************************************************************************/

static void setcur_(char *fn)
{
    int r;   /* result code */
    char *s;   /* buffer for name */


    r = sc_chdir(fn);   /* change current directory */
    if (r < 0)   /* process unix error */
    unixerr();
    /* release string */

    Free(s);
}


/********************************************************************************

Break file specification

Breaks a filespec down into its components, the path, name and extention.
Note that we don't validate file specifications here. Note that any part of
the file specification could be returned blank.

For Unix, we trim leading and trailing spaces, but leave any embedded spaces
or ".".

The path is straightforward, and consists of any number of /x sections. The
presense of a trailing "/" without a name means the entire thing gets parsed
as a path, including any embedded spaces or "." characters.

Unix allows any number of "." characters, so we consider the extention to be
only the last such section, which could be null. Unix does not technically
consider "." to be a special character, but if the brknam and maknam procedures
are properly paired, it will effectively be treated the same as if the "."
were a normal character.

********************************************************************************/

static void brknam_(char *fn, char *p, char *n, char *e)
{
    /* file specification */
    /* path */
    /* name */
    /* extention */
    int i, s, f, ln, t;   /* string indexes */


    /* clear all strings */
    clears(p);
    clears(n);
    clears(e);
    ln = strlen(fn);   /* find length of string */
    if (ln == 0)
    error("File specification is empty");
    s = 1;   /* set 1st character source */
    /* skip spaces */
    while (fn[s-1] == ' ' && s < ln)
    s++;
    /* find last '/', that will mark the path end */
    f = 0;
    for (i = 1; i <= ln; i++) {
    if (fn[i-1] == '/')
    f = i;
}
    if (f != 0) {  /* there is a path */
    extract(p, fn, s, f);   /* place path */
    s = f + 1;   /* reset next character */

}

    /* skip any leading '.' in name */
    t = s;
    while (fn[t-1] == '.' && t <= ln)
    t++;
    /* find last '.', that will mark the extention */
    f = 0;
    for (i = t; i <= ln; i++) {
    if (fn[i-1] == '.')
    f = i;
}
    if (f == 0) {  /* there is an extention */
    /* just place the rest as the name, and leave extention blank */
    if (s <= ln) {
    extract(n, fn, s, ln);
    /* no extention */


}

    return;
}

    extract(n, fn, s, f - 1);   /* place name */
    s = f + 1;   /* reset to after "." */
    /* get the rest as an extention */

    extract(e, fn, s, ln);
}


/* Local variables for maknam_: */
struct LOC_maknam_ {
    char *fn;
} ;

/* concatenate without leading space */

static void catnls(char *d, char *s, int *p, struct LOC_maknam_ *LINK)
{
    int i, f, ln;
    bufstr buff;


    ln = strlen(s);   /* find length */
    f = 0;   /* clear found */
    /* find first non-space position */
    for (i = 1; i <= ln; i++) {
    if (s[i-1] != ' ' && f == 0)
    f = i;
}
    if (f == 0)  /* found */
    return;


    /* check to long for result */
    if (ln - f + *p + 1 > strlen(LINK->fn))
    error("Name too long for buffer");
    extract(buff, s, f, ln);   /* place contents */
    strinsert(d, (void *)buff, *p);
    *p += strlen(buff);   /* advance position */

}


/********************************************************************************

Make specification

Creates a file specification from its components, the path, name and extention.
We make sure that the path is properly terminated with ':' or '\' before
concatenating.

********************************************************************************/

static void maknam_(char *fn_, char *p, char *n, char *e)
{
    /* file specification to build */
    /* path */
    /* name */
    /* extention */
    struct LOC_maknam_ V;
    int i;   /* index for string */
    int fsi;   /* index for output filename */


    V.fn = fn_;
    clears(V.fn);   /* clear output string */
    fsi = 1;   /* set 1st output character */
    catnls(V.fn, p, &fsi, &V);   /* place path */
    /* check path properly terminated */
    i = strlen(p);   /* find length */
    if (i != 0) {   /* not null */
    if (p[i-1] != '/')
    catnls(V.fn, "/", &fsi, &V);
}
    /* terminate path */
    catnls(V.fn, n, &fsi, &V);   /* place name */
    if (*e != '\0') {  /* there is an extention */
    catnls(V.fn, ".", &fsi, &V);   /* place '.' */
    /* place extention */

    catnls(V.fn, e, &fsi, &V);
}


}


/********************************************************************************

Make full file specification

If the given file specification has a default path (the current path), then
the current path is added to it. Essentially "normalizes" file specifications.
No validity check is done. Garbage in, garbage out.

********************************************************************************/

static void fulnam_(char *fn)
{
    /* file specification */
    bufstr p, n, e, ps;   /* filespec components */


    brknam_(fn, p, n, e);   /* break spec down */
    /* if the path is blank, then default to current */
    if (p[0] == ' ')
    p[0] = '.';
    if ((comps(n, ".") || comps(n, "..")) && e[0] == ' ') {
    /* its '.' or '..', find equivalent path */
    getcur_(ps);   /* save current path */
    setcur_(fn);   /* set candidate path */
    getcur_(fn);   /* get washed path */
    /* reset old path */

    setcur_(ps);
    return;
}

    getcur_(ps);   /* save current path */
    setcur_(p);   /* set candidate path */
    getcur_(p);   /* get washed path */
    setcur_(ps);   /* reset old path */
    /* reassemble */

    maknam_(fn, p, n, e);


}


/* Local variables for getpgm_: */
struct LOC_getpgm_ {
    char cp[256];   /* store for command line */
    int ci;   /* index for command line */
} ;

static char chkcmd(struct LOC_getpgm_ *LINK)
{
    char c;


    if (LINK->ci <= strlen(LINK->cp))
    c = LINK->cp[LINK->ci-1];
    else
    c = ' ';
    return c;

}


/********************************************************************************

Get program path

There is no direct call for program path. So we get the command line, and
extract the program path from that.

********************************************************************************/

static void getpgm(char *path, int len)
{

    struct LOC_getpgm_ V;
    int pi;   /* index for path */
    bufstr pn;   /* program name holder */
    bufstr n, e;   /* name component holders */
    char STR1[256];


    clears(pn);   /* clear result */
    /*sc_getcmd(cp);*/
    /* get commandline */
    V.ci = 1;   /* set 1st command line position */
    pi = 1;   /* set 1st path position */
    /* skip spaces in command line */
    while (chkcmd(&V) == ' ' && V.ci <= sprintf(STR1, "%c", *V.cp))
    V.ci++;
    /* place program name and path */
    while (chkcmd(&V) != ' ' && V.ci <= sprintf(STR1, "%c", *V.cp)) {
    plcstr(pn, &pi, chkcmd(&V));
    V.ci++;

}

    Free(V.cp);   /* release command line */
    fulnam_(pn);   /* clean that */
    /* extract path from that */

    brknam_(pn, p, n, e);
}


/********************************************************************************

Get user path

There is no direct call for user path. We create it from the environment
variables as follows.

1. If there is a "home", "userhome" or "userdir" string, the path is taken from
that.

2. If there is a "user" or "username" string, the path becomes "/home/name"
(no drive).

3. If none of these environmental variables are found, the user path is
returned identical to the program path.

The caller should check if the path exists. If not, then the program path
should be used instead, or the current path as required. The filenames used
with program and user paths should be unique in case they end up in the same
directory.

********************************************************************************/

static void getusr_(char *fn)
{
    bufstr b, b1;   /* buffer for result */


    getenvp("home", b);
    if (b[0] == ' ')   /* place result */
    {  /* not found */
    getenvp("userhome", b);
    if (b[0] == ' ') {  /* not found */
    getenvp("userdir", b);
    if (b[0] == ' ') {  /* not found */
    getenvp("user", b);
    if (b[0] != ' ') {  /* path that */
    strcpy(b1, b);   /* copy */
    copys(b, "/home/");   /* set prefix */
    /* combine */

    cat(b, b, b1);
}

    else {  /* path that */
    getenvp("username", b);
    if (b[0] != ' ') {
    strcpy(b1, b);   /* copy */
    copys(b, "/home/");   /* set prefix */
    /* combine */

    cat(b, b, b1);
}

    else {
    getpgm_(b);
    /* all fails, set to program path */

}
}
}


}


}


    /* not found */


    copys(fn, b);
}


/********************************************************************************

Set attributes on file

Sets any of several attributes on a file. Set directory attribute is not
possible. This is done with makpth.

********************************************************************************/

static void setatr_(char *fn, attrset a)
{
    /* file to set attributes on */
    /* attribute set */
    /* no unix attributes can be set */

}


/********************************************************************************

Reset attributes on file

Resets any of several attributes on a file. Reset directory attribute is not
possible.

********************************************************************************/

static void resatr_(char *fn, attrset a)
{
    /* file to set attributes on */
    /* attribute set */
    /* no unix attributes can be reset */

}


/********************************************************************************

Reset backup time

There is no backup time in Unix. Instead, we reset the archive bit,
which effectively means "back this file up now".

********************************************************************************/

static void bakupd_(char *fn)
{
    setatr_(fn, 1 << ((int)atarc));
}


/********************************************************************************

Set user permissions

Sets user permisions

********************************************************************************/

static void setuper_(char *fn, permset p)
{
    sc_sstat sr;   /* stat() record */
    int r;   /* result code */


    r = sc_stat(fn, &sr);   /* get stat structure on file */
    if (r < 0)   /* process unix error */
    unixerr();
    sr.st_mode &= 0777;   /* mask permissions */
    if (((1 << ((int)pmread)) & p) != 0)   /* set read */
    sr.st_mode |= sc_s_iread;
    if (((1 << ((int)pmwrite)) & p) != 0)   /* set write */
    sr.st_mode |= sc_s_iwrite;
    if (((1 << ((int)pmexec)) & p) != 0)   /* set execute */
    sr.st_mode |= sc_s_iexec;
    r = sc_chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Reset user permissions

Resets user permissions.

********************************************************************************/

static void resuper_(char *fn, permset p)
{
    sc_sstat sr;   /* stat() record */
    int r;   /* result code */


    r = sc_stat(fn, &sr);   /* get stat structure on file */
    if (r < 0)   /* process unix error */
    unixerr();
    sr.st_mode &= 0777;   /* mask permissions */
    if (((1 << ((int)pmread)) & p) != 0)   /* set read */
    sr.st_mode &= ~sc_s_iread;
    if (((1 << ((int)pmwrite)) & p) != 0)   /* set write */
    sr.st_mode &= ~sc_s_iwrite;
    if (((1 << ((int)pmexec)) & p) != 0)   /* set execute */
    sr.st_mode &= ~sc_s_iexec;
    r = sc_chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Set group permissions

Sets group permissions.

********************************************************************************/

static void setgper_(char *fn, permset p)
{
    sc_sstat sr;   /* stat() record */
    int r;   /* result code */


    r = sc_stat(fn, &sr);   /* get stat structure on file */
    if (r < 0)   /* process unix error */
    unixerr();
    sr.st_mode &= 0777;   /* mask permissions */
    if (((1 << ((int)pmread)) & p) != 0)   /* set read */
    sr.st_mode |= sc_s_igread;
    if (((1 << ((int)pmwrite)) & p) != 0)   /* set write */
    sr.st_mode |= sc_s_igwrite;
    if (((1 << ((int)pmexec)) & p) != 0)   /* set execute */
    sr.st_mode |= sc_s_igexec;
    r = sc_chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Reset group permissions

Resets group permissions.

********************************************************************************/

static void resgper_(char *fn, permset p)
{
    sc_sstat sr;   /* stat() record */
    int r;   /* result code */


    r = sc_stat(fn, &sr);   /* get stat structure on file */
    if (r < 0)   /* process unix error */
    unixerr();
    sr.st_mode &= 0777;   /* mask permissions */
    if (((1 << ((int)pmread)) & p) != 0)   /* set read */
    sr.st_mode &= ~sc_s_igread;
    if (((1 << ((int)pmwrite)) & p) != 0)   /* set write */
    sr.st_mode &= ~sc_s_igwrite;
    if (((1 << ((int)pmexec)) & p) != 0)   /* set execute */
    sr.st_mode &= ~sc_s_igexec;
    r = sc_chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Set other (global) permissions

Sets other permissions.

********************************************************************************/

static void setoper_(char *fn, permset p)
{
    sc_sstat sr;   /* stat() record */
    int r;   /* result code */


    r = sc_stat(fn, &sr);   /* get stat structure on file */
    if (r < 0)   /* process unix error */
    unixerr();
    sr.st_mode &= 0777;   /* mask permissions */
    if (((1 << ((int)pmread)) & p) != 0)   /* set read */
    sr.st_mode |= sc_s_ioread;
    if (((1 << ((int)pmwrite)) & p) != 0)   /* set write */
    sr.st_mode |= sc_s_iowrite;
    if (((1 << ((int)pmexec)) & p) != 0)   /* set execute */
    sr.st_mode |= sc_s_ioexec;
    r = sc_chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Reset other (global) permissions

Resets other permissions.

********************************************************************************/

static void resoper_(char *fn, permset p)
{
    sc_sstat sr;   /* stat() record */
    int r;   /* result code */


    r = sc_stat(fn, &sr);   /* get stat structure on file */
    if (r < 0)   /* process unix error */
    unixerr();
    sr.st_mode &= 0777;   /* mask permissions */
    if (((1 << ((int)pmread)) & p) != 0)   /* set read */
    sr.st_mode &= ~sc_s_ioread;
    if (((1 << ((int)pmwrite)) & p) != 0)   /* set write */
    sr.st_mode &= ~sc_s_iowrite;
    if (((1 << ((int)pmexec)) & p) != 0)   /* set execute */
    sr.st_mode &= ~sc_s_ioexec;
    r = sc_chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Set program error code

Sets the return error code for the entire program (main and threads). The
value of the code is not defined, other than:

0:    No error
<>0:  Error encountered

********************************************************************************/

static void seterr_(int c)
{
    wrapper_exit_code = c;   /* place exit code */

}


/********************************************************************************

Make path

Create a new path. Only one new level at a time may be created.

********************************************************************************/

static void makpth_(char *fn)
{
    int r;   /* result code */


    /* make directory, give all permissions allowable */
    r = sc_mkdir(fn,
        sc_s_iread | sc_s_iwrite | sc_s_iexec | sc_s_igread | sc_s_igwrite |
        sc_s_igexec | sc_s_ioread | sc_s_iowrite | sc_s_ioexec);
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Remove path

Create a new path. Only one new level at a time may be deleted.

********************************************************************************/

static void rempth_(char *fn)
{
    int r;   /* result code */


    r = sc_rmdir(fn);   /* remove directory */
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Find valid filename characters

Returns the set of characters allowed in a filespecification. This allows a
specification to be gathered by the user.
Virtually anything can be stuffed into a Unix name. We don't diferentiate
shell special characters because names can be escaped (quoted), and shells
have different special characters anyway.
As a result, we only exclude the file characters that would cause problems
with common IP procedures:

1. Space, because most command line names are space delimited.

2. Non printing, so we don't create names that cannot be seen as well as
removed.

3. '-', because that is the Unix option character.

4. '#', because that is the IP universal option character.

Unfortunately, this can create the inability to access filenames with spaces.
For such reasons, the program will probally have to determine its own
specials in these cases.

********************************************************************************/

static void filchr_(uchar *fc)
{
    /*fc := ['!'..'~']-['-', '#'];*/

}


/********************************************************************************

Find option character

Returns the character used to introduce a command line option.
In unix this is "-". Unix sometimes uses "+" for add/subtract option, but this
is overly cute and not common.

********************************************************************************/

static char optchr_(void)
{
    return '-';

}


/*******************************************************************************

Find path separator character

Returns the character used to separate filename path sections.
In windows/dos this is "\".

*******************************************************************************/

static char pthchr_(void)
{
    return '\\';

}


main(int argc, char *argv[])
{
    char STR1[256];
    int FORLIM;

    /* Unix gives us a read only copy of the environment, so we copy to a string
       list and perform our own reads and writes on that */
    envlst = NULL;   /* clear environment strings */
    ep = sc_allenv();   /* get unix environment pointers */
    FORLIM = strlen(ep);
    for (ei = 1; ei <= FORLIM; ei++) {  /* copy environment strings */
    p = Malloc(sizeof(envrec));   /* get a new environment entry */
    p->next = envlst;   /* push onto environment list */
    envlst = p;
    si = index_(ep[ei-1], "=");   /* find location of '=' */
    if (si == 0)
    error("Invalid environment string format");
    extract(p->name, ep[ei-1], 1, si - 1);   /* get the name string */
    /* get the data string */

    extract(p->data, ep[ei-1], si + 1, strlen(ep[ei-1]));
}
/* p2c: services.pas, line 1681:
 * Warning: Mixing non-strings with strings [170] */

    Free(ep);   /* release environment table */
    getenvp("path", pthstr);   /* load up the current path */
    /* make sure left aligned */

    trim(pthstr, pthstr);
    exit(EXIT_SUCCESS);
}



/* End. */
