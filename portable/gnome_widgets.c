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

/* widget type */
typedef enum  {
    wtbutton, wtcheckbox, wtradiobutton, wtgroup, wtbackground,
    wtscrollvert, wtscrollhoriz, wtnumselbox, wteditbox,
    wtprogressbar, wtlistbox, wtdropbox, wtdropeditbox,
    wtslidehoriz, wtslidevert, wttabbar
} wigtyp;

/* Widget control structure */
typedef struct wigrec* wigptr;
typedef struct wigrec {

    wigptr next;    /* next entry in list */
    wigtyp typ;     /* type of widget */
    int    pressed; /* in the pressed state */
    FILE*  wf;      /* output file for the widget window */
    char*  title;   /* title text */
    FILE*  parent;  /* parent window */
    FILE*  evtfil;  /* file to post menu events to */
    int    id;      /* id number */
    int    wid;     /* widget window id */

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

static pa_pevthan widget_event_old;   /* previous event vector save */
static wigptr     wigfre;             /* free widget entry list */
static filptr     opnfil[MAXFIL];     /* open files table */
static wigptr     xltwig[MAXFIL*2+1]; /* widget entry equivalence table */

/** ****************************************************************************

Process error

*******************************************************************************/

static void error(char* es)

{

    fprintf(stderr, "Error: widgets: %s\n", es);
    fflush(stderr);

    exit(1);

}

/** ****************************************************************************

Get file entry

Allocates and initalizes a new file entry. File entries are left in the opnfil
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

/** ****************************************************************************

Widget event handler

Handles the events posted to widgets.

*******************************************************************************/

static void widget_event(pa_evtrec* ev)

{

    pa_evtrec er; /* outbound button event */
    wigptr    wg; /* pointer to widget */

    /* if not our window, send it on */
    wg = xltwig[ev->winid+MAXFIL]; /* get possible widget entry */
    if (!wg) widget_event_old(ev);
    else { /* handle it here */

        if (ev->etype == pa_etredraw) { /* redraw the window */

            /* color the background */
            pa_fcolor(wg->wf, pa_white);
            pa_frect(wg->wf, 1, 1, pa_maxxg(wg->wf),
                      pa_maxyg(wg->wf));
            /* outline */
            pa_fcolorg(wg->wf, INT_MAX/4, INT_MAX/4, INT_MAX/4);
            pa_rrect(wg->wf, 2, 2, pa_maxxg(wg->wf)-1,
                     pa_maxyg(wg->wf)-1, 20, 20);
            if (wg->pressed) pa_fcolor(wg->wf, pa_red);
            else pa_fcolor(wg->wf, pa_black);
            pa_cursorg(wg->wf,
                       pa_maxxg(wg->wf)/2-pa_strsiz(wg->wf, wg->title)/2,
                       pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2);
            fprintf(wg->wf, "%s", wg->title); /* place button title */

        } else if (ev->etype == pa_etmouba && ev->amoubn) {

            /* send event back to parent window */
            er.etype = pa_etbutton; /* set button event */
            er.butid = wg->id; /* set id */
            pa_sendevent(wg->parent, &er); /* send the event to the parent */

            /* process button press */
            wg->pressed = TRUE;
            pa_fcolor(wg->wf, pa_black);
            pa_frrect(wg->wf, 3, 3, pa_maxxg(wg->wf)-3,
                     pa_maxyg(wg->wf)-3, 20, 20);
            pa_fcolor(wg->wf, pa_white);
            pa_cursorg(wg->wf,
                       pa_maxxg(wg->wf)/2-pa_strsiz(wg->wf, wg->title)/2,
                       pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2);
            fprintf(wg->wf, "%s", wg->title); /* place button title */

        } else if (ev->etype == pa_etmoubd) {

            wg->pressed = FALSE;
            pa_fcolor(wg->wf, pa_white);
            pa_frrect(wg->wf, 3, 3, pa_maxxg(wg->wf)-3,
                     pa_maxyg(wg->wf)-3, 20, 20);
            pa_fcolor(wg->wf, pa_black);
            pa_cursorg(wg->wf,
                       pa_maxxg(wg->wf)/2-pa_strsiz(wg->wf, wg->title)/2,
                       pa_maxyg(wg->wf)/2-pa_chrsizy(wg->wf)/2);
            fprintf(wg->wf, "%s", wg->title); /* place button title */

        }

    }

}

/*******************************************************************************

Create widget

Creates a widget within the given window, within the specified bounding box,
and using the face string and type, and the given id. The string may or may not
be used.

Widgets use the subthread to buffer them. There were various problems from
trying to start them on the main window.

*******************************************************************************/

static void widget(FILE* f, int x1, int y1, int x2, int y2, char* s, int id,
                   wigtyp typ, wigptr* wp)

{

    int fn; /* logical file name */

    if (id <=0 || id > MAXWIG) error("Invalid widget id");
    makfil(f); /* ensure there is a file entry and validate */
    fn = fileno(f); /* get the file index */
    if (opnfil[fn]->widgets[id]) error("Widget by id already in use");
    opnfil[fn]->widgets[id] = getwig(); /* get widget entry */
    *wp = opnfil[fn]->widgets[id]; /* index that */

    (*wp)->title = s; /* place title */
    (*wp)->wid = pa_getwid(); /* allocate a buried wid */
    pa_openwin(&stdin, &(*wp)->wf, f, (*wp)->wid); /* open widget window */
    (*wp)->parent = f; /* save parent file */
    xltwig[(*wp)->wid+MAXFIL] = *wp; /* set the tracking entry for the window */
    (*wp)->id = id; /* set button widget id */
    pa_buffer((*wp)->wf, FALSE); /* turn off buffering */
    pa_auto((*wp)->wf, FALSE); /* turn off auto */
    pa_curvis((*wp)->wf, FALSE); /* turn off cursor */
    pa_font((*wp)->wf, PA_FONT_SIGN); /* set sign font */
    pa_bold((*wp)->wf, TRUE); /* set bold font */
    pa_setposg((*wp)->wf, x1, y1); /* place at position */
    pa_setsizg((*wp)->wf, x2-x1, y2-y1); /* set size */
    pa_frame((*wp)->wf, FALSE); /* turn off frame */
    pa_binvis((*wp)->wf); /* no background write */
    (*wp)->typ = typ; /* place type */

}

/** ****************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

void pa_killwidget(FILE* f, int id)

{

}

/** ****************************************************************************

Select/deselect widget

Selects or deselects a widget.

*******************************************************************************/

void pa_selectwidget(FILE* f, int id, int e)

{

}

/** ****************************************************************************

Enable/disable widget

Enables or disables a widget.

*******************************************************************************/

void pa_enablewidget(FILE* f, int id, int e)

{

}

/** ****************************************************************************

Get widget text

Retrives the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

*******************************************************************************/

void pa_getwidgettext(FILE* f, int id, char* s, int sl)

{

}

/** ****************************************************************************

put edit box text

Places text into an edit box.

*******************************************************************************/

void pa_putwidgettext(FILE* f, int id, char* s)

{

}

/** ****************************************************************************

Resize widget

Changes the size of a widget.

*******************************************************************************/

void pa_sizwidgetg(FILE* f, int id, int x, int y)

{

}

/** ****************************************************************************

Reposition widget

Changes the parent position of a widget.

*******************************************************************************/

void pa_poswidgetg(FILE* f, int id, int x, int y)

{

}

/** ****************************************************************************

Place widget to back of Z order

*******************************************************************************/

void pa_backwidget(FILE* f, int id)

{

}

/** ****************************************************************************

Place widget to back of Z order

*******************************************************************************/

void pa_frontwidget(FILE* f, int id)

{

}

/** ****************************************************************************

Find minimum/standard button size

Finds the minimum size for a button. Given the face string, the minimum size of
a button is calculated and returned.

*******************************************************************************/

void pa_buttonsizg(FILE* f, char* s, int* w, int* h)

{

}

void pa_buttonsiz(FILE* f, char* s, int* w, int* h)

{

}

/** ****************************************************************************

Create button

Creates a standard button within the specified rectangle, on the given window.

*******************************************************************************/

void pa_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    int fn; /* file number */
    wigptr wp; /* widget entry pointer */

    widget(f, x1, y1, x2, y2, s, id, wtbutton, &wp);
    pa_linewidth(wp->wf, 3); /* thicker lines */

}

void pa_button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

/** ****************************************************************************

Find minimum/standard checkbox size

Finds the minimum size for a checkbox. Given the face string, the minimum size of
a checkbox is calculated and returned.

*******************************************************************************/

void pa_checkboxsizg(FILE* f, char* s, int* w, int* h)

{

}

void pa_checkboxsiz(FILE* f, char* s,  int* w, int* h)

{

}

/** ****************************************************************************

Create checkbox

Creates a standard checkbox within the specified rectangle, on the given
window.

*******************************************************************************/

void pa_checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

void pa_checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

/** ****************************************************************************

Find minimum/standard radio button size

Finds the minimum size for a radio button. Given the face string, the minimum
size of a radio button is calculated and returned.

*******************************************************************************/

void pa_radiobuttonsizg(FILE* f, char* s, int* w, int* h)

{

}

void pa_radiobuttonsiz(FILE* f, char* s, int* w, int* h)

{

}

/** ****************************************************************************

Create radio button

Creates a standard radio button within the specified rectangle, on the given
window.

*******************************************************************************/

void pa_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

void pa_radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

/** ****************************************************************************

Find minimum/standard group size

Finds the minimum size for a group. Given the face string, the minimum
size of a group is calculated and returned.

*******************************************************************************/

void pa_groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h,
               int* ox, int* oy)

{

}

void pa_groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h,
              int* ox, int* oy)

{

}

/** ****************************************************************************

Create group box

Creates a group box, which is really just a decorative feature that gererates
no messages. It is used as a background for other widgets.

*******************************************************************************/

void pa_groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

void pa_group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

/** ****************************************************************************

Create background box

Creates a background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

*******************************************************************************/

void pa_backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_background(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

/** ****************************************************************************

Find minimum/standard vertical scrollbar size

Finds the minimum size for a vertical scrollbar. The minimum size of a vertical
scrollbar is calculated and returned.

*******************************************************************************/

void pa_scrollvertsizg(FILE* f, int* w, int* h)

{

}

void pa_scrollvertsiz(FILE* f, int* w, int* h)

{

}

/** ****************************************************************************

Create vertical scrollbar

Creates a vertical scrollbar.

*******************************************************************************/

void pa_scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

/** ****************************************************************************

Find minimum/standard horizontal scrollbar size

Finds the minimum size for a horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated and returned.

*******************************************************************************/

void pa_scrollhorizsizg(FILE* f, int* w, int* h)

{

}

void pa_scrollhorizsiz(FILE* f, int* w, int* h)

{

}

/** ****************************************************************************

Create horizontal scrollbar

Creates a horizontal scrollbar.

*******************************************************************************/

void pa_scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

/** ****************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

*******************************************************************************/

void pa_scrollpos(FILE* f, int id, int r)

{

}

/** ****************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

*******************************************************************************/

void pa_scrollsiz(FILE* f, int id, int r)

{

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

}

/** ****************************************************************************

Create number selector

Creates an up/down control for numeric selection.

*******************************************************************************/

void pa_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

}

void pa_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

}

/** ****************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void pa_editboxsizg(FILE* f, char* s, int* w, int* h)

{

}

void pa_editboxsiz(FILE* f, char* s, int* w, int* h)

{

}

/** ****************************************************************************

Create edit box

Creates single line edit box

*******************************************************************************/

void pa_editboxg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_editbox(FILE* f, int x1, int y1, int x2, int y2, int id)

{

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

}

/** ****************************************************************************

Create progress bar

Creates a progress bar.

*******************************************************************************/

void pa_progbarg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_progbar(FILE* f, int x1, int y1, int x2, int y2, int id)

{

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

}

/** ****************************************************************************

Create list box

Creates a list box. Fills it with the string list provided.

*******************************************************************************/

void pa_listboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

}

void pa_listbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

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

}

/** ****************************************************************************

Create dropdown box

Creates a dropdown box. Fills it with the string list provided.

*******************************************************************************/

void pa_dropboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

}

void pa_dropbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

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

}

/** ****************************************************************************

Create dropdown edit box

Creates a dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

void pa_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

}

void pa_dropeditbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

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

}

/** ****************************************************************************

Create horizontal slider

Creates a horizontal slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void pa_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

}

void pa_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

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

}

/** ****************************************************************************

Create vertical slider

Creates a vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void pa_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

}

void pa_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

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

}

/** ****************************************************************************

Create tab bar

Creates a tab bar with the given orientation.

Bug: has strange overwrite mode where when the widget is first created, it
allows itself to be overwritten by the main window. This is worked around by
creating and distroying another widget.

*******************************************************************************/

void pa_tabbarg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
             pa_tabori tor, int id)

{

}

void pa_tabbar(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
            pa_tabori tor, int id)

{

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

}

/** ****************************************************************************

Display choose color dialog

Presents the choose color dialog, then returns the resulting color.

Bug: does not take the input color as the default.

*******************************************************************************/

void pa_querycolor(int* r, int* g, int* b)

{

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

}

/** ****************************************************************************

Widgets startup

*******************************************************************************/

static void pa_init_widgets(int argc, char *argv[]) __attribute__((constructor (102)));
static void pa_init_widgets(int argc, char *argv[])

{

    int fn; /* file number */

    /* override the event handler */
    pa_eventsover(widget_event, &widget_event_old);

    wigfre = NULL; /* clear widget free list */

    /* clear open files table */
    for (fn = 0; fn < MAXFIL; fn++) opnfil[fn] = NULL;

    /* clear window equivalence table */
    for (fn = 0; fn < MAXFIL*2+1; fn++)
        /* clear window logical number translator table */
        xltwig[fn] = NULL; /* set no widget entry */

}

/** ****************************************************************************

Widgets shutdown

*******************************************************************************/

static void pa_deinit_widgets(void) __attribute__((destructor (102)));
static void pa_deinit_widgets()

{

}
