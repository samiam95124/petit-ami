/*******************************************************************************
*                                                                              *
*                         LINUX FRAME BUFFER INTERFACE                          *
*                                                                              *
* Exposes the Linux frame buffer device, /dev/fb0, as a flat grid of pixels    *
* a caller draws into directly. The design is the one in doc/framebuffer.md:    *
*                                                                              *
*   1. One resolution, the best the device offers, set at initialization.       *
*   2. Linear, no paging.                                                       *
*   3. A grid of pixels, each pixel a contiguous value.                         *
*   4. 8, 16 or 32 bits per pixel.                                              *
*   5. RGB only.                                                                *
*                                                                              *
* The module is self initializing and self destructing: the device is opened,   *
* its mode read, and the buffer mapped when the program starts, and unmapped     *
* and closed when it ends. There is nothing to call to set it up.               *
*                                                                              *
* Three calls are the whole interface. Everything else is done by writing to     *
* the buffer the third one hands back, a pixel at a time, in the format the      *
* second one names.                                                             *
*                                                                              *
*******************************************************************************/

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

/*******************************************************************************

Return display geometry

Returns the size of the display in pixels: the number of rows (its height) and
the number of columns (its width).

*******************************************************************************/

void frame_geometry(long* rows, long* columns);

/*******************************************************************************

Return pixel size

Returns the size of one pixel in bytes, one of:

    Bytes   Format
    ============================================================
    3       24 bits, 8 each of r, g, b.
    4       32 bits, 8 each of a, r, g, b; alpha ignored.
    6       48 bits, 16 each of r, g, b.
    8       64 bits, 16 each of a, r, g, b; alpha ignored.
    12      96 bits, 32 each of r, g, b.
    16      128 bits, 32 each of a, r, g, b; alpha ignored.

A pointer into the buffer is formed with the matching integer type, so the
caller reads this once and casts accordingly.

*******************************************************************************/

void frame_pixsiz(long* pixsiz);

/*******************************************************************************

Return buffer base

Returns the base address of the frame buffer. The pixel at (x, y), with x the
column and y the row, is at

    base + y*columns*pixsiz + x*pixsiz

Writing there puts a pixel on the display.

*******************************************************************************/

void frame_buffer(void** buffer);

/*******************************************************************************

Component order within a pixel

The device on this machine packs a 32 bit pixel as blue, green, red, alpha in
ascending byte order (XRGB8888 little-endian). These give the byte offset of
each component within a pixel, read from the device at init, so a caller need
not assume the order.

*******************************************************************************/

void frame_rgboff(long* roff, long* goff, long* boff);

#endif /* FRAMEBUFFER_H */
