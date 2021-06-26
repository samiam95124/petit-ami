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

/* colors displayable in text mode */
typedef enum { black, white, red, green, blue, cyan,
               yellow, magenta } color;

/* events */
typedef enum {

    /** ANSI character returned */     etchar,
    /** cursor up one line */          etup,
    /** down one line */               etdown,
    /** left one character */          etleft,
    /** right one character */         etright,
    /** left one word */               etleftw,
    /** right one word */              etrightw,
    /** home of document */            ethome,
    /** home of screen */              ethomes,
    /** home of line */                ethomel,
    /** end of document */             etend,
    /** end of screen */               etends,
    /** end of line */                 etendl,
    /** scroll left one character */   etscrl,
    /** scroll right one character */  etscrr,
    /** scroll up one line */          etscru,
    /** scroll down one line */        etscrd,
    /** page down */                   etpagd,
    /** page up */                     etpagu,
    /** tab */                         ettab,
    /** enter line */                  etenter,
    /** insert block */                etinsert,
    /** insert line */                 etinsertl,
    /** insert toggle */               etinsertt,
    /** delete block */                etdel,
    /** delete line */                 etdell,
    /** delete character forward */    etdelcf,
    /** delete character backward */   etdelcb,
    /** copy block */                  etcopy,
    /** copy line */                   etcopyl,
    /** cancel current operation */    etcan,
    /** stop current operation */      etstop,
    /** continue current operation */  etcont,
    /** print document */              etprint,
    /** print block */                 etprintb,
    /** print screen */                etprints,
    /** function key */                etfun,
    /** display menu */                etmenu,
    /** mouse button assertion */      etmouba,
    /** mouse button deassertion */    etmoubd,
    /** mouse move */                  etmoumov,
    /** timer matures */               ettim,
    /** joystick button assertion */   etjoyba,
    /** joystick button deassertion */ etjoybd,
    /** joystick move */               etjoymov,
    /** window was resized */          etresize,
    /** terminate program */           etterm

} evtcod;

typedef struct {

    /* identifier of window for event */ int winid;
    /* event type */                     evtcod etype;
    /* event was handled */              int handled;
    union {

        /* these events require parameter data */

        /** etchar: ANSI character returned */  char echar;
        /** ettim: timer handle that matured */ int timnum;
        /** etmoumov: */
        struct {

            /** mouse number */   int mmoun;
            /** mouse movement */ int moupx, moupy;

        };
        /** etmouba */
        struct {

            /** mouse handle */  int amoun;
            /** button number */ int amoubn;

        };
        /** etmoubd */
        struct {

            /** mouse handle */  int dmoun;
            /** button number */ int dmoubn;

        };
        /** etjoyba */
        struct {

            /** joystick number */ int ajoyn;
            /** button number */   int ajoybn;

        };
        /** etjoybd */
        struct {

            /** joystick number */ int djoyn;
            /** button number */   int djoybn;

        };
        /** etjoymov */
        struct {

            /** joystick number */      int mjoyn;
            /** joystick coordinates */ int joypx, joypy, joypz;

        };
        /** function key */ int fkey;

     };

} evtrec, *evtptr;

/** event function pointer */
typedef void (*pevthan)(evtrec*);

/* procedural interface */
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
void blink(FILE* f, int e);
void blink(int e);
void reverse(FILE* f, int e);
void reverse(int e);
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
void eventover(evtcod e, pevthan eh, pevthan* oeh);
void eventsover(pevthan eh, pevthan* oeh);

/* object based interface */
class term {

FILE*      infile;
FILE*      outfile;

public:

/* constructor */
term();

/* methods */
void cursor(int x, int y);
int  maxx(void);
int  maxy(void);
void home(void);
void del(void);
void up(void);
void down(void);
void left(void);
void right(void);
void blink(int e);
void reverse(int e);
void underline(int e);
void superscript(int e);
void subscript(int e);
void italic(int e);
void bold(int e);
void strikeout(int e);
void standout(int e);
void fcolor(color c);
void bcolor(color c);
void autom(int e);
void curvis(int e);
void scroll(int x, int y);
int  curx(void);
int  cury(void);
int  curbnd(void);
void select(int u, int d);
void event(evtrec* er);
void timer(int i, int t, int r);
void killtimer(int i);
int  mouse(void);
int  mousebutton(int m);
int  joystick(void);
int  joybutton(int j);
int  joyaxis(int j);
void settab(int t);
void restab(int t);
void clrtab(void);
int  funkey(void);
void frametimer(int e);
void autohold(int e);
void wrtstr(char *s);
static void termCB(evtrec* er);

/* virtual callbacks */
virtual int evchar(char c);
virtual int evup(void);
virtual int evdown(void);
virtual int evleft(void);
virtual int evright(void);
virtual int evleftw(void);
virtual int evrightw(void);
virtual int evhome(void);
virtual int evhomes(void);
virtual int evhomel(void);
virtual int evend(void);
virtual int evends(void);
virtual int evendl(void);
virtual int evscrl(void);
virtual int evscrr(void);
virtual int evscru(void);
virtual int evscrd(void);
virtual int evpagd(void);
virtual int evpagu(void);
virtual int evtab(void);
virtual int eventer(void);
virtual int evinsert(void);
virtual int evinsertl(void);
virtual int evinsertt(void);
virtual int evdel(void);
virtual int evdell(void);
virtual int evdelcf(void);
virtual int evdelcb(void);
virtual int evcopy(void);
virtual int evcopyl(void);
virtual int evcan(void);
virtual int evstop(void);
virtual int evcont(void);
virtual int evprint(void);
virtual int evprintb(void);
virtual int evprints(void);
virtual int evfun(int k);
virtual int evmenu(void);
virtual int evmouba(int m, int b);
virtual int evmoubd(int m, int b);
virtual int evmoumov(int m, int x, int y);
virtual int evtim(int t);
virtual int evjoyba(int j, int b);
virtual int evjoybd(int j, int b);
virtual int evjoymov(int j, int x, int y, int z);
virtual int evresize(void);
virtual int evterm(void);

}; /* class term */

} /* namespace terminal */

#endif /* __TERMINAL_HPP__ */
