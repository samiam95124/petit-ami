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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <localdefs.h>

#define PA_MAXTIM 10 /**< maximum number of timers available */

/** colors displayable in text mode */
typedef enum { pa_black, pa_white, pa_red, pa_green, pa_blue, pa_cyan,
               pa_yellow, pa_magenta } pa_color;

/** events */
typedef enum {

    /** ANSI character returned */      pa_etchar,
    /** cursor up one line */           pa_etup,
    /** down one line */                pa_etdown,
    /** left one character */           pa_etleft,
    /** right one character */          pa_etright,
    /** left one word */                pa_etleftw,
    /** right one word */               pa_etrightw,
    /** home of document */             pa_ethome,
    /** home of screen */               pa_ethomes,
    /** home of line */                 pa_ethomel,
    /** end of document */              pa_etend,
    /** end of screen */                pa_etends,
    /** end of line */                  pa_etendl,
    /** scroll left one character */    pa_etscrl,
    /** scroll right one character */   pa_etscrr,
    /** scroll up one line */           pa_etscru,
    /** scroll down one line */         pa_etscrd,
    /** page down */                    pa_etpagd,
    /** page up */                      pa_etpagu,
    /** tab */                          pa_ettab,
    /** enter line */                   pa_etenter,
    /** insert block */                 pa_etinsert,
    /** insert line */                  pa_etinsertl,
    /** insert toggle */                pa_etinsertt,
    /** delete block */                 pa_etdel,
    /** delete line */                  pa_etdell,
    /** delete character forward */     pa_etdelcf,
    /** delete character backward */    pa_etdelcb,
    /** copy block */                   pa_etcopy,
    /** copy line */                    pa_etcopyl,
    /** cancel current operation */     pa_etcan,
    /** stop current operation */       pa_etstop,
    /** continue current operation */   pa_etcont,
    /** print document */               pa_etprint,
    /** print block */                  pa_etprintb,
    /** print screen */                 pa_etprints,
    /** function key */                 pa_etfun,
    /** display menu */                 pa_etmenu,
    /** mouse button assertion */       pa_etmouba,
    /** mouse button deassertion */     pa_etmoubd,
    /** mouse move */                   pa_etmoumov,
    /** timer matures */                pa_ettim,
    /** joystick button assertion */    pa_etjoyba,
    /** joystick button deassertion */  pa_etjoybd,
    /** joystick move */                pa_etjoymov,
    /** window was resized */           pa_etresize,
    /** window has focus */             pa_etfocus,    
    /** window lost focus */            pa_etnofocus,  
    /** window being hovered */         pa_ethover,    
    /** window stopped being hovered */ pa_etnohover,  
    /** terminate program */            pa_etterm,
    /** frame sync */                   pa_etframe,

    /* Reserved extra code areas, these are module defined. */
    pa_etsys    = 0x1000, /* start of base system reserved codes */
    pa_etman    = 0x2000, /* start of window management reserved codes */
    pa_etwidget = 0x3000, /* start of widget reserved codes */
    pa_etuser   = 0x4000  /* start of user defined codes */

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
                                        int joyp4, joyp5, joyp6;

        };
        /** function key */ int fkey;
        /** etresize */
        struct {

            int rszx, rszy;

        };

     };

} pa_evtrec, *pa_evtptr;

/** event function pointer */
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
void pa_timer(FILE* f, int i, long t, int r);
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
void pa_autohold(int e);
void pa_wrtstr(FILE* f, char *s);
void pa_wrtstrn(FILE* f, char* s, int n);
void pa_sizbuf(FILE* f, int x, int y);
void pa_title(FILE* f, char* ts);
void pa_fcolorc(FILE* f, int r, int g, int b);
void pa_bcolorc(FILE* f, int r, int g, int b);
void pa_eventover(pa_evtcod e, pa_pevthan eh,  pa_pevthan* oeh);
void pa_eventsover(pa_pevthan eh,  pa_pevthan* oeh);
void pa_sendevent(FILE* f, pa_evtrec* er);

/* Window management functions */

void pa_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid);
void pa_buffer(FILE* f, int e);
void pa_getsiz(FILE* f, int* x, int* y);
void pa_setsiz(FILE* f, int x, int y);
void pa_setpos(FILE* f, int x, int y);
void pa_scnsiz(FILE* f, int* x, int* y);
void pa_scncen(FILE* f, int* x, int* y);
void pa_winclient(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms);
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
typedef void (*pa_openwin_t)(FILE** infile, FILE** outfile, FILE* parent, int wid);
typedef void (*pa_buffer_t)(FILE* f, int e);
typedef void (*pa_getsiz_t)(FILE* f, int* x, int* y);
typedef void (*pa_setsiz_t)(FILE* f, int x, int y);
typedef void (*pa_setpos_t)(FILE* f, int x, int y);
typedef void (*pa_scnsiz_t)(FILE* f, int* x, int* y);
typedef void (*pa_scncen_t)(FILE* f, int* x, int* y);
typedef void (*pa_winclient_t)(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms);
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

/*
 * Overrider routines
 */
void _pa_scroll_ovr(pa_scroll_t nfp, pa_scroll_t* ofp);
void _pa_cursor_ovr(pa_cursor_t nfp, pa_cursor_t* ofp);
void _pa_maxx_ovr(pa_maxx_t nfp, pa_maxx_t* ofp);
void _pa_maxy_ovr(pa_maxy_t nfp, pa_maxy_t* ofp);
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
void _pa_bcolor_ovr(pa_bcolor_t nfp, pa_bcolor_t* ofp);
void _pa_curbnd_ovr(pa_curbnd_t nfp, pa_curbnd_t* ofp);
void _pa_auto_ovr(pa_auto_t nfp, pa_auto_t* ofp);
void _pa_curvis_ovr(pa_curvis_t nfp, pa_curvis_t* ofp);
void _pa_curx_ovr(pa_curx_t nfp, pa_curx_t* ofp);
void _pa_cury_ovr(pa_cury_t nfp, pa_cury_t* ofp);
void _pa_select_ovr(pa_select_t nfp, pa_select_t* ofp);
void _pa_wrtstr_ovr(pa_wrtstr_t nfp, pa_wrtstr_t* ofp);
void _pa_sizbuf_ovr(pa_sizbuf_t nfp, pa_sizbuf_t* ofp);
void _pa_fcolorc_ovr(pa_fcolorc_t nfp, pa_fcolorc_t* ofp);
void _pa_bcolorc_ovr(pa_bcolorc_t nfp, pa_bcolorc_t* ofp);
void _pa_del_ovr(pa_del_t nfp, pa_del_t* ofp);
void _pa_title_ovr(pa_title_t nfp, pa_title_t* ofp);
void _pa_getwinid_ovr(pa_getwinid_t nfp, pa_getwinid_t* ofp);
void _pa_openwin_ovr(pa_openwin_t nfp, pa_openwin_t* ofp);
void _pa_buffer_ovr(pa_buffer_t nfp, pa_buffer_t* ofp);
void _pa_menu_ovr(pa_menu_t nfp, pa_menu_t* ofp);
void _pa_menuena_ovr(pa_menuena_t nfp, pa_menuena_t* ofp);
void _pa_menusel_ovr(pa_menusel_t nfp, pa_menusel_t* ofp);
void _pa_stdmenu_ovr(pa_stdmenu_t nfp, pa_stdmenu_t* ofp);
void _pa_front_ovr(pa_front_t nfp, pa_front_t* ofp);
void _pa_back_ovr(pa_back_t nfp, pa_back_t* ofp);
void _pa_getsiz_ovr(pa_getsiz_t nfp, pa_getsiz_t* ofp);
void _pa_setsiz_ovr(pa_setsiz_t nfp, pa_setsiz_t* ofp);
void _pa_setpos_ovr(pa_setpos_t nfp, pa_setpos_t* ofp);
void _pa_scnsiz_ovr(pa_scnsiz_t nfp, pa_scnsiz_t* ofp);
void _pa_scncen_ovr(pa_scncen_t nfp, pa_scncen_t* ofp);
void _pa_winclient_ovr(pa_winclient_t nfp, pa_winclient_t* ofp);
void _pa_frame_ovr(pa_frame_t nfp, pa_frame_t* ofp);
void _pa_sizable_ovr(pa_sizable_t nfp, pa_sizable_t* ofp);
void _pa_sysbar_ovr(pa_sysbar_t nfp, pa_sysbar_t* ofp);
void _pa_focus_ovr(pa_focus_t nfp, pa_focus_t* ofp);

/*
 * Extension override types
 */
typedef void (*pa_sendevent_t)(FILE* f, pa_evtrec* er);
typedef void (*pa_title_t)(FILE* f, char* ts);
typedef void (*pa_openwin_t)(FILE** infile, FILE** outfile, FILE* parent, int wid);
typedef void (*pa_buffer_t)(FILE* f, int e);
typedef void (*pa_sizbuf_t)(FILE* f, int x, int y);
typedef void (*pa_getsiz_t)(FILE* f, int* x, int* y);
typedef void (*pa_setsiz_t)(FILE* f, int x, int y);
typedef void (*pa_setpos_t)(FILE* f, int x, int y);
typedef void (*pa_scnsiz_t)(FILE* f, int* x, int* y);
typedef void (*pa_scncen_t)(FILE* f, int* x, int* y);
typedef void (*pa_winclient_t)(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms);
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

/*
 * Event override types
 */
typedef int (*pa_evchar_t)(char c);
typedef int (*pa_evup_t)(void);
typedef int (*pa_evdown_t)(void);
typedef int (*pa_evleft_t)(void);
typedef int (*pa_evright_t)(void);
typedef int (*pa_evleftw_t)(void);
typedef int (*pa_evrightw_t)(void);
typedef int (*pa_evhome_t)(void);
typedef int (*pa_evhomes_t)(void);
typedef int (*pa_evhomel_t)(void);
typedef int (*pa_evend_t)(void);
typedef int (*pa_evends_t)(void);
typedef int (*pa_evendl_t)(void);
typedef int (*pa_evscrl_t)(void);
typedef int (*pa_evscrr_t)(void);
typedef int (*pa_evscru_t)(void);
typedef int (*pa_evscrd_t)(void);
typedef int (*pa_evpagd_t)(void);
typedef int (*pa_evpagu_t)(void);
typedef int (*pa_evtab_t)(void);
typedef int (*pa_eventer_t)(void);
typedef int (*pa_evinsert_t)(void);
typedef int (*pa_evinsertl_t)(void);
typedef int (*pa_evinsertt_t)(void);
typedef int (*pa_evdel_t)(void);
typedef int (*pa_evdell_t)(void);
typedef int (*pa_evdelcf_t)(void);
typedef int (*pa_evdelcb_t)(void);
typedef int (*pa_evcopy_t)(void);
typedef int (*pa_evcopyl_t)(void);
typedef int (*pa_evcan_t)(void);
typedef int (*pa_evstop_t)(void);
typedef int (*pa_evcont_t)(void);
typedef int (*pa_evprint_t)(void);
typedef int (*pa_evprintb_t)(void);
typedef int (*pa_evprints_t)(void);
typedef int (*pa_evfun_t)(int k);
typedef int (*pa_evmenu_t)(void);
typedef int (*pa_evmouba_t)(int m, int b);
typedef int (*pa_evmoubd_t)(int m, int b);
typedef int (*pa_evmoumov_t)(int m, int x, int y);
typedef int (*pa_evtim_t)(int t);
typedef int (*pa_evjoyba_t)(int j, int b);
typedef int (*pa_evjoybd_t)(int j, int b);
typedef int (*pa_evjoymov_t)(int j, int x, int y, int z);
typedef int (*pa_evresize_t)(int rszx, int rszy);
typedef int (*pa_evfocus_t)(void);
typedef int (*pa_evnofocus_t)(void);
typedef int (*pa_evhover_t)(void);
typedef int (*pa_evnohover_t)(void);
typedef int (*pa_evterm_t)(void);
typedef int (*pa_evframe_t)(void);

#ifdef __cplusplus
}
#endif

#endif /* __TERMINAL_H__ */
