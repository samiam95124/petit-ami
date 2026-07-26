/*******************************************************************************
*                                                                              *
*                    macOS Cocoa/Quartz graphics implementation                *
*                                                                              *
* Implements the Ami graphics API using CoreGraphics for drawing and           *
* CoreText for font rendering.  Window/event management is delegated to the   *
* Objective-C shim in graphics_cocoa.m via pa_cocoa.h.                        *
*                                                                              *
* No override vector system — widgets are handled natively by Cocoa controls. *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>

#include <localdefs.h>
#include <config.h>
#include <graphics.h>
#include "pa_cocoa.h"

/* libc/stdio.c vector override types — same ABI as the internal vectors */
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef ssize_t (*pread_t)(int, void*, size_t);
typedef int     (*pclose_t)(int);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_read(pread_t  nfp, pread_t*  ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);

/*******************************************************************************
*                                                                              *
*                              Constants                                       *
*                                                                              *
*******************************************************************************/

#define MAXFIL      100     /* maximum open file/window slots */
#define MAXPIC      50      /* maximum loadable pictures */
#define MAXTIM      10      /* maximum timers per window */
#define FRMTIM      (MAXTIM-1) /* reserved slot for frame timer (0-based) */
#define MAXCON      10      /* maximum screen contexts per window */
#define MAXXD       80      /* default terminal width in chars */
#define MAXYD       25      /* default terminal height in chars */
#define DEF_FONT_H  16      /* default font height in points */

/* PA angles: LONG_MAX = 360 degrees */
#define ANG2RAD(a)  ((a) * (2.0 * M_PI) / (double)LONG_MAX)

/*******************************************************************************
*                                                                              *
*                              Types                                           *
*                                                                              *
*******************************************************************************/

/* Drawing mode */
typedef enum { mdnorm, mdinvis, mdxor, mdand, mdor } drawmode;

/* Color as RGBA floats */
typedef struct { CGFloat r, g, b, a; } pa_rgba;

/* Font entry */
typedef struct fontrec {
    struct fontrec* next;
    char*           name;       /* display name */
    CTFontRef       ctfont;     /* CoreText font at current size */
    int             fixed;      /* is fixed-pitch */
    CGFloat         size;       /* current size in points */
} fontrec, *fontptr;

/* Screen context (PA supports multiple virtual screens per window) */
typedef struct scncon {
    long        curx,  cury;   /* text cursor (1-based char) */
    long        curxg, curyg;  /* graphics cursor (1-based pixel) */
    int         curv;          /* cursor visible */
    pa_rgba     fc, bc;        /* foreground / background color */
    drawmode    fmod, bmod;    /* draw modes */
    CGFloat     lwidth;        /* line width */
    fontptr     font;          /* current font */
    int         bold, italic, underline, strikeout;
    int         superscript, subscript;
    int         condensed, extended;
    int         light, xlight, xbold;
    int         hollow, raised;
    int         reverse;
    int         autof;         /* auto scroll/wrap mode */
    long        textpath;      /* text angle (PA units: LONG_MAX=360°) */
    long        offx, offy;    /* viewport offset (pixels) */
    float       scalex, scaley;/* viewport scale */
} scncon, *scnptr;

/* Per-window record */
typedef struct winrec {
    pa_winhan   han;            /* Cocoa window handle */
    FILE*       infile;         /* associated input  stream */
    FILE*       outfile;        /* associated output stream */
    int         wid;            /* window id */
    int         parwid;         /* parent window id */

    /* dimensions */
    long        maxxg, maxyg;  /* graphical size in pixels */
    long        maxx,  maxy;   /* text size in chars */
    long        charspace;     /* pixels per char column */
    long        linespace;     /* pixels per char row */
    long        dpmx, dpmy;    /* dots per meter x/y */

    /* screens */
    scncon      screens[MAXCON];
    int         curdsp;        /* current display screen (1-based) */
    int         curupd;        /* current update screen  (1-based) */

    /* state */
    int         visible;
    int         frame, sizable, sysbar;
    int         bufmod;        /* double-buffer on */
    int         focus;
    int         fautohold;     /* auto hold on exit */
    int         frmrun;        /* frame timer running */

    /* font */
    fontptr     cfont;         /* current font */
    CGFloat     fontsz;        /* font size in pixels (CTFont logical size) */
    float       gfpoint;       /* current font size in typographic points */

    /* event handler */
    ami_pevthan evthan;
} winrec, *winptr;

/*******************************************************************************
*                                                                              *
*                              Globals                                         *
*                                                                              *
*******************************************************************************/

static winrec   wintbl[MAXFIL];         /* window table indexed by FILE slot */
static int      opnfil[MAXFIL];         /* 1 = slot in use */
static FILE*    filwin[MAXFIL];         /* file → window mapping */
static fontptr  fntlst;                 /* global font list */
static int      fntcnt;                 /* number of fonts */
static int      inited;                 /* library initialised */
static int      fend;                   /* program ending flag */
static int      fautohold;             /* global auto hold */
static pwrite_t ofpwrite;              /* saved write vector */
static pread_t  ofpread;               /* saved read vector */
static pclose_t ofpclose;              /* saved close vector */
static int      maxxd;                 /* default window width in pixels */
static int      maxyd;                 /* default window height in pixels */

/* config settable runtime options */
static int      cfgmaxxd;             /* configured terminal width in chars */
static int      cfgmaxyd;             /* configured terminal height in chars */
static int      dialogerr;            /* send runtime errors to dialog */
static int      mouseenb = 1;         /* enable mouse */
static int      joyenb;               /* enable joysticks */
static int      dmpevt;               /* enable dump Petit-Ami events */
static int      dmpmsg;               /* enable dump messages (diagnostic) */
static int      prtftm;               /* print font metrics (diagnostic) */
static int      conpnt;               /* size of console font in points */

/* forward declarations */
static void    clear_window(winptr win);
static void    plcchr(winptr win, char c);
static ssize_t iwrite(int fd, const void* buff, size_t count);
static ssize_t iread(int fd, void* buff, size_t count);
static int     iclose(int fd);
static void    update_metrics(winptr win);


/* standard color table: ami_color → RGBA */
static const pa_rgba colortbl[] = {
    {0,0,0,1},       /* ami_black    */
    {1,1,1,1},       /* ami_white    */
    {1,0,0,1},       /* ami_red      */
    {0,1,0,1},       /* ami_green    */
    {0,0,1,1},       /* ami_blue     */
    {0,1,1,1},       /* ami_cyan     */
    {1,1,0,1},       /* ami_yellow   */
    {1,0,1,1},       /* ami_magenta  */
    {1,1,1,1}        /* ami_backcolor (white) */
};

/*******************************************************************************
*                                                                              *
*                              Utilities                                       *
*                                                                              *
*******************************************************************************/

/* Get window record from FILE* */
static winptr f2win(FILE* f)
{
    int fd = fileno(f);
    if (fd < 0 || fd >= MAXFIL || !opnfil[fd]) return NULL;
    return &wintbl[fd];
}

/* Get current update screen */
static scnptr curscn(winptr win)
{
    return &win->screens[win->curupd - 1];
}

/* Apply foreground color to CGContext */
static void set_fg(CGContextRef ctx, pa_rgba c)
{
    CGContextSetRGBStrokeColor(ctx, c.r, c.g, c.b, c.a);
    CGContextSetRGBFillColor(ctx, c.r, c.g, c.b, c.a);
}

static void set_fg_only(CGContextRef ctx, pa_rgba c)
{
    CGContextSetRGBStrokeColor(ctx, c.r, c.g, c.b, c.a);
}

static void set_fill_only(CGContextRef ctx, pa_rgba c)
{
    CGContextSetRGBFillColor(ctx, c.r, c.g, c.b, c.a);
}

/* Convert PA 1-based coordinate to CG 0-based */
#define PX(x) ((x) - 1)
#define PY(y) ((y) - 1)

/* Convert ami_color to pa_rgba */
static pa_rgba ami2rgba(ami_color c)
{
    if (c < 0 || c > ami_backcolor) c = ami_white;
    return colortbl[c];
}

/* Allocate a new window slot, returns fd or -1 */
static int alloc_win(void)
{
    for (int i = 3; i < MAXFIL; i++) /* skip stdin/stdout/stderr */
        if (!opnfil[i]) return i;
    return -1;
}

/*******************************************************************************
*                                                                              *
*                              Font management                                 *
*                                                                              *
*******************************************************************************/

static CTFontRef make_ctfont(const char* name, CGFloat size, int bold, int italic)
{
    CTFontSymbolicTraits traits = 0;
    if (bold)   traits |= kCTFontTraitBold;
    if (italic) traits |= kCTFontTraitItalic;

    CFStringRef cfname = CFStringCreateWithCString(NULL, name,
                                                   kCFStringEncodingUTF8);
    CTFontRef base = CTFontCreateWithName(cfname, size, NULL);
    CFRelease(cfname);

    if (traits) {
        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(base, size,
                                                              NULL, traits, traits);
        if (styled) { CFRelease(base); return styled; }
    }
    return base;
}

/* Rebuild the current font's CTFont based on screen context effects.
 * Called when bold, italic, condensed, extended, etc. change. */
static void rebuild_font(winptr win)
{
    scnptr sc = curscn(win);
    fontptr fp = sc->font ? sc->font : fntlst;
    if (!fp || !fp->name) return;

    int b = sc->bold || sc->xbold;
    int it = sc->italic;

    if (fp->ctfont) CFRelease(fp->ctfont);
    fp->ctfont = make_ctfont(fp->name, win->fontsz, b, it);

    /* apply condensed/extended via a matrix transform */
    if (fp->ctfont && (sc->condensed || sc->extended)) {
        CGFloat xscale = sc->condensed ? 0.8 : 1.25;
        CGAffineTransform matrix = CGAffineTransformMakeScale(xscale, 1.0);
        CTFontRef transformed = CTFontCreateCopyWithAttributes(fp->ctfont,
                                    win->fontsz, &matrix, NULL);
        if (transformed) {
            CFRelease(fp->ctfont);
            fp->ctfont = transformed;
        }
    }

    update_metrics(win);
}

/* Build the standard 4 fonts (TERM, BOOK, SIGN, TECH) */
static void init_fonts(void)
{
    /* font names to try, in preference order, for each slot */
    static const char* term_names[] = {
        "Menlo", "Monaco", "Courier New", "Courier", NULL
    };
    static const char* book_names[] = {
        "Georgia", "Palatino", "Times New Roman", "Times", NULL
    };
    static const char* sign_names[] = {
        "Helvetica Neue", "Helvetica", "Arial", "Verdana", NULL
    };

    const char** lists[4] = { term_names, book_names, sign_names, sign_names };
    fntlst = NULL;
    fntcnt = 0;

    for (int slot = 0; slot < 4; slot++) {
        fontptr fp = calloc(1, sizeof(fontrec));
        /* for TERM slot, try system monospace font (SF Mono) first */
        if (slot == 0) {
            CTFontRef f = pa_cocoa_system_mono_font((CGFloat)DEF_FONT_H);
            if (f) {
                CFStringRef fname = CTFontCopyFullName(f);
                char nbuf[256];
                CFStringGetCString(fname, nbuf, sizeof(nbuf),
                                   kCFStringEncodingUTF8);
                CFRelease(fname);
                fp->ctfont = f;
                fp->name   = strdup(nbuf);
                fp->size   = DEF_FONT_H;
                fp->fixed  = 1;
            }
        }
        /* fall through to name list if system mono not available */
        if (!fp->ctfont) {
            for (const char** np = lists[slot]; *np; np++) {
                CTFontRef f = make_ctfont(*np, DEF_FONT_H, 0, 0);
                if (f) {
                    fp->ctfont = f;
                    fp->name   = strdup(*np);
                    fp->size   = DEF_FONT_H;
                    fp->fixed  = (slot == 0);
                    break;
                }
            }
        }
        if (!fp->ctfont) {
            /* last resort: system font */
            fp->ctfont = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem,
                                                       DEF_FONT_H, NULL);
            fp->name   = strdup("System");
            fp->size   = DEF_FONT_H;
        }
        fp->next = fntlst;
        fntlst   = fp;
        fntcnt++;
    }

    /* reverse list so TERM=1, BOOK=2, SIGN=3, TECH=4 */
    fontptr prev = NULL, cur = fntlst, next;
    while (cur) { next = cur->next; cur->next = prev; prev = cur; cur = next; }
    fntlst = prev;

    /* enumerate all installed system fonts after the 4 standard ones */
    CTFontCollectionRef collection = CTFontCollectionCreateFromAvailableFonts(NULL);
    if (collection) {
        CFArrayRef descs = CTFontCollectionCreateMatchingFontDescriptors(collection);
        if (descs) {
            CFIndex n = CFArrayGetCount(descs);
            /* find the tail of the font list */
            fontptr tail = fntlst;
            while (tail && tail->next) tail = tail->next;

            for (CFIndex i = 0; i < n; i++) {
                CTFontDescriptorRef desc = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descs, i);
                CFStringRef cfname = CTFontDescriptorCopyAttribute(desc, kCTFontDisplayNameAttribute);
                if (!cfname) continue;

                char namebuf[256];
                if (!CFStringGetCString(cfname, namebuf, sizeof(namebuf), kCFStringEncodingUTF8)) {
                    CFRelease(cfname);
                    continue;
                }
                CFRelease(cfname);

                /* skip duplicates of the 4 standard fonts */
                int dup = 0;
                fontptr chk = fntlst;
                while (chk) {
                    if (chk->name && strcmp(chk->name, namebuf) == 0) { dup = 1; break; }
                    chk = chk->next;
                }
                if (dup) continue;

                CTFontRef f = make_ctfont(namebuf, DEF_FONT_H, 0, 0);
                if (!f) continue;

                fontptr fp = calloc(1, sizeof(fontrec));
                fp->ctfont = f;
                fp->name   = strdup(namebuf);
                fp->size   = DEF_FONT_H;
                fp->fixed  = (CTFontGetSymbolicTraits(f) & kCTFontTraitMonoSpace) != 0;
                fp->next   = NULL;

                if (tail) { tail->next = fp; tail = fp; }
                else      { fntlst = fp; tail = fp; }
                fntcnt++;
            }
            CFRelease(descs);
        }
        CFRelease(collection);
    }
}

/* Measure a string with current font; returns width in pixels */
static CGFloat measure_string(fontptr fp, const char* s, int len)
{
    if (!fp || !fp->ctfont || !s || len <= 0) return 0;
    CFStringRef  cfs = CFStringCreateWithBytes(NULL, (const UInt8*)s, len,
                                               kCFStringEncodingUTF8, false);
    CGFloat w = 0;
    if (cfs) {
        CFStringRef keys[1]   = { kCTFontAttributeName };
        CFTypeRef   values[1] = { fp->ctfont };
        CFDictionaryRef attrs = CFDictionaryCreate(NULL,
                                   (const void**)keys, (const void**)values,
                                   1, &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, cfs, attrs);
        CTLineRef line = CTLineCreateWithAttributedString(as);
        w = (CGFloat)CTLineGetTypographicBounds(line, NULL, NULL, NULL);
        CFRelease(line);
        CFRelease(as);
        CFRelease(attrs);
        CFRelease(cfs);
    }
    return w;
}

/* Draw a string at (x, y) — top-left based, PA 1-based coords.
 * Handles text effects: bold, italic (via font), underline, strikeout,
 * superscript, subscript, reverse, hollow, raised, light. */
static void draw_string(CGContextRef ctx, fontptr fp, scnptr sc,
                        long x, long y, const char* s, int len)
{
    if (!ctx || !fp || !fp->ctfont || !s || len <= 0) return;

    /* for superscript/subscript, use a smaller font */
    CTFontRef drawFont = fp->ctfont;
    int releaseDraw = 0;
    CGFloat yoff = 0;
    if (sc->superscript || sc->subscript) {
        CGFloat smallSize = fp->size * 0.6;
        drawFont = CTFontCreateCopyWithAttributes(fp->ctfont, smallSize, NULL, NULL);
        releaseDraw = 1;
        if (sc->superscript) yoff = 0; /* top of cell */
        else yoff = fp->size * 0.4; /* lower in cell */
    }

    CFStringRef cfs = CFStringCreateWithBytes(NULL, (const UInt8*)s, len,
                                              kCFStringEncodingUTF8, false);
    if (!cfs) { if (releaseDraw) CFRelease(drawFont); return; }

    /* determine colors: swap fg/bg for reverse mode */
    pa_rgba fgc = sc->fc, bgc = sc->bc;
    if (sc->reverse) { pa_rgba t = fgc; fgc = bgc; bgc = t; }

    /* light/xlight: reduce alpha */
    CGFloat alpha = 1.0;
    if (sc->xlight) alpha = 0.3;
    else if (sc->light) alpha = 0.6;

    CGColorRef color = CGColorCreateGenericRGB(fgc.r, fgc.g, fgc.b, alpha);

    CFStringRef keys[2]   = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    CFTypeRef   values[2] = { drawFont, color };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL,
                               (const void**)keys, (const void**)values,
                               2, &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);
    CGColorRelease(color);

    CFAttributedStringRef as   = CFAttributedStringCreate(NULL, cfs, attrs);
    CTLineRef             line = CTLineCreateWithAttributedString(as);

    CGFloat ascent, descent;
    CGFloat width = CTLineGetTypographicBounds(line, &ascent, &descent, NULL);

    CGFloat tx = PX(x);
    CGFloat ty = PY(y) + ascent + yoff;

    /* draw text */
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, tx, ty);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    if (sc->textpath != LONG_MAX / 4) {
        CGFloat rot = -(sc->textpath - LONG_MAX / 4) * (2.0 * M_PI) / (double)LONG_MAX;
        CGContextRotateCTM(ctx, rot);
    }
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextSetTextPosition(ctx, 0, 0);

    if (sc->hollow) {
        /* hollow: stroke the text outlines instead of filling */
        CGContextSetTextDrawingMode(ctx, kCGTextStroke);
        CGContextSetRGBStrokeColor(ctx, fgc.r, fgc.g, fgc.b, alpha);
        CGContextSetLineWidth(ctx, 0.5);
    } else if (sc->raised) {
        /* raised: draw with a slight shadow/offset for embossed look */
        CGContextSetShadow(ctx, CGSizeMake(1, -1), 1.0);
    }

    CTLineDraw(line, ctx);
    CGContextRestoreGState(ctx);

    /* underline: draw a line below the baseline */
    if (sc->underline) {
        CGFloat uy = PY(y) + ascent + descent * 0.5;
        CGContextSetRGBStrokeColor(ctx, fgc.r, fgc.g, fgc.b, alpha);
        CGContextSetLineWidth(ctx, 1.0);
        CGContextMoveToPoint(ctx, tx, uy);
        CGContextAddLineToPoint(ctx, tx + width, uy);
        CGContextStrokePath(ctx);
    }

    /* strikeout: draw a line through the middle of the text */
    if (sc->strikeout) {
        CGFloat sy = PY(y) + ascent * 0.6;
        CGContextSetRGBStrokeColor(ctx, fgc.r, fgc.g, fgc.b, alpha);
        CGContextSetLineWidth(ctx, 1.0);
        CGContextMoveToPoint(ctx, tx, sy);
        CGContextAddLineToPoint(ctx, tx + width, sy);
        CGContextStrokePath(ctx);
    }

    CFRelease(line);
    CFRelease(as);
    CFRelease(attrs);
    CFRelease(cfs);
    if (releaseDraw) CFRelease(drawFont);
}

/*******************************************************************************
*                                                                              *
*                        Window init / deinit                                  *
*                                                                              *
*******************************************************************************/

static void win_init(winptr win, int wid, int parwid, int w, int h)
{
    memset(win, 0, sizeof(winrec));
    win->wid     = wid;
    win->parwid  = parwid;
    win->maxxg   = w;
    win->maxyg   = h;
    win->visible = FALSE;
    win->frame   = TRUE;
    win->sizable = TRUE;
    win->sysbar  = TRUE;
    win->bufmod  = TRUE;
    win->curdsp  = 1;
    win->curupd  = 1;
    win->cfont   = fntlst;
    win->fontsz  = DEF_FONT_H;

    /* screen size in mm from shim */
    int smm_w  = pa_cocoa_screen_wmm();
    int smm_h  = pa_cocoa_screen_hmm();
    int spx_w  = pa_cocoa_screen_w();
    int spx_h  = pa_cocoa_screen_h();
    win->dpmx  = (smm_w > 0) ? spx_w * 1000 / smm_w : 3780; /* ~96 dpi */
    win->dpmy  = (smm_h > 0) ? spx_h * 1000 / smm_h : 3780;
    win->gfpoint = win->fontsz * 2835.0f / (float)win->dpmy;

    /* character cell size from font metrics */
    CTFontRef f = fntlst ? fntlst->ctfont : NULL;
    if (f) {
        win->linespace  = (int)(win->fontsz + 0.5);
        {
            /* use advance of 'M' as max_advance approximation */
            CGGlyph  glyph;
            UniChar  ch = 'M';
            CTFontGetGlyphsForCharacters(f, &ch, &glyph, 1);
            CGSize   adv;
            CTFontGetAdvancesForGlyphs(f, kCTFontOrientationDefault,
                                       &glyph, &adv, 1);
            win->charspace = (int)(adv.width + 0.5);
        }
        if (win->charspace <= 0) win->charspace = win->linespace / 2;
    } else {
        win->linespace = DEF_FONT_H + 2;
        win->charspace = (DEF_FONT_H + 2) / 2;
    }
    win->maxx = w / win->charspace;
    win->maxy = h / win->linespace;
    if (win->maxx < 1) win->maxx = 1;
    if (win->maxy < 1) win->maxy = 1;

    /* init all screens */
    for (int i = 0; i < MAXCON; i++) {
        scnptr sc   = &win->screens[i];
        sc->curx    = 1;
        sc->cury    = 1;
        sc->curxg   = 1;
        sc->curyg   = 1;
        sc->curv    = TRUE;
        sc->fc      = colortbl[ami_black];
        sc->bc      = colortbl[ami_white];
        sc->fmod    = mdnorm;
        sc->bmod    = mdnorm;
        sc->lwidth  = 1.0;
        sc->font    = fntlst;
        sc->autof   = TRUE;
        sc->textpath = LONG_MAX / 4; /* default: east (normal reading) */
        sc->offx    = 0;
        sc->offy    = 0;
        sc->scalex  = 1.0f;
        sc->scaley  = 1.0f;
    }
}

/*******************************************************************************
*                                                                              *
*                        Library init (constructor)                            *
*                                                                              *
*******************************************************************************/

__attribute__((constructor))
static void pa_graphics_init(void)
{
    if (inited) return;
    inited = TRUE;

    pa_cocoa_init();
    init_fonts();
    memset(opnfil, 0, sizeof(opnfil));

    fautohold = TRUE; /* hold window open on exit until keypress */

    /* read configuration file */
    {
        ami_valptr config_root = NULL;
        ami_valptr term_root;
        ami_valptr graph_root;
        ami_valptr diag_root;
        ami_valptr mac_root;
        ami_valptr vp;
        char*      errstr;

        ami_config(&config_root);

        /* find "terminal" block */
        term_root = ami_schlst("terminal", config_root);
        if (term_root && term_root->sublist) term_root = term_root->sublist;

        if (term_root) {

            vp = ami_schlst("maxxd", term_root);
            if (vp) {

                cfgmaxxd = strtol(vp->value, &errstr, 10);
                if (*errstr) cfgmaxxd = 0;

            }
            vp = ami_schlst("maxyd", term_root);
            if (vp) {

                cfgmaxyd = strtol(vp->value, &errstr, 10);
                if (*errstr) cfgmaxyd = 0;

            }
            vp = ami_schlst("joystick", term_root);
            if (vp) {

                joyenb = strtol(vp->value, &errstr, 10);
                if (*errstr) joyenb = 0;

            }
            vp = ami_schlst("mouse", term_root);
            if (vp) {

                mouseenb = strtol(vp->value, &errstr, 10);
                if (*errstr) mouseenb = 1;

            }
            vp = ami_schlst("dump_event", term_root);
            if (vp) {

                dmpevt = strtol(vp->value, &errstr, 10);
                if (*errstr) dmpevt = 0;

            }

        }

        /* find "graphics" block */
        graph_root = ami_schlst("graphics", config_root);
        if (graph_root) {

            vp = ami_schlst("console_points", graph_root->sublist);
            if (vp) {

                conpnt = strtol(vp->value, &errstr, 10);
                if (*errstr) conpnt = 0;

            }

            vp = ami_schlst("dialogerr", graph_root->sublist);
            if (vp) {

                dialogerr = strtol(vp->value, &errstr, 10);
                if (*errstr) dialogerr = 0;

            }

            /* find macosx subsection */
            mac_root = ami_schlst("macosx", graph_root->sublist);
            if (mac_root) {

                /* find diagnostic subsection */
                diag_root = ami_schlst("diagnostics", mac_root->sublist);
                if (diag_root) {

                    vp = ami_schlst("dump_messages", diag_root->sublist);
                    if (vp) {

                        dmpmsg = strtol(vp->value, &errstr, 10);
                        if (*errstr) dmpmsg = 0;

                    }

                    vp = ami_schlst("print_font_metrics", diag_root->sublist);
                    if (vp) {

                        prtftm = strtol(vp->value, &errstr, 10);
                        if (*errstr) prtftm = 0;

                    }

                }

            }

        }
    }

    /* turn off I/O buffering */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* hook libc/stdio.c write/read vectors */
    ovr_write(iwrite, &ofpwrite);
    ovr_read(iread,  &ofpread);
    ovr_close(iclose, &ofpclose);

    /* compute default window pixel size from TERM font character grid */
    {
        fontptr fp = fntlst;
        int termw = cfgmaxxd > 0 ? cfgmaxxd : MAXXD;
        int termh = cfgmaxyd > 0 ? cfgmaxyd : MAXYD;
        int fontsz = conpnt > 0 ? conpnt : DEF_FONT_H;

        if (conpnt > 0 && fp) {
            CTFontRef newfont = CTFontCreateCopyWithAttributes(fp->ctfont,
                                    (CGFloat)fontsz, NULL, NULL);
            if (newfont) {
                CTFontRef old = fp->ctfont;
                fp->ctfont = newfont;
                fp->size = fontsz;
                CFRelease(old);
            }
        }

        CGGlyph glyph; UniChar ch = 'M'; CGSize adv;
        CTFontGetGlyphsForCharacters(fp->ctfont, &ch, &glyph, 1);
        CTFontGetAdvancesForGlyphs(fp->ctfont, kCTFontOrientationDefault,
                                   &glyph, &adv, 1);
        int cs = (int)(adv.width + 0.5);
        int ls = (int)(fp->size + 0.5);
        if (cs <= 0) cs = ls / 2;
        maxxd = termw * cs;
        maxyd = termh * ls;
    }

    /* open default window for stdout (fd=1) */
    pa_winhan han = pa_cocoa_create_window(100, 100, maxxd, maxyd,
#if defined(__MACH__) || defined(__FreeBSD__)
                                           getprogname()
#else
                                           "ami"
#endif
                                           );
    if (han) {
        winptr win = &wintbl[1];
        win_init(win, 1, 0, maxxd, maxyd);
        win->han   = han;
        win->focus = TRUE;
        opnfil[1]  = 1; /* mark slot in use */
        pa_cocoa_set_bufmod(han, win->bufmod);
        pa_cocoa_set_background(han, 1.0f, 1.0f, 1.0f);
        clear_window(win);
        pa_cocoa_show_window(han);
        pa_cocoa_flush(han);
    }

    if (joyenb) pa_cocoa_joy_init();

    pa_cocoa_start_event_thread();
}

__attribute__((destructor))
static void pa_graphics_deinit(void)
{
    winptr win;
    ami_evtrec er;

    /* check if stdout is attached to a window */
    win = NULL;
    if (opnfil[1]) win = &wintbl[1];
    if (win && win->han) {

        /* if the program exits without the user ordering an exit,
           and autohold is set, hold the window open */
        if (!fend && fautohold) {

            /* construct "Finished - <progname>" title */
            const char* fini = "Finished - ";
            const char* pname = getprogname();
            char* trmnam = malloc(strlen(fini) + strlen(pname) + 1);
            if (trmnam) {
                strcpy(trmnam, fini);
                strcat(trmnam, pname);
                pa_cocoa_set_title(win->han, trmnam);
                free(trmnam);
            }

            /* wait for a formal end (etterm from close button or Ctrl-C) */
            while (!fend) ami_event(stdin, &er);

        }

    }

    /* restore stdio vectors */
    {
        pwrite_t cppwrite;
        pread_t  cppread;
        pclose_t cppclose;
        ovr_write(ofpwrite, &cppwrite);
        ovr_read(ofpread, &cppread);
        ovr_close(ofpclose, &cppclose);
    }

    if (joyenb) pa_cocoa_joy_deinit();
    pa_cocoa_deinit();
}

/*******************************************************************************
*                                                                              *
*                        Context helpers                                       *
*                                                                              *
*******************************************************************************/

/* Map PA drawing mode to CoreGraphics blend mode */
static CGBlendMode mode2blend(drawmode m)
{
    switch (m) {
    case mdnorm:   return kCGBlendModeNormal;
    case mdinvis:  return kCGBlendModeNormal; /* caller should skip draw */
    case mdxor:    return kCGBlendModeDifference;
    case mdand:    return kCGBlendModeMultiply;
    case mdor:     return kCGBlendModeScreen;
    default:       return kCGBlendModeNormal;
    }
}

/* Get the CGContext for a window and configure it for the current screen.
 * Returns NULL if the foreground mode is invisible (caller should skip). */
static CGContextRef get_ctx(winptr win)
{
    scnptr sc = curscn(win);
    if (sc->fmod == mdinvis) return NULL; /* invisible — skip drawing */
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return NULL;
    CGContextSetLineWidth(ctx, sc->lwidth);
    CGContextSetShouldAntialias(ctx, false); /* crisp lines */
    CGContextSetBlendMode(ctx, mode2blend(sc->fmod));
    set_fg(ctx, sc->fc);
    return ctx;
}

/* Apply viewport offset/scale transform to the bitmap context.
 * Resets the CTM to retina-scale-only, then applies offset + scale. */
static void settrans(winptr win)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;
    scnptr sc = curscn(win);
    CGAffineTransform cur = CGContextGetCTM(ctx);
    CGAffineTransform inv = CGAffineTransformInvert(cur);
    CGContextConcatCTM(ctx, inv);
    size_t pw = CGBitmapContextGetWidth(ctx);
    CGFloat retina = (win->maxxg > 0) ? (CGFloat)pw / win->maxxg : 1.0;
    CGContextScaleCTM(ctx, retina, retina);
    if (sc->offx || sc->offy)
        CGContextTranslateCTM(ctx, (CGFloat)sc->offx, (CGFloat)sc->offy);
    if (sc->scalex != 1.0f || sc->scaley != 1.0f)
        CGContextScaleCTM(ctx, sc->scalex, sc->scaley);
}

/* Clear the window to background color */
static void clear_window(winptr win)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;
    scnptr sc = curscn(win);
    CGContextSaveGState(ctx);
    CGAffineTransform cur = CGContextGetCTM(ctx);
    CGAffineTransform inv = CGAffineTransformInvert(cur);
    CGContextConcatCTM(ctx, inv);
    size_t pw = CGBitmapContextGetWidth(ctx);
    CGFloat retina = (win->maxxg > 0) ? (CGFloat)pw / win->maxxg : 1.0;
    CGContextScaleCTM(ctx, retina, retina);
    CGContextSetBlendMode(ctx, kCGBlendModeNormal);
    CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
    CGContextFillRect(ctx, CGRectMake(0, 0, win->maxxg, win->maxyg));
    CGContextRestoreGState(ctx);
}

/*******************************************************************************
*                                                                              *
*                   Character output / terminal emulation                      *
*                                                                              *
*******************************************************************************/

/* Scroll window content up by one text line.
 * Uses raw bitmap memmove: the bitmap context is CG y-up, so row 0 of the
 * data is the BOTTOM of the screen. Shifts physical rows toward higher
 * addresses (visually upward), then redraws the freed bottom strip with
 * the background color. */
static void scroll_up(winptr win)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;

    size_t   width    = CGBitmapContextGetWidth(ctx);
    size_t   height   = CGBitmapContextGetHeight(ctx);
    size_t   rowbytes = CGBitmapContextGetBytesPerRow(ctx);
    uint8_t* data     = (uint8_t*)CGBitmapContextGetData(ctx);
    if (!data) return;

    /* scale: physical pixels per logical pixel */
    float scale   = (win->maxyg > 0) ? (float)height / win->maxyg : 1.0f;
    size_t physLS = (size_t)(win->linespace * scale + 0.5f);
    if (physLS == 0 || physLS >= height) return;
    (void)width; /* not needed for row-wise memmove */

    /* Move the content up one text line. The bitmap is CG y-up: data row 0
       is the BOTTOM of the screen (drawing flips logical coordinates via
       PY()), so shifting visual content up means moving data rows toward
       higher addresses. */
    memmove(data + physLS * rowbytes, data, (height - physLS) * rowbytes);

    /* fill the freed rows at the bottom with the background color using CG */
    scnptr sc = curscn(win);
    CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
    long clearY = (win->maxy - 1) * win->linespace; /* top of last text row, 0-based */
    CGContextFillRect(ctx, CGRectMake(0, PY(clearY + 1), win->maxxg, win->linespace));
    /* restore foreground */
    CGContextSetRGBFillColor(ctx, sc->fc.r, sc->fc.g, sc->fc.b, 1.0);
}

/* Draw one character cell at the current text cursor position.
 * Fills the background cell first, then draws the glyph.
 * Uses curxg/curyg (pixel coords) so arbitrary cursor placement works.
 * Returns the actual pixel width of the character drawn. */
static int draw_char_at(winptr win, char c)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return win->charspace;
    scnptr  sc  = curscn(win);
    fontptr fp  = sc->font ? sc->font : fntlst;

    /* measure actual character width */
    int cw;
    if (fp && !fp->fixed) {
        cw = (int)(measure_string(fp, &c, 1) + 0.5);
        if (cw <= 0) cw = win->charspace;
    } else {
        cw = win->charspace;
    }

    long px = sc->curxg - 1; /* 1-based to 0-based */
    long py = sc->curyg - 1;

    /* fill cell background */
    if (sc->bmod != mdinvis) {
        CGContextSetBlendMode(ctx, mode2blend(sc->bmod));
        CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
        if (sc->textpath != LONG_MAX / 4) {
            CGFloat ascent = fp ? CTFontGetAscent(fp->ctfont) : (CGFloat)win->linespace * 0.75;
            CGContextSaveGState(ctx);
            CGContextTranslateCTM(ctx, px, py + ascent);
            CGContextScaleCTM(ctx, 1.0, -1.0);
            double a = -(sc->textpath - LONG_MAX / 4) * (2.0 * M_PI) / (double)LONG_MAX;
            CGContextRotateCTM(ctx, a);
            CGContextFillRect(ctx, CGRectMake(0, ascent - (CGFloat)win->linespace,
                                              cw, win->linespace));
            CGContextRestoreGState(ctx);
        } else {
            CGContextFillRect(ctx, CGRectMake(px, py, cw, win->linespace));
        }
    }

    /* draw glyph (use foreground blend mode) */
    CGContextSetBlendMode(ctx, mode2blend(sc->fmod));
    char s[2] = { c, 0 };
    CGContextSetRGBFillColor(ctx, sc->fc.r, sc->fc.g, sc->fc.b, 1.0);
    draw_string(ctx, fp, sc, px + 1, py + 1, s, 1);

    return cw;
}

/* Place one character into a window, handling control characters. */
static void plcchr(winptr win, char c)
{
    scnptr sc = curscn(win);

    if (c == '\n') {
        /* newline: CR+LF — move to column 1 on next row */
        sc->curx  = 1;
        sc->curxg = 1;
        if (sc->cury >= win->maxy && sc->autof) {
            scroll_up(win);
        } else {
            sc->cury++;
            sc->curyg += win->linespace;
        }
    } else if (c == '\r') {
        sc->curx  = 1;
        sc->curxg = 1;
    } else if (c == '\b') {
        if (sc->curx > 1) {
            sc->curx--;
            sc->curxg = (sc->curx - 1) * win->charspace + 1;
        }
    } else if (c == '\f') {
        /* form feed: clear screen and home cursor */
        clear_window(win);
        sc->curx  = 1;
        sc->cury  = 1;
        sc->curxg = 1;
        sc->curyg = 1;
    } else if (c == '\t') {
        /* advance to next 8-column tab stop */
        long next = ((sc->curx - 1) / 8 + 1) * 8 + 1;
        if (next > win->maxx) next = win->maxx;
        sc->curx  = next;
        sc->curxg = (sc->curx - 1) * win->charspace + 1;
    } else if ((unsigned char)c >= 0x20) {
        /* printable character — draw and advance by actual glyph width */
        int cs = draw_char_at(win, c);
        if (sc->textpath != LONG_MAX / 4) {
            double a = sc->textpath * (2.0 * M_PI) / (double)LONG_MAX;
            sc->curxg += (int)round(cs * sin(a));
            sc->curyg -= (int)round(cs * cos(a));
        } else {
            sc->curx++;
            sc->curxg += cs;
            if (sc->curx > win->maxx && sc->autof) {
                sc->curx  = 1;
                sc->curxg = 1;
                if (sc->cury >= win->maxy) {
                    scroll_up(win);
                } else {
                    sc->cury++;
                    sc->curyg += win->linespace;
                }
            }
        }
    }
}

/* Sync cursor state to the Cocoa view for overlay drawing */
static void update_cursor(winptr win)
{
    scnptr sc = curscn(win);
    int vis = sc->curv && win->focus &&
              sc->curxg >= 1 && sc->curyg >= 1 &&
              sc->curx <= win->maxx && sc->cury <= win->maxy;
    pa_cocoa_set_cursor(win->han, vis,
                        sc->curxg - 1, sc->curyg - 1,
                        win->charspace, win->linespace);
}

/* Write interceptor: routes writes to stdout/window fds through plcchr() */
static ssize_t iwrite(int fd, const void* buff, size_t count)
{
    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && wintbl[fd].han) {
        const char* p   = (const char*)buff;
        size_t      cnt = count;
        winptr      win = &wintbl[fd];
        while (cnt--) plcchr(win, *p++);
        update_cursor(win);
        pa_cocoa_flush(win->han);
        return (ssize_t)count;
    }
    return (*ofpwrite)(fd, buff, count);
}

/* Read interceptor: routes reads from window input fds (future: keyboard queue) */
static ssize_t iread(int fd, void* buff, size_t count)
{
    return (*ofpread)(fd, buff, count);
}

static int iclose(int fd)
{
    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && wintbl[fd].han) {
        pa_cocoa_destroy_window(wintbl[fd].han);
        wintbl[fd].han = NULL;
        opnfil[fd] = 0;
    }
    return (*ofpclose)(fd);
}

/*******************************************************************************
*                                                                              *
*                        Event translation                                     *
*                                                                              *
*******************************************************************************/

/* Map a PA window handle back to a wid */
static int han2wid(pa_winhan han)
{
    for (int i = 0; i < MAXFIL; i++)
        if (opnfil[i] && wintbl[i].han == han) return wintbl[i].wid;
    return 0;
}

static void translate_event(const pa_rawevent* raw, ami_evtrec* er)
{
    memset(er, 0, sizeof(*er));
    er->winid = han2wid(raw->win);

    switch (raw->type) {

    case PA_EVT_CHAR:
        if (raw->key.ch == 3) {
            /* Ctrl-C: generate etterm */
            er->etype = ami_etterm;
            fend = TRUE;
        } else {
            er->etype = ami_etchar;
            er->echar = (char)raw->key.ch;
        }
        break;

    case PA_EVT_KEYDOWN:
        switch (raw->special.code) {
        case PA_KEY_UP:       er->etype = ami_etup;      break;
        case PA_KEY_DOWN:     er->etype = ami_etdown;    break;
        case PA_KEY_LEFT:     er->etype = ami_etleft;    break;
        case PA_KEY_RIGHT:    er->etype = ami_etright;   break;
        case PA_KEY_HOME:     er->etype = ami_ethome;    break;
        case PA_KEY_END:      er->etype = ami_etend;     break;
        case PA_KEY_PAGEUP:   er->etype = ami_etpagu;    break;
        case PA_KEY_PAGEDOWN: er->etype = ami_etpagd;    break;
        case PA_KEY_DELETE:   er->etype = ami_etdelcf;   break;
        case PA_KEY_BACK:     er->etype = ami_etdelcb;   break;
        case PA_KEY_ENTER:    er->etype = ami_etenter;   break;
        case PA_KEY_TAB:      er->etype = ami_ettab;     break;
        case PA_KEY_ESC:      er->etype = ami_etcan;     break;
        default:
            if (raw->special.code >= PA_KEY_F1 &&
                raw->special.code <= PA_KEY_F12) {
                er->etype = ami_etfun;
                er->fkey  = raw->special.code - PA_KEY_F1 + 1;
            }
            break;
        }
        break;

    case PA_EVT_MOUSE_MOVE:
        er->etype  = ami_etmoumovg;
        er->mmoung = 1;
        er->moupxg = raw->mouse.x;
        er->moupyg = raw->mouse.y;
        break;

    case PA_EVT_MOUSE_DOWN:
        er->etype  = ami_etmouba;
        er->amoun  = 1;
        er->amoubn = raw->mouse.buttons;
        break;

    case PA_EVT_MOUSE_UP:
        er->etype  = ami_etmoubd;
        er->dmoun  = 1;
        er->dmoubn = raw->mouse.buttons;
        break;

    case PA_EVT_RESIZE:
        er->etype  = ami_etresize;
        er->rszxg  = raw->resize.w;
        er->rszyg  = raw->resize.h;
        er->rszx   = raw->resize.w;
        er->rszy   = raw->resize.h;
        {
            winptr win = NULL;
            for (int i = 0; i < MAXFIL; i++)
                if (opnfil[i] && wintbl[i].han == raw->win)
                    { win = &wintbl[i]; break; }
            if (win) {
                er->rszx = raw->resize.w / win->charspace;
                er->rszy = raw->resize.h / win->linespace;
                if (!win->bufmod) {
                    win->maxxg = raw->resize.w;
                    win->maxyg = raw->resize.h;
                    win->maxx  = er->rszx;
                    win->maxy  = er->rszy;
                    pa_cocoa_resize_bitmap(win->han,
                                           raw->resize.w, raw->resize.h);
                }
            }
        }
        break;

    case PA_EVT_CLOSE:
        er->etype = ami_etterm;
        break;

    case PA_EVT_FOCUS:
        er->etype = ami_etfocus;
        {
            winptr fw = NULL;
            for (int i = 0; i < MAXFIL; i++)
                if (opnfil[i] && wintbl[i].han == raw->win)
                    { fw = &wintbl[i]; break; }
            if (fw) {
                fw->focus = TRUE;
                update_cursor(fw);
                pa_cocoa_flush(fw->han);
            }
        }
        break;

    case PA_EVT_UNFOCUS:
        er->etype = ami_etnofocus;
        {
            winptr fw = NULL;
            for (int i = 0; i < MAXFIL; i++)
                if (opnfil[i] && wintbl[i].han == raw->win)
                    { fw = &wintbl[i]; break; }
            if (fw) {
                fw->focus = FALSE;
                update_cursor(fw);
                pa_cocoa_flush(fw->han);
            }
        }
        break;

    case PA_EVT_TIMER:
        if (raw->timer.id == FRMTIM) {
            er->etype = ami_etframe;
        } else {
            er->etype  = ami_ettim;
            er->timnum = raw->timer.id + 1; /* PA timers are 1-based */
        }
        break;

    case PA_EVT_REDRAW:
        er->etype = ami_etredraw;
        er->rsx   = raw->redraw.x;
        er->rsy   = raw->redraw.y;
        er->rex   = raw->redraw.x + raw->redraw.w - 1;
        er->rey   = raw->redraw.y + raw->redraw.h - 1;
        break;

    case PA_EVT_JOY_MOVE:
        er->etype = ami_etjoymov;
        er->winid = 0;
        er->mjoyn = raw->joymove.jn + 1;
        /* the cocoa bridge delivers axes at int full scale; the API is long
           full scale (factor is 1 where long is 32 bits) */
        er->joypx = (long)raw->joymove.ax[0]*(LONG_MAX/INT_MAX);
        er->joypy = (long)raw->joymove.ax[1]*(LONG_MAX/INT_MAX);
        er->joypz = (long)raw->joymove.ax[2]*(LONG_MAX/INT_MAX);
        er->joyp4 = (long)raw->joymove.ax[3]*(LONG_MAX/INT_MAX);
        er->joyp5 = (long)raw->joymove.ax[4]*(LONG_MAX/INT_MAX);
        er->joyp6 = (long)raw->joymove.ax[5]*(LONG_MAX/INT_MAX);
        break;

    case PA_EVT_JOY_DOWN:
        er->etype  = ami_etjoyba;
        er->winid  = 0;
        er->ajoyn  = raw->joybtn.jn + 1;
        er->ajoybn = raw->joybtn.btn;
        break;

    case PA_EVT_JOY_UP:
        er->etype  = ami_etjoybd;
        er->winid  = 0;
        er->djoyn  = raw->joybtn.jn + 1;
        er->djoybn = raw->joybtn.btn;
        break;

    case PA_EVT_MENU:
        er->etype  = ami_etmenus;
        er->menuid = raw->menu.id;
        break;

    case PA_EVT_MIN:
        er->etype = ami_etmin;
        break;

    case PA_EVT_MAX:
        er->etype = ami_etmax;
        break;

    case PA_EVT_NORM:
        er->etype = ami_etnorm;
        break;

    case PA_EVT_WIDGET:
        switch (raw->widget.act) {
        case PA_WIDGET_BUTTON:
            er->etype = ami_etbutton;
            er->butid = raw->widget.id;
            break;
        case PA_WIDGET_CHECKBOX:
            er->etype  = ami_etchkbox;
            er->ckbxid = raw->widget.id;
            break;
        case PA_WIDGET_RADIO:
            er->etype  = ami_etradbut;
            er->radbid = raw->widget.id;
            break;
        case PA_WIDGET_SCROLL_PAGEUP:
            er->etype  = ami_etsclulp;
            er->sclupid = raw->widget.id;
            break;
        case PA_WIDGET_SCROLL_PAGEDN:
            er->etype  = ami_etscldrp;
            er->scldpid = raw->widget.id;
            break;
        case PA_WIDGET_SCROLL_POS:
            er->etype  = ami_etsclpos;
            er->sclpid = raw->widget.id;
            er->sclpos = raw->widget.pos;
            break;
        case PA_WIDGET_SLIDER_POS:
            er->etype  = ami_etsldpos;
            er->sldpid = raw->widget.id;
            er->sldpos = raw->widget.pos;
            break;
        case PA_WIDGET_EDIT_DONE:
            er->etype  = ami_etedtbox;
            er->edtbid = raw->widget.id;
            break;
        case PA_WIDGET_NUM_DONE:
            er->etype  = ami_etnumbox;
            er->numbid = raw->widget.id;
            er->numbsl = raw->widget.pos;
            break;
        case PA_WIDGET_LIST_SEL:
            er->etype  = ami_etlstbox;
            er->lstbid = raw->widget.id;
            er->lstbsl = raw->widget.pos;
            break;
        case PA_WIDGET_DROP_SEL:
            er->etype  = ami_etdrpbox;
            er->drpbid = raw->widget.id;
            er->drpbsl = raw->widget.pos;
            break;
        case PA_WIDGET_DROPED_DONE:
            er->etype  = ami_etdrebox;
            er->drebid = raw->widget.id;
            break;
        case PA_WIDGET_TAB_SEL:
            er->etype  = ami_ettabbar;
            er->tabid  = raw->widget.id;
            er->tabsel = raw->widget.pos;
            break;
        }
        break;

    default:
        er->etype = ami_etchar;
        er->echar = 0;
        break;
    }
}

/*******************************************************************************
*                                                                              *
*                           PA API — text operations                           *
*                                                                              *
*******************************************************************************/

void ami_cursor(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    sc->curx   = x;
    sc->cury   = y;
    sc->curxg  = (x-1) * win->charspace + 1;
    sc->curyg  = (y-1) * win->linespace + 1;
}

long ami_maxx(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxx : 80;
}

long ami_maxy(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxy : 25;
}

void ami_home(FILE* f)    { ami_cursor(f, 1, 1); }

void ami_del(FILE* f)
{
    /* delete character at cursor — stub */
}

void ami_up(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->cury > 1) ami_cursor(f, sc->curx, sc->cury - 1);
}

void ami_down(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->cury < win->maxy) ami_cursor(f, sc->curx, sc->cury + 1);
}

void ami_left(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->curx > 1) ami_cursor(f, sc->curx - 1, sc->cury);
}

void ami_right(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->curx < win->maxx) ami_cursor(f, sc->curx + 1, sc->cury);
}

void ami_blink(FILE* f, long e)      { /* stub — CoreText doesn't blink */ }

void ami_reverse(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->reverse = e;
}

void ami_superscript(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->superscript = e;
    curscn(win)->subscript = 0; /* mutually exclusive */
}

void ami_subscript(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->subscript = e;
    curscn(win)->superscript = 0; /* mutually exclusive */
}

void ami_underline(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->underline = e;
}

void ami_italic(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->italic = e;
    rebuild_font(win);
}

void ami_bold(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bold = e;
    rebuild_font(win);
}

void ami_strikeout(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->strikeout = e;
}

void ami_standout(FILE* f, long e)
{
    /* implement as reverse video */
    ami_reverse(f, e);
}

void ami_fcolor(FILE* f, ami_color c)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->fc = ami2rgba(c);
}

void ami_bcolor(FILE* f, ami_color c)
{
    winptr win = f2win(f); if (!win) return;
    pa_rgba bc = ami2rgba(c);
    curscn(win)->bc = bc;
    if (win->han) pa_cocoa_set_background(win->han, bc.r, bc.g, bc.b);
}

void ami_auto(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->autof = e;
}

void ami_curvis(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->curv = e;
    update_cursor(win);
    pa_cocoa_flush(win->han);
}

void ami_scroll(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    ami_scrollg(f, x * win->charspace, y * win->linespace);
}

long ami_curx(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->curx : 1;
}

long ami_cury(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->cury : 1;
}

long ami_curbnd(FILE* f)
{
    winptr win = f2win(f); if (!win) return FALSE;
    scnptr sc  = curscn(win);
    return sc->curx >= 1 && sc->curx <= win->maxx &&
           sc->cury >= 1 && sc->cury <= win->maxy;
}

void ami_select(FILE* f, long u, long d)
{
    winptr win = f2win(f); if (!win) return;
    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON) return;
    win->curupd = u;
    win->curdsp = d;
    pa_cocoa_select_screens(win->han, u - 1, d - 1);
}

void ami_settab(FILE* f, long t)  { /* stub */ }
void ami_restab(FILE* f, long t)  { /* stub */ }
void ami_clrtab(FILE* f)         { /* stub */ }

void ami_viewoffg(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc = curscn(win);
    sc->offx = x;
    sc->offy = y;
    settrans(win);
}

void ami_viewscale(FILE* f, float x, float y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc = curscn(win);
    sc->scalex = x;
    sc->scaley = y;
    settrans(win);
}
void ami_linestyle(FILE* f, ami_lstyle style)     { /* stub */ }
void ami_setpoints(FILE* f, float ps)
{
    winptr win = f2win(f); if (!win) return;
    int pixsiz = (int)(ps * (float)win->dpmy / 2835.0f + 0.5f);
    if (pixsiz < 1) pixsiz = 1;
    win->fontsz  = (CGFloat)pixsiz;
    win->gfpoint = ps;
    scnptr sc = curscn(win);
    if (sc->font && sc->font->name) {
        if (sc->font->ctfont) CFRelease(sc->font->ctfont);
        sc->font->ctfont = make_ctfont(sc->font->name, win->fontsz,
                                       sc->bold, sc->italic);
        sc->font->size   = win->fontsz;
        update_metrics(win);
    }
}

float ami_points(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->gfpoint : 11.0f;
}

long  ami_funkey(FILE* f)         { return 12; /* F1-F12 */ }

void ami_frametimer(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    if (e) {
        if (!win->frmrun) {
            pa_cocoa_set_timer(win->han, FRMTIM, 170, 1); /* 17ms repeating */
            win->frmrun = TRUE;
        }
    } else {
        if (win->frmrun) {
            pa_cocoa_kill_timer(win->han, FRMTIM);
            win->frmrun = FALSE;
        }
    }
}
void ami_autohold(long e)            { fautohold = e; }

void ami_wrtstr(FILE* f, char* s)
{
    if (!s) return;
    /* route through fwrite so stdio layer handles it */
    fwrite(s, 1, strlen(s), f);
}

void ami_wrtstrn(FILE* f, char* s, long n)
{
    if (!s || n <= 0) return;
    fwrite(s, 1, n, f);
}

void ami_sizbufg(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    if (x < 1 || y < 1) return;
    pa_cocoa_resize_bitmap(win->han, x, y);
    win->maxxg = x;
    win->maxyg = y;
    win->maxx  = x / win->charspace;
    win->maxy  = y / win->linespace;
    if (win->maxx < 1) win->maxx = 1;
    if (win->maxy < 1) win->maxy = 1;
    /* the buffer's drawing surface was recreated: drawing attributes reset
       to defaults, matching the Windows DC-recreation behavior */
    curscn(win)->lwidth = 1.0;
}

void ami_sizbuf(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    ami_sizbufg(f, x * win->charspace, y * win->linespace);
}

void ami_title(FILE* f, char* ts)
{
    winptr win = f2win(f); if (!win || !ts) return;
    pa_cocoa_set_title(win->han, ts);
}

void ami_eventover(ami_evtcod e, ami_pevthan eh, ami_pevthan* oeh)
{
    /* no override system in this implementation */
    if (oeh) *oeh = NULL;
}

void ami_eventsover(ami_pevthan eh, ami_pevthan* oeh)
{
    if (oeh) *oeh = NULL;
}

void ami_sendevent(FILE* f, ami_evtrec* er) { /* stub */ }

/*******************************************************************************
*                                                                              *
*                        PA API — graphical operations                         *
*                                                                              *
*******************************************************************************/

long ami_maxxg(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxxg : maxxd;
}

long ami_maxyg(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxyg : maxyd;
}

long ami_curxg(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->curxg : 1;
}

long ami_curyg(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->curyg : 1;
}

void ami_cursorg(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    sc->curxg  = x;
    sc->curyg  = y;
    sc->curx   = (x - 1) / win->charspace + 1;
    sc->cury   = (y - 1) / win->linespace + 1;
}

void ami_line(FILE* f, long x1, long y1, long x2, long y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx,    PX(x1) + 0.5, PY(y1) + 0.5);
    CGContextAddLineToPoint(ctx, PX(x2) + 0.5, PY(y2) + 0.5);
    CGContextStrokePath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_linewidth(FILE* f, long w)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    sc->lwidth = (CGFloat)(w > 0 ? w : 1);
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (ctx) CGContextSetLineWidth(ctx, sc->lwidth);
}

void ami_rect(FILE* f, long x1, long y1, long x2, long y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGRect r = CGRectMake(PX(x1)+0.5, PY(y1)+0.5, x2-x1, y2-y1);
    CGContextStrokeRect(ctx, r);
    pa_cocoa_flush(win->han);
}

void ami_frect(FILE* f, long x1, long y1, long x2, long y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGContextFillRect(ctx, CGRectMake(PX(x1), PY(y1), x2-x1+1, y2-y1+1));
    pa_cocoa_flush(win->han);
}

/* Build a rounded rectangle CGPath using elliptical corner arcs.
 * xs, ys are the full width/height of the corner ellipses (PA convention).
 * Uses CGPath with per-arc transforms since CGContext transforms reset paths. */
static CGPathRef create_rrect_path(CGFloat x, CGFloat y,
                                   CGFloat w, CGFloat h,
                                   CGFloat xs, CGFloat ys)
{
    CGFloat rx = xs / 2.0;
    CGFloat ry = ys / 2.0;
    if (rx > w / 2.0) rx = w / 2.0;
    if (ry > h / 2.0) ry = h / 2.0;

    CGMutablePathRef path = CGPathCreateMutable();

    /* top edge */
    CGPathMoveToPoint(path, NULL, x + rx, y);
    CGPathAddLineToPoint(path, NULL, x + w - rx, y);

    /* top-right corner */
    CGAffineTransform t = CGAffineTransformMake(rx, 0, 0, ry, x + w - rx, y + ry);
    CGPathAddArc(path, &t, 0, 0, 1, -M_PI_2, 0, false);

    /* right edge */
    CGPathAddLineToPoint(path, NULL, x + w, y + h - ry);

    /* bottom-right corner */
    t = CGAffineTransformMake(rx, 0, 0, ry, x + w - rx, y + h - ry);
    CGPathAddArc(path, &t, 0, 0, 1, 0, M_PI_2, false);

    /* bottom edge */
    CGPathAddLineToPoint(path, NULL, x + rx, y + h);

    /* bottom-left corner */
    t = CGAffineTransformMake(rx, 0, 0, ry, x + rx, y + h - ry);
    CGPathAddArc(path, &t, 0, 0, 1, M_PI_2, M_PI, false);

    /* left edge */
    CGPathAddLineToPoint(path, NULL, x, y + ry);

    /* top-left corner */
    t = CGAffineTransformMake(rx, 0, 0, ry, x + rx, y + ry);
    CGPathAddArc(path, &t, 0, 0, 1, M_PI, M_PI + M_PI_2, false);

    CGPathCloseSubpath(path);
    return path;
}

void ami_rrect(FILE* f, long x1, long y1, long x2, long y2, long xs, long ys)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGPathRef path = create_rrect_path(PX(x1)+0.5, PY(y1)+0.5, x2-x1, y2-y1, xs, ys);
    CGContextAddPath(ctx, path);
    CGPathRelease(path);
    CGContextStrokePath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_frrect(FILE* f, long x1, long y1, long x2, long y2, long xs, long ys)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGPathRef path = create_rrect_path(PX(x1), PY(y1), x2-x1+1, y2-y1+1, xs, ys);
    CGContextAddPath(ctx, path);
    CGPathRelease(path);
    CGContextFillPath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_ellipse(FILE* f, long x1, long y1, long x2, long y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGContextStrokeEllipseInRect(ctx,
        CGRectMake(PX(x1)+0.5, PY(y1)+0.5, x2-x1, y2-y1));
    pa_cocoa_flush(win->han);
}

void ami_fellipse(FILE* f, long x1, long y1, long x2, long y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGContextFillEllipseInRect(ctx,
        CGRectMake(PX(x1), PY(y1), x2-x1+1, y2-y1+1));
    pa_cocoa_flush(win->han);
}

/* Convert PA angle to CG angle.
 * PA: 0 = top center, clockwise, LONG_MAX = 360°.
 * CG (flipped Y-down context): 0 = right (3 o'clock), clockwise.
 * So CG = PA_radians - π/2. */
#define PA2CG(a) (ANG2RAD(a) - M_PI_2)

void ami_arc(FILE* f, long x1, long y1, long x2, long y2, long sa, long ea)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGFloat cx = (PX(x1) + PX(x2)) / 2.0;
    CGFloat cy = (PY(y1) + PY(y2)) / 2.0;
    CGFloat rx = (x2 - x1) / 2.0;
    CGFloat ry = (y2 - y1) / 2.0;
    CGFloat start = PA2CG(sa);
    CGFloat end   = PA2CG(ea);
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, cx, cy);
    CGContextScaleCTM(ctx, rx, ry);
    CGContextBeginPath(ctx);
    CGContextAddArc(ctx, 0, 0, 1, start, end, 0);
    CGContextRestoreGState(ctx);
    CGContextStrokePath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_farc(FILE* f, long x1, long y1, long x2, long y2, long sa, long ea)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGFloat cx = (PX(x1) + PX(x2)) / 2.0;
    CGFloat cy = (PY(y1) + PY(y2)) / 2.0;
    CGFloat rx = (x2 - x1) / 2.0;
    CGFloat ry = (y2 - y1) / 2.0;
    CGFloat start = PA2CG(sa);
    CGFloat end   = PA2CG(ea);
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, cx, cy);
    CGContextScaleCTM(ctx, rx, ry);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, 0, 0);
    CGContextAddArc(ctx, 0, 0, 1, start, end, 0);
    CGContextClosePath(ctx);
    CGContextRestoreGState(ctx);
    CGContextFillPath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_fchord(FILE* f, long x1, long y1, long x2, long y2, long sa, long ea)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGFloat cx = (PX(x1) + PX(x2)) / 2.0;
    CGFloat cy = (PY(y1) + PY(y2)) / 2.0;
    CGFloat rx = (x2 - x1) / 2.0;
    CGFloat ry = (y2 - y1) / 2.0;
    CGFloat start = PA2CG(sa);
    CGFloat end   = PA2CG(ea);
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, cx, cy);
    CGContextScaleCTM(ctx, rx, ry);
    CGContextBeginPath(ctx);
    CGContextAddArc(ctx, 0, 0, 1, start, end, 0);
    CGContextClosePath(ctx);
    CGContextRestoreGState(ctx);
    CGContextFillPath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_ftriangle(FILE* f, long x1, long y1, long x2, long y2, long x3, long y3)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx,    PX(x1), PY(y1));
    CGContextAddLineToPoint(ctx, PX(x2), PY(y2));
    CGContextAddLineToPoint(ctx, PX(x3), PY(y3));
    CGContextClosePath(ctx);
    CGContextFillPath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_setpixel(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGContextFillRect(ctx, CGRectMake(PX(x), PY(y), 1, 1));
    pa_cocoa_flush(win->han);
}

void ami_fover(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->fmod = mdnorm;
}

void ami_bover(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bmod = mdnorm;
}

void ami_finvis(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->fmod = mdinvis;
}

void ami_binvis(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bmod = mdinvis;
}

void ami_fxor(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->fmod = mdxor; }
void ami_bxor(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->bmod = mdxor; }
void ami_fand(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->fmod = mdand; }
void ami_band(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->bmod = mdand; }
void ami_for(FILE* f)  { winptr win = f2win(f); if (win) curscn(win)->fmod = mdor;  }
void ami_bor(FILE* f)  { winptr win = f2win(f); if (win) curscn(win)->bmod = mdor;  }

long ami_chrsizx(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->charspace : 8;
}

long ami_chrsizy(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->linespace : 16;
}

long ami_fonts(FILE* f) { return fntcnt; }

/* Update charspace/linespace/maxx/maxy from the current font */
static void update_metrics(winptr win)
{
    scnptr sc = curscn(win);
    fontptr fp = sc->font ? sc->font : fntlst;
    if (!fp || !fp->ctfont) return;
    CTFontRef cf = fp->ctfont;
    /* linespace = requested font height (fontsz), matching Linux behavior
     * where linespace = gfhigh. This ensures chrsizy/fontsiz round-trip. */
    win->linespace = (int)(win->fontsz + 0.5);
    CGGlyph  glyph;
    UniChar  ch = 'M';
    CTFontGetGlyphsForCharacters(cf, &ch, &glyph, 1);
    CGSize   adv;
    CTFontGetAdvancesForGlyphs(cf, kCTFontOrientationDefault,
                               &glyph, &adv, 1);
    win->charspace = (int)(adv.width + 0.5);
    if (win->charspace <= 0) win->charspace = win->linespace / 2;
    win->maxx = win->maxxg / win->charspace;
    win->maxy = win->maxyg / win->linespace;
    if (win->maxx < 1) win->maxx = 1;
    if (win->maxy < 1) win->maxy = 1;
}

void ami_font(FILE* f, long fc)
{
    winptr win = f2win(f); if (!win) return;
    fontptr fp = fntlst;
    int i = 1;
    while (fp && i < fc) { fp = fp->next; i++; }
    if (fp) {
        curscn(win)->font = fp;
        win->cfont        = fp;
        /* rebuild at current window font size */
        if (fp->name && fp->size != win->fontsz) {
            if (fp->ctfont) CFRelease(fp->ctfont);
            fp->ctfont = make_ctfont(fp->name, win->fontsz,
                                     curscn(win)->bold, curscn(win)->italic);
            fp->size = win->fontsz;
        }
        update_metrics(win);
    }
}

void ami_fontnam(FILE* f, long fc, char* fns, long fnsl)
{
    fontptr fp = fntlst;
    int i = 1;
    while (fp && i < fc) { fp = fp->next; i++; }
    if (fp && fp->name) {
        strncpy(fns, fp->name, fnsl-1);
        fns[fnsl-1] = 0;
    } else if (fnsl > 0) fns[0] = 0;
}

void ami_fontsiz(FILE* f, long s)
{
    winptr win = f2win(f); if (!win) return;
    win->fontsz = (CGFloat)(s > 0 ? s : DEF_FONT_H);
    win->gfpoint = win->fontsz * 2835.0f / (float)win->dpmy;
    /* rebuild current font at new size */
    scnptr sc = curscn(win);
    if (sc->font && sc->font->name) {
        if (sc->font->ctfont) CFRelease(sc->font->ctfont);
        sc->font->ctfont = make_ctfont(sc->font->name, win->fontsz,
                                       sc->bold, sc->italic);
        sc->font->size   = win->fontsz;
        update_metrics(win);
    }
}

void ami_chrspcy(FILE* f, long s)  { /* stub */ }
void ami_chrspcx(FILE* f, long s)  { /* stub */ }

long ami_dpmx(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->dpmx : 3780;
}

long ami_dpmy(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->dpmy : 3780;
}

long ami_strsiz(FILE* f, const char* s)
{
    winptr win = f2win(f); if (!win || !s) return 0;
    scnptr sc  = curscn(win);
    return (long)measure_string(sc->font ? sc->font : win->cfont,
                                s, (int)strlen(s));
}

long ami_chrpos(FILE* f, const char* s, long p)
{
    winptr win = f2win(f); if (!win || !s || p <= 0) return 0;
    scnptr sc  = curscn(win);
    int len = (int)strlen(s);
    if (p > len) p = len;
    return (long)measure_string(sc->font ? sc->font : win->cfont, s, (int)p);
}

#define MINJST 1 /* minimum pixels for space in justification */

/* Compute justification spacing parameters.
 * Returns: spc = pixels per space, ss = total space budget remaining. */
static void just_params(winptr win, const char* s, long n,
                        long* out_spc, long* out_ss)
{
    fontptr fp = curscn(win)->font ? curscn(win)->font : win->cfont;
    int l = (int)strlen(s);
    long sz = 0; /* critical size: chars + min space */
    long ns = 0; /* number of spaces */
    long cs = 0; /* total character width (no spaces) */
    for (int i = 0; i < l; i++) {
        if (s[i] == ' ') { sz += MINJST; ns++; }
        else {
            int cw = (int)(measure_string(fp, &s[i], 1) + 0.5);
            sz += cw;
            cs += cw;
        }
    }
    long spc = MINJST;
    long ss = ns * MINJST;
    if (ns > 0 && n > sz) { spc = (n - cs) / ns; ss = n - cs; }
    *out_spc = spc;
    *out_ss  = ss;
}

void ami_writejust(FILE* f, const char* s, long n)
{
    winptr win = f2win(f); if (!win || !s) return;
    scnptr sc  = curscn(win);
    fontptr fp = sc->font ? sc->font : win->cfont;
    int l = (int)strlen(s);

    long spc, ss;
    just_params(win, s, n, &spc, &ss);

    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;

    for (int i = 0; i < l; i++) {
        if (s[i] == ' ') {
            /* advance by justified space width */
            long cbs = (spc > ss) ? ss : spc;
            /* draw background for space */
            if (sc->bmod != mdinvis) {
                CGContextSetBlendMode(ctx, mode2blend(sc->bmod));
                CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
                CGContextFillRect(ctx, CGRectMake(sc->curxg - 1, sc->curyg - 1,
                                                  cbs, win->linespace));
                CGContextSetBlendMode(ctx, mode2blend(sc->fmod));
            }
            if (spc > ss) sc->curxg += ss;
            else { sc->curxg += spc; ss -= spc; }
        } else {
            /* draw character normally */
            int cw = draw_char_at(win, s[i]);
            sc->curxg += cw;
            sc->curx = (sc->curxg - 1) / win->charspace + 1;
        }
    }
    pa_cocoa_flush(win->han);
}

long ami_justpos(FILE* f, const char* s, long p, long n)
{
    winptr win = f2win(f); if (!win || !s) return 0;
    fontptr fp = curscn(win)->font ? curscn(win)->font : win->cfont;
    int l = (int)strlen(s);
    if (p < 0 || p >= l) return 0;

    long spc, ss;
    just_params(win, s, n, &spc, &ss);

    long cp = 0;  /* accumulated pixel position */
    long crp = 0; /* result position */
    for (int i = 0; i < l; i++) {
        if (i == p) crp = cp;
        if (s[i] == ' ') {
            if (spc > ss) cp += ss;
            else { cp += spc; ss -= spc; }
        } else {
            cp += (int)(measure_string(fp, &s[i], 1) + 0.5);
        }
    }
    return crp;
}

void ami_condensed(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->condensed = e;
    curscn(win)->extended = 0;
    rebuild_font(win);
}

void ami_extended(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->extended = e;
    curscn(win)->condensed = 0;
    rebuild_font(win);
}

void ami_xlight(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->xlight = e;
    /* xlight uses a lighter weight — approximated by alpha */
}

void ami_light(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->light = e;
}

void ami_xbold(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->xbold = e;
    rebuild_font(win);
}

void ami_hollow(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->hollow = e;
}

void ami_raised(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->raised = e;
}
void ami_settabg(FILE* f, long t)    { /* stub */ }
void ami_restabg(FILE* f, long t)    { /* stub */ }

long ami_baseline(FILE* f)
{
    winptr win = f2win(f); if (!win) return 0;
    scnptr sc  = curscn(win);
    fontptr fp = sc->font ? sc->font : win->cfont;
    if (!fp || !fp->ctfont) return 0;
    return (long)CTFontGetAscent(fp->ctfont);
}

void ami_fcolorg(FILE* f, long r, long g, long b)
{
    winptr win = f2win(f); if (!win) return;
    pa_rgba c = { r/(double)LONG_MAX, g/(double)LONG_MAX, b/(double)LONG_MAX, 1.0 };
    curscn(win)->fc = c;
}

void ami_fcolorc(FILE* f, long r, long g, long b) { ami_fcolorg(f, r, g, b); }

void ami_bcolorg(FILE* f, long r, long g, long b)
{
    winptr win = f2win(f); if (!win) return;
    pa_rgba c = { r/(double)LONG_MAX, g/(double)LONG_MAX, b/(double)LONG_MAX, 1.0 };
    curscn(win)->bc = c;
    if (win->han) pa_cocoa_set_background(win->han, c.r, c.g, c.b);
}

void ami_bcolorc(FILE* f, long r, long g, long b) { ami_bcolorg(f, r, g, b); }

/* Picture storage */
static CGImageRef pictbl[MAXPIC]; /* loaded pictures, 1-based index */

/* Set or overwrite extension on a filename */
static void setext(char* fn, const char* ext)
{
    char* p = strrchr(fn, '.');
    char* s = strrchr(fn, '/');
    if (p && (!s || p > s)) *p = 0; /* remove existing extension */
    strcat(fn, ext);
}

void ami_loadpict(FILE* f, long p, char* fn)
{
    winptr win = f2win(f); if (!win) return;
    if (p < 1 || p > MAXPIC) return;

    /* delete any existing picture in this slot */
    if (pictbl[p-1]) { CGImageRelease(pictbl[p-1]); pictbl[p-1] = NULL; }

    /* copy filename and add .bmp extension if needed */
    char fnh[512];
    strncpy(fnh, fn, sizeof(fnh)-5);
    fnh[sizeof(fnh)-5] = 0;
    setext(fnh, ".bmp");

    /* read file into memory */
    FILE* pf = fopen(fnh, "rb");
    if (!pf) return;
    fseek(pf, 0, SEEK_END);
    long sz = ftell(pf);
    fseek(pf, 0, SEEK_SET);
    uint8_t* buf = malloc(sz);
    if (!buf) { fclose(pf); return; }
    fread(buf, 1, sz, pf);
    fclose(pf);

    /* create CGImage via ImageIO (handles BMP natively) */
    CFDataRef data = CFDataCreate(NULL, buf, sz);
    free(buf);
    if (!data) return;

    CGImageSourceRef src = CGImageSourceCreateWithData(data, NULL);
    CFRelease(data);
    if (!src) return;

    pictbl[p-1] = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
}

long ami_pictsizx(FILE* f, long p)
{
    if (p < 1 || p > MAXPIC || !pictbl[p-1]) return 0;
    return (long)CGImageGetWidth(pictbl[p-1]);
}

long ami_pictsizy(FILE* f, long p)
{
    if (p < 1 || p > MAXPIC || !pictbl[p-1]) return 0;
    return (long)CGImageGetHeight(pictbl[p-1]);
}

void ami_picture(FILE* f, long p, long x1, long y1, long x2, long y2)
{
    winptr win = f2win(f); if (!win) return;
    if (p < 1 || p > MAXPIC || !pictbl[p-1]) return;
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;

    /* draw the image scaled to fit the bounding box */
    CGFloat dx = PX(x1), dy = PY(y1);
    CGFloat dw = x2 - x1 + 1, dh = y2 - y1 + 1;

    /* CGContextDrawImage draws in CG's Y-up coords, but our context is
     * flipped (Y-down). Save state, flip locally, draw, restore. */
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, dx, dy + dh);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextDrawImage(ctx, CGRectMake(0, 0, dw, dh), pictbl[p-1]);
    CGContextRestoreGState(ctx);

    pa_cocoa_flush(win->han);
}

void ami_delpict(FILE* f, long p)
{
    if (p < 1 || p > MAXPIC) return;
    if (pictbl[p-1]) { CGImageRelease(pictbl[p-1]); pictbl[p-1] = NULL; }
}
void ami_scrollg(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;
    scnptr sc = curscn(win);
    long w = win->maxxg;
    long h = win->maxyg;

    if (x == 0 && y == 0) return;

    /* if scroll exceeds screen, just clear */
    if (labs(x) >= w || labs(y) >= h) {
        clear_window(win);
        pa_cocoa_flush(win->han);
        return;
    }

    /* All operations use raw pixel data to avoid mixing memmove with CG calls
       (which is undefined per Apple docs for bitmap contexts). */
    size_t   pw       = CGBitmapContextGetWidth(ctx);
    size_t   ph       = CGBitmapContextGetHeight(ctx);
    size_t   rowbytes = CGBitmapContextGetBytesPerRow(ctx);
    uint8_t* data     = (uint8_t*)CGBitmapContextGetData(ctx);
    if (!data) return;

    float scale = (w > 0) ? (float)pw / w : 1.0f;
    int ppx = (int)(labs(x) * scale + 0.5f);
    int ppy = (int)(labs(y) * scale + 0.5f);
    int bpp = (int)(rowbytes / pw);

    /* build fill pixel in bitmap format (ARGB premultiplied, host byte order) */
    uint32_t fr = (uint32_t)(sc->bc.r * 255 + 0.5);
    uint32_t fg = (uint32_t)(sc->bc.g * 255 + 0.5);
    uint32_t fb = (uint32_t)(sc->bc.b * 255 + 0.5);
    uint32_t fillpx = (0xFFu << 24) | (fr << 16) | (fg << 8) | fb;

    /* shift rows vertically */
    if (y > 0) {
        memmove(data, data + ppy * rowbytes, (ph - ppy) * rowbytes);
    } else if (y < 0) {
        memmove(data + ppy * rowbytes, data, (ph - ppy) * rowbytes);
    }

    /* shift columns horizontally */
    if (x != 0) {
        for (size_t row = 0; row < ph; row++) {
            uint8_t* rp = data + row * rowbytes;
            if (x > 0) {
                memmove(rp, rp + ppx * bpp, (pw - ppx) * bpp);
            } else {
                memmove(rp + ppx * bpp, rp, (pw - ppx) * bpp);
            }
        }
    }

    /* fill vacated strips with background color using raw pixels */
    /* vertical vacated strip */
    if (y > 0) {
        /* bottom strip: rows ph-ppy .. ph-1 */
        for (size_t row = ph - ppy; row < ph; row++) {
            uint32_t* rp = (uint32_t*)(data + row * rowbytes);
            for (size_t col = 0; col < pw; col++) rp[col] = fillpx;
        }
    } else if (y < 0) {
        /* top strip: rows 0 .. ppy-1 */
        for (size_t row = 0; row < (size_t)ppy; row++) {
            uint32_t* rp = (uint32_t*)(data + row * rowbytes);
            for (size_t col = 0; col < pw; col++) rp[col] = fillpx;
        }
    }
    /* horizontal vacated strip */
    if (x > 0) {
        /* right strip: cols pw-ppx .. pw-1 */
        for (size_t row = 0; row < ph; row++) {
            uint32_t* rp = (uint32_t*)(data + row * rowbytes);
            for (size_t col = pw - ppx; col < pw; col++) rp[col] = fillpx;
        }
    } else if (x < 0) {
        /* left strip: cols 0 .. ppx-1 */
        for (size_t row = 0; row < ph; row++) {
            uint32_t* rp = (uint32_t*)(data + row * rowbytes);
            for (size_t col = 0; col < (size_t)ppx; col++) rp[col] = fillpx;
        }
    }

    pa_cocoa_flush(win->han);
}
void ami_path(FILE* f, long a)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->textpath = a;
}

/*******************************************************************************
*                                                                              *
*                        PA API — window management                            *
*                                                                              *
*******************************************************************************/

void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, long wid)
{
    if (!inited) pa_graphics_init();

    /* Open /dev/null to obtain real file descriptors.  The fd numbers index
     * into opnfil[]/wintbl[], so iwrite() can route writes to plcchr(). */
    FILE* inf  = fopen("/dev/null", "r");
    FILE* outf = fopen("/dev/null", "w");
    if (!inf || !outf) { if (inf) fclose(inf); if (outf) fclose(outf); return; }
    setvbuf(inf,  NULL, _IONBF, 0);
    setvbuf(outf, NULL, _IONBF, 0);

    int ifn = fileno(inf);
    int ofn = fileno(outf);
    if (ifn < 0 || ofn < 0 || ofn >= MAXFIL) {
        fclose(inf); fclose(outf); return;
    }

    /* create the Cocoa window: with a parent it becomes an embedded child
       view clipped to the parent; otherwise a top-level window cascaded
       from the main window */
    int wx = 100 + (wid - 1) * 30;
    int wy = 100 + (wid - 1) * 30;
    pa_winhan han;
    winptr pwin = parent ? f2win(parent) : NULL;
    if (pwin && pwin->han)
        han = pa_cocoa_create_child_window(pwin->han, wx - 100, wy - 100,
                                           maxxd, maxyd);
    else
        han = pa_cocoa_create_window(wx, wy, maxxd, maxyd, "");
    if (!han) { fclose(inf); fclose(outf); return; }

    winptr win = &wintbl[ofn];
    win_init(win, wid, parent ? fileno(parent) : 0, maxxd, maxyd);
    win->han     = han;
    win->infile  = inf;
    win->outfile = outf;
    opnfil[ofn]  = 1;
    pa_cocoa_set_bufmod(han, win->bufmod);
    pa_cocoa_set_background(han, 1.0f, 1.0f, 1.0f);

    pa_cocoa_show_window(han);
    clear_window(win);
    pa_cocoa_flush(han);

    *infile  = inf;
    *outfile = outf;
}

void ami_buffer(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    win->bufmod = e;
    if (win->han) pa_cocoa_set_bufmod(win->han, e);
}

/* Standard character cell for desktop (parentless) windows. The desktop
   has no character grid, so window sizes/positions in character terms use
   this made-up cell, matching the Windows/Linux implementations. */
#define STDCHRX 8
#define STDCHRY 12

/* get parent window record, or NULL if window has no parent */
static winptr parwin(winptr win)
{
    if (win->parwid > 0 && win->parwid < MAXFIL && opnfil[win->parwid])
        return &wintbl[win->parwid];
    return NULL;
}

/* chrome extra size (window minus client) for the window's current state */
static void curextra(winptr win, int* dw, int* dh)
{
    pa_cocoa_frame_extra(win->han, win->frame, win->sizable, win->sysbar,
                         dw, dh);
}

/* window sizes are OUTER dimensions, frame included, per PA semantics */

void ami_getsizg(FILE* f, long* x, long* y)
{
    winptr win = f2win(f);
    /* the actual on-screen window size, not the buffer size: the drawing
       surface is independent of the window in buffered mode */
    if (win) {
        int w, h; /* shim boundary: pixel sizes pass as int */
        pa_cocoa_get_window_size(win->han, &w, &h);
        *x = w; *y = h;
    } else { *x = maxxd; *y = maxyd; }
}

void ami_getsiz(FILE* f, long* x, long* y)
{
    winptr win = f2win(f);
    if (!win) { *x = 80; *y = 25; return; }
    ami_getsizg(f, x, y);
    winptr par = parwin(win);
    if (par) { *x = (*x - 1) / par->charspace + 1;
               *y = (*y - 1) / par->linespace + 1; }
    else     { *x = (*x - 1) / STDCHRX + 1;
               *y = (*y - 1) / STDCHRY + 1; }
}

void ami_setsizg(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    int dw, dh;
    curextra(win, &dw, &dh);
    int cw = x - dw; if (cw < 1) cw = 1;
    int ch = y - dh; if (ch < 1) ch = 1;
    pa_cocoa_resize_window(win->han, cw, ch);
    /* the drawing surface (maxxg/maxyg) belongs to the buffer, sized by
       ami_sizbuf; only an unbuffered window's surface tracks the window */
    if (!win->bufmod) {
        win->maxxg = cw;
        win->maxyg = ch;
        win->maxx  = cw / win->charspace;
        win->maxy  = ch / win->linespace;
        pa_cocoa_resize_bitmap(win->han, cw, ch);
        scnptr sc = curscn(win);
        sc->lwidth = 1.0;
        CGContextRef ctx = pa_cocoa_get_context(win->han);
        if (ctx) CGContextSetLineWidth(ctx, 1.0);
    }
}

void ami_setsiz(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    winptr par = parwin(win);
    if (par) ami_setsizg(f, x * par->charspace, y * par->linespace);
    else     ami_setsizg(f, x * STDCHRX, y * STDCHRY);
}

void ami_setposg(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    /* pa_cocoa_move_window positions a child relative to the parent's
       top-left and a top-level window on the screen */
    pa_cocoa_move_window(win->han, x - 1, y - 1);
}

void ami_setpos(FILE* f, long x, long y)
{
    winptr win = f2win(f); if (!win) return;
    winptr par = parwin(win);
    /* character positions scale by the parent's character cell; a window
       with no parent uses the standard desktop character cell */
    if (par) ami_setposg(f, (x - 1) * par->charspace + 1,
                            (y - 1) * par->linespace + 1);
    else     ami_setposg(f, (x - 1) * STDCHRX + 1,
                            (y - 1) * STDCHRY + 1);
}

void ami_scnsiz(FILE* f, long* x, long* y)
{
    winptr win = f2win(f);
    int sw = pa_cocoa_screen_w();
    int sh = pa_cocoa_screen_h();
    *x = win ? sw / win->charspace : 80;
    *y = win ? sh / win->linespace : 25;
}

void ami_scnsizg(FILE* f, long* x, long* y)
{
    *x = pa_cocoa_screen_w();
    *y = pa_cocoa_screen_h();
}

void ami_scncen(FILE* f, long* x, long* y)
{
    winptr win = f2win(f);
    int sw = pa_cocoa_screen_w();
    int sh = pa_cocoa_screen_h();
    *x = win ? sw / win->charspace / 2 : 40;
    *y = win ? sh / win->linespace / 2 : 12;
}

void ami_scnceng(FILE* f, long* x, long* y)
{
    *x = pa_cocoa_screen_w() / 2;
    *y = pa_cocoa_screen_h() / 2;
}

void ami_winclientg(FILE* f, long cx, long cy, long* wx, long* wy, ami_winmodset ms)
{
    winptr win = f2win(f);
    if (!win) { *wx = cx; *wy = cy; return; }
    int dw, dh;
    pa_cocoa_frame_extra(win->han,
                         (ms & BIT(ami_wmframe))  != 0,
                         (ms & BIT(ami_wmsize))   != 0,
                         (ms & BIT(ami_wmsysbar)) != 0,
                         &dw, &dh);
    *wx = cx + dw;
    *wy = cy + dh;
}

void ami_winclient(FILE* f, long cx, long cy, long* wx, long* wy, ami_winmodset ms)
{
    winptr win = f2win(f);
    if (!win) { *wx = cx; *wy = cy; return; }
    /* client size in this window's characters -> outer size in the parent's
       characters (or the standard desktop cell for parentless windows) */
    ami_winclientg(f, cx * win->charspace, cy * win->linespace, wx, wy, ms);
    winptr par = parwin(win);
    if (par) { *wx = (*wx - 1) / par->charspace + 1;
               *wy = (*wy - 1) / par->linespace + 1; }
    else     { *wx = (*wx - 1) / STDCHRX + 1;
               *wy = (*wy - 1) / STDCHRY + 1; }
}

void ami_front(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_front(win->han);
}

void ami_back(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_back(win->han);
}

void ami_frame(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    win->frame = e;
    pa_cocoa_set_frame(win->han, e);
}

void ami_sizable(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    win->sizable = e;
    pa_cocoa_set_sizable(win->han, e);
}

void ami_sysbar(FILE* f, long e)
{
    winptr win = f2win(f); if (!win) return;
    win->sysbar = e;
    pa_cocoa_set_sysbar(win->han, e);
}
void ami_menu(FILE* f, ami_menuptr m)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_menu(win->han, (void*)m);
}

void ami_menuena(FILE* f, long id, long onoff)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_menu_enable(win->han, id, onoff);
}

void ami_menusel(FILE* f, long id, long select)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_menu_check(win->han, id, select);
}

static void stdmenu_append(ami_menuptr* list, ami_menuptr m)
{
    m->next = NULL;
    if (!*list) { *list = m; return; }
    ami_menuptr lp = *list;
    while (lp->next) lp = lp->next;
    lp->next = m;
}

static void stdmenu_new(ami_menuptr* m, int id, const char* face)
{
    *m = malloc(sizeof(ami_menurec));
    (*m)->next   = NULL;
    (*m)->branch = NULL;
    (*m)->onoff  = FALSE;
    (*m)->oneof  = FALSE;
    (*m)->bar    = FALSE;
    (*m)->id     = id;
    (*m)->face   = strdup(face);
}

static void stdmenu_additem(ami_stdmenusel sms, int i, ami_menuptr* m,
                            ami_menuptr* list, const char* face, int bar)
{
    if (BIT(i) & sms) {
        stdmenu_new(m, i, face);
        stdmenu_append(list, *m);
        (*m)->bar = bar;
    }
}

void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)
{
    ami_menuptr m, hm;

    *sm = NULL;

    if (sms & (BIT(AMI_SMNEW) | BIT(AMI_SMOPEN) | BIT(AMI_SMCLOSE) |
               BIT(AMI_SMSAVE) | BIT(AMI_SMSAVEAS) | BIT(AMI_SMPAGESET) |
               BIT(AMI_SMPRINT) | BIT(AMI_SMEXIT))) {
        stdmenu_new(&hm, 0, "File");
        stdmenu_append(sm, hm);
        stdmenu_additem(sms, AMI_SMNEW, &m, &hm->branch, "New", FALSE);
        stdmenu_additem(sms, AMI_SMOPEN, &m, &hm->branch, "Open", FALSE);
        stdmenu_additem(sms, AMI_SMCLOSE, &m, &hm->branch, "Close", FALSE);
        stdmenu_additem(sms, AMI_SMSAVE, &m, &hm->branch, "Save", FALSE);
        stdmenu_additem(sms, AMI_SMSAVEAS, &m, &hm->branch, "Save As", TRUE);
        stdmenu_additem(sms, AMI_SMPAGESET, &m, &hm->branch, "Page Setup", FALSE);
        stdmenu_additem(sms, AMI_SMPRINT, &m, &hm->branch, "Print", TRUE);
        stdmenu_additem(sms, AMI_SMEXIT, &m, &hm->branch, "Exit", FALSE);
    }

    if (sms & (BIT(AMI_SMUNDO) | BIT(AMI_SMCUT) | BIT(AMI_SMPASTE) |
               BIT(AMI_SMDELETE) | BIT(AMI_SMFIND) | BIT(AMI_SMFINDNEXT) |
               BIT(AMI_SMREPLACE) | BIT(AMI_SMGOTO) | BIT(AMI_SMSELECTALL))) {
        stdmenu_new(&hm, 0, "Edit");
        stdmenu_append(sm, hm);
        stdmenu_additem(sms, AMI_SMUNDO, &m, &hm->branch, "Undo", TRUE);
        stdmenu_additem(sms, AMI_SMCUT, &m, &hm->branch, "Cut", FALSE);
        stdmenu_additem(sms, AMI_SMPASTE, &m, &hm->branch, "Paste", FALSE);
        stdmenu_additem(sms, AMI_SMDELETE, &m, &hm->branch, "Delete", TRUE);
        stdmenu_additem(sms, AMI_SMFIND, &m, &hm->branch, "Find", FALSE);
        stdmenu_additem(sms, AMI_SMFINDNEXT, &m, &hm->branch, "Find Next", FALSE);
        stdmenu_additem(sms, AMI_SMREPLACE, &m, &hm->branch, "Replace", FALSE);
        stdmenu_additem(sms, AMI_SMGOTO, &m, &hm->branch, "Goto", TRUE);
        stdmenu_additem(sms, AMI_SMSELECTALL, &m, &hm->branch, "Select All", FALSE);
    }

    while (pm) {
        m = pm;
        pm = pm->next;
        stdmenu_append(sm, m);
    }

    if (sms & (BIT(AMI_SMNEWWINDOW) | BIT(AMI_SMTILEHORIZ) | BIT(AMI_SMTILEVERT) |
               BIT(AMI_SMCASCADE) | BIT(AMI_SMCLOSEALL))) {
        stdmenu_new(&hm, 0, "Window");
        stdmenu_append(sm, hm);
        stdmenu_additem(sms, AMI_SMNEWWINDOW, &m, &hm->branch, "New Window", TRUE);
        stdmenu_additem(sms, AMI_SMTILEHORIZ, &m, &hm->branch, "Tile Horizontally", FALSE);
        stdmenu_additem(sms, AMI_SMTILEVERT, &m, &hm->branch, "Tile Vertically", FALSE);
        stdmenu_additem(sms, AMI_SMCASCADE, &m, &hm->branch, "Cascade", TRUE);
        stdmenu_additem(sms, AMI_SMCLOSEALL, &m, &hm->branch, "Close All", FALSE);
    }

    if (sms & (BIT(AMI_SMHELPTOPIC) | BIT(AMI_SMABOUT))) {
        stdmenu_new(&hm, 0, "Help");
        stdmenu_append(sm, hm);
        stdmenu_additem(sms, AMI_SMHELPTOPIC, &m, &hm->branch, "Help Topics", TRUE);
        stdmenu_additem(sms, AMI_SMABOUT, &m, &hm->branch, "About", FALSE);
    }
}

long ami_getwinid(void)
{
    static long next = 1;
    return next++;
}

void ami_focus(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_focus(win->han);
}

/* Return the window handle for stdout (used by screen_capture) */
pa_winhan pa_stdout_winhan(void)
{
    if (opnfil[1] && wintbl[1].han) return wintbl[1].han;
    return NULL;
}

/*******************************************************************************
*                                                                              *
*                        PA API — event handling                               *
*                                                                              *
*******************************************************************************/

void ami_event(FILE* f, ami_evtrec* er)
{
    pa_rawevent raw;
    pa_cocoa_process_ns_events();
    pa_cocoa_wait(&raw);
    translate_event(&raw, er);
    if (er->etype == ami_etterm) fend = TRUE;
}

void ami_timer(FILE* f, long i, long t, long r)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_set_timer(win->han, i-1, t, r); /* PA timers 1-based */
}

void ami_killtimer(FILE* f, long i)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_kill_timer(win->han, i-1);
}

long ami_mouse(FILE* f)        { return 1; }  /* one mouse */
long ami_mousebutton(FILE* f, long m) { return 3; }  /* three buttons */
long ami_joystick(FILE* f)         { return pa_cocoa_joy_count(); }
long ami_joybutton(FILE* f, long j) { return pa_cocoa_joy_buttons(j); }
long ami_joyaxis(FILE* f, long j)   { return pa_cocoa_joy_axes(j); }

/*******************************************************************************
*                                                                              *
*                        PA API — widgets                                      *
*                                                                              *
*******************************************************************************/

long ami_getwigid(FILE* f)
{
    static long next = 1;
    return next++;
}

void ami_killwidget(FILE* f, long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_kill_widget(win->han, id);
}

void ami_selectwidget(FILE* f, long id, long e)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_widget_select(win->han, id, e);
}

void ami_enablewidget(FILE* f, long id, long e)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_widget_enable(win->han, id, e);
}

void ami_getwidgettext(FILE* f, long id, char* s, long sl)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_widget_get_text(win->han, id, s, sl);
}

void ami_putwidgettext(FILE* f, long id, char* s)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_widget_text(win->han, id, s);
}

void ami_sizwidget(FILE* f, long id, long x, long y)   { /* stub */ }
void ami_sizwidgetg(FILE* f, long id, long x, long y)  { /* stub */ }
void ami_poswidget(FILE* f, long id, long x, long y)   { /* stub */ }
void ami_poswidgetg(FILE* f, long id, long x, long y)  { /* stub */ }
void ami_backwidget(FILE* f, long id)                { /* stub */ }
void ami_frontwidget(FILE* f, long id)               { /* stub */ }
void ami_focuswidget(FILE* f, long id)               { /* stub */ }

/* The plain widget calls take CHARACTER coordinates/sizes; the -g calls
   take pixels. Character rects convert to pixel rects covering the full
   character cells (Windows semantics), and pixel sizes convert to
   character sizes rounding up. */

static void chr2grrect(winptr win, long* x1, long* y1, long* x2, long* y2)
{
    *x1 = (*x1 - 1) * win->charspace + 1;
    *y1 = (*y1 - 1) * win->linespace + 1;
    *x2 = (*x2) * win->charspace;
    *y2 = (*y2) * win->linespace;
}

static void gr2chrsiz(winptr win, long* w, long* h)
{
    *w = (*w - 1) / win->charspace + 1;
    *h = (*h - 1) / win->linespace + 1;
}

void ami_buttonsizg(FILE* f, char* s, long* w, long* h)
{
    *w = ami_strsiz(f, s) + 20;
    *h = ami_chrsizy(f) + 8;
}

void ami_buttonsiz(FILE* f, char* s, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_buttonsizg(f, s, w, h);
    gr2chrsiz(win, w, h);
}

void ami_buttong(FILE* f, long x1, long y1, long x2, long y2, char* s, long id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_button(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

void ami_button(FILE* f, long x1, long y1, long x2, long y2, char* s, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_buttong(f, x1, y1, x2, y2, s, id);
}

void ami_checkboxsizg(FILE* f, char* s, long* w, long* h)
{
    *w = ami_strsiz(f, s) + 24;
    *h = ami_chrsizy(f) + 4;
}

void ami_checkboxsiz(FILE* f, char* s, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_checkboxsizg(f, s, w, h);
    gr2chrsiz(win, w, h);
}

void ami_checkboxg(FILE* f, long x1, long y1, long x2, long y2, char* s, long id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_checkbox(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

void ami_checkbox(FILE* f, long x1, long y1, long x2, long y2, char* s, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_checkboxg(f, x1, y1, x2, y2, s, id);
}

void ami_radiobuttonsizg(FILE* f, char* s, long* w, long* h)
{
    *w = ami_strsiz(f, s) + 24;
    *h = ami_chrsizy(f) + 4;
}

void ami_radiobuttonsiz(FILE* f, char* s, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_radiobuttonsizg(f, s, w, h);
    gr2chrsiz(win, w, h);
}

void ami_radiobuttong(FILE* f, long x1, long y1, long x2, long y2, char* s, long id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_radiobutton(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

void ami_radiobutton(FILE* f, long x1, long y1, long x2, long y2, char* s, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_radiobuttong(f, x1, y1, x2, y2, s, id);
}

void ami_groupsizg(FILE* f, char* s, long cw, long ch, long* w, long* h, long* ox, long* oy)
{
    /* the group must be at least wide enough for its title as NSBox
       renders it, and tall enough for the title plus the client area */
    int tw, th;
    pa_cocoa_group_title_size(s, &tw, &th);
    *w = tw + 7 * 2;
    if (cw + 7 * 2 > *w) *w = cw + 7 * 2;
    *h  = th + ch + 5 * 2;
    *ox = 5;
    *oy = th;
}

void ami_groupsiz(FILE* f, char* s, long cw, long ch, long* w, long* h, long* ox, long* oy)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; *ox = *oy = 0; return; }
    ami_groupsizg(f, s, cw * win->charspace, ch * win->linespace, w, h, ox, oy);
    gr2chrsiz(win, w, h);
    gr2chrsiz(win, ox, oy);
}

void ami_groupg(FILE* f, long x1, long y1, long x2, long y2, char* s, long id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_group(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

void ami_group(FILE* f, long x1, long y1, long x2, long y2, char* s, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_groupg(f, x1, y1, x2, y2, s, id);
}

void ami_backgroundg(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_background(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

void ami_background(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_backgroundg(f, x1, y1, x2, y2, id);
}

void ami_scrollvertsizg(FILE* f, long* w, long* h)  { *w = 16; *h = 100; }

void ami_scrollvertsiz(FILE* f, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_scrollvertsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

void ami_scrollvertg(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollvert(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

void ami_scrollvert(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_scrollvertg(f, x1, y1, x2, y2, id);
}

void ami_scrollhorizsizg(FILE* f, long* w, long* h)  { *w = 100; *h = 16; }

void ami_scrollhorizsiz(FILE* f, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_scrollhorizsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

void ami_scrollhorizg(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollhoriz(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

void ami_scrollhoriz(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_scrollhorizg(f, x1, y1, x2, y2, id);
}

void ami_scrollpos(FILE* f, long id, long r)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollbar_pos(win->han, id, r);
}

void ami_scrollsiz(FILE* f, long id, long r)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollbar_siz(win->han, id, r);
}

void ami_numselboxsizg(FILE* f, long l, long u, long* w, long* h) { *w = 80; *h = 24; }

void ami_numselboxsiz(FILE* f, long l, long u, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_numselboxsizg(f, l, u, w, h);
    gr2chrsiz(win, w, h);
}
void ami_numselboxg(FILE* f, long x1, long y1, long x2, long y2, long l, long u, long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_numselbox(win->han, x1, y1, x2-x1+1, y2-y1+1, l, u, id);
}

void ami_numselbox(FILE* f, long x1, long y1, long x2, long y2, long l, long u, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_numselboxg(f, x1, y1, x2, y2, l, u, id);
}

void ami_editboxsizg(FILE* f, char* s, long* w, long* h)
{ *w = ami_strsiz(f, s) + 20; *h = ami_chrsizy(f) + 8; }

void ami_editboxsiz(FILE* f, char* s, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_editboxsizg(f, s, w, h);
    gr2chrsiz(win, w, h);
}

void ami_editboxg(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_editbox(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

void ami_editbox(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_editboxg(f, x1, y1, x2, y2, id);
}

void ami_progbarsizg(FILE* f, long* w, long* h) { *w = 200; *h = 20; }

void ami_progbarsiz(FILE* f, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_progbarsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

void ami_progbarg(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_progressbar(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

void ami_progbar(FILE* f, long x1, long y1, long x2, long y2, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_progbarg(f, x1, y1, x2, y2, id);
}

void ami_progbarpos(FILE* f, long id, long pos)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_progressbar_pos(win->han, id, pos);
}

void ami_listboxsizg(FILE* f, ami_strptr sp, long* w, long* h)
{
    /* wide enough for the longest entry plus scroller and border, tall
       enough for all entries at the native ~18px row height */
    long mw = 0;
    int  n  = 0;
    for (ami_strptr p = sp; p; p = p->next) {
        long sw = ami_strsiz(f, p->str);
        if (sw > mw) mw = sw;
        n++;
    }
    *w = mw + 40;
    *h = n * 18 + 6;
}

void ami_listboxsiz(FILE* f, ami_strptr sp, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_listboxsizg(f, sp, w, h);
    gr2chrsiz(win, w, h);
}

void ami_listboxg(FILE* f, long x1, long y1, long x2, long y2, ami_strptr sp, long id)
{
    winptr win = f2win(f); if (!win) return;
    int n = 0;
    for (ami_strptr p = sp; p; p = p->next) n++;
    if (!n) return;
    const char** strs = malloc(n * sizeof(char*));
    if (!strs) return;
    int i = 0;
    for (ami_strptr p = sp; p; p = p->next) strs[i++] = p->str;
    pa_cocoa_listbox(win->han, x1, y1, x2-x1+1, y2-y1+1, strs, n, id);
    free(strs);
}

void ami_listbox(FILE* f, long x1, long y1, long x2, long y2, ami_strptr sp, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_listboxg(f, x1, y1, x2, y2, sp, id);
}

void ami_dropboxsizg(FILE* f, ami_strptr sp, long* cw, long* ch, long* ow, long* oh)
{ *cw = 150; *ch = 24; *ow = 150; *oh = 100; }

void ami_dropboxsiz(FILE* f, ami_strptr sp, long* cw, long* ch, long* ow, long* oh)
{
    winptr win = f2win(f); if (!win) { *cw = *ch = *ow = *oh = 1; return; }
    ami_dropboxsizg(f, sp, cw, ch, ow, oh);
    gr2chrsiz(win, cw, ch);
    gr2chrsiz(win, ow, oh);
}

void ami_dropboxg(FILE* f, long x1, long y1, long x2, long y2, ami_strptr sp, long id)
{
    winptr win = f2win(f); if (!win) return;
    int n = 0;
    for (ami_strptr p = sp; p; p = p->next) n++;
    if (!n) return;
    const char** strs = malloc(n * sizeof(char*));
    if (!strs) return;
    int i = 0;
    for (ami_strptr p = sp; p; p = p->next) strs[i++] = p->str;
    pa_cocoa_dropbox(win->han, x1, y1, x2-x1+1, y2-y1+1, strs, n, id);
    free(strs);
}

void ami_dropbox(FILE* f, long x1, long y1, long x2, long y2, ami_strptr sp, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_dropboxg(f, x1, y1, x2, y2, sp, id);
}

void ami_dropeditboxsizg(FILE* f, ami_strptr sp, long* cw, long* ch, long* ow, long* oh)
{ ami_dropboxsizg(f, sp, cw, ch, ow, oh); }

void ami_dropeditboxsiz(FILE* f, ami_strptr sp, long* cw, long* ch, long* ow, long* oh)
{
    winptr win = f2win(f); if (!win) { *cw = *ch = *ow = *oh = 1; return; }
    ami_dropeditboxsizg(f, sp, cw, ch, ow, oh);
    gr2chrsiz(win, cw, ch);
    gr2chrsiz(win, ow, oh);
}

void ami_dropeditboxg(FILE* f, long x1, long y1, long x2, long y2, ami_strptr sp, long id)
{
    winptr win = f2win(f); if (!win) return;
    int n = 0;
    for (ami_strptr p = sp; p; p = p->next) n++;
    if (!n) return;
    const char** strs = malloc(n * sizeof(char*));
    if (!strs) return;
    int i = 0;
    for (ami_strptr p = sp; p; p = p->next) strs[i++] = p->str;
    pa_cocoa_dropeditbox(win->han, x1, y1, x2-x1+1, y2-y1+1, strs, n, id);
    free(strs);
}

void ami_dropeditbox(FILE* f, long x1, long y1, long x2, long y2, ami_strptr sp, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_dropeditboxg(f, x1, y1, x2, y2, sp, id);
}

void ami_slidehorizsizg(FILE* f, long* w, long* h) { *w = 150; *h = 20; }

void ami_slidehorizsiz(FILE* f, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_slidehorizsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

void ami_slidehorizg(FILE* f, long x1, long y1, long x2, long y2, long mark, long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_slider_horiz(win->han, x1, y1, x2-x1+1, y2-y1+1, mark, id);
}

void ami_slidehoriz(FILE* f, long x1, long y1, long x2, long y2, long mark, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_slidehorizg(f, x1, y1, x2, y2, mark, id);
}

void ami_slidevertsizg(FILE* f, long* w, long* h) { *w = 20; *h = 150; }

void ami_slidevertsiz(FILE* f, long* w, long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_slidevertsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

void ami_slidevertg(FILE* f, long x1, long y1, long x2, long y2, long mark, long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_slider_vert(win->han, x1, y1, x2-x1+1, y2-y1+1, mark, id);
}

void ami_slidevert(FILE* f, long x1, long y1, long x2, long y2, long mark, long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_slidevertg(f, x1, y1, x2, y2, mark, id);
}

/* tab bar height, matching the shim's PATabBar bar strip */
#define TABBAR_H 24

/* size of a tab panel for the given client size: the bar adds its height
   on the oriented side; (ox,oy) is the client offset within the panel */
void ami_tabbarsizg(FILE* f, ami_tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy)
{
    switch (tor) {
    case ami_totop:
        *w = cw; *h = ch + TABBAR_H; *ox = 0; *oy = TABBAR_H; break;
    case ami_tobottom:
        *w = cw; *h = ch + TABBAR_H; *ox = 0; *oy = 0; break;
    case ami_toleft:
        *w = cw + TABBAR_H; *h = ch; *ox = TABBAR_H; *oy = 0; break;
    default: /* ami_toright */
        *w = cw + TABBAR_H; *h = ch; *ox = 0; *oy = 0; break;
    }
}

void ami_tabbarsiz(FILE* f, ami_tabori tor, long cw, long ch, long* w, long* h, long* ox, long* oy)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; *ox = *oy = 0; return; }
    ami_tabbarsizg(f, tor, cw * win->charspace, ch * win->linespace,
                   w, h, ox, oy);
    gr2chrsiz(win, w, h);
    /* offsets round up to whole cells so the client clears the bar */
    *ox = (*ox + win->charspace - 1) / win->charspace;
    *oy = (*oy + win->linespace - 1) / win->linespace;
}

void ami_tabbarclientg(FILE* f, ami_tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy)
{
    switch (tor) {
    case ami_totop:
        *cw = w; *ch = h - TABBAR_H; *ox = 0; *oy = TABBAR_H; break;
    case ami_tobottom:
        *cw = w; *ch = h - TABBAR_H; *ox = 0; *oy = 0; break;
    case ami_toleft:
        *cw = w - TABBAR_H; *ch = h; *ox = TABBAR_H; *oy = 0; break;
    default: /* ami_toright */
        *cw = w - TABBAR_H; *ch = h; *ox = 0; *oy = 0; break;
    }
    if (*cw < 1) *cw = 1;
    if (*ch < 1) *ch = 1;
}

void ami_tabbarclient(FILE* f, ami_tabori tor, long w, long h, long* cw, long* ch, long* ox, long* oy)
{
    winptr win = f2win(f); if (!win) { *cw = *ch = 1; *ox = *oy = 0; return; }
    ami_tabbarclientg(f, tor, w * win->charspace, h * win->linespace,
                      cw, ch, ox, oy);
    gr2chrsiz(win, cw, ch);
    *ox = (*ox + win->charspace - 1) / win->charspace;
    *oy = (*oy + win->linespace - 1) / win->linespace;
}

static void tabbar_common(winptr win, long x1, long y1, long x2, long y2,
                          ami_strptr sp, ami_tabori tor, long barh, long id)
{
    int n = 0;
    for (ami_strptr p = sp; p; p = p->next) n++;
    if (!n) return;
    const char** strs = malloc(n * sizeof(char*));
    if (!strs) return;
    int i = 0;
    for (ami_strptr p = sp; p; p = p->next) strs[i++] = p->str;
    pa_cocoa_tabbar(win->han, x1, y1, x2-x1+1, y2-y1+1, strs, n,
                    (int)tor, barh, id);
    free(strs);
}

void ami_tabbarg(FILE* f, long x1, long y1, long x2, long y2, ami_strptr sp, ami_tabori tor, long id)
{
    winptr win = f2win(f); if (!win) return;
    tabbar_common(win, x1, y1, x2, y2, sp, tor, TABBAR_H, id);
}

void ami_tabbar(FILE* f, long x1, long y1, long x2, long y2, ami_strptr sp, ami_tabori tor, long id)
{
    winptr win = f2win(f); if (!win) return;
    /* character metrics rounded the bar up to whole cells; use that same
       thickness so the client area lands exactly on character cells */
    long barh;
    if (tor == ami_totop || tor == ami_tobottom)
        barh = (TABBAR_H + win->linespace - 1) / win->linespace
               * win->linespace;
    else
        barh = (TABBAR_H + win->charspace - 1) / win->charspace
               * win->charspace;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    tabbar_common(win, x1, y1, x2, y2, sp, tor, barh, id);
}

void ami_tabsel(FILE* f, long id, long tn)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_tabbar_sel(win->han, id, tn);
}

/*******************************************************************************
*                                                                              *
*                        PA API — dialogs                                      *
*                                                                              *
*******************************************************************************/

void ami_alert(char* title, char* message)
{
    pa_cocoa_alert(title, message);
}

void ami_querycolor(long* r, long* g, long* b)
{
    pa_cocoa_query_color(r, g, b);
}

void ami_queryopen(char* s, long sl)  { pa_cocoa_query_open(s, sl); }
void ami_querysave(char* s, long sl)  { pa_cocoa_query_save(s, sl); }

void ami_queryfind(char* s, long sl, ami_qfnopts* opt)
{
    pa_cocoa_query_find(s, sl, opt);
}

void ami_queryfindrep(char* s, long sl, char* r, long rl, ami_qfropts* opt)
{
    pa_cocoa_query_findrep(s, sl, r, rl, opt);
}

void ami_queryfont(FILE* f, long* fc, long* s, long* fr, long* fg,
                   long* fb, long* br, long* bg, long* bb,
                   ami_qfteffects* effect)
{
    char family[128] = "";
    int  n;

    /* seed the dialog with the current font code's family name */
    n = 1;
    for (fontptr fp = fntlst; fp; fp = fp->next, n++)
        if (n == *fc && fp->name) {
            strncpy(family, fp->name, sizeof(family) - 1);
            break;
        }

    pa_cocoa_query_font(family, sizeof(family), s,
                        fr, fg, fb, br, bg, bb);

    /* map the returned family name back to a PA font code; if the family
       is not in the font list, leave the code unchanged */
    n = 1;
    for (fontptr fp = fntlst; fp; fp = fp->next, n++)
        if (fp->name && !strcasecmp(fp->name, family)) {
            *fc = n;
            break;
        }
    (void)effect; /* text effects unchanged */
}
