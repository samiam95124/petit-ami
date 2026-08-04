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

/* menu ids of our own, after the standard ones */
#define MENUMAIL  (AMI_SMMAX+1) /* the mail menu itself */
#define MENUFETCH (AMI_SMMAX+2) /* fetch new mail */
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

#define WHEELMSGS 1 /* messages the wheel moves per notch, in the list */
#define WHEELROWS 3 /* lines it moves per notch, in the reader */

#define TIMFETCH  1 /* the timer that steps a fetch along */
#define TIMPOLL   2 /* and the one that starts one now and then */

#define OFF 0
#define ON  1

/* a folder on the server, and the file it is kept in */
typedef struct {

    char name[MAXSTR];  /* the name the server knows it by */
    char show[MAXSTR];  /* the name shown, without the [Gmail]/ part */
    char file[MAXSTR*2]; /* the mbox file it is kept in */
    long msgs;          /* messages in the file */
    long noselect;      /* the server says it holds no messages */
    long local;         /* ours alone: a folder with no server side */
    long srv;           /* which server it belongs to, -1 if local */

} foldrec;

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

static foldrec folders[MAXFOLDER];
static long    foldct;
static long    foldsel = -1;    /* the folder being shown */

static msgrec* msgs;            /* the messages of that folder */
static long    msgct;
static long    msgmax;
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
static void drawread(void);
static void layout(void);

/*******************************************************************************

Odds and ends

*******************************************************************************/

/* say something went wrong, in a way the user can see */
static void fail(const char* what)

{

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

    char           dig[DIGLEN];
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

static int hasdigest(const char* d)

{

    digent* p;

    for (p = digtab[dighash(d)]; p; p = p->next)
        if (!strcmp(p->dig, d)) return (TRUE);

    return (FALSE);

}

static void adddigest(const char* d)

{

    digent* p;
    long    b;

    if (hasdigest(d)) return;
    b = dighash(d);
    p = getmem(sizeof(digent));
    copystr(p->dig, d, DIGLEN);
    p->next = digtab[b];
    digtab[b] = p;
    digct++;

}

/* the file a folder's digests are kept in */
static void digfile(long fold, char* fn, long fnl)

{

    snprintf(fn, fnl, "%s.dig", folders[fold].file);

}

/* Take the digests of every message in a folder's mailbox, writing them
   beside it and adding them to the table. The mailbox is walked exactly
   as the indexing walks it, so the two agree on what a message is. */
static long digestfolder(long fold)

{

    FILE* f;
    FILE* out;
    char  fn[MAXSTR*2+8];
    char* buf;
    long  n;
    long  i, start, blkstart;
    long  got = 0;
    char  hex[DIGLEN];

    f = fopen(folders[fold].file, "r");
    if (!f) return (0);
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = getmem(n+1);
    n = fread(buf, 1, n, f);
    buf[n] = 0;
    fclose(f);
    digfile(fold, fn, sizeof(fn));
    out = fopen(fn, "w");
    if (out) fprintf(out, "%ld\n", n); /* the size these were taken at */
    start = -1;
    blkstart = 0;
    for (i = 0; i < n; i++) {

        int atsep = !strncmp(buf+i, "From ", 5) &&
                    (i == 0 || (i >= 2 && buf[i-1] == '\n' &&
                                (buf[i-2] == '\n' ||
                                 (buf[i-2] == '\r' && i >= 3 &&
                                  buf[i-3] == '\n'))));

        if (atsep) {

            if (start >= 0) { /* the message that just ended */

                long e = i;

                while (e > start && (buf[e-1] == '\n' || buf[e-1] == '\r'))
                    e--;
                digestof(buf+start, e-start, hex);
                adddigest(hex);
                if (out) fprintf(out, "%s\n", hex);
                got++;

            }
            blkstart = i;
            while (i < n && buf[i] != '\n') i++;
            start = i+1;

        }
        while (i < n && buf[i] != '\n') i++;

    }
    if (start >= 0 && start < n) { /* the last one */

        long e = n;

        while (e > start && (buf[e-1] == '\n' || buf[e-1] == '\r')) e--;
        digestof(buf+start, e-start, hex);
        adddigest(hex);
        if (out) fprintf(out, "%s\n", hex);
        got++;

    }
    (void)blkstart;
    free(buf);
    if (out) fclose(out);

    return (got);

}

/* Load a folder's digests, taking them afresh if the file is missing or
   was written when the mailbox was a different size. */
static void loaddigests(long fold)

{

    char        fn[MAXSTR*2+8];
    FILE*       f;
    struct stat sb;
    char        line[DIGLEN+8];
    long        was = -1;

    if (stat(folders[fold].file, &sb)) return; /* no mailbox */
    digfile(fold, fn, sizeof(fn));
    f = fopen(fn, "r");
    if (f && fgets(line, sizeof(line), f)) was = atol(line);
    if (!f || was != (long)sb.st_size) { /* stale, or never taken */

        if (f) fclose(f);
        digestfolder(fold);

        return;

    }
    while (fgets(line, sizeof(line), f)) {

        trim(line);
        if (strlen(line) == DIGLEN-1) adddigest(line);

    }
    fclose(f);

}

/* The mailboxes of a store written before accounts had names carry no
   name, and would show up as belonging to no server. Give them the
   first account's, once, by renaming them and what goes with them. A
   store already named is left alone, since every name it holds matches
   an account. */
static void migratestore(void)

{

    ami_filptr lp = NULL;
    ami_filptr fp;
    char       what[MAXSTR*2];
    long       i;

    if (!srvct) return; /* no account to attribute them to */
    snprintf(what, sizeof(what), "%s/*.mbox", store);
    ami_list(what, &lp);
    for (fp = lp; fp; fp = fp->next) {

        char nm[MAXSTR];
        char old[MAXSTR*2], new[MAXSTR*2];
        long n;
        int  named = FALSE;

        copystr(nm, fp->name, sizeof(nm));
        n = strlen(nm);
        if (n > 5 && !strcmp(nm+n-5, ".mbox")) nm[n-5] = 0; else continue;
        if (!strncmp(nm, "local_", 6)) continue; /* ours, and unnamed */
        for (i = 0; i < srvct; i++) {

            long k = strlen(servers[i].name);

            if (!strncmp(nm, servers[i].name, k) && nm[k] == '_')
                { named = TRUE; break; }

        }
        if (named) continue;
        /* the mailbox, and the two files that walk with it */
        {

            static const char* ext[] = { "", ".state", ".dig" };
            long e;

            for (e = 0; e < 3; e++) {

                snprintf(old, sizeof(old), "%s/%s.mbox%s", store, nm, ext[e]);
                snprintf(new, sizeof(new), "%.400s/%.60s_%.150s.mbox%.8s",
                         store, servers[0].name, nm, ext[e]);
                rename(old, new); /* what is not there does not move */

            }

        }

    }

}

/* Rename what an account owns, when the account is renamed. The
   mailboxes carry their account's name, so they have to move with it or
   be fetched again for nothing. */
static void renamestore(const char* was, const char* now)

{

    ami_filptr lp = NULL;
    ami_filptr fp;
    char       what[MAXSTR*2];
    long       wl = strlen(was);

    if (!wl || !strcmp(was, now)) return;
    snprintf(what, sizeof(what), "%s/*.mbox", store);
    ami_list(what, &lp);
    for (fp = lp; fp; fp = fp->next) {

        char nm[MAXSTR];
        char old[MAXSTR*2], new[MAXSTR*2];
        long n;
        static const char* ext[] = { "", ".state", ".dig" };
        long e;

        copystr(nm, fp->name, sizeof(nm));
        n = strlen(nm);
        if (n > 5 && !strcmp(nm+n-5, ".mbox")) nm[n-5] = 0; else continue;
        if (strncmp(nm, was, wl) || nm[wl] != '_') continue; /* not its */
        for (e = 0; e < 3; e++) {

            snprintf(old, sizeof(old), "%.400s/%.150s.mbox%.8s",
                     store, nm, ext[e]);
            snprintf(new, sizeof(new), "%.400s/%.60s_%.150s.mbox%.8s",
                     store, now, nm+wl+1, ext[e]);
            rename(old, new); /* what is not there does not move */

        }

    }

}

/* every folder in the store, so that a fetch knows what is already here */
static void loadalldigests(void)

{

    long i;

    for (i = 0; i < foldct; i++) loaddigests(i);

}

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
    tm.tm_isdst = -1;
    t = mktime(&tm);
    if (t == (time_t)-1) return (0);
    /* today gets the time, anything older gets the date */
    if (now-t < 12*60*60) {

        long h12 = hour%12;

        if (!h12) h12 = 12;
        snprintf(show, sn, "%ld:%02ld %s", h12, min, hour < 12? "AM": "PM");

    } else if (now-t < 300L*24*60*60)
        snprintf(show, sn, "%s %ld", months[i], day);
    else snprintf(show, sn, "%s %ld, %ld", months[i], day, year);

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
static void mboxwrite(const char* file, const char* msg, long len)

{

    FILE* f = fopen(file, "a");
    char  from[MAXSTR];
    char  addr[MAXSTR];
    char  date[MAXSTR];
    char  show[40];
    long  when;
    const char* p;
    const char* e;

    if (!f) { fail("Cannot write to the mail store"); return; }
    findheader(msg, "From", from, sizeof(from));
    addrof(from, addr, sizeof(addr));
    findheader(msg, "Date", date, sizeof(date));
    when = parsedate(date, show, sizeof(show));
    if (!when) when = (long)time(NULL);
    {

        time_t t = when;

        fprintf(f, "From %s %s", addr, ctime(&t)); /* ctime ends the line */

    }
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
    fputc('\n', f); /* a blank line ends a message, always */
    fclose(f);

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
static void indexmsg(const char* msg, long len, long off)

{

    msgrec* m;
    char    from[MAXSTR];
    char    date[MAXSTR];
    char*   text;

    if (msgct >= msgmax) {

        msgmax = msgmax? msgmax*2: 256;
        msgs = realloc(msgs, msgmax*sizeof(msgrec));
        if (!msgs) { fail("Out of memory"); exit(1); }

    }
    m = &msgs[msgct++];
    m->off = off;
    m->len = len;
    digestof(msg, len, m->dig);
    classify(msg, m->cat, sizeof(m->cat));
    findheader(msg, "From", from, sizeof(from));
    nameof(from, m->from, sizeof(m->from));
    addrof(from, m->addr, sizeof(m->addr));
    if (!findheader(msg, "Subject", m->subject, sizeof(m->subject)))
        copystr(m->subject, "(no subject)", sizeof(m->subject));
    findheader(msg, "Date", date, sizeof(date));
    m->date = parsedate(date, m->when, sizeof(m->when));
    /* only what the list will show: see decodepart */
    text = textof(msg, len, SNIPPET*2);
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
static void indexfolder(long fold)

{

    FILE* f;
    char* buf;
    long  n;
    long  i, start;
    struct timespec t0;

    if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
    msgct = 0;
    msgtop = 0;
    msgsel = -1;
    if (fold < 0) return;
    f = fopen(folders[fold].file, "r");
    if (!f) return; /* nothing fetched yet, which is not an error */
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = getmem(n+1);
    n = fread(buf, 1, n, f);
    buf[n] = 0;
    fclose(f);
    start = -1;
    for (i = 0; i < n; i++) {

        int atsep = !strncmp(buf+i, "From ", 5) &&
                    (i == 0 || (i >= 2 && buf[i-1] == '\n' &&
                                (buf[i-2] == '\n' ||
                                 (buf[i-2] == '\r' && i >= 3 &&
                                  buf[i-3] == '\n'))));

        if (atsep) {

            long e = i;

            if (start >= 0) {

                /* the message runs to the blank line before this one */
                while (e > start && (buf[e-1] == '\n' || buf[e-1] == '\r'))
                    e--;
                indexmsg(buf+start, e-start, start);

            }
            /* the message itself begins after the separator line */
            while (i < n && buf[i] != '\n') i++;
            start = i+1;

        }
        while (i < n && buf[i] != '\n') i++;

    }
    if (start >= 0 && start < n) {

        long e = n;

        while (e > start && (buf[e-1] == '\n' || buf[e-1] == '\r')) e--;
        indexmsg(buf+start, e-start, start);

    }
    free(buf);
    qsort(msgs, msgct, sizeof(msgrec), bydate);
    folders[fold].msgs = msgct;
    if (diag) {

        struct timespec t1;

        clock_gettime(CLOCK_MONOTONIC, &t1);
        fprintf(stderr, "index: %s, %ld bytes, %ld messages, %.0fms\n",
                folders[fold].show, n, msgct,
                (t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);

    }

}

/* How many messages are in a folder's file. The file is only counted,
   not taken apart: the separators are found and nothing else, so a
   mailbox of sixty megabytes costs a read rather than a parse. The
   folder being shown is counted exactly by the indexing, which knows;
   this is for all the others, which the list never touches. */
static long countfolder(const char* file)

{

    FILE*  f = fopen(file, "r");
    static const char from[] = "From ";
    char   buf[65536];
    long   n = 0;
    long   got;
    int    atbol = TRUE;    /* this character starts a line */
    int    blank = TRUE;    /* the line before it held nothing */
    int    matching = FALSE; /* a separator is being matched here */
    long   hold = 0;        /* how much of it has matched */

    if (!f) return (0);
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0) {

        long i;

        for (i = 0; i < got; i++) {

            char c = buf[i];

            if (c == '\r') continue; /* whatever the line endings are */
            if (c == '\n') {

                blank = atbol; /* nothing came between the two line ends */
                atbol = TRUE;
                matching = FALSE;

                continue;

            }
            if (atbol) { /* a separator can only begin a line */

                matching = blank;
                hold = 0;
                atbol = FALSE;

            }
            /* The match runs on past the start of the line, which is
               what the first try of this got wrong: it asked to be at
               the start of a line for every character of "From " and so
               never matched more than the F. */
            if (matching) {

                if (c == from[hold]) { if (++hold == 5) { n++;
                                                          matching = FALSE; } }
                else matching = FALSE;

            }

        }

    }
    fclose(f);

    return (n);

}

/* count every folder that has not been counted */
static void countfolders(void)

{

    long i;

    for (i = 0; i < foldct; i++)
        folders[i].msgs = countfolder(folders[i].file);

}

/*******************************************************************************

Local folders

A local folder is a mailbox of the program's own: a file in the store
with no counterpart on the server, holding messages moved into it by
hand. The server is never told. This is the local-first model: the
server keeps what it keeps, and how the mail is organized here is this
program's business, done with file operations and nothing else.

Local folders are listed under the server's folders, below a rule, and
their files are named local_* so the two kinds can never be confused.

*******************************************************************************/

/* the file a local folder of this name lives in */
static void localfile(const char* show, char* fn, long fnl)

{

    char nm[MAXSTR/2];
    long i;

    copystr(nm, show, sizeof(nm));
    for (i = 0; nm[i]; i++)
        if (nm[i] == '/' || nm[i] == '\\' || nm[i] == ' ' ||
            nm[i] == '[' || nm[i] == ']' || nm[i] == '"') nm[i] = '_';
    snprintf(fn, fnl, "%s/local_%s.mbox", store, nm);

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
    /* Both mailboxes changed under their digest files, which are taken
       again from what is there rather than patched, so they cannot drift
       from it. The digests themselves do not change: a message moved is
       the same message. */
    digestfolder(fold);

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
static char  poplab[2][MAXSTR]; /* the entries' faces */

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
    ami_fcolorc(popwf, 120, 120, 120);
    ami_line(popwf, 0, 0, w-1, 0);
    ami_line(popwf, 0, h-1, w-1, h-1);
    ami_line(popwf, 0, 0, 0, h-1);
    ami_line(popwf, w-1, 0, w-1, h-1);
    ami_fcolor(popwf, ami_black);
    for (i = 0; i < 2; i++) {

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
    snprintf(poplab[1], sizeof(poplab[1]), "Local folder for %s", nm);
    poprowh = chrh+10;
    w = ami_strsiz(listwf, poplab[0]);
    if (ami_strsiz(listwf, poplab[1]) > w) w = ami_strsiz(listwf, poplab[1]);
    w += 20;
    h = poprowh*2+6;
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

    } else { /* everything here from this sender, to a folder of theirs */

        long m;

        for (m = 0; m < msgct; m++)
            if (!strcmp(msgs[m].addr, msgs[i].addr)) set[m] = TRUE;
        copystr(who, msgs[i].from, sizeof(who));
        dst = localfolder(who);

    }
    if (dst < 0) { free(set); fail("No room for another folder"); return; }
    moved = movelocal(foldsel, folders[dst].file, set);
    free(set);
    msgsel = -1;
    indexfolder(foldsel);
    countfolders();
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

/* get one line back, without its line ending */
static int imline(char* buf, long bn)

{

    if (!fgets(buf, bn, imap)) return (FALSE);
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
    /* * LIST (\HasNoChildren) "/" "INBOX" -- the name is last, and quoted
       unless it has nothing in it needing quotes */
    p = strrchr(line, '"');
    if (p && p != line) { /* quoted: find the quote that opens it */

        q = p-1;
        while (q > line && *q != '"') q--;
        if (*q != '"') return;
        q++;
        while (q < p && o < MAXSTR-1) name[o++] = *q++;

    } else { /* not quoted: the last blank separated word */

        q = strrchr(line, ' ');
        if (!q) return;
        q++;
        while (*q && o < MAXSTR-1) name[o++] = *q++;

    }
    name[o] = 0;
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

        snprintf(fn, sizeof(fn), "%.60s_%.180s",
                 listsrv >= 0? servers[listsrv].name: "mail", name);
        for (i = 0; fn[i]; i++)
            if (fn[i] == '/' || fn[i] == '\\' || fn[i] == ' ' ||
                fn[i] == '[' || fn[i] == ']') fn[i] = '_';
        snprintf(f->file, sizeof(f->file), "%s/%s.mbox", store, fn);

    }

}

/* The folders that are already in the store, for when the server cannot
   be reached. The whole point of keeping the mail in files is that it can
   be read without a server, so a program that shows nothing until it has
   connected has thrown that away. The name is recovered from the file
   name, which is the folder name with the awkward characters replaced,
   so it comes back readable if not always exact. */
static void storefolders(int localpass)

{

    ami_filptr lp = NULL;
    ami_filptr fp;
    char       what[MAXSTR*2];

    snprintf(what, sizeof(what), "%s/*.mbox", store);
    ami_list(what, &lp);
    for (fp = lp; fp && foldct < MAXFOLDER; fp = fp->next) {

        foldrec* f;
        char     nm[MAXSTR/2];
        long     n;
        int      isloc;

        copystr(nm, fp->name, sizeof(nm));
        n = strlen(nm);
        if (n > 5 && !strcmp(nm+n-5, ".mbox")) nm[n-5] = 0; else continue;
        /* The two kinds are loaded in separate passes, server folders
           first, so the locals always sit at the end of the list, below
           the rule the folder pane draws between the sections. */
        isloc = !strncmp(nm, "local_", 6);
        if (isloc != localpass) continue;
        f = &folders[foldct++];
        copystr(f->name, isloc? nm+6: nm, MAXSTR);
        copystr(f->show, isloc? nm+6: nm, MAXSTR);
        snprintf(f->file, sizeof(f->file), "%s/%s.mbox", store, nm);
        f->noselect = FALSE;
        f->local = isloc;
        f->srv = -1;
        if (!isloc) { /* which server's file is this? */

            long k;

            for (k = 0; k < srvct; k++) {

                long n = strlen(servers[k].name);

                if (!strncmp(nm, servers[k].name, n) && nm[n] == '_')
                    { f->srv = k; break; }

            }

        }
        { /* the real name, if the state file beside it kept one */

            char  sn[MAXSTR*2+8];
            char  real[MAXSTR];
            long  v, u;
            FILE* sf;

            snprintf(sn, sizeof(sn), "%s.state", f->file);
            sf = fopen(sn, "r");
            if (sf) {

                if (fscanf(sf, "%ld %ld %499[^\n]", &v, &u, real) == 3) {

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

/* connect and log in */
static int imapopen(void)

{

    unsigned long addr;
    char          tag[20];
    char          line[MAXLINE];

    if (imap) return (TRUE); /* already there */
    ami_addrnet(imapsrv, &addr);
    imap = ami_opennet(addr, imapport, TRUE);
    if (!imap) { fail("Cannot reach the mail server"); return (FALSE); }
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
        snprintf(msg, sizeof(msg),
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
static void readstate(long fold, long* validity, long* lastuid)

{

    char  fn[MAXSTR*2+8];
    FILE* f;

    *validity = 0;
    *lastuid = 0;
    snprintf(fn, sizeof(fn), "%s.state", folders[fold].file);
    f = fopen(fn, "r");
    if (!f) return;
    if (fscanf(f, "%ld %ld", validity, lastuid) != 2)
        { *validity = 0; *lastuid = 0; }
    fclose(f);

}

static void writestate(long fold, long validity, long lastuid)

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
    fprintf(f, "%ld %ld %s\n", validity, lastuid, folders[fold].name);
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
static void smtpcheck(void)

{

    unsigned long addr;
    FILE* f;
    char  b64[MAXSTR*2];
    char  msg[MAXSTR*2];
    long  code;

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
static void status(const char* s)

{

    (void)s;

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

    ami_fcolorc(f, 180, 180, 180);
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
            ami_fcolorc(foldwf, 130, 130, 130);
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
    ami_fcolorc(listwf, 110, 110, 110);
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
        fprintf(listwf, "Nothing in %s yet. Mail/Fetch reads the server.",
                folders[foldsel].show);
    
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
            if (readlines >= readmax) {

                readmax = readmax? readmax*2: 200;
                readline = realloc(readline, readmax*sizeof(char*));
                if (!readline) { fail("Out of memory"); exit(1); }

            }
            readline[readlines] = getmem(strlen(part)+1);
            strcpy(readline[readlines++], part);
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
        ami_fcolorc(srvwf, 110, 110, 110);
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
    ami_winclientg(srvwf, ami_strsiz(srvwf, "0")*96,
                   (eh+chrh/2)*SRVFLDS+bh+chrh*6, &wx, &wy,
                   BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
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

    long top = chrh*2; /* under the menu bar, which is drawn in the client */
    long h = ami_maxyg(stdout)-top;

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

}

/*******************************************************************************

The menu

*******************************************************************************/

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
static long fetchfold;   /* the folder being read */
static long fetchi;      /* which of that folder's uids is next */
static long fetchgot;    /* messages taken, this fetch */
static long fetchdup;    /* and passed over as already here */
static long fetchlast;   /* the highest uid taken from this folder */
static long fetchseen;   /* the highest uid taken before this fetch */

/* say where the fetch has got to */
static void fetchsay(void)

{

    static long said;

    /* The folder pane is the progress display, its counts climbing as
       the messages land -- but not for every one of them: redrawing the
       pane a hundred times a second is a flicker, not a display. */
    if (++said >= 10) { said = 0; drawfolders(); }

}

/* Move to the next folder worth reading, and ask it what it holds.
   Gives FALSE when there are no folders left. */
static int fetchnext(void)

{

    char tag[20];
    long validity;
    long lo, hi;

    while (++fetchfold < foldct) {

        if (folders[fetchfold].noselect) continue;
        if (folders[fetchfold].local) continue; /* nothing to fetch from */
        if (folders[fetchfold].srv != fetchsrv) { /* a different server */

            imapclose();
            fetchsrv = folders[fetchfold].srv;
            if (fetchsrv < 0) continue;
            useserver(fetchsrv);
            if (!imapopen()) continue;

        }
        fetchi = 0;
        uidct = 0;
        fetchsay();
        exists = 0;
        uidvalidity = 0;
        imsend(tag, sizeof(tag), "EXAMINE \"%s\"",
               folders[fetchfold].name);
        if (!imwait(tag, examline)) continue; /* gone, or not a folder */
        if (!exists) continue; /* nothing in it */
        readstate(fetchfold, &validity, &fetchseen);
        /* a folder that was rebuilt on the server has new uids for the
           same messages, so what was taken before means nothing */
        if (validity != uidvalidity) fetchseen = 0;
        fetchlast = fetchseen;
        /* the most recent messages, as many as were asked for */
        hi = exists;
        lo = hi-limit+1;
        if (lo < 1) lo = 1;
        imsend(tag, sizeof(tag), "FETCH %ld:%ld (UID)", lo, hi);
        if (!imwait(tag, uidline)) continue;
        if (uidct) return (TRUE);

    }

    return (FALSE);

}

/* the fetch is over, however it ended */
static void fetchend(void)

{

    char msg[MAXSTR];

    ami_killtimer(stdout, TIMFETCH);
    fetching = FALSE;
    imapclose();
    if (diag) fprintf(stderr, "fetch: %ld new, %ld already here\n",
                      fetchgot, fetchdup);
    drawfolders(); /* the counts as they finally stand */
    snprintf(msg, sizeof(msg), "%ld new message%s", fetchgot,
             fetchgot == 1? "": "s");
    status(msg);
    countfolders();
    if (foldsel < 0 && foldct) showfolder(0);
    else if (foldsel >= 0) { indexfolder(foldsel); drawlist(); }
    drawfolders();

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

    if (!fetching) return;
    while (fetchi >= uidct) { /* this folder is done */

        if (fetchfold >= 0 && fetchfold < foldct && uidct)
            writestate(fetchfold, uidvalidity, fetchlast);
        if (!fetchnext()) { fetchend(); return; }

    }
    uid = uidlist[fetchi++];
    if (uid <= fetchseen) { fetchsay(); return; } /* already have it */
    imsend(tag, sizeof(tag), "UID FETCH %ld (BODY.PEEK[])", uid);
    /* the reply is a line ending in a literal, then the message itself,
       then the rest of the reply and the tagged answer */
    if (!imline(line, sizeof(line))) { fetchend(); return; }
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
            fetchdup++;
            fetchsay();

            return;

        }
        adddigest(hex);
        /* and beside the mailbox, so the next run knows it without
           reading every message again */
        {

            char  fn[MAXSTR*2+8];
            FILE* df;

            digfile(fetchfold, fn, sizeof(fn));
            df = fopen(fn, "a");
            if (df) { fprintf(df, "%s\n", hex); fclose(df); }

        }

    }
    mboxwrite(folders[fetchfold].file, msg, n);
    free(msg);
    fetchgot++;
    folders[fetchfold].msgs++; /* the pane's count is the progress */
    imwait(tag, NULL); /* the ) and the answer */
    if (uid > fetchlast) fetchlast = uid;
    fetchsay();

}

/* start one */
static void fetchall(int relist)

{

    long i;
    char wasname[MAXSTR];
    long wassrv = -1;

    if (fetching) return; /* one at a time */
    /* Keep which folder is being read: rebuilding the list throws the
       selection away, and a fetch that moved the reader back to the
       first folder every quarter minute would be unusable. */
    wasname[0] = 0;
    if (foldsel >= 0 && foldsel < foldct) {

        copystr(wasname, folders[foldsel].name, MAXSTR);
        wassrv = folders[foldsel].srv;

    }
    /* A look on the timer does not ask the servers what folders they
       have. Folders do not come and go by the quarter minute, the asking
       costs a connection and a round trip to each of them, and
       rebuilding the list redraws the pane -- which is what made it
       flicker every time the timer came round. Get Mail asks; the timer
       only fetches. */
    if (!foldct) relist = TRUE; /* unless nothing is known yet */
    if (!relist) {

        status("Looking...");
        fetching = TRUE;
        fetchsrv = -2;
        fetchfold = -1;
        fetchi = 0;
        uidct = 0;
        fetchgot = 0;
        fetchdup = 0;
        loadalldigests();
        ami_timer(stdout, TIMFETCH, 100, TRUE);

        return;

    }
    status("Connecting...");
    /* Every server's folders, in the order they are configured, then the
       local ones. The list on the server is the authority on what
       folders that server has, so it replaces what was read off the
       store. */
    foldct = 0;
    foldsel = -1;
    for (i = 0; i < srvct; i++) {

        if (!*servers[i].imap || !*servers[i].user) continue;
        if (!getfolders(i)) { imapclose(); continue; }
        imapclose(); /* one at a time: the connection is per server */

    }
    storefolders(TRUE); /* the locals rejoin, after the servers' */
    countfolders();
    if (*wasname) { /* put the reader back on the folder it was reading */

        for (i = 0; i < foldct; i++)
            if (folders[i].srv == wassrv &&
                !strcmp(folders[i].name, wasname)) { foldsel = i; break; }

    }
    drawfolders();
    fetching = TRUE;
    fetchsrv = -2; /* no server open yet */
    fetchfold = -1;
    fetchi = 0;
    uidct = 0;
    fetchgot = 0;
    fetchdup = 0;
    /* what is already here, so that none of it is taken twice */
    loadalldigests();
    if (diag) fprintf(stderr, "digests known: %ld over %ld folders\n",
                      digct, foldct);
    /* Every hundredth of a second, which is far faster than a message
       can be fetched over a network: the timer sets the pace only when
       the network is quicker than the eye. */
    ami_timer(stdout, TIMFETCH, 100, TRUE);

}

/* show a folder */
static void showfolder(long i)

{

    if (i < 0 || i >= foldct) return;
    foldsel = i;
    popclose();
    closeread();
    indexfolder(i);
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
    ami_winclientg(stdout, ami_strsiz(stdout, "0")*130, chrh*46, &wx, &wy,
                   BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(stdout, wx, wy);
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
    ami_scrollvertsizg(listwf, &sbw, &wy);
    ami_scrollvertg(listwf, 1, 1, sbw, chrh*10, SBLIST); /* moved by layout */
    layout();
    /* Start from the store, not from the server. Whatever was fetched
       before can be read without a network at all, which is the point of
       keeping it in files, and starting this way means the program comes
       up at once and comes up on a train. The server is asked only when
       the user asks for it. */
    storefolders(FALSE);
    storefolders(TRUE);
    countfolders();
    if (foldct) showfolder(0);
    drawfolders();
    drawlist();
    if (!haveaccount())
        status("No account yet. Mail/Server asks for one.");
    else if (!foldct) status("Nothing fetched yet. Mail/Get Mail reads the "
                             "server.");
    else status("");
    if (dofetch) fetchall(TRUE);
    /* Look again every so often while the program is up. The timer is
       in hundred microsecond counts, so a second is ten thousand. */
    if (pollsec > 0) ami_timer(stdout, TIMPOLL, pollsec*10000, TRUE);
    do {

        ami_event(stdin, &er);
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
        if (er.winid == SRVWIN) { srvevent(&er); continue; }
        if (er.winid == READWIN) {

            switch (er.etype) {

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

                    poprow = r >= 0 && r < 2? r: -1;
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
                /* What this window shows between its panes is the one
                   divider, so that is all a redraw of it costs. */
                divider(stdout, foldw+4, chrh*2, foldw+4,
                        ami_maxyg(stdout));
                break;

            case ami_ettim:
                /* one step of a fetch, if one is running */
                if (er.timnum == TIMFETCH) fetchstep();
                /* and the periodic look at the servers, which does
                   nothing while a fetch is already under way */
                else if (er.timnum == TIMPOLL && !fetching && haveaccount())
                    fetchall(FALSE); /* look, but do not relist */
                break;

            case ami_etmenus:
                switch (er.menuid) {

                    case MENUSRV: srvopen(); break;

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
                        foldct = 0;
                        foldsel = -1;
                        for (k = 0; k < srvct; k++) {

                            if (!*servers[k].imap || !*servers[k].user)
                                continue;
                            getfolders(k);
                            imapclose();

                        }
                        storefolders(TRUE);
                        countfolders();
                        drawfolders();
                        break;

                    }

                    case MENUCHECK:
                        if (!haveaccount()) { srvopen(); break; }
                        smtpcheck();
                        break;

                    case AMI_SMHELPTOPIC:
                        ami_alert("Mail",
                                  "Mail/Server sets the account. Mail/Get "
                                  "Mail reads it. Then pick a folder at the "
                                  "left and a message on the right.");
                        break;

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
    } while (er.etype != ami_etterm ||
             er.winid == READWIN || er.winid == SRVWIN);
    done:
    if (fetching) ami_killtimer(stdout, TIMFETCH);
    popclose();
    srvclose();
    closeread();
    imapclose();

    return (0);

}
