/*******************************************************************************
*                                                                              *
*                        GRAPHICAL MODE LIBRARY FOR GTK                        *
*                                                                              *
*                       Copyright (C) 2019 Scott A. Franco                     *
*                                                                              *
*                            2019/05/03 S. A. Franco                           *
*                                                                              *
* Implements the graphical mode functions on GTK. Gralib is upward             *
* compatible with trmlib functions.                                            *
*                                                                              *
* Proposed improvements:                                                       *
*                                                                              *
* Move(f, d, dx, dy, s, sx1, sy1, sx2, sy2)                                    *
*                                                                              *
* Moves a block of pixels from one buffer to another, or to a different place  *
* in the same buffer. Used to implement various features like intrabuffer      *
* moves, off screen image chaching, special clipping, etc.                     *
*                                                                              *
* fand, band                                                                   *
*                                                                              *
* Used with move to implement arbitrary clips usng move, above.                *
*                                                                              *
* History:                                                                     *
*                                                                              *
* Gralib started in 1996 as a graphical window demonstrator as a twin to       *
* ansilib, the ANSI control character based terminal mode library.             *
* In 2003, gralib was upgraded to the graphical terminal standard.             *
* In 2005, gralib was upgraded to include the window mangement calls, and the  *
* widget calls.                                                                *
*                                                                              *
* Gralib uses three different tasks. The main task is passed on to the         *
* program, and two subthreads are created. The first one is to run the         *
* display, and the second runs widgets. The Display task both isolates the     *
* user interface from any hangs or slowdowns in the main thread, and also      *
* allows the display task to be a completely regular windows message loop      *
* with class handler, that just happens to communicate all of its results      *
* back to the main thread. This solves several small problems with adapting    *
* the X Windows/Mac OS style we use to Windows style. The main and the         *
* display thread are "joined" such that they can both access the same          *
* windows. The widget task is required because of this joining, and serves to  *
* isolate the running of widgets from the main or display threads.             *
*                                                                              *
*******************************************************************************/

#include <termios.h>
#include <stdlib.h>
#include <string.h>

#include <sys/timerfd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

#define MAXTIM       10; /* maximum number of timers available */
#define MAXBUF       10; /* maximum number of buffers available */
#define PA_FONT_TERM  1; /* terminal font */
#define PA_FONT_BOOK  2; /* book font */
#define PA_FONT_SIGN  3; /* sign font */
#define PA_FONT_TECH  4; /* technical font (vector font) */
#define IOWIN         1;  /* logical window number of input/output pair */

/* standardized menu entries */

#define PA_SMNEW       1; /* new file */
#define PA_SMOPEN      2; /* open file */
#define PA_SMCLOSE     3; /* close file */
#define PA_SMSAVE      4; /* save file */
#define PA_SMSAVEAS    5; /* save file as name */
#define PA_SMPAGESET   6; /* page setup */
#define PA_SMPRINT     7; /* print */
#define PA_SMEXIT      8; /* exit program */
#define PA_SMUNDO      9; /* undo edit */
#define PA_SMCUT       10; /* cut selection */
#define PA_SMPASTE     11; /* paste selection */
#define PA_SMDELETE    12; /* delete selection */
#define PA_SMFIND      13; /* find text */
#define PA_SMFINDNEXT  14; /* find next */
#define PA_SMREPLACE   15; /* replace text */
#define PA_SMGOTO      16; /* goto line */
#define PA_SMSELECTALL 17; /* select all text */
#define PA_SMNEWWINDOW 18; /* new window */
#define PA_SMTILEHORIZ 19; /* tile child windows horizontally */
#define PA_SMTILEVERT  20; /* tile child windows vertically */
#define PA_SMCASCADE   21; /* cascade windows */
#define PA_SMCLOSEALL  22; /* close all windows */
#define PA_SMHELPTOPIC 23; /* help topics */
#define PA_SMABOUT     24; /* about this program */
#define PA_SMMAX       24; /* maximum defined standard menu entries */

typedef char* string;  /* general string type */
typedef string* pstring; /* pointer to string */
typedef enum { false, true } boolean; /* boolean */
/* Colors displayable in text mode. Background is the color that will match
   widgets placed onto it. */
typedef enum pa_color = (pa_black, pa_white, pa_red, pa_green, pa_blue, pa_cyan,
                         pa_yellow, pa_magenta, pa_backcolor);
typedef int joyhan; /* joystick handles */
typedef int joynum; /* number of joysticks */
typedef int joybut; /* joystick buttons */
typedef int joybtn; /* joystick number of buttons */
typedef int joyaxn; /* joystick axies */
typedef int mounum; /* number of mice */
typedef int mouhan; /* mouse handles */
typedef int moubut; /* mouse buttons */
typedef int timhan; /* timer handle */
typedef int funky;  /* function keys */
/* events */
typedef enum pa_evtcod = (
    pa_etchar,     /* ANSI character returned */
    pa_etup,       /* cursor up one line */
    pa_etdown,     /* down one line */
    pa_etleft,     /* left one character */
    pa_etright,    /* right one character */
    pa_etleftw,    /* left one word */
    pa_etrightw,   /* right one word */
    pa_ethome,     /* home of document */
    pa_ethomes,    /* home of screen */
    pa_ethomel,    /* home of line */
    pa_etend,      /* end of document */
    pa_etends,     /* end of screen */
    pa_etendl,     /* end of line */
    pa_etscrl,     /* scroll left one character */
    pa_etscrr,     /* scroll right one character */
    pa_etscru,     /* scroll up one line */
    pa_etscrd,     /* scroll down one line */
    pa_etpagd,     /* page down */
    pa_etpagu,     /* page up */
    pa_ettab,      /* tab */
    pa_etenter,    /* enter line */
    pa_etinsert,   /* insert block */
    pa_etinsertl,  /* insert line */
    pa_etinsertt,  /* insert toggle */
    pa_etdel,      /* delete block */
    pa_etdell,     /* delete line */
    pa_etdelcf,    /* delete character forward */
    pa_etdelcb,    /* delete character backward */
    pa_etcopy,     /* copy block */
    pa_etcopyl,    /* copy line */
    pa_etcan,      /* cancel current operation */
    pa_etstop,     /* stop current operation */
    pa_etcont,     /* continue current operation */
    pa_etprint,    /* print document */
    pa_etprintb,   /* print block */
    pa_etprints,   /* print screen */
    pa_etfun,      /* function key */
    pa_etmenu,     /* display menu */
    pa_etmouba,    /* mouse button assertion */
    pa_etmoubd,    /* mouse button deassertion */
    pa_etmoumov,   /* mouse move */
    pa_ettim,      /* timer matures */
    pa_etjoyba,    /* joystick button assertion */
    pa_etjoybd,    /* joystick button deassertion */
    pa_etjoymov,   /* joystick move */
    pa_etresize,   /* window was resized */
    pa_etterm,     /* terminate program */
    pa_etmoumovg,  /* mouse move graphical */
    pa_etframe,    /* frame sync */
    pa_etredraw,   /* window redraw */
    pa_etmin,      /* window minimized */
    pa_etmax,      /* window maximized */
    pa_etnorm,     /* window normalized */
    pa_etmenus,    /* menu item selected */
    pa_etbutton,   /* button assert */
    pa_etchkbox,   /* checkbox click */
    pa_etradbut,   /* radio button click */
    pa_etsclull,   /* scroll up/left line */
    pa_etscldrl,   /* scroll down/right line */
    pa_etsclulp,   /* scroll up/left page */
    pa_etscldrp,   /* scroll down/right page */
    pa_etsclpos,   /* scroll bar position */
    pa_etedtbox,   /* edit box signals done */
    pa_etnumbox,   /* number select box signals done */
    pa_etlstbox,   /* list box selection */
    pa_etdrpbox,   /* drop box selection */
    pa_etdrebox,   /* drop edit box selection */
    pa_etsldpos,   /* slider position */
    pa_ettabbar);  /* tab bar select */
/* event record */
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
        /** etmoumovg: */
        struct {

            /** mouse number */   int mmoung;
            /** mouse movement */ int moupxg, moupyg;

        };
        /** etredraw */
        struct {

            /** bounding rectangle */
            int rsx, rsy, rex, rey;

        };
        int menuid; /* menu item selected */
        int butid; /* button id */
        int ckbxid; /* checkbox */
        int radbid; /* radio button */
        int sclulid; /* scroll up/left line */
        int scldlid; /* scroll down/right line */
        int sclupid; /* scroll up/left page */
        int scldpid; /* scroll down/right page */
        struct {

            int sclpid; /* scroll bar */
            int sclpos; /* scroll bar position */

        };
        int edtbid; /* edit box complete */
        struct {

            int numbid; /* num sel box select */
            int numbsl; /* num select value */

        };
        struct {

            int lstbid; /* list box select */
            int lstbsl; /* list box select number */

        };
        struct {

            int drpbid; /* drop box select */
            int drpbsl; /* drop box select */

        };
        int drebid; /* drop edit box select */
        struct {

            int sldpid; /* slider position */
            int sldpos; /* slider position */

        };
        struct {

            int tabid;  /* tab bar */
            int tabsel; /* tab select */

        };

     };

} pa_evtrec;
     /* menu */
typedef     pa_menuptr = ^pa_menurec;
typedef struct {

        next:   menuptr; /* next menu item in list */
        branch: menuptr; /* menu branch */
        onoff:  boolean; /* on/off highlight */
        oneof:  boolean; /* "one of" highlight */
        bar:    boolean; /* place bar under */
        id:     integer; /* id of menu item */
        face:   pstring  /* text to place on button */

} pa_menurec;
     /* standard menu selector */
typedef int pa_stdmenusel;
/* windows mode sets */
typedef enum {

    pa_wmframe, /* frame on/off */
    pa_wmsize,  /* size bars on/off */
    pa_wmsysbar /* system bar on/off */

} pa_winmod;
typedef int pa_winmodset;
/* string set for list box */
typedef pa_strrec* pa_strptr;
typedef struct {

    strptr next; /* next entry in list */
    string str;  /* string */

} pa_strrec;
/* orientation for tab bars */
typedef enum { pa_totop, pa_toright, pa_tobottom, pa_toleft } pa_tabori;
/* settable items in find query */
typedef enum { pa_qfncase, pa_qfnup, pa_qfnre } pa_qfnopt;
typedef int pa_qfnopts;
/* settable items in replace query */
typedef enum { pa_qfrcase, pa_qfrup, pa_qfrre, pa_qfrfind, pa_qfrallfil, pa_qfralllin } pa_qfropt;
typedef int pa_qfropts;
/* effects in font query */
typedef enum { qfteblink, pa_qftereverse, pa_qfteunderline, pa_qftesuperscript,
                  pa_qftesubscript, pa_qfteitalic, pa_qftebold, pa_qftestrikeout,
                  pa_qftestandout, pa_qftecondensed, pa_qfteextended, pa_qftexlight,
                  pa_qftelight, pa_qftexbold, pa_qftehollow, pa_qfteraised} qfteffect;
typedef int pa_qfteffects;

/* functions at this level */

/* text */

void pa_cursor(FILE* f, int x, int y);
int pa_maxx(FILE* f);
int pa_maxy(FILE* f);
void pa_home(FILE* f);
void pa_del(FILE* f);
void pa_up(FILE* f);
void pa_down(FILE* f);
void pa_left(FILE* f);
void pa_right(FILE* f);
void pa_blink(FILE* f, boolean e);
void pa_reverse(FILE* f, boolean e);
void pa_underline(FILE* f, boolean e);
void pa_superscript(FILE* f, boolean e);
void pa_subscript(FILE* f, boolean e);
void pa_italic(FILE* f, boolean e);
void pa_bold(FILE* f, boolean e);
void pa_strikeout(FILE* f, boolean e);
void pa_standout(FILE* f, boolean e);
void pa_fcolor(FILE* f, pa_color c);
void pa_bcolor(FILE* f, pa_color c);
void pa_auto(FILE* f, boolean e);
void pa_curvis(FILE* f, boolean e);
void pa_scroll(FILE* f, int x, int y);
int pa_curx(FILE* f);
int pa_cury(FILE* f);
int pa_curbnd(FILE* f);
void pa_select(FILE* f, int u, int d);
void pa_event(FILE* f, pa_evtrec* er);
void pa_timer(FILE* f, timhan i, int t, boolean r);
void pa_killtimer(FILE* f, timhan i);
mounum pa_mouse(FILE* f);
moubut function pa_mousebutton(FILE* f, mouhan m);
joynum function pa_joystick(FILE* f);
joybtn function pa_joybutton(FILE* f, joyhan j);
joyaxn function pa_joyaxis(FILE* f, joyhan j);
void pa_settab(FILE* f, int t);
void pa_restab(FILE* f, int t);
void pa_clrtab(FILE* f);
funky pa_funkey(FILE* f);
void pa_frametimer(FILE* f, boolean e);
void pa_autohold(boolean e);
void pa_wrtstr(FILE* f, string s);

/* graphical */

int pa_maxxg(FILE* f);
int pa_maxyg(FILE* f);
int pa_curxg(FILE* f);
int pa_curyg(FILE* f);
void pa_line(FILE* f, int x1, int y1, int x2, int y2);
void pa_linewidth(FILE* f, int w);
void pa_rect(FILE* f, int x1, int y1, int x2, int y2);
void pa_frect(FILE* f, int x1, int y1, int x2, int y2);
void pa_rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
void pa_frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
void pa_ellipse(FILE* f, int x1, int y1, int x2, int y2);
void pa_fellipse(FILE* f, int x1, int y1, int x2, int y2);
void pa_arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
void pa_farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
void pa_fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
void pa_ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3);
void pa_cursorg(FILE* f, int x, int y);
int pa_baseline(FILE* f);
void pa_setpixel(FILE* f, int x, int y);
void pa_fover(FILE* f);
void pa_bover(FILE* f);
void pa_finvis(FILE* f);
void pa_binvis(FILE* f);
void pa_fxor(FILE* f);
void pa_bxor(FILE* f);
int pa_chrsizx(FILE* f);
int pa_chrsizy(FILE* f);
int pa_fonts(FILE* f);
void pa_font(FILE* f, int fc);
void pa_fontnam(FILE* f, int fc, string fns);
void pa_fontsiz(FILE* f, int s);
void pa_chrspcy(FILE* f, int s);
void pa_chrspcx(FILE* f, int s);
int pa_dpmx(FILE* f);
int pa_dpmy(FILE* f);
int pa_strsiz(FILE* f, string s);
int pa_strsizp(FILE* f, string s);
int pa_chrpos(FILE* f, string s, int p);
void pa_writejust(FILE* f, string s, int n);
int pa_justpos(FILE* f, string s, int p, int n);
void pa_condensed(FILE* f, boolean e);
void pa_extended(FILE* f, boolean e);
void pa_xlight(FILE* f, boolean e);
void pa_light(FILE* f, boolean e);
void pa_xbold(FILE* f, boolean e);
void pa_hollow(FILE* f, boolean e);
void pa_raised(FILE* f, boolean e);
void pa_settabg(FILE* f, boolean t);
void pa_restabg(FILE* f, boolean t);
/* note since C has no overloads, we put the rgb sets for character fonts as a
   different name function, with 'c' appended */
void pa_fcolorg(FILE* f, int r, int g, int b);
void pa_fcolorc(FILE* f, int r, int g, int b);
void pa_bcolorg(FILE* f, int r, int g, int b);
void pa_bcolorc(FILE* f, int r, int g, int b);
void pa_loadpict(FILE* f, int p, string fn);
int pa_pictsizx(FILE* f, int p);
int pa_pictsizy(FILE* f, int p);
void pa_picture(FILE* f, int p, int x1, int y1, int x2, int y2);
void pa_delpict(FILE* f, int p);
void pa_scrollg(FILE* f, int x, int y);

/* Window management functions */

void pa_title(FILE* f, string ts);
void pa_openwin(FILE* infile, FILE* outfile, FILE* parent, int wid);
void pa_buffer(FILE* f, boolean e);
void pa_sizbuf(FILE* f, int x, int y);
void pa_sizbufg(FILE* f, int x, int y);
void pa_getsiz(FILE* f, int* x, int* y);
void pa_getsizg(FILE* f, int* x, int* y);
void pa_setsiz(FILE* f, int x, int y);
void pa_setsizg(FILE* f, int x, int y);
void pa_setpos(FILE* f, int x, int y);
void pa_setposg(FILE* f, int x, int y);
void pa_scnsiz(FILE* f, int* x, int* y);
void pa_scnsizg(FILE* f, int* x, int*y);
void pa_winclient(FILE* f, int cx, int cy, int* wx, int* wy, winmodset ms);
void pa_winclientg(FILE* f, int cx, int cy, int* wx, int* wy, winmodset ms);
void pa_front(FILE* f);
void pa_back(FILE* f);
void pa_frame(FILE* f, boolean e);
void pa_sizable(FILE* f, boolean e);
void pa_sysbar(FILE* f, boolean e);
void pa_menu(FILE* f, menuptr m);
void pa_menuena(FILE* f, int id, boolean onoff);
void pa_menusel(FILE* f, int id, boolean select);
void pa_stdmenu(stdmenusel sms, menuptr* sm, menuptr pm);

/* widgets/controls */

void pa_killwidget(FILE* f, int id);
void pa_selectwidget(FILE* f, int id, boolean e);
void pa_enablewidget(FILE* f, int id, boolean e);
void pa_getwidgettext(FILE* f, int id, string s);
void pa_putwidgettext(FILE* f, int id, string s);
void pa_sizwidgetg(FILE* f, int id, int x, int y);
void pa_poswidgetg(FILE* f, int id, int x, int y);
void pa_buttonsiz(FILE* f, string s, int* int* w, h);
void pa_buttonsizg(FILE* f, string s, int* w, int* h);
void pa_button(FILE* f, int x1, int y1, int x2, int y2, string s, int id);
void pa_buttong(FILE* f, int x1, int y1, int x2, int y2, string s, int id);
void pa_checkboxsiz(FILE* f, string s, int* w, int* h);
void pa_checkboxsizg(FILE* f, string s, int* w, int* h);
void pa_checkbox(FILE* f, int x1, int y1, int x2, int y2; string s, int id);
void pa_checkboxg(FILE* f, int x1, int y1, int x2, int y2; string s, int id);
void pa_radiobuttonsiz(FILE* f, view s: string; int* w, int* h);
void pa_radiobuttonsizg(FILE* f, view s: string; int* w, int* h);
void pa_radiobutton(FILE* f, int x1, int y1, int x2, int y2, string s, int id);
void pa_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, string s, int id);
void pa_groupsizg(FILE* f, string s, int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
void pa_groupsiz(FILE* f, string s, int cw, int ch, int* w, int* h, int* ox,
                 int* oy);
void pa_group(FILE* f, int x1, int y1, int x2, int y2, string s, int id);
void pa_groupg(FILE* f, int x1, int y1, int x2, int y2 string s, int id);
void pa_background(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_backgroundg(FILE* f, int x1, int y1, int x2, int y2, id);
void pa_scrollvertsizg(FILE* f, int* w, int* h);
void pa_scrollvertsiz(FILE* f, int* w, int* h);
void pa_scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_scrollhorizsizg(FILE* f, int* w, int* h);
void pa_scrollhorizsiz(FILE* f, int* w, int* h);
void pa_scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_scrollpos(FILE* f, int id, int r);
void pa_scrollsiz(FILE* f, int id, int r);
void pa_numselboxsizg(FILE* f, int l, int u; int* w, int* h);
void pa_numselboxsiz(FILE* f, l, u; int* w, int* h);
void pa_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                  int id);
void pa_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                   int id);
void pa_editboxsizg(FILE* f, string s, int* w, int* h);
void pa_editboxsiz(FILE* f, string s, int* w, int* h);
void pa_editbox(FILE* f, int x1, int y1, int x2, int y2; int id);
void pa_editboxg(FILE* f, int x1, int y1, int x2, int y2; int id);
void pa_progbarsizg(FILE* f, int* w, int* h);
void pa_progbarsiz(FILE* f, int* w, int* h);
void pa_progbar(FILE* f, int x1, int y1, int x2, int y2; int id);
void pa_progbarg(FILE* f, int x1, int y1, int x2, int 2; int id);
void pa_progbarpos(FILE* f, int id; int pos);
void pa_listboxsizg(FILE* f, strptr sp, int* w, int* h);
void pa_listboxsiz(FILE* f, strptr sp, int* w, int* h);
void pa_listbox(FILE* f, int x1, int y1, int x2, y2; strptr sp, int id);
void pa_listboxg(FILE* f, int x1, int y1, int x2, int y2; strptr sp, id);
void pa_dropboxsizg(FILE* f, sp: strptr; int* cw, int* ch, int* ow, int* oh);
void pa_dropboxsiz(FILE* f, sp: strptr; int* cw, int* ch, int* ow, int* oh);
void pa_dropbox(FILE* f, int x1, int y1, int x2, int y2; strptr sp, int id);
void pa_dropboxg(FILE* f, int x1, int y1, int x2, int y2; strptr sp, int id);
void pa_dropeditboxsizg(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh);
void pa_dropeditboxsiz(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropeditbox(FILE* f, int x1, int y1, int x2, int y2; strptr sp, int id);
void pa_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2; strptr sp,
                     int id);
void pa_slidehorizsizg(FILE* f, int* w, int* h);
void pa_slidehorizsiz(FILE* f, int* w, int* h);
void pa_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void pa_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void pa_slidevertsizg(FILE* f, int* w, int* h);
void pa_slidevertsiz(FILE* f, int* w, int* h);
void pa_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void pa_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void pa_tabbarsizg(FILE* f, tor: tabori; int cw, int ch, int* w, int* h,
                   int* ox, int* oy);
void pa_tabbarsiz(FILE* f, tor: tabori; int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
void pa_tabbarclientg(FILE* f, tor: tabori; int w, int h, int* cw, int* ch,
                      int* ox, int* oy);
void pa_tabbarclient(FILE* f, tor: tabori; int w, int h, int* cw, int* ch,
                     int* ox, int* oy);
void pa_tabbar(FILE* f, int x1, int y1, int x2, int y2; strptr sp, tor: tabori,
               int id);
void pa_tabbarg(FILE* f, int x1, int y1, int x2, int y2; strptr sp, tor: tabori,
                int id);
void pa_tabsel(FILE* f, int id; int tn);
void pa_alert(string title, string message);
void pa_querycolor(int* r, int* g, int* b);
void pa_queryopen(pstring s);
void pa_querysave(pstring s);
void pa_queryfind(pstring s, qfnopts* opt);
void pa_queryfindrep(pstring s, pstring r, qfropts* opt);
void pa_queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb, int* br,
                  int* bg, int* bb, qfteffects* effect);

typedef enum {

    eftbful,  /* File table full */
    ejoyacc,  /* Joystick access */
    etimacc,  /* Timer access */
    efilopr,  /* Cannot perform operation on special file */
    einvscn,  /* Invalid screen number */
    einvhan,  /* Invalid handle */
    einvtab,  /* Invalid tab position */
    eatopos,  /* Cannot position text by pixel with auto on */
    eatocur,  /* Cannot position outside screen with auto on */
    eatoofg,  /* Cannot reenable auto off grid */
    eatoecb,  /* Cannot reenable auto outside screen */
    einvftn,  /* Invalid font number */
    etrmfnt,  /* Valid terminal font not found */
    eatofts,  /* Cannot resize font with auto enabled */
    eatoftc,  /* Cannot change fonts with auto enabled */
    einvfnm,  /* Invalid logical font number */
    efntemp,  /* Empty logical font */
    etrmfts,  /* Cannot size terminal font */
    etabful,  /* Too many tabs set */
    eatotab,  /* Cannot use graphical tabs with auto on */
    estrinx,  /* String index out of range */
    epicfnf,  /* Picture file not found */
    epicftl,  /* Picture filename too large */
    etimnum,  /* Invalid timer number */
    ejstsys,  /* Cannot justify system font */
    efnotwin, /* File is not attached to a window */
    ewinuse,  /* Window id in use */
    efinuse,  /* File already in use */
    einmode,  /* Input side of window in wrong mode */
    edcrel,   /* Cannot release Windows device context */
    einvsiz,  /* Invalid buffer size */
    ebufoff,  /* buffered mode not enabled */
    edupmen,  /* Menu id was duplicated */
    emennf,   /* Meny id was not found */
    ewignf,   /* Widget id was not found */
    ewigdup,  /* Widget id was duplicated */
    einvspos, /* Invalid scroll bar slider position */
    einvssiz, /* Invalid scroll bar size */
    ectlfal,  /* Attempt to create control fails */
    eprgpos,  /* Invalid progress bar position */
    estrspc,  /* Out of string space */
    etabbar,  /* Unable to create tab in tab bar */
    efildlg,  /* Unable to create file dialog */
    efnddlg,  /* Unable to create find dialog */
    efntdlg,  /* Unable to create font dialog */
    efndstl,  /* Find/replace string too long */
    einvwin,  /* Invalid window number */
    einvjye,  /* Invalid joystick event */
    ejoyqry,  /* Could not get information on joystick */
    einvjoy,  /* Invalid joystick ID */
    eclsinw,  /* Cannot directly close input side of window */
    ewigsel,  /* Widget is not selectable */
    ewigptxt, /* Cannot put text in this widget */
    ewiggtxt, /* Cannot get text from this widget */
    ewigdis,  /* Cannot disable this widget */
    estrato,  /* Cannot direct write string with auto on */
    etabsel,  /* Invalid tab select */
    esystem); /* System consistency check */

} errcod;

/*******************************************************************************

Scroll screen

Scrolls the ANSI terminal screen by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source and destination rectangles, and use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

void scrollg(FILE* f, int x, int y);

{

}

void scroll(FILE* f, int x, int y);

{

}

/*******************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

void cursor(FILE* f, int x, int y);

{

}

/*******************************************************************************

Position cursor graphical

Moves the cursor to the specified x and y location in pixels.

*******************************************************************************/

void cursorg(FILE* f, int x, int y);

{

}

/*******************************************************************************

Find character baseline

Returns the offset, from the top of the current fonts character bounding box,
to the font baseline. The baseline is the line all characters rest on.

*******************************************************************************/

int baseline(FILE* f);

{

}

/*******************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int maxx(FILE* f);

{

}

/*******************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int maxy(FILE* f);

{

}

/*******************************************************************************

Return maximum x dimension graphical

Returns the maximum x dimension, which is the width of the client surface in
pixels.

*******************************************************************************/

int maxxg(FILE* f);

{

}

/*******************************************************************************

Return maximum y dimension graphical

Returns the maximum y dimension, which is the height of the client surface in
pixels.

*******************************************************************************/

int maxyg(FILE* f);

{

}

/*******************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void home(FILE* f);

{

}

/*******************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

void up(FILE* f);

{

}

/*******************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

void down(FILE* f);

{

}

/*******************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

void left(FILE* f);

{
}

/*******************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

void right(FILE* f);

{

}

/*******************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

*******************************************************************************/

void blink(FILE* f, boolean e);

{

}

/*******************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void reverse(FILE* f, boolean e);

{

}

/*******************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.
This is not implemented, but could be done by drawing a line under each
character drawn.

*******************************************************************************/

void underline(FILE* f, boolean e);

{

}

/*******************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void superscript(FILE* f, boolean e);

{

}

/*******************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void subscript(FILE* f, boolean e);

{

}

/*******************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

Italic is causing problems with fixed mode on some fonts, and Windows does not
seem to want to share with me just what the true width of an italic font is
(without taking heroic measures like drawing and testing pixels). So we disable
italic on fixed fonts.

*******************************************************************************/

void italic(FILE* f, boolean e);

{

}

/*******************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void bold(FILE* f, boolean e);

{

}

/*******************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

void strikeout(FILE* f, boolean e);

{

}

/*******************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void standout(FILE* f, boolean e);

{

}

/*******************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

*******************************************************************************/

void fcolor(FILE* f, pa_color c);

{

}

void fcolorc(FILE* f, int r, int g, int b);

{

}

/*******************************************************************************

Set foreground color graphical

Sets the foreground color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

Fcolor exists as an overload to the text version, but we also provide an
fcolorg for backward compatiblity to the days before overloads.

*******************************************************************************/

void fcolorg(FILE* f, int r, int g, int b);

{

}

/*******************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void bcolor(FILE* f, pa_color c);

{

}

void bcolorc(FILE* f, int r, int g, int b);

{

}

/*******************************************************************************

Set background color graphical

Sets the background color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

*******************************************************************************/

void bcolorg(FILE* f, r, g, b: integer);

{

}

/*******************************************************************************

Enable/disable automatic scroll and wrap


Enables or disables automatic screen scroll and end of line wrapping. When the
cursor leaves the screen in automatic mode, the following occurs:

up       Scroll down
down     Scroll up
right    Line down, start at left
left     Line up, start at right

These movements can be combined. Leaving the screen right from the lower right
corner will both wrap and scroll up. Leaving the screen left from upper left
will wrap and scroll down.

With auto disabled, no automatic scrolling will occur, and any movement of the
cursor off screen will simply cause the cursor to be undefined. In this
package that means the cursor is off, and no characters are written. On a
real terminal, it simply means that the position is undefined, and could be
anywhere.

*******************************************************************************/

void auto(FILE* f, boolean e);

{

}

/*******************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void curvis(FILE* f, boolean e);

{

}

/*******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int curx(FILE* f);

{

}

/*******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int cury(FILE* f);

{

}

/*******************************************************************************

Get location of cursor in x graphical

Returns the current location of the cursor in x, in pixels.

*******************************************************************************/

int curxg(FILE* f);

{

}

/*******************************************************************************

Get location of cursor in y graphical

Returns the current location of the cursor in y, in pixels.

*******************************************************************************/

int curyg(FILE* f);

{

}

/*******************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
then a new screen is allocated and cleared.
The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

*******************************************************************************/

void select(FILE* f, int u, int d);

{

}

/*******************************************************************************

Write string to current cursor position

Writes a string to the current cursor position, then updates the cursor
position. This acts as a series of write character calls. However, it eliminates
several layers of protocol, and results in much faster write time for
applications that require it.

It is an error to call this routine with auto enabled, since it could exceed
the bounds of the screen.

No control characters or other interpretation is done, and invisible characters
such as controls are not suppressed.

*******************************************************************************/

void wrtstr(FILE* f, string s);

{

}

/*******************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void del(FILE* f);

{

}

/*******************************************************************************

Draw line

Draws a single line in the foreground color.

*******************************************************************************/

void line(FILE* f, int x1, int y1, int x2, int y2);

{

}

/*******************************************************************************

Draw rectangle

Draws a rectangle in foreground color.

*******************************************************************************/

void rect(FILE* f, int x1, int y1, int x2, int y2);

{

}

/*******************************************************************************

Draw filled rectangle

Draws a filled rectangle in foreground color.

*******************************************************************************/

void frect(FILE* f, int x1, int y1, int x2, int y2);

{

}

/*******************************************************************************

Draw rounded rectangle

Draws a rounded rectangle in foreground color.

*******************************************************************************/

void rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);

{

}

/*******************************************************************************

Draw filled rounded rectangle

Draws a filled rounded rectangle in foreground color.

*******************************************************************************/

void frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);

{

}

/*******************************************************************************

Draw ellipse

Draws an ellipse with the current foreground color and line width.

*******************************************************************************/

void ellipse(FILE* f, int x1, int y1, int x2, int y2);

{

}

/*******************************************************************************

Draw filled ellipse

Draws a filled ellipse with the current foreground color.

*******************************************************************************/

void fellipse(FILE* f, int x1, int y1, int x2, int y2);

{

}

/*******************************************************************************

Draw arc

Draws an arc in the current foreground color and line width. The containing
rectangle of the ellipse is given, and the start and end angles clockwise from
0 degrees delimit the arc.

Windows takes the start and end delimited by a line extending from the center
of the arc. The way we do the convertion is to project the angle upon a circle
whose radius is the precision we wish to use for the calculation. Then that
point on the circle is found by triangulation.

The larger the circle of precision, the more angles can be represented, but
the trade off is that the circle must not reach the edge of an integer
(-maxint..maxint). That means that the total logical coordinate space must be
shortened by the precision. To find out what division of the circle precis
represents, use cd := precis*2*pi. So, for example, precis = 100 means 628
divisions of the circle.

The end and start points can be negative.
Note that Windows draws arcs counterclockwise, so our start and end points are
swapped.

Negative angles are allowed.

*******************************************************************************/

void arc(FILE* f, x1, y1, x2, y2, sa, ea: integer);

{

}

/*******************************************************************************

Draw filled arc

Draws a filled arc in the current foreground color. The same comments apply
as for the arc function above.

*******************************************************************************/

void farc(FILE* f, x1, y1, x2, y2, sa, ea: integer);

{

}

/*******************************************************************************

Draw filled cord

Draws a filled cord in the current foreground color. The same comments apply
as for the arc function above.

*******************************************************************************/

void fchord(FILE* f, x1, y1, x2, y2, sa, ea: integer);

{

}

/*******************************************************************************

Draw filled triangle

Draws a filled triangle in the current foreground color.

*******************************************************************************/

void ftriangle(FILE* f, x1, y1, x2, y2, x3, y3: integer);

{

}

/*******************************************************************************

Set pixel

Sets a single logical pixel to the foreground color.

*******************************************************************************/

void setpixel(FILE* f, x, y: integer);

{

}

/*******************************************************************************

Set foreground to overwrite

Sets the foreground write mode to overwrite.

*******************************************************************************/

void fover(FILE* f);

{

}

/*******************************************************************************

Set background to overwrite

Sets the background write mode to overwrite.

*******************************************************************************/

void bover(FILE* f);

{

}

/*******************************************************************************

Set foreground to invisible

Sets the foreground write mode to invisible.

*******************************************************************************/

void finvis(FILE* f);

{

}

/*******************************************************************************

Set background to invisible

Sets the background write mode to invisible.

*******************************************************************************/

void binvis(FILE* f);

{

}

/*******************************************************************************

Set foreground to xor

Sets the foreground write mode to xor.

*******************************************************************************/

void fxor(FILE* f);

{

}

/*******************************************************************************

Set background to xor

Sets the background write mode to xor.

*******************************************************************************/

void bxor(FILE* f);

{

}

/*******************************************************************************

Set line width

Sets the width of lines and several other figures.

*******************************************************************************/

void linewidth(FILE* f, w: integer);

{

}

/*******************************************************************************

Find character size x

Returns the character width.

*******************************************************************************/

function chrsizx(FILE* f): integer;

{

}

/*******************************************************************************

Find character size y

Returns the character height.

*******************************************************************************/

function chrsizy(FILE* f): integer;

{

}

/*******************************************************************************

Find number of installed fonts

Finds the total number of installed fonts.

*******************************************************************************/

function fonts(FILE* f): integer;

{

}

/*******************************************************************************

Change fonts

Changes the current font to the indicated logical font number.

*******************************************************************************/

void font(FILE* f, fc: integer);

{

}

/*******************************************************************************

Find name of font

Returns the name of a font by number.

*******************************************************************************/

void fontnam(FILE* f, fc: integer; var fns: string);

{

}

/*******************************************************************************

Change font size

Changes the font sizing to match the given character height. The character
and line spacing are changed, as well as the baseline.

*******************************************************************************/

void fontsiz(FILE* f, s: integer);

{

}

/*******************************************************************************

Set character extra spacing y

Sets the extra character space to be added between lines, also referred to
as "ledding".

Not implemented yet.

*******************************************************************************/

void chrspcy(FILE* f, s: integer);

{

}

/*******************************************************************************

Sets extra character space x

Sets the extra character space to be added between characters, referred to
as "spacing".

Not implemented yet.

*******************************************************************************/

void chrspcx(FILE* f, s: integer);

{

}

/*******************************************************************************

Find dots per meter x

Returns the number of dots per meter resolution in x.

*******************************************************************************/

function dpmx(FILE* f): integer;

{

}

/*******************************************************************************

Find dots per meter y

Returns the number of dots per meter resolution in y.

*******************************************************************************/

function dpmy(FILE* f): integer;

{

}

/*******************************************************************************

Find string size in pixels

Returns the number of pixels wide the given string would be, considering
character spacing and kerning.

*******************************************************************************/

function strsiz(FILE* f, view s: string): integer;

{

}

function strsizp(FILE* f, view s: string): integer;

{

}

/*******************************************************************************

Find character position in string

Finds the pixel offset to the given character in the string.

*******************************************************************************/

function chrpos(FILE* f, view s: string; p: integer): integer;

{

}

/*******************************************************************************

Write justified text

Writes a string of text with justification. The string and the width in pixels
is specified. Auto mode cannot be on for this function, nor can it be used on
the system font.

*******************************************************************************/

void writejust(FILE* f, view s: string; n: integer);

{

}

/*******************************************************************************

Find justified character position

Given a string, a character position in that string, and the total length
of the string in pixels, returns the offset in pixels from the start of the
string to the given character, with justification taken into account.
The model used is that the extra space needed is divided by the number of
spaces, with the fractional part lost.

*******************************************************************************/

function justpos(FILE* f, view s: string; p, n: integer): integer;

{

}

/*******************************************************************************

Turn on condensed attribute

Turns on/off the condensed attribute. Condensed is a character set with a
shorter baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void condensed(FILE* f, e: boolean);

{

}

/*******************************************************************************

Turn on extended attribute

Turns on/off the extended attribute. Extended is a character set with a
longer baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void extended(FILE* f, e: boolean);

{

   refer(f, e) /* stub out */

}

/*******************************************************************************

Turn on extra light attribute

Turns on/off the extra light attribute. Extra light is a character thiner than
light.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void xlight(FILE* f, e: boolean);

{

}

/*******************************************************************************

Turn on light attribute

Turns on/off the light attribute. Light is a character thiner than normal
characters in the currnet font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void light(FILE* f, e: boolean);

{

}

/*******************************************************************************

Turn on extra bold attribute

Turns on/off the extra bold attribute. Extra bold is a character thicker than
bold.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void xbold(FILE* f, e: boolean);

{

}

/*******************************************************************************

Turn on hollow attribute

Turns on/off the hollow attribute. Hollow is an embossed or 3d effect that
makes the characters appear sunken into the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void hollow(FILE* f, e: boolean);

{

}

/*******************************************************************************

Turn on raised attribute

Turns on/off the raised attribute. Raised is an embossed or 3d effect that
makes the characters appear coming off the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void raised(FILE* f, e: boolean);

{

}

/*******************************************************************************

Delete picture

Deletes a loaded picture.

*******************************************************************************/

void delpict(FILE* f, p: integer);

{

}

/*******************************************************************************

Load picture

Loads a picture into a slot of the loadable pictures array.

*******************************************************************************/

void loadpict(FILE* f, p: integer; view fn: string);

{

}

/*******************************************************************************

Find size x of picture

Returns the size in x of the logical picture.

*******************************************************************************/

function pictsizx(FILE* f, p: integer): integer;

{

}

/*******************************************************************************

Find size y of picture

Returns the size in y of the logical picture.

*******************************************************************************/

function pictsizy(FILE* f, p: integer): integer;

{

}

/*******************************************************************************

Draw picture

Draws a picture from the given file to the rectangle. The picture is resized
to the size of the rectangle.

Images will be kept in a rotating cache to prevent repeating reloads.

*******************************************************************************/

void picture(FILE* f, p: integer; x1, y1, x2, y2: integer);

{

}

/*******************************************************************************

Set viewport offset graphical

Sets the offset of the viewport in logical space, in pixels, anywhere from
-maxint to maxint.

*******************************************************************************/

void viewoffg(FILE* f, x, y: integer);

{

}

/*******************************************************************************

Set viewport scale

Sets the viewport scale in x and y. The scale is a real fraction between 0 and
1, with 1 being 1:1 scaling. Viewport scales are allways smaller than logical
scales, which means that there are more than one logical pixel to map to a
given physical pixel, but never the reverse. This means that pixels are lost
in going to the display, but the display never needs to interpolate pixels
from logical pixels.

Note:

Right now, symmetrical scaling (both x and y scales set the same) are all that
works completely, since we don't presently have a method to warp text to fit
a scaling process. However, this can be done by various means, including
painting into a buffer and transfering asymmetrically, or using outlines.

*******************************************************************************/

void viewscale(FILE* f, x, y: real);

{

}

/*******************************************************************************

Aquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

*******************************************************************************/

void event(FILE* f, var er: evtrec);

{

}

/*******************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds. 
This means that a bit more than 24 hours can be measured without using the
sign.

Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it sends a timer mature event to
the associated input file.

*******************************************************************************/

void timer(FILE* f,     /* file to send event to */
                    i: timhan;   /* timer handle */
                    t: integer;  /* number of tenth-milliseconds to run */
                    r: boolean); /* timer is to rerun after completion */

{

}

/*******************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

*******************************************************************************/

void killtimer(FILE* f,    /* file to kill timer on */
                        i: timhan); /* handle of timer */

{

}

/*******************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

void frametimer(FILE* f, e: boolean);

{

}

/*******************************************************************************

Set automatic hold state

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from gralib.
This exists to allow the results of gralib unaware programs to be viewed after
termination, instead of exiting an distroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fufills the requirement of
holding gralib unaware programs.

*******************************************************************************/

void autohold(e: boolean);

{

}

/*******************************************************************************

Return number of mice

Returns the number of mice implemented. Windows supports only one mouse.

*******************************************************************************/

function mouse(FILE* f): mounum;

{

}

/*******************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

*******************************************************************************/

function mousebutton(FILE* f, m: mouhan): moubut;

{

}

/*******************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

function joystick(FILE* f): joynum;

{

}

/*******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

function joybutton(FILE* f, j: joyhan): joybtn;

{

}

/*******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

function joyaxis(FILE* f, j: joyhan): joyaxn;

{

}

/*******************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

*******************************************************************************/

void settabg(FILE* f, t: integer);

{

}

/*******************************************************************************

Set tab

Sets a tab at the indicated collumn number.

*******************************************************************************/

void settab(FILE* f, t: integer);

{

}

/*******************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

*******************************************************************************/

void restabg(FILE* f, t: integer);

{

}

/*******************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void restab(FILE* f, t: integer);

{

}

/*******************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void clrtab(FILE* f);

{

}

/*******************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

*******************************************************************************/

function funkey(FILE* f): funky;

{

}

/*******************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

void title(FILE* f, view ts: string);

{

}

/*******************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.
The window id can be from 1 to ss_maxhdl, but the input and output file ids
of 1 and 2 are reserved for the input and output files, and cannot be used
directly. These ids will be be opened as a pair anytime the "_input" or
"_output" file names are seen.

*******************************************************************************/

void openwin(var infile, outfile, parent: text; wid: ss_filhdl);

{

}

/*******************************************************************************

Size buffer pixel

Sets or resets the size of the buffer surface, in pixel units.

*******************************************************************************/

void sizbufg(FILE* f, x, y: integer);

{

}

/*******************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

void sizbuf(FILE* f, x, y: integer);

{

}

/*******************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

void buffer(FILE* f, e: boolean);

{

}

/*******************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

*******************************************************************************/

void menu(FILE* f, m: menuptr);

{

}

/*******************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

*******************************************************************************/

void menuena(FILE* f, id: integer; onoff: boolean);

{

}

/*******************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

*******************************************************************************/

void menusel(FILE* f, id: integer; select: boolean);

{

}

/*******************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

void front(FILE* f);

{

}

/*******************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

void back(FILE* f);

{

}

/*******************************************************************************

Get window size graphical

Gets the onscreen window size.

*******************************************************************************/

void getsizg(FILE* f, var x, y: integer);

{

}

/*******************************************************************************

Get window size character

Gets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are returned. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void getsiz(FILE* f, var x, y: integer);

{

}

/*******************************************************************************

Set window size graphical

Sets the onscreen window to the given size.

*******************************************************************************/

void setsizg(FILE* f, x, y: integer);

{

}

/*******************************************************************************

Set window size character

Sets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void setsiz(FILE* f, x, y: integer);

{

}

/*******************************************************************************

Set window position graphical

Sets the onscreen window to the given position in its parent.

*******************************************************************************/

void setposg(FILE* f, x, y: integer);

{

}

/*******************************************************************************

Set window position character

Sets the onscreen window position, in character terms. If the window has a
parent, the demensions are converted to the current character size there.
Otherwise, pixel based demensions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void setpos(FILE* f, x, y: integer);

{

}

/*******************************************************************************

Get screen size graphical

Gets the total screen size.

*******************************************************************************/

void scnsizg(FILE* f, var x, y: integer);

{

}

/*******************************************************************************

Find window size from client

Finds the window size, in parent terms, needed to result in a given client
window size.

Note: this routine should be able to find the minimum size of a window using
the given style, and return the minimums if the input size is lower than this.
This does not seem to be obvious under Windows.

Do we also need a menu style type ?

*******************************************************************************/

void winclient(FILE* f, cx, cy: integer; var wx, wy: integer;
                    view ms: winmodset);

{

}

void winclientg(FILE* f, cx, cy: integer; var wx, wy: integer;
                     view ms: winmodset);

{

}

/*******************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

*******************************************************************************/

void scnsiz(FILE* f, var x, y: integer);

{

}

/*******************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

void frame(FILE* f, e: boolean);

{

}

/*******************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

*******************************************************************************/

void sizable(FILE* f, e: boolean);

{

}

/*******************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

*******************************************************************************/

void sysbar(FILE* f, e: boolean);

{

}

/*******************************************************************************

Create standard menu

Creates a standard menu set. Given a set of standard items selected in a set,
and a program added menu list, creates a new standard menu.

On this windows version, the standard lists are:

file edit <program> window help

That is, all of the standard items are sorted into the lists at the start and
end of the menu, then the program selections placed in the menu.

*******************************************************************************/

void stdmenu(view sms: stdmenusel; var sm: menuptr; pm: menuptr);

{

}

/*******************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

void killwidget(FILE* f, id: integer);

{

}

/*******************************************************************************

Select/deselect widget

Selects or deselects a widget.

*******************************************************************************/

void selectwidget(FILE* f, id: integer; e: boolean);

{

}

/*******************************************************************************

Enable/disable widget

Enables or disables a widget.

*******************************************************************************/

void enablewidget(FILE* f, id: integer; e: boolean);

{

}

/*******************************************************************************

Get widget text

Retrives the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

*******************************************************************************/

void getwidgettext(FILE* f, id: integer; var s: pstring);

{

}

/*******************************************************************************

put edit box text

Places text into an edit box.

*******************************************************************************/

void putwidgettext(FILE* f, id: integer; view s: string);

{

}

/*******************************************************************************

Resize widget

Changes the size of a widget.

*******************************************************************************/

void sizwidgetg(FILE* f, id: integer; x, y: integer);

{

}

/*******************************************************************************

Reposition widget

Changes the parent position of a widget.

*******************************************************************************/

void poswidgetg(FILE* f, id: integer; x, y: integer);

{

}

/*******************************************************************************

Place widget to back of Z order

*******************************************************************************/

void backwidget(FILE* f, id: integer);

{

}

/*******************************************************************************

Place widget to back of Z order

*******************************************************************************/

void frontwidget(FILE* f, id: integer);

{

}

/*******************************************************************************

Find minimum/standard button size

Finds the minimum size for a button. Given the face string, the minimum size of
a button is calculated and returned.

*******************************************************************************/

void buttonsizg(FILE* f, view s: string; var w, h: integer);

{

}

void buttonsiz(FILE* f, view s: string; var w, h: integer);

{

}

/*******************************************************************************

Create button

Creates a standard button within the specified rectangle, on the given window.

*******************************************************************************/

void buttong(FILE* f, x1, y1, x2, y2: integer; view s: string;
                 id: integer);

{

}

void button(FILE* f, x1, y1, x2, y2: integer; view s: string;
                 id: integer);

{

}

/*******************************************************************************

Find minimum/standard checkbox size

Finds the minimum size for a checkbox. Given the face string, the minimum size of
a checkbox is calculated and returned.

*******************************************************************************/

void checkboxsizg(FILE* f, view s: string; var w, h: integer);

{

}

void checkboxsiz(FILE* f, view s: string; var w, h: integer);

{

}

/*******************************************************************************

Create checkbox

Creates a standard checkbox within the specified rectangle, on the given
window.

*******************************************************************************/

void checkboxg(FILE* f, x1, y1, x2, y2: integer; view s: string;
                   id: integer);

{

}

void checkbox(FILE* f, x1, y1, x2, y2: integer; view s: string;
                   id: integer);

{

}

/*******************************************************************************

Find minimum/standard radio button size

Finds the minimum size for a radio button. Given the face string, the minimum
size of a radio button is calculated and returned.

*******************************************************************************/

void radiobuttonsizg(FILE* f, view s: string; var w, h: integer);

{

}

void radiobuttonsiz(FILE* f, view s: string; var w, h: integer);

{

}

/*******************************************************************************

Create radio button

Creates a standard radio button within the specified rectangle, on the given
window.

*******************************************************************************/

void radiobuttong(FILE* f, x1, y1, x2, y2: integer; view s: string;
                      id: integer);

{

}

void radiobutton(FILE* f, x1, y1, x2, y2: integer; view s: string;
                   id: integer);

{

}

/*******************************************************************************

Find minimum/standard group size

Finds the minimum size for a group. Given the face string, the minimum
size of a group is calculated and returned.

*******************************************************************************/

void groupsizg(FILE* f, view s: string; cw, ch: integer;
                    var w, h, ox, oy: integer);

{

}

void groupsiz(FILE* f, view s: string; cw, ch: integer;
                   var w, h, ox, oy: integer);

{

}

/*******************************************************************************

Create group box

Creates a group box, which is really just a decorative feature that gererates
no messages. It is used as a background for other widgets.

*******************************************************************************/

void groupg(FILE* f, x1, y1, x2, y2: integer; view s: string;
                 id: integer);

{

}

void group(FILE* f, x1, y1, x2, y2: integer; view s: string;
                   id: integer);

{

}

/*******************************************************************************

Create background box

Creates a background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

*******************************************************************************/

void backgroundg(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

void background(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

/*******************************************************************************

Find minimum/standard vertical scrollbar size

Finds the minimum size for a vertical scrollbar. The minimum size of a vertical
scrollbar is calculated and returned.

*******************************************************************************/

void scrollvertsizg(FILE* f, var w, h: integer);

{

}

void scrollvertsiz(FILE* f, var w, h: integer);

{

}

/*******************************************************************************

Create vertical scrollbar

Creates a vertical scrollbar.

*******************************************************************************/

void scrollvertg(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

void scrollvert(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

/*******************************************************************************

Find minimum/standard horizontal scrollbar size

Finds the minimum size for a horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated and returned.

*******************************************************************************/

void scrollhorizsizg(FILE* f, var w, h: integer);

{

}

void scrollhorizsiz(FILE* f, var w, h: integer);

{

}

/*******************************************************************************

Create horizontal scrollbar

Creates a horizontal scrollbar.

*******************************************************************************/

void scrollhorizg(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

void scrollhoriz(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

/*******************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

*******************************************************************************/

void scrollpos(FILE* f, id: integer; r: integer);

{

}

/*******************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

*******************************************************************************/

void scrollsiz(FILE* f, id: integer; r: integer);

{

}

/*******************************************************************************

Find minimum/standard number select box size

Finds the minimum size for a number select box. The minimum size of a number
select box is calculated and returned.

*******************************************************************************/

void numselboxsizg(FILE* f, l, u: integer; var w, h: integer);

{

}

void numselboxsiz(FILE* f, l, u: integer; var w, h: integer);

{

}

/*******************************************************************************

Create number selector

Creates an up/down control for numeric selection.

*******************************************************************************/

void numselboxg(FILE* f, x1, y1, x2, y2: integer; l, u: integer;
                     id: integer);

{

}

void numselbox(FILE* f, x1, y1, x2, y2: integer; l, u: integer;
                     id: integer);

{

}

/*******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void editboxsizg(FILE* f, view s: string; var w, h: integer);

{

}

void editboxsiz(FILE* f, view s: string; var w, h: integer);

{

}

/*******************************************************************************

Create edit box

Creates single line edit box

*******************************************************************************/

void editboxg(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

void editbox(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

/*******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void progbarsizg(FILE* f, var w, h: integer);

{

}

void progbarsiz(FILE* f, var w, h: integer);

{

}

/*******************************************************************************

Create progress bar

Creates a progress bar.

*******************************************************************************/

void progbarg(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

void progbar(FILE* f, x1, y1, x2, y2: integer; id: integer);

{

}

/*******************************************************************************

Set progress bar position

Sets the position of a progress bar, from 0 to maxint.

*******************************************************************************/

void progbarpos(FILE* f, id: integer; pos: integer);

{

}

/*******************************************************************************

Find minimum/standard list box size

Finds the minimum size for an list box. Given a string list, the minimum size
of an list box is calculated and returned.

Windows listboxes pretty much ignore the size given. If you allocate more space
than needed, it will only put blank lines below if enough space for an entire
line is present. If the size does not contain exactly enough to display the
whole line list, the box will colapse to a single line with an up/down
control. The only thing that is garanteed is that the box will fit within the
specified rectangle, one way or another.

*******************************************************************************/

void listboxsizg(FILE* f, sp: strptr; var w, h: integer);

{

}

void listboxsiz(FILE* f, sp: strptr; var w, h: integer);

{

}

/*******************************************************************************

Create list box

Creates a list box. Fills it with the string list provided.

*******************************************************************************/

void listboxg(FILE* f, x1, y1, x2, y2: integer; sp: strptr;
                   id: integer);

{

}

void listbox(FILE* f, x1, y1, x2, y2: integer; sp: strptr;
                   id: integer);

{

}

/*******************************************************************************

Find minimum/standard dropbox size

Finds the minimum size for a dropbox. Given the face string, the minimum size of
a dropbox is calculated and returned, for both the "open" and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void dropboxsizg(FILE* f, sp: strptr; var cw, ch, ow, oh: integer);

{

}

void dropboxsiz(FILE* f, sp: strptr; var cw, ch, ow, oh: integer);

{

}

/*******************************************************************************

Create dropdown box

Creates a dropdown box. Fills it with the string list provided.

*******************************************************************************/

void dropboxg(FILE* f, x1, y1, x2, y2: integer; sp: strptr;
                   id: integer);

{

}

void dropbox(FILE* f, x1, y1, x2, y2: integer; sp: strptr;
                   id: integer);

{

}

/*******************************************************************************

Find minimum/standard drop edit box size

Finds the minimum size for a drop edit box. Given the face string, the minimum
size of a drop edit box is calculated and returned, for both the "open" and 
"closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void dropeditboxsizg(FILE* f, sp: strptr; var cw, ch, ow, oh: integer);

{

}

void dropeditboxsiz(FILE* f, sp: strptr; var cw, ch, ow, oh: integer);

{

}

/*******************************************************************************

Create dropdown edit box

Creates a dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

void dropeditboxg(FILE* f, x1, y1, x2, y2: integer; sp: strptr;
                       id: integer);

{

}

void dropeditbox(FILE* f, x1, y1, x2, y2: integer; sp: strptr;
                       id: integer);

{

}

overload void dropeditbox(x1, y1, x2, y2: integer; sp: strptr;
                                id: integer);

{

}

/*******************************************************************************

Find minimum/standard horizontal slider size

Finds the minimum size for a horizontal slider. The minimum size of a horizontal
slider is calculated and returned.

*******************************************************************************/

void slidehorizsizg(FILE* f, var w, h: integer);

{

}

void slidehorizsiz(FILE* f, var w, h: integer);

{

}

/*******************************************************************************

Create horizontal slider

Creates a horizontal slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void slidehorizg(FILE* f, x1, y1, x2, y2: integer; mark: integer;
                      id: integer);

{

}

void slidehoriz(FILE* f, x1, y1, x2, y2: integer; mark: integer;
                      id: integer);

{

}

/*******************************************************************************

Find minimum/standard vertical slider size

Finds the minimum size for a vertical slider. The minimum size of a vertical
slider is calculated and returned.

*******************************************************************************/

void slidevertsizg(FILE* f, var w, h: integer);

{

}

void slidevertsiz(FILE* f, var w, h: integer);

{

}

/*******************************************************************************

Create vertical slider

Creates a vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void slidevertg(FILE* f, x1, y1, x2, y2: integer; mark: integer;
                     id: integer);

{

}

void slidevert(FILE* f, x1, y1, x2, y2: integer; mark: integer;
                     id: integer);

{

}

/*******************************************************************************

Find minimum/standard tab bar size

Finds the minimum size for a tab bar. The minimum size of a tab bar is
calculated and returned.

*******************************************************************************/

void tabbarsizg(FILE* f, tor: tabori; cw, ch: integer;
                     var w, h, ox, oy: integer);

{

}

void tabbarsiz(FILE* f, tor: tabori; cw, ch: integer;
                    var w, h, ox, oy: integer);

{

}

/*******************************************************************************

Find client from tabbar size

Given a tabbar size and orientation, this routine gives the client size and 
offset. This is used where the tabbar size is fixed, but the client area is
flexible.

*******************************************************************************/

void tabbarclientg(FILE* f, tor: tabori; w, h: integer;
                     var cw, ch, ox, oy: integer);

{

}

void tabbarclient(FILE* f, tor: tabori; w, h: integer;
                    var cw, ch, ox, oy: integer);

{

}

/*******************************************************************************

Create tab bar

Creates a tab bar with the given orientation.

Bug: has strange overwrite mode where when the widget is first created, it
allows itself to be overwritten by the main window. This is worked around by
creating and distroying another widget.

*******************************************************************************/

void tabbarg(FILE* f, x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                  id: integer);

{

}

void tabbar(FILE* f, x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                  id: integer);

{

}

/*******************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

*******************************************************************************/

void tabsel(FILE* f, id: integer; tn: integer);

{

}

/*******************************************************************************

Output message dialog

Outputs a message dialog with the given title and message strings.

*******************************************************************************/

void alert(view title, message: string);

{

}

/*******************************************************************************

Display choose color dialog

Presents the choose color dialog, then returns the resulting color.

Bug: does not take the input color as the default.

*******************************************************************************/

void querycolor(var r, g, b: integer);

{

}

/*******************************************************************************

Display choose file dialog for open

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

void queryopen(var s: pstring);

{

}

/*******************************************************************************

Display choose file dialog for save

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

void querysave(var s: pstring);

{

}

/*******************************************************************************

Display choose find text dialog

Presents the choose find text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: should return null string on cancel. Unlike other dialogs, windows
provides no indication of if the cancel button was pushed. To do this, we
would need to hook (or subclass) the find dialog.

After note: tried hooking the window. The issue is that the cancel button is
just a simple button that gets pressed. Trying to rely on the button id
sounds very system dependent, since that could change. One method might be
to retrive the button text, but this is still fairly system dependent. We
table this issue until later.

*******************************************************************************/

void queryfind(var s: pstring; var opt: qfnopts);

{

}

/*******************************************************************************

Display choose replace text dialog

Presents the choose replace text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: See comment, queryfind.

*******************************************************************************/

void queryfindrep(var s, r: pstring; var opt: qfropts);

{

}

/*******************************************************************************

Display choose font dialog

Presents the choose font dialog, then returns the resulting logical font
number, size, foreground color, background color, and effects (in a special
effects set for this routine).

The parameters are "flow through", meaning that they should be set to their
defaults before the call, and changes are made, then updated to the parameters.
During the routine, the state of the parameters given are presented to the
user as the defaults.

*******************************************************************************/

void queryfont(FILE* f, var fc, s, fr, fg, fb, br, bg, bb: integer;
                    var effect: qfteffects);

{

}

/*******************************************************************************

Gralib startup

*******************************************************************************/

static void pa_init_graphics (void) __attribute__((constructor (102)));
static void pa_init_graphics()

{

}

/*******************************************************************************

Gralib shutdown

*******************************************************************************/

static void pa_deinit_graphics (void) __attribute__((destructor (102)));
static void pa_deinit_graphics()

{

}
