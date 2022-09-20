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

/** Error codes this module */
typedef enum {

    pa_dispeftbful,          /* file table full */
    pa_dispejoyacc,          /* joystick access */
    pa_dispetimacc,          /* timer access */
    pa_dispefilopr,          /* cannot perform operation on special file */
    pa_dispeinvpos,          /* invalid screen position */
    pa_dispefilzer,          /* filename is empty */
    pa_dispeinvscn,          /* invalid screen number */
    pa_dispeinvhan,          /* invalid handle */
    pa_dispemouacc,          /* mouse access */
    pa_dispeoutdev,          /* output device error */
    pa_dispeinpdev,          /* input device error */
    pa_dispeinvtab,          /* invalid tab stop */
    pa_dispeinvjoy,          /* Invalid joystick ID */
    pa_dispecfgval,          /* invalid configuration value */
    pa_dispesendevent_unimp, /* sendevent unimplemented */
    pa_dispeopenwin_unimp,   /* openwin unimplemented */
    pa_dispebuffer_unimp,    /* buffer unimplemented */
    pa_dispesizbuf_unimp,    /* sizbuf unimplemented */
    pa_dispegetsiz_unimp,    /* getsiz unimplemented */
    pa_dispesetsiz_unimp,    /* setsiz unimplemented */
    pa_dispesetpos_unimp,    /* setpos unimplemented */
    pa_dispescnsiz_unimp,    /* scnsiz unimplemented */
    pa_dispescncen_unimp,    /* scncen unimplemented */
    pa_dispewinclient_unimp, /* winclient unimplemented */
    pa_dispefront_unimp,     /* front unimplemented */
    pa_dispeback_unimp,      /* back unimplemented */
    pa_dispeframe_unimp,     /* frame unimplemented */
    pa_dispesizable_unimp,   /* sizable unimplemented */
    pa_dispesysbar_unimp,    /* sysbar unimplemented */
    pa_dispemenu_unimp,      /* menu unimplemented */
    pa_dispemenuena_unimp,   /* menuena unimplemented */
    pa_dispemenusel_unimp,   /* menusel unimplemented */
    pa_dispestdmenu_unimp,   /* stdmenu unimplemented */
    pa_dispegetwinid_unimp,  /* getwinid unimplemented */
    pa_dispefocus_unimp,     /* focus unimplemented */
    pa_dispesystem           /* system fault */

} pa_errcod;

/** event function pointer */
typedef void (*pa_pevthan)(pa_evtrec*);

/** error function pointer */
typedef void (*pa_errhan)(pa_errcod e);

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

/* 
 * Routines at this level 
 */
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
void pa_eventsover(pa_pevthan eh, pa_pevthan* oeh);
void pa_sendevent(FILE* f, pa_evtrec* er);
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
void pa_focus(FILE* f);
int pa_getwinid(void);
void pa_errorover(pa_errhan nfp, pa_errhan* ofp);

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

/*
 * Event function overrides
 */
void pa_charover(pa_evchar_t eh, pa_evchar_t* oeh);
void pa_upover(pa_evup_t eh, pa_evup_t* oeh);
void pa_downover(pa_evdown_t eh, pa_evdown_t* oeh);
void pa_leftover(pa_evleft_t eh, pa_evleft_t* oeh);
void pa_rightover(pa_evright_t eh, pa_evright_t* oeh);
void pa_leftwover(pa_evleftw_t eh, pa_evleftw_t* oeh);
void pa_rightwover(pa_evrightw_t eh, pa_evrightw_t* oeh);
void pa_homeover(pa_evhome_t eh, pa_evhome_t* oeh);
void pa_homesover(pa_evhomes_t eh, pa_evhomes_t* oeh);
void pa_homelover(pa_evhomel_t eh, pa_evhomel_t* oeh);
void pa_endover(pa_evend_t eh, pa_evend_t* oeh);
void pa_endsover(pa_evends_t eh, pa_evends_t* oeh);
void pa_endlover(pa_evendl_t eh, pa_evendl_t* oeh);
void pa_scrlover(pa_evscrl_t eh, pa_evscrl_t* oeh);
void pa_scrrover(pa_evscrr_t eh, pa_evscrr_t* oeh);
void pa_scruover(pa_evscru_t eh, pa_evscru_t* oeh);
void pa_scrdover(pa_evscrd_t eh, pa_evscrd_t* oeh);
void pa_pagdover(pa_evpagd_t eh, pa_evpagd_t* oeh);
void pa_paguover(pa_evpagu_t eh, pa_evpagu_t* oeh);
void pa_tabover(pa_evtab_t eh, pa_evtab_t* oeh);
void pa_enterover(pa_eventer_t eh, pa_eventer_t* oeh);
void pa_insertover(pa_evinsert_t eh, pa_evinsert_t* oeh);
void pa_insertlover(pa_evinsertl_t eh, pa_evinsertl_t* oeh);
void pa_inserttover(pa_evinsertt_t eh, pa_evinsertt_t* oeh);
void pa_delover(pa_evdel_t eh, pa_evdel_t* oeh);
void pa_dellover(pa_evdell_t eh, pa_evdell_t* oeh);
void pa_delcfover(pa_evdelcf_t eh, pa_evdelcf_t* oeh);
void pa_delcbover(pa_evdelcb_t eh, pa_evdelcb_t* oeh);
void pa_copyover(pa_evcopy_t eh, pa_evcopy_t* oeh);
void pa_copylover(pa_evcopyl_t eh, pa_evcopyl_t* oeh);
void pa_canover(pa_evcan_t eh, pa_evcan_t* oeh);
void pa_stopover(pa_evstop_t eh, pa_evstop_t* oeh);
void pa_contover(pa_evcont_t eh, pa_evcont_t* oeh);
void pa_printover(pa_evprint_t eh, pa_evprint_t* oeh);
void pa_printbover(pa_evprintb_t eh, pa_evprintb_t* oeh);
void pa_printsover(pa_evprints_t eh, pa_evprints_t* oeh);
void pa_funover(pa_evfun_t eh, pa_evfun_t* oeh);
void pa_menuover(pa_evmenu_t eh, pa_evmenu_t* oeh);
void pa_moubaover(pa_evmouba_t eh, pa_evmouba_t* oeh);
void pa_moubdover(pa_evmoubd_t eh, pa_evmoubd_t* oeh);
void pa_moumovover(pa_evmoumov_t eh, pa_evmoumov_t* oeh);
void pa_timover(pa_evtim_t eh, pa_evtim_t* oeh);
void pa_joybaover(pa_evjoyba_t eh, pa_evjoyba_t* oeh);
void pa_joybdover(pa_evjoybd_t eh, pa_evjoybd_t* oeh);
void pa_joymovover(pa_evjoymov_t eh, pa_evjoymov_t* oeh);
void pa_resizeover(pa_evresize_t eh, pa_evresize_t* oeh);
void pa_focusover(pa_evfocus_t eh, pa_evfocus_t* oeh);
void pa_nofocusover(pa_evnofocus_t eh, pa_evnofocus_t* oeh);
void pa_hoverover(pa_evhover_t eh, pa_evhover_t* oeh);
void pa_nohoverover(pa_evnohover_t eh, pa_evnohover_t* oeh);
void pa_termover(pa_evterm_t eh, pa_evterm_t* oeh);
void pa_frameover(pa_evframe_t eh, pa_evframe_t* oeh);

/** linux system error function pointer */
typedef void (*_pa_linuxerrhan)(int e);

/** linux system error function override */
void _pa_linuxerrorover(_pa_linuxerrhan nfp, _pa_linuxerrhan* ofp);

/*
 * Override vector types
 *
 */
typedef void (*_pa_cursor_t)(FILE* f, int x, int y);
typedef int (*_pa_maxx_t)(FILE* f);
typedef int (*_pa_maxy_t)(FILE* f);
typedef void (*_pa_home_t)(FILE* f);
typedef void (*_pa_del_t)(FILE* f);
typedef void (*_pa_up_t)(FILE* f);
typedef void (*_pa_down_t)(FILE* f);
typedef void (*_pa_left_t)(FILE* f);
typedef void (*_pa_right_t)(FILE* f);
typedef void (*_pa_blink_t)(FILE* f, int e);
typedef void (*_pa_reverse_t)(FILE* f, int e);
typedef void (*_pa_underline_t)(FILE* f, int e);
typedef void (*_pa_superscript_t)(FILE* f, int e);
typedef void (*_pa_subscript_t)(FILE* f, int e);
typedef void (*_pa_italic_t)(FILE* f, int e);
typedef void (*_pa_bold_t)(FILE* f, int e);
typedef void (*_pa_strikeout_t)(FILE* f, int e);
typedef void (*_pa_standout_t)(FILE* f, int e);
typedef void (*_pa_fcolor_t)(FILE* f, pa_color c);
typedef void (*_pa_bcolor_t)(FILE* f, pa_color c);
typedef int (*_pa_curbnd_t)(FILE* f);
typedef void (*_pa_auto_t)(FILE* f, int e);
typedef void (*_pa_curvis_t)(FILE* f, int e);
typedef void (*_pa_scroll_t)(FILE* f, int x, int y);
typedef int (*_pa_curx_t)(FILE* f);
typedef int (*_pa_cury_t)(FILE* f);
typedef void (*_pa_select_t)(FILE* f, int u, int d);
typedef void (*_pa_event_t)(FILE* f, pa_evtrec* er);
typedef void (*_pa_timer_t)(FILE* f, int i, long t, int r);
typedef void (*_pa_killtimer_t)(FILE* f, int i);
typedef int (*_pa_mouse_t)(FILE* f);
typedef int (*_pa_mousebutton_t)(FILE* f, int m);
typedef int (*_pa_joystick_t)(FILE* f);
typedef int (*_pa_joybutton_t)(FILE* f, int j);
typedef int (*_pa_joyaxis_t)(FILE* f, int j);
typedef void (*_pa_settab_t)(FILE* f, int t);
typedef void (*_pa_restab_t)(FILE* f, int t);
typedef void (*_pa_clrtab_t)(FILE* f);
typedef int (*_pa_funkey_t)(FILE* f);
typedef void (*_pa_frametimer_t)(FILE* f, int e);
typedef void (*_pa_autohold_t)(int e);
typedef void (*_pa_wrtstr_t)(FILE* f, char* s);
typedef void (*_pa_wrtstrn_t)(FILE* f, char* s, int n);
typedef void (*_pa_sizbuf_t)(FILE* f, int x, int y);
typedef void (*_pa_title_t)(FILE* f, char* ts);
typedef void (*_pa_fcolorc_t)(FILE* f, int r, int g, int b);
typedef void (*_pa_bcolorc_t)(FILE* f, int r, int g, int b);
typedef void (*_pa_eventover_t)(pa_evtcod e, pa_pevthan eh,  pa_pevthan* oeh);
typedef void (*_pa_eventsover_t)(pa_pevthan eh,  pa_pevthan* oeh);
typedef void (*_pa_sendevent_t)(FILE* f, pa_evtrec* er);
typedef void (*_pa_title_t)(FILE* f, char* ts);
typedef void (*_pa_openwin_t)(FILE** infile, FILE** outfile, FILE* parent, int wid);
typedef void (*_pa_buffer_t)(FILE* f, int e);
typedef void (*_pa_getsiz_t)(FILE* f, int* x, int* y);
typedef void (*_pa_setsiz_t)(FILE* f, int x, int y);
typedef void (*_pa_setpos_t)(FILE* f, int x, int y);
typedef void (*_pa_scnsiz_t)(FILE* f, int* x, int* y);
typedef void (*_pa_scncen_t)(FILE* f, int* x, int* y);
typedef void (*_pa_winclient_t)(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms);
typedef void (*_pa_front_t)(FILE* f);
typedef void (*_pa_back_t)(FILE* f);
typedef void (*_pa_frame_t)(FILE* f, int e);
typedef void (*_pa_sizable_t)(FILE* f, int e);
typedef void (*_pa_sysbar_t)(FILE* f, int e);
typedef void (*_pa_menu_t)(FILE* f, pa_menuptr m);
typedef void (*_pa_menuena_t)(FILE* f, int id, int onoff);
typedef void (*_pa_menusel_t)(FILE* f, int id, int select);
typedef void (*_pa_stdmenu_t)(pa_stdmenusel sms, pa_menuptr* sm, pa_menuptr pm);
typedef void (*_pa_focus_t)(FILE* f);
typedef int (*_pa_getwinid_t)(void);

/*
 * Overrider routines
 */
void _pa_cursor_ovr(_pa_cursor_t nfp, _pa_cursor_t* ofp);
void _pa_maxx_ovr(_pa_maxx_t nfp, _pa_maxx_t* ofp);
void _pa_maxy_ovr(_pa_maxy_t nfp, _pa_maxy_t* ofp);
void _pa_home_ovr(_pa_home_t nfp, _pa_home_t* ofp);
void _pa_del_ovr(_pa_del_t nfp, _pa_del_t* ofp);
void _pa_up_ovr(_pa_up_t nfp, _pa_up_t* ofp);
void _pa_down_ovr(_pa_down_t nfp, _pa_down_t* ofp);
void _pa_left_ovr(_pa_left_t nfp, _pa_left_t* ofp);
void _pa_right_ovr(_pa_right_t nfp, _pa_right_t* ofp);
void _pa_blink_ovr(_pa_blink_t nfp, _pa_blink_t* ofp);
void _pa_reverse_ovr(_pa_reverse_t nfp, _pa_reverse_t* ofp);
void _pa_underline_ovr(_pa_underline_t nfp, _pa_underline_t* ofp);
void _pa_superscript_ovr(_pa_superscript_t nfp, _pa_superscript_t* ofp);
void _pa_subscript_ovr(_pa_subscript_t nfp, _pa_subscript_t* ofp);
void _pa_italic_ovr(_pa_italic_t nfp, _pa_italic_t* ofp);
void _pa_bold_ovr(_pa_bold_t nfp, _pa_bold_t* ofp);
void _pa_strikeout_ovr(_pa_strikeout_t nfp, _pa_strikeout_t* ofp);
void _pa_standout_ovr(_pa_standout_t nfp, _pa_standout_t* ofp);
void _pa_fcolor_ovr(_pa_fcolor_t nfp, _pa_fcolor_t* ofp);
void _pa_bcolor_ovr(_pa_bcolor_t nfp, _pa_bcolor_t* ofp);
void _pa_curbnd_ovr(_pa_curbnd_t nfp, _pa_curbnd_t* ofp);
void _pa_auto_ovr(_pa_auto_t nfp, _pa_auto_t* ofp);
void _pa_curvis_ovr(_pa_curvis_t nfp, _pa_curvis_t* ofp);
void _pa_scroll_ovr(_pa_scroll_t nfp, _pa_scroll_t* ofp);
void _pa_curx_ovr(_pa_curx_t nfp, _pa_curx_t* ofp);
void _pa_cury_ovr(_pa_cury_t nfp, _pa_cury_t* ofp);
void _pa_select_ovr(_pa_select_t nfp, _pa_select_t* ofp);
void _pa_event_ovr(_pa_event_t nfp, _pa_event_t* ofp);
void _pa_timer_ovr(_pa_timer_t nfp, _pa_timer_t* ofp);
void _pa_killtimer(_pa_killtimer_t nfp, _pa_killtimer_t* ofp);
int  _pa_mouse(_pa_mouse_t nfp, _pa_mouse_t* ofp);
int  _pa_mousebutton(_pa_mousebutton_t nfp, _pa_mousebutton_t* ofp);
int  _pa_joystick(_pa_joystick_t nfp, _pa_joystick_t* ofp);
int  _pa_joybutton(_pa_joybutton_t nfp, _pa_joybutton_t* ofp);
int  _pa_joyaxis(_pa_joyaxis_t nfp, _pa_joyaxis_t* ofp);
void _pa_settab(_pa_settab_t nfp, _pa_settab_t* ofp);
void _pa_restab(_pa_restab_t nfp, _pa_restab_t* ofp);
void _pa_clrtab(_pa_clrtab_t nfp, _pa_clrtab_t* ofp);
int  _pa_funkey(_pa_funkey_t nfp, _pa_funkey_t* ofp);
void _pa_frametimer(_pa_frametimer_t nfp, _pa_frametimer_t* ofp);
void _pa_autohold(_pa_autohold_t nfp, _pa_autohold_t* ofp);
void _pa_wrtstr_ovr(_pa_wrtstr_t nfp, _pa_wrtstr_t* ofp);
void _pa_wrtstrn(_pa_wrtstrn_t nfp, _pa_wrtstrn_t* ofp);
void _pa_sizbuf_ovr(_pa_sizbuf_t nfp, _pa_sizbuf_t* ofp);
void _pa_title_ovr(_pa_title_t nfp, _pa_title_t* ofp);
void _pa_fcolorc_ovr(_pa_fcolorc_t nfp, _pa_fcolorc_t* ofp);
void _pa_bcolorc_ovr(_pa_bcolorc_t nfp, _pa_bcolorc_t* ofp);
void _pa_eventover_ovr(_pa_eventover_t nfp, _pa_eventover_t* ofp);
void _pa_eventsover_ovr(_pa_eventsover_t nfp, _pa_eventsover_t* ofp);
void _pa_sendevent(_pa_sendevent_t nfp, _pa_sendevent_t* ofp);
void _pa_openwin_ovr(_pa_openwin_t nfp, _pa_openwin_t* ofp);
void _pa_buffer_ovr(_pa_buffer_t nfp, _pa_buffer_t* ofp);
void _pa_front_ovr(_pa_front_t nfp, _pa_front_t* ofp);
void _pa_back_ovr(_pa_back_t nfp, _pa_back_t* ofp);
void _pa_getsiz_ovr(_pa_getsiz_t nfp, _pa_getsiz_t* ofp);
void _pa_setsiz_ovr(_pa_setsiz_t nfp, _pa_setsiz_t* ofp);
void _pa_setpos_ovr(_pa_setpos_t nfp, _pa_setpos_t* ofp);
void _pa_scnsiz_ovr(_pa_scnsiz_t nfp, _pa_scnsiz_t* ofp);
void _pa_scncen_ovr(_pa_scncen_t nfp, _pa_scncen_t* ofp);
void _pa_winclient_ovr(_pa_winclient_t nfp, _pa_winclient_t* ofp);
void _pa_front_ovr(_pa_front_t nfp, _pa_front_t* ofp);
void _pa_back_ovr(_pa_back_t nfp, _pa_back_t* ofp);
void _pa_frame_ovr(_pa_frame_t nfp, _pa_frame_t* ofp);
void _pa_sizable_ovr(_pa_sizable_t nfp, _pa_sizable_t* ofp);
void _pa_sysbar_ovr(_pa_sysbar_t nfp, _pa_sysbar_t* ofp);
void _pa_menu_ovr(_pa_menu_t nfp, _pa_menu_t* ofp);
void _pa_menuena_ovr(_pa_menuena_t nfp, _pa_menuena_t* ofp);
void _pa_menusel_ovr(_pa_menusel_t nfp, _pa_menusel_t* ofp);
void _pa_stdmenu_ovr(_pa_stdmenu_t nfp, _pa_stdmenu_t* ofp);
void _pa_focus_ovr(_pa_focus_t nfp, _pa_focus_t* ofp);
void _pa_getwinid_ovr(_pa_getwinid_t nfp, _pa_getwinid_t* ofp);

/*
 * Event function override override types
 */
typedef void (*_pa_charover_t)(pa_evchar_t eh, pa_evchar_t* oeh);
typedef void (*_pa_upover_t)(pa_evup_t eh, pa_evup_t* oeh);
typedef void (*_pa_downover_t)(pa_evdown_t eh, pa_evdown_t* oeh);
typedef void (*_pa_leftover_t)(pa_evleft_t eh, pa_evleft_t* oeh);
typedef void (*_pa_rightover_t)(pa_evright_t eh, pa_evright_t* oeh);
typedef void (*_pa_leftwover_t)(pa_evleftw_t eh, pa_evleftw_t* oeh);
typedef void (*_pa_rightwover_t)(pa_evrightw_t eh, pa_evrightw_t* oeh);
typedef void (*_pa_homeover_t)(pa_evhome_t eh, pa_evhome_t* oeh);
typedef void (*_pa_homesover_t)(pa_evhomes_t eh, pa_evhomes_t* oeh);
typedef void (*_pa_homelover_t)(pa_evhomel_t eh, pa_evhomel_t* oeh);
typedef void (*_pa_endover_t)(pa_evend_t eh, pa_evend_t* oeh);
typedef void (*_pa_endsover_t)(pa_evends_t eh, pa_evends_t* oeh);
typedef void (*_pa_endlover_t)(pa_evendl_t eh, pa_evendl_t* oeh);
typedef void (*_pa_scrlover_t)(pa_evscrl_t eh, pa_evscrl_t* oeh);
typedef void (*_pa_scrrover_t)(pa_evscrr_t eh, pa_evscrr_t* oeh);
typedef void (*_pa_scruover_t)(pa_evscru_t eh, pa_evscru_t* oeh);
typedef void (*_pa_scrdover_t)(pa_evscrd_t eh, pa_evscrd_t* oeh);
typedef void (*_pa_pagdover_t)(pa_evpagd_t eh, pa_evpagd_t* oeh);
typedef void (*_pa_paguover_t)(pa_evpagu_t eh, pa_evpagu_t* oeh);
typedef void (*_pa_tabover_t)(pa_evtab_t eh, pa_evtab_t* oeh);
typedef void (*_pa_enterover_t)(pa_eventer_t eh, pa_eventer_t* oeh);
typedef void (*_pa_insertover_t)(pa_evinsert_t eh, pa_evinsert_t* oeh);
typedef void (*_pa_insertlover_t)(pa_evinsertl_t eh, pa_evinsertl_t* oeh);
typedef void (*_pa_inserttover_t)(pa_evinsertt_t eh, pa_evinsertt_t* oeh);
typedef void (*_pa_delover_t)(pa_evdel_t eh, pa_evdel_t* oeh);
typedef void (*_pa_dellover_t)(pa_evdell_t eh, pa_evdell_t* oeh);
typedef void (*_pa_delcfover_t)(pa_evdelcf_t eh, pa_evdelcf_t* oeh);
typedef void (*_pa_delcbover_t)(pa_evdelcb_t eh, pa_evdelcb_t* oeh);
typedef void (*_pa_copyover_t)(pa_evcopy_t eh, pa_evcopy_t* oeh);
typedef void (*_pa_copylover_t)(pa_evcopyl_t eh, pa_evcopyl_t* oeh);
typedef void (*_pa_canover_t)(pa_evcan_t eh, pa_evcan_t* oeh);
typedef void (*_pa_stopover_t)(pa_evstop_t eh, pa_evstop_t* oeh);
typedef void (*_pa_contover_t)(pa_evcont_t eh, pa_evcont_t* oeh);
typedef void (*_pa_printover_t)(pa_evprint_t eh, pa_evprint_t* oeh);
typedef void (*_pa_printbover_t)(pa_evprintb_t eh, pa_evprintb_t* oeh);
typedef void (*_pa_printsover_t)(pa_evprints_t eh, pa_evprints_t* oeh);
typedef void (*_pa_funover_t)(pa_evfun_t eh, pa_evfun_t* oeh);
typedef void (*_pa_menuover_t)(pa_evmenu_t eh, pa_evmenu_t* oeh);
typedef void (*_pa_moubaover_t)(pa_evmouba_t eh, pa_evmouba_t* oeh);
typedef void (*_pa_moubdover_t)(pa_evmoubd_t eh, pa_evmoubd_t* oeh);
typedef void (*_pa_moumovover_t)(pa_evmoumov_t eh, pa_evmoumov_t* oeh);
typedef void (*_pa_timover_t)(pa_evtim_t eh, pa_evtim_t* oeh);
typedef void (*_pa_joybaover_t)(pa_evjoyba_t eh, pa_evjoyba_t* oeh);
typedef void (*_pa_joybdover_t)(pa_evjoybd_t eh, pa_evjoybd_t* oeh);
typedef void (*_pa_joymovover_t)(pa_evjoymov_t eh, pa_evjoymov_t* oeh);
typedef void (*_pa_resizeover_t)(pa_evresize_t eh, pa_evresize_t* oeh);
typedef void (*_pa_focusover_t)(pa_evfocus_t eh, pa_evfocus_t* oeh);
typedef void (*_pa_nofocusover_t)(pa_evnofocus_t eh, pa_evnofocus_t* oeh);
typedef void (*_pa_hoverover_t)(pa_evhover_t eh, pa_evhover_t* oeh);
typedef void (*_pa_nohoverover_t)(pa_evnohover_t eh, pa_evnohover_t* oeh);
typedef void (*_pa_termover_t)(pa_evterm_t eh, pa_evterm_t* oeh);
typedef void (*_pa_frameover_t)(pa_evframe_t eh, pa_evframe_t* oeh);

/*
 * Event function override overrides
 */
void _pa_charover_ovr(_pa_charover_t eh, _pa_charover_t* oeh);
void _pa_upover_ovr(_pa_upover_t eh, _pa_upover_t* oeh);
void _pa_downover_ovr(_pa_downover_t eh, _pa_downover_t* oeh);
void _pa_leftover_ovr(_pa_leftover_t eh, _pa_leftover_t* oeh);
void _pa_rightover_ovr(_pa_rightover_t eh, _pa_rightover_t* oeh);
void _pa_leftwover_ovr(_pa_leftwover_t eh, _pa_leftwover_t* oeh);
void _pa_rightwover_ovr(_pa_rightwover_t eh, _pa_rightwover_t* oeh);
void _pa_homeover_ovr(_pa_homeover_t eh, _pa_homeover_t* oeh);
void _pa_homesover_ovr(_pa_homesover_t eh, _pa_homesover_t* oeh);
void _pa_homelover_ovr(_pa_homelover_t eh, _pa_homelover_t* oeh);
void _pa_endover_ovr(_pa_endover_t eh, _pa_endover_t* oeh);
void _pa_endsover_ovr(_pa_endsover_t eh, _pa_endsover_t* oeh);
void _pa_endlover_ovr(_pa_endlover_t eh, _pa_endlover_t* oeh);
void _pa_scrlover_ovr(_pa_scrlover_t eh, _pa_scrlover_t* oeh);
void _pa_scrrover_ovr(_pa_scrrover_t eh, _pa_scrrover_t* oeh);
void _pa_scruover_ovr(_pa_scruover_t eh, _pa_scruover_t* oeh);
void _pa_scrdover_ovr(_pa_scrdover_t eh, _pa_scrdover_t* oeh);
void _pa_pagdover_ovr(_pa_pagdover_t eh, _pa_pagdover_t* oeh);
void _pa_paguover_ovr(_pa_paguover_t eh, _pa_paguover_t* oeh);
void _pa_tabover_ovr(_pa_tabover_t eh, _pa_tabover_t* oeh);
void _pa_enterover_ovr(_pa_enterover_t eh, _pa_enterover_t* oeh);
void _pa_insertover_ovr(_pa_insertover_t eh, _pa_insertover_t* oeh);
void _pa_insertlover_ovr(_pa_insertlover_t eh, _pa_insertlover_t* oeh);
void _pa_inserttover_ovr(_pa_inserttover_t eh, _pa_inserttover_t* oeh);
void _pa_delover_ovr(_pa_delover_t eh, _pa_delover_t* oeh);
void _pa_dellover_ovr(_pa_dellover_t eh, _pa_dellover_t* oeh);
void _pa_delcfover_ovr(_pa_delcfover_t eh, _pa_delcfover_t* oeh);
void _pa_delcbover_ovr(_pa_delcbover_t eh, _pa_delcbover_t* oeh);
void _pa_copyover_ovr(_pa_copyover_t eh, _pa_copyover_t* oeh);
void _pa_copylover_ovr(_pa_copylover_t eh, _pa_copylover_t* oeh);
void _pa_canover_ovr(_pa_canover_t eh, _pa_canover_t* oeh);
void _pa_stopover_ovr(_pa_stopover_t eh, _pa_stopover_t* oeh);
void _pa_contover_ovr(_pa_contover_t eh, _pa_contover_t* oeh);
void _pa_printover_ovr(_pa_printover_t eh, _pa_printover_t* oeh);
void _pa_printbover_ovr(_pa_printbover_t eh, _pa_printbover_t* oeh);
void _pa_printsover_ovr(_pa_printsover_t eh, _pa_printsover_t* oeh);
void _pa_funover_ovr(_pa_funover_t eh, _pa_funover_t* oeh);
void _pa_menuover_ovr(_pa_menuover_t eh, _pa_menuover_t* oeh);
void _pa_moubaover_ovr(_pa_moubaover_t eh, _pa_moubaover_t* oeh);
void _pa_moubdover_ovr(_pa_moubdover_t eh, _pa_moubdover_t* oeh);
void _pa_moumovover_ovr(_pa_moumovover_t eh, _pa_moumovover_t* oeh);
void _pa_timover_ovr(_pa_timover_t eh, _pa_timover_t* oeh);
void _pa_joybaover_ovr(_pa_joybaover_t eh, _pa_joybaover_t* oeh);
void _pa_joybdover_ovr(_pa_joybdover_t eh, _pa_joybdover_t* oeh);
void _pa_joymovover_ovr(_pa_joymovover_t eh, _pa_joymovover_t* oeh);
void _pa_resizeover_ovr(_pa_resizeover_t eh, _pa_resizeover_t* oeh);
void _pa_focusover_ovr(_pa_focusover_t eh, _pa_focusover_t* oeh);
void _pa_nofocusover_ovr(_pa_nofocusover_t eh, _pa_nofocusover_t* oeh);
void _pa_hoverover_ovr(_pa_hoverover_t eh, _pa_hoverover_t* oeh);
void _pa_nohoverover_ovr(_pa_nohoverover_t eh, _pa_nohoverover_t* oeh);
void _pa_termover_ovr(_pa_termover_t eh, _pa_termover_t* oeh);
void _pa_frameover_ovr(_pa_frameover_t eh, _pa_frameover_t* oeh);

#ifdef __cplusplus
}
#endif

#endif /* __TERMINAL_H__ */
