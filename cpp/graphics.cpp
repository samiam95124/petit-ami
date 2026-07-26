/** ****************************************************************************
 *
 * Graphics library interface C++ wrapper
 *
 * Wraps the calls in graphics with C++ conventions. This brings several
 * advantages over C code:
 *
 * 1. The functions and other defintions do not need a "ami_" prefix, but rather
 * we let the namespace feature handle namespace isolation.
 *
 * 2. Parameters like what file handle controls the output can be defaulted.
 *
 * 3. A graph object can be used instead of individual calls.
 *
 * 4. Instead of registering callbacks in C, the graph object features virtual
 * functions for each event than can be individually overriden.
 *
 * Graphics has two distinct types of interfaces, the procedural and the object/
 * class interfaces. The procedural interface expects the specification of
 * what output surface we are talking to to be the first parameter of all
 * procedures and functions (even if defaulted to stdin or stdout). The object/
 * class interface keeps that as part of the object.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

extern "C" {

#include <stdio.h>

#include <graphics.h>

}

#include "graphics.hpp"

namespace graphics {

/* hook for sending events back to methods */
graph* graphoCB;
pevthan graphoeh;

/* procedures and functions */

/* text */
void cursor(FILE* f, long x, long y) { ami_cursor(f, x, y); }
void cursor(long x, long y) { ami_cursor(stdout, x, y); }
long  maxx(FILE* f) { return ami_maxx(f); }
long  maxx(void) { return ami_maxx(stdout); }
long  maxy(FILE* f) { return ami_maxy(f); }
long  maxy(void) { return ami_maxy(stdout); }
void home(FILE* f) { ami_home(f); }
void home(void) { ami_home(stdout); }
void del(FILE* f) { ami_del(f); }
void del(void) { ami_del(stdout); }
void up(FILE* f) { ami_up(f); }
void up(void) { ami_up(stdout); }
void down(FILE* f) { ami_down(f); }
void down(void) { ami_down(stdout); }
void left(FILE* f) { ami_left(f); }
void left(void) { ami_left(stdout); }
void right(FILE* f) { ami_right(f); }
void right(void) { ami_right(stdout); }
void blink(FILE* f, long e) { ami_blink(f, e); }
void blink(long e) { ami_blink(stdout, e); }
void reverse(FILE* f, long e) { ami_reverse(f, e); }
void reverse(long e) { ami_reverse(stdout, e); }
void underline(FILE* f, long e) { ami_underline(f, e); }
void underline(long e) { ami_underline(stdout, e); }
void superscript(FILE* f, long e) { ami_superscript(f, e); }
void superscript(long e) { ami_superscript(stdout, e); }
void subscript(FILE* f, long e) { ami_subscript(f, e); }
void subscript(long e) { ami_subscript(stdout, e); }
void italic(FILE* f, long e) { ami_italic(f, e); }
void italic(long e) { ami_italic(stdout, e); }
void bold(FILE* f, long e) { ami_bold(f, e); }
void bold(long e) { ami_bold(stdout, e); }
void strikeout(FILE* f, long e) { ami_strikeout(f, e); }
void strikeout(long e) { ami_strikeout(stdout, e); }
void standout(FILE* f, long e) { ami_standout(f, e); }
void standout(long e) { ami_standout(stdout, e); }
void fcolor(FILE* f, color c) { ami_fcolor(f, (ami_color)c); }
void fcolor(color c) { ami_fcolor(stdout, (ami_color)c); }
void bcolor(FILE* f, color c) { ami_bcolor(f, (ami_color)c); }
void bcolor(color c) { ami_bcolor(stdout, (ami_color)c); }
void autom(FILE* f, long e) { ami_auto(f, e); }
void autom(long e) { ami_auto(stdout, e); }
void curvis(FILE* f, long e) { ami_curvis(f, e); }
void curvis(long e) { ami_curvis(stdout, e); }
void scroll(FILE* f, long x, long y) { ami_scroll(f, x, y); }
void scroll(long x, long y) { ami_scroll(stdout, x, y); }
long  curx(FILE* f) { return ami_curx(f); }
long  curx(void) { return ami_curx(stdout); }
long  cury(FILE* f) { return ami_cury(f); }
long  cury(void) { return ami_cury(stdout); }
long  curbnd(FILE* f) { return ami_curbnd(f); }
long  curbnd(void) { return ami_curbnd(stdout); }
void select(FILE* f, long u, long d) { ami_select(f, u, d); }
void select(long u, long d) { ami_select(stdout, u, d); }
void event(FILE* f, evtrec* er) { ami_event(f, (ami_evtptr)er); }
void event(evtrec* er) { ami_event(stdin, (ami_evtptr)er); }
void timer(FILE* f, long i, long t, long r) { ami_timer(f, i, t, r); }
void timer(long i, long t, long r) { ami_timer(stdout, i, t, r); }
void killtimer(FILE* f, long i) { ami_killtimer(f, i); }
void killtimer(long i) { ami_killtimer(stdout, i); }
long  mouse(FILE* f) { return ami_mouse(f); }
long  mouse(void) { return ami_mouse(stdout); }
long  mousebutton(FILE* f, long m) { return ami_mousebutton(f, m); }
long  mousebutton(long m) { return ami_mousebutton(stdout, m); }
long  joystick(FILE* f) { return ami_joystick(f); }
long  joystick(void) { return ami_joystick(stdout); }
long  joybutton(FILE* f, long j) { return ami_joybutton(f, j); }
long  joybutton(long j) { return ami_joybutton(stdout, j); }
long  joyaxis(FILE* f, long j) { return ami_joyaxis(f, j); }
long  joyaxis(long j) { return ami_joyaxis(stdout, j); }
void settab(FILE* f, long t) { ami_settab(f, t); }
void settab(long t) { ami_settab(stdout, t); }
void restab(FILE* f, long t) { ami_restab(f, t); }
void restab(long t) { ami_restab(stdout, t); }
void clrtab(FILE* f) { ami_clrtab(f); }
void clrtab(void) { ami_clrtab(stdout); }
long  funkey(FILE* f) { return ami_funkey(f); }
long  funkey(void) { return ami_funkey(stdout); }
void frametimer(FILE* f, long e) { ami_frametimer(f, e); }
void frametimer(long e) { ami_frametimer(stdout, e); }
void autohold(long e) { ami_autohold(e); }
void wrtstr(FILE* f, char* s) { ami_wrtstr(f, s); }
void wrtstr(char* s) { ami_wrtstr(stdout, s); }
void wrtstrn(FILE* f, char* s, long n) { ami_wrtstrn(f, s, n); }
void wrtstrn(char* s, long n) { ami_wrtstrn(stdout, s, n); }
void sizbuf(FILE* f, long x, long y) { ami_sizbuf(f, x, y); }
void sizbuf(long x, long y) { ami_sizbuf(stdout, x, y); }
void title(FILE* f, char* ts) { ami_title(f, ts); }
void title(char* ts) { ami_title(stdout, ts); }
void eventover(evtcod e, pevthan eh, pevthan* oeh) { ami_eventover((ami_evtcod)e, (ami_pevthan)eh, (ami_pevthan*)oeh); }
void eventsover(pevthan eh, pevthan* oeh) { ami_eventsover((ami_pevthan)eh, (ami_pevthan*)oeh); }
void sendevent(FILE* f, evtrec* er) { ami_sendevent(f, (ami_evtptr)er); }
void sendevent(evtrec* er) { ami_sendevent(stdout, (ami_evtptr)er); }

/* graphical */
long  maxxg(FILE* f) { return ami_maxxg(f); }
long  maxxg(void) { return ami_maxxg(stdout); }
long  maxyg(FILE* f) { return ami_maxyg(f); }
long  maxyg(void) { return ami_maxyg(stdout); }
long  curxg(FILE* f) { return ami_curxg(f); }
long  curxg(void) { return ami_curxg(stdout); }
long  curyg(FILE* f) { return ami_curyg(f); }
long  curyg(void) { return ami_curyg(stdout); }
void line(FILE* f, long x1, long y1, long x2, long y2) { ami_line(f, x1, y1, x2, y2); }
void line(long x1, long y1, long x2, long y2) { ami_line(stdout, x1, y1, x2, y2); }
void linewidth(FILE* f, long w) { ami_linewidth(f, w); }
void linewidth(long w) { ami_linewidth(stdout, w); }
void rect(FILE* f, long x1, long y1, long x2, long y2) { ami_rect(f, x1, y1, x2, y2); }
void rect(long x1, long y1, long x2, long y2) { ami_rect(stdout, x1, y1, x2, y2); }
void frect(FILE* f, long x1, long y1, long x2, long y2) { ami_frect(f, x1, y1, x2, y2); }
void frect(long x1, long y1, long x2, long y2) { ami_frect(stdout, x1, y1, x2, y2); }
void rrect(FILE* f, long x1, long y1, long x2, long y2, long xs, long ys) { ami_rrect(f, x1, y1, x2, y2, xs, ys); }
void rrect(long x1, long y1, long x2, long y2, long xs, long ys) { ami_rrect(stdout, x1, y1, x2, y2, xs, ys); }
void frrect(FILE* f, long x1, long y1, long x2, long y2, long xs, long ys) { ami_frrect(f, x1, y1, x2, y2, xs, ys); }
void frrect(long x1, long y1, long x2, long y2, long xs, long ys) { ami_frrect(stdout, x1, y1, x2, y2, xs, ys); }
void ellipse(FILE* f, long x1, long y1, long x2, long y2) { ami_ellipse(f, x1, y1, x2, y2); }
void ellipse(long x1, long y1, long x2, long y2) { ami_ellipse(stdout, x1, y1, x2, y2); }
void fellipse(FILE* f, long x1, long y1, long x2, long y2) { ami_fellipse(f, x1, y1, x2, y2); }
void fellipse(long x1, long y1, long x2, long y2) { ami_fellipse(stdout, x1, y1, x2, y2); }
void arc(FILE* f, long x1, long y1, long x2, long y2, long sa, long ea) { ami_arc(f, x1, y1, x2, y2, sa, ea); }
void arc(long x1, long y1, long x2, long y2, long sa, long ea) { ami_arc(stdout, x1, y1, x2, y2, sa, ea); }
void farc(FILE* f, long x1, long y1, long x2, long y2, long sa, long ea) { ami_farc(f, x1, y1, x2, y2, sa, ea); }
void farc(long x1, long y1, long x2, long y2, long sa, long ea) { ami_farc(stdout, x1, y1, x2, y2, sa, ea); }
void fchord(FILE* f, long x1, long y1, long x2, long y2, long sa, long ea) { ami_fchord(f, x1, y1, x2, y2, sa, ea); }
void fchord(long x1, long y1, long x2, long y2, long sa, long ea) { ami_fchord(stdout, x1, y1, x2, y2, sa, ea); }
void ftriangle(FILE* f, long x1, long y1, long x2, long y2, long x3, long y3) { ami_ftriangle(f, x1, y1, x2, y2, x3, y3); }
void ftriangle(long x1, long y1, long x2, long y2, long x3, long y3) { ami_ftriangle(stdout, x1, y1, x2, y2, x3, y3); }
void cursorg(FILE* f, long x, long y) { ami_cursorg(f, x, y); }
void cursorg(long x, long y) { ami_cursorg(stdout, x, y); }
long  baseline(FILE* f) { return ami_baseline(f); }
long  baseline(void) { return ami_baseline(stdout); }
void setpixel(FILE* f, long x, long y) { ami_setpixel(f, x, y); }
void setpixel(long x, long y) { ami_setpixel(stdout, x, y); }
void fover(FILE* f) { ami_fover(f); }
void fover(void) { ami_fover(stdout); }
void bover(FILE* f) { ami_bover(f); }
void bover(void) { ami_bover(stdout); }
void finvis(FILE* f) { ami_finvis(f); }
void finvis(void) { ami_finvis(stdout); }
void binvis(FILE* f) { ami_binvis(f); }
void binvis(void) { ami_binvis(stdout); }
void fxor(FILE* f) { ami_fxor(f); }
void fxor(void) { ami_fxor(stdout); }
void bxor(FILE* f) { ami_bxor(f); }
void bxor(void) { ami_bxor(stdout); }
void fand(FILE* f) { ami_fand(f); }
void fand(void) { ami_fand(stdout); }
void band(FILE* f) { ami_band(f); }
void band(void) { ami_band(stdout); }
void for_(FILE* f) { ami_for(f); }
void for_(void) { ami_for(stdout); }
void bor(FILE* f) { ami_bor(f); }
void bor(void) { ami_bor(stdout); }
long  chrsizx(FILE* f) { return ami_chrsizx(f); }
long  chrsizx(void) { return ami_chrsizx(stdout); }
long  chrsizy(FILE* f) { return ami_chrsizy(f); }
long  chrsizy(void) { return ami_chrsizy(stdout); }
long  fonts(FILE* f) { return ami_fonts(f); }
long  fonts(void) { return ami_fonts(stdout); }
void font(FILE* f, long fc) { ami_font(f, fc); }
void font(long fc) { ami_font(stdout, fc); }
void fontnam(FILE* f, long fc, char* fns, long fnsl) { ami_fontnam(f, fc, fns, fnsl); }
void fontnam(long fc, char* fns, long fnsl) { ami_fontnam(stdout, fc, fns, fnsl); }
void fontsiz(FILE* f, long s) { ami_fontsiz(f, s); }
void fontsiz(long s) { ami_fontsiz(stdout, s); }
void chrspcy(FILE* f, long s) { ami_chrspcy(f, s); }
void chrspcy(long s) { ami_chrspcy(stdout, s); }
void chrspcx(FILE* f, long s) { ami_chrspcx(f, s); }
void chrspcx(long s) { ami_chrspcx(stdout, s); }
long  dpmx(FILE* f) { return ami_dpmx(f); }
long  dpmx(void) { return ami_dpmx(stdout); }
long  dpmy(FILE* f) { return ami_dpmy(f); }
long  dpmy(void) { return ami_dpmy(stdout); }
long  strsiz(FILE* f, const char* s) { return ami_strsiz(f, s); }
long  strsiz(const char* s) { return ami_strsiz(stdout, s); }
long  chrpos(FILE* f, const char* s, long p) { return ami_chrpos(f, s, p); }
long  chrpos(const char* s, long p) { return ami_chrpos(stdout, s, p); }
void writejust(FILE* f, const char* s, long n) { ami_writejust(f, s, n); }
void writejust(const char* s, long n) { ami_writejust(stdout, s, n); }
long  justpos(FILE* f, const char* s, long p, long n) { return ami_justpos(f, s, p, n); }
long  justpos(const char* s, long p, long n) { return ami_justpos(stdout, s, p, n); }
void condensed(FILE* f, long e) { ami_condensed(f, e); }
void condensed(long e) { ami_condensed(stdout, e); }
void extended(FILE* f, long e) { ami_extended(f, e); }
void extended(long e) { ami_extended(stdout, e); }
void xlight(FILE* f, long e) { ami_xlight(f, e); }
void xlight(long e) { ami_xlight(stdout, e); }
void light(FILE* f, long e) { ami_light(f, e); }
void light(long e) { ami_light(stdout, e); }
void xbold(FILE* f, long e) { ami_xbold(f, e); }
void xbold(long e) { ami_xbold(stdout, e); }
void hollow(FILE* f, long e) { ami_hollow(f, e); }
void hollow(long e) { ami_hollow(stdout, e); }
void raised(FILE* f, long e) { ami_raised(f, e); }
void raised(long e) { ami_raised(stdout, e); }
void settabg(FILE* f, long t) { ami_settabg(f, t); }
void settabg(long t) { ami_settabg(stdout, t); }
void restabg(FILE* f, long t) { ami_restabg(f, t); }
void restabg(long t) { ami_restabg(stdout, t); }
void fcolorg(FILE* f, long r, long g, long b) { ami_fcolorg(f, r, g, b); }
void fcolorg(long r, long g, long b) { ami_fcolorg(stdout, r, g, b); }
void fcolorc(FILE* f, long r, long g, long b) { ami_fcolorc(f, r, g, b); }
void fcolorc(long r, long g, long b) { ami_fcolorc(stdout, r, g, b); }
void bcolorg(FILE* f, long r, long g, long b) { ami_bcolorg(f, r, g, b); }
void bcolorg(long r, long g, long b) { ami_bcolorg(stdout, r, g, b); }
void bcolorc(FILE* f, long r, long g, long b) { ami_bcolorc(f, r, g, b); }
void bcolorc(long r, long g, long b) { ami_bcolorc(stdout, r, g, b); }
void loadpict(FILE* f, long p, char* fn) { ami_loadpict(f, p, fn); }
void loadpict(long p, char* fn) { ami_loadpict(stdout, p, fn); }
long  pictsizx(FILE* f, long p) { return ami_pictsizx(f, p); }
long  pictsizx(long p) { return ami_pictsizx(stdout, p); }
long  pictsizy(FILE* f, long p) { return ami_pictsizy(f, p); }
long  pictsizy(long p) { return ami_pictsizy(stdout, p); }
void picture(FILE* f, long p, long x1, long y1, long x2, long y2) { ami_picture(f, p, x1, y1, x2, y2); }
void picture(long p, long x1, long y1, long x2, long y2) { ami_picture(stdout, p, x1, y1, x2, y2); }
void delpict(FILE* f, long p) { ami_delpict(f, p); }
void delpict(long p) { ami_delpict(stdout, p); }
void scrollg(FILE* f, long x, long y) { ami_scrollg(f, x, y); }
void scrollg(long x, long y) { ami_scrollg(stdout, x, y); }
void path(FILE* f, long a) { ami_path(f, a); }
void path(long a) { ami_path(stdout, a); }

/* window management */
void openwin(FILE** infile, FILE** outfile, FILE* parent, long wid) { ami_openwin(infile, outfile, parent, wid); }
void buffer(FILE* f, long e) { ami_buffer(f, e); }
void buffer(long e) { ami_buffer(stdout, e); }
void sizbufg(FILE* f, long x, long y) { ami_sizbufg(f, x, y); }
void sizbufg(long x, long y) { ami_sizbufg(stdout, x, y); }
void getsiz(FILE* f, long* x, long* y) { ami_getsiz(f, x, y); }
void getsiz(long* x, long* y) { ami_getsiz(stdout, x, y); }
void getsizg(FILE* f, long* x, long* y) { ami_getsizg(f, x, y); }
void getsizg(long* x, long* y) { ami_getsizg(stdout, x, y); }
void setsiz(FILE* f, long x, long y) { ami_setsiz(f, x, y); }
void setsiz(long x, long y) { ami_setsiz(stdout, x, y); }
void setsizg(FILE* f, long x, long y) { ami_setsizg(f, x, y); }
void setsizg(long x, long y) { ami_setsizg(stdout, x, y); }
void setpos(FILE* f, long x, long y) { ami_setpos(f, x, y); }
void setpos(long x, long y) { ami_setpos(stdout, x, y); }
void setposg(FILE* f, long x, long y) { ami_setposg(f, x, y); }
void setposg(long x, long y) { ami_setposg(stdout, x, y); }
void scnsiz(FILE* f, long* x, long* y) { ami_scnsiz(f, x, y); }
void scnsiz(long* x, long* y) { ami_scnsiz(stdout, x, y); }
void scnsizg(FILE* f, long* x, long* y) { ami_scnsizg(f, x, y); }
void scnsizg(long* x, long* y) { ami_scnsizg(stdout, x, y); }
void scncen(FILE* f, long* x, long* y) { ami_scncen(f, x, y); }
void scncen(long* x, long* y) { ami_scncen(stdout, x, y); }
void scnceng(FILE* f, long* x, long* y) { ami_scnceng(f, x, y); }
void scnceng(long* x, long* y) { ami_scnceng(stdout, x, y); }
void winclient(FILE* f, long cx, long cy, long* wx, long* wy, winmodset ms) { ami_winclient(f, cx, cy, wx, wy, (ami_winmodset)ms); }
void winclient(long cx, long cy, long* wx, long* wy, winmodset ms) { ami_winclient(stdout, cx, cy, wx, wy, (ami_winmodset)ms); }
void winclientg(FILE* f, long cx, long cy, long* wx, long* wy, winmodset ms) { ami_winclientg(f, cx, cy, wx, wy, (ami_winmodset)ms); }
void winclientg(long cx, long cy, long* wx, long* wy, winmodset ms) { ami_winclientg(stdout, cx, cy, wx, wy, (ami_winmodset)ms); }
void front(FILE* f) { ami_front(f); }
void front(void) { ami_front(stdout); }
void back(FILE* f) { ami_back(f); }
void back(void) { ami_back(stdout); }
void frame(FILE* f, long e) { ami_frame(f, e); }
void frame(long e) { ami_frame(stdout, e); }
void sizable(FILE* f, long e) { ami_sizable(f, e); }
void sizable(long e) { ami_sizable(stdout, e); }
void sysbar(FILE* f, long e) { ami_sysbar(f, e); }
void sysbar(long e) { ami_sysbar(stdout, e); }
void menu(FILE* f, menuptr m) { ami_menu(f, (ami_menuptr)m); }
void menu(menuptr m) { ami_menu(stdout, (ami_menuptr)m); }
void menuena(FILE* f, long id, long onoff) { ami_menuena(f, id, onoff); }
void menuena(long id, long onoff) { ami_menuena(stdout, id, onoff); }
void menusel(FILE* f, long id, long select) { ami_menusel(f, id, select); }
void menusel(long id, long select) { ami_menusel(stdout, id, select); }
void stdmenu(stdmenusel sms, menuptr* sm, menuptr pm) { ami_stdmenu(sms, (ami_menuptr*)sm, (ami_menuptr)pm); }
long  getwinid(void) { return ami_getwinid(); }
void focus(FILE* f) { ami_focus(f); }
void focus(void) { ami_focus(stdout); }

/* widgets/controls */
long  getwigid(FILE* f) { return ami_getwigid(f); }
long  getwigid(void) { return ami_getwigid(stdout); }
void killwidget(FILE* f, long id) { ami_killwidget(f, id); }
void killwidget(long id) { ami_killwidget(stdout, id); }
void selectwidget(FILE* f, long id, long e) { ami_selectwidget(f, id, e); }
void selectwidget(long id, long e) { ami_selectwidget(stdout, id, e); }
void enablewidget(FILE* f, long id, long e) { ami_enablewidget(f, id, e); }
void enablewidget(long id, long e) { ami_enablewidget(stdout, id, e); }
void getwidgettext(FILE* f, long id, char* s, long sl) { ami_getwidgettext(f, id, s, sl); }
void getwidgettext(long id, char* s, long sl) { ami_getwidgettext(stdout, id, s, sl); }
void putwidgettext(FILE* f, long id, char* s) { ami_putwidgettext(f, id, s); }
void putwidgettext(long id, char* s) { ami_putwidgettext(stdout, id, s); }
void sizwidget(FILE* f, long id, long x, long y) { ami_sizwidget(f, id, x, y); }
void sizwidget(long id, long x, long y) { ami_sizwidget(stdout, id, x, y); }
void sizwidgetg(FILE* f, long id, long x, long y) { ami_sizwidgetg(f, id, x, y); }
void sizwidgetg(long id, long x, long y) { ami_sizwidgetg(stdout, id, x, y); }
void poswidget(FILE* f, long id, long x, long y) { ami_poswidget(f, id, x, y); }
void poswidget(long id, long x, long y) { ami_poswidget(stdout, id, x, y); }
void poswidgetg(FILE* f, long id, long x, long y) { ami_poswidgetg(f, id, x, y); }
void poswidgetg(long id, long x, long y) { ami_poswidgetg(stdout, id, x, y); }
void backwidget(FILE* f, long id) { ami_backwidget(f, id); }
void backwidget(long id) { ami_backwidget(stdout, id); }
void frontwidget(FILE* f, long id) { ami_frontwidget(f, id); }
void frontwidget(long id) { ami_frontwidget(stdout, id); }
void focuswidget(FILE* f, long id) { ami_focuswidget(f, id); }
void focuswidget(long id) { ami_focuswidget(stdout, id); }
void buttonsiz(FILE* f, char* s, long* w, long* h) { ami_buttonsiz(f, s, w, h); }
void buttonsiz(char* s, long* w, long* h) { ami_buttonsiz(stdout, s, w, h); }
void buttonsizg(FILE* f, char* s, long* w, long* h) { ami_buttonsizg(f, s, w, h); }
void buttonsizg(char* s, long* w, long* h) { ami_buttonsizg(stdout, s, w, h); }
void button(FILE* f, long x1, long y1, long x2, long y2, char* s, long id) { ami_button(f, x1, y1, x2, y2, s, id); }
void button(long x1, long y1, long x2, long y2, char* s, long id) { ami_button(stdout, x1, y1, x2, y2, s, id); }
void buttong(FILE* f, long x1, long y1, long x2, long y2, char* s, long id) { ami_buttong(f, x1, y1, x2, y2, s, id); }
void buttong(long x1, long y1, long x2, long y2, char* s, long id) { ami_buttong(stdout, x1, y1, x2, y2, s, id); }
void checkboxsiz(FILE* f, char* s, long* w, long* h) { ami_checkboxsiz(f, s, w, h); }
void checkboxsiz(char* s, long* w, long* h) { ami_checkboxsiz(stdout, s, w, h); }
void checkboxsizg(FILE* f, char* s, long* w, long* h) { ami_checkboxsizg(f, s, w, h); }
void checkboxsizg(char* s, long* w, long* h) { ami_checkboxsizg(stdout, s, w, h); }
void checkbox(FILE* f, long x1, long y1, long x2, long y2, char* s, long id) { ami_checkbox(f, x1, y1, x2, y2, s, id); }
void checkbox(long x1, long y1, long x2, long y2, char* s, long id) { ami_checkbox(stdout, x1, y1, x2, y2, s, id); }
void checkboxg(FILE* f, long x1, long y1, long x2, long y2, char* s, long id) { ami_checkboxg(f, x1, y1, x2, y2, s, id); }
void checkboxg(long x1, long y1, long x2, long y2, char* s, long id) { ami_checkboxg(stdout, x1, y1, x2, y2, s, id); }
void radiobuttonsiz(FILE* f, char* s, long* w, long* h) { ami_radiobuttonsiz(f, s, w, h); }
void radiobuttonsiz(char* s, long* w, long* h) { ami_radiobuttonsiz(stdout, s, w, h); }
void radiobuttonsizg(FILE* f, char* s, long* w, long* h) { ami_radiobuttonsizg(f, s, w, h); }
void radiobuttonsizg(char* s, long* w, long* h) { ami_radiobuttonsizg(stdout, s, w, h); }
void radiobutton(FILE* f, long x1, long y1, long x2, long y2, char* s, long id) { ami_radiobutton(f, x1, y1, x2, y2, s, id); }
void radiobutton(long x1, long y1, long x2, long y2, char* s, long id) { ami_radiobutton(stdout, x1, y1, x2, y2, s, id); }
void radiobuttong(FILE* f, long x1, long y1, long x2, long y2, char* s, long id) { ami_radiobuttong(f, x1, y1, x2, y2, s, id); }
void radiobuttong(long x1, long y1, long x2, long y2, char* s, long id) { ami_radiobuttong(stdout, x1, y1, x2, y2, s, id); }
void groupsiz(FILE* f, char* s, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_groupsiz(f, s, cw, ch, w, h, ox, oy); }
void groupsiz(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_groupsiz(stdout, s, cw, ch, w, h, ox, oy); }
void groupsizg(FILE* f, char* s, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_groupsizg(f, s, cw, ch, w, h, ox, oy); }
void groupsizg(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_groupsizg(stdout, s, cw, ch, w, h, ox, oy); }
void group(FILE* f, long x1, long y1, long x2, long y2, char* s, long id) { ami_group(f, x1, y1, x2, y2, s, id); }
void group(long x1, long y1, long x2, long y2, char* s, long id) { ami_group(stdout, x1, y1, x2, y2, s, id); }
void groupg(FILE* f, long x1, long y1, long x2, long y2, char* s, long id) { ami_groupg(f, x1, y1, x2, y2, s, id); }
void groupg(long x1, long y1, long x2, long y2, char* s, long id) { ami_groupg(stdout, x1, y1, x2, y2, s, id); }
void background(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_background(f, x1, y1, x2, y2, id); }
void background(long x1, long y1, long x2, long y2, long id) { ami_background(stdout, x1, y1, x2, y2, id); }
void backgroundg(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_backgroundg(f, x1, y1, x2, y2, id); }
void backgroundg(long x1, long y1, long x2, long y2, long id) { ami_backgroundg(stdout, x1, y1, x2, y2, id); }
void scrollvertsiz(FILE* f, long* w, long* h) { ami_scrollvertsiz(f, w, h); }
void scrollvertsiz(long* w, long* h) { ami_scrollvertsiz(stdout, w, h); }
void scrollvertsizg(FILE* f, long* w, long* h) { ami_scrollvertsizg(f, w, h); }
void scrollvertsizg(long* w, long* h) { ami_scrollvertsizg(stdout, w, h); }
void scrollvert(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_scrollvert(f, x1, y1, x2, y2, id); }
void scrollvert(long x1, long y1, long x2, long y2, long id) { ami_scrollvert(stdout, x1, y1, x2, y2, id); }
void scrollvertg(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_scrollvertg(f, x1, y1, x2, y2, id); }
void scrollvertg(long x1, long y1, long x2, long y2, long id) { ami_scrollvertg(stdout, x1, y1, x2, y2, id); }
void scrollhorizsiz(FILE* f, long* w, long* h) { ami_scrollhorizsiz(f, w, h); }
void scrollhorizsiz(long* w, long* h) { ami_scrollhorizsiz(stdout, w, h); }
void scrollhorizsizg(FILE* f, long* w, long* h) { ami_scrollhorizsizg(f, w, h); }
void scrollhorizsizg(long* w, long* h) { ami_scrollhorizsizg(stdout, w, h); }
void scrollhoriz(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_scrollhoriz(f, x1, y1, x2, y2, id); }
void scrollhoriz(long x1, long y1, long x2, long y2, long id) { ami_scrollhoriz(stdout, x1, y1, x2, y2, id); }
void scrollhorizg(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_scrollhorizg(f, x1, y1, x2, y2, id); }
void scrollhorizg(long x1, long y1, long x2, long y2, long id) { ami_scrollhorizg(stdout, x1, y1, x2, y2, id); }
void scrollpos(FILE* f, long id, long r) { ami_scrollpos(f, id, r); }
void scrollpos(long id, long r) { ami_scrollpos(stdout, id, r); }
void scrollsiz(FILE* f, long id, long r) { ami_scrollsiz(f, id, r); }
void scrollsiz(long id, long r) { ami_scrollsiz(stdout, id, r); }
void numselboxsiz(FILE* f, long l, long u, long* w, long* h) { ami_numselboxsiz(f, l, u, w, h); }
void numselboxsiz(long l, long u, long* w, long* h) { ami_numselboxsiz(stdout, l, u, w, h); }
void numselboxsizg(FILE* f, long l, long u, long* w, long* h) { ami_numselboxsizg(f, l, u, w, h); }
void numselboxsizg(long l, long u, long* w, long* h) { ami_numselboxsizg(stdout, l, u, w, h); }
void numselbox(FILE* f, long x1, long y1, long x2, long y2, long l, long u, long id) { ami_numselbox(f, x1, y1, x2, y2, l, u, id); }
void numselbox(long x1, long y1, long x2, long y2, long l, long u, long id) { ami_numselbox(stdout, x1, y1, x2, y2, l, u, id); }
void numselboxg(FILE* f, long x1, long y1, long x2, long y2, long l, long u, long id) { ami_numselboxg(f, x1, y1, x2, y2, l, u, id); }
void numselboxg(long x1, long y1, long x2, long y2, long l, long u, long id) { ami_numselboxg(stdout, x1, y1, x2, y2, l, u, id); }
void editboxsiz(FILE* f, char* s, long* w, long* h) { ami_editboxsiz(f, s, w, h); }
void editboxsiz(char* s, long* w, long* h) { ami_editboxsiz(stdout, s, w, h); }
void editboxsizg(FILE* f, char* s, long* w, long* h) { ami_editboxsizg(f, s, w, h); }
void editboxsizg(char* s, long* w, long* h) { ami_editboxsizg(stdout, s, w, h); }
void editbox(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_editbox(f, x1, y1, x2, y2, id); }
void editbox(long x1, long y1, long x2, long y2, long id) { ami_editbox(stdout, x1, y1, x2, y2, id); }
void editboxg(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_editboxg(f, x1, y1, x2, y2, id); }
void editboxg(long x1, long y1, long x2, long y2, long id) { ami_editboxg(stdout, x1, y1, x2, y2, id); }
void progbarsiz(FILE* f, long* w, long* h) { ami_progbarsiz(f, w, h); }
void progbarsiz(long* w, long* h) { ami_progbarsiz(stdout, w, h); }
void progbarsizg(FILE* f, long* w, long* h) { ami_progbarsizg(f, w, h); }
void progbarsizg(long* w, long* h) { ami_progbarsizg(stdout, w, h); }
void progbar(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_progbar(f, x1, y1, x2, y2, id); }
void progbar(long x1, long y1, long x2, long y2, long id) { ami_progbar(stdout, x1, y1, x2, y2, id); }
void progbarg(FILE* f, long x1, long y1, long x2, long y2, long id) { ami_progbarg(f, x1, y1, x2, y2, id); }
void progbarg(long x1, long y1, long x2, long y2, long id) { ami_progbarg(stdout, x1, y1, x2, y2, id); }
void progbarpos(FILE* f, long id, long pos) { ami_progbarpos(f, id, pos); }
void progbarpos(long id, long pos) { ami_progbarpos(stdout, id, pos); }
void listboxsiz(FILE* f, strptr sp, long* w, long* h) { ami_listboxsiz(f, (ami_strptr)sp, w, h); }
void listboxsiz(strptr sp, long* w, long* h) { ami_listboxsiz(stdout, (ami_strptr)sp, w, h); }
void listboxsizg(FILE* f, strptr sp, long* w, long* h) { ami_listboxsizg(f, (ami_strptr)sp, w, h); }
void listboxsizg(strptr sp, long* w, long* h) { ami_listboxsizg(stdout, (ami_strptr)sp, w, h); }
void listbox(FILE* f, long x1, long y1, long x2, long y2, strptr sp, long id) { ami_listbox(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void listbox(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_listbox(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void listboxg(FILE* f, long x1, long y1, long x2, long y2, strptr sp, long id) { ami_listboxg(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void listboxg(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_listboxg(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropboxsiz(FILE* f, strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropboxsiz(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropboxsiz(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropboxsizg(FILE* f, strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropboxsizg(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropboxsizg(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropbox(FILE* f, long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropbox(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropbox(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropbox(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropboxg(FILE* f, long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropboxg(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropboxg(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropboxg(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropeditboxsiz(FILE* f, strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropeditboxsiz(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropeditboxsiz(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsizg(FILE* f, strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropeditboxsizg(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropeditboxsizg(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditbox(FILE* f, long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropeditbox(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropeditbox(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropeditbox(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropeditboxg(FILE* f, long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropeditboxg(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropeditboxg(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropeditboxg(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void slidehorizsiz(FILE* f, long* w, long* h) { ami_slidehorizsiz(f, w, h); }
void slidehorizsiz(long* w, long* h) { ami_slidehorizsiz(stdout, w, h); }
void slidehorizsizg(FILE* f, long* w, long* h) { ami_slidehorizsizg(f, w, h); }
void slidehorizsizg(long* w, long* h) { ami_slidehorizsizg(stdout, w, h); }
void slidehoriz(FILE* f, long x1, long y1, long x2, long y2, long mark, long id) { ami_slidehoriz(f, x1, y1, x2, y2, mark, id); }
void slidehoriz(long x1, long y1, long x2, long y2, long mark, long id) { ami_slidehoriz(stdout, x1, y1, x2, y2, mark, id); }
void slidehorizg(FILE* f, long x1, long y1, long x2, long y2, long mark, long id) { ami_slidehorizg(f, x1, y1, x2, y2, mark, id); }
void slidehorizg(long x1, long y1, long x2, long y2, long mark, long id) { ami_slidehorizg(stdout, x1, y1, x2, y2, mark, id); }
void slidevertsiz(FILE* f, long* w, long* h) { ami_slidevertsiz(f, w, h); }
void slidevertsiz(long* w, long* h) { ami_slidevertsiz(stdout, w, h); }
void slidevertsizg(FILE* f, long* w, long* h) { ami_slidevertsizg(f, w, h); }
void slidevertsizg(long* w, long* h) { ami_slidevertsizg(stdout, w, h); }
void slidevert(FILE* f, long x1, long y1, long x2, long y2, long mark, long id) { ami_slidevert(f, x1, y1, x2, y2, mark, id); }
void slidevert(long x1, long y1, long x2, long y2, long mark, long id) { ami_slidevert(stdout, x1, y1, x2, y2, mark, id); }
void slidevertg(FILE* f, long x1, long y1, long x2, long y2, long mark, long id) { ami_slidevertg(f, x1, y1, x2, y2, mark, id); }
void slidevertg(long x1, long y1, long x2, long y2, long mark, long id) { ami_slidevertg(stdout, x1, y1, x2, y2, mark, id); }
void tabbarsiz(FILE* f, tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_tabbarsiz(f, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarsiz(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_tabbarsiz(stdout, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarsizg(FILE* f, tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_tabbarsizg(f, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarsizg(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_tabbarsizg(stdout, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarclient(FILE* f, tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy) { ami_tabbarclient(f, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbarclient(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy) { ami_tabbarclient(stdout, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbarclientg(FILE* f, tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy) { ami_tabbarclientg(f, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbarclientg(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy) { ami_tabbarclientg(stdout, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbar(FILE* f, long x1, long y1, long x2, long y2, strptr sp, tabori tor, long id) { ami_tabbar(f, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void tabbar(long x1, long y1, long x2, long y2, strptr sp, tabori tor, long id) { ami_tabbar(stdout, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void tabbarg(FILE* f, long x1, long y1, long x2, long y2, strptr sp, tabori tor, long id) { ami_tabbarg(f, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void tabbarg(long x1, long y1, long x2, long y2, strptr sp, tabori tor, long id) { ami_tabbarg(stdout, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void tabsel(FILE* f, long id, long tn) { ami_tabsel(f, id, tn); }
void tabsel(long id, long tn) { ami_tabsel(stdout, id, tn); }

/* dialogs */
void alert(char* title, char* message) { ami_alert(title, message); }
void querycolor(long* r, long* g, long* b) { ami_querycolor(r, g, b); }
void queryopen(char* s, long sl) { ami_queryopen(s, sl); }
void querysave(char* s, long sl) { ami_querysave(s, sl); }
void queryfind(char* s, long sl, qfnopts* opt) { ami_queryfind(s, sl, (ami_qfnopts*)opt); }
void queryfindrep(char* s, long sl, char* r, long rl, qfropts* opt) { ami_queryfindrep(s, sl, r, rl, (ami_qfropts*)opt); }
void queryfont(FILE* f, long* fc, long* s, long* fr, long* fg, long* fb, long* br,
               long* bg, long* bb, qfteffects* effect) { ami_queryfont(f, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }
void queryfont(long* fc, long* s, long* fr, long* fg, long* fb, long* br,
               long* bg, long* bb, qfteffects* effect) { ami_queryfont(stdout, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }

/* methods */
graph::graph(void)

{

    infile = stdin;
    outfile = stdout;
    graphoCB = this;
    eventsover(graphCB, &graphoeh);

}

/* text */
void graph::cursor(long x, long y) { ami_cursor(outfile, x, y); }
long  graph::maxx(void) { return ami_maxx(outfile); }
long  graph::maxy(void) { return ami_maxy(outfile); }
void graph::home(void) { ami_home(outfile); }
void graph::del(void) { ami_del(outfile); }
void graph::up(void) { ami_up(outfile); }
void graph::down(void) { ami_down(outfile); }
void graph::left(void) { ami_left(outfile); }
void graph::right(void) { ami_right(outfile); }
void graph::blink(long e) { ami_blink(outfile, e); }
void graph::reverse(long e) { ami_reverse(outfile, e); }
void graph::underline(long e) { ami_underline(outfile, e); }
void graph::superscript(long e) { ami_superscript(outfile, e); }
void graph::subscript(long e) { ami_subscript(outfile, e); }
void graph::italic(long e) { ami_italic(outfile, e); }
void graph::bold(long e) { ami_bold(outfile, e); }
void graph::strikeout(long e) { ami_strikeout(outfile, e); }
void graph::standout(long e) { ami_standout(outfile, e); }
void graph::fcolor(color c) { ami_fcolor(outfile, (ami_color)c); }
void graph::bcolor(color c) { ami_bcolor(outfile, (ami_color)c); }
void graph::autom(long e) { ami_auto(outfile, e); }
void graph::curvis(long e) { ami_curvis(outfile, e); }
void graph::scroll(long x, long y) { ami_scroll(outfile, x, y); }
long  graph::curx(void) { return ami_curx(outfile); }
long  graph::cury(void) { return ami_cury(outfile); }
long  graph::curbnd(void) { return ami_curbnd(outfile); }
void graph::select(long u, long d) { ami_select(outfile, u, d); }
void graph::event(evtrec* er) { ami_event(infile, (ami_evtptr)er); }
void graph::timer(long i, long t, long r) { ami_timer(outfile, i, t, r); }
void graph::killtimer(long i) { ami_killtimer(outfile, i); }
long  graph::mouse(void) { return ami_mouse(outfile); }
long  graph::mousebutton(long m) { return ami_mousebutton(outfile, m); }
long  graph::joystick(void) { return ami_joystick(outfile); }
long  graph::joybutton(long j) { return ami_joybutton(outfile, j); }
long  graph::joyaxis(long j) { return ami_joyaxis(outfile, j); }
void graph::settab(long t) { ami_settab(outfile, t); }
void graph::restab(long t) { ami_restab(outfile, t); }
void graph::clrtab(void) { ami_clrtab(outfile); }
long  graph::funkey(void) { return ami_funkey(outfile); }
void graph::frametimer(long e) { ami_frametimer(outfile, e); }
void graph::autohold(long e) { ami_autohold(e); }
void graph::wrtstr(char* s) { ami_wrtstr(outfile, s); }
void graph::wrtstrn(char* s, long n) { ami_wrtstrn(outfile, s, n); }
void graph::sizbuf(long x, long y) { ami_sizbuf(outfile, x, y); }
void graph::title(char* ts) { ami_title(outfile, ts); }
void graph::sendevent(evtrec* er) { ami_sendevent(outfile, (ami_evtptr)er); }

/* graphical */
long  graph::maxxg(void) { return ami_maxxg(outfile); }
long  graph::maxyg(void) { return ami_maxyg(outfile); }
long  graph::curxg(void) { return ami_curxg(outfile); }
long  graph::curyg(void) { return ami_curyg(outfile); }
void graph::line(long x1, long y1, long x2, long y2) { ami_line(outfile, x1, y1, x2, y2); }
void graph::linewidth(long w) { ami_linewidth(outfile, w); }
void graph::rect(long x1, long y1, long x2, long y2) { ami_rect(outfile, x1, y1, x2, y2); }
void graph::frect(long x1, long y1, long x2, long y2) { ami_frect(outfile, x1, y1, x2, y2); }
void graph::rrect(long x1, long y1, long x2, long y2, long xs, long ys) { ami_rrect(outfile, x1, y1, x2, y2, xs, ys); }
void graph::frrect(long x1, long y1, long x2, long y2, long xs, long ys) { ami_frrect(outfile, x1, y1, x2, y2, xs, ys); }
void graph::ellipse(long x1, long y1, long x2, long y2) { ami_ellipse(outfile, x1, y1, x2, y2); }
void graph::fellipse(long x1, long y1, long x2, long y2) { ami_fellipse(outfile, x1, y1, x2, y2); }
void graph::arc(long x1, long y1, long x2, long y2, long sa, long ea) { ami_arc(outfile, x1, y1, x2, y2, sa, ea); }
void graph::farc(long x1, long y1, long x2, long y2, long sa, long ea) { ami_farc(outfile, x1, y1, x2, y2, sa, ea); }
void graph::fchord(long x1, long y1, long x2, long y2, long sa, long ea) { ami_fchord(outfile, x1, y1, x2, y2, sa, ea); }
void graph::ftriangle(long x1, long y1, long x2, long y2, long x3, long y3) { ami_ftriangle(outfile, x1, y1, x2, y2, x3, y3); }
void graph::cursorg(long x, long y) { ami_cursorg(outfile, x, y); }
long  graph::baseline(void) { return ami_baseline(outfile); }
void graph::setpixel(long x, long y) { ami_setpixel(outfile, x, y); }
void graph::fover(void) { ami_fover(outfile); }
void graph::bover(void) { ami_bover(outfile); }
void graph::finvis(void) { ami_finvis(outfile); }
void graph::binvis(void) { ami_binvis(outfile); }
void graph::fxor(void) { ami_fxor(outfile); }
void graph::bxor(void) { ami_bxor(outfile); }
void graph::fand(void) { ami_fand(outfile); }
void graph::band(void) { ami_band(outfile); }
void graph::for_(void) { ami_for(outfile); }
void graph::bor(void) { ami_bor(outfile); }
long  graph::chrsizx(void) { return ami_chrsizx(outfile); }
long  graph::chrsizy(void) { return ami_chrsizy(outfile); }
long  graph::fonts(void) { return ami_fonts(outfile); }
void graph::font(long fc) { ami_font(outfile, fc); }
void graph::fontnam(long fc, char* fns, long fnsl) { ami_fontnam(outfile, fc, fns, fnsl); }
void graph::fontsiz(long s) { ami_fontsiz(outfile, s); }
void graph::chrspcy(long s) { ami_chrspcy(outfile, s); }
void graph::chrspcx(long s) { ami_chrspcx(outfile, s); }
long  graph::dpmx(void) { return ami_dpmx(outfile); }
long  graph::dpmy(void) { return ami_dpmy(outfile); }
long  graph::strsiz(const char* s) { return ami_strsiz(outfile, s); }
long  graph::chrpos(const char* s, long p) { return ami_chrpos(outfile, s, p); }
void graph::writejust(const char* s, long n) { ami_writejust(outfile, s, n); }
long  graph::justpos(const char* s, long p, long n) { return ami_justpos(outfile, s, p, n); }
void graph::condensed(long e) { ami_condensed(outfile, e); }
void graph::extended(long e) { ami_extended(outfile, e); }
void graph::xlight(long e) { ami_xlight(outfile, e); }
void graph::light(long e) { ami_light(outfile, e); }
void graph::xbold(long e) { ami_xbold(outfile, e); }
void graph::hollow(long e) { ami_hollow(outfile, e); }
void graph::raised(long e) { ami_raised(outfile, e); }
void graph::settabg(long t) { ami_settabg(outfile, t); }
void graph::restabg(long t) { ami_restabg(outfile, t); }
void graph::fcolorg(long r, long g, long b) { ami_fcolorg(outfile, r, g, b); }
void graph::fcolorc(long r, long g, long b) { ami_fcolorc(outfile, r, g, b); }
void graph::bcolorg(long r, long g, long b) { ami_bcolorg(outfile, r, g, b); }
void graph::bcolorc(long r, long g, long b) { ami_bcolorc(outfile, r, g, b); }
void graph::loadpict(long p, char* fn) { ami_loadpict(outfile, p, fn); }
long  graph::pictsizx(long p) { return ami_pictsizx(outfile, p); }
long  graph::pictsizy(long p) { return ami_pictsizy(outfile, p); }
void graph::picture(long p, long x1, long y1, long x2, long y2) { ami_picture(outfile, p, x1, y1, x2, y2); }
void graph::delpict(long p) { ami_delpict(outfile, p); }
void graph::scrollg(long x, long y) { ami_scrollg(outfile, x, y); }
void graph::path(long a) { ami_path(outfile, a); }

/* window management */
void graph::buffer(long e) { ami_buffer(outfile, e); }
void graph::sizbufg(long x, long y) { ami_sizbufg(outfile, x, y); }
void graph::getsiz(long* x, long* y) { ami_getsiz(outfile, x, y); }
void graph::getsizg(long* x, long* y) { ami_getsizg(outfile, x, y); }
void graph::setsiz(long x, long y) { ami_setsiz(outfile, x, y); }
void graph::setsizg(long x, long y) { ami_setsizg(outfile, x, y); }
void graph::setpos(long x, long y) { ami_setpos(outfile, x, y); }
void graph::setposg(long x, long y) { ami_setposg(outfile, x, y); }
void graph::scnsiz(long* x, long* y) { ami_scnsiz(outfile, x, y); }
void graph::scnsizg(long* x, long* y) { ami_scnsizg(outfile, x, y); }
void graph::scncen(long* x, long* y) { ami_scncen(outfile, x, y); }
void graph::scnceng(long* x, long* y) { ami_scnceng(outfile, x, y); }
void graph::winclient(long cx, long cy, long* wx, long* wy, winmodset ms) { ami_winclient(outfile, cx, cy, wx, wy, (ami_winmodset)ms); }
void graph::winclientg(long cx, long cy, long* wx, long* wy, winmodset ms) { ami_winclientg(outfile, cx, cy, wx, wy, (ami_winmodset)ms); }
void graph::front(void) { ami_front(outfile); }
void graph::back(void) { ami_back(outfile); }
void graph::frame(long e) { ami_frame(outfile, e); }
void graph::sizable(long e) { ami_sizable(outfile, e); }
void graph::sysbar(long e) { ami_sysbar(outfile, e); }
void graph::menu(menuptr m) { ami_menu(outfile, (ami_menuptr)m); }
void graph::menuena(long id, long onoff) { ami_menuena(outfile, id, onoff); }
void graph::menusel(long id, long select) { ami_menusel(outfile, id, select); }
void graph::focus(void) { ami_focus(outfile); }

/* widgets */
long  graph::getwigid(void) { return ami_getwigid(outfile); }
void graph::killwidget(long id) { ami_killwidget(outfile, id); }
void graph::selectwidget(long id, long e) { ami_selectwidget(outfile, id, e); }
void graph::enablewidget(long id, long e) { ami_enablewidget(outfile, id, e); }
void graph::getwidgettext(long id, char* s, long sl) { ami_getwidgettext(outfile, id, s, sl); }
void graph::putwidgettext(long id, char* s) { ami_putwidgettext(outfile, id, s); }
void graph::sizwidget(long id, long x, long y) { ami_sizwidget(outfile, id, x, y); }
void graph::sizwidgetg(long id, long x, long y) { ami_sizwidgetg(outfile, id, x, y); }
void graph::poswidget(long id, long x, long y) { ami_poswidget(outfile, id, x, y); }
void graph::poswidgetg(long id, long x, long y) { ami_poswidgetg(outfile, id, x, y); }
void graph::backwidget(long id) { ami_backwidget(outfile, id); }
void graph::frontwidget(long id) { ami_frontwidget(outfile, id); }
void graph::focuswidget(long id) { ami_focuswidget(outfile, id); }
void graph::buttonsiz(char* s, long* w, long* h) { ami_buttonsiz(outfile, s, w, h); }
void graph::buttonsizg(char* s, long* w, long* h) { ami_buttonsizg(outfile, s, w, h); }
void graph::button(long x1, long y1, long x2, long y2, char* s, long id) { ami_button(outfile, x1, y1, x2, y2, s, id); }
void graph::buttong(long x1, long y1, long x2, long y2, char* s, long id) { ami_buttong(outfile, x1, y1, x2, y2, s, id); }
void graph::checkboxsiz(char* s, long* w, long* h) { ami_checkboxsiz(outfile, s, w, h); }
void graph::checkboxsizg(char* s, long* w, long* h) { ami_checkboxsizg(outfile, s, w, h); }
void graph::checkbox(long x1, long y1, long x2, long y2, char* s, long id) { ami_checkbox(outfile, x1, y1, x2, y2, s, id); }
void graph::checkboxg(long x1, long y1, long x2, long y2, char* s, long id) { ami_checkboxg(outfile, x1, y1, x2, y2, s, id); }
void graph::radiobuttonsiz(char* s, long* w, long* h) { ami_radiobuttonsiz(outfile, s, w, h); }
void graph::radiobuttonsizg(char* s, long* w, long* h) { ami_radiobuttonsizg(outfile, s, w, h); }
void graph::radiobutton(long x1, long y1, long x2, long y2, char* s, long id) { ami_radiobutton(outfile, x1, y1, x2, y2, s, id); }
void graph::radiobuttong(long x1, long y1, long x2, long y2, char* s, long id) { ami_radiobuttong(outfile, x1, y1, x2, y2, s, id); }
void graph::groupsiz(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_groupsiz(outfile, s, cw, ch, w, h, ox, oy); }
void graph::groupsizg(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_groupsizg(outfile, s, cw, ch, w, h, ox, oy); }
void graph::group(long x1, long y1, long x2, long y2, char* s, long id) { ami_group(outfile, x1, y1, x2, y2, s, id); }
void graph::groupg(long x1, long y1, long x2, long y2, char* s, long id) { ami_groupg(outfile, x1, y1, x2, y2, s, id); }
void graph::background(long x1, long y1, long x2, long y2, long id) { ami_background(outfile, x1, y1, x2, y2, id); }
void graph::backgroundg(long x1, long y1, long x2, long y2, long id) { ami_backgroundg(outfile, x1, y1, x2, y2, id); }
void graph::scrollvertsiz(long* w, long* h) { ami_scrollvertsiz(outfile, w, h); }
void graph::scrollvertsizg(long* w, long* h) { ami_scrollvertsizg(outfile, w, h); }
void graph::scrollvert(long x1, long y1, long x2, long y2, long id) { ami_scrollvert(outfile, x1, y1, x2, y2, id); }
void graph::scrollvertg(long x1, long y1, long x2, long y2, long id) { ami_scrollvertg(outfile, x1, y1, x2, y2, id); }
void graph::scrollhorizsiz(long* w, long* h) { ami_scrollhorizsiz(outfile, w, h); }
void graph::scrollhorizsizg(long* w, long* h) { ami_scrollhorizsizg(outfile, w, h); }
void graph::scrollhoriz(long x1, long y1, long x2, long y2, long id) { ami_scrollhoriz(outfile, x1, y1, x2, y2, id); }
void graph::scrollhorizg(long x1, long y1, long x2, long y2, long id) { ami_scrollhorizg(outfile, x1, y1, x2, y2, id); }
void graph::scrollpos(long id, long r) { ami_scrollpos(outfile, id, r); }
void graph::scrollsiz(long id, long r) { ami_scrollsiz(outfile, id, r); }
void graph::numselboxsiz(long l, long u, long* w, long* h) { ami_numselboxsiz(outfile, l, u, w, h); }
void graph::numselboxsizg(long l, long u, long* w, long* h) { ami_numselboxsizg(outfile, l, u, w, h); }
void graph::numselbox(long x1, long y1, long x2, long y2, long l, long u, long id) { ami_numselbox(outfile, x1, y1, x2, y2, l, u, id); }
void graph::numselboxg(long x1, long y1, long x2, long y2, long l, long u, long id) { ami_numselboxg(outfile, x1, y1, x2, y2, l, u, id); }
void graph::editboxsiz(char* s, long* w, long* h) { ami_editboxsiz(outfile, s, w, h); }
void graph::editboxsizg(char* s, long* w, long* h) { ami_editboxsizg(outfile, s, w, h); }
void graph::editbox(long x1, long y1, long x2, long y2, long id) { ami_editbox(outfile, x1, y1, x2, y2, id); }
void graph::editboxg(long x1, long y1, long x2, long y2, long id) { ami_editboxg(outfile, x1, y1, x2, y2, id); }
void graph::progbarsiz(long* w, long* h) { ami_progbarsiz(outfile, w, h); }
void graph::progbarsizg(long* w, long* h) { ami_progbarsizg(outfile, w, h); }
void graph::progbar(long x1, long y1, long x2, long y2, long id) { ami_progbar(outfile, x1, y1, x2, y2, id); }
void graph::progbarg(long x1, long y1, long x2, long y2, long id) { ami_progbarg(outfile, x1, y1, x2, y2, id); }
void graph::progbarpos(long id, long pos) { ami_progbarpos(outfile, id, pos); }
void graph::listboxsiz(strptr sp, long* w, long* h) { ami_listboxsiz(outfile, (ami_strptr)sp, w, h); }
void graph::listboxsizg(strptr sp, long* w, long* h) { ami_listboxsizg(outfile, (ami_strptr)sp, w, h); }
void graph::listbox(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_listbox(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::listboxg(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_listboxg(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropboxsiz(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropboxsizg(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropbox(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropbox(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropboxg(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropboxg(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropeditboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropeditboxsiz(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropeditboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh) { ami_dropeditboxsizg(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropeditbox(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropeditbox(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropeditboxg(long x1, long y1, long x2, long y2, strptr sp, long id) { ami_dropeditboxg(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::slidehorizsiz(long* w, long* h) { ami_slidehorizsiz(outfile, w, h); }
void graph::slidehorizsizg(long* w, long* h) { ami_slidehorizsizg(outfile, w, h); }
void graph::slidehoriz(long x1, long y1, long x2, long y2, long mark, long id) { ami_slidehoriz(outfile, x1, y1, x2, y2, mark, id); }
void graph::slidehorizg(long x1, long y1, long x2, long y2, long mark, long id) { ami_slidehorizg(outfile, x1, y1, x2, y2, mark, id); }
void graph::slidevertsiz(long* w, long* h) { ami_slidevertsiz(outfile, w, h); }
void graph::slidevertsizg(long* w, long* h) { ami_slidevertsizg(outfile, w, h); }
void graph::slidevert(long x1, long y1, long x2, long y2, long mark, long id) { ami_slidevert(outfile, x1, y1, x2, y2, mark, id); }
void graph::slidevertg(long x1, long y1, long x2, long y2, long mark, long id) { ami_slidevertg(outfile, x1, y1, x2, y2, mark, id); }
void graph::tabbarsiz(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_tabbarsiz(outfile, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void graph::tabbarsizg(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy) { ami_tabbarsizg(outfile, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void graph::tabbarclient(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy) { ami_tabbarclient(outfile, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void graph::tabbarclientg(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy) { ami_tabbarclientg(outfile, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void graph::tabbar(long x1, long y1, long x2, long y2, strptr sp, tabori tor, long id) { ami_tabbar(outfile, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void graph::tabbarg(long x1, long y1, long x2, long y2, strptr sp, tabori tor, long id) { ami_tabbarg(outfile, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void graph::tabsel(long id, long tn) { ami_tabsel(outfile, id, tn); }

/* dialogs */
void graph::queryfont(long* fc, long* s, long* fr, long* fg, long* fb, long* br,
                      long* bg, long* bb, qfteffects* effect) { ami_queryfont(outfile, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }

/* virtual callbacks */
long graph::evchar(char c) { return 0; }
long graph::evup(void) { return 0; }
long graph::evdown(void) { return 0; }
long graph::evleft(void) { return 0; }
long graph::evright(void) { return 0; }
long graph::evleftw(void) { return 0; }
long graph::evrightw(void) { return 0; }
long graph::evhome(void) { return 0; }
long graph::evhomes(void) { return 0; }
long graph::evhomel(void) { return 0; }
long graph::evend(void) { return 0; }
long graph::evends(void) { return 0; }
long graph::evendl(void) { return 0; }
long graph::evscrl(void) { return 0; }
long graph::evscrr(void) { return 0; }
long graph::evscru(void) { return 0; }
long graph::evscrd(void) { return 0; }
long graph::evpagd(void) { return 0; }
long graph::evpagu(void) { return 0; }
long graph::evtab(void) { return 0; }
long graph::eventer(void) { return 0; }
long graph::evinsert(void) { return 0; }
long graph::evinsertl(void) { return 0; }
long graph::evinsertt(void) { return 0; }
long graph::evdel(void) { return 0; }
long graph::evdell(void) { return 0; }
long graph::evdelcf(void) { return 0; }
long graph::evdelcb(void) { return 0; }
long graph::evcopy(void) { return 0; }
long graph::evcopyl(void) { return 0; }
long graph::evcan(void) { return 0; }
long graph::evstop(void) { return 0; }
long graph::evcont(void) { return 0; }
long graph::evprint(void) { return 0; }
long graph::evprintb(void) { return 0; }
long graph::evprints(void) { return 0; }
long graph::evfun(long k) { return 0; }
long graph::evmenu(void) { return 0; }
long graph::evmouba(long m, long b) { return 0; }
long graph::evmoubd(long m, long b) { return 0; }
long graph::evmoumov(long m, long x, long y) { return 0; }
long graph::evtim(long t) { return 0; }
long graph::evjoyba(long j, long b) { return 0; }
long graph::evjoybd(long j, long b) { return 0; }
long graph::evjoymov(long j, long x, long y, long z) { return 0; }
long graph::evresize(void) { return 0; }
long graph::evfocus(void) { return 0; }
long graph::evnofocus(void) { return 0; }
long graph::evhover(void) { return 0; }
long graph::evnohover(void) { return 0; }
long graph::evterm(void) { return 0; }
long graph::evframe(void) { return 0; }
long graph::evmoumovg(long m, long x, long y) { return 0; }
long graph::evredraw(long x1, long y1, long x2, long y2) { return 0; }
long graph::evmin(void) { return 0; }
long graph::evmax(void) { return 0; }
long graph::evnorm(void) { return 0; }
long graph::evmenus(long id) { return 0; }
long graph::evbutton(long id) { return 0; }
long graph::evchkbox(long id) { return 0; }
long graph::evradbut(long id) { return 0; }
long graph::evsclull(long id) { return 0; }
long graph::evscldrl(long id) { return 0; }
long graph::evsclulp(long id) { return 0; }
long graph::evscldrp(long id) { return 0; }
long graph::evsclpos(long id, long pos) { return 0; }
long graph::evedtbox(long id) { return 0; }
long graph::evnumbox(long id, long val) { return 0; }
long graph::evlstbox(long id, long sel) { return 0; }
long graph::evdrpbox(long id, long sel) { return 0; }
long graph::evdrebox(long id) { return 0; }
long graph::evsldpos(long id, long pos) { return 0; }
long graph::evtabbar(long id, long sel) { return 0; }

void graph::graphCB(evtrec* er)

{

    long handled;

    switch (er->etype) {

        case etchar:    handled = graphoCB->evchar(er->echar); break;
        case etup:      handled = graphoCB->evup(); break;
        case etdown:    handled = graphoCB->evdown(); break;
        case etleft:    handled = graphoCB->evleft(); break;
        case etright:   handled = graphoCB->evright(); break;
        case etleftw:   handled = graphoCB->evleftw(); break;
        case etrightw:  handled = graphoCB->evrightw(); break;
        case ethome:    handled = graphoCB->evhome(); break;
        case ethomes:   handled = graphoCB->evhomes(); break;
        case ethomel:   handled = graphoCB->evhomel(); break;
        case etend:     handled = graphoCB->evend(); break;
        case etends:    handled = graphoCB->evends(); break;
        case etendl:    handled = graphoCB->evendl(); break;
        case etscrl:    handled = graphoCB->evscrl(); break;
        case etscrr:    handled = graphoCB->evscrr(); break;
        case etscru:    handled = graphoCB->evscru(); break;
        case etscrd:    handled = graphoCB->evscrd(); break;
        case etpagd:    handled = graphoCB->evpagd(); break;
        case etpagu:    handled = graphoCB->evpagu(); break;
        case ettab:     handled = graphoCB->evtab(); break;
        case etenter:   handled = graphoCB->eventer(); break;
        case etinsert:  handled = graphoCB->evinsert(); break;
        case etinsertl: handled = graphoCB->evinsertl(); break;
        case etinsertt: handled = graphoCB->evinsertt(); break;
        case etdel:     handled = graphoCB->evdel(); break;
        case etdell:    handled = graphoCB->evdell(); break;
        case etdelcf:   handled = graphoCB->evdelcf(); break;
        case etdelcb:   handled = graphoCB->evdelcb(); break;
        case etcopy:    handled = graphoCB->evcopy(); break;
        case etcopyl:   handled = graphoCB->evcopyl(); break;
        case etcan:     handled = graphoCB->evcan(); break;
        case etstop:    handled = graphoCB->evstop(); break;
        case etcont:    handled = graphoCB->evcont(); break;
        case etprint:   handled = graphoCB->evprint(); break;
        case etprintb:  handled = graphoCB->evprintb(); break;
        case etprints:  handled = graphoCB->evprints(); break;
        case etfun:     handled = graphoCB->evfun(er->fkey); break;
        case etmenu:    handled = graphoCB->evmenu(); break;
        case etmouba:   handled = graphoCB->evmouba(er->amoun, er->amoubn);
            break;
        case etmoubd:   handled = graphoCB->evmoubd(er->dmoun, er->dmoubn);
            break;
        case etmoumov:
            handled = graphoCB->evmoumov(er->mmoun, er->moupx, er->moupy);
            break;
        case ettim:     handled = graphoCB->evtim(er->timnum); break;
        case etjoyba:   handled = graphoCB->evjoyba(er->ajoyn, er->ajoybn);
            break;
        case etjoybd:   handled = graphoCB->evjoybd(er->djoyn, er->djoybn);
            break;
        case etjoymov:
            handled = graphoCB->evjoymov(er->mjoyn, er->joypx, er->joypy,
                                         er->joypz);
            break;
        case etresize:  handled = graphoCB->evresize(); break;
        case etfocus:   handled = graphoCB->evfocus(); break;
        case etnofocus: handled = graphoCB->evnofocus(); break;
        case ethover:   handled = graphoCB->evhover(); break;
        case etnohover: handled = graphoCB->evnohover(); break;
        case etterm:    handled = graphoCB->evterm(); break;
        case etframe:   handled = graphoCB->evframe(); break;
        case etmoumovg:
            handled = graphoCB->evmoumovg(er->mmoung, er->moupxg, er->moupyg);
            break;
        case etredraw:  handled = graphoCB->evredraw(er->rsx, er->rsy, er->rex, er->rey); break;
        case etmin:     handled = graphoCB->evmin(); break;
        case etmax:     handled = graphoCB->evmax(); break;
        case etnorm:    handled = graphoCB->evnorm(); break;
        case etmenus:   handled = graphoCB->evmenus(er->menuid); break;
        case etbutton:  handled = graphoCB->evbutton(er->butid); break;
        case etchkbox:  handled = graphoCB->evchkbox(er->ckbxid); break;
        case etradbut:  handled = graphoCB->evradbut(er->radbid); break;
        case etsclull:  handled = graphoCB->evsclull(er->sclulid); break;
        case etscldrl:  handled = graphoCB->evscldrl(er->scldrid); break;
        case etsclulp:  handled = graphoCB->evsclulp(er->sclupid); break;
        case etscldrp:  handled = graphoCB->evscldrp(er->scldpid); break;
        case etsclpos:  handled = graphoCB->evsclpos(er->sclpid, er->sclpos); break;
        case etedtbox:  handled = graphoCB->evedtbox(er->edtbid); break;
        case etnumbox:  handled = graphoCB->evnumbox(er->numbid, er->numbsl); break;
        case etlstbox:  handled = graphoCB->evlstbox(er->lstbid, er->lstbsl); break;
        case etdrpbox:  handled = graphoCB->evdrpbox(er->drpbid, er->drpbsl); break;
        case etdrebox:  handled = graphoCB->evdrebox(er->drebid); break;
        case etsldpos:  handled = graphoCB->evsldpos(er->sldpid, er->sldpos); break;
        case ettabbar:  handled = graphoCB->evtabbar(er->tabid, er->tabsel); break;
        default: handled = 0; break;

    }

    er->handled = handled;
    if (!handled) (*graphoeh)(er);

}

} /* namespace graphics */
