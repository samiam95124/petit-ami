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

#ifdef __cplusplus
extern "C" {
#endif

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
    pa_etfocus,    /* window has focus */
    pa_etnofocus,  /* window lost focus */
    pa_ethover,    /* window being hovered */
    pa_etnohover,  /* window stopped being hovered */
    pa_etterm,     /* terminate program */
    pa_etframe,    /* frame sync */
    pa_etmoumovg,  /* mouse move graphical */
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
        /* pa_etfun */
        /** function key */ int fkey;
        /* pa_etresize */
        struct {

            int rszx, rszy, rszxg, rszyg;

        };

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
void pa_wrtstrn(FILE* f, char* s, int n);
void pa_sizbuf(FILE* f, int x, int y);
void pa_title(FILE* f, char* ts);
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
void pa_path(FILE* f, int a);

/* Window management functions */

void pa_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid);
void pa_buffer(FILE* f, int e);
void pa_sizbufg(FILE* f, int x, int y);
void pa_getsiz(FILE* f, int* x, int* y);
void pa_getsizg(FILE* f, int* x, int* y);
void pa_setsiz(FILE* f, int x, int y);
void pa_setsizg(FILE* f, int x, int y);
void pa_setpos(FILE* f, int x, int y);
void pa_setposg(FILE* f, int x, int y);
void pa_scnsiz(FILE* f, int* x, int* y);
void pa_scnsizg(FILE* f, int* x, int*y);
void pa_scncen(FILE* f, int* x, int* y);
void pa_scnceng(FILE* f, int* x, int* y);
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

/*
 * Override vector types
 *
 */
typedef void (*pa_cursor_t)(FILE* f, int x, int y);
typedef int (*pa_maxx_t)(FILE* f);
typedef int (*pa_maxy_t)(FILE* f);
typedef void (*pa_home_t)(FILE* f);
typedef void (*pa_del_t)(FILE* f);
typedef void (*pa_up_t)(FILE* f);
typedef void (*pa_down_t)(FILE* f);
typedef void (*pa_left_t)(FILE* f);
typedef void (*pa_right_t)(FILE* f);
typedef void (*pa_blink_t)(FILE* f, int e);
typedef void (*pa_reverse_t)(FILE* f, int e);
typedef void (*pa_underline_t)(FILE* f, int e);
typedef void (*pa_superscript_t)(FILE* f, int e);
typedef void (*pa_subscript_t)(FILE* f, int e);
typedef void (*pa_italic_t)(FILE* f, int e);
typedef void (*pa_bold_t)(FILE* f, int e);
typedef void (*pa_strikeout_t)(FILE* f, int e);
typedef void (*pa_standout_t)(FILE* f, int e);
typedef void (*pa_fcolor_t)(FILE* f, pa_color c);
typedef void (*pa_bcolor_t)(FILE* f, pa_color c);
typedef void (*pa_auto_t)(FILE* f, int e);
typedef void (*pa_curvis_t)(FILE* f, int e);
typedef void (*pa_scroll_t)(FILE* f, int x, int y);
typedef int (*pa_curx_t)(FILE* f);
typedef int (*pa_cury_t)(FILE* f);
typedef int (*pa_curbnd_t)(FILE* f);
typedef void (*pa_select_t)(FILE* f, int u, int d);
typedef void (*pa_event_t)(FILE* f, pa_evtrec* er);
typedef void (*pa_timer_t)(FILE* f, int i, long t, int r);
typedef void (*pa_killtimer_t)(FILE* f, int i);
typedef int (*pa_mouse_t)(FILE* f);
typedef int (*pa_mousebutton_t)(FILE* f, int m);
typedef int (*pa_joystick_t)(FILE* f);
typedef int (*pa_joybutton_t)(FILE* f, int j);
typedef int (*pa_joyaxis_t)(FILE* f, int j);
typedef void (*pa_settab_t)(FILE* f, int t);
typedef void (*pa_restab_t)(FILE* f, int t);
typedef void (*pa_clrtab_t)(FILE* f);
typedef int (*pa_funkey_t)(FILE* f);
typedef void (*pa_frametimer_t)(FILE* f, int e);
typedef void (*pa_autohold_t)(int e);
typedef void (*pa_wrtstr_t)(FILE* f, char* s);
typedef void (*pa_wrtstrn_t)(FILE* f, char* s, int n);
typedef void (*pa_sizbuf_t)(FILE* f, int x, int y);
typedef void (*pa_title_t)(FILE* f, char* ts);
typedef void (*pa_fcolorc_t)(FILE* f, int r, int g, int b);
typedef void (*pa_bcolorc_t)(FILE* f, int r, int g, int b);
typedef void (*pa_eventover_t)(pa_evtcod e, pa_pevthan eh,  pa_pevthan* oeh);
typedef void (*pa_eventsover_t)(pa_pevthan eh,  pa_pevthan* oeh);
typedef void (*pa_sendevent_t)(FILE* f, pa_evtrec* er);
typedef int (*pa_maxxg_t)(FILE* f);
typedef int (*pa_maxyg_t)(FILE* f);
typedef int (*pa_curxg_t)(FILE* f);
typedef int (*pa_curyg_t)(FILE* f);
typedef void (*pa_line_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*pa_linewidth_t)(FILE* f, int w);
typedef void (*pa_rect_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*pa_frect_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*pa_rrect_t)(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
typedef void (*pa_frrect_t)(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
typedef void (*pa_ellipse_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*pa_fellipse_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*pa_arc_t)(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
typedef void (*pa_farc_t)(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
typedef void (*pa_fchord_t)(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
typedef void (*pa_ftriangle_t)(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3);
typedef void (*pa_cursorg_t)(FILE* f, int x, int y);
typedef int (*pa_baseline_t)(FILE* f);
typedef void (*pa_setpixel_t)(FILE* f, int x, int y);
typedef void (*pa_fover_t)(FILE* f);
typedef void (*pa_bover_t)(FILE* f);
typedef void (*pa_finvis_t)(FILE* f);
typedef void (*pa_binvis_t)(FILE* f);
typedef void (*pa_fxor_t)(FILE* f);
typedef void (*pa_bxor_t)(FILE* f);
typedef void (*pa_fand_t)(FILE* f);
typedef void (*pa_band_t)(FILE* f);
typedef void (*pa_for_t)(FILE* f);
typedef void (*pa_bor_t)(FILE* f);
typedef int (*pa_chrsizx_t)(FILE* f);
typedef int (*pa_chrsizy_t)(FILE* f);
typedef int (*pa_fonts_t)(FILE* f);
typedef void (*pa_font_t)(FILE* f, int fc);
typedef void (*pa_fontnam_t)(FILE* f, int fc, char* fns, int fnsl);
typedef void (*pa_fontsiz_t)(FILE* f, int s);
typedef void (*pa_chrspcy_t)(FILE* f, int s);
typedef void (*pa_chrspcx_t)(FILE* f, int s);
typedef int (*pa_dpmx_t)(FILE* f);
typedef int (*pa_dpmy_t)(FILE* f);
typedef int (*pa_strsiz_t)(FILE* f, const char* s);
typedef int (*pa_chrpos_t)(FILE* f, const char* s, int p);
typedef void (*pa_writejust_t)(FILE* f, const char* s, int n);
typedef int (*pa_justpos_t)(FILE* f, const char* s, int p, int n);
typedef void (*pa_condensed_t)(FILE* f, int e);
typedef void (*pa_extended_t)(FILE* f, int e);
typedef void (*pa_xlight_t)(FILE* f, int e);
typedef void (*pa_light_t)(FILE* f, int e);
typedef void (*pa_xbold_t)(FILE* f, int e);
typedef void (*pa_hollow_t)(FILE* f, int e);
typedef void (*pa_raised_t)(FILE* f, int e);
typedef void (*pa_settabg_t)(FILE* f, int t);
typedef void (*pa_restabg_t)(FILE* f, int t);
typedef void (*pa_fcolorg_t)(FILE* f, int r, int g, int b);
typedef void (*pa_bcolorg_t)(FILE* f, int r, int g, int b);
typedef void (*pa_loadpict_t)(FILE* f, int p, char* fn);
typedef int (*pa_pictsizx_t)(FILE* f, int p);
typedef int (*pa_pictsizy_t)(FILE* f, int p);
typedef void (*pa_picture_t)(FILE* f, int p, int x1, int y1, int x2, int y2);
typedef void (*pa_delpict_t)(FILE* f, int p);
typedef void (*pa_scrollg_t)(FILE* f, int x, int y);
typedef void (*pa_path_t)(FILE* f, int a);
typedef void (*pa_openwin_t)(FILE** infile, FILE** outfile, FILE* parent, int wid);
typedef void (*pa_buffer_t)(FILE* f, int e);
typedef void (*pa_sizbufg_t)(FILE* f, int x, int y);
typedef void (*pa_getsiz_t)(FILE* f, int* x, int* y);
typedef void (*pa_getsizg_t)(FILE* f, int* x, int* y);
typedef void (*pa_setsiz_t)(FILE* f, int x, int y);
typedef void (*pa_setsizg_t)(FILE* f, int x, int y);
typedef void (*pa_setpos_t)(FILE* f, int x, int y);
typedef void (*pa_setposg_t)(FILE* f, int x, int y);
typedef void (*pa_scnsiz_t)(FILE* f, int* x, int* y);
typedef void (*pa_scnsizg_t)(FILE* f, int* x, int*y);
typedef void (*pa_scncen_t)(FILE* f, int* x, int* y);
typedef void (*pa_scnceng_t)(FILE* f, int* x, int* y);
typedef void (*pa_winclient_t)(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms);
typedef void (*pa_winclientg_t)(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms);
typedef void (*pa_front_t)(FILE* f);
typedef void (*pa_back_t)(FILE* f);
typedef void (*pa_frame_t)(FILE* f, int e);
typedef void (*pa_sizable_t)(FILE* f, int e);
typedef void (*pa_sysbar_t)(FILE* f, int e);
typedef void (*pa_menu_t)(FILE* f, pa_menuptr m);
typedef void (*pa_menuena_t)(FILE* f, int id, int onoff);
typedef void (*pa_menusel_t)(FILE* f, int id, int select);
typedef void (*pa_stdmenu_t)(pa_stdmenusel sms, pa_menuptr* sm, pa_menuptr pm);
typedef int (*pa_getwinid_t)(void);
typedef void (*pa_focus_t)(FILE* f);
typedef int (*pa_getwigid_t)(FILE* f);
typedef void (*pa_killwidget_t)(FILE* f, int id);
typedef void (*pa_selectwidget_t)(FILE* f, int id, int e);
typedef void (*pa_enablewidget_t)(FILE* f, int id, int e);
typedef void (*pa_getwidgettext_t)(FILE* f, int id, char* s, int sl);
typedef void (*pa_putwidgettext_t)(FILE* f, int id, char* s);
typedef void (*pa_sizwidget_t)(FILE* f, int id, int x, int y);
typedef void (*pa_sizwidgetg_t)(FILE* f, int id, int x, int y);
typedef void (*pa_poswidget_t)(FILE* f, int id, int x, int y);
typedef void (*pa_poswidgetg_t)(FILE* f, int id, int x, int y);
typedef void (*pa_backwidget_t)(FILE* f, int id);
typedef void (*pa_frontwidget_t)(FILE* f, int id);
typedef void (*pa_focuswidget_t)(FILE* f, int id);
typedef void (*pa_buttonsiz_t)(FILE* f, char* s, int* w, int* h);
typedef void (*pa_buttonsizg_t)(FILE* f, char* s, int* w, int* h);
typedef void (*pa_button_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*pa_buttong_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*pa_checkboxsiz_t)(FILE* f, char* s, int* w, int* h);
typedef void (*pa_checkboxsizg_t)(FILE* f, char* s, int* w, int* h);
typedef void (*pa_checkbox_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*pa_checkboxg_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*pa_radiobuttonsiz_t)(FILE* f, char* s, int* w, int* h);
typedef void (*pa_radiobuttonsizg_t)(FILE* f, char* s, int* w, int* h);
typedef void (*pa_radiobutton_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*pa_radiobuttong_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*pa_groupsizg_t)(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
typedef void (*pa_groupsiz_t)(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox,
                 int* oy);
typedef void (*pa_group_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*pa_groupg_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*pa_background_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_backgroundg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_scrollvertsizg_t)(FILE* f, int* w, int* h);
typedef void (*pa_scrollvertsiz_t)(FILE* f, int* w, int* h);
typedef void (*pa_scrollvert_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_scrollvertg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_scrollhorizsizg_t)(FILE* f, int* w, int* h);
typedef void (*pa_scrollhorizsiz_t)(FILE* f, int* w, int* h);
typedef void (*pa_scrollhoriz_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_scrollhorizg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_scrollpos_t)(FILE* f, int id, int r);
typedef void (*pa_scrollsiz_t)(FILE* f, int id, int r);
typedef void (*pa_numselboxsizg_t)(FILE* f, int l, int u, int* w, int* h);
typedef void (*pa_numselboxsiz_t)(FILE* f, int l, int u, int* w, int* h);
typedef void (*pa_numselbox_t)(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                  int id);
typedef void (*pa_numselboxg_t)(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                   int id);
typedef void (*pa_editboxsizg_t)(FILE* f, char* s, int* w, int* h);
typedef void (*pa_editboxsiz_t)(FILE* f, char* s, int* w, int* h);
typedef void (*pa_editbox_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_editboxg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_progbarsizg_t)(FILE* f, int* w, int* h);
typedef void (*pa_progbarsiz_t)(FILE* f, int* w, int* h);
typedef void (*pa_progbar_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_progbarg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*pa_progbarpos_t)(FILE* f, int id, int pos);
typedef void (*pa_listboxsizg_t)(FILE* f, pa_strptr sp, int* w, int* h);
typedef void (*pa_listboxsiz_t)(FILE* f, pa_strptr sp, int* w, int* h);
typedef void (*pa_listbox_t)(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
typedef void (*pa_listboxg_t)(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
typedef void (*pa_dropboxsizg_t)(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh);
typedef void (*pa_dropboxsiz_t)(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh);
typedef void (*pa_dropbox_t)(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
typedef void (*pa_dropboxg_t)(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
typedef void (*pa_dropeditboxsizg_t)(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh);
typedef void (*pa_dropeditboxsiz_t)(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh);
typedef void (*pa_dropeditbox_t)(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id);
typedef void (*pa_dropeditboxg_t)(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
                     int id);
typedef void (*pa_slidehorizsizg_t)(FILE* f, int* w, int* h);
typedef void (*pa_slidehorizsiz_t)(FILE* f, int* w, int* h);
typedef void (*pa_slidehoriz_t)(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
typedef void (*pa_slidehorizg_t)(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
typedef void (*pa_slidevertsizg_t)(FILE* f, int* w, int* h);
typedef void (*pa_slidevertsiz_t)(FILE* f, int* w, int* h);
typedef void (*pa_slidevert_t)(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
typedef void (*pa_slidevertg_t)(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
typedef void (*pa_tabbarsizg_t)(FILE* f, pa_tabori tor, int cw, int ch, int* w, int* h,
                   int* ox, int* oy);
typedef void (*pa_tabbarsiz_t)(FILE* f, pa_tabori tor, int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
typedef void (*pa_tabbarclientg_t)(FILE* f, pa_tabori tor, int w, int h, int* cw, int* ch,
                      int* ox, int* oy);
typedef void (*pa_tabbarclient_t)(FILE* f, pa_tabori tor, int w, int h, int* cw, int* ch,
                     int* ox, int* oy);
typedef void (*pa_tabbar_t)(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
               pa_tabori tor, int id);
typedef void (*pa_tabbarg_t)(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
                pa_tabori tor, int id);
typedef void (*pa_tabsel_t)(FILE* f, int id, int tn);
typedef void (*pa_alert_t)(char* title, char* message);
typedef void (*pa_querycolor_t)(int* r, int* g, int* b);
typedef void (*pa_queryopen_t)(char* s, int sl);
typedef void (*pa_querysave_t)(char* s, int sl);
typedef void (*pa_queryfind_t)(char* s, int sl, pa_qfnopts* opt);
typedef void (*pa_queryfindrep_t)(char* s, int sl, char* r, int rl, pa_qfropts* opt);
typedef void (*pa_queryfont_t)(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb, int* br,
                  int* bg, int* bb, pa_qfteffects* effect);

/*
 * Overrider routines
 */
void _pa_scrollg_ovr(pa_scrollg_t nfp, pa_scrollg_t* ofp);
void _pa_scroll_ovr(pa_scroll_t nfp, pa_scroll_t* ofp);
void _pa_cursor_ovr(pa_cursor_t nfp, pa_cursor_t* ofp);
void _pa_cursorg_ovr(pa_cursorg_t nfp, pa_cursorg_t* ofp);
void _pa_baseline_ovr(pa_baseline_t nfp, pa_baseline_t* ofp);
void _pa_maxx_ovr(pa_maxx_t nfp, pa_maxx_t* ofp);
void _pa_maxy_ovr(pa_maxy_t nfp, pa_maxy_t* ofp);
void _pa_maxxg_ovr(pa_maxxg_t nfp, pa_maxxg_t* ofp);
void _pa_maxyg_ovr(pa_maxyg_t nfp, pa_maxyg_t* ofp);
void _pa_home_ovr(pa_home_t nfp, pa_home_t* ofp);
void _pa_up_ovr(pa_up_t nfp, pa_up_t* ofp);
void _pa_down_ovr(pa_down_t nfp, pa_down_t* ofp);
void _pa_left_ovr(pa_left_t nfp, pa_left_t* ofp);
void _pa_right_ovr(pa_right_t nfp, pa_right_t* ofp);
void _pa_blink_ovr(pa_blink_t nfp, pa_blink_t* ofp);
void _pa_reverse_ovr(pa_reverse_t nfp, pa_reverse_t* ofp);
void _pa_underline_ovr(pa_underline_t nfp, pa_underline_t* ofp);
void _pa_superscript_ovr(pa_superscript_t nfp, pa_superscript_t* ofp);
void _pa_subscript_ovr(pa_subscript_t nfp, pa_subscript_t* ofp);
void _pa_italic_ovr(pa_italic_t nfp, pa_italic_t* ofp);
void _pa_bold_ovr(pa_bold_t nfp, pa_bold_t* ofp);
void _pa_strikeout_ovr(pa_strikeout_t nfp, pa_strikeout_t* ofp);
void _pa_standout_ovr(pa_standout_t nfp, pa_standout_t* ofp);
void _pa_fcolor_ovr(pa_fcolor_t nfp, pa_fcolor_t* ofp);
void _pa_fcolorc_ovr(pa_fcolorc_t nfp, pa_fcolorc_t* ofp);
void _pa_fcolorg_ovr(pa_fcolorg_t nfp, pa_fcolorg_t* ofp);
void _pa_bcolor_ovr(pa_bcolor_t nfp, pa_bcolor_t* ofp);
void _pa_bcolorc_ovr(pa_bcolorc_t nfp, pa_bcolorc_t* ofp);
void _pa_bcolorg_ovr(pa_bcolorg_t nfp, pa_bcolorg_t* ofp);
void _pa_curbnd_ovr(pa_curbnd_t nfp, pa_curbnd_t* ofp);
void _pa_auto_ovr(pa_auto_t nfp, pa_auto_t* ofp);
void _pa_curvis_ovr(pa_curvis_t nfp, pa_curvis_t* ofp);
void _pa_curx_ovr(pa_curx_t nfp, pa_curx_t* ofp);
void _pa_cury_ovr(pa_cury_t nfp, pa_cury_t* ofp);
void _pa_curxg_ovr(pa_curxg_t nfp, pa_curxg_t* ofp);
void _pa_curyg_ovr(pa_curyg_t nfp, pa_curyg_t* ofp);
void _pa_select_ovr(pa_select_t nfp, pa_select_t* ofp);
void _pa_wrtstr_ovr(pa_wrtstr_t nfp, pa_wrtstr_t* ofp);
void _pa_sizbuf_ovr(pa_sizbuf_t nfp, pa_sizbuf_t* ofp);
void _pa_del_ovr(pa_del_t nfp, pa_del_t* ofp);
void _pa_line_ovr(pa_line_t nfp, pa_line_t* ofp);
void _pa_rect_ovr(pa_rect_t nfp, pa_rect_t* ofp);
void _pa_frect_ovr(pa_frect_t nfp, pa_frect_t* ofp);
void _pa_rrect_ovr(pa_rrect_t nfp, pa_rrect_t* ofp);
void _pa_frrect_ovr(pa_frrect_t nfp, pa_frrect_t* ofp);
void _pa_ellipse_ovr(pa_ellipse_t nfp, pa_ellipse_t* ofp);
void _pa_fellipse_ovr(pa_fellipse_t nfp, pa_fellipse_t* ofp);
void _pa_arc_ovr(pa_arc_t nfp, pa_arc_t* ofp);
void _pa_farc_ovr(pa_farc_t nfp, pa_farc_t* ofp);
void _pa_fchord_ovr(pa_fchord_t nfp, pa_fchord_t* ofp);
void _pa_ftriangle_ovr(pa_ftriangle_t nfp, pa_ftriangle_t* ofp);
void _pa_setpixel_ovr(pa_setpixel_t nfp, pa_setpixel_t* ofp);
void _pa_fover_ovr(pa_fover_t nfp, pa_fover_t* ofp);
void _pa_bover_ovr(pa_bover_t nfp, pa_bover_t* ofp);
void _pa_finvis_ovr(pa_finvis_t nfp, pa_finvis_t* ofp);
void _pa_binvis_ovr(pa_binvis_t nfp, pa_binvis_t* ofp);
void _pa_fxor_ovr(pa_fxor_t nfp, pa_fxor_t* ofp);
void _pa_bxor_ovr(pa_bxor_t nfp, pa_bxor_t* ofp);
void _pa_fand_ovr(pa_fand_t nfp, pa_fand_t* ofp);
void _pa_band_ovr(pa_band_t nfp, pa_band_t* ofp);
void _pa_for_ovr(pa_for_t nfp, pa_for_t* ofp);
void _pa_bor_ovr(pa_bor_t nfp, pa_bor_t* ofp);
void _pa_linewidth_ovr(pa_linewidth_t nfp, pa_linewidth_t* ofp);
void _pa_chrsizx_ovr(pa_chrsizx_t nfp, pa_chrsizx_t* ofp);
void _pa_chrsizy_ovr(pa_chrsizy_t nfp, pa_chrsizy_t* ofp);
void _pa_fonts_ovr(pa_fonts_t nfp, pa_fonts_t* ofp);
void _pa_font_ovr(pa_font_t nfp, pa_font_t* ofp);
void _pa_fontnam_ovr(pa_fontnam_t nfp, pa_fontnam_t* ofp);
void _pa_fontsiz_ovr(pa_fontsiz_t nfp, pa_fontsiz_t* ofp);
void _pa_chrspcy_ovr(pa_chrspcy_t nfp, pa_chrspcy_t* ofp);
void _pa_chrspcx_ovr(pa_chrspcx_t nfp, pa_chrspcx_t* ofp);
void _pa_dpmx_ovr(pa_dpmx_t nfp, pa_dpmx_t* ofp);
void _pa_dpmy_ovr(pa_dpmy_t nfp, pa_dpmy_t* ofp);
void _pa_strsiz_ovr(pa_strsiz_t nfp, pa_strsiz_t* ofp);
void _pa_chrpos_ovr(pa_chrpos_t nfp, pa_chrpos_t* ofp);
void _pa_writejust_ovr(pa_writejust_t nfp, pa_writejust_t* ofp);
void _pa_justpos_ovr(pa_justpos_t nfp, pa_justpos_t* ofp);
void _pa_condensed_ovr(pa_condensed_t nfp, pa_condensed_t* ofp);
void _pa_extended_ovr(pa_extended_t nfp, pa_extended_t* ofp);
void _pa_xlight_ovr(pa_xlight_t nfp, pa_xlight_t* ofp);
void _pa_light_ovr(pa_light_t nfp, pa_light_t* ofp);
void _pa_xbold_ovr(pa_xbold_t nfp, pa_xbold_t* ofp);
void _pa_hollow_ovr(pa_hollow_t nfp, pa_hollow_t* ofp);
void _pa_raised_ovr(pa_raised_t nfp, pa_raised_t* ofp);
void _pa_delpict_ovr(pa_delpict_t nfp, pa_delpict_t* ofp);
void _pa_loadpict_ovr(pa_loadpict_t nfp, pa_loadpict_t* ofp);
void _pa_pictsizx_ovr(pa_pictsizx_t nfp, pa_pictsizx_t* ofp);
void _pa_pictsizy_ovr(pa_pictsizy_t nfp, pa_pictsizy_t* ofp);
void _pa_picture_ovr(pa_picture_t nfp, pa_picture_t* ofp);
void _pa_event_ovr(pa_event_t nfp, pa_event_t* ofp);
void _pa_sendevent_ovr(pa_sendevent_t nfp, pa_sendevent_t* ofp);
void _pa_eventover_ovr(pa_eventover_t nfp, pa_eventover_t* ofp);
void _pa_eventsover_ovr(pa_eventsover_t nfp, pa_eventsover_t* ofp);
void _pa_timer_ovr(pa_timer_t nfp, pa_timer_t* ofp);
void _pa_killtimer_ovr(pa_killtimer_t nfp, pa_killtimer_t* ofp);
void _pa_frametimer_ovr(pa_frametimer_t nfp, pa_frametimer_t* ofp);
void _pa_autohold_ovr(pa_autohold_t nfp, pa_autohold_t* ofp);
void _pa_mouse_ovr(pa_mouse_t nfp, pa_mouse_t* ofp);
void _pa_mousebutton_ovr(pa_mousebutton_t nfp, pa_mousebutton_t* ofp);
void _pa_joystick_ovr(pa_joystick_t nfp, pa_joystick_t* ofp);
void _pa_joybutton_ovr(pa_joybutton_t nfp, pa_joybutton_t* ofp);
void _pa_joyaxis_ovr(pa_joyaxis_t nfp, pa_joyaxis_t* ofp);
void _pa_settabg_ovr(pa_settabg_t nfp, pa_settabg_t* ofp);
void _pa_settab_ovr(pa_settab_t nfp, pa_settab_t* ofp);
void _pa_restabg_ovr(pa_restabg_t nfp, pa_restabg_t* ofp);
void _pa_restab_ovr(pa_restab_t nfp, pa_restab_t* ofp);
void _pa_clrtab_ovr(pa_clrtab_t nfp, pa_clrtab_t* ofp);
void _pa_funkey_ovr(pa_funkey_t nfp, pa_funkey_t* ofp);
void _pa_title_ovr(pa_title_t nfp, pa_title_t* ofp);
void _pa_getwinid_ovr(pa_getwinid_t nfp, pa_getwinid_t* ofp);
void _pa_openwin_ovr(pa_openwin_t nfp, pa_openwin_t* ofp);
void _pa_sizbufg_ovr(pa_sizbufg_t nfp, pa_sizbufg_t* ofp);
void _pa_buffer_ovr(pa_buffer_t nfp, pa_buffer_t* ofp);
void _pa_menu_ovr(pa_menu_t nfp, pa_menu_t* ofp);
void _pa_menuena_ovr(pa_menuena_t nfp, pa_menuena_t* ofp);
void _pa_menusel_ovr(pa_menusel_t nfp, pa_menusel_t* ofp);
void _pa_stdmenu_ovr(pa_stdmenu_t nfp, pa_stdmenu_t* ofp);
void _pa_front_ovr(pa_front_t nfp, pa_front_t* ofp);
void _pa_back_ovr(pa_back_t nfp, pa_back_t* ofp);
void _pa_getsizg_ovr(pa_getsizg_t nfp, pa_getsizg_t* ofp);
void _pa_getsiz_ovr(pa_getsiz_t nfp, pa_getsiz_t* ofp);
void _pa_setsizg_ovr(pa_setsizg_t nfp, pa_setsizg_t* ofp);
void _pa_setsiz_ovr(pa_setsiz_t nfp, pa_setsiz_t* ofp);
void _pa_setposg_ovr(pa_setposg_t nfp, pa_setposg_t* ofp);
void _pa_setpos_ovr(pa_setpos_t nfp, pa_setpos_t* ofp);
void _pa_scnsizg_ovr(pa_scnsizg_t nfp, pa_scnsizg_t* ofp);
void _pa_scnsiz_ovr(pa_scnsiz_t nfp, pa_scnsiz_t* ofp);
void _pa_scnceng_ovr(pa_scnceng_t nfp, pa_scnceng_t* ofp);
void _pa_scncen_ovr(pa_scncen_t nfp, pa_scncen_t* ofp);
void _pa_winclientg_ovr(pa_winclientg_t nfp, pa_winclientg_t* ofp);
void _pa_winclient_ovr(pa_winclient_t nfp, pa_winclient_t* ofp);
void _pa_frame_ovr(pa_frame_t nfp, pa_frame_t* ofp);
void _pa_sizable_ovr(pa_sizable_t nfp, pa_sizable_t* ofp);
void _pa_sysbar_ovr(pa_sysbar_t nfp, pa_sysbar_t* ofp);
void _pa_focus_ovr(pa_focus_t nfp, pa_focus_t* ofp);
void _pa_path_ovr(pa_path_t nfp, pa_path_t* ofp);
void _pa_getwigid_ovr(pa_getwigid_t nfp, pa_getwigid_t* ofp);
void _pa_killwidget_ovr(pa_killwidget_t nfp, pa_killwidget_t* ofp);
void _pa_selectwidget_ovr(pa_selectwidget_t nfp, pa_selectwidget_t* ofp);
void _pa_enablewidget_ovr(pa_enablewidget_t nfp, pa_enablewidget_t* ofp);
void _pa_getwidgettext_ovr(pa_getwidgettext_t nfp, pa_getwidgettext_t* ofp);
void _pa_putwidgettext_ovr(pa_putwidgettext_t nfp, pa_putwidgettext_t* ofp);
void _pa_sizwidget_ovr(pa_sizwidget_t nfp, pa_sizwidget_t* ofp);
void _pa_sizwidgetg_ovr(pa_sizwidgetg_t nfp, pa_sizwidgetg_t* ofp);
void _pa_poswidget_ovr(pa_poswidget_t nfp, pa_poswidget_t* ofp);
void _pa_poswidgetg_ovr(pa_poswidgetg_t nfp, pa_poswidgetg_t* ofp);
void _pa_backwidget_ovr(pa_backwidget_t nfp, pa_backwidget_t* ofp);
void _pa_frontwidget_ovr(pa_frontwidget_t nfp, pa_frontwidget_t* ofp);
void _pa_focuswidget_ovr(pa_focuswidget_t nfp, pa_focuswidget_t* ofp);
void _pa_buttonsiz_ovr(pa_buttonsiz_t nfp, pa_buttonsiz_t* ofp);
void _pa_buttonsizg_ovr(pa_buttonsizg_t nfp, pa_buttonsizg_t* ofp);
void _pa_button_ovr(pa_button_t nfp, pa_button_t* ofp);
void _pa_buttong_ovr(pa_buttong_t nfp, pa_buttong_t* ofp);
void _pa_checkboxsiz_ovr(pa_checkboxsiz_t nfp, pa_checkboxsiz_t* ofp);
void _pa_checkboxsizg_ovr(pa_checkboxsizg_t nfp, pa_checkboxsizg_t* ofp);
void _pa_checkbox_ovr(pa_checkbox_t nfp, pa_checkbox_t* ofp);
void _pa_checkboxg_ovr(pa_checkboxg_t nfp, pa_checkboxg_t* ofp);
void _pa_radiobuttonsiz_ovr(pa_radiobuttonsiz_t nfp, pa_radiobuttonsiz_t* ofp);
void _pa_radiobuttonsizg_ovr(pa_radiobuttonsizg_t nfp, pa_radiobuttonsizg_t* ofp);
void _pa_radiobutton_ovr(pa_radiobutton_t nfp, pa_radiobutton_t* ofp);
void _pa_radiobuttong_ovr(pa_radiobuttong_t nfp, pa_radiobuttong_t* ofp);
void _pa_groupsizg_ovr(pa_groupsizg_t nfp, pa_groupsizg_t* ofp);
void _pa_groupsiz_ovr(pa_groupsiz_t nfp, pa_groupsiz_t* ofp);
void _pa_group_ovr(pa_group_t nfp, pa_group_t* ofp);
void _pa_groupg_ovr(pa_groupg_t nfp, pa_groupg_t* ofp);
void _pa_background_ovr(pa_background_t nfp, pa_background_t* ofp);
void _pa_backgroundg_ovr(pa_backgroundg_t nfp, pa_backgroundg_t* ofp);
void _pa_scrollvertsizg_ovr(pa_scrollvertsizg_t nfp, pa_scrollvertsizg_t* ofp);
void _pa_scrollvertsiz_ovr(pa_scrollvertsiz_t nfp, pa_scrollvertsiz_t* ofp);
void _pa_scrollvert_ovr(pa_scrollvert_t nfp, pa_scrollvert_t* ofp);
void _pa_scrollvertg_ovr(pa_scrollvertg_t nfp, pa_scrollvertg_t* ofp);
void _pa_scrollhorizsizg_ovr(pa_scrollhorizsizg_t nfp, pa_scrollhorizsizg_t* ofp);
void _pa_scrollhorizsiz_ovr(pa_scrollhorizsiz_t nfp, pa_scrollhorizsiz_t* ofp);
void _pa_scrollhoriz_ovr(pa_scrollhoriz_t nfp, pa_scrollhoriz_t* ofp);
void _pa_scrollhorizg_ovr(pa_scrollhorizg_t nfp, pa_scrollhorizg_t* ofp);
void _pa_scrollpos_ovr(pa_scrollpos_t nfp, pa_scrollpos_t* ofp);
void _pa_scrollsiz_ovr(pa_scrollsiz_t nfp, pa_scrollsiz_t* ofp);
void _pa_numselboxsizg_ovr(pa_numselboxsizg_t nfp, pa_numselboxsizg_t* ofp);
void _pa_numselboxsiz_ovr(pa_numselboxsiz_t nfp, pa_numselboxsiz_t* ofp);
void _pa_numselbox_ovr(pa_numselbox_t nfp, pa_numselbox_t* ofp);
void _pa_numselboxg_ovr(pa_numselboxg_t nfp, pa_numselboxg_t* ofp);
void _pa_editboxsizg_ovr(pa_editboxsizg_t nfp, pa_editboxsizg_t* ofp);
void _pa_editboxsiz_ovr(pa_editboxsiz_t nfp, pa_editboxsiz_t* ofp);
void _pa_editbox_ovr(pa_editbox_t nfp, pa_editbox_t* ofp);
void _pa_editboxg_ovr(pa_editboxg_t nfp, pa_editboxg_t* ofp);
void _pa_progbarsizg_ovr(pa_progbarsizg_t nfp, pa_progbarsizg_t* ofp);
void _pa_progbarsiz_ovr(pa_progbarsiz_t nfp, pa_progbarsiz_t* ofp);
void _pa_progbar_ovr(pa_progbar_t nfp, pa_progbar_t* ofp);
void _pa_progbarg_ovr(pa_progbarg_t nfp, pa_progbarg_t* ofp);
void _pa_progbarpos_ovr(pa_progbarpos_t nfp, pa_progbarpos_t* ofp);
void _pa_listboxsizg_ovr(pa_listboxsizg_t nfp, pa_listboxsizg_t* ofp);
void _pa_listboxsiz_ovr(pa_listboxsiz_t nfp, pa_listboxsiz_t* ofp);
void _pa_listbox_ovr(pa_listbox_t nfp, pa_listbox_t* ofp);
void _pa_listboxg_ovr(pa_listboxg_t nfp, pa_listboxg_t* ofp);
void _pa_dropboxsizg_ovr(pa_dropboxsizg_t nfp, pa_dropboxsizg_t* ofp);
void _pa_dropboxsiz_ovr(pa_dropboxsiz_t nfp, pa_dropboxsiz_t* ofp);
void _pa_dropbox_ovr(pa_dropbox_t nfp, pa_dropbox_t* ofp);
void _pa_dropboxg_ovr(pa_dropboxg_t nfp, pa_dropboxg_t* ofp);
void _pa_dropeditboxsizg_ovr(pa_dropeditboxsizg_t nfp, pa_dropeditboxsizg_t* ofp);
void _pa_dropeditboxsiz_ovr(pa_dropeditboxsiz_t nfp, pa_dropeditboxsiz_t* ofp);
void _pa_dropeditbox_ovr(pa_dropeditbox_t nfp, pa_dropeditbox_t* ofp);
void _pa_dropeditboxg_ovr(pa_dropeditboxg_t nfp, pa_dropeditboxg_t* ofp);
void _pa_slidehorizsizg_ovr(pa_slidehorizsizg_t nfp, pa_slidehorizsizg_t* ofp);
void _pa_slidehorizsiz_ovr(pa_slidehorizsiz_t nfp, pa_slidehorizsiz_t* ofp);
void _pa_slidehoriz_ovr(pa_slidehoriz_t nfp, pa_slidehoriz_t* ofp);
void _pa_slidehorizg_ovr(pa_slidehorizg_t nfp, pa_slidehorizg_t* ofp);
void _pa_slidevertsizg_ovr(pa_slidevertsizg_t nfp, pa_slidevertsizg_t* ofp);
void _pa_slidevertsiz_ovr(pa_slidevertsiz_t nfp, pa_slidevertsiz_t* ofp);
void _pa_slidevert_ovr(pa_slidevert_t nfp, pa_slidevert_t* ofp);
void _pa_slidevertg_ovr(pa_slidevertg_t nfp, pa_slidevertg_t* ofp);
void _pa_tabbarsizg_ovr(pa_tabbarsizg_t nfp, pa_tabbarsizg_t* ofp);
void _pa_tabbarsiz_ovr(pa_tabbarsiz_t nfp, pa_tabbarsiz_t* ofp);
void _pa_tabbarclientg_ovr(pa_tabbarclientg_t nfp, pa_tabbarclientg_t* ofp);
void _pa_tabbarclient_ovr(pa_tabbarclient_t nfp, pa_tabbarclient_t* ofp);
void _pa_tabbar_ovr(pa_tabbar_t nfp, pa_tabbar_t* ofp);
void _pa_tabbarg_ovr(pa_tabbarg_t nfp, pa_tabbarg_t* ofp);
void _pa_tabsel_ovr(pa_tabsel_t nfp, pa_tabsel_t* ofp);
void _pa_alert_ovr(pa_alert_t nfp, pa_alert_t* ofp);
void _pa_querycolor_ovr(pa_querycolor_t nfp, pa_querycolor_t* ofp);
void _pa_queryopen_ovr(pa_queryopen_t nfp, pa_queryopen_t* ofp);
void _pa_querysave_ovr(pa_querysave_t nfp, pa_querysave_t* ofp);
void _pa_queryfind_ovr(pa_queryfind_t nfp, pa_queryfind_t* ofp);
void _pa_queryfindrep_ovr(pa_queryfindrep_t nfp, pa_queryfindrep_t* ofp);
void _pa_queryfont_ovr(pa_queryfont_t nfp, pa_queryfont_t* ofp);

#ifdef __cplusplus
}
#endif

#endif /* __GRAPH_H__ */
