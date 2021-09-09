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

/* Widget control structure */
typedef struct widget* widptr;
typedef struct widget {

    int            pressed; /* in the pressed state */
    FILE*          wf;      /* output file for the widget window */
    char*          title;   /* title text */
    FILE*          parent;  /* parent window */
    FILE*          evtfil;  /* file to post menu events to */
    int            id;      /* id number */
    int            wid;     /* widget window id */

} widget;

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

/* button control structure */
widget buttonw;

static pa_pevthan button_event_old;

static void button_event(pa_evtrec* ev)

{

    pa_evtrec er; /* outbound button event */

    /* if not our window, send it on */
    if (ev->winid != buttonw.wid) button_event_old(ev);
    else { /* handle it here */

        if (ev->etype == pa_etredraw) { /* redraw the window */

            /* color the background */
            pa_fcolor(buttonw.wf, pa_white);
            pa_frect(buttonw.wf, 1, 1, pa_maxxg(buttonw.wf),
                      pa_maxyg(buttonw.wf));
            /* outline */
            pa_fcolorg(buttonw.wf, INT_MAX/4, INT_MAX/4, INT_MAX/4);
            pa_rrect(buttonw.wf, 2, 2, pa_maxxg(buttonw.wf)-1,
                     pa_maxyg(buttonw.wf)-1, 20, 20);
            if (buttonw.pressed) pa_fcolor(buttonw.wf, pa_red);
            else pa_fcolor(buttonw.wf, pa_black);
            pa_cursorg(buttonw.wf,
                       pa_maxxg(buttonw.wf)/2-pa_strsiz(buttonw.wf, buttonw.title)/2,
                       pa_maxyg(buttonw.wf)/2-pa_chrsizy(buttonw.wf)/2);
            fprintf(buttonw.wf, "%s", buttonw.title); /* place button title */

        } else if (ev->etype == pa_etmouba && ev->amoubn) {

            /* send event back to parent window */
            er.etype = pa_etbutton; /* set button event */
            er.butid = buttonw.id; /* set id */
            pa_sendevent(buttonw.parent, &er); /* send the event to the parent */

            /* process button press */
            buttonw.pressed = TRUE;
            pa_fcolor(buttonw.wf, pa_black);
            pa_frrect(buttonw.wf, 3, 3, pa_maxxg(buttonw.wf)-3,
                     pa_maxyg(buttonw.wf)-3, 20, 20);
            pa_fcolor(buttonw.wf, pa_white);
            pa_cursorg(buttonw.wf,
                       pa_maxxg(buttonw.wf)/2-pa_strsiz(buttonw.wf, buttonw.title)/2,
                       pa_maxyg(buttonw.wf)/2-pa_chrsizy(buttonw.wf)/2);
            fprintf(buttonw.wf, "%s", buttonw.title); /* place button title */

        } else if (ev->etype == pa_etmoubd) {

            buttonw.pressed = FALSE;
            pa_fcolor(buttonw.wf, pa_white);
            pa_frrect(buttonw.wf, 3, 3, pa_maxxg(buttonw.wf)-3,
                     pa_maxyg(buttonw.wf)-3, 20, 20);
            pa_fcolor(buttonw.wf, pa_black);
            pa_cursorg(buttonw.wf,
                       pa_maxxg(buttonw.wf)/2-pa_strsiz(buttonw.wf, buttonw.title)/2,
                       pa_maxyg(buttonw.wf)/2-pa_chrsizy(buttonw.wf)/2);
            fprintf(buttonw.wf, "%s", buttonw.title); /* place button title */

        }

    }

}

void pa_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* override the event handler */
    pa_eventsover(button_event, &button_event_old);
    buttonw.title = s; /* place title */
    buttonw.wid = pa_getwid(); /* allocate a buried wid */
    pa_openwin(&stdin, &buttonw.wf, f, buttonw.wid); /* open widget window */
    buttonw.parent = f; /* save parent file */
    buttonw.id = id; /* set button widget id */
    pa_buffer(buttonw.wf, FALSE); /* turn off buffering */
    pa_auto(buttonw.wf, FALSE); /* turn off auto */
    pa_curvis(buttonw.wf, FALSE); /* turn off cursor */
    pa_font(buttonw.wf, PA_FONT_SIGN); /* set button font */
    pa_bold(buttonw.wf, TRUE); /* set bold font */
    pa_setposg(buttonw.wf, x1, y1); /* place at position */
    pa_setsizg(buttonw.wf, x2-x1, y2-y1); /* set size */
    pa_frame(buttonw.wf, FALSE); /* turn off frame */
    pa_binvis(buttonw.wf); /* no background write */
    pa_linewidth(buttonw.wf, 3); /* thicker lines */

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

Gralib startup

*******************************************************************************/

static void pa_init_widgets(int argc, char *argv[]) __attribute__((constructor (102)));
static void pa_init_widgets(int argc, char *argv[])

{


}

/** ****************************************************************************

Gralib shutdown

*******************************************************************************/

static void pa_deinit_widgets(void) __attribute__((destructor (102)));
static void pa_deinit_widgets()

{

}
