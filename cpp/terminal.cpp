/** ****************************************************************************
 *
 * Terminal library interface C++ wrapper
 *
 * Wraps the calls in terminal with C++ conventions. This brings several
 * advantages over C code:
 *
 * 1. The functions and other defintions do not need a "pa_" prefix, but rather
 * we let the namespace feature handle namespace isolation.
 *
 * 2. Parameters like what file handle controls the terminal can be defaulted.
 *
 * 3. A terminal object can be used instead of individual calls.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

extern "C" {

#include <stdio.h>

#include <terminal.h>

}

#include "terminal.hpp"

namespace terminal {

void cursor(FILE* f, int x, int y) { pa_cursor(f, x, y); }
void cursor(int x, int y) { pa_cursor(stdout, x, y); }
int  maxx(FILE* f) { return pa_maxx(f); }
int  maxx(void) { return pa_maxx(stdout); }
int  maxy(FILE* f) { return pa_maxy(f); }
int  maxy(void) { return pa_maxy(stdout); }
void home(FILE* f) { pa_home(f); }
void home(void) { pa_home(stdout); }
void del(FILE* f) { pa_del(f); }
void del(void) { pa_del(stdout); }
void up(FILE* f) { pa_up(f); }
void up(void) { pa_up(stdout); }
void down(FILE* f) { pa_down(f); }
void down(void) { pa_down(stdout); }
void left(FILE* f) { pa_left(f); }
void left(void) { pa_left(stdout); }
void right(FILE* f) { pa_right(f); }
void right(void) { pa_right(stdout); }
void blink(FILE* f, int e) { pa_blink(f, e); }
void blink(int e) { pa_blink(stdout, e); }
void reverse(FILE* f, int e) { pa_reverse(f, e); }
void reverse(int e) { pa_reverse(stdout, e); }
void underline(FILE* f, int e) { pa_underline(f, e); }
void underline(int e) { pa_underline(stdout, e); }
void superscript(FILE* f, int e) { pa_superscript(f, e); }
void superscript(int e) { pa_superscript(stdout, e); }
void subscript(FILE* f, int e) { pa_subscript(f, e); }
void subscript(int e) { pa_subscript(stdout, e); }
void italic(FILE* f, int e) { pa_italic(f, e); }
void italic(int e) { pa_italic(stdout, e); }
void bold(FILE* f, int e) { pa_bold(f, e); }
void bold(int e) { pa_bold(stdout, e); }
void strikeout(FILE* f, int e) { pa_strikeout(f, e); }
void strikeout(int e) { pa_strikeout(stdout, e); }
void standout(FILE* f, int e) { pa_standout(f, e); }
void standout(int e) { pa_standout(stdout, e); }
void fcolor(FILE* f, color c) { pa_fcolor(f, c); }
void fcolor(color c) { pa_fcolor(stdout, c); }
void bcolor(FILE* f, color c) { pa_bcolor(f, c); }
void bcolor(color c) { pa_bcolor(stdout, c); }
void autom(FILE* f, int e) { pa_auto(f, e); }
void autom(int e) { pa_auto(stdout, e); }
void curvis(FILE* f, int e) { pa_curvis(f, e); }
void curvis(int e) { pa_curvis(stdout, e); }
void scroll(FILE* f, int x, int y) { pa_scroll(f, x, y); }
void scroll(int x, int y) { pa_scroll(stdout, x, y); }
int  curx(FILE* f) { return pa_curx(f); }
int  curx(void) { return pa_curx(stdout); }
int  cury(FILE* f) { return pa_cury(f); }
int  cury(void) { return pa_cury(stdout); }
int  curbnd(FILE* f) { return pa_curbnd(f); }
int  curbnd(void) { return pa_curbnd(stdout); }
void select(FILE *f, int u, int d) { pa_select(f, u, d); }
void select(int u, int d) { pa_select(stdout, u, d); }
void event(FILE* f, evtrec* er) { pa_event(f, er); }
void event(evtrec* er) { pa_event(stdout, er); }
void timer(FILE* f, int i, int t, int r) { pa_timer(f, i, t, r); }
void timer(int i, int t, int r) { pa_timer(stdout, i, t, r); }
void killtimer(FILE* f, int i) { pa_killtimer(f, i); }
void killtimer(int i) { pa_killtimer(stdout, i); }
int  mouse(FILE *f) { return pa_mouse(f); }
int  mouse(void) { return pa_mouse(stdout); }
int  mousebutton(FILE* f, int m) { return pa_mousebutton(f, m); }
int  mousebutton(int m) { return pa_mousebutton(stdout, m); }
int  joystick(FILE* f) { return pa_joystick(f); }
int  joystick(void) { return pa_joystick(stdout); }
int  joybutton(FILE* f, int j) { return pa_joybutton(f, j); }
int  joybutton(int j) { return pa_joybutton(stdout, j); }
int  joyaxis(FILE* f, int j) { return pa_joyaxis(f, j); }
int  joyaxis(int j) { return pa_joyaxis(stdout, j); }
void settab(FILE* f, int t) { pa_settab(f, t); }
void settab(int t) { pa_settab(stdout, t); }
void restab(FILE* f, int t) { pa_restab(f, t); }
void restab(int t) { pa_restab(stdout, t); }
void clrtab(FILE* f) { pa_clrtab(f); }
void clrtab(void) { pa_clrtab(stdout); }
int  funkey(FILE* f) { return pa_funkey(f); }
int  funkey(void) { return pa_funkey(stdout); }
void frametimer(FILE* f, int e) { pa_frametimer(f, e); }
void frametimer(int e) { pa_frametimer(stdout, e); }
void autohold(FILE* f, int e) { pa_autohold(f, e); }
void autohold(int e) { pa_autohold(stdout, e); }
void wrtstr(FILE* f, char *s) { pa_wrtstr(f, s); }
void wrtstr(char *s) { pa_wrtstr(stdout, s); }

}
