/*******************************************************************************
*                                                                              *
*                             RANDOM TORTURE TEST                              *
*                                                                              *
* Runs random procedures of the Petit-Ami API, on several threads at once,     *
* until it is cancelled. The point is not what the screen looks like: nothing  *
* the test draws is checked. The point is that the library survives any        *
* sequence of calls from any number of threads, so the test's only judgement   *
* is that it is still running. A fault is the library's error message, a       *
* crash or a hang, and the seed and the narration (-v) replay the run that    *
* found it.                                                                    *
*                                                                              *
* Each test is one case of a large switch, drawn from the API of each module   *
* and from the individual tests, terminal_test, graphics_test, window_test     *
* and the rest: text, attributes and colors, fonts, cursor motion and          *
* scrolling, the figures, buffers, child windows -- opened at random places    *
* and sizes, with random tests run inside them, recursively -- the network,    *
* against a message server on a thread of this program, and, asked for, the    *
* synthesizer. Every parameter is random within the range the window or        *
* buffer allows.                                                               *
*                                                                              *
* Each thread has a random stream of its own, seeded from the run's seed and   *
* the thread's number, so a run is reproduced from its seed alone, thread by   *
* thread; the interleaving between threads is the system's and is the thing    *
* under test. The main thread runs the event loop: it keeps the count in the   *
* title, and the terminate event -- the close button, control-c in the        *
* window -- stops the workers and ends the run.                                *
*                                                                              *
*******************************************************************************/

/*******************************************************************************

Usage:

    random_test [-t threads] [-s] [-v] [seed]

    -t     The number of worker threads, 4 by default; 1 runs single
           threaded.
    -s     Include the synthesizer: random notes and instruments on the
           first synthesizer output. Off unless asked for, since it plays.
    -v     Narrate: each thread names each test on the error channel as it
           starts it, so the last line names the test a fault was in.
    seed   The seed. Left off, or 0, one is taken from the clock. The seed
           is always reported on the error channel, so any run can be
           replayed.

The run ends on the terminate event: close the window, or control-c in it.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include <localdefs.h>
#include <services.h>
#include <sound.h>
#include <network.h>
#include <graphics.h>

#define MAXTHREADS 32   /* worker threads at most */
#define MAXDEPTH   3    /* child windows within child windows */
#define MAXCHILD   30   /* tests run in a child window at most */
#define NETPORT    4919 /* the message echo server's port */
#define MAXMSG     1000 /* longest message exchanged */
#define MAXSTR     80   /* longest string written */
#define MAXBUF     2000 /* largest buffer dimension asked for, in pixels */
#define STATTIM    10   /* the timer the count runs on */

/* the tests: one case each */
typedef enum {

    tchar, tstring, tcursor, tcursorg, thome, tmove, tdel, tnewline, tclear,
    tattr, tcolor, tcolorg, tfont, tfontsiz, ttab, tscroll, tscrollg, tauto,
    tcurvis, twrtstr, tjust,
    tline, tlinewidth, tlinestyle, trect, tfrect, trrect, tfrrect, tellipse,
    tfellipse, tarc, tfarc, tfchord, ttriangle, tpixel, tmode,
    tselect, tsizbuf, tsizbufg, tbuffer, tchild, tchildpos, tchildsiz,
    tchildorder, tchildframe, ttitle, tview,
    tnet, tsound,
    tmax /* the count */

} testcod;

static const char* testnam[] = {

    "char", "string", "cursor", "cursorg", "home", "move", "del", "newline",
    "clear", "attr", "color", "colorg", "font", "fontsiz", "tab", "scroll",
    "scrollg", "auto", "curvis", "wrtstr", "just",
    "line", "linewidth", "linestyle", "rect", "frect", "rrect", "frrect",
    "ellipse", "fellipse", "arc", "farc", "fchord", "triangle", "pixel", "mode",
    "select", "sizbuf", "sizbufg", "buffer", "child", "childpos", "childsiz",
    "childorder", "childframe", "title", "view",
    "net", "sound"

};

/* a thread's state: its random stream, and the window its tests are on */
typedef struct {

    int                thread; /* thread number, 1-n */
    unsigned long long rs;     /* random stream state */
    FILE*              f;      /* the window under test */
    int                depth;  /* child window depth, 0 on the main window */
    int                child;  /* the window is a child, this thread's own */
    int                bufon;  /* the window's buffer is on */
    unsigned long long count;  /* tests run */

} ctx;

/* the run's settings */
static int                threads = 4;    /* worker threads */
static int                sound = FALSE;  /* the synthesizer is included */
static int                verbose = FALSE; /* narrate each test */
static unsigned long long seed;           /* the run's seed */

/* the run's state */
static volatile int  stop;          /* the workers are to stop */
static ami_long      lockid;        /* the run's lock */
static ami_long      sigid;         /* a worker stopped */
static int           running;       /* workers still running */
static int           started;       /* workers started, under the lock */
static ctx           ctxs[MAXTHREADS]; /* the threads' states */
static ami_long      nextwid = 2;   /* next window id, under the lock */
static ami_ulong     netaddr;       /* the loopback address */
static int           netok;         /* the echo server is up */

/*******************************************************************************

Random numbers

A stream per thread, xorshift, so that a run replays from its seed whatever
the platform's rand() does, and whatever the other threads do.

*******************************************************************************/

static unsigned long long rnd64(ctx* c)

{

    unsigned long long x = c->rs;

    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    c->rs = x;

    return (x*0x2545F4914F6CDD1DULL);

}

/* 0 to n-1 */
static ami_long rnd(ctx* c, ami_long n)

{

    if (n <= 1) return (0);

    return ((ami_long)(rnd64(c)%(unsigned long long)n));

}

/* lo to hi inclusive */
static ami_long rndr(ctx* c, ami_long lo, ami_long hi)

{

    if (hi <= lo) return (lo);

    return (lo+rnd(c, hi-lo+1));

}

/* a coordinate in the window, 1 to its extent */
static ami_long rndx(ctx* c) { return (rndr(c, 1, ami_maxxg(c->f))); }
static ami_long rndy(ctx* c) { return (rndr(c, 1, ami_maxyg(c->f))); }

/* a ratioed value, 0 to LONG_MAX, as colors and angles take */
static ami_long rndratio(ctx* c) { return ((ami_long)(rnd64(c) & LONG_MAX)); }

/* a random printable string */
static void rndstr(ctx* c, char* s, int max)

{

    int i, n = (int)rndr(c, 1, max);

    for (i = 0; i < n; i++) s[i] = (char)rndr(c, ' ', '~');
    s[n] = 0;

}

/*******************************************************************************

The message echo server

Runs on a thread of its own for the life of the program: takes the port
once, and sends back whatever it is sent, to whoever sent it. The network
test's exchange is against it. The socket is one for the run: a message sent
while the port was between one binding and the next would be lost, and the
sender's read of the reply would time out, which the library reports as a
fault. A read waits for the message to be there first, since the read
itself gives up after a while.

*******************************************************************************/

static void echoserver(void)

{

    ami_long fn, len;
    char     buf[MAXMSG];

    fn = ami_waitmsg(NETPORT, FALSE);
    for (;;) {

        while (!ami_rdymsg(fn, 100000)) ; /* until a message is there */
        len = ami_rdmsg(fn, buf, MAXMSG);
        if (len > 0) ami_wrmsg(fn, buf, len);

    }

}

/*******************************************************************************

The tests

Each takes the thread's state and does one random thing to its window.

*******************************************************************************/

static void runtests(ctx* c, int n); /* forward */

/* a child window of the current window: opened at a random place and size,
   a random number of tests run in it, and closed */
static void childtest(ctx* c)

{

    ctx      cc;
    FILE*    win;
    ami_long wid, px, py, w, h;

    if (c->depth >= MAXDEPTH) return;
    ami_lock(lockid);
    wid = nextwid++;
    ami_unlock(lockid);
    ami_openwin(&stdin, &win, c->f, wid);
    /* somewhere on the parent, some size that fits it */
    px = rndr(c, 1, ami_maxxg(c->f)/2);
    py = rndr(c, 1, ami_maxyg(c->f)/2);
    w = rndr(c, ami_chrsizx(win)*4, ami_maxxg(c->f)-px+1);
    h = rndr(c, ami_chrsizy(win)*2, ami_maxyg(c->f)-py+1);
    ami_setposg(win, px, py);
    ami_setsizg(win, w, h);
    cc = *c;
    cc.f = win;
    cc.depth = c->depth+1;
    cc.child = TRUE;
    cc.bufon = TRUE;
    runtests(&cc, (int)rndr(c, 1, MAXCHILD));
    c->rs = cc.rs; /* the stream went on in the child */
    c->count = cc.count;
    fclose(win);

}

/* the network exchange: a random message to the echo server and back. The
   server takes one exchange at a time, so the exchanges are one at a time. */
static void nettest(ctx* c)

{

    ami_long fn, len, i;
    char     buf[MAXMSG];

    if (!netok) return;
    len = rndr(c, 1, MAXMSG);
    for (i = 0; i < len; i++) buf[i] = (char)rnd(c, 256);
    ami_lock(lockid);
    fn = ami_openmsg(netaddr, NETPORT, FALSE);
    ami_wrmsg(fn, buf, len);
    ami_rdmsg(fn, buf, MAXMSG);
    ami_clsmsg(fn);
    ami_unlock(lockid);

}

/* the synthesizer: a note on, a note off, or an instrument change, on a
   random channel */
static void soundtest(ctx* c)

{

    ami_long ch = rndr(c, 1, 16);

    switch (rnd(c, 3)) {

        case 0: ami_noteon(1, 0, ch, rndr(c, 1, 128), rndratio(c)); break;
        case 1: ami_noteoff(1, 0, ch, rndr(c, 1, 128), rndratio(c)); break;
        case 2: ami_instchange(1, 0, ch, rndr(c, 1, 128)); break;

    }

}

/* one random test */
static void test(ctx* c)

{

    FILE*    f = c->f;
    testcod  t;
    char     s[MAXSTR+1];
    ami_long x1, y1, x2, y2, x3, y3, xs, ys;

    do {

        t = (testcod)rnd(c, tmax);
        /* the tests that are not for this window or this run come up again */
        if (t == tsound && !sound) t = tmax;
        else if ((t == tsizbuf || t == tsizbufg) && !c->bufon) t = tmax;
        else if ((t == tbuffer || t == tchildpos || t == tchildsiz ||
                  t == tchildorder || t == tchildframe) && !c->child) t = tmax;

    } while (t == tmax);
    c->count++;
    if (verbose) {

        fprintf(stderr, "random_test: thread %d test %llu: %s\n",
                c->thread, c->count, testnam[t]);
        fflush(stderr);

    }
    switch (t) {

        /* text */
        case tchar:     fputc((int)rndr(c, ' ', '~'), f); break;
        case tstring:   rndstr(c, s, MAXSTR); fputs(s, f); break;
        case tcursor:   ami_cursor(f, rndr(c, 1, ami_maxx(f)), rndr(c, 1, ami_maxy(f)));
                        break;
        case tcursorg:  ami_cursorg(f, rndx(c), rndy(c)); break;
        case thome:     ami_home(f); break;
        case tmove:     switch (rnd(c, 4)) {

                            case 0: ami_up(f); break;
                            case 1: ami_down(f); break;
                            case 2: ami_left(f); break;
                            case 3: ami_right(f); break;

                        }
                        break;
        case tdel:      ami_del(f); break;
        case tnewline:  fputc('\n', f); break;
        case tclear:    fputc('\f', f); break;
        case tattr:     switch (rnd(c, 16)) {

                            case 0:  ami_bold(f, rnd(c, 2)); break;
                            case 1:  ami_italic(f, rnd(c, 2)); break;
                            case 2:  ami_underline(f, rnd(c, 2)); break;
                            case 3:  ami_strikeout(f, rnd(c, 2)); break;
                            case 4:  ami_standout(f, rnd(c, 2)); break;
                            case 5:  ami_reverse(f, rnd(c, 2)); break;
                            case 6:  ami_blink(f, rnd(c, 2)); break;
                            case 7:  ami_superscript(f, rnd(c, 2)); break;
                            case 8:  ami_subscript(f, rnd(c, 2)); break;
                            case 9:  ami_condensed(f, rnd(c, 2)); break;
                            case 10: ami_extended(f, rnd(c, 2)); break;
                            case 11: ami_xlight(f, rnd(c, 2)); break;
                            case 12: ami_light(f, rnd(c, 2)); break;
                            case 13: ami_xbold(f, rnd(c, 2)); break;
                            case 14: ami_hollow(f, rnd(c, 2)); break;
                            case 15: ami_raised(f, rnd(c, 2)); break;

                        }
                        break;
        case tcolor:    if (rnd(c, 2)) ami_fcolor(f, (ami_color)rnd(c, 8));
                        else ami_bcolor(f, (ami_color)rnd(c, 8));
                        break;
        case tcolorg:   if (rnd(c, 2))
                            ami_fcolorg(f, rndratio(c), rndratio(c), rndratio(c));
                        else
                            ami_bcolorg(f, rndratio(c), rndratio(c), rndratio(c));
                        break;
        case tfont:     ami_font(f, rndr(c, AMI_FONT_TERM, AMI_FONT_TECH)); break;
        case tfontsiz:  ami_fontsiz(f, rndr(c, 6, 48)); break;
        case ttab:      switch (rnd(c, 4)) {

                            case 0: ami_settab(f, rndr(c, 1, ami_maxx(f))); break;
                            case 1: ami_restab(f, rndr(c, 1, ami_maxx(f))); break;
                            case 2: ami_clrtab(f); break;
                            case 3: fputc('\t', f); break;

                        }
                        break;
        case tscroll:   ami_scroll(f, rndr(c, -3, 3), rndr(c, -3, 3)); break;
        case tscrollg:  ami_scrollg(f, rndr(c, -20, 20), rndr(c, -20, 20)); break;
        case tauto:     ami_auto(f, rnd(c, 2)); break;
        case tcurvis:   ami_curvis(f, rnd(c, 2)); break;
        case twrtstr:   rndstr(c, s, MAXSTR);
                        if (rnd(c, 2)) ami_wrtstr(f, s);
                        else ami_wrtstrn(f, s, rndr(c, 0, (ami_long)strlen(s)));
                        break;
        case tjust:     rndstr(c, s, MAXSTR);
                        x1 = ami_strsiz(f, s);
                        ami_writejust(f, s, rndr(c, x1, x1*2+1));
                        ami_justpos(f, s, rndr(c, 1, (ami_long)strlen(s)), x1*2);
                        ami_chrpos(f, s, rndr(c, 1, (ami_long)strlen(s)));
                        break;

        /* figures */
        case tline:     ami_line(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case tlinewidth: ami_linewidth(f, rndr(c, 1, 10)); break;
        case tlinestyle: ami_linestyle(f, (ami_lstyle)rnd(c, 3)); break;
        case trect:     ami_rect(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case tfrect:    ami_frect(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case trrect:
        case tfrrect:   x1 = rndx(c); y1 = rndy(c); x2 = rndx(c); y2 = rndy(c);
                        xs = rndr(c, 0, labs(x2-x1)/2+1);
                        ys = rndr(c, 0, labs(y2-y1)/2+1);
                        if (t == trrect) ami_rrect(f, x1, y1, x2, y2, xs, ys);
                        else ami_frrect(f, x1, y1, x2, y2, xs, ys);
                        break;
        case tellipse:  ami_ellipse(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case tfellipse: ami_fellipse(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case tarc:      ami_arc(f, rndx(c), rndy(c), rndx(c), rndy(c),
                                rndratio(c), rndratio(c));
                        break;
        case tfarc:     ami_farc(f, rndx(c), rndy(c), rndx(c), rndy(c),
                                 rndratio(c), rndratio(c));
                        break;
        case tfchord:   ami_fchord(f, rndx(c), rndy(c), rndx(c), rndy(c),
                                   rndratio(c), rndratio(c));
                        break;
        case ttriangle: x1 = rndx(c); y1 = rndy(c); x2 = rndx(c); y2 = rndy(c);
                        x3 = rndx(c); y3 = rndy(c);
                        ami_ftriangle(f, x1, y1, x2, y2, x3, y3);
                        break;
        case tpixel:    ami_setpixel(f, rndx(c), rndy(c)); break;
        case tmode:     switch (rnd(c, 10)) {

                            case 0: ami_fover(f); break;
                            case 1: ami_bover(f); break;
                            case 2: ami_finvis(f); break;
                            case 3: ami_binvis(f); break;
                            case 4: ami_fxor(f); break;
                            case 5: ami_bxor(f); break;
                            case 6: ami_fand(f); break;
                            case 7: ami_band(f); break;
                            case 8: ami_for(f); break;
                            case 9: ami_bor(f); break;

                        }
                        break;

        /* buffers and windows */
        case tselect:   ami_select(f, rndr(c, 1, 4), rndr(c, 1, 4)); break;
        case tsizbuf:   ami_sizbuf(f, rndr(c, 1, MAXBUF/ami_chrsizx(f)),
                                   rndr(c, 1, MAXBUF/ami_chrsizy(f)));
                        break;
        case tsizbufg:  ami_sizbufg(f, rndr(c, ami_chrsizx(f), MAXBUF),
                                    rndr(c, ami_chrsizy(f), MAXBUF));
                        break;
        case tbuffer:   c->bufon = (int)rnd(c, 2); ami_buffer(f, c->bufon); break;
        case tchild:    childtest(c); break;
        case tchildpos: ami_setposg(f, rndr(c, 1, MAXBUF), rndr(c, 1, MAXBUF)); break;
        case tchildsiz: ami_setsizg(f, rndr(c, ami_chrsizx(f)*2, MAXBUF),
                                    rndr(c, ami_chrsizy(f)*2, MAXBUF));
                        break;
        case tchildorder: if (rnd(c, 2)) ami_front(f); else ami_back(f); break;
        case tchildframe: switch (rnd(c, 3)) {

                            case 0: ami_frame(f, rnd(c, 2)); break;
                            case 1: ami_sysbar(f, rnd(c, 2)); break;
                            case 2: ami_sizable(f, rnd(c, 2)); break;

                        }
                        break;
        case ttitle:    rndstr(c, s, MAXSTR); ami_title(f, s); break;
        case tview:     if (rnd(c, 2)) ami_viewoffg(f, rndr(c, -50, 50), rndr(c, -50, 50));
                        else {

                            float sx = (float)rndr(c, 5, 20)/10.0f;
                            float sy = (float)rndr(c, 5, 20)/10.0f;

                            ami_viewscale(f, sx, sy);

                        }
                        break;

        /* the other modules */
        case tnet:      nettest(c); break;
        case tsound:    soundtest(c); break;
        case tmax:      break;

    }

}

/* n tests, or until the stop */
static void runtests(ctx* c, int n)

{

    int i;

    for (i = 0; i < n && !stop; i++) test(c);

}

/*******************************************************************************

The worker thread

Takes the next thread's state, runs tests on the main window until the stop,
and reports itself stopped.

*******************************************************************************/

static void worker(void)

{

    ctx* c;

    ami_lock(lockid);
    c = &ctxs[started++];
    ami_unlock(lockid);
    while (!stop) test(c);
    ami_lock(lockid);
    running--;
    ami_sendsig(sigid);
    ami_unlock(lockid);

}

/*******************************************************************************

Main

*******************************************************************************/

int main(int argc, char* argv[])

{

    int                i;
    ami_evtrec         er;
    unsigned long long total;
    char               title[80];

    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "-t") && i+1 < argc) {

            threads = atoi(argv[++i]);
            if (threads < 1) threads = 1;
            if (threads > MAXTHREADS) threads = MAXTHREADS;

        } else if (!strcmp(argv[i], "-s")) sound = TRUE;
        else if (!strcmp(argv[i], "-v")) verbose = TRUE;
        else seed = strtoull(argv[i], NULL, 10);

    }
    if (!seed) seed = (unsigned long long)time(NULL)*2654435761ULL+
                      (unsigned long long)ami_clock();
    fprintf(stderr, "random_test: seed %llu, %d thread%s%s\n", seed, threads,
            threads == 1? "": "s", sound? ", with sound": "");
    fflush(stderr);

    ami_autohold(FALSE); /* the run ends on the terminate event, not on its own */
    ami_curvis(stdout, FALSE);
    ami_title(stdout, "random_test");
    lockid = ami_initlock();
    sigid = ami_initsig();

    /* the message echo server, on its thread */
    ami_addrnet("127.0.0.1", &netaddr);
    ami_newthread(echoserver);
    netok = TRUE;

    /* the synthesizer, when asked for and there is one */
    if (sound) {

        if (ami_synthout() < 1) {

            fprintf(stderr, "random_test: no synthesizer output, sound left out\n");
            sound = FALSE;

        } else ami_opensynthout(1);

    }

    /* the workers: each with a stream of its own from the seed */
    for (i = 0; i < threads; i++) {

        ctxs[i].thread = i+1;
        ctxs[i].rs = seed ^ ((unsigned long long)(i+1)*0x9E3779B97F4A7C15ULL);
        if (!ctxs[i].rs) ctxs[i].rs = 1; /* xorshift must not start at zero */
        ctxs[i].f = stdout;
        ctxs[i].depth = 0;
        ctxs[i].child = FALSE;
        ctxs[i].bufon = TRUE;
        ctxs[i].count = 0;

    }
    running = threads;
    for (i = 0; i < threads; i++) ami_newthread(worker);

    /* the event loop: the count in the title once a second, until the
       terminate event */
    ami_timer(stdout, STATTIM, 10000, TRUE);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_ettim && er.timnum == STATTIM) {

            total = 0;
            for (i = 0; i < threads; i++) total += ctxs[i].count;
            sprintf(title, "random_test: %llu tests", total);
            ami_title(stdout, title);

        }

    } while (er.etype != ami_etterm);

    /* stop the workers, and wait for them to finish what they were in */
    stop = TRUE;
    ami_lock(lockid);
    while (running > 0) ami_waitsig(lockid, sigid);
    ami_unlock(lockid);
    if (sound) ami_closesynthout(1);
    total = 0;
    for (i = 0; i < threads; i++) total += ctxs[i].count;
    fprintf(stderr, "random_test: seed %llu, %llu tests, stopped\n", seed, total);

    return (0);

}
