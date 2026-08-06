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

#define MAXTIM AMI_MAXTIM /**< maximum number of timers available */

/* colors displayable in text mode */
typedef enum { black, white, red, green, blue, cyan,
               yellow, magenta } color;

/* events */
typedef enum {

    /** ANSI character returned */      etchar,
    /** cursor up one line */           etup,
    /** down one line */                etdown,
    /** left one character */           etleft,
    /** right one character */          etright,
    /** left one word */                etleftw,
    /** right one word */               etrightw,
    /** home of document */             ethome,
    /** home of screen */               ethomes,
    /** home of line */                 ethomel,
    /** end of document */              etend,
    /** end of screen */                etends,
    /** end of line */                  etendl,
    /** scroll left one character */    etscrl,
    /** scroll right one character */   etscrr,
    /** scroll up one line */           etscru,
    /** scroll down one line */         etscrd,
    /** page down */                    etpagd,
    /** page up */                      etpagu,
    /** tab */                          ettab,
    /** enter line */                   etenter,
    /** insert block */                 etinsert,
    /** insert line */                  etinsertl,
    /** insert toggle */                etinsertt,
    /** delete block */                 etdel,
    /** delete line */                  etdell,
    /** delete character forward */     etdelcf,
    /** delete character backward */    etdelcb,
    /** copy block */                   etcopy,
    /** copy line */                    etcopyl,
    /** cancel current operation */     etcan,
    /** stop current operation */       etstop,
    /** continue current operation */   etcont,
    /** print document */               etprint,
    /** print block */                  etprintb,
    /** print screen */                 etprints,
    /** function key */                 etfun,
    /** display menu */                 etmenu,
    /** mouse button assertion */       etmouba,
    /** mouse button deassertion */     etmoubd,
    /** mouse move */                   etmoumov,
    /** timer matures */                ettim,
    /** joystick button assertion */    etjoyba,
    /** joystick button deassertion */  etjoybd,
    /** joystick move */                etjoymov,
    /** window was resized */           etresize,
    /** window has focus */             etfocus,    
    /** window lost focus */            etnofocus,  
    /** window being hovered */         ethover,    
    /** window stopped being hovered */ etnohover,
    /** terminate program */            etterm,
    /** frame sync */                   etframe,
    /** window redraw */                etredraw,
    /** window minimized */             etmin,
    /** window maximized */             etmax,
    /** window normalized */            etnorm,
    /** menu item selected */           etmenus,

    /* Reserved extra code areas, these are module defined. */
    etsys    = 0x1000, /* start of base system reserved codes */
    etman    = 0x2000, /* start of window management reserved codes */
    etwidget = 0x3000, /* start of widget reserved codes */
    etuser   = 0x4000  /* start of user defined codes */

} evtcod;

typedef struct {

    /* identifier of window for event */ long winid;
    /* event type */                     evtcod etype;
    /* event was handled */              long handled;
    union {

        /* these events require parameter data */

        /** etchar: ANSI character returned */  char echar;
        /** ettim: timer handle that matured */ long timnum;
        /** etmoumov: */
        struct {

            /** mouse number */   long mmoun;
            /** mouse movement */ long moupx, moupy;

        };
        /** etmouba */
        struct {

            /** mouse handle */  long amoun;
            /** button number */ long amoubn;

        };
        /** etmoubd */
        struct {

            /** mouse handle */  long dmoun;
            /** button number */ long dmoubn;

        };
        /** etjoyba */
        struct {

            /** joystick number */ long ajoyn;
            /** button number */   long ajoybn;

        };
        /** etjoybd */
        struct {

            /** joystick number */ long djoyn;
            /** button number */   long djoybn;

        };
        /** etjoymov */
        struct {

            /** joystick number */      long mjoyn;
            /** joystick coordinates */ long joypx, joypy, joypz;
                                        long joyp4, joyp5, joyp6;

        };
        /** function key */ long fkey;
        /** etresize */
        struct {

            long rszx, rszy;

        };
        /* etmenus */
        long menuid; /* menu item selected */

     };

} evtrec, *evtptr;

/** event function pointer */
typedef void (*pevthan)(evtrec*);

/* procedural interface */
void cursor(FILE* f, long x, long y);
void cursor(long x, long y);
long  maxx(FILE* f);
long  maxx(void);
long  maxy(FILE* f);
long  maxy(void);
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
void blink(FILE* f, long e);
void blink(long e);
void reverse(FILE* f, long e);
void reverse(long e);
void underline(FILE* f, long e);
void underline(long e);
void superscript(FILE* f, long e);
void superscript(long e);
void subscript(FILE* f, long e);
void subscript(long e);
void italic(FILE* f, long e);
void italic(long e);
void bold(FILE* f, long e);
void bold(long e);
void strikeout(FILE* f, long e);
void strikeout(long e);
void standout(FILE* f, long e);
void standout(long e);
void fcolor(FILE* f, color c);
void fcolor(color c);
void bcolor(FILE* f, color c);
void bcolor(color c);
void autom(FILE* f, long e);
void autom(long e);
void curvis(FILE* f, long e);
void curvis(long e);
void scroll(FILE* f, long x, long y);
void scroll(long x, long y);
long  curx(FILE* f);
long  curx(void);
long  cury(FILE* f);
long  cury(void);
long  curbnd(FILE* f);
long  curbnd(void);
void select(FILE *f, long u, long d);
void select(long u, long d);
void event(FILE* f, evtrec* er);
void event(evtrec* er);
void timer(FILE* f, long i, long t, long r);
void timer(long i, long t, long r);
void killtimer(FILE* f, long i);
void killtimer(long i);
long  mouse(FILE *f);
long  mouse(void);
long  mousebutton(FILE* f, long m);
long  mousebutton(long m);
long  joystick(FILE* f);
long  joystick(void);
long  joybutton(FILE* f, long j);
long  joybutton(long j);
long  joyaxis(FILE* f, long j);
long  joyaxis(long j);
void settab(FILE* f, long t);
void settab(long t);
void restab(FILE* f, long t);
void restab(long t);
void clrtab(FILE* f);
void clrtab(void);
long  funkey(FILE* f);
long  funkey(void);
void frametimer(FILE* f, long e);
void frametimer(long e);
void autohold(FILE* f, long e);
void autohold(long e);
void wrtstr(FILE* f, char *s);
void wrtstr(char *s);
void wrtstr(FILE* f, char *s, long n);
void wrtstr(char *s, long n);
void wrtstrn(FILE* f, char* s, long n);
void wrtstrn(char* s, long n);
void sizbuf(FILE* f, long x, long y);
void sizbuf(long x, long y);
void eventover(evtcod e, pevthan eh, pevthan* oeh);
void eventsover(pevthan eh, pevthan* oeh);

/* object based interface */
class term {

FILE*      infile;
FILE*      outfile;

public:

/* constructor */
term();

/* destructor */
~term();

/* methods */
void cursor(long x, long y);
long  maxx(void);
long  maxy(void);
void home(void);
void del(void);
void up(void);
void down(void);
void left(void);
void right(void);
void blink(long e);
void reverse(long e);
void underline(long e);
void superscript(long e);
void subscript(long e);
void italic(long e);
void bold(long e);
void strikeout(long e);
void standout(long e);
void fcolor(color c);
void bcolor(color c);
void autom(long e);
void curvis(long e);
void scroll(long x, long y);
long  curx(void);
long  cury(void);
long  curbnd(void);
void select(long u, long d);
void event(evtrec* er);
void timer(long i, long t, long r);
void killtimer(long i);
long  mouse(void);
long  mousebutton(long m);
long  joystick(void);
long  joybutton(long j);
long  joyaxis(long j);
void settab(long t);
void restab(long t);
void clrtab(void);
long  funkey(void);
void frametimer(long e);
void autohold(long e);
void wrtstr(char *s);
void wrtstr(char *s, long n);
void wrtstrn(char *s, long n);
void sizbuf(long x, long y);
static void termCB(evtrec* er);

/* virtual callbacks */
virtual long evchar(char c);
virtual long evup(void);
virtual long evdown(void);
virtual long evleft(void);
virtual long evright(void);
virtual long evleftw(void);
virtual long evrightw(void);
virtual long evhome(void);
virtual long evhomes(void);
virtual long evhomel(void);
virtual long evend(void);
virtual long evends(void);
virtual long evendl(void);
virtual long evscrl(void);
virtual long evscrr(void);
virtual long evscru(void);
virtual long evscrd(void);
virtual long evpagd(void);
virtual long evpagu(void);
virtual long evtab(void);
virtual long eventer(void);
virtual long evinsert(void);
virtual long evinsertl(void);
virtual long evinsertt(void);
virtual long evdel(void);
virtual long evdell(void);
virtual long evdelcf(void);
virtual long evdelcb(void);
virtual long evcopy(void);
virtual long evcopyl(void);
virtual long evcan(void);
virtual long evstop(void);
virtual long evcont(void);
virtual long evprint(void);
virtual long evprintb(void);
virtual long evprints(void);
virtual long evfun(long k);
virtual long evmenu(void);
virtual long evmouba(long m, long b);
virtual long evmoubd(long m, long b);
virtual long evmoumov(long m, long x, long y);
virtual long evtim(long t);
virtual long evjoyba(long j, long b);
virtual long evjoybd(long j, long b);
virtual long evjoymov(long j, long x, long y, long z);
virtual long evresize(void);
virtual long evfocus(void);
virtual long evnofocus(void);
virtual long evhover(void);
virtual long evnohover(void);
virtual long evterm(void);

}; /* class term */

} /* namespace terminal */

#endif /* __TERMINAL_HPP__ */
