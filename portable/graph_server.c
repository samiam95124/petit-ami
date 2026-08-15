/*******************************************************************************
*                                                                              *
*                        REMOTE DISPLAY SERVER MODULE                          *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
*                            2026/08/14 S. A. Franco                           *
*                                                                              *
* The server side of remote display mode: the display, and the user, are      *
* here. The server links with the standard graphics package and runs          *
* standalone. The main thread runs the command loop: wait for a message of    *
* the remote display protocol (graph_remote.h, and the paper                  *
* doc/remote.pdf), execute it against the display library, and reply if the   *
* call returns values. A second thread runs the event pump: call event()      *
* forever and send each event out on the event channel the moment it is      *
* ready; events are pushed, never polled.                                     *
*                                                                              *
* The command port comes from GRAPH_PORT, GR_DEFPORT if unset; the event      *
* channel is the next port, and the file port the one after. The server      *
* holds the file cache in its current directory, filling it by the file      *
* transfer exchange: on a miss it names its file port in the request, and    *
* the client connects and streams the file in, close delimited.               *
*                                                                              *
* The server serves one client at a time, and when the client says bye it    *
* resets and cycles back to wait for the next connection, until the console   *
* interrupt cancels it. It must be started before its client.                 *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>

#include <graphics.h>
#include <network.h>
#include <graph_remote.h>
#include <widget_base.h>
#include <execinfo.h>

#define MAXHND 512   /* window handles */
#define MAXSTR 4096  /* string unmarshal bound */

/* channels */
static long cmdfn = -1;  /* command channel */
static long evtfn = -1;  /* event channel */
static long srvport;     /* command port */
static long msgmax;      /* channel message size bound */
static int  gtrace;      /* diagnostic trace: --trace or GRAPH_TRACE;
                            every message prints to the error channel.
                            Slow, and worth it. */

/* message assembly */
static unsigned char* sbuf; /* reply buffer */
static long           soff;
static unsigned char* rbuf; /* receive buffer: points at the message
                               being executed, which may sit inside a
                               batched datagram */
static unsigned char* rbase; /* the buffer datagrams land in */
static long           roff;
static long           rlen;

/* the event channel is written by the pump thread and, for errors, the
   main thread; the lock keeps the messages whole */
static pthread_mutex_t evsend = PTHREAD_MUTEX_INITIALIZER;

/* The display library is multithreadable and carries its own locks: the
   pump's event() runs concurrently with command execution, and any
   deadlock between them is the library's bug, to be fixed there. The
   pump winds down at session end by a wake event injected from the main
   thread. */
static int byeseen;   /* the client said bye; wind down */
static int  hellopend;   /* a mid-session hello opened the next session */

/* The pump lives across sessions: it forwards events while a client is
   up, discards them while the server is idle, and treats a terminate
   from the display, the close button or a control-c typed in the
   window, on an idle server as cancellation. It stops only for the
   process exit, which must not tear the library down around it. */
static pthread_t pump;     /* the pump thread */
static int       pumpstop; /* exiting: leave the library and return */
static int       clientup; /* a client is connected */
static int       sigasked; /* a terminate asked this session to end */

/* the wake: a user event code, injected to return the pump from event()
   and never forwarded */
#define GR_EVWAKE (ami_etuser+0x100)

/* window handle map */
static FILE* h2f[MAXHND];  /* handle to window file */
static long  h2lw[MAXHND]; /* handle to logical window id */

/* the main window as the server presented it, restored between
   sessions: a session's title and size are the session's */
static char srvname[64];
static long origw, origh;

/*******************************************************************************

Report error and stop

*******************************************************************************/

static void error(const char* es)

{

    write(2, "*** graph_server: ", 18);
    write(2, es, strlen(es));
    write(2, "\n", 1);
    exit(1);

}

/* A session error: the client's fault or the wire's, not the server's.
   The error prints, the session drops, and the server recycles to wait
   for a new connection; only faults of the server itself exit. The
   jump lands in the session loop, which winds down whatever the
   session had running. */

#include <setjmp.h>

static jmp_buf sesjmp;    /* recycle point */
static int     pumping;   /* the pump thread is up */
static int     cleaning;  /* winding down; a second fault is fatal */

static void sesserr(const char* es)

{

    write(2, "*** graph_server: ", 18);
    write(2, es, strlen(es));
    write(2, " -- session dropped, awaiting new connection\n", 45);
    if (cleaning) exit(1); /* faulted while winding down */
    longjmp(sesjmp, 1);

}

/*******************************************************************************

Message assembly

*******************************************************************************/

static gr_msghdr* shdr(void) { return ((gr_msghdr*)sbuf); }
static gr_msghdr* rhdr(void) { return ((gr_msghdr*)rbuf); }

/* unmarshal from the received message */
static long gi(void)

{

    long v;

    if (roff+8 > rlen) sesserr("Message truncated");
    memcpy(&v, rbuf+roff, 8);
    roff += 8;

    return (v);

}

static double gf(void)

{

    double v;

    if (roff+8 > rlen) sesserr("Message truncated");
    memcpy(&v, rbuf+roff, 8);
    roff += 8;

    return (v);

}

static int g4(void)

{

    int v;

    if (roff+4 > rlen) sesserr("Message truncated");
    memcpy(&v, rbuf+roff, 4);
    roff += 4;

    return (v);

}

/* a counted string into a bounded buffer, terminated */
static void gstr(char* dst, long dl)

{

    long n = g4();
    long c = n;

    if (roff+n > rlen) sesserr("Message truncated");
    if (c > dl-1) c = dl-1;
    memcpy(dst, rbuf+roff, c);
    dst[c] = 0;
    roff += n;

}

/* a counted string, allocated */
static char* gsdup(void)

{

    long  n = g4();
    char* s;

    if (roff+n > rlen) sesserr("Message truncated");
    s = malloc(n+1);
    if (!s) error("Out of memory");
    memcpy(s, rbuf+roff, n);
    s[n] = 0;
    roff += n;

    return (s);

}

/* a menu tree, allocated. The display library may hold the tree for the
   life of the menu, so it is not freed after the call. */
static ami_menuptr gmenu(void)

{

    ami_menuptr lst = NULL;
    ami_menuptr lp = NULL;
    ami_menuptr e;
    int         n, i;
    long        flags;

    n = g4();
    for (i = 0; i < n; i++) {

        e = malloc(sizeof(ami_menurec));
        if (!e) error("Out of memory");
        flags = gi();
        e->onoff = !!(flags&1);
        e->oneof = !!(flags&2);
        e->bar = !!(flags&4);
        e->id = gi();
        e->face = gsdup();
        e->branch = gmenu();
        e->next = NULL;
        if (lp) lp->next = e; else lst = e;
        lp = e;

    }

    return (lst);

}

/* a string list, allocated; the display library may hold it */
static ami_strptr gslst(void)

{

    ami_strptr lst = NULL;
    ami_strptr lp = NULL;
    ami_strptr e;
    int        n, i;

    n = g4();
    for (i = 0; i < n; i++) {

        e = malloc(sizeof(ami_strrec));
        if (!e) error("Out of memory");
        e->str = gsdup();
        e->next = NULL;
        if (lp) lp->next = e; else lst = e;
        lp = e;

    }

    return (lst);

}

/* reply assembly */
static void rbegin(void)

{

    shdr()->mid = GR_MREPLY;
    shdr()->seq = rhdr()->seq;
    shdr()->wid = 0;
    soff = sizeof(gr_msghdr);

}

static void ri(long v)

{

    if (soff+8 > msgmax) sesserr("Reply too large for channel");
    memcpy(sbuf+soff, &v, 8);
    soff += 8;

}

static void rf(double v)

{

    if (soff+8 > msgmax) sesserr("Reply too large for channel");
    memcpy(sbuf+soff, &v, 8);
    soff += 8;

}

static void r4(int v)

{

    if (soff+4 > msgmax) sesserr("Reply too large for channel");
    memcpy(sbuf+soff, &v, 4);
    soff += 4;

}

/* a string by the critical buffer rule: the length is the occupancy, which
   may be the full buffer with no terminator */
static void rstrn(const char* s, long n)

{

    r4((int)n);
    if (soff+n > msgmax) sesserr("Reply too large for channel");
    memcpy(sbuf+soff, s, n);
    soff += n;

}

static void rstr(const char* s, long bl)

{

    rstrn(s, strnlen(s, bl));

}

static void rmenu(ami_menuptr m)

{

    ami_menuptr p;
    int         n = 0;

    for (p = m; p; p = p->next) n++;
    r4(n);
    for (p = m; p; p = p->next) {

        ri((!!p->onoff)|((!!p->oneof)<<1)|((!!p->bar)<<2));
        ri(p->id);
        rstrn(p->face, p->face? strlen(p->face): 0);
        rmenu(p->branch);

    }

}

static void rsend(void)

{

    shdr()->len = (int)soff;
    if (gtrace)
        fprintf(stderr, "gs> %-16s w%-3ld s%-5d len%ld\n",
                gr_msgname(shdr()->mid), shdr()->wid, shdr()->seq, soff);
    ami_wrmsg(cmdfn, sbuf, soff);

}

/*******************************************************************************

Window handles

*******************************************************************************/

static FILE* wf(long h)

{

    if (h < 1 || h >= MAXHND || !h2f[h]) sesserr("Invalid window handle");

    return (h2f[h]);

}

/* handle from the logical window id of an event */
static long lw2h(long lw)

{

    long h;

    for (h = 1; h < MAXHND; h++)
        if (h2f[h] && h2lw[h] == lw) return (h);

    return (lw); /* unknown: pass as is */

}

/*******************************************************************************

File transfer

The cache is the current directory. A miss requests the file: the request
names our file port, the client connects and streams the file in, close
delimited. An empty stream is a file the client does not hold either, and
the display library's complaint follows when the call proceeds.

*******************************************************************************/

/* base name of a client supplied name: never a path on our side */
static const char* basenm(const char* fn)

{

    const char* p = strrchr(fn, '/');

    return (p? p+1: fn);

}

/* The picture extension rule of the display library: .bmp is set or
   overwritten. The names cross the wire as given, and both ends apply
   the rule where they touch a file. */
static void setbmp(char* dst, long dl, const char* fn)

{

    const char* dot = strrchr(fn, '.');
    const char* sl = strrchr(fn, '/');
    long        n;

    if (dot && (!sl || dot > sl)) n = dot-fn; /* strip the extension */
    else n = strlen(fn);
    if (n > dl-5) n = dl-5;
    memcpy(dst, fn, n);
    strcpy(dst+n, ".bmp");

}

/* find or fetch a named file; returns the name to use */
static const char* fndfile(const char* fn, char* cb, long cbl)

{

    FILE* tf;
    FILE* nf;
    FILE* lf;
    char  buf[4096];
    size_t r;

    char en[MAXSTR]; /* the name under the extension rule */

    /* the name as given, then the base name in the cache, both under
       the extension rule of the library */
    setbmp(en, MAXSTR, fn);
    tf = fopen(en, "rb");
    if (tf) { fclose(tf); snprintf(cb, cbl, "%s", en); return (cb); }
    setbmp(cb, cbl, basenm(fn));
    tf = fopen(cb, "rb");
    if (tf) { fclose(tf); return (cb); }
    /* request it: the client connects to our file port and streams it */
    shdr()->mid = GR_MFILEREQ;
    shdr()->seq = 0;
    shdr()->wid = 0;
    soff = sizeof(gr_msghdr);
    rstrn(fn, strlen(fn));
    ri(srvport+2);
    shdr()->len = (int)soff;
    ami_wrmsg(cmdfn, sbuf, soff);
    nf = ami_waitnet(srvport+2, 0);
    if (!nf) sesserr("Cannot open file transfer connection");
    lf = fopen(cb, "wb");
    if (!lf) sesserr("Cannot write file cache");
    while ((r = fread(buf, 1, sizeof(buf), nf)) > 0)
        if (fwrite(buf, 1, r, lf) != r) sesserr("Cannot write file cache");
    fclose(lf);
    fclose(nf);

    return (cb);

}

/*******************************************************************************

Event pump

The second thread: take each event from the display library as it comes
and push it out, translating the logical window id to the wire handle and
carrying the event union raw, as the same width hosts of the protocol
allow.

*******************************************************************************/

#define EVUNION (7*8)

/* Event thinning: the movement streams, mouse, graphical mouse and
   joystick, and window resizes, arrive faster than they matter, a
   joystick idling at full rate being the standing example. Each stream
   sends at most one event per interval per device; between sends the
   latest holds as pending. Ordering stays truthful where it counts: a
   pending movement flushes before any other event goes out, so a click
   always follows the position it happened at, and only the trailing
   rest position can lag, until the next event of any kind carries it. */

#define THINMS   10  /* the interval, milliseconds */
#define THINDEV  8   /* devices per stream */
#define THINRSZ  16  /* resize slots */

typedef struct thinslot {

    int        dirty;   /* a pending event holds */
    double     last;    /* when one last sent */
    gr_msgevt  ev;      /* the pending event */

} thinslot;

static thinslot thmou[THINDEV];  /* mouse moves, by mouse */
static thinslot thmoug[THINDEV]; /* graphical mouse moves */
static thinslot thjoy[THINDEV];  /* joystick moves, by stick */
static thinslot thrsz[THINRSZ];  /* resizes, by window handle */

static double thnow(void)

{

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec+ts.tv_nsec/1e9);

}

/* drop pending thinned events; a new session never sees stale ones */
static void thinclear(void)

{

    int i;

    for (i = 0; i < THINDEV; i++)
        thmou[i].dirty = thmoug[i].dirty = thjoy[i].dirty = 0;
    for (i = 0; i < THINRSZ; i++) thrsz[i].dirty = 0;

}

/* the thin slot for a wire event, or NULL if the type never thins */
static thinslot* thinfor(gr_msgevt* we)

{

    long d;

    switch (we->etype) {

        case ami_etmoumov:  d = we->p[0]; 
            return (d >= 1 && d <= THINDEV)? &thmou[d-1]: NULL;
        case ami_etmoumovg: d = we->p[0];
            return (d >= 1 && d <= THINDEV)? &thmoug[d-1]: NULL;
        case ami_etjoymov:  d = we->p[0];
            return (d >= 1 && d <= THINDEV)? &thjoy[d-1]: NULL;
        case ami_etresize:  d = we->winid;
            return (d >= 1 && d <= THINRSZ)? &thrsz[d-1]: NULL;
        default: return (NULL);

    }

}

static void evsend1(gr_msgevt* we);

/* flush every pending movement, before any other event overtakes it */
static void thinflush(void)

{

    int i;

    for (i = 0; i < THINDEV; i++) {

        if (thmou[i].dirty)  { thmou[i].dirty = 0;  evsend1(&thmou[i].ev); }
        if (thmoug[i].dirty) { thmoug[i].dirty = 0; evsend1(&thmoug[i].ev); }
        if (thjoy[i].dirty)  { thjoy[i].dirty = 0;  evsend1(&thjoy[i].ev); }

    }
    for (i = 0; i < THINRSZ; i++)
        if (thrsz[i].dirty) { thrsz[i].dirty = 0; evsend1(&thrsz[i].ev); }

}

/* put one event on the wire */
static void evsend1(gr_msgevt* we)

{

    unsigned char ebuf[sizeof(gr_msghdr)+sizeof(gr_msgevt)];
    gr_msghdr*    h = (gr_msghdr*)ebuf;

    if (gtrace)
        fprintf(stderr, "gs>e %-15s w%ld\n",
                gr_evtname((long)we->etype), we->winid);
    h->len = sizeof(ebuf);
    h->mid = GR_MEVENT;
    h->seq = 0;
    h->wid = 0;
    memcpy(ebuf+sizeof(gr_msghdr), we, sizeof(gr_msgevt));
    pthread_mutex_lock(&evsend);
    ami_wrmsg(evtfn, ebuf, sizeof(ebuf));
    pthread_mutex_unlock(&evsend);

}

static void* evpump(void* arg)

{

    unsigned char ebuf[sizeof(gr_msghdr)+sizeof(gr_msgevt)];
    gr_msghdr*    h = (gr_msghdr*)ebuf;
    gr_msgevt*    we = (gr_msgevt*)(ebuf+sizeof(gr_msghdr));
    ami_evtrec    er;

    (void)arg;
    for (;;) {

        ami_event(stdin, &er);
        if (pumpstop) return (NULL); /* the process is exiting */
        if ((long)er.etype == GR_EVWAKE) continue; /* ours, not the wire's */
        if (gtrace && !clientup)
            fprintf(stderr, "gs.e %-15s w%ld (idle)\n",
                    gr_evtname((long)er.etype), er.winid);
        if (er.etype == ami_etterm) {

            /* A terminate from the display, control-c or the close
               button, shuts the whole server down, as the console
               interrupt does: one stroke. With a client up it winds
               down politely, the terminate forwarding so the program
               exits and its bye takes the server; idle, or asked twice,
               the signal path exits directly. */
            if (!clientup || sigasked) { kill(getpid(), SIGINT); continue; }
            sigasked = 1; /* the bye that follows ends the server */

        }
        if (!clientup) continue; /* idle: nobody to forward to */
        (void)h;
        memset(we, 0, sizeof(gr_msgevt));
        we->winid = lw2h(er.winid);
        we->etype = (long)er.etype;
        we->handled = 0;
        memcpy(we->p, &er.echar, EVUNION);
        {

            thinslot* ts = thinfor(we);

            if (ts) {

                double now = thnow();

                if (now-ts->last >= THINMS/1000.0) {

                    /* due: this one goes, and carries any pendings;
                       its own pending it supersedes outright */
                    ts->dirty = 0;
                    thinflush();
                    ts->last = now;
                    evsend1(we);

                } else {

                    /* within the interval: hold as the latest */
                    ts->ev = *we;
                    ts->dirty = 1;

                }

            } else {

                /* every other event flushes the pendings ahead of it */
                thinflush();
                evsend1(we);

            }

        }

    }

    return (NULL);

}

/* stop the pump for a process exit: wake it out of the library and
   collect it, so the teardown finds the library empty */
static void stoppump(void)

{

    ami_evtrec wake;

    pumpstop = 1;
    memset(&wake, 0, sizeof(wake));
    wake.etype = (ami_evtcod)GR_EVWAKE;
    ami_sendevent(stdout, &wake);
    pthread_join(pump, NULL);

}

/*******************************************************************************

Console interrupt

The interrupt asks the program to terminate, as the close button does: a
terminate event goes to the client, the program winds down, and the bye
ends the server. Before a client exists there is nothing to ask, and a
second interrupt is force.

*******************************************************************************/


/* The mask must be in place before any thread exists: a process directed
   signal lands on any thread that leaves it unblocked, and a thread
   spawned by the display or sound constructors would take the interrupt
   to the default disposition, or discard it where the shell set ignore
   for a background job. First constructor in, so every later thread
   inherits the block and the signal stays pending for sigwait(). */
static void sigmask_early(void) __attribute__((constructor (101)));
static void sigmask_early(void)

{

    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    /* A background launch inherits ignore for these, and an ignored
       signal is discarded even while blocked, starving sigwait(). The
       default disposition keeps a blocked signal pending. */
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

}

static void* sigrun(void* arg)

{

    sigset_t      set;
    int           sig;
    unsigned char ebuf[sizeof(gr_msghdr)+sizeof(gr_msgevt)];
    gr_msghdr*    h = (gr_msghdr*)ebuf;
    gr_msgevt*    we = (gr_msgevt*)(ebuf+sizeof(gr_msghdr));

    (void)arg;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    for (;;) {

        if (sigwait(&set, &sig)) return (NULL);
        if (!clientup || sigasked) { stoppump(); exit(1); }
        sigasked = 1;
        if (gtrace) fprintf(stderr, "gs:  interrupt -> ETTERM to client\n");
        h->len = sizeof(ebuf);
        h->mid = GR_MEVENT;
        h->seq = 0;
        h->wid = 0;
        memset(we, 0, sizeof(gr_msgevt));
        we->winid = 1;
        we->etype = (long)ami_etterm;
        pthread_mutex_lock(&evsend);
        ami_wrmsg(evtfn, ebuf, sizeof(ebuf));
        pthread_mutex_unlock(&evsend);

    }

    return (NULL);

}

/*******************************************************************************

The command dispatch

Each message unpacks its payload in declaration order, into locals so the
order is the order, and executes against the display library.

*******************************************************************************/

static void dispatch(void)

{

    FILE* f = NULL;
    long  a, b, c, d, e, g, i, j;
    long  o1, o2, o3, o4;
    double fa, fb;
    char  s1[MAXSTR];
    char  s2[MAXSTR];

    if (gtrace)
        fprintf(stderr, "gs< %-16s w%-3ld s%-5d len%ld\n",
                gr_msgname(rhdr()->mid), rhdr()->wid, rhdr()->seq, rlen);
    if (rhdr()->wid) f = wf(rhdr()->wid);
    switch (rhdr()->mid) {

        /* ------------------------------------------------------ session */

        case GR_MHELLO:
            a = gi();
            rbegin(); ri(GR_VERSION); rsend();
            if (a != GR_VERSION) sesserr("Client protocol version mismatch");
            if (clientup) {

                /* A hello mid-session: the old client went without its
                   bye, a kill or a dead network, and a new one is
                   knocking. The old session winds down and this hello,
                   already answered, opens the new one. */
                hellopend = 1;
                byeseen = 1;

            }
            break;
        case GR_MSYNC:
            /* the flow fence: the reply is the client's permission to
               send another window */
            rbegin(); ri(0); rsend();
            break;
        case GR_MBYE:
            /* wind down: the main loop stops the pump first, so nothing
               is inside the display library when the exit takes it down */
            byeseen = 1;
            break;

        /* -------------------------------------------------- byte stream */

        case GR_MWRITE:
            a = g4();
            if (roff+a > rlen) sesserr("Message truncated");
            fwrite(rbuf+roff, 1, a, f);
            fflush(f);
            break;
        case GR_MCLOSEWIN:
            fclose(f);
            h2f[rhdr()->wid] = NULL;
            break;

        /* ----------------------------------------------- terminal level */

        case GR_MCURSOR: a = gi(); b = gi(); ami_cursor(f, a, b); break;
        case GR_MMAXX: rbegin(); ri(ami_maxx(f)); rsend(); break;
        case GR_MMAXY: rbegin(); ri(ami_maxy(f)); rsend(); break;
        case GR_MHOME: ami_home(f); break;
        case GR_MDEL: ami_del(f); break;
        case GR_MUP: ami_up(f); break;
        case GR_MDOWN: ami_down(f); break;
        case GR_MLEFT: ami_left(f); break;
        case GR_MRIGHT: ami_right(f); break;
        case GR_MBLINK: a = gi(); ami_blink(f, a); break;
        case GR_MREVERSE: a = gi(); ami_reverse(f, a); break;
        case GR_MUNDERLINE: a = gi(); ami_underline(f, a); break;
        case GR_MSUPERSCRIPT: a = gi(); ami_superscript(f, a); break;
        case GR_MSUBSCRIPT: a = gi(); ami_subscript(f, a); break;
        case GR_MITALIC: a = gi(); ami_italic(f, a); break;
        case GR_MBOLD: a = gi(); ami_bold(f, a); break;
        case GR_MSTRIKEOUT: a = gi(); ami_strikeout(f, a); break;
        case GR_MSTANDOUT: a = gi(); ami_standout(f, a); break;
        case GR_MFCOLOR: a = gi(); ami_fcolor(f, (ami_color)a); break;
        case GR_MBCOLOR: a = gi(); ami_bcolor(f, (ami_color)a); break;
        case GR_MAUTO: a = gi(); ami_auto(f, a); break;
        case GR_MCURVIS: a = gi(); ami_curvis(f, a); break;
        case GR_MSCROLL: a = gi(); b = gi(); ami_scroll(f, a, b); break;
        case GR_MCURX: rbegin(); ri(ami_curx(f)); rsend(); break;
        case GR_MCURY: rbegin(); ri(ami_cury(f)); rsend(); break;
        case GR_MCURBND: rbegin(); ri(ami_curbnd(f)); rsend(); break;
        case GR_MSELECT: a = gi(); b = gi(); ami_select(f, a, b); break;
        case GR_MTIMER:
            a = gi(); b = gi(); c = gi(); ami_timer(f, a, b, c); break;
        case GR_MKILLTIMER: a = gi(); ami_killtimer(f, a); break;
        case GR_MMOUSE: rbegin(); ri(ami_mouse(f)); rsend(); break;
        case GR_MMOUSEBUTTON:
            a = gi(); rbegin(); ri(ami_mousebutton(f, a)); rsend(); break;
        case GR_MJOYSTICK: rbegin(); ri(ami_joystick(f)); rsend(); break;
        case GR_MJOYBUTTON:
            a = gi(); rbegin(); ri(ami_joybutton(f, a)); rsend(); break;
        case GR_MJOYAXIS:
            a = gi(); rbegin(); ri(ami_joyaxis(f, a)); rsend(); break;
        case GR_MSETTAB: a = gi(); ami_settab(f, a); break;
        case GR_MRESTAB: a = gi(); ami_restab(f, a); break;
        case GR_MCLRTAB: ami_clrtab(f); break;
        case GR_MFUNKEY: rbegin(); ri(ami_funkey(f)); rsend(); break;
        case GR_MFRAMETIMER: a = gi(); ami_frametimer(f, a); break;
        case GR_MAUTOHOLD:
            /* accepted and swallowed: the hold acts at process exit,
               which a session never is, and the server's own exit must
               never hold */
            a = gi();
            break;
        case GR_MWRTSTR: gstr(s1, MAXSTR); ami_wrtstr(f, s1); break;
        case GR_MWRTSTRN:
            a = g4();
            if (roff+a > rlen) sesserr("Message truncated");
            if (a > MAXSTR) a = MAXSTR;
            memcpy(s1, rbuf+roff, a);
            ami_wrtstrn(f, s1, a);
            break;
        case GR_MSIZBUF: a = gi(); b = gi(); ami_sizbuf(f, a, b); break;
        case GR_MTITLE: gstr(s1, MAXSTR); ami_title(f, s1); break;
        case GR_MFCOLORC:
            a = gi(); b = gi(); c = gi(); ami_fcolorc(f, a, b, c); break;
        case GR_MBCOLORC:
            a = gi(); b = gi(); c = gi(); ami_bcolorc(f, a, b, c); break;

        /* ---------------------------------------------- graphical level */

        case GR_MMAXXG: rbegin(); ri(ami_maxxg(f)); rsend(); break;
        case GR_MMAXYG: rbegin(); ri(ami_maxyg(f)); rsend(); break;
        case GR_MCURXG: rbegin(); ri(ami_curxg(f)); rsend(); break;
        case GR_MCURYG: rbegin(); ri(ami_curyg(f)); rsend(); break;
        case GR_MLINE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_line(f, a, b, c, d); break;
        case GR_MLINEWIDTH: a = gi(); ami_linewidth(f, a); break;
        case GR_MLINESTYLE: a = gi(); ami_linestyle(f, (ami_lstyle)a); break;
        case GR_MRECT:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_rect(f, a, b, c, d); break;
        case GR_MFRECT:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_frect(f, a, b, c, d); break;
        case GR_MRRECT:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_rrect(f, a, b, c, d, e, g); break;
        case GR_MFRRECT:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_frrect(f, a, b, c, d, e, g); break;
        case GR_MELLIPSE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_ellipse(f, a, b, c, d); break;
        case GR_MFELLIPSE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_fellipse(f, a, b, c, d); break;
        case GR_MARC:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_arc(f, a, b, c, d, e, g); break;
        case GR_MFARC:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_farc(f, a, b, c, d, e, g); break;
        case GR_MFCHORD:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_fchord(f, a, b, c, d, e, g); break;
        case GR_MFTRIANGLE:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_ftriangle(f, a, b, c, d, e, g); break;
        case GR_MCURSORG: a = gi(); b = gi(); ami_cursorg(f, a, b); break;
        case GR_MBASELINE: rbegin(); ri(ami_baseline(f)); rsend(); break;
        case GR_MSETPIXEL: a = gi(); b = gi(); ami_setpixel(f, a, b); break;
        case GR_MFOVER: ami_fover(f); break;
        case GR_MBOVER: ami_bover(f); break;
        case GR_MFINVIS: ami_finvis(f); break;
        case GR_MBINVIS: ami_binvis(f); break;
        case GR_MFXOR: ami_fxor(f); break;
        case GR_MBXOR: ami_bxor(f); break;
        case GR_MFAND: ami_fand(f); break;
        case GR_MBAND: ami_band(f); break;
        case GR_MFOR: ami_for(f); break;
        case GR_MBOR: ami_bor(f); break;
        case GR_MCHRSIZX: rbegin(); ri(ami_chrsizx(f)); rsend(); break;
        case GR_MCHRSIZY: rbegin(); ri(ami_chrsizy(f)); rsend(); break;
        case GR_MFONTS: rbegin(); ri(ami_fonts(f)); rsend(); break;
        case GR_MFONT: a = gi(); ami_font(f, a); break;
        case GR_MFONTNAM:
            a = gi();
            ami_fontnam(f, a, s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend();
            break;
        case GR_MFONTSIZ: a = gi(); ami_fontsiz(f, a); break;
        case GR_MSETPOINTS: fa = gf(); ami_setpoints(f, (float)fa); break;
        case GR_MPOINTS: rbegin(); rf(ami_points(f)); rsend(); break;
        case GR_MCHRSPCY: a = gi(); ami_chrspcy(f, a); break;
        case GR_MCHRSPCX: a = gi(); ami_chrspcx(f, a); break;
        case GR_MDPMX: rbegin(); ri(ami_dpmx(f)); rsend(); break;
        case GR_MDPMY: rbegin(); ri(ami_dpmy(f)); rsend(); break;
        case GR_MSTRSIZ:
            gstr(s1, MAXSTR);
            rbegin(); ri(ami_strsiz(f, s1)); rsend();
            break;
        case GR_MCHRPOS:
            gstr(s1, MAXSTR); a = gi();
            rbegin(); ri(ami_chrpos(f, s1, a)); rsend();
            break;
        case GR_MWRITEJUST:
            gstr(s1, MAXSTR); a = gi(); ami_writejust(f, s1, a); break;
        case GR_MJUSTPOS:
            gstr(s1, MAXSTR); a = gi(); b = gi();
            rbegin(); ri(ami_justpos(f, s1, a, b)); rsend();
            break;
        case GR_MCONDENSED: a = gi(); ami_condensed(f, a); break;
        case GR_MEXTENDED: a = gi(); ami_extended(f, a); break;
        case GR_MXLIGHT: a = gi(); ami_xlight(f, a); break;
        case GR_MLIGHT: a = gi(); ami_light(f, a); break;
        case GR_MXBOLD: a = gi(); ami_xbold(f, a); break;
        case GR_MHOLLOW: a = gi(); ami_hollow(f, a); break;
        case GR_MRAISED: a = gi(); ami_raised(f, a); break;
        case GR_MSETTABG: a = gi(); ami_settabg(f, a); break;
        case GR_MRESTABG: a = gi(); ami_restabg(f, a); break;
        case GR_MFCOLORG:
            a = gi(); b = gi(); c = gi(); ami_fcolorg(f, a, b, c); break;
        case GR_MBCOLORG:
            a = gi(); b = gi(); c = gi(); ami_bcolorg(f, a, b, c); break;
        case GR_MLOADPICT:
            a = gi(); gstr(s1, MAXSTR);
            ami_loadpict(f, a, (char*)fndfile(s1, s2, MAXSTR));
            rbegin(); ri(0); rsend();
            break;
        case GR_MPICTSIZX:
            a = gi(); rbegin(); ri(ami_pictsizx(f, a)); rsend(); break;
        case GR_MPICTSIZY:
            a = gi(); rbegin(); ri(ami_pictsizy(f, a)); rsend(); break;
        case GR_MPICTURE:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_picture(f, a, b, c, d, e); break;
        case GR_MDELPICT: a = gi(); ami_delpict(f, a); break;
        case GR_MSCROLLG: a = gi(); b = gi(); ami_scrollg(f, a, b); break;
        case GR_MPATH: a = gi(); ami_path(f, a); break;
        case GR_MVIEWOFFG: a = gi(); b = gi(); ami_viewoffg(f, a, b); break;
        case GR_MVIEWSCALE:
            fa = gf(); fb = gf();
            ami_viewscale(f, (float)fa, (float)fb); break;
        case GR_MSCALEX:
            a = gi(); rbegin(); ri(ami_scalex(f, a)); rsend(); break;
        case GR_MSCALEY:
            a = gi(); rbegin(); ri(ami_scaley(f, a)); rsend(); break;
        case GR_MBLOCKCOPYG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            i = gi(); j = gi(); o1 = gi(); o2 = gi();
            ami_blockcopyg(f, a, b, c, d, e, g, i, j, o1, o2); break;

        /* --------------------------------------------- window management */

        case GR_MOPENWIN: {

            FILE* inf = stdin;
            FILE* outf = NULL;
            long par, h;

            par = gi(); h = gi(); a = gi();
            if (h < 1 || h >= MAXHND || h2f[h])
                sesserr("Invalid window handle");
            ami_openwin(&inf, &outf, par? wf(par): NULL, a);
            h2f[h] = outf;
            h2lw[h] = a;
            break;

        }
        case GR_MBUFFER: a = gi(); ami_buffer(f, a); break;
        case GR_MSIZBUFG: a = gi(); b = gi(); ami_sizbufg(f, a, b); break;
        case GR_MGETSIZ:
            ami_getsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MGETSIZG:
            ami_getsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSETSIZ: a = gi(); b = gi(); ami_setsiz(f, a, b); break;
        case GR_MSETSIZG: a = gi(); b = gi(); ami_setsizg(f, a, b); break;
        case GR_MSETPOS: a = gi(); b = gi(); ami_setpos(f, a, b); break;
        case GR_MSETPOSG: a = gi(); b = gi(); ami_setposg(f, a, b); break;
        case GR_MDRAGWIN: ami_dragwin(f); break;
        case GR_MSCNSIZ:
            ami_scnsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCNSIZG:
            ami_scnsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCNCEN:
            ami_scncen(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCNCENG:
            ami_scnceng(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MWINCLIENT:
            a = gi(); b = gi(); c = gi();
            ami_winclient(f, a, b, &o1, &o2, (ami_winmodset)c);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MWINCLIENTG:
            a = gi(); b = gi(); c = gi();
            ami_winclientg(f, a, b, &o1, &o2, (ami_winmodset)c);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MFRONT: ami_front(f); break;
        case GR_MBACK: ami_back(f); break;
        case GR_MFRAME: a = gi(); ami_frame(f, a); break;
        case GR_MSIZABLE: a = gi(); ami_sizable(f, a); break;
        case GR_MSYSBAR: a = gi(); ami_sysbar(f, a); break;
        case GR_MMENU: ami_menu(f, gmenu()); break;
        case GR_MMENUENA: a = gi(); b = gi(); ami_menuena(f, a, b); break;
        case GR_MMENUSEL: a = gi(); b = gi(); ami_menusel(f, a, b); break;
        case GR_MSTDMENU: {

            ami_menuptr pm, sm = NULL;

            a = gi();
            pm = gmenu();
            ami_stdmenu((ami_stdmenusel)a, &sm, pm);
            rbegin(); rmenu(sm); rsend();
            break;

        }
        case GR_MGETWINID: rbegin(); ri(ami_getwinid()); rsend(); break;
        case GR_MFOCUS: ami_focus(f); break;

        /* --------------------------------------------------- widgets */

        case GR_MGETWIGID: rbegin(); ri(ami_getwigid(f)); rsend(); break;
        case GR_MKILLWIDGET: a = gi(); ami_killwidget(f, a); break;
        case GR_MSELECTWIDGET:
            a = gi(); b = gi(); ami_selectwidget(f, a, b); break;
        case GR_MENABLEWIDGET:
            a = gi(); b = gi(); ami_enablewidget(f, a, b); break;
        case GR_MGETWIDGETTEXT:
            a = gi();
            ami_getwidgettext(f, a, s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend();
            break;
        case GR_MPUTWIDGETTEXT:
            a = gi(); gstr(s1, MAXSTR); ami_putwidgettext(f, a, s1); break;
        case GR_MSIZWIDGET:
            a = gi(); b = gi(); c = gi(); ami_sizwidget(f, a, b, c); break;
        case GR_MSIZWIDGETG:
            a = gi(); b = gi(); c = gi(); ami_sizwidgetg(f, a, b, c); break;
        case GR_MPOSWIDGET:
            a = gi(); b = gi(); c = gi(); ami_poswidget(f, a, b, c); break;
        case GR_MPOSWIDGETG:
            a = gi(); b = gi(); c = gi(); ami_poswidgetg(f, a, b, c); break;
        case GR_MBACKWIDGET: a = gi(); ami_backwidget(f, a); break;
        case GR_MFRONTWIDGET: a = gi(); ami_frontwidget(f, a); break;
        case GR_MFOCUSWIDGET: a = gi(); ami_focuswidget(f, a); break;
        case GR_MBUTTONSIZ:
            gstr(s1, MAXSTR);
            ami_buttonsiz(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MBUTTONSIZG:
            gstr(s1, MAXSTR);
            ami_buttonsizg(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MBUTTON:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_button(f, a, b, c, d, s1, e); break;
        case GR_MBUTTONG:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_buttong(f, a, b, c, d, s1, e); break;
        case GR_MCHECKBOXSIZ:
            gstr(s1, MAXSTR);
            ami_checkboxsiz(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MCHECKBOXSIZG:
            gstr(s1, MAXSTR);
            ami_checkboxsizg(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MCHECKBOX:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_checkbox(f, a, b, c, d, s1, e); break;
        case GR_MCHECKBOXG:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_checkboxg(f, a, b, c, d, s1, e); break;
        case GR_MRADIOBUTTONSIZ:
            gstr(s1, MAXSTR);
            ami_radiobuttonsiz(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MRADIOBUTTONSIZG:
            gstr(s1, MAXSTR);
            ami_radiobuttonsizg(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MRADIOBUTTON:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_radiobutton(f, a, b, c, d, s1, e); break;
        case GR_MRADIOBUTTONG:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_radiobuttong(f, a, b, c, d, s1, e); break;
        case GR_MGROUPSIZG:
            gstr(s1, MAXSTR); a = gi(); b = gi();
            ami_groupsizg(f, s1, a, b, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MGROUPSIZ:
            gstr(s1, MAXSTR); a = gi(); b = gi();
            ami_groupsiz(f, s1, a, b, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MGROUP:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_group(f, a, b, c, d, s1, e); break;
        case GR_MGROUPG:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_groupg(f, a, b, c, d, s1, e); break;
        case GR_MBACKGROUND:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_background(f, a, b, c, d, e); break;
        case GR_MBACKGROUNDG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_backgroundg(f, a, b, c, d, e); break;
        case GR_MSCROLLVERTSIZG:
            ami_scrollvertsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCROLLVERTSIZ:
            ami_scrollvertsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCROLLVERT:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_scrollvert(f, a, b, c, d, e); break;
        case GR_MSCROLLVERTG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_scrollvertg(f, a, b, c, d, e); break;
        case GR_MSCROLLHORIZSIZG:
            ami_scrollhorizsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCROLLHORIZSIZ:
            ami_scrollhorizsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCROLLHORIZ:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_scrollhoriz(f, a, b, c, d, e); break;
        case GR_MSCROLLHORIZG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_scrollhorizg(f, a, b, c, d, e); break;
        case GR_MSCROLLPOS: a = gi(); b = gi(); ami_scrollpos(f, a, b); break;
        case GR_MSCROLLSIZ: a = gi(); b = gi(); ami_scrollsiz(f, a, b); break;
        case GR_MNUMSELBOXSIZG:
            a = gi(); b = gi();
            ami_numselboxsizg(f, a, b, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MNUMSELBOXSIZ:
            a = gi(); b = gi();
            ami_numselboxsiz(f, a, b, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MNUMSELBOX:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            i = gi(); ami_numselbox(f, a, b, c, d, e, g, i); break;
        case GR_MNUMSELBOXG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            i = gi(); ami_numselboxg(f, a, b, c, d, e, g, i); break;
        case GR_MEDITBOXSIZG:
            gstr(s1, MAXSTR);
            ami_editboxsizg(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MEDITBOXSIZ:
            gstr(s1, MAXSTR);
            ami_editboxsiz(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MEDITBOX:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_editbox(f, a, b, c, d, e); break;
        case GR_MEDITBOXG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_editboxg(f, a, b, c, d, e); break;
        case GR_MPROGBARSIZG:
            ami_progbarsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MPROGBARSIZ:
            ami_progbarsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MPROGBAR:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_progbar(f, a, b, c, d, e); break;
        case GR_MPROGBARG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_progbarg(f, a, b, c, d, e); break;
        case GR_MPROGBARPOS: a = gi(); b = gi(); ami_progbarpos(f, a, b); break;
        case GR_MLISTBOXSIZG:
            ami_listboxsizg(f, gslst(), &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MLISTBOXSIZ:
            ami_listboxsiz(f, gslst(), &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MLISTBOX: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_listbox(f, a, b, c, d, sp, e); break;

        }
        case GR_MLISTBOXG: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_listboxg(f, a, b, c, d, sp, e); break;

        }
        case GR_MDROPBOXSIZG:
            ami_dropboxsizg(f, gslst(), &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MDROPBOXSIZ:
            ami_dropboxsiz(f, gslst(), &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MDROPBOX: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_dropbox(f, a, b, c, d, sp, e); break;

        }
        case GR_MDROPBOXG: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_dropboxg(f, a, b, c, d, sp, e); break;

        }
        case GR_MDROPEDITBOXSIZG:
            ami_dropeditboxsizg(f, gslst(), &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MDROPEDITBOXSIZ:
            ami_dropeditboxsiz(f, gslst(), &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MDROPEDITBOX: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_dropeditbox(f, a, b, c, d, sp, e); break;

        }
        case GR_MDROPEDITBOXG: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_dropeditboxg(f, a, b, c, d, sp, e); break;

        }
        case GR_MSLIDEHORIZSIZG:
            ami_slidehorizsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSLIDEHORIZSIZ:
            ami_slidehorizsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSLIDEHORIZ:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_slidehoriz(f, a, b, c, d, e, g); break;
        case GR_MSLIDEHORIZG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_slidehorizg(f, a, b, c, d, e, g); break;
        case GR_MSLIDEVERTSIZG:
            ami_slidevertsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSLIDEVERTSIZ:
            ami_slidevertsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSLIDEVERT:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_slidevert(f, a, b, c, d, e, g); break;
        case GR_MSLIDEVERTG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_slidevertg(f, a, b, c, d, e, g); break;
        case GR_MTABBARSIZG:
            a = gi(); b = gi(); c = gi();
            ami_tabbarsizg(f, (ami_tabori)a, b, c, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MTABBARSIZ:
            a = gi(); b = gi(); c = gi();
            ami_tabbarsiz(f, (ami_tabori)a, b, c, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MTABBARCLIENTG:
            a = gi(); b = gi(); c = gi();
            ami_tabbarclientg(f, (ami_tabori)a, b, c, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MTABBARCLIENT:
            a = gi(); b = gi(); c = gi();
            ami_tabbarclient(f, (ami_tabori)a, b, c, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MTABBAR: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            g = gi(); ami_tabbar(f, a, b, c, d, sp, (ami_tabori)e, g); break;

        }
        case GR_MTABBARG: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            g = gi(); ami_tabbarg(f, a, b, c, d, sp, (ami_tabori)e, g); break;

        }
        case GR_MTABSEL: a = gi(); b = gi(); ami_tabsel(f, a, b); break;

        /* --------------------------------------------------- dialogs */

        case GR_MALERT:
            gstr(s1, MAXSTR); gstr(s2, MAXSTR); ami_alert(s1, s2); break;
        case GR_MQUERYCOLOR:
            o1 = gi(); o2 = gi(); o3 = gi();
            ami_querycolor(&o1, &o2, &o3);
            rbegin(); ri(o1); ri(o2); ri(o3); rsend(); break;
        case GR_MQUERYOPEN:
            gstr(s1, MAXSTR);
            ami_queryopen(s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend(); break;
        case GR_MQUERYSAVE:
            gstr(s1, MAXSTR);
            ami_querysave(s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend(); break;
        case GR_MQUERYFIND: {

            ami_qfnopts opt;
            gstr(s1, MAXSTR); opt = (ami_qfnopts)gi();
            ami_queryfind(s1, MAXSTR, &opt);
            rbegin(); rstr(s1, MAXSTR); ri((long)opt); rsend(); break;

        }
        case GR_MQUERYFINDREP: {

            ami_qfropts opt;
            gstr(s1, MAXSTR); gstr(s2, MAXSTR); opt = (ami_qfropts)gi();
            ami_queryfindrep(s1, MAXSTR, s2, MAXSTR, &opt);
            rbegin(); rstr(s1, MAXSTR); rstr(s2, MAXSTR); ri((long)opt);
            rsend(); break;

        }
        case GR_MQUERYFONT: {

            long fc, s, fr, fg, fb, br, bg, bb;
            ami_qfteffects eff;

            fc = gi(); s = gi(); fr = gi(); fg = gi(); fb = gi();
            br = gi(); bg = gi(); bb = gi(); eff = (ami_qfteffects)gi();
            ami_queryfont(f, &fc, &s, &fr, &fg, &fb, &br, &bg, &bb, &eff);
            rbegin(); ri(fc); ri(s); ri(fr); ri(fg); ri(fb);
            ri(br); ri(bg); ri(bb); ri((long)eff); rsend(); break;

        }

        default: sesserr("Unknown message");

    }

}

/* Execute every message in the datagram at rbase, dlen bytes; a
   datagram carries one or more messages back to back, the header
   lengths delimiting them. rbuf walks the batch; a session error inside
   longjmps out, and the fault path restores rbuf to rbase. */
static void dgram(long dlen)

{

    long off = 0;
    long ml;

    while (off < dlen) {

        if (dlen-off < (long)sizeof(gr_msghdr)) {

            rbuf = rbase;
            sesserr("Short message from client");

        }
        rbuf = rbase+off;
        ml = rhdr()->len;
        if (ml < (long)sizeof(gr_msghdr) || off+ml > dlen) {

            rbuf = rbase;
            sesserr("Message framing error");

        }
        rlen = ml;
        roff = sizeof(gr_msghdr);
        dispatch();
        off += ml;

    }
    rbuf = rbase;

}

/* A crash in the field must tell its story: the fatal signals print
   the backtrace of the thread that died, resolvable with addr2line,
   before going down. gdb slows the process enough to hide the races
   this exists to catch. */
static void crashbt(int sig)

{

    void* frames[32];
    int   n;

    fprintf(stderr, "*** graph_server: fatal signal %d, backtrace:\n", sig);
    n = backtrace(frames, 32);
    backtrace_symbols_fd(frames, n, 2);
    _exit(128+sig);

}

/*******************************************************************************

Main: bind, greet, pump, serve

Both channels bind before the hello is answered, so nothing the client
sends afterward can land on an unbound port. The event pump starts once
the client's event channel hello has arrived and named the peer.

*******************************************************************************/

int main(int argc, char* argv[])

{

    signal(SIGSEGV, crashbt);
    signal(SIGBUS, crashbt);
    signal(SIGFPE, crashbt);

    const char* sp;
    pthread_t   st;
    sigset_t    sset;
    unsigned long la;

    {

        int i;

        for (i = 1; i < argc; i++)
            if (!strcmp(argv[i], "--trace")) gtrace = 1;

    }
    if (getenv("GRAPH_TRACE")) gtrace = 1;
    /* the console interrupt waits on its own thread and asks the program
       to terminate, as the close button does; the mask went up in the
       first constructor, before any thread existed */
    (void)sset;
    pthread_create(&st, NULL, sigrun, NULL);
    /* The server never holds its final screen: it is infrastructure,
       and its exit must release the display and the ports at once. The
       hold belongs to programs, and a session's program never exits
       this process. */
    ami_autohold(0);
    sp = getenv("GRAPH_PORT");
    srvport = sp && sp[0]? atol(sp): GR_DEFPORT;
    ami_addrnet("127.0.0.1", &la);
    msgmax = ami_maxmsg(la);
    if (msgmax < 1024) error("Message channel too small");
    sbuf = malloc(msgmax);
    rbuf = rbase = malloc(msgmax);
    if (!sbuf || !rbuf) error("Out of memory");

    /* both channels up before anyone is answered. The channels arrive
       with short receive timeouts, meant for transient exchanges where a
       lost datagram should fail the read; these channels are persistent,
       a server without a client yet and a quiet command channel both
       being normal, so the timeouts clear. The logical id of a message
       channel is its descriptor. */
    cmdfn = ami_waitmsg(srvport, 0);
    evtfn = ami_waitmsg(srvport+1, 0);
    {

        /* deep receive buffers: a full flow window must fit with room,
           and events should never drop for a slow moment */
        int rb = 2*1024*1024;

        setsockopt((int)cmdfn, SOL_SOCKET, SO_RCVBUF, &rb, sizeof(rb));
        setsockopt((int)evtfn, SOL_SOCKET, SO_RCVBUF, &rb, sizeof(rb));

    }
    {

        struct timeval tv = { 0, 0 };

        setsockopt((int)cmdfn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt((int)evtfn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    }

    /* the main window is handle 1 */
    h2f[1] = stdout;
    h2lw[1] = 1;

    /* the main window as presented, for the between-session restore */
    {

        const char* bn = strrchr(argv[0], '/');

        snprintf(srvname, sizeof(srvname), "%s", bn? bn+1: argv[0]);
        ami_getsizg(stdout, &origw, &origh);

    }

    /* the idle window says what it is; a blank window reads as a hang */
    printf("Remote display server awaiting connection on port %ld.\n",
           srvport);
    printf("Control-c in this window, or its close button, shuts the "
           "server down.\n");
    fflush(stdout);

    /* the pump lives across sessions: idle it discards, and the display
       can cancel an idle server */
    if (pthread_create(&pump, NULL, evpump, NULL))
        error("Cannot start event pump");

    /* the session loop: serve a client, reset, wait for the next; a
       session error jumps back here, winds down, and recycles. The
       console interrupt, or a terminate from the display of an idle
       server, is the way out. */
    for (;;) {

        long h;
        int  faulted;

        faulted = setjmp(sesjmp);
        rbuf = rbase; /* a fault mid-batch leaves the walk pointer inside */
        if (faulted) goto winddown; /* the pump lives on, idle */

        /* fresh session */
        byeseen = 0;
        sigasked = 0;
        cleaning = 0;

        /* the hello; a mid-session hello was already read and answered,
           and its client is waiting on the event channel exchange */
        if (!hellopend) {

            rlen = ami_rdmsg(cmdfn, rbase, msgmax);
            if (rlen < (long)sizeof(gr_msghdr))
                sesserr("Short message from client");
            if (((gr_msghdr*)rbase)->mid != GR_MHELLO)
                sesserr("Protocol failure: no hello");
            dgram(rlen);

        }
        hellopend = 0;

        /* The event channel hello names the peer for events. The wait is
           bounded: a client that died between hellos must not wedge the
           server, and its successor's hello waits on the command
           channel. The logical id of a message channel is its
           descriptor. */
        {

            struct timeval tv = { 10, 0 };
            fd_set        fs;
            int           r;

            /* the bound by select, the read by the message call, which
               is what records the peer the events go back to */
            FD_ZERO(&fs);
            FD_SET((int)evtfn, &fs);
            r = select((int)evtfn+1, &fs, NULL, NULL, &tv);
            if (r <= 0)
                sesserr("Protocol failure: no event channel hello");
            rlen = ami_rdmsg(evtfn, rbase, msgmax);
            if (rlen < (long)sizeof(gr_msghdr) ||
                ((gr_msghdr*)rbuf)->mid != GR_MEVOPEN)
                sesserr("Protocol failure: no event channel hello");

        }
        thinclear(); /* no pending events from a prior session */
        clientup = 1; /* the pump forwards from here on */

        /* The command loop. A burst of commands executes under one hold
           of the display lock: after the blocking read, the socket
           drains without blocking, so drawing throughput is bounded by
           the wire and not by the heartbeat. The logical id of a message
           channel is its descriptor. */
        while (!byeseen) {

            ssize_t r;

            rlen = ami_rdmsg(cmdfn, rbase, msgmax);
            for (;;) {

                dgram(rlen);
                if (byeseen) break;
                r = recv((int)cmdfn, rbase, msgmax, MSG_DONTWAIT);
                if (r < 0) break; /* the burst is drained */
                rlen = r;

            }

        }
        /* an interrupt asked this session to end: the server was being
           cancelled, so it goes down with the session */
        if (sigasked) { stoppump(); exit(0); }

winddown:
        clientup = 0;
        cleaning = 1; /* a fault in the winddown itself is fatal */

        /* Reset for the next client: the session's windows close, and
           the main window comes back to a reasonable state. A timer the
           session left running is not recovered, the library having no
           way to ask, and would tick into the next session. */
        for (h = 2; h < MAXHND; h++) if (h2f[h]) {

            fclose(h2f[h]);
            h2f[h] = 0;

        }
        wb_purge(stdout); /* the session's widgets go with it */
        ami_title(stdout, srvname); /* the session's title went with it */
        ami_setsizg(stdout, origw, origh); /* and its size */
        ami_auto(stdout, 0); /* off first: the resets below are illegal
                                with it on, and it cannot come back on
                                until the geometry is standard again */
        ami_fover(stdout);
        ami_bover(stdout);
        ami_fcolor(stdout, ami_black);
        ami_bcolor(stdout, ami_white);
        ami_viewoffg(stdout, 0, 0);
        ami_viewscale(stdout, 1.0, 1.0);
        ami_path(stdout, LONG_MAX/4);
        putchar('\f'); /* clear, and home the cursor to the grid */
        fflush(stdout);
        ami_auto(stdout, 1);
        ami_curvis(stdout, 1);
        printf("Session ended. Remote display server awaiting connection "
               "on port %ld.\n", srvport);
        printf("Control-c in this window, or its close button, shuts the "
               "server down.\n");
        fflush(stdout);
        if (faulted) {

            /* drop whatever the dead session left on the channels, so
               the next hello read is not stale traffic */
            while (recv((int)cmdfn, rbase, msgmax, MSG_DONTWAIT) >= 0);
            while (recv((int)evtfn, rbase, msgmax, MSG_DONTWAIT) >= 0);

        }
        cleaning = 0;

    }

}
