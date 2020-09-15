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
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
*******************************************************************************/

#include <errno.h>
#include <sys/types.h>
//#include <sys/status.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>

#include <services.h> /* the header for this file */

/* contains the program invocation path */
extern char *program_invocation_name;

/* contains the entire environment strings array */
extern char **environ;

/* give bit in word from ordinal position */
#define BIT(b) (1 << b)

#define HOURSEC         3600   /* number of seconds in an hour */
#define DAYSEC          (HOURSEC * 24)   /* number of seconds in a day */
#define YEARSEC         (DAYSEC * 365)   /* number of seconds in year */
/* Unix time adjustment for 1970 */
#define UNIXADJ         (YEARSEC * 30 + DAYSEC * 7)

/* maximum size of holding buffers (I had to make this very large for large
   paths [sam]) */
#define MAXSTR          500

typedef char bufstr[MAXSTR]; /* standard string buffer */

#define MAXARG 1000 /* maximum number of argv strings */
#define MAXENV 10000 /* maximum number of environment strings */

static bufstr pthstr;   /* buffer for execution path */

static pa_envrec *envlst;   /* our environment list */

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

    error(strerror(errno));

}

/********************************************************************************

Check file exists

Checks if the named file exists. Returns true if so.

********************************************************************************/

static int exists(char *fn)
{

    FILE *fp;

    if (fp = fopen(fn, "r")) fclose(fp);

    return !!fp;

}

/********************************************************************************

Extract string

Extracts a substring. The characters in the source string indicated are extracted
and placed into the destination string. It is an error if the number of
characters in the substring are too large for the destination.

********************************************************************************/

static void extract(char *d, int l, char *s, int st, int ed)
{

    char *ds = d;

    if (ed-st+1 > l) error("String too large for desination");
    while (st <= ed) *d++ = s[st++];
    *d = 0;

}

/********************************************************************************

Trim leading and trailing spaces off string.

Removes leading and trailing spaces from a given string. Since the string will
shrink by definition, no length is needed.

********************************************************************************/

static void trim(char *s)
{

    while (*s == ' ') s++;
    while (*s != ' ' && *s) s++;
    *s = 0;

}

/********************************************************************************

Find number of words in string

Finds the number of space delimited words in a string.

********************************************************************************/

static int words(char *s)
{

    int wc;
    int ichar;
    int ispace;

    wc = 0;
    ichar = 0;
    ispace = 0;
    while (*s) {

        if (*s == ' ') {

            if (!ispace) { ispace = 1; ichar = 0; }

        } else {

            if (!ichar) { ichar = 1; ispace = 0; wc++; }

        }
        s++;

    }

    return wc;

}

/********************************************************************************

Extract words from string

Extracts a series of space delimited words from a string.

********************************************************************************/

static void extwords(char *d, int dl, char *s, int st, int ed)
{

    int wc;
    int ichar;
    int ispace;

    wc = 0;
    ichar = 0;
    ispace = 0;
    if (dl < 1) error("String too large for destination");
    dl--; /* create room for terminator */
    while (*s) {

        if (*s == ' ') {

            if (ichar) wc++; /* count end of word */
            if (!ispace) { ispace = 1; ichar = 0; }

        } else {

            if (!ichar) { ichar = 1; ispace = 0; }
            if (wc >=st && wc <= ed) {

                if (!dl) error("String too large for destination");
                *d++ = *s;
                dl--;

            }

        }
        s++;

    }
    *d = 0;

}

/********************************************************************************

Match filenames with wildcards

match with wildcards at the given a and b positions. we use shortest string first
matching. Wildcards are only recognized in string a.

********************************************************************************/

static int match(char *a, char *b, int ia, int ib)

{

    int m;

    m = 1;   /* default to matches */
    while (ia < strlen(a) && ib < strlen(b) && m) {  /* match characters */

        if (a[ia] == '*') { /* multicharacter wildcard, go searching */

            /* skip all wildcards in match expression name. For each '*' or
               '?', we skip one character in the matched name. The idea being
               that '*' means 1 or more matched characters */
            while (ia < strlen(a) && ib < strlen(b) &&
                   (a[ia] == '?' || a[ia] == '*')) {

                ia++;   /* next character */
                ib++;

            }
            /* recursively match to string until we find a match for the rest
               or run out of string */
            while (ib < strlen(b) && !match(a, b, ia, ib)) ib++;
            if (ib >= strlen(b)) m = 0; /* didn't match, set false */
            else { ia = strlen(a); ib = strlen(b); } /* terminate */

        } else if (a[ia] != b[ib] && a[ia] != '?') m = 0; /* fail match */
        else { ia++; ib++; }

    }

    return ia == strlen(a) && ib == strlen(b);

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
    /** file list returned */ pa_filrec **l
)

{

    struct dirent* dr; /* Unix directory record */
    DIR*           dd; /* directory file descriptor */
    int            r;  /* result code */
    struct stat    sr; /* stat() record */
    pa_filrec*     fp; /* file entry pointer */
    pa_filrec*     lp; /* last entry pointer */
    int            i;  /* name index */
    bufstr         p;  /* filename components */
    bufstr         n;
    bufstr         e;
    bufstr         fn; /* holder for directory name */
    int            ls;

    *l = NULL; /* clear destination list */
    lp = NULL; /* clear last pointer */
    pa_brknam(f, p, MAXSTR, n, MAXSTR, e, MAXSTR); /* break up filename */
    /* check wildcards in path */
    if (strstr(p, "*") || strstr(p, "?")) error("Path cannot contain wildcards");
    /* construct name of containing directory */
    pa_maknam(fn, MAXSTR, p, ".", "");
    dd = opendir(fn); /* open the directory */
    if (!dd) unixerr(); /* process unix open error */
    pa_maknam(fn, MAXSTR, "", n, e);   /* reform name without path */
    do { /* read directory entries */

        errno = 0; /* clear any error */
        dr = readdir(dd);
        if (errno) unixerr(); /* process unix error */
        if (dr) { /* valid next */

            if (match(fn, dr->d_name, 0, 0)) { /* matching filename, add to list */

                fp = malloc(sizeof(pa_filrec)); /* create a new file entry */
                /* copy to new filename string */
                fp->name = malloc(strlen(dr->d_name)+1);
                strcpy(fp->name, dr->d_name);
                r = stat(fp->name, &sr); /* get stat structure on file */
                if (r < 0) unixerr(); /* process unix error */
                /* file information in stat record, translate to our format */
                strcpy(fp->name, dr->d_name);   /* place filename */
                fp->size = sr.st_size;   /* place size */
                /* there is actually a real unix allocation, but I haven't figgured out
                   how to calculate it from block/blocksize */
                fp->alloc = sr.st_size;   /* place allocation */
                fp->attr = 0;   /* clear attributes */
                /* clear permissions to all is allowed */
                fp->user = BIT(pa_pmread) | BIT(pa_pmwrite) | BIT(pa_pmexec) | BIT(pa_pmdel) |
                           BIT(pa_pmvis) | BIT(pa_pmcopy) | BIT(pa_pmren);
                fp->other = BIT(pa_pmread) | BIT(pa_pmwrite) | BIT(pa_pmexec) | BIT(pa_pmdel) |
                            BIT(pa_pmvis) | BIT(pa_pmcopy) | BIT(pa_pmren);
                fp->group = BIT(pa_pmread) | BIT(pa_pmwrite) | BIT(pa_pmexec) | BIT(pa_pmdel) |
                            BIT(pa_pmvis) | BIT(pa_pmcopy) | BIT(pa_pmren);
                /* check and set directory attribute */
                if (sr.st_mode & S_IFDIR) fp->attr |= BIT(pa_atdir);
                /* check and set any system special file */
                if (sr.st_mode & S_IFIFO) fp->attr |= BIT(pa_atsys);
                if (sr.st_mode & S_IFCHR) fp->attr |= BIT(pa_atsys);
                if (sr.st_mode & S_IFBLK) fp->attr |= BIT(pa_atsys);
                /* check hidden. in Unix, this is done with a leading '.'. We remove
                   visiblity priveledges */
                if (dr->d_name[0] == '.') {

                    fp->user &= ~BIT(pa_pmvis);
                    fp->group &= ~BIT(pa_pmvis);
                    fp->other &= ~BIT(pa_pmvis);

                }
                /* check and set executable attribute. Unix has separate executable
                   permissions for each permission type, we set executable if any of
                   them are true */
                if (sr.st_mode & S_IXUSR) fp->attr |= BIT(pa_atexec);
                /* set execute permissions to user */
                if (sr.st_mode & S_IXUSR) fp->user &= ~BIT(pa_pmexec);
                /* set read permissions to user */
                if (sr.st_mode & S_IRUSR) fp->user &= ~BIT(pa_pmread);
                /* set write permissions to user */
                if (sr.st_mode & S_IWUSR) fp->user &= ~BIT(pa_pmwrite);
                /* set execute permissions to group */
                if (sr.st_mode & S_IXGRP) fp->group &= ~BIT(pa_pmexec);
                /* set read permissions to group */
                if (sr.st_mode & S_IRGRP) fp->group &= ~BIT(pa_pmread);
                /* set write permissions to group */
                if (sr.st_mode & S_IWGRP) fp->group &= ~BIT(pa_pmwrite);
                /* set execute permissions to other */
                if (sr.st_mode & S_IXOTH) fp->other = fp->group & (~BIT(pa_pmexec));
                /* set read permissions to other */
                if (sr.st_mode & S_IROTH) fp->other = fp->group & (~BIT(pa_pmread));
                /* set write permissions to other */
                if (sr.st_mode & S_IWOTH) fp->other = fp->group & (~BIT(pa_pmwrite));
                /* set times */
                fp->create = sr.st_ctime-UNIXADJ;
                fp->modify = sr.st_mtime-UNIXADJ;
                fp->access = sr.st_atime-UNIXADJ;
                fp->backup = -INT_MAX; /* no backup time for Unix */
                /* insert entry to list */
                if (*l == NULL) *l = fp; /* insert new top */
                else lp->next = fp; /* insert next entry */
                lp = fp;   /* set new last */
                fp->next = NULL;   /* clear next */

            }

        }

    } while (dr);
    r = closedir(dd);
    if (r < 0) unixerr();  /* process unix error */

}

/********************************************************************************

Get time string

Converts the given time into a string.

********************************************************************************/

void pa_times(
    /** result string */           char *s,
    /** result string length */    int sl,
    /** time to convert */         int t
)

{

    int h;   /* hour */
    int m;   /* minute */
    int sec; /* second */
    int  am;  /* am flag */
    int  pm;  /* pm flag */

    if (sl < 11-(!pa_time24hour()*3)) /* string to small to hold result */
        error("String buffer to small to hold time");
    /* because leap adjustments are made in terms of days, we just remove
       the days to find the time of day in seconds. this is completely
       independent of leap adjustments */
    t %= DAYSEC;   /* find number of seconds in day */
    /* if before 2000, find from remaining seconds */
    if (t < 0) t += DAYSEC;
    h = t / HOURSEC;   /* find hours */
    t %= HOURSEC;   /* subtract hours */
    m = t / 60;   /* find minutes */
    sec = t % 60;   /* find seconds */
    pm = 0; /* clear am and pm flags */
    am = 0;
    if (!pa_time24hour()) { /* do am/pm adjustment */

        if (h == 0) h = 12; /* hour zero */
        else if (h > 12) { h -= 12; pm = 1; } /* 1 pm to 11 pm */

    }
    /* place hour:miniute:second */
    switch (pa_timeorder()) {

        case 1:
            s += sprintf(s, "%02d%c%02d%c%02d", h, pa_timesep(), m, pa_timesep(), sec);
            break;
        case 2:
            s += sprintf(s, "%02d%c%02d%c%02d", h, pa_timesep(), sec, pa_timesep(), m);
            break;
        case 3:
            s += sprintf(s, "%02d%c%02d%c%02d", m, pa_timesep(), h, pa_timesep(), sec);
            break;
        case 4:
            s += sprintf(s, "%02d%c%02d%c%02d", m, pa_timesep(), sec, pa_timesep(), h);
            break;
        case 5:
            s += sprintf(s, "%02d%c%02d%c%02d", sec, pa_timesep(), h, pa_timesep(), m);
            break;
        case 6:
            s += sprintf(s, "%02d%c%02d%c%02d", sec, pa_timesep(), m, pa_timesep(), h);
            break;

    }
    if (pm) strcat(s, " pm");
    if (am) strcat(s, " am");
    *s = 0; /* terminate string */

}

/********************************************************************************

Get date string

Converts the given date into a string.

********************************************************************************/

/*
 * Check year is a leap year
 */
#define LEAPYEAR(y) ((y & 3) == 0 && y % 100 != 0 || y % 400 == 0)

void pa_dates(
    /** string to place date into */   char *s,
    /** string to place date length */ int sl,
    /** time record to write from */   int t
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

    if (sl < 10) /* string to small to hold result */
        error("*** String to small to hold date");
    if (t < 0) y = 1999; else y = 2000; /* set initial year */
    done = 0;   /* set no loop exit */
    t = abs(t);   /* find seconds magnitude */
    do {

        yd = 365;   /* set days in this year */
        if (LEAPYEAR(y)) yd = 366; /* set leap year days */
        else  yd = 365;
        /* set normal year days */
        if (t/DAYSEC > yd) {  /* remove another year */

            if (y >= 2000) y++; else y--; /* find next year */
            t -= yd * DAYSEC; /* remove that many seconds */

        } else done = 1;

    } while (!done);   /* until year found */
    leap = 0;   /* set no leap day */
    /* check leap year, and set leap day accordingly */
    if (LEAPYEAR(y)) leap = 1;
    t = t/DAYSEC+1; /* find days into year */
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
    switch (pa_dateorder()) { /* place according to current location format */

        case 1:
            s += sprintf(s, "%04d%c%02d%c%02d", y, pa_datesep(), m, pa_datesep(), d);
            break;
        case 2:
            s += sprintf(s, "%04d%c%02d%c%02d", y, pa_datesep(), d, pa_datesep(), m);
            break;
        case 3:
            s += sprintf(s, "%02d%c%02d%c%04d", m, pa_datesep(), d, pa_datesep(), y);
            break;
        case 4:
            s += sprintf(s, "%02d%c%04d%c%02d", m, pa_datesep(), y, pa_datesep(), d);
            break;
        case 5:
            s += sprintf(s, "%02d%c%02d%c%04d", d, pa_datesep(), m, pa_datesep(), y);
            break;
        case 6:
            s += sprintf(s, "%02d%c%04d%c%02d", d, pa_datesep(), y, pa_datesep(), m);
            break;

    }
    *s = 0; /* terminate string */

}

/********************************************************************************

Write time

Writes the time to a given file, from a time record.

********************************************************************************/

void pa_writetime(
        /** file to write to */ FILE *f,
        /** time record to write from */ int t
)

{

    bufstr s;

    pa_times(s, MAXSTR, t);   /* convert time to string form */
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

    pa_dates(s, MAXSTR, t);   /* convert date to string form */
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

    return ((int) r-UNIXADJ);   /* return S2000 time */

}


/********************************************************************************

Convert to local time

Converts a GMT standard time to the local time using time zone and daylight
savings. Does not compensate for 30 minute increments in daylight savings or
timezones.

********************************************************************************/

int pa_local(int t)
{

    return t+pa_timezone()+pa_daysave()*HOURSEC;

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

    struct timeval tv; /* record to get time */
    int            r;  /* return value */

    r = gettimeofday(&tv, NULL); /* get time info */
    if (r < 0) unixerr(); /* process unix error */

    /* for Unix, the time is kept in microseconds since the start of the last
       second. we find the number of 100usecs, then add 48 hours worth of
       seconds from standard time */
    return (tv.tv_usec / 100 + tv.tv_sec % (DAYSEC * 2) * 10000);

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

    t = pa_clock();   /* get the current time */
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
    if (!*s) r = 0; /* no filename exists */

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
    if (!*s) r = 0; /* no filename exists */

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
    if (ln) { /* not null */

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

static void fndenv(
    /* string name */                      char*       esn,
    /* returns environment string entry */ pa_envptr*  ep
)

{

    pa_envptr p; /* pointer to environment entry */

    p = envlst; /* index top of environment list */
    *ep = NULL; /* set no string found */
    while (p && *ep == NULL) {  /* traverse */

        if (!strcmp(esn, p->name)) *ep = p; /* found */
        else p = p->next;/* next string */

    }

}

/*******************************************************************************

Get environment string

Returns an environment string by name.

*******************************************************************************/

void pa_getenv(
    /** string name */        char* esn,
    /** string data */        char* esd,
    /** string data length */ int esdl
)
{

    pa_envrec *p;

    *esd = 0;
    fndenv(esn, &p);
    if (p) {

        if (strlen(p->data)+1 > esdl) error("String too large for destination");
        if (p) strcpy(esd, p->data);

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

    pa_envrec *p;   /* pointer to environment entry */

    fndenv(sn, &p); /* find environment string */
    if (p) { /* found */

        free(p->data);   /* release last contents */
        /* create new data string */
        p->data = (char *) malloc(strlen(sd));
        if (!p) error("Could not allocate string");
        strcpy(p->data, sd);

    } else {

        p = malloc(sizeof(pa_envrec)); /* get a new environment entry */
        if (!p) error("Could not allocate structure");
        p->next = envlst; /* push onto environment list */
        envlst = p;
        /* create new name string and place */
        p->name = (char *) malloc(strlen(sn)+1);
        if (!p) error("Could not allocate string");
        strcpy(p->name, sn);
        /* create new data string and place */
        p->data = (char *) malloc(strlen(sd)+1);
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

    pa_envrec *p, *l; /* pointer to environment entry */

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
        free(p); /* release entry */

    }

}

/********************************************************************************

Get environment strings all

Returns a table with the entire environment string set in it.

********************************************************************************/

void pa_allenv(
    /* environment table */ pa_envrec **el
)

{

    pa_envrec *p, *lp, *tp; /* environment pointers */

    /* copy current environment list */
    lp = envlst; /* index top of environment list */
    tp = NULL; /* clear destination */
    while (lp != NULL) {  /* copy entries */

        p = malloc(sizeof(pa_envrec)); /* create a new entry */
        p->next = tp;   /* push onto list */
        tp = p;
        p->name = (char *) malloc(strlen(lp->name)+1);
        strcpy(p->name, lp->name);   /* place name */
        p->data = (char *) malloc(strlen(lp->data)+1);
        strcpy(p->data, lp->data);   /* place data */
        lp = lp->next;   /* next entry */

    }
    /* reverse to destination */
    *el = NULL; /* clear destination */
    while (tp) {

        p = tp;
        tp = tp->next;
        p->next = *el;
        *el = p;

    }

}

/********************************************************************************

Create argv array from string

Fills an argv array with the words from a given string.

********************************************************************************/

void cpyargv(
    /* command to use */ char *cmd,
    /* argv array */     char *argv[],
    /* argv length */    int max
)
{

    int i;
    int wc;
    bufstr arg;

    wc = words(cmd); /* find number of words in command */
    /* construct argv list for new command */
    for (i = 0; i < wc; i++) {

        extwords(arg, MAXSTR, cmd, i, i);
        argv[i] = malloc(strlen(arg)+1);
        strcpy(argv[i], arg);

    }
    argv[i] = NULL;

}

/********************************************************************************

Create Linux environment array from services format

Creates a copy of a services environment string array from a services format
environment list.

********************************************************************************/

void cpyenv(
    /* services environment list */ pa_envptr env,
    /* Linux environment array */   char *envp[],
    /* Linux environment array length */ int envpl
)
{

    int i;

    i = 0;
    while (env) { /* traverse services environment list */

        if (!envpl) error("Environment list too large");
        envp[i] = malloc(strlen(env->name)+1+strlen(env->data)+1);
        strcpy(envp[i], env->name);
        strcat(envp[i], "=");
        strcat(envp[i], env->data);
        i++;
        envpl--;
        env = env->next;

    }

}

/********************************************************************************

Path program name

Given a program with possible path, checks it exists and tries to path it if it
does not exist and no path is provided. Gives and error if not successful.
Returns the properly pathed command if found.

********************************************************************************/

void cmdpth(
    /* command to search for */           char *cn,
    /* result correctly pathed command */ char *pcn,
    /* result length */                   int  pcnl
)
{

    bufstr p, n, e; /* filename components */
    bufstr pc;      /* path copy */
    bufstr ncn;
    char *cp;

    strcpy(ncn, cn); /* copy command to temp */
    if (!exists(cn)) {  /* does not exist in current form */

        /* perform pathing search */
        pa_brknam(cn, p, MAXSTR, n, MAXSTR, e, MAXSTR); /* break down the name */
        *ncn = 0;
        if (*p == 0 && *pthstr != 0) {

            strcpy(pc, pthstr);   /* make a copy of the path */
            trim(pc);   /* make sure left aligned */
            while (*pc != 0) {  /* match path components */

                cp = strchr(pc, ':' /*pa_pthchr()*/); /* find next path separator */
                if (!cp) {  /* none left, use entire remaining */

                    strcpy(p, pc); /* none left, use entire remaining */
                    pc[0] = 0; /* clear the rest */

                } else { /* copy partial */

                    extract(p, MAXSTR, pc, 0, cp-pc-1);   /* get left side to path */
                    extract(pc, MAXSTR, pc, cp-pc+1, strlen(pc)); /* remove from path */
                    trim(pc); /* make sure left aligned */

                }
                pa_maknam(ncn, MAXSTR, p, n, e);   /* create filename */
                if (exists(ncn)) *pc = 0;  /* found, indicate stop */

            }
            if (!exists(ncn)) error("Command does not exist");

        } else error("Command does not exist");

    }
    if (strlen(ncn)+1 > pcnl) error("String too large for destination\n");
    strcpy(pcn, ncn); /* copy to result */

}

/********************************************************************************

Execute program

Executes a program by name. Does not wait for the program to complete.

********************************************************************************/

void pa_exec(
    /* program name to execute */ char *cmd
)

{

    int r;              /* result code */
    int pid;            /* task id for child process */
    bufstr cn;          /* buffer for command filename */
    int wc;             /* word count in command */

    wc = words(cmd);   /* find number of words in command */
    if (wc == 0)
    error("Command is empty");
    extwords(cn, MAXSTR, cmd, 0, 0);  /* get the command verb */
    cmdpth(cn, cn, MAXSTR); /* fix path */

    /* on fork, the child is going to see a zero return, and the parent will
       get the process id. Although this seems dangerous, forked processes
       are truly independent, and so don't care what language is running */
    pid = fork(); /* start subprocess */
    if (pid == 0) { /* we are the child */

        char *argv[MAXARG]; /* argv array to be passed */
        char *envp[MAXENV]; /* environment string array to be passed */

        cpyargv(cmd, argv, MAXARG); /* construct argv list for new command */
        /* construct environment list for new command */
        cpyenv(envlst, envp, MAXENV);
        r = execve(cn, argv, envp);   /* execute directory */
        if (r < 0) unixerr();  /* process unix error */
        error("Execute failed");

    }

}

/********************************************************************************

Execute program with wait

Executes a program by name. Waits for the program to complete.

********************************************************************************/

void pa_execw(
    /* program name to execute */ char *cmd,
    /* return error */ int *err
)

{

    int r;              /* result code */
    int pid;            /* task id for child process */
    bufstr cn;          /* buffer for command filename */
    int wc;             /* word count in command */

    wc = words(cmd);   /* find number of words in command */
    if (wc == 0)
    error("Command is empty");
    extwords(cn, MAXSTR, cmd, 0, 0);  /* get the command verb */
    cmdpth(cn, cn, MAXSTR); /* fix path */

    /* on fork, the child is going to see a zero return, and the parent will
       get the process id. Although this seems dangerous, forked processes
       are truly independent, and so don't care what language is running */
    pid = fork(); /* start subprocess */
    if (pid == 0) { /* we are the child */

        char *argv[MAXARG]; /* argv array to be passed */
        char *envp[MAXENV]; /* environment string array to be passed */

        cpyargv(cmd, argv, MAXARG); /* construct argv list for new command */
        /* construct environment list for new command */
        cpyenv(envlst, envp, MAXENV);
        r = execve(cn, argv, envp);   /* execute directory */
        if (r < 0) unixerr();  /* process unix error */
        error("Execute failed");

    } else { /* we are the parent */

        waitpid(pid, err, 0);

    }

}

/********************************************************************************

Execute program with environment

Executes a program by name. Does not wait for the program to complete. Supplies
the program environment.

********************************************************************************/

void pa_exece(
    /* program name to execute */ char      *cmd,
    /* environment */             pa_envrec *el
)

{

    int r;              /* result code */
    int pid;            /* task id for child process */
    bufstr cn;          /* buffer for command filename */
    int wc;             /* word count in command */

    wc = words(cmd);   /* find number of words in command */
    if (wc == 0)
    error("Command is empty");
    extwords(cn, MAXSTR, cmd, 0, 0);  /* get the command verb */
    cmdpth(cn, cn, MAXSTR); /* fix path */

    /* on fork, the child is going to see a zero return, and the parent will
       get the process id. Although this seems dangerous, forked processes
       are truly independent, and so don't care what language is running */
    pid = fork(); /* start subprocess */
    if (pid == 0) { /* we are the child */

        char *argv[MAXARG]; /* argv array to be passed */
        char *envp[MAXENV]; /* environment string array to be passed */

        cpyargv(cmd, argv, MAXARG); /* construct argv list for new command */
        /* construct environment list for new command */
        cpyenv(el, envp, MAXENV);
        r = execve(cn, argv, envp);   /* execute directory */
        if (r < 0) unixerr();  /* process unix error */
        error("Execute failed");

    }

}


/********************************************************************************

Execute program with environment and wait

Executes a program by name. Waits for the program to complete. Supplies the
program environment.

********************************************************************************/

void pa_execew(
        /* program name to execute */ char *cmd,
        /* environment */             pa_envrec *el,
        /* return error */            int *err
)

{

    int r;              /* result code */
    int pid;            /* task id for child process */
    bufstr cn;          /* buffer for command filename */
    int wc;             /* word count in command */

    wc = words(cmd);   /* find number of words in command */
    if (wc == 0)
    error("Command is empty");
    extwords(cn, MAXSTR, cmd, 0, 0);  /* get the command verb */
    cmdpth(cn, cn, MAXSTR); /* fix path */

    /* on fork, the child is going to see a zero return, and the parent will
       get the process id. Although this seems dangerous, forked processes
       are truly independent, and so don't care what language is running */
    pid = fork(); /* start subprocess */
    if (pid == 0) { /* we are the child */

        char *argv[MAXARG]; /* argv array to be passed */
        char *envp[MAXENV]; /* environment string array to be passed */

        cpyargv(cmd, argv, MAXARG); /* construct argv list for new command */
        /* construct environment list for new command */
        cpyenv(el, envp, MAXENV);
        r = execve(cn, argv, envp);   /* execute directory */
        if (r < 0) unixerr();  /* process unix error */
        error("Execute failed");

    } else { /* we are the parent */

        waitpid(pid, err, 0);

    }

}

/********************************************************************************

Get current path

Returns the current path in the given padded string.

********************************************************************************/

void pa_getcur(
        /** buffer to get path */ char *fn,
        /** length of buffer */   int l
)

{

    fn = getcwd(fn, l);   /* get the current path */
    if (!fn) unixerr(); /* process unix error */

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
        /* path */               char *p, int pl,
        /* name */               char *n, int nl,
        /* extention */          char *e, int el
)

{

    int i, s, f, t; /* string indexes */
    char *s1, *s2;

    /* clear all strings */
    *p = 0;
    *n = 0;
    *e = 0;
    if (!strlen(fn)) error("File specification is empty");
    s1 = fn; /* index file spec */
    /* skip spaces */
    while (*s1 && *s1 == ' ') s1++;
    /* find last '/' that will mark the path */
    s2 = strrchr(s1, '/');
    if (s2) {

        /* there was a path, store that */
        while (s1 <= s2) {

            if (!pl) error("String to large for destination\n");
            *p++ = *s1++;
            pl--;

        }
        if (!nl) error("String to large for destination\n");
        *p = 0;
        /* now s1 points after path */

    }
    /* keep any leading '.' characters from fooling extension finder */
    s2 = s1; /* copy start point in name */
    while (*s2 == '.') s2++;
    /* find last '.' that will mark the extension */
    s2 = strrchr(s2, '.');
    if (s2) {

        /* there was a name, store that */
        while (s1 < s2) {

            if (!nl) error("String to large for destination\n");
            *n++ = *s1++;
            nl--;

        }
        if (!nl) error("String to large for destination\n");
        *n = 0;
        /* now s1 points to extension or nothing */

    } else {

        /* no extension */
        if (strlen(s1)+1 > nl) error("String to large for destination\n");
        strcpy(n, s1);
        while (*s1) s1++;

    }
    if (*s1 == '.') {

        /* there is an extension */
        if (strlen(s1)+1 > el) error("String to large for destination\n");
        s1++; /* skip '.' */
        strcpy(e, s1);

    }

}

/********************************************************************************

Make specification

Creates a file specification from its components, the path, name and extention.
We make sure that the path is properly terminated with ':' or '\' before
concatenating.

********************************************************************************/

void pa_maknam(
    /** file specification to build */ char *fn,
    /** file specification length */   int fnl,
    /** path */                        char *p,
    /** filename */                    char *n,
    /** extension */                   char *e
)
{

    int i;   /* index for string */
    int fsi;   /* index for output filename */

    strcpy(fn, p); /* place path */
    /* check path properly terminated */
    i = strlen(p);   /* find length */
    if (*p) /* not null */
        if (p[i-1] != '/') strcat(fn, "/"); /* add path separator */
    /* terminate path */
    strcat(fn, n); /* place name */
    if (*e) {  /* there is an extention */

        strcat(fn, "."); /* place '.' */
        strcat(fn, e); /* place extension */

    }

}

/********************************************************************************

Make full file specification

If the given file specification has a default path (the current path), then
the current path is added to it. Essentially "normalizes" file specifications.
No validity check is done. Garbage in, garbage out.

********************************************************************************/

void pa_fulnam(
    /** filename */        char *fn,
    /** filename length */ int fnl
)
{

    /* file specification */
    bufstr p, n, e, ps;   /* filespec components */

    pa_brknam(fn, p, MAXSTR, n, MAXSTR, e, MAXSTR);   /* break spec down */
    /* if the path is blank, then default to current */
    if (!*p) strcpy(p, ".");
    pa_getcur(ps, MAXSTR);   /* save current path */
    pa_setcur(p);   /* set candidate path */
    pa_getcur(p, MAXSTR);   /* get washed path */
    pa_setcur(ps);   /* reset old path */
    /* reassemble */
    pa_maknam(fn, fnl, p, n, e);

}

/********************************************************************************

Get program path

There is no direct call for program path. So we get the command line, and
extract the program path from that.

Note: this does not work for standard CLIB programs. We need another solution.

********************************************************************************/

void pa_getpgm(
    /** program path */        char* p,
    /** program path length */ int   pl
)
{

    bufstr pn;   /* program name holder */
    bufstr n, e;   /* name component holders */

    strcpy(pn, program_invocation_name); /* copy invoke name to path */
    pa_fulnam(pn, MAXSTR);   /* clean that */
    pa_brknam(pn, p, pl, n, MAXSTR, e, MAXSTR); /* extract path from that */

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

void pa_getusr(
    /** pathname */        char *fn,
    /** pathname length */ int fnl
)
{

    bufstr b, b1;   /* buffer for result */
    int i, f;
    char* envnam[] = { "home", "userhome", "userdir", "user", "username",
                       "HOME", "USERHOME", "USERDIR", "USER", "USERNAME",
                       0 };

    *fn = 0; /* clear result */
    /* find applicable environment names, in order */
    for (i = 0, f = -1; envnam[i] && f == -1; i++) {

        pa_getenv(envnam[i], b, MAXSTR);
        if (*b && f < 0) f = i;

    }
    if (f >= 0) {

        /* check names used to derive path */
        if (!strcmp(envnam[f], "user") || !strcmp(envnam[f], "USER") ||
            !strcmp(envnam[f], "username") || !strcmp(envnam[f], "USERNAME")) {

            strcpy(b1, b); /* copy */
            strcpy(b, "/home/"); /* set prefix */
            strcat(b, b1); /* combine */

        }

    } else {

        /* all fails, set to program path */
        pa_getpgm(b, MAXSTR);

    }
    strcpy(fn, b); /* place result */

}

/********************************************************************************

Set attributes on file

Sets any of several attributes on a file. Set directory attribute is not
possible. This is done with makpth.

********************************************************************************/

void pa_setatr(char *fn, pa_attrset a)
{

    /* no unix attributes can be set */

}

/********************************************************************************

Reset attributes on file

Resets any of several attributes on a file. Reset directory attribute is not
possible.

********************************************************************************/

void pa_resatr(char *fn, pa_attrset a)
{

    /* no unix attributes can be reset */

}

/********************************************************************************

Reset backup time

There is no backup time in Unix. Instead, we reset the archive bit,
which effectively means "back this file up now".

********************************************************************************/

void pa_bakupd(char *fn)
{

    pa_setatr(fn, BIT(pa_atarc));

}

/********************************************************************************

Set user permissions

Sets user permisions

********************************************************************************/

void pa_setuper(char *fn, pa_permset p)
{

    struct stat sr; /* stat() record */
    int          r; /* result code */

    r = stat(fn, &sr);   /* get stat structure on file */
    if (r < 0)   /* process unix error */
    unixerr();
    sr.st_mode &= 0777; /* mask permissions */
    if (BIT(pa_pmread) & p) sr.st_mode |= S_IRUSR; /* set read */
    if (BIT(pa_pmwrite) & p) sr.st_mode |= S_IWUSR; /* set write */
    if (BIT(pa_pmexec) & p) sr.st_mode |= S_IXUSR; /* set execute */
    r = chmod(fn, sr.st_mode); /* set mode */
    if (r < 0) unixerr();  /* process unix error */

}


/********************************************************************************

Reset user permissions

Resets user permissions.

********************************************************************************/

void pa_resuper(char *fn, pa_permset p)
{

    struct stat sr;   /* stat() record */
    int         r;    /* result code */

    r = stat(fn, &sr);   /* get stat structure on file */
    if (r < 0) unixerr();  /* process unix error */
    sr.st_mode &= 0777;   /* mask permissions */
    if (BIT(pa_pmread) & p) sr.st_mode &= ~S_IRUSR; /* set read */
    if (BIT(pa_pmwrite) & p) sr.st_mode &= ~S_IWUSR; /* set write */
    if (BIT(pa_pmexec) & p) sr.st_mode &= ~S_IXUSR; /* set execute */
    r = chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0) unixerr();  /* process unix error */

}


/********************************************************************************

Set group permissions

Sets group permissions.

********************************************************************************/

void pa_setgper(char *fn, pa_permset p)
{

    struct stat sr;   /* stat() record */
    int r;   /* result code */

    r = stat(fn, &sr); /* get stat structure on file */
    if (r < 0) unixerr(); /* process unix error */
    sr.st_mode &= 0777;   /* mask permissions */
    if (BIT(pa_pmread) & p) sr.st_mode |= S_IRGRP;  /* set read */
    if (BIT(pa_pmwrite) & p) sr.st_mode |= S_IWGRP;  /* set write */
    if (BIT(pa_pmexec) & p) sr.st_mode |= S_IXGRP;  /* set execute */
    r = chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0) unixerr();  /* process unix error */

}


/********************************************************************************

Reset group permissions

Resets group permissions.

********************************************************************************/

void pa_resgper(char *fn, pa_permset p)
{
    struct stat sr; /* stat() record */
    int         r;  /* result code */

    r = stat(fn, &sr); /* get stat structure on file */
    if (r < 0) unixerr(); /* process unix error */
    sr.st_mode &= 0777;   /* mask permissions */
    if (BIT(pa_pmread) & p)  sr.st_mode &= ~S_IRGRP; /* set read */
    if (BIT(pa_pmwrite) & p) sr.st_mode &= ~S_IWGRP;  /* set write */
    if (BIT(pa_pmexec) & p) sr.st_mode &= ~S_IXGRP;  /* set execute */
    r = chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0) unixerr();  /* process unix error */

}


/********************************************************************************

Set other (global) permissions

Sets other permissions.

********************************************************************************/

void pa_setoper(char *fn, pa_permset p)
{

    struct stat sr; /* stat() record */
    int         r;  /* result code */

    r = stat(fn, &sr);   /* get stat structure on file */
    if (r < 0) unixerr();  /* process unix error */
    sr.st_mode &= 0777;   /* mask permissions */
    if (BIT(pa_pmread) & p) sr.st_mode |= S_IROTH;  /* set read */
    if (BIT(pa_pmwrite) & p) sr.st_mode |= S_IWOTH;  /* set write */
    if (BIT(pa_pmexec) & p) sr.st_mode |= S_IXOTH;  /* set execute */
    r = chmod(fn, sr.st_mode);   /* set mode */
    if (r < 0) unixerr();  /* process unix error */

}


/********************************************************************************

Reset other (global) permissions

Resets other permissions.

********************************************************************************/

void pa_resoper(char *fn, pa_permset p)
{

    struct stat sr; /* stat() record */
    int         r;  /* result code */

    r = stat(fn, &sr); /* get stat structure on file */
    if (r < 0) unixerr(); /* process unix error */
    sr.st_mode &= 0777; /* mask permissions */
    if (BIT(pa_pmread) & p) sr.st_mode &= ~S_IROTH; /* set read */
    if (BIT(pa_pmwrite) & p) sr.st_mode &= ~S_IWOTH; /* set write */
    if (BIT(pa_pmexec) & p) sr.st_mode &= ~S_IXOTH; /* set execute */
    r = chmod(fn, sr.st_mode); /* set mode */
    if (r < 0) unixerr(); /* process unix error */

}

/********************************************************************************

Make path

Create a new path. Only one new level at a time may be created.

********************************************************************************/

void pa_makpth(char *fn)
{

    int r;   /* result code */

    /* make directory, give all permissions allowable */
    r = mkdir(fn,
        S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP |
        S_IROTH | S_IWOTH | S_IXOTH);
    if (r < 0) unixerr(); /* process unix error */

}

/********************************************************************************

Remove path

Create a new path. Only one new level at a time may be deleted.

********************************************************************************/

void pa_rempth(char *fn)
{

    int r;   /* result code */

    r = rmdir(fn); /* remove directory */
    if (r < 0) unixerr(); /* process unix error */


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

void pa_filchr(pa_chrset fc)
{

    int i;

    /* clear set */
    for (i = 0; i < SETLEN; i++) fc[i] = 0;
    /* add everything but control characters and space */
    for (i = ' '+1; i <= 0x7f; i++) ADDSET(fc, i);
    SUBSET(fc, '-'); /* add option character */
    SUBSET(fc, pa_pthchr()); /* add path character */

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

There are algorithims that can find the approximate location of a host from
network connections. This would effectively allow the automatic setting of a
host location.

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

    "Afghanistan",        4, "Aland Islands",    248, "Albania",           8,
    "Algeria",           12, "American Samoa",    16, "Andorra",          20,
    "Angola",            24, "Anguilla",         660, "Antarctica",       10,
    "Antigua and Barbuda", 28,
    "Argentina",         32, "Armenia",           51, "Aruba",            533,
    "Australia",         36, "Austria",           40, "Azerbaijan",        31,
    "Bahamas",           44, "Bahrain",           48, "Bangladesh",        50,
    "Barbados",          52, "Belarus",          112, "Belgium",          56,
    "Belize",            84, "Benin",            204, "Bermuda",          60,
    "Bhutan",            64, "Bolivia",          68,
    "Bonaire, Sint Eustatius and Saba", 535,
    "Bosnia and Herzegovina", 70,
    "Botswana",          72, "Bouvet Island",      74, "Brazil",           76,
    "British Indian Ocean Territory",  86,
    "Brunei Darussalam", 96, "Bulgaria",          100, "Burkina Faso",     854,
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
    "Korea, North",      408, "Korea, South",     410, "Kuwait",           414,
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
    "Netherlands",       528, "New Caledonia",    540, "New Zealand",      554,
    "Nicaragua",         558, "Niger",            562, "Nigeria",          566,
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
    "Solomon Islands",    90, "Somalia",          706,
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
    "Viet Nam",          704, "Virgin Islands, British", 92,
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
    while (p->countrynum && p->countrynum != c) p++;
    if (!p->countrynum) error("Country number invalid");
    if (strlen(p->countrystr) > len) error("Country string too large for buffer");
    strncpy(s, p->countrystr, len);

}

/** ****************************************************************************

Find timezone offset

Finds the host location offset for the GMT to local time in seconds. It is
negative for zones west of the prime meridian, and positive for zones east.

*******************************************************************************/

int pa_timezone(void)

{

    time_t t, nt;  /* seconds time holder */
    struct tm gmt; /* time structure gmt */
    struct tm lcl; /* local time structure */

    t = time(NULL); /* get seconds time */
    gmtime_r(&t, &gmt); /* get gmt */
    localtime_r(&t, &lcl); /* get local */
    nt = (lcl.tm_hour-gmt.tm_hour)*HOURSEC-(!!lcl.tm_isdst*HOURSEC);
    /* adjust for GMT ahead of local */
    if (lcl.tm_mday = gmt.tm_mday) nt -= 24*HOURSEC;

    /* return hour difference */
    return nt;

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

    return !!plcl->tm_isdst; /* return dst active status */

}

/** ****************************************************************************

Find if 12 or 24 hour time is in effect

Returns true if 24 hour time is in use in the current host location.

*******************************************************************************/

int pa_time24hour(void)

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
    while (p->langnum && p->langnum != l) p++;
    if (!p->langnum) error("Language number invalid");
    if (strlen(p->langstr) > len) error("Language string too large for buffer");
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

int pa_dateorder(void)

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

Initialize services

We initialize all variables and tables.

The Linux environment string table is fixed in length, and thus cannot be
Edited. We load it to a dynamic table, and thus may edit it. It can then be
passed on to a subprogram, either in the edited form, or unmodified.

Note the environment is unordered.

*******************************************************************************/

static void pa_init_services (void) __attribute__((constructor (102)));
static void pa_init_services()

{

    int        envlen; /* number of environment strings */
    char**     ep;     /* unix environment string table */
    int        ei;     /* index for string table */
    int        si;     /* index for strings */
    pa_envrec* p;      /* environment entry pointer */
    pa_envrec* p1;
    char*      cp;
    int        l;

    /* Copy environment to local */
    envlst = NULL;   /* clear environment strings */
    ep = environ;   /* get unix environment pointers */
    while (*ep != NULL) {  /* copy environment strings */

        p = malloc(sizeof(pa_envrec)); /* get a new environment entry */
        p->next = envlst; /* push onto environment list */
        envlst = p;
        cp = strchr(*ep, '='); /* find location of '=' */
        if (!cp) error("Invalid environment string format");
        /* get the name string */
        l = cp-*ep;
        p->name = malloc(l+1);
        extract(p->name, l, *ep, 0, cp-*ep-1);
        /* get the data string */
        l = strlen(*ep)-(cp-*ep+1);
        p->data = malloc(l+1);
        extract(p->data, l, *ep, cp+1-*ep, strlen(*ep)-1);
        ep++; /* next environment string */

    }
    /* reverse the environment to original order for neatness */
    p = envlst;
    envlst = NULL;
    while (p) {

        p1 = p;
        p = p->next;
        p1->next = envlst;
        envlst = p1;

     }
    pa_getenv("PATH", pthstr, MAXSTR); /* load up the current path */
    trim(pthstr); /* make sure left aligned */

}

/** ****************************************************************************

Deinitialize services

Not used at present

*******************************************************************************/

static void pa_deinit_services (void) __attribute__((destructor (102)));
static void pa_deinit_services()

{

    int        ti; /* index for timers */
    pa_envrec* p;  /* environment entry pointer */

    while (envlst) {

        p = envlst;
        envlst = envlst->next;
        free(p);

    }

}
