/** ****************************************************************************
*                                                                              *
*                          SYSTEM EVENT HANDLER                                *
*                                                                              *
* System event handler for MAC OS X/BSD.                                       *
*                                                                              *
* Contains an interface to the system event handler. This is an abstraction    *
* for having the system be able to handle multiple event types and return a    *
* logical event for each one. It is used by terminal and graphical terminal    *
* Petit-Ami modules to abstract the differences between linux, which uses      *
* select() and pselect() for this purpose, and BSD/Mac OS X, which uses        *
* kqueues for this purpose.                                                    *
*                                                                              *
* It implements the following system event types:                              *
*                                                                              *
* input         An input file, has data to read.                               *
* signal        The OS has issued a signal.                                    *
* timer         A timer has fired.                                             *
*                                                                              *
* The client registers each event that will be included. In the case of        *
* timers, the API controls the timer period and repeat, and gives a call to    *
* cancel an active timer.                                                      *
*                                                                              *
* Calls in this package:                                                       *
*                                                                              *
* int system_event_addseinp(int fid);                                          *
*                                                                              *
* Registers an input file to be monitored for input ready, or one or more      *
* bytes ready to read. The logical system event number is returned.            *
*                                                                              *
* int system_event_addsesig(int sig);                                          *
*                                                                              *
* Registers a signal to be monitored. A handler is also registered, and sends  *
* an event when the signal occurs. The logical system event number is          *
* returned.                                                                    *
*                                                                              *
* int system_event_addsetim(int sid, int t, int r);                            *
*                                                                              *
* Activates a timer with the given time and repeat state. An event occurs when *
* the timer fires, and may repeat. Both accepts a logical system event number  *
* and returns one. If the input event is 0, a new event is created.            *
*                                                                              *
* void system_event_deasetim(int sid);                                         *
*                                                                              *
* Deactivates the timer with the given system event number regardless of the   *
* state of the timer.                                                          *
*                                                                              *
* void system_event_getsevt(sevptr ev);                                        *
*                                                                              *
* Returns the next active system event. The event is returned in the record:   *
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

#include <stdio.h>
#include <sys/select.h>
#include <sys/event.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

/* somewhat kludgy, but there should be only one include file for this */
#include "../linux/system_event.h"

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); \
                                fflush(stderr); } while (0)

#define MAXSYS 100 /* number of possible logical system events */

/* logical system event record */
typedef struct systrk* sevtptr; /* pointer to system event */
typedef struct systrk {

    sevttyp typ; /* system event type */
    int     fid; /* logical file id if used */
    int     sig; /* signal number */
    int     ei;  /* event index */

} systrk;

/* logical system event tracking array */
static sevtptr systab[MAXSYS];
static int sysno; /* number of system event ids allocated */

static sigset_t sigmsk; /* signal mask */
static sigset_t sigact; /* signal active */

static struct kevent chgevt[MAXSYS]; /* event change list */
static int kerque; /* kernel queue fid */
static int nchg; /* number of change filters defined */

/*******************************************************************************

Handle signal from Linux

Handle signal from linux kernel.

*******************************************************************************/

static void sig_handler(int signo)

{

    /* add this signal to active set */
    sigaddset(&sigact, signo);

}

/** *****************************************************************************

Get system logical event

Finds a slot in the system event id table and allocates that, then returns the
resuling logical id.

*******************************************************************************/

static int getsys(void)

{

    int sid; /* system event id */
    int fid; /* found id */

    sid = 0; /* set 1st table entry */
    fid = -1; /* set no id */
    while (sid < MAXSYS && fid < 0) { /* search free system id */

        if (!systab[sid]) fid = sid; /* set entry found */
        sid++; /* next slot */

    }
    if (fid < 0) { /* event table full */

        fprintf(stderr, "*** System event: Event table full\n");
        fflush(stderr);
        exit(1);

    }
    systab[fid] = malloc(sizeof(systrk)); /* get new event entry */
    if (!systab[fid]) {

        fprintf(stderr, "*** System event: Out of memory\n");
        fflush(stderr);
        exit(1);

    }
    systab[fid]->typ = se_none; /* set no type */
    systab[fid]->fid = -1; /* set no file id */
    systab[fid]->sig = -1; /* set no signal id */

    sysno++; /* count active entries */

    return (fid+1); /* exit with system id */

}

/** *****************************************************************************

Add file id to system event handler

Adds the given logical file id to the system event handler set. Returns a system
event logical number.

*******************************************************************************/

int system_event_addseinp(int fid)

{

    int sid; /* system logical event id */

    sid = getsys(); /* get a new system event id */
    systab[sid-1]->typ = se_inp; /* set type */
    systab[sid-1]->fid = fid; /* set fid for file */

    if (nchg >= MAXSYS) {

        fprintf(stderr, "*** System event: Too many events defined\n");
        fflush(stderr);
        exit(1);

    }

    /* construct event for fid */
    EV_SET(&chgevt[nchg], fid, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
    systab[sid-1]->ei = nchg; /* link to event entry */
    nchg++; /* count events registered */

    return (sid); /* exit with logical event id */

}

/** *****************************************************************************

Add signal to system event handler

Adds the given signal number to the system event handler set. Note that this
routine assumes you will let this package handle the interrupt completely,
including handling the signal. If both use of this package to handle signals,
as well as allowing the client to handle signals, a chain handler using
sigaction should be used.

*******************************************************************************/

int system_event_addsesig(int sig)

{

    int sid; /* system logical event id */

    sid = getsys(); /* get a new system event id */
    systab[sid-1]->typ = se_inp; /* set type */
    systab[sid-1]->sig = sig; /* set signal */

    /* add this signal to mask set */
    sigaddset(&sigmsk, sig);

    /* add signal handler */
    signal(sig, sig_handler);

    /* construct event for fid */
    EV_SET(&chgevt[nchg], sig, EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0, 0);
    systab[sid-1]->ei = nchg; /* link to event entry */
    nchg++; /* count events registered */

    return (sid); /* exit with logical event id */

}

/** *****************************************************************************

Activate timer entry

Sets a timer to run with the number of 100us counts, and a repeat status.
Both takes a system event id and returns one. If the system event id is 0, a new
system id is allocated. Then either the new system id or the existing one is
returned.

*******************************************************************************/

int system_event_addsetim(int sid, int t, int r)

{

    if (!sid) { /* no previous system id */

        sid = getsys(); /* get a new system event id */
        systab[sid-1]->typ = se_tim; /* set type */
        systab[sid-1]->ei = nchg; /* link to event entry */
        nchg++; /* count events registered */

    }

    /* construct timer event */
    EV_SET(&chgevt[systab[sid-1]->ei], sid, EVFILT_TIMER, EV_ADD | EV_ENABLE |
           !!r*EV_ONESHOT, NOTE_USECONDS, (int64_t)t*100, 0);

    return (sid);

}

/** *****************************************************************************

Deactivate timer entry

Kills a given timer, by it's id number. Only repeating timers should be killed.
Killed timers are not removed. Once a timer is set active, it is always set
in reserve.

*******************************************************************************/

void system_event_deasetim(int sid)

{

    if (sid <= 0 || !systab[sid-1]) {

        fprintf(stderr, "*** System event: Invalid system event id\n");
        fflush(stderr);
        exit(1);

    }

    /* construct timer event */
    EV_SET(&chgevt[systab[sid-1]->ei], sid, EVFILT_TIMER, EV_ADD | EV_DISABLE,
           0, 0, 0);

}

/** *****************************************************************************

Get system event

Get the next system event that occurs. One of an input key, a timer, a frame
timer, or a joystick event occurs, and we return this. The event that is
returned is cleared.

*******************************************************************************/

void system_event_getsevt(sevptr ev)

{

    int           rv;             /* return value */
    int           ti;             /* index for timers */
    int           ji;             /* index for joysticks */
    int           si;             /* index for system event entries */
    uint64_t      exp;            /* timer expiration time */
    struct kevent events[MAXSYS]; /* event list */
    int           nev;            /* number of events available */
    int           ei;             /* event index */

    ev->typ = se_none; /* set no event occurred */
    nev = 0; /* set no events read */
    ei = 0; /* clear event index */
    do { /* find an active event */

        if (ei >= nev) {

            /* out of events, read the next ones */
            nev = kevent(kerque, chgevt, nchg, events, MAXSYS, NULL);
            if (nev <= 0) {

                fprintf(stderr, "*** System event: Error reading next event\n");
                fflush(stderr);
                exit(1);

            }
            ei = 0; /* reset the event index */

        }
        for (ei = 0; ei < nev; ei++) { /* traverse the event array */

            if (events[ei].filter == EVFILT_READ) { /* it's a fid ready event */

                for (si = 0; si < sysno; si++) if (systab[si])
                    if (systab[si]->fid >= 0 && systab[si]->typ == se_inp &&
                        events[ei].ident == systab[si]->fid) {

                    /* fid has flagged */
                    ev->typ = systab[si]->typ; /* set key event occurred */
                    ev->lse = si+1; /* set system logical event no */

                }

            } else if (events[ei].filter == EVFILT_SIGNAL) { /* it's a signal */

                for (si = 0; si < sysno; si++) if (systab[si])
                    if (systab[si]->typ == se_sig && systab[si]->sig > 0 &&
                        sigismember(&sigact, systab[si]->sig)) {

                    /* signal has flagged */
                    ev->typ = systab[si]->typ; /* set key event occurred */
                    ev->lse = si+1; /* set system logical event no */
                    sigdelset(&sigact, systab[si]->sig); /* remove signal */

                }

            } else if (events[ei].filter == EVFILT_TIMER) {

                /* ident carries the sid */
                ev->typ = systab[si]->typ; /* set key event occurred */
                ev->lse = events[ei].ident; /* set system logical event no */

            }

        }

    } while (ev->typ == se_none);
#if 1
switch (ev->typ) {
    case se_none: fprintf(stderr, "lse: %d None\n", ev->lse); break;
    case se_inp:  fprintf(stderr, "lse: %d Input file ready\n", ev->lse); break;
    case se_tim:  fprintf(stderr, "lse: %d Timer\n", ev->lse); break;
    case se_sig:  fprintf(stderr, "lse: %d Signal\n", ev->lse); break;
}
fflush(stderr);
#endif

}

/** *****************************************************************************

Initialize system event handler

Sets up the system event handler.

*******************************************************************************/

static void init_system_event (void) __attribute__((constructor (101)));
static void init_system_event()

{

    int si; /* index for system event array */

    /* clear the tracking array */
    for (si = 0; si < MAXSYS; si++) systab[si] = NULL;

    /* clear the set of signals we capture */
    sigemptyset(&sigmsk);

    /* clear active signals */
    sigemptyset(&sigmsk);

    sysno = 0; /* set no active system events */

    /* create kernel queue */
    kerque = kqueue();
    if (kerque ==-1) {

        fprintf(stderr, "*** System event: Could not create kernel queue\n");
        fflush(stderr);
        exit(1);

    }

    /* set no change filters registered */
    nchg = 0;

}

/** *****************************************************************************

Deinitialize system event handler

Tears down the system event handler.

*******************************************************************************/

static void deinit_system_event (void) __attribute__((destructor (101)));
static void deinit_system_event()

{

    /* close the event queue */
    close(kerque);

}
