/*******************************************************************************
*                                                                              *
*                          PDF PRINT OUTPUT MODULE                             *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
*                            2026/08/13 S. A. Franco                           *
*                                                                              *
* Takes the drawing call set of graphics.h (no windows, no widgets) and        *
* composes a .pdf file from it. A print file is created with:                  *
*                                                                              *
*     openprint(&f, "name")                                                    *
*                                                                              *
* The name is either a standard filename, receiving the .pdf document, or a   *
* printer device name, recognized by the ':' in it: "lp0:", "lp1:", etc.,     *
* which is the printer by number as listed by the spooler, or "lp:", the      *
* system default printer. Printer output is composed the same way, then       *
* handed to the print spooler when the file is closed.                        *
*                                                                              *
* The page model is the one the manual gives for printers (4.25, 6.20): a     *
* one page buffer written with the normal terminal and graphics calls. When   *
* a form-feed ('\f') is output, the page is complete and a new page begins.   *
* A partial page left at close is ejected automatically. There is no input   *
* side: the event loop and all input devices give errors, as does select(),   *
* since a printer has one surface and no return path.                         *
*                                                                              *
* The page is US letter, 8.5" wide by 11" tall, at 600 DPI, giving a          *
* 5100x6600 coordinate space. The default font is 12 point terminal           *
* (Courier), which lays out the classic printer page: 85 columns by 66        *
* lines, 10 characters per inch, 6 lines per inch.                            *
*                                                                              *
* The page buffer is kept at the highest level .pdf provides, not as pixels:  *
* text is set in the standard PDF fonts with exact metrics, and the drawing   *
* primitives are emitted as vector paths, so the output is resolution        *
* independent above the 600 DPI coordinate grid. The standard fonts stand in  *
* for the Petit-Ami standard set: Courier for terminal, Times for book,       *
* Helvetica for sign and technical, with the bold and italic attributes       *
* selecting the matching face. The xor, and, and or raster modes have no      *
* true equivalent in .pdf; they are approximated by the Exclusion, Multiply   *
* and Screen blend modes, which agree with the raster operations on the      *
* black and white extremes. Pictures are 24 bit .bmp files, carried into the  *
* document as images.                                                          *
*                                                                              *
* The module interdicts the drawing vectors of the display library and the    *
* system write and close vectors, just as network does, so that only files    *
* made by openprint() are affected; all other files pass through untouched.   *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

#include <graphics.h>

/* the local library headers do not carry the process pipe calls */
extern FILE* popen(const char* command, const char* type);
extern int pclose(FILE* stream);

#define MAXPFIL 512          /* maximum file descriptors tracked */
#define MAXPIC  50           /* loadable pictures per print file */
#define MAXTAB  100          /* tab stops */
#define MAXPDEV 100          /* printer device name length */

#define PAGEDPI 600          /* dots per inch of the page space */
#define PAGEW   5100         /* page width, 8.5" */
#define PAGEH   6600         /* page height, 11" */
#define PDFW    612          /* page width in points */
#define PDFH    792          /* page height in points */
#define DEFSIZ  100          /* default font size, 12 points */

/* raster modes */
typedef enum { mdnorm, mdinvis, mdxor, mdand, mdor } mdcod;

/* font families */
typedef enum { fmcour, fmtimes, fmhelv } fmcod;

/*
 * Character widths of the standard PDF fonts, thousandths of the em, for
 * characters 32 to 126, taken from the font metric files. Courier is fixed
 * at 600 and needs no table.
 */

static const short helv_r_w[95] = {
    278, 278, 355, 556, 556, 889, 667, 221, 333, 333, 389, 584,
    278, 333, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 278, 278, 584, 584, 584, 556, 1015, 667, 667, 722,
    722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278,
    278, 278, 469, 556, 222, 556, 556, 500, 556, 556, 278, 556,
    556, 222, 222, 500, 222, 833, 556, 556, 556, 556, 333, 500,
    278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584,
};
static const short helv_b_w[95] = {
    278, 333, 474, 556, 556, 889, 722, 278, 333, 333, 389, 584,
    278, 333, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 333, 333, 584, 584, 584, 611, 975, 722, 722, 722,
    722, 667, 611, 778, 722, 278, 556, 722, 611, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333,
    278, 333, 584, 556, 278, 556, 611, 556, 611, 556, 333, 611,
    611, 278, 278, 556, 278, 889, 611, 611, 611, 611, 389, 556,
    333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584,
};
static const short helv_i_w[95] = {
    278, 278, 355, 556, 556, 889, 667, 222, 333, 333, 389, 584,
    278, 333, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 278, 278, 584, 584, 584, 556, 1015, 667, 667, 722,
    722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278,
    278, 278, 469, 556, 222, 556, 556, 500, 556, 556, 278, 556,
    556, 222, 222, 500, 222, 833, 556, 556, 556, 556, 333, 500,
    278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584,
};
static const short helv_bi_w[95] = {
    278, 333, 474, 556, 556, 889, 722, 278, 333, 333, 389, 584,
    278, 333, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 333, 333, 584, 584, 584, 611, 975, 722, 722, 722,
    722, 667, 611, 778, 722, 278, 556, 722, 611, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333,
    278, 333, 584, 556, 278, 556, 611, 556, 611, 556, 333, 611,
    611, 278, 278, 556, 278, 889, 611, 611, 611, 611, 389, 556,
    333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584,
};
static const short times_r_w[95] = {
    250, 333, 408, 500, 500, 833, 778, 333, 333, 333, 500, 564,
    250, 333, 250, 278, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 278, 278, 564, 564, 564, 444, 921, 722, 667, 667,
    722, 611, 556, 722, 722, 333, 389, 722, 611, 889, 722, 722,
    556, 722, 667, 556, 611, 722, 722, 944, 722, 722, 611, 333,
    278, 333, 469, 500, 333, 444, 500, 444, 500, 444, 333, 500,
    500, 278, 278, 500, 278, 778, 500, 500, 500, 500, 333, 389,
    278, 500, 500, 722, 500, 500, 444, 480, 200, 480, 541,
};
static const short times_b_w[95] = {
    250, 333, 555, 500, 500, 1000, 833, 333, 333, 333, 500, 570,
    250, 333, 250, 278, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 333, 333, 570, 570, 570, 500, 930, 722, 667, 722,
    722, 667, 611, 778, 778, 389, 500, 778, 667, 944, 722, 778,
    611, 778, 722, 556, 667, 722, 722, 1000, 722, 722, 667, 333,
    278, 333, 581, 500, 333, 500, 556, 444, 556, 444, 333, 500,
    556, 278, 333, 556, 278, 833, 556, 500, 556, 556, 444, 389,
    333, 556, 500, 722, 500, 500, 444, 394, 220, 394, 520,
};
static const short times_i_w[95] = {
    250, 333, 420, 500, 500, 833, 778, 333, 333, 333, 500, 675,
    250, 333, 250, 278, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 333, 333, 675, 675, 675, 500, 920, 611, 611, 667,
    722, 611, 611, 722, 722, 333, 444, 667, 556, 833, 667, 722,
    611, 722, 611, 500, 556, 722, 611, 833, 611, 556, 556, 389,
    278, 389, 422, 500, 333, 500, 500, 444, 500, 444, 278, 500,
    500, 278, 278, 444, 278, 722, 500, 500, 500, 500, 389, 389,
    278, 500, 444, 667, 444, 444, 389, 400, 275, 400, 541,
};
static const short times_bi_w[95] = {
    250, 389, 555, 500, 500, 833, 778, 333, 333, 333, 500, 570,
    250, 333, 250, 278, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 333, 333, 570, 570, 570, 500, 832, 667, 667, 667,
    722, 667, 667, 722, 778, 389, 500, 667, 611, 889, 722, 722,
    611, 722, 667, 556, 611, 722, 667, 889, 667, 611, 611, 333,
    278, 333, 570, 500, 333, 500, 500, 444, 500, 444, 333, 500,
    556, 278, 278, 500, 278, 778, 556, 500, 500, 500, 389, 389,
    278, 556, 444, 667, 500, 444, 389, 348, 220, 348, 570,
};

/* ascender and descender by family, thousandths of the em, from the font
   metric files */
static const short famasc[3] = { 604, 683, 729 };
static const short famdsc[3] = { 186, 217, 218 };

/* PDF base font names by variant: family*4 + bold*2 + italic */
static const char* fntnam[12] = {

    "Courier",     "Courier-Oblique",  "Courier-Bold",  "Courier-BoldOblique",
    "Times-Roman", "Times-Italic",     "Times-Bold",    "Times-BoldItalic",
    "Helvetica",   "Helvetica-Oblique","Helvetica-Bold","Helvetica-BoldOblique"

};

/* growable byte buffer */
typedef struct sbuf {

    char* s;   /* data */
    long  len; /* occupied */
    long  cap; /* allocated */

} sbuf;

/* completed page list */
typedef struct pgrec* pgptr;
typedef struct pgrec {

    pgptr next; /* next page */
    sbuf  cs;   /* content stream */

} pgrec;

/* document image list */
typedef struct imgrec* imgptr;
typedef struct imgrec {

    imgptr         next; /* next image */
    long           w, h; /* dimensions in pixels */
    unsigned char* rgb;  /* pixel data, top down, rgb */
    long           idx;  /* image index in document */
    long           objn; /* assigned object number */

} imgrec;

/* per print file record */
typedef struct prtrec* prtptr;
typedef struct prtrec {

    int    prtdev;         /* goes to a printer, not a file */
    char   pdev[MAXPDEV];  /* printer name, empty for system default */
    char*  title;          /* document title, or NULL */

    sbuf   page;           /* current page content stream */
    int    anyops;         /* current page has been marked */
    pgptr  pages;          /* completed pages, in order */
    pgptr  paget;          /* tail of completed pages */
    long   npages;         /* count of completed pages */

    imgptr imgs;           /* document images */
    long   nimgs;          /* count of document images */
    imgptr pictbl[MAXPIC]; /* loadable picture slots */

    /* drawing state */
    int    fr, fg, fb;     /* foreground color, 0-255 */
    int    br, bg, bb;     /* background color, 0-255 */
    mdcod  fmod, bmod;     /* foreground and background raster modes */
    long   lw;             /* line width */
    ami_lstyle lstyle;     /* line style */
    fmcod  fam;            /* font family */
    long   fontc;          /* logical font code, 1-4 */
    long   fsiz;           /* font size, the em, in pixels */
    long   chrx, chry;     /* extra character and line spacing */
    long   curx, cury;     /* cursor, pixels, 1 based, top left of cell */
    long   angle;          /* character drawing angle, LONG_MAX ratio */
    int    aital, abold;   /* italic, bold */
    int    aunder, astrike;/* underline, strikeout */
    int    arev;           /* reverse */
    int    asuper, asub;   /* superscript, subscript */
    int    acond, aext;    /* condensed, extended */
    int    autom;          /* auto wrap/eject mode */
    float  vsx, vsy;       /* view scales */
    long   goffx, goffy;   /* view offsets */
    long   tabs[MAXTAB];   /* tab stops, pixels, sorted */
    int    ntabs;          /* number of tab stops */

    /* state as last emitted to the current page, -1 for unknown */
    int    efr, efg, efb;  /* emitted fill color */
    int    esr, esg, esb;  /* emitted stroke color */
    long   elw;            /* emitted line width */
    int    elstyle;        /* emitted line style */
    int    ebm;            /* emitted blend mode */
    int    fntuse[12];     /* fonts used in the document */

} prtrec;

/* print files table, by file descriptor */
static prtptr prtfil[MAXPFIL];

/* types of system vectors for override calls */
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*pclose_t)(int);

/* system override calls */
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);

/* saved system vectors */
static pwrite_t dn_write;
static pclose_t dn_close;

/* saved display library vectors, one per interdicted call */
static ami_cursor_t      dn_cursor;      static ami_maxx_t        dn_maxx;
static ami_maxy_t        dn_maxy;        static ami_home_t        dn_home;
static ami_del_t         dn_del;         static ami_up_t          dn_up;
static ami_down_t        dn_down;        static ami_left_t        dn_left;
static ami_right_t       dn_right;       static ami_blink_t       dn_blink;
static ami_reverse_t     dn_reverse;     static ami_underline_t   dn_underline;
static ami_superscript_t dn_superscript; static ami_subscript_t   dn_subscript;
static ami_italic_t      dn_italic;      static ami_bold_t        dn_bold;
static ami_strikeout_t   dn_strikeout;   static ami_standout_t    dn_standout;
static ami_fcolor_t      dn_fcolor;      static ami_bcolor_t      dn_bcolor;
static ami_auto_t        dn_auto;        static ami_curvis_t      dn_curvis;
static ami_scroll_t      dn_scroll;      static ami_curx_t        dn_curx;
static ami_cury_t        dn_cury;        static ami_curbnd_t      dn_curbnd;
static ami_select_t      dn_select;      static ami_event_t       dn_event;
static ami_timer_t       dn_timer;       static ami_killtimer_t   dn_killtimer;
static ami_mouse_t       dn_mouse;       static ami_mousebutton_t dn_mousebutton;
static ami_joystick_t    dn_joystick;    static ami_joybutton_t   dn_joybutton;
static ami_joyaxis_t     dn_joyaxis;     static ami_settab_t      dn_settab;
static ami_restab_t      dn_restab;      static ami_clrtab_t      dn_clrtab;
static ami_funkey_t      dn_funkey;      static ami_frametimer_t  dn_frametimer;
static ami_wrtstr_t      dn_wrtstr;      static ami_wrtstrn_t     dn_wrtstrn;
static ami_sizbuf_t      dn_sizbuf;      static ami_title_t       dn_title;
static ami_sendevent_t   dn_sendevent;

static ami_maxxg_t       dn_maxxg;       static ami_maxyg_t       dn_maxyg;
static ami_curxg_t       dn_curxg;       static ami_curyg_t       dn_curyg;
static ami_line_t        dn_line;        static ami_linewidth_t   dn_linewidth;
static ami_linestyle_t   dn_linestyle;   static ami_rect_t        dn_rect;
static ami_frect_t       dn_frect;       static ami_rrect_t       dn_rrect;
static ami_frrect_t      dn_frrect;      static ami_ellipse_t     dn_ellipse;
static ami_fellipse_t    dn_fellipse;    static ami_arc_t         dn_arc;
static ami_farc_t        dn_farc;        static ami_fchord_t      dn_fchord;
static ami_ftriangle_t   dn_ftriangle;   static ami_cursorg_t     dn_cursorg;
static ami_baseline_t    dn_baseline;    static ami_setpixel_t    dn_setpixel;
static ami_fover_t       dn_fover;       static ami_bover_t       dn_bover;
static ami_finvis_t      dn_finvis;      static ami_binvis_t      dn_binvis;
static ami_fxor_t        dn_fxor;        static ami_bxor_t        dn_bxor;
static ami_fand_t        dn_fand;        static ami_band_t        dn_band;
static ami_for_t         dn_for;         static ami_bor_t         dn_bor;
static ami_chrsizx_t     dn_chrsizx;     static ami_chrsizy_t     dn_chrsizy;
static ami_fonts_t       dn_fonts;       static ami_font_t        dn_font;
static ami_fontnam_t     dn_fontnam;     static ami_fontsiz_t     dn_fontsiz;
static ami_setpoints_t   dn_setpoints;   static ami_points_t      dn_points;
static ami_chrspcy_t     dn_chrspcy;     static ami_chrspcx_t     dn_chrspcx;
static ami_dpmx_t        dn_dpmx;        static ami_dpmy_t        dn_dpmy;
static ami_strsiz_t      dn_strsiz;      static ami_chrpos_t      dn_chrpos;
static ami_writejust_t   dn_writejust;   static ami_justpos_t     dn_justpos;
static ami_condensed_t   dn_condensed;   static ami_extended_t    dn_extended;
static ami_xlight_t      dn_xlight;      static ami_light_t       dn_light;
static ami_xbold_t       dn_xbold;       static ami_hollow_t      dn_hollow;
static ami_raised_t      dn_raised;      static ami_settabg_t     dn_settabg;
static ami_restabg_t     dn_restabg;     static ami_fcolorg_t     dn_fcolorg;
static ami_fcolorc_t     dn_fcolorc;     static ami_bcolorg_t     dn_bcolorg;
static ami_bcolorc_t     dn_bcolorc;     static ami_loadpict_t    dn_loadpict;
static ami_pictsizx_t    dn_pictsizx;    static ami_pictsizy_t    dn_pictsizy;
static ami_picture_t     dn_picture;     static ami_delpict_t     dn_delpict;
static ami_scrollg_t     dn_scrollg;     static ami_path_t        dn_path;
static ami_viewoffg_t    dn_viewoffg;    static ami_viewscale_t   dn_viewscale;
static ami_scalex_t      dn_scalex;      static ami_scaley_t      dn_scaley;
static ami_blockcopyg_t  dn_blockcopyg;  static ami_openwin_t     dn_openwin;

/*******************************************************************************

Report error and stop

*******************************************************************************/

static void error(const char* es)

{

    /* bypass the file layers: write directly to the error channel */
    write(2, "*** pdfgraph: ", 14);
    write(2, es, strlen(es));
    write(2, "\n", 1);
    exit(1);

}

/*******************************************************************************

Growable buffer

The page contents, and the finished document, are byte buffers that grow as
needed.

*******************************************************************************/

static void sbcatn(sbuf* b, const char* d, long n)

{

    char* ns;

    if (b->len+n > b->cap) {

        b->cap = b->cap? b->cap*2: 4096;
        while (b->len+n > b->cap) b->cap *= 2;
        ns = realloc(b->s, b->cap);
        if (!ns) error("Out of memory");
        b->s = ns;

    }
    memcpy(b->s+b->len, d, n);
    b->len += n;

}

static void sbf(sbuf* b, const char* fmt, ...)

{

    char    s[512];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(s, sizeof(s), fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof(s)) error("Buffer format overflow");
    sbcatn(b, s, n);

}

static void sbfree(sbuf* b)

{

    if (b->s) free(b->s);
    b->s = NULL;
    b->len = 0;
    b->cap = 0;

}

/*******************************************************************************

File and state access

*******************************************************************************/

/* index print file from FILE*, NULL if not a print file */
static prtptr txt2prt(FILE* f)

{

    int fn;

    if (!f) return (NULL);
    fn = fileno(f);
    if (fn < 0 || fn >= MAXPFIL) return (NULL);

    return (prtfil[fn]);

}

/* logical to physical transforms, as the display library */
#define L2PX(p, v) ((long)((v)*(p)->vsx)+(p)->goffx)
#define L2PY(p, v) ((long)((v)*(p)->vsy)+(p)->goffy)
#define L2PW(p, n) ((long)((n)*(p)->vsx))
#define L2PH(p, n) ((long)((n)*(p)->vsy))

/* font variant in use: family*4 + bold*2 + italic */
static int fntvar(prtptr p)

{

    return (p->fam*4+!!p->abold*2+!!p->aital);

}

/* width of a character in em thousandths for the current variant */
static long thwid(prtptr p, unsigned char c)

{

    const short* t;

    if (p->fam == fmcour) return (600); /* fixed pitch */
    switch (fntvar(p)) {

        case 4:  t = times_r_w; break;
        case 5:  t = times_i_w; break;
        case 6:  t = times_b_w; break;
        case 7:  t = times_bi_w; break;
        case 8:  t = helv_r_w; break;
        case 9:  t = helv_i_w; break;
        case 10: t = helv_b_w; break;
        case 11: t = helv_bi_w; break;
        default: t = helv_r_w;

    }
    if (c < 32 || c > 126) c = 'o'; /* estimate for the upper set */

    return (t[c-32]);

}

/* attribute scale on advance: super/subscript are reduced */
static double atscl(prtptr p)

{

    double s = 1.0;

    if (p->asuper || p->asub) s *= 0.6;
    if (p->acond) s *= 0.8;
    if (p->aext) s *= 1.2;

    return (s);

}

/* advance width of a character in pixels */
static long chwid(prtptr p, unsigned char c)

{

    return ((long)(thwid(p, c)*p->fsiz*atscl(p)/1000.0)+p->chrx);

}

/* width of a string in pixels */
static long strwid(prtptr p, const char* s, long n)

{

    long w = 0;

    while (n--) w += chwid(p, (unsigned char)*s++);

    return (w);

}

/* character cell width: the widest character of the font */
static long celwid(prtptr p)

{

    long w = 600, i;

    if (p->fam != fmcour) {

        w = 0;
        for (i = 32; i <= 126; i++)
            if (thwid(p, i) > w) w = thwid(p, i);

    }

    return ((long)(w*p->fsiz/1000.0)+p->chrx);

}

/* line advance */
static long linadv(prtptr p)

{

    return (p->fsiz+p->chry);

}

/* ascent of the current font in pixels: top of cell to baseline */
static long ascpx(prtptr p)

{

    return ((long)(p->fsiz*(double)famasc[p->fam]/
                   (famasc[p->fam]+famdsc[p->fam])));

}

/*******************************************************************************

Page content emission

The current page starts lazily: the first operation lays down the transform
from PDF points to the 600 DPI page space. The graphics state emitters place
colors, width and blend mode only when they differ from what the page already
holds.

*******************************************************************************/

/* begin the page if it has not begun */
static void pagebeg(prtptr p)

{

    if (!p->anyops) {

        /* points to page space: scale by 72/600 and flip y */
        sbf(&p->page, "q 0.12 0 0 -0.12 0 %d cm\n", PDFH);
        p->anyops = 1;
        /* all graphics state is unknown to the new page */
        p->efr = -1; p->esr = -1; p->elw = -1; p->elstyle = -1; p->ebm = -1;

    }

}

/* emit blend mode for a raster mode */
static void embm(prtptr p, mdcod md)

{

    int bm;

    switch (md) {

        case mdxor: bm = 1; break;
        case mdand: bm = 2; break;
        case mdor:  bm = 3; break;
        default:    bm = 0;

    }
    if (bm != p->ebm) { sbf(&p->page, "/GS%d gs\n", bm); p->ebm = bm; }

}

/* emit fill color */
static void emfill(prtptr p, int r, int g, int b)

{

    if (r != p->efr || g != p->efg || b != p->efb) {

        sbf(&p->page, "%.3f %.3f %.3f rg\n", r/255.0, g/255.0, b/255.0);
        p->efr = r; p->efg = g; p->efb = b;

    }

}

/* emit stroke color */
static void emstroke(prtptr p)

{

    if (p->fr != p->esr || p->fg != p->esg || p->fb != p->esb) {

        sbf(&p->page, "%.3f %.3f %.3f RG\n",
            p->fr/255.0, p->fg/255.0, p->fb/255.0);
        p->esr = p->fr; p->esg = p->fg; p->esb = p->fb;

    }

}

/* emit line width and style */
static void emline(prtptr p)

{

    long w = L2PW(p, p->lw);
    long u;

    if (w < 1) w = 1;
    if (w != p->elw) { sbf(&p->page, "%ld w\n", w); p->elw = w; }
    if ((int)p->lstyle != p->elstyle) {

        u = w > 6? w: 6; /* keep patterns visible at hairline widths */
        switch (p->lstyle) {

            case ami_lsdash: sbf(&p->page, "[%ld %ld] 0 d\n", u*8, u*4); break;
            case ami_lsdot:  sbf(&p->page, "[%ld %ld] 0 d\n", u, u*3); break;
            default:         sbf(&p->page, "[] 0 d\n");

        }
        p->elstyle = p->lstyle;

    }

}

/* prepare for a fill; FALSE if the foreground is invisible */
static int fillpre(prtptr p)

{

    if (p->fmod == mdinvis) return (0);
    pagebeg(p);
    embm(p, p->fmod);
    emfill(p, p->fr, p->fg, p->fb);

    return (1);

}

/* prepare for a stroke; FALSE if the foreground is invisible */
static int strokepre(prtptr p)

{

    if (p->fmod == mdinvis) return (0);
    pagebeg(p);
    embm(p, p->fmod);
    emstroke(p);
    emline(p);

    return (1);

}

/*******************************************************************************

Text output

Text is set in the standard PDF font matching the current family and the bold
and italic attributes, at the current size, with underlines and strikeouts
drawn as rules. The character drawing angle rotates the baseline. The
background, unless invisible, is laid down as a cell high bar behind the run,
just as the character cell background paints on a display.

*******************************************************************************/

/* escape a string into PDF literal form */
static void emstr(prtptr p, const char* s, long n)

{

    unsigned char c;

    sbcatn(&p->page, "(", 1);
    while (n--) {

        c = *s++;
        if (c == '(' || c == ')' || c == '\\')
            sbf(&p->page, "\\%c", c);
        else if (c < 32 || c > 126) sbf(&p->page, "\\%03o", c);
        else sbcatn(&p->page, (char*)&c, 1);

    }
    sbcatn(&p->page, ") ", 2);

}

/* set a text run at the cursor and advance. Spacing, if not NULL, is the
   width each space character is to occupy, with rem extra pixels spread over
   the first spaces, for justified writes. */
static void emrun(prtptr p, const char* s, long n, long spc, long rem)

{

    long   w, i, a, sw;
    long   bx, by;
    int    fr, fgc, fb, br, bgc, bb;
    long   asc;
    double th, cs, sn, siz;
    long   rise;

    if (!n) return;
    /* the run width */
    if (spc < 0) w = strwid(p, s, n);
    else {

        w = rem;
        for (i = 0; i < n; i++)
            w += s[i] == ' '? spc: chwid(p, (unsigned char)s[i]);

    }
    pagebeg(p);
    /* resolve reverse: swap the colors for this run */
    fr = p->fr; fgc = p->fg; fb = p->fb;
    br = p->br; bgc = p->bg; bb = p->bb;
    if (p->arev) {

        fr = p->br; fgc = p->bg; fb = p->bb;
        br = p->fr; bgc = p->fg; bb = p->fb;

    }
    bx = L2PX(p, p->curx); by = L2PY(p, p->cury);
    /* the cell background */
    if (p->bmod != mdinvis) {

        embm(p, p->bmod);
        emfill(p, br, bgc, bb);
        sbf(&p->page, "%ld %ld %ld %ld re f\n",
            bx-1, by-1, L2PW(p, w), L2PH(p, linadv(p)));

    }
    if (p->fmod == mdinvis) { p->curx += w; return; }
    embm(p, p->fmod);
    emfill(p, fr, fgc, fb);
    p->fntuse[fntvar(p)] = 1;
    asc = ascpx(p);
    siz = p->fsiz*(p->asuper || p->asub? 0.6: 1.0);
    rise = 0;
    if (p->asuper) rise = (long)(p->fsiz*0.33);
    if (p->asub) rise = -(long)(p->fsiz*0.15);
    /* the baseline, rotated by the drawing angle */
    th = ((double)p->angle/LONG_MAX-0.25)*2.0*M_PI;
    cs = cos(th); sn = sin(th);
    sbf(&p->page, "BT /F%d %.2f Tf\n", fntvar(p), L2PH(p, (long)siz)*1.0);
    if (p->acond || p->aext)
        sbf(&p->page, "%d Tz\n", p->acond? 80: 120);
    if (p->chrx) sbf(&p->page, "%ld Tc\n", L2PW(p, p->chrx));
    if (rise) sbf(&p->page, "%ld Ts\n", L2PH(p, rise));
    sbf(&p->page, "%.4f %.4f %.4f %.4f %ld %ld Tm\n",
        cs, -sn, -sn, -cs, bx-1, by-1+asc);
    if (spc < 0) { emstr(p, s, n); sbf(&p->page, "Tj ET\n"); }
    else {

        /* justified: spread with kern adjustments after each space */
        sbcatn(&p->page, "[", 1);
        i = 0;
        while (i < n) {

            a = i;
            while (i < n && s[i] != ' ') i++;
            if (i > a) emstr(p, s+a, i-a);
            while (i < n && s[i] == ' ') {

                sw = spc;
                if (rem) { sw++; rem--; }
                emstr(p, s+i, 1);
                /* the adjustment is in em thousandths, and subtracts */
                sbf(&p->page, "%ld ",
                    -(long)((sw-chwid(p, ' '))*1000.0/(siz*atscl(p)/1.0)));
                i++;

            }

        }
        sbf(&p->page, "] TJ ET\n");

    }
    if (p->acond || p->aext) sbf(&p->page, "BT 100 Tz ET\n");
    /* underline and strikeout rules */
    if (p->aunder)
        sbf(&p->page, "%ld %ld %ld %ld re f\n",
            bx-1, by-1+asc+(long)(p->fsiz*0.06),
            L2PW(p, w), (long)(p->fsiz*0.05)+1);
    if (p->astrike)
        sbf(&p->page, "%ld %ld %ld %ld re f\n",
            bx-1, by-1+asc-(long)(asc*0.30),
            L2PW(p, w), (long)(p->fsiz*0.05)+1);
    p->curx += w;

}

/*******************************************************************************

Page control

*******************************************************************************/

static void prthome(prtptr p)

{

    p->curx = 1;
    p->cury = 1;

}

/* eject the page: complete the current page, even if blank, and begin anew */
static void formfeed(prtptr p)

{

    pgptr pg;

    pagebeg(p); /* even a blank page is a page */
    sbf(&p->page, "Q\n");
    pg = malloc(sizeof(pgrec));
    if (!pg) error("Out of memory");
    pg->cs = p->page; /* the page takes the buffer */
    pg->next = NULL;
    if (p->paget) p->paget->next = pg;
    else p->pages = pg;
    p->paget = pg;
    p->npages++;
    p->page.s = NULL; p->page.len = 0; p->page.cap = 0;
    p->anyops = 0;
    prthome(p);

}

/* line feed, honoring the page bottom in auto mode */
static void linefeed(prtptr p)

{

    p->curx = 1;
    p->cury += linadv(p);
    if (p->autom && p->cury+p->fsiz-1 > PAGEH) formfeed(p);

}

/*******************************************************************************

Document assembly

The finished document is standard PDF 1.4: the completed pages become page
objects with their content streams, the fonts that were touched become
standard font objects, the blend modes and images used are carried, and the
cross reference table closes the file.

*******************************************************************************/

static const char* bmnames[4] = { "Normal", "Exclusion", "Multiply", "Screen" };

static void assemble(prtptr p, sbuf* doc)

{

    long   nobj, i, k;
    long*  off;
    long   fobj[12];
    long   gobj[4];
    imgptr ip;
    pgptr  pg;
    long   objn, xref;

    /* if nothing was ever printed, the document is one blank page */
    if (!p->npages && !p->anyops) formfeed(p);
    /* an unfinished page is ejected, as the manual allows */
    if (p->anyops) formfeed(p);

    /* assign object numbers: 1 catalog, 2 pages, 3 info, 4 resources,
       then fonts, blend modes, images, then content and page per page */
    objn = 5;
    for (i = 0; i < 12; i++) { fobj[i] = 0; if (p->fntuse[i]) fobj[i] = objn++; }
    for (i = 0; i < 4; i++) gobj[i] = objn++;
    for (ip = p->imgs; ip; ip = ip->next) ip->objn = objn++;
    nobj = objn-1+p->npages*2;
    off = calloc(nobj+1, sizeof(long));
    if (!off) error("Out of memory");

    sbf(doc, "%%PDF-1.4\n%%\342\343\317\323\n");

    off[1] = doc->len;
    sbf(doc, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    off[2] = doc->len;
    sbf(doc, "2 0 obj\n<< /Type /Pages /Count %ld /MediaBox [0 0 %d %d]\n"
             "/Kids [", p->npages, PDFW, PDFH);
    for (i = 0; i < p->npages; i++) sbf(doc, "%ld 0 R ", objn+i*2+1);
    sbf(doc, "] >>\nendobj\n");

    off[3] = doc->len;
    sbf(doc, "3 0 obj\n<< /Producer (Petit-Ami pdfgraph)");
    if (p->title) {

        sbf(doc, " /Title (");
        for (i = 0; p->title[i]; i++) {

            unsigned char c = p->title[i];
            if (c == '(' || c == ')' || c == '\\') sbf(doc, "\\%c", c);
            else if (c < 32 || c > 126) sbf(doc, "\\%03o", c);
            else sbcatn(doc, (char*)&c, 1);

        }
        sbf(doc, ")");

    }
    sbf(doc, " >>\nendobj\n");

    off[4] = doc->len;
    sbf(doc, "4 0 obj\n<< /ProcSet [/PDF /Text /ImageC]\n/Font << ");
    for (i = 0; i < 12; i++)
        if (fobj[i]) sbf(doc, "/F%ld %ld 0 R ", i, fobj[i]);
    sbf(doc, ">>\n/ExtGState << ");
    for (i = 0; i < 4; i++) sbf(doc, "/GS%ld %ld 0 R ", i, gobj[i]);
    sbf(doc, ">>\n/XObject << ");
    for (ip = p->imgs; ip; ip = ip->next)
        sbf(doc, "/Im%ld %ld 0 R ", ip->idx, ip->objn);
    sbf(doc, ">> >>\nendobj\n");

    for (i = 0; i < 12; i++) if (fobj[i]) {

        off[fobj[i]] = doc->len;
        sbf(doc, "%ld 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /%s "
                 "/Encoding /WinAnsiEncoding >>\nendobj\n", fobj[i], fntnam[i]);

    }

    for (i = 0; i < 4; i++) {

        off[gobj[i]] = doc->len;
        sbf(doc, "%ld 0 obj\n<< /Type /ExtGState /BM /%s >>\nendobj\n",
            gobj[i], bmnames[i]);

    }

    for (ip = p->imgs; ip; ip = ip->next) {

        off[ip->objn] = doc->len;
        sbf(doc, "%ld 0 obj\n<< /Type /XObject /Subtype /Image /Width %ld "
                 "/Height %ld /ColorSpace /DeviceRGB /BitsPerComponent 8 "
                 "/Length %ld >>\nstream\n", ip->objn, ip->w, ip->h,
                 ip->w*ip->h*3);
        sbcatn(doc, (char*)ip->rgb, ip->w*ip->h*3);
        sbf(doc, "\nendstream\nendobj\n");

    }

    k = objn;
    for (pg = p->pages; pg; pg = pg->next) {

        off[k] = doc->len;
        sbf(doc, "%ld 0 obj\n<< /Length %ld >>\nstream\n", k, pg->cs.len);
        sbcatn(doc, pg->cs.s, pg->cs.len);
        sbf(doc, "endstream\nendobj\n");
        off[k+1] = doc->len;
        sbf(doc, "%ld 0 obj\n<< /Type /Page /Parent 2 0 R /Resources 4 0 R "
                 "/Contents %ld 0 R >>\nendobj\n", k+1, k);
        k += 2;

    }

    xref = doc->len;
    sbf(doc, "xref\n0 %ld\n0000000000 65535 f \n", nobj+1);
    for (i = 1; i <= nobj; i++) sbf(doc, "%010ld 00000 n \n", off[i]);
    sbf(doc, "trailer\n<< /Size %ld /Root 1 0 R /Info 3 0 R >>\n"
             "startxref\n%ld\n%%%%EOF\n", nobj+1, xref);
    free(off);

}

/*******************************************************************************

Document output

A file name document writes to its own descriptor, below our own
interdiction. A printer document is handed to the spooler.

*******************************************************************************/

static void prtout(int fd, prtptr p)

{

    sbuf    doc = { NULL, 0, 0 };
    long    i;
    ssize_t r;
    char    cmd[MAXPDEV+32];
    FILE*   pf;

    assemble(p, &doc);
    if (p->prtdev) {

        /* to the spooler; -s silences the job id report */
        if (p->pdev[0])
            snprintf(cmd, sizeof(cmd), "lp -s -d '%s' -", p->pdev);
        else snprintf(cmd, sizeof(cmd), "lp -s -");
        pf = popen(cmd, "w");
        if (!pf) error("Cannot start print spooler");
        if (fwrite(doc.s, 1, doc.len, pf) != (size_t)doc.len)
            error("Cannot spool print job");
        if (pclose(pf)) error("Print spooler failed");

    } else {

        i = 0;
        while (i < doc.len) {

            r = (*dn_write)(fd, doc.s+i, doc.len-i);
            if (r <= 0) error("Cannot write print file");
            i += r;

        }

    }
    sbfree(&doc);

}

/* take down and free a print file entry */
static void prtfree(int fd)

{

    prtptr p = prtfil[fd];
    pgptr  pg;
    imgptr ip;

    prtout(fd, p);
    while (p->pages) {

        pg = p->pages;
        p->pages = pg->next;
        sbfree(&pg->cs);
        free(pg);

    }
    while (p->imgs) {

        ip = p->imgs;
        p->imgs = ip->next;
        free(ip->rgb);
        free(ip);

    }
    sbfree(&p->page);
    if (p->title) free(p->title);
    free(p);
    prtfil[fd] = NULL;

}

/*******************************************************************************

Open print file

Creates a print file. The name is either a filename for the .pdf document, or
a printer, recognized by the ':' in the name: "lp0:", "lp1:", ... select the
printer by number as the spooler lists them, and "lp:" is the system default
printer.

*******************************************************************************/

static void fndprinter(int pn, char* dst, int dstl)

{

    FILE* lp;
    char  ln[MAXPDEV];
    int   i = 0;

    dst[0] = 0;
    lp = popen("lpstat -e 2>/dev/null", "r");
    if (!lp) error("Cannot enumerate printers");
    while (fgets(ln, sizeof(ln), lp)) {

        ln[strcspn(ln, "\n")] = 0;
        if (i == pn && ln[0]) { strncpy(dst, ln, dstl-1); dst[dstl-1] = 0; }
        i++;

    }
    pclose(lp);
    if (!dst[0]) error("No such printer");
    if (strchr(dst, '\'')) error("Unusable printer name");

}

void ami_openprint(FILE** f, char* n)

{

    prtptr p;
    int    fd, i;
    char*  cp;
    long   pn;

    p = calloc(1, sizeof(prtrec));
    if (!p) error("Out of memory");
    cp = strchr(n, ':');
    if (cp) { /* a printer device */

        if (n[0] != 'l' || n[1] != 'p' || cp[1])
            error("Invalid printer device name");
        p->prtdev = 1;
        if (cp != n+2) { /* numbered: find it now, to fail early */

            pn = strtol(n+2, &cp, 10);
            if (*cp != ':') error("Invalid printer device name");
            fndprinter(pn, p->pdev, MAXPDEV);

        }
        /* the descriptor only anchors the file id */
        fd = open("/dev/null", O_WRONLY);

    } else fd = open(n, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd < 0) error("Cannot open print file");
    if (fd >= MAXPFIL) error("Invalid file handle");

    /* the default state: 12 point terminal font, black on white, page top */
    p->fr = 0; p->fg = 0; p->fb = 0;
    p->br = 255; p->bg = 255; p->bb = 255;
    p->fmod = mdnorm; p->bmod = mdnorm;
    p->lw = 1;
    p->lstyle = ami_lssolid;
    p->fam = fmcour;
    p->fontc = AMI_FONT_TERM;
    p->fsiz = DEFSIZ;
    p->angle = LONG_MAX/4;
    p->autom = 1;
    p->vsx = 1.0; p->vsy = 1.0;
    prthome(p);
    /* tabs every 8 characters, as a terminal presents */
    for (i = 1; i*8*60+1 < PAGEW && p->ntabs < MAXTAB; i++)
        p->tabs[p->ntabs++] = (long)i*8*60+1;
    prtfil[fd] = p;

    *f = fdopen(fd, "w");
    if (!*f) error("Cannot open print file");
    /* unbuffered, so that writes and drawing calls stay in order */
    setvbuf(*f, NULL, _IONBF, 0);

}

/*******************************************************************************

System vector interdiction

Writes to a print file are the character stream: printables set text at the
cursor, and the controls carry their printer meanings, with form-feed
completing the page. The close completes the document.

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buf, size_t n)

{

    prtptr      p;
    const char* s = buf;
    size_t      i = 0;
    long        a, w;

    if (fd < 0 || fd >= MAXPFIL || !prtfil[fd])
        return ((*dn_write)(fd, buf, n));
    p = prtfil[fd];
    while (i < n) {

        unsigned char c = s[i];
        if (c >= 32 && c != 127) {

            /* batch a run of printables, wrapping in auto mode */
            a = i;
            w = 0;
            while (i < n && (unsigned char)s[i] >= 32 &&
                   (unsigned char)s[i] != 127) {

                long cw = chwid(p, (unsigned char)s[i]);
                if (p->autom && p->curx+w+cw-1 > PAGEW) break;
                w += cw;
                i++;

            }
            if (i == a) { linefeed(p); continue; } /* no room: wrap first */
            emrun(p, s+a, i-a, -1, 0);

        } else {

            switch (c) {

                case '\n': linefeed(p); break;
                case '\r': p->curx = 1; break;
                case '\f': formfeed(p); break;
                case '\b': p->curx -= celwid(p);
                           if (p->curx < 1) p->curx = 1;
                           break;
                case '\t': {

                    int ti;
                    for (ti = 0; ti < p->ntabs; ti++)
                        if (p->tabs[ti] > p->curx)
                            { p->curx = p->tabs[ti]; break; }
                    break;

                }
                default: break; /* other controls are dropped */

            }
            i++;

        }

    }

    return (n);

}

static int iclose(int fd)

{

    if (fd >= 0 && fd < MAXPFIL && prtfil[fd]) prtfree(fd);

    return ((*dn_close)(fd));

}

/*******************************************************************************

Display vector interdiction

Each vector of the drawing set checks for a print file and either executes
into the page or passes down the chain. The input side calls give errors on a
print file, as the manual specifies: a printer has no input.

*******************************************************************************/

static void noinput(void) { error("No input from a print file"); }

/* terminal level */

static void cursor_pvf(FILE* f, long x, long y)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_cursor)(f, x, y); return; }
    p->curx = (x-1)*celwid(p)+1;
    p->cury = (y-1)*linadv(p)+1;
}

static long maxx_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_maxx)(f));
    return (PAGEW/celwid(p));
}

static long maxy_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_maxy)(f));
    return (PAGEH/linadv(p));
}

static void home_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_home)(f); return; }
    prthome(p);
}

static void del_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    long w;
    if (!p) { (*dn_del)(f); return; }
    w = celwid(p);
    p->curx -= w;
    if (p->curx < 1) p->curx = 1;
    /* erase the cell */
    if (p->bmod != mdinvis) {

        pagebeg(p);
        embm(p, p->bmod);
        emfill(p, p->br, p->bg, p->bb);
        sbf(&p->page, "%ld %ld %ld %ld re f\n",
            L2PX(p, p->curx)-1, L2PY(p, p->cury)-1,
            L2PW(p, w), L2PH(p, linadv(p)));

    }
}

static void up_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_up)(f); return; }
    p->cury -= linadv(p);
}

static void down_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_down)(f); return; }
    p->cury += linadv(p);
}

static void left_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_left)(f); return; }
    p->curx -= celwid(p);
}

static void right_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_right)(f); return; }
    p->curx += celwid(p);
}

/* attributes. Blink does not exist on paper, as the manual notes. */

static void blink_pvf(FILE* f, long e)
{
    if (!txt2prt(f)) (*dn_blink)(f, e);
}

static void reverse_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_reverse)(f, e); return; }
    p->arev = !!e;
}

static void underline_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_underline)(f, e); return; }
    p->aunder = !!e;
}

static void superscript_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_superscript)(f, e); return; }
    p->asuper = !!e;
}

static void subscript_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_subscript)(f, e); return; }
    p->asub = !!e;
}

static void italic_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_italic)(f, e); return; }
    p->aital = !!e;
}

static void bold_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_bold)(f, e); return; }
    p->abold = !!e;
}

static void strikeout_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_strikeout)(f, e); return; }
    p->astrike = !!e;
}

static void standout_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_standout)(f, e); return; }
    p->arev = !!e; /* standout presents as reverse */
}

/* colors */

static void colrgb(ami_color c, int* r, int* g, int* b)
{
    switch (c) {

        case ami_black:     *r = 0;   *g = 0;   *b = 0;   break;
        case ami_white:     *r = 255; *g = 255; *b = 255; break;
        case ami_red:       *r = 255; *g = 0;   *b = 0;   break;
        case ami_green:     *r = 0;   *g = 255; *b = 0;   break;
        case ami_blue:      *r = 0;   *g = 0;   *b = 255; break;
        case ami_cyan:      *r = 0;   *g = 255; *b = 255; break;
        case ami_yellow:    *r = 255; *g = 255; *b = 0;   break;
        case ami_magenta:   *r = 255; *g = 0;   *b = 255; break;
        case ami_backcolor: *r = 247; *g = 247; *b = 247; break;
        default:            *r = 0;   *g = 0;   *b = 0;

    }
}

static void fcolor_pvf(FILE* f, ami_color c)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_fcolor)(f, c); return; }
    colrgb(c, &p->fr, &p->fg, &p->fb);
}

static void bcolor_pvf(FILE* f, ami_color c)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_bcolor)(f, c); return; }
    colrgb(c, &p->br, &p->bg, &p->bb);
}

/* LONG_MAX ratio to color byte */
static int ratb(long v)
{
    if (v < 0) v = 0;
    return ((int)(v/(LONG_MAX/255)));
}

static void fcolorg_pvf(FILE* f, long r, long g, long b)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_fcolorg)(f, r, g, b); return; }
    p->fr = ratb(r); p->fg = ratb(g); p->fb = ratb(b);
}

static void fcolorc_pvf(FILE* f, long r, long g, long b)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_fcolorc)(f, r, g, b); return; }
    p->fr = ratb(r); p->fg = ratb(g); p->fb = ratb(b);
}

static void bcolorg_pvf(FILE* f, long r, long g, long b)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_bcolorg)(f, r, g, b); return; }
    p->br = ratb(r); p->bg = ratb(g); p->bb = ratb(b);
}

static void bcolorc_pvf(FILE* f, long r, long g, long b)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_bcolorc)(f, r, g, b); return; }
    p->br = ratb(r); p->bg = ratb(g); p->bb = ratb(b);
}

static void auto_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_auto)(f, e); return; }
    p->autom = !!e;
}

static void curvis_pvf(FILE* f, long e)
{
    if (!txt2prt(f)) (*dn_curvis)(f, e);
    /* there is no cursor on paper */
}

static void scroll_pvf(FILE* f, long x, long y)
{
    if (!txt2prt(f)) { (*dn_scroll)(f, x, y); return; }
    error("Cannot scroll a print file");
}

static long curx_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_curx)(f));
    return ((p->curx-1)/celwid(p)+1);
}

static long cury_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_cury)(f));
    return ((p->cury-1)/linadv(p)+1);
}

static long curbnd_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_curbnd)(f));
    return (p->curx >= 1 && p->curx+celwid(p)-1 <= PAGEW &&
            p->cury >= 1 && p->cury+linadv(p)-1 <= PAGEH);
}

/* the input side does not exist on a print file */

static void select_pvf(FILE* f, long u, long d)
{
    if (!txt2prt(f)) { (*dn_select)(f, u, d); return; }
    error("Cannot select screens on a print file");
}

static void event_pvf(FILE* f, ami_evtrec* er)
{
    if (!txt2prt(f)) { (*dn_event)(f, er); return; }
    noinput();
}

static void timer_pvf(FILE* f, long i, long t, long r)
{
    if (!txt2prt(f)) { (*dn_timer)(f, i, t, r); return; }
    noinput();
}

static void killtimer_pvf(FILE* f, long i)
{
    if (!txt2prt(f)) { (*dn_killtimer)(f, i); return; }
    noinput();
}

static long mouse_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_mouse)(f));
    noinput();
    return (0);
}

static long mousebutton_pvf(FILE* f, long m)
{
    if (!txt2prt(f)) return ((*dn_mousebutton)(f, m));
    noinput();
    return (0);
}

static long joystick_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_joystick)(f));
    noinput();
    return (0);
}

static long joybutton_pvf(FILE* f, long j)
{
    if (!txt2prt(f)) return ((*dn_joybutton)(f, j));
    noinput();
    return (0);
}

static long joyaxis_pvf(FILE* f, long j)
{
    if (!txt2prt(f)) return ((*dn_joyaxis)(f, j));
    noinput();
    return (0);
}

static long funkey_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_funkey)(f));
    noinput();
    return (0);
}

static void frametimer_pvf(FILE* f, long e)
{
    if (!txt2prt(f)) { (*dn_frametimer)(f, e); return; }
    noinput();
}

static void sendevent_pvf(FILE* f, ami_evtrec* er)
{
    if (!txt2prt(f)) { (*dn_sendevent)(f, er); return; }
    noinput();
}

/* tabs */

static void instab(prtptr p, long t)
{
    int i, j;
    for (i = 0; i < p->ntabs; i++) {

        if (p->tabs[i] == t) return;
        if (p->tabs[i] > t) break;

    }
    if (p->ntabs >= MAXTAB) error("Too many tabs");
    for (j = p->ntabs; j > i; j--) p->tabs[j] = p->tabs[j-1];
    p->tabs[i] = t;
    p->ntabs++;
}

static void remtab(prtptr p, long t)
{
    int i, j;
    for (i = 0; i < p->ntabs; i++) if (p->tabs[i] == t) {

        for (j = i; j < p->ntabs-1; j++) p->tabs[j] = p->tabs[j+1];
        p->ntabs--;
        return;

    }
}

static void settab_pvf(FILE* f, long t)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_settab)(f, t); return; }
    instab(p, (t-1)*celwid(p)+1);
}

static void restab_pvf(FILE* f, long t)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_restab)(f, t); return; }
    remtab(p, (t-1)*celwid(p)+1);
}

static void clrtab_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_clrtab)(f); return; }
    p->ntabs = 0;
}

static void settabg_pvf(FILE* f, long t)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_settabg)(f, t); return; }
    instab(p, t);
}

static void restabg_pvf(FILE* f, long t)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_restabg)(f, t); return; }
    remtab(p, t);
}

/* direct strings */

static void wrtstr_pvf(FILE* f, char* s)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_wrtstr)(f, s); return; }
    emrun(p, s, strlen(s), -1, 0);
}

static void wrtstrn_pvf(FILE* f, char* s, long n)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_wrtstrn)(f, s, n); return; }
    emrun(p, s, n, -1, 0);
}

static void sizbuf_pvf(FILE* f, long x, long y)
{
    if (!txt2prt(f)) { (*dn_sizbuf)(f, x, y); return; }
    error("Cannot resize a print page");
}

static void title_pvf(FILE* f, char* ts)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_title)(f, ts); return; }
    /* the title of a print file is the document title */
    if (p->title) free(p->title);
    p->title = malloc(strlen(ts)+1);
    if (!p->title) error("Out of memory");
    strcpy(p->title, ts);
}

/* graphical level */

static long maxxg_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_maxxg)(f));
    return (PAGEW);
}

static long maxyg_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_maxyg)(f));
    return (PAGEH);
}

static long curxg_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_curxg)(f));
    return (p->curx);
}

static long curyg_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_curyg)(f));
    return (p->cury);
}

static void cursorg_pvf(FILE* f, long x, long y)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_cursorg)(f, x, y); return; }
    p->curx = x;
    p->cury = y;
}

static long baseline_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_baseline)(f));
    return (ascpx(p));
}

static void line_pvf(FILE* f, long x1, long y1, long x2, long y2)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_line)(f, x1, y1, x2, y2); return; }
    if (!strokepre(p)) return;
    sbf(&p->page, "%.1f %.1f m %.1f %.1f l S\n",
        L2PX(p, x1)-0.5, L2PY(p, y1)-0.5, L2PX(p, x2)-0.5, L2PY(p, y2)-0.5);
}

static void linewidth_pvf(FILE* f, long w)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_linewidth)(f, w); return; }
    p->lw = w;
}

static void linestyle_pvf(FILE* f, ami_lstyle style)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_linestyle)(f, style); return; }
    p->lstyle = style;
}

/* rationalize a rectangle to right/down */
static void ratrect(long* x1, long* y1, long* x2, long* y2)
{
    long t;
    if (*x1 > *x2) { t = *x1; *x1 = *x2; *x2 = t; }
    if (*y1 > *y2) { t = *y1; *y1 = *y2; *y2 = t; }
}

static void rect_pvf(FILE* f, long x1, long y1, long x2, long y2)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_rect)(f, x1, y1, x2, y2); return; }
    ratrect(&x1, &y1, &x2, &y2);
    if (!strokepre(p)) return;
    sbf(&p->page, "%.1f %.1f %.1f %.1f re S\n",
        L2PX(p, x1)-0.5, L2PY(p, y1)-0.5,
        (double)L2PW(p, x2-x1+1)-1, (double)L2PH(p, y2-y1+1)-1);
}

static void frect_pvf(FILE* f, long x1, long y1, long x2, long y2)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_frect)(f, x1, y1, x2, y2); return; }
    ratrect(&x1, &y1, &x2, &y2);
    if (!fillpre(p)) return;
    sbf(&p->page, "%ld %ld %ld %ld re f\n",
        L2PX(p, x1)-1, L2PY(p, y1)-1, L2PW(p, x2-x1+1), L2PH(p, y2-y1+1));
}

/* emit a rounded rectangle path */
static void rrpath(prtptr p, long x1, long y1, long x2, long y2,
                   long xs, long ys)
{
    double l = L2PX(p, x1)-1, t = L2PY(p, y1)-1;
    double r = l+L2PW(p, x2-x1+1), b = t+L2PH(p, y2-y1+1);
    double rx = L2PW(p, xs)/2.0, ry = L2PH(p, ys)/2.0;
    double k = 0.552284749831;

    if (rx > (r-l)/2) rx = (r-l)/2;
    if (ry > (b-t)/2) ry = (b-t)/2;
    sbf(&p->page, "%.1f %.1f m\n", l+rx, t);
    sbf(&p->page, "%.1f %.1f l %.1f %.1f %.1f %.1f %.1f %.1f c\n",
        r-rx, t,  r-rx+k*rx, t,  r, t+ry-k*ry,  r, t+ry);
    sbf(&p->page, "%.1f %.1f l %.1f %.1f %.1f %.1f %.1f %.1f c\n",
        r, b-ry,  r, b-ry+k*ry,  r-rx+k*rx, b,  r-rx, b);
    sbf(&p->page, "%.1f %.1f l %.1f %.1f %.1f %.1f %.1f %.1f c\n",
        l+rx, b,  l+rx-k*rx, b,  l, b-ry+k*ry,  l, b-ry);
    sbf(&p->page, "%.1f %.1f l %.1f %.1f %.1f %.1f %.1f %.1f c h\n",
        l, t+ry,  l, t+ry-k*ry,  l+rx-k*rx, t,  l+rx, t);
}

static void rrect_pvf(FILE* f, long x1, long y1, long x2, long y2,
                      long xs, long ys)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_rrect)(f, x1, y1, x2, y2, xs, ys); return; }
    ratrect(&x1, &y1, &x2, &y2);
    if (!strokepre(p)) return;
    rrpath(p, x1, y1, x2, y2, xs, ys);
    sbf(&p->page, "S\n");
}

static void frrect_pvf(FILE* f, long x1, long y1, long x2, long y2,
                       long xs, long ys)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_frrect)(f, x1, y1, x2, y2, xs, ys); return; }
    ratrect(&x1, &y1, &x2, &y2);
    if (!fillpre(p)) return;
    rrpath(p, x1, y1, x2, y2, xs, ys);
    sbf(&p->page, "f\n");
}

/* emit an elliptical arc as bezier segments. Angles are radians from three
   o'clock, counterclockwise as seen on the page. */
static void arcsegs(prtptr p, double cx, double cy, double rx, double ry,
                    double a0, double a1, int mv)
{
    int    n, i;
    double d, t0, t1, h;
    double x0, y0, x3, y3;

    d = a1-a0;
    n = (int)(fabs(d)/(M_PI/2))+1;
    if (mv) sbf(&p->page, "%.1f %.1f m\n", cx+rx*cos(a0), cy-ry*sin(a0));
    for (i = 0; i < n; i++) {

        t0 = a0+d*i/n;
        t1 = a0+d*(i+1)/n;
        h = 4.0/3.0*tan((t1-t0)/4);
        x0 = cx+rx*cos(t0); y0 = cy-ry*sin(t0);
        x3 = cx+rx*cos(t1); y3 = cy-ry*sin(t1);
        sbf(&p->page, "%.1f %.1f %.1f %.1f %.1f %.1f c\n",
            x0-h*rx*sin(t0), y0-h*ry*cos(t0),
            x3+h*rx*sin(t1), y3+h*ry*cos(t1),
            x3, y3);

    }
}

/* find center and radii of the bounding rectangle */
static void elpparm(prtptr p, long x1, long y1, long x2, long y2,
                    double* cx, double* cy, double* rx, double* ry)
{
    double l = L2PX(p, x1)-1, t = L2PY(p, y1)-1;
    double w = L2PW(p, x2-x1+1), h = L2PH(p, y2-y1+1);

    *cx = l+w/2; *cy = t+h/2;
    *rx = w/2; *ry = h/2;
}

static void ellipse_pvf(FILE* f, long x1, long y1, long x2, long y2)
{
    prtptr p = txt2prt(f);
    double cx, cy, rx, ry;
    if (!p) { (*dn_ellipse)(f, x1, y1, x2, y2); return; }
    ratrect(&x1, &y1, &x2, &y2);
    if (!strokepre(p)) return;
    elpparm(p, x1, y1, x2, y2, &cx, &cy, &rx, &ry);
    arcsegs(p, cx, cy, rx, ry, 0, 2*M_PI, 1);
    sbf(&p->page, "h S\n");
}

static void fellipse_pvf(FILE* f, long x1, long y1, long x2, long y2)
{
    prtptr p = txt2prt(f);
    double cx, cy, rx, ry;
    if (!p) { (*dn_fellipse)(f, x1, y1, x2, y2); return; }
    ratrect(&x1, &y1, &x2, &y2);
    if (!fillpre(p)) return;
    elpparm(p, x1, y1, x2, y2, &cx, &cy, &rx, &ry);
    arcsegs(p, cx, cy, rx, ry, 0, 2*M_PI, 1);
    sbf(&p->page, "h f\n");
}

/* arc angle in LONG_MAX ratio to radians */
static double arcrad(long a)
{
    return ((double)a/LONG_MAX*2.0*M_PI);
}

static void arc_pvf(FILE* f, long x1, long y1, long x2, long y2,
                    long sa, long ea)
{
    prtptr p = txt2prt(f);
    double cx, cy, rx, ry;
    if (!p) { (*dn_arc)(f, x1, y1, x2, y2, sa, ea); return; }
    ratrect(&x1, &y1, &x2, &y2);
    if (sa == ea) return;
    if (!strokepre(p)) return;
    elpparm(p, x1, y1, x2, y2, &cx, &cy, &rx, &ry);
    arcsegs(p, cx, cy, rx, ry, arcrad(sa), arcrad(ea), 1);
    sbf(&p->page, "S\n");
}

static void farc_pvf(FILE* f, long x1, long y1, long x2, long y2,
                     long sa, long ea)
{
    prtptr p = txt2prt(f);
    double cx, cy, rx, ry;
    if (!p) { (*dn_farc)(f, x1, y1, x2, y2, sa, ea); return; }
    ratrect(&x1, &y1, &x2, &y2);
    if (sa == ea) return;
    if (!fillpre(p)) return;
    elpparm(p, x1, y1, x2, y2, &cx, &cy, &rx, &ry);
    /* a pie: center to start, around, and closed */
    sbf(&p->page, "%.1f %.1f m %.1f %.1f l\n", cx, cy,
        cx+rx*cos(arcrad(sa)), cy-ry*sin(arcrad(sa)));
    arcsegs(p, cx, cy, rx, ry, arcrad(sa), arcrad(ea), 0);
    sbf(&p->page, "h f\n");
}

static void fchord_pvf(FILE* f, long x1, long y1, long x2, long y2,
                       long sa, long ea)
{
    prtptr p = txt2prt(f);
    double cx, cy, rx, ry;
    if (!p) { (*dn_fchord)(f, x1, y1, x2, y2, sa, ea); return; }
    ratrect(&x1, &y1, &x2, &y2);
    if (sa == ea) return;
    if (!fillpre(p)) return;
    elpparm(p, x1, y1, x2, y2, &cx, &cy, &rx, &ry);
    arcsegs(p, cx, cy, rx, ry, arcrad(sa), arcrad(ea), 1);
    sbf(&p->page, "h f\n");
}

static void ftriangle_pvf(FILE* f, long x1, long y1, long x2, long y2,
                          long x3, long y3)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_ftriangle)(f, x1, y1, x2, y2, x3, y3); return; }
    if (!fillpre(p)) return;
    sbf(&p->page, "%ld %ld m %ld %ld l %ld %ld l h f\n",
        L2PX(p, x1)-1, L2PY(p, y1)-1, L2PX(p, x2)-1, L2PY(p, y2)-1,
        L2PX(p, x3)-1, L2PY(p, y3)-1);
}

static void setpixel_pvf(FILE* f, long x, long y)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_setpixel)(f, x, y); return; }
    if (!fillpre(p)) return;
    sbf(&p->page, "%ld %ld 1 1 re f\n", L2PX(p, x)-1, L2PY(p, y)-1);
}

/* raster modes */

static void fover_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_fover)(f); return; }
    p->fmod = mdnorm;
}

static void bover_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_bover)(f); return; }
    p->bmod = mdnorm;
}

static void finvis_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_finvis)(f); return; }
    p->fmod = mdinvis;
}

static void binvis_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_binvis)(f); return; }
    p->bmod = mdinvis;
}

static void fxor_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_fxor)(f); return; }
    p->fmod = mdxor;
}

static void bxor_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_bxor)(f); return; }
    p->bmod = mdxor;
}

static void fand_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_fand)(f); return; }
    p->fmod = mdand;
}

static void band_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_band)(f); return; }
    p->bmod = mdand;
}

static void for_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_for)(f); return; }
    p->fmod = mdor;
}

static void bor_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_bor)(f); return; }
    p->bmod = mdor;
}

/* fonts and metrics */

static long chrsizx_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_chrsizx)(f));
    return (celwid(p));
}

static long chrsizy_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_chrsizy)(f));
    return (linadv(p));
}

static long fonts_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_fonts)(f));
    return (4); /* the standard set */
}

static void font_pvf(FILE* f, long fc)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_font)(f, fc); return; }
    switch (fc) {

        case AMI_FONT_TERM: p->fam = fmcour; break;
        case AMI_FONT_BOOK: p->fam = fmtimes; break;
        case AMI_FONT_SIGN: p->fam = fmhelv; break;
        /* there is no technical font in the standard set */
        case AMI_FONT_TECH: p->fam = fmhelv; break;
        default: error("Invalid font code");

    }
    p->fontc = fc;
}

static void fontnam_pvf(FILE* f, long fc, char* fns, long fnsl)
{
    const char* s;
    if (!txt2prt(f)) { (*dn_fontnam)(f, fc, fns, fnsl); return; }
    switch (fc) {

        case AMI_FONT_TERM: s = "Courier"; break;
        case AMI_FONT_BOOK: s = "Times"; break;
        case AMI_FONT_SIGN: s = "Helvetica"; break;
        case AMI_FONT_TECH: s = "Helvetica"; break;
        default: error("Invalid font code"); s = "";

    }
    strncpy(fns, s, fnsl);

}

static void fontsiz_pvf(FILE* f, long s)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_fontsiz)(f, s); return; }
    if (s < 1) error("Invalid font size");
    p->fsiz = s;
}

static void setpoints_pvf(FILE* f, float ps)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_setpoints)(f, ps); return; }
    if (ps <= 0) error("Invalid font size");
    p->fsiz = (long)(ps/72.0*PAGEDPI+0.5);
}

static float points_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_points)(f));
    return ((float)p->fsiz*72.0/PAGEDPI);
}

static void chrspcy_pvf(FILE* f, long s)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_chrspcy)(f, s); return; }
    p->chry = s;
}

static void chrspcx_pvf(FILE* f, long s)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_chrspcx)(f, s); return; }
    p->chrx = s;
}

static long dpmx_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_dpmx)(f));
    return ((long)(PAGEDPI/0.0254+0.5));
}

static long dpmy_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_dpmy)(f));
    return ((long)(PAGEDPI/0.0254+0.5));
}

static long strsiz_pvf(FILE* f, const char* s)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_strsiz)(f, s));
    return (strwid(p, s, strlen(s)));
}

static long chrpos_pvf(FILE* f, const char* s, long q)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_chrpos)(f, s, q));
    if (q < 0 || q >= (long)strlen(s)) error("String index out of range");
    return (strwid(p, s, q));
}

#define MINJST 1 /* minimum pixels for a space in justification */

/* find justified space width and remainder for width n, as the display
   library */
static void jstparm(prtptr p, const char* s, long n, long* spc, long* rem)
{
    long l = strlen(s), i, ns = 0, cs = 0, sz = 0;

    for (i = 0; i < l; i++)
        if (s[i] == ' ') { sz += MINJST; ns++; }
        else { sz += chwid(p, (unsigned char)s[i]);
               cs += chwid(p, (unsigned char)s[i]); }
    *spc = MINJST;
    *rem = 0;
    if (n > sz && ns) {

        *spc = (n-cs)/ns;
        *rem = (n-cs)-*spc*ns; /* spread over the first spaces */

    }
}

static void writejust_pvf(FILE* f, const char* s, long n)
{
    prtptr p = txt2prt(f);
    long spc, rem;
    if (!p) { (*dn_writejust)(f, s, n); return; }
    jstparm(p, s, n, &spc, &rem);
    emrun(p, s, strlen(s), spc, rem);
}

static long justpos_pvf(FILE* f, const char* s, long q, long n)
{
    prtptr p = txt2prt(f);
    long spc, rem, cp = 0, i;
    if (!p) return ((*dn_justpos)(f, s, q, n));
    if (q < 0 || q >= (long)strlen(s)) error("String index out of range");
    jstparm(p, s, n, &spc, &rem);
    for (i = 0; i < q; i++)
        if (s[i] == ' ') { cp += spc; if (rem) { cp++; rem--; } }
        else cp += chwid(p, (unsigned char)s[i]);
    return (cp);
}

static void condensed_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_condensed)(f, e); return; }
    p->acond = !!e;
}

static void extended_pvf(FILE* f, long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_extended)(f, e); return; }
    p->aext = !!e;
}

/* the weights and effects the standard fonts do not carry are quietly
   without effect, as they are on displays without them */

static void xlight_pvf(FILE* f, long e)
{
    if (!txt2prt(f)) (*dn_xlight)(f, e);
}

static void light_pvf(FILE* f, long e)
{
    if (!txt2prt(f)) (*dn_light)(f, e);
}

static void xbold_pvf(FILE* f, long e)
{
    if (!txt2prt(f)) (*dn_xbold)(f, e);
}

static void hollow_pvf(FILE* f, long e)
{
    if (!txt2prt(f)) (*dn_hollow)(f, e);
}

static void raised_pvf(FILE* f, long e)
{
    if (!txt2prt(f)) (*dn_raised)(f, e);
}

/* pictures */

static unsigned long rd32(FILE* pf)
{
    unsigned long v;
    v = getc(pf);
    v |= (unsigned long)getc(pf) << 8;
    v |= (unsigned long)getc(pf) << 16;
    v |= (unsigned long)getc(pf) << 24;
    return (v);
}

static unsigned int rd16(FILE* pf)
{
    unsigned int v;
    v = getc(pf);
    v |= (unsigned int)getc(pf) << 8;
    return (v);
}

static void loadpict_pvf(FILE* f, long pn, char* fn)
{
    prtptr p = txt2prt(f);
    FILE*  pf;
    imgptr ip;
    char   fnb[256];
    long   dofs, w, h, i, x;
    int    bot;

    if (!p) { (*dn_loadpict)(f, pn, fn); return; }
    if (pn < 1 || pn > MAXPIC) error("Invalid picture handle");
    /* pictures are .bmp files; supply the extension if it is missing */
    if (strlen(fn) > sizeof(fnb)-5) error("Picture filename too long");
    strcpy(fnb, fn);
    if (!strrchr(fnb, '.')) strcat(fnb, ".bmp");
    pf = fopen(fnb, "r");
    if (!pf) error("Cannot open picture file");
    if (getc(pf) != 'B' || getc(pf) != 'M') error("Picture is not a .bmp");
    rd32(pf); rd32(pf); /* file size, reserved */
    dofs = rd32(pf);
    rd32(pf); /* header size */
    w = (long)(int)rd32(pf);
    h = (long)(int)rd32(pf);
    bot = h > 0; /* bottom up rows */
    if (h < 0) h = -h;
    rd16(pf); /* planes */
    if (rd16(pf) != 24) error("Picture must be 24 bit Truecolor");
    if (rd32(pf) != 0) error("Picture must be uncompressed");
    if (w < 1 || h < 1 || w > 30000 || h > 30000) error("Invalid picture");
    ip = malloc(sizeof(imgrec));
    if (!ip) error("Out of memory");
    ip->w = w;
    ip->h = h;
    ip->rgb = malloc(w*h*3);
    if (!ip->rgb) error("Out of memory");
    fseek(pf, dofs, SEEK_SET);
    for (i = 0; i < h; i++) {

        long r = bot? h-1-i: i; /* destination row, top down */
        unsigned char* d = ip->rgb+r*w*3;
        for (x = 0; x < w; x++) {

            int b = getc(pf), g = getc(pf), rr = getc(pf);
            if (rr < 0) error("Picture file truncated");
            *d++ = rr; *d++ = g; *d++ = b;

        }
        for (x = w*3; x & 3; x++) getc(pf); /* rows pad to 32 bits */

    }
    fclose(pf);
    /* the image joins the document; the slot points at it */
    ip->idx = p->nimgs++;
    ip->objn = 0;
    ip->next = p->imgs;
    p->imgs = ip;
    p->pictbl[pn-1] = ip;
}

static long pictsizx_pvf(FILE* f, long pn)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_pictsizx)(f, pn));
    if (pn < 1 || pn > MAXPIC || !p->pictbl[pn-1])
        error("Invalid picture handle");
    return (p->pictbl[pn-1]->w);
}

static long pictsizy_pvf(FILE* f, long pn)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_pictsizy)(f, pn));
    if (pn < 1 || pn > MAXPIC || !p->pictbl[pn-1])
        error("Invalid picture handle");
    return (p->pictbl[pn-1]->h);
}

static void picture_pvf(FILE* f, long pn, long x1, long y1, long x2, long y2)
{
    prtptr p = txt2prt(f);
    long   x, y, w, h;
    if (!p) { (*dn_picture)(f, pn, x1, y1, x2, y2); return; }
    if (pn < 1 || pn > MAXPIC || !p->pictbl[pn-1])
        error("Invalid picture handle");
    ratrect(&x1, &y1, &x2, &y2);
    pagebeg(p);
    x = L2PX(p, x1)-1; y = L2PY(p, y1)-1;
    w = L2PW(p, x2-x1+1); h = L2PH(p, y2-y1+1);
    /* the image unit square is y up; stand it on its feet in page space */
    sbf(&p->page, "q %ld 0 0 %ld %ld %ld cm /Im%ld Do Q\n",
        w, -h, x, y+h, p->pictbl[pn-1]->idx);
}

static void delpict_pvf(FILE* f, long pn)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_delpict)(f, pn); return; }
    if (pn < 1 || pn > MAXPIC || !p->pictbl[pn-1])
        error("Invalid picture handle");
    /* the slot frees; a placed image stays with the document */
    p->pictbl[pn-1] = NULL;
}

/* views and transforms */

static void scrollg_pvf(FILE* f, long x, long y)
{
    if (!txt2prt(f)) { (*dn_scrollg)(f, x, y); return; }
    error("Cannot scroll a print file");
}

static void path_pvf(FILE* f, long a)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_path)(f, a); return; }
    if (p->autom) error("Cannot set character drawing angle in auto mode");
    p->angle = a;
}

static void viewoffg_pvf(FILE* f, long x, long y)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_viewoffg)(f, x, y); return; }
    p->goffx = x;
    p->goffy = y;
}

static void viewscale_pvf(FILE* f, float x, float y)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_viewscale)(f, x, y); return; }
    p->vsx = x;
    p->vsy = y;
}

static long scalex_pvf(FILE* f, long x)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_scalex)(f, x));
    return (L2PX(p, x));
}

static long scaley_pvf(FILE* f, long y)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_scaley)(f, y));
    return (L2PY(p, y));
}

static void blockcopyg_pvf(FILE* f, long s, long d, long sx1, long sy1,
                           long sx2, long sy2, long dx1, long dy1,
                           long dx2, long dy2)
{
    if (!txt2prt(f)) {

        (*dn_blockcopyg)(f, s, d, sx1, sy1, sx2, sy2, dx1, dy1, dx2, dy2);
        return;

    }
    error("No screens on a print file");
}

static void openwin_pvf(FILE** infile, FILE** outfile, FILE* parent, long wid)
{
    if (parent && txt2prt(parent)) error("Cannot open a window on a print file");
    (*dn_openwin)(infile, outfile, parent, wid);
}

/*******************************************************************************

Initialize and deinitialize

The module hooks in above the display library, and below nothing: it takes
each vector, keeps the chain, and passes through everything that is not a
print file. Print files still open at exit are completed, so a program that
neglects the close still prints.

*******************************************************************************/

static void ami_init_pdfgraph(void) __attribute__((constructor (103)));
static void ami_init_pdfgraph(void)

{

    ovr_write(iwrite, &dn_write);
    ovr_close(iclose, &dn_close);

    _pa_cursor_ovr(cursor_pvf, &dn_cursor);
    _pa_maxx_ovr(maxx_pvf, &dn_maxx);
    _pa_maxy_ovr(maxy_pvf, &dn_maxy);
    _pa_home_ovr(home_pvf, &dn_home);
    _pa_del_ovr(del_pvf, &dn_del);
    _pa_up_ovr(up_pvf, &dn_up);
    _pa_down_ovr(down_pvf, &dn_down);
    _pa_left_ovr(left_pvf, &dn_left);
    _pa_right_ovr(right_pvf, &dn_right);
    _pa_blink_ovr(blink_pvf, &dn_blink);
    _pa_reverse_ovr(reverse_pvf, &dn_reverse);
    _pa_underline_ovr(underline_pvf, &dn_underline);
    _pa_superscript_ovr(superscript_pvf, &dn_superscript);
    _pa_subscript_ovr(subscript_pvf, &dn_subscript);
    _pa_italic_ovr(italic_pvf, &dn_italic);
    _pa_bold_ovr(bold_pvf, &dn_bold);
    _pa_strikeout_ovr(strikeout_pvf, &dn_strikeout);
    _pa_standout_ovr(standout_pvf, &dn_standout);
    _pa_fcolor_ovr(fcolor_pvf, &dn_fcolor);
    _pa_bcolor_ovr(bcolor_pvf, &dn_bcolor);
    _pa_auto_ovr(auto_pvf, &dn_auto);
    _pa_curvis_ovr(curvis_pvf, &dn_curvis);
    _pa_scroll_ovr(scroll_pvf, &dn_scroll);
    _pa_curx_ovr(curx_pvf, &dn_curx);
    _pa_cury_ovr(cury_pvf, &dn_cury);
    _pa_curbnd_ovr(curbnd_pvf, &dn_curbnd);
    _pa_select_ovr(select_pvf, &dn_select);
    _pa_event_ovr(event_pvf, &dn_event);
    _pa_timer_ovr(timer_pvf, &dn_timer);
    _pa_killtimer_ovr(killtimer_pvf, &dn_killtimer);
    _pa_mouse_ovr(mouse_pvf, &dn_mouse);
    _pa_mousebutton_ovr(mousebutton_pvf, &dn_mousebutton);
    _pa_joystick_ovr(joystick_pvf, &dn_joystick);
    _pa_joybutton_ovr(joybutton_pvf, &dn_joybutton);
    _pa_joyaxis_ovr(joyaxis_pvf, &dn_joyaxis);
    _pa_settab_ovr(settab_pvf, &dn_settab);
    _pa_restab_ovr(restab_pvf, &dn_restab);
    _pa_clrtab_ovr(clrtab_pvf, &dn_clrtab);
    _pa_funkey_ovr(funkey_pvf, &dn_funkey);
    _pa_frametimer_ovr(frametimer_pvf, &dn_frametimer);
    _pa_wrtstr_ovr(wrtstr_pvf, &dn_wrtstr);
    _pa_wrtstrn_ovr(wrtstrn_pvf, &dn_wrtstrn);
    _pa_sizbuf_ovr(sizbuf_pvf, &dn_sizbuf);
    _pa_title_ovr(title_pvf, &dn_title);
    _pa_sendevent_ovr(sendevent_pvf, &dn_sendevent);

    _pa_maxxg_ovr(maxxg_pvf, &dn_maxxg);
    _pa_maxyg_ovr(maxyg_pvf, &dn_maxyg);
    _pa_curxg_ovr(curxg_pvf, &dn_curxg);
    _pa_curyg_ovr(curyg_pvf, &dn_curyg);
    _pa_line_ovr(line_pvf, &dn_line);
    _pa_linewidth_ovr(linewidth_pvf, &dn_linewidth);
    _pa_linestyle_ovr(linestyle_pvf, &dn_linestyle);
    _pa_rect_ovr(rect_pvf, &dn_rect);
    _pa_frect_ovr(frect_pvf, &dn_frect);
    _pa_rrect_ovr(rrect_pvf, &dn_rrect);
    _pa_frrect_ovr(frrect_pvf, &dn_frrect);
    _pa_ellipse_ovr(ellipse_pvf, &dn_ellipse);
    _pa_fellipse_ovr(fellipse_pvf, &dn_fellipse);
    _pa_arc_ovr(arc_pvf, &dn_arc);
    _pa_farc_ovr(farc_pvf, &dn_farc);
    _pa_fchord_ovr(fchord_pvf, &dn_fchord);
    _pa_ftriangle_ovr(ftriangle_pvf, &dn_ftriangle);
    _pa_cursorg_ovr(cursorg_pvf, &dn_cursorg);
    _pa_baseline_ovr(baseline_pvf, &dn_baseline);
    _pa_setpixel_ovr(setpixel_pvf, &dn_setpixel);
    _pa_fover_ovr(fover_pvf, &dn_fover);
    _pa_bover_ovr(bover_pvf, &dn_bover);
    _pa_finvis_ovr(finvis_pvf, &dn_finvis);
    _pa_binvis_ovr(binvis_pvf, &dn_binvis);
    _pa_fxor_ovr(fxor_pvf, &dn_fxor);
    _pa_bxor_ovr(bxor_pvf, &dn_bxor);
    _pa_fand_ovr(fand_pvf, &dn_fand);
    _pa_band_ovr(band_pvf, &dn_band);
    _pa_for_ovr(for_pvf, &dn_for);
    _pa_bor_ovr(bor_pvf, &dn_bor);
    _pa_chrsizx_ovr(chrsizx_pvf, &dn_chrsizx);
    _pa_chrsizy_ovr(chrsizy_pvf, &dn_chrsizy);
    _pa_fonts_ovr(fonts_pvf, &dn_fonts);
    _pa_font_ovr(font_pvf, &dn_font);
    _pa_fontnam_ovr(fontnam_pvf, &dn_fontnam);
    _pa_fontsiz_ovr(fontsiz_pvf, &dn_fontsiz);
    _pa_setpoints_ovr(setpoints_pvf, &dn_setpoints);
    _pa_points_ovr(points_pvf, &dn_points);
    _pa_chrspcy_ovr(chrspcy_pvf, &dn_chrspcy);
    _pa_chrspcx_ovr(chrspcx_pvf, &dn_chrspcx);
    _pa_dpmx_ovr(dpmx_pvf, &dn_dpmx);
    _pa_dpmy_ovr(dpmy_pvf, &dn_dpmy);
    _pa_strsiz_ovr(strsiz_pvf, &dn_strsiz);
    _pa_chrpos_ovr(chrpos_pvf, &dn_chrpos);
    _pa_writejust_ovr(writejust_pvf, &dn_writejust);
    _pa_justpos_ovr(justpos_pvf, &dn_justpos);
    _pa_condensed_ovr(condensed_pvf, &dn_condensed);
    _pa_extended_ovr(extended_pvf, &dn_extended);
    _pa_xlight_ovr(xlight_pvf, &dn_xlight);
    _pa_light_ovr(light_pvf, &dn_light);
    _pa_xbold_ovr(xbold_pvf, &dn_xbold);
    _pa_hollow_ovr(hollow_pvf, &dn_hollow);
    _pa_raised_ovr(raised_pvf, &dn_raised);
    _pa_settabg_ovr(settabg_pvf, &dn_settabg);
    _pa_restabg_ovr(restabg_pvf, &dn_restabg);
    _pa_fcolorg_ovr(fcolorg_pvf, &dn_fcolorg);
    _pa_fcolorc_ovr(fcolorc_pvf, &dn_fcolorc);
    _pa_bcolorg_ovr(bcolorg_pvf, &dn_bcolorg);
    _pa_bcolorc_ovr(bcolorc_pvf, &dn_bcolorc);
    _pa_loadpict_ovr(loadpict_pvf, &dn_loadpict);
    _pa_pictsizx_ovr(pictsizx_pvf, &dn_pictsizx);
    _pa_pictsizy_ovr(pictsizy_pvf, &dn_pictsizy);
    _pa_picture_ovr(picture_pvf, &dn_picture);
    _pa_delpict_ovr(delpict_pvf, &dn_delpict);
    _pa_scrollg_ovr(scrollg_pvf, &dn_scrollg);
    _pa_path_ovr(path_pvf, &dn_path);
    _pa_viewoffg_ovr(viewoffg_pvf, &dn_viewoffg);
    _pa_viewscale_ovr(viewscale_pvf, &dn_viewscale);
    _pa_scalex_ovr(scalex_pvf, &dn_scalex);
    _pa_scaley_ovr(scaley_pvf, &dn_scaley);
    _pa_blockcopyg_ovr(blockcopyg_pvf, &dn_blockcopyg);
    _pa_openwin_ovr(openwin_pvf, &dn_openwin);

}

static void ami_deinit_pdfgraph(void) __attribute__((destructor (103)));
static void ami_deinit_pdfgraph(void)

{

    int      fd;
    pwrite_t cpwrite;
    pclose_t cpclose;

    /* complete any print file the program left open */
    for (fd = 0; fd < MAXPFIL; fd++)
        if (prtfil[fd]) { prtfree(fd); (*dn_close)(fd); }

    /* pop off the display vectors; the modules below deinstall after us
       and check that they come off the top of the chain */
    { ami_cursor_t t;      _pa_cursor_ovr(dn_cursor, &t); }
    { ami_maxx_t t;        _pa_maxx_ovr(dn_maxx, &t); }
    { ami_maxy_t t;        _pa_maxy_ovr(dn_maxy, &t); }
    { ami_home_t t;        _pa_home_ovr(dn_home, &t); }
    { ami_del_t t;         _pa_del_ovr(dn_del, &t); }
    { ami_up_t t;          _pa_up_ovr(dn_up, &t); }
    { ami_down_t t;        _pa_down_ovr(dn_down, &t); }
    { ami_left_t t;        _pa_left_ovr(dn_left, &t); }
    { ami_right_t t;       _pa_right_ovr(dn_right, &t); }
    { ami_blink_t t;       _pa_blink_ovr(dn_blink, &t); }
    { ami_reverse_t t;     _pa_reverse_ovr(dn_reverse, &t); }
    { ami_underline_t t;   _pa_underline_ovr(dn_underline, &t); }
    { ami_superscript_t t; _pa_superscript_ovr(dn_superscript, &t); }
    { ami_subscript_t t;   _pa_subscript_ovr(dn_subscript, &t); }
    { ami_italic_t t;      _pa_italic_ovr(dn_italic, &t); }
    { ami_bold_t t;        _pa_bold_ovr(dn_bold, &t); }
    { ami_strikeout_t t;   _pa_strikeout_ovr(dn_strikeout, &t); }
    { ami_standout_t t;    _pa_standout_ovr(dn_standout, &t); }
    { ami_fcolor_t t;      _pa_fcolor_ovr(dn_fcolor, &t); }
    { ami_bcolor_t t;      _pa_bcolor_ovr(dn_bcolor, &t); }
    { ami_auto_t t;        _pa_auto_ovr(dn_auto, &t); }
    { ami_curvis_t t;      _pa_curvis_ovr(dn_curvis, &t); }
    { ami_scroll_t t;      _pa_scroll_ovr(dn_scroll, &t); }
    { ami_curx_t t;        _pa_curx_ovr(dn_curx, &t); }
    { ami_cury_t t;        _pa_cury_ovr(dn_cury, &t); }
    { ami_curbnd_t t;      _pa_curbnd_ovr(dn_curbnd, &t); }
    { ami_select_t t;      _pa_select_ovr(dn_select, &t); }
    { ami_event_t t;       _pa_event_ovr(dn_event, &t); }
    { ami_timer_t t;       _pa_timer_ovr(dn_timer, &t); }
    { ami_killtimer_t t;   _pa_killtimer_ovr(dn_killtimer, &t); }
    { ami_mouse_t t;       _pa_mouse_ovr(dn_mouse, &t); }
    { ami_mousebutton_t t; _pa_mousebutton_ovr(dn_mousebutton, &t); }
    { ami_joystick_t t;    _pa_joystick_ovr(dn_joystick, &t); }
    { ami_joybutton_t t;   _pa_joybutton_ovr(dn_joybutton, &t); }
    { ami_joyaxis_t t;     _pa_joyaxis_ovr(dn_joyaxis, &t); }
    { ami_settab_t t;      _pa_settab_ovr(dn_settab, &t); }
    { ami_restab_t t;      _pa_restab_ovr(dn_restab, &t); }
    { ami_clrtab_t t;      _pa_clrtab_ovr(dn_clrtab, &t); }
    { ami_funkey_t t;      _pa_funkey_ovr(dn_funkey, &t); }
    { ami_frametimer_t t;  _pa_frametimer_ovr(dn_frametimer, &t); }
    { ami_wrtstr_t t;      _pa_wrtstr_ovr(dn_wrtstr, &t); }
    { ami_wrtstrn_t t;     _pa_wrtstrn_ovr(dn_wrtstrn, &t); }
    { ami_sizbuf_t t;      _pa_sizbuf_ovr(dn_sizbuf, &t); }
    { ami_title_t t;       _pa_title_ovr(dn_title, &t); }
    { ami_sendevent_t t;   _pa_sendevent_ovr(dn_sendevent, &t); }
    { ami_maxxg_t t;       _pa_maxxg_ovr(dn_maxxg, &t); }
    { ami_maxyg_t t;       _pa_maxyg_ovr(dn_maxyg, &t); }
    { ami_curxg_t t;       _pa_curxg_ovr(dn_curxg, &t); }
    { ami_curyg_t t;       _pa_curyg_ovr(dn_curyg, &t); }
    { ami_line_t t;        _pa_line_ovr(dn_line, &t); }
    { ami_linewidth_t t;   _pa_linewidth_ovr(dn_linewidth, &t); }
    { ami_linestyle_t t;   _pa_linestyle_ovr(dn_linestyle, &t); }
    { ami_rect_t t;        _pa_rect_ovr(dn_rect, &t); }
    { ami_frect_t t;       _pa_frect_ovr(dn_frect, &t); }
    { ami_rrect_t t;       _pa_rrect_ovr(dn_rrect, &t); }
    { ami_frrect_t t;      _pa_frrect_ovr(dn_frrect, &t); }
    { ami_ellipse_t t;     _pa_ellipse_ovr(dn_ellipse, &t); }
    { ami_fellipse_t t;    _pa_fellipse_ovr(dn_fellipse, &t); }
    { ami_arc_t t;         _pa_arc_ovr(dn_arc, &t); }
    { ami_farc_t t;        _pa_farc_ovr(dn_farc, &t); }
    { ami_fchord_t t;      _pa_fchord_ovr(dn_fchord, &t); }
    { ami_ftriangle_t t;   _pa_ftriangle_ovr(dn_ftriangle, &t); }
    { ami_cursorg_t t;     _pa_cursorg_ovr(dn_cursorg, &t); }
    { ami_baseline_t t;    _pa_baseline_ovr(dn_baseline, &t); }
    { ami_setpixel_t t;    _pa_setpixel_ovr(dn_setpixel, &t); }
    { ami_fover_t t;       _pa_fover_ovr(dn_fover, &t); }
    { ami_bover_t t;       _pa_bover_ovr(dn_bover, &t); }
    { ami_finvis_t t;      _pa_finvis_ovr(dn_finvis, &t); }
    { ami_binvis_t t;      _pa_binvis_ovr(dn_binvis, &t); }
    { ami_fxor_t t;        _pa_fxor_ovr(dn_fxor, &t); }
    { ami_bxor_t t;        _pa_bxor_ovr(dn_bxor, &t); }
    { ami_fand_t t;        _pa_fand_ovr(dn_fand, &t); }
    { ami_band_t t;        _pa_band_ovr(dn_band, &t); }
    { ami_for_t t;         _pa_for_ovr(dn_for, &t); }
    { ami_bor_t t;         _pa_bor_ovr(dn_bor, &t); }
    { ami_chrsizx_t t;     _pa_chrsizx_ovr(dn_chrsizx, &t); }
    { ami_chrsizy_t t;     _pa_chrsizy_ovr(dn_chrsizy, &t); }
    { ami_fonts_t t;       _pa_fonts_ovr(dn_fonts, &t); }
    { ami_font_t t;        _pa_font_ovr(dn_font, &t); }
    { ami_fontnam_t t;     _pa_fontnam_ovr(dn_fontnam, &t); }
    { ami_fontsiz_t t;     _pa_fontsiz_ovr(dn_fontsiz, &t); }
    { ami_setpoints_t t;   _pa_setpoints_ovr(dn_setpoints, &t); }
    { ami_points_t t;      _pa_points_ovr(dn_points, &t); }
    { ami_chrspcy_t t;     _pa_chrspcy_ovr(dn_chrspcy, &t); }
    { ami_chrspcx_t t;     _pa_chrspcx_ovr(dn_chrspcx, &t); }
    { ami_dpmx_t t;        _pa_dpmx_ovr(dn_dpmx, &t); }
    { ami_dpmy_t t;        _pa_dpmy_ovr(dn_dpmy, &t); }
    { ami_strsiz_t t;      _pa_strsiz_ovr(dn_strsiz, &t); }
    { ami_chrpos_t t;      _pa_chrpos_ovr(dn_chrpos, &t); }
    { ami_writejust_t t;   _pa_writejust_ovr(dn_writejust, &t); }
    { ami_justpos_t t;     _pa_justpos_ovr(dn_justpos, &t); }
    { ami_condensed_t t;   _pa_condensed_ovr(dn_condensed, &t); }
    { ami_extended_t t;    _pa_extended_ovr(dn_extended, &t); }
    { ami_xlight_t t;      _pa_xlight_ovr(dn_xlight, &t); }
    { ami_light_t t;       _pa_light_ovr(dn_light, &t); }
    { ami_xbold_t t;       _pa_xbold_ovr(dn_xbold, &t); }
    { ami_hollow_t t;      _pa_hollow_ovr(dn_hollow, &t); }
    { ami_raised_t t;      _pa_raised_ovr(dn_raised, &t); }
    { ami_settabg_t t;     _pa_settabg_ovr(dn_settabg, &t); }
    { ami_restabg_t t;     _pa_restabg_ovr(dn_restabg, &t); }
    { ami_fcolorg_t t;     _pa_fcolorg_ovr(dn_fcolorg, &t); }
    { ami_fcolorc_t t;     _pa_fcolorc_ovr(dn_fcolorc, &t); }
    { ami_bcolorg_t t;     _pa_bcolorg_ovr(dn_bcolorg, &t); }
    { ami_bcolorc_t t;     _pa_bcolorc_ovr(dn_bcolorc, &t); }
    { ami_loadpict_t t;    _pa_loadpict_ovr(dn_loadpict, &t); }
    { ami_pictsizx_t t;    _pa_pictsizx_ovr(dn_pictsizx, &t); }
    { ami_pictsizy_t t;    _pa_pictsizy_ovr(dn_pictsizy, &t); }
    { ami_picture_t t;     _pa_picture_ovr(dn_picture, &t); }
    { ami_delpict_t t;     _pa_delpict_ovr(dn_delpict, &t); }
    { ami_scrollg_t t;     _pa_scrollg_ovr(dn_scrollg, &t); }
    { ami_path_t t;        _pa_path_ovr(dn_path, &t); }
    { ami_viewoffg_t t;    _pa_viewoffg_ovr(dn_viewoffg, &t); }
    { ami_viewscale_t t;   _pa_viewscale_ovr(dn_viewscale, &t); }
    { ami_scalex_t t;      _pa_scalex_ovr(dn_scalex, &t); }
    { ami_scaley_t t;      _pa_scaley_ovr(dn_scaley, &t); }
    { ami_blockcopyg_t t;  _pa_blockcopyg_ovr(dn_blockcopyg, &t); }
    { ami_openwin_t t;     _pa_openwin_ovr(dn_openwin, &t); }

    /* swap the system vectors back; if we don't come off the top of the
       chain the stacking is corrupt */
    ovr_write(dn_write, &cpwrite);
    ovr_close(dn_close, &cpclose);
    if (cpwrite != iwrite || cpclose != iclose)
        error("System consistency check");

}
