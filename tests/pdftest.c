/*******************************************************************************
*                                                                              *
*                           PDF PRINT OUTPUT TEST                              *
*                                                                              *
* Exercises the pdfgraph module: opens a print file, runs the character set,   *
* the attributes, the standard fonts, colors and tabs, then the drawing set,   *
* type sizes and justified writes, and the automatic wrap and page eject.      *
* The result is test.pdf in the current directory, for examination with any    *
* .pdf reader.                                                                 *
*                                                                              *
* The page is US letter at 600 DPI, 5100 by 6600, 85 by 66 characters in the   *
* default 12 point terminal font.                                              *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <limits.h>
#include <graphics.h>

/* a labeled figure: the label goes under the figure box */
static void figlab(FILE* p, long x, long y, const char* s)

{

    ami_font(p, AMI_FONT_SIGN);
    ami_fontsiz(p, 80);
    ami_cursorg(p, x, y);
    fprintf(p, "%s", s);
    ami_font(p, AMI_FONT_TERM);
    ami_fontsiz(p, 100);

}

int main(void)

{

    FILE* p;
    long  x, y;
    int   i;

    ami_openprint(&p, "test.pdf");
    ami_title(p, "pdfgraph test document");

    /* ============================ page 1: text ============================ */

    fprintf(p, "pdfgraph test, page 1: text and attributes\n");
    fprintf(p, "page: %ld x %ld characters, %ld x %ld pixels at 600 DPI\n\n",
            ami_maxx(p), ami_maxy(p), ami_maxxg(p), ami_maxyg(p));

    fprintf(p, "plain terminal text: the quick brown fox jumps over the "
               "lazy dog 0123456789\n");
    ami_bold(p, 1);
    fprintf(p, "bold text\n");
    ami_bold(p, 0);
    ami_italic(p, 1);
    fprintf(p, "italic text\n");
    ami_bold(p, 1);
    fprintf(p, "bold italic text\n");
    ami_bold(p, 0);
    ami_italic(p, 0);
    ami_underline(p, 1);
    fprintf(p, "underlined text\n");
    ami_underline(p, 0);
    ami_strikeout(p, 1);
    fprintf(p, "struck out text\n");
    ami_strikeout(p, 0);
    ami_reverse(p, 1);
    fprintf(p, "reversed text\n");
    ami_reverse(p, 0);
    fprintf(p, "superscript: x");
    ami_superscript(p, 1);
    fprintf(p, "2");
    ami_superscript(p, 0);
    fprintf(p, "  subscript: H");
    ami_subscript(p, 1);
    fprintf(p, "2");
    ami_subscript(p, 0);
    fprintf(p, "O\n");
    ami_condensed(p, 1);
    fprintf(p, "condensed text\n");
    ami_condensed(p, 0);
    ami_extended(p, 1);
    fprintf(p, "extended text\n");
    ami_extended(p, 0);
    fprintf(p, "\n");

    /* the standard fonts */
    fprintf(p, "the standard fonts:\n");
    ami_font(p, AMI_FONT_TERM);
    fprintf(p, "terminal: the quick brown fox jumps over the lazy dog\n");
    ami_font(p, AMI_FONT_BOOK);
    fprintf(p, "book: the quick brown fox jumps over the lazy dog\n");
    ami_font(p, AMI_FONT_SIGN);
    fprintf(p, "sign: the quick brown fox jumps over the lazy dog\n");
    ami_font(p, AMI_FONT_TECH);
    fprintf(p, "technical: the quick brown fox jumps over the lazy dog\n");
    ami_font(p, AMI_FONT_TERM);
    fprintf(p, "\n");

    /* colors */
    fprintf(p, "the primary colors: ");
    ami_fcolor(p, ami_red);     fprintf(p, "red ");
    ami_fcolor(p, ami_green);   fprintf(p, "green ");
    ami_fcolor(p, ami_blue);    fprintf(p, "blue ");
    ami_fcolor(p, ami_cyan);    fprintf(p, "cyan ");
    ami_fcolor(p, ami_yellow);  fprintf(p, "yellow ");
    ami_fcolor(p, ami_magenta); fprintf(p, "magenta ");
    ami_fcolor(p, ami_black);   fprintf(p, "black\n");
    ami_bcolor(p, ami_yellow);
    fprintf(p, "black on a yellow background\n");
    ami_bcolor(p, ami_white);
    fprintf(p, "\n");

    /* tabs */
    fprintf(p, "tab stops:\n");
    fprintf(p, "1\t2\t3\t4\t5\t6\t7\n");
    fprintf(p, "one\ttwo\tthree\tfour\tfive\tsix\tseven\n");
    putc('\f', p);

    /* =========================== page 2: drawing ========================== */

    fprintf(p, "pdfgraph test, page 2: the drawing set\n");

    /* framed and filled figures across the top */
    ami_linewidth(p, 6);
    ami_rect(p, 300, 300, 1500, 1100);
    ami_fcolor(p, ami_cyan);
    ami_frect(p, 360, 360, 1440, 1040);
    ami_fcolor(p, ami_black);
    figlab(p, 300, 1150, "rect/frect");

    ami_rrect(p, 1700, 300, 2900, 1100, 250, 250);
    ami_fcolor(p, ami_yellow);
    ami_frrect(p, 1760, 360, 2840, 1040, 250, 250);
    ami_fcolor(p, ami_black);
    figlab(p, 1700, 1150, "rrect/frrect");

    ami_ellipse(p, 3100, 300, 4300, 1100);
    ami_fcolor(p, ami_magenta);
    ami_fellipse(p, 3160, 360, 4240, 1040);
    ami_fcolor(p, ami_black);
    figlab(p, 3100, 1150, "ellipse/fellipse");

    /* lines: widths, then styles */
    ami_linewidth(p, 1);
    ami_line(p, 300, 1500, 4800, 1500);
    ami_linewidth(p, 6);
    ami_line(p, 300, 1600, 4800, 1600);
    ami_linewidth(p, 12);
    ami_line(p, 300, 1700, 4800, 1700);
    ami_linewidth(p, 24);
    ami_line(p, 300, 1800, 4800, 1800);
    ami_linewidth(p, 12);
    ami_linestyle(p, ami_lsdash);
    ami_line(p, 300, 1950, 4800, 1950);
    ami_linestyle(p, ami_lsdot);
    ami_line(p, 300, 2050, 4800, 2050);
    ami_linestyle(p, ami_lssolid);
    figlab(p, 300, 2100, "line widths 1, 6, 12, 24; dashed; dotted");

    /* arcs, pies, chords, triangle */
    ami_linewidth(p, 8);
    ami_arc(p, 300, 2400, 1500, 3600, 0, LONG_MAX/4);
    figlab(p, 300, 3650, "arc, quarter turn");
    ami_fcolor(p, ami_green);
    ami_farc(p, 1700, 2400, 2900, 3600, 0, LONG_MAX/3);
    ami_fcolor(p, ami_black);
    figlab(p, 1700, 3650, "farc, a pie");
    ami_fcolor(p, ami_red);
    ami_fchord(p, 3100, 2400, 4300, 3600, 0, LONG_MAX/3);
    ami_fcolor(p, ami_black);
    figlab(p, 3100, 3650, "fchord");
    ami_fcolor(p, ami_blue);
    ami_ftriangle(p, 300, 3950, 1500, 3950, 900, 4850);
    ami_fcolor(p, ami_black);
    figlab(p, 300, 4900, "ftriangle");

    /* single pixels: a diagonal */
    for (i = 0; i < 400; i++) ami_setpixel(p, 1700+i*2, 3950+i*2);
    figlab(p, 1700, 4900, "setpixel diagonal");

    /* graphically placed text */
    ami_font(p, AMI_FONT_SIGN);
    ami_fontsiz(p, 300);
    ami_cursorg(p, 300, 5400);
    fprintf(p, "Text placed graphically, 36 points");
    ami_font(p, AMI_FONT_TERM);
    ami_fontsiz(p, 100);
    putc('\f', p);

    /* ================= page 3: type sizes and justification =============== */

    fprintf(p, "pdfgraph test, page 3: type sizes and justified writes\n\n");

    ami_font(p, AMI_FONT_SIGN);
    y = 500;
    for (i = 0; i < 6; i++) {

        static const long pts[6] = { 8, 12, 18, 24, 36, 48 };

        ami_fontsiz(p, pts[i]*600/72); /* points to pixels */
        ami_cursorg(p, 300, y);
        fprintf(p, "%ld point sign text", pts[i]);
        y += pts[i]*600/72+100;

    }

    /* a justified block: margin rules, then the lines spread to them */
    ami_font(p, AMI_FONT_BOOK);
    ami_fontsiz(p, 100);
    x = 4500; /* the right margin of the block */
    ami_linewidth(p, 3);
    ami_line(p, 300, y, 300, y+1000);
    ami_line(p, 300+x, y, 300+x, y+1000);
    y += 100;
    ami_cursorg(p, 300, y);
    ami_writejust(p, "The writejust procedure sets a string spread out to a "
                     "given width in pixels.", x);
    y += 130;
    ami_cursorg(p, 300, y);
    ami_writejust(p, "The extra space is divided among the spaces of the "
                     "line, as the margins here show.", x);
    y += 130;
    ami_cursorg(p, 300, y);
    ami_writejust(p, "Justified text against both margins is the mark of set "
                     "type.", x);
    ami_font(p, AMI_FONT_TERM);
    putc('\f', p);

    /* ==================== page 4: wrap and page eject ===================== */

    fprintf(p, "pdfgraph test, page 4: automatic wrap, and the close ejects "
               "this page\n\n");
    for (i = 0; i < 6; i++)
        fprintf(p, "auto wrap check: the quick brown fox jumps over the lazy "
                   "dog, and the line runs off the right edge and wraps. ");
    fprintf(p, "\n\nend of test; this page was not ejected by the program, "
               "the close of the print file ejects it.\n");

    fclose(p);
    printf("test.pdf written\n");

    return (0);

}
