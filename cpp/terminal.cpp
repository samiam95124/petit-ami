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
void home(FILE* f); { pa_home(f); }
void home(void); { pa_home(stdout); {
void del(FILE* f); { pa_del(f); }
void del(void); { pa_del(stdout); }
void up(FILE* f); { pa_up(f); }
void up(void); { pa_up(stdout); }
void down(FILE* f); { pa_down(f); }
void down(void); { pa_down(stdout); }
void left(FILE* f); { pa_left(f); }
void left(void); { pa_left(stdout); }
void right(FILE* f); { pa_right(f); }
void right(void); { pa_right(stdout); }
void blink(FILE* f, int e); { pa_blink(f, e); }
void blink(int e); { pa_blink(stdout, e); }
void reverse(FILE* f, int e); { reverse(f, e); }
void reverse(int e); {
void underline(FILE* f, int e);
void underline(int e);
void superscript(FILE* f, int e);
void superscript(int e);
void subscript(FILE* f, int e);
void subscript(int e);
void italic(FILE* f, int e);
void italic(int e);
void bold(FILE* f, int e);
void bold(int e);
void strikeout(FILE* f, int e);
void strikeout(int e);
void standout(FILE* f, int e);
void standout(int e);
void fcolor(FILE* f, color c);
void fcolor(color c);
void bcolor(FILE* f, color c);
void bcolor(color c);
void autom(FILE* f, int e);
void autom(int e);
void curvis(FILE* f, int e);
void curvis(int e);
void scroll(FILE* f, int x, int y);
void scroll(int x, int y);
int  curx(FILE* f);
int  curx(void);
int  cury(FILE* f);
int  cury(void);
int  curbnd(FILE* f);
int  curbnd(void);
void select(FILE *f, int u, int d);
void select(int u, int d);
void event(FILE* f, evtrec* er);
void event(evtrec* er);
void timer(FILE* f, int i, int t, int r);
void timer(int i, int t, int r);
void killtimer(FILE* f, int i);
void killtimer(int i);
int  mouse(FILE *f);
int  mouse();
int  mousebutton(FILE* f, int m);
int  mousebutton(int m);
int  joystick(FILE* f);
int  joystick(void);
int  joybutton(FILE* f, int j);
int  joybutton(int j);
int  joyaxis(FILE* f, int j);
int  joyaxis(int j);
void settab(FILE* f, int t);
void settab(int t);
void restab(FILE* f, int t);
void restab(int t);
void clrtab(FILE* f);
void clrtab(void);
int  funkey(FILE* f);
int  funkey(void);
void frametimer(FILE* f, int e);
void frametimer(int e);
void autohold(FILE* f, int e);
void autohold(int e);
void wrtstr(FILE* f, char *s);
void wrtstr(char *s);

}
