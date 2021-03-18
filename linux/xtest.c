/*
 * Hello world Xwindows demo program
 */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
int main(void) {

    Display*     d;
    Window       w;
    XEvent       e;
    const char*  msg = "Hello, World!";
    int          s;
    int          x;
    GC           gracxt; /* graphics context */
    XFontStruct* font;   /* current font */
    const char*  title = "Hello Window";
    int          depth;
    Pixmap       pm;
 
    d = XOpenDisplay(NULL);
    if (d == NULL) {

        fprintf(stderr, "Cannot open display\n");
        exit(1);

    }
 
    s = DefaultScreen(d);

    /* The default font for xwindows works on low DPI displays, but not on higher
       so we reselect based on DPI. This should work everywhere */
    font = XLoadQueryFont(d,
        "-bitstream-courier 10 pitch-bold-r-normal--0-0-200-200-m-0-iso8859-1");
    if (!font) {

        fprintf(stderr, "*** No font ***\n");
        exit(1);

    }
    gracxt = XDefaultGC(d, s);
    XSetFont(d, gracxt, font->fid);

    w = XCreateSimpleWindow(d, RootWindow(d, s), 10, 10, 640, 480, 5,
                            BlackPixel(d, s), WhitePixel(d, s));
    XSelectInput(d, w, ExposureMask | KeyPressMask);
    XMapWindow(d, w);

    XStoreName(d, w, title );
    XSetIconName(d, w, title );

    /* set up pixmap backing buffer for text grid */
    depth = DefaultDepth(d, s);
    pm = XCreatePixmap(d, w, 640, 480, depth);
    XSetForeground(d, gracxt, WhitePixel(d, s));
    XFillRectangle(d, pm, gracxt, 0, 0, 640, 480);
    XSetForeground(d, gracxt, BlackPixel(d, s));
    /* now draw the text on the pixmap */
    XDrawString(d, pm, gracxt, 10, 50, msg, strlen(msg));
 
    while (1) {

        XNextEvent(d, &e);
        if (e.type == Expose)
            /* paint the contents of the buffer to the screen */
            XCopyArea(d, pm, w, gracxt, 0, 0, 640, 480, 0, 0);
            /* or draw it directly */
            //XDrawString(d, w, gracxt, 10, 50, msg, strlen(msg));
        if (e.type == KeyPress) break; /* exit on any key */

    }

    XCloseDisplay(d);

    return 0;

}
