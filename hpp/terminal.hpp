/** ****************************************************************************
 *
 * Terminal library interface C++ wrapper header
 *
 * Redeclares terminal library definitions using the terminal namespace.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

#ifndef __TERMINAL_HPP__
#define __TERMINAL_HPP__

#include <terminal.h>

namespace terminal {

#define MAXTIM PA_MAXTIM /**< maximum number of timers available */

typedef pa_color color;
typedef pa_evtcod evtcod;
typedef pa_evtrec evtrec;
typedef pa_evtptr evtptr;

void cursor(FILE* f, int x, int y);
void cursor(int x, int y);
int  maxx(FILE* f);
int  maxx(void);
int  maxy(FILE* f);
int  maxy(void);
void home(FILE* f);
void home(void);
void del(FILE* f);
void del(void);
void up(FILE* f);
void up(void);
void down(FILE* f);
void down(void);
void left(FILE* f);
void left(void);
void right(FILE* f);
void right(void);
void blink(int e);
void blink(int e);
void reverse(int e);
void reverse(int e);
void underline(int e);
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
int  mouse(void);
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

#endif /* __TERMINAL_HPP__ */
