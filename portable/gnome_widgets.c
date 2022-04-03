/** ****************************************************************************

                           WIDGETS PACKAGE FOR GNOME APPEARANCE

                            Copyright (C) 2021 Scott A. Franco

                                2021/08/08 S. A. Franco

This is the Gnome look and feel widget package for Petit-Ami. This is used for
systems that don't have a standard widget package, like XWindows. It uses
Petit-Ami graphics statements to construct and operate widgets, and thus is
portable to any system with Petit-Ami up to the graphical management level.

Although it is portable, the look and feel should match the system it is on.
Thus this package implements the Gnome look and feel, or at least a reasonable
subset of it.

                          BSD LICENSE INFORMATION

Copyright (C) 2019 - Scott A. Franco

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

/* whitebook definitions */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* linux definitions */
#include <limits.h>

/* local definitions */
#include <localdefs.h>
#include <config.h>
#include <graphics.h>

/* external definitions */
extern char *program_invocation_short_name;

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

#define MAXFIL 100 /* maximum open files */
#define MAXWIG 100 /* maximum widgets per window */
/* amount of space in pixels to add around scrollbar sliders */
#define ENDSPACE 6
#define ENDLEDSPC 10 /* space at start and end of text edit box */

/* macro to make a color from RGB values */
#define RGB(r, g, b) (r<<16|g<<8|b)

/* macros to unpack color table entries to INT_MAX ratioed numbers */
#define RED(v)   (INT_MAX/256*(v >> 16))       /* red */
#define GREEN(v) (INT_MAX/256*(v >> 8 & 0xff)) /* green */
#define BLUE(v)  (INT_MAX/256*(v & 0xff))      /* blue */

/* default values for color table. Note these can be overridden. */
#define TD_BUTTBACKPRESSED  RGB(211, 211, 211) /* button background pressed */
#define TD_BUTTBACK         RGB(252, 252, 252) /* button background not pressed */
#define TD_BUTTOUT          RGB(196, 196, 196) /* button outline */
#define TD_TEXT             RGB(61, 61, 61)    /* widget face text */
#define TD_TEXTDIS          RGB(191, 191, 191) /* widget face text disabled */
#define TD_FOCUS            RGB(236, 174, 152) /* widget focused outline */
#define TD_CHKRAD           RGB(146, 77, 139)  /* checkbox/radio button selected */
#define TD_CHKRADOUT        RGB(186, 186, 186) /* checkbox/radio button outline */
#define TD_SCROLLBACK       RGB(210, 210, 210) /* scrollbar background */
#define TD_SCROLLBAR        RGB(135, 135, 135) /* scrollbar not pressed */
#define TD_SCROLLBARPRESSED RGB(195, 65, 19)   /* scrollbar pressed */

/* values table ids */

typedef enum {

    th_buttbackpressed,  /* button background when pressed */
    th_buttback,         /* button background when not pressed */
    th_buttout,          /* button outline */
    th_text,             /* button face text enabled */
    th_textdis,          /* button face text disabled */
    th_focus,            /* button focused outline */
    th_chkrad,           /* checkbox/radio button selected */
    th_chkradout,        /* checkbox/radio button outline */
    th_scrollback,       /* scrollbar background */
    th_scrollbar,        /* scrollbar not pressed */
    th_scrollbarpressed, /* scrollbar pressed */
    th_endmarker         /* end of theme entries */

} themeindex;

/* widget type */
typedef enum  {

    wtbutton, wtcheckbox, wtradiobutton, wtgroup, wtbackground,
    wtscrollvert, wtscrollhoriz, wtnumselbox, wteditbox,
    wtprogressbar, wtlistbox, wtdropbox, wtdropeditbox,
    wtslidehoriz, wtslidevert, wttabbar, wtalert, wtquerycolor,
    wtqueryopen, wtquerysave, wtqueryfind, wtqueryfindrep,
    wtqueryfont

} wigtyp;

/* Widget control structure */
typedef struct wigrec* wigptr;
typedef struct wigrec {

    wigptr next;       /* next entry in list */
    wigtyp typ;        /* type of widget */
    int    pressed;    /* in the pressed state */
    int    lpressed;   /* last pressed state */
    int    select;     /* the current on/off state */
    FILE*  wf;         /* output file for the widget window */
    char*  face;       /* face text */
    FILE*  parent;     /* parent window */
    FILE*  evtfil;     /* file to post menu events to */
    int    id;         /* id number */
    int    wid;        /* widget window id */
    int    enb;        /* widget is enabled */
    int    sclsiz;     /* scrollbar size in MAXINT ratio */
    int    sclpos;     /* scrollbar position in MAXINT ratio */
    int    mpx, mpy;   /* mouse tracking in widget */
    int    lmpx, lmpy; /* last mouse position */
    int    curs;       /* text cursor */
    int    tleft;      /* text left side index */
    int    focus;      /* focused */
    int    ins;        /* insert/overwrite mode */

} wigrec;

/* File tracking.
  Files can be passthrough to the OS, or can be associated with a window. If
  on a window, they can be output, or they can be input. In the case of
  input, the file has its own input queue, and will receive input from all
  windows that are attached to it. */
typedef struct filrec* filptr;
typedef struct filrec {

    wigptr widgets[MAXWIG]; /* table of widgets in window */

} filrec;

static pa_pevthan    widget_event_old;   /* previous event vector save */
static wigptr        wigfre;             /* free widget entry list */
static filptr        opnfil[MAXFIL];     /* open files table */
static wigptr        xltwig[MAXFIL*2+1]; /* widget entry equivalence table */
static FILE*         win0;               /* "window zero" dummy window */
/* table of colors or other theme values */
static unsigned long themetable[th_endmarker];

/** ****************************************************************************

Process error

*******************************************************************************/

static void error(char* es)

{

    fprintf(stderr, "Error: widgets: %s\n", es);
    fflush(stderr);

    exit(1);

}

/*******************************************************************************

Place string in storage

Places the given string into dynamic storage, and returns that.

*******************************************************************************/

static char* str(char* s)

{

    char* p;

    p = malloc(strlen(s)+1);
    strcpy(p, s);

    return (p);

}

/** ****************************************************************************

Get file entry

Allocates and initializes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

static filptr getfil(void)

{

    filptr fp;
    int    i;

    fp = malloc(sizeof(filrec)); /* get new file entry */
    /* clear widget table */
    for (i = 0; i < MAXWIG; i++) fp->widgets[i] = NULL;

    return (fp); /* exit with file entry */

}

/** ****************************************************************************

Make file entry

If the indicated file does not contain a file control structure, one is created.
Otherwise it is a no-op.

*******************************************************************************/

static void makfil(FILE* f)

{

    int fn;

    if (!f) error("Invalid window file");
    fn = fileno(f); /* get the file logical number */
    if (fn > MAXFIL) error("Invalid file number");
    /* check table empty */
    if (!opnfil[fn]) opnfil[fn] = getfil(); /* allocate file entry */

}

/*******************************************************************************

Get widget

Get a widget and place into the window tracking list. If a free widget entry
is available, that will be used, otherwise a new entry is allocated.

*******************************************************************************/

static wigptr getwig(void)

{

    wigptr wp; /* widget pointer */

    if (wigfre) { /* used entry exists, get that */

        wp = wigfre; /* index top entry */
        wigfre = wigfre->next; /* gap out */

    } else wp = malloc(sizeof(wigrec)); /* get entry */

    return wp; /* return entry */

}

/*******************************************************************************

Put widget

Removes the widget from the window list, and releases the widget entry to free
list.

*******************************************************************************/

static void putwig(wigptr wp)

{

    wp->next = wigfre; /* push to free list */
    wigfre = wp;

}

/*******************************************************************************

Find widget

Given a file specification and a widget id, returns a pointer to the given
widget. Validates the file and the widget number.

*******************************************************************************/

static wigptr fndwig(FILE* f, int id)

{

    int       fn;  /* logical file name */
    wigptr    wp;  /* widget entry pointer */

    if (id <= 0 || id > MAXWIG) error("Invalid widget id");
    fn = fileno(f); /* get the file index */
    if (fn < 0 || fn > MAXFIL) error("Invalid file number");
    if (!opnfil[fn]->widgets[id]) error("No widget by given id");
    wp = opnfil[fn]->widgets[id]; /* index that */

    return (wp); /* return the widget pointer */

}

/*******************************************************************************

Send redraw to window

Sends a redraw request to the given window. The common workflow with widgets
is to reconfigure it by changing the parameters of it, then sending it a redraw
to update itself with the new parameters.

*******************************************************************************/

static void widget_redraw(FILE* f)

{

    pa_evtrec ev;  /* outbound menu event */

    ev.etype = pa_etredraw; /* set redraw event */
    pa_sendevent(f, &ev); /* send to widget window */

}

/** ****************************************************************************

Draw foreground color from theme table

Takes a file and a theme index, and sets the foreground color from the theme
table.

*******************************************************************************/

static void fcolort(FILE* f, themeindex t)

{

    pa_fcolorg(f, RED(themetable[t]), GREEN(themetable[t]),
                  BLUE(themetable[t]));

}

/** ****************************************************************************

Draw background color from theme table

Takes a file and a theme index, and sets the background color from the theme
table.

*******************************************************************************/

static void bcolort(FILE* f, themeindex t)

{

    pa_bcolorg(f, RED(themetable[t]), GREEN(themetable[t]),
                  BLUE(themetable[t]));

}

/** ****************************************************************************

Button event handler

Handles the events posted to buttons.

*******************************************************************************/

static void button_draw(wigptr wg)

{

    /* color the background */
    if (wg->pressed) fcolort(wg->wf, th_buttbackpressed);
    else fcolort(wg->wf, th_buttback);
    pa_frect(wg->wf, 1, 1, pa_maxxg(wg->wf),
              pa_maxyg(wg->wf));
    /* outline */
    pa_linewidth(wg->wf, 4);
    if (wg->focus) fcolort(wg->wf, th_focus);
    else fcolort(wg->wf, th_buttout);
    pa_rrect(wg->wf, 2, 2, pa_maxxg(wg->wf)-1,
             pa_maxyg(wg->wf)-1, 20, 20);
    if (wg->enb) fcolort(wg->wf, th_text);
    else fcolort(wg->wf, th_textdis);
    pa_cursorg(wg->wf,
               pa_maxxg(wg->wf)/2-pa_strsiz(wg->wf, wg->face)/2,
               pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2);
    fprintf(wg->wf, "%s", wg->face); /* place button face */

}

static void button_event(pa_evtrec* ev, wigptr wg)

{

    pa_evtrec er; /* outbound button event */

    if (ev->etype == pa_etredraw) button_draw(wg); /* redraw the window */
    else if (ev->etype == pa_etmouba && ev->amoubn == 1) {

        if (wg->enb) { /* enabled */

            /* send event back to parent window */
            er.etype = pa_etbutton; /* set button event */
            er.butid = wg->id; /* set id */
            pa_sendevent(wg->parent, &er); /* send the event to the parent */

        }

        /* process button press */
        wg->pressed = TRUE;
        button_draw(wg); /* redraw the window */

    } else if (ev->etype == pa_etmoubd  && ev->dmoubn == 1) {

        wg->pressed = FALSE;
        button_draw(wg); /* redraw the window */

    } else if (ev->etype == pa_etfocus) {

        wg->focus = 1; /* in focus */
        button_draw(wg); /* redraw the window */

    } else if (ev->etype == pa_etnofocus) {

        wg->focus = 0; /* out of focus */
        button_draw(wg); /* redraw the window */

    }

}

/** ****************************************************************************

Checkbox event handler

Handles the events posted to checkboxes.

*******************************************************************************/

static void checkbox_draw(wigptr wg)

{

    int sq; /* size of checkbox square */
    int sqm; /* center x of checkbox square */
    int sqo; /* checkbox offset left */
    int md; /* checkbox center line */
    int cb; /* bounding box of check figure */

    /* color the background */
    pa_fcolor(wg->wf, pa_backcolor);
    pa_frect(wg->wf, 1, 1, pa_maxxg(wg->wf), pa_maxyg(wg->wf));
    /* outline */
    pa_linewidth(wg->wf, 4);
    if (wg->focus) {

        fcolort(wg->wf, th_focus);
        pa_rrect(wg->wf, 2, 2, pa_maxxg(wg->wf)-1,
                 pa_maxyg(wg->wf)-1, 20, 20);

    }
    /* draw text */
    if (wg->enb) fcolort(wg->wf, th_text);
    else fcolort(wg->wf, th_textdis);
    pa_cursorg(wg->wf, pa_chrsizy(wg->wf)+pa_chrsizy(wg->wf)/2,
                       pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2);
    fprintf(wg->wf, "%s", wg->face); /* place button face */
    /* set size of square as ratio of font height */
    sq = 0.80*pa_chrsizy(wg->wf);
    md = pa_maxyg(wg->wf)/2; /* set middle line of checkbox */
    sqo = pa_maxyg(wg->wf)/4; /* set offset of square from left */
    sqm = sqo+sq/2; /* set square middle x */
    cb = sq*.70; /* set bounding box of check figure */

    if (wg->select) {

        /* place selected checkmark */
        fcolort(wg->wf, th_chkrad);
        pa_frrect(wg->wf, sqo, md-sq/2, sqo+sq, md+sq/2, 10, 10);
        pa_fcolor(wg->wf, pa_white);
        pa_linewidth(wg->wf, 4);
        pa_line(wg->wf, sqm-cb/2, md-cb*.10,
                        sqm, md+cb*.25);
        pa_line(wg->wf, sqm-1, md+cb*.25-1,
                        sqm+cb/2, md-cb*.4);
        pa_linewidth(wg->wf, 1);

    } else {

        /* place non-selected checkmark background */
        pa_fcolor(wg->wf, pa_white);
        pa_frrect(wg->wf, sqo, md-sq/2, sqo+sq, md+sq/2, 10, 10);
        pa_linewidth(wg->wf, 2);
        fcolort(wg->wf, th_chkradout);
        pa_rrect(wg->wf, sqo, md-sq/2, sqo+sq, md+sq/2, 10, 10);

    }

}

static void checkbox_event(pa_evtrec* ev, wigptr wg)

{

    pa_evtrec er; /* outbound checkbox event */

    if (ev->etype == pa_etredraw) checkbox_draw(wg); /* redraw the window */
    else if (ev->etype == pa_etmouba && ev->amoubn == 1) {

        if (wg->enb) { /* enabled */

            /* send event back to parent window */
            er.etype = pa_etchkbox; /* set checkbox event */
            er.butid = wg->id; /* set id */
            pa_sendevent(wg->parent, &er); /* send the event to the parent */

        }
        checkbox_draw(wg);

    } else if (ev->etype == pa_etmoubd && ev->amoubn == 1) checkbox_draw(wg);
    else if (ev->etype == pa_etfocus) {

        wg->focus = 1; /* in focus */
        checkbox_draw(wg); /* redraw the window */

    } else if (ev->etype == pa_etnofocus) {

        wg->focus = 0; /* out of focus */
        checkbox_draw(wg); /* redraw the window */

    }

}

/** ****************************************************************************

Radio button event handler

Handles the events posted to radiobuttons.

*******************************************************************************/

static void radiobutton_draw(wigptr wg)

{

    int cr; /* size of radiobutton circle */
    int crm; /* center x of radiobutton circle */
    int cro; /* radiobutton offset left */
    int md; /* radiobutton center line */

    /* color the background */
    pa_fcolor(wg->wf, pa_backcolor);
    pa_frect(wg->wf, 1, 1, pa_maxxg(wg->wf), pa_maxyg(wg->wf));
    /* outline */
    pa_linewidth(wg->wf, 4);
    if (wg->focus) {

        fcolort(wg->wf, th_focus);
        pa_rrect(wg->wf, 2, 2, pa_maxxg(wg->wf)-1,
                 pa_maxyg(wg->wf)-1, 20, 20);

    }
    /* draw text */
    if (wg->enb) fcolort(wg->wf, th_text);
    else fcolort(wg->wf, th_textdis);
    pa_cursorg(wg->wf, pa_chrsizy(wg->wf)+pa_chrsizy(wg->wf)/2,
                       pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2);
    fprintf(wg->wf, "%s", wg->face); /* place button face */
    /* set size of circle as ratio of font height */
    cr = 0.80*pa_chrsizy(wg->wf);
    md = pa_maxyg(wg->wf)/2; /* set middle line of radiobutton */
    cro = pa_maxyg(wg->wf)/4; /* set offset of circle from left */
    crm = cro+cr/2; /* set circle middle x */

    if (wg->select) {

        /* place select figure */
        fcolort(wg->wf, th_chkrad);
        pa_fellipse(wg->wf, cro, md-cr/2, cro+cr, md+cr/2);
        pa_fcolor(wg->wf, pa_white);
        pa_fellipse(wg->wf, crm-cr/6, md-cr/6, crm+cr/6, md+cr/6);

    } else {

        /* place non-selected background */
        pa_fcolor(wg->wf, pa_white);
        pa_fellipse(wg->wf, cro, md-cr/2, cro+cr, md+cr/2);
        pa_linewidth(wg->wf, 2);
        fcolort(wg->wf, th_chkradout);
        pa_ellipse(wg->wf, cro, md-cr/2, cro+cr, md+cr/2);

    }

}

static void radiobutton_event(pa_evtrec* ev, wigptr wg)

{

    pa_evtrec er; /* outbound radiobutton event */

    if (ev->etype == pa_etredraw) radiobutton_draw(wg); /* redraw the window */
    else if (ev->etype == pa_etmouba && ev->amoubn == 1) {

        if (wg->enb) { /* enabled */

            /* send event back to parent window */
            er.etype = pa_etradbut; /* set button event */
            er.butid = wg->id; /* set id */
            pa_sendevent(wg->parent, &er); /* send the event to the parent */

        }
        radiobutton_draw(wg);

    } else if (ev->etype == pa_etmoubd) radiobutton_draw(wg);
    else if (ev->etype == pa_etfocus) {

        wg->focus = 1; /* in focus */
        radiobutton_draw(wg); /* redraw the window */

    } else if (ev->etype == pa_etnofocus) {

        wg->focus = 0; /* out of focus */
        radiobutton_draw(wg); /* redraw the window */

    }
}

/** ****************************************************************************

Vertical scrollbar event handler

Handles the events posted to vertical scrollbars.

*******************************************************************************/

static void scrollvert_draw(wigptr wg)

{

    int       sclsizp; /* size of slider in pixels */
    int       sclposp; /* offset of slider in pixels */
    int       remsizp; /* remaining space after slider in pixels */
    int       totsizp; /* total size of slider space after padding */
    int       botposp; /* bottom position of slider */
    int       inbar;   /* mouse is in scroll bar */
    int       sclpos;  /* new scrollbar position */
    pa_evtrec er;      /* outbound button event */
    int       y;

    totsizp = pa_maxyg(wg->wf)-ENDSPACE-ENDSPACE; /* find net total slider space */
    /* find size of slider in pixels */
    sclsizp = round((double)totsizp*wg->sclsiz/INT_MAX);
    /* find remaining size after slider */
    remsizp = totsizp-sclsizp;
    /* find position of top of slider in pixels offset */
    sclposp = round((double)remsizp*wg->sclpos/INT_MAX);
    /* find bottom of slider in pixels offset */
    botposp = sclposp+sclsizp-1;
    /* set status of mouse inside the bar */
    inbar = wg->mpy >= sclposp+ENDSPACE && wg->mpy <= botposp+ENDSPACE;

    /* process off bar click */
    if (!inbar && wg->pressed && !wg->lpressed) {

        /* find new top if click is middle */
        y = wg->mpy-sclsizp/2;
        if (y < ENDSPACE) y = ENDSPACE; /* limit top travel */
        else if (y+sclsizp > pa_maxyg(wg->wf)-ENDSPACE)
            y = pa_maxyg(wg->wf)-sclsizp-ENDSPACE;
        /* find new ratioed position */
        sclpos = round((double)INT_MAX*(y-ENDSPACE)/remsizp);
        /* send event back to parent window */
        er.etype = pa_etsclpos; /* set scroll position event */
        er.sclpid = wg->id; /* set id */
        er.sclpos = sclpos; /* set scrollbar position */
        pa_sendevent(wg->parent, &er); /* send the event to the parent */

    } else if (inbar && wg->pressed && wg->lpressed && wg->mpy != wg->lmpy) {

        /* mouse bar drag, process */
        y = sclposp+(wg->mpy-wg->lmpy); /* find difference in pixel location */
        if (y < 0) y = 0; /* limit to zero */
        if (y > remsizp) y = remsizp; /* limit to max */
        /* find new ratioed position */
        sclpos = round((double)INT_MAX*y/remsizp);
        /* send event back to parent window */
        er.etype = pa_etsclpos; /* set scroll position event */
        er.sclpid = wg->id; /* set id */
        er.sclpos = sclpos; /* set scrollbar position */
        pa_sendevent(wg->parent, &er); /* send the event to the parent */

    }

    /* color the background */
    fcolort(wg->wf, th_scrollback);
    pa_frect(wg->wf, 1, 1, pa_maxxg(wg->wf), pa_maxyg(wg->wf));
    if (wg->pressed && inbar)
        /* color as pressed */
        fcolort(wg->wf, th_scrollbarpressed);
    else
        /* color as not pressed */
        fcolort(wg->wf, th_scrollbar);
    pa_frrect(wg->wf, ENDSPACE, ENDSPACE+sclposp, pa_maxxg(wg->wf)-ENDSPACE, ENDSPACE+sclposp+sclsizp,
              10, 10);

}

static void scrollvert_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) scrollvert_draw(wg); /* redraw the window */
    else if (ev->etype == pa_etmouba && ev->amoubn == 1) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = TRUE; /* set is pressed */
        scrollvert_draw(wg);

    } else if (ev->etype == pa_etmoubd) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = FALSE; /* set not pressed */
        scrollvert_draw(wg);

    } else if (ev->etype == pa_etmoumovg) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        /* mouse moved, track position */
        wg->lmpx = wg->mpx; /* move present to last */
        wg->lmpy = wg->mpy;
        wg->mpx = ev->moupxg; /* set present position */
        wg->mpy = ev->moupyg;
        scrollvert_draw(wg);
        wg->lmpx = wg->mpx; /* now set equal to cancel move */
        wg->lmpy = wg->mpy;

    }

}

/** ****************************************************************************

Horizontal scrollbar event handler

Handles the events posted to horizontal scrollbars.

*******************************************************************************/

static void scrollhoriz_draw(wigptr wg)

{

    int       sclsizp; /* size of slider in pixels */
    int       sclposp; /* offset of slider in pixels */
    int       remsizp; /* remaining space after slider in pixels */
    int       totsizp; /* total size of slider space after padding */
    int       botposp; /* bottom position of slider */
    int       inbar;   /* mouse is in scroll bar */
    int       sclpos;  /* new scrollbar position */
    pa_evtrec er;      /* outbound button event */
    int       x;

    totsizp = pa_maxxg(wg->wf)-ENDSPACE-ENDSPACE; /* find net total slider space */
    /* find size of slider in pixels */
    sclsizp = round((double)totsizp*wg->sclsiz/INT_MAX);
    /* find remaining size after slider */
    remsizp = totsizp-sclsizp;
    /* find position of top of slider in pixels offset */
    sclposp = round((double)remsizp*wg->sclpos/INT_MAX);
    /* find bottom of slider in pixels offset */
    botposp = sclposp+sclsizp-1;
    /* set status of mouse inside the bar */
    inbar = wg->mpx >= sclposp+ENDSPACE && wg->mpx <= botposp+ENDSPACE;

    /* process off bar click */
    if (!inbar && wg->pressed && !wg->lpressed) {

        /* find new top if click is middle */
        x = wg->mpx-sclsizp/2;
        if (x < ENDSPACE) x = ENDSPACE; /* limit top travel */
        else if (x+sclsizp > pa_maxxg(wg->wf)-ENDSPACE)
            x = pa_maxxg(wg->wf)-sclsizp-ENDSPACE;
        /* find new ratioed position */
        sclpos = round((double)INT_MAX*(x-ENDSPACE)/remsizp);
        /* send event back to parent window */
        er.etype = pa_etsclpos; /* set scroll position event */
        er.sclpid = wg->id; /* set id */
        er.sclpos = sclpos; /* set scrollbar position */
        pa_sendevent(wg->parent, &er); /* send the event to the parent */

    } else if (inbar && wg->pressed && wg->lpressed && wg->mpx != wg->lmpx) {

        /* mouse bar drag, process */
        x = sclposp+(wg->mpx-wg->lmpx); /* find difference in pixel location */
        if (x < 0) x = 0; /* limit to zero */
        if (x > remsizp) x = remsizp; /* limit to max */
        /* find new ratioed position */
        sclpos = round((double)INT_MAX*x/remsizp);
        /* send event back to parent window */
        er.etype = pa_etsclpos; /* set scroll position event */
        er.sclpid = wg->id; /* set id */
        er.sclpos = sclpos; /* set scrollbar position */
        pa_sendevent(wg->parent, &er); /* send the event to the parent */

    }

    /* color the background */
    fcolort(wg->wf, th_scrollback);
    pa_frect(wg->wf, 1, 1, pa_maxxg(wg->wf), pa_maxyg(wg->wf));
    if (wg->pressed && inbar)
        /* color as pressed */
        fcolort(wg->wf, th_scrollbarpressed);
    else
        /* color as not pressed */
        fcolort(wg->wf, th_scrollbar);
    pa_frrect(wg->wf, ENDSPACE+sclposp, ENDSPACE, ENDSPACE+sclposp+sclsizp, pa_maxyg(wg->wf)-ENDSPACE,
              10, 10);

}

static void scrollhoriz_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) scrollhoriz_draw(wg); /* redraw the window */
    else if (ev->etype == pa_etmouba && ev->amoubn == 1) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = TRUE; /* set is pressed */
        scrollhoriz_draw(wg);

    } else if (ev->etype == pa_etmoubd) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = FALSE; /* set not pressed */
        scrollhoriz_draw(wg);

    } else if (ev->etype == pa_etmoumovg) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        /* mouse moved, track position */
        wg->lmpx = wg->mpx; /* move present to last */
        wg->lmpy = wg->mpy;
        wg->mpx = ev->moupxg; /* set present position */
        wg->mpy = ev->moupyg;
        scrollhoriz_draw(wg);
        wg->lmpx = wg->mpx; /* now set equal to cancel move */
        wg->lmpy = wg->mpy;

    }

}

/** ****************************************************************************

Group box event handler

Handles the events posted to group boxes.

*******************************************************************************/

static void group_draw(wigptr wg)

{

    /* color the background */
    pa_fcolor(wg->wf, pa_backcolor);
    pa_frect(wg->wf, 1, 1, pa_maxxg(wg->wf), pa_maxyg(wg->wf));
    fcolort(wg->wf, th_buttout);
    pa_linewidth(wg->wf, 2);
    pa_rect(wg->wf, 2, pa_chrsizy(wg->wf)/2, pa_maxxg(wg->wf), pa_maxyg(wg->wf));
    pa_fcolor(wg->wf, pa_black);
    pa_cursorg(wg->wf, 1, 1);
    pa_bover(wg->wf);
    pa_bcolor(wg->wf, pa_backcolor);
    fprintf(wg->wf, "%s", wg->face); /* place button face */


}

static void group_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) group_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Group box event handler

Handles the events posted to group boxes.

*******************************************************************************/

static void background_draw(wigptr wg)

{

    /* color the background */
    pa_fcolor(wg->wf, pa_backcolor);
    pa_frect(wg->wf, 1, 1, pa_maxxg(wg->wf), pa_maxyg(wg->wf));
    pa_fcolor(wg->wf, pa_black);

}

static void background_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) background_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Number select box event handler

Handles the events posted to number select boxes.

*******************************************************************************/

static void numselbox_draw(wigptr wg)

{

}

static void numselbox_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) numselbox_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Edit box event handler

Handles the events posted to edit boxes.

*******************************************************************************/

static void editbox_draw(wigptr wg)

{

    int   cl;
    int   x;
    char* s;

    /* color the background */
    pa_fcolor(wg->wf, pa_white);
    pa_frect(wg->wf, 1, 1, pa_maxxg(wg->wf), pa_maxyg(wg->wf));
    /* outline */
    if (wg->focus) {

        pa_linewidth(wg->wf, 4);
        pa_fcolorg(wg->wf, INT_MAX/256*236, INT_MAX/256*174, INT_MAX/256*152);

    } else {

        pa_linewidth(wg->wf, 2);
        pa_fcolorg(wg->wf, INT_MAX/256*196, INT_MAX/256*196, INT_MAX/256*196);

    }
    pa_rrect(wg->wf, 2, 2, pa_maxxg(wg->wf)-1,
             pa_maxyg(wg->wf)-1, 20, 20);
    if (wg->enb) pa_fcolor(wg->wf, pa_black);
    else pa_fcolorg(wg->wf, INT_MAX-INT_MAX/4, INT_MAX-INT_MAX/4, INT_MAX-INT_MAX/4);
    pa_cursorg(wg->wf, ENDLEDSPC, pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2);
    /* check cursor in box */
    if (wg->tleft > strlen(wg->face)) wg->tleft = 0;
    cl = ENDLEDSPC+pa_chrpos(wg->wf, wg->face, wg->curs)-
         pa_chrpos(wg->wf, wg->face, wg->tleft);
    while (cl < ENDLEDSPC && wg->tleft > 0) {

        /* cursor out of field left */
        wg->tleft--; /* back up left margin */
        /* recalculate */
        cl = ENDLEDSPC+pa_chrpos(wg->wf, wg->face, wg->curs)-
             pa_chrpos(wg->wf, wg->face, wg->tleft);

    }
    while (cl > pa_maxxg(wg->wf)-ENDLEDSPC && wg->tleft < strlen(wg->face)) {

        /* cursor out of field right */
        wg->tleft++; /* advance left margin */
        /* recalculate */
        cl = ENDLEDSPC+pa_chrpos(wg->wf, wg->face, wg->curs)-
             pa_chrpos(wg->wf, wg->face, wg->tleft);

    }
    /* display only characters that completely fit the field */
    s = &wg->face[wg->tleft]; /* index displayable string */
    while (*s && pa_curxg(wg->wf)+pa_chrpos(wg->wf, s, 1) <
                 pa_maxxg(wg->wf)-ENDLEDSPC)
        fputc(*s++, wg->wf);
    if (wg->focus) { /* if in focus, draw the cursor */

        /* find x location of cursor */
        x = ENDLEDSPC+pa_chrpos(wg->wf, wg->face, wg->curs)-
            pa_chrpos(wg->wf, wg->face, wg->tleft);
        if (wg->ins) { /* in overwrite mode */

            pa_reverse(wg->wf, TRUE); /* set reverse mode */
            pa_bover(wg->wf); /* paint background */
            /* index cursor character */
            pa_cursorg(wg->wf, x, pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2);
            /* if off the end of string, use space to reverse */
            if (wg->curs >= strlen(wg->face)) fputc(' ', wg->wf);
            else fputc(wg->face[wg->curs], wg->wf);
            pa_reverse(wg->wf, FALSE); /* reset reverse mode */
            pa_binvis(wg->wf); /* remove background */

        } else { /* in insert mode */

            pa_linewidth(wg->wf, 2); /* set line size */
            pa_line(wg->wf, x, pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2,
                            x, pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2+
                            pa_chrsizy(wg->wf));

        }

    }

}

static void editbox_event(pa_evtrec* ev, wigptr wg)

{

    char*     s;    /* temp string */
    int       l;    /* length */
    int       span; /* span between characters */
    int       off;  /* offset from last character */
    pa_evtrec er;   /* outbound button event */
    int       i;

    switch (ev->etype) {

        case pa_etredraw: /* redraw window */
            editbox_draw(wg); /* redraw the window */
            break;
        case pa_etchar: /* enter character */
            l = strlen(wg->face); /* get length of existing face string */
            if (!wg->ins || wg->curs >= l) { /* insert mode or end */

                s = malloc(l+1+1); /* get new face string */
                strcpy(s, wg->face); /* copy old string into place */
                free(wg->face); /* release previous string */
                /* move characters after cursor up */
                for (i = l; i >= wg->curs; i--) s[i+1] = s[i];
                wg->face = s; /* place new string */

            }
            wg->face[wg->curs] = ev->echar; /* place new character */
            wg->curs++; /* position after character inserted */
            editbox_draw(wg); /* redraw the window */
            break;

        case pa_etfocus: /* gain focus */
            wg->focus = 1; /* in focus */
            editbox_draw(wg); /* redraw */
            break;

        case pa_etnofocus: /* lose focus */
            wg->focus = 0; /* out of focus */
            editbox_draw(wg); /* redraw */
            break;

        case pa_etright: /* right character */
            /* not extreme right, go right */
            if (wg->curs < strlen(wg->face)) {

                wg->curs++;
                editbox_draw(wg); /* redraw */

            }
            break;

        case pa_etleft: /* left character */
            /* not extreme left, go left */
            if (wg->curs > 0) {

                wg->curs--;
                editbox_draw(wg); /* redraw */

            }
            break;

        case pa_etdelcb: /* delete character backward */
            /* not extreme left, delete left */
            if (wg->curs > 0) {

                l = strlen(wg->face); /* get length of existing face string */
                /* back up right characters past cursor */
                for (i = wg->curs-1; i < l; i++) wg->face[i] = wg->face[i+1];
                wg->curs--;
                editbox_draw(wg); /* redraw */

            }
            break;

        case pa_etdelcf: /* delete character forward */
            /* not extreme right, go right */
            if (wg->curs < strlen(wg->face)) {

                l = strlen(wg->face); /* get length of existing face string */
                /* back up right characters past cursor */
                for (i = wg->curs; i < l; i++) wg->face[i] = wg->face[i+1];
                editbox_draw(wg); /* redraw */

            }
            break;

        case pa_etmoumovg: /* mouse moved */
            /* track position */
            wg->mpx = ev->moupxg; /* set present position */
            wg->mpy = ev->moupyg;
            break;

        case pa_etmouba: /* mouse click */
            if (ev->amoubn == 1) {

                /* mouse click, select character it indexes */
                l = strlen(wg->face); /* get length of existing face string */
                i = 0;
                /* find first character beyond click */
                while (ENDLEDSPC+pa_chrpos(wg->wf, wg->face, i) < wg->mpx && i < l) i++;
                if (i) {

                    /* find span between last and next characters */
                    span = pa_chrpos(wg->wf, wg->face, i)-
                           pa_chrpos(wg->wf, wg->face, i-1);
                    /* find offset last to mouse click */
                    off = wg->mpx-(ENDLEDSPC+pa_chrpos(wg->wf, wg->face, i-1));
                    /* if mouse click is closer to last, index last */
                    if (off < span/2) i--;

                }
                wg->curs = i; /* set final position */
                editbox_draw(wg); /* redraw */

            }
            break;

        case pa_etenter: /* signal entry done */
            /* send event back to parent window */
            er.etype = pa_etedtbox; /* set button event */
            er.edtbid = wg->id; /* set id */
            pa_sendevent(wg->parent, &er); /* send the event to the parent */
            break;

        case pa_ethomel: /* beginning of line */
            wg->curs = 0;
            editbox_draw(wg); /* redraw */
            break;

        case pa_etendl: /* end of line */
            wg->curs = strlen(wg->face);
            editbox_draw(wg); /* redraw */
            break;

        case pa_etinsertt: /* toggle insert mode */
            wg->ins = !wg->ins;
            editbox_draw(wg); /* redraw */
            break;

        case pa_etdell: /* delete whole line */
            wg->curs = 0;
            wg->face[0] = 0;
            editbox_draw(wg); /* redraw */
            break;

        case pa_etleftw: /* left word */
            /* back over any spaces */
            while (wg->curs > 0 && wg->face[wg->curs-1] == ' ') wg->curs--;
            /* now back over any non-space */
            while (wg->curs > 0 && wg->face[wg->curs-1] != ' ') wg->curs--;
            editbox_draw(wg); /* redraw */
            break;

        case pa_etrightw: /* right word */
            l = strlen(wg->face); /* get string length */
            /* advance over any non-space */
            while (wg->curs < l && wg->face[wg->curs] != ' ') wg->curs++;
            /* advance over any spaces */
            while (wg->curs < l && wg->face[wg->curs] == ' ') wg->curs++;
            editbox_draw(wg); /* redraw */
            break;

    }

}

/** ****************************************************************************

Progress bar event handler

Handles the events posted to progress bars.

*******************************************************************************/

static void progressbar_draw(wigptr wg)

{

}

static void progressbar_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) progressbar_draw(wg); /* redraw the window */

}

/** ****************************************************************************

List box event handler

Handles the events posted to list boxes.

*******************************************************************************/

static void listbox_draw(wigptr wg)

{

}

static void listbox_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) listbox_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Drop box event handler

Handles the events posted to drop boxes.

*******************************************************************************/

static void dropbox_draw(wigptr wg)

{

}

static void dropbox_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) dropbox_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Drop edit box event handler

Handles the events posted to drop edit boxes.

*******************************************************************************/

static void dropeditbox_draw(wigptr wg)

{

}

static void dropeditbox_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) dropeditbox_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Horizontal slider event handler

Handles the events posted to a horizontal slider.

*******************************************************************************/

static void slidehoriz_draw(wigptr wg)

{

}

static void slidehoriz_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) slidehoriz_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Vertical slider event handler

Handles the events posted to a vertical slider.

*******************************************************************************/

static void slidevert_draw(wigptr wg)

{

}

static void slidevert_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) slidevert_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Tab bar event handler

Handles the events posted to a tab bar.

*******************************************************************************/

static void tabbar_draw(wigptr wg)

{

}

static void tabbar_event(pa_evtrec* ev, wigptr wg)

{

    if (ev->etype == pa_etredraw) tabbar_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Widget event handler

Handles the events posted to widgets.

*******************************************************************************/

static void widget_event(pa_evtrec* ev)

{

    wigptr wg; /* pointer to widget */

    /* if not our window, send it on */
    wg = xltwig[ev->winid+MAXFIL]; /* get possible widget entry */
    if (!wg) widget_event_old(ev);
    else switch (wg->typ) { /* handle according to type */

        case wtbutton:       button_event(ev, wg); break;
        case wtcheckbox:     checkbox_event(ev, wg); break;
        case wtradiobutton:  radiobutton_event(ev, wg); break;
        case wtgroup:        group_event(ev, wg); break;
        case wtbackground:   background_event(ev, wg); break;
        case wtscrollvert:   scrollvert_event(ev, wg); break;
        case wtscrollhoriz:  scrollhoriz_event(ev, wg); break;
        case wtnumselbox:    numselbox_event(ev, wg); break;
        case wteditbox:      editbox_event(ev, wg); break;
        case wtprogressbar:  progressbar_event(ev, wg); break;
        case wtlistbox:      listbox_event(ev, wg); break;
        case wtdropbox:      dropbox_event(ev, wg); break;
        case wtdropeditbox:  dropeditbox_event(ev, wg); break;
        case wtslidehoriz:   slidehoriz_event(ev, wg); break;
        case wtslidevert:    slidevert_event(ev, wg); break;
        case wttabbar:       tabbar_event(ev, wg); break;
        case wtalert:        break;
        case wtquerycolor:   break;
        case wtqueryopen:    break;
        case wtquerysave:    break;
        case wtqueryfind:    break;
        case wtqueryfindrep: break;
        case wtqueryfont:    break;

    }

}

/*******************************************************************************

Create widget

Creates a widget within the given window, within the specified bounding box,
and using the face string and type, and the given id. The string may or may not
be used.

*******************************************************************************/

static void widget(FILE* f, int x1, int y1, int x2, int y2, char* s, int id,
                   wigtyp typ, wigptr* wp)

{

    int fn; /* logical file name */

    if (id <= 0 || id > MAXWIG) error("Invalid widget id");
    makfil(f); /* ensure there is a file entry and validate */
    fn = fileno(f); /* get the file index */
    if (opnfil[fn]->widgets[id]) error("Widget by id already in use");
    opnfil[fn]->widgets[id] = getwig(); /* get widget entry */
    *wp = opnfil[fn]->widgets[id]; /* index that */

    (*wp)->face = str(s); /* place face */
    (*wp)->wid = pa_getwid(); /* allocate a buried wid */
    pa_openwin(&stdin, &(*wp)->wf, f, (*wp)->wid); /* open widget window */
    (*wp)->parent = f; /* save parent file */
    xltwig[(*wp)->wid+MAXFIL] = *wp; /* set the tracking entry for the window */
    (*wp)->id = id; /* set button widget id */
    pa_buffer((*wp)->wf, FALSE); /* turn off buffering */
    pa_auto((*wp)->wf, FALSE); /* turn off auto */
    pa_curvis((*wp)->wf, FALSE); /* turn off cursor */
    pa_font((*wp)->wf, PA_FONT_SIGN); /* set sign font */
    pa_frame((*wp)->wf, FALSE); /* turn off frame */
    pa_setposg((*wp)->wf, x1, y1); /* place at position */
    pa_setsizg((*wp)->wf, x2-x1, y2-y1); /* set size */
    pa_binvis((*wp)->wf); /* no background write */
    (*wp)->typ = typ; /* place type */
    (*wp)->pressed = FALSE; /* set not pressed */
    (*wp)->select = FALSE; /* set not selected */
    (*wp)->enb = TRUE; /* set is enabled */
    (*wp)->sclsiz = INT_MAX/10; /* set default size scrollbar */
    (*wp)->sclpos = 0; /* set scrollbar position top/left */
    (*wp)->curs = 0; /* set text cursor */
    (*wp)->tleft = 0; /* set text left side in edit box */
    (*wp)->focus = 0; /* set not focused */
    (*wp)->ins = 0; /* set insert mode */

}

/** ****************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

void pa_killwidget(FILE* f, int id)

{

    wigptr wp; /* widget entry pointer */
    int    fn; /* logical file name */

    wp = fndwig(f, id); /* index the widget */
    fclose(wp->wf); /* close the window file */
    fn = fileno(f); /* get the logical file number */
    opnfil[fn]->widgets[id] = NULL; /* clear widget slot  */
    putwig(wp); /* release widget data */

}

/** ****************************************************************************

Select/deselect widget

Selects or deselects a widget.

*******************************************************************************/

void pa_selectwidget(FILE* f, int id, int e)

{

    wigptr    wp;  /* widget entry pointer */
    int       chg; /* widget state changes */

    wp = fndwig(f, id); /* index the widget */
    /* check this widget is selectable */
    if (wp->typ != wtcheckbox && wp->typ != wtradiobutton)
        error("Widget is not selectable");
    chg = wp->select != !!e; /* check select state changes */
    wp->select = !!e; /* set select state */
    /* if the select changes, refresh the checkbox */
    if (chg) widget_redraw(wp->wf); /* send redraw to widget */

}

/** ****************************************************************************

Enable/disable widget

Enables or disables a widget.

*******************************************************************************/

void pa_enablewidget(FILE* f, int id, int e)

{

    wigptr    wp;  /* widget entry pointer */
    int       chg; /* widget state changes */

    e = !!e; /* clean the enable value */
    wp = fndwig(f, id); /* index the widget */
    /* check this widget can be enabled/disabled */
    if (wp->typ != wtbutton && wp->typ != wtcheckbox &&
        wp->typ != wtradiobutton)
        error("Widget is not disablable");
    chg = wp->enb != e; /* check enable state changes */
    wp->enb = e; /* set enable state */
    /* if the select changes, refresh the checkbox */
    if (chg) widget_redraw(wp->wf); /* send redraw to widget */

}

/** ****************************************************************************

Get widget text

Retrives the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

*******************************************************************************/

void pa_getwidgettext(FILE* f, int id, char* s, int sl)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* check this widget can have face text read */
    if (wp->typ != wteditbox && wp->typ != wtdropeditbox)
        error("Widget content cannot be read");
    /* check face text too large for buffer */
    if (strlen(wp->face) >= sl) error("Face text too large for result");
    strcpy(s, wp->face); /* copy face text to result */

}

/** ****************************************************************************

put edit box text

Places text into an edit box.

*******************************************************************************/

void pa_putwidgettext(FILE* f, int id, char* s)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* check this widget can have face text read */
    if (wp->typ != wteditbox && wp->typ != wtdropeditbox)
        error("Widget contents cannot be written");
    free(wp->face); /* dispose of previous face string */
    wp->face = str(s); /* place new face */
    widget_redraw(wp->wf); /* send redraw to widget */

}

/** ****************************************************************************

Resize widget

Changes the size of a widget.

*******************************************************************************/

void pa_sizwidgetg(FILE* f, int id, int x, int y)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    pa_setsizg(wp->wf, x, y); /* set size */

}

void pa_sizwidget(FILE* f, int id, int x, int y)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* form graphical from character size */
    x = (x-1)*pa_chrsizx(f)+1;
    y = (y-1)*pa_chrsizy(f)+1;
    pa_setsizg(wp->wf, x, y); /* set size */

}

/** ****************************************************************************

Reposition widget

Changes the parent position of a widget.

*******************************************************************************/

void pa_poswidgetg(FILE* f, int id, int x, int y)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    pa_setposg(wp->wf, x, y); /* set size */

}

void pa_poswidget(FILE* f, int id, int x, int y)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* form graphical from character coordinates */
    x = (x-1)*pa_chrsizx(f)+1;
    y = (y-1)*pa_chrsizy(f)+1;
    pa_setposg(wp->wf, x, y); /* set size */

}

/** ****************************************************************************

Place widget to back of Z order

*******************************************************************************/

void pa_backwidget(FILE* f, int id)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    pa_back(wp->wf); /* place to back */

}

/** ****************************************************************************

Place widget to back of Z order

*******************************************************************************/

void pa_frontwidget(FILE* f, int id)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    pa_front(wp->wf); /* place to front */

}

/** ****************************************************************************

Find minimum/standard button size

Finds the minimum size for a button. Given the face string, the minimum/ideal
size of a button is calculated and returned.

Note the spacing is copied from gnome defaults.

*******************************************************************************/

void pa_buttonsizg(FILE* f, char* s, int* w, int* h)

{

    *h = pa_chrsizy(win0)*2; /* set height */
    *w = pa_strsiz(win0, s)+pa_chrsizy(win0)*2;

}

void pa_buttonsiz(FILE* f, char* s, int* w, int* h)

{

    pa_buttonsizg(f, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / pa_chrsizx(f)+1;
    *h = (*h-1) / pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create button

Creates a standard button within the specified rectangle, on the given window.

*******************************************************************************/

void pa_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, s, id, wtbutton, &wp);
    pa_linewidth(wp->wf, 3); /* thicker lines */

}

void pa_button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_buttong(f, x1, y1, x2, y2, s, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard checkbox size

Finds the minimum size for a checkbox. Given the face string, the minimum size
of a checkbox is calculated and returned.

*******************************************************************************/

void pa_checkboxsizg(FILE* f, char* s, int* w, int* h)

{

    *h = pa_chrsizy(win0)*2; /* set height */
    *w = pa_chrsizy(win0)+pa_chrsizy(win0)/2+pa_strsiz(win0, s)+
         pa_chrsizy(win0)/2;

}

void pa_checkboxsiz(FILE* f, char* s,  int* w, int* h)

{

    pa_checkboxsizg(f, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / pa_chrsizx(f)+1;
    *h = (*h-1) / pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create checkbox

Creates a standard checkbox within the specified rectangle, on the given
window.

*******************************************************************************/

void pa_checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, s, id, wtcheckbox, &wp);

}

void pa_checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_checkboxg(f, x1, y1, x2, y2, s, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard radio button size

Finds the minimum size for a radio button. Given the face string, the minimum
size of a radio button is calculated and returned.

*******************************************************************************/

void pa_radiobuttonsizg(FILE* f, char* s, int* w, int* h)

{

    *h = pa_chrsizy(win0)*2; /* set height */
    *w = pa_chrsizy(win0)+pa_chrsizy(win0)/2+pa_strsiz(win0, s)+
         pa_chrsizy(win0)/2;

}

void pa_radiobuttonsiz(FILE* f, char* s, int* w, int* h)

{

    pa_radiobuttonsizg(f, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / pa_chrsizx(f)+1;
    *h = (*h-1) / pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create radio button

Creates a standard radio button within the specified rectangle, on the given
window.

*******************************************************************************/

void pa_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, s, id, wtradiobutton, &wp);

}

void pa_radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_radiobuttong(f, x1, y1, x2, y2, s, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard group size

Finds the minimum size for a group. Given the face string, the minimum
size of a group is calculated and returned.

*******************************************************************************/

void pa_groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h,
               int* ox, int* oy)

{

    *h = pa_chrsizy(win0)+ch+5; /* set height */
    *w = pa_strsiz(win0, s);
    /* if string is less than client, width is client */
    if (*w < cw+7) *w = cw+7;
    *ox = 4; /* set offset to client */
    *oy = pa_chrsizy(win0);

}

void pa_groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h,
              int* ox, int* oy)

{

    /* convert client sizes to graphical */
    cw = cw*pa_chrsizx(f);
    ch = ch*pa_chrsizy(f);
    pa_groupsizg(f, s, cw, ch, w, h, ox, oy); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/pa_chrsizx(f)+1;
    *h = (*h-1)/pa_chrsizy(f)+1;
    *ox = (*ox-1)/pa_chrsizx(f)+1;
    *oy = (*oy-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create group box

Creates a group box, which is really just a decorative feature that gererates
no messages. It is used as a background for other widgets.

*******************************************************************************/

void pa_groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, s, id, wtgroup, &wp);

}

void pa_group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_groupg(f, x1, y1, x2, y2, s, id); /* create button graphical */

}

/** ****************************************************************************

Create background box

Creates a background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

*******************************************************************************/

void pa_backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtbackground, &wp);

}

void pa_background(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_backgroundg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard vertical scrollbar size

Finds the minimum size for a vertical scrollbar. The minimum size of a vertical
scrollbar is calculated and returned.

*******************************************************************************/

void pa_scrollvertsizg(FILE* f, int* w, int* h)

{

    *w = 20;
    *h = 40;

}

void pa_scrollvertsiz(FILE* f, int* w, int* h)

{

    pa_scrollvertsizg(f, w, h); /* get in graphics terms */
    /* change graphical size to character */
    *w = (*w-1)/pa_chrsizx(f)+1;
    *h = (*h-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create vertical scrollbar

Creates a vertical scrollbar.

*******************************************************************************/

void pa_scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtscrollvert, &wp);

}

void pa_scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_scrollvertg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard horizontal scrollbar size

Finds the minimum size for a horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated and returned.

*******************************************************************************/

void pa_scrollhorizsizg(FILE* f, int* w, int* h)

{

    *w = 40;
    *h = 20;

}

void pa_scrollhorizsiz(FILE* f, int* w, int* h)

{

    pa_scrollhorizsizg(f, w, h); /* get in graphics terms */
    /* change graphical size to character */
    *w = (*w-1)/pa_chrsizx(f)+1;
    *h = (*h-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create horizontal scrollbar

Creates a horizontal scrollbar.

*******************************************************************************/

void pa_scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtscrollhoriz, &wp);

}

void pa_scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_scrollhorizg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

*******************************************************************************/

void pa_scrollpos(FILE* f, int id, int r)

{

    wigptr wp;  /* widget entry pointer */

    if (r < 0) error("Invalid scroll bar postition");
    wp = fndwig(f, id); /* index the widget */
    /* check this widget is a scrollbar */
    if (wp->typ != wtscrollvert && wp->typ != wtscrollhoriz)
        error("Widget not a scroll bar");
    wp->sclpos = r; /* set scroll bar postition */
    widget_redraw(wp->wf); /* send redraw to widget */

}

/** ****************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

*******************************************************************************/

void pa_scrollsiz(FILE* f, int id, int r)

{

    wigptr wp;  /* widget entry pointer */

    if (r < 0) error("Invalid scroll bar size");
    wp = fndwig(f, id); /* index the widget */
    /* check this widget is a scrollbar */
    if (wp->typ != wtscrollvert && wp->typ != wtscrollhoriz)
        error("Widget not a scroll bar");
    wp->sclsiz = r; /* set scroll bar size */
    widget_redraw(wp->wf); /* send redraw to widget */

}

/** ****************************************************************************

Find minimum/standard number select box size

Finds the minimum size for a number select box. The minimum size of a number
select box is calculated and returned.

*******************************************************************************/

void pa_numselboxsizg(FILE* f, int l, int u, int* w, int* h)

{

}

void pa_numselboxsiz(FILE* f, int l, int u, int* w, int* h)

{

    pa_numselboxsizg(f, l, u, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / pa_chrsizx(f)+1;
    *h = (*h-1) / pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create number selector

Creates an up/down control for numeric selection.

*******************************************************************************/

void pa_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtnumselbox, &wp);

}

void pa_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_numselboxg(f, x1, y1, x2, y2, l, u, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void pa_editboxsizg(FILE* f, char* s, int* w, int* h)

{

    *h = pa_chrsizy(win0)*1.5; /* set height */
    *w = pa_strsiz(win0, s);

}

void pa_editboxsiz(FILE* f, char* s, int* w, int* h)

{

    pa_editboxsizg(f, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / pa_chrsizx(f)+1;
    *h = (*h-1) / pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create edit box

Creates single line edit box

*******************************************************************************/

void pa_editboxg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wteditbox, &wp);
    pa_curvis(wp->wf, FALSE); /* turn on cursor */

}

void pa_editbox(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_editboxg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void pa_progbarsizg(FILE* f, int* w, int* h)

{

}

void pa_progbarsiz(FILE* f, int* w, int* h)

{

    pa_progbarsizg(f, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/pa_chrsizx(f)+1;
    *h = (*h-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create progress bar

Creates a progress bar.

*******************************************************************************/

void pa_progbarg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtprogressbar, &wp);

}

void pa_progbar(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_progbarg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Set progress bar position

Sets the position of a progress bar, from 0 to maxint.

*******************************************************************************/

void pa_progbarpos(FILE* f, int id, int pos)

{

}

/** ****************************************************************************

Find minimum/standard list box size

Finds the minimum size for an list box. Given a string list, the minimum size
of an list box is calculated and returned.

Windows listboxes pretty much ignore the size given. If you allocate more space
than needed, it will only put blank lines below if enough space for an entire
line is present. If the size does not contain exactly enough to display the
whole line list, the box will colapse to a single line with an up/down
control. The only thing that is garanteed is that the box will fit within the
specified rectangle, one way or another.

*******************************************************************************/

void pa_listboxsizg(FILE* f, pa_strptr sp, int* w, int* h)

{

}

void pa_listboxsiz(FILE* f, pa_strptr sp, int* w, int* h)

{

    pa_listboxsizg(f, sp, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/pa_chrsizx(f)+1;
    *h = (*h-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create list box

Creates a list box. Fills it with the string list provided.

*******************************************************************************/

void pa_listboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtlistbox, &wp);

}

void pa_listbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_listboxg(f, x1, y1, x2, y2, sp, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard dropbox size

Finds the minimum size for a dropbox. Given the face string, the minimum size of
a dropbox is calculated and returned, for both the "open" and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void pa_dropboxsizg(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

}

void pa_dropboxsiz(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    pa_dropboxsizg(f, sp, cw, ch, ow, oh); /* get size */
    /* change graphical size to character */
    *cw = (*cw-1)/pa_chrsizx(f)+1;
    *ch = (*ch-1)/pa_chrsizy(f)+1;
    *ow = (*ow-1)/pa_chrsizx(f)+1;
    *oh = (*oh-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create dropdown box

Creates a dropdown box. Fills it with the string list provided.

*******************************************************************************/

void pa_dropboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtdropbox, &wp);

}

void pa_dropbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_dropboxg(f, x1, y1, x2, y2, sp, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard drop edit box size

Finds the minimum size for a drop edit box. Given the face string, the minimum
size of a drop edit box is calculated and returned, for both the "open" and
"closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void pa_dropeditboxsizg(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

}

void pa_dropeditboxsiz(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    pa_dropeditboxsizg(f, sp, cw, ch, ow, oh); /* get size */
    /* change graphical size to character */
    *cw = (*cw-1)/pa_chrsizx(f)+1;
    *ch = (*ch-1)/pa_chrsizy(f)+1;
    *ow = (*ow-1)/pa_chrsizx(f)+1;
    *oh = (*oh-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create dropdown edit box

Creates a dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

void pa_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtdropeditbox, &wp);

}

void pa_dropeditbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_dropeditboxg(f, x1, y1, x2, y2, sp, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard horizontal slider size

Finds the minimum size for a horizontal slider. The minimum size of a horizontal
slider is calculated and returned.

*******************************************************************************/

void pa_slidehorizsizg(FILE* f, int * w, int* h)

{

}

void pa_slidehorizsiz(FILE* f, int* w, int* h)

{

    pa_slidehorizsizg(f, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/pa_chrsizx(f)+1;
    *h = (*h-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create horizontal slider

Creates a horizontal slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void pa_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtslidehoriz, &wp);

}

void pa_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_slidehorizg(f, x1, y1, x2, y2, mark, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard vertical slider size

Finds the minimum size for a vertical slider. The minimum size of a vertical
slider is calculated and returned.

*******************************************************************************/

void pa_slidevertsizg(FILE* f, int* w, int* h)

{

}

void pa_slidevertsiz(FILE* f, int* w, int* h)

{

    pa_slidevertsizg(f, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/pa_chrsizx(f)+1;
    *h = (*h-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create vertical slider

Creates a vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void pa_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wtslidevert, &wp);

}

void pa_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_slidevertg(f, x1, y1, x2, y2, mark, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard tab bar size

Finds the minimum size for a tab bar. The minimum size of a tab bar is
calculated and returned.

*******************************************************************************/

void pa_tabbarsizg(FILE* f, pa_tabori tor, int cw, int ch, int* w, int* h,
                int* ox, int* oy)

{

}

void pa_tabbarsiz(FILE* f, pa_tabori tor, int cw, int ch, int * w, int* h,
               int* ox, int* oy)

{

    int gw, gh, gox, goy;

    /* convert client sizes to graphical */
    cw = cw*pa_chrsizx(f);
    ch = ch*pa_chrsizy(f);
    pa_tabbarsizg(f, tor, cw, ch, &gw, &gh, &gox, &goy); /* get size */
    /* change graphical size to character */
    *w = (gw-1) / pa_chrsizx(f)+1;
    *h = (gh-1) / pa_chrsizy(f)+1;
    *ox = (gox-1) / pa_chrsizx(f)+1;
    *oy = (goy-1) / pa_chrsizy(f)+1;

}

/** ****************************************************************************

Find client from tabbar size

Given a tabbar size and orientation, this routine gives the client size and
offset. This is used where the tabbar size is fixed, but the client area is
flexible.

*******************************************************************************/

void pa_tabbarclientg(FILE* f, pa_tabori tor, int w, int h, int* cw, int* ch,
                   int* ox, int* oy)

{

}

void pa_tabbarclient(FILE* f, pa_tabori tor, int w, int h, int* cw, int* ch,
                  int* ox, int* oy)

{

    int gw, gh, gox, goy;

    /* convert sizes to graphical */
    w = w*pa_chrsizx(f);
    h = h*pa_chrsizy(f);
    pa_tabbarsizg(f, tor, w, h, &gw, &gh, &gox, &goy); /* get size */
    /* change graphical size to character */
    *cw = (gw-1)/pa_chrsizx(f)+1;
    *ch = (gh-1)/pa_chrsizy(f)+1;
    *ox = (gox-1)/pa_chrsizx(f)+1;
    *oy = (goy-1)/pa_chrsizy(f)+1;

}

/** ****************************************************************************

Create tab bar

Creates a tab bar with the given orientation.

*******************************************************************************/

void pa_tabbarg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
             pa_tabori tor, int id)

{

    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, "", id, wttabbar, &wp);

}

void pa_tabbar(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
            pa_tabori tor, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*pa_chrsizx(f)+1;
    y1 = (y1-1)*pa_chrsizy(f)+1;
    x2 = (x2)*pa_chrsizx(f);
    y2 = (y2)*pa_chrsizy(f);
    pa_tabbarg(f, x1, y1, x2, y2, sp, tor, id); /* create button graphical */

}

/** ****************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

*******************************************************************************/

void pa_tabsel(FILE* f, int id, int tn)

{

}

/** ****************************************************************************

Output message dialog

Outputs a message dialog with the given title and message strings.

*******************************************************************************/

void pa_alert(char* title, char* message)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, message, id, wtalert, &wp);

}

/** ****************************************************************************

Display choose color dialog

Presents the choose color dialog, then returns the resulting color.

Bug: does not take the input color as the default.

*******************************************************************************/

void pa_querycolor(int* r, int* g, int* b)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, "", id, wtquerycolor, &wp);

}

/** ****************************************************************************

Display choose file dialog for open

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

void pa_queryopen(char* s, int sl)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, s, id, wtqueryopen, &wp);

}

/** ****************************************************************************

Display choose file dialog for save

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

void pa_querysave(char* s, int sl)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, s, id, wtquerysave, &wp);

}

/** ****************************************************************************

Display choose find text dialog

Presents the choose find text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: should return null string on cancel. Unlike other dialogs, windows
provides no indication of if the cancel button was pushed. To do this, we
would need to hook (or subclass) the find dialog.

After note: tried hooking the window. The issue is that the cancel button is
just a simple button that gets pressed. Trying to rely on the button id
sounds very system dependent, since that could change. One method might be
to retrive the button text, but this is still fairly system dependent. We
table this issue until later.

*******************************************************************************/

void pa_queryfind(char* s, int sl, pa_qfnopts* opt)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, s, id, wtqueryfind, &wp);

}

/** ****************************************************************************

Display choose replace text dialog

Presents the choose replace text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: See comment, queryfind.

*******************************************************************************/

void pa_queryfindrep(char* s, int sl, char* r, int rl, pa_qfropts* opt)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, s, id, wtqueryfindrep, &wp);

}

/** ****************************************************************************

Display choose font dialog

Presents the choose font dialog, then returns the resulting logical font
number, size, foreground color, background color, and effects (in a special
effects set for this routine).

The parameters are "flow through", meaning that they should be set to their
defaults before the call, and changes are made, then updated to the parameters.
During the routine, the state of the parameters given are presented to the
user as the defaults.

*******************************************************************************/

void pa_queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb,
               int* br, int* bg, int* bb, pa_qfteffects* effect)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, "", id, wtqueryfont, &wp);

}

/** ****************************************************************************

Widgets startup

*******************************************************************************/

static void pa_init_widgets(int argc, char *argv[]) __attribute__((constructor (102)));
static void pa_init_widgets(int argc, char *argv[])

{

    int fn; /* file number */
    int wid; /* window id */

    /* override the event handler */
    pa_eventsover(widget_event, &widget_event_old);

    wigfre = NULL; /* clear widget free list */

    /* clear open files table */
    for (fn = 0; fn < MAXFIL; fn++) opnfil[fn] = NULL;

    /* clear window equivalence table */
    for (fn = 0; fn < MAXFIL*2+1; fn++)
        /* clear window logical number translator table */
        xltwig[fn] = NULL; /* set no widget entry */

    /* open "window 0" dummy window */
    pa_openwin(&stdin, &win0, NULL, pa_getwid()); /* open window */
    pa_buffer(win0, FALSE); /* turn off buffering */
    pa_auto(win0, FALSE); /* turn off auto (for font change) */
    pa_font(win0, PA_FONT_SIGN); /* set sign font */
    pa_frame(win0, FALSE); /* turn off frame */

    /* fill out the theme table defaults */
    themetable[th_buttbackpressed]  = TD_BUTTBACKPRESSED;
    themetable[th_buttback]         = TD_BUTTBACK;
    themetable[th_buttout]          = TD_BUTTOUT;
    themetable[th_text]             = TD_TEXT;
    themetable[th_textdis]          = TD_TEXTDIS;
    themetable[th_focus]            = TD_FOCUS;
    themetable[th_chkrad]           = TD_CHKRAD;
    themetable[th_chkradout]        = TD_CHKRADOUT;
    themetable[th_scrollback]       = TD_SCROLLBACK;
    themetable[th_scrollbar]        = TD_SCROLLBAR;
    themetable[th_scrollbarpressed] = TD_SCROLLBARPRESSED;

}

/** ****************************************************************************

Widgets shutdown

*******************************************************************************/

static void pa_deinit_widgets(void) __attribute__((destructor (102)));
static void pa_deinit_widgets()

{

    /* should shut down all widgets */

}
