/** ****************************************************************************
 *
 * Terminal library interface header
 *
 * Declares a routines and data for the Petit Ami terminal level
 * interface. The terminal interface describes a 2 demensional, fixed window on
 * which characters are drawn. Each character can have colors or attributes.
 * The size of the window can be determined, and timer, mouse, and joystick
 * services are supported.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include <stdio.h>

#define MAXTIM /**< maximum number of timers available */

/** colors displayable in text mode */
typedef enum { black, white, red, green, blue, cyan, yellow, magenta } color;

/** events */
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
    /** int key */                     etfun,
    /** display menu */                etmenu,
    /** mouse button assertion */      etmouba,
    /** mouse button deassertion */    etmoubd,
    /** mouse move */                  etmoumov,
    /** timer matures */               ettim,
    /** joystick button assertion */   etjoyba,
    /** joystick button deassertion */ etjoybd,
    /** joystick move */               etjoymov,
    /** terminate program */           etterm

} evtcod;

/** event record */

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
        struct {

            /** mouse handle */  int amoun;
            /** button number */ int amoubn;

        };
        struct {

            /** mouse handle */  int dmoun;
            /** button number */ int dmoubn;

        };
        struct {

            /** joystick number */ int ajoyn;
            /** button number */   int ajoybn;

        };
        struct {

            /** joystick number */ int djoyn;
            /** button number */   int djoybn;

        };
        struct {

            /** joystick number */      int mjoyn;
            /** joystick coordinates */ int joypx, joypy, joypz;

        };
        /** function key */ int fkey;

     };

} evtrec;

/** event function pointer */
typedef void (*pevthan)(evtrec*);

/* routines at this level */

void cursor(FILE* f, int x, int y);
int maxx(FILE* f);
int maxy(FILE* f);
void home(FILE* f);
void del(FILE* f);
void up(FILE* f);
void down(FILE* f);
void left(FILE* f);
void right(FILE* f);
void blink(FILE* f, int e);
void reverse(FILE* f, int e);
void underline(FILE* f, int e);
void superscript(FILE* f, int e);
void subscript(FILE* f, int e);
void italic(FILE* f, int e);
void bold(FILE* f, int e);
void strikeout(FILE* f, int e);
void standout(FILE* f, int e);
void fcolor(FILE* f, color c);
void bcolor(FILE* f, color c);
void automode(FILE* f, int e);
void curvis(FILE* f, int e);
void scroll(FILE* f, int x, int y);
int curx(FILE* f);
int cury(FILE* f);
int curbnd(FILE* f);
void selects(FILE *f, int u, int d);
void event(FILE* f, evtrec* er);
void timer(FILE* f, int i, int t, int r);
void killtimer(FILE* f, int i);
int mouse(FILE *f);
int mousebutton(FILE* f, int m);
int joystick(FILE* f);
int joybutton(FILE* f, int j);
int joyaxis(FILE* f, int j);
void settab(FILE* f, int t);
void restab(FILE* f, int t);
void clrtab(FILE* f);
int funkey(FILE* f);
void frametimer(FILE* f, int e);
void autohold(FILE* f, int e);
void wrtstr(FILE* f, char *s);
void wrtstrn(FILE* f, char *s, int n);
void eventover(evtcod e, pevthan eh,  pevthan* oeh);

#endif /* __TERMINAL_H__ */
