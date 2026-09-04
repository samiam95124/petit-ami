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
* Text that does not fit its column runs on into the empty cells to its       *
* right, so a title need not fit the column it starts in; it stops at the      *
* first cell holding anything. A number that does not fit shows as # marks     *
* rather than a truncated number, which would read as a different number.      *
*                                                                              *
* A cell shows its value; the entry line at the top shows what the current     *
* cell holds, which for a formula is the formula. Formulas recalculate         *
* whenever anything changes, and a formula that cannot be evaluated shows      *
* ERR -- a bad reference, a division by zero, or a reference cycle, which      *
* ends at the nesting bound rather than hanging.                               *
*                                                                              *
* The file is an OpenDocument spreadsheet, .ods, the ISO/IEC 26300 format      *
* that LibreOffice and the rest read and write, so a sheet written here        *
* opens there and one written there opens here, formulas and all.              *
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
#include <strings.h> /* strncasecmp, for the help search */
#include <ctype.h>
#include <math.h>
#include <limits.h>

#include <zlib.h> /* the .ods container is a zip, deflated */

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
#define WHEELROWS 3   /* rows the view moves per notch of the wheel */

/* the scroll bars */
#define SBVERT   1 /* vertical scroll bar widget id */
#define SBHORIZ  2 /* horizontal scroll bar widget id */

/* The help window is a window of the program's own, id 2; the sheet is
   always window 1. Its widgets are numbered in it, and widget numbers
   belong to their window, so these do not collide with the bars above. */
#define HELPWIN   2 /* the help window */
#define HELPFIND  1 /* the search entry */
#define HELPLIST  2 /* the topic list */
#define HELPCLOSE 3 /* the close button */

/* The help text is a file, not something built into the program, so
   that it can be as long as it deserves and can be changed without a
   compiler. It is markdown of the plain kind; see the file itself. */
#define HELPFILE  "spreadsheet.md"

/* Menu ids of our own, after the standard ones. They hang under one
   Sheet entry of our own, which the standard menu places between its
   Edit and Window menus. */
#define MENUSHEET  (AMI_SMMAX+1) /* the sheet menu itself */
#define MENUWIDER  (AMI_SMMAX+2) /* wider columns */
#define MENUNARROW (AMI_SMMAX+3) /* narrower columns */
#define MENURECALC (AMI_SMMAX+5) /* recalculate */

/* a cell holds what the user typed; the value is derived */
typedef struct {

    char* text;  /* contents as entered, NULL if empty */
    double val;  /* last computed value */
    int    isnum; /* the value is a number, not text */
    int    err;  /* evaluation failed */

} cellrec;

static cellrec cells[ROWS][COLS];

static int    curx, cury;      /* the current cell, 0 based */
static int    orgx, orgy;      /* top left cell shown, 0 based */
static int    colw, rowh;      /* cell size in pixels */
static int    hdrw, hdrh;      /* header sizes in pixels */
static int    cellx, celly;    /* whole cells that fit in the grid area */
static int    gridx0, gridy0;  /* the grid area, in pixels */
static int    gridx1, gridy1;
static ami_long sbw, sbh;        /* scroll bar thickness */
static int    coldig = COLDIG; /* column width in digits */
static char   entry[CELLEN];   /* the entry under construction */
static int    entering;        /* an entry is being typed */
static char   filename[500];   /* the file this sheet came from */
static int    modified;        /* the sheet has unsaved changes */
static int    evalnest;        /* formula evaluation depth */
static int    mpx, mpy;        /* the mouse, tracked in pixels */
static int    diag;            /* report to stderr, from -d */
static char*  undtxt;          /* the contents a cell had before the last
                                  change, for undo */
static int    undx, undy;      /* the cell that change was in */
static int    undval;          /* there is something to undo */
static char*  clip;            /* the cut cell, for paste */

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
static void colnam(ami_long x, char* s)

{

    if (x < 26) sprintf(s, "%c", (int)('A'+x));
    else sprintf(s, "%c%c", (int)('A'+x/26-1), (int)('A'+x%26));

}

/* the name of a cell, as A1 */
static void cellnam(ami_long x, ami_long y, char* s)

{

    char c[4];

    colnam(x, c);
    sprintf(s, "%s%lld", c, AMI_LONG_CAST(y+1));

}

/* parse a cell reference at the parse position; returns TRUE and the
   coordinates if one is there */
static int cellref(ami_long* x, ami_long* y)

{

    const char* sp = pp;
    int r = 0;

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

static double cellval(ami_long x, ami_long y); /* forward */

static void skipsp(void) { while (*pp == ' ') pp++; }

/* SUM(A1:B9) and friends: the function name has been consumed */
static double range(int fn)

{

    ami_long x1, y1, x2, y2, x, y;
    double acc = 0, v;
    int    n = 0;
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
    ami_long x, y;
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
static double cellval(ami_long x, ami_long y)

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

    ami_long x, y;
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
static void setcell(ami_long x, ami_long y, const char* s)

{

    cellrec* c = &cells[y][x];

    /* what the cell held is kept for undo */
    if (undtxt) free(undtxt);
    undtxt = NULL;
    if (c->text) {

        undtxt = malloc(strlen(c->text)+1);
        if (undtxt) strcpy(undtxt, c->text);

    }
    undx = x;
    undy = y;
    undval = TRUE;
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

    ami_long x, y;

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
static void celldsp(ami_long x, ami_long y, char* s, int sl)

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

The sheet is kept as an OpenDocument spreadsheet (.ods, ISO/IEC 26300):
a zip container holding the mimetype, a manifest, and content.xml, which
carries the cells. That is what LibreOffice and the rest read and write,
so a sheet written here opens there and a sheet written there opens
here.

Only what this program has is written: values, text, and formulas. A
formula crosses over in the ODF form, which brackets its references --
=SUM(A1:A3) is stored as of:=SUM([.A1:.A3]) -- so the translation is at
the brackets, both ways.

Reading takes the zip apart far enough to find content.xml, inflating
it when the writer compressed it (they all do), and scans the XML for
rows and cells. It is not a general XML parser: it looks for the
handful of elements a spreadsheet body is made of, which is enough for
the files LibreOffice writes and all the files this program writes.

*******************************************************************************/

/* a growing byte buffer, for building the zip and for the inflated xml */
typedef struct {

    char*  p;   /* the bytes */
    size_t len; /* how many */
    size_t max; /* room */

} buffer;

static void bufput(buffer* b, const void* d, size_t n)

{

    if (b->len+n > b->max) {

        while (b->len+n > b->max) b->max = b->max? b->max*2: 4096;
        b->p = realloc(b->p, b->max);
        if (!b->p) { ami_alert("spreadsheet", "Out of memory"); exit(1); }

    }
    memcpy(b->p+b->len, d, n);
    b->len += n;

}

static void bufstr(buffer* b, const char* s) { bufput(b, s, strlen(s)); }

static void buf16(buffer* b, unsigned v)

{

    char c[2];

    c[0] = v&0xff; c[1] = v >> 8&0xff;
    bufput(b, c, 2);

}

static void buf32(buffer* b, unsigned int v)

{

    char c[4];

    c[0] = v&0xff; c[1] = v >> 8&0xff; c[2] = v >> 16&0xff; c[3] = v >> 24&0xff;
    bufput(b, c, 4);

}

/* xml text with the five characters that cannot appear as themselves */
static void bufxml(buffer* b, const char* s)

{

    while (*s) {

        switch (*s) {

            case '&':  bufstr(b, "&amp;"); break;
            case '<':  bufstr(b, "&lt;"); break;
            case '>':  bufstr(b, "&gt;"); break;
            case '"':  bufstr(b, "&quot;"); break;
            case '\'': bufstr(b, "&apos;"); break;
            default:   bufput(b, s, 1); break;

        }
        s++;

    }

}

/* one stored (uncompressed) zip member, appended to the archive and
   noted for the central directory */
typedef struct {

    const char* name;
    unsigned int crc;
    size_t len;
    size_t off;

} zipent;

static void zipadd(buffer* z, zipent* e, const char* name, const char* data,
                   size_t len)

{

    e->name = name;
    e->crc = crc32(0, (const Bytef*)data, len);
    e->len = len;
    e->off = z->len;
    buf32(z, 0x04034b50); /* local file header */
    buf16(z, 20);         /* version needed */
    buf16(z, 0);          /* flags */
    buf16(z, 0);          /* stored */
    buf16(z, 0);          /* time */
    buf16(z, 0x21);       /* date, 1980-01-01 */
    buf32(z, e->crc);
    buf32(z, len);        /* compressed size */
    buf32(z, len);        /* uncompressed size */
    buf16(z, strlen(name));
    buf16(z, 0);          /* no extra */
    bufstr(z, name);
    bufput(z, data, len);

}

static void zipend(buffer* z, zipent* ents, int n)

{

    size_t dir = z->len;
    size_t dirlen;
    int    i;

    for (i = 0; i < n; i++) {

        buf32(z, 0x02014b50); /* central directory header */
        buf16(z, 20);         /* version made by */
        buf16(z, 20);         /* version needed */
        buf16(z, 0);          /* flags */
        buf16(z, 0);          /* stored */
        buf16(z, 0);          /* time */
        buf16(z, 0x21);       /* date */
        buf32(z, ents[i].crc);
        buf32(z, ents[i].len);
        buf32(z, ents[i].len);
        buf16(z, strlen(ents[i].name));
        buf16(z, 0);          /* extra */
        buf16(z, 0);          /* comment */
        buf16(z, 0);          /* disk */
        buf16(z, 0);          /* internal attributes */
        buf32(z, 0);          /* external attributes */
        buf32(z, ents[i].off);
        bufstr(z, ents[i].name);

    }
    dirlen = z->len-dir; /* taken before the record below adds to it */
    buf32(z, 0x06054b50); /* end of central directory */
    buf16(z, 0);          /* disk */
    buf16(z, 0);          /* disk with directory */
    buf16(z, n);
    buf16(z, n);
    buf32(z, dirlen);
    buf32(z, dir);
    buf16(z, 0);          /* no comment */

}

/* a formula in the ODF form: the references are bracketed and dotted,
   =SUM(A1:A3) becoming of:=SUM([.A1:.A3]) */
static void odfformula(buffer* b, const char* f)

{

    const char* p = f+1; /* past the = */
    ami_long x, y;

    bufstr(b, "of:=");
    while (*p) {

        if (isalpha((unsigned char)*p)) { /* a name, or a reference */

            const char* sp = p;

            pp = p;
            perr = FALSE;
            if (cellref(&x, &y) && !perr) { /* a reference, maybe a range */

                const char* re = pp;

                if (*pp == ':') { /* a range: both ends bracketed */

                    const char* mid = pp;

                    pp++;
                    if (cellref(&x, &y) && !perr) {

                        bufstr(b, "[.");
                        bufput(b, sp, mid-sp);
                        bufstr(b, ":.");
                        bufput(b, mid+1, pp-mid-1);
                        bufstr(b, "]");
                        p = pp;
                        continue;

                    }
                    pp = re; /* not a range after all */

                }
                bufstr(b, "[.");
                bufput(b, sp, re-sp);
                bufstr(b, "]");
                p = re;

                continue;

            }
            /* a function name, which carries over as it stands */
            while (isalpha((unsigned char)*p)) { bufput(b, p, 1); p++; }

            continue;

        }
        bufxml(b, (char[2]){*p, 0});
        p++;

    }

}

/* The same in reverse: of:=SUM([.A1:.A3]) becomes =SUM(A1:A3). The
   result carries its own leading =, whatever the source had. */
static void plainformula(char* d, const char* s, int dl)

{

    int i = 0;
    const char* b;

    if (!strncmp(s, "of:", 3)) s += 3;
    if (*s != '=' && i < dl-1) d[i++] = '='; /* it must lead with one */
    b = s;
    while (*s && i < dl-1) {

        /* the brackets and the sheet marks come off; a dot only where it
           marks a reference, never one inside a number */
        if (*s == '[' || *s == ']' || *s == '$') s++;
        else if (*s == '.' && (s == b || !isdigit((unsigned char)s[-1]))) s++;
        else d[i++] = *s++;

    }
    d[i] = 0;

}

/* the content.xml of the sheet as it stands */
static void odscontent(buffer* b)

{

    ami_long x, y, lastx, lasty, gap;
    char s[CELLEN];
    cellrec* c;

    bufstr(b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    bufstr(b, "<office:document-content "
        "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
        "xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" "
        "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
        "xmlns:of=\"urn:oasis:names:tc:opendocument:xmlns:of:1.2\" "
        "office:version=\"1.2\">"
        "<office:body><office:spreadsheet>"
        "<table:table table:name=\"Sheet1\">");
    /* the last row and column that hold anything bound the writing */
    lastx = lasty = -1;
    for (y = 0; y < ROWS; y++) for (x = 0; x < COLS; x++)
        if (cells[y][x].text) { if (y > lasty) lasty = y;
                                if (x > lastx) lastx = x; }
    bufstr(b, "<table:table-column table:number-columns-repeated=\"");
    sprintf(s, "%d", lastx+1 > 0? lastx+1: 1);
    bufstr(b, s);
    bufstr(b, "\"/>");
    for (y = 0; y <= lasty; y++) {

        bufstr(b, "<table:table-row>");
        gap = 0;
        for (x = 0; x <= lastx; x++) {

            c = &cells[y][x];
            if (!c->text) { gap++; continue; } /* empty cells go as a run */
            if (gap) {

                bufstr(b, "<table:table-cell table:number-columns-repeated=\"");
                sprintf(s, "%d", gap);
                bufstr(b, s);
                bufstr(b, "\"/>");
                gap = 0;

            }
            bufstr(b, "<table:table-cell");
            if (c->text[0] == '=') { /* a formula, with its last value */

                bufstr(b, " table:formula=\"");
                odfformula(b, c->text);
                bufstr(b, "\"");

            }
            if (c->text[0] == '=' || c->isnum) { /* a value */

                bufstr(b, " office:value-type=\"float\" office:value=\"");
                sprintf(s, "%.15g", c->val);
                bufstr(b, s);
                bufstr(b, "\">");
                celldsp(x, y, s, sizeof(s));
                bufstr(b, "<text:p>");
                bufxml(b, s);
                bufstr(b, "</text:p>");

            } else { /* text */

                bufstr(b, " office:value-type=\"string\">");
                bufstr(b, "<text:p>");
                bufxml(b, c->text);
                bufstr(b, "</text:p>");

            }
            bufstr(b, "</table:table-cell>");

        }
        bufstr(b, "</table:table-row>");

    }
    bufstr(b, "</table:table></office:spreadsheet></office:body>"
              "</office:document-content>");

}

static void savesheet(const char* fn)

{

    buffer z = { NULL, 0, 0 };
    buffer c = { NULL, 0, 0 };
    zipent ents[3];
    FILE*  f;
    static const char* manifest =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<manifest:manifest "
        "xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" "
        "manifest:version=\"1.2\">"
        "<manifest:file-entry manifest:full-path=\"/\" "
        "manifest:media-type=\"application/vnd.oasis.opendocument.spreadsheet\"/>"
        "<manifest:file-entry manifest:full-path=\"content.xml\" "
        "manifest:media-type=\"text/xml\"/>"
        "</manifest:manifest>";
    static const char* mime =
        "application/vnd.oasis.opendocument.spreadsheet";

    odscontent(&c);
    /* the mimetype goes first and stored, which is how the format is
       recognized without unpacking it */
    zipadd(&z, &ents[0], "mimetype", mime, strlen(mime));
    zipadd(&z, &ents[1], "META-INF/manifest.xml", manifest, strlen(manifest));
    zipadd(&z, &ents[2], "content.xml", c.p, c.len);
    zipend(&z, ents, 3);
    f = fopen(fn, "wb");
    if (!f) { ami_alert("spreadsheet", "Cannot write file"); }
    else {

        fwrite(z.p, 1, z.len, f);
        fclose(f);
        modified = FALSE;
        strncpy(filename, fn, sizeof(filename)-1);

    }
    free(z.p);
    free(c.p);

}

/* Find content.xml in the zip and give it back inflated.
   The read goes by the central directory, not by walking the local
   headers: a writer may stream a member, leaving the sizes zero in its
   local header and putting them in a descriptor after the data, which
   is what LibreOffice does, and a walk of local headers then loses its
   place. The central directory always carries the sizes and the offset
   of each member. */
static char* odsread(const char* fn)

{

    FILE*  f;
    buffer raw = { NULL, 0, 0 };
    char   buf[4096];
    size_t n;
    int    i, cd, cnt, ent;
    char*  out = NULL;
    unsigned char* d;

    f = fopen(fn, "rb");
    if (!f) return (NULL);
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) bufput(&raw, buf, n);
    fclose(f);
    d = (unsigned char*)raw.p;
    /* the end of central directory record, last in the file */
    cd = -1;
    for (i = (int)raw.len-22; i >= 0; i--)
        if (d[i] == 0x50 && d[i+1] == 0x4b && d[i+2] == 0x05 &&
            d[i+3] == 0x06) { cd = i; break; }
    if (cd < 0) { free(raw.p); return (NULL); } /* not a zip */
    cnt = d[cd+10]|d[cd+11] << 8;
    ent = d[cd+16]|d[cd+17] << 8|(int)d[cd+18] << 16|(int)d[cd+19] << 24;
    /* the directory entries, looking for content.xml */
    for (i = 0; i < cnt && ent+46 <= (int)raw.len; i++) {

        unsigned char* p = d+ent;
        int method, nlen, elen, clen, csize, usize, loc;

        if (!(p[0] == 0x50 && p[1] == 0x4b && p[2] == 0x01 && p[3] == 0x02))
            break;
        method = p[10]|p[11] << 8;
        csize = p[20]|p[21] << 8|(int)p[22] << 16|(int)p[23] << 24;
        usize = p[24]|p[25] << 8|(int)p[26] << 16|(int)p[27] << 24;
        nlen = p[28]|p[29] << 8;
        elen = p[30]|p[31] << 8;
        clen = p[32]|p[33] << 8;
        loc = p[42]|p[43] << 8|(int)p[44] << 16|(int)p[45] << 24;
        if (nlen == 11 && !memcmp(p+46, "content.xml", 11)) {

            unsigned char* lp = d+loc;
            char* data;

            if (loc < 0 || loc+30 > (int)raw.len) break;
            /* the local header carries its own name and extra lengths */
            data = (char*)lp+30+(lp[26]|lp[27] << 8)+(lp[28]|lp[29] << 8);
            if (method == 0) { /* stored */

                out = malloc(csize+1);
                if (out) { memcpy(out, data, csize); out[csize] = 0; }

            } else if (method == 8) { /* deflated */

                z_stream zs;

                out = malloc(usize+1);
                if (out) {

                    memset(&zs, 0, sizeof(zs));
                    zs.next_in = (Bytef*)data;
                    zs.avail_in = csize;
                    zs.next_out = (Bytef*)out;
                    zs.avail_out = usize;
                    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK ||
                        inflate(&zs, Z_FINISH) < 0) { free(out); out = NULL; }
                    else out[zs.total_out] = 0;
                    inflateEnd(&zs);

                }

            }
            break;

        }
        ent += 46+nlen+elen+clen;

    }
    free(raw.p);

    return (out);

}

/* the text of an xml attribute, or NULL: the tag is one element */
static int xmlatt(const char* tag, const char* name, char* d, int dl)

{

    const char* p = tag;
    int n = strlen(name);
    int i = 0;

    while ((p = strstr(p, name))) {

        if ((p == tag || p[-1] == ' ') && p[n] == '=' && p[n+1] == '"') {

            p += n+2;
            while (*p && *p != '"' && i < dl-1) {

                if (!strncmp(p, "&amp;", 5)) { d[i++] = '&'; p += 5; }
                else if (!strncmp(p, "&lt;", 4)) { d[i++] = '<'; p += 4; }
                else if (!strncmp(p, "&gt;", 4)) { d[i++] = '>'; p += 4; }
                else if (!strncmp(p, "&quot;", 6)) { d[i++] = '"'; p += 6; }
                else if (!strncmp(p, "&apos;", 6)) { d[i++] = '\''; p += 6; }
                else d[i++] = *p++;

            }
            d[i] = 0;

            return (TRUE);

        }
        p += n;

    }

    return (FALSE);

}

/* the text of the <text:p> runs inside a cell element */
static void xmltext(const char* cell, const char* end, char* d, int dl)

{

    const char* p = cell;
    int i = 0;

    while ((p = strstr(p, "<text:p")) && p < end) {

        p = strchr(p, '>');
        if (!p) break;
        p++;
        while (*p && *p != '<' && i < dl-1) {

            if (!strncmp(p, "&amp;", 5)) { d[i++] = '&'; p += 5; }
            else if (!strncmp(p, "&lt;", 4)) { d[i++] = '<'; p += 4; }
            else if (!strncmp(p, "&gt;", 4)) { d[i++] = '>'; p += 4; }
            else if (!strncmp(p, "&quot;", 6)) { d[i++] = '"'; p += 6; }
            else if (!strncmp(p, "&apos;", 6)) { d[i++] = '\''; p += 6; }
            else d[i++] = *p++;

        }

    }
    d[i] = 0;

}

static void loadsheet(const char* fn)

{

    char* xml;
    char* p;
    ami_long x = 0, y = -1;
    char  att[CELLEN], txt[CELLEN];
    FILE* t;

    /* a name that is not there yet is a new sheet to be written there,
       which is how a sheet is started from the command line */
    t = fopen(fn, "rb");
    if (!t) {

        clearsheet();
        strncpy(filename, fn, sizeof(filename)-1);

        return;

    }
    fclose(t);
    xml = odsread(fn);
    if (!xml) { ami_alert("spreadsheet", "Not a spreadsheet file"); return; }
    clearsheet();
    p = xml;
    while ((p = strpbrk(p, "<"))) {

        if (!strncmp(p, "<table:table-row", 16)) { /* a row, maybe repeated */

            char* e = strchr(p, '>');
            int   rep = 1;

            if (!e) break;
            *e = 0; /* the element alone, for the attribute search */
            /* a run of like rows is written once and counted, which is
               how the empty rows between entries arrive */
            if (xmlatt(p, "table:number-rows-repeated", att, sizeof(att)))
                rep = atol(att);
            *e = '>';
            y += rep;
            x = 0;
            p = e;

        } else if (!strncmp(p, "<table:table-cell", 17)) {

            char* e = strchr(p, '>');
            char* close;
            int   rep = 1;
            int   empty = TRUE;
            int   selfend; /* the element closes itself: it holds nothing */

            if (!e) break;
            selfend = e > p && e[-1] == '/';
            *e = 0; /* the element alone, for the attribute search */
            if (xmlatt(p, "table:number-columns-repeated", att, sizeof(att)))
                rep = atol(att);
            if (xmlatt(p, "table:formula", att, sizeof(att))) {

                char f[CELLEN];

                plainformula(f, att, sizeof(f));
                if (y >= 0 && y < ROWS && x < COLS) {

                    cells[y][x].text = malloc(strlen(f)+1);
                    if (cells[y][x].text) strcpy(cells[y][x].text, f);

                }
                empty = FALSE;

            } else if (xmlatt(p, "office:value", att, sizeof(att))) {

                if (y >= 0 && y < ROWS && x < COLS) {

                    cells[y][x].text = malloc(strlen(att)+1);
                    if (cells[y][x].text) strcpy(cells[y][x].text, att);

                }
                empty = FALSE;

            }
            *e = '>';
            if (empty && !selfend) { /* text, if it holds any */

                /* An element that closes itself holds nothing. Looking
                   for its closing tag found the next cell's instead, and
                   the empty cell took that cell's text -- which is how a
                   value appeared in the cells before it, and how a row
                   took the text of a row below. */
                close = strstr(e, "</table:table-cell>");
                if (!close) close = e+1;
                xmltext(e, close, txt, sizeof(txt));
                if (*txt && y >= 0 && y < ROWS && x < COLS) {

                    cells[y][x].text = malloc(strlen(txt)+1);
                    if (cells[y][x].text) strcpy(cells[y][x].text, txt);

                }

            }
            x += rep;
            p = e+1;

        } else p++;

    }
    free(xml);
    recalc();
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
static ami_long fullscale(int num, int den)

{

    if (den < 1 || num <= 0) return (0);
    if (num >= den) return (LONG_MAX);

    return (LONG_MAX/den*num); /* num < den, so this cannot overflow */

}

/* Set a scroll bar from the view: the thumb is the visible share of the
   sheet, at the scrolled position. */
static void setbar(int id, int org, int vis, int total)

{

    int max = total-vis;

    if (max < 1) max = 1;
    ami_scrollsiz(stdout, id, fullscale(vis, total));
    ami_scrollpos(stdout, id, fullscale(org, max));

}

/* a cell offset from a bar position, over the given travel */
static int barcell(ami_long pos, int travel)

{

    if (travel < 1 || pos <= 0) return (0);
    if (pos >= LONG_MAX) return (travel);

    return ((int)((double)travel*((double)pos/LONG_MAX)+0.5));

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
        "layout: window %lldx%lld grid %d,%d-%d,%d cells %dx%d "
        "bars: vert at %d,%d %lldx%d horiz at %d,%d %dx%lld\n",
        AMI_LONG_CAST(ami_maxxg(stdout)), AMI_LONG_CAST(ami_maxyg(stdout)), gridx0, gridy0, gridx1, gridy1,
        cellx, celly, gridx1+1, gridy0, AMI_LONG_CAST(sbw), gridy1-gridy0,
        gridx0, gridy1+1, gridx1-gridx0, AMI_LONG_CAST(sbh));

}

/* the pixel origin of a displayed cell */
static void cellpos(ami_long x, ami_long y, ami_long* px, ami_long* py)

{

    *px = hdrw+(x-orgx)*colw+1;
    *py = rowh*2+hdrh+(y-orgy)*rowh+1;

}

/* A text cell prints past its own cell, into the empty cells to its
   right, so a title need not fit the column it starts in. The run ends
   at the first cell holding anything, or at the edge of the grid.
   Numbers do not run on: a number that does not fit is not a number the
   reader can trust, so it shows as # marks instead. */
static ami_long spillwid(ami_long x, ami_long y)

{

    int w = colw;
    int i;
    ami_long px, py;

    for (i = x+1; i < COLS; i++) {

        cellpos(i, y, &px, &py);
        if (px > gridx1) break; /* the edge of the grid */
        if (cells[y][i].text) break; /* the next cell holds something */
        w += colw;

    }
    cellpos(x, y, &px, &py);
    if (px+w-2 > gridx1) w = gridx1-px+2; /* clipped to the grid */

    return (w);

}

/* clip a string to a pixel width, in place */
static void clipstr(char* s, int w)

{

    int i = strlen(s);

    while (i && ami_strsiz(stdout, s) > w) s[--i] = 0;

}

/* The face of one cell. The faces of a row go down before any of its
   contents, so that text running past its own cell is not painted over
   by the cells it runs into. */
static void drawface(ami_long x, ami_long y)

{

    ami_long px, py;

    if (x < orgx || y < orgy) return;
    cellpos(x, y, &px, &py);
    if (px > gridx1 || py > gridy1) return;
    /* the current cell is marked */
    if (x == curx && y == cury) ami_fcolor(stdout, ami_cyan);
    else ami_fcolor(stdout, ami_white);
    ami_frect(stdout, px, py, px+colw-2, py+rowh-2);

}

/* the contents of one cell */
static void drawcell(ami_long x, ami_long y)

{

    ami_long px, py;
    char s[CELLEN];
    int tw;
    int  val; /* the cell shows a value, not text */

    if (x < orgx || y < orgy) return;
    cellpos(x, y, &px, &py);
    if (px > gridx1 || py > gridy1) return;
    if (entering && x == curx && y == cury) strcpy(s, entry);
    else celldsp(x, y, s, sizeof(s));
    if (!*s) return;
    /* a value keeps to its own cell, text runs on into empty cells */
    val = !entering && cells[y][x].text &&
          (cells[y][x].isnum || cells[y][x].text[0] == '=' ||
           cells[y][x].err);
    if (val) { /* numbers to the right, in their own cell */

        ami_fcolor(stdout, ami_black);
        if (ami_strsiz(stdout, s) > colw-4) { /* it does not fit */

            int i;

            for (i = 0; i < (int)sizeof(s)-1; i++) s[i] = '#';
            s[i] = 0;
            clipstr(s, colw-4);

        }
        tw = ami_strsiz(stdout, s);
        ami_cursorg(stdout, px+colw-3-tw, py+2);

    } else { /* text to the left, running on if it must */

        ami_long w = spillwid(x, y);
        int ex;

        /* clipped first, so what follows measures the text as it will
           actually be drawn */
        clipstr(s, w-4);
        tw = ami_strsiz(stdout, s);
        ex = px+2+tw; /* where the text ends */
        if (ex > px+w-2) ex = px+w-2; /* never past the run */
        if (ex > px+colw-1) {

            /* Clear as far as the text reaches, which takes the grid
               lines under it, as a sheet does where text crosses a cell
               edge. Only that far: clearing the whole run erased the
               lines of every empty cell after the text as well. */
            ami_fcolor(stdout, ami_white);
            /* between the lines above and below, which stay: the run
               crosses the cell edges beside it, not the rows */
            ami_frect(stdout, px+colw-1, py, ex, py+rowh-2);

        }
        ami_fcolor(stdout, ami_black);
        ami_cursorg(stdout, px+2, py+2);

    }
    fprintf(stdout, "%s", s);

}

/* the whole window: entry line, headers, cells, grid */
static void drawall(void)

{

    ami_long x, y, px, py;
    char s[CELLEN+40];
    char nam[16];

    int nx = (gridx1-gridx0+colw-1)/colw+1; /* cells to cover the area */
    int ny = (gridy1-gridy0+rowh-1)/rowh+1;

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
        sprintf(s, "%lld", AMI_LONG_CAST(y+1));
        ami_cursorg(stdout, hdrw-2-ami_strsiz(stdout, s), py+2);
        fprintf(stdout, "%s", s);

    }
    /* The grid lines, stopped at the grid area. They go down before the
       cells, so text that runs past its own cell can take the lines it
       crosses with it. */
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
    /* the cells, faces first so a run is not painted over */
    for (y = orgy; y < orgy+ny && y < ROWS; y++) {

        for (x = orgx; x < orgx+nx && x < COLS; x++) drawface(x, y);
        for (x = orgx; x < orgx+nx && x < COLS; x++) drawcell(x, y);

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

    int ox = orgx, oy = orgy;

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
static void scrollto(int nx, int ny, int frombar)

{

    int ox = orgx, oy = orgy;

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

Help topics

A window of the program's own, not a dialog. A dialog stops the program
and runs a loop of its own until it is answered; help is not a question,
so it opens beside the sheet and stays up while the sheet is worked on.

That takes no machinery. There is one event queue for the program, and
every event names the window it came from, so the main loop tells the
help window's events from the sheet's by their window id and hands them
here. Nothing is nested, and no thread is needed -- though one could be
put in charge of this window just as well, since its state is all here.

Widgets are numbered within their window, so the search entry, the topic
list and the close button are 1, 2 and 3 here even though the sheet's
scroll bars are 1 and 2 there.

*******************************************************************************/

/* A topic is a title and the text under it, both pointing into the
   block the help file was read into. */
typedef struct { char* title; char* text; } helprec;

/* The wrapped text, one entry per line as it appears on the screen. The
   text is wrapped once, when the topic is picked or the window resized,
   and drawn from there, which is what makes it scrollable. */
typedef struct { char* s; int bold; int ind; } helpline;

static FILE*     helpwf;      /* the help window, NULL when closed */
static char*     helpbuf;     /* the help file, read whole */
static helprec*  helptopics;  /* the topics in it */
static int       helptopicct;
static int*     helpmatch;   /* the topics the search matched */
static int       helpmatches; /* how many of them */
static int       helpsel;     /* the topic shown, -1 for none */
static int       helpx0, helpy0; /* the topic list, in pixels */
static int       helpx1, helpy1;
static int       helplistup;  /* the list box has been made */
static helpline* helplines;   /* the topic, wrapped to the pane */
static int       helplinect;
static int       helplinemax;
static int       helptop;     /* first wrapped line shown */
static int       helppage;    /* wrapped lines the pane holds */
static char*     helpprog;    /* argv[0], to find the help file by */

static void helpout(const char* s, int bold, int ind); /* forward */

/*******************************************************************************

Reading the help file

The file is markdown, of the plain kind: a line beginning with a single
# names a topic and everything after it belongs to that topic until the
next one or the end of the file. Within a topic a blank line separates
paragraphs, ## is a heading inside the topic and - is a list item.

Keeping the text in a file rather than in the program means the help can
be rewritten, corrected or translated without a compiler, and it can be
as long as it deserves to be. The file is read whole and the titles and
texts point into it, so a topic costs two pointers.

*******************************************************************************/

/* is this the start of a topic: one #, then a space, then a title? */
static int helphead(const char* p)

{

    return (*p == '#' && p[1] != '#');

}

/* Find and read the help file. It is looked for beside the program
   first, since that is where an installed program's own files belong,
   then in the source directory it is kept in, then where the program
   was run from. The first one found is the one used. */
static int helpread(void)

{

    char  path[600];
    char  dir[500];
    char* e;
    FILE* f = NULL;
    int   i, n;

    /* the directory the program was run from, with its slash */
    dir[0] = 0;
    if (helpprog) {

        snprintf(dir, sizeof(dir), "%s", helpprog);
        e = strrchr(dir, '/');
        if (e) e[1] = 0; else dir[0] = 0;

    }
    for (i = 0; i < 4 && !f; i++) {

        switch (i) {

            /* beside the program, then the source beside that, which is
               where it sits in a build tree, then the two the same way
               from wherever the program was run */
            case 0: snprintf(path, sizeof(path), "%s%s", dir, HELPFILE);
                    break;
            case 1: snprintf(path, sizeof(path), "%s../graph_programs/%s",
                             dir, HELPFILE); break;
            case 2: snprintf(path, sizeof(path), "%s", HELPFILE); break;
            default: snprintf(path, sizeof(path), "graph_programs/%s",
                              HELPFILE); break;

        }
        f = fopen(path, "r");
        if (diag) fprintf(stderr, "help: %s: %s\n", path, f? "found": "no");

    }
    if (!f) return (FALSE);
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    helpbuf = malloc(n+1);
    if (!helpbuf) { ami_alert("spreadsheet", "Out of memory"); exit(1); }
    n = fread(helpbuf, 1, n, f);
    helpbuf[n] = 0;
    fclose(f);

    return (TRUE);

}

/* Break the file into topics. The heads are found first and the block
   cut afterwards, in that order: cutting as it went would overwrite the
   newline that the test for the next head stands on, and every other
   topic would be passed over. */
static void helpsplit(void)

{

    char*  p;
    char** head; /* the # of each topic */
    int    n, i;

    /* count the heads, then take them, walking by lines both times */
    n = 0;
    for (p = helpbuf; *p; ) {

        if (helphead(p)) n++;
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }
    head = malloc((n+1)*sizeof(char*));
    helptopics = malloc((n+1)*sizeof(helprec));
    helpmatch = malloc((n+1)*sizeof(int));
    if (!head || !helptopics || !helpmatch)
        { ami_alert("spreadsheet", "Out of memory"); exit(1); }
    n = 0;
    for (p = helpbuf; *p; ) {

        if (helphead(p)) head[n++] = p;
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }
    helptopicct = n;
    /* the title is the rest of the head line, the text is what follows */
    for (i = 0; i < n; i++) {

        char* t = head[i]+1;
        char* e;

        while (*t == ' ') t++;
        helptopics[i].title = t;
        e = t;
        while (*e && *e != '\n') e++;
        if (*e) *e++ = 0; /* end the title */
        while (*e == '\n') e++; /* and the blank line under it */
        helptopics[i].text = e;

    }
    /* Now the cutting: each text ends where the next topic begins. The
       last ends at the end of the file, which is already a zero. */
    for (i = 0; i+1 < n; i++) *head[i+1] = 0;

}

/* load the help, once, and say so if there is none */
static void helpload(void)

{

    static char nofile[400];

    if (helptopics) return; /* already loaded */
    if (helpread()) helpsplit();
    if (!helptopicct) { /* no file, or a file with no topics in it */

        snprintf(nofile, sizeof(nofile),
                 "The help file %s was not found, or holds no topics.\n"
                 "\n"
                 "It is looked for beside the program and in the "
                 "graph_programs directory of the source. Help is kept in "
                 "a file so that it can be changed without rebuilding the "
                 "program; if the file is missing, only the help is.",
                 HELPFILE);
        helptopics = malloc(sizeof(helprec));
        helpmatch = malloc(sizeof(int));
        if (!helptopics || !helpmatch)
            { ami_alert("spreadsheet", "Out of memory"); exit(1); }
        helptopics[0].title = "No help file";
        helptopics[0].text = nofile;
        helptopicct = 1;

    }

}

/*******************************************************************************

The topic list

*******************************************************************************/

/* How many times does the topic hold the text, in either case? A count
   rather than a yes or no, because the list shows it: a topic the word
   is the subject of holds it many times, and one that merely mentions
   it in passing holds it once, and the reader can tell them apart
   without opening either. */
static int helpcount(const helprec* h, const char* what)

{

    const char* p;
    int         n = strlen(what);
    int         c = 0;

    if (!n) return (0); /* an empty search matches everything, uncounted */
    for (p = h->title; *p; p++)
        if (!strncasecmp(p, what, n)) c++;
    for (p = h->text; *p; p++)
        if (!strncasecmp(p, what, n)) c++;

    return (c);

}

/* Build the list of topics matching the search and put it in the list
   box. The list box is made again rather than changed, since a list box
   is given its contents when it is made; it copies them, so the list
   built here is ours to free. */
static void helpfill(const char* what)

{

    ami_strptr sl = NULL, sp, lp = NULL;
    int        i, c;
    char       lab[300];

    /* the strings are ours until the list box has them; it copies */
    helpmatches = 0;
    for (i = 0; i < helptopicct; i++) {

        c = helpcount(&helptopics[i], what);
        if (*what && !c) continue; /* not this one */
        /* the count goes beside the title, so that a topic the word is
           the subject of can be told from one that mentions it once */
        if (*what) snprintf(lab, sizeof(lab), "%s (%d)",
                            helptopics[i].title, c);
        else snprintf(lab, sizeof(lab), "%s", helptopics[i].title);
        sp = malloc(sizeof(ami_strrec));
        if (!sp) { ami_alert("spreadsheet", "Out of memory"); exit(1); }
        sp->str = strdup(lab);
        if (!sp->str) { ami_alert("spreadsheet", "Out of memory"); exit(1); }
        sp->next = NULL;
        if (lp) lp->next = sp; else sl = sp;
        lp = sp;
        helpmatch[helpmatches++] = i;

    }
    /* a search that matches nothing still needs a list, or there would
       be no box to type the next search against */
    if (!sl) {

        sl = malloc(sizeof(ami_strrec));
        if (!sl) { ami_alert("spreadsheet", "Out of memory"); exit(1); }
        sl->str = strdup("(no topic matches)");
        if (!sl->str) { ami_alert("spreadsheet", "Out of memory"); exit(1); }
        sl->next = NULL;

    }
    if (helplistup) ami_killwidget(helpwf, HELPLIST);
    ami_listboxg(helpwf, helpx0, helpy0, helpx1, helpy1, sl, HELPLIST);
    helplistup = TRUE;
    while (sl) { sp = sl->next; free(sl->str); free(sl); sl = sp; }
    /* the topic shown is only still shown if the search kept it */
    if (helpsel >= 0) {

        for (i = 0; i < helpmatches; i++) if (helpmatch[i] == helpsel) break;
        if (i >= helpmatches) helpsel = -1;

    }

}

/*******************************************************************************

Laying the topic out

The text is wrapped to the pane once and kept as lines, so that drawing
it, scrolling it and knowing how much of it there is are all the same
small piece of work. It is wrapped again when the window is resized,
which is the only thing that can change the answer.

*******************************************************************************/

/* keep one finished line */
static void helpout(const char* s, int bold, int ind)

{

    if (helplinect >= helplinemax) {

        helplinemax = helplinemax? helplinemax*2: 100;
        helplines = realloc(helplines, helplinemax*sizeof(helpline));
        if (!helplines) { ami_alert("spreadsheet", "Out of memory"); exit(1); }

    }
    helplines[helplinect].s = strdup(s);
    if (!helplines[helplinect].s)
        { ami_alert("spreadsheet", "Out of memory"); exit(1); }
    helplines[helplinect].bold = bold;
    helplines[helplinect].ind = ind;
    helplinect++;

}

/* Wrap one paragraph, which arrives as a single string with its line
   breaks already turned into spaces. The break goes at the last word
   that still fits, and fitting is measured with the font rather than
   counted in characters, since the font is not fixed pitch. */
static void helpwrap(const char* s, int bold, int ind, int w)

{

    char line[500];
    char try[500];
    int n;

    ami_bold(helpwf, bold);
    while (*s) {

        const char* q = s;

        n = 0;
        line[0] = 0;
        while (*q) { /* as many whole words as fit */

            const char* e = q;
            int         m;

            while (*e && *e != ' ') e++; /* the next word */
            m = e-s;
            if (m >= (int)sizeof(try)) break;
            memcpy(try, s, m);
            try[m] = 0;
            if (n && ami_strsiz(helpwf, try) > w-ind) break;
            strcpy(line, try);
            n = m;
            q = e;
            while (*q == ' ') q++;
            if (!*e) break;

        }
        if (!n) { /* one word wider than the pane: put it down anyway */

            while (*q && *q != ' ') q++;
            n = q-s;
            if (n >= (int)sizeof(line)) n = sizeof(line)-1;
            memcpy(line, s, n);
            line[n] = 0;

        }
        helpout(line, bold, ind);
        s += n;
        while (*s == ' ') s++;

    }
    ami_bold(helpwf, FALSE);

}

/* Wrap the topic being shown to the pane. The markdown is read here: a
   blank line ends a paragraph, ## is a heading within the topic, and -
   is a list item, which is wrapped with its later lines lined up under
   the first word rather than under the dash. */
static void helplay1(int w)

{

    const char* p;
    char        para[4000];
    int         pl = 0;
    int         bold = FALSE;
    int         ind = 0;
    int         i;

    for (i = 0; i < helplinect; i++) free(helplines[i].s);
    helplinect = 0;
    helptop = 0;
    if (helpsel < 0) { helpout("Pick a topic.", FALSE, 0); return; }
    /* the title of the topic, in bold, and a line under it */
    helpout(helptopics[helpsel].title, TRUE, 0);
    helpout("", FALSE, 0);
    p = helptopics[helpsel].text;
    while (1) {

        const char* e = p;
        int         n;

        while (*e && *e != '\n') e++;
        n = e-p;
        while (n && (p[n-1] == ' ' || p[n-1] == '\r')) n--; /* trailing */
        if (!n || *p == '#' || *p == '-' || *p == '*' || !*p) {

            /* the line before is finished, whatever this one is */
            if (pl) { para[pl] = 0; helpwrap(para, bold, ind, w); pl = 0; }
            bold = FALSE;
            ind = 0;

        }
        if (!*p) break;
        if (!n) helpout("", FALSE, 0); /* a blank line stays blank */
        else if (*p == '#') { /* a heading inside the topic */

            const char* t = p;

            while (*t == '#') t++;
            while (*t == ' ') t++;
            bold = TRUE;
            n -= t-p;
            if (n > (int)sizeof(para)-1) n = sizeof(para)-1;
            memcpy(para, t, n);
            pl = n;

        } else if (*p == '-' || *p == '*') { /* a list item */

            ind = ami_strsiz(helpwf, "00");
            if (n > (int)sizeof(para)-1) n = sizeof(para)-1;
            memcpy(para, p, n);
            pl = n;

        } else { /* ordinary text, joined to the line before it */

            if (pl && pl < (int)sizeof(para)-1) para[pl++] = ' ';
            if (pl+n > (int)sizeof(para)-1) n = sizeof(para)-1-pl;
            memcpy(para+pl, p, n);
            pl += n;

        }
        p = *e? e+1: e;

    }

}

/* draw the topic from the line it is scrolled to */
static void helpdraw(void)

{

    ami_long chrh = ami_chrsizy(helpwf);
    ami_long x = helpx1+ami_strsiz(helpwf, "00");
    ami_long y = helpy0;
    int i;

    /* down to and including the line the count is written on, which is
       under the pane: leave it out and each count is written over the
       one before it */
    ami_fcolor(helpwf, ami_white);
    ami_frect(helpwf, x, helpy0, ami_maxxg(helpwf), helpy1+chrh);
    ami_fcolor(helpwf, ami_black);
    helppage = (helpy1-helpy0)/chrh;
    if (helppage < 1) helppage = 1;
    if (helptop > helplinect-helppage) helptop = helplinect-helppage;
    if (helptop < 0) helptop = 0;
    for (i = helptop; i < helplinect && y+chrh <= helpy1; i++) {

        if (*helplines[i].s) {

            ami_bold(helpwf, helplines[i].bold);
            ami_cursorg(helpwf, x+helplines[i].ind, y);
            fprintf(helpwf, "%s", helplines[i].s);
            ami_bold(helpwf, FALSE);

        }
        y += chrh;

    }
    /* say there is more, since a pane with no bar gives no other sign */
    if (helplinect > helppage) {

        char more[80];

        if (helptop+helppage >= helplinect) strcpy(more, "-- end --");
        else sprintf(more, "-- %d more line%s, wheel or page keys --",
                     helplinect-helptop-helppage,
                     helplinect-helptop-helppage == 1? "": "s");
        ami_fcolor(helpwf, ami_blue);
        ami_cursorg(helpwf, x, helpy1);
        fprintf(helpwf, "%s", more);
        ami_fcolor(helpwf, ami_black);

    }

}

/* wrap and draw, which is what everything that changes the topic wants */
static void helptext(void)

{

    helplay1(ami_maxxg(helpwf)-ami_strsiz(helpwf, "00")-
             (helpx1+ami_strsiz(helpwf, "00")));
    helpdraw();

}

/* scroll the topic by so many lines */
static void helpscroll(int by)

{

    int was = helptop;

    helptop += by;
    if (helptop > helplinect-helppage) helptop = helplinect-helppage;
    if (helptop < 0) helptop = 0;
    if (helptop != was) helpdraw();

}

/* place the widgets and work out the panes, on opening and on resize */
static void helplay(void)

{

    ami_long chrh = ami_chrsizy(helpwf);
    ami_long chrw = ami_strsiz(helpwf, "0");
    int lw   = chrw*30;  /* the topic list */
    ami_long bw, bh, ew, eh;

    ami_buttonsizg(helpwf, "Close", &bw, &bh);
    ami_editboxsizg(helpwf, "0", &ew, &eh);
    /* the search entry along the top of the list column */
    helpx0 = chrw*2;
    helpy0 = chrh*2+eh;
    helpx1 = helpx0+lw;
    helpy1 = ami_maxyg(helpwf)-bh-chrh*2;
    if (helpy1 < helpy0+chrh) helpy1 = helpy0+chrh;
    ami_poswidgetg(helpwf, HELPFIND, helpx0+ami_strsiz(helpwf, "Search: "),
                   chrh);
    ami_sizwidgetg(helpwf, HELPFIND,
                   lw-ami_strsiz(helpwf, "Search: "), eh);
    ami_poswidgetg(helpwf, HELPCLOSE, ami_maxxg(helpwf)-bw-chrw*2,
                   ami_maxyg(helpwf)-bh-chrh/2);
    ami_sizwidgetg(helpwf, HELPCLOSE, bw, bh);
    /* The list is moved and sized rather than made again, so that the
       topic picked in it stays picked across a resize. Only a change
       of contents needs it made again, which is what helpfill is for. */
    if (helplistup) {

        ami_poswidgetg(helpwf, HELPLIST, helpx0, helpy0);
        ami_sizwidgetg(helpwf, HELPLIST, helpx1-helpx0, helpy1-helpy0);

    } else helpfill("");
    /* the frame, the label, and the topic */
    fprintf(helpwf, "\f");
    ami_fcolor(helpwf, ami_black);
    ami_cursorg(helpwf, helpx0, chrh);
    fprintf(helpwf, "Search:");
    helptext();

}

/* close the help window, if it is open */
static void helpclose(void)

{

    int i;

    if (!helpwf) return;
    fclose(helpwf);
    helpwf = NULL;
    helplistup = FALSE;
    for (i = 0; i < helplinect; i++) free(helplines[i].s);
    helplinect = 0;

}

/* Open the help window. A second open just brings the one already up to
   the front, as help does everywhere. */
static void helpopen(void)

{

    ami_long wx, wy;

    if (helpwf) { ami_front(helpwf); return; }
    helpload();
    helpsel = -1;
    helptop = 0;
    ami_openwin(&stdin, &helpwf, NULL, HELPWIN);
    ami_title(helpwf, "Spreadsheet help");
    /* Unbuffered, so the window's measurements are the window's. A
       buffered window answers maxxg with the buffer, which is not what
       the layout wants here: this window has nothing to keep. */
    ami_buffer(helpwf, FALSE);
    ami_auto(helpwf, FALSE);
    ami_curvis(helpwf, FALSE);
    ami_font(helpwf, AMI_FONT_SIGN);
    ami_setpoints(helpwf, 12.0);
    ami_binvis(helpwf);
    ami_winclientg(helpwf, ami_strsiz(helpwf, "0")*86, ami_chrsizy(helpwf)*26,
                   &wx, &wy, BIT(ami_wmframe) | BIT(ami_wmsize) |
                             BIT(ami_wmsysbar));
    ami_setsizg(helpwf, wx, wy);
    /* The entry and the button are made once here and moved by the
       layout after. The list is made by the layout, which is where its
       rectangle is known and where it is made again on every search. */
    ami_editboxg(helpwf, 1, 1, 2, 2, HELPFIND);
    ami_buttong(helpwf, 1, 1, 2, 2, "Close", HELPCLOSE);
    helplay();

}

/* An event with the help window's id on it. The main loop hands them
   here and goes on; nothing about the sheet is touched. */
static void helpevent(ami_evtrec* er)

{

    char srch[100];

    switch (er->etype) {

        case ami_etterm:   /* the window was closed, not the program */
        case ami_etbutton: helpclose(); break;

        case ami_etresize:
        case ami_etredraw: helplay(); break;

        case ami_etmouba: /* the wheel, as buttons 4 and 5 */
            if (er->amoubn == 4) helpscroll(-WHEELROWS);
            else if (er->amoubn == 5) helpscroll(WHEELROWS);
            break;

        case ami_etpagu: helpscroll(-(helppage-1)); break;
        case ami_etpagd: helpscroll(helppage-1); break;
        case ami_etup:   helpscroll(-1); break;
        case ami_etdown: helpscroll(1); break;
        case ami_ethome: helpscroll(-helplinect); break;
        case ami_etend:  helpscroll(helplinect); break;

        case ami_etlstbox: /* a topic was picked */
            if (er->lstbsl >= 1 && er->lstbsl <= helpmatches)
                helpsel = helpmatch[er->lstbsl-1];
            helptext();
            break;

        case ami_etedtbox: /* the search was entered */
            ami_getwidgettext(helpwf, HELPFIND, srch, sizeof(srch));
            helpfill(srch);
            helptext();
            break;

        default: break;

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

/* The standard menu, with a Sheet menu of our own. The standard menu
   builds File and Edit, places the program's own entries after them,
   and finishes with Window and Help; only the standard entries this
   program actually implements are asked for. */
static void setupmenu(void)

{

    ami_menuptr ml = NULL; /* our own entries */
    ami_menuptr sh;        /* the sheet menu */
    ami_menuptr mp;
    ami_menuptr sm = NULL; /* the completed menu */

    newmenu(&sh, FALSE, FALSE, OFF, MENUSHEET, "Sheet");
    appendmenu(&ml, sh);
    ami_stdmenu(BIT(AMI_SMNEW) | BIT(AMI_SMOPEN) |
                BIT(AMI_SMSAVE) | BIT(AMI_SMSAVEAS) | BIT(AMI_SMEXIT) |
                BIT(AMI_SMUNDO) | BIT(AMI_SMCUT) | BIT(AMI_SMPASTE) |
                BIT(AMI_SMDELETE) | BIT(AMI_SMFIND) | BIT(AMI_SMGOTO) |
                BIT(AMI_SMHELPTOPIC) | BIT(AMI_SMABOUT),
                &sm, ml);
    /* The sheet menu's entries hang on after the standard menu is
       built: the splice inside it appends the program's entries with a
       routine that clears the branch link, so a submenu built before
       the call does not survive it. */
    newmenu(&mp, FALSE, FALSE, OFF, MENUWIDER, "Wider Columns");
    appendmenu(&sh->branch, mp);
    newmenu(&mp, FALSE, FALSE, ON, MENUNARROW, "Narrower Columns");
    appendmenu(&sh->branch, mp);
    newmenu(&mp, FALSE, FALSE, ON, MENURECALC, "Recalculate");
    appendmenu(&sh->branch, mp);
    ami_menu(stdout, sm);

}

/* find the next cell holding the given text, from the one after the
   current, and go to it */
static void findcell(const char* what)

{

    int i, n = (int)ROWS*COLS;
    ami_long x = curx, y = cury;

    for (i = 0; i < n; i++) { /* every cell once, from the next */

        if (++x >= COLS) { x = 0; if (++y >= ROWS) y = 0; }
        if (cells[y][x].text && strstr(cells[y][x].text, what)) {

            curx = x;
            cury = y;
            follow();
            drawall();

            return;

        }

    }
    ami_alert("Find", "No cell holds that");

}

/* go to a cell by name, as A1 or BZ100 */
static void gotocell(const char* name)

{

    ami_long x, y;

    pp = name;
    perr = FALSE;
    if (!cellref(&x, &y) || perr || *pp) {

        ami_alert("Go to", "Not a cell name");

        return;

    }
    curx = x;
    cury = y;
    follow();
    drawall();

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
    ami_long x, y;

    helpprog = argv[0]; /* the help file is looked for beside the program */
    if (argc > 1 && !strcmp(argv[1], "-d")) { /* report what happens */

        diag = TRUE;
        argc--;
        argv++;

    }
    ami_title(stdout, "Spreadsheet");
    /* File/Exit means exit: without this the window is held open after
       main returns, waiting to be closed a second time */
    ami_autohold(FALSE);
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
        /* Every event names the window it came from, so the help window
           is serviced from this same loop: hand it anything of its own
           and go on. A terminate for it closes it, not the program,
           which the loop condition below allows for. */
        if (er.winid == HELPWIN) { helpevent(&er); continue; }
        if (diag) switch (er.etype) { /* the events the bars send */

            case ami_etmouba:
                fprintf(stderr, "click button %lld at %d,%d\n",
                        AMI_LONG_CAST(er.amoubn), mpx, mpy);
                break;
            case ami_etsclull: fprintf(stderr, "scroll line back, bar %lld\n",
                                       AMI_LONG_CAST(er.sclulid)); break;
            case ami_etscldrl: fprintf(stderr, "scroll line on, bar %lld\n",
                                       AMI_LONG_CAST(er.scldrid)); break;
            case ami_etsclulp: fprintf(stderr, "scroll page back, bar %lld\n",
                                       AMI_LONG_CAST(er.sclupid)); break;
            case ami_etscldrp: fprintf(stderr, "scroll page on, bar %lld\n",
                                       AMI_LONG_CAST(er.scldpid)); break;
            case ami_etsclpos: fprintf(stderr, "scroll to %lld, bar %lld\n",
                                       AMI_LONG_CAST(er.sclpos), AMI_LONG_CAST(er.sclpid)); break;
            case ami_etresize: fprintf(stderr, "resize to %lldx%lld\n",
                                       AMI_LONG_CAST(er.rszxg), AMI_LONG_CAST(er.rszyg)); break;
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

            case ami_etmouba: /* a click picks a cell, the wheel scrolls */
                /* The wheel arrives as buttons 4 and 5, which is how X
                   delivers it: a notch is a press and a release of a
                   button that is not there. Scroll the view by a few
                   rows without moving the current cell, as the bars
                   do, which is what a sheet does under the wheel. */
                if (er.amoubn == 4) { scrollto(orgx, orgy-WHEELROWS, 0); break; }
                if (er.amoubn == 5) { scrollto(orgx, orgy+WHEELROWS, 0); break; }
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
                        if (diag) fprintf(stderr, "queryopen: calling\n");
                        ami_queryopen(fn, sizeof(fn));
                        if (diag) fprintf(stderr, "queryopen: returned [%s]\n",
                                          fn);
                        if (*fn) { loadsheet(fn); layout(); drawall(); }
                        if (diag) fprintf(stderr, "queryopen: loaded\n");
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

                    case AMI_SMUNDO: /* the last cell change back */
                        if (undval) {

                            char* t = undtxt;

                            undtxt = NULL; /* setcell takes the current */
                            curx = undx;
                            cury = undy;
                            setcell(undx, undy, t);
                            if (t) free(t);
                            undval = FALSE; /* one change deep */
                            follow();
                            drawall();

                        }
                        break;

                    case AMI_SMCUT: /* the cell to the clipboard */
                        if (clip) free(clip);
                        clip = NULL;
                        if (cells[cury][curx].text) {

                            clip = malloc(strlen(cells[cury][curx].text)+1);
                            if (clip) strcpy(clip, cells[cury][curx].text);

                        }
                        setcell(curx, cury, NULL);
                        drawall();
                        break;

                    case AMI_SMPASTE: /* the clipboard to the cell */
                        if (clip) { setcell(curx, cury, clip); drawall(); }
                        break;

                    case AMI_SMDELETE: /* empty the cell */
                        setcell(curx, cury, NULL);
                        drawall();
                        break;

                    case AMI_SMFIND: { /* the next cell holding text */

                        ami_qfnopts opt = 0;

                        fn[0] = 0;
                        ami_queryfind(fn, sizeof(fn), &opt);
                        if (*fn) findcell(fn);
                        break;

                    }

                    case AMI_SMGOTO: { /* to a cell by name */

                        ami_qfnopts opt = 0;

                        fn[0] = 0;
                        ami_queryfind(fn, sizeof(fn), &opt);
                        if (*fn) gotocell(fn);
                        break;

                    }

                    case AMI_SMHELPTOPIC:
                        helpopen();
                        break;

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

                }
                break;

            default: break;

        }

    /* a terminate for the help window ended that window, not this */
    } while (er.etype != ami_etterm || er.winid == HELPWIN);
    done:
    helpclose();

    return (0);

}
