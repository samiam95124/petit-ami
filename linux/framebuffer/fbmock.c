/*******************************************************************************
*                                                                              *
*                        MOCK FRAME BUFFER INTERFACE                           *
*                                                                              *
* Presents the framebuffer.h interface over plain memory, with no device at    *
* all, so the frame buffer graphics module can run headless: on a desktop,     *
* under a test harness, anywhere without a console. The "screen" is a          *
* malloc'd 32 bit BGRA buffer of a fixed size; at exit it is written to a      *
* .ppm file so a run's final display can be inspected.                         *
*                                                                              *
*   FBMOCK_SIZE=1280x800   environment: the screen size (default 1920x1080)   *
*   FBMOCK_OUT=file.ppm    environment: the exit dump (default fbmock.ppm,    *
*                          empty string for no dump)                          *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framebuffer.h"

static long           rows = 1080, cols = 1920;
static unsigned char* mem;

static void init_fbmock(void) __attribute__((constructor(102)));
static void init_fbmock(void)

{

    const char* s;
    long r, c;

    s = getenv("FBMOCK_SIZE");
    if (s && sscanf(s, "%ldx%ld", &c, &r) == 2 && c > 0 && r > 0)
        { cols = c; rows = r; }
    mem = calloc((size_t)rows*cols, 4);
    if (!mem) { fprintf(stderr, "*** fbmock: no memory\n"); exit(1); }

}

static void deinit_fbmock(void) __attribute__((destructor(102)));
static void deinit_fbmock(void)

{

    const char* fn;
    FILE* f;
    long x, y;
    unsigned char* p;

    fn = getenv("FBMOCK_OUT");
    if (!fn) fn = "fbmock.ppm";
    if (mem && *fn) {

        f = fopen(fn, "w");
        if (f) {

            fprintf(f, "P6\n%ld %ld\n255\n", cols, rows);
            for (y = 0; y < rows; y++) for (x = 0; x < cols; x++) {

                p = mem+((size_t)y*cols+x)*4;
                fputc(p[2], f); /* r */
                fputc(p[1], f); /* g */
                fputc(p[0], f); /* b */

            }
            fclose(f);

        }

    }
    free(mem);
    mem = NULL;

}

void frame_geometry(long* prows, long* pcolumns)

{

    if (prows) *prows = rows;
    if (pcolumns) *pcolumns = cols;

}

void frame_pixsiz(long* pixsiz)

{

    if (pixsiz) *pixsiz = 4;

}

void frame_buffer(void** buffer)

{

    if (buffer) *buffer = mem;

}

void frame_rgboff(long* roff, long* goff, long* boff)

{

    /* BGRA ascending, the common device layout */
    if (roff) *roff = 2;
    if (goff) *goff = 1;
    if (boff) *boff = 0;

}

void frame_flush(long x, long y, long w, long h)

{

    /* the memory is the display; there is nothing behind it to follow */
    (void)x; (void)y; (void)w; (void)h;

}
