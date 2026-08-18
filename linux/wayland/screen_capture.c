/*******************************************************************************
*                                                                              *
*                           SCREEN CAPTURE MODULE                              *
*                                                                              *
* Wayland version. Appends the pixel contents of the running Petit-Ami        *
* window to a file called "test_images" as a PNG. Multiple PNGs are           *
* concatenated back to back in the same file; each is self delimiting (PNG    *
* signature at the start, IEND chunk at the end) so readers can walk them     *
* sequentially.                                                                *
*                                                                              *
* The file is opened by a module constructor and closed by a destructor.       *
* The caller only needs to call screen_capture() at each interesting moment    *
* (e.g. just before waitnext() in a test program).                             *
*                                                                              *
* Output format matches linux/screen_capture.c and macosx/screen_capture.c    *
* (concatenated PNGs) so test_images files are portable across platforms for  *
* cross-platform regression comparison. The client area is captured, which is  *
* what the X11 capturer grabs there: the window frame belongs to the desktop   *
* in that world, and to the decorations module in this one.                    *
*                                                                              *
* There is nothing to discover and no compositor to ask. A Wayland client      *
* draws its window into a canvas of its own, and that canvas is the picture:   *
* the backend hands over the window it presents and the pixels are read        *
* straight out of it. A compositor screen grab would need a portal and the     *
* user's permission; this needs neither, and cannot catch anything on screen   *
* that is not ours.                                                            *
*                                                                              *
* Viewer:                                                                      *
*   bin/testviewer    # walks through the concatenated PNGs with arrow keys    *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <setjmp.h>
#include <png.h>

#include "graphics_i.h"

#define CAPTURE_FILENAME "test_images"

/* ---------- module state ---------- */

static FILE     *cap_file        = NULL;
static uint32_t  cap_frame_count = 0;
static int       cap_disabled    = 0;
static int       cap_nowin       = 0;   /* "no window" already reported */

/* ---------- PNG writing ---------- */

static void png_write_to_file(png_structp png, png_bytep data, png_size_t len) {
    FILE *f = (FILE *)png_get_io_ptr(png);
    fwrite(data, 1, len, f);
}

static void png_flush_file(png_structp png) {
    FILE *f = (FILE *)png_get_io_ptr(png);
    fflush(f);
}

static void write_png_frame(FILE *f, uint8_t *rgb_rows, int width, int height) {
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              NULL, NULL, NULL);
    if (!png) return;
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, NULL); return; }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return;
    }
    png_set_write_fn(png, f, png_write_to_file, png_flush_file);
    png_set_IHDR(png, info, width, height, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    for (int y = 0; y < height; y++)
        png_write_row(png, rgb_rows + y * width * 3);

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fflush(f);
}

/* ---------- public entry point ---------- */

void screen_capture(void) {
    pd_win    *win;
    pd_canvas *can;
    uint32_t  *pix;
    uint8_t   *rgb;
    int        w, h, stride, x, y;

    if (cap_disabled || !cap_file) return;

    win = grx_capturewin(); /* the window the backend presents */
    if (!win) {
        if (!cap_nowin) {
            fprintf(stderr, "screen_capture: no Petit-Ami window found\n");
            cap_nowin = 1;
        }
        return;
    }
    can = pd_wincanvas(win);
    if (!can) return;
    pd_cansize(can, &w, &h);
    if (w <= 0 || h <= 0) return;

    rgb = malloc((size_t)w*h*3);
    if (!rgb) {
        fprintf(stderr, "screen_capture: out of memory\n");
        return;
    }
    pix = pd_canlock(can, &stride);
    for (y = 0; y < h; y++) {

        uint32_t *sp = pix+(size_t)y*stride;
        uint8_t  *dp = rgb+(size_t)y*w*3;

        for (x = 0; x < w; x++) {

            uint32_t p = sp[x];

            *dp++ = (p>>16)&0xff;
            *dp++ = (p>>8)&0xff;
            *dp++ = p&0xff;

        }

    }
    pd_canunlock(can);

    write_png_frame(cap_file, rgb, w, h);
    cap_frame_count++;

    free(rgb);
}

/* ---------- module lifecycle ---------- */

__attribute__((constructor))
static void screen_capture_init(void) {
    remove(CAPTURE_FILENAME);
    cap_file = fopen(CAPTURE_FILENAME, "wb");
    if (!cap_file) {
        perror("screen_capture: fopen " CAPTURE_FILENAME);
        cap_disabled = 1;
    }
}

__attribute__((destructor))
static void screen_capture_fini(void) {
    if (cap_file) {
        fflush(cap_file);
        fclose(cap_file);
        cap_file = NULL;
        if (cap_frame_count == 0) {
            remove(CAPTURE_FILENAME);
        } else if (!isatty(fileno(stderr))) {
            /* The summary is for harness runs with stderr captured. On a
               terminal it landed on the program's final screen, printing
               over whatever the test left there. */
            fprintf(stderr, "screen_capture: wrote %u frames to %s\n",
                    cap_frame_count, CAPTURE_FILENAME);
        }
    }
}
