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

#define PA_MAXTIM 10 /**< maximum number of timers available */

/** colors displayable in text mode */
typedef enum { pa_black, pa_white, pa_red, pa_green, pa_blue, pa_cyan,
               pa_yellow, pa_magenta } pa_color;

/** events */
typedef enum {

    /** ANSI character returned */     pa_etchar,
    /** cursor up one line */          pa_etup,
    /** down one line */               pa_etdown,
    /** left one character */          pa_etleft,
    /** right one character */         pa_etright,
    /** left one word */               pa_etleftw,
    /** right one word */              pa_etrightw,
    /** home of document */            pa_ethome,
    /** home of screen */              pa_ethomes,
    /** home of line */                pa_ethomel,
    /** end of document */             pa_etend,
    /** end of screen */               pa_etends,
    /** end of line */                 pa_etendl,
    /** scroll left one character */   pa_etscrl,
    /** scroll right one character */  pa_etscrr,
    /** scroll up one line */          pa_etscru,
    /** scroll down one line */        pa_etscrd,
    /** page down */                   pa_etpagd,
    /** page up */                     pa_etpagu,
    /** tab */                         pa_ettab,
    /** enter line */                  pa_etenter,
    /** insert block */                pa_etinsert,
    /** insert line */                 pa_etinsertl,
    /** insert toggle */               pa_etinsertt,
    /** delete block */                pa_etdel,
    /** delete line */                 pa_etdell,
    /** delete character forward */    pa_etdelcf,
    /** delete character backward */   pa_etdelcb,
    /** copy block */                  pa_etcopy,
    /** copy line */                   pa_etcopyl,
    /** cancel current operation */    pa_etcan,
    /** stop current operation */      pa_etstop,
    /** continue current operation */  pa_etcont,
    /** print document */              pa_etprint,
    /** print block */                 pa_etprintb,
    /** print screen */                pa_etprints,
    /** function key */                pa_etfun,
    /** display menu */                pa_etmenu,
    /** mouse button assertion */      pa_etmouba,
    /** mouse button deassertion */    pa_etmoubd,
    /** mouse move */                  pa_etmoumov,
    /** timer matures */               pa_ettim,
    /** joystick button assertion */   pa_etjoyba,
    /** joystick button deassertion */ pa_etjoybd,
    /** joystick move */               pa_etjoymov,
    /** terminate program */           pa_etterm

} pa_evtcod;

/** event record */

typedef struct {

	/* identifier of window for event */ int winid;
    /* event type */                     pa_evtcod etype;
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

} pa_evtrec;

/** event function pointer */
typedef void (*pa_pevthan)(pa_evtrec*);

/* routines at this level */

void pa_cursor(FILE* f, int x, int y);
int  pa_maxx(FILE* f);
int  pa_maxy(FILE* f);
void pa_home(FILE* f);
void pa_del(FILE* f);
void pa_up(FILE* f);
void pa_down(FILE* f);
void pa_left(FILE* f);
void pa_right(FILE* f);
void pa_blink(FILE* f, int e);
void pa_reverse(FILE* f, int e);
void pa_underline(FILE* f, int e);
void pa_superscript(FILE* f, int e);
void pa_subscript(FILE* f, int e);
void pa_italic(FILE* f, int e);
void pa_bold(FILE* f, int e);
void pa_strikeout(FILE* f, int e);
void pa_standout(FILE* f, int e);
void pa_fcolor(FILE* f, pa_color c);
void pa_bcolor(FILE* f, pa_color c);
void pa_auto(FILE* f, int e);
void pa_curvis(FILE* f, int e);
void pa_scroll(FILE* f, int x, int y);
int  pa_curx(FILE* f);
int  pa_cury(FILE* f);
int  pa_curbnd(FILE* f);
void pa_select(FILE *f, int u, int d);
void pa_event(FILE* f, pa_evtrec* er);
void pa_timer(FILE* f, int i, int t, int r);
void pa_killtimer(FILE* f, int i);
int  pa_mouse(FILE *f);
int  pa_mousebutton(FILE* f, int m);
int  pa_joystick(FILE* f);
int  pa_joybutton(FILE* f, int j);
int  pa_joyaxis(FILE* f, int j);
void pa_settab(FILE* f, int t);
void pa_restab(FILE* f, int t);
void pa_clrtab(FILE* f);
int  pa_funkey(FILE* f);
void pa_frametimer(FILE* f, int e);
void pa_autohold(FILE* f, int e);
void pa_wrtstr(FILE* f, char *s);
void pa_wrtstrn(FILE* f, char *s, int n);
void pa_eventover(pa_evtcod e, pa_pevthan eh,  pa_pevthan* oeh);

#endif /* __TERMINAL_H__ */
