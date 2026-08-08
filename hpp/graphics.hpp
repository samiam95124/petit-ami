/** ****************************************************************************
 *
 * Graphics library interface C++ wrapper header
 *
 * Redeclares graphics library definitions using the graphics namespace.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

#ifndef __GRAPHICS_HPP__
#define __GRAPHICS_HPP__

#include <graphics.h>

namespace graphics {

#define MAXTIM      AMI_MAXTIM      /**< maximum number of timers available */

/* standard fonts */
#define FONT_TERM   AMI_FONT_TERM   /**< terminal (fixed space) font */
#define FONT_BOOK   AMI_FONT_BOOK   /**< serif font */
#define FONT_SIGN   AMI_FONT_SIGN   /**< san-serif font */
#define FONT_TECH   AMI_FONT_TECH   /**< technical (scalable) font */

/* standardized menu entries */
#define SMNEW        AMI_SMNEW        /**< new file */
#define SMOPEN       AMI_SMOPEN       /**< open file */
#define SMCLOSE      AMI_SMCLOSE      /**< close file */
#define SMSAVE       AMI_SMSAVE       /**< save file */
#define SMSAVEAS     AMI_SMSAVEAS     /**< save file as name */
#define SMPAGESET    AMI_SMPAGESET    /**< page setup */
#define SMPRINT      AMI_SMPRINT      /**< print */
#define SMEXIT       AMI_SMEXIT       /**< exit program */
#define SMUNDO       AMI_SMUNDO       /**< undo edit */
#define SMCUT        AMI_SMCUT        /**< cut selection */
#define SMPASTE      AMI_SMPASTE      /**< paste selection */
#define SMDELETE     AMI_SMDELETE     /**< delete selection */
#define SMFIND       AMI_SMFIND       /**< find text */
#define SMFINDNEXT   AMI_SMFINDNEXT   /**< find next */
#define SMREPLACE    AMI_SMREPLACE    /**< replace text */
#define SMGOTO       AMI_SMGOTO       /**< goto line */
#define SMSELECTALL  AMI_SMSELECTALL  /**< select all text */
#define SMNEWWINDOW  AMI_SMNEWWINDOW  /**< new window */
#define SMTILEHORIZ  AMI_SMTILEHORIZ  /**< tile child windows horizontally */
#define SMTILEVERT   AMI_SMTILEVERT   /**< tile child windows vertically */
#define SMCASCADE    AMI_SMCASCADE    /**< cascade windows */
#define SMCLOSEALL   AMI_SMCLOSEALL   /**< close all windows */
#define SMHELPTOPIC  AMI_SMHELPTOPIC  /**< help topics */
#define SMABOUT      AMI_SMABOUT      /**< about this program */
#define SMMAX        AMI_SMMAX        /**< maximum defined standard menu entries */

/* colors displayable in text mode */
typedef enum { black, white, red, green, blue, cyan,
               yellow, magenta, backcolor } color;

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
    /** mouse move graphical */         etmoumovg,
    /** window redraw */                etredraw,
    /** window minimized */             etmin,
    /** window maximized */             etmax,
    /** window normalized */            etnorm,
    /** menu item selected */           etmenus,
    /** button assert */                etbutton,
    /** checkbox click */               etchkbox,
    /** radio button click */           etradbut,
    /** scroll up/left line */          etsclull,
    /** scroll down/right line */       etscldrl,
    /** scroll up/left page */          etsclulp,
    /** scroll down/right page */       etscldrp,
    /** scroll bar position */          etsclpos,
    /** edit box signals done */        etedtbox,
    /** number select box signals done */ etnumbox,
    /** list box selection */           etlstbox,
    /** drop box selection */           etdrpbox,
    /** drop edit box signals done */   etdrebox,
    /** slider position */              etsldpos,
    /** tab bar select */               ettabbar,
    /** enlarge what is displayed */    etusize,
    /** reduce what is displayed */     etdsize,

    /* Reserved extra code areas, these are module defined. */
    etsys    = 0x1000, /**< start of base system reserved codes */
    etman    = 0x2000, /**< start of window management reserved codes */
    etwidget = 0x3000, /**< start of widget reserved codes */
    etuser   = 0x4000  /**< start of user defined codes */

} evtcod;

/* event record */
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
        /* etmouba */
        struct {

            /** mouse handle */  long amoun;
            /** button number */ long amoubn;

        };
        /* etmoubd */
        struct {

            /** mouse handle */  long dmoun;
            /** button number */ long dmoubn;

        };
        /* etjoyba */
        struct {

            /** joystick number */ long ajoyn;
            /** button number */   long ajoybn;

        };
        /* etjoybd */
        struct {

            /** joystick number */ long djoyn;
            /** button number */   long djoybn;

        };
        /* etjoymov */
        struct {

            /** joystick number */      long mjoyn;
            /** joystick coordinates */ long joypx, joypy, joypz;
                                        long joyp4, joyp5, joyp6;

        };
        /* etfun */
        /** function key */ long fkey;
        /* etresize */
        struct {

            long rszx, rszy, rszxg, rszyg;

        };

        /** etmoumovg: */
        struct {

            /** mouse number */   long mmoung;
            /** mouse movement */ long moupxg, moupyg;

        };
        /** etredraw */
        struct {

            /** bounding rectangle */
            long rsx, rsy, rex, rey;

        };
        /* etmenus */
        long menuid; /**< menu item selected */
        /* etbutton */
        long butid; /**< button id */
        /* etchkbox */
        long ckbxid; /**< checkbox id */
        /* etradbut */
        long radbid; /**< radio button id */
        /* etsclull */
        long sclulid; /**< scroll up/left line id */
        /* etscldrl */
        long scldrid; /**< scroll down/right line id */
        /* etsclulp */
        long sclupid; /**< scroll up/left page id */
        /* etscldrp */
        long scldpid; /**< scroll down/right page id */
        /* etsclpos */
        struct {

            long sclpid; /**< scroll bar id */
            long sclpos; /**< scroll bar position */

        };
        /* etedtbox */
        long edtbid; /**< edit box complete id */
        /* etnumbox */
        struct {

            long numbid; /**< num sel box id */
            long numbsl; /**< num select value */

        };
        /* etlstbox */
        struct {

            long lstbid; /**< list box id */
            long lstbsl; /**< list box select number */

        };
        /* etdrpbox */
        struct {

            long drpbid; /**< drop box id */
            long drpbsl; /**< drop box select */

        };
        /* etdrebox */
        long drebid; /**< drop edit box id */
        /* etsldpos */
        struct {

            long sldpid; /**< slider id */
            long sldpos; /**< slider position */

        };
        /* ettabbar */
        struct {

            long tabid;  /**< tab bar id */
            long tabsel; /**< tab select */

        };

     };

} evtrec, *evtptr;

/** event function pointer */
typedef void (*pevthan)(evtrec*);

/* menu record */
typedef struct menurec* menuptr;
typedef struct menurec {

        menuptr next;   /**< next menu item in list */
        menuptr branch; /**< menu branch */
        long     onoff;  /**< on/off highlight */
        long     oneof;  /**< "one of" highlight */
        long     bar;    /**< place bar under */
        long     id;     /**< id of menu item */
        char*   face;   /**< text to place on button */

} menurec;

/* standard menu selector */
typedef long stdmenusel;

/* windows mode sets */
typedef enum {

    wmframe, /**< frame on/off */
    wmsize,  /**< size bars on/off */
    wmsysbar /**< system bar on/off */

} winmod;
typedef long winmodset;

/* string set for list box */
typedef struct strrec* strptr;
typedef struct strrec {

    strptr next; /**< next entry in list */
    char*  str;  /**< string */

} strrec;

/* orientation for tab bars */
typedef enum { totop, toright, tobottom, toleft } tabori;

/* settable items in find query */
typedef enum { qfncase, qfnup, qfnre } qfnopt;
typedef long qfnopts;

/* settable items in replace query */
typedef enum { qfrcase, qfrup, qfrre, qfrfind, qfrallfil, qfralllin } qfropt;
typedef long qfropts;

/* effects in font query */
typedef enum { qfteblink, qftereverse, qfteunderline, qftesuperscript,
               qftesubscript, qfteitalic, qftebold, qftestrikeout,
               qftestandout, qftecondensed, qfteextended, qftexlight,
               qftelight, qftexbold, qftehollow, qfteraised } qfteffect;
typedef long qfteffects;

/* procedural interface */

/* text */
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
void select(FILE* f, long u, long d);
void select(long u, long d);
void event(FILE* f, evtrec* er);
void event(evtrec* er);
void timer(FILE* f, long i, long t, long r);
void timer(long i, long t, long r);
void killtimer(FILE* f, long i);
void killtimer(long i);
long  mouse(FILE* f);
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
void autohold(long e);
void wrtstr(FILE* f, char* s);
void wrtstr(char* s);
void wrtstrn(FILE* f, char* s, long n);
void wrtstrn(char* s, long n);
void sizbuf(FILE* f, long x, long y);
void sizbuf(long x, long y);
void title(FILE* f, char* ts);
void title(char* ts);
void eventover(evtcod e, pevthan eh, pevthan* oeh);
void eventsover(pevthan eh, pevthan* oeh);
void sendevent(FILE* f, evtrec* er);
void sendevent(evtrec* er);

/* graphical */
long  maxxg(FILE* f);
long  maxxg(void);
long  maxyg(FILE* f);
long  maxyg(void);
long  curxg(FILE* f);
long  curxg(void);
long  curyg(FILE* f);
long  curyg(void);
void line(FILE* f, long x1, long y1, long x2, long y2);
void line(long x1, long y1, long x2, long y2);
void linewidth(FILE* f, long w);
void linewidth(long w);
void rect(FILE* f, long x1, long y1, long x2, long y2);
void rect(long x1, long y1, long x2, long y2);
void frect(FILE* f, long x1, long y1, long x2, long y2);
void frect(long x1, long y1, long x2, long y2);
void rrect(FILE* f, long x1, long y1, long x2, long y2, long xs, long ys);
void rrect(long x1, long y1, long x2, long y2, long xs, long ys);
void frrect(FILE* f, long x1, long y1, long x2, long y2, long xs, long ys);
void frrect(long x1, long y1, long x2, long y2, long xs, long ys);
void ellipse(FILE* f, long x1, long y1, long x2, long y2);
void ellipse(long x1, long y1, long x2, long y2);
void fellipse(FILE* f, long x1, long y1, long x2, long y2);
void fellipse(long x1, long y1, long x2, long y2);
void arc(FILE* f, long x1, long y1, long x2, long y2, long sa, long ea);
void arc(long x1, long y1, long x2, long y2, long sa, long ea);
void farc(FILE* f, long x1, long y1, long x2, long y2, long sa, long ea);
void farc(long x1, long y1, long x2, long y2, long sa, long ea);
void fchord(FILE* f, long x1, long y1, long x2, long y2, long sa, long ea);
void fchord(long x1, long y1, long x2, long y2, long sa, long ea);
void ftriangle(FILE* f, long x1, long y1, long x2, long y2, long x3, long y3);
void ftriangle(long x1, long y1, long x2, long y2, long x3, long y3);
void cursorg(FILE* f, long x, long y);
void cursorg(long x, long y);
long  baseline(FILE* f);
long  baseline(void);
void setpixel(FILE* f, long x, long y);
void setpixel(long x, long y);
void fover(FILE* f);
void fover(void);
void bover(FILE* f);
void bover(void);
void finvis(FILE* f);
void finvis(void);
void binvis(FILE* f);
void binvis(void);
void fxor(FILE* f);
void fxor(void);
void bxor(FILE* f);
void bxor(void);
void fand(FILE* f);
void fand(void);
void band(FILE* f);
void band(void);
void for_(FILE* f);
void for_(void);
void bor(FILE* f);
void bor(void);
long  chrsizx(FILE* f);
long  chrsizx(void);
long  chrsizy(FILE* f);
long  chrsizy(void);
long  fonts(FILE* f);
long  fonts(void);
void font(FILE* f, long fc);
void font(long fc);
void fontnam(FILE* f, long fc, char* fns, long fnsl);
void fontnam(long fc, char* fns, long fnsl);
void fontsiz(FILE* f, long s);
void fontsiz(long s);
void chrspcy(FILE* f, long s);
void chrspcy(long s);
void chrspcx(FILE* f, long s);
void chrspcx(long s);
long  dpmx(FILE* f);
long  dpmx(void);
long  dpmy(FILE* f);
long  dpmy(void);
long  strsiz(FILE* f, const char* s);
long  strsiz(const char* s);
long  chrpos(FILE* f, const char* s, long p);
long  chrpos(const char* s, long p);
void writejust(FILE* f, const char* s, long n);
void writejust(const char* s, long n);
long  justpos(FILE* f, const char* s, long p, long n);
long  justpos(const char* s, long p, long n);
void condensed(FILE* f, long e);
void condensed(long e);
void extended(FILE* f, long e);
void extended(long e);
void xlight(FILE* f, long e);
void xlight(long e);
void light(FILE* f, long e);
void light(long e);
void xbold(FILE* f, long e);
void xbold(long e);
void hollow(FILE* f, long e);
void hollow(long e);
void raised(FILE* f, long e);
void raised(long e);
void settabg(FILE* f, long t);
void settabg(long t);
void restabg(FILE* f, long t);
void restabg(long t);
void fcolorg(FILE* f, long r, long g, long b);
void fcolorg(long r, long g, long b);
void fcolorc(FILE* f, long r, long g, long b);
void fcolorc(long r, long g, long b);
void bcolorg(FILE* f, long r, long g, long b);
void bcolorg(long r, long g, long b);
void bcolorc(FILE* f, long r, long g, long b);
void bcolorc(long r, long g, long b);
void loadpict(FILE* f, long p, char* fn);
void loadpict(long p, char* fn);
long  pictsizx(FILE* f, long p);
long  pictsizx(long p);
long  pictsizy(FILE* f, long p);
long  pictsizy(long p);
void picture(FILE* f, long p, long x1, long y1, long x2, long y2);
void picture(long p, long x1, long y1, long x2, long y2);
void delpict(FILE* f, long p);
void delpict(long p);
void scrollg(FILE* f, long x, long y);
void scrollg(long x, long y);
void path(FILE* f, long a);
void path(long a);

/* window management */
void openwin(FILE** infile, FILE** outfile, FILE* parent, long wid);
void buffer(FILE* f, long e);
void buffer(long e);
void sizbufg(FILE* f, long x, long y);
void sizbufg(long x, long y);
void getsiz(FILE* f, long* x, long* y);
void getsiz(long* x, long* y);
void getsizg(FILE* f, long* x, long* y);
void getsizg(long* x, long* y);
void setsiz(FILE* f, long x, long y);
void setsiz(long x, long y);
void setsizg(FILE* f, long x, long y);
void setsizg(long x, long y);
void setpos(FILE* f, long x, long y);
void setpos(long x, long y);
void setposg(FILE* f, long x, long y);
void setposg(long x, long y);
void scnsiz(FILE* f, long* x, long* y);
void scnsiz(long* x, long* y);
void scnsizg(FILE* f, long* x, long* y);
void scnsizg(long* x, long* y);
void scncen(FILE* f, long* x, long* y);
void scncen(long* x, long* y);
void scnceng(FILE* f, long* x, long* y);
void scnceng(long* x, long* y);
void winclient(FILE* f, long cx, long cy, long* wx, long* wy, winmodset ms);
void winclient(long cx, long cy, long* wx, long* wy, winmodset ms);
void winclientg(FILE* f, long cx, long cy, long* wx, long* wy, winmodset ms);
void winclientg(long cx, long cy, long* wx, long* wy, winmodset ms);
void front(FILE* f);
void front(void);
void back(FILE* f);
void back(void);
void frame(FILE* f, long e);
void frame(long e);
void sizable(FILE* f, long e);
void sizable(long e);
void sysbar(FILE* f, long e);
void sysbar(long e);
void menu(FILE* f, menuptr m);
void menu(menuptr m);
void menuena(FILE* f, long id, long onoff);
void menuena(long id, long onoff);
void menusel(FILE* f, long id, long select);
void menusel(long id, long select);
void stdmenu(stdmenusel sms, menuptr* sm, menuptr pm);
long  getwinid(void);
void focus(FILE* f);
void focus(void);

/* widgets/controls */
long  getwigid(FILE* f);
long  getwigid(void);
void killwidget(FILE* f, long id);
void killwidget(long id);
void selectwidget(FILE* f, long id, long e);
void selectwidget(long id, long e);
void enablewidget(FILE* f, long id, long e);
void enablewidget(long id, long e);
void getwidgettext(FILE* f, long id, char* s, long sl);
void getwidgettext(long id, char* s, long sl);
void putwidgettext(FILE* f, long id, char* s);
void putwidgettext(long id, char* s);
void sizwidget(FILE* f, long id, long x, long y);
void sizwidget(long id, long x, long y);
void sizwidgetg(FILE* f, long id, long x, long y);
void sizwidgetg(long id, long x, long y);
void poswidget(FILE* f, long id, long x, long y);
void poswidget(long id, long x, long y);
void poswidgetg(FILE* f, long id, long x, long y);
void poswidgetg(long id, long x, long y);
void backwidget(FILE* f, long id);
void backwidget(long id);
void frontwidget(FILE* f, long id);
void frontwidget(long id);
void focuswidget(FILE* f, long id);
void focuswidget(long id);
void buttonsiz(FILE* f, char* s, long* w, long* h);
void buttonsiz(char* s, long* w, long* h);
void buttonsizg(FILE* f, char* s, long* w, long* h);
void buttonsizg(char* s, long* w, long* h);
void checkboxsiz(FILE* f, char* s, long* w, long* h);
void checkboxsiz(char* s, long* w, long* h);
void checkboxsizg(FILE* f, char* s, long* w, long* h);
void checkboxsizg(char* s, long* w, long* h);
void radiobuttonsiz(FILE* f, char* s, long* w, long* h);
void radiobuttonsiz(char* s, long* w, long* h);
void radiobuttonsizg(FILE* f, char* s, long* w, long* h);
void radiobuttonsizg(char* s, long* w, long* h);
void groupsiz(FILE* f, char* s, long cw, long ch, long* w, long* h, long* ox, long* oy);
void groupsiz(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy);
void groupsizg(FILE* f, char* s, long cw, long ch, long* w, long* h, long* ox, long* oy);
void groupsizg(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy);
void scrollvertsiz(FILE* f, long* w, long* h);
void scrollvertsiz(long* w, long* h);
void scrollvertsizg(FILE* f, long* w, long* h);
void scrollvertsizg(long* w, long* h);
void scrollhorizsiz(FILE* f, long* w, long* h);
void scrollhorizsiz(long* w, long* h);
void scrollhorizsizg(FILE* f, long* w, long* h);
void scrollhorizsizg(long* w, long* h);
void scrollpos(FILE* f, long id, long r);
void scrollpos(long id, long r);
void scrollsiz(FILE* f, long id, long r);
void scrollsiz(long id, long r);
void numselboxsiz(FILE* f, long l, long u, long* w, long* h);
void numselboxsiz(long l, long u, long* w, long* h);
void numselboxsizg(FILE* f, long l, long u, long* w, long* h);
void numselboxsizg(long l, long u, long* w, long* h);
void editboxsiz(FILE* f, char* s, long* w, long* h);
void editboxsiz(char* s, long* w, long* h);
void editboxsizg(FILE* f, char* s, long* w, long* h);
void editboxsizg(char* s, long* w, long* h);
void progbarsiz(FILE* f, long* w, long* h);
void progbarsiz(long* w, long* h);
void progbarsizg(FILE* f, long* w, long* h);
void progbarsizg(long* w, long* h);
void progbarpos(FILE* f, long id, long pos);
void progbarpos(long id, long pos);
void listboxsiz(FILE* f, strptr sp, long* w, long* h);
void listboxsiz(strptr sp, long* w, long* h);
void listboxsizg(FILE* f, strptr sp, long* w, long* h);
void listboxsizg(strptr sp, long* w, long* h);
void dropboxsiz(FILE* f, strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropboxsizg(FILE* f, strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropeditboxsiz(FILE* f, strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropeditboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropeditboxsizg(FILE* f, strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropeditboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh);
void slidehorizsiz(FILE* f, long* w, long* h);
void slidehorizsiz(long* w, long* h);
void slidehorizsizg(FILE* f, long* w, long* h);
void slidehorizsizg(long* w, long* h);
void slidevertsiz(FILE* f, long* w, long* h);
void slidevertsiz(long* w, long* h);
void slidevertsizg(FILE* f, long* w, long* h);
void slidevertsizg(long* w, long* h);
void tabbarsiz(FILE* f, tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy);
void tabbarsiz(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy);
void tabbarsizg(FILE* f, tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy);
void tabbarsizg(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy);
void tabbarclient(FILE* f, tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy);
void tabbarclient(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy);
void tabbarclientg(FILE* f, tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy);
void tabbarclientg(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy);
void tabsel(FILE* f, long id, long tn);
void tabsel(long id, long tn);

/* dialogs */
void alert(char* title, char* message);
void querycolor(long* r, long* g, long* b);
void queryopen(char* s, long sl);
void querysave(char* s, long sl);
void queryfind(char* s, long sl, qfnopts* opt);
void queryfindrep(char* s, long sl, char* r, long rl, qfropts* opt);
void queryfont(FILE* f, long* fc, long* s, long* fr, long* fg, long* fb, long* br,
               long* bg, long* bb, qfteffects* effect);
void queryfont(long* fc, long* s, long* fr, long* fg, long* fb, long* br,
               long* bg, long* bb, qfteffects* effect);

/* object based interface */
class graph {

FILE* infile;
FILE* outfile;

public:

/* constructor */
graph();

/* destructor */
~graph();

/* methods */

/* text */
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
void wrtstr(char* s);
void wrtstrn(char* s, long n);
void sizbuf(long x, long y);
void title(char* ts);
void sendevent(evtrec* er);

/* graphical */
long  maxxg(void);
long  maxyg(void);
long  curxg(void);
long  curyg(void);
void line(long x1, long y1, long x2, long y2);
void linewidth(long w);
void rect(long x1, long y1, long x2, long y2);
void frect(long x1, long y1, long x2, long y2);
void rrect(long x1, long y1, long x2, long y2, long xs, long ys);
void frrect(long x1, long y1, long x2, long y2, long xs, long ys);
void ellipse(long x1, long y1, long x2, long y2);
void fellipse(long x1, long y1, long x2, long y2);
void arc(long x1, long y1, long x2, long y2, long sa, long ea);
void farc(long x1, long y1, long x2, long y2, long sa, long ea);
void fchord(long x1, long y1, long x2, long y2, long sa, long ea);
void ftriangle(long x1, long y1, long x2, long y2, long x3, long y3);
void cursorg(long x, long y);
long  baseline(void);
void setpixel(long x, long y);
void fover(void);
void bover(void);
void finvis(void);
void binvis(void);
void fxor(void);
void bxor(void);
void fand(void);
void band(void);
void for_(void);
void bor(void);
long  chrsizx(void);
long  chrsizy(void);
long  fonts(void);
void font(long fc);
void fontnam(long fc, char* fns, long fnsl);
void fontsiz(long s);
void chrspcy(long s);
void chrspcx(long s);
long  dpmx(void);
long  dpmy(void);
long  strsiz(const char* s);
long  chrpos(const char* s, long p);
void writejust(const char* s, long n);
long  justpos(const char* s, long p, long n);
void condensed(long e);
void extended(long e);
void xlight(long e);
void light(long e);
void xbold(long e);
void hollow(long e);
void raised(long e);
void settabg(long t);
void restabg(long t);
void fcolorg(long r, long g, long b);
void fcolorc(long r, long g, long b);
void bcolorg(long r, long g, long b);
void bcolorc(long r, long g, long b);
void loadpict(long p, char* fn);
long  pictsizx(long p);
long  pictsizy(long p);
void picture(long p, long x1, long y1, long x2, long y2);
void delpict(long p);
void scrollg(long x, long y);
void path(long a);

/* window management */
void buffer(long e);
void sizbufg(long x, long y);
void getsiz(long* x, long* y);
void getsizg(long* x, long* y);
void setsiz(long x, long y);
void setsizg(long x, long y);
void setpos(long x, long y);
void setposg(long x, long y);
void scnsiz(long* x, long* y);
void scnsizg(long* x, long* y);
void scncen(long* x, long* y);
void scnceng(long* x, long* y);
void winclient(long cx, long cy, long* wx, long* wy, winmodset ms);
void winclientg(long cx, long cy, long* wx, long* wy, winmodset ms);
void front(void);
void back(void);
void frame(long e);
void sizable(long e);
void sysbar(long e);
void menu(menuptr m);
void menuena(long id, long onoff);
void menusel(long id, long select);
void focus(void);

/* widgets */
long  getwigid(void);
void killwidget(long id);
void selectwidget(long id, long e);
void enablewidget(long id, long e);
void getwidgettext(long id, char* s, long sl);
void putwidgettext(long id, char* s);
void sizwidget(long id, long x, long y);
void sizwidgetg(long id, long x, long y);
void poswidget(long id, long x, long y);
void poswidgetg(long id, long x, long y);
void backwidget(long id);
void frontwidget(long id);
void focuswidget(long id);
void buttonsiz(char* s, long* w, long* h);
void buttonsizg(char* s, long* w, long* h);
void checkboxsiz(char* s, long* w, long* h);
void checkboxsizg(char* s, long* w, long* h);
void radiobuttonsiz(char* s, long* w, long* h);
void radiobuttonsizg(char* s, long* w, long* h);
void groupsiz(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy);
void groupsizg(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy);
void scrollvertsiz(long* w, long* h);
void scrollvertsizg(long* w, long* h);
void scrollhorizsiz(long* w, long* h);
void scrollhorizsizg(long* w, long* h);
void scrollpos(long id, long r);
void scrollsiz(long id, long r);
void numselboxsiz(long l, long u, long* w, long* h);
void numselboxsizg(long l, long u, long* w, long* h);
void editboxsiz(char* s, long* w, long* h);
void editboxsizg(char* s, long* w, long* h);
void progbarsiz(long* w, long* h);
void progbarsizg(long* w, long* h);
void progbarpos(long id, long pos);
void listboxsiz(strptr sp, long* w, long* h);
void listboxsizg(strptr sp, long* w, long* h);
void dropboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropeditboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropeditboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh);
void slidehorizsiz(long* w, long* h);
void slidehorizsizg(long* w, long* h);
void slidevertsiz(long* w, long* h);
void slidevertsizg(long* w, long* h);
void tabbarsiz(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy);
void tabbarsizg(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy);
void tabbarclient(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy);
void tabbarclientg(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy);
void tabsel(long id, long tn);

/* dialogs */
void queryfont(long* fc, long* s, long* fr, long* fg, long* fb, long* br,
               long* bg, long* bb, qfteffects* effect);

void button(long x1, long y1, long x2, long y2, char* s, long id);
void buttong(long x1, long y1, long x2, long y2, char* s, long id);
void checkbox(long x1, long y1, long x2, long y2, char* s, long id);
void checkboxg(long x1, long y1, long x2, long y2, char* s, long id);
void radiobutton(long x1, long y1, long x2, long y2, char* s, long id);
void radiobuttong(long x1, long y1, long x2, long y2, char* s, long id);
void group(long x1, long y1, long x2, long y2, char* s, long id);
void groupg(long x1, long y1, long x2, long y2, char* s, long id);
void background(long x1, long y1, long x2, long y2, long id);
void backgroundg(long x1, long y1, long x2, long y2, long id);
void scrollvert(long x1, long y1, long x2, long y2, long id);
void scrollvertg(long x1, long y1, long x2, long y2, long id);
void scrollhoriz(long x1, long y1, long x2, long y2, long id);
void scrollhorizg(long x1, long y1, long x2, long y2, long id);
void numselbox(long x1, long y1, long x2, long y2, long l, long u, long id);
void numselboxg(long x1, long y1, long x2, long y2, long l, long u, long id);
void editbox(long x1, long y1, long x2, long y2, long id);
void editboxg(long x1, long y1, long x2, long y2, long id);
void progbar(long x1, long y1, long x2, long y2, long id);
void progbarg(long x1, long y1, long x2, long y2, long id);
void listbox(long x1, long y1, long x2, long y2, strptr sp, long id);
void listboxg(long x1, long y1, long x2, long y2, strptr sp, long id);
void dropbox(long x1, long y1, long x2, long y2, strptr sp, long id);
void dropboxg(long x1, long y1, long x2, long y2, strptr sp, long id);
void dropeditbox(long x1, long y1, long x2, long y2, strptr sp, long id);
void dropeditboxg(long x1, long y1, long x2, long y2, strptr sp, long id);
void slidehoriz(long x1, long y1, long x2, long y2, long mark, long id);
void slidehorizg(long x1, long y1, long x2, long y2, long mark, long id);
void slidevert(long x1, long y1, long x2, long y2, long mark, long id);
void slidevertg(long x1, long y1, long x2, long y2, long mark, long id);
void tabbar(long x1, long y1, long x2, long y2, strptr sp, tabori tor, long id);
void tabbarg(long x1, long y1, long x2, long y2, strptr sp, tabori tor, long id);

static void graphCB(evtrec* er);

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
virtual long evframe(void);
virtual long evmoumovg(long m, long x, long y);
virtual long evredraw(long x1, long y1, long x2, long y2);
virtual long evmin(void);
virtual long evmax(void);
virtual long evnorm(void);
virtual long evmenus(long id);
virtual long evbutton(long id);
virtual long evchkbox(long id);
virtual long evradbut(long id);
virtual long evsclull(long id);
virtual long evscldrl(long id);
virtual long evsclulp(long id);
virtual long evscldrp(long id);
virtual long evsclpos(long id, long pos);
virtual long evedtbox(long id);
virtual long evnumbox(long id, long val);
virtual long evlstbox(long id, long sel);
virtual long evdrpbox(long id, long sel);
virtual long evdrebox(long id);
virtual long evsldpos(long id, long pos);
virtual long evtabbar(long id, long sel);
virtual long evusize(void);
virtual long evdsize(void);

}; /* class graph */


/*******************************************************************************

The window and widget objects

A window object is one window: the main one, a child of another window,
or an independent top level. Every drawing and configuration call is a
method, the object converts to FILE* so the stdio calls and the C
interface speak to it directly, and the event virtuals below fire for
this window's events alone: the wrapper routes each event by the window
id in its record to the object that holds that window. Any number of
window objects may exist, and events for windows with no object fall
through to the procedural loop untouched.

A widget is made on a window by constructing one of the typed widget
classes; the id is allocated for you unless given. Widget events are
routed to the widget object first -- a button's pressed(), a slider's
moved() -- then, unhandled, to the window's virtual with the id, then
to the chain. The destructor kills the widget.

The widget creators do not appear as free functions of the namespace,
since C++ will not let a class and a function share a name: the classes
are the creators. The C interface keeps them all.

One rule of order: make a widget's window before the widget, and let
the widget die before its window -- which member order in a subclassed
window does by itself, members being destroyed before their base. A
window destroyed first disarms the widgets it still carries, so their
destructors do not reach into a closed window.

*******************************************************************************/

class widget;
void windowCB(evtrec* er); /* the wrapper's chain hook */

class window {

friend class widget;
friend void windowCB(evtrec* er);

protected:

FILE* wf;     /* the window, which is what every C call takes */
long  wid;    /* its logical id, which events carry */
long  owned;  /* it was opened here, and closes here */
long  nextid; /* widget ids not yet given out */
widget* wlist; /* the widgets made on this window */

public:

/* constructor: attach to the main window */
window();

/* constructor: open a window, a child of the given parent, or an
   independent top level when the parent is NULL */
window(window* parent);

/* destructor */
virtual ~window();

/* the window, for the stdio calls and the C interface */
operator FILE*(void);

/* the logical window id, as events carry it */
long id(void);

/* the next widget id not yet given out */
long newid(void);

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
void wrtstr(char* s);
void wrtstrn(char* s, long n);
void sizbuf(long x, long y);
void title(char* ts);
void sendevent(evtrec* er);
long  maxxg(void);
long  maxyg(void);
long  curxg(void);
long  curyg(void);
void line(long x1, long y1, long x2, long y2);
void linewidth(long w);
void rect(long x1, long y1, long x2, long y2);
void frect(long x1, long y1, long x2, long y2);
void rrect(long x1, long y1, long x2, long y2, long xs, long ys);
void frrect(long x1, long y1, long x2, long y2, long xs, long ys);
void ellipse(long x1, long y1, long x2, long y2);
void fellipse(long x1, long y1, long x2, long y2);
void arc(long x1, long y1, long x2, long y2, long sa, long ea);
void farc(long x1, long y1, long x2, long y2, long sa, long ea);
void fchord(long x1, long y1, long x2, long y2, long sa, long ea);
void ftriangle(long x1, long y1, long x2, long y2, long x3, long y3);
void cursorg(long x, long y);
long  baseline(void);
void setpixel(long x, long y);
void fover(void);
void bover(void);
void finvis(void);
void binvis(void);
void fxor(void);
void bxor(void);
void fand(void);
void band(void);
void for_(void);
void bor(void);
long  chrsizx(void);
long  chrsizy(void);
long  fonts(void);
void font(long fc);
void fontnam(long fc, char* fns, long fnsl);
void fontsiz(long s);
void chrspcy(long s);
void chrspcx(long s);
long  dpmx(void);
long  dpmy(void);
long  strsiz(const char* s);
long  chrpos(const char* s, long p);
void writejust(const char* s, long n);
long  justpos(const char* s, long p, long n);
void condensed(long e);
void extended(long e);
void xlight(long e);
void light(long e);
void xbold(long e);
void hollow(long e);
void raised(long e);
void settabg(long t);
void restabg(long t);
void fcolorg(long r, long g, long b);
void fcolorc(long r, long g, long b);
void bcolorg(long r, long g, long b);
void bcolorc(long r, long g, long b);
void loadpict(long p, char* fn);
long  pictsizx(long p);
long  pictsizy(long p);
void picture(long p, long x1, long y1, long x2, long y2);
void delpict(long p);
void scrollg(long x, long y);
void path(long a);
void buffer(long e);
void sizbufg(long x, long y);
void getsiz(long* x, long* y);
void getsizg(long* x, long* y);
void setsiz(long x, long y);
void setsizg(long x, long y);
void setpos(long x, long y);
void setposg(long x, long y);
void scnsiz(long* x, long* y);
void scnsizg(long* x, long* y);
void scncen(long* x, long* y);
void scnceng(long* x, long* y);
void winclient(long cx, long cy, long* wx, long* wy, winmodset ms);
void winclientg(long cx, long cy, long* wx, long* wy, winmodset ms);
void front(void);
void back(void);
void frame(long e);
void sizable(long e);
void sysbar(long e);
void menu(menuptr m);
void menuena(long id, long onoff);
void menusel(long id, long select);
void focus(void);
long  getwigid(void);
void killwidget(long id);
void selectwidget(long id, long e);
void enablewidget(long id, long e);
void getwidgettext(long id, char* s, long sl);
void putwidgettext(long id, char* s);
void sizwidget(long id, long x, long y);
void sizwidgetg(long id, long x, long y);
void poswidget(long id, long x, long y);
void poswidgetg(long id, long x, long y);
void backwidget(long id);
void frontwidget(long id);
void focuswidget(long id);
void buttonsiz(char* s, long* w, long* h);
void buttonsizg(char* s, long* w, long* h);
void checkboxsiz(char* s, long* w, long* h);
void checkboxsizg(char* s, long* w, long* h);
void radiobuttonsiz(char* s, long* w, long* h);
void radiobuttonsizg(char* s, long* w, long* h);
void groupsiz(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy);
void groupsizg(char* s, long cw, long ch, long* w, long* h, long* ox, long* oy);
void scrollvertsiz(long* w, long* h);
void scrollvertsizg(long* w, long* h);
void scrollhorizsiz(long* w, long* h);
void scrollhorizsizg(long* w, long* h);
void scrollpos(long id, long r);
void scrollsiz(long id, long r);
void numselboxsiz(long l, long u, long* w, long* h);
void numselboxsizg(long l, long u, long* w, long* h);
void editboxsiz(char* s, long* w, long* h);
void editboxsizg(char* s, long* w, long* h);
void progbarsiz(long* w, long* h);
void progbarsizg(long* w, long* h);
void progbarpos(long id, long pos);
void listboxsiz(strptr sp, long* w, long* h);
void listboxsizg(strptr sp, long* w, long* h);
void dropboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropeditboxsiz(strptr sp, long* cw, long* ch, long* ow, long* oh);
void dropeditboxsizg(strptr sp, long* cw, long* ch, long* ow, long* oh);
void slidehorizsiz(long* w, long* h);
void slidehorizsizg(long* w, long* h);
void slidevertsiz(long* w, long* h);
void slidevertsizg(long* w, long* h);
void tabbarsiz(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy);
void tabbarsizg(tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy);
void tabbarclient(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy);
void tabbarclientg(tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy);
void tabsel(long id, long tn);
void queryfont(long* fc, long* s, long* fr, long* fg, long* fb, long* br,
                      long* bg, long* bb, qfteffects* effect);

/* the event virtuals, fired for this window's events alone */
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
virtual long evframe(void);
virtual long evmoumovg(long m, long x, long y);
virtual long evredraw(long x1, long y1, long x2, long y2);
virtual long evmin(void);
virtual long evmax(void);
virtual long evnorm(void);
virtual long evmenus(long id);
virtual long evbutton(long id);
virtual long evchkbox(long id);
virtual long evradbut(long id);
virtual long evsclull(long id);
virtual long evscldrl(long id);
virtual long evsclulp(long id);
virtual long evscldrp(long id);
virtual long evsclpos(long id, long pos);
virtual long evedtbox(long id);
virtual long evnumbox(long id, long val);
virtual long evlstbox(long id, long sel);
virtual long evdrpbox(long id, long sel);
virtual long evdrebox(long id);
virtual long evsldpos(long id, long pos);
virtual long evtabbar(long id, long sel);
virtual long evusize(void);
virtual long evdsize(void);

}; /* class window */

class widget {

friend class window;
friend void windowCB(evtrec* er);

protected:

window& w;    /* the window the widget is on */
long    wid;  /* the widget id within it */
long    dead; /* the window went first: do not reach after it */
widget* next; /* next widget on the window */

/* the base registers; the typed class creates */
widget(window& wo, long id);

public:

/* destructor: kills the widget */
virtual ~widget();

/* the widget id */
long id(void);

/* operations, all from the C widget set */
void kill(void);
void select(long e);
void enable(long e);
void gettext(char* s, long sl);
void puttext(const char* s);
void pos(long x, long y);
void siz(long x, long y);
void back(void);
void front(void);
void focus(void);

/* the event virtuals; a typed widget overrides what it answers */
virtual long pressed(void);        /* button */
virtual long clicked(void);        /* checkbox, radio button */
virtual long done(void);           /* edit box, drop edit box */
virtual long selected(long v);     /* list, drop, number select, tab bar */
virtual long moved(long v);        /* slider, scroll bar position */
virtual long upline(void);         /* scroll bar steps */
virtual long downline(void);
virtual long uppage(void);
virtual long downpage(void);

}; /* class widget */

/* the typed widgets: constructing one makes it */
class button: public widget {

public:

button(window& wo, long x1, long y1, long x2, long y2, const char* s,
       long id = 0);

}; /* class button */

class checkbox: public widget {

public:

checkbox(window& wo, long x1, long y1, long x2, long y2, const char* s,
         long id = 0);

}; /* class checkbox */

class radiobutton: public widget {

public:

radiobutton(window& wo, long x1, long y1, long x2, long y2, const char* s,
            long id = 0);

}; /* class radiobutton */

class group: public widget {

public:

group(window& wo, long x1, long y1, long x2, long y2, const char* s,
      long id = 0);

}; /* class group */

class background: public widget {

public:

background(window& wo, long x1, long y1, long x2, long y2, long id = 0);

}; /* class background */

class scrollvert: public widget {

public:

scrollvert(window& wo, long x1, long y1, long x2, long y2, long id = 0);

}; /* class scrollvert */

class scrollhoriz: public widget {

public:

scrollhoriz(window& wo, long x1, long y1, long x2, long y2, long id = 0);

}; /* class scrollhoriz */

class numselbox: public widget {

public:

numselbox(window& wo, long x1, long y1, long x2, long y2, long l, long u,
          long id = 0);

}; /* class numselbox */

class editbox: public widget {

public:

editbox(window& wo, long x1, long y1, long x2, long y2, long id = 0);

}; /* class editbox */

class progbar: public widget {

public:

progbar(window& wo, long x1, long y1, long x2, long y2, long id = 0);

}; /* class progbar */

class listbox: public widget {

public:

listbox(window& wo, long x1, long y1, long x2, long y2, strptr sp,
        long id = 0);

}; /* class listbox */

class dropbox: public widget {

public:

dropbox(window& wo, long x1, long y1, long x2, long y2, strptr sp,
        long id = 0);

}; /* class dropbox */

class dropeditbox: public widget {

public:

dropeditbox(window& wo, long x1, long y1, long x2, long y2, strptr sp,
            long id = 0);

}; /* class dropeditbox */

class slidehoriz: public widget {

public:

slidehoriz(window& wo, long x1, long y1, long x2, long y2, long mark,
           long id = 0);

}; /* class slidehoriz */

class slidevert: public widget {

public:

slidevert(window& wo, long x1, long y1, long x2, long y2, long mark,
          long id = 0);

}; /* class slidevert */

class tabbar: public widget {

public:

tabbar(window& wo, long x1, long y1, long x2, long y2, strptr sp,
       tabori tor, long id = 0);

}; /* class tabbar */

} /* namespace graphics */

#endif /* __GRAPHICS_HPP__ */
