/** ****************************************************************************
 *
 * Terminal library interface C++ wrapper
 *
 * Wraps the calls in terminal with C++ conventions. This brings several
 * advantages over C code:
 *
 * 1. The functions and other defintions do not need a "ami_" prefix, but rather
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
#include <stdlib.h>

#include <terminal.h>

}

#include "terminal.hpp"

namespace terminal {

/* hook for sending events back to methods */
term* termoCB;
pevthan termoeh;

/* procedures and functions */
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
void select(FILE *f, long u, long d) { ami_select(f, u, d); }
void select(long u, long d) { ami_select(stdout, u, d); }
void event(FILE* f, evtrec* er) { ami_event(f, (ami_evtptr)er); }
void event(evtrec* er) { ami_event(stdin, (ami_evtptr)er); }
void timer(FILE* f, long i, long t, long r) { ami_timer(f, i, t, r); }
void timer(long i, long t, long r) { ami_timer(stdout, i, t, r); }
void killtimer(FILE* f, long i) { ami_killtimer(f, i); }
void killtimer(long i) { ami_killtimer(stdout, i); }
long  mouse(FILE *f) { return ami_mouse(f); }
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
void wrtstr(FILE* f, char *s) { ami_wrtstr(f, s); }
void wrtstr(char *s) { ami_wrtstr(stdout, s); }
void wrtstr(FILE* f, char *s, long n) { ami_wrtstrn(f, s, n); }
void wrtstr(char *s, long n) { ami_wrtstrn(stdout, s, n); }
void wrtstrn(FILE* f, char *s, long n) { ami_wrtstrn(f, s, n); }
void wrtstrn(char *s, long n) { ami_wrtstrn(stdout, s, n); }
void sizbuf(FILE* f, long x, long y) { ami_sizbuf(f, x, y); }
void sizbuf(long x, long y) { ami_sizbuf(stdout, x, y); }
void eventover(evtcod e, pevthan eh, pevthan* oeh) { ami_eventover((ami_evtcod)e, (ami_pevthan)eh, (ami_pevthan*)oeh); }
void eventsover(pevthan eh, pevthan* oeh) { ami_eventsover((ami_pevthan)eh, (ami_pevthan*)oeh); }

/* methods */
term::term(void)

{

    /* One term object at a time. The events come back through one
       global hook: a second object would not share the events with the
       first, it would take them all -- and the second hooking of the
       chain saves the hook itself as the handler to pass unhandled
       events to, which is a loop with no bottom. Refusing is the only
       honest answer the wrapper has. */
    if (termoCB) {

        fprintf(stderr, "terminal: only one term object may exist\n");
        exit(1);

    }
    infile = stdin;
    outfile = stdout;
    termoCB = this;
    eventsover(termCB, &termoeh);

}

term::~term(void)

{

    pevthan tmp;

    /* put the chain back the way it was found, so no event is ever
       delivered to an object that no longer exists */
    eventsover(termoeh, &tmp);
    termoCB = 0;

}

void term::cursor(long x, long y) { ami_cursor(outfile, x, y); }
long  term::maxx(void) { return ami_maxx(outfile); }
long  term::maxy(void) { return ami_maxy(outfile); }
void term::home(void) { ami_home(outfile); }
void term::del(void) { ami_del(outfile); }
void term::up(void) { ami_up(outfile); }
void term::down(void) { ami_down(outfile); }
void term::left(void) { ami_left(outfile); }
void term::right(void) { ami_right(outfile); }
void term::blink(long e) { ami_blink(outfile, e); }
void term::reverse(long e) { ami_reverse(outfile, e); }
void term::underline(long e) { ami_underline(outfile, e); }
void term::superscript(long e) { ami_superscript(outfile, e); }
void term::subscript(long e) { ami_subscript(outfile, e); }
void term::italic(long e) { ami_italic(outfile, e); }
void term::bold(long e) { ami_bold(outfile, e); }
void term::strikeout(long e) { ami_strikeout(outfile, e); }
void term::standout(long e) { ami_standout(outfile, e); }
void term::fcolor(color c) { ami_fcolor(outfile, (ami_color)c); }
void term::bcolor(color c) { ami_bcolor(outfile, (ami_color)c); }
void term::autom(long e) { ami_auto(outfile, e); }
void term::curvis(long e) { ami_curvis(outfile, e); }
void term::scroll(long x, long y) { ami_scroll(outfile, x, y); }
long  term::curx(void) { return ami_curx(outfile); }
long  term::cury(void) { return ami_cury(outfile); }
long  term::curbnd(void) { return ami_curbnd(outfile); }
void term::select(long u, long d) { ami_select(outfile, u, d); }
void term::event(evtrec* er) { ami_event(infile, (ami_evtptr)er); }
void term::timer(long i, long t, long r) { ami_timer(outfile, i, t, r); }
void term::killtimer(long i) { ami_killtimer(outfile, i); }
long  term::mouse(void) { return ami_mouse(outfile); }
long  term::mousebutton(long m) { return ami_mousebutton(outfile, m); }
long  term::joystick(void) { return ami_joystick(outfile); }
long  term::joybutton(long j) { return ami_joybutton(outfile, j); }
long  term::joyaxis(long j) { return ami_joyaxis(outfile, j); }
void term::settab(long t) { ami_settab(outfile, t); }
void term::restab(long t) { ami_restab(outfile, t); }
void term::clrtab(void) { ami_clrtab(outfile); }
long  term::funkey(void) { return ami_funkey(outfile); }
void term::frametimer(long e) { ami_frametimer(outfile, e); }
void term::autohold(long e) { ami_autohold(e); }
void term::wrtstr(char *s) { ami_wrtstr(outfile, s); }
void term::wrtstr(char *s, long n) { ami_wrtstrn(outfile, s, n); }
void term::wrtstrn(char *s, long n) { ami_wrtstrn(outfile, s, n); }
void term::sizbuf(long x, long y) { ami_sizbuf(outfile, x, y); }

/* virtual callbacks */
long term::evchar(char c) { return 0; }
long term::evup(void) { return 0; }
long term::evdown(void) { return 0; }
long term::evleft(void) { return 0; }
long term::evright(void) { return 0; }
long term::evleftw(void) { return 0; }
long term::evrightw(void) { return 0; }
long term::evhome(void) { return 0; }
long term::evhomes(void) { return 0; }
long term::evhomel(void) { return 0; }
long term::evend(void) { return 0; }
long term::evends(void) { return 0; }
long term::evendl(void) { return 0; }
long term::evscrl(void) { return 0; }
long term::evscrr(void) { return 0; }
long term::evscru(void) { return 0; }
long term::evscrd(void) { return 0; }
long term::evpagd(void) { return 0; }
long term::evpagu(void) { return 0; }
long term::evtab(void) { return 0; }
long term::eventer(void) { return 0; }
long term::evinsert(void) { return 0; }
long term::evinsertl(void) { return 0; }
long term::evinsertt(void) { return 0; }
long term::evdel(void) { return 0; }
long term::evdell(void) { return 0; }
long term::evdelcf(void) { return 0; }
long term::evdelcb(void) { return 0; }
long term::evcopy(void) { return 0; }
long term::evcopyl(void) { return 0; }
long term::evcan(void) { return 0; }
long term::evstop(void) { return 0; }
long term::evcont(void) { return 0; }
long term::evprint(void) { return 0; }
long term::evprintb(void) { return 0; }
long term::evprints(void) { return 0; }
long term::evfun(long k) { return 0; }
long term::evmenu(void) { return 0; }
long term::evmouba(long m, long b) { return 0; }
long term::evmoubd(long m, long b) { return 0; }
long term::evmoumov(long m, long x, long y) { return 0; }
long term::evtim(long t) { return 0; }
long term::evjoyba(long j, long b) { return 0; }
long term::evjoybd(long j, long b) { return 0; }
long term::evjoymov(long j, long x, long y, long z) { return 0; }
long term::evresize(void) { return 0; }
long term::evfocus(void) { return 0; }
long term::evnofocus(void) { return 0; }
long term::evhover(void) { return 0; }
long term::evnohover(void) { return 0; }
long term::evterm(void) { return 0; }

void term::termCB(evtrec* er)

{

    long handled;

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
        /* not handled by the class, pass to next handler */
        default:        handled = 0; break;

    }

    if (!handled) termoeh(er);

}

} /* namespace terminal */
