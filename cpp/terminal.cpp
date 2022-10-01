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
 * 4. Instead of registering callbacks in C, the term object features virtual
 * functions for each event than can be individually overriden.
 *
 * Terminal has two distinct types of interfaces, the procedural and the object/
 * class interfaces. The procedural interface expects the specification of
 * what terminal surface we are talking to to be the first parameter of all
 * procedures and functions (even if defaulted to stdin or stdout). The object/
 * class interface keeps that as part of the object.
 *
 * Since the terminal, just as the graphics interface, only specifies the
 * default interface (usually specified by stdin/stdout), the object/class
 * interface does not get interesting until multiple screens/windows are used.
 * This is a consequence of the upward compatible model.
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

/* hook for sending events back to methods */
term* termoCB;
pevthan termoeh;

/* procedures and functions */
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
void fcolor(FILE* f, color c) { pa_fcolor(f, (pa_color)c); }
void fcolor(color c) { pa_fcolor(stdout, (pa_color)c); }
void bcolor(FILE* f, color c) { pa_bcolor(f, (pa_color)c); }
void bcolor(color c) { pa_bcolor(stdout, (pa_color)c); }
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
void event(FILE* f, evtrec* er) { pa_event(f, (pa_evtptr)er); }
void event(evtrec* er) { pa_event(stdin, (pa_evtptr)er); }
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
void autohold(int e) { pa_autohold(e); }
void wrtstr(FILE* f, char *s) { pa_wrtstr(f, s); }
void wrtstr(char *s) { pa_wrtstr(stdout, s); }
void wrtstr(FILE* f, char *s, int n) { pa_wrtstrn(f, s, n); }
void wrtstr(char *s, int n) { pa_wrtstrn(stdout, s, n); }
void wrtstrn(FILE* f, char *s, int n) { pa_wrtstrn(f, s, n); }
void wrtstrn(char *s, int n) { pa_wrtstrn(stdout, s, n); }
void sizbuf(FILE* f, int x, int y) { pa_sizbuf(f, x, y); }
void sizbuf(int x, int y) { pa_sizbuf(stdout, x, y); }
void eventover(evtcod e, pevthan eh, pevthan* oeh) { pa_eventover((pa_evtcod)e, (pa_pevthan)eh, (pa_pevthan*)oeh); }
void eventsover(pevthan eh, pevthan* oeh) { pa_eventsover((pa_pevthan)eh, (pa_pevthan*)oeh); }

/* methods */
term::term(void)

{

    infile = stdin;
    outfile = stdout;
    termoCB = this;
    eventsover(termCB, &termoeh);

}

void term::cursor(int x, int y) { pa_cursor(outfile, x, y); }
int  term::maxx(void) { return pa_maxx(outfile); }
int  term::maxy(void) { return pa_maxy(outfile); }
void term::home(void) { pa_home(outfile); }
void term::del(void) { pa_del(outfile); }
void term::up(void) { pa_up(outfile); }
void term::down(void) { pa_down(outfile); }
void term::left(void) { pa_left(outfile); }
void term::right(void) { pa_right(outfile); }
void term::blink(int e) { pa_blink(outfile, e); }
void term::reverse(int e) { pa_reverse(outfile, e); }
void term::underline(int e) { pa_underline(outfile, e); }
void term::superscript(int e) { pa_superscript(outfile, e); }
void term::subscript(int e) { pa_subscript(outfile, e); }
void term::italic(int e) { pa_italic(outfile, e); }
void term::bold(int e) { pa_bold(outfile, e); }
void term::strikeout(int e) { pa_strikeout(outfile, e); }
void term::standout(int e) { pa_standout(outfile, e); }
void term::fcolor(color c) { pa_fcolor(outfile, (pa_color)c); }
void term::bcolor(color c) { pa_bcolor(outfile, (pa_color)c); }
void term::autom(int e) { pa_auto(outfile, e); }
void term::curvis(int e) { pa_curvis(outfile, e); }
void term::scroll(int x, int y) { pa_scroll(outfile, x, y); }
int  term::curx(void) { return pa_curx(outfile); }
int  term::cury(void) { return pa_cury(outfile); }
int  term::curbnd(void) { return pa_curbnd(outfile); }
void term::select(int u, int d) { pa_select(outfile, u, d); }
void term::event(evtrec* er) { pa_event(infile, (pa_evtptr)er); }
void term::timer(int i, int t, int r) { pa_timer(outfile, i, t, r); }
void term::killtimer(int i) { pa_killtimer(outfile, i); }
int  term::mouse(void) { return pa_mouse(outfile); }
int  term::mousebutton(int m) { return pa_mousebutton(outfile, m); }
int  term::joystick(void) { return pa_joystick(outfile); }
int  term::joybutton(int j) { return pa_joybutton(outfile, j); }
int  term::joyaxis(int j) { return pa_joyaxis(outfile, j); }
void term::settab(int t) { pa_settab(outfile, t); }
void term::restab(int t) { pa_restab(outfile, t); }
void term::clrtab(void) { pa_clrtab(outfile); }
int  term::funkey(void) { return pa_funkey(outfile); }
void term::frametimer(int e) { pa_frametimer(outfile, e); }
void term::autohold(int e) { pa_autohold(e); }
void term::wrtstr(char *s) { pa_wrtstr(outfile, s); }
void term::wrtstr(char *s, int n) { pa_wrtstrn(outfile, s, n); }
void term::wrtstrn(char *s, int n) { pa_wrtstrn(outfile, s, n); }
void term::sizbuf(int x, int y) { pa_sizbuf(outfile, x, y); }

/* virtual callbacks */
int term::evchar(char c) { return 0; }
int term::evup(void) { return 0; }
int term::evdown(void) { return 0; }
int term::evleft(void) { return 0; }
int term::evright(void) { return 0; }
int term::evleftw(void) { return 0; }
int term::evrightw(void) { return 0; }
int term::evhome(void) { return 0; }
int term::evhomes(void) { return 0; }
int term::evhomel(void) { return 0; }
int term::evend(void) { return 0; }
int term::evends(void) { return 0; }
int term::evendl(void) { return 0; }
int term::evscrl(void) { return 0; }
int term::evscrr(void) { return 0; }
int term::evscru(void) { return 0; }
int term::evscrd(void) { return 0; }
int term::evpagd(void) { return 0; }
int term::evpagu(void) { return 0; }
int term::evtab(void) { return 0; }
int term::eventer(void) { return 0; }
int term::evinsert(void) { return 0; }
int term::evinsertl(void) { return 0; }
int term::evinsertt(void) { return 0; }
int term::evdel(void) { return 0; }
int term::evdell(void) { return 0; }
int term::evdelcf(void) { return 0; }
int term::evdelcb(void) { return 0; }
int term::evcopy(void) { return 0; }
int term::evcopyl(void) { return 0; }
int term::evcan(void) { return 0; }
int term::evstop(void) { return 0; }
int term::evcont(void) { return 0; }
int term::evprint(void) { return 0; }
int term::evprintb(void) { return 0; }
int term::evprints(void) { return 0; }
int term::evfun(int k) { return 0; }
int term::evmenu(void) { return 0; }
int term::evmouba(int m, int b) { return 0; }
int term::evmoubd(int m, int b) { return 0; }
int term::evmoumov(int m, int x, int y) { return 0; }
int term::evtim(int t) { return 0; }
int term::evjoyba(int j, int b) { return 0; }
int term::evjoybd(int j, int b) { return 0; }
int term::evjoymov(int j, int x, int y, int z) { return 0; }
int term::evresize(void) { return 0; }
int term::evfocus(void) { return 0; }
int term::evnofocus(void) { return 0; }
int term::evhover(void) { return 0; }
int term::evnohover(void) { return 0; }
int term::evterm(void) { return 0; }

void term::termCB(evtrec* er)

{

    int handled;

    switch (er->etype) {

        case etchar:    handled = termoCB->evchar(er->echar); break;
        case etup:      handled = termoCB->evup(); break;
        case etdown:    handled = termoCB->evdown(); break;
        case etleft:    handled = termoCB->evleft(); break;
        case etright:   handled = termoCB->evright(); break;
        case etleftw:   handled = termoCB->evleftw(); break;
        case etrightw:  handled = termoCB->evrightw(); break;
        case ethome:    handled = termoCB->evhome(); break;
        case ethomes:   handled = termoCB->evhomes(); break;
        case ethomel:   handled = termoCB->evhomel(); break;
        case etend:     handled = termoCB->evend(); break;
        case etends:    handled = termoCB->evends(); break;
        case etendl:    handled = termoCB->evendl(); break;
        case etscrl:    handled = termoCB->evscrl(); break;
        case etscrr:    handled = termoCB->evscrr(); break;
        case etscru:    handled = termoCB->evscru(); break;
        case etscrd:    handled = termoCB->evscrd(); break;
        case etpagd:    handled = termoCB->evpagd(); break;
        case etpagu:    handled = termoCB->evpagu(); break;
        case ettab:     handled = termoCB->evtab(); break;
        case etenter:   handled = termoCB->eventer(); break;
        case etinsert:  handled = termoCB->evinsert(); break;
        case etinsertl: handled = termoCB->evinsertl(); break;
        case etinsertt: handled = termoCB->evinsertt(); break;
        case etdel:     handled = termoCB->evdel(); break;
        case etdell:    handled = termoCB->evdell(); break;
        case etdelcf:   handled = termoCB->evdelcf(); break;
        case etdelcb:   handled = termoCB->evdelcb(); break;
        case etcopy:    handled = termoCB->evcopy(); break;
        case etcopyl:   handled = termoCB->evcopyl(); break;
        case etcan:     handled = termoCB->evcan(); break;
        case etstop:    handled = termoCB->evstop(); break;
        case etcont:    handled = termoCB->evcont(); break;
        case etprint:   handled = termoCB->evprint(); break;
        case etprintb:  handled = termoCB->evprintb(); break;
        case etprints:  handled = termoCB->evprints(); break;
        case etfun:     handled = termoCB->evfun(er->fkey); break;
        case etmenu:    handled = termoCB->evmenu(); break;
        case etmouba:   handled = termoCB->evmouba(er->amoun, er->amoubn);
            break;
        case etmoubd:   handled = termoCB->evmoubd(er->dmoun, er->dmoubn);
            break;
        case etmoumov:
            handled = termoCB->evmoumov(er->mmoun, er->moupx, er->moupy);
            break;
        case ettim:     handled = termoCB->evtim(er->timnum); break;
        case etjoyba:   handled = termoCB->evjoyba(er->ajoyn, er->ajoybn);
            break;
        case etjoybd:   handled = termoCB->evjoybd(er->djoyn, er->djoybn);
            break;
        case etjoymov:
            handled = termoCB->evjoymov(er->mjoyn, er->joypx, er->joypy,
                                        er->joypz);
            break;
        case etresize:  handled = termoCB->evresize(); break;
        case etfocus:   handled = termoCB->evfocus(); break;
        case etnofocus: handled = termoCB->evnofocus(); break;
        case ethover:   handled = termoCB->evhover(); break;
        case etnohover: handled = termoCB->evnohover(); break;
        case etterm:    handled = termoCB->evterm(); break;

    }

    if (!handled) termoeh(er);

}

} /* namespace terminal */
