/*******************************************************************************
*                                                                              *
*                        GRAPHICAL MODE LIBRARY HEADER                         *
*                                                                              *
*                       Copyright (C) 2019 Scott A. Franco                     *
*                                                                              *
*                            2019/05/03 S. A. Franco                           *
*                                                                              *
* Describes the full Petit-ami graphical subsystem, including terminal level,  *
* graphics level, windowing, and widgets.                                      *
*                                                                              *
*******************************************************************************/

#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include <stdio.h>

#define PA_MAXTIM 10 /**< maximum number of timers available */

/* standard fonts */

#define PA_FONT_TERM 1 /* terminal (fixed space) font */
#define PA_FONT_BOOK 2 /* serif font */
#define PA_FONT_SIGN 3 /* san-serif font */
#define PA_FONT_TECH 4 /* technical (scalable) font */

/* standardized menu entries */

#define PA_SMNEW        1 /* new file */
#define PA_SMOPEN       2 /* open file */
#define PA_SMCLOSE      3 /* close file */
#define PA_SMSAVE       4 /* save file */
#define PA_SMSAVEAS     5 /* save file as name */
#define PA_SMPAGESET    6 /* page setup */
#define PA_SMPRINT      7 /* print */
#define PA_SMEXIT       8 /* exit program */
#define PA_SMUNDO       9 /* undo edit */
#define PA_SMCUT       10 /* cut selection */
#define PA_SMPASTE     11 /* paste selection */
#define PA_SMDELETE    12 /* delete selection */
#define PA_SMFIND      13 /* find text */
#define PA_SMFINDNEXT  14 /* find next */
#define PA_SMREPLACE   15 /* replace text */
#define PA_SMGOTO      16 /* goto line */
#define PA_SMSELECTALL 17 /* select all text */
#define PA_SMNEWWINDOW 18 /* new window */
#define PA_SMTILEHORIZ 19 /* tile child windows horizontally */
#define PA_SMTILEVERT  20 /* tile child windows vertically */
#define PA_SMCASCADE   21 /* cascade windows */
#define PA_SMCLOSEALL  22 /* close all windows */
#define PA_SMHELPTOPIC 23 /* help topics */
#define PA_SMABOUT     24 /* about this program */
#define PA_SMMAX       24 /* maximum defined standard menu entries */

/* Colors displayable in text mode. Background is the color that will match
   widgets placed onto it. */
typedef enum { pa_black, pa_white, pa_red, pa_green, pa_blue, pa_cyan,
               pa_yellow, pa_magenta, pa_backcolor } pa_color;
/* events */
typedef enum {
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
    pa_etframe,    /* frame sync */
    pa_etmoumovg,  /* mouse move graphical */
    pa_etredraw,   /* window redraw */
    pa_etmin,      /* window minimized */
    pa_etmax,      /* window maximized */
    pa_etnorm,     /* window normalized */
    pa_etfocus,    /* window has focus */
    pa_etnofocus,  /* window lost focus */
    pa_ethover,    /* window being hovered */
    pa_etnohover,  /* window stopped being hovered */
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
    pa_etdrebox,   /* drop edit box signals done */
    pa_etsldpos,   /* slider position */
    pa_ettabbar,   /* tab bar select */

    /* Reserved extra code areas, these are module defined. */
    pa_etsys    = 0x1000, /* start of base system reserved codes */
    pa_etman    = 0x2000, /* start of window management reserved codes */
    pa_etwidget = 0x3000, /* start of widget reserved codes */
    pa_etuser   = 0x4000  /* start of user defined codes */
} pa_evtcod;
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
        /* etmouba */
        struct {

            /** mouse handle */  int amoun;
            /** button number */ int amoubn;

        };
        /* etmoubd */
        struct {

            /** mouse handle */  int dmoun;
            /** button number */ int dmoubn;

        };
        /* pa_etjoyba */
        struct {

            /** joystick number */ int ajoyn;
            /** button number */   int ajoybn;

        };
        /* pa_etjoybd */
        struct {

            /** joystick number */ int djoyn;
            /** button number */   int djoybn;

        };
        /* pa_etjoymov */
        struct {

            /** joystick number */      int mjoyn;
            /** joystick coordinates */ int joypx, joypy, joypz;
                                        int joyp4, joyp5, joyp6;

        };
        /* pa_etresize */
        struct {

            int rszx, rszy, rszxg, rszyg;

        };
        /* pa_etfun */
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
        /* pa_etmenus */
        int menuid; /* menu item selected */
        /* pa_etbutton */
        int butid; /* button id */
        /* pa_etchkbox */
        int ckbxid; /* checkbox id */
        /* pa_etradbut */
        int radbid; /* radio button id */
        /* pa_etsclull */
        int sclulid; /* scroll up/left line id*/
        /* pa_etscldrl */
        int scldrid; /* scroll down/right line id */
        /* pa_etsclulp */
        int sclupid; /* scroll up/left page id */
        /* pa_etscldrp */
        int scldpid; /* scroll down/right page id */
        /* pa_etsclpos */
        struct {

            int sclpid; /* scroll bar id */
            int sclpos; /* scroll bar position */

        };
        /* pa_etedtbox */
        int edtbid; /* edit box complete id */
        /* pa_etnumbox */
        struct { /* number select box signals done */

            int numbid; /* num sel box id */
            int numbsl; /* num select value */

        };
        /* pa_etlstbox */
        struct {

            int lstbid; /* list box id */
            int lstbsl; /* list box select number */

        };
        /* pa_etdrpbox */
        struct {

            int drpbid; /* drop box id */
            int drpbsl; /* drop box select */

        };
        /* pa_etdrebox */
        int drebid; /* drop edit box id */
        /* pa_etsldpos */
        struct {

            int sldpid; /* slider id */
            int sldpos; /* slider position */

        };
        /* pa_ettabbar */
        struct {

            int tabid;  /* tab bar id */
            int tabsel; /* tab select */

        };

     };

} pa_evtrec, *pa_evtptr;

/* event function pointer */
typedef void (*pa_pevthan)(pa_evtrec*);

/* menu */
typedef struct pa_menurec* pa_menuptr;
typedef struct pa_menurec {

        pa_menuptr next;   /* next menu item in list */
        pa_menuptr branch; /* menu branch */
        int        onoff;  /* on/off highlight */
        int        oneof;  /* "one of" highlight */
        int        bar;    /* place bar under */
        int        id;     /* id of menu item */
        char*      face;   /* text to place on button */

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
typedef struct pa_strrec* pa_strptr;
typedef struct pa_strrec {

    pa_strptr next; /* next entry in list */
    char*    str;  /* string */

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
typedef enum { pa_qfteblink, pa_qftereverse, pa_qfteunderline, pa_qftesuperscript,
                  pa_qftesubscript, pa_qfteitalic, pa_qftebold, pa_qftestrikeout,
                  pa_qftestandout, pa_qftecondensed, pa_qfteextended, pa_qftexlight,
                  pa_qftelight, pa_qftexbold, pa_qftehollow, pa_qfteraised} pa_qfteffect;
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
int pa_curx(FILE* f);
int pa_cury(FILE* f);
int pa_curbnd(FILE* f);
void pa_select(FILE* f, int u, int d);
void pa_event(FILE* f, pa_evtrec* er);
void pa_timer(FILE* f, int i, long t, int r);
void pa_killtimer(FILE* f, int i);
int pa_mouse(FILE* f);
int pa_mousebutton(FILE* f, int m);
int pa_joystick(FILE* f);
int pa_joybutton(FILE* f, int j);
int pa_joyaxis(FILE* f, int j);
void pa_settab(FILE* f, int t);
void pa_restab(FILE* f, int t);
void pa_clrtab(FILE* f);
int pa_funkey(FILE* f);
void pa_frametimer(FILE* f, int e);
void pa_autohold(int e);
void pa_wrtstr(FILE* f, char* s);
void pa_eventover(pa_evtcod e, pa_pevthan eh,  pa_pevthan* oeh);
void pa_eventsover(pa_pevthan eh,  pa_pevthan* oeh);
void pa_sendevent(FILE* f, pa_evtrec* er);

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
void pa_fand(FILE* f);
void pa_band(FILE* f);
void pa_for(FILE* f);
void pa_bor(FILE* f);
int pa_chrsizx(FILE* f);
int pa_chrsizy(FILE* f);
int pa_fonts(FILE* f);
void pa_font(FILE* f, int fc);
void pa_fontnam(FILE* f, int fc, char* fns, int fnsl);
void pa_fontsiz(FILE* f, int s);
void pa_chrspcy(FILE* f, int s);
void pa_chrspcx(FILE* f, int s);
int pa_dpmx(FILE* f);
int pa_dpmy(FILE* f);
int pa_strsiz(FILE* f, const char* s);
int pa_chrpos(FILE* f, const char* s, int p);
void pa_writejust(FILE* f, const char* s, int n);
int pa_justpos(FILE* f, const char* s, int p, int n);
void pa_condensed(FILE* f, int e);
void pa_extended(FILE* f, int e);
void pa_xlight(FILE* f, int e);
void pa_light(FILE* f, int e);
void pa_xbold(FILE* f, int e);
void pa_hollow(FILE* f, int e);
void pa_raised(FILE* f, int e);
void pa_settabg(FILE* f, int t);
void pa_restabg(FILE* f, int t);
void pa_fcolorg(FILE* f, int r, int g, int b);
void pa_fcolorc(FILE* f, int r, int g, int b);
void pa_bcolorg(FILE* f, int r, int g, int b);
void pa_bcolorc(FILE* f, int r, int g, int b);
void pa_loadpict(FILE* f, int p, char* fn);
int pa_pictsizx(FILE* f, int p);
int pa_pictsizy(FILE* f, int p);
void pa_picture(FILE* f, int p, int x1, int y1, int x2, int y2);
void pa_delpict(FILE* f, int p);
void pa_scrollg(FILE* f, int x, int y);
void pa_chrangle(FILE* f, int a);

/* Window management functions */

void pa_title(FILE* f, char* ts);
void pa_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid);
void pa_buffer(FILE* f, int e);
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
void pa_winclient(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms);
void pa_winclientg(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms);
void pa_front(FILE* f);
void pa_back(FILE* f);
void pa_frame(FILE* f, int e);
void pa_sizable(FILE* f, int e);
void pa_sysbar(FILE* f, int e);
void pa_menu(FILE* f, pa_menuptr m);
void pa_menuena(FILE* f, int id, int onoff);
void pa_menusel(FILE* f, int id, int select);
void pa_stdmenu(pa_stdmenusel sms, pa_menuptr* sm, pa_menuptr pm);
int pa_getwinid(void);
void pa_focus(FILE* f);

/* widgets/controls */

int pa_getwigid(FILE* f);
void pa_killwidget(FILE* f, int id);
void pa_selectwidget(FILE* f, int id, int e);
void pa_enablewidget(FILE* f, int id, int e);
void pa_getwidgettext(FILE* f, int id, char* s, int sl);
void pa_putwidgettext(FILE* f, int id, char* s);
void pa_sizwidget(FILE* f, int id, int x, int y);
void pa_sizwidgetg(FILE* f, int id, int x, int y);
void pa_poswidget(FILE* f, int id, int x, int y);
void pa_poswidgetg(FILE* f, int id, int x, int y);
void pa_backwidget(FILE* f, int id);
void pa_frontwidget(FILE* f, int id);
void pa_focuswidget(FILE* f, int id);
void pa_buttonsiz(FILE* f, char* s, int* w, int* h);
void pa_buttonsizg(FILE* f, char* s, int* w, int* h);
void pa_button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void pa_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void pa_checkboxsiz(FILE* f, char* s, int* w, int* h);
void pa_checkboxsizg(FILE* f, char* s, int* w, int* h);
void pa_checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void pa_checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void pa_radiobuttonsiz(FILE* f, char* s, int* w, int* h);
void pa_radiobuttonsizg(FILE* f, char* s, int* w, int* h);
void pa_radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void pa_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void pa_groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
void pa_groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox,
                 int* oy);
void pa_group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void pa_groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void pa_background(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id);
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
void pa_numselboxsizg(FILE* f, int l, int u, int* w, int* h);
void pa_numselboxsiz(FILE* f, int l, int u, int* w, int* h);
void pa_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                  int id);
void pa_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                   int id);
void pa_editboxsizg(FILE* f, char* s, int* w, int* h);
void pa_editboxsiz(FILE* f, char* s, int* w, int* h);
void pa_editbox(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_editboxg(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_progbarsizg(FILE* f, int* w, int* h);
void pa_progbarsiz(FILE* f, int* w, int* h);
void pa_progbar(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_progbarg(FILE* f, int x1, int y1, int x2, int y2, int id);
void pa_progbarpos(FILE* f, int id, int pos);
void pa_listboxsizg(FILE* f, pa_strptr sp, int* w, int* h);
void pa_listboxsiz(FILE* f, pa_strptr sp, int* w, int* h);
void pa_listbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
void pa_listboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
void pa_dropboxsizg(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh);
void pa_dropboxsiz(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh);
void pa_dropbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
void pa_dropboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
void pa_dropeditboxsizg(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh);
void pa_dropeditboxsiz(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh);
void pa_dropeditbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
void pa_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
                     int id);
void pa_slidehorizsizg(FILE* f, int* w, int* h);
void pa_slidehorizsiz(FILE* f, int* w, int* h);
void pa_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void pa_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void pa_slidevertsizg(FILE* f, int* w, int* h);
void pa_slidevertsiz(FILE* f, int* w, int* h);
void pa_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void pa_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void pa_tabbarsizg(FILE* f, pa_tabori tor, int cw, int ch, int* w, int* h,
                   int* ox, int* oy);
void pa_tabbarsiz(FILE* f, pa_tabori tor, int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
void pa_tabbarclientg(FILE* f, pa_tabori tor, int w, int h, int* cw, int* ch,
                      int* ox, int* oy);
void pa_tabbarclient(FILE* f, pa_tabori tor, int w, int h, int* cw, int* ch,
                     int* ox, int* oy);
void pa_tabbar(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
               pa_tabori tor, int id);
void pa_tabbarg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
                pa_tabori tor, int id);
void pa_tabsel(FILE* f, int id, int tn);
void pa_alert(char* title, char* message);
void pa_querycolor(int* r, int* g, int* b);
void pa_queryopen(char* s, int sl);
void pa_querysave(char* s, int sl);
void pa_queryfind(char* s, int sl, pa_qfnopts* opt);
void pa_queryfindrep(char* s, int sl, char* r, int rl, pa_qfropts* opt);
void pa_queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb, int* br,
                  int* bg, int* bb, pa_qfteffects* effect);

#endif /* __GRAPH_H__ */
