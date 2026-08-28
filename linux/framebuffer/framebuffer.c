/*******************************************************************************
*                                                                              *
*                         LINUX FRAME BUFFER INTERFACE                          *
*                                                                              *
* The Linux frame buffer, /dev/fb0, presented as a flat grid of pixels. See     *
* framebuffer.h and doc/framebuffer.md for the interface and its reasoning.     *
*                                                                              *
* The device is opened and its screen mapped by a constructor when the program  *
* starts, and unmapped and closed by a destructor when it ends. The buffer is    *
* the device's own memory, so a write to it is a write to the screen: on a       *
* text console this shows at once, and under a graphical desktop the compositor  *
* owns the display and nothing is seen until the console is brought forward.     *
*                                                                              *
* The mode is the device's current one, which the kernel sets to the panel's     *
* native resolution -- the highest it has -- so there is nothing to choose. A     *
* fuller module would enumerate /sys/class/graphics/fb0/modes and select the      *
* largest; here the current mode already is that.                               *
*                                                                              *
* The line length the device reports is honored: a scanline can be wider than    *
* the visible pixels (padding for alignment), so the address of a row is         *
* row*line_length, not row*columns*pixsiz. frame_buffer() and the geometry        *
* calls hide that -- but a caller stepping the buffer by hand must use the        *
* stride, which is why frame_geometry gives columns and frame_pixsiz the pixel     *
* size: their product is the row's used width, and the mapping accounts for any    *
* padding past it.                                                              *
*                                                                              *
*                          BSD LICENSE INFORMATION                              *
*                                                                              *
* Copyright (C) 2026 - Scott A. Franco                                          *
*                                                                              *
* All rights reserved.                                                          *
*                                                                              *
* Redistribution and use in source and binary forms, with or without            *
* modification, are permitted provided that the following conditions            *
* are met:                                                                      *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright             *
*    notice, this list of conditions and the following disclaimer.              *
* 2. Redistributions in binary form must reproduce the above copyright          *
*    notice, this list of conditions and the following disclaimer in the         *
*    documentation and/or other materials provided with the distribution.        *
* 3. Neither the name of the project nor the names of its contributors          *
*    may be used to endorse or promote products derived from this software      *
*    without specific prior written permission.                                 *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND        *
* ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED. IN NO EVENT SHALL THE        *
* PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DAMAGES ARISING IN ANY WAY OUT OF     *
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.    *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include "framebuffer.h"

#define FBDEV "/dev/fb0" /* the frame buffer device */

/* ---------- module state, set once at init ---------- */

static int    fbfd       = -1;   /* device file descriptor */
static void*  fbmem      = NULL; /* mapped screen memory */
static long   fbmemlen   = 0;    /* bytes mapped */
static long   fbcols     = 0;    /* visible pixels across (width) */
static long   fbrows     = 0;    /* visible pixels down (height) */
static long   fbpixsiz   = 0;    /* bytes per pixel */
static long   fbstride   = 0;    /* bytes per scanline (>= cols*pixsiz) */
static long   fbroff     = 0;    /* red byte offset within a pixel */
static long   fbgoff     = 0;    /* green byte offset */
static long   fbboff     = 0;    /* blue byte offset */

/* The reduced mode: FBSIZE=WxH in the environment exposes a logical
   screen of that size in a shadow buffer, and a scanout thread
   replicates it integer-scaled and centered onto the device at about
   thirty a second, skipping rows that have not changed. The layers
   above see only the small screen, so drawing and composition shrink
   with it -- the testing knob for a large panel. */
static long           lrows;      /* logical rows, 0 = native mode */
static long           lcols;      /* logical columns */
static unsigned char* shadow;     /* the logical screen */
static unsigned char* shadowprev; /* last scanned-out copy */
static long           scale;      /* device pixels per logical pixel */
static long           scoffx;     /* centering offset on the device */
static long           scoffy;
static pthread_t      scanthread; /* the scanout */
static int            scanrun;    /* it should keep running */

/*******************************************************************************

Fail

Prints the reason on stderr and ends the program. The frame buffer is the whole
display of a program that uses it; there is nothing to fall back to.

*******************************************************************************/

static void fail(const char* what)

{

    fprintf(stderr, "*** framebuffer: %s: %s\n", what,
            errno ? strerror(errno) : "cannot continue");
    exit(1);

}

/*******************************************************************************

Component byte offset from a bit offset

The device gives each component's position as a bit offset and length. For the
packed true color modes this module supports, a component is a whole number of
bytes in and a whole number of bytes long, so the byte offset is the bit offset
over eight.

*******************************************************************************/

static long byteoff(struct fb_bitfield* bf)

{

    return (bf->offset/8);

}

/*******************************************************************************

The scanout

In the reduced mode the layers above draw the shadow; this thread
replicates changed rows onto the device, each logical pixel a scale by
scale block, centered. About thirty passes a second; an unchanged
screen costs a row compare per row and nothing more.

*******************************************************************************/

static void* scanout(void* arg)

{

    struct timespec ts = { 0, 33*1000*1000 };
    long   y, x, r;
    size_t lrow = (size_t)lcols*fbpixsiz;
    unsigned char* xrow;

    (void)arg;
    xrow = malloc(lrow*scale);
    if (!xrow) return (NULL);
    while (scanrun) {

        for (y = 0; y < lrows; y++) {

            unsigned char* sr = shadow+(size_t)y*lrow;

            unsigned char* pr = shadowprev+(size_t)y*lrow;

            if (memcmp(sr, pr, lrow)) {

                /* Expand the snapshot, never a second read of the live
                   row: the layers above write the shadow while this
                   runs, and a device row built from a fresher read than
                   the snapshot differs from it invisibly -- the compare
                   never fires again and the tear stays on screen. From
                   the snapshot the device always matches shadowprev,
                   so any tear is caught on the next pass. */
                memcpy(pr, sr, lrow);
                /* expand the row once, then lay it down scale times */
                for (x = 0; x < lcols; x++)
                    for (r = 0; r < scale; r++)
                        memcpy(xrow+((size_t)x*scale+r)*fbpixsiz,
                               pr+(size_t)x*fbpixsiz, fbpixsiz);
                for (r = 0; r < scale; r++)
                    memcpy(fbmem+((size_t)(scoffy+y*scale+r)*fbcols+scoffx)*
                               fbpixsiz,
                           xrow, lrow*scale);

            }

        }
        nanosleep(&ts, NULL);

    }
    free(xrow);

    return (NULL);

}

/*******************************************************************************

Initialize the frame buffer

Opens the device, reads its variable and fixed screen information, checks the
format is one this module supports, and maps the screen. Runs before main().

*******************************************************************************/

static void init_framebuffer(void) __attribute__((constructor(102)));
static void init_framebuffer(void)

{

    struct fb_var_screeninfo vinfo; /* the changeable mode: resolution, format */
    struct fb_fix_screeninfo finfo; /* the fixed facts: stride, mapped length */

    const char* dev;
    int         isfile = 0;

    errno = 0;
    /* FBDEV overrides the device path; a plain file serves the test
       rig, taking its geometry from FBDEVGEOM (default 3840x2160) */
    dev = getenv("FBDEV");
    if (!dev) dev = FBDEV;
    else isfile = 1;
    fbfd = open(dev, O_RDWR | (isfile? O_CREAT: 0), 0644);
    if (fbfd < 0) fail("cannot open the frame buffer device");

    if (!isfile) {

        if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo))
            fail("cannot read the mode");
        if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo))
            fail("cannot read the layout");

        /* the mode is the device's current one, the panel's native
           resolution */
        fbcols   = vinfo.xres;
        fbrows   = vinfo.yres;
        fbpixsiz = vinfo.bits_per_pixel/8;
        fbstride = finfo.line_length;

    } else {

        /* the file rig: BGRA 32 bit at the given geometry */
        const char* g = getenv("FBDEVGEOM");

        fbcols = 3840;
        fbrows = 2160;
        if (g) sscanf(g, "%ldx%ld", &fbcols, &fbrows);
        fbpixsiz = 4;
        fbstride = fbcols*fbpixsiz;
        memset(&vinfo, 0, sizeof(vinfo));
        vinfo.red.offset = 16;
        vinfo.green.offset = 8;
        vinfo.blue.offset = 0;
        if (ftruncate(fbfd, fbstride*fbrows))
            fail("cannot size the file rig");

    }

    if (fbpixsiz != 1 && fbpixsiz != 2 && fbpixsiz != 4 &&
        fbpixsiz != 3 && fbpixsiz != 6 && fbpixsiz != 8 &&
        fbpixsiz != 12 && fbpixsiz != 16)
        fail("unsupported pixel size");
    if (vinfo.grayscale) fail("grayscale displays are not supported");

    fbroff = byteoff(&vinfo.red);
    fbgoff = byteoff(&vinfo.green);
    fbboff = byteoff(&vinfo.blue);

    /* The interface promises linear addressing: the pixel at (x, y) is at
       base + (y*columns + x)*pixsiz. That holds only when the scanline has
       no padding past the visible pixels. This device has none; a device
       that padded would need a stride the three-call interface does not
       carry, so refuse it rather than draw a sheared image. */
    if (fbstride != fbcols*fbpixsiz) fail("the scanline is padded");

    /* The device's reported map length is not trusted: the DRM-emulated
       fbdev fills smem_len with garbage. The screen is stride*rows bytes,
       which is what we map. */
    fbmemlen = fbstride*fbrows;
    if (fbmemlen <= 0) fail("the screen has no size");

    fbmem = mmap(NULL, fbmemlen, PROT_READ|PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmem == MAP_FAILED) { fbmem = NULL; fail("cannot map the screen"); }

    /* the reduced mode */
    {
        const char* sz = getenv("FBSIZE");
        long w, h;

        if (sz && sscanf(sz, "%ldx%ld", &w, &h) == 2 &&
            w >= 64 && h >= 64 && w <= fbcols && h <= fbrows) {

            lcols = w;
            lrows = h;
            scale = fbcols/w < fbrows/h? fbcols/w: fbrows/h;
            if (scale < 1) scale = 1;
            scoffx = (fbcols-w*scale)/2;
            scoffy = (fbrows-h*scale)/2;
            shadow = calloc((size_t)w*h, fbpixsiz);
            shadowprev = malloc((size_t)w*h*fbpixsiz);
            if (!shadow || !shadowprev) fail("no memory for the shadow");
            /* force the first scanout whole */
            memset(shadowprev, 0xff, (size_t)w*h*fbpixsiz);
            /* the border clears once */
            memset(fbmem, 0, fbmemlen);
            scanrun = 1;
            if (pthread_create(&scanthread, NULL, scanout, NULL))
                fail("cannot start the scanout");

        }
    }

}

/*******************************************************************************

Deinitialize the frame buffer

Unmaps the screen and closes the device. Runs after main() returns or exit().

*******************************************************************************/

static void deinit_framebuffer(void) __attribute__((destructor(102)));
static void deinit_framebuffer(void)

{

    if (scanrun) {

        scanrun = 0;
        pthread_join(scanthread, NULL);

    }
    free(shadow);
    free(shadowprev);
    shadow = NULL;
    if (fbmem && fbmemlen > 0) munmap(fbmem, fbmemlen);
    if (fbfd >= 0) close(fbfd);
    fbmem = NULL; fbfd = -1;

}

/* ---------- the interface ---------- */

void frame_geometry(long* rows, long* columns)

{

    if (rows) *rows = lrows? lrows: fbrows;
    if (columns) *columns = lcols? lcols: fbcols;

}

void frame_pixsiz(long* pixsiz)

{

    if (pixsiz) *pixsiz = fbpixsiz;

}

void frame_buffer(void** buffer)

{

    if (buffer) *buffer = shadow? (void*)shadow: fbmem;

}

void frame_rgboff(long* roff, long* goff, long* boff)

{

    if (roff) *roff = fbroff;
    if (goff) *goff = fbgoff;
    if (boff) *boff = fbboff;

}
