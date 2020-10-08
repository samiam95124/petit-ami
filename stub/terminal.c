/** ****************************************************************************
*                                                                              *
*                                 Petit AMI                                    *
*                                                                              *
*                             TERMINAL MODULE                                  *
*                                                                              *
*                              Created 2020                                    *
*                                                                              *
*                               S. A. FRANCO                                   *
* This is a stubbed version. It exists so that Petit-Ami can be compiled and   *
* linked without all of the modules being complete.                            *
*                                                                              *
* All of the calls in this library print an error and exit.                    *
*                                                                              *
* One use of the stubs module is to serve as a prototype for a new module      *
* implementation.                                                              *
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

#include <stdlib.h>
#include <stdio.h>
#include <terminal.h>

/** ****************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.

*******************************************************************************/

static void error(char *s)
{

    fprintf(stderr, "\nError: Terminal: %s\n", s);

    exit(1);

}

/** ****************************************************************************

Position cursor

This is the external interface to cursor.

*******************************************************************************/

void pa_cursor(FILE *f, int x, int y)

{

    error("pa_cursor: Is not implemented");

}

/*******************************************************************************

Find if cursor is in screen bounds

This is the external interface to curbnd.

*******************************************************************************/

int pa_curbnd(FILE *f)

{

    error("pa_curbnd: Is not implemented");

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int pa_maxx(FILE *f)

{

    error("pa_maxx: Is not implemented");

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int pa_maxy(FILE *f)

{

    error("pa_maxy: Is not implemented");

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void pa_home(FILE *f)

{

    error("pa_home: Is not implemented");

}

/** ****************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void pa_del(FILE* f)

{

    error("pa_del: Is not implemented");

}

/** ****************************************************************************

Move cursor up

This is the external interface to up.

*******************************************************************************/

void pa_up(FILE *f)

{

    error("pa_up: Is not implemented");

}


/** ****************************************************************************

Move cursor down

This is the external interface to down.

*******************************************************************************/

void pa_down(FILE *f)

{

    error("pa_down: Is not implemented");

}

/** ****************************************************************************

Move cursor left

This is the external interface to left.

*******************************************************************************/

void pa_left(FILE *f)

{

    error("pa_left: Is not implemented");

}

/** ****************************************************************************

Move cursor right

This is the external interface to right.

*******************************************************************************/

void pa_right(FILE *f)

{

    error("pa_right: Is not implemented");

}

/** ****************************************************************************

Turn on blink attribute

Turns on/off the blink attribute. Note that under windows 95 in a shell window,
blink does not mean blink, but instead "bright". We leave this alone because
we are supposed to also work over a com interface.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void pa_blink(FILE *f, int e)

{

    error("pa_blink: Is not implemented");

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void pa_reverse(FILE *f, int e)

{

    error("pa_reverse: Is not implemented");

}

/** ****************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void pa_underline(FILE *f, int e)

{

    error("pa_underline: Is not implemented");

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_superscript(FILE *f, int e)

{

    error("pa_superscript: Is not implemented");

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_subscript(FILE *f, int e)

{

    error("pa_subscript: Is not implemented");

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_italic(FILE *f, int e)

{

    error("pa_italic: Is not implemented");

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void pa_bold(FILE *f, int e)

{

    error("pa_bold: Is not implemented");

}

/** ****************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.

Not implemented.

*******************************************************************************/

void pa_strikeout(FILE *f, int e)

{

    error("pa_strikeout: Is not implemented");

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_standout(FILE *f, int e)

{

    error("pa_standout: Is not implemented");

}

/** ****************************************************************************

Set foreground color

Sets the foreground (text) color from the universal primary code.

*******************************************************************************/

void pa_fcolor(FILE *f, pa_color c)

{

    error("pa_fcolor: Is not implemented");

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void pa_bcolor(FILE *f, pa_color c)

{

    error("pa_bcolor: Is not implemented");

}

/** ****************************************************************************

Enable/disable automatic scroll

Enables or disables automatic screen scroll. With automatic scroll on, moving
off the screen at the top or bottom will scroll up or down, respectively.

*******************************************************************************/

void pa_auto(FILE *f, int e)

{

    error("pa_auto: Is not implemented");

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void pa_curvis(FILE *f, int e)

{

    error("pa_curvis: Is not implemented");

}

/** ****************************************************************************

Scroll screen

Process full delta scroll on screen. This is the external interface to this
int.

*******************************************************************************/

void pa_scroll(FILE *f, int x, int y)

{

    error("pa_scroll: Is not implemented");

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int pa_curx(FILE *f)

{

    error("pa_curx: Is not implemented");

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int pa_cury(FILE *f)

{

    error("pa_cury: Is not implemented");

}

/** ****************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
then a new screen is allocated and cleared.

The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

Note that split update and display screens are not implemented at present.

*******************************************************************************/

void pa_select(FILE *f, int u, int d)

{

    error("pa_select: Is not implemented");

}

/** ****************************************************************************

Acquire next input event

Decodes the input for various events. These are sent to the override handlers
first, then if no chained handler dealt with it, we return the event to the
caller.

*******************************************************************************/

void pa_event(FILE* f, pa_evtrec *er)

{

    error("pa_event: Is not implemented");

}

/** ****************************************************************************

Set timer

*******************************************************************************/

void pa_timer(/* file to send event to */              FILE *f,
              /* timer handle */                       int i,
              /* number of 100us counts */             int t,
              /* timer is to rerun after completion */ int r)

{

    error("pa_timer: Is not implemented");

}

/** ****************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.
Killed timers are not removed. Once a timer is set active, it is always set
in reserve.

*******************************************************************************/

void pa_killtimer(/* file to kill timer on */ FILE *f,
                  /* handle of timer */       int i)

{

    error("pa_killtimer: Is not implemented");

}

/** ****************************************************************************

Returns number of mice

Returns the number of mice attached. In xterm, we can't actually determine if
we have a mouse or not, so we just assume we have one. It will be a dead mouse
if none is available, never changing it's state.

*******************************************************************************/

int pa_mouse(FILE *f)

{

    error("pa_mouse: Is not implemented");

}

/** ****************************************************************************

Returns number of buttons on a mouse

Returns the number of buttons implemented on a given mouse. With xterm we have
to assume 3 buttons.

*******************************************************************************/

int pa_mousebutton(FILE *f, int m)

{

    error("pa_mousebutton: Is not implemented");

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

int pa_joystick(FILE *f)

{

    error("pa_joystick: Is not implemented");

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

int pa_joybutton(FILE *f, int j)

{

    error("pa_joybutton: Is not implemented");

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodementional
joystick can be considered a slider without positional meaning.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

int pa_joyaxis(FILE *f, int j)

{

    error("pa_joyaxis: Is not implemented");

}

/** ****************************************************************************

settab

Sets a tab. The tab number t is 1 to n, and indicates the column for the tab.
Setting a tab stop means that when a tab is received, it will move to the next
tab stop that is set. If there is no next tab stop, nothing will happen.

*******************************************************************************/

void pa_settab(FILE* f, int t)

{

    error("pa_settab: Is not implemented");

}

/** ****************************************************************************

restab

Resets a tab. The tab number t is 1 to n, and indicates the column for the tab.

*******************************************************************************/

void pa_restab(FILE* f, int t)

{

    error("pa_restab: Is not implemented");

}

/** ****************************************************************************

clrtab

Clears all tabs.

*******************************************************************************/

void pa_clrtab(FILE* f)

{

    error("pa_clrtab: Is not implemented");

}

/** ****************************************************************************

funkey

Return number of function keys. xterm gives us F1 to F9, takes F10 and F11,
and leaves us F12. It only reserves F10 of the shifted keys, takes most of
the ALT-Fx keys, and leaves all of the CONTROL-Fx keys.
The tradition in PA is to take the F1-F10 keys (it's a nice round number),
but more can be allocated if needed.

*******************************************************************************/

int pa_funkey(FILE* f)

{

    error("pa_funkey: Is not implemented");

}

/** ****************************************************************************

Frametimer

Enables or disables the framing timer.

Not currently implemented.

*******************************************************************************/

void pa_frametimer(FILE* f, int e)

{

    error("pa_frametimer: Is not implemented");

}

/** ****************************************************************************

Autohold

Turns on or off automatic hold mode.

We don't implement automatic hold here.

*******************************************************************************/

void pa_autohold(FILE* f, int e)

{

    error("pa_autohold: Is not implemented");

}

/** ****************************************************************************

Write string direct

Writes a string direct to the terminal, bypassing character handling.

*******************************************************************************/

void pa_wrtstr(FILE* f, char *s)

{

    error("pa_wrtstr: Is not implemented");

}

/** ****************************************************************************

Override event handler

Overrides or "hooks" the indicated event handler. The existing even handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

void pa_eventover(pa_evtcod e, pa_pevthan eh,  pa_pevthan* oeh)

{

    error("pa_eventover: Is not implemented");

}
