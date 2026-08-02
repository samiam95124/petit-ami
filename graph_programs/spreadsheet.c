/*******************************************************************************
*                                                                              *
*                              SPREADSHEET                                     *
*                                                                              *
* A working spreadsheet on Petit-Ami graphics: a scrolling grid of cells       *
* holding numbers, text and formulas, with the standard menu, the standard     *
* file dialogs, and a window that reflows when it is resized.                  *
*                                                                              *
* Build and run:                                                               *
*                                                                              *
*     make spreadsheet                                                         *
*     bin/spreadsheet [-d] [<file>]                                            *
*                                                                              *
* -d reports the layout and the events to stderr, for when the window          *
* behaves unexpectedly.                                                        *
*                                                                              *
* Using it:                                                                    *
*                                                                              *
* Click a cell, or move with the arrow keys, page up and down, home and end.   *
* The scroll bars at the right and the bottom move the view over the whole      *
* sheet, which is 78 columns (A through BZ) by 1000 rows.                       *
* Type to enter: digits and text go in as typed, and a leading = makes a       *
* formula. Enter commits and steps down, tab commits and steps right, escape   *
* abandons the entry. Delete or backspace on a settled cell clears it.         *
*                                                                              *
* Formulas:                                                                    *
*                                                                              *
*     =A1+B2*2          the four operators, parentheses, unary minus           *
*     =SUM(A1:A10)      SUM, AVG, MIN, MAX, COUNT over a range                 *
*     =2*(SUM(B1:B9)+1) they nest                                              *
*                                                                              *
* A cell shows its value; the entry line at the top shows what the current     *
* cell holds, which for a formula is the formula. Formulas recalculate         *
* whenever anything changes, and a formula that cannot be evaluated shows      *
* ERR -- a bad reference, a division by zero, or a reference cycle, which      *
* ends at the nesting bound rather than hanging.                               *
*                                                                              *
* The file format is one line per non-empty cell, "A1: contents", which is     *
* both what it writes and what it reads.                                       *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2026 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions are met:  *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright notice,    *
*    this list of conditions and the following disclaimer.                     *
* 2. Redistributions in binary form must reproduce the above copyright notice, *
*    this list of conditions and the following disclaimer in the               *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors may be  *
*    used to endorse or promote products derived from this software without    *
*    specific prior written permission.                                        *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND ANY  *
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED    *
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE       *
* DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY *
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES   *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN  *
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#include <localdefs.h>
#include <graphics.h>

#define OFF 0
#define ON  1

#define COLS     78   /* columns, A through Z, AA through BZ */
/* the sheet is fixed at these bounds; the window shows what fits and the
   scroll bars move the view over the rest */
#define ROWS     1000 /* rows */
#define CELLEN   80   /* longest cell contents */
#define COLDIG   9    /* column width in digits of the display font */
#define HDRDIG   4    /* row header width in digits */
#define MAXNEST  32   /* deepest formula evaluation, catches cycles */

/* the scroll bars */
#define SBVERT   1 /* vertical scroll bar widget id */
#define SBHORIZ  2 /* horizontal scroll bar widget id */

/* menu ids of our own, after the standard ones */
#define MENUWIDER  (AMI_SMMAX+1) /* wider columns */
#define MENUNARROW (AMI_SMMAX+2) /* narrower columns */
#define MENUCLEAR  (AMI_SMMAX+3) /* clear the sheet */
#define MENURECALC (AMI_SMMAX+4) /* recalculate */

/* a cell holds what the user typed; the value is derived */
typedef struct {

    char* text;  /* contents as entered, NULL if empty */
    double val;  /* last computed value */
    int    isnum; /* the value is a number, not text */
    int    err;  /* evaluation failed */

} cellrec;

static cellrec cells[ROWS][COLS];

static long   curx, cury;      /* the current cell, 0 based */
static long   orgx, orgy;      /* top left cell shown, 0 based */
static long   colw, rowh;      /* cell size in pixels */
static long   hdrw, hdrh;      /* header sizes in pixels */
static long   cellx, celly;    /* whole cells that fit in the grid area */
static long   gridx0, gridy0;  /* the grid area, in pixels */
static long   gridx1, gridy1;
static long   sbw, sbh;        /* scroll bar thickness */
static long   coldig = COLDIG; /* column width in digits */
static char   entry[CELLEN];   /* the entry under construction */
static int    entering;        /* an entry is being typed */
static char   filename[500];   /* the file this sheet came from */
static int    modified;        /* the sheet has unsaved changes */
static int    evalnest;        /* formula evaluation depth */
static long   mpx, mpy;        /* the mouse, tracked in pixels */
static int    diag;            /* report to stderr, from -d */

/* the parser's position in a formula, and its error flag */
static const char* pp;
static int    perr;

static double expr(void); /* forward */
static void   layout(void); /* forward */
static void   drawall(void); /* forward */

/*******************************************************************************

Cell naming

*******************************************************************************/

/* the name of a column, as A or AB */
static void colnam(long x, char* s)

{

    if (x < 26) sprintf(s, "%c", (int)('A'+x));
    else sprintf(s, "%c%c", (int)('A'+x/26-1), (int)('A'+x%26));

}

/* the name of a cell, as A1 */
static void cellnam(long x, long y, char* s)

{

    char c[4];

    colnam(x, c);
    sprintf(s, "%s%ld", c, y+1);

}

/* parse a cell reference at the parse position; returns TRUE and the
   coordinates if one is there */
static int cellref(long* x, long* y)

{

    const char* sp = pp;
    long r = 0;

    if (!isalpha((unsigned char)*pp)) return (FALSE);
    *x = toupper((unsigned char)*pp)-'A';
    pp++;
    if (isalpha((unsigned char)*pp)) { /* a two letter column */

        *x = (*x+1)*26+(toupper((unsigned char)*pp)-'A');
        pp++;

    }
    if (!isdigit((unsigned char)*pp)) { pp = sp; return (FALSE); }
    while (isdigit((unsigned char)*pp)) r = r*10+(*pp++-'0');
    *y = r-1;
    if (*x < 0 || *x >= COLS || *y < 0 || *y >= ROWS) { perr = TRUE; }

    return (TRUE);

}

/*******************************************************************************

Formula evaluation

A recursive descent parser over the formula text, with the value of a
referenced cell taken by evaluating that cell in turn. The nesting counter
bounds the recursion, so a reference cycle ends as an error instead of a
stack overflow.

*******************************************************************************/

static double cellval(long x, long y); /* forward */

static void skipsp(void) { while (*pp == ' ') pp++; }

/* SUM(A1:B9) and friends: the function name has been consumed */
static double range(int fn)

{

    long x1, y1, x2, y2, x, y;
    double acc = 0, v;
    long   n = 0;
    int    first = TRUE;

    skipsp();
    if (*pp != '(') { perr = TRUE; return (0); }
    pp++;
    skipsp();
    if (!cellref(&x1, &y1)) { perr = TRUE; return (0); }
    skipsp();
    if (*pp != ':') { perr = TRUE; return (0); }
    pp++;
    skipsp();
    if (!cellref(&x2, &y2)) { perr = TRUE; return (0); }
    skipsp();
    if (*pp != ')') { perr = TRUE; return (0); }
    pp++;
    if (perr) return (0);
    if (x1 > x2) { x = x1; x1 = x2; x2 = x; }
    if (y1 > y2) { y = y1; y1 = y2; y2 = y; }
    for (y = y1; y <= y2; y++) for (x = x1; x <= x2; x++) {

        if (!cells[y][x].text) continue; /* empty cells do not count */
        v = cellval(x, y);
        n++;
        switch (fn) {

            case 0: acc += v; break; /* SUM */
            case 1: acc += v; break; /* AVG, divided below */
            case 2: if (first || v < acc) acc = v; break; /* MIN */
            case 3: if (first || v > acc) acc = v; break; /* MAX */
            case 4: acc = n; break; /* COUNT */

        }
        first = FALSE;

    }
    if (fn == 1) acc = n? acc/n: 0;
    if (fn == 4) acc = n;

    return (acc);

}

/* a number, a cell reference, a function, a parenthesized expression, or
   a unary minus before any of them */
static double factor(void)

{

    double v;
    long   x, y;
    char   name[16];
    int    i;

    skipsp();
    if (*pp == '-') { pp++; return (-factor()); }
    if (*pp == '+') { pp++; return (factor()); }
    if (*pp == '(') {

        pp++;
        v = expr();
        skipsp();
        if (*pp == ')') pp++; else perr = TRUE;

        return (v);

    }
    if (isdigit((unsigned char)*pp) || *pp == '.')
        return (strtod(pp, (char**)&pp));
    if (isalpha((unsigned char)*pp)) {

        /* a function name, or a cell reference */
        i = 0;
        while (isalpha((unsigned char)pp[i]) && i < (int)sizeof(name)-1) i++;
        memcpy(name, pp, i);
        name[i] = 0;
        for (x = 0; x < i; x++) name[x] = toupper((unsigned char)name[x]);
        if (i > 1 && (!strcmp(name, "SUM") || !strcmp(name, "AVG") ||
                      !strcmp(name, "MIN") || !strcmp(name, "MAX") ||
                      !strcmp(name, "COUNT"))) {

            pp += i;

            return (range(!strcmp(name, "SUM")? 0:
                          !strcmp(name, "AVG")? 1:
                          !strcmp(name, "MIN")? 2:
                          !strcmp(name, "MAX")? 3: 4));

        }
        if (cellref(&x, &y)) {

            if (perr) return (0);

            return (cellval(x, y));

        }

    }
    perr = TRUE;

    return (0);

}

static double term(void)

{

    double v = factor();
    double d;

    for (;;) {

        skipsp();
        if (*pp == '*') { pp++; v *= factor(); }
        else if (*pp == '/') {

            pp++;
            d = factor();
            if (d == 0) { perr = TRUE; return (0); }
            v /= d;

        } else return (v);

    }

}

static double expr(void)

{

    double v = term();

    for (;;) {

        skipsp();
        if (*pp == '+') { pp++; v += term(); }
        else if (*pp == '-') { pp++; v -= term(); }
        else return (v);

    }

}

/* the value of a cell: a number as itself, a formula evaluated, text as
   zero */
static double cellval(long x, long y)

{

    cellrec*    c;
    const char* savepp;
    int         saveerr;
    double      v;
    char*       ep;

    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return (0);
    c = &cells[y][x];
    if (!c->text) return (0);
    if (c->text[0] != '=') { /* a number, or text */

        v = strtod(c->text, &ep);
        while (*ep == ' ') ep++;

        return (*ep? 0: v);

    }
    if (evalnest >= MAXNEST) { perr = TRUE; return (0); } /* a cycle */
    evalnest++;
    savepp = pp; /* the parse of the referring formula is suspended */
    saveerr = perr;
    pp = c->text+1;
    perr = FALSE;
    v = expr();
    skipsp();
    if (*pp) perr = TRUE; /* trailing junk */
    c->err = perr;
    c->val = v;
    c->isnum = TRUE;
    pp = savepp;
    if (perr) saveerr = TRUE; /* a bad reference poisons the referrer */
    perr = saveerr;
    evalnest--;

    return (v);

}

/* recompute every cell that holds a formula */
static void recalc(void)

{

    long x, y;
    cellrec* c;
    char* ep;

    for (y = 0; y < ROWS; y++) for (x = 0; x < COLS; x++) {

        c = &cells[y][x];
        c->err = FALSE;
        c->isnum = FALSE;
        c->val = 0;
        if (!c->text) continue;
        if (c->text[0] == '=') { /* a formula */

            evalnest = 0;
            perr = FALSE;
            pp = c->text+1;
            c->val = expr();
            skipsp();
            if (*pp) perr = TRUE;
            c->err = perr;
            c->isnum = TRUE;

        } else { /* a number, or text */

            c->val = strtod(c->text, &ep);
            while (*ep == ' ') ep++;
            c->isnum = c->text[0] && !*ep;

        }

    }

}

/*******************************************************************************

Sheet contents

*******************************************************************************/

/* place contents in a cell, or clear it with NULL */
static void setcell(long x, long y, const char* s)

{

    cellrec* c = &cells[y][x];

    if (c->text) free(c->text);
    c->text = NULL;
    if (s && *s) {

        c->text = malloc(strlen(s)+1);
        if (!c->text) { ami_alert("spreadsheet", "Out of memory"); exit(1); }
        strcpy(c->text, s);

    }
    modified = TRUE;
    recalc();

}

/* empty the sheet */
static void clearsheet(void)

{

    long x, y;

    for (y = 0; y < ROWS; y++) for (x = 0; x < COLS; x++)
        if (cells[y][x].text) {

            free(cells[y][x].text);
            cells[y][x].text = NULL;

        }
    memset(cells, 0, sizeof(cells));
    curx = cury = 0;
    orgx = orgy = 0;
    modified = FALSE;

}

/* what a cell displays: its value, or its text, or an error */
static void celldsp(long x, long y, char* s, long sl)

{

    cellrec* c = &cells[y][x];
    double   v;

    if (!c->text) { *s = 0; return; }
    if (c->err) { snprintf(s, sl, "%s", "ERR"); return; }
    if (c->text[0] == '=' || c->isnum) { /* a value */

        v = c->val;
        if (v == floor(v) && fabs(v) < 1e15) snprintf(s, sl, "%.0f", v);
        else snprintf(s, sl, "%.4g", v);

    } else snprintf(s, sl, "%s", c->text); /* text */

}

/*******************************************************************************

Files

*******************************************************************************/

static void loadsheet(const char* fn)

{

    FILE* f;
    char  line[CELLEN+40];
    char* p;
    long  x, y;

    f = fopen(fn, "r");
    if (!f) { ami_alert("spreadsheet", "Cannot open file"); return; }
    clearsheet();
    while (fgets(line, sizeof(line), f)) {

        p = strchr(line, '\n');
        if (p) *p = 0;
        p = strchr(line, ':');
        if (!p) continue; /* not a cell line */
        *p++ = 0;
        while (*p == ' ') p++;
        pp = line;
        perr = FALSE;
        if (!cellref(&x, &y) || perr || *pp) continue; /* bad name */
        if (cells[y][x].text) free(cells[y][x].text);
        cells[y][x].text = malloc(strlen(p)+1);
        if (!cells[y][x].text) { ami_alert("spreadsheet", "Out of memory");
                                 exit(1); }
        strcpy(cells[y][x].text, p);

    }
    fclose(f);
    recalc();
    modified = FALSE;
    strncpy(filename, fn, sizeof(filename)-1);

}

static void savesheet(const char* fn)

{

    FILE* f;
    long  x, y;
    char  nam[16];

    f = fopen(fn, "w");
    if (!f) { ami_alert("spreadsheet", "Cannot write file"); return; }
    for (y = 0; y < ROWS; y++) for (x = 0; x < COLS; x++)
        if (cells[y][x].text) {

            cellnam(x, y, nam);
            fprintf(f, "%s: %s\n", nam, cells[y][x].text);

        }
    fclose(f);
    modified = FALSE;
    strncpy(filename, fn, sizeof(filename)-1);

}

/*******************************************************************************

Display

The grid is drawn from the top left cell shown, filling whatever client
area the window has. Everything is derived from the font metrics, so the
sheet reflows on a resize or a column width change.

*******************************************************************************/

/* A fraction as a ratio of LONG_MAX, which is the full scale the bars
   work in. In integers: LONG_MAX is not representable as a double, so
   the obvious double form rounds up to one past it, and the cast back
   overflows to a negative -- which the scroll bar rejects as an invalid
   position, exactly at the end of the sheet, where num reaches den. */
static long fullscale(long num, long den)

{

    if (den < 1 || num <= 0) return (0);
    if (num >= den) return (LONG_MAX);

    return (LONG_MAX/den*num); /* num < den, so this cannot overflow */

}

/* Set a scroll bar from the view: the thumb is the visible share of the
   sheet, at the scrolled position. */
static void setbar(long id, long org, long vis, long total)

{

    long max = total-vis;

    if (max < 1) max = 1;
    ami_scrollsiz(stdout, id, fullscale(vis, total));
    ami_scrollpos(stdout, id, fullscale(org, max));

}

/* a cell offset from a bar position, over the given travel */
static long barcell(long pos, long travel)

{

    if (travel < 1 || pos <= 0) return (0);
    if (pos >= LONG_MAX) return (travel);

    return ((long)((double)travel*((double)pos/LONG_MAX)+0.5));

}

/* recompute the geometry from the window and the font, place the scroll
   bars, and set them from the view */
static void layout(void)

{

    colw = ami_strsiz(stdout, "0")*coldig;
    rowh = ami_chrsizy(stdout)+4;
    hdrw = ami_strsiz(stdout, "0")*HDRDIG;
    hdrh = rowh; /* the column header row */
    /* the grid area: below the entry line and the column headers, and
       inside the scroll bars */
    gridx0 = hdrw;
    gridy0 = rowh*2+hdrh;
    gridx1 = ami_maxxg(stdout)-sbw;
    gridy1 = ami_maxyg(stdout)-sbh;
    if (gridx1 < gridx0+colw) gridx1 = gridx0+colw;
    if (gridy1 < gridy0+rowh) gridy1 = gridy0+rowh;
    /* whole cells that fit, which is what a page moves by */
    cellx = (gridx1-gridx0)/colw;
    celly = (gridy1-gridy0)/rowh;
    if (cellx < 1) cellx = 1;
    if (celly < 1) celly = 1;
    /* the bars line the grid area: vertical at the right of it,
       horizontal under it, each spanning the grid */
    ami_poswidgetg(stdout, SBVERT, gridx1+1, gridy0);
    ami_sizwidgetg(stdout, SBVERT, sbw, gridy1-gridy0);
    ami_poswidgetg(stdout, SBHORIZ, gridx0, gridy1+1);
    ami_sizwidgetg(stdout, SBHORIZ, gridx1-gridx0, sbh);
    setbar(SBVERT, orgy, celly, ROWS);
    setbar(SBHORIZ, orgx, cellx, COLS);
    if (diag) fprintf(stderr,
        "layout: window %ldx%ld grid %ld,%ld-%ld,%ld cells %ldx%ld "
        "bars: vert at %ld,%ld %ldx%ld horiz at %ld,%ld %ldx%ld\n",
        ami_maxxg(stdout), ami_maxyg(stdout), gridx0, gridy0, gridx1, gridy1,
        cellx, celly, gridx1+1, gridy0, sbw, gridy1-gridy0,
        gridx0, gridy1+1, gridx1-gridx0, sbh);

}

/* the pixel origin of a displayed cell */
static void cellpos(long x, long y, long* px, long* py)

{

    *px = hdrw+(x-orgx)*colw+1;
    *py = rowh*2+hdrh+(y-orgy)*rowh+1;

}

/* draw one cell, headers included in the caller's frame */
static void drawcell(long x, long y)

{

    long px, py;
    char s[CELLEN];
    long tw;

    if (x < orgx || y < orgy) return;
    cellpos(x, y, &px, &py);
    if (px > gridx1 || py > gridy1) return;
    /* the cell face: the current cell is marked */
    if (x == curx && y == cury) ami_fcolor(stdout, ami_cyan);
    else ami_fcolor(stdout, ami_white);
    ami_frect(stdout, px, py, px+colw-2, py+rowh-2);
    ami_fcolor(stdout, ami_black);
    if (entering && x == curx && y == cury) strcpy(s, entry);
    else celldsp(x, y, s, sizeof(s));
    if (*s) {

        /* numbers to the right, text to the left, as a sheet does */
        tw = ami_strsiz(stdout, s);
        if (tw > colw-4) tw = colw-4;
        if (!entering && cells[y][x].text &&
            (cells[y][x].isnum || cells[y][x].text[0] == '=' ||
             cells[y][x].err))
            ami_cursorg(stdout, px+colw-3-tw, py+2);
        else ami_cursorg(stdout, px+2, py+2);
        /* clipped to the cell by the window's own edge; keep it short */
        if (ami_strsiz(stdout, s) > colw-4) {

            long i = strlen(s);
            while (i && ami_strsiz(stdout, s) > colw-4) s[--i] = 0;

        }
        fprintf(stdout, "%s", s);

    }

}

/* the whole window: entry line, headers, cells, grid */
static void drawall(void)

{

    long x, y, px, py;
    char s[CELLEN+40];
    char nam[16];

    long nx = (gridx1-gridx0+colw-1)/colw+1; /* cells to cover the area */
    long ny = (gridy1-gridy0+rowh-1)/rowh+1;

    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, 1, 1, ami_maxxg(stdout), ami_maxyg(stdout));
    /* the entry line: the current cell and what it holds */
    cellnam(curx, cury, nam);
    if (entering) snprintf(s, sizeof(s), "%s  %s_", nam, entry);
    else snprintf(s, sizeof(s), "%s  %s", nam,
                  cells[cury][curx].text? cells[cury][curx].text: "");
    ami_fcolor(stdout, ami_black);
    ami_cursorg(stdout, 2, 2);
    fprintf(stdout, "%s", s);
    ami_line(stdout, 1, rowh*2-2, ami_maxxg(stdout), rowh*2-2);
    /* the header bands */
    ami_fcolor(stdout, ami_yellow);
    ami_frect(stdout, 1, rowh*2, gridx1, rowh*2+hdrh-1);
    ami_frect(stdout, 1, rowh*2, hdrw-1, gridy1);
    ami_fcolor(stdout, ami_black);
    /* the column headers */
    for (x = orgx; x < orgx+nx && x < COLS; x++) {

        cellpos(x, orgy, &px, &py);
        if (px > gridx1) break;
        colnam(x, s);
        ami_cursorg(stdout, px+colw/2-ami_strsiz(stdout, s)/2, rowh*2+2);
        fprintf(stdout, "%s", s);

    }
    /* the row headers */
    for (y = orgy; y < orgy+ny && y < ROWS; y++) {

        cellpos(orgx, y, &px, &py);
        if (py > gridy1) break;
        sprintf(s, "%ld", y+1);
        ami_cursorg(stdout, hdrw-2-ami_strsiz(stdout, s), py+2);
        fprintf(stdout, "%s", s);

    }
    /* the cells */
    for (y = orgy; y < orgy+ny && y < ROWS; y++)
        for (x = orgx; x < orgx+nx && x < COLS; x++) drawcell(x, y);
    /* the grid lines, stopped at the grid area */
    ami_fcolor(stdout, ami_black);
    for (x = orgx; x <= orgx+nx && x <= COLS; x++) {

        cellpos(x, orgy, &px, &py);
        if (px-1 > gridx1) break;
        ami_line(stdout, px-1, rowh*2, px-1, gridy1);

    }
    for (y = orgy; y <= orgy+ny && y <= ROWS; y++) {

        cellpos(orgx, y, &px, &py);
        if (py-1 > gridy1) break;
        ami_line(stdout, 1, py-1, gridx1, py-1);

    }

}

/* bring the current cell into view, and redraw if the view moved */
static void clamporg(void)

{

    if (orgx > COLS-cellx) orgx = COLS-cellx;
    if (orgy > ROWS-celly) orgy = ROWS-celly;
    if (orgx < 0) orgx = 0;
    if (orgy < 0) orgy = 0;

}

static void follow(void)

{

    long ox = orgx, oy = orgy;

    if (curx < orgx) orgx = curx;
    if (curx >= orgx+cellx) orgx = curx-cellx+1;
    if (cury < orgy) orgy = cury;
    if (cury >= orgy+celly) orgy = cury-celly+1;
    clamporg();
    if (ox != orgx || oy != orgy) {

        setbar(SBVERT, orgy, celly, ROWS);
        setbar(SBHORIZ, orgx, cellx, COLS);
        drawall();

    }

}

/* Scroll the view without moving the current cell, as the bars do. The
   bar the scroll came from, if any, is left alone: it reported where the
   user put its slider and has been set there, and setting it again from
   the view would drag the slider back to the nearest whole cell -- which
   on a bar with few cells of travel is most of the way back where it
   started, so the bar appeared not to work. */
static void scrollto(long nx, long ny, long frombar)

{

    long ox = orgx, oy = orgy;

    orgx = nx;
    orgy = ny;
    clamporg();
    if (ox != orgx || oy != orgy) {

        if (frombar != SBVERT) setbar(SBVERT, orgy, celly, ROWS);
        if (frombar != SBHORIZ) setbar(SBHORIZ, orgx, cellx, COLS);
        drawall();

    }

}

/*******************************************************************************

Menu

*******************************************************************************/

static void appendmenu(ami_menuptr* list, ami_menuptr m)

{

    ami_menuptr lp;

    m->next = NULL;
    m->branch = NULL;
    if (!*list) *list = m;
    else { lp = *list; while (lp->next) lp = lp->next; lp->next = m; }

}

static void newmenu(ami_menuptr* mp, int onoff, int oneof, int bar,
                    int id, const char* face)

{

    *mp = malloc(sizeof(ami_menurec));
    if (!*mp) { ami_alert("spreadsheet", "Out of memory"); exit(1); }
    (*mp)->onoff = onoff;
    (*mp)->oneof = oneof;
    (*mp)->bar = bar;
    (*mp)->id = id;
    (*mp)->face = malloc(strlen(face)+1);
    if (!(*mp)->face) { ami_alert("spreadsheet", "Out of memory"); exit(1); }
    strcpy((*mp)->face, face);
    (*mp)->next = NULL;
    (*mp)->branch = NULL;

}

/* the standard file and edit menus, with a sheet menu of our own after */
static void setupmenu(void)

{

    ami_menuptr ml = NULL; /* our own entries */
    ami_menuptr mp;
    ami_menuptr sm = NULL; /* the completed menu */

    newmenu(&mp, FALSE, FALSE, OFF, MENUWIDER, "Wider columns");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, MENUNARROW, "Narrower columns");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, MENURECALC, "Recalculate");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, MENUCLEAR, "Clear sheet");
    appendmenu(&ml, mp);
    ami_stdmenu(BIT(AMI_SMNEW) | BIT(AMI_SMOPEN) | BIT(AMI_SMSAVE) |
                BIT(AMI_SMSAVEAS) | BIT(AMI_SMEXIT) | BIT(AMI_SMABOUT),
                &sm, ml);
    ami_menu(stdout, sm);

}

/*******************************************************************************

Entry

*******************************************************************************/

/* start typing into the current cell */
static void startentry(char first)

{

    entering = TRUE;
    entry[0] = first;
    entry[1] = 0;
    drawall();

}

/* commit what was typed */
static void commit(void)

{

    if (!entering) return;
    entering = FALSE;
    setcell(curx, cury, entry);
    entry[0] = 0;

}

/*******************************************************************************

Main

*******************************************************************************/

int main(int argc, char* argv[])

{

    ami_evtrec er;
    char  fn[500];
    long  x, y;

    if (argc > 1 && !strcmp(argv[1], "-d")) { /* report what happens */

        diag = TRUE;
        argc--;
        argv++;

    }
    ami_title(stdout, "Spreadsheet");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_font(stdout, AMI_FONT_TERM); /* a fixed pitch font suits a grid */
    /* by point size, so the sheet is the same size on any display: a
       pixel height would come out small on a high density screen */
    ami_setpoints(stdout, 12.0);
    ami_binvis(stdout);
    setupmenu();
    clearsheet();
    if (argc > 1) loadsheet(argv[1]);
    /* the scroll bars, at their standard thickness */
    ami_scrollvertsizg(stdout, &sbw, &y);
    ami_scrollhorizsizg(stdout, &x, &sbh);
    ami_scrollvertg(stdout, 1, 1, sbw, 100, SBVERT);
    ami_scrollhorizg(stdout, 1, 1, 100, sbh, SBHORIZ);
    layout();
    drawall();
    do {

        ami_event(stdin, &er);
        if (diag) switch (er.etype) { /* the events the bars send */

            case ami_etmouba:
                fprintf(stderr, "click button %ld at %ld,%ld\n",
                        er.amoubn, mpx, mpy);
                break;
            case ami_etsclull: fprintf(stderr, "scroll line back, bar %ld\n",
                                       er.sclulid); break;
            case ami_etscldrl: fprintf(stderr, "scroll line on, bar %ld\n",
                                       er.scldrid); break;
            case ami_etsclulp: fprintf(stderr, "scroll page back, bar %ld\n",
                                       er.sclupid); break;
            case ami_etscldrp: fprintf(stderr, "scroll page on, bar %ld\n",
                                       er.scldpid); break;
            case ami_etsclpos: fprintf(stderr, "scroll to %ld, bar %ld\n",
                                       er.sclpos, er.sclpid); break;
            case ami_etresize: fprintf(stderr, "resize to %ldx%ld\n",
                                       er.rszxg, er.rszyg); break;
            case ami_etredraw: fprintf(stderr, "redraw\n"); break;
            default: break;

        }
        switch (er.etype) {

            case ami_etresize:
                /* The window is buffered, so its measurements are the
                   buffer's: without this the sheet kept the size it
                   started at while the window grew around it. The
                   buffer follows the window, and everything after is
                   derived from the new measurements. */
                ami_sizbufg(stdout, er.rszxg, er.rszyg);
                layout();
                clamporg();
                drawall();
                break;

            case ami_etredraw:
                layout();
                drawall();
                break;

            case ami_etmoumovg: /* the mouse, for the next click */
                mpx = er.moupxg;
                mpy = er.moupyg;
                break;

            case ami_etmouba: /* a click picks a cell */
                if (er.amoubn != 1) break;
                if (mpx < gridx0 || mpy < gridy0 ||
                    mpx > gridx1 || mpy > gridy1) break;
                x = (mpx-gridx0)/colw+orgx;
                y = (mpy-gridy0)/rowh+orgy;
                if (x < 0 || x >= COLS || y < 0 || y >= ROWS) break;
                commit();
                curx = x;
                cury = y;
                drawall();
                break;

            case ami_etchar: /* typing enters */
                if (er.echar >= ' ' && er.echar != 0x7f) {

                    if (!entering) startentry(er.echar);
                    else if (strlen(entry) < CELLEN-1) {

                        entry[strlen(entry)+1] = 0;
                        entry[strlen(entry)] = er.echar;
                        drawall();

                    }

                }
                break;

            case ami_etenter: /* commit and step down */
                commit();
                if (cury < ROWS-1) cury++;
                follow();
                drawall();
                break;

            case ami_ettab: /* commit and step right */
                commit();
                if (curx < COLS-1) curx++;
                follow();
                drawall();
                break;

            case ami_etcan: /* abandon the entry */
                entering = FALSE;
                entry[0] = 0;
                drawall();
                break;

            case ami_etdelcb: /* backspace: in an entry, or clear the cell */
                if (entering) {

                    if (*entry) entry[strlen(entry)-1] = 0;
                    if (!*entry) entering = FALSE;

                } else setcell(curx, cury, NULL);
                drawall();
                break;

            case ami_etdelcf: /* delete clears the cell */
                if (!entering) setcell(curx, cury, NULL);
                drawall();
                break;

            case ami_etup:
                commit();
                if (cury) cury--;
                follow();
                drawall();
                break;

            case ami_etdown:
                commit();
                if (cury < ROWS-1) cury++;
                follow();
                drawall();
                break;

            case ami_etleft:
                commit();
                if (curx) curx--;
                follow();
                drawall();
                break;

            case ami_etright:
                commit();
                if (curx < COLS-1) curx++;
                follow();
                drawall();
                break;

            case ami_etpagu:
                commit();
                cury -= celly;
                if (cury < 0) cury = 0;
                follow();
                drawall();
                break;

            case ami_etpagd:
                commit();
                cury += celly;
                if (cury >= ROWS) cury = ROWS-1;
                follow();
                drawall();
                break;

            case ami_ethome:
            case ami_ethomes:
                commit();
                curx = 0;
                cury = 0;
                follow();
                drawall();
                break;

            case ami_ethomel:
                commit();
                curx = 0;
                follow();
                drawall();
                break;

            case ami_etendl:
                commit();
                /* the last occupied cell in the row, or the first */
                for (x = COLS-1; x > 0 && !cells[cury][x].text; x--);
                curx = x;
                follow();
                drawall();
                break;

            case ami_etsclull: /* a line back */
                if (er.sclulid == SBVERT) scrollto(orgx, orgy-1, 0);
                else scrollto(orgx-1, orgy, 0);
                break;

            case ami_etscldrl: /* a line on */
                if (er.scldrid == SBVERT) scrollto(orgx, orgy+1, 0);
                else scrollto(orgx+1, orgy, 0);
                break;

            case ami_etsclulp: /* a page back */
                if (er.sclupid == SBVERT) scrollto(orgx, orgy-celly, 0);
                else scrollto(orgx-cellx, orgy, 0);
                break;

            case ami_etscldrp: /* a page on */
                if (er.scldpid == SBVERT) scrollto(orgx, orgy+celly, 0);
                else scrollto(orgx+cellx, orgy, 0);
                break;

            case ami_etsclpos: /* the slider was placed */
                /* the slider goes where the user put it, then the view
                   follows from there */
                ami_scrollpos(stdout, er.sclpid, er.sclpos);
                if (er.sclpid == SBVERT)
                    scrollto(orgx, barcell(er.sclpos, ROWS-celly), SBVERT);
                else
                    scrollto(barcell(er.sclpos, COLS-cellx), orgy, SBHORIZ);
                break;

            case ami_etmenus: /* the menu */
                switch (er.menuid) {

                    case AMI_SMNEW:
                        clearsheet();
                        filename[0] = 0;
                        drawall();
                        break;

                    case AMI_SMOPEN:
                        fn[0] = 0;
                        ami_queryopen(fn, sizeof(fn));
                        if (*fn) { loadsheet(fn); layout(); drawall(); }
                        break;

                    case AMI_SMSAVE:
                        if (*filename) savesheet(filename);
                        else {

                            fn[0] = 0;
                            ami_querysave(fn, sizeof(fn));
                            if (*fn) savesheet(fn);

                        }
                        break;

                    case AMI_SMSAVEAS:
                        strcpy(fn, filename);
                        ami_querysave(fn, sizeof(fn));
                        if (*fn) savesheet(fn);
                        break;

                    case AMI_SMEXIT:
                        goto done;

                    case AMI_SMABOUT:
                        ami_alert("Spreadsheet",
                                  "A spreadsheet on Petit-Ami graphics");
                        break;

                    case MENUWIDER:
                        if (coldig < 20) coldig++;
                        layout();
                        follow();
                        drawall();
                        break;

                    case MENUNARROW:
                        if (coldig > 5) coldig--;
                        layout();
                        follow();
                        drawall();
                        break;

                    case MENURECALC:
                        recalc();
                        drawall();
                        break;

                    case MENUCLEAR:
                        clearsheet();
                        drawall();
                        break;

                }
                break;

            default: break;

        }

    } while (er.etype != ami_etterm);
    done:

    return (0);

}
