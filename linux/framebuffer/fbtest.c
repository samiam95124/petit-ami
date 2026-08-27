/*******************************************************************************
*                                                                              *
*                        FRAME BUFFER INTERFACE TEST                            *
*                                                                              *
* Exercises framebuffer.h with nothing else: fills the screen, draws borders    *
* and bands and a moving box, all through the three interface calls and direct   *
* writes to the buffer. Run it on a text console -- switch to a virtual          *
* terminal with ctrl+alt+F3 and log in -- where the frame buffer is the actual   *
* display. Under a graphical desktop the compositor owns the screen and nothing  *
* is seen.                                                                       *
*                                                                              *
*     framebuffer/fbtest [seconds]                                              *
*                                                                              *
* It runs for the given number of seconds (default 5) and then exits, leaving    *
* the console as it was.                                                         *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "framebuffer.h"

static long cols, rows, ps, roff, goff, boff;
static unsigned char* fb;

/* set one pixel from r, g, b components, 0..255, for the 3 and 4 byte modes */
static void pix(long x, long y, int r, int g, int b)

{

    unsigned char* p;

    if (x < 0 || x >= cols || y < 0 || y >= rows) return;
    p = fb + (y*cols + x)*ps;
    p[roff] = r; p[goff] = g; p[boff] = b;
    if (ps == 4) p[6-roff-goff-boff] = 255; /* the leftover byte is alpha */

}

static void band(long y0, long y1, int r, int g, int b)

{

    long x, y;

    for (y = y0; y < y1; y++) for (x = 0; x < cols; x++) pix(x, y, r, g, b);

}

int main(int argc, char** argv)

{

    void*  base;
    int    secs = argc > 1 ? atoi(argv[1]) : 5;
    long   x, y, i, bw;
    struct timespec ts = { 0, 16*1000*1000 }; /* ~60 a second */
    time_t t0;

    frame_geometry(&rows, &cols);
    frame_pixsiz(&ps);
    frame_rgboff(&roff, &goff, &boff);
    frame_buffer(&base);
    fb = base;

    fprintf(stderr, "framebuffer: %ld x %ld, %ld bytes per pixel, "
                    "r@%ld g@%ld b@%ld\n", cols, rows, ps, roff, goff, boff);

    /* three colour bands, so the component order is plainly right: a red band,
       a green band, a blue band, top to bottom */
    band(0,          rows/3,     220,  40,  40);
    band(rows/3,   2*rows/3,      40, 200,  60);
    band(2*rows/3,   rows,        50,  70, 230);

    /* a white border a few pixels thick */
    for (i = 0; i < 4; i++) {

        for (x = 0; x < cols; x++) { pix(x, i, 255,255,255); pix(x, rows-1-i, 255,255,255); }
        for (y = 0; y < rows; y++) { pix(i, y, 255,255,255); pix(cols-1-i, y, 255,255,255); }

    }

    /* A yellow box that slides across the middle and back for the run. Only
       the sliver it enters is drawn yellow and the sliver it vacates erased
       to the green band behind it; the body of the box is never rewritten.
       Writing straight to the frame buffer has no vertical sync, so a box
       redrawn whole each frame tears -- the display scans out the erased,
       not-yet-redrawn state and green shows through. Touching only the
       moving edges leaves nothing mid-box to catch half-written. */
    bw = cols/12; if (bw < 20) bw = 20;
    long step = cols/240; if (step < 1) step = 1;
    long pos = 4, dir = 1, lo = 4, hi = cols-bw-4;
    long top = rows/2-bw/3, bot = rows/2+bw/3, np;

    for (y = top; y < bot; y++) for (x = pos; x < pos+bw; x++)
        pix(x, y, 255, 230, 40); /* the box, drawn once to start */
    t0 = time(NULL);
    while (time(NULL)-t0 < secs) {

        np = pos + dir*step;
        if (np <= lo) { np = lo; dir = 1; }
        if (np >= hi) { np = hi; dir = -1; }
        if (np > pos) { /* moving right: draw the leading strip, erase behind */
            for (y = top; y < bot; y++) {
                for (x = pos+bw; x < np+bw; x++) pix(x, y, 255, 230, 40);
                for (x = pos; x < np; x++)       pix(x, y,  40, 200,  60);
            }
        } else if (np < pos) { /* moving left */
            for (y = top; y < bot; y++) {
                for (x = np; x < pos; x++)       pix(x, y, 255, 230, 40);
                for (x = np+bw; x < pos+bw; x++) pix(x, y,  40, 200,  60);
            }
        }
        pos = np;
        nanosleep(&ts, NULL);

    }

    return 0;

}
