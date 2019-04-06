/*******************************************************************************
*                                                                              *
*                     UNIX/LINUX EXTENDED FUNCTION LIBRARY                     *
*                                                                              *
*                              Created 1996                                    *
*                                                                              *
*                               S. A. MOORE                                    *
*                                                                              *
* Contains various system oriented library functions, including files,         *
* directories, time, program execution, environment, and random numbers.       *
* This implementation is specific to the Unix system, but services tends to    *
* have processing elements that are universal.                                 *
*                                                                              *
* To Do:                                                                       *
*                                                                              *
* 1. This version is US english only. Need translations according to locale.   *
*                                                                              *
********************************************************************************/

#include <errno.h>
#include <sys/types.h>
//#include <sys/status.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include "services.h" /* the header for this file */

/* give bit in word from ordinal position */
#define bit(b) (1 << b)

#define hoursec         3600   /* number of seconds in an hour */
#define daysec          (hoursec * 24)   /* number of seconds in a day */
#define yearsec         (daysec * 365)   /* number of seconds in year */
/* Unix time adjustment for 1970 */
#define unixadj         (yearsec * 30 + daysec * 7)

/* maximum size of holding buffers (I had to make this very large for large
   paths [sam]) */
#define MAXSTR          500

typedef char bufstr[MAXSTR]; /* standard string buffer */

static bufstr pthstr;   /* buffer for execution path */

static envrec *envlst;   /* our environment list */

/* dummy routine placeholders */
static void cat(char *d, char *s1, char *s2) { }
static int index(char *s, char *m) { }
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

/********************************************************************************

Match filenames with wildcards

match with wildcards at the given a and b positions. we use shortest string first
matching.

********************************************************************************/

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
            while (ib < strlen(b) && !match(a, b, ia, ib)) ib++;
            if (ib >= strlen(b)) m = 0; /* didn't match, set false */
            ib = strlen(b);   /* terminate */

        }

        else if (a[ia-1] != b[ib-1] && a[ia-1] != '?')
            m = 0; /* fail match */
        if (!m) {

            ia++;   /* next character */
            ib++;

        }

    }
    return m;

}

/********************************************************************************

Create file list

Accepts a filename, that may include wildcards. All of the matching files are
found, and a list of file entries is returned. The file entries are in standard
directory format. The path may not contain wildcards.

The entries are allocated from general storage, and both the entry and the
filename should be disposed of by the caller when they are no longer needed.
If no files are matched, the returned list is nil.

********************************************************************************/

void pa_list(
    /** file to search for */ char *f,
    /** file list returned */ filrec **l
)

{

    struct dirent dr; /* Unix directory record */
    int           fd; /* directory file descriptor */
    int           r;  /* result code */
    int           rd;
    struct stat   sr; /* stat() record */
    filrec*       fp; /* file entry pointer */
    filrec*       lp; /* last entry pointer */
    int           i;  /* name index */
    bufstr        p;  /* filename components */
    bufstr        n;
    bufstr        e;
    bufstr        fn; /* holder for directory name */

    *l = NULL; /* clear destination list */
    lp = NULL; /* clear last pointer */
    brknam(f, p, n, e); /* break up filename */
    /* check wildcards in path */
    if (strstr(p, "*") || strstr(p, "?")) error("Path cannot contain wildcards");
    /* construct name of containing directory */
    maknam(fn, p, ".", "");
    fd = open(fn, O_RDONLY, 0); /* open the directory */
    if (fd < 0) unixerr(); /* process unix open error */
    maknam(fn, "", n, e);   /* reform name without path */
    do { /* read directory entries */

        rd = readdir(fd, &dr, 1);
        if (rd < 0) unixerr(); /* process unix error */
        if (rd == 1) { /* valid next */

            if (match(fn, dr.d_name, 1, 1)) { /* matching filename, add to list */
            fp = malloc(sizeof(filrec)); /* create a new file entry */
            fp^.name = malloc(strlen(dr.d_name)); /* copy to new filename string */
            strcpy(fp^.name, dr.d_name);
            r = stat(fp^.name, &sr); /* get stat structure on file */
            if (r < 0) unixerr(); /* process unix error */
            /* file information in stat record, translate to our format */
            copys(fp->name, dn);   /* place filename */
            fp->size = sr.st_size;   /* place size */
            /* there is actually a real unix allocation, but I haven't figgured out
               how to calculate it from block/blocksize */
            fp->alloc = sr.st_size;   /* place allocation */
            fp->attr = 0;   /* clear attributes */
            /* clear permissions to all is allowed */
            fp->user = bit(pmread) | bit(pmwrite) | bit(pmexec) | bit(pmdel) |
                       bit(pmvis) | bit(pmcopy) | bit(pmren);
            fp->other = bit(pmread) | bit(pmwrite) | bit(pmexec) | bit(pmdel) |
                        bit(pmvis) | bit(pmcopy) | bit(pmren);
            fp->group = bit(pmread) | bit(pmwrite) | bit(pmexec) | bit(pmdel) |
                        bit(pmvis)) | bit(pmcopy)) | bit(pmren);
            /* check and set directory attribute */
            if ((sr.st_mode & sc_s_ifdir) != 0) fp->attr |= bit(atdir);
            /* check and set any system special file */
            if ((sr.st_mode & sc_s_ififo) != 0) fp->attr |= bit(atsys);
            if ((sr.st_mode & sc_s_ifchr) != 0) fp->attr |= bit(atsys);
            if ((sr.st_mode & sc_s_ifblk) != 0) fp->attr |= bit(atsys);
            /* check hidden. in Unix, this is done with a leading '.'. We remove
               visiblity priveledges */
            if (dr.d_name[0] == '.') {

                fp->user &= ~bit(pmvis);
                fp->group &= ~bit(pmvis);
                fp->other &= ~bit(pmvis);

            }
            /* check and set executable attribute. Unix has separate executable
               permissions for each permission type, we set executable if any of
               them are true */
            if ((sr.st_mode & sc_s_iexec) != 0) fp->attr |= bit(atexec);
            /* set execute permissions to user */
            if ((sr.st_mode & sc_s_iexec) == 0) fp->user &= ~bit(pmexec);
            /* set read permissions to user */
            if ((sr.st_mode & sc_s_iread) == 0) fp->user &= ~bit(pmread);
            /* set write permissions to user */
            if ((sr.st_mode & sc_s_iwrite) == 0) fp->user &= ~bit(pmwrite);
            /* set execute permissions to group */
            if ((sr.st_mode & sc_s_igexec) == 0) fp->group &= ~bit(pmexec);
            /* set read permissions to group */
            if ((sr.st_mode & sc_s_igread) == 0) fp->group &= ~bit(pmread);
            /* set write permissions to group */
            if ((sr.st_mode & sc_s_igwrite) == 0) fp->group &= ~bit(pmwrite);
            /* set execute permissions to other */
            if ((sr.st_mode & sc_s_ioexec) == 0) fp->other = fp->group & (~bit(pmexec));
            /* set read permissions to other */
            if ((sr.st_mode & sc_s_ioread) == 0) fp->other = fp->group & (~bit(pmread));
            /* set write permissions to other */
            if ((sr.st_mode & sc_s_iowrite) == 0) fp->other = fp->group & (~bit(pmwrite));
            /* set times */
            fp->create = sr.st_ctime-unixadj;
            fp->modify = sr.st_mtime-unixadj;
            fp->access = sr.st_atime-unixadj;
            fp->backup = -INT_MAX; /* no backup time for Unix */
            /* insert entry to list */
            if (*l == NULL) *l = fp; /* insert new top */
            else lp->next = fp; /* insert next entry */
            lp = fp;   /* set new last */
            fp->next = NULL;   /* clear next */

        }

    } while (rd == 1);
    r = close(fd);
    if (r < 0) unixerr();  /* process unix error */

}

/********************************************************************************

Get time string padded

Converts the given time into a string.

********************************************************************************/

void pa_times(
    /** result string */           char *s,
    /** length of string buffer */ int len;
    /** time to convert */         int t
)

{

    char h;   /* hour */
    char m;   /* minute */
    char sec; /* second */
    int  am;  /* am flag */
    int  pm;  /* pm flag */

    if (len < 11-(!time24hour()*3)) /* string to small to hold result */
        error("*** String buffer to small to hold time");
    i = 1;   /* set 1st string place */
    /* because leap adjustments are made in terms of days, we just remove
       the days to find the time of day in seconds. this is completely
       independent of leap adjustments */
    t %= daysec;   /* find number of seconds in day */
    /* if before 2000, find from remaining seconds */
    if (t < 0) t += daysec;
    h = t / hoursec;   /* find hours */
    t %= hoursec;   /* subtract hours */
    m = t / 60;   /* find minutes */
    sec = t % 60;   /* find seconds */
    pm = 0; /* clear am and pm flags */
    am = 0;
    if (!time24hour()) { /* do am/pm adjustment */

        if (h == 0) h = 12; /* hour zero */
        else if (h > 12) { h -= 12; pm = 1; } /* 1 pm to 11 pm */

    }
    /* place hour:miniute:second */
    switch (timeorder()) {

        case 1:
            s += sprintf(s, "%02d%c%02d%c%02d", h, timesep(), m, timesep(), s);
            break;
        case 2:
            s += sprintf(s, "%02d%c%02d%c%02d", h, timesep(), s, timesep(), m);
            break;
        case 3:
            s += sprintf(s, "%02d%c%02d%c%02d", m, timesep(), h, timesep(), s);
            break;
        case 4:
            s += sprintf(s, "%02d%c%02d%c%02d", m, timesep(), s, timesep(), h);
            break;
        case 5:
            s += sprintf(s, "%02d%c%02d%c%02d", s, timesep(), h, timesep(), m);
            break;
        case 6:
            s += sprintf(s, "%02d%c%02d%c%02d", s, timesep(), m, timesep(), h);
            break;

    }
    if (pm) s += sprintf(s, " pm");
    if (am) s += sprintf(s, " am");
    *s = 0; /* terminate string */

}

/********************************************************************************

Get date string padded

Converts the given date into a string.

********************************************************************************/

/*
 * Check year is a leap year
 */
#define leapyear(y) ((y & 3) == 0 && y % 100 != 0 || y % 400 == 0)

void pa_dates(
    /* string to place date into */ char *s,
    /* time record to write from */ int t
)

{

    int y;   /* year holder */
    char d;   /* day holder */
    char m;   /* month holder */
    char dm;   /* temp days of month holder */
    char leap;   /* leap year adder */
    char di;   /* day counter array index */
    short yd;   /* years in day holder */
    int done;   /* loop complete flag */
    int days[12];   /* days in months */

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

    if (strlen(V.s) < 10) /* string to small to hold result */
        error("*** String to small to hold date");
    if (t < 0) y = 1999; else y = 2000; /* set initial year */
    done = 0;   /* set no loop exit */
    t = abs(t);   /* find seconds magnitude */
    do {

        yd = 365;   /* set days in this year */
        if (leapyear(y)) yd = 366; /* set leap year days */
        else  yd = 365;
        /* set normal year days */
        if (t/daysec > yd) {  /* remove another year */

            if (y >= 2000) y++; else y--; /* find next year */
            t -= yd * daysec; /* remove that many seconds */

        } else done = 1;

    } while (!done);   /* until year found */
    leap = 0;   /* set no leap day */
    /* check leap year, and set leap day accordingly */
    if (leapyear(y, &V)) leap = 1;
    t = t/daysec+1; /* find days into year */
    if (y < 2000) t = leap - t + 366; /* adjust for negative years */
    di = 1;   /* set 1st month */
    while (di <= 12) {  /* fit months */

        dm = days[di-1]; /* get the days of month */
        if (di == 2) dm += leap; /* february, add leap day */
        /* check remaining day falls within month */
        if (dm >= t) { m = di; d = t; di = 13; }
        else { t -= dm; di++; }

    }
    /* place year/month/day */
    switch (dateorder()) { /* place according to current location format */

        case 1:
            s += sprintf(s, "%04d%c%02d%c%02d", y, datesep(), m, datesep(), d);
            break;
        case 2:
            s += sprintf(s, "%04d%c%02d%c%02d", y, datesep(), d, datesep(), m);
            break;
        case 3:
            s += sprintf(s, "%02d%c%02d%c%04d", m, datesep(), d, datesep(), y);
            break;
        case 4:
            s += sprintf(s, "%02d%c%04d%c%02d", m, datesep(), y, datesep(), d);
            break;
        case 5:
            s += sprintf(s, "%02d%c%02d%c%04d", d, datesep(), m, datesep(), y);
            break;
        case 6:
            s += sprintf(s, "%02d%c%04d%c%02d", d, datesep(), y, datesep(), m);
            break;

    }
    *s = 0; /* terminate string */

}

/********************************************************************************

Write time

Writes the time to a given file, from a time record.

********************************************************************************/

void pa_writetime(
        /* file to write to */ FILE *f,
        /* time record to write from */ int t
)

{

    char s[MAXSTR];

    times(s, t);   /* convert time to string form */
    fputs(s, f);   /* output */

}

/********************************************************************************

Write date

Writes the date to a given file, from a time record.
Note that this routine should check and obey the international format settings
used by windows.

********************************************************************************/

void pa_writedate(
        /* file to write to */ FILE *f,
        /* time record to write from */ int t
)

{

    char s[MAXSTR];

    dates(s, t);   /* convert date to string form */
    fputs(s, f);   /* output */

}

/********************************************************************************

Find current time

Finds the current time as an S2000 integer.

********************************************************************************/

int pa_time(void)

{

    time_t r;   /* return value */

    r = time(NULL); /* get current time */
    if (r < 0) unixerr();  /* process unix error */

    return ((int) r-unixadj);   /* return S2000 time */

}


/********************************************************************************

Convert to local time

Converts a GMT standard time to the local time using time zone and daylight
savings. Does not compensate for 30 minute increments in daylight savings or
timezones.

********************************************************************************/

int pa_local(int t)
{

    return t+timezone()+daysave*60

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

int pa_clock(void)

{

    sc_timeval tv; /* record to get time */
    int r;         /* return value */

    r = sc_gettimeofday(&tv, NULL); /* get time info */
    if (r < 0) unixerr(); /* process unix error */

    /* for Unix, the time is kept in microseconds since the start of the last
       second. we find the number of 100usecs, then add 48 hours worth of
       seconds from standard time */
    return (tv.tv_usec / 100 + tv.tv_sec % (daysec * 2) * 10000);

}

/********************************************************************************

Find elapsed time

Finds the time elapsed since a reference time. The reference time should be
obtained from "clock". Rollover is properly handled, but the maximum elapsed
time that can be measured is 24 hours.

********************************************************************************/

int pa_elapsed(int r)
{

    /* reference time */
    int t;

    t = uclock();   /* get the current time */
    if (t >= r) t -= r; /* time has not wrapped */
    else t += INT_MAX-r; /* time has wrapped */

    return t;   /* return result */

}

/********************************************************************************

Validate filename

Finds if the given string contains a valid filename. Returns true if so,
otherwise false.

There is not much that is not valid in Unix. We only error on a filename that
is null or all blanks

********************************************************************************/

int pa_validfile(
    /* string to validate */ char *s
)

{

    int r;   /* good/bad result */

    r = 1;   /* set result good by default */
    while (*s && *s == ' ') s++; /* check all blanks */
    if (*s == '\0') r = 0; /* no filename exists */

    return r;   /* return error status */

}


/********************************************************************************

Validate pathname

Finds if the given string contains a valid pathname. Returns true if so,
otherwise false. There is not much that is not valid in Unix. We only error on a
filename that is null or all blanks

********************************************************************************/

int pa_validpath(
    /* string to validate */ char *s
)

{

    int r;   /* good/bad result */

    r = 1;   /* set result good by default */
    while (*s && *s == ' ') s++; /* check all blanks */
    if (*s == '\0') r = 0; /* no filename exists */

    return r;   /* return error status */

}

/********************************************************************************

Check wildcarded filename

Checks if the given filename has a wildcard character, '*' or '?' imbedded.
Also checks if the filename ends in '/', which is an implied '*.*' wildcard
on that directory.

********************************************************************************/

int pa_wild(
    /* filename */ char *s
)

{

    int r;   /* result flag */
    int i;   /* index for string */
    int ln;   /* length of string */

    ln = strlen(s);   /* find length */
    r = 0;   /* set no wildcard found */
    if (ln <= 0) { /* not null */

        /* search and flag wildcard characters */
        for (i = 0; i < ln; i++) { if (s[i] == '*' || s[i] == '?') r = 1; }
        if (s[ln-1] == '/') r = 1; /* last was '/', it's wild */

    }

    return r;

}

/********************************************************************************

Find environment string

Finds the environment string by name, and returns that. Returns nil if not
found.

********************************************************************************/

void pa_getenv(
    /* string name */                 char   *esn,
    /* returns environment pointer */ envrec **ep
)

{

    envrec *p;   /* pointer to environment entry */

    p = envlst; /* index top of environment list */
    *ep = NULL; /* set no string found */
    while (p != NULL && *ep == NULL) {  /* traverse */

        if (!strcmp(esn, p->name)) *ep = p; /* found */
        else p = p->next;/* next string */

    }

}

/********************************************************************************

Set environment string

Sets an environment string by name.

********************************************************************************/

void pa_setenv(
    /* name of string */ char *sn,
    /* value of string */char *sd
)

{

    envrec *p;   /* pointer to environment entry */

    fndenv(sn, &p); /* find environment string */
    if (p != NULL) { /* found */

        Free(p->data);   /* release last contents */
        /* create new data string */
        p->data = (char *) malloc(strlen(sd));
        if (!p) error("Could not allocate string");
        strcpy(p->data, sd);

    } else {

        p = malloc(sizeof(envrec)); /* get a new environment entry */
        if (!p) error("Could not allocate structure");
        p->next = envlst; /* push onto environment list */
        envlst = p;
        /* create new name string and place */
        p->name = (char *) malloc(strlen(sn));
        if (!p) error("Could not allocate string");
        strcpy(p->data, sd);
        /* create new data string and place */
        p->data = (char *) malloc(strlen(sd));
        if (!p) error("Could not allocate string");
        strcpy(p->data, sd);

    }

}

/********************************************************************************

Remove environment string

Removes an environment string by name.

********************************************************************************/

void pa_remenv(
        /* name of string */ char *sn
)

{

    envrec *p, *l;   /* pointer to environment entry */

    fndenv(sn, &p);   /* find environment string */
    if (p != NULL) { /* found */

        /* remove entry from list */
        if (envlst == p) envlst = p->next; /* gap from list top */
        else { /* search */

            /* find last entry that indexes this one */
            l = envlst;  /* index top of list */
            while (l->next != p && l != NULL) l = l->next; /* search */
            if (l == NULL) error("Bad environment list");
            l->next = p->next; /* gap out of list */

        }
        free(p->name); /* release name */
        free(p->data); /* release data */
        Free(p); /* release entry */

    }

}

/********************************************************************************

Get environment strings all

Returns a table with the entire environment string set in it.

********************************************************************************/

void pa_allenv(
    /* environment table */ envrec **el
)

{

    envrec *p, *lp;   /* environment pointers */

    /* copy current environment list */
    lp = envlst; /* index top of environment list */
    *el = NULL; /* clear destination */
    while (lp != NULL) {  /* copy entries */

        p = malloc(sizeof(envrec)); /* create a new entry */
        p->next = *el;   /* push onto list */
        *el = p;
        strcpy(p.name, lp.name);   /* place name */
        strcpy(p.data, lp.data);   /* place data */
        lp = lp->next;   /* next entry */

    }

}

/********************************************************************************

Execute program

Executes a program by name. Does not wait for the program to complete.

********************************************************************************/

void pa_exec(
    /* program name to execute */ char *cmd
)

{

    int r;          /* result code */
    int pid;        /* task id for child process */
    bufstr cn;      /* buffer for command filename */
    bufstr cmds;    /* buffer for commands */
    int wc;         /* word count in command */
    bufstr p, n, e; /* filename components */
    bufstr pc;      /* path copy */
    char **av, **ev;
    int i;

    wc = words(cmd);   /* find number of words in command */
    if (wc == 0)
    error("Command is empty");
    extword(cn, cmd, 1, 1);  /* get the command verb */
    if (!exists(cn)) {  /* does not exist in current form */

        /* perform pathing search */
        brknam(cn, p, n, e);   /* break down the name */
        if (*p == 0 && *pthstr != 0) {

            strcpy(pc, pthstr);   /* make a copy of the path */
            trim(pc, pc);   /* make sure left aligned */
            while (*pc != 0) {  /* match path components */

                i = index(pc, ":");   /* find next path separator */
                if (i == 0) {  /* none left, use entire remaining */

                    strcpy(p, pc); /* none left, use entire remaining */
                    pc[0] = 0; /* clear the rest */

                } else { /* copy partial */

                    extract(p, pc, 1, i - 1);   /* get left side to path */
                    extract(pc, pc, i + 1, strlen(pc)); /* remove from path */
                    trim(pc, pc); /* make sure left aligned */

                }
                maknam(cn, p, n, e);   /* create filename */
                if (exists(cn)) *pc = 0;  /* found, indicate stop */

            }

            if (!exists(cn)) error("Command does not exist");

        } else error("Command does not exist");

    }
    /* on fork, the child is going to see a zero return, and the parent will
       get the process id. Although this seems dangerous, forked processes
       are truly independent, and so don't care what language is running */
    pid = fork(); /* start subprocess */
    if (pid == 0) { /* we are the child */

        r = sc_execve(cn, av, ev);   /* execute directory */
        if (r < 0)   /* process unix error */
        unixerr();
        error("Should not continue from execute");

    }

}

/********************************************************************************

Execute program with wait

Executes a program by name. Waits for the program to complete.

********************************************************************************/

void pa_execw(
    /* program name to execute */ char *cmd,
    /* return error */ int *e
)

{

    error("Function not implemented");

}


/********************************************************************************

Execute program with environment

Executes a program by name. Does not wait for the program to complete. Supplies
the program environment.

********************************************************************************/

void pa_exece(
    /* program name to execute */ char *cmd,
    /* environment */ envrec *el
)

{

    error("Function not implemented");

}


/********************************************************************************

Execute program with environment and wait

Executes a program by name. Waits for the program to complete. Supplies the
program environment.

********************************************************************************/

void pa_execew(
        /* program name to execute */ char *cmd,
        /* environment */ envrec *el,
        /* return error */int *e
)

{

    error("Function not implemented");

}


/********************************************************************************

Get current path

Returns the current path in the given padded string.

********************************************************************************/

void pa_getcur(
        /* buffer to get path */ char *fn,
        /* length of buffer */   int l
)

{

    int r;   /* result code */
    bufstr ts; /* temp buffer */

    r = getcwd(ts);   /* get the current path */
    if (r < 0) unixerr(); /* process unix error */
    if (strlen(ts) > l) error("Path is too long for buffer")
    strcpy(fn, ts); /* copy to result */

}


/********************************************************************************

Set current path

Sets the current path from the given string.

********************************************************************************/

void pa_setcur(
        /* path to set */ char *fn
)

{

    int r;   /* result code */
    char *s; /* buffer for name */

    r = chdir(fn); /* change current directory */
    if (r < 0) unixerr();  /* process unix error */

}

/********************************************************************************

Break file specification

Breaks a filespec down into its components, the path, name and extension.
Note that we don't validate file specifications here. Note that any part of
the file specification could be returned blank.

For Unix, we trim leading and trailing spaces, but leave any embedded spaces
or ".".

The path is straightforward, and consists of any number of /x sections. The
presense of a trailing "/" without a name means the entire thing gets parsed
as a path, including any embedded spaces or "." characters.

Unix allows any number of "." characters, so we consider the extension to be
only the last such section, which could be null. Unix does not technically
consider "." to be a special character, but if the brknam and maknam procedures
are properly paired, it will effectively be treated the same as if the "."
were a normal character.

********************************************************************************/

void pa_brknam(
        /* file specification */ char *fn,
        /* path */               char *p,
        /* name */               char *n,
        /* extention */          char *e
)

{

    int i, s, f, ln, t; /* string indexes */

    /* clear all strings */
    *p = 0;
    *n = 0;
    *e = 0;
    ln = strlen(fn);   /* find length of string */
    if (ln == 0) error("File specification is empty");
    s = 1;   /* set 1st character source */
    /* skip spaces */
    while (fn[s-1] == ' ' && s < ln) s++;
    /* find last '/', that will mark the path end */
    f = 0;
    for (i = 1; i <= ln; i++) if (fn[i-1] == '/') f = i;
    if (f != 0) {  /* there is a path */

        extract(p, fn, s, f); /* place path */
        s = f + 1; /* reset next character */

    }
    /* skip any leading '.' in name */
    t = s;
    while (fn[t-1] == '.' && t <= ln) t++;
    /* find last '.', that will mark the extension */
    f = 0;
    for (i = t; i <= ln; i++) if (fn[i-1] == '.') f = i;
    if (f != 0) {  /* there is an extension */

        extract(n, fn, s, f - 1); /* place name */
        s = f + 1; /* reset to after "." */
        extract(e, fn, s, ln); /* get the rest as an extention */

    } else {  /* no extension */

        /* just place the rest as the name, and leave extension blank */
        if (s <= ln) extract(n, fn, s, ln);

    }

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

void pa_maknam(char *fn_, char *p, char *n, char *e)
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

void pa_fulnam_(char *fn)
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
    char cp[MAXSTR];   /* store for command line */
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

Note: this does not work for standard CLIB programs. We need another solution.

********************************************************************************/

void pa_getpgm(char *path, int len)
{

    struct LOC_getpgm_ V;
    int pi;   /* index for path */
    bufstr pn;   /* program name holder */
    bufstr n, e;   /* name component holders */
    char STR1[MAXSTR];


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

void pa_getusr(char *fn)
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

void pa_setatr(char *fn, attrset a)
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

void pa_resatr(char *fn, attrset a)
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

void pa_bakupd(char *fn)
{
    setatr_(fn, 1 << ((int)atarc));
}


/********************************************************************************

Set user permissions

Sets user permisions

********************************************************************************/

void pa_setuper(char *fn, permset p)
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

void pa_resuper(char *fn, permset p)
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

void pa_setgper(char *fn, permset p)
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

void pa_resgper(char *fn, permset p)
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

void pa_setoper(char *fn, permset p)
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

void pa_resoper(char *fn, permset p)
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

Make path

Create a new path. Only one new level at a time may be created.

********************************************************************************/

void pa_makpth(char *fn)
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

void pa_rempth(char *fn)
{
    int r;   /* result code */


    r = sc_rmdir(fn);   /* remove directory */
    if (r < 0)   /* process unix error */
    unixerr();

}


/********************************************************************************

Find valid filename characters

Returns the set of characters allowed in a file specification. This allows a
specification to be gathered by the user.
Virtually anything can be stuffed into a Unix name. We don't differentiate
shell special characters because names can be escaped (quoted), and shells
have different special characters anyway.
As a result, we only exclude the file characters that would cause problems
with common IP procedures:

1. Space, because most command line names are space delimited.

2. Non printing, so we don't create names that cannot be seen as well as
removed.

3. '-', because that is the Unix option character.

Unfortunately, this can create the inability to access filenames with spaces.
For such reasons, the program will probably have to determine its own
specials in these cases.

********************************************************************************/

void pa_filchr(uchar *fc)
{

    /* add everything but control characters and space */
    for (i = ' '+1, i <= 0x7f; i++) ADDSET(i);
    SUBSET('-'); /* add option character */
    SUBSET(pthchr()); /* add path character */

}


/********************************************************************************

Find option character

Returns the character used to introduce a command line option.
In unix this is "-". Unix sometimes uses "+" for add/subtract option, but this
is overly cute and not common.

********************************************************************************/

char pa_optchr(void)
{
    return '-';

}

/*******************************************************************************

Find path separator character

Returns the character used to separate filename path sections.
In windows/dos this is "\", in Unix/Linux it is '/'. One possible solution to
pathing is to accept both characters as a path separator. This means that
systems that use the '\' as a forcing character would need to represent the
separator as '\\'.

*******************************************************************************/

char pa_pthchr(void)
{

    return '/';

}

/** ****************************************************************************

Find latitude

Finds the latitude of the host. Returns the latitude as a ratioed integer:

0           Equator
INT_MAX     North pole
-INT_MAX    South pole

This means each increment equals 0.0000000419 degrees or about 0.00465 meters
(approximate because it is an angular measurement on an elipsiod).

Note that implementations are generally divided into stationary and mobile
hosts. A stationary host like a desktop computer can be set once on install.
A mobile host is constantly reading its location (usually from a GPS).

*******************************************************************************/

int pa_latitude(void)

{

}

/** ****************************************************************************

Find longitude

Finds the longitude of the host. Returns the longitude as a ratioed integer:

0           The prime meridian (Greenwitch)
INT_MAX     The prime meridian eastward around the world
-INT_MAX    The prime meridian westward around the world

This means that each increment equals 0.0000000838 degrees or about 0.00933
meters (approximate because it is an angular measurement on an elipsoid).

Note that implementations are generally divided into stationary and mobile
hosts. A stationary host like a desktop computer can be set once on install.
A mobile host is constantly reading its location (usually from a GPS).

*******************************************************************************/

int pa_longitude(void)

{

}

/** ****************************************************************************

Find altitude

Finds the altitude of the host. Returns the altitude as a ratioed integer:

0           MSL
INT_MAX     100km high
-INT_MAX    100km depth

This means that each increment is 0.0000465 meters. MSL is determined by WGS84
(World Geodetic System of 1984), which estalishes an ideal elipsoid as an
approximation of the shape of the world. This is the basis for GPS and
establishes GPS as the main reference MSL surface, which typically must be
corrected for the exact local system in use, which could be:

1. Local MSL.
2. Presure altitude.

Or another system.

Note that implementations are generally divided into stationary and mobile
hosts. A stationary host like a desktop computer can be set once on install.
A mobile host is constantly reading its location (usually from a GPS).

*******************************************************************************/

int pa_altitude(void)

{

    return 0;

}

/** ****************************************************************************

Find country code

Gives the ISO 3166-1 1 to 3 digit numeric code for the country of the host
computer. Note that the country of host may be set by the user, or may be
determined by latitude/longitude.

*******************************************************************************/

int pa_country(void)

{

    return 840; /* USA */

}

/** ****************************************************************************

Find country identifier string

Finds the identifier string for the given ISO 3166-1 country code. If the string
does not fit into the string provided, an error results.

*******************************************************************************/

typedef struct {

    char *countrystr;
    int  countrynum;

} countryety;

countryety countrytab[] = {

    "Afghanistan",       004, "Aland Islands",    248, "Albania",          008,
    "Algeria",           012, "American Samoa",   016, "Andorra",          020,
    "Angola",            024, "Anguilla",         660, "Antarctica",       010,
    "Antigua and Barbuda", 028,
    "Argentina",         032, "Armenia",          051, "Aruba",            533,
    "Australia",         036, "Austria",          040, "Azerbaijan",       031,
    "Bahamas",           044, "Bahrain",          048, "Bangladesh",       050,
    "Barbados",          052, "Belarus",          112, "Belgium",          056,
    "Belize",            084, "Benin",            204, "Bermuda",          060,
    "Bhutan",            064, "Bolivia",          068,
    "Bonaire, Sint Eustatius and Saba", 535,
    "Bosnia and Herzegovina", 070,
    "Botswana",          072, "Bouvet Island",    074, "Brazil",           076,
    "British Indian Ocean Territory",  086,
    "Brunei Darussalam", 096, "Bulgaria",         100, "Burkina Faso",     854,
    "Burundi",           108, "Cambodia",         116, "Cameroon",         120,
    "Canada",            124, "Cabo Verde",       132, "Cayman Islands",   136,
    "Central African Republic", 140,
    "Chad",              148, "Chile",            152, "China",            156,
    "Christmas Island",  162, "Cocos (Keeling) Islands", 166,
    "Colombia",          170, "Comoros",          174, "Congo",            178,
    "Congo, the Democratic Republic of the", 180,
    "Cook Islands",      184, "Costa Rica",       188, "Côte d'Ivoire",    384,
    "Croatia",           191, "Cuba",             192, "Curaçao",          531,
    "Cyprus",            196, "Czech Republic",   203, "Denmark",          208,
    "Djibouti",          262, "Dominica",         212,
    "Dominican Republic", 214,
    "Ecuador",           218, "Egypt",            818, "El Salvador",      222,
    "Equatorial Guinea", 226, "Eritrea",          232, "Estonia",          233,
    "Ethiopia",          231, "Falkland Islands (Malvinas)", 238,
    "Faroe Islands",     234,  "Fiji",            242, "Finland",          246,
    "France",            250, "French Guiana",    254, "French Polynesia", 258,
    "French Southern Territories", 260,
    "Gabon",             266, "Gambia",           270, "Georgia",          268,
    "Germany",           276, "Ghana",            288, "Gibraltar",        292,
    "Greece",            300, "Greenland",        304, "Grenada",          308,
    "Guadeloupe",        312, "Guam",             316, "Guatemala",        320,
    "Guernsey",          831, "Guinea",           324, "Guinea-Bissau",    624,
    "Guyana",            328, "Haiti",            332,
    "Heard Island and McDonald Islands", 334,
    "Holy See (Vatican City State)", 336,
    "Honduras",          340, "Hong Kong",        344, "Hungary",          348,
    "Iceland",           352, "India",            356, "Indonesia",        360,
    "Iran, Islamic Republic of", 364,
    "Iraq",              368, "Ireland",          372, "Isle of Man",      833,
    "Israel",            376, "Italy",            380, "Jamaica",          388,
    "Japan",             392, "Jersey",           832, "Jordan",           400,
    "Kazakhstan",        398, "Kenya",            404, "Kiribati",         296,
    "Korea, North",      408", "Korea, South",    410, "Kuwait",           414,
    "Kyrgyzstan",        417, "Lao",              418, "Latvia",           428,
    "Lebanon",           422, "Lesotho",          426, "Liberia",          430,
    "Libya",             434, "Liechtenstein",    438, "Lithuania",        440,
    "Luxembourg",        442, "Macao",            446, "Macedonia",        807,
    "Madagascar",        450, "Malawi",           454, "Malaysia",         458,
    "Maldives",          462, "Mali",             466, "Malta",            470,
    "Marshall Islands",  584, "Martinique",       474, "Mauritania",       478,
    "Mauritius",         480, "Mayotte",          175, "Mexico",           484,
    "Micronesia",        583, "Moldova",          498, "Monaco",           492,
    "Mongolia",          496, "Montenegro",       499, "Montserrat",       500,
    "Morocco",           504, "Mozambique",       508, "Myanmar",          104,
    "Namibia",           516, "Nauru",            520, "Nepal",            524,
    "Netherlands",       528, "New Caledonia      540, "New Zealand,       554,
    "Nicaragua",         558, "Niger"             562, "Nigeria",          566,
    "Niue",              570, "Norfolk Island",   574,
    "Northern Mariana Islands", 580,
    "Norway",            578, "Oman",             512, "Pakistan",         586,
    "Palau",             585, "Palestine",        275, "Panama",           591,
    "Papua New Guinea",  598, "Paraguay",         600, "Peru",             604,
    "Philippines",       608, "Pitcairn",         612, "Poland",           616,
    "Portugal",          620, "Puerto Rico",      630, "Qatar",            634,
    "Réunion",           638, "Romania",          642,
    "Russian Federation", 643,
    "Rwanda",            646, "Saint Barthélemy", 652,
    "Saint Helena, Ascension and Tristan da Cunha", 654,
    "Saint Kitts and Nevis", 659,
    "Saint Lucia",       662, "Saint Martin",     663,
    "Saint Pierre and Miquelon", 666,
    "Saint Vincent and the Grenadines", 670,
    "Samoa",             882, "San Marino",       674,
    "Sao Tome and Principe", 678,
    "Saudi Arabia",      682, "Senegal",          686, "Serbia",           688,
    "Seychelles",        690, "Sierra Leone",     694, "Singapore",        702,
    "Sint Maarten",      534, "Slovakia",         703, "Slovenia",         705,
    "Solomon Islands",   090, "Somalia",          706,
    "South Africa",      710,
    "South Georgia and the South Sandwich Islands", 239,
    "South Sudan",       728, "Spain",            724, "Sri Lanka",        144,
    "Sudan",             729, "Suriname",         740, "Svalbard and Jan Mayen", 744,
    "Swaziland",         748, "Sweden",           752, "Switzerland",      756,
    "Syria",             760, "Taiwan",           158, "Tajikistan",       762,
    "Tanzania",          834, "Thailand",         764, "Timor-Leste",      626,
    "Togo",              768, "Tokelau",          772, "Tonga",            776,
    "Trinidad and Tobago", 780, "Tunisia",        788, "Turkey",           792,
    "Turkmenistan",      795, "Turks and Caicos Islands",    796,
    "Tuvalu",            798, "Uganda",           800, "Ukraine",          804,
    "United Arab Emirates", 784,
    "United Kingdom",    826, "United States",    840,
    "United States Minor Outlying Islands", 581,
    "Uruguay",           858, "Uzbekistan",       860,
    "Vanuatu",           548, "Venezuela",        862,
    "Viet Nam",          704, "Virgin Islands, British", 092,
    "Virgin Islands, U.S.", 850,
    "Wallis and Futuna", 876, "Western Sahara",   732, "Yemen",            887,
    "Zambia",            894, "Zimbabwe",         716

};

void pa_countrys(
    /** string buffer */           char* s,
    /** length of buffer */        int len,
    /** ISO 3166-1 country code */ int c)

{

    countryety* p; /* pointer to language entry */

    p = countrytab;
    while (p->countrynum && p-countrynum != l) p++;
    if (!p-countrynum) error("Country number invalid");
    if (strlen(p->countrystr) > len) error("Country string too large for buffer")
    strncpy(s, p->countrystr, len);

}

/** ****************************************************************************

Find timezone offset

Finds the host location offset for the GMT to local time in seconds. It is
negative for zones west of the prime meridian, and positive for zones east.

*******************************************************************************/

int pa_timezone(void)

{

    time_t t;        /* seconds time holder */
    struct tm* pgmt; /* time structure gmt */
    struct tm* plcl; /* local time structure */

    t = time(NULL); /* get seconds time */
    pgmt = localtime(&t); /* get gmt */
    plcl = localtime(&t); /* get local */

    return pgmt->tm_hour-plcl->tm_hour; /* return hour difference */

}

/** ****************************************************************************

Find daylight savings time

Finds if daylight savings time is in effect. It returns true if daylight savings
time is in effect at the present time, which in the majority of locations means
to add one hour to the local time (some locations offset by 30 minutes).

daysave() is automatically adjusted for time of year. That is, if the location
uses daylight savings time, but it is not currently in effect, the function
returns false.

Note that local() already takes daylight savings into account.

*******************************************************************************/

int pa_daysave(void)


{

    time_t t; /* seconds time holder */
    struct tm* plcl; /* local time structure */

    t = time(NULL); /* get seconds time */
    plcl = localtime(&t); /* get local */

    return pcl->tm_isdst; /* return dst active status */

}

/** ****************************************************************************

Find if 12 or 24 hour time is in effect

Returns true if 24 hour time is in use in the current host location.

*******************************************************************************/

int pa_time24hour(void);

{

    return 0;

}

/** ****************************************************************************

Find language code

Finds a numeric code for the host language using the ISO 639-1 language list.
639-1 does not prescribe a numeric code for languages, so the exact code is
defined by the Petit Ami standard from an alphabetic list of the 639-1
languages. This unfortunately means that any changes or additions must
necessarily be added at the end, and thus out of order.

*******************************************************************************/

int pa_language(void)

{

    return 30; /* english */

}

/** ****************************************************************************

Find language identifier string from language code

Finds a language identifer string from a given language code. If the identifier
string is too long for the string buffer, an error results.

*******************************************************************************/

typedef struct {

    int langnum;
    char *langstr;

} langety;

langety langtab[] = {

    1,  "Afan",         36, "French",      71,  "Lithuanian",     106, "Siswati",
    2,  "Abkhazian",    37, "Frisian",     72,  "Macedonian",     107, "Slovak",
    3,  "Afar",         38, "Galician",    73,  "Malagasy",       108, "Slovenian",
    4,  "Afrikaans",    39, "Georgian",    74,  "Malay",          109, "Somali",
    5,  "Albanian",     40, "German",      75,  "Malayalam",      110, "Spanish",
    6,  "Amharic",      41, "Greek",       76,  "Maltese",        111, "Sudanese",
    7,  "Arabic",       42, "Greenlandic", 77,  "Maori",          112, "Swahili",
    8,  "Armenian",     43, "Guarani",     78,  "Marathi",        113, "Swedish",
    9,  "Assamese",     44, "Gujarati",    79,  "Moldavian",      114, "Tagalog",
    0,  "Aymara",       45, "Hausa",       80,  "Mongolian",      115, "Tajik",
    11, "Azerbaijani",  46, "Hebrew",      81,  "Nauru",          116, "Tamil",
    12, "Bashkir",      47, "Hindi",       82,  "Nepali",         117, "Tatar",
    13, "Basque",       48, "Hungarian",   83,  "Norwegian",      118, "Tegulu",
    14, "Bengali",      49, "Icelandic",   84,  "Occitan",        119, "Thai",
    15, "Bhutani",      50, "Indonesian",  85,  "Oriya",          120, "Tibetan",
    16, "Bihari",       51, "Interlingua", 86,  "Pashto",         121, "Tigrinya",
    17, "Bislama",      52, "Interlingue", 87,  "Persian",        122, "Tonga",
    18, "Breton",       53, "Inupiak",     88,  "Polish",         123, "Tsonga",
    19, "Bulgarian",    54, "Inuktitut",   89,  "Portuguese",     124, "Turkish",
    20, "Burmese",      55, "Irish",       90,  "Punjabi",        125, "Turkmen",
    21, "Byelorussian", 56, "Italian",     91,  "Quechua",        126, "Twi",
    22, "Cambodian",    57, "Japanese",    92,  "Rhaeto-Romance", 127, "Uigur",
    23, "Catalan",      58, "Javanese",    93,  "Romanian",       128, "Ukrainian",
    24, "Chinese",      59, "Kannada",     94,  "Russian",        129, "Urdu",
    25, "Corsican",     60, "Kashmiri",    95,  "Samoan",         130, "Uzbek",
    26, "Croatian",     61, "Kazakh",      96,  "Sangro",         131, "Vietnamese",
    27, "Czech",        62, "Kinyarwanda", 97,  "Sanskrit",       132, "Volapuk",
    28, "Danish",       63, "Kirghiz",     98,  "ScotsGaelic",    133, "Welch",
    29, "Dutch",        64, "Kirundi",     99,  "Serbian",        134, "Wolof",
    30, "English",      65, "Korean",      100, "Serbo-Croatian", 135, "Xhosa",
    31, "Esperanto",    66, "Kurdish",     101, "Sesotho",        136, "Yiddish",
    32, "Estonian",     67, "Laothian",    102, "Setswana",       137, "Yoruba",
    33, "Faeroese",     68, "Latin",       103, "Shona",          138, "Zhuang",
    34, "Fiji",         69, "Latvian",     104, "Sindhi",         139, "Zulu",
    35, "Finnish",      70, "Lingala",     105, "Singhalese",     0,   ""

};

void pa_languages(char* s, int len, int l)

{

    langety* p; /* pointer to language entry */

    p = langtab;
    while (p->langnum && p-langnum != l) p++;
    if (!p-langnum) error("Language number invalid");
    if (strlen(p->langstr) > len) error("Language string too large for buffer")
    strncpy(s, p->langstr, len);

}

/** ****************************************************************************

Find the current decimal point character

Finds the decimal point character of the host, which is generally '.' or ','.

*******************************************************************************/

char pa_decimal(void)

{

    return '.';

}

/** ****************************************************************************

Finds the number separator

finds the number separator of the host, which is generally ',' or '.', and is
generally used to mark 3 digit groups, ie., 3,000,000.

*******************************************************************************/

char pa_numbersep(void)

{

    return ',';

}

/** ****************************************************************************

Find the time order

Returns a code for order of time presentation, which is:

1   hour-minute-second
2   hour-second-minute
3   minute-hour-second
4   minute-second-hour
5   second-minute-hour
6   second-minute-hour

The #1 format is the recommended standard for international exchange and is
compatible with computer sorting. Thus it can be common to override the local
presentation with it for archival use.

Note that times() compensates for this.

*******************************************************************************/

int pa_timeorder(void)

{

    return 1;

}

/** ****************************************************************************

Find the date order

Returns a code for order of date presentation, which is:

1   year-month-day
2   year-day-month
3   month-day-year
4   month-year-day
5   day-month-year
6   day-year-month

The #1 format is the recommended standard for international exchange and is
compatible with computer sorting. Thus it can be common to override the local
presentation with it for archival use.

The representation of year as 2 digits is depreciated, due to year 2000 issues
and because it makes the ordering of y-m-d more obvious.

Note that dates() compensates for this.

*******************************************************************************/

int pa_dateorder(void);

{

    return 1;

}
/** ****************************************************************************

Find date separator character

Finds the date separator character of the host.

Note that dates() uses this character.

*******************************************************************************/

char pa_datesep(void)

{

    return '/';

}

/** ****************************************************************************

Find time separator character

Finds the time separator character of the host.

Note that times() uses this character.

*******************************************************************************/

char pa_timesep(void)

{

    return ':';

}

/** ****************************************************************************

Find the currency marker character

Finds the currency symbol of the host country.

*******************************************************************************/

char pa_currchr(void)

{

    return '$';

}

/** ****************************************************************************

Start new thread

Starts a new thread. The address the thread is to run at is specified, and the
location of an integer to place the thread id that is generated. The thread
is started, then the thread id is placed for the caller.

Note that the thread ids are numbered from 1 to N, and that thread 1 is always
reserved for the main thread.

*******************************************************************************/

void pa_newthread(int addr, int *id)

{

}

/** ****************************************************************************

Kill thread

Kills and removes the indicated thread. It is possible to kill oneself, in which
case the routine may not return. However, there are a lot of different
interpretations of what that means, especially in the multiprocessor case. It is
certainly possible that the deleted thread will continue even for a time after
being killed because of interprocessor signal latency.

*******************************************************************************/

void pa_killthread(int id)

{

}

/** ****************************************************************************

Flag signal

Flags a signal to all threads. Accepts the signal id. All threads have a logical
"copy" of the signal status, either not flagged (false), or flagged (true). When
a signal occurs, all threads have the signal status set true. Each thread will
check the status of its copy of the signal separately, and set the flag to "off"
after having checked it. Multiple signals without having checked it have no
effect for those threads that have the signal flag already set.

Note: uses pulseevent, which the Windows documents state has issues. Further,
a setevent/resetevent has the same issue (see MSDN article 173260). The
alternative the Windows documents give is to use a construct exclusive to Vista,
which is not at present a workable alternative, since Windows XP must be
supported.

The issue boils down to request/acknowledge pairing, and the code here satisfies
that. We signal, give the other thread a chance to run, then loop until our
request counts clear. This means that we will retry continually a
non-functioning signal.

*******************************************************************************/

void pa_signal(int* sid)

{

}

/** ****************************************************************************

Flag "one only" signal

This is the same as "signal", except that only the thread that has been waiting
the longest for the signal is flagged for the signal. This can be more efficent
than signaling all threads when only one thread can actually use the signal.
For example, a buffer not empty signal needs only one thread.

If there are no threads awaiting the signal, then the implementation can either
just flag all threads as per the signal call, or may implement a conditional
flag that is only given to the first thread that looks at the signal.

Note that it is possible that an implementation can just implement signalone
as a call to signal.

Note: For the Windows NT/XP version, we pulse the event then see if it cleared
one, then try again, etc. This is how it has to work to get around Windows
problems. However, the documentation implies that more than one thread could
be released via this method. This fits within the definition of this function
that allows the release of from 1 to N threads, where N is all threads waiting.

*******************************************************************************/

void pa_signalone(int* sid)

{

}

/** ****************************************************************************

Wait signal

Awaits the indicated signal by id. If the signal has already happened, then
the call returns immediately. If not, the current thread is suspended until the
signal occurs. The signal is always cleared on exit. Each thread effectively
has its own copy of the signal, an a signal will remain true for each thread
until waited on.

Signals are not "guaranteed", that is, there is no indication that
another thread might not have serviced the event associated with the signal. This
applies even to the "signalone" call. For this reason, the caller must be ready
to check if the event is still active, and if necessary, loop calling wait until
an unserviced signal is found.

In addition, signal queues can be implemented either as "fair" (first come,
first serve), or "unfair", in which case the thread flagged is a random one.

*******************************************************************************/

void pa_wait(int lid, int* sid)

{

}

/** ****************************************************************************

Create lock

Creates a new critical section lock, and returns the id for the lock. Logical
lock ids are numbers from 1-N. IP Pascal garantees that at least 10 locks will
be available, but 100 locks or more are common.

*******************************************************************************/

void pa_newlock(int* id)

{

}

/** ****************************************************************************

Remove lock

Removes a lock by logical id. If any thread is awaiting in a lock, then an error
results. Removing the lock frees up its logical id for reuse.

*******************************************************************************/

void pa_displock(int id)

{

}

/** ****************************************************************************

Lock critical section

Activates a critical section by logical id. If the lock is not active, it is
flagged active, and the thread continues. If the lock is active, then the
calling thread is suspended until it is. Typically, lock queuing is "fair", in
that the first to come to the lock is the first, in order, receive it. However,
this behavior is not garanteed.

Critical section locks have a number of features designed to resist deadlock.
If the same thread performs the same lock more than once, the lock request is
simply counted on lock, and decremented on unlock. When the lock count equals
zero, the unlock operation actually occurs. This covers the case where a thread
recursively executes the same locking routine, either directly or indirectly.

In addition, waiting for a signal while holding a lock causes the lock to be
freed while waiting for the signal, then the lock is reasserted when the signal
flags and the thread continues. Callers must, therefore, treat a "wait" call
as the sequence:

unlock
wait
lock

And watch for data change after the wait for signal.

*******************************************************************************/

void pa_lock(int id)

{

}

/** ****************************************************************************

Unlock critical section

Frees up the lock for a critical section by logical id. If other threads are
waiting on the lock, they are scheduled to run, but the exact order is system
defined. They may be run in fair order, and they may run immediately
(presumably interrupting the current thread), or may be scheduled for later.

*******************************************************************************/

void pa_unlock(int id)

{

}

/** ****************************************************************************

Initialize services

We initialize all variables and tables.

*******************************************************************************/

static void pa_init_services (void) __attribute__((constructor (102)));
static void pa_init_services()

{

    int envlen; /* number of environment strings */
    char **ep; /* unix environment string table */
    int ei;    /* index for string table */
    int si;    /* index for strings */
    envrec *p; /* environment entry pointer */

    /* Copy environment to local */
    envlst = NULL;   /* clear environment strings */
    ep = allenv();   /* get unix environment pointers */
    envlen = strlen(ep);
    for (ei = 1; ei <= envlen; ei++) {  /* copy environment strings */

        p = malloc(sizeof(envrec));   /* get a new environment entry */
        p->next = envlst; /* push onto environment list */
        envlst = p;
        si = index(ep[ei-1], "="); /* find location of '=' */
        if (si == 0) error("Invalid environment string format");
        extract(p->name, ep[ei-1], 1, si - 1); /* get the name string */
        /* get the data string */
        extract(p->data, ep[ei-1], si + 1, strlen(ep[ei-1]));

    }
    free(ep); /* release environment table */
    getenv("path", pthstr, MAXSTR); /* load up the current path */
    trim(pthstr, pthstr); /* make sure left aligned */

}

/** ****************************************************************************

Deinitialize services

Not used at present

*******************************************************************************/

static void pa_deinit_terminal (void) __attribute__((destructor (102)));
static void pa_deinit_terminal()

{

    int ti; /* index for timers */

    /* restore terminal */
    tcsetattr(0,TCSAFLUSH,&trmsav);

    /* close any open timers */
    for (ti = 0; ti < MAXTIM; ti++) if (timtbl[ti] != -1) close(timtbl[ti]);

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    punlink_t cppunlink;
    plseek_t cpplseek;

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);

    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose || cppunlink != iunlink || cpplseek != ilseek)
        error(esysflt);

}
