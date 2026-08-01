/** ****************************************************************************

\file

\brief WIDGET DEMONSTRATOR: HOW TO MAKE YOUR OWN WIDGET

Copyright (C) 2026 Scott A. Franco

This is a demonstrator for creating your own widgets on top of Petit-Ami
graphics. It is a cut down copy of gnome_widgets.c, the existence proof
that a complete widget set can be built on nothing but the PA drawing and
event calls, holding exactly one widget so the shape of the thing can be
seen whole.

The widget here is a "kick button": a button whose face is a still photo
of a soccer player. Press it and the player kicks the ball, as a frame
animation with a kick sound at the moment of contact. That is all it
does. It is deliberately not useful; it is deliberately a widget no stock
widget set would ever give you, which is the point of rolling your own.

                            THE RECIPE

A widget is a small frameless subwindow of the window that owns it. That
one idea buys everything else: the graphics module clips it, buffers it,
occludes it and delivers events addressed to it, exactly as for any other
window. What remains for the widget author is:

1. CREATE: open a child window over the widget's rectangle in the parent,
   with frame, buffering, auto and cursor all off. See widget() below.
   Keep a record per widget, found from the parent file and the widget id.

2. INTERCEPT: hook the event chain once at startup (ami_eventsover). Every
   event delivered to the program passes through you first; events whose
   window id belongs to one of your widget windows are yours, everything
   else is passed on down the chain unseen. See widget_event() below.
   Because modules hook the same chain in constructor order, your widgets
   coexist with the stock widget set; each module fields its own.

3. DRAW: paint the face on the widget window when a redraw event arrives,
   and whenever your own state changes. The face here is a picture
   (ami_loadpict/ami_picture); the stock set draws with lines, rectangles
   and text. Anything the drawing calls can do can be a widget face.

4. BEHAVE: react to the mouse and keyboard events aimed at your window,
   and report to the parent window's event queue with ami_sendevent. A
   widget standing in for a stock kind would report the stock code (a
   button reports ami_etbutton); a widget with no stock analogue defines
   its own code from the user space, ami_etuser and up, with the event
   record's union fields its own to assign. This widget defines ETKICKED
   (widget_demo.h), fired not at the press but at the frame where the
   foot meets the ball, the widget id riding in the record.

5. DESTROY: close the widget window and release the record, and do the
   same for all widgets in a window when the window file closes under
   them (the close override below).

What was left out of the copy, and why: the theme system (this widget's
few colors are constants below; gnome_widgets reads a config-driven theme
table); the "window 0" font metrics window (a picture face needs no font
measurements); the sizing calls (the demo takes the rectangle it is
given); and the subclassing fields, by which stock widgets build compound
widgets from simpler ones. All are worth studying in gnome_widgets.c once
the skeleton here is understood.

                              THE ASSETS

The animation frames (tests/widget_demo/kick01.bmp ...) are derived from
"Boykickingball-slowmotion-tokyo-20160423.webm" by Nesnad, from Wikimedia
Commons, licensed CC BY 4.0 (creativecommons.org/licenses/by/4.0). The
kick sound (tests/widget_demo/kick.wav) is synthesized for this demo and
carries no license restrictions. Pictures load only as 24 bit uncompressed
.bmp, which is why the frames are stored that way.

                          BSD LICENSE INFORMATION

Copyright (C) 2026 - Scott A. Franco

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the project nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include <localdefs.h>
#include <graphics.h>
#include <sound.h>

#include <widget_demo.h>

#define MAXFIL 100 /* maximum open files */
#define MAXWIG 100 /* maximum widgets per window */

/* the animation */
#define FRAMES    24  /* frames in the kick animation */
#define FRATE     700 /* frame time, timer units of 100us: 70ms a frame */
#define KICKFRM   12  /* the frame where the foot meets the ball */
#define FRMTIMER  1   /* the widget window timer used for the animation */
/* where the assets live, relative to the run directory */
#define FRMFILE   "tests/widget_demo/kick%02d" /* .bmp is implied */
#define WAVFILE   "tests/widget_demo/kick"     /* .wav is implied */
#define KICKWAV   42 /* wave table slot for the kick sound. The wave table
                        is global to the program, so a widget package must
                        pick a slot its clients are unlikely to use */

/* the widget's few colors, packed RGB. The stock set draws from a theme
   table read from configuration; a one widget demo hardcodes them */
#define RGB(r, g, b) ((r) << 16|(g) << 8|(b))
#define COL_OUTLINE  RGB(0x8f, 0x8f, 0x8f) /* resting outline */
#define COL_HOVER    RGB(0x4a, 0x90, 0xd9) /* outline with the mouse over */
#define COL_FOCUS    RGB(0x1a, 0x5f, 0xb4) /* outline with the focus */

/* system override for close(): a window file closed with widgets still
   in it takes the widgets down with it */
typedef int (*pclose_t)(int);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
#ifndef __MACH__
#define NOCANCEL
extern void ovr_close_nocancel(pclose_t nfp, pclose_t* ofp);
#endif

/* Widget record: everything the widget needs to know about one instance.
   The stock set's record carries the union of what seventeen widget kinds
   need; one widget carries only its own */
typedef struct wigrec* wigptr;
typedef struct wigrec {

    /** next entry in free list */            wigptr next;
    /** output file for the widget window */  FILE*  wf;
    /** parent window */                      FILE*  parent;
    /** id number, client assigned */         long   id;
    /** widget window id */                   long   wid;
    /** widget is enabled */                  long   enb;
    /** focused */                            long   focus;
    /** hovered */                            long   hover;
    /** the kick is running */                long   playing;
    /** current animation frame, 1 based */   long   frame;

} wigrec;

/* Per window file record: the widgets living in that window, indexed by
   client id. Ids may be negative (anonymous); 0 is never used */
typedef struct filrec* filptr;
typedef struct filrec {

    wigptr widgets[MAXWIG*2+1];

} filrec;

static ami_pevthan widget_event_old;    /* downchain event handler */
static wigptr      wigfre;              /* free widget entry list */
static filptr      opnfil[MAXFIL];      /* open files table */
static wigptr      xltwig[MAXFIL*2+1];  /* widget window id to widget */
static int         sndopn;              /* kick sound is loaded */

/* saved close() vectors */
static pclose_t ofpclose;
#ifdef NOCANCEL
static pclose_t ofpclose_nocancel;
#endif

/** ****************************************************************************

Process error

*******************************************************************************/

static void error(
    /** Error string */ char* es
)

{

    fprintf(stderr, "Error: widget_demo: %s\n", es);
    fflush(stderr);

    exit(1);

}

/** ****************************************************************************

Get file entry

Allocates and initializes a new file entry.

\returns File entry pointer.

*******************************************************************************/

static filptr getfil(void)

{

    filptr fp;
    long   i;

    fp = malloc(sizeof(filrec));
    if (!fp) error("Out of memory");
    for (i = 0; i < MAXWIG*2+1; i++) fp->widgets[i] = NULL;

    return (fp);

}

/** ****************************************************************************

Make file entry

If the indicated file does not have a file tracking structure, one is
created. Otherwise it is a no-op.

*******************************************************************************/

static void makfil(
    /** Window file */ FILE* f
)

{

    long fn;

    if (!f) error("Invalid window file");
    fn = fileno(f);
    if (fn < 0 || fn > MAXFIL) error("Invalid file number");
    if (!opnfil[fn]) opnfil[fn] = getfil();

}

/** ****************************************************************************

Get widget

Gets a widget record off the free list, or allocates one, and initializes
it to the resting state.

\returns Pointer to new widget.

*******************************************************************************/

static wigptr getwig(void)

{

    wigptr wp;

    if (wigfre) {

        wp = wigfre;
        wigfre = wigfre->next;

    } else wp = malloc(sizeof(wigrec));
    if (!wp) error("Out of memory");
    wp->enb = TRUE;
    wp->focus = FALSE;
    wp->hover = FALSE;
    wp->playing = FALSE;
    wp->frame = 1; /* resting on the still */

    return (wp);

}

/** ****************************************************************************

Put widget

Releases a widget record to the free list.

*******************************************************************************/

static void putwig(
    /** Pointer to widget to free */ wigptr wp
)

{

    wp->next = wigfre;
    wigfre = wp;

}

/** ****************************************************************************

Find widget

Given a window file and a widget id, returns the widget. Validates both.

\returns Pointer to found widget.

*******************************************************************************/

static wigptr fndwig(
    /** Window file pointer */ FILE* f,
    /** Logical widget id */   long  id
)

{

    long fn;

    if (id <= -MAXWIG || id > MAXWIG || !id) error("Invalid widget id");
    fn = fileno(f);
    if (fn < 0 || fn > MAXFIL) error("Invalid file number");
    if (!opnfil[fn] || !opnfil[fn]->widgets[id+MAXWIG])
        error("No widget by given id");

    return (opnfil[fn]->widgets[id+MAXWIG]);

}

/** ****************************************************************************

Create widget

The heart of the pattern: a widget is a frameless subwindow of its parent,
placed over the widget's rectangle. Buffering is off so draws appear as
made, auto is off so drawing cannot scroll it, the cursor is off so no
cursor shows in it, and the frame is off so it is nothing but its face.
Events addressed to this window arrive at widget_event() below.

*******************************************************************************/

static void widget(
    /** Parent window file */              FILE* f,
    /** Containing rectangle for widget */ long x1, long y1, long x2, long y2,
    /** logical id for widget */           long id,
    /** Widget I/O pointer */              wigptr* wpr
)

{

    long   fn;
    wigptr wp;

    if (id <= -MAXWIG || id > MAXWIG || !id) error("Invalid widget id");
    makfil(f);
    fn = fileno(f);
    if (opnfil[fn]->widgets[id+MAXWIG]) error("Widget by id already in use");
    wp = getwig();
    opnfil[fn]->widgets[id+MAXWIG] = wp;
    wp->wid = ami_getwinid(); /* allocate a buried window id */
    ami_openwin(&stdin, &wp->wf, f, wp->wid); /* open widget window */
    wp->parent = f;
    xltwig[wp->wid+MAXFIL] = wp; /* events for that window find the widget */
    wp->id = id;
    ami_buffer(wp->wf, FALSE); /* draws appear as made */
    ami_auto(wp->wf, FALSE);   /* drawing must not scroll the face */
    ami_curvis(wp->wf, FALSE); /* a widget face never shows a cursor */
    ami_frame(wp->wf, FALSE);  /* the widget is nothing but its face */
    ami_setposg(wp->wf, x1, y1);
    ami_setsizg(wp->wf, x2-x1, y2-y1);
    ami_binvis(wp->wf); /* no background writes */

    *wpr = wp;

}

/** ****************************************************************************

Kick button draw

Draws the face: the current animation frame scaled over the whole widget
(frame 1 is the resting still photo), and an outline over it whose color
reports the widget's state: focused, hovered, or resting. The face is a
picture, but any drawing the graphics calls can make can be a face.

*******************************************************************************/

static void kickbutton_draw(
    /** Widget data pointer */ wigptr wg
)

{

    long fr = wg->frame;

    if (fr < 1 || fr > FRAMES) fr = 1;
    ami_picture(wg->wf, fr, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    ami_linewidth(wg->wf, 3);
    if (wg->focus && wg->enb)
        ami_fcolorg(wg->wf, COL_FOCUS >> 16&0xff, COL_FOCUS >> 8&0xff,
                            COL_FOCUS&0xff);
    else if (wg->hover && wg->enb)
        ami_fcolorg(wg->wf, COL_HOVER >> 16&0xff, COL_HOVER >> 8&0xff,
                            COL_HOVER&0xff);
    else
        ami_fcolorg(wg->wf, COL_OUTLINE >> 16&0xff, COL_OUTLINE >> 8&0xff,
                            COL_OUTLINE&0xff);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 20, 20);

}

/** ****************************************************************************

Kick button event handler

The widget's behavior, fed by the events the graphics module addresses to
the widget's window:

- redraw paints the face.
- a button 1 press starts the kick: a repeating timer on the widget
  window paces the frames.
- each timer tick advances a frame; at the contact frame the kick sound
  fires and the kick reports to the parent as the user defined ETKICKED
  event -- the widget's meaning is "he kicked the ball", and that is
  when the ball is kicked; after the last frame the timer is killed and
  the face returns to the still.
- focus, hover and their ends just repaint, showing the state color.

*******************************************************************************/

static void kickbutton_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr      wg
)

{

    ami_evtrec er;

    if (ev->etype == ami_etredraw) kickbutton_draw(wg);
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        if (wg->enb && !wg->playing) {

            /* roll the animation off the press */
            wg->playing = TRUE;
            wg->frame = 1;
            ami_timer(wg->wf, FRMTIMER, FRATE, TRUE);

        }

    } else if (ev->etype == ami_ettim && ev->timnum == FRMTIMER) {

        if (wg->playing && wg->frame < FRAMES) {

            wg->frame++;
            if (wg->frame == KICKFRM) {

                /* The foot meets the ball: the sound, and the report.
                   The kick goes to the parent as the widget's own user
                   defined event, at the moment it means. */
                if (sndopn) ami_playwave(AMI_WAVE_OUT, 0, KICKWAV);
                er.etype = ETKICKED;
                er.butid = wg->id; /* ours to assign for our own code */
                ami_sendevent(wg->parent, &er);

            }
            kickbutton_draw(wg);

        } else { /* done: back to the still */

            ami_killtimer(wg->wf, FRMTIMER);
            wg->playing = FALSE;
            wg->frame = 1;
            kickbutton_draw(wg);

        }

    } else if (ev->etype == ami_etfocus) {

        wg->focus = TRUE;
        kickbutton_draw(wg);

    } else if (ev->etype == ami_etnofocus) {

        wg->focus = FALSE;
        kickbutton_draw(wg);

    } else if (ev->etype == ami_ethover) {

        wg->hover = TRUE;
        kickbutton_draw(wg);

    } else if (ev->etype == ami_etnohover) {

        wg->hover = FALSE;
        kickbutton_draw(wg);

    }

}

/** ****************************************************************************

Widget event intercept

Hooked into the event chain at startup. Events whose window id belongs to
one of our widget windows are dispatched to the widget's handler; all
others pass down the chain to whoever hooked before us -- the stock
widget set, and finally the program. This is what lets any number of
widget packages coexist: each recognizes only its own windows.

*******************************************************************************/

static void widget_event(
    /** Event record pointer */ ami_evtrec* ev
)

{

    wigptr wg;

    wg = NULL;
    if (ev->winid > -MAXFIL && ev->winid <= MAXFIL)
        wg = xltwig[ev->winid+MAXFIL];
    if (!wg) widget_event_old(ev); /* not ours: down the chain */
    else kickbutton_event(ev, wg); /* ours: the one widget we hold */

}

/** ****************************************************************************

Kill widget internal

Takes the widget down: window id tracking cleared first, so a window id
reused after the widget is freed cannot dispatch events to the freed
entry, then the widget window is closed and the record released.

*******************************************************************************/

static void intkillwidget(
    /** file id */           long fn,
    /** Logical widget id */ long id
)

{

    wigptr wp;

    if (fn < 0 || fn > MAXFIL) error("Invalid file number");
    if (!opnfil[fn]) error("File by id not open");
    if (id <= -MAXWIG || id > MAXWIG || !id) error("Invalid widget id");
    if (!opnfil[fn]->widgets[id+MAXWIG]) error("No widget by given id");
    wp = opnfil[fn]->widgets[id+MAXWIG];
    xltwig[wp->wid+MAXFIL] = NULL;
    fclose(wp->wf); /* close the window, erasing the widget */
    opnfil[fn]->widgets[id+MAXWIG] = NULL;
    putwig(wp);

}

/** ****************************************************************************

Close intercepts

A window file closed with widgets still in it takes the widgets down with
it; without this, closing the parent stranded the widget windows and
their records.

*******************************************************************************/

static int ivclose(
    /** Base call vector */ pclose_t closedc,
    /** File logical id */  int fd
)

{

    int i;

    if (fd >= 0 && fd < MAXFIL && opnfil[fd]) {

        for (i = 0; i < MAXWIG*2+1; i++)
            if (opnfil[fd]->widgets[i]) intkillwidget(fd, i-MAXWIG);
        free(opnfil[fd]);
        opnfil[fd] = NULL;

    }

    return (*closedc)(fd);

}

static int iclose(int fd)

{

    return ivclose(ofpclose, fd);

}

#ifdef NOCANCEL
static int iclose_nocancel(int fd)

{

    return ivclose(ofpclose_nocancel, fd);

}
#endif

/** ****************************************************************************

Create kick button

The public call: creates a kick button in the given window over the given
rectangle, in graphical coordinates, with the given client id. The face
photo and the animation frames load onto the widget's own window (each
window carries its own picture table), and the kick sound loads once for
the program.

A widget package defines its own entry calls. The stock set overrides the
ami_ widget vectors because it implements the standard set; a user widget
has no vector to override, and none is needed: this is just a call.

*******************************************************************************/

void kickbutton(
    /** Parent window file */   FILE* f,
    /** Containing rectangle */ long x1, long y1, long x2, long y2,
    /** logical id for widget */ long id
)

{

    wigptr wp;
    char   fn[100];
    long   i;

    widget(f, x1, y1, x2, y2, id, &wp); /* the subwindow and the record */
    /* the still and the animation frames, pictures 1..FRAMES on the
       widget window */
    for (i = 1; i <= FRAMES; i++) {

        sprintf(fn, FRMFILE, (int)i);
        ami_loadpict(wp->wf, i, fn);

    }
    if (!sndopn) { /* the kick sound loads once */

        ami_openwaveout(AMI_WAVE_OUT);
        ami_loadwave(KICKWAV, WAVFILE);
        sndopn = TRUE;

    }
    kickbutton_draw(wp); /* show the resting face */

}

/** ****************************************************************************

Kill kick button

Removes the kick button by id from the window. The package that creates a
widget kind removes it: the stock killwidget call knows nothing of this
widget, and need not.

*******************************************************************************/

void kickbuttonkill(
    /** Parent window file */    FILE* f,
    /** logical id for widget */ long  id
)

{

    wigptr wp = fndwig(f, id); /* validates */

    if (wp->playing) ami_killtimer(wp->wf, FRMTIMER);
    intkillwidget(fileno(f), id);

}

/** ****************************************************************************

Enable/disable kick button

A disabled button shows no state colors and does not kick.

*******************************************************************************/

void kickbuttonenable(
    /** Parent window file */    FILE* f,
    /** logical id for widget */ long  id,
    /** enable/disable */        long  e
)

{

    wigptr wp = fndwig(f, id);

    wp->enb = !!e;
    kickbutton_draw(wp);

}

/** ****************************************************************************

Widget demo startup/shutdown

Runs after graphics (102), sound (103) and the stock widget set (105), so
the event hook lands on top of the chain: our widgets are recognized
first, everything else falls through to the stock set and the program.
The teardown runs first for the same ordering reason, and puts back the
vectors it took, verifying that what it removes is its own.

*******************************************************************************/

static void init_widget_demo(void) __attribute__((constructor (110)));
static void init_widget_demo()

{

    long fn;

    wigfre = NULL;
    sndopn = FALSE;
    for (fn = 0; fn < MAXFIL; fn++) opnfil[fn] = NULL;
    for (fn = 0; fn < MAXFIL*2+1; fn++) xltwig[fn] = NULL;

    /* hook the event chain */
    ami_eventsover(widget_event, &widget_event_old);

    /* hook close(), so a closing window takes its widgets with it */
    ovr_close(iclose, &ofpclose);
#ifdef NOCANCEL
    ovr_close_nocancel(iclose_nocancel, &ofpclose_nocancel);
#endif

}

static void deinit_widget_demo(void) __attribute__((destructor (110)));
static void deinit_widget_demo()

{

    ami_pevthan cppevt;
    pclose_t    cppclose;
#ifdef NOCANCEL
    pclose_t    cppclose_nocancel;
#endif

    /* put back the vectors we took, and check what we took out is ours */
    ami_eventsover(widget_event_old, &cppevt);
    if (cppevt != widget_event) error("Event vector mismatch");
    ovr_close(ofpclose, &cppclose);
    if (cppclose != iclose) error("System override vector mismatch");
#ifdef NOCANCEL
    ovr_close_nocancel(ofpclose_nocancel, &cppclose_nocancel);
    if (cppclose_nocancel != iclose_nocancel)
        error("System override vector mismatch");
#endif

}
