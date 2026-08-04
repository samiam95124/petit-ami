/*******************************************************************************
*                                                                              *
*                                MAIL READER                                   *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
* Reads mail from an IMAP server and shows it, in the manner of the web mail   *
* readers: the folders down the left, the messages of the chosen folder on     *
* the right, one line each, with the sender, the subject, the start of the     *
* message and when it arrived.                                                 *
*                                                                              *
* Nothing on the server is changed. The folder is opened with EXAMINE, which   *
* the protocol defines as read only, and the message is asked for with         *
* BODY.PEEK[], which is the form that does not mark it read. Either alone      *
* would do; both are used because a mail reader that quietly marks a thousand  *
* messages read is a mail reader nobody will trust twice.                      *
*                                                                              *
* What comes off the server is written to a file as it arrived, headers and    *
* all, in mbox format: the message is not taken apart to be stored, only to    *
* be shown. Everything the reader displays it finds by searching that file     *
* again, so the file is the truth and the display is a view of it. A file      *
* that this program wrote can be read by any other mail program, and one       *
* written by another mail program can be read by this.                         *
*                                                                              *
* The mail is kept in a directory of its own, ~/.amimail by default, one       *
* .mbox file per folder and a small state file beside it recording how far     *
* the folder was read, so that fetching again fetches only what is new.        *
*                                                                              *
* Usage:                                                                       *
*                                                                              *
*     mail [--fetch|-f] [--diag|-d] [--limit=<n>]                              *
*                                                                              *
* --fetch fetches on startup rather than waiting for Mail/Fetch. --diag        *
* reports the conversation with the server to stderr, with the password        *
* held back.                                                                   *
*                                                                              *
* The account is given in the Petit-Ami configuration, under a branch of its   *
* own, which puts it in petit_ami.cfg beside the program, in the user's path,  *
* or in the current directory, as config defines:                              *
*                                                                              *
*     begin mail                                                               *
*         imap     imap.gmail.com                                              *
*         imapport 993                                                         *
*         smtp     smtp.gmail.com                                              *
*         smtpport 465                                                         *
*         user     someone@gmail.com                                           *
*         pass     abcdefghijklmnop                                            *
*         limit    200                                                         *
*         store    /home/someone/.amimail                                      *
*     end                                                                      *
*                                                                              *
* For Gmail the password is not the account password but an application        *
* password, which Google issues per program to an account that has two step    *
* verification turned on. Since that file then holds a password, this warns    *
* if it is one that anybody but its owner can read.                            *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <limits.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>

#include <openssl/evp.h>

#include <localdefs.h>
#include <graphics.h>
#include <network.h>
#include <services.h>
#include <option.h>

#define MAXSTR    500  /* the usual string */
#define MAXLINE   4000 /* longest protocol or message line taken whole */
#define MAXFOLDER 200  /* folders on the server */
#define MAXMSG    20000 /* messages indexed in one folder */
#define SNIPPET   400  /* characters of the message kept for the list */
#define DEFLIMIT  200  /* messages fetched from a folder, most recent first */

/* the windows: the panes are children of the main window, the reader is a
   window of its own */
#define MAINWIN   1 /* the main window, which is stdout */
#define FOLDWIN   2 /* the folder pane */
#define LISTWIN   3 /* the message list pane */
#define READWIN   4 /* a message, opened to be read */
#define SRVWIN    5 /* the server form */
#define POPWIN    6 /* the message menu, on the second mouse button */
#define HELPWIN   7 /* the help window */
#define BANWIN    8 /* the banner across the top */
#define CMPWIN    9 /* the compose window */
#define BANPIC    1 /* the picture in it */

/* its widgets, numbered within it */
#define HELPFIND  1 /* the search entry */
#define HELPLIST  2 /* the topic list */
#define HELPCLOSE 3 /* the close button */

/* The help text is a file, not something built into the program, so
   that it can be as long as it deserves and can be corrected without a
   compiler. It is markdown of the plain kind; see the file itself. */
#define HELPFILE  "mail.md"

/* the scroll bars, which are widgets of the pane they are in */
#define SBLIST    1 /* the message list */
#define SBREAD    1 /* the reader */

/* the widgets of the server form, numbered within its own window */
#define SRVIMAP   1 /* the imap server */
#define SRVIPORT  2 /* and its port */
#define SRVSMTP   3 /* the sending server */
#define SRVSPORT  4 /* and its port */
#define SRVUSER   5 /* the user name */
#define SRVPASS   6 /* the password */
#define SRVLIMIT  7 /* messages fetched per folder */
#define SRVNAME   8 /* what this account is called */
#define SRVPOLL   9 /* seconds between looks at the servers */
#define SRVOK    10 /* save and close */
#define SRVCAN   11 /* close and keep what was there */
#define SRVNEXT  12 /* show the next account */
#define SRVNEW   13 /* start another one */
#define SRVDEL   14 /* take this one away */
#define SRVSEND  15 /* mail is sent from this account */

/* the compose window's widgets */
#define CMPTO     1 /* who it is to */
#define CMPCC     2 /* and who else */
#define CMPSUB    3 /* what it is about */
#define CMPSEND   4 /* send it */
#define CMPCAN    5 /* and think better of it */
#define CMPSB     6 /* the bar down the side of the body */

/* menu ids of our own, after the standard ones */
#define MENUMAIL  (AMI_SMMAX+1) /* the mail menu itself */
#define MENUFETCH (AMI_SMMAX+2) /* fetch new mail */
#define MENUCOMP  (AMI_SMMAX+6) /* write a message */
#define MENUREPLY (AMI_SMMAX+7) /* answer the message being read */
#define MENUREPALL (AMI_SMMAX+8) /* and everybody it went to */
#define MENUFWD   (AMI_SMMAX+9) /* pass it on */
#define MENUFOLD  (AMI_SMMAX+3) /* fetch the folder list again */
#define MENUCHECK (AMI_SMMAX+4) /* check that mail could be sent */
#define MENUSRV   (AMI_SMMAX+5) /* the server form */

/* What a notch of the wheel moves. One message, because a message is a
   thing and a notch is a step, and the list is read by stepping through
   it. Text is not read that way, so the reader keeps three: a message of
   any length would be tedious at one line a notch. */
#define DIGLEN    65 /* a sha-256 as hex, and its terminator */
#define MAXSRV    8  /* servers this program will gather from */
#define DEFPOLL   15 /* seconds between looks at the servers */
#define NETWAIT   45 /* seconds to wait on a server before giving up */

#define WHEELMSGS 1 /* messages the wheel moves per notch, in the list */
#define WHEELROWS 3 /* lines it moves per notch, in the reader */

#define TIMFETCH  1 /* the timer that steps a fetch along */
#define TIMPOLL   2 /* and the one that starts one now and then */

#define OFF 0
#define ON  1

/* a folder on the server, and the file it is kept in */

/* A message, as found in the mbox file. The file is the message; this is
   only what the list needs to draw a line without reading it again. */
typedef struct {

    long off;             /* where the message starts in the file */
    long len;             /* how long it is */
    char from[MAXSTR];    /* who it is from, shown */
    char subject[MAXSTR]; /* the subject line */
    char addr[100];       /* the sender's address, for matching */
    char dig[DIGLEN];     /* what the message is, wherever it came from */
    char cat[32];         /* what kind of mail it is */
    char snip[SNIPPET];   /* the start of the message */
    char when[40];        /* the date, shown the way mail readers show it */
    long date;            /* the date, for sorting */

} msgrec;

typedef struct {

    char name[MAXSTR];  /* the name the server knows it by */
    char show[MAXSTR];  /* the name shown, without the [Gmail]/ part */
    char file[MAXSTR*2]; /* the mbox file it is kept in */
    long msgs;          /* messages in the file */
    long dirty;         /* something has been put in it since it was read */
    msgrec* idx;        /* every message in it, read from the index file */
    long idxct;         /* how many */
    long idxmax;        /* and how many the array has room for */
    long idxok;         /* the index has been read and is good */
    long wantidx;       /* it wants reading, when the worker gets to it */
    long noselect;      /* the server says it holds no messages */
    long local;         /* ours alone: a folder with no server side */
    long srv;           /* which server it belongs to, -1 if local */

} foldrec;

static foldrec folders[MAXFOLDER];
static long    foldct;
static long    foldsel = -1;    /* the folder being shown */

static msgrec* msgs;            /* the messages of that folder */
static long    msgct;

/* The list the display draws is the selected folder's index -- not a
   copy of it, and not one built for the purpose. This points the two at
   each other, and is called wherever either can move: the array is
   grown as mail arrives, and growing it can move it. Every caller holds
   the lock, which is what makes that safe. */
static void useidx(void)

{

    if (foldsel >= 0 && foldsel < foldct) {

        msgs = folders[foldsel].idx;
        msgct = folders[foldsel].idxct;

    } else { msgs = NULL; msgct = 0; }

}
static long    msgsel = -1;     /* the message being read */
static long    msgtop;          /* the first message shown */
static long    listshown;       /* the first one actually on the screen */

/* the account */
/* An account on a server. There may be several: mail is gathered from
   all of them, and where it goes locally is not their business -- the
   local folders are one set, shared, since a folder called Bills is
   about bills and not about which company carried the letter. */
typedef struct {

    char name[64];     /* what its owner calls it: google, work, old */
    char imap[MAXSTR]; /* where the mail is read from */
    long imapport;
    char smtp[MAXSTR]; /* and where it would be sent from */
    long smtpport;
    char user[MAXSTR];
    char pass[MAXSTR];
    long limit;        /* messages taken from each of its folders */

} srvrec;

static srvrec servers[MAXSRV];
static long   srvct;    /* how many there are */
static long   srvedit;  /* the one the form is showing */
static long   pollsec = DEFPOLL; /* how often to look, in seconds */
/* Which account mail is sent from. One of them, and only one: a message
   goes out over one connection with one name on it, so the box that
   says which is a choice among the accounts, not a setting each of them
   carries. */
static long   sendsrv;

/* A message waiting to go. The sending is network, and network is the
   worker's, so what is written is left here and taken by it. */
static long   sendwant;
static char   outto[MAXSTR];
static char   outcc[MAXSTR];
static char   outsub[MAXSTR];
static char*  outbody;
static char   outinreply[MAXSTR];
static char   outrefs[MAXSTR*2];

/* the one being talked to just now, which the protocol routines use */
static char imapsrv[MAXSTR] = "imap.gmail.com";
static long imapport = 993;
static char smtpsrv[MAXSTR] = "smtp.gmail.com";
static long smtpport = 465;
static char username[MAXSTR];
static char password[MAXSTR];
static char store[MAXSTR];      /* the directory the mail is kept in */
static long limit = DEFLIMIT;   /* messages fetched from a folder */

static long diag;               /* report the conversation to stderr */
static char* mailprog;          /* argv[0], to find the rules by */

/* the windows and their measurements */
static FILE* foldwf;            /* the folder pane */
static FILE* listwf;            /* the message list pane */
static FILE* readwf;            /* the reader, NULL when closed */
static FILE* srvwf;             /* the server form, NULL when closed */
static long  chrh;              /* the height of a line, in pixels */
static long  rowh;              /* the height of a message line */
static long  foldw;             /* the width of the folder pane */
static long  sbw;               /* scroll bar thickness */
static long  listrows;          /* message lines the list holds */
static long  foldy[MAXFOLDER];  /* where each folder was drawn, for clicks */
static long  listx, listy;      /* where the list pane sits on the window */
static long  fromx;             /* where the sender column ends */
static long  catx;              /* and where the category column ends */
static long  datex;             /* where the date column begins */
static long  mpx, mpy;          /* the mouse, in pixels of its own window */

/* the reader */
static char* readtext;          /* the message being read, decoded */
static long  readtop;           /* the first line of it shown */
static long  readshown;         /* the first line actually on the screen */
static char** readline;         /* it, wrapped to the window */
static long  readlines;
static long  readmax;

/* the connection to the server */
static FILE* imap;
static long  imaptag;

static void drawlist(void);     /* forward */
static void drawfolders(void);
static void showfolder(long i);
static void indexfolder(long fold);
static void status(const char* s);
static void srvdir(long srv, char* dn, long dnl);
static void safename(const char* nm, char* fn, long fnl);
static void drawread(void);
static void layout(void);
static void drawfolders(void);
static void drawlist(void);

/*******************************************************************************

Odds and ends

*******************************************************************************/

/* A colour named the way everybody names one, in parts of 255.

   The library takes each part as a fraction of the whole range of a
   long, not as a byte: 245 out of LONG_MAX is not a light grey, it is
   black, and every "quiet grey" in this program was black until this
   was noticed. Written once, here, so it cannot be got wrong twice. */
static long rgb(long c)

{

    if (c < 0) c = 0;
    if (c > 255) c = 255;

    return (c*(LONG_MAX/255));

}

/* say something went wrong, in a way the user can see */
/* A fetch the timer started is a fetch nobody asked for, and a server
   that is down will fail every one of them. Putting a box on the screen
   for each is a program that cannot be left alone: come back after an
   hour away and there is an hour of boxes to dismiss. Failures nobody
   asked for go to the status line, where they can be read and ignored. */
static long quietfail;
static long wrkgo;      /* a fetch is running on the other thread */

static void copystr(char* d, const char* s, long dl);

/* Something the worker ran into. It cannot put a box on the screen --
   nothing on that thread may call the graphics at all -- so it leaves
   the words here and the main thread shows them on its next tick. Only
   one is kept: a fetch that has gone wrong goes wrong the same way over
   and over, and the reader wants to be told once. */
static char failsaid[MAXSTR*3];
static char sentsaid[MAXSTR]; /* and what went right */
static long sendfail;         /* and whether it was a send that failed */
static long failwait;
static long fetchasked; /* somebody asked for this fetch and is waiting */

static void fail(const char* what)

{

    if (wrkgo || quietfail) { /* a fetch is running, or nobody asked */

        copystr(failsaid, what, sizeof(failsaid));
        failwait = TRUE;

        return;

    }
    ami_alert("Mail", (char*)what);

}

static void* getmem(long n)

{

    void* p = malloc(n);

    if (!p) { fail("Out of memory"); exit(1); }

    return (p);

}

/* trim the blanks and the line ending off both ends of a string */
static void trim(char* s)

{

    char* p = s;
    long  n;

    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p)+1);
    n = strlen(s);
    while (n && (s[n-1] == '\r' || s[n-1] == '\n' ||
                 s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;

}

/* copy with a limit, always terminated */
static void copystr(char* d, const char* s, long n)

{

    strncpy(d, s, n-1);
    d[n-1] = 0;

}

/*******************************************************************************

Digests

Every message gets a SHA-256 of the bytes it is stored as. It is what
lets this program know a message it already has, whoever sends it and
whatever folder it arrives in: mail moved between folders keeps its
digest, mail fetched twice has the same digest, and mail from a second
server that is the same mail has the same digest. UIDs cannot do any of
that -- they belong to one folder on one server and mean nothing
anywhere else.

The digest covers the message and not the separator line above it, since
that line is this program's own and carries a time that is not the
message's.

The digests of a folder are kept in a file beside its mailbox, one to a
line, with the size the mailbox had when they were taken on the first
line. A mailbox that has grown since is read again and the file rebuilt,
so the two cannot drift apart without being noticed.

*******************************************************************************/

static void digestbytes(const char* data, long len, char* hex); /* forward */

/* The SHA-256 of a message, taken over one canonical form of it so that
   the same message gives the same digest wherever it is seen. What
   arrives from the server and what is written to the mailbox are not the
   same bytes: storing escapes any line that begins "From " with a >, so
   that it cannot be mistaken for a separator, and drops the newlines
   that trail the message. Undo both before taking the digest, and the
   message as received and the message as stored agree -- which is the
   whole point of having one. */
static void digestof(const char* data, long len, char* hex)

{

    char* norm = malloc(len+1);
    long  i, o = 0;
    int   atbol = TRUE;

    if (!norm) { fail("Out of memory"); exit(1); }
    for (i = 0; i < len; i++) {

        if (atbol && data[i] == '>') { /* a line the storing escaped? */

            long j = i;

            while (j < len && data[j] == '>') j++;
            if (j+5 <= len && !strncmp(data+j, "From ", 5)) i++; /* one off */

        }
        norm[o++] = data[i];
        atbol = data[i] == '\n';

    }
    while (o && (norm[o-1] == '\n' || norm[o-1] == '\r')) o--;
    digestbytes(norm, o, hex);
    free(norm);

}

/* the SHA-256 of a block of bytes, as hex */
static void digestbytes(const char* data, long len, char* hex)

{

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int  n = 0;
    EVP_MD_CTX*   c = EVP_MD_CTX_new();
    long          i;

    if (!c) { fail("Out of memory"); exit(1); }
    EVP_DigestInit_ex(c, EVP_sha256(), NULL);
    EVP_DigestUpdate(c, data, len);
    EVP_DigestFinal_ex(c, md, &n);
    EVP_MD_CTX_free(c);
    for (i = 0; i < (long)n; i++) sprintf(hex+i*2, "%02x", md[i]);
    hex[n*2] = 0;

}

/* The digests this store holds, in a table of their own. Kept as a plain
   open hash: the whole point is that asking whether a message is already
   here costs nothing, so a search of every folder will not do. */
#define DIGBKT 4096

typedef struct digent {

    long           fold; /* the folder holding the message */
    long           rec;  /* and which of its messages it is */
    struct digent* next;

} digent;

static digent* digtab[DIGBKT];
static long    digct;

static long dighash(const char* d)

{

    long h = 0;
    long i;

    /* the digest is already spread evenly, so any few of its characters
       make as good a bucket as all of them */
    for (i = 0; i < 8; i++) h = h*31+d[i];

    return ((h & 0x7fffffff) % DIGBKT);

}

/* What a digest names: a message, in a folder, at a place in its file.
   The table holds no digests of its own -- it holds where to find them,
   so that knowing a message is here also says where here is. That is
   what makes it worth anything to an audit: not "something with this
   digest was seen once" but "this message is in that file at that
   offset". */
static const char* digof(const digent* p)

{

    return (folders[p->fold].idx[p->rec].dig);

}

/* the message this digest names, or none */
static int finddigest(const char* d, long* fold, long* rec)

{

    digent* p;

    for (p = digtab[dighash(d)]; p; p = p->next)
        if (!strcmp(digof(p), d)) {

            if (fold) *fold = p->fold;
            if (rec) *rec = p->rec;

            return (TRUE);

        }

    return (FALSE);

}

static int hasdigest(const char* d)

{

    return (finddigest(d, NULL, NULL));

}

static void adddigest(long fold, long rec)

{

    digent* p;
    long    b;
    const char* d = folders[fold].idx[rec].dig;

    if (hasdigest(d)) return;
    b = dighash(d);
    p = getmem(sizeof(digent));
    p->fold = fold;
    p->rec = rec;
    p->next = digtab[b];
    digtab[b] = p;
    digct++;

}

/* Take the table again over every folder's index. Sorting a folder moves
   its messages about and the table names them by where they sit, so it
   is taken again whenever an index is rebuilt rather than added to. */
static void rehashall(void)

{

    long i, j;

    for (i = 0; i < DIGBKT; i++) {

        digent* p = digtab[i];

        while (p) { digent* n = p->next; free(p); p = n; }
        digtab[i] = NULL;

    }
    digct = 0;
    for (i = 0; i < foldct; i++)
        for (j = 0; j < folders[i].idxct; j++) adddigest(i, j);

}

/* the file a folder's digests are kept in */
/*******************************************************************************

The index file

One beside each mailbox, holding a line for every message in it: where
it is in the file and how long it is, what it is (its digest), who it is
from, what it is about, when it came and the start of what it says --
everything the list shows and everything the store is searched by.

It is written as it is filled, a line at a time, and read back at
startup. Reading it costs a few megabytes; working it out again from the
mailbox costs reading and parsing every byte of mail there is, which on
this store is five gigabytes and half a minute of somebody's attention.

The file needs no stamp saying how far it goes. The messages it names
say that themselves: the furthest of them ends where the indexed part of
the mailbox ends, and anything after that in the mailbox is new. A
mailbox that is shorter than that, or that does not have a separator
where the index says a message begins, has been rewritten under the
index, and the index is thrown away and taken again.

Fields are separated by tabs. Anything that could hold a tab or a line
end -- a subject, a snippet -- is written with those escaped, since a
record is a line and has to stay one.

*******************************************************************************/

/* The line every index file begins with. The number is the version of
   what follows, and moving it throws away every index written before:
   version 1 held dates read without the sender's timezone, so mail from
   a server keeping UTC sorted hours into the future. An index that is
   wrong is worse than none, since nothing would ever go back and look
   at the mailbox again. */
#define IDXHEAD "ami-mail-index 2"

static void idxfile(long fold, char* fn, long fnl)

{

    snprintf(fn, fnl, "%s.idx", folders[fold].file);

}

static void digfile(long fold, char* fn, long fnl)

{

    snprintf(fn, fnl, "%s.dig", folders[fold].file);

}

/* a field, with what would break the line taken out of it */
static void idxput(FILE* f, const char* s)

{

    fputc('\t', f);
    while (*s) {

        switch (*s) {

            case '\\': fputs("\\\\", f); break;
            case '\t': fputs("\\t", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': break;
            default:   fputc(*s, f); break;

        }
        s++;

    }

}

/* and back again: the next field of the line, up to the tab */
static char* idxget(char* p, char* d, long dl)

{

    long i = 0;

    if (!p) { if (dl) *d = 0; return (NULL); }
    if (*p == '\t') p++;
    while (*p && *p != '\t') {

        char c = *p++;

        if (c == '\\' && *p) {

            c = *p++;
            if (c == 't') c = '\t';
            else if (c == 'n') c = '\n';

        }
        if (i < dl-1) d[i++] = c;

    }
    if (dl) d[i] = 0;

    return (p);

}

static void idxwrite(FILE* f, const msgrec* m)

{

    fprintf(f, "%ld\t%ld\t%ld", m->off, m->len, m->date);
    idxput(f, m->dig);
    idxput(f, m->cat);
    idxput(f, m->when);
    idxput(f, m->addr);
    idxput(f, m->from);
    idxput(f, m->subject);
    idxput(f, m->snip);
    fputc('\n', f);

}

static int idxread(char* line, msgrec* m)

{

    char* p = line;
    char  num[40];

    p = idxget(p, num, sizeof(num)); m->off = atol(num);
    p = idxget(p, num, sizeof(num)); m->len = atol(num);
    p = idxget(p, num, sizeof(num)); m->date = atol(num);
    p = idxget(p, m->dig, sizeof(m->dig));
    p = idxget(p, m->cat, sizeof(m->cat));
    p = idxget(p, m->when, sizeof(m->when));
    p = idxget(p, m->addr, sizeof(m->addr));
    p = idxget(p, m->from, sizeof(m->from));
    p = idxget(p, m->subject, sizeof(m->subject));
    p = idxget(p, m->snip, sizeof(m->snip));

    return (p && m->len > 0 && strlen(m->dig) == DIGLEN-1);

}

/* Room for one more in the index being built.

   A folder is read into here and put in place in one move, and never
   added to where it stands. It was added to where it stood, and that is
   what crashed the program: growing an array moves it, the display's
   list IS the array, and a display drawing the folder that was being
   read went on drawing the block that had just been freed underneath
   it. Reading a folder while looking at it is the ordinary case, not a
   corner of one.

   The fetch is different and adds in place, but the fetch does it
   holding the lock and points the display at the array again
   afterwards, which is the same protection by another road. */
static msgrec* bidx;
static long    bct;
static long    bmax;

static msgrec* bldroom(void)

{

    if (bct >= bmax) {

        bmax = bmax? bmax*2: 256;
        bidx = realloc(bidx, bmax*sizeof(msgrec));
        if (!bidx) { fail("Out of memory"); exit(1); }

    }

    return (&bidx[bct++]);

}

/* room for one more in a folder's index */
static msgrec* idxroom(long fold)

{

    foldrec* fo = &folders[fold];

    if (fo->idxct >= fo->idxmax) {

        fo->idxmax = fo->idxmax? fo->idxmax*2: 256;
        fo->idx = realloc(fo->idx, fo->idxmax*sizeof(msgrec));
        if (!fo->idx) { fail("Out of memory"); exit(1); }

    }

    return (&fo->idx[fo->idxct++]);

}

/* Where the mailbox has been indexed up to: the end of the message that
   reaches furthest into it. The file is written in the order messages
   arrive but sorted in memory, so it is the highest end and not the
   last line. */
static long idxend(void)

{

    long i;
    long e = 0;

    for (i = 0; i < bct; i++) {

        long x = bidx[i].off+bidx[i].len;

        if (x > e) e = x;

    }

    return (e);

}

/*******************************************************************************

Reading a folder

An index file is read if there is one and it still fits the mailbox, and
whatever the mailbox holds beyond what the index names is read from the
mailbox itself and added. So a folder that has not changed costs reading
a few hundred kilobytes; one that has had a hundred messages put in it
costs those hundred; and only a folder with no index at all, or one
whose mailbox has been rewritten under it, costs reading the whole
thing.

*******************************************************************************/

/* Does the index still describe this mailbox? The messages it names say
   how far it goes; the mailbox must be at least that long, and must
   have a separator line where the index says the last message begins.
   Anything else means the mailbox has been rewritten -- by a move, by a
   hand, by another program -- and the index is not to be trusted. */
static int idxfits(long fold)

{

    struct stat sb;
    long        e;
    long        off = -1;
    long        i;
    FILE*       f;
    char        buf[300];
    long        back, n;
    char*       ln;

    if (stat(folders[fold].file, &sb)) return (FALSE); /* no mailbox */
    e = idxend();
    if (!e) return (bct == 0); /* nothing named, nothing to fit */
    if (e > (long)sb.st_size) return (FALSE);  /* the mailbox lost bytes */
    for (i = 0; i < bct; i++)
        if (bidx[i].off+bidx[i].len == e) { off = bidx[i].off; break; }
    if (off <= 0) return (FALSE);
    f = fopen(folders[fold].file, "r");
    if (!f) return (FALSE);
    back = off > (long)sizeof(buf)-1? (long)sizeof(buf)-1: off;
    fseek(f, off-back, SEEK_SET);
    n = fread(buf, 1, back, f);
    fclose(f);
    if (n <= 0) return (FALSE);
    buf[n] = 0;
    /* the separator is the line that ends where the message begins */
    ln = buf+n;
    if (ln > buf && ln[-1] == '\n') ln--;
    while (ln > buf && ln[-1] != '\n') ln--;

    return (!strncmp(ln, "From ", 5));

}

/* read the index file, if there is one */
static void idxload(long fold)

{

    char  fn[MAXSTR*2+8];
    FILE* f;
    char* line;
    long  lsz = MAXSTR*4+SNIPPET*2+400;

    bct = 0;
    folders[fold].idxok = FALSE;
    idxfile(fold, fn, sizeof(fn));
    f = fopen(fn, "r");
    if (!f) return;
    line = getmem(lsz);
    if (!fgets(line, lsz, f) || strncmp(line, IDXHEAD, strlen(IDXHEAD)))
        { free(line); fclose(f); return; } /* not one of ours */
    while (fgets(line, lsz, f)) {

        msgrec m;

        trim(line);
        if (!*line) continue;
        if (!idxread(line, &m)) continue; /* a line that says nothing */
        *bldroom() = m;

    }
    free(line);
    fclose(f);
    folders[fold].idxok = idxfits(fold);
    if (!folders[fold].idxok) bct = 0; /* it will be taken again */

}

/* write the whole of it */
static void idxsave(long fold)

{

    char  fn[MAXSTR*2+8];
    FILE* f;
    long  i;

    idxfile(fold, fn, sizeof(fn));
    f = fopen(fn, "w");
    if (!f) return; /* an index that cannot be kept is worked out again */
    fprintf(f, "%s\n", IDXHEAD);
    for (i = 0; i < bct; i++) idxwrite(f, &bidx[i]);
    fclose(f);
    /* the digests file it replaces, which held one of these ten fields */
    digfile(fold, fn, sizeof(fn));
    remove(fn);

}

/* Throw an index away, file and all. The mailbox under it has been
   rewritten -- by a move, or by a hand -- so every offset in it is
   wrong. What is in it cannot be patched: a message moved is the same
   message, but it is not in the same place. */
static void idxdrop(long fold)

{

    char fn[MAXSTR*2+8];

    idxfile(fold, fn, sizeof(fn));
    remove(fn);
    folders[fold].idxct = 0;
    folders[fold].idxok = FALSE;
    folders[fold].wantidx = TRUE;

}

/* and add to it as messages arrive */
static void idxappend(long fold, const msgrec* m)

{

    char  fn[MAXSTR*2+8];
    FILE* f;
    int   made;

    idxfile(fold, fn, sizeof(fn));
    made = access(fn, F_OK) != 0;
    f = fopen(fn, "a");
    if (!f) return;
    if (made) fprintf(f, "%s\n", IDXHEAD);
    idxwrite(f, m);
    fclose(f);

}

/* Move a flat store into directories. Everything used to live in one
   place, named for its account or with local_ in front of it; each
   account has a directory of its own now, and what is ours has one
   called local. Done once, on startup, so that nothing has to be
   fetched again. */
static void migratestore(void)

{

    ami_filptr lp = NULL;
    ami_filptr fp;
    char       what[MAXSTR*2];

    snprintf(what, sizeof(what), "%.400s/*.mbox", store);
    ami_list(what, &lp);
    for (fp = lp; fp; fp = fp->next) {

        char nm[MAXSTR];
        char dn[MAXSTR];
        char old[MAXSTR*2], new[MAXSTR*2];
        long n, i;
        long srv = -2;
        const char* leaf = NULL;
        static const char* ext[] = { "", ".state", ".dig", ".idx" };
        long e;

        copystr(nm, fp->name, sizeof(nm));
        n = strlen(nm);
        if (n > 5 && !strcmp(nm+n-5, ".mbox")) nm[n-5] = 0; else continue;
        if (!strncmp(nm, "local_", 6)) { srv = -1; leaf = nm+6; }
        else for (i = 0; i < srvct; i++) { /* named for an account? */

            long k = strlen(servers[i].name);

            if (!strncmp(nm, servers[i].name, k) && nm[k] == '_')
                { srv = i; leaf = nm+k+1; break; }

        }
        /* and a store older still, from before accounts had names at
           all: it belongs to the first of them */
        if (srv == -2 && srvct) { srv = 0; leaf = nm; }
        if (srv == -2) continue;
        srvdir(srv, dn, sizeof(dn));
        { /* the shown name, if the state file says what it really is */

            char  sn[MAXSTR*2+8];
            char  real[MAXSTR];
            char  lf[MAXSTR];
            long  v, u, u2;
            FILE* sf;

            copystr(lf, leaf, sizeof(lf));
            /* [Gmail]/All Mail was written _Gmail__All_Mail when the
               awkward characters were taken out of it. Inside the
               account's own directory that front half says nothing, and
               a store whose state files have been lost has nothing else
               to go on. */
            if (!strncmp(lf, "_Gmail__", 8))
                memmove(lf, lf+8, strlen(lf+8)+1);
            snprintf(sn, sizeof(sn), "%.400s/%.150s.mbox.state", store, nm);
            sf = fopen(sn, "r");
            if (sf) {

                if (fscanf(sf, "%ld %ld %ld %499[^\n]", &v, &u, &u2,
                           real) == 4 ||
                    (rewind(sf), fscanf(sf, "%ld %ld %499[^\n]",
                                        &v, &u, real) == 3)) {

                    trim(real);
                    if (!strncmp(real, "[Gmail]/", 8))
                        safename(real+8, lf, sizeof(lf));
                    else safename(real, lf, sizeof(lf));

                }
                fclose(sf);

            }
            for (e = 0; e < 4; e++) {

                snprintf(old, sizeof(old), "%.400s/%.150s.mbox%.8s",
                         store, nm, ext[e]);
                snprintf(new, sizeof(new), "%.400s/%.150s.mbox%.8s",
                         dn, lf, ext[e]);
                rename(old, new); /* what is not there does not move */

            }

        }

    }

}

/* Renaming an account renames its directory, so that everything it
   holds goes with it rather than being fetched again. */
static void renamestore(const char* was, const char* now)

{

    char        old[MAXSTR*2], new[MAXSTR*2];
    struct stat sb;

    if (!*was || !strcmp(was, now)) return;
    snprintf(old, sizeof(old), "%.400s/%.60s", store, was);
    snprintf(new, sizeof(new), "%.400s/%.60s", store, now);
    if (stat(old, &sb)) return; /* nothing there to move */
    if (!rename(old, new)) return; /* the whole directory went */
    /* It would not: renaming a directory onto one that already exists
       fails, and the new name's directory may well have been made
       already by anything that asked where the account's mail goes. So
       move what is in it, one file at a time, and take the empty
       directory away after. Failing quietly here is what left an
       account renamed with its mail still under the old name and every
       folder reading (not fetched). */
    {

        ami_filptr lp = NULL;
        ami_filptr fp;
        char       what[MAXSTR*2];

        snprintf(what, sizeof(what), "%.400s/*", old);
        ami_list(what, &lp);
        for (fp = lp; fp; fp = fp->next) {

            char a[MAXSTR*2], b[MAXSTR*2];

            snprintf(a, sizeof(a), "%.400s/%.150s", old, fp->name);
            snprintf(b, sizeof(b), "%.400s/%.150s", new, fp->name);
            rename(a, b);

        }
        rmdir(old); /* which does nothing unless it is empty */

    }

}

/* every folder in the store, so that a fetch knows what is already here */

/*******************************************************************************

The account

The account is the program's own business and is kept in the program's
own file, in the mail store, written only by the form under Mail/Server.
It is not on the command line, since a password on a command line is a
password in everybody's process list, and it is not in the Petit-Ami
configuration, which is a general facility shared by everything and no
place for one program's password.

The file is written readable by its owner and nobody else, and it is one
setting to a line, so it can be read and corrected with any editor.

*******************************************************************************/

/* where the account is kept */
static void acctfile(char* fn, long fnl)

{

    snprintf(fn, fnl, "%s/account", store);

}

/* Make the one being talked to be this one. The protocol routines work
   from these, so setting them is how a server is chosen. */
static void useserver(long i)

{

    if (i < 0 || i >= srvct) return;
    copystr(imapsrv, servers[i].imap, MAXSTR);
    imapport = servers[i].imapport;
    copystr(smtpsrv, servers[i].smtp, MAXSTR);
    smtpport = servers[i].smtpport;
    copystr(username, servers[i].user, MAXSTR);
    copystr(password, servers[i].pass, MAXSTR);
    limit = servers[i].limit;

}

/* an account with nothing in it, ready to be filled in */
static void blankserver(srvrec* r)

{

    memset(r, 0, sizeof(srvrec));
    copystr(r->name, "new", sizeof(r->name));
    copystr(r->imap, "imap.gmail.com", MAXSTR);
    r->imapport = 993;
    copystr(r->smtp, "smtp.gmail.com", MAXSTR);
    r->smtpport = 465;
    r->limit = DEFLIMIT;

}

/* Read the accounts. Each is a block: "server <name>" to "end", and
   anything outside a block is a setting of the program's own. The old
   one account form, which had no blocks at all, is still read: its
   settings arrive before any server line and make up the first one. */
static void readaccount(void)

{

    char    fn[MAXSTR*2];
    FILE*   f;
    char    line[MAXSTR*2];
    srvrec* r = NULL;
    int     old = FALSE;

    acctfile(fn, sizeof(fn));
    f = fopen(fn, "r");
    if (!f) return; /* there is none yet, which is not an error */
    while (fgets(line, sizeof(line), f)) {

        char* p = line;
        char* v;

        trim(p);
        if (!*p || *p == '#') continue;
        v = p;
        while (*v && *v != ' ' && *v != '\t') v++;
        if (*v) *v++ = 0;
        while (*v == ' ' || *v == '\t') v++;
        if (!strcasecmp(p, "server")) { /* a new account begins */

            if (srvct >= MAXSRV) break;
            r = &servers[srvct++];
            blankserver(r);
            copystr(r->name, v, sizeof(r->name));
            old = FALSE;

            continue;

        }
        if (!strcasecmp(p, "end")) { r = NULL; continue; }
        if (!strcasecmp(p, "poll")) { pollsec = atol(v); continue; }
        if (!strcasecmp(p, "sendfrom")) { sendsrv = atol(v); continue; }
        if (!r) { /* the old form: settings before any server line */

            if (srvct >= MAXSRV) break;
            r = &servers[srvct++];
            blankserver(r);
            copystr(r->name, "mail", sizeof(r->name));
            old = TRUE;

        }
        if (!strcasecmp(p, "imap")) copystr(r->imap, v, MAXSTR);
        else if (!strcasecmp(p, "imapport")) r->imapport = atol(v);
        else if (!strcasecmp(p, "smtp")) copystr(r->smtp, v, MAXSTR);
        else if (!strcasecmp(p, "smtpport")) r->smtpport = atol(v);
        else if (!strcasecmp(p, "user")) copystr(r->user, v, MAXSTR);
        else if (!strcasecmp(p, "pass")) copystr(r->pass, v, MAXSTR);
        else if (!strcasecmp(p, "limit")) r->limit = atol(v);
        (void)old;

    }
    fclose(f);
    if (srvct) useserver(0);

}

static void writeaccount(void)

{

    char   fn[MAXSTR*2];
    FILE*  f;
    mode_t um;
    long   i;

    acctfile(fn, sizeof(fn));
    /* The mask is set around the open so that the file is never readable
       by anyone else, not even for the moment between being made and
       being given its permissions. */
    um = umask(S_IRWXG | S_IRWXO);
    f = fopen(fn, "w");
    umask(um);
    if (!f) {

        char msg[MAXSTR*4];

        snprintf(msg, sizeof(msg), "%s could not be written:\n%s",
                 fn, strerror(errno));
        fail(msg);

        return;

    }
    fprintf(f, "# Mail accounts. Written by the Config form in mail.\n");
    fprintf(f, "poll %ld\n", pollsec);
    fprintf(f, "sendfrom %ld\n", sendsrv);
    for (i = 0; i < srvct; i++) {

        fprintf(f, "\nserver %s\n", servers[i].name);
        fprintf(f, "imap %s\n", servers[i].imap);
        fprintf(f, "imapport %ld\n", servers[i].imapport);
        fprintf(f, "smtp %s\n", servers[i].smtp);
        fprintf(f, "smtpport %ld\n", servers[i].smtpport);
        fprintf(f, "user %s\n", servers[i].user);
        fprintf(f, "pass %s\n", servers[i].pass);
        fprintf(f, "limit %ld\n", servers[i].limit);
        fprintf(f, "end\n");

    }
    fclose(f);
    chmod(fn, S_IRUSR | S_IWUSR);

}

/* is there enough to talk to a server with? */
static int haveaccount(void)

{

    long i;

    for (i = 0; i < srvct; i++)
        if (*servers[i].imap && *servers[i].user && *servers[i].pass)
            return (TRUE);

    return (FALSE);

}

/*******************************************************************************

Headers

Mail headers are folded: a header too long for a line is continued on the
next line, which begins with a space. And anything that is not plain
ascii is encoded, by RFC 2047, as =?charset?B?...?= or =?charset?Q?...?=.
Both are undone here, since a subject line shown with its encoding still
on it is not a subject line anybody can read.

*******************************************************************************/

static long b64val(int c)

{

    if (c >= 'A' && c <= 'Z') return (c-'A');
    if (c >= 'a' && c <= 'z') return (c-'a'+26);
    if (c >= '0' && c <= '9') return (c-'0'+52);
    if (c == '+') return (62);
    if (c == '/') return (63);

    return (-1);

}

/* decode base64 into the buffer, giving the length */
static long b64dec(const char* s, long n, char* d, long dn)

{

    long acc = 0, bits = 0, o = 0;
    long i;

    for (i = 0; i < n && o < dn-1; i++) {

        long v = b64val(s[i]);

        if (v < 0) continue; /* whitespace, padding, anything else */
        acc = (acc<<6) | v;
        bits += 6;
        if (bits >= 8) {

            bits -= 8;
            d[o++] = (acc>>bits) & 0xff;

        }

    }
    d[o] = 0;

    return (o);

}

/* decode quoted printable into the buffer, giving the length. In a header
   word an underscore stands for a space; in a body it does not. */
static long qpdec(const char* s, long n, char* d, long dn, int inhdr)

{

    long i, o = 0;

    for (i = 0; i < n && o < dn-1; i++) {

        if (s[i] == '=' && i+2 < n && isxdigit((unsigned char)s[i+1]) &&
                                      isxdigit((unsigned char)s[i+2])) {

            char h[3];

            h[0] = s[i+1]; h[1] = s[i+2]; h[2] = 0;
            d[o++] = (char)strtol(h, NULL, 16);
            i += 2;

        } else if (s[i] == '=' && i+1 < n && s[i+1] == '\n') i++; /* soft */
        else if (s[i] == '=' && i+2 < n && s[i+1] == '\r' && s[i+2] == '\n')
            i += 2; /* a soft break, which joins the lines */
        else if (inhdr && s[i] == '_') d[o++] = ' ';
        else d[o++] = s[i];

    }
    d[o] = 0;

    return (o);

}

/* Undo the RFC 2047 encoding of a header. Only the bytes are recovered:
   a character set that is not ascii or utf8 is left as its bytes, which
   is what the display can show anyway. */
static void hdrdecode(char* s)

{

    char  out[MAXSTR];
    char* p = s;
    long  o = 0;

    while (*p && o < (long)sizeof(out)-1) {

        if (p[0] == '=' && p[1] == '?') {

            char* cs = p+2;
            char* enc;
            char* txt;
            char* end;

            enc = strchr(cs, '?');
            if (!enc) { out[o++] = *p++; continue; }
            txt = strchr(enc+1, '?');
            if (!txt) { out[o++] = *p++; continue; }
            end = strstr(txt+1, "?=");
            if (!end) { out[o++] = *p++; continue; }
            /* the encoding is the one character between the question marks */
            if (enc[1] == 'B' || enc[1] == 'b')
                o += b64dec(txt+1, end-(txt+1), out+o, sizeof(out)-o);
            else if (enc[1] == 'Q' || enc[1] == 'q')
                o += qpdec(txt+1, end-(txt+1), out+o, sizeof(out)-o, TRUE);
            else { out[o++] = *p++; continue; }
            p = end+2;
            /* Space between two encoded words is not part of the text and
               is dropped, which is what puts the words back together. */
            if (*p == ' ' && p[1] == '=' && p[2] == '?') p++;

        } else out[o++] = *p++;

    }
    out[o] = 0;
    copystr(s, out, MAXSTR);

}

/* Find a header in a message and give its value, unfolded and decoded.
   The message is the whole thing, headers then a blank line then the
   body, which is how it is stored. */
static int findheader(const char* msg, const char* name, char* val, long vn)

{

    const char* p = msg;
    long        nl = strlen(name);

    *val = 0;
    while (*p && !(p[0] == '\n' && (p[1] == '\n' || (p[1] == '\r' &&
                                                     p[2] == '\n')))) {

        if (!strncasecmp(p, name, nl) && p[nl] == ':') {

            const char* v = p+nl+1;
            long        o = 0;

            while (*v == ' ' || *v == '\t') v++;
            /* take it, and any line after it that begins with a blank,
               which is the same header continued */
            while (*v && o < vn-1) {

                if (*v == '\n') {

                    if (v[1] != ' ' && v[1] != '\t') break; /* it ends */
                    while (*v == '\n' || *v == '\r' ||
                           *v == ' ' || *v == '\t') v++;
                    val[o++] = ' '; /* the fold becomes one space */

                } else if (*v == '\r') v++;
                else val[o++] = *v++;

            }
            val[o] = 0;
            trim(val);
            hdrdecode(val);

            return (TRUE);

        }
        /* on to the next header */
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }

    return (FALSE);

}

/* where the body starts: after the blank line that ends the headers */
static const char* bodyof(const char* msg)

{

    const char* p = msg;

    while (*p) {

        if (p[0] == '\n' && p[1] == '\n') return (p+2);
        if (p[0] == '\n' && p[1] == '\r' && p[2] == '\n') return (p+3);
        p++;

    }

    return (p); /* no body at all */

}

/*******************************************************************************

What kind of mail this is

Every message is given a category when it is indexed. The rules are in a
file, not in this program: mail.cat, read at startup, one rule to a line
-- a category and what it matches -- with the first match naming the
message. Nothing about kinds of mail is written into the code, so the
categories can be changed, added to or thrown away without a compiler.

The rule that earns its keep is the last one. Machine-sent mail says so
in its headers: List-Unsubscribe, List-Id, Precedence: bulk. Mail with
none of those, having matched nothing more particular, was written by a
person -- which is the division that actually matters in a mailbox, and
the one headers get right nearly always.

*******************************************************************************/

typedef struct catrule {

    char            cat[32];   /* what it is called */
    char            kind[12];  /* from, to, subject, header, bulk, always */
    char            match[200]; /* what it looks for */
    struct catrule* next;

} catrule;

static catrule* catrules;
static long     catct;

/* read the rules, once */
static void loadcats(void)

{

    static const char* where[] = { "%smail.cat", "%s../graph_programs/mail.cat",
                                   "mail.cat", "graph_programs/mail.cat" };
    char     path[MAXSTR];
    char     dir[MAXSTR];
    char*    e;
    FILE*    f = NULL;
    char     line[MAXSTR];
    catrule* last = NULL;
    long     i;

    if (catrules) return;
    dir[0] = 0;
    if (mailprog) {

        snprintf(dir, sizeof(dir), "%s", mailprog);
        e = strrchr(dir, '/');
        if (e) e[1] = 0; else dir[0] = 0;

    }
    for (i = 0; i < 4 && !f; i++) {

        if (i < 2) snprintf(path, sizeof(path), where[i], dir);
        else snprintf(path, sizeof(path), "%s", where[i]);
        f = fopen(path, "r");

    }
    if (!f) return; /* no rules: everything will be "other" */
    while (fgets(line, sizeof(line), f)) {

        char*    p = line;
        char*    k;
        char*    m;
        catrule* r;

        trim(p);
        if (!*p || *p == '#') continue;
        k = p;
        while (*k && *k != ' ' && *k != '\t') k++;
        if (*k) *k++ = 0;
        while (*k == ' ' || *k == '\t') k++;
        m = k;
        while (*m && *m != ' ' && *m != '\t') m++;
        if (*m) *m++ = 0;
        while (*m == ' ' || *m == '\t') m++;
        r = getmem(sizeof(catrule));
        copystr(r->cat, p, sizeof(r->cat));
        copystr(r->kind, k, sizeof(r->kind));
        copystr(r->match, m, sizeof(r->match));
        r->next = NULL;
        if (last) last->next = r; else catrules = r;
        last = r;
        catct++;

    }
    fclose(f);

}

/* does this text hold that, in any case? */
static int holds(const char* hay, const char* needle)

{

    long n = strlen(needle);
    const char* p;

    if (!n) return (FALSE);
    for (p = hay; *p; p++) if (!strncasecmp(p, needle, n)) return (TRUE);

    return (FALSE);

}

/* Does the message carry the marks of having been sent to a list? This
   is the structural test, and the one that tells a person from a
   machine without knowing anything about either. */
static int isbulk(const char* msg)

{

    char v[MAXSTR];

    if (findheader(msg, "List-Unsubscribe", v, sizeof(v))) return (TRUE);
    if (findheader(msg, "List-Id", v, sizeof(v))) return (TRUE);
    if (findheader(msg, "Precedence", v, sizeof(v)) &&
        (holds(v, "bulk") || holds(v, "list"))) return (TRUE);
    if (findheader(msg, "Auto-Submitted", v, sizeof(v)) &&
        !holds(v, "no")) return (TRUE);

    return (FALSE);

}

/* what kind of mail this message is */
static void classify(const char* msg, char* cat, long cl)

{

    catrule* r;
    char     from[MAXSTR], to[MAXSTR], subj[MAXSTR], v[MAXSTR];

    copystr(cat, "other", cl);
    loadcats();
    findheader(msg, "From", from, sizeof(from));
    findheader(msg, "Subject", subj, sizeof(subj));
    findheader(msg, "To", to, sizeof(to));
    for (r = catrules; r; r = r->next) {

        int hit = FALSE;

        if (!strcasecmp(r->kind, "from")) hit = holds(from, r->match);
        else if (!strcasecmp(r->kind, "subject")) hit = holds(subj, r->match);
        else if (!strcasecmp(r->kind, "to")) {

            hit = holds(to, r->match);
            if (!hit && findheader(msg, "Cc", v, sizeof(v)))
                hit = holds(v, r->match);

        } else if (!strcasecmp(r->kind, "header"))
            hit = findheader(msg, r->match, v, sizeof(v));
        else if (!strcasecmp(r->kind, "bulk")) hit = isbulk(msg);
        else if (!strcasecmp(r->kind, "always")) hit = TRUE;
        if (hit) { copystr(cat, r->cat, cl); return; }

    }

}

/*******************************************************************************

Making a message readable

Mail is not text any more. It is nearly always MIME, often several
copies of the same message in different forms, and what is wanted is the
plain text one. This finds it, decodes it, and where there is no plain
text one at all it takes the html and strips the tags, which is not
pretty but is better than showing the markup.

*******************************************************************************/

/* the value of a parameter of a header, as charset= or boundary= */
static void hdrparam(const char* hdr, const char* name, char* val, long vn)

{

    const char* p = hdr;
    long        nl = strlen(name);

    *val = 0;
    while ((p = strchr(p, ';'))) {

        p++;
        while (*p == ' ' || *p == '\t') p++;
        if (!strncasecmp(p, name, nl) && p[nl] == '=') {

            const char* v = p+nl+1;
            long        o = 0;
            char        q = 0;

            if (*v == '"') q = *v++;
            while (*v && o < vn-1 && (q? *v != q: (*v != ';' && *v != ' ')))
                val[o++] = *v++;
            val[o] = 0;

            return;

        }

    }

}

/* take the tags out of html, leaving what was between them */
static void detag(const char* s, char* d, long dn)

{

    long o = 0;
    int  sp = FALSE;

    while (*s && o < dn-1) {

        if (*s == '<') { /* a tag, and maybe a whole part */

            if (!strncasecmp(s, "<br", 3) || !strncasecmp(s, "</p", 3) ||
                !strncasecmp(s, "</div", 5) || !strncasecmp(s, "</tr", 4)) {

                if (o < dn-1) d[o++] = '\n';

            }
            if (!strncasecmp(s, "<style", 6) || !strncasecmp(s, "<script", 7)) {

                const char* e = strchr(s+1, '>');

                /* everything in these is for the machine, not the reader */
                while (*s && strncasecmp(s, "</", 2)) s++;
                while (*s && *s != '>') s++;
                if (*s) s++;
                if (!e) break;
                continue;

            }
            while (*s && *s != '>') s++;
            if (*s) s++;
            sp = TRUE;

        } else if (*s == '&') { /* an entity, of the few worth knowing */

            if (!strncasecmp(s, "&nbsp;", 6)) { d[o++] = ' '; s += 6; }
            else if (!strncasecmp(s, "&amp;", 5)) { d[o++] = '&'; s += 5; }
            else if (!strncasecmp(s, "&lt;", 4)) { d[o++] = '<'; s += 4; }
            else if (!strncasecmp(s, "&gt;", 4)) { d[o++] = '>'; s += 4; }
            else if (!strncasecmp(s, "&quot;", 6)) { d[o++] = '"'; s += 6; }
            else if (!strncasecmp(s, "&#39;", 5)) { d[o++] = '\''; s += 5; }
            else d[o++] = *s++;

        } else {

            if (sp && *s != '\n' && o && d[o-1] != '\n' && d[o-1] != ' ')
                d[o++] = ' ';
            sp = FALSE;
            if (o < dn-1) d[o++] = *s++; else s++;

        }

    }
    d[o] = 0;

}

/* decode one part according to what its headers say it is */
static char* decodepart(const char* part, long len, int* ishtml, long want)

{

    char  enc[MAXSTR];
    char  typ[MAXSTR];
    char* raw = getmem(len+1);
    char* out;
    const char* body;
    long  bl;
    long  room;

    memcpy(raw, part, len);
    raw[len] = 0;
    findheader(raw, "Content-Transfer-Encoding", enc, sizeof(enc));
    findheader(raw, "Content-Type", typ, sizeof(typ));
    *ishtml = !strncasecmp(typ, "text/html", 9);
    body = bodyof(raw);
    bl = len-(body-raw);
    /* Only as much as the caller has a use for. The list wants a few
       hundred characters to show beside the subject; decoding a
       megabyte of base64 to throw all but four hundred bytes of it away
       is what made opening a folder of large messages slow. The reader
       asks for the whole thing by wanting nothing in particular. */
    room = bl+2;
    if (want > 0 && want+2 < room) room = want+2;
    out = getmem(room);
    if (!strcasecmp(enc, "base64")) b64dec(body, bl, out, room);
    else if (!strcasecmp(enc, "quoted-printable"))
        qpdec(body, bl, out, room, FALSE);
    else {

        if (bl > room-1) bl = room-1;
        memcpy(out, body, bl);
        out[bl] = 0;

    }
    free(raw);

    return (out);

}

/* What a part says it is. Only the head of it is looked at: a part can
   be an attachment of twenty megabytes, and its headers are in the
   first few lines of it. */
static void parttype(const char* part, long len, char* typ, long tn)

{

    char hdr[4000];
    long n = len;

    if (n > (long)sizeof(hdr)-1) n = sizeof(hdr)-1;
    memcpy(hdr, part, n);
    hdr[n] = 0;
    findheader(hdr, "Content-Type", typ, tn);

}

/* Find the text of a message: the plain text part if there is one, the
   html one with its tags taken out if there is not. Multipart messages
   are walked into, since the plain part is nearly always inside one. */
static char* textof(const char* msg, long len, long want)

{

    char  typ[MAXSTR];
    char  bound[MAXSTR];
    char* best = NULL;
    int   besthtml = TRUE;
    char  sep[MAXSTR+8];
    const char* p;
    long  sl;

    findheader(msg, "Content-Type", typ, sizeof(typ));
    if (strncasecmp(typ, "multipart/", 10)) { /* one part, the whole thing */

        int   ishtml;
        char* t = decodepart(msg, len, &ishtml, want);

        if (ishtml) {

            char* d = getmem(strlen(t)+1);

            detag(t, d, strlen(t)+1);
            free(t);
            t = d;

        }

        return (t);

    }
    hdrparam(typ, "boundary", bound, sizeof(bound));
    if (!*bound) { /* multipart with no boundary: take it as it lies */

        long  n = want > 0 && want < len? want: len;
        char* t = getmem(n+1);

        memcpy(t, msg, n);
        t[n] = 0;

        return (t);

    }
    snprintf(sep, sizeof(sep), "--%s", bound);
    sl = strlen(sep);
    /* every part between the separators, keeping the best one found */
    p = msg;
    while ((p = strstr(p, sep))) {

        const char* s = p+sl;
        const char* e;
        int         ishtml;
        char*       t;
        char        ptyp[MAXSTR];

        if (s[0] == '-' && s[1] == '-') break; /* the end separator */
        while (*s == '\r') s++;
        if (*s == '\n') s++;
        e = strstr(s, sep);
        if (!e) e = msg+len;
        parttype(s, e-s, ptyp, sizeof(ptyp));
        if (!strncasecmp(ptyp, "multipart/", 10)) {

            /* A part can be a multipart itself, and usually is: mail
               with an attachment is a multipart/mixed holding a
               multipart/alternative holding the text. Taken as a part
               rather than gone into, the reader is shown the separators
               and the part headers as though they were the message. */
            t = textof(s, e-s, want);
            ishtml = FALSE; /* whatever it found, it found the best of */

        } else if (*ptyp && strncasecmp(ptyp, "text/", 5)) {

            p = e; /* an attachment, a picture, something not to read */
            continue;

        } else t = decodepart(s, e-s, &ishtml, want);
        if (!best || (besthtml && !ishtml)) { /* plain beats html */

            free(best);
            best = t;
            besthtml = ishtml;
            if (!ishtml) break; /* nothing beats plain text */

        } else free(t);
        p = e;

    }
    if (!best) { best = getmem(1); *best = 0; }
    if (besthtml) {

        char* d = getmem(strlen(best)+1);

        detag(best, d, strlen(best)+1);
        free(best);
        best = d;

    }

    return (best);

}

/*******************************************************************************

Dates

The date on a message is RFC 5322: "Fri, 1 Aug 2025 13:06:22 -0700". It is
shown the way mail readers show it, which is the time for something that
came today and the date for anything older, because that is the part the
reader wants.

*******************************************************************************/

static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* pull a date apart, giving the time it stands for, and how to show it */
static long parsedate(const char* s, char* show, long sn)

{

    char      mon[10];
    long      day = 0, year = 0, hour = 0, min = 0, sec = 0;
    long      i;
    struct tm tm;
    time_t    t;
    time_t    now = time(NULL);

    *show = 0;
    while (*s == ' ') s++;
    if (isalpha((unsigned char)*s)) { /* the day name, which tells us nothing */

        while (*s && *s != ',') s++;
        if (*s == ',') s++;

    }
    mon[0] = 0;
    if (sscanf(s, " %ld %9s %ld %ld:%ld:%ld", &day, mon, &year,
               &hour, &min, &sec) < 3) return (0);
    for (i = 0; i < 12; i++) if (!strncasecmp(mon, months[i], 3)) break;
    if (i >= 12) return (0);
    if (year < 100) year += year < 70? 2000: 1900;
    memset(&tm, 0, sizeof(tm));
    tm.tm_mday = day;
    tm.tm_mon = i;
    tm.tm_year = year-1900;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    /* The zone the sender wrote it in. Without it every message from a
       server keeping UTC -- which is most of them -- was read as though
       its clock were ours, and landed hours in the future: mail sent at
       eight in the evening in London sorted above mail sent here five
       minutes ago, and a message answered to yourself came back ten rows
       down the list instead of at the top of it. */
    {

        const char* z = s;
        long        off = 0;   /* seconds east of UTC */
        int         got = FALSE;

        /* the offset comes after the time, as +hhmm or a name */
        /* four of them: the day, the month, the year and the time, which
           is one word however many parts it has. Stepping over six --
           which the first try of this did -- steps over the zone as
           well, and every message goes back to being read in our own
           time, which is the fault this is here to fix. */
        for (i = 0; i < 4 && *z; i++) {

            while (*z == ' ') z++;
            while (*z && *z != ' ') z++;

        }
        while (*z == ' ') z++;
        if (*z == '+' || *z == '-') {

            long hh = 0, mm = 0;

            if (sscanf(z+1, "%2ld%2ld", &hh, &mm) == 2) {

                off = (hh*60+mm)*60;
                if (*z == '-') off = -off;
                got = TRUE;

            }

        } else if (isalpha((unsigned char)*z)) {

            static const struct { const char* nm; long off; } zones[] = {

                { "UT", 0 }, { "GMT", 0 }, { "Z", 0 },
                { "EST", -5 }, { "EDT", -4 }, { "CST", -6 }, { "CDT", -5 },
                { "MST", -7 }, { "MDT", -6 }, { "PST", -8 }, { "PDT", -7 },
                { NULL, 0 }

            };
            long k;

            for (k = 0; zones[k].nm; k++)
                if (!strncasecmp(z, zones[k].nm, strlen(zones[k].nm))) {

                    off = zones[k].off*3600;
                    got = TRUE;

                    break;

                }

        }
        /* timegm reads the fields as UTC, which they are once the
           sender's offset is taken off them */
        if (got) {

            tm.tm_isdst = 0;
            t = timegm(&tm)-off;

        } else { /* no zone: the best that can be done is our own */

            tm.tm_isdst = -1;
            t = mktime(&tm);

        }

    }
    if (t == (time_t)-1) return (0);
    /* Shown in our own time, whatever time the sender kept: a message is
       filed by when it arrived here. */
    {

        struct tm lt;

        localtime_r(&t, &lt);
        if (now-t < 12*60*60) {

            long h12 = lt.tm_hour%12;

            if (!h12) h12 = 12;
            snprintf(show, sn, "%ld:%02d %s", h12, lt.tm_min,
                     lt.tm_hour < 12? "AM": "PM");

        } else if (now-t < 300L*24*60*60)
            snprintf(show, sn, "%s %d", months[lt.tm_mon], lt.tm_mday);
        else snprintf(show, sn, "%s %d, %d", months[lt.tm_mon], lt.tm_mday,
                      lt.tm_year+1900);

    }

    return ((long)t);

}

/*******************************************************************************

The mbox file

A message is stored as it arrived, with a line before it saying who it is
from and when it got here. That line is the only thing added, and it is
how every message after the first is found: a line beginning "From " at
the start of a line begins a message. A line in the message that would
look like one has a > put in front of it, which is what mail programs
have always done and what they all undo on the way out.

*******************************************************************************/

/* the address out of a From header: what is inside the angle brackets if
   there are any, the whole thing if not */
static void addrof(const char* from, char* addr, long an)

{

    const char* p = strchr(from, '<');
    long        o = 0;

    if (p) {

        p++;
        while (*p && *p != '>' && o < an-1) addr[o++] = *p++;

    } else while (*from && *from != ' ' && o < an-1) addr[o++] = *from++;
    addr[o] = 0;
    if (!o) copystr(addr, "unknown", an);

}

/* The name to show for a sender. "Scott Franco <x@y.com>" shows as the
   name, "x@y.com" shows as the address: what the sender called
   themselves if they said, what they are if they did not. */
static void nameof(const char* from, char* name, long nn)

{

    const char* p = strchr(from, '<');
    long        o = 0;

    if (p && p != from) { /* there is a name before the address */

        const char* q = from;

        while (*q == ' ' || *q == '"') q++;
        while (q < p && o < nn-1) name[o++] = *q++;
        while (o && (name[o-1] == ' ' || name[o-1] == '"')) o--;
        name[o] = 0;
        if (o) return;

    }
    addrof(from, name, nn);

}

/* write one message to the folder's mbox file */
/* Store a message, and say where it went: the offset the message itself
   begins at, past the separator line, and the length it came out as --
   which is not the length it went in as, since storing escapes any line
   that could be mistaken for a separator. Those two are what the index
   needs to find it again. */
static long mboxwrite(const char* file, const char* msg, long len,
                      long* stored)

{

    FILE* f = fopen(file, "a");
    long  off;
    char  from[MAXSTR];
    char  addr[MAXSTR];
    char  date[MAXSTR];
    char  show[40];
    long  when;
    const char* p;
    const char* e;

    if (!f) { fail("Cannot write to the mail store"); return (-1); }
    findheader(msg, "From", from, sizeof(from));
    addrof(from, addr, sizeof(addr));
    findheader(msg, "Date", date, sizeof(date));
    when = parsedate(date, show, sizeof(show));
    if (!when) when = (long)time(NULL);
    {

        time_t t = when;

        fprintf(f, "From %s %s", addr, ctime(&t)); /* ctime ends the line */

    }
    off = ftell(f); /* the message itself starts here */
    /* the message, with any line that looks like a separator marked */
    p = msg;
    e = msg+len;
    while (p < e) {

        const char* q = p;

        while (q < e && *q != '\n') q++;
        if (!strncmp(p, "From ", 5) || (*p == '>' && strstr(p, "From ") == p+1))
            fputc('>', f);
        fwrite(p, 1, q-p, f);
        fputc('\n', f);
        p = q < e? q+1: e;

    }
    if (stored) *stored = ftell(f)-off;
    fputc('\n', f); /* a blank line ends a message, always */
    fclose(f);

    return (off);

}

/*******************************************************************************

Indexing a folder

The file is read and every message in it noted: where it is, who it is
from, its subject, when it came, and the start of its text. That is all
the list needs. The message itself is read again from the file when it is
opened, so nothing is held twice.

*******************************************************************************/

/* the start of the message, for the list: the first text, run together */
static void snipof(const char* text, char* snip, long sn)

{

    long o = 0;

    while (*text && o < sn-1) {

        if (*text == '\n' || *text == '\r' || *text == '\t') {

            /* line breaks become one space, since the line is one line */
            if (o && snip[o-1] != ' ') snip[o++] = ' ';
            text++;

        } else if (*text == ' ') {

            if (o && snip[o-1] != ' ') snip[o++] = ' ';
            text++;

        } else snip[o++] = *text++;

    }
    while (o && snip[o-1] == ' ') o--;
    snip[o] = 0;

}

/* note one message found in the file */
/* The index being built. It is built to one side and put in place in one
   move, because it is built on the fetch thread and the display is
   reading the one it already has the whole time. */

/* One message. What is passed is as much of it as was kept -- the
   headers and the beginning of the body, which is all the list shows --
   while len is the length of the whole thing, which is what reading it
   later will need. */
static void fillrec(msgrec* m, const char* msg, long have, long len,
                    long off, const char* dig)

{

    char    from[MAXSTR];
    char    date[MAXSTR];
    char*   text;

    m->off = off;
    m->len = len;
    copystr(m->dig, dig, DIGLEN);
    classify(msg, m->cat, sizeof(m->cat));
    findheader(msg, "From", from, sizeof(from));
    nameof(from, m->from, sizeof(m->from));
    addrof(from, m->addr, sizeof(m->addr));
    if (!findheader(msg, "Subject", m->subject, sizeof(m->subject)))
        copystr(m->subject, "(no subject)", sizeof(m->subject));
    findheader(msg, "Date", date, sizeof(date));
    m->date = parsedate(date, m->when, sizeof(m->when));
    /* only what the list will show: see decodepart */
    text = textof(msg, have, SNIPPET*2);
    snipof(text, m->snip, sizeof(m->snip));
    free(text);

}

/* newest first, which is the order every mail reader shows */
static int bydate(const void* a, const void* b)

{

    const msgrec* x = a;
    const msgrec* y = b;

    if (x->date < y->date) return (1);
    if (x->date > y->date) return (-1);

    return (0);

}

/* Read a folder's file and note every message in it. The separator is a
   line beginning "From " that follows a blank line, or the first line of
   the file; anything else beginning "From " is part of a message. */
/*******************************************************************************

What the worker is doing

None of this is drawn by the worker -- it draws nothing at all. It is
left here and picked up by the display on its next tick. The words
change once a folder and the numbers change once a message, so the two
are kept apart: the display reads a string that is not being rewritten
under it, and puts the numbers -- single words, which cannot be read
half-written -- against it itself.

*******************************************************************************/

static void dlock(void);
static void dunlock(void);
static void serveindex(void);
static void servesend(void);
static int reachable(const char* host, long port, long secs);
static void newmenu(ami_menuptr* mp, int onoff, int bar, int select,
                    long id, char* face);
static void appendmenu(ami_menuptr* list, ami_menuptr m);
static void kickworker(void);

static char wrkwhat[MAXSTR]; /* what is being worked on */
static long wrkpos;          /* how far into it */
static long wrkmax;          /* and how big it is */
static long wrkfolds;        /* the folder pane wants redrawing */
static long wrklist;         /* and so does the message list */
static long wrkstop;         /* drop what you are doing */
static long wrkbusy;         /* it has something in hand just now */

/* Which folder the display wants read, which folder the list it is
   showing was read from, and how many messages were in it then. The last
   two are how a fetch knows whether the open folder needs reading again:
   if nothing landed in it, the list on the screen is still right, and
   reading four gigabytes to find that out is four gigabytes wasted. */
static long idxwant = -1;
static long idxfold = -1;
static long idxdoing = -1;   /* the folder being read just now */

/* How much of a message is kept for what the list shows: the headers
   and the start of the body. A mailbox of four gigabytes was being read
   into one allocation of four gigabytes and every message decoded in
   full to find forty characters of snippet. */
#define MSGCAP 65536

/* The digest of a message, taken as the message goes past rather than
   from a copy of it. It has to agree exactly with digestof(), which
   works on a whole message in memory: escapes undone, and every newline
   at the end taken off. The escape is undone at the start of a line
   only, and the trailing newlines are held back and only given to the
   digest if something follows them. */
/* How much of a run of line ends can be held back. The canonical form
   drops every one of them at the end of a message, so none can be given
   to the digest until something with content follows -- and a blank line
   is line ends and nothing else, so a blank line does not settle the
   question, it lengthens it. Half a kilobyte is more consecutive blank
   lines than mail has; a message ending in more would be digested a
   line end too long, and would not match one digested whole. */
#define PENDMAX 512

typedef struct {

    EVP_MD_CTX* c;
    char        pend[PENDMAX]; /* line ends held back */
    long        pendn;

} digrun;

static void digstart(digrun* d)

{

    d->c = EVP_MD_CTX_new();
    if (!d->c) { fail("Out of memory"); exit(1); }
    EVP_DigestInit_ex(d->c, EVP_sha256(), NULL);
    d->pendn = 0;

}

/* one line of it, or one piece of a line too long to come in one */
static void digline(digrun* d, const char* p, long n, int atbol)

{

    long t;

    if (atbol && n && *p == '>') { /* a line the storing escaped? */

        long j = 0;

        while (j < n && p[j] == '>') j++;
        if (j+5 <= n && !strncmp(p+j, "From ", 5)) { p++; n--; } /* one off */

    }
    t = n;
    while (t && (p[t-1] == '\n' || p[t-1] == '\r')) t--;
    if (!t) { /* nothing but line ends: hold them with the rest */

        if (d->pendn+n <= PENDMAX) { memcpy(d->pend+d->pendn, p, n);
                                     d->pendn += n; }
        else { /* further than can be held: give up holding */

            EVP_DigestUpdate(d->c, d->pend, d->pendn);
            EVP_DigestUpdate(d->c, p, n);
            d->pendn = 0;

        }

        return;

    }
    /* something with content in it: whatever was held really was in the
       middle of the message after all */
    if (d->pendn) { EVP_DigestUpdate(d->c, d->pend, d->pendn); d->pendn = 0; }
    EVP_DigestUpdate(d->c, p, t);
    if (n-t > 0 && n-t <= PENDMAX) { memcpy(d->pend, p+t, n-t); d->pendn = n-t; }

}

static void digend(digrun* d, char* hex)

{

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int  n = 0;
    long          i;

    EVP_DigestFinal_ex(d->c, md, &n);
    EVP_MD_CTX_free(d->c);
    d->c = NULL;
    for (i = 0; i < (long)n; i++) sprintf(hex+i*2, "%02x", md[i]);
    hex[n*2] = 0;

}

/*******************************************************************************

Keeping the indexes across a rebuild of the folder list

Asking the servers what folders they have builds the list again from
their answers, which throws away everything the old list held --
including every folder's index. Reading them all back means reading
every mailbox in the store, and losing them means the digests go with
them: a fetch straight after a Get Mail would find nothing already here
and store the whole store a second time. It did, once, and doubled a
test mailbox before the counts gave it away.

So the indexes are set aside before the list is taken again, and each
one is given back to whichever folder ends up with its mailbox. What no
folder claims is let go: its mailbox is not there any more.

*******************************************************************************/

typedef struct {

    char    file[MAXSTR*2];
    msgrec* idx;
    long    ct;
    long    max;
    long    ok;

} idxkeep;

static idxkeep* kept;
static long     keptct;

static void idxsetaside(void)

{

    long i;

    free(kept);
    kept = getmem(sizeof(idxkeep)*(foldct? foldct: 1));
    keptct = 0;
    for (i = 0; i < foldct; i++) {

        if (!folders[i].idx) continue;
        copystr(kept[keptct].file, folders[i].file, MAXSTR*2);
        kept[keptct].idx = folders[i].idx;
        kept[keptct].ct = folders[i].idxct;
        kept[keptct].max = folders[i].idxmax;
        kept[keptct].ok = folders[i].idxok;
        keptct++;
        folders[i].idx = NULL; /* it belongs to the kept list now */
        folders[i].idxct = 0;
        folders[i].idxmax = 0;
        folders[i].idxok = FALSE;

    }

}

static void idxgiveback(void)

{

    long i, j;

    for (i = 0; i < foldct; i++)
        for (j = 0; j < keptct; j++)
            if (kept[j].idx && !strcmp(kept[j].file, folders[i].file)) {

                folders[i].idx = kept[j].idx;
                folders[i].idxct = kept[j].ct;
                folders[i].idxmax = kept[j].max;
                folders[i].idxok = kept[j].ok;
                kept[j].idx = NULL;
                break;

            }
    for (j = 0; j < keptct; j++) free(kept[j].idx); /* nobody wanted it */
    free(kept);
    kept = NULL;
    keptct = 0;
    rehashall(); /* the table names messages by folder, and folders moved */

}

/* A folder has been read, however it was read: from its index file, or
   from the mailbox, or both. It is put in order, the display is pointed
   at it, and the table that finds a message by its digest is taken
   again -- which has to happen on every path, including the one where
   the index was complete and nothing was read at all. It was missing
   from exactly that path once, and the whole store lost its dedup: a
   second fetch stored every message it already had. */
static void idxdone(long fold)

{

    msgrec* was;

    /* Sorted here, where nothing else can see it. Sorting the array the
       display is drawing moves the rows out from under it. */
    qsort(bidx, bct, sizeof(msgrec), bydate);
    dlock();
    was = folders[fold].idx;
    folders[fold].idx = bidx;
    folders[fold].idxct = bct;
    folders[fold].idxmax = bmax;
    folders[fold].msgs = bct;
    folders[fold].idxok = TRUE;
    folders[fold].dirty = FALSE;
    idxfold = fold;
    useidx(); /* the display's list is the new array from here on */
    dunlock();
    /* the array it had is nobody's now */
    free(was);
    bidx = NULL;
    bct = 0;
    bmax = 0;
    rehashall();

}

/* Read a folder: its index file if there is a good one, and whatever
   the mailbox holds past the end of what that names. */
static void indexfolder(long fold)

{

    FILE*  f;
    char*  hold;
    char   line[MAXLINE];
    char   hex[DIGLEN];
    digrun dg;
    long   holdn = 0;
    long   start = -1;    /* where the message being read began */
    long   endpos = 0;    /* and where its last line of substance ended */
    long   pos = 0;       /* where in the file the next piece begins */
    long   size = 0;
    long   from = 0;      /* where the reading of the mailbox begins */
    long   had;           /* how many were already known */
    int    blank = TRUE;  /* the line before this one held nothing */
    int    atbol = TRUE;  /* and this piece begins a line */
    int    stopped = FALSE;
    struct timespec t0;

    if (fold < 0) return;
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
    bct = 0;
    idxload(fold);
    had = bct;
    from = folders[fold].idxok? idxend(): 0;
    f = fopen(folders[fold].file, "r");
    if (!f) { /* nothing fetched yet, which is not an error */

        dlock();
        folders[fold].idxct = 0;
        folders[fold].msgs = 0;
        idxfold = fold;
        useidx();
        dunlock();

        return;

    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, from, SEEK_SET);
    pos = from;
    if (size <= from) { /* the index already names all of it */

        fclose(f);
        idxdone(fold);
        if (diag) fprintf(stderr, "index: %s, %ld known, nothing new\n",
                          folders[fold].name, folders[fold].idxct);

        return;

    }
    hold = getmem(MSGCAP+1);
    digstart(&dg);
    /* Read through a piece at a time. A separator is a line beginning
       "From " with nothing on the line before it, which is what the
       storing writes and what it escapes in the messages themselves. A
       line longer than the buffer comes in pieces, and only the first
       piece of one begins a line: everything that asks what a line
       starts with has to ask that first. */
    while (fgets(line, sizeof(line), f)) {

        long ll = strlen(line);
        int  sep = atbol && blank && !strncmp(line, "From ", 5);

        if (sep) {

            if (start >= 0) {

                msgrec m;

                digend(&dg, hex);
                hold[holdn] = 0;
                fillrec(&m, hold, holdn, endpos-start, start, hex);
                *bldroom() = m;
                digstart(&dg);

            }
            start = pos+ll; /* the message begins after the separator */
            endpos = start;
            holdn = 0;
            /* Say how far along, and give up if the reader has asked for
               another folder: there is no sense reading four gigabytes
               nobody is waiting for. */
            if (!(bct%256)) {

                wrkpos = pos-from;
                wrkmax = size-from;
                if ((idxwant >= 0 && idxwant != fold) || wrkstop)
                    { stopped = TRUE; break; }

            }

        } else if (start >= 0) {

            digline(&dg, line, ll, atbol);
            if (holdn < MSGCAP) {

                long take = ll;

                if (take > MSGCAP-holdn) take = MSGCAP-holdn;
                memcpy(hold+holdn, line, take);
                holdn += take;

            }

        }
        atbol = ll && line[ll-1] == '\n';
        if (atbol) blank = ll == 1 || (ll == 2 && line[0] == '\r');
        if (!blank || !atbol) endpos = pos+ll; /* messages do not end blank */
        pos += ll;

    }
    if (start >= 0 && !stopped) { /* the last one */

        msgrec m;

        digend(&dg, hex);
        hold[holdn] = 0;
        fillrec(&m, hold, holdn, endpos-start, start, hex);
        *bldroom() = m;

    } else if (dg.c) { digend(&dg, hex); }
    free(hold);
    fclose(f);
    /* What was read is written down: the whole file if it was worked out
       from nothing, the new lines only if it was added to. */
    if (!from) idxsave(fold);
    else {

        long i;

        for (i = had; i < bct; i++) idxappend(fold, &bidx[i]);

    }
    idxdone(fold);
    if (diag) {

        struct timespec t1;

        clock_gettime(CLOCK_MONOTONIC, &t1);
        fprintf(stderr, "index: %s, %ld new of %ld, read from %ld of %ld, "
                "%.0fms\n", folders[fold].name, folders[fold].idxct-had,
                folders[fold].idxct, from, size,
                (t1.tv_sec-t0.tv_sec)*1000.0+(t1.tv_nsec-t0.tv_nsec)/1000000.0);

    }

}

/* How many messages a folder holds comes from its index, so asking is
   free and nothing reads a mailbox to answer it. A folder with no index
   yet asks for one. */
static void countfolders(void)

{

    long i;

    for (i = 0; i < foldct; i++) {

        folders[i].msgs = folders[i].idxct;
        if (!folders[i].idxok) folders[i].wantidx = TRUE;

    }

}

static void srvdir(long srv, char* dn, long dnl)

{

    struct stat sb;

    snprintf(dn, dnl, "%.400s/%.60s", store,
             srv >= 0 && srv < srvct? servers[srv].name: "local");
    if (stat(dn, &sb)) ami_makpth(dn);

}

/* a name with the awkward characters taken out, fit to be a file name */
static void safename(const char* nm, char* fn, long fnl)

{

    long i;

    snprintf(fn, fnl, "%s", nm);
    for (i = 0; fn[i]; i++)
        if (fn[i] == '/' || fn[i] == '\\' || fn[i] == ' ' ||
            fn[i] == '[' || fn[i] == ']' || fn[i] == '"') fn[i] = '_';

}

/* the file a local folder of this name lives in */
static void localfile(const char* show, char* fn, long fnl)

{

    char nm[MAXSTR/2];
    long i;

    copystr(nm, show, sizeof(nm));
    for (i = 0; nm[i]; i++)
        if (nm[i] == '/' || nm[i] == '\\' || nm[i] == ' ' ||
            nm[i] == '[' || nm[i] == ']' || nm[i] == '"') nm[i] = '_';
    {

        char dn[MAXSTR];

        srvdir(-1, dn, sizeof(dn));
        snprintf(fn, fnl, "%.400s/%.150s.mbox", dn, nm);

    }

}

/* Find a local folder by its shown name, or make one. Making one is
   writing its name file, so it comes back after a restart, and putting
   it in the list, which keeps locals after the server's folders. */
static long localfolder(const char* show)

{

    long     i;
    foldrec* f;
    char     fn[MAXSTR*2+8];
    FILE*    sf;

    for (i = 0; i < foldct; i++)
        if (folders[i].local && !strcmp(folders[i].show, show)) return (i);
    if (foldct >= MAXFOLDER) return (-1);
    f = &folders[foldct++];
    copystr(f->name, show, MAXSTR);
    copystr(f->show, show, MAXSTR);
    localfile(show, f->file, sizeof(f->file));
    f->noselect = FALSE;
    f->local = TRUE;
    /* Belonging to no server, which is the whole of what a local folder
       is. Left unsaid it inherits whatever was in the slot, and the
       folder appears under a server's heading -- which is exactly the
       thing a local folder is not. */
    f->srv = -1;
    f->msgs = 0;
    /* the name beside the file, as the server folders keep theirs */
    snprintf(fn, sizeof(fn), "%s.state", f->file);
    sf = fopen(fn, "w");
    if (sf) { fprintf(sf, "0 0 %s\n", show); fclose(sf); }

    return (foldct-1);

}

/* Move messages out of a folder's file and into another's. The set says
   which, by index in the folder's message list. The blocks are moved
   whole and verbatim -- separator line to trailing blank -- so nothing
   is reencoded, requoted or otherwise touched on the way. */
static long movelocal(long fold, const char* dstfile, const char* set)

{

    FILE* f;
    FILE* out;
    FILE* dst;
    char  tmp[MAXSTR*2+8];
    char* buf;
    long  n;
    long  i, start, blkstart;
    long  moved = 0;

    f = fopen(folders[fold].file, "r");
    if (!f) return (0);
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = getmem(n+1);
    n = fread(buf, 1, n, f);
    buf[n] = 0;
    fclose(f);
    snprintf(tmp, sizeof(tmp), "%s/movetmp", store);
    out = fopen(tmp, "w");
    dst = fopen(dstfile, "a");
    if (!out || !dst) {

        if (out) fclose(out);
        if (dst) fclose(dst);
        free(buf);
        fail("The move could not open its files");

        return (0);

    }
    /* walk the separators exactly as the indexing does, so the blocks
       here are the messages there */
    start = -1;
    blkstart = 0;
    for (i = 0; i < n; i++) {

        int atsep = !strncmp(buf+i, "From ", 5) &&
                    (i == 0 || (i >= 2 && buf[i-1] == '\n' &&
                                (buf[i-2] == '\n' ||
                                 (buf[i-2] == '\r' && i >= 3 &&
                                  buf[i-3] == '\n'))));

        if (atsep) {

            if (start >= 0) { /* the block that just ended */

                long m;

                for (m = 0; m < msgct; m++) if (msgs[m].off == start) break;
                if (m < msgct && set[m]) {

                    fwrite(buf+blkstart, 1, i-blkstart, dst);
                    moved++;

                } else fwrite(buf+blkstart, 1, i-blkstart, out);

            }
            blkstart = i;
            while (i < n && buf[i] != '\n') i++;
            start = i+1;

        }
        while (i < n && buf[i] != '\n') i++;

    }
    if (start >= 0) { /* the last block */

        long m;

        for (m = 0; m < msgct; m++) if (msgs[m].off == start) break;
        if (m < msgct && set[m]) {

            fwrite(buf+blkstart, 1, n-blkstart, dst);
            moved++;

        } else fwrite(buf+blkstart, 1, n-blkstart, out);

    }
    free(buf);
    fclose(dst);
    fclose(out);
    rename(tmp, folders[fold].file);
    /* This mailbox has been written out again without the messages that
       left it, so everything after the first of them sits somewhere
       else. The index is thrown away and taken again from what is
       actually there. */
    idxdrop(fold);

    return (moved);

}

/*******************************************************************************

The message menu

The second mouse button on a message opens a small menu beside it. The
menu is a window like everything else here, serviced from the same event
loop by its id, and everything it offers is local: the server is not
touched, not even asked.

*******************************************************************************/

static FILE* popwf;      /* the menu, NULL when closed */
static long  popmsg;     /* the message it is for */
static long  poprow = -1; /* the entry under the mouse */
static long  poprowh;    /* the height of an entry */
static char  poplab[3][MAXSTR]; /* the entries' faces */

/* The part of an address that says who sent it, for gathering their
   mail together. That is the domain and not the whole address: LinkedIn
   writes from messages-noreply@, notifications-noreply@, inmail-hit-
   reply@ and linkedin@em.linkedin.com, and a reader who asks for a
   folder of LinkedIn mail means all of it.

   Not for the domains people have their own addresses at, though. Two
   friends at gmail.com are two people, and sweeping every message from
   gmail.com into one folder is not what anybody meant. */
static void senderkey(const char* addr, char* key, long kl)

{

    static const char* personal[] = {

        "gmail.com", "googlemail.com", "yahoo.com", "hotmail.com",
        "outlook.com", "live.com", "icloud.com", "me.com", "aol.com",
        "protonmail.com", "proton.me", "gmx.com", "mail.com", NULL

    };
    const char* at = strchr(addr, '@');
    long        i;

    if (!at || !at[1]) { copystr(key, addr, kl); return; }
    for (i = 0; personal[i]; i++)
        if (!strcasecmp(at+1, personal[i]))
            { copystr(key, addr, kl); return; } /* a person, not a sender */
    copystr(key, at+1, kl);

}

/* does this message come from that sender? */
static int fromsender(const msgrec* m, const char* key)

{

    char k[100];

    senderkey(m->addr, k, sizeof(k));

    return (!strcasecmp(k, key));

}

static void popclose(void)

{

    if (popwf) { fclose(popwf); popwf = NULL; }
    poprow = -1;

}

static void popdraw(void)

{

    long i;
    long w = ami_maxxg(popwf);
    long h = ami_maxyg(popwf);

    fprintf(popwf, "\f");
    /* the edge, so it reads as a card over the list */
    ami_fcolorc(popwf, rgb(120), rgb(120), rgb(120));
    ami_line(popwf, 0, 0, w-1, 0);
    ami_line(popwf, 0, h-1, w-1, h-1);
    ami_line(popwf, 0, 0, 0, h-1);
    ami_line(popwf, w-1, 0, w-1, h-1);
    ami_fcolor(popwf, ami_black);
    for (i = 0; i < 3; i++) {

        ami_cursorg(popwf, 8, 3+i*poprowh+(poprowh-chrh)/2);
        fprintf(popwf, "%s", poplab[i]);

    }

}

/* open the menu for a message, beside the mouse */
static void popopen(long i, long x, long y)

{

    long w, h;
    char nm[60];

    popclose();
    popmsg = i;
    copystr(nm, msgs[i].from, sizeof(nm));
    snprintf(poplab[0], sizeof(poplab[0]), "Move to local Trash");
    {

        char key[100];
        long n;

        /* Say what it will gather, since the sender's name and what
           their mail comes from are not always the same word. */
        senderkey(msgs[i].addr, key, sizeof(key));
        n = 0;
        {

            long m;

            for (m = 0; m < msgct; m++) if (fromsender(&msgs[m], key)) n++;

        }
        snprintf(poplab[1], sizeof(poplab[1]),
                 "Local folder for %s (%ld here)", key, n);
        /* And by the name they show, which is not the same thing: a
           domain gathers eight sorts of Facebook notice into one folder,
           and a name keeps a person who writes through LinkedIn out of
           the LinkedIn folder. Both are offered, with what each would
           take, because which is wanted depends on the sender. */
        n = 0;
        {

            long m;

            for (m = 0; m < msgct; m++)
                if (!strcmp(msgs[m].from, msgs[i].from)) n++;

        }
        snprintf(poplab[2], sizeof(poplab[2]),
                 "Local folder for \"%s\" (%ld here)", nm, n);

    }
    poprowh = chrh+10;
    w = ami_strsiz(listwf, poplab[0]);
    if (ami_strsiz(listwf, poplab[1]) > w) w = ami_strsiz(listwf, poplab[1]);
    if (ami_strsiz(listwf, poplab[2]) > w) w = ami_strsiz(listwf, poplab[2]);
    w += 20;
    h = poprowh*3+6;
    /* The menu is a child of the main window, not of the list: a child
       is clipped by its parent, and a menu opened near the bottom of
       the list would be cut off by it. The mouse position arrives in
       the list's coordinates, so it is shifted by where the list sits. */
    x += listx;
    y += listy;
    if (x+w > ami_maxxg(stdout)) x = ami_maxxg(stdout)-w;
    if (y+h > ami_maxyg(stdout)) y = ami_maxyg(stdout)-h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    ami_openwin(&stdin, &popwf, stdout, POPWIN);
    ami_frame(popwf, FALSE);
    ami_auto(popwf, FALSE);
    ami_curvis(popwf, FALSE);
    ami_font(popwf, AMI_FONT_SIGN);
    ami_setpoints(popwf, 11.0);
    ami_binvis(popwf);
    ami_setsizg(popwf, w, h);
    ami_setposg(popwf, x, y);
    /* in front of the panes: it is a sibling of them, and a menu behind
       the list it belongs to is a menu nobody can see */
    ami_front(popwf);
    popdraw();

}

/* what was picked: 0 is trash, 1 is a folder for the sender */
static void popact(long row)

{

    char* set;
    long  dst;
    long  moved;
    long  i = popmsg;
    char  msg[MAXSTR];
    char  who[60];

    popclose();
    if (foldsel < 0 || i < 0 || i >= msgct) return;
    set = getmem(msgct);
    memset(set, 0, msgct);
    if (row == 0) { /* this one message, to the local trash */

        set[i] = TRUE;
        dst = localfolder("Trash");
        copystr(who, "Trash", sizeof(who));

    } else if (row == 1) { /* everything from that place */

        long m;
        char key[100];

        senderkey(msgs[i].addr, key, sizeof(key));
        for (m = 0; m < msgct; m++)
            if (fromsender(&msgs[m], key)) set[m] = TRUE;
        copystr(who, msgs[i].from, sizeof(who));
        dst = localfolder(who);

    } else { /* everything from that name */

        long m;

        for (m = 0; m < msgct; m++)
            if (!strcmp(msgs[m].from, msgs[i].from)) set[m] = TRUE;
        copystr(who, msgs[i].from, sizeof(who));
        dst = localfolder(who);

    }
    if (dst < 0) { free(set); fail("No room for another folder"); return; }
    moved = movelocal(foldsel, folders[dst].file, set);
    free(set);
    msgsel = -1;
    /* The counts are worked out rather than counted again: what left
       this folder arrived in that one, and counting means reading every
       mailbox in the store through -- gigabytes, to learn a number
       already known. The folder itself is read again by the worker,
       since its file has just changed under the list. */
    folders[foldsel].msgs -= moved;
    if (folders[foldsel].msgs < 0) folders[foldsel].msgs = 0;
    folders[dst].msgs += moved;
    folders[foldsel].dirty = TRUE;
    folders[dst].dirty = TRUE;
    msgct = 0;
    idxfold = -1;
    idxwant = foldsel;
    kickworker();
    drawlist();
    drawfolders();
    snprintf(msg, sizeof(msg), "%ld message%s moved to %s -- locally; the "
             "server is not touched", moved, moved == 1? "": "s", who);
    status(msg);

}

/* read one message back out of the file, whole */
static char* getmsg(long fold, long i)

{

    FILE* f;
    char* buf;

    if (fold < 0 || i < 0 || i >= msgct) return (NULL);
    f = fopen(folders[fold].file, "r");
    if (!f) return (NULL);
    buf = getmem(msgs[i].len+1);
    fseek(f, msgs[i].off, SEEK_SET);
    msgs[i].len = fread(buf, 1, msgs[i].len, f);
    buf[msgs[i].len] = 0;
    fclose(f);

    return (buf);

}

/*******************************************************************************

Talking to the IMAP server

The protocol is lines. A command is given a tag, and everything the
server says back is either untagged, beginning with *, which is data, or
tagged with our tag, which is the answer and ends the command. What makes
it more than that is the literal: a length in braces at the end of a
line, followed by exactly that many bytes, which may be anything at all
including newlines. That is how a message arrives.

*******************************************************************************/

/* send a command, with a tag of its own, and say what the tag was */
static void imsend(char* tag, long tn, const char* fmt, ...)

{

    va_list ap;
    char    cmd[MAXLINE];

    snprintf(tag, tn, "a%03ld", ++imaptag);
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    if (diag) fprintf(stderr, "> %s %s\n", tag, cmd);
    fprintf(imap, "%s %s\r\n", tag, cmd);
    fflush(imap);

}

static long netquiet; /* the last read got nothing at all */

/*******************************************************************************

Two threads

The fetching is done on a thread of its own, and the display is left to
the main one. It has to be: a read from a server can take as long as the
server feels like taking, and while it did that on the display thread
the window did not repaint and the mouse did nothing. Timeouts bounded
that wait; they did not remove it.

The division is simple, and it is worth stating plainly because the
whole of the locking depends on it.

    The worker owns the network. It opens connections, reads messages,
    appends them to the mailboxes, writes the digests and the markers,
    and moves the folder counts up. It draws nothing and calls no
    graphics function at all.

    The main thread owns the display. Everything it does, it does while
    handling an event, and it looks at the worker's progress on a timer
    -- ten times a second, which is faster than the eye and slower than
    the flicker that redrawing per message would be.

One lock guards what they share: the folder table and the files under
the store. The main thread holds it for as long as it is handling an
event and drops it while it waits for the next one, so every click,
redraw and menu choice is already covered without a lock of its own.
The worker takes it around the commit of each message -- the append, the
digest, the count -- which is short, and around anything that rebuilds
the folder table.

Because the lock is not recursive, it is taken in exactly those two
places and nowhere else. No helper takes it, so no helper can be called
from a path that already holds it and deadlock.

*******************************************************************************/

static long datlock;   /* what the two of them share */

static void dlock(void)

{

    if (datlock) ami_lock(datlock);

}

static void dunlock(void)

{

    if (datlock) ami_unlock(datlock);

}

static long wrkstart;  /* the thread has been made */
static long timerrun;  /* the timer that watches it is going */
static long wrkdone;   /* it finished, and nobody has noticed yet */
static long wrkrelist; /* this fetch is to ask what folders there are */
static long wrkcount;  /* and this one is only to count the store */

/* get one line back, without its line ending */
static int imline(char* buf, long bn)

{

    if (!fgets(buf, bn, imap)) {

        /* Nothing came, and something should have: the server has gone
           quiet or the connection has died under us. Worth telling apart
           from a refusal -- a refused login is a password to go and look
           at, and silence is not. */
        netquiet = TRUE;
        if (diag) fprintf(stderr, "! no answer from %s\n", imapsrv);

        return (FALSE);

    }
    trim(buf);
    if (diag) fprintf(stderr, "< %s\n", buf);

    return (TRUE);

}

/* Is there a literal at the end of this line, and how long? A literal is
   the count in braces, last thing on the line. */
static long literalof(const char* line)

{

    const char* p = strrchr(line, '{');

    if (!p || !strchr(p, '}')) return (-1);
    if (strchr(p, '}')[1]) return (-1); /* the brace is not last */

    return (atol(p+1));

}

/* read exactly n bytes, which is what a literal is */
static char* imliteral(long n)

{

    char* buf = getmem(n+1);
    long  got = 0;

    while (got < n) {

        long r = fread(buf+got, 1, n-got, imap);

        if (r <= 0) break;
        got += r;

    }
    buf[got] = 0;

    return (buf);

}

/* what the server said to the last command, for when it said no */
static char imanswer[MAXLINE];

/* Read to the answer to the command with this tag, handing every
   untagged line to the caller. Gives TRUE if the server said OK. */
static int imwait(const char* tag, void (*line)(const char*))

{

    char line1[MAXLINE];
    long tl = strlen(tag);

    imanswer[0] = 0;
    while (imline(line1, sizeof(line1))) {

        if (!strncmp(line1, tag, tl) && line1[tl] == ' ') {

            const char* r = line1+tl+1;

            /* Keep it. A server that refuses a command usually says why,
               and what it says is worth more to the user than anything
               this program could make up: Gmail answers a login with the
               wrong sort of password by naming the right sort. */
            copystr(imanswer, r, MAXLINE);

            return (!strncasecmp(r, "OK", 2));

        }
        if (line) (*line)(line1);

    }
    copystr(imanswer, "The server stopped answering.", MAXLINE);

    return (FALSE); /* the connection went away */

}

/* the folder list, gathered from the LIST replies */
static long listsrv = -1; /* the server whose folders are arriving */

static void listline(const char* line)

{

    const char* p;
    const char* q;
    char        name[MAXSTR];
    long        o = 0;
    foldrec*    f;

    if (strncmp(line, "* LIST", 6)) return;
    /* A LIST reply is the flags in parentheses, then the delimiter, then
       the name:

           * LIST (\HasNoChildren) "/" "INBOX"      as Gmail writes it
           * LIST (\HasChildren) "." INBOX          as Dovecot writes it

       Either part may be quoted or not, and taking the text between the
       last pair of quotes -- which is what this did -- reads Dovecot's
       delimiter as the name. Every folder then came back called "." and
       not one of them could be opened, which is what an account on
       shared hosting looked like: logged in, folders listed, nothing
       fetched. So it is read in order: past the flags, past the
       delimiter, and what is left is the name. */
    p = line+6;
    while (*p == ' ') p++;
    if (*p == '(') { while (*p && *p != ')') p++; if (*p) p++; } /* flags */
    while (*p == ' ') p++;
    if (*p == '"') { /* the delimiter, quoted */

        p++;
        while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
        if (*p) p++;

    } else while (*p && *p != ' ') p++; /* or NIL, unquoted */
    while (*p == ' ') p++;
    if (*p == '"') { /* the name, quoted */

        p++;
        while (*p && *p != '"' && o < MAXSTR-1) {

            if (*p == '\\' && p[1]) p++; /* what is escaped is literal */
            name[o++] = *p++;

        }

    } else while (*p && o < MAXSTR-1) name[o++] = *p++; /* or bare */
    name[o] = 0;
    trim(name);
    (void)q;
    if (!o || foldct >= MAXFOLDER) return;
    f = &folders[foldct++];
    copystr(f->name, name, MAXSTR);
    /* the name to show: Gmail puts its own folders under [Gmail]/, which
       is a fact about Gmail and not something the reader needs to see */
    if (!strncmp(name, "[Gmail]/", 8)) copystr(f->show, name+8, MAXSTR);
    else copystr(f->show, name, MAXSTR);
    /* A folder marked \\Noselect is a place to hang other folders under
       and holds no messages -- Gmail's [Gmail] is one. It is not a
       folder to a reader, so it is not listed as one. */
    if (strstr(line, "\\Noselect")) { foldct--; return; }
    f->noselect = FALSE;
    f->local = FALSE;
    f->srv = listsrv; /* the server whose list this is */
    /* The file it goes in, named for its server as well as itself: two
       servers may each have an INBOX, and they are not the same mail. */
    {

        char fn[MAXSTR/2];
        long i;

        char  dn[MAXSTR];
        char  sn[MAXSTR*2+8];
        FILE* sf;

        (void)i;
        /* Named for what it is called rather than for the path the
           server files it under: inside the account's own directory the
           [Gmail]/ in front of a Gmail folder says nothing that the
           directory has not said already. */
        safename(f->show, fn, sizeof(fn));
        srvdir(listsrv, dn, sizeof(dn));
        snprintf(f->file, sizeof(f->file), "%.400s/%.150s.mbox", dn, fn);
        /* Unless another folder has that name already. Two folders
           whose shown names come out the same would otherwise share one
           mailbox and merge their mail, which is worse than an ugly
           name, so the second keeps its full one. */
        snprintf(sn, sizeof(sn), "%s.state", f->file);
        sf = fopen(sn, "r");
        if (sf) {

            char real[MAXSTR];
            long v, u;

            if (fscanf(sf, "%ld %ld %499[^\n]", &v, &u, real) == 3) {

                trim(real);
                if (strcmp(real, name)) { /* somebody else's */

                    safename(name, fn, sizeof(fn));
                    snprintf(f->file, sizeof(f->file), "%.400s/%.150s.mbox",
                             dn, fn);

                }

            }
            fclose(sf);

        }

    }

}

/* The folders that are already in the store, for when the server cannot
   be reached. The whole point of keeping the mail in files is that it can
   be read without a server, so a program that shows nothing until it has
   connected has thrown that away. The name is recovered from the file
   name, which is the folder name with the awkward characters replaced,
   so it comes back readable if not always exact. */
/* Read one directory of the store into the folder list: an account's,
   or the one holding what is ours. The folders of a directory are its
   mailboxes, and each keeps its real name in the state file beside it,
   since a file name has had the awkward characters taken out of it. */
static void storefolders(long srv)

{

    ami_filptr lp = NULL;
    ami_filptr fp;
    char       dn[MAXSTR];
    char       what[MAXSTR*2];

    srvdir(srv, dn, sizeof(dn));
    snprintf(what, sizeof(what), "%.400s/*.mbox", dn);
    ami_list(what, &lp);
    for (fp = lp; fp && foldct < MAXFOLDER; fp = fp->next) {

        foldrec* f;
        char     nm[MAXSTR/2];
        long     n;

        copystr(nm, fp->name, sizeof(nm));
        n = strlen(nm);
        if (n > 5 && !strcmp(nm+n-5, ".mbox")) nm[n-5] = 0; else continue;
        f = &folders[foldct++];
        copystr(f->name, nm, MAXSTR);
        copystr(f->show, nm, MAXSTR);
        { /* the file name had its spaces taken out; put them back for
             the reader, until a fetch writes the real name beside it */

            long k;

            for (k = 0; f->show[k]; k++) if (f->show[k] == '_')
                f->show[k] = ' ';

        }
        snprintf(f->file, sizeof(f->file), "%.400s/%.150s.mbox", dn, nm);
        f->noselect = FALSE;
        f->local = srv < 0;
        f->srv = srv;
        { /* the real name, if the state file beside it kept one */

            char  sn[MAXSTR*2+8];
            char  real[MAXSTR];
            long  v, u, u2;
            FILE* sf;

            snprintf(sn, sizeof(sn), "%s.state", f->file);
            sf = fopen(sn, "r");
            if (sf) {

                if (fscanf(sf, "%ld %ld %ld %499[^\n]", &v, &u, &u2,
                           real) == 4 ||
                    (rewind(sf), fscanf(sf, "%ld %ld %499[^\n]",
                                        &v, &u, real) == 3)) {

                    trim(real);
                    copystr(f->name, real, MAXSTR);
                    if (!strncmp(real, "[Gmail]/", 8))
                        copystr(f->show, real+8, MAXSTR);
                    else copystr(f->show, real, MAXSTR);

                }
                fclose(sf);

            }

        }

    }

}

/* every account's folders, then the local ones, which is the order the
   pane shows them in */
static void storeall(void)

{

    long i;

    for (i = 0; i < srvct; i++) storefolders(i);
    storefolders(-1);

}

/* connect and log in */
static int imapopen(void)

{

    unsigned long addr;
    char          tag[20];
    char          line[MAXLINE];

    if (imap) return (TRUE); /* already there */
    if (!reachable(imapsrv, imapport, 10)) {

        char msg[MAXSTR*2];

        snprintf(msg, sizeof(msg), "Cannot reach %s on port %ld.", imapsrv,
                 imapport);
        fail(msg);

        return (FALSE);

    }
    ami_addrnet(imapsrv, &addr);
    imap = ami_opennet(addr, imapport, TRUE);
    if (!imap) { fail("Cannot reach the mail server"); return (FALSE); }
    { /* a read that never comes back must not stop the program

         A server that stops answering -- and one will, over tens of
         thousands of messages -- leaves a read waiting for data that is
         never sent. Every read here is on the same thread as the
         display, so the whole program stops with it: the window does
         not repaint and the mouse does nothing, which looks like a
         crash and is not. A timeout turns that into an error the
         program can act on. */

        struct timeval tv;
        int            fd = fileno(imap);
        int            on = 1;

        tv.tv_sec = NETWAIT;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        /* and let the system notice a connection that has gone away
           rather than waiting on one that will never speak again */
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

    }
    if (!imline(line, sizeof(line))) { /* the greeting */

        fail("The mail server said nothing");
        fclose(imap);
        imap = NULL;

        return (FALSE);

    }
    /* The password is sent in the clear inside the TLS connection, which
       is what LOGIN is and what every mail program does. It is never
       written to the diagnostic. */
    if (diag) fprintf(stderr, "> a%03ld LOGIN %s <password>\n",
                      imaptag+1, username);
    {

        long sav = diag;

        diag = FALSE;
        netquiet = FALSE;
        imsend(tag, sizeof(tag), "LOGIN \"%s\" \"%s\"", username, password);
        diag = sav;

    }
    if (!imwait(tag, NULL)) {

        char msg[MAXSTR*3];
        char said[MAXSTR];

        /* the server's own words, with the NO or BAD and any bracketed
           code taken off the front, since those mean nothing to a reader */
        copystr(said, imanswer, MAXSTR);
        {

            char* p = said;

            if (!strncasecmp(p, "NO ", 3)) p += 3;
            else if (!strncasecmp(p, "BAD ", 4)) p += 4;
            if (*p == '[') { while (*p && *p != ']') p++; if (*p) p++; }
            while (*p == ' ') p++;
            memmove(said, p, strlen(p)+1);

        }
        if (netquiet) /* it did not refuse us, it said nothing at all */
            snprintf(msg, sizeof(msg),
                     "%s stopped answering during the login.\n"
                     "It will be left alone for a while and tried again.",
                     imapsrv);
        else snprintf(msg, sizeof(msg),
                      "%s would not accept the login.\n%s", imapsrv, said);
        fail(msg);
        fclose(imap);
        imap = NULL;

        return (FALSE);

    }

    return (TRUE);

}

static void imapclose(void)

{

    char tag[20];

    if (!imap) return;
    imsend(tag, sizeof(tag), "LOGOUT");
    imwait(tag, NULL);
    fclose(imap);
    imap = NULL;

}

/* ask the server what folders there are */
static int getfolders(long srv)

{

    char tag[20];

    useserver(srv);
    listsrv = srv;
    if (!imapopen()) return (FALSE);
    imsend(tag, sizeof(tag), "LIST \"\" \"*\"");
    if (!imwait(tag, listline)) { fail("The server would not list the "
                                       "folders"); return (FALSE); }

    return (TRUE);

}

/* what the last fetch of this folder reached */
/* What a folder has been read of: the stretch of uids already here,
   from the oldest taken to the newest. Only the newest was remembered
   before, with everything below it assumed present -- which is true
   while a fetch always takes the newest so many and works upward, and
   false the moment somebody asks for more of the folder than they asked
   for last time. Those older messages are all below the mark, so every
   one of them was skipped and raising the limit did nothing at all. */
static void readstate(long fold, long* validity, long* lowuid,
                      long* lastuid)

{

    char  fn[MAXSTR*2+8];
    FILE* f;
    char  line[MAXSTR];
    long  a, b, c;

    *validity = 0;
    *lowuid = 0;
    *lastuid = 0;
    snprintf(fn, sizeof(fn), "%s.state", folders[fold].file);
    f = fopen(fn, "r");
    if (!f) return;
    if (fgets(line, sizeof(line), f)) {

        if (sscanf(line, "%ld %ld %ld", &a, &b, &c) == 3 && c >= b) {

            /* validity, the oldest taken, the newest */
            *validity = a;
            *lowuid = b;
            *lastuid = c;

        } else if (sscanf(line, "%ld %ld", &a, &b) == 2) {

            /* the older form, which knew only the newest. Nothing can be
               said about what is below it, so nothing is assumed: the
               stretch is empty and an older message is fetched again
               rather than passed over. The digests will know it if it
               is already here. */
            *validity = a;
            *lowuid = 0;
            *lastuid = 0;
            (void)b;

        }

    }
    fclose(f);

}

static void writestate(long fold, long validity, long lowuid, long lastuid)

{

    char  fn[MAXSTR*2+8];
    FILE* f;

    snprintf(fn, sizeof(fn), "%s.state", folders[fold].file);
    f = fopen(fn, "w");
    if (!f) return;
    /* The name goes down with it. The file it is kept in has the
       awkward characters taken out of the name, which cannot be undone,
       so without this a folder read back from the store on its own
       shows as _Gmail__Sent_Mail rather than Sent Mail. */
    fprintf(f, "%ld %ld %ld %s\n", validity, lowuid, lastuid,
            folders[fold].name);
    fclose(f);

}

/* what EXAMINE says about the folder, gathered from its untagged lines */
static long exists, uidvalidity;

static void examline(const char* line)

{

    const char* p;
    char        what[40];
    long        n;

    /* The word is checked, not assumed. sscanf gives the number of
       assignments it made before it gave up, so "* 0 RECENT" against
       "* %ld EXISTS" assigns the 0 and answers 1 just as a real EXISTS
       does -- and RECENT comes straight after EXISTS, so the count was
       being thrown away and every folder looked empty. */
    if (sscanf(line, "* %ld %39s", &n, what) == 2 &&
        !strcasecmp(what, "EXISTS")) exists = n;
    p = strstr(line, "[UIDVALIDITY ");
    if (p) uidvalidity = atol(p+13);

}

/* the uids of the messages asked for, gathered from the FETCH replies */
static long* uidlist;
static long  uidct;
static long  uidmax;

static void uidline(const char* line)

{

    const char* p = strstr(line, "UID ");

    if (strncmp(line, "* ", 2) || !strstr(line, "FETCH") || !p) return;
    if (uidct >= uidmax) {

        uidmax = uidmax? uidmax*2: 256;
        uidlist = realloc(uidlist, uidmax*sizeof(long));
        if (!uidlist) { fail("Out of memory"); exit(1); }

    }
    uidlist[uidct++] = atol(p+4);

}

/*******************************************************************************

Talking to the SMTP server

Nothing is sent yet: this program reads mail. What is here is the part
that proves the account could send -- connect, say hello, start TLS if
the port is the one that needs it, log in, and say goodbye -- so that the
sending half has somewhere to stand when it is written.

*******************************************************************************/

/* read a reply, which may be several lines, and give its code */
static long smtpresp(FILE* f)

{

    char line[MAXLINE];
    long code = 0;

    while (fgets(line, sizeof(line), f)) {

        trim(line);
        if (diag) fprintf(stderr, "< %s\n", line);
        code = atol(line);
        if (strlen(line) < 4 || line[3] != '-') break; /* the last line */

    }

    return (code);

}

static void smtpsend(FILE* f, const char* fmt, ...)

{

    va_list ap;
    char    cmd[MAXLINE];

    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    if (diag) fprintf(stderr, "> %s\n", cmd);
    fprintf(f, "%s\r\n", cmd);
    fflush(f);

}

/* base64 encode, which is how a password is given to SMTP */
static void b64enc(const char* s, long n, char* d, long dn)

{

    static const char* tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz0123456789+/";
    long i, o = 0;

    for (i = 0; i < n; i += 3) {

        long v = (unsigned char)s[i];
        long r = n-i;

        v = v<<8 | (r > 1? (unsigned char)s[i+1]: 0);
        v = v<<8 | (r > 2? (unsigned char)s[i+2]: 0);
        if (o < dn-4) {

            d[o++] = tab[(v>>18)&0x3f];
            d[o++] = tab[(v>>12)&0x3f];
            d[o++] = r > 1? tab[(v>>6)&0x3f]: '=';
            d[o++] = r > 2? tab[v&0x3f]: '=';

        }

    }
    d[o] = 0;

}

/* Prove the account can send, without sending anything. */
/*******************************************************************************

Sending

The one thing this program does that reaches out rather than reads. It
speaks just enough SMTP: a connection, a greeting, a login, who it is
from, who it is to, and the message itself.

A copy of everything sent is put in a local folder called Sent. That is
the same rule the rest of the program is built to -- what is here is
here, in a file, not on somebody's server -- and it means a sent message
can be read, searched and counted like any other.

*******************************************************************************/

/* Split a list of addresses on commas, giving each in turn. Quoted
   display names can hold commas, so the split ignores anything inside
   quotes or angle brackets. */
static const char* nextaddr(const char* p, char* d, long dl)

{

    long i = 0;
    int  quote = FALSE;
    int  angle = FALSE;

    if (!p || !*p) { if (dl) *d = 0; return (NULL); }
    while (*p == ' ' || *p == ',') p++;
    while (*p && (quote || angle || *p != ',')) {

        if (*p == '"') quote = !quote;
        if (*p == '<') angle = TRUE;
        if (*p == '>') angle = FALSE;
        if (i < dl-1) d[i++] = *p;
        p++;

    }
    if (dl) d[i] = 0;
    trim(d);

    return (*p? p+1: NULL);

}

/* Something that will do as a message id: the time, a count within the
   second, and the sending account's domain. */
static void makemsgid(char* d, long dl)

{

    static long ct;
    const char* at = sendsrv >= 0 && sendsrv < srvct?
                     strchr(servers[sendsrv].user, '@'): NULL;

    snprintf(d, dl, "<%ld.%ld.amimail@%s>", (long)time(NULL), ++ct,
             at? at+1: "localhost");

}

/* the date, as a message header wants it */
static void makedate(char* d, long dl)

{

    time_t    t = time(NULL);
    struct tm lt;

    localtime_r(&t, &lt);
    strftime(d, dl, "%a, %d %b %Y %H:%M:%S %z", &lt);

}

/* Send one message. Gives an empty string if it went, and what went
   wrong if it did not. The whole of it happens on the fetch thread, so
   that a server taking its time over a message does not stop the
   display. */
static void sendmail(const char* to, const char* cc, const char* subject,
                     const char* body, const char* inreply,
                     const char* refs, char* err, long errl)

{

    unsigned long addr;
    FILE*  f;
    char   b64[MAXSTR*2];
    char   one[MAXSTR];
    char   ssrv[MAXSTR];  /* the sending account's own details, taken
                             here rather than from whatever the fetch
                             has open: the two run at once */
    char   suser[MAXSTR];
    char   spass[MAXSTR];
    long   sport;
    char   msgid[MAXSTR];
    char   date[MAXSTR];
    char*  msg;
    long   msgl;
    long   o = 0;
    long   code;
    const char* p;
    long   sent = 0;

    *err = 0;
    if (sendsrv < 0 || sendsrv >= srvct) sendsrv = 0;
    dlock();
    copystr(ssrv, servers[sendsrv].smtp, MAXSTR);
    sport = servers[sendsrv].smtpport;
    copystr(suser, servers[sendsrv].user, MAXSTR);
    copystr(spass, servers[sendsrv].pass, MAXSTR);
    dunlock();
    if (!*ssrv || !*suser) {

        copystr(err, "No account is set to send from. Config/Servers has a "
                "box for it, and a sending server to fill in.", errl);

        return;

    }
    if (!reachable(ssrv, sport, 10)) {

        snprintf(err, errl, "Cannot reach %s on port %ld.\n"
                 "Check the sending server and its port in Config/Servers. "
                 "Gmail sends on 465.", ssrv, sport);

        return;

    }
    ami_addrnet(ssrv, &addr);
    /* 465 is TLS from the first byte; 587 begins in the clear and turns
       it on with STARTTLS, which this cannot do through the library's
       socket, so 465 is the one to use */
    f = ami_opennet(addr, sport, TRUE);
    if (!f) { snprintf(err, errl, "Cannot reach %s.", ssrv); return; }
    {

        struct timeval tv;
        int            fd = fileno(f);

        tv.tv_sec = NETWAIT;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    }
    if (smtpresp(f)/100 != 2)
        { copystr(err, "The sending server did not greet us.", errl);
          fclose(f); return; }
    smtpsend(f, "EHLO localhost");
    if (smtpresp(f)/100 != 2)
        { copystr(err, "The sending server refused EHLO.", errl);
          fclose(f); return; }
    smtpsend(f, "AUTH LOGIN");
    if (smtpresp(f) != 334)
        { copystr(err, "The sending server would not start a login.", errl);
          fclose(f); return; }
    b64enc(suser, strlen(suser), b64, sizeof(b64));
    smtpsend(f, "%s", b64);
    if (smtpresp(f) != 334)
        { copystr(err, "The sending server refused the user name.", errl);
          fclose(f); return; }
    b64enc(spass, strlen(spass), b64, sizeof(b64));
    if (diag) fprintf(stderr, "> <password>\n");
    fprintf(f, "%s\r\n", b64);
    fflush(f);
    if (smtpresp(f)/100 != 2) {

        copystr(err, "The sending server would not accept the login.\n"
                "For Gmail this must be an application password.", errl);
        fclose(f);

        return;

    }
    smtpsend(f, "MAIL FROM:<%s>", suser);
    if (smtpresp(f)/100 != 2)
        { copystr(err, "The sending server would not take the sender.", errl);
          fclose(f); return; }
    /* everybody it is going to, whether they are shown or not */
    for (p = to; (p = nextaddr(p, one, sizeof(one))) || *one; ) {

        char just[MAXSTR];

        if (*one) {

            addrof(one, just, sizeof(just));
            smtpsend(f, "RCPT TO:<%s>", *just? just: one);
            if (smtpresp(f)/100 == 2) sent++;

        }
        if (!p) break;

    }
    for (p = cc; p && (p = nextaddr(p, one, sizeof(one))) || (cc && *one); ) {

        char just[MAXSTR];

        if (*one) {

            addrof(one, just, sizeof(just));
            smtpsend(f, "RCPT TO:<%s>", *just? just: one);
            if (smtpresp(f)/100 == 2) sent++;

        }
        if (!p) break;

    }
    if (!sent) {

        copystr(err, "Nobody the message is addressed to was accepted.", errl);
        smtpsend(f, "QUIT");
        smtpresp(f);
        fclose(f);

        return;

    }
    smtpsend(f, "DATA");
    if (smtpresp(f) != 354)
        { copystr(err, "The sending server would not take the message.", errl);
          fclose(f); return; }
    makemsgid(msgid, sizeof(msgid));
    makedate(date, sizeof(date));
    /* The message itself, headers and all, kept so that the same bytes
       can be put in the Sent folder as went down the wire. */
    msgl = strlen(body)+MAXSTR*8;
    msg = getmem(msgl);
    o += snprintf(msg+o, msgl-o, "From: %s\r\n", suser);
    o += snprintf(msg+o, msgl-o, "To: %s\r\n", to);
    if (cc && *cc) o += snprintf(msg+o, msgl-o, "Cc: %s\r\n", cc);
    o += snprintf(msg+o, msgl-o, "Subject: %s\r\n", subject);
    o += snprintf(msg+o, msgl-o, "Date: %s\r\n", date);
    o += snprintf(msg+o, msgl-o, "Message-ID: %s\r\n", msgid);
    if (inreply && *inreply) {

        o += snprintf(msg+o, msgl-o, "In-Reply-To: %s\r\n", inreply);
        o += snprintf(msg+o, msgl-o, "References: %s%s%s\r\n",
                      refs && *refs? refs: "", refs && *refs? " ": "",
                      inreply);

    }
    o += snprintf(msg+o, msgl-o, "MIME-Version: 1.0\r\n");
    o += snprintf(msg+o, msgl-o,
                  "Content-Type: text/plain; charset=UTF-8\r\n");
    o += snprintf(msg+o, msgl-o, "\r\n");
    /* the body, with every line ending as the wire wants it and any
       line of a single dot made safe */
    for (p = body; *p; ) {

        const char* q = p;

        while (*q && *q != '\n') q++;
        if (q-p == 1 && *p == '.') o += snprintf(msg+o, msgl-o, ".");
        if (o+(q-p)+3 < msgl) {

            memcpy(msg+o, p, q-p);
            o += q-p;
            o += snprintf(msg+o, msgl-o, "\r\n");

        }
        p = *q? q+1: q;

    }
    msg[o] = 0;
    fwrite(msg, 1, o, f);
    fprintf(f, "\r\n.\r\n");
    fflush(f);
    code = smtpresp(f);
    smtpsend(f, "QUIT");
    smtpresp(f);
    fclose(f);
    if (code/100 != 2) {

        snprintf(err, errl, "The sending server would not take the message.");
        free(msg);

        return;

    }
    /* And a copy here, which is the point of the whole program: what is
       sent is ours as much as what is received. */
    {

        long fold;
        char hex[DIGLEN];
        long off, stored = 0;

        digestof(msg, o, hex);
        /* The whole of it with the display shut out: making the folder
           adds to the table the display draws from, and the message
           itself grows the array the display's list IS. */
        dlock();
        fold = localfolder("Sent");
        if (fold >= 0) {


            off = mboxwrite(folders[fold].file, msg, o, &stored);
            if (off >= 0) {

                msgrec* m = idxroom(fold);

                fillrec(m, msg, o, stored, off, hex);
                adddigest(fold, folders[fold].idxct-1);
                idxappend(fold, m);
                folders[fold].msgs = folders[fold].idxct;
                folders[fold].dirty = TRUE;
                useidx();

            }

        }
        dunlock();
        wrkfolds = TRUE;
        wrklist = TRUE;

    }
    free(msg);

}

static void smtpcheck(void)

{

    unsigned long addr;
    FILE* f;
    char  b64[MAXSTR*2];
    char  msg[MAXSTR*2];
    long  code;

    if (!reachable(smtpsrv, smtpport, 10)) {

        char msg[MAXSTR*2];

        snprintf(msg, sizeof(msg), "Cannot reach %s on port %ld.\n"
                 "Gmail sends on 465.", smtpsrv, smtpport);
        fail(msg);

        return;

    }
    ami_addrnet(smtpsrv, &addr);
    /* 465 is TLS from the first byte; 587 begins in the clear and turns
       it on with STARTTLS, which this cannot do through the library's
       socket, so 465 is the one to use */
    f = ami_opennet(addr, smtpport, TRUE);
    if (!f) { fail("Cannot reach the sending server"); return; }
    if (smtpresp(f)/100 != 2)
        { fail("The sending server did not greet us"); fclose(f); return; }
    smtpsend(f, "EHLO localhost");
    if (smtpresp(f)/100 != 2)
        { fail("The sending server refused EHLO"); fclose(f); return; }
    smtpsend(f, "AUTH LOGIN");
    if (smtpresp(f) != 334) {

        fail("The sending server would not start a login.\n"
             "On port 587 the login begins in the clear and needs "
             "STARTTLS; try port 465 instead.");
        fclose(f);

        return;

    }
    b64enc(username, strlen(username), b64, sizeof(b64));
    smtpsend(f, "%s", b64);
    if (smtpresp(f) != 334)
        { fail("The sending server refused the user name"); fclose(f);
          return; }
    b64enc(password, strlen(password), b64, sizeof(b64));
    if (diag) fprintf(stderr, "> <password>\n");
    fprintf(f, "%s\r\n", b64);
    fflush(f);
    code = smtpresp(f);
    smtpsend(f, "QUIT");
    smtpresp(f);
    fclose(f);
    if (code/100 == 2)
        snprintf(msg, sizeof(msg),
                 "Mail could be sent from this account.\n"
                 "%s accepted the login on port %ld. Nothing was sent.",
                 smtpsrv, smtpport);
    else
        snprintf(msg, sizeof(msg),
                 "%s would not accept the login on port %ld.\n"
                 "For Gmail this must be an application password.",
                 smtpsrv, smtpport);
    fail(msg);

}

/*******************************************************************************

The display

Three windows. The folders are a pane down the left, the messages a pane
on the right, and a message opened to be read gets a window of its own.
They are all serviced from the one event loop: every event names the
window it came from, so there is nothing to arrange.

*******************************************************************************/

/* The scroll bar runs from 0 to LONG_MAX. The arithmetic is integer on
   purpose: LONG_MAX does not survive a trip through a double -- it
   rounds up to 2^63 and comes back negative -- and the bar answers an
   out of range position with an error. */
static long fullscale(long num, long den)

{

    if (den < 1 || num <= 0) return (0);
    if (num >= den) return (LONG_MAX);

    return (LONG_MAX/den*num); /* num < den, so this cannot overflow */

}

/* Set while a bar's own position event is being answered. The bar has
   been echoed where the user put it; setting it again from the view
   would drag the thumb back to the nearest whole line, which on a bar
   with little travel is most of the way back where it started. */
static int fromdrag;

/* and back, from a bar position to a line number */
static long scaleback(long pos, long travel)

{

    if (travel < 1 || pos <= 0) return (0);
    if (pos >= LONG_MAX) return (travel);

    return ((long)((double)travel*((double)pos/LONG_MAX)+0.5));

}

/* There is no status line any more: everything it said is said in the
   window itself -- the folder counts climb while a fetch runs, the list
   says what an empty folder is, and the forms speak for themselves. The
   calls remain as markers of where a quieter program once spoke. */
static long fitchars(FILE* f, char* s, long w);
static void commas(long n, char* s, long sl);
static void divider(FILE* f, long x1, long y1, long x2, long y2);
static long progw; /* how wide the bar is */
static long progh; /* and how tall */

/*******************************************************************************

Writing a message

A window with the three fields a message needs and a body to type in.
The fields are edit boxes, which the library gives us; the body is not,
since there is no widget for more than one line of text, so it is kept
and drawn here.

The body is a list of lines and a place in one of them, which is the
model the editor in this kit uses and the simplest one that behaves the
way a person expects. Everything that types into it comes to the window
as an event: a click on an edit box takes the keys away, and a click on
the body gives them back, which is what the library does with focus and
what makes the two kinds of field live together.

*******************************************************************************/

#define CMPMAX  20000 /* the longest line worth calling a line */

static FILE*  cmpwf;            /* the window, when it is open */
static char** cmpline;          /* the body, a line at a time */
static long   cmpct;            /* how many lines */
static long   cmpmax;           /* room for how many */
static long   cmpcl, cmpcc;     /* the caret: which line, which column */
static long   cmptop;           /* the first line shown */
static long   cmprows;          /* how many fit */
static long   cmpy0;            /* where the body starts down the window */
static long   cmpsbw;           /* the bar beside it */
static long   cmpfocus;         /* the body has the keys */
static long   cmpmx, cmpmy;     /* where the mouse is in the window */
static char   cmpinreply[MAXSTR]; /* what this answers, if it answers */
static char   cmprefs[MAXSTR*2];

/* The message on show in the reader, in the pieces a reply is made of.
   Kept when it is opened, since the reply is written from the headers
   and the text rather than from the file again. */
static char   redfrom[MAXSTR];
static char   redto[MAXSTR];
static char   redcc[MAXSTR];
static char   redsubj[MAXSTR];
static char   reddate[MAXSTR];
static char   redid[MAXSTR];
static char   redrefs[MAXSTR*2];
static char*  redtext;

/* room for one more line */
static void cmproom(long n)

{

    if (n < cmpmax) return;
    cmpmax = cmpmax? cmpmax*2: 64;
    while (cmpmax <= n) cmpmax *= 2;
    cmpline = realloc(cmpline, cmpmax*sizeof(char*));
    if (!cmpline) { fail("Out of memory"); exit(1); }

}

static void cmpclear(void)

{

    long i;

    for (i = 0; i < cmpct; i++) free(cmpline[i]);
    cmpct = 0;
    cmpcl = 0;
    cmpcc = 0;
    cmptop = 0;

}

/* put a line in, at a place */
static void cmpput(long at, const char* text)

{

    long i;

    cmproom(cmpct+1);
    for (i = cmpct; i > at; i--) cmpline[i] = cmpline[i-1];
    cmpline[at] = getmem(strlen(text)+1);
    strcpy(cmpline[at], text);
    cmpct++;

}

static void cmptake(long at)

{

    long i;

    if (at < 0 || at >= cmpct) return;
    free(cmpline[at]);
    for (i = at; i < cmpct-1; i++) cmpline[i] = cmpline[i+1];
    cmpct--;

}

/* the whole of it, as one string, which is what sending wants */
static char* cmptext(void)

{

    long  n = 1;
    long  i;
    char* t;

    for (i = 0; i < cmpct; i++) n += strlen(cmpline[i])+1;
    t = getmem(n);
    *t = 0;
    for (i = 0; i < cmpct; i++) { strcat(t, cmpline[i]); strcat(t, "\n"); }

    return (t);

}

/*******************************************************************************

The compose window: laying it out, drawing it, and answering it

*******************************************************************************/

static void cmpclose(void)

{

    if (!cmpwf) return;
    ami_killwidget(cmpwf, CMPTO);
    ami_killwidget(cmpwf, CMPCC);
    ami_killwidget(cmpwf, CMPSUB);
    ami_killwidget(cmpwf, CMPSEND);
    ami_killwidget(cmpwf, CMPCAN);
    ami_killwidget(cmpwf, CMPSB);
    fclose(cmpwf); /* which is how a window is closed here */
    cmpwf = NULL;
    cmpclear();

}

/* how many lines of the body are on show */
static long cmpvis(void)

{

    long n = (ami_maxyg(cmpwf)-cmpy0-8)/chrh;

    return (n < 1? 1: n);

}

static void cmpbar(void)

{

    if (cmpct > cmpvis())
        ami_scrollsiz(cmpwf, CMPSB, fullscale(cmpvis(), cmpct));
    else ami_scrollsiz(cmpwf, CMPSB, INT_MAX);
    ami_scrollpos(cmpwf, CMPSB, fullscale(cmptop, cmpct-1));

}

/* the body, and the caret in it */
static void cmpdraw(void)

{

    long i;
    long y;

    if (!cmpwf) return;
    ami_fcolor(cmpwf, ami_white);
    ami_frect(cmpwf, 1, cmpy0, ami_maxxg(cmpwf)-cmpsbw-2, ami_maxyg(cmpwf));
    ami_fcolor(cmpwf, ami_black);
    cmprows = cmpvis();
    if (cmpcl < cmptop) cmptop = cmpcl;
    if (cmpcl >= cmptop+cmprows) cmptop = cmpcl-cmprows+1;
    if (cmptop < 0) cmptop = 0;
    y = cmpy0+4;
    for (i = cmptop; i < cmpct && i < cmptop+cmprows; i++) {

        ami_cursorg(cmpwf, 8, y);
        fprintf(cmpwf, "%s", cmpline[i]);
        if (i == cmpcl && cmpfocus) { /* the caret, where the typing goes */

            char  upto[CMPMAX];
            long  x;

            copystr(upto, cmpline[i], sizeof(upto));
            if (cmpcc < (long)strlen(upto)) upto[cmpcc] = 0;
            x = 8+ami_strsiz(cmpwf, upto);
            ami_fcolorc(cmpwf, rgb(200), rgb(40), rgb(40));
            ami_linewidth(cmpwf, 2);
            ami_line(cmpwf, x, y, x, y+chrh);
            ami_linewidth(cmpwf, 1);
            ami_fcolor(cmpwf, ami_black);

        }
        y += chrh;

    }
    cmpbar();

}

static void cmplay(void)

{

    long chrw = ami_strsiz(cmpwf, "0");
    long labw = ami_strsiz(cmpwf, "Subject  ");
    long ew, eh, bw, bh;
    long y;
    static const char* lab[] = { "To", "Cc", "Subject" };
    static const long  wid[] = { CMPTO, CMPCC, CMPSUB };
    long i;

    ami_editboxsizg(cmpwf, "0", &ew, &eh);
    ami_buttonsizg(cmpwf, "Cancel", &bw, &bh);
    fprintf(cmpwf, "\f");
    ami_fcolor(cmpwf, ami_black);
    y = chrh/2;
    for (i = 0; i < 3; i++) {

        ami_cursorg(cmpwf, chrw, y+(eh-chrh)/2);
        fprintf(cmpwf, "%s", lab[i]);
        ami_poswidgetg(cmpwf, wid[i], chrw+labw, y);
        ami_sizwidgetg(cmpwf, wid[i], ami_maxxg(cmpwf)-labw-chrw*2, eh);
        y += eh+chrh/4;

    }
    y += chrh/4;
    ami_poswidgetg(cmpwf, CMPSEND, chrw+labw, y);
    ami_sizwidgetg(cmpwf, CMPSEND, bw, bh);
    ami_poswidgetg(cmpwf, CMPCAN, chrw+labw+bw+chrw*2, y);
    ami_sizwidgetg(cmpwf, CMPCAN, bw, bh);
    y += bh+chrh/2;
    /* the line that says the message begins here */
    divider(cmpwf, 1, y, ami_maxxg(cmpwf), y);
    cmpy0 = y+2;
    ami_poswidgetg(cmpwf, CMPSB, ami_maxxg(cmpwf)-cmpsbw, cmpy0);
    ami_sizwidgetg(cmpwf, CMPSB, cmpsbw, ami_maxyg(cmpwf)-cmpy0);
    cmpdraw();

}

/* Open it, with whatever is already known filled in: a reply comes with
   everything but the words. */
static void cmpopen(const char* to, const char* cc, const char* subject,
                    const char* body, const char* inreply, const char* refs)

{

    long wx, wy;
    const char* p;

    if (cmpwf) { ami_front(cmpwf); return; } /* one at a time */
    ami_openwin(&stdin, &cmpwf, NULL, CMPWIN);
    ami_title(cmpwf, "Write a message");
    ami_auto(cmpwf, FALSE);
    ami_curvis(cmpwf, FALSE);
    ami_font(cmpwf, AMI_FONT_SIGN);
    ami_setpoints(cmpwf, 11.0);
    ami_binvis(cmpwf);
    ami_winclientg(cmpwf, ami_strsiz(cmpwf, "0")*90, chrh*34, &wx, &wy,
                   BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(cmpwf, wx, wy);
    {

        long ew, eh, bw, bh;

        ami_editboxsizg(cmpwf, "0", &ew, &eh);
        ami_buttonsizg(cmpwf, "Cancel", &bw, &bh);
        ami_editboxg(cmpwf, 1, 1, 100, 1+eh, CMPTO);
        ami_editboxg(cmpwf, 1, 1, 100, 1+eh, CMPCC);
        ami_editboxg(cmpwf, 1, 1, 100, 1+eh, CMPSUB);
        ami_buttong(cmpwf, 1, 1, 1+bw, 1+bh, "Send", CMPSEND);
        ami_buttong(cmpwf, 1, 1, 1+bw, 1+bh, "Cancel", CMPCAN);
        ami_scrollvertsizg(cmpwf, &cmpsbw, &eh);
        ami_scrollvertg(cmpwf, 1, 1, cmpsbw, chrh*10, CMPSB);

    }
    ami_putwidgettext(cmpwf, CMPTO, (char*)(to? to: ""));
    ami_putwidgettext(cmpwf, CMPCC, (char*)(cc? cc: ""));
    ami_putwidgettext(cmpwf, CMPSUB, (char*)(subject? subject: ""));
    copystr(cmpinreply, inreply? inreply: "", sizeof(cmpinreply));
    copystr(cmprefs, refs? refs: "", sizeof(cmprefs));
    /* the body, broken where its lines break */
    cmpclear();
    for (p = body? body: ""; ; ) {

        const char* q = p;
        char        one[CMPMAX];
        long        n;

        while (*q && *q != '\n') q++;
        n = q-p;
        if (n > CMPMAX-1) n = CMPMAX-1;
        memcpy(one, p, n);
        one[n] = 0;
        while (n && (one[n-1] == '\r')) one[--n] = 0;
        cmpput(cmpct, one);
        if (!*q) break;
        p = q+1;

    }
    if (!cmpct) cmpput(0, "");
    cmpcl = 0;
    cmpcc = 0;
    cmpfocus = TRUE;
    cmplay();

}

/* one character into the line the caret is on */
static void cmpins(char c)

{

    char* l = cmpline[cmpcl];
    long  n = strlen(l);
    char* d;

    if (n+2 > CMPMAX) return;
    d = getmem(n+2);
    memcpy(d, l, cmpcc);
    d[cmpcc] = c;
    strcpy(d+cmpcc+1, l+cmpcc);
    free(cmpline[cmpcl]);
    cmpline[cmpcl] = d;
    cmpcc++;

}

/* the line broken where the caret is */
static void cmpsplit(void)

{

    char* l = cmpline[cmpcl];
    char* rest = getmem(strlen(l+cmpcc)+1);

    strcpy(rest, l+cmpcc);
    l[cmpcc] = 0;
    cmpput(cmpcl+1, rest);
    free(rest);
    cmpcl++;
    cmpcc = 0;

}

/* and joined to the one after it */
static void cmpjoin(long at)

{

    char* a;
    char* b;
    char* d;

    if (at < 0 || at+1 >= cmpct) return;
    a = cmpline[at];
    b = cmpline[at+1];
    d = getmem(strlen(a)+strlen(b)+1);
    strcpy(d, a);
    strcat(d, b);
    free(cmpline[at]);
    cmpline[at] = d;
    cmptake(at+1);

}

/* Everything typed into the body. The fields beside it are edit boxes
   and answer for themselves; this is the part with no widget to do it. */
static void cmpkey(ami_evtrec* er)

{

    long n = (long)strlen(cmpline[cmpcl]);

    switch (er->etype) {

        case ami_etchar:
            if (er->echar >= ' ' && er->echar < 0x7f) cmpins(er->echar);
            break;
        case ami_etenter: cmpsplit(); break;
        case ami_ettab: { long i; for (i = 0; i < 4; i++) cmpins(' '); break; }
        case ami_etdelcb: /* backspace: within the line, or the break above */
            if (cmpcc > 0) {

                char* l = cmpline[cmpcl];

                memmove(l+cmpcc-1, l+cmpcc, strlen(l+cmpcc)+1);
                cmpcc--;

            } else if (cmpcl > 0) {

                cmpcc = strlen(cmpline[cmpcl-1]);
                cmpjoin(cmpcl-1);
                cmpcl--;

            }
            break;
        case ami_etdelcf:
            if (cmpcc < n) {

                char* l = cmpline[cmpcl];

                memmove(l+cmpcc, l+cmpcc+1, strlen(l+cmpcc+1)+1);

            } else cmpjoin(cmpcl);
            break;
        case ami_etdell:
            if (cmpct > 1) { cmptake(cmpcl); if (cmpcl >= cmpct) cmpcl = cmpct-1; }
            else { free(cmpline[0]); cmpline[0] = getmem(1); *cmpline[0] = 0; }
            cmpcc = 0;
            break;
        case ami_etleft:
            if (cmpcc > 0) cmpcc--;
            else if (cmpcl > 0) { cmpcl--; cmpcc = strlen(cmpline[cmpcl]); }
            break;
        case ami_etright:
            if (cmpcc < n) cmpcc++;
            else if (cmpcl < cmpct-1) { cmpcl++; cmpcc = 0; }
            break;
        case ami_etup: if (cmpcl > 0) cmpcl--; break;
        case ami_etdown: if (cmpcl < cmpct-1) cmpcl++; break;
        case ami_etpagu: cmpcl -= cmpvis()-1; if (cmpcl < 0) cmpcl = 0; break;
        case ami_etpagd:
            cmpcl += cmpvis()-1;
            if (cmpcl > cmpct-1) cmpcl = cmpct-1;
            break;
        case ami_ethomel: case ami_ethomes: cmpcc = 0; break;
        case ami_etendl: case ami_etends: cmpcc = strlen(cmpline[cmpcl]); break;
        case ami_ethome: cmpcl = 0; cmpcc = 0; break;
        case ami_etend:
            cmpcl = cmpct-1;
            cmpcc = strlen(cmpline[cmpcl]);
            break;
        default: return;

    }
    /* the caret cannot stand past the end of the line it is on */
    n = (long)strlen(cmpline[cmpcl]);
    if (cmpcc > n) cmpcc = n;
    cmpdraw();

}

/* Send what has been written. The sending itself is network and slow,
   so it is handed to the fetch thread; this only gathers it up. */
static void cmpdosend(void)

{

    char  to[MAXSTR], cc[MAXSTR], sub[MAXSTR];
    char* body;

    ami_getwidgettext(cmpwf, CMPTO, to, sizeof(to));
    ami_getwidgettext(cmpwf, CMPCC, cc, sizeof(cc));
    ami_getwidgettext(cmpwf, CMPSUB, sub, sizeof(sub));
    trim(to);
    if (!*to) { fail("There is nobody to send it to."); return; }
    body = cmptext();
    /* No lock here: this runs while the display is handling an event,
       and the display holds the lock for the whole of that. Taking it
       again is a thread waiting for itself, which is what the library
       calls a deadlock avoided and what it stops the program for. */
    copystr(outto, to, sizeof(outto));
    copystr(outcc, cc, sizeof(outcc));
    copystr(outsub, sub, sizeof(outsub));
    free(outbody);
    outbody = body;
    copystr(outinreply, cmpinreply, sizeof(outinreply));
    copystr(outrefs, cmprefs, sizeof(outrefs));
    sendwant = TRUE;
    kickworker();
    status("Sending...");
    cmpclose();

}

static void cmpevent(ami_evtrec* er)

{

    switch (er->etype) {

        case ami_etterm: cmpclose(); break;
        case ami_etresize:
            ami_sizbufg(cmpwf, er->rszxg, er->rszyg);
            cmplay();
            break;
        case ami_etredraw: cmplay(); break;
        case ami_etbutton:
            if (er->butid == CMPSEND) cmpdosend();
            else if (er->butid == CMPCAN) cmpclose();
            break;
        case ami_etmouba:
            /* A click in the body is what gives the keys back to the
               window after an edit box has had them, and it puts the
               caret where it was clicked. */
            if (er->amoubn == 1 && cmpmy >= cmpy0) {

                long l = cmptop+(cmpmy-cmpy0-4)/chrh;

                cmpfocus = TRUE;
                if (l < 0) l = 0;
                if (l > cmpct-1) l = cmpct-1;
                cmpcl = l;
                { /* the column the click landed nearest */

                    char upto[CMPMAX];
                    long i;

                    copystr(upto, cmpline[cmpcl], sizeof(upto));
                    for (i = 0; upto[i]; i++) {

                        char c = upto[i+1];

                        upto[i+1] = 0;
                        if (8+ami_strsiz(cmpwf, upto) > cmpmx) break;
                        upto[i+1] = c;

                    }
                    cmpcc = i;

                }
                cmpdraw();

            } else if (er->amoubn == 1) cmpfocus = FALSE;
            break;
        case ami_etmoumovg: cmpmx = er->moupxg; cmpmy = er->moupyg; break;
        case ami_etsclull: cmptop--; if (cmptop < 0) cmptop = 0; cmpdraw(); break;
        case ami_etscldrl: cmptop++; cmpdraw(); break;
        case ami_etsclulp: cmptop -= cmpvis()-1; cmpdraw(); break;
        case ami_etscldrp: cmptop += cmpvis()-1; cmpdraw(); break;
        case ami_etsclpos:
            cmptop = scaleback(er->sclpos, cmpct-1);
            cmpdraw();
            break;
        default: cmpkey(er); break;

    }

}

/*******************************************************************************

The banner

A band across the top under the menu: the program's name at the left in
large type, and at the right the picture that goes with it. It is a
window of its own, like the panes, so it is placed and drawn in one
space and answers for its own redrawing.

The picture is a bitmap beside the program, since that is the one form
the library reads. It is loaded once; if it is not found the banner is
the name alone, which is a banner still.

*******************************************************************************/

static FILE* banwf;    /* the banner is a pane, like the others */
static long  banh;     /* how tall it is */
static long  picw;     /* the picture, at the size it was made */
static long  pich;
static long  havepic;

/* Can this server be reached at all?

   The library treats a connection it cannot make as an error, and an
   error stops the program: ami_opennet does not come back to say no. A
   mail program meets servers that are down -- a wrong port in a form, a
   host that has moved, a network that is not there -- and it must go on
   meeting them, so the connection is tried here first with a plain
   socket, and the library is only asked for one that is going to work.

   The try is made without blocking and given a few seconds, since the
   whole point is not to hang on a host that is not answering. */
static int reachable(const char* host, long port, long secs)

{

    struct addrinfo  hints;
    struct addrinfo* res = NULL;
    struct addrinfo* p;
    char             portstr[16];
    int              ok = FALSE;

    if (!host || !*host) return (FALSE);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* which is what the library speaks */
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%ld", port);
    if (getaddrinfo(host, portstr, &hints, &res)) return (FALSE);
    for (p = res; p && !ok; p = p->ai_next) {

        int            sk = socket(p->ai_family, SOCK_STREAM, 0);
        int            fl;
        struct timeval tv;
        fd_set         wr;

        if (sk < 0) continue;
        fl = fcntl(sk, F_GETFL, 0);
        fcntl(sk, F_SETFL, fl|O_NONBLOCK);
        if (!connect(sk, p->ai_addr, p->ai_addrlen)) ok = TRUE;
        else if (errno == EINPROGRESS) {

            FD_ZERO(&wr);
            FD_SET(sk, &wr);
            tv.tv_sec = secs;
            tv.tv_usec = 0;
            if (select(sk+1, NULL, &wr, NULL, &tv) > 0) {

                int       e = 0;
                socklen_t el = sizeof(e);

                if (!getsockopt(sk, SOL_SOCKET, SO_ERROR, &e, &el) && !e)
                    ok = TRUE;

            }

        }
        close(sk);

    }
    freeaddrinfo(res);
    if (!ok && diag) fprintf(stderr, "! cannot reach %s port %ld\n", host, port);

    return (ok);

}

/* Something kept beside the program: the categories, the help, the
   picture. Looked for where the program is, then where the source is,
   so that it works run from anywhere and from the build directory. */
static int resfile(const char* leaf, char* path, long pl)

{

    char  dir[MAXSTR];
    char* e;
    FILE* f;
    long  i;

    dir[0] = 0;
    if (mailprog) {

        snprintf(dir, sizeof(dir), "%s", mailprog);
        e = strrchr(dir, '/');
        if (e) e[1] = 0; else dir[0] = 0;

    }
    for (i = 0; i < 4; i++) {

        if (i == 0) snprintf(path, pl, "%s%s", dir, leaf);
        else if (i == 1) snprintf(path, pl, "%s../graph_programs/%s", dir, leaf);
        else if (i == 2) snprintf(path, pl, "%s", leaf);
        else snprintf(path, pl, "graph_programs/%s", leaf);
        f = fopen(path, "r");
        if (f) { fclose(f); return (TRUE); }

    }

    return (FALSE);

}

static void drawbanner(void)

{

    long y;

    if (!banwf) return;
    ami_bcolorc(banwf, rgb(255), rgb(255), rgb(255));
    fprintf(banwf, "\f");
    /* the name, at the left, set in the middle of the band */
    ami_fcolorc(banwf, rgb(40), rgb(40), rgb(60));
    ami_cursorg(banwf, 16, (banh-ami_chrsizy(banwf))/2);
    fprintf(banwf, "Ami Mail");
    ami_fcolor(banwf, ami_black);
    /* and the picture at the right, at the size it was made */
    if (havepic)
        ami_picture(banwf, BANPIC, ami_maxxg(banwf)-picw-16, (banh-pich)/2,
                    ami_maxxg(banwf)-16, (banh-pich)/2+pich);
    /* Two lines under it, which is what says the banner is not part of
       what is below it. */
    y = banh-4;
    ami_fcolorc(banwf, rgb(120), rgb(120), rgb(140));
    ami_linewidth(banwf, 2);
    ami_line(banwf, 1, y, ami_maxxg(banwf), y);
    ami_line(banwf, 1, y+4, ami_maxxg(banwf), y+4);
    ami_linewidth(banwf, 1);
    ami_fcolor(banwf, ami_black);

}

/*******************************************************************************

The status bar

Anything that takes longer than an instant says so. The line at the foot
of the window carries what is being done and, when the work has a known
size, a bar showing how far along it is.

The measure is the event horizon: about a thirtieth of a second, the
point at which a delay stops being instantaneous and becomes a wait. A
program that goes quiet across a wait cannot be told from one that has
fallen over -- which is exactly what a fetch of seventy thousand
messages looked like before there was a thread to fetch on and a line to
say so.

*******************************************************************************/

static char statsaid[MAXSTR]; /* what the line says now */
static long stath;            /* how tall the strip is */
static long statmax;          /* the size of the job, or none if not known */
static long statpos;          /* and how far into it */

/* the strip's own drawing, text and all */
/* The strip is a band at the foot of the main window, in the room the
   panes are laid out to leave.

   Everything in it is drawn -- the band, the words and the bar -- and
   nothing in it is a widget. A bar drawn beside the words it belongs to
   is four lines of code, needs no window of its own, and is placed by
   the same arithmetic as everything else in the strip. Everything in it is drawn -- the band,
   the words and the bar -- and nothing in it is a widget.

   The bar was a widget to begin with, and it cost two days to learn why
   it should not be. A widget is a window of its own, placed in its own
   right, and in a window carrying a menu what is placed and what is
   drawn do not land in the same place. Drawing a bar is four lines of
   code and puts it exactly where the words beside it are. */
static void drawstatus(void)

{

    long y = ami_maxyg(stdout)-stath;         /* at the foot */
    long bx = ami_maxxg(stdout)-progw-8;
    long by = y+(stath-progh)/2;

    /* the band, laid down as one line as thick as the strip: a filled
       rectangle does not paint in this window, a line does */
    ami_fcolorc(stdout, rgb(245), rgb(245), rgb(245));
    ami_linewidth(stdout, stath);
    ami_line(stdout, 1, y+stath/2, ami_maxxg(stdout), y+stath/2);
    ami_linewidth(stdout, 1);
    ami_fcolor(stdout, ami_black);
    divider(stdout, 1, y, ami_maxxg(stdout), y);
    if (*statsaid) {

        char t[MAXSTR];

        copystr(t, statsaid, sizeof(t));
        /* it shares the strip with the bar, and never writes over it */
        fitchars(stdout, t, bx-16);
        ami_cursorg(stdout, 8, y+(stath-chrh)/2+1);
        ami_fcolorc(stdout, rgb(80), rgb(80), rgb(80));
        fprintf(stdout, "%s", t);
        ami_fcolor(stdout, ami_black);

    }
    /* the bar: what is done, then what is left, then a line round both */
    if (statmax > 0) {

        long w = progw*statpos/statmax;

        if (w > 0) {

            ami_fcolorc(stdout, rgb(120), rgb(60), rgb(130));
            ami_linewidth(stdout, progh-2);
            ami_line(stdout, bx+1, by+progh/2, bx+w, by+progh/2);

        }
        if (w < progw) {

            ami_fcolorc(stdout, rgb(225), rgb(225), rgb(225));
            ami_linewidth(stdout, progh-2);
            ami_line(stdout, bx+w+1, by+progh/2, bx+progw, by+progh/2);

        }
        ami_linewidth(stdout, 1);
        ami_fcolorc(stdout, rgb(150), rgb(150), rgb(150));
        ami_rect(stdout, bx, by, bx+progw, by+progh);
        ami_fcolor(stdout, ami_black);

    }

}

static void status(const char* s)

{

    if (!s) s = "";
    if (!strcmp(s, statsaid)) return; /* it already says that */
    copystr(statsaid, s, sizeof(statsaid));
    drawstatus();

}

/* How far along, from nothing to all of it. The widget takes the whole
   range of a long, so the fraction is worked out in that range rather
   than in percent, which would step the bar in hundredths. */
static void statprog(long pos, long max)

{

    if (max <= 0) { /* no size to it: there is no bar */

        if (statmax) { statmax = 0; statpos = 0; drawstatus(); }

        return;

    }
    if (pos < 0) pos = 0;
    if (pos > max) pos = max;
    if (statmax == max && statpos == pos) return; /* it is already there */
    statmax = max;
    statpos = pos;
    drawstatus();

}

/* How many characters of this string fit in the width, found by
   halving. Taking a character off and measuring the whole string again
   until it fits measures the string as many times as there are
   characters to lose, and every measurement walks the string: a few
   hundred characters cost a few hundred measurements, and a message
   list redraws every row of forty on every click. Halving costs the
   logarithm of that -- eight or nine measurements instead of four
   hundred. */
/* the width of an average character of the font in use, for guessing
   with; measured once, since the font does not change under us */
static long avgchrw(FILE* f)

{

    static long w;

    if (!w) w = ami_strsiz(f, "abcdefghijklmnopqrstuvwxyz")/26;
    if (w < 1) w = 1;

    return (w);

}

static long fitchars(FILE* f, char* s, long w)

{

    long n = strlen(s);
    long lo = 0, hi;
    long cap;
    char c;

    /* Cut it down before measuring it. Measuring costs a walk of the
       whole string, so the four hundred characters of a snippet are
       walked by every measurement even though sixty of them will fit.
       Three times the average character's worth is far more than can
       fit and costs nothing to work out. */
    cap = w/avgchrw(f)*3+8;
    if (n > cap) { s[cap] = 0; n = cap; }
    hi = n;
    if (ami_strsiz(f, s) <= w) return (n); /* it all fits */
    while (lo < hi) {

        long mid = (lo+hi+1)/2;

        c = s[mid];
        s[mid] = 0;
        if (ami_strsiz(f, s) <= w) lo = mid; else hi = mid-1;
        s[mid] = c;

    }

    return (lo);

}

/* The dividers between the parts of the display, all drawn alike: a
   quiet grey, so they mark the columns off without shouting over the
   text the way black rules would. */
static void divider(FILE* f, long x1, long y1, long x2, long y2)

{

    ami_fcolorc(f, rgb(180), rgb(180), rgb(180));
    ami_linewidth(f, 2);
    ami_line(f, x1, y1, x2, y2);
    ami_linewidth(f, 1);
    ami_fcolor(f, ami_black);

}

/* A number written the way a mail reader writes it, in threes: eleven
   thousand messages reads as 11,421 and not as 11421. */
static void commas(long n, char* s, long sl)

{

    char b[40];
    long i, o = 0, l;

    snprintf(b, sizeof(b), "%ld", n);
    l = strlen(b);
    for (i = 0; i < l && o < sl-2; i++) {

        if (i && (l-i)%3 == 0) s[o++] = ',';
        s[o++] = b[i];

    }
    s[o] = 0;

}

/* cut a string to fit a width, with an ellipsis if it had to be cut */
static void clipstr(FILE* f, char* s, long w)

{

    long n = fitchars(f, s, w);

    if (n >= (long)strlen(s)) return; /* nothing to cut */
    s[n] = 0;
    if (n > 3) strcpy(s+n-3, "...");

}

static void drawfolders(void)

{

    long i;
    long y = 4;
    long cw;
    long sec;
    long w;
    char cnt[40];
    char head[120];

    if (!foldwf) return;
    w = ami_maxxg(foldwf);
    /* No clearing of the pane first. Every line paints its own ground
       before its text, so a clear only puts up a blank that the drawing
       covers again -- and a pane that goes blank and comes back is a
       blink, which during a fetch happens over and over. What is
       cleared is the room below the last line, which nothing else
       reaches. */
    ami_fcolor(foldwf, ami_black);
    /* a section for each server, then one for the folders that are ours
       alone, since two servers may each have an INBOX and a local Trash
       is not the server's Trash */
    for (sec = 0; sec <= srvct; sec++) {

        long shown = 0;
        long srv = sec < srvct? sec: -1;

        if (sec) { /* a rule between the sections */

            y += chrh/2;
            ami_fcolor(foldwf, ami_white);
            ami_frect(foldwf, 0, y-2, w, y+2);
            ami_fcolor(foldwf, ami_black);
            divider(foldwf, 6, y, w-8, y);
            y += chrh;

        }
        if (srv >= 0) snprintf(head, sizeof(head), "%s Server Folders",
                               servers[srv].name);
        else copystr(head, "Local Folders", sizeof(head));
        clipstr(foldwf, head, w-12);
        ami_fcolor(foldwf, ami_white);
        ami_frect(foldwf, 0, y-2, w, y+chrh+2);
        ami_fcolor(foldwf, ami_black);
        ami_bold(foldwf, TRUE);
        ami_cursorg(foldwf, 6, y);
        fprintf(foldwf, "%s", head);
        ami_bold(foldwf, FALSE);
        y += chrh*2;
        for (i = 0; i < foldct; i++) {

            char nm[MAXSTR];

            if (folders[i].srv != srv) continue;
            if (folders[i].local != (srv < 0)) continue;
            shown++;
            foldy[i] = y; /* where it landed, for the click to find */
            /* its own ground: the mark if it is the one being read,
               white if it is not */
            ami_fcolor(foldwf, i == foldsel? ami_cyan: ami_white);
            ami_frect(foldwf, 2, y-2, w-2, y+chrh);
            ami_fcolor(foldwf, ami_black);
            copystr(nm, folders[i].show, MAXSTR);
            cnt[0] = 0;
            if (folders[i].msgs > 0) commas(folders[i].msgs, cnt,
                                            sizeof(cnt));
            cw = *cnt? ami_strsiz(foldwf, cnt)+8: 0;
            ami_bold(foldwf, i == foldsel);
            clipstr(foldwf, nm, w-16-cw);
            ami_cursorg(foldwf, 8, y);
            fprintf(foldwf, "%s", nm);
            if (*cnt) {

                ami_cursorg(foldwf, w-8-ami_strsiz(foldwf, cnt), y);
                fprintf(foldwf, "%s", cnt);

            }
            ami_bold(foldwf, FALSE);
            y += chrh+4;

        }
        if (!shown) { /* say so rather than leave a gap */

            ami_fcolor(foldwf, ami_white);
            ami_frect(foldwf, 0, y-2, w, y+chrh);
            ami_fcolorc(foldwf, rgb(130), rgb(130), rgb(130));
            ami_cursorg(foldwf, 8, y);
            fprintf(foldwf, "%s", srv >= 0? "(not fetched)": "(none yet)");
            ami_fcolor(foldwf, ami_black);
            y += chrh+4;

        }

    }
    if (y < ami_maxyg(foldwf)) { /* the room the sections did not fill */

        ami_fcolor(foldwf, ami_white);
        ami_frect(foldwf, 0, y, w, ami_maxyg(foldwf));
        ami_fcolor(foldwf, ami_black);

    }

}


/* One line of the message list: who it is from, then the subject, then
   as much of the message as is left over, then when it came. This is the
   layout the web readers use and it is the right one: the eye runs down
   the senders, and the subject and the start of the text read as one
   sentence. */
static void drawmsg(long i, long y)

{

    msgrec* m = &msgs[i];
    long    w = ami_maxxg(listwf)-sbw;
    long    x;
    char    s[MAXSTR+SNIPPET];
    long    subw;

    if (i == msgsel) {

        ami_fcolor(listwf, ami_cyan);
        ami_frect(listwf, 0, y-2, w, y+rowh-4);
        ami_fcolor(listwf, ami_black);

    }
    /* The sender. Bold goes on before the fit is measured: bold is
       wider than regular, so a name measured regular and drawn bold
       runs past the divider it was cut to. */
    ami_bold(listwf, TRUE);
    copystr(s, m->from, MAXSTR);
    clipstr(listwf, s, fromx-14);
    ami_cursorg(listwf, 6, y);
    fprintf(listwf, "%s", s);
    /* what kind of mail it is, in its own column and in a quieter grey:
       it is there to be glanced past, not read */
    copystr(s, m->cat, sizeof(s));
    clipstr(listwf, s, catx-fromx-16);
    ami_fcolorc(listwf, rgb(110), rgb(110), rgb(110));
    ami_cursorg(listwf, fromx+8, y);
    fprintf(listwf, "%s", s);
    ami_fcolor(listwf, ami_black);
    /* the subject, then the start of the message after it */
    x = catx+8;
    subw = datex-x-8;
    copystr(s, m->subject, MAXSTR);
    clipstr(listwf, s, subw);
    ami_cursorg(listwf, x, y);
    fprintf(listwf, "%s", s);
    x += ami_strsiz(listwf, s);
    ami_bold(listwf, FALSE);
    if (*m->snip && x < datex-ami_strsiz(listwf, "  ")) {

        snprintf(s, sizeof(s), " - %s", m->snip);
        clipstr(listwf, s, datex-x-8);
        ami_fcolor(listwf, ami_black);
        ami_cursorg(listwf, x, y);
        fprintf(listwf, "%s", s);

    }
    /* the date, against the right */
    copystr(s, m->when, sizeof(m->when));
    ami_cursorg(listwf, w-ami_strsiz(listwf, s)-8, y);
    fprintf(listwf, "%s", s);
    /* This row's piece of the column dividers. Drawn with the row so
       that a row drawn on its own -- the selection moving, a scroll
       filling in -- keeps the line whole. A vertical line survives a
       vertical scroll, so the pieces always join. */
    divider(listwf, fromx, y-2, fromx, y+rowh-4);
    divider(listwf, catx, y-2, catx, y+rowh-4);
    divider(listwf, datex-8, y-2, datex-8, y+rowh-4);

}

/* Draw one row where it belongs, background and all. Moving from one
   message to the next changes two rows out of thirty, and drawing a row
   costs a few text writes, so drawing the two beats drawing the lot: a
   redraw of the whole list is over a tenth of a second, which is what
   made stepping down a folder feel slow. */
static void drawrow(long i)

{

    long y;

    if (!listwf || i < msgtop || i >= msgct) return;
    y = 4+(i-msgtop)*rowh;
    if (y+rowh > ami_maxyg(listwf)) return; /* not on the screen */
    ami_fcolor(listwf, ami_white);
    ami_frect(listwf, 0, y-2, ami_maxxg(listwf)-sbw, y+rowh-4);
    ami_fcolor(listwf, ami_black);
    drawmsg(i, y);
    ami_fcolor(listwf, ami_white);
    ami_line(listwf, 0, y+rowh-3, ami_maxxg(listwf)-sbw, y+rowh-3);
    ami_fcolor(listwf, ami_black);

}

/* move the mark from one message to another, without drawing the rest */
static void selectmsg(long i)

{

    long was = msgsel;

    if (i == msgsel) return;
    msgsel = i;
    if (was >= 0) drawrow(was);
    drawrow(i);

}

/* How many rows the list shows. The last one is only drawn if it fits
   whole, which is what the drawing loop does. */
static long listvis(void)

{

    long n = (ami_maxyg(listwf)-4)/rowh;

    return (n < 1? 1: n);

}

/* keep the top of the list inside the list */
static void listclamp(void)

{

    if (msgtop > msgct-listvis()) msgtop = msgct-listvis();
    if (msgtop < 0) msgtop = 0;

}

static void setlistbar(void);   /* forward */

/* Draw only the rows a redraw rectangle touches, and the dividers down
   it. A resize brings two of these -- the strip down the right and the
   strip along the bottom -- and they are the whole of what needs
   painting, since the buffer keeps what was already there. */
static void listrect(long y1, long y2)

{

    long i;
    long first = msgtop+(y1-4)/rowh;
    long last  = msgtop+(y2-4)/rowh;

    if (first < msgtop) first = msgtop;
    if (last >= msgct) last = msgct-1;
    divider(listwf, fromx, y1, fromx, y2);
    divider(listwf, catx, y1, catx, y2);
    divider(listwf, datex-8, y1, datex-8, y2);
    for (i = first; i <= last; i++) drawrow(i);
    setlistbar();

}

/* Move the list to where it is now supposed to be, by moving what is
   already on the screen. Going down a row brings one row into view;
   drawing thirty of them to show one is what made a wheel notch cost an
   eighth of a second. */
static void showlist(void)

{

    long vis;
    long d;

    if (!listwf || foldsel < 0 || !msgct) { drawlist(); return; }
    listclamp();
    vis = listvis();
    d = msgtop-listshown;
    if (!d) { setlistbar(); return; }
    if (d >= vis || d <= -vis) { drawlist(); return; } /* nothing kept */
    {

        struct timespec t0, t1;

        if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
        ami_scrollg(listwf, 0, d*rowh);
        listshown = msgtop;
        if (diag) {

            clock_gettime(CLOCK_MONOTONIC, &t1);
            fprintf(stderr, "listscroll: %ld row%s %.1fms\n", d < 0? -d: d,
                    d == 1 || d == -1? "": "s",
                    (t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);

        }

    }
    if (d > 0) { /* the new rows come in at the foot */

        long i;

        for (i = msgtop+vis-d; i < msgtop+vis; i++) drawrow(i);

    } else { /* and at the head */

        long i;

        for (i = msgtop; i < msgtop-d; i++) drawrow(i);

    }
    setlistbar();

}

static void setlistbar(void)

{

    long max = msgct-listrows;

    if (fromdrag) return;
    if (max < 1) max = 1;
    ami_scrollsiz(listwf, SBLIST, fullscale(listrows, msgct));
    ami_scrollpos(listwf, SBLIST, fullscale(msgtop, max));

}

static void drawlist(void)

{

    long i;
    long y = 4;
    struct timespec t0, t1;

    if (!listwf) return;
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
    /* No clearing of the whole pane first. Every row paints its own
       ground before its text, so a clear only puts up a blank that the
       drawing immediately covers -- and a blank surface followed by a
       redraw is a flash the reader can see. What is cleared is what the
       rows will not reach, below the last of them. */
    ami_fcolor(listwf, ami_black);
    if (foldsel < 0) {

        ami_cursorg(listwf, 8, y);
        fprintf(listwf, "Pick a folder.");
    
        return;

    }
    if (!msgct) {

        ami_cursorg(listwf, 8, y);
        /* An empty list means one of two things and they are not alike:
           there is nothing in the folder, or there is and it has not
           been read yet. Four gigabytes takes half a minute to read, and
           for that half minute the reader should not be told the folder
           is empty. */
        if (idxwant == foldsel || idxdoing == foldsel)
            fprintf(listwf, "Reading %s...", folders[foldsel].show);
        else fprintf(listwf, "Nothing in %s yet. Mail/Fetch reads the "
                     "server.", folders[foldsel].show);

        return;

    }
    /* the column dividers first, full height, so the rows overprint
       their own pieces and the empty part of the list is ruled too */
    divider(listwf, fromx, 0, fromx, ami_maxyg(listwf));
    divider(listwf, catx, 0, catx, ami_maxyg(listwf));
    divider(listwf, datex-8, 0, datex-8, ami_maxyg(listwf));
    for (i = msgtop; i < msgct && y+rowh <= ami_maxyg(listwf); i++) {

        drawrow(i);
        y += rowh;

    }
    if (y < ami_maxyg(listwf)) { /* the room the rows did not fill */

        ami_fcolor(listwf, ami_white);
        ami_frect(listwf, 0, y, ami_maxxg(listwf), ami_maxyg(listwf));
        ami_fcolor(listwf, ami_black);
        divider(listwf, fromx, y, fromx, ami_maxyg(listwf));
        divider(listwf, catx, y, catx, ami_maxyg(listwf));
        divider(listwf, datex-8, y, datex-8, ami_maxyg(listwf));

    }
    listshown = msgtop;
    setlistbar();
    if (diag) {

        clock_gettime(CLOCK_MONOTONIC, &t1);
        fprintf(stderr, "list: %ld rows %.0fms\n", i-msgtop,
                (t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);

    }

}

/*******************************************************************************

Reading one message

*******************************************************************************/

/* wrap the message to the window it is being read in */
/* one more line for the reader to show */
static void readput(const char* s)

{

    if (readlines >= readmax) {

        readmax = readmax? readmax*2: 200;
        readline = realloc(readline, readmax*sizeof(char*));
        if (!readline) { fail("Out of memory"); exit(1); }

    }
    readline[readlines] = getmem(strlen(s)+1);
    strcpy(readline[readlines++], s);

}

static void wrapread(void)

{

    const char* p;
    long        w = ami_maxxg(readwf)-16-sbw;
    long        i;

    for (i = 0; i < readlines; i++) free(readline[i]);
    readlines = 0;
    if (!readtext) return;
    p = readtext;
    while (*p) {

        const char* e = p;
        char        line[MAXLINE];
        long        n;

        while (*e && *e != '\n') e++;
        n = e-p;
        if (n >= (long)sizeof(line)) n = sizeof(line)-1;
        memcpy(line, p, n);
        line[n] = 0;
        while (n && (line[n-1] == '\r' || line[n-1] == ' ')) line[--n] = 0;
        if (!*line) {

            /* A blank line is a line and nothing more. It cannot go
               round the breaking loop below: nothing fits in no width,
               so that loop takes its one character from past the end of
               the string -- and what is past the end is whatever the
               line before left in the buffer. Every blank line in every
               message was followed by a ghost of the line above it. */
            readput("");
            p = *e? e+1: e;

            continue;

        }
        /* a line too long for the window is broken at a space */
        do {

            char  part[MAXLINE];
            long  cut;

            copystr(part, line, sizeof(part));
            cut = fitchars(readwf, part, w);
            if (cut < (long)strlen(part)) {

                long b = cut;

                /* back up to the last space, so words stay whole */
                while (b && part[b-1] != ' ') b--;
                if (b > 1) cut = b;
                part[cut] = 0;

            }
            /* Always take at least one character. A window too narrow
               for a single character would otherwise take none, and a
               loop that takes nothing off the front of the line never
               reaches the end of it. */
            if (cut < 1) { cut = 1; part[1] = 0; }
            readput(part);
            memmove(line, line+cut, strlen(line+cut)+1);
            while (*line == ' ') memmove(line, line+1, strlen(line));

        } while (*line);
        p = *e? e+1: e;

    }

}

/* how many lines of the message the window holds */
static long readpage(void)

{

    long page = (ami_maxyg(readwf)-8)/chrh;

    return (page < 1? 1: page);

}

/* keep the top of the message inside the message */
static void readclamp(void)

{

    long page = readpage();

    if (readtop > readlines-page) readtop = readlines-page;
    if (readtop < 0) readtop = 0;

}

/* put the bar where the text is */
static void readbar(void)

{

    long page = readpage();
    long max = readlines-page;

    if (fromdrag) return;
    if (max < 1) max = 1;
    ami_scrollsiz(readwf, SBREAD, fullscale(page, readlines));
    ami_scrollpos(readwf, SBREAD, fullscale(readtop, max));

}

/* draw the lines from a to b, having cleared the room they go in */
static void readrows(long a, long b)

{

    long i;

    if (a < readtop) a = readtop;
    if (b > readlines-1) b = readlines-1;
    if (a > b) return;
    ami_fcolor(readwf, ami_white);
    ami_frect(readwf, 0, 4+(a-readtop)*chrh, ami_maxxg(readwf),
              4+(b-readtop+1)*chrh);
    ami_fcolor(readwf, ami_black);
    for (i = a; i <= b; i++) if (*readline[i]) {

        ami_cursorg(readwf, 8, 4+(i-readtop)*chrh);
        fprintf(readwf, "%s", readline[i]);

    }

}

static void drawread(void)

{

    if (!readwf) return;
    fprintf(readwf, "\f");
    ami_fcolor(readwf, ami_black);
    readclamp();
    readrows(readtop, readtop+readpage()-1);
    readshown = readtop;
    readbar();

}

/* Move the text to where it is now supposed to be. A step of a line or
   two moves the pixels that are already on the screen and draws only
   the lines that come into view -- a message page is thirty odd lines
   of text and drawing one of them is a thirtieth of the work of drawing
   them all. Anything further than a screen is drawn afresh, since none
   of what is up would be kept anyway. */
static void showread(void)

{

    long page;
    long d;

    if (!readwf) return;
    readclamp();
    page = readpage();
    d = readtop-readshown;
    if (!d) { readbar(); return; } /* it did not move */
    if (d >= page || d <= -page) { drawread(); return; } /* nothing kept */
    /* Positive moves the picture up, which is what going down the
       message does, and leaves the strip at the bottom to be filled. */
    {

        struct timespec t0, t1;

        if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
        ami_scrollg(readwf, 0, d*chrh);
        readshown = readtop;
        if (d > 0) readrows(readtop+page-d, readtop+page-1); /* at the foot */
        else readrows(readtop, readtop-d-1);                 /* at the head */
        readbar();
        if (diag) {

            clock_gettime(CLOCK_MONOTONIC, &t1);
            fprintf(stderr, "scroll: %ld line%s %.1fms\n", d < 0? -d: d,
                    d == 1 || d == -1? "": "s",
                    (t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);

        }

    }

}

/* open a message to be read: the headers worth showing, then the text */
/* Quote what is being answered, the way mail has quoted since there was
   mail: the sender and the date, then the text with a > before every
   line of it. */
static char* quoted(const char* who, const char* when, const char* text,
                    int mark)

{

    long  n = strlen(text)*2+MAXSTR*2+64;
    char* d = getmem(n);
    long  o = 0;
    const char* p;

    if (mark) o += snprintf(d+o, n-o, "On %s, %s wrote:\n", when, who);
    for (p = text; *p; ) {

        const char* q = p;

        while (*q && *q != '\n') q++;
        if (o+(q-p)+4 < n) {

            if (mark) { d[o++] = '>'; if (q > p) d[o++] = ' '; }
            memcpy(d+o, p, q-p);
            o += q-p;
            d[o++] = '\n';

        }
        p = *q? q+1: q;

    }
    d[o] = 0;

    return (d);

}

/* Answer it, answer everybody on it, or pass it on. */
static void answer(long what)

{

    char  subj[MAXSTR*2];
    char  to[MAXSTR*2];
    char  cc[MAXSTR*2];
    char* body;
    char  who[MAXSTR];

    if (!readwf) return;
    nameof(redfrom, who, sizeof(who));
    if (!*who) copystr(who, redfrom, sizeof(who));
    *cc = 0;
    if (what == MENUFWD) {

        snprintf(subj, sizeof(subj), "%s%s",
                 strncasecmp(redsubj, "Fwd:", 4)? "Fwd: ": "", redsubj);
        *to = 0;
        body = quoted(who, reddate, redtext? redtext: "", FALSE);
        {   /* what is passed on says what it was */

            char* d = getmem(strlen(body)+MAXSTR*4);

            sprintf(d, "\n---------- Forwarded message ----------\n"
                    "From: %s\nDate: %s\nSubject: %s\nTo: %s\n\n%s",
                    redfrom, reddate, redsubj, redto, body);
            free(body);
            body = d;

        }

    } else {

        snprintf(subj, sizeof(subj), "%s%s",
                 strncasecmp(redsubj, "Re:", 3)? "Re: ": "", redsubj);
        /* the answer goes to whoever wrote it */
        addrof(redfrom, to, sizeof(to));
        if (!*to) copystr(to, redfrom, sizeof(to));
        if (what == MENUREPALL) {

            /* and to everybody it was sent to, less ourselves: answering
               everybody does not mean answering yourself */
            char  mine[MAXSTR];
            char  one[MAXSTR];
            const char* p;
            long  o = 0;

            copystr(mine, sendsrv >= 0 && sendsrv < srvct?
                    servers[sendsrv].user: "", sizeof(mine));
            for (p = redto; p || *one; ) {

                char just[MAXSTR];

                p = nextaddr(p, one, sizeof(one));
                if (*one) {

                    addrof(one, just, sizeof(just));
                    if (!*just) copystr(just, one, sizeof(just));
                    if (strcasecmp(just, mine) && strcasecmp(just, to) &&
                        o+strlen(just)+3 < sizeof(cc))
                        o += snprintf(cc+o, sizeof(cc)-o, "%s%s",
                                      o? ", ": "", just);

                }
                if (!p) break;

            }
            for (p = redcc; p || *one; ) {

                char just[MAXSTR];

                p = nextaddr(p, one, sizeof(one));
                if (*one) {

                    addrof(one, just, sizeof(just));
                    if (!*just) copystr(just, one, sizeof(just));
                    if (strcasecmp(just, mine) && strcasecmp(just, to) &&
                        o+strlen(just)+3 < sizeof(cc))
                        o += snprintf(cc+o, sizeof(cc)-o, "%s%s",
                                      o? ", ": "", just);

                }
                if (!p) break;

            }

        }
        body = quoted(who, reddate, redtext? redtext: "", TRUE);
        {   /* a line to write on, above what is being answered */

            char* d = getmem(strlen(body)+4);

            sprintf(d, "\n\n%s", body);
            free(body);
            body = d;

        }

    }
    /* A reply belongs to the conversation it answers and says so; a
       forward is a new message to somebody who was not in it, and
       putting it in the thread would file it under a conversation they
       have never seen. */
    if (what == MENUFWD) cmpopen(to, cc, subj, body, "", "");
    else cmpopen(to, cc, subj, body, redid, redrefs);
    free(body);
    /* the caret where the writing goes, which is the top */
    cmpcl = 0;
    cmpcc = 0;
    cmpdraw();

}

static void openmsg(long i)

{

    char* raw;
    char* text;
    char  from[MAXSTR], to[MAXSTR], subj[MAXSTR], date[MAXSTR];
    char  title[MAXSTR];
    long  wx, wy;
    long  n;
    struct timespec t0, t1, t2, t3, t4;

    if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
    raw = getmsg(foldsel, i);
    if (!raw) { fail("The message could not be read back"); return; }
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t1);
    findheader(raw, "From", from, sizeof(from));
    findheader(raw, "To", to, sizeof(to));
    findheader(raw, "Subject", subj, sizeof(subj));
    findheader(raw, "Date", date, sizeof(date));
    text = textof(raw, strlen(raw), 0); /* all of it, to be read */
    /* what a reply to this would be made of */
    copystr(redfrom, from, sizeof(redfrom));
    copystr(redto, to, sizeof(redto));
    copystr(redsubj, subj, sizeof(redsubj));
    copystr(reddate, date, sizeof(reddate));
    if (!findheader(raw, "Cc", redcc, sizeof(redcc))) *redcc = 0;
    if (!findheader(raw, "Message-ID", redid, sizeof(redid))) *redid = 0;
    if (!findheader(raw, "References", redrefs, sizeof(redrefs))) *redrefs = 0;
    free(redtext);
    redtext = getmem(strlen(text)+1);
    strcpy(redtext, text);
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t2);
    free(readtext);
    n = strlen(from)+strlen(to)+strlen(subj)+strlen(date)+strlen(text)+200;
    readtext = getmem(n);
    snprintf(readtext, n,
             "From:    %s\nTo:      %s\nDate:    %s\nSubject: %s\n"
             "\n"
             "%s", from, to, date, subj, text);
    free(text);
    free(raw);
    readtop = 0;
    if (!readwf) {

        ami_openwin(&stdin, &readwf, NULL, READWIN);
        {   /* what can be done with a message that is being read */

            ami_menuptr mp;
            ami_menuptr ml = NULL;

            newmenu(&mp, FALSE, FALSE, OFF, MENUREPLY, "Reply");
            appendmenu(&ml, mp);
            newmenu(&mp, FALSE, FALSE, OFF, MENUREPALL, "Reply All");
            appendmenu(&ml, mp);
            newmenu(&mp, FALSE, FALSE, OFF, MENUFWD, "Forward");
            appendmenu(&ml, mp);
            ami_menu(readwf, ml);

        }
        ami_auto(readwf, FALSE);
        ami_curvis(readwf, FALSE);
        ami_font(readwf, AMI_FONT_TERM);
        ami_setpoints(readwf, 11.0);
        ami_binvis(readwf);
        ami_winclientg(readwf, ami_strsiz(readwf, "0")*84, chrh*40, &wx, &wy,
                       BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
        ami_setsizg(readwf, wx, wy);
        /* down and to the right, so it does not sit on top of the list
           it was opened from */
        ami_setposg(readwf, 80, 80);
        ami_scrollvertsizg(readwf, &sbw, &wy);
        ami_scrollvertg(readwf, ami_maxxg(readwf)-sbw, 1, sbw,
                        ami_maxyg(readwf), SBREAD);

    }
    copystr(title, *subj? subj: "(no subject)", MAXSTR);
    ami_title(readwf, title);
    ami_front(readwf);
    wrapread();
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t3);
    drawread();
    if (diag) {

        clock_gettime(CLOCK_MONOTONIC, &t4);
        fprintf(stderr, "open: %ld bytes, read %.0fms decode %.0fms "
                        "wrap %.0fms (%ld lines) draw %.0fms\n", msgs[i].len,
                (t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6,
                (t2.tv_sec-t1.tv_sec)*1e3+(t2.tv_nsec-t1.tv_nsec)/1e6,
                (t3.tv_sec-t2.tv_sec)*1e3+(t3.tv_nsec-t2.tv_nsec)/1e6,
                readlines,
                (t4.tv_sec-t3.tv_sec)*1e3+(t4.tv_nsec-t3.tv_nsec)/1e6);

    }

}

static void closeread(void)

{

    long i;

    if (!readwf) return;
    fclose(readwf);
    readwf = NULL;
    for (i = 0; i < readlines; i++) free(readline[i]);
    readlines = 0;
    free(readtext);
    readtext = NULL;

}

/*******************************************************************************

The server form

Everything needed to reach an account, in one window: where the servers
are, who to log in as, and how much of a folder to take at a time. It is
a window of the program's own like the reader, so it is filled in with
the rest of the program still in view, and it is serviced from the same
event loop.

The password is shown as it is typed. There is no widget in the library
that hides what is entered, which is the one thing a form like this
would want.

*******************************************************************************/

/* the fields, in the order they appear */
static const struct {

    long  id;
    char* label;
    char* note;

} srvfld[] = {

    { SRVNAME,  "Name",           "what to call this account here" },
    { SRVIMAP,  "Mail server",    "where the mail is read from" },
    { SRVIPORT, "Port",           "993 for the secure one, which is usual" },
    { SRVSMTP,  "Sending server", "where mail would be sent from" },
    { SRVSPORT, "Port",           "465 for the secure one" },
    { SRVUSER,  "User",           "the whole address, as someone@gmail.com" },
    { SRVPASS,  "Password",       "for Gmail, an application password" },
    { SRVLIMIT, "Messages",       "how many of each folder to fetch" },
    { SRVPOLL,  "Look every",     "seconds between looks at the servers" },

};
#define SRVFLDS ((long)(sizeof(srvfld)/sizeof(srvfld[0])))

static void srvlay(void)

{

    long chrw = ami_strsiz(srvwf, "0");
    long labw = ami_strsiz(srvwf, "Sending server  ");
    long ew, eh, bw, bh;
    long y;
    long i;

    ami_editboxsizg(srvwf, "0", &ew, &eh);
    ami_buttonsizg(srvwf, "Cancel", &bw, &bh);
    fprintf(srvwf, "\f");
    ami_fcolor(srvwf, ami_black);
    y = chrh;
    for (i = 0; i < SRVFLDS; i++) {

        ami_cursorg(srvwf, chrw*2, y+(eh-chrh)/2);
        fprintf(srvwf, "%s", srvfld[i].label);
        ami_poswidgetg(srvwf, srvfld[i].id, chrw*2+labw, y);
        ami_sizwidgetg(srvwf, srvfld[i].id, chrw*30, eh);
        /* what the field is for, beside it, since a form that only says
           "Port" leaves the reader to guess which port */
        ami_cursorg(srvwf, chrw*2+labw+chrw*32, y+(eh-chrh)/2);
        fprintf(srvwf, "%s", srvfld[i].note);
        y += eh+chrh/2;

    }
    y += chrh/2;
    /* One account sends, and only one: a message goes out over one
       connection with one name on it. Checking the box here is choosing
       among the accounts, so checking it for one clears it for the rest
       -- which is what the box does when it is clicked, not something
       the reader has to remember to do. */
    {

        long cw, ch;

        ami_checkboxsizg(srvwf, "Send mail from this account", &cw, &ch);
        ami_poswidgetg(srvwf, SRVSEND, chrw*2+labw, y);
        ami_sizwidgetg(srvwf, SRVSEND, cw, ch);
        ami_selectwidget(srvwf, SRVSEND, srvedit == sendsrv);
        y += ch+chrh/2;

    }
    ami_poswidgetg(srvwf, SRVOK, chrw*2+labw, y);
    ami_sizwidgetg(srvwf, SRVOK, bw, bh);
    ami_poswidgetg(srvwf, SRVCAN, chrw*2+labw+bw+chrw*2, y);
    ami_sizwidgetg(srvwf, SRVCAN, bw, bh);
    ami_poswidgetg(srvwf, SRVNEXT, chrw*2+labw+(bw+chrw*2)*2, y);
    ami_sizwidgetg(srvwf, SRVNEXT, bw, bh);
    ami_poswidgetg(srvwf, SRVNEW, chrw*2+labw+(bw+chrw*2)*3, y);
    ami_sizwidgetg(srvwf, SRVNEW, bw, bh);
    ami_poswidgetg(srvwf, SRVDEL, chrw*2+labw+(bw+chrw*2)*4, y);
    ami_sizwidgetg(srvwf, SRVDEL, bw, bh);
    /* which of them is being shown, since the fields do not say */
    {

        char n[80];

        snprintf(n, sizeof(n), "account %ld of %ld", srvedit+1,
                 srvct > srvedit? srvct: srvedit+1);
        ami_fcolorc(srvwf, rgb(110), rgb(110), rgb(110));
        ami_cursorg(srvwf, chrw*2, y+bh+chrh/2);
        fprintf(srvwf, "%s", n);
        ami_fcolor(srvwf, ami_black);

    }

}

/* fill the form in from what the program is using now */
static void srvload(void)

{

    char num[40];

    if (srvedit >= srvct) { /* one that does not exist yet */

        if (srvct < MAXSRV) { blankserver(&servers[srvct]); srvedit = srvct; }
        else srvedit = 0;

    }
    ami_putwidgettext(srvwf, SRVNAME, servers[srvedit].name);
    sprintf(num, "%ld", pollsec);
    ami_putwidgettext(srvwf, SRVPOLL, num);
    ami_putwidgettext(srvwf, SRVIMAP, servers[srvedit].imap);
    sprintf(num, "%ld", servers[srvedit].imapport);
    ami_putwidgettext(srvwf, SRVIPORT, num);
    ami_putwidgettext(srvwf, SRVSMTP, servers[srvedit].smtp);
    sprintf(num, "%ld", servers[srvedit].smtpport);
    ami_putwidgettext(srvwf, SRVSPORT, num);
    ami_putwidgettext(srvwf, SRVUSER, servers[srvedit].user);
    ami_putwidgettext(srvwf, SRVPASS, servers[srvedit].pass);
    sprintf(num, "%ld", servers[srvedit].limit);
    ami_putwidgettext(srvwf, SRVLIMIT, num);

}

/* take what was typed and keep it */
static void srvsave(void)

{

    char    s[MAXSTR];
    srvrec* r;

    if (srvedit >= MAXSRV) return;
    if (srvedit >= srvct) srvct = srvedit+1; /* a new one is kept */
    r = &servers[srvedit];
    ami_getwidgettext(srvwf, SRVNAME, s, sizeof(s));
    trim(s);
    if (*s && strcmp(s, r->name)) { /* it has been renamed */

        char was[64];

        copystr(was, r->name, sizeof(was));
        copystr(r->name, s, sizeof(r->name));
        /* The mailboxes are named for their account, so they follow it.
           Without this a rename orphans everything the account has
           fetched and it all comes down again under the new name. */
        renamestore(was, r->name);

    }
    ami_getwidgettext(srvwf, SRVIMAP, s, sizeof(s));
    trim(s);
    copystr(r->imap, s, MAXSTR);
    ami_getwidgettext(srvwf, SRVIPORT, s, sizeof(s));
    if (atol(s) > 0) r->imapport = atol(s);
    ami_getwidgettext(srvwf, SRVSMTP, s, sizeof(s));
    trim(s);
    copystr(r->smtp, s, MAXSTR);
    ami_getwidgettext(srvwf, SRVSPORT, s, sizeof(s));
    if (atol(s) > 0) r->smtpport = atol(s);
    ami_getwidgettext(srvwf, SRVUSER, s, sizeof(s));
    trim(s);
    copystr(r->user, s, MAXSTR);
    /* the password is taken as typed, since a space may be part of it */
    ami_getwidgettext(srvwf, SRVPASS, s, sizeof(s));
    copystr(r->pass, s, MAXSTR);
    ami_getwidgettext(srvwf, SRVLIMIT, s, sizeof(s));
    if (atol(s) > 0) r->limit = atol(s);
    ami_getwidgettext(srvwf, SRVPOLL, s, sizeof(s));
    if (atol(s) >= 0) pollsec = atol(s);
    writeaccount();
    useserver(0);
    /* the periodic look follows whatever was just set */
    ami_killtimer(stdout, TIMPOLL);
    if (pollsec > 0) ami_timer(stdout, TIMPOLL, pollsec*10000, TRUE);

}

static void srvclose(void)

{

    if (srvwf) { fclose(srvwf); srvwf = NULL; }

}

static void srvopen(void)

{

    long wx, wy;
    long ew, eh, bw, bh;
    long i;

    if (srvwf) { ami_front(srvwf); return; }
    ami_openwin(&stdin, &srvwf, NULL, SRVWIN);
    ami_title(srvwf, "Mail server");
    ami_buffer(srvwf, FALSE);
    ami_auto(srvwf, FALSE);
    ami_curvis(srvwf, FALSE);
    ami_font(srvwf, AMI_FONT_SIGN);
    ami_setpoints(srvwf, 11.0);
    ami_binvis(srvwf);
    ami_editboxsizg(srvwf, "0", &ew, &eh);
    ami_buttonsizg(srvwf, "Cancel", &bw, &bh);
    {

        /* Wide enough for whichever is wider: the fields with their
           notes beside them, or the row of buttons under them. The row
           was running off the edge, and the last button with it -- a
           form that will not show what it offers. */
        long chrw = ami_strsiz(srvwf, "0");
        long labw = ami_strsiz(srvwf, "Sending server  ");
        long need = chrw*2+labw+(bw+chrw*2)*4+bw+chrw*2;
        long want = chrw*96;

        if (need > want) want = need;
        ami_winclientg(srvwf, want, (eh+chrh/2)*SRVFLDS+bh*2+chrh*8, &wx, &wy,
                       BIT(ami_wmframe) | BIT(ami_wmsize) |
                       BIT(ami_wmsysbar));

    }
    ami_setsizg(srvwf, wx, wy);
    ami_setposg(srvwf, 120, 120);
    /* made here, placed by the layout, which runs again on a resize */
    for (i = 0; i < SRVFLDS; i++)
        ami_editboxg(srvwf, 1, 1, 2, 2, srvfld[i].id);
    ami_buttong(srvwf, 1, 1, 2, 2, "Save", SRVOK);
    ami_buttong(srvwf, 1, 1, 2, 2, "Cancel", SRVCAN);
    ami_buttong(srvwf, 1, 1, 2, 2, "Next", SRVNEXT);
    ami_buttong(srvwf, 1, 1, 2, 2, "Add", SRVNEW);
    ami_buttong(srvwf, 1, 1, 2, 2, "Remove", SRVDEL);
    ami_checkboxg(srvwf, 1, 1, 2, 2, "Send mail from this account", SRVSEND);
    srvlay();
    srvload();

}

/* an event with the form's window id on it */
static void srvevent(ami_evtrec* er)

{

    switch (er->etype) {

        case ami_etterm: srvclose(); break;

        case ami_etredraw:
        case ami_etresize: srvlay(); break;

        case ami_etchkbox:
            if (er->ckbxid == SRVSEND) {

                /* Ticking it names this account; unticking it would
                   leave nothing able to send, so the box goes back on
                   and says so by staying on. */
                sendsrv = srvedit;
                ami_selectwidget(srvwf, SRVSEND, TRUE);
                writeaccount();

            }
            break;

        case ami_etbutton:
            if (er->butid == SRVNEXT) { /* show the one after this */

                srvsave();
                srvedit = srvct? (srvedit+1)%srvct: 0;
                srvlay();
                srvload();

            } else if (er->butid == SRVNEW) { /* another account */

                srvsave();
                if (srvct < MAXSRV) srvedit = srvct;
                srvlay();
                srvload();

            } else if (er->butid == SRVDEL) { /* take this one away */

                long i;

                if (srvedit < srvct) {

                    for (i = srvedit; i+1 < srvct; i++)
                        servers[i] = servers[i+1];
                    srvct--;

                }
                if (srvedit >= srvct) srvedit = srvct? srvct-1: 0;
                writeaccount();
                srvlay();
                srvload();

            } else if (er->butid == SRVOK) {

                srvsave();
                srvclose();
                status(haveaccount()?
                       "Account saved. Mail/Get Mail reads the server.":
                       "The server, the user and the password are all "
                       "needed before mail can be fetched.");

            } else srvclose();
            break;

        default: break;

    }

}

/*******************************************************************************

Layout

The panes are placed on the main window and moved when it is resized.
The folder pane takes a fixed width, since folder names are short and a
folder list that grows with the window is wasted space.

*******************************************************************************/

static void layout(void)

{

    long top = 1+banh; /* under the banner, which is under the menu */
    long h = ami_maxyg(stdout)-top-stath; /* the strip has the foot of it */

    /* The banner is as wide as the window and stays where it is put. */
    ami_setposg(banwf, 1, 1);
    ami_setsizg(banwf, ami_maxxg(stdout), banh);
    ami_sizbufg(banwf, ami_maxxg(stdout), banh);

    if (diag) fprintf(stderr, "layout: buf %ldx%ld stath %ld progh %ld "
                      "panes %ld tall\n", ami_maxxg(stdout), ami_maxyg(stdout),
                      stath, progh, h);

    /* the main window shows between and around the panes, so it is
       cleared here rather than left as whatever was under it */
    fprintf(stdout, "\f");

    foldw = ami_strsiz(stdout, "0")*22;
    if (foldw > ami_maxxg(stdout)/2) foldw = ami_maxxg(stdout)/2;
    ami_setposg(foldwf, 1, top);
    /* The buffer follows the window. Sizing the window alone leaves the
       buffer the size it was, and a buffered window answers with its
       buffer -- so everything drawn from its measurements would be laid
       out for the pane it used to be. */
    ami_setsizg(foldwf, foldw, h);
    ami_sizbufg(foldwf, foldw, h);
    /* the gap between the panes holds the divider between the sections */
    listx = foldw+8;
    listy = top;
    ami_setposg(listwf, listx, listy);
    ami_setsizg(listwf, ami_maxxg(stdout)-foldw-8, h);
    ami_sizbufg(listwf, ami_maxxg(stdout)-foldw-8, h);
    divider(stdout, foldw+4, top, foldw+4, top+h);
    /* the columns of the list, kept here so the rows and their dividers
       agree on where the columns are */
    fromx = ami_strsiz(listwf, "0")*18;
    catx = fromx+ami_strsiz(listwf, "promotions  ");
    datex = ami_maxxg(listwf)-sbw-ami_strsiz(listwf, "Sep 30, 2025 ");
    /* The bar down the right of the message list is moved and sized, not
       made again: a widget id is taken until the widget is killed, so
       making it a second time is an error, and a resize would raise it. */
    ami_poswidgetg(listwf, SBLIST, ami_maxxg(listwf)-sbw, 1);
    ami_sizwidgetg(listwf, SBLIST, sbw, ami_maxyg(listwf));
    listrows = ami_maxyg(listwf)/rowh;
    if (listrows < 1) listrows = 1;
    /* The panes are cleared and drawn again, since this is the one thing
       that moves what is in them: a pane that has changed height has the
       old rows where the new ones are not, and a buffer keeps whatever
       nothing has drawn over. Once per layout, which is once per resize
       -- not once per redraw, which is what made a resize flash. */
    fprintf(foldwf, "\f");
    fprintf(listwf, "\f");
    drawfolders();
    drawlist();
    drawstatus();
    drawbanner();

}

/*******************************************************************************

Help topics

A window of the program's own, not a dialog. A dialog stops the program
and runs a loop of its own until it is answered; help is not a question,
so it opens beside the mail and stays up while the mail is worked on.

That takes no machinery. There is one event queue for the program, and
every event names the window it came from, so the main loop tells the
help window's events from the mail's by their window id and hands them
here. Nothing is nested, and no thread is needed -- though one could be
put in charge of this window just as well, since its state is all here.

Widgets are numbered within their window, so the search entry, the topic
list and the close button are 1, 2 and 3 here even though the mail's
scroll bars are 1 and 2 there.

*******************************************************************************/

/* A topic is a title and the text under it, both pointing into the
   block the help file was read into. */
typedef struct { char* title; char* text; } helprec;

/* The wrapped text, one entry per line as it appears on the screen. The
   text is wrapped once, when the topic is picked or the window resized,
   and drawn from there, which is what makes it scrollable. */
typedef struct { char* s; int bold; long ind; } helpline;

static FILE*     helpwf;      /* the help window, NULL when closed */
static char*     helpbuf;     /* the help file, read whole */
static helprec*  helptopics;  /* the topics in it */
static long      helptopicct;
static long*     helpmatch;   /* the topics the search matched */
static long      helpmatches; /* how many of them */
static long      helpsel;     /* the topic shown, -1 for none */
static long      helpx0, helpy0; /* the topic list, in pixels */
static long      helpx1, helpy1;
static int       helplistup;  /* the list box has been made */
static helpline* helplines;   /* the topic, wrapped to the pane */
static long      helplinect;
static long      helplinemax;
static long      helptop;     /* first wrapped line shown */
static long      helppage;    /* wrapped lines the pane holds */
static char*     mailprog;    /* argv[0], to find the help file by */

static void helpout(const char* s, int bold, long ind); /* forward */

/*******************************************************************************

Reading the help file

The file is markdown, of the plain kind: a line beginning with a single
# names a topic and everything after it belongs to that topic until the
next one or the end of the file. Within a topic a blank line separates
paragraphs, ## is a heading inside the topic and - is a list item.

Keeping the text in a file rather than in the program means the help can
be rewritten, corrected or translated without a compiler, and it can be
as long as it deserves to be. The file is read whole and the titles and
texts point into it, so a topic costs two pointers.

*******************************************************************************/

/* is this the start of a topic: one #, then a space, then a title? */
static int helphead(const char* p)

{

    return (*p == '#' && p[1] != '#');

}

/* Find and read the help file. It is looked for beside the program
   first, since that is where an installed program's own files belong,
   then in the source directory it is kept in, then where the program
   was run from. The first one found is the one used. */
static int helpread(void)

{

    char  path[600];
    char  dir[500];
    char* e;
    FILE* f = NULL;
    long  i, n;

    /* the directory the program was run from, with its slash */
    dir[0] = 0;
    if (mailprog) {

        snprintf(dir, sizeof(dir), "%s", mailprog);
        e = strrchr(dir, '/');
        if (e) e[1] = 0; else dir[0] = 0;

    }
    for (i = 0; i < 4 && !f; i++) {

        switch (i) {

            /* beside the program, then the source beside that, which is
               where it sits in a build tree, then the two the same way
               from wherever the program was run */
            case 0: snprintf(path, sizeof(path), "%s%s", dir, HELPFILE);
                    break;
            case 1: snprintf(path, sizeof(path), "%s../graph_programs/%s",
                             dir, HELPFILE); break;
            case 2: snprintf(path, sizeof(path), "%s", HELPFILE); break;
            default: snprintf(path, sizeof(path), "graph_programs/%s",
                              HELPFILE); break;

        }
        f = fopen(path, "r");
        if (diag) fprintf(stderr, "help: %s: %s\n", path, f? "found": "no");

    }
    if (!f) return (FALSE);
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    helpbuf = malloc(n+1);
    if (!helpbuf) { ami_alert("Mail", "Out of memory"); exit(1); }
    n = fread(helpbuf, 1, n, f);
    helpbuf[n] = 0;
    fclose(f);

    return (TRUE);

}

/* Break the file into topics. The heads are found first and the block
   cut afterwards, in that order: cutting as it went would overwrite the
   newline that the test for the next head stands on, and every other
   topic would be passed over. */
static void helpsplit(void)

{

    char*  p;
    char** head; /* the # of each topic */
    long   n, i;

    /* count the heads, then take them, walking by lines both times */
    n = 0;
    for (p = helpbuf; *p; ) {

        if (helphead(p)) n++;
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }
    head = malloc((n+1)*sizeof(char*));
    helptopics = malloc((n+1)*sizeof(helprec));
    helpmatch = malloc((n+1)*sizeof(long));
    if (!head || !helptopics || !helpmatch)
        { ami_alert("Mail", "Out of memory"); exit(1); }
    n = 0;
    for (p = helpbuf; *p; ) {

        if (helphead(p)) head[n++] = p;
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }
    helptopicct = n;
    /* the title is the rest of the head line, the text is what follows */
    for (i = 0; i < n; i++) {

        char* t = head[i]+1;
        char* e;

        while (*t == ' ') t++;
        helptopics[i].title = t;
        e = t;
        while (*e && *e != '\n') e++;
        if (*e) *e++ = 0; /* end the title */
        while (*e == '\n') e++; /* and the blank line under it */
        helptopics[i].text = e;

    }
    /* Now the cutting: each text ends where the next topic begins. The
       last ends at the end of the file, which is already a zero. */
    for (i = 0; i+1 < n; i++) *head[i+1] = 0;

}

/* load the help, once, and say so if there is none */
static void helpload(void)

{

    static char nofile[400];

    if (helptopics) return; /* already loaded */
    if (helpread()) helpsplit();
    if (!helptopicct) { /* no file, or a file with no topics in it */

        snprintf(nofile, sizeof(nofile),
                 "The help file %s was not found, or holds no topics.\n"
                 "\n"
                 "It is looked for beside the program and in the "
                 "graph_programs directory of the source. Help is kept in "
                 "a file so that it can be changed without rebuilding the "
                 "program; if the file is missing, only the help is.",
                 HELPFILE);
        helptopics = malloc(sizeof(helprec));
        helpmatch = malloc(sizeof(long));
        if (!helptopics || !helpmatch)
            { ami_alert("Mail", "Out of memory"); exit(1); }
        helptopics[0].title = "No help file";
        helptopics[0].text = nofile;
        helptopicct = 1;

    }

}

/*******************************************************************************

The topic list

*******************************************************************************/

/* How many times does the topic hold the text, in either case? A count
   rather than a yes or no, because the list shows it: a topic the word
   is the subject of holds it many times, and one that merely mentions
   it in passing holds it once, and the reader can tell them apart
   without opening either. */
static long helpcount(const helprec* h, const char* what)

{

    const char* p;
    long        n = strlen(what);
    long        c = 0;

    if (!n) return (0); /* an empty search matches everything, uncounted */
    for (p = h->title; *p; p++)
        if (!strncasecmp(p, what, n)) c++;
    for (p = h->text; *p; p++)
        if (!strncasecmp(p, what, n)) c++;

    return (c);

}

/* Build the list of topics matching the search and put it in the list
   box. The list box is made again rather than changed, since a list box
   is given its contents when it is made; it copies them, so the list
   built here is ours to free. */
static void helpfill(const char* what)

{

    ami_strptr sl = NULL, sp, lp = NULL;
    long       i, c;
    char       lab[300];

    /* the strings are ours until the list box has them; it copies */
    helpmatches = 0;
    for (i = 0; i < helptopicct; i++) {

        c = helpcount(&helptopics[i], what);
        if (*what && !c) continue; /* not this one */
        /* the count goes beside the title, so that a topic the word is
           the subject of can be told from one that mentions it once */
        if (*what) snprintf(lab, sizeof(lab), "%s (%ld)",
                            helptopics[i].title, c);
        else snprintf(lab, sizeof(lab), "%s", helptopics[i].title);
        sp = malloc(sizeof(ami_strrec));
        if (!sp) { ami_alert("Mail", "Out of memory"); exit(1); }
        sp->str = strdup(lab);
        if (!sp->str) { ami_alert("Mail", "Out of memory"); exit(1); }
        sp->next = NULL;
        if (lp) lp->next = sp; else sl = sp;
        lp = sp;
        helpmatch[helpmatches++] = i;

    }
    /* a search that matches nothing still needs a list, or there would
       be no box to type the next search against */
    if (!sl) {

        sl = malloc(sizeof(ami_strrec));
        if (!sl) { ami_alert("Mail", "Out of memory"); exit(1); }
        sl->str = strdup("(no topic matches)");
        if (!sl->str) { ami_alert("Mail", "Out of memory"); exit(1); }
        sl->next = NULL;

    }
    if (helplistup) ami_killwidget(helpwf, HELPLIST);
    ami_listboxg(helpwf, helpx0, helpy0, helpx1, helpy1, sl, HELPLIST);
    helplistup = TRUE;
    while (sl) { sp = sl->next; free(sl->str); free(sl); sl = sp; }
    /* the topic shown is only still shown if the search kept it */
    if (helpsel >= 0) {

        for (i = 0; i < helpmatches; i++) if (helpmatch[i] == helpsel) break;
        if (i >= helpmatches) helpsel = -1;

    }

}

/*******************************************************************************

Laying the topic out

The text is wrapped to the pane once and kept as lines, so that drawing
it, scrolling it and knowing how much of it there is are all the same
small piece of work. It is wrapped again when the window is resized,
which is the only thing that can change the answer.

*******************************************************************************/

/* keep one finished line */
static void helpout(const char* s, int bold, long ind)

{

    if (helplinect >= helplinemax) {

        helplinemax = helplinemax? helplinemax*2: 100;
        helplines = realloc(helplines, helplinemax*sizeof(helpline));
        if (!helplines) { ami_alert("Mail", "Out of memory"); exit(1); }

    }
    helplines[helplinect].s = strdup(s);
    if (!helplines[helplinect].s)
        { ami_alert("Mail", "Out of memory"); exit(1); }
    helplines[helplinect].bold = bold;
    helplines[helplinect].ind = ind;
    helplinect++;

}

/* Wrap one paragraph, which arrives as a single string with its line
   breaks already turned into spaces. The break goes at the last word
   that still fits, and fitting is measured with the font rather than
   counted in characters, since the font is not fixed pitch. */
static void helpwrap(const char* s, int bold, long ind, long w)

{

    char line[500];
    char try[500];
    long n;

    ami_bold(helpwf, bold);
    while (*s) {

        const char* q = s;

        n = 0;
        line[0] = 0;
        while (*q) { /* as many whole words as fit */

            const char* e = q;
            long        m;

            while (*e && *e != ' ') e++; /* the next word */
            m = e-s;
            if (m >= (long)sizeof(try)) break;
            memcpy(try, s, m);
            try[m] = 0;
            if (n && ami_strsiz(helpwf, try) > w-ind) break;
            strcpy(line, try);
            n = m;
            q = e;
            while (*q == ' ') q++;
            if (!*e) break;

        }
        if (!n) { /* one word wider than the pane: put it down anyway */

            while (*q && *q != ' ') q++;
            n = q-s;
            if (n >= (long)sizeof(line)) n = sizeof(line)-1;
            memcpy(line, s, n);
            line[n] = 0;

        }
        helpout(line, bold, ind);
        s += n;
        while (*s == ' ') s++;

    }
    ami_bold(helpwf, FALSE);

}

/* Wrap the topic being shown to the pane. The markdown is read here: a
   blank line ends a paragraph, ## is a heading within the topic, and -
   is a list item, which is wrapped with its later lines lined up under
   the first word rather than under the dash. */
static void helplay1(long w)

{

    const char* p;
    char        para[4000];
    long        pl = 0;
    int         bold = FALSE;
    long        ind = 0;
    long        i;

    for (i = 0; i < helplinect; i++) free(helplines[i].s);
    helplinect = 0;
    helptop = 0;
    if (helpsel < 0) { helpout("Pick a topic.", FALSE, 0); return; }
    /* the title of the topic, in bold, and a line under it */
    helpout(helptopics[helpsel].title, TRUE, 0);
    helpout("", FALSE, 0);
    p = helptopics[helpsel].text;
    while (1) {

        const char* e = p;
        long        n;

        while (*e && *e != '\n') e++;
        n = e-p;
        while (n && (p[n-1] == ' ' || p[n-1] == '\r')) n--; /* trailing */
        if (!n || *p == '#' || *p == '-' || *p == '*' || !*p) {

            /* the line before is finished, whatever this one is */
            if (pl) { para[pl] = 0; helpwrap(para, bold, ind, w); pl = 0; }
            bold = FALSE;
            ind = 0;

        }
        if (!*p) break;
        if (!n) helpout("", FALSE, 0); /* a blank line stays blank */
        else if (*p == '#') { /* a heading inside the topic */

            const char* t = p;

            while (*t == '#') t++;
            while (*t == ' ') t++;
            bold = TRUE;
            n -= t-p;
            if (n > (long)sizeof(para)-1) n = sizeof(para)-1;
            memcpy(para, t, n);
            pl = n;

        } else if (*p == '-' || *p == '*') { /* a list item */

            ind = ami_strsiz(helpwf, "00");
            if (n > (long)sizeof(para)-1) n = sizeof(para)-1;
            memcpy(para, p, n);
            pl = n;

        } else { /* ordinary text, joined to the line before it */

            if (pl && pl < (long)sizeof(para)-1) para[pl++] = ' ';
            if (pl+n > (long)sizeof(para)-1) n = sizeof(para)-1-pl;
            memcpy(para+pl, p, n);
            pl += n;

        }
        p = *e? e+1: e;

    }

}

/* draw the topic from the line it is scrolled to */
static void helpdraw(void)

{

    long chrh = ami_chrsizy(helpwf);
    long x = helpx1+ami_strsiz(helpwf, "00");
    long y = helpy0;
    long i;

    /* down to and including the line the count is written on, which is
       under the pane: leave it out and each count is written over the
       one before it */
    ami_fcolor(helpwf, ami_white);
    ami_frect(helpwf, x, helpy0, ami_maxxg(helpwf), helpy1+chrh);
    ami_fcolor(helpwf, ami_black);
    helppage = (helpy1-helpy0)/chrh;
    if (helppage < 1) helppage = 1;
    if (helptop > helplinect-helppage) helptop = helplinect-helppage;
    if (helptop < 0) helptop = 0;
    for (i = helptop; i < helplinect && y+chrh <= helpy1; i++) {

        if (*helplines[i].s) {

            ami_bold(helpwf, helplines[i].bold);
            ami_cursorg(helpwf, x+helplines[i].ind, y);
            fprintf(helpwf, "%s", helplines[i].s);
            ami_bold(helpwf, FALSE);

        }
        y += chrh;

    }
    /* say there is more, since a pane with no bar gives no other sign */
    if (helplinect > helppage) {

        char more[80];

        if (helptop+helppage >= helplinect) strcpy(more, "-- end --");
        else sprintf(more, "-- %ld more line%s, wheel or page keys --",
                     helplinect-helptop-helppage,
                     helplinect-helptop-helppage == 1? "": "s");
        ami_fcolor(helpwf, ami_blue);
        ami_cursorg(helpwf, x, helpy1);
        fprintf(helpwf, "%s", more);
        ami_fcolor(helpwf, ami_black);

    }

}

/* wrap and draw, which is what everything that changes the topic wants */
static void helptext(void)

{

    helplay1(ami_maxxg(helpwf)-ami_strsiz(helpwf, "00")-
             (helpx1+ami_strsiz(helpwf, "00")));
    helpdraw();

}

/* scroll the topic by so many lines */
static void helpscroll(long by)

{

    long was = helptop;

    helptop += by;
    if (helptop > helplinect-helppage) helptop = helplinect-helppage;
    if (helptop < 0) helptop = 0;
    if (helptop != was) helpdraw();

}

/* place the widgets and work out the panes, on opening and on resize */
static void helplay(void)

{

    long chrh = ami_chrsizy(helpwf);
    long chrw = ami_strsiz(helpwf, "0");
    long lw   = chrw*30;  /* the topic list */
    long bw, bh, ew, eh;

    ami_buttonsizg(helpwf, "Close", &bw, &bh);
    ami_editboxsizg(helpwf, "0", &ew, &eh);
    /* the search entry along the top of the list column */
    helpx0 = chrw*2;
    helpy0 = chrh*2+eh;
    helpx1 = helpx0+lw;
    helpy1 = ami_maxyg(helpwf)-bh-chrh*2;
    if (helpy1 < helpy0+chrh) helpy1 = helpy0+chrh;
    ami_poswidgetg(helpwf, HELPFIND, helpx0+ami_strsiz(helpwf, "Search: "),
                   chrh);
    ami_sizwidgetg(helpwf, HELPFIND,
                   lw-ami_strsiz(helpwf, "Search: "), eh);
    ami_poswidgetg(helpwf, HELPCLOSE, ami_maxxg(helpwf)-bw-chrw*2,
                   ami_maxyg(helpwf)-bh-chrh/2);
    ami_sizwidgetg(helpwf, HELPCLOSE, bw, bh);
    /* The list is moved and sized rather than made again, so that the
       topic picked in it stays picked across a resize. Only a change
       of contents needs it made again, which is what helpfill is for. */
    if (helplistup) {

        ami_poswidgetg(helpwf, HELPLIST, helpx0, helpy0);
        ami_sizwidgetg(helpwf, HELPLIST, helpx1-helpx0, helpy1-helpy0);

    } else helpfill("");
    /* the frame, the label, and the topic */
    fprintf(helpwf, "\f");
    ami_fcolor(helpwf, ami_black);
    ami_cursorg(helpwf, helpx0, chrh);
    fprintf(helpwf, "Search:");
    helptext();

}

/* close the help window, if it is open */
static void helpclose(void)

{

    long i;

    if (!helpwf) return;
    fclose(helpwf);
    helpwf = NULL;
    helplistup = FALSE;
    for (i = 0; i < helplinect; i++) free(helplines[i].s);
    helplinect = 0;

}

/* Open the help window. A second open just brings the one already up to
   the front, as help does everywhere. */
static void helpopen(void)

{

    long wx, wy;

    if (helpwf) { ami_front(helpwf); return; }
    helpload();
    helpsel = -1;
    helptop = 0;
    ami_openwin(&stdin, &helpwf, NULL, HELPWIN);
    ami_title(helpwf, "Spreadsheet help");
    /* Unbuffered, so the window's measurements are the window's. A
       buffered window answers maxxg with the buffer, which is not what
       the layout wants here: this window has nothing to keep. */
    ami_buffer(helpwf, FALSE);
    ami_auto(helpwf, FALSE);
    ami_curvis(helpwf, FALSE);
    ami_font(helpwf, AMI_FONT_SIGN);
    ami_setpoints(helpwf, 12.0);
    ami_binvis(helpwf);
    ami_winclientg(helpwf, ami_strsiz(helpwf, "0")*86, ami_chrsizy(helpwf)*26,
                   &wx, &wy, BIT(ami_wmframe) | BIT(ami_wmsize) |
                             BIT(ami_wmsysbar));
    ami_setsizg(helpwf, wx, wy);
    /* The entry and the button are made once here and moved by the
       layout after. The list is made by the layout, which is where its
       rectangle is known and where it is made again on every search. */
    ami_editboxg(helpwf, 1, 1, 2, 2, HELPFIND);
    ami_buttong(helpwf, 1, 1, 2, 2, "Close", HELPCLOSE);
    helplay();

}

/* An event with the help window's id on it. The main loop hands them
   here and goes on; nothing about the mail is touched. */
static void helpevent(ami_evtrec* er)

{

    char srch[100];

    switch (er->etype) {

        case ami_etterm:   /* the window was closed, not the program */
        case ami_etbutton: helpclose(); break;

        case ami_etresize:
        case ami_etredraw: helplay(); break;

        case ami_etmouba: /* the wheel, as buttons 4 and 5 */
            if (er->amoubn == 4) helpscroll(-WHEELROWS);
            else if (er->amoubn == 5) helpscroll(WHEELROWS);
            break;

        case ami_etpagu: helpscroll(-(helppage-1)); break;
        case ami_etpagd: helpscroll(helppage-1); break;
        case ami_etup:   helpscroll(-1); break;
        case ami_etdown: helpscroll(1); break;
        case ami_ethome: helpscroll(-helplinect); break;
        case ami_etend:  helpscroll(helplinect); break;

        case ami_etlstbox: /* a topic was picked */
            if (er->lstbsl >= 1 && er->lstbsl <= helpmatches)
                helpsel = helpmatch[er->lstbsl-1];
            helptext();
            break;

        case ami_etedtbox: /* the search was entered */
            ami_getwidgettext(helpwf, HELPFIND, srch, sizeof(srch));
            helpfill(srch);
            helptext();
            break;

        default: break;

    }

}

/*******************************************************************************
/*******************************************************************************

The menu

*******************************************************************************/

static int reachable(const char* host, long port, long secs);
static void newmenu(ami_menuptr* mp, int onoff, int bar, int select,
                    long id, char* face)

{

    *mp = malloc(sizeof(ami_menurec));
    if (!*mp) { fail("Out of memory"); exit(1); }
    (*mp)->next = NULL;
    (*mp)->branch = NULL;
    (*mp)->onoff = onoff;
    (*mp)->oneof = FALSE;
    (*mp)->bar = bar;
    (*mp)->id = id;
    (*mp)->face = malloc(strlen(face)+1);
    if (!(*mp)->face) { fail("Out of memory"); exit(1); }
    strcpy((*mp)->face, face);

}

static void appendmenu(ami_menuptr* list, ami_menuptr m)

{

    ami_menuptr lp;

    if (!*list) *list = m;
    else { lp = *list; while (lp->next) lp = lp->next; lp->next = m; }

}

static void setupmenu(void)

{

    ami_menuptr ml = NULL;
    ami_menuptr ma;
    ami_menuptr mp;
    ami_menuptr sm = NULL;

    /* No File menu. All it held was Exit, which the window's own close
       box does, in the corner every window has it in. Get Mail is what
       this program is for, so it stands on the bar rather than inside
       something. */
    newmenu(&mp, FALSE, FALSE, OFF, MENUCOMP, "Compose");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, MENUFETCH, "Get Mail");
    appendmenu(&ml, mp);
    newmenu(&ma, FALSE, FALSE, OFF, MENUMAIL, "Config");
    appendmenu(&ml, ma);
    ami_stdmenu(BIT(AMI_SMHELPTOPIC) | BIT(AMI_SMABOUT), &sm, ml);
    /* as in the spreadsheet, the branch is hung on after the standard
       menu is built, since building it clears the branch link */
    newmenu(&mp, FALSE, FALSE, OFF, MENUSRV, "Servers...");
    appendmenu(&ma->branch, mp);
    newmenu(&mp, FALSE, FALSE, ON, MENUFOLD, "Refresh Folder List");
    appendmenu(&ma->branch, mp);
    newmenu(&mp, FALSE, FALSE, ON, MENUCHECK, "Check Sending");
    appendmenu(&ma->branch, mp);
    ami_menu(stdout, sm);

}

/*******************************************************************************

Fetching

*******************************************************************************/

/*******************************************************************************

Fetching, a message at a time

A fetch of a real mailbox is thousands of messages and takes as long as
it takes. Done in one loop it would hold the event loop shut for all of
that time, and a window that is not in its event loop is a window that
shows nothing: the library puts its drawing on the wire in ami_event and
nowhere else, so a progress line written from inside such a loop is
written to a screen that never repaints. The window would sit there,
frozen and blank, for hours.

So the fetch is a state machine, stepped by a timer. Each tick takes one
message and returns, which keeps the event loop turning: the count
appears as it climbs, the window redraws, the folders can be clicked
through while it runs, and it can be stopped.

*******************************************************************************/

static int  fetching;    /* a fetch is under way */
static long fetchsrv;    /* the server being read */
static long fetchfold;   /* where the stepping has got to in the order */
static long fetchcur = -1; /* and the folder that place names */
static long fetchi;      /* which of that folder's uids is next */
static long fetchgot;    /* messages taken, this fetch */
static long fetchdup;    /* and passed over as already here */
static long fetchlast;   /* the highest uid taken from this folder */
static long fetchseen;   /* the highest taken before this fetch */
static long fetchlow;    /* and the lowest, which together say what is
                            already here rather than only how far up */
static long fetchnewlow; /* the lowest this fetch has reached */

/*******************************************************************************

Leaving a server alone after it stops answering

A server that has gone quiet is not helped by being asked again a
quarter of a minute later, and a server that has gone quiet because it
has had enough of us -- which is what tens of thousands of messages
earns from a large provider -- is made worse by it. So a failure puts
that account aside for a while, and the while doubles each time: a
quarter minute, then half, then a minute, up to a quarter of an hour.
Anything that arrives puts it back to nothing.

This is the whole of the retry policy, and it is deliberately not a loop
around the read. The fetch is already resumable -- every folder
remembers the stretch it has taken, and every message is known by its
digest -- so the next look IS the retry, and it costs nothing to let it
be the next scheduled one. A retry inside the fetch would only be the
same request sent sooner, to a server that has just declined to answer.

*******************************************************************************/

#define BACKOFF   15  /* seconds to wait after the first failure */
#define BACKMAX   900 /* and never more than a quarter of an hour */

static time_t srvquiet[MAXSRV]; /* not to be asked before this time */
static long   srvwait[MAXSRV];  /* how long it was left alone last time */

/* that account has stopped answering */
static void serverfailed(long srv)

{

    if (srv < 0 || srv >= MAXSRV) return;
    srvwait[srv] = srvwait[srv]? srvwait[srv]*2: BACKOFF;
    if (srvwait[srv] > BACKMAX) srvwait[srv] = BACKMAX;
    srvquiet[srv] = time(NULL)+srvwait[srv];
    if (diag) fprintf(stderr, "! %s left alone for %lds\n",
                      servers[srv].name, srvwait[srv]);

}

/* and that one is talking again */
static void serverspoke(long srv)

{

    if (srv < 0 || srv >= MAXSRV) return;
    srvwait[srv] = 0;
    srvquiet[srv] = 0;

}

/* is this account being left alone just now? */
static int serverquiet(long srv)

{

    if (srv < 0 || srv >= MAXSRV) return (FALSE);

    return (srvquiet[srv] && time(NULL) < srvquiet[srv]);

}

/* say where the fetch has got to */
/* The folder pane is the progress display, its counts climbing as the
   messages land. The worker does not draw it -- it says that it wants
   drawing, and the main thread does it on the next tick. */
static void fetchsay(void)

{

    wrkfolds = TRUE;

}

/* Move to the next folder worth reading, and ask it what it holds.
   Gives FALSE when there are no folders left. */
/* The order the folders are fetched in: the accounts take turns, one
   folder each, round and round, rather than one account being finished
   before the next is begun. A big account and a small one arrive
   together that way; done in order, a mailbox of tens of thousands
   starves everything configured after it, and the second account sits
   at (not fetched) through cycle after cycle of the first.

   Worked out once at the start of a fetch, into an order the stepping
   walks straight down. */
static long fetchord[MAXFOLDER];
static long fetchordct;

static void fetchorder(void)

{

    long round;
    long i;

    fetchordct = 0;
    for (round = 0; round < MAXFOLDER; round++) {

        long took = 0;

        for (i = 0; i < srvct; i++) { /* one folder from each, in turn */

            long seen = 0;
            long k;

            if (serverquiet(i)) continue; /* it is not answering yet */

            for (k = 0; k < foldct; k++) {

                if (folders[k].srv != i) continue;
                if (folders[k].local || folders[k].noselect) continue;
                if (seen++ != round) continue;
                fetchord[fetchordct++] = k;
                took++;
                break;

            }

        }
        if (!took) break; /* every account is out of folders */

    }

}

static int fetchnext(void)

{

    char tag[20];
    long validity;
    long lo, hi;

    while (++fetchfold < fetchordct) {

        long fold = fetchord[fetchfold];

        if (folders[fold].srv != fetchsrv) { /* a different account */

            imapclose();
            fetchsrv = folders[fold].srv;
            if (fetchsrv < 0) continue;
            snprintf(wrkwhat, sizeof(wrkwhat), "Connecting to %s",
                     servers[folders[fold].srv].name);
            wrkpos = 0;
            wrkmax = 0;
            /* The server's name and password are copied out of the
               table the config window writes, so the copy is made with
               that window shut out of it. */
            dlock();
            useserver(fetchsrv);
            dunlock();
            if (!imapopen()) { serverfailed(fetchsrv); fetchsrv = -2; continue; }
            serverspoke(fetchsrv);

        }
        fetchi = 0;
        uidct = 0;
        fetchsay();
        snprintf(wrkwhat, sizeof(wrkwhat), "%s: %s",
                 fetchsrv >= 0? servers[fetchsrv].name: "", folders[fold].show);
        wrkpos = 0;
        wrkmax = 0;
        exists = 0;
        uidvalidity = 0;
        fetchcur = fold; /* the folder this step is working on */
        imsend(tag, sizeof(tag), "EXAMINE \"%s\"", folders[fold].name);
        if (!imwait(tag, examline)) continue; /* gone, or not a folder */
        if (!exists) continue; /* nothing in it */
        readstate(fold, &validity, &fetchlow, &fetchseen);
        /* a folder that was rebuilt on the server has new uids for the
           same messages, so what was taken before means nothing */
        if (validity != uidvalidity) { fetchlow = 0; fetchseen = 0; }
        fetchlast = fetchseen;
        fetchnewlow = 0;
        /* the most recent messages, as many as were asked for */
        hi = exists;
        lo = hi-limit+1;
        if (lo < 1) lo = 1;
        imsend(tag, sizeof(tag), "FETCH %ld:%ld (UID)", lo, hi);
        if (!imwait(tag, uidline)) continue;
        wrkmax = uidct; /* what this folder is offering */
        if (uidct) return (TRUE);

    }

    return (FALSE);

}

/* The fetch is over, however it ended. This runs on the worker, so it
   puts the connection down and says so; what the reader sees of the end
   is drawn by the main thread when it notices. */
static void fetchend(void)

{

    long i;

    imapclose();
    /* What arrived was put at the end of each folder's index as it came.
       The list is shown newest first, so the folders that took anything
       are put in order -- and the table that finds a message by its
       digest is taken again, since it names them by where they sit and
       sorting has moved them. */
    for (i = 0; i < foldct; i++) if (folders[i].dirty) {

        dlock();
        qsort(folders[i].idx, folders[i].idxct, sizeof(msgrec), bydate);
        folders[i].dirty = FALSE;
        useidx();
        dunlock();
        wrklist = TRUE;

    }
    rehashall();
    if (diag) fprintf(stderr, "fetch: %ld new, %ld already here\n",
                      fetchgot, fetchdup);
    wrkfolds = TRUE;
    wrklist = TRUE;
    wrkdone = TRUE;

}

/* One step: one message, or the move to the next folder. Called from
   the event loop on every tick of the timer, so that everything between
   the steps -- drawing, clicking, closing -- still works. */
static void fetchstep(void)

{

    char  tag[20];
    char  line[MAXLINE];
    long  uid;
    long  n;
    char* msg;

    if (wrkdone) return;
    while (fetchi >= uidct) { /* this folder is done */

        if (fetchcur >= 0 && fetchcur < foldct && uidct)
            writestate(fetchcur, uidvalidity,
                       fetchlow? fetchlow: fetchnewlow, fetchlast);
        if (!fetchnext()) { fetchend(); return; }

    }
    uid = uidlist[fetchi++];
    wrkpos = fetchi;
    /* Already here if it falls inside the stretch this folder has been
       read of. Below that stretch is mail older than anything taken so
       far, which is exactly what asking for more of a folder is for. */
    if (fetchlow && uid >= fetchlow && uid <= fetchseen)
        { if (!fetchnewlow || uid < fetchnewlow) fetchnewlow = uid;
          fetchsay(); return; }
    imsend(tag, sizeof(tag), "UID FETCH %ld (BODY.PEEK[])", uid);
    /* the reply is a line ending in a literal, then the message itself,
       then the rest of the reply and the tagged answer */
    if (!imline(line, sizeof(line))) {

        /* The server stopped answering. What has been taken is written
           and kept -- the marker for this folder goes down with the
           stretch reached -- and the fetch ends rather than reading a
           socket that is not going to speak. The next look picks it up
           from there. */
        if (fetchcur >= 0 && fetchcur < foldct)
            writestate(fetchcur, uidvalidity,
                       fetchlow? fetchlow: fetchnewlow, fetchlast);
        serverfailed(fetchsrv);
        status("The server stopped answering. What arrived is kept.");
        fetchend();

        return;

    }
    n = literalof(line);
    if (n < 0) { imwait(tag, NULL); return; } /* no message came */
    msg = imliteral(n);
    {

        char hex[DIGLEN];

        /* What the message is, rather than where the server filed it.
           A message already in this store is not written again, whatever
           folder it is in and whichever server it came from -- which is
           what happens when mail is moved here, or fetched twice, or
           arrives in two folders that are views of the same mail, as
           All Mail is of everything. */
        digestof(msg, n, hex);
        if (hasdigest(hex)) {

            free(msg);
            imwait(tag, NULL);
            if (uid > fetchlast) fetchlast = uid;
            if (!fetchnewlow || uid < fetchnewlow) fetchnewlow = uid;
            fetchdup++;
            fetchsay();

            return;

        }
        /* The message goes into the store, into the folder's index and
           into the table that finds it, with the display shut out of all
           three. This is the one place the worker writes what the main
           thread reads: half a message in a mailbox is what a reader
           would find if it read a folder in the middle of an append. */
        dlock();
        {

            long    off, stored = 0;
            msgrec* m;

            off = mboxwrite(folders[fetchcur].file, msg, n, &stored);
            if (off >= 0) {

                m = idxroom(fetchcur);
                fillrec(m, msg, n, stored, off, hex);
                adddigest(fetchcur, folders[fetchcur].idxct-1);
                /* written down as it arrives, so that a program stopped
                   halfway keeps what it had */
                idxappend(fetchcur, m);
                folders[fetchcur].msgs = folders[fetchcur].idxct;
                folders[fetchcur].dirty = TRUE;
                useidx(); /* the array may have moved as it grew */

            }

        }
        dunlock();

    }
    free(msg);
    fetchgot++;
    imwait(tag, NULL); /* the ) and the answer */
    if (uid > fetchlast) fetchlast = uid;
    if (!fetchnewlow || uid < fetchnewlow) fetchnewlow = uid;
    fetchsay();

}

/* start one */
/* The worker's half of a fetch: everything with a network in it, and
   everything slow enough to be worth keeping off the display. It runs
   on the fetch thread from beginning to end and draws nothing. */
static void fetchrun(void)

{

    long i;

    if (wrkrelist) {

        char wasname[MAXSTR];
        long wassrv = -1;

        /* Asking every server what folders it has, and rebuilding the
           table from the answers. The table is what the display reads,
           so the rebuild is done with the display shut out: a pane drawn
           halfway through would be drawn off a list with some servers in
           it and some not.

           This is the one place the display can be held up for as long
           as a server takes to answer, and it is why the timer never
           asks: Get Mail asks, and Get Mail is somebody waiting. */
        dlock();
        wasname[0] = 0;
        /* Keep which folder is being read: rebuilding the list throws
           the selection away, and a fetch that moved the reader back to
           the first folder every quarter minute would be unusable. */
        if (foldsel >= 0 && foldsel < foldct) {

            copystr(wasname, folders[foldsel].name, MAXSTR);
            wassrv = folders[foldsel].srv;

        }
        idxsetaside();
        foldct = 0;
        foldsel = -1;
        for (i = 0; i < srvct; i++) {

            if (!*servers[i].imap || !*servers[i].user) continue;
            /* If the server cannot be reached -- or is one being left
               alone after it stopped answering -- keep what the store
               knows of its folders rather than leaving the section
               empty: an account that is merely unreachable has not
               stopped having folders. */
            if (serverquiet(i)) { storefolders(i); continue; }
            snprintf(wrkwhat, sizeof(wrkwhat), "Asking %s what folders it has",
                     servers[i].name);
            wrkpos = 0;
            wrkmax = 0;
            if (!getfolders(i))
                { imapclose(); serverfailed(i); storefolders(i); continue; }
            imapclose(); /* one at a time: the connection is per server */

        }
        storefolders(-1); /* the locals rejoin, after the servers' */
        idxgiveback();    /* and every folder gets its index back */
        if (*wasname) /* put the reader back on the folder it was reading */
            for (i = 0; i < foldct; i++)
                if (folders[i].srv == wassrv &&
                    !strcmp(folders[i].name, wasname)) { foldsel = i; break; }
        dunlock();
        wrkfolds = TRUE;
        wrklist = TRUE;

    }
    /* Every folder that has not been read is read, and what they hold is
       what the store holds: the counts, and the digests that say whether
       an arriving message is already here. Reading is an index file
       each; only a folder without one costs anything, and only once. */
    countfolders();
    serveindex();
    wrkfolds = TRUE;
    if (wrkcount) return; /* reading the store was the whole job */
    if (diag) fprintf(stderr, "digests known: %ld over %ld folders\n",
                      digct, foldct);
    fetchsrv = -2; /* no server open yet */
    fetchorder();
    fetchfold = -1;
    fetchcur = -1;
    fetchi = 0;
    uidct = 0;
    fetchgot = 0;
    fetchdup = 0;
    /* Straight through, one message after another, with the reads
       blocking as reads do. There is no state machine any more and no
       timer setting the pace: that apparatus existed only to give the
       display a turn between messages, and the display has a thread of
       its own now. */
    while (!wrkdone && !wrkstop) { servesend(); serveindex(); fetchstep(); }

}

/* Send whatever the compose window has left to be sent. Between
   messages of a fetch as well as when nothing else is happening, so a
   message written during a long fetch goes now rather than after it. */
static void servesend(void)

{

    char to[MAXSTR], cc[MAXSTR], sub[MAXSTR], inreply[MAXSTR];
    char refs[MAXSTR*2];
    char err[MAXSTR*3];
    char* body;

    if (!sendwant) return;
    dlock();
    sendwant = FALSE;
    copystr(to, outto, sizeof(to));
    copystr(cc, outcc, sizeof(cc));
    copystr(sub, outsub, sizeof(sub));
    copystr(inreply, outinreply, sizeof(inreply));
    copystr(refs, outrefs, sizeof(refs));
    body = outbody;
    outbody = NULL;
    dunlock();
    snprintf(wrkwhat, sizeof(wrkwhat), "Sending to %s", to);
    wrkpos = 0;
    wrkmax = 0;
    sendmail(to, cc, sub, body? body: "", inreply, refs, err, sizeof(err));
    free(body);
    wrkwhat[0] = 0;
    if (*err) {

        copystr(failsaid, err, sizeof(failsaid));
        failwait = TRUE;
        sendfail = TRUE;

    }
    else copystr(sentsaid, "Message sent, and a copy put in Sent.",
                 sizeof(sentsaid));
    wrkfolds = TRUE;

}

/* Read whatever folder the display has asked for. Called from the idle
   loop and from between messages of a fetch, so that opening a folder
   answers while mail is arriving. */
static void serveindex(void)

{

    long i;

    while (!wrkstop) {

        long f = idxwant;

        if (f < 0) { /* nobody is waiting; read whatever wants reading */

            for (i = 0; i < foldct; i++) if (folders[i].wantidx) break;
            if (i >= foldct) break;
            f = i;

        }
        idxwant = -1;
        folders[f].wantidx = FALSE;
        idxdoing = f;
        snprintf(wrkwhat, sizeof(wrkwhat), "Reading %s", folders[f].show);
        wrkpos = 0;
        wrkmax = 0;
        indexfolder(f);
        idxdoing = -1;
        wrkwhat[0] = 0;
        wrkpos = 0;
        wrkmax = 0;
        wrklist = TRUE;

    }

}

/* The fetch thread. It is made once and lives as long as the program:
   the services thread table has no way to give an entry back, so a
   thread per fetch would run it out inside an hour of quarter-minute
   looks. It waits for work rather than being made for it. */
static void mailwork(void)

{

    while (TRUE) {

        wrkbusy = TRUE;
        servesend();
        serveindex();
        if (wrkgo) {

            wrkdone = FALSE;
            fetchrun();
            wrkdone = TRUE; /* however it ended */
            wrkgo = FALSE;

        }
        wrkbusy = FALSE;
        usleep(50000); /* a twentieth of a second, unnoticeable at a start */

    }

}

/* Set the worker going, and the timer that watches it. Everything that
   wants work done goes through here. */
static void kickworker(void)

{

    if (!wrkstart) { ami_newthread(mailwork); wrkstart = TRUE; }
    /* Ten times a second, to pick up what the worker has done and draw
       it. Faster than the eye and slower than the flicker that drawing
       per message would be. */
    ami_timer(stdout, TIMFETCH, 1000, TRUE);
    timerrun = TRUE;

}

/* start one */
static void fetchall(int relist)

{

    if (fetching) return; /* one at a time */
    if (!haveaccount()) return;
    /* A look on the timer does not ask the servers what folders they
       have. Folders do not come and go by the quarter minute, the asking
       costs a connection and a round trip to each of them, and
       rebuilding the list redraws the pane -- which is what made it
       flicker every time the timer came round. Get Mail asks; the timer
       only fetches. */
    if (!foldct) relist = TRUE; /* unless nothing is known yet */
    if (!relist) { /* an account nothing is known of has to be asked */

        long k;

        for (k = 0; k < srvct && !relist; k++) {

            long m;

            if (!*servers[k].imap || !*servers[k].user) continue;
            if (serverquiet(k)) continue; /* not while it is not answering */
            for (m = 0; m < foldct; m++) if (folders[m].srv == k) break;
            /* A newly added account has no folders here, and a look
               that only walks the folders it knows would never touch it
               -- so it would never get any, and never be looked at
               again. One that has none is asked. */
            if (m >= foldct) relist = TRUE;

        }

    }
    fetching = TRUE;
    fetchasked = !quietfail; /* the timer sets that flag; a menu does not */
    failwait = FALSE;
    wrkrelist = relist;
    wrkcount = FALSE;
    wrkdone = FALSE;
    wrkgo = TRUE; /* and it is off */
    kickworker();

}

/* Count the store without fetching anything. The counts are what the
   folder pane shows, and finding them means reading every mailbox
   through -- gigabytes, at startup, in front of somebody waiting for a
   window. It goes to the worker like everything else slow. */
static void countlater(void)

{

    if (fetching) return;
    fetching = TRUE;
    fetchasked = FALSE; /* nobody asked to be told about counting */
    wrkrelist = FALSE;
    wrkcount = TRUE;
    wrkdone = FALSE;
    wrkgo = TRUE;
    kickworker();

}

static void showfolder(long i);

/* What the worker has done, drawn. This is the main thread's whole part
   in a fetch: it looks ten times a second, draws what has changed, and
   tidies up when the worker says it has finished. */
static void fetchpick(void)

{

    long folds = wrkfolds;
    long list  = wrklist;
    long done  = wrkdone && !wrkgo;

    wrkfolds = FALSE;
    wrklist = FALSE;
    if (folds) drawfolders();
    if (*wrkwhat) { /* what it is doing, and how far into it */

        char t[MAXSTR*2];
        char a[40], b[40];

        if (wrkmax > 0) {

            commas(wrkpos, a, sizeof(a));
            commas(wrkmax, b, sizeof(b));
            snprintf(t, sizeof(t), "%s - %s of %s", wrkwhat, a, b);

        } else copystr(t, wrkwhat, sizeof(t));
        status(t);
        statprog(wrkpos, wrkmax);

    }
    if (*sentsaid) { status(sentsaid); *sentsaid = 0; }
    if (failwait) {

        failwait = FALSE;
        /* A look on the timer is nobody waiting, and a server that is
           down fails every one of them: an hour away from it would be
           an hour of boxes to dismiss. Those are left to the diagnostic
           and to the account being set aside. */
        /* A failure to send is always somebody waiting: they pressed
           Send and are owed an answer. */
        if (fetchasked || !fetching || sendfail) ami_alert("Mail", failsaid);
        sendfail = FALSE;

    }
    if (done) {

        char t[MAXSTR];

        fetching = FALSE;
        *wrkwhat = 0;
        statprog(0, 0); /* the bar empties: there is nothing running */
        if (wrkcount) status("");
        else {

            char a[40], b[40];

            commas(fetchgot, a, sizeof(a));
            commas(fetchdup, b, sizeof(b));
            snprintf(t, sizeof(t), "%s new, %s already here", a, b);
            status(t);

        }

    }
    if (list) { drawlist(); drawfolders(); } /* the worker read a folder */
    if (done) {

        /* Reading a folder means reading its whole mailbox, so it is
           asked for only when the mailbox has actually changed under the
           reader: a fetch that put nothing in the folder being read
           leaves what is on the screen right, and finding that out by
           reading four gigabytes is four gigabytes wasted. */
        /* Nothing is read again here. What arrived went into the
           folder's index as it was stored, so the list is already right;
           the worker has put it in order and the drawing above has shown
           it. */
        if (foldsel < 0 && foldct) showfolder(0);
        drawfolders();

    }
    /* The timer runs while there is anything to watch, and stops when
       there is not: a tick ten times a second forever is a program that
       never lets the machine alone. Everything the worker might be in
       the middle of has to be in this test, and something waiting to be
       said counts as well as something being done. Sending was not:
       the tick after Send found no fetch running and stopped the timer
       before the worker had even picked the message up, so what came
       back -- that it had gone, or why it had not -- was never looked
       at, and a message that never left looked exactly like one that
       did. */
    if (timerrun && !fetching && !wrkgo && !wrkbusy && idxwant < 0 &&
        !sendwant && !failwait && !*sentsaid && !*wrkwhat) {

        ami_killtimer(stdout, TIMFETCH);
        timerrun = FALSE;

    }

}

/* show a folder */
static void showfolder(long i)

{

    if (i < 0 || i >= foldct) return;
    foldsel = i;
    popclose();
    closeread();
    msgtop = 0;
    msgsel = -1;
    /* If the folder has been read, showing it is showing what is already
       here -- the list is its index, not a copy of it, and nothing is
       read at all. Only a folder that has never been read costs
       anything, and that is asked for rather than done: a mailbox of
       four gigabytes takes as long as four gigabytes takes, and doing it
       here is the window going dead on a click. */
    useidx();
    if (!folders[i].idxok) {

        msgct = 0;
        if (idxdoing != i) idxwant = i; /* it is already being read */
        kickworker();

    }
    drawfolders();
    drawlist();

}

/*******************************************************************************

Main

*******************************************************************************/

/* The command line. The account is not here: a password on a command
   line is a password in everybody's process list. */
static long dofetch;   /* fetch on startup */

static ami_optrec opttbl[] = {

    { "fetch", &dofetch, NULL,   NULL, NULL },
    { "f",     &dofetch, NULL,   NULL, NULL },
    { "diag",  &diag,    NULL,   NULL, NULL },
    { "d",     &diag,    NULL,   NULL, NULL },
    { "limit", NULL,     &limit, NULL, NULL },
    { NULL,    NULL,     NULL,   NULL, NULL }

};

int main(int argc, char* argv[])

{

    ami_evtrec er;
    char       msg[MAXSTR];
    long       i;
    long       argi = 1;
    long       argcl = argc;
    long       wx, wy;

    /* the command line, by the option package, which knows what an
       option looks like on the system it is running on */
    mailprog = argv[0]; /* the rules are looked for beside the program */
    ami_options(&argi, &argcl, argv, opttbl, TRUE);
    if (argcl > 1) {

        fprintf(stderr, "Usage: mail [--fetch|-f] [--diag|-d] "
                        "[--limit=<n>]\n");

        return (1);

    }
    ami_title(stdout, "Mail");
    ami_autohold(FALSE);
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_font(stdout, AMI_FONT_SIGN);
    ami_setpoints(stdout, 11.0);
    ami_binvis(stdout);
    chrh = ami_chrsizy(stdout);
    rowh = chrh+8;
    { /* the store, if it is not there yet: makpth is an error if it is */

        char        path[MAXSTR-16];
        struct stat sb;

        ami_getusr(path, sizeof(path));
        snprintf(store, MAXSTR, "%s/.amimail", path);
        if (stat(store, &sb)) ami_makpth(store);

    }
    readaccount(); /* if there is one; the program comes up either way */
    migratestore(); /* mailboxes from before accounts had names */
    {

        long cw = ami_strsiz(stdout, "0")*130; /* the room wanted inside */
        long ch = chrh*46;

        ami_winclientg(stdout, cw, ch, &wx, &wy,
                       BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
        ami_setsizg(stdout, wx, wy);
        /* The buffer is NOT sized here. It follows the window, and the
           only thing that knows what the window actually became is the
           resize the window manager sends back -- which may be nothing
           like what was asked for. Sizing it here to what was asked for
           puts the buffer ahead of the window instead of behind it, and
           then every measurement is of a window that does not exist:
           the status strip is drawn at the foot of a buffer taller than
           the window, where nobody can see it. The resize that arrives
           at startup does the sizing, as every later one does. */

    }
    /* The menu is built once the window is its final size. Built before,
       the menu strip follows the resize but its newly exposed right end
       is never painted, and sits there as a black box until something
       makes the window resize again. */
    setupmenu();
    /* the two panes */
    ami_openwin(&stdin, &foldwf, stdout, FOLDWIN);
    /* A pane is part of the main window, not a window of its own: no
       frame, no title bar, no sizing bars. */
    ami_frame(foldwf, FALSE);

    ami_auto(foldwf, FALSE);
    ami_curvis(foldwf, FALSE);
    ami_font(foldwf, AMI_FONT_SIGN);
    ami_setpoints(foldwf, 11.0);
    ami_binvis(foldwf);
    ami_openwin(&stdin, &listwf, stdout, LISTWIN);
    ami_frame(listwf, FALSE);
    ami_auto(listwf, FALSE);
    ami_curvis(listwf, FALSE);
    ami_font(listwf, AMI_FONT_SIGN);
    ami_setpoints(listwf, 11.0);
    ami_binvis(listwf);
    /* The strip at the foot, and the bar in it. Its natural height is
       what the strip is built around; its width is set here, since a bar
       as wide as a window says less than a short one does. */
    /* the banner across the top: its own window, so that it is placed
       and drawn in one space and answers for its own redrawing */
    ami_openwin(&stdin, &banwf, stdout, BANWIN);
    ami_frame(banwf, FALSE);
    ami_auto(banwf, FALSE);
    ami_curvis(banwf, FALSE);
    ami_font(banwf, AMI_FONT_SIGN);
    ami_setpoints(banwf, 24.0);
    {

        char path[MAXSTR];

        if (resfile("mail.bmp", path, sizeof(path))) {

            ami_loadpict(banwf, BANPIC, path);
            picw = ami_pictsizx(banwf, BANPIC);
            pich = ami_pictsizy(banwf, BANPIC);
            havepic = picw > 0 && pich > 0;

        }

    }
    /* as tall as the picture wants, or as the name does */
    banh = havepic? pich+16: ami_chrsizy(banwf)+16;
    /* the strip along the foot, and the bar drawn in it */
    progh = chrh-2;
    progw = ami_strsiz(stdout, "0")*20;
    stath = chrh+8;
    ami_scrollvertsizg(listwf, &sbw, &wy);
    ami_scrollvertg(listwf, 1, 1, sbw, chrh*10, SBLIST); /* moved by layout */
    layout();
    /* Start from the store, not from the server. Whatever was fetched
       before can be read without a network at all, which is the point of
       keeping it in files, and starting this way means the program comes
       up at once and comes up on a train. The server is asked only when
       the user asks for it. */
    datlock = ami_initlock(); /* before there is a second thread to want it */
    storeall();
    if (foldct) showfolder(0);
    drawfolders();
    drawlist();
    if (!haveaccount())
        status("No account yet. Mail/Server asks for one.");
    else if (!foldct) status("Nothing fetched yet. Mail/Get Mail reads the "
                             "server.");
    else status("");
    /* The counts come from reading every mailbox through, which on a
       store of gigabytes is not something to do in front of somebody
       waiting for a window. The worker does it, and the pane fills in
       as it goes. */
    if (dofetch) fetchall(TRUE); else countlater();
    /* Look again every so often while the program is up. The timer is
       in hundred microsecond counts, so a second is ten thousand. */
    if (pollsec > 0) ami_timer(stdout, TIMPOLL, pollsec*10000, TRUE);
    /* Held for as long as this thread is doing anything, dropped while
       it waits for the next event. Every click, redraw, menu choice and
       tick is covered by it without a lock of its own, and the worker
       gets its turn in the gap -- which is nearly all of the time, since
       waiting for an event is nearly all this thread does. */
    dlock();
    do {

        dunlock();
        ami_event(stdin, &er);
        dlock();
        if (diag) switch (er.etype) {

            case ami_etresize:
                fprintf(stderr, "resize win %ld: %ldx%ld\n", er.winid,
                        er.rszxg, er.rszyg);
                break;
            case ami_etredraw:
                fprintf(stderr, "redraw win %ld: %ld,%ld to %ld,%ld\n",
                        er.winid, er.rsx, er.rsy, er.rex, er.rey);
                break;
            default: break;

        }
        /* Every event names the window it came from. The reader is a
           window of its own and the panes are windows of their own, so
           this one loop serves them all. */
        if (er.winid == HELPWIN) { helpevent(&er); continue; }
        if (er.winid == CMPWIN) { cmpevent(&er); continue; }
        if (er.winid == SRVWIN) { srvevent(&er); continue; }
        if (er.winid == READWIN) {

            switch (er.etype) {

                case ami_etmenus:
                    if (er.menuid == MENUREPLY || er.menuid == MENUREPALL ||
                        er.menuid == MENUFWD) answer(er.menuid);
                    break;
                case ami_etterm: closeread(); break;
                case ami_etredraw:
                case ami_etresize: wrapread(); drawread(); break;
                case ami_etmouba:
                    if (er.amoubn == 4) { readtop -= WHEELROWS; showread(); }
                    else if (er.amoubn == 5)
                        { readtop += WHEELROWS; showread(); }
                    break;
                case ami_etsclull: readtop--; showread(); break;
                case ami_etscldrl: readtop++; showread(); break;
                case ami_etsclulp: readtop -= readpage()-1; showread(); break;
                case ami_etscldrp: readtop += readpage()-1; showread(); break;
                case ami_etsclpos:
                    /* echo the bar where the user put it and do not set
                       it again from the view, which would drag the
                       thumb back to the nearest whole line */
                    ami_scrollpos(readwf, SBREAD, er.sclpos);
                    readtop = scaleback(er.sclpos, readlines-1);
                    fromdrag = TRUE;
                    showread();
                    fromdrag = FALSE;
                    break;
                case ami_etup: readtop--; showread(); break;
                case ami_etdown: readtop++; showread(); break;
                case ami_etpagu: readtop -= readpage()-1; showread(); break;
                case ami_etpagd: readtop += readpage()-1; showread(); break;
                case ami_ethome: readtop = 0; showread(); break;
                case ami_etend: readtop = readlines; showread(); break;
                case ami_etcan: closeread(); break;
                default: break;

            }
            continue;

        }
        if (er.winid == BANWIN) {

            if (er.etype == ami_etredraw) drawbanner();
            else if (er.etype == ami_etresize)
                { ami_sizbufg(banwf, er.rszxg, er.rszyg); drawbanner(); }

            continue;

        }
        if (er.winid == FOLDWIN) {

            switch (er.etype) {

                case ami_etmoumovg: mpx = er.moupxg; mpy = er.moupyg; break;
                case ami_etmouba: {

                    long best = -1;

                    if (er.amoubn != 1) break;
                    /* Which folder the click landed on, found from where
                       each was drawn rather than by counting rows: the
                       two headings and the rule between the lists make
                       row arithmetic wrong. */
                    for (i = 0; i < foldct; i++)
                        if (mpy >= foldy[i]-2 && mpy < foldy[i]+chrh+2)
                            best = i;
                    if (best >= 0) showfolder(best);
                    break;

                }
                case ami_etredraw: drawfolders(); break;
                case ami_etresize:
                    ami_sizbufg(foldwf, er.rszxg, er.rszyg);
                    drawfolders();
                    break;
                default: break;

            }
            continue;

        }
        if (er.winid == POPWIN) {

            switch (er.etype) {

                /* The menu does nothing at all until a button goes
                   down: it notes where the mouse is and no more. It
                   drew itself again on every move to follow the pointer
                   with a highlight, and that redraw was what made it
                   disappear the moment the mouse set off towards an
                   entry. A menu that waits for the click is what every
                   other program does anyway. */
                case ami_etmoumovg: {

                    long r = (er.moupyg-3)/(poprowh? poprowh: 1);

                    poprow = r >= 0 && r < 3? r: -1;
                    break;

                }
                case ami_etmouba: /* a button down on an entry takes it */
                    if (poprow >= 0) popact(poprow);
                    else popclose(); /* down on the edge: put it away */
                    break;
                case ami_etmoubd: break; /* the opening click coming up */
                case ami_etcan:
                case ami_etterm: popclose(); break;
                default: break;

            }
            continue;

        }
        if (er.winid == LISTWIN) {

            /* An open menu is cancelled by the next click, and by
               nothing else, which is what every other program does.
               Closing on anything that was not a mouse move looked
               reasonable and was not: the button that opened it comes
               up a moment later, and the mouse leaving this pane for
               the menu sends this pane a nohover -- so the menu was
               destroyed either as it was born or the moment the mouse
               set off towards it. */
            if (popwf && er.etype == ami_etmouba) { popclose(); continue; }
            switch (er.etype) {

                case ami_etmoumovg: mpx = er.moupxg; mpy = er.moupyg; break;
                case ami_etmouba:
                    if (er.amoubn == 4) {

                        msgtop -= WHEELMSGS;
                        showlist();

                    } else if (er.amoubn == 5) {

                        msgtop += WHEELMSGS;
                        showlist();

                    } else if (er.amoubn == 1) {

                        i = msgtop+(mpy-4)/rowh;
                        if (i >= 0 && i < msgct) { selectmsg(i); openmsg(i); }

                    } else if (er.amoubn == 2 || er.amoubn == 3) {

                        /* the second button: the message menu */
                        i = msgtop+(mpy-4)/rowh;
                        if (i >= 0 && i < msgct) {

                            selectmsg(i);
                            popopen(i, mpx, mpy);

                        }

                    }
                    break;
                case ami_etsclull: case ami_etup:
                    msgtop--;
                    showlist();
                    break;
                case ami_etscldrl: case ami_etdown:
                    msgtop++;
                    showlist();
                    break;
                case ami_etsclulp: case ami_etpagu:
                    msgtop -= listvis()-1;
                    showlist();
                    break;
                case ami_etscldrp: case ami_etpagd:
                    msgtop += listvis()-1;
                    showlist();
                    break;
                case ami_etsclpos:
                    ami_scrollpos(listwf, SBLIST, er.sclpos);
                    msgtop = scaleback(er.sclpos, msgct-listvis());
                    fromdrag = TRUE;
                    showlist();
                    fromdrag = FALSE;
                    break;
                case ami_etredraw: /* only what was exposed */
                    if (foldsel >= 0 && msgct) listrect(er.rsy, er.rey);
                    else drawlist();
                    break;

                case ami_etresize: {

                    /* The columns are measured from the right edge, so a
                       change of width moves all of them and the rows
                       have to be laid again. A change of height moves
                       nothing: the rows that come into view arrive as a
                       redraw of their own. */
                    static long prevw;

                    ami_sizbufg(listwf, er.rszxg, er.rszyg);
                    datex = er.rszxg-sbw-ami_strsiz(listwf, "Sep 30, 2025 ");
                    listrows = er.rszyg/rowh;
                    if (listrows < 1) listrows = 1;
                    if (er.rszxg != prevw) drawlist();
                    prevw = er.rszxg;
                    break;

                }
                default: break;

            }
            continue;

        }
        switch (er.etype) {

            case ami_etresize:
                /* Buffer follow mode. The buffer is made the size the
                   window now is, which leaves what was already drawn
                   where it was, and the panes are placed again. Nothing
                   is drawn here: the redraws that follow say what the
                   resize exposed, and the panes report their own. */
                ami_sizbufg(stdout, er.rszxg, er.rszyg);
                layout();
                break;

            case ami_etredraw:
                /* What this window shows for itself is the divider
                   between the panes and the strip at the foot, so that
                   is all a redraw of it costs. */
                /* The band under the menu is ground the panes used to
                   stand on: when they move down to make room for it,
                   what they leave is the main window's again, and the
                   main window has to paint it. */
                divider(stdout, foldw+4, 1+banh, foldw+4,
                        ami_maxyg(stdout)-stath);
                drawstatus();
                break;

            case ami_ettim:
                /* one step of a fetch, if one is running */
                if (er.timnum == TIMFETCH) fetchpick();
                /* and the periodic look at the servers, which does
                   nothing while a fetch is already under way */
                else if (er.timnum == TIMPOLL && !fetching && haveaccount()) {

                    quietfail = TRUE; /* nobody asked for this one */
                    fetchall(FALSE);  /* look, but do not relist */
                    quietfail = FALSE;

                }
                break;

            case ami_etmenus:
                switch (er.menuid) {

                    case MENUSRV: srvopen(); break;

                    case MENUCOMP:
                        if (!haveaccount()) { srvopen(); break; }
                        cmpopen("", "", "", "", "", "");
                        break;

                    case MENUFETCH:
                        if (!haveaccount()) {

                            status("No account yet. Mail/Server asks for "
                                   "one.");
                            srvopen();

                        } else fetchall(TRUE);
                        break;

                    case MENUFOLD: { /* every server's list, again */

                        long k;

                        if (!haveaccount()) { srvopen(); break; }
                        idxsetaside(); /* the indexes outlive the list */
                        foldct = 0;
                        foldsel = -1;
                        for (k = 0; k < srvct; k++) {

                            if (!*servers[k].imap || !*servers[k].user)
                                continue;
                            getfolders(k);
                            imapclose();

                        }
                        storefolders(-1);
                        idxgiveback();
                        countfolders();
                        kickworker();
                        drawfolders();
                        break;

                    }

                    case MENUCHECK:
                        if (!haveaccount()) { srvopen(); break; }
                        /* It sends over the same name and password the
                           fetch is using, and setting them up under a
                           fetch that is halfway through a login is how
                           one account's password goes to another
                           account's server. It waits. */
                        if (fetching) {

                            ami_alert("Mail", "Not while mail is being "
                                      "fetched. Try again when it has "
                                      "finished.");

                            break;

                        }
                        smtpcheck();
                        break;

                    case AMI_SMHELPTOPIC: helpopen(); break;

                    case AMI_SMABOUT:
                        ami_alert("Mail",
                                  "A mail reader on Petit-Ami graphics");
                        break;

                    case AMI_SMEXIT: goto done;

                }
                break;

            default: break;

        }

    /* a terminate for the reader closed the reader, not the program */
    } while (er.etype != ami_etterm || er.winid == READWIN ||
             er.winid == SRVWIN || er.winid == HELPWIN ||
             er.winid == CMPWIN);
    done:
    /* The lock is held here, so the worker is not in the middle of
       writing a message: what is in the store is whole. It is told to
       stop and is not waited for -- it may be in a read that has forty
       five seconds left to run, and nobody should wait that long to
       close a window. Nor is its connection closed from here: it is the
       worker's, and closing it under a thread reading it is how a tidy
       exit turns into a crash. Every message is written and closed as it
       arrives, so there is nothing outstanding to lose. */
    wrkstop = TRUE;
    if (fetching) ami_killtimer(stdout, TIMFETCH);
    popclose();
    helpclose();
    srvclose();
    closeread();
    cmpclose();
    dunlock();

    return (0);

}
