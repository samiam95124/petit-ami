/** ****************************************************************************
*                                                                              *
*                     X MODEL ON WAYLAND - SHIM DEFINITIONS                    *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
* Declares the X window and drawing model implemented on Wayland by wlshim.c.  *
* graphics_wl.c is kept structurally parallel to the Xlib backend in           *
* graphics.c (the house rule between backends); this shim is what lets the    *
* parallel structure survive the platform change: the same call shapes, with  *
* rasterization done by our own scanline code into shared memory buffers and  *
* window management done by xdg-shell.                                        *
*                                                                              *
* The types deliberately carry the X names. This file is only included by the *
* Wayland backend and its shim; it never coexists with Xlib headers in one    *
* translation unit.                                                           *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* (same terms as graphics.c)                                                   *
*                                                                              *
*******************************************************************************/

#ifndef WLSHIM_H
#define WLSHIM_H

#include <stdint.h>
#include <stddef.h>

/* keysym values: xkbcommon's XKB_KEY_* are numerically identical to Xlib's
   XK_*; alias the set the backend uses so the key translation code ports
   unchanged */
#include <xkbcommon/xkbcommon-keysyms.h>

#define XK_Alt_L      XKB_KEY_Alt_L
#define XK_Alt_R      XKB_KEY_Alt_R
#define XK_BackSpace  XKB_KEY_BackSpace
#define XK_Caps_Lock  XKB_KEY_Caps_Lock
#define XK_Control_L  XKB_KEY_Control_L
#define XK_Control_R  XKB_KEY_Control_R
#define XK_Delete     XKB_KEY_Delete
#define XK_Down       XKB_KEY_Down
#define XK_End        XKB_KEY_End
#define XK_Escape     XKB_KEY_Escape
#define XK_F1         XKB_KEY_F1
#define XK_F2         XKB_KEY_F2
#define XK_F3         XKB_KEY_F3
#define XK_F4         XKB_KEY_F4
#define XK_F5         XKB_KEY_F5
#define XK_F6         XKB_KEY_F6
#define XK_F7         XKB_KEY_F7
#define XK_F8         XKB_KEY_F8
#define XK_F9         XKB_KEY_F9
#define XK_F10        XKB_KEY_F10
#define XK_F11        XKB_KEY_F11
#define XK_F12        XKB_KEY_F12
#define XK_Home       XKB_KEY_Home
#define XK_Insert     XKB_KEY_Insert
#define XK_KP_Add     XKB_KEY_KP_Add
#define XK_KP_Subtract XKB_KEY_KP_Subtract
#define XK_Left       XKB_KEY_Left
#define XK_Page_Down  XKB_KEY_Page_Down
#define XK_Page_Up    XKB_KEY_Page_Up
#define XK_Return     XKB_KEY_Return
#define XK_Right      XKB_KEY_Right
#define XK_Shift_L    XKB_KEY_Shift_L
#define XK_Shift_R    XKB_KEY_Shift_R
#define XK_Tab        XKB_KEY_Tab
#define XK_Up         XKB_KEY_Up
#define XK_c          XKB_KEY_c
#define XK_C          XKB_KEY_C
#define XK_e          XKB_KEY_e
#define XK_E          XKB_KEY_E
#define XK_h          XKB_KEY_h
#define XK_H          XKB_KEY_H
#define XK_p          XKB_KEY_p
#define XK_P          XKB_KEY_P
#define XK_q          XKB_KEY_q
#define XK_Q          XKB_KEY_Q
#define XK_s          XKB_KEY_s
#define XK_S          XKB_KEY_S
#define XK_v          XKB_KEY_v
#define XK_V          XKB_KEY_V
#define XK_equal      XKB_KEY_equal
#define XK_minus      XKB_KEY_minus
#define XK_plus       XKB_KEY_plus
#define XK_underscore XKB_KEY_underscore

/* base scalar types, as X defines them */
typedef unsigned long XID;
typedef XID Window;
typedef XID Pixmap;
typedef XID Drawable;
typedef XID Cursor;
typedef XID Colormap;
typedef XID KeySym;
typedef unsigned long Atom;
typedef unsigned long Time;
typedef char* XPointer;
typedef int Status;
typedef int Bool;
#define True  1
#define False 0

/* the display connection; opaque here, defined by the shim */
typedef struct _XDisplay Display;

/* the graphics context; opaque handle */
typedef struct _XGC* GC;

/* visual is only ever passed through; opaque */
typedef struct _Visual Visual;

/* reserved ids */
#define None 0L
#define CurrentTime 0L
#define CopyFromParent 0L

/* window classes */
#define InputOutput 1

/* event types (X numbering kept so the debug printers stay truthful) */
#define KeyPress         2
#define KeyRelease       3
#define ButtonPress      4
#define ButtonRelease    5
#define MotionNotify     6
#define EnterNotify      7
#define LeaveNotify      8
#define FocusIn          9
#define FocusOut         10
#define KeymapNotify     11
#define Expose           12
#define GraphicsExpose   13
#define NoExpose         14
#define VisibilityNotify 15
#define CreateNotify     16
#define DestroyNotify    17
#define UnmapNotify      18
#define MapNotify        19
#define MapRequest       20
#define ReparentNotify   21
#define ConfigureNotify  22
#define ConfigureRequest 23
#define GravityNotify    24
#define ResizeRequest    25
#define CirculateNotify  26
#define CirculateRequest 27
#define PropertyNotify   28
#define SelectionClear   29
#define SelectionRequest 30
#define SelectionNotify  31
#define ColormapNotify   32
#define ClientMessage    33
#define MappingNotify    34
#define LASTEvent        36
/* wlshim extension, outside the X space: the compositor's frame callback
   delivered as an event, enabled per toplevel by wlshim_frameevents() */
#define FrameNotify      40

/* event masks */
#define NoEventMask             0L
#define KeyPressMask            (1L<<0)
#define KeyReleaseMask          (1L<<1)
#define ButtonPressMask         (1L<<2)
#define ButtonReleaseMask       (1L<<3)
#define EnterWindowMask         (1L<<4)
#define LeaveWindowMask         (1L<<5)
#define PointerMotionMask       (1L<<6)
#define ButtonMotionMask        (1L<<13)
#define ExposureMask            (1L<<15)
#define VisibilityChangeMask    (1L<<16)
#define StructureNotifyMask     (1L<<17)
#define SubstructureNotifyMask  (1L<<19)
#define SubstructureRedirectMask (1L<<20)
#define FocusChangeMask         (1L<<21)
#define PropertyChangeMask      (1L<<22)

/* buttons and modifier masks */
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5
#define Button1Mask (1<<8)
#define Button2Mask (1<<9)
#define Button3Mask (1<<10)
#define ShiftMask   (1<<0)
#define LockMask    (1<<1)
#define ControlMask (1<<2)
#define Mod1Mask    (1<<3)

/* GC functions */
#define GXclear 0x0
#define GXand   0x1
#define GXcopy  0x3
#define GXnoop  0x5
#define GXxor   0x6
#define GXor    0x7

/* line/cap/join/fill styles */
#define LineSolid      0
#define LineOnOffDash  1
#define LineDoubleDash 2
#define CapButt        1
#define CapRound       2
#define JoinMiter      0
#define FillSolid      0
#define FillStippled   2

/* arc modes */
#define ArcChord    0
#define ArcPieSlice 1

/* polygon shapes/modes */
#define Convex          2
#define Nonconvex       1
#define CoordModeOrigin 0

/* property modes and types */
#define PropModeReplace 0
#define XA_ATOM     ((Atom)4)
#define XA_CARDINAL ((Atom)6)
#define AnyPropertyType ((Atom)0)
#define PropertyNewValue 0

/* input focus */
#define RevertToNone   0
#define RevertToParent 2

/* grabs */
#define GrabModeAsync 1
#define GrabSuccess   0

/* return and error codes */
#define Success     0
#define BadWindow   3
#define BadPixmap   4
#define BadDrawable 9
#define AllPlanes   (~0UL)

/* window map states */
#define IsUnmapped   0
#define IsUnviewable 1
#define IsViewable   2

/* stacking */
#define Above 0
#define Below 1

/* configure window masks */
#define CWX         (1<<0)
#define CWY         (1<<1)
#define CWWidth     (1<<2)
#define CWHeight    (1<<3)
#define CWStackMode (1<<6)
#define CWBackPixmap (1L<<0) /* set-attributes namespace; shim ignores */

/* image format */
#define XYBitmap 0
#define ZPixmap  2

/* XSetWindowAttributes: only the fields the backend touches */
typedef struct {

    Pixmap background_pixmap;
    unsigned long background_pixel;
    unsigned long event_mask;

} XSetWindowAttributes;

typedef struct {

    int x, y;
    int width, height;
    int depth;
    Window root;
    int map_state;
    int override_redirect;

} XWindowAttributes;

typedef struct {

    int x, y;
    int width, height;
    int stack_mode;

} XWindowChanges;

/* size hints: stored, honored as min==max fixed sizing on toplevels */
typedef struct {

    long flags;
    int x, y;
    int width, height;
    int min_width, min_height;
    int max_width, max_height;

} XSizeHints;

#define PPosition  (1L<<2)
#define PSize      (1L<<3)
#define PMinSize   (1L<<4)
#define PMaxSize   (1L<<5)

/* GC values (only fields used) */
typedef struct {

    int function;
    unsigned long foreground;
    unsigned long background;

} XGCValues;

#define GCFunction   (1L<<0)
#define GCForeground (1L<<2)
#define GCBackground (1L<<3)

/* image: our own layout; XPutPixel/XGetPixel are functions in the shim */
typedef struct _XImage {

    int   width, height;
    int   format;
    int   depth;
    int   bytes_per_line;
    int   bits_per_pixel;
    char* data;

} XImage;

/* events: structures mirror X's field names, holding just the fields the
   backend reads or writes */
typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        window;

} XAnyEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        window;
    Window        root;
    Window        subwindow;
    Time          time;
    int           x, y;
    int           x_root, y_root;
    unsigned int  state;
    unsigned int  keycode;
    Bool          same_screen;

} XKeyEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        window;
    Window        root;
    Window        subwindow;
    Time          time;
    int           x, y;
    int           x_root, y_root;
    unsigned int  state;
    unsigned int  button;
    Bool          same_screen;

} XButtonEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        window;
    Window        root;
    Window        subwindow;
    Time          time;
    int           x, y;
    int           x_root, y_root;
    unsigned int  state;
    char          is_hint;
    Bool          same_screen;

} XMotionEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        window;
    Window        root;
    Window        subwindow;
    Time          time;
    int           x, y;
    int           x_root, y_root;
    int           mode;
    int           detail;
    Bool          same_screen;
    Bool          focus;
    unsigned int  state;

} XCrossingEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        window;
    int           mode;
    int           detail;

} XFocusChangeEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        window;
    int           x, y;
    int           width, height;
    int           count;

} XExposeEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        event;
    Window        window;

} XMapEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        event;
    Window        window;

} XUnmapEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        event;
    Window        window;
    int           x, y;
    int           width, height;
    int           border_width;
    Window        above;
    Bool          override_redirect;

} XConfigureEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        window;
    Atom          atom;
    Time          time;
    int           state;

} XPropertyEvent;

typedef struct {

    int           type;
    unsigned long serial;
    Bool          send_event;
    Display*      display;
    Window        window;
    Atom          message_type;
    int           format;
    union {

        char  b[20];
        short s[10];
        long  l[5];

    } data;

} XClientMessageEvent;

typedef union _XEvent {

    int                 type;
    XAnyEvent           xany;
    XKeyEvent           xkey;
    XButtonEvent        xbutton;
    XMotionEvent        xmotion;
    XCrossingEvent      xcrossing;
    XFocusChangeEvent   xfocus;
    XExposeEvent        xexpose;
    XMapEvent           xmap;
    XUnmapEvent         xunmap;
    XConfigureEvent     xconfigure;
    XPropertyEvent      xproperty;
    XClientMessageEvent xclient;

} XEvent;

/* error event and handler, kept for the xerror diagnostics shape */
typedef struct {

    int           type;
    Display*      display;
    XID           resourceid;
    unsigned long serial;
    unsigned char error_code;
    unsigned char request_code;
    unsigned char minor_code;

} XErrorEvent;

typedef int (*XErrorHandler)(Display*, XErrorEvent*);

/* connection and screen macro layer */
Display* XOpenDisplay(const char* name);
int      XCloseDisplay(Display* d);
int      wlshim_fd(Display* d);
int      wlshim_screen(Display* d);
int      wlshim_width(Display* d);
int      wlshim_height(Display* d);
int      wlshim_widthmm(Display* d);
int      wlshim_heightmm(Display* d);
int      wlshim_depth(Display* d);
Window   wlshim_root(Display* d);
Visual*  wlshim_visual(Display* d);
unsigned long wlshim_nextreq(Display* d);

#define ConnectionNumber(d)   wlshim_fd(d)
#define DefaultScreen(d)      wlshim_screen(d)
#define DisplayWidth(d, s)    wlshim_width(d)
#define DisplayHeight(d, s)   wlshim_height(d)
#define DisplayWidthMM(d, s)  wlshim_widthmm(d)
#define DisplayHeightMM(d, s) wlshim_heightmm(d)
#define DefaultDepth(d, s)    wlshim_depth(d)
#define RootWindow(d, s)      wlshim_root(d)
#define DefaultRootWindow(d)  wlshim_root(d)
#define DefaultVisual(d, s)   wlshim_visual(d)
#define BlackPixel(d, s)      0x000000UL
#define WhitePixel(d, s)      0xffffffUL
#define XNextRequest(d)       wlshim_nextreq(d)

/* threading and sync */
Status XInitThreads(void);
int    XFlush(Display* d);
int    XSync(Display* d, Bool discard);
int    XSynchronize(Display* d, Bool onoff);
XErrorHandler XSetErrorHandler(XErrorHandler h);
int    XGetErrorText(Display* d, int code, char* buf, int len);

/* window management */
Window XCreateWindow(Display* d, Window parent, int x, int y,
                     unsigned int w, unsigned int h, unsigned int bw,
                     int depth, unsigned int cls, Visual* visual,
                     unsigned long valuemask, XSetWindowAttributes* attr);
Window XCreateSimpleWindow(Display* d, Window parent, int x, int y,
                           unsigned int w, unsigned int h, unsigned int bw,
                           unsigned long border, unsigned long background);
int    XDestroyWindow(Display* d, Window w);
int    XMapWindow(Display* d, Window w);
int    XUnmapWindow(Display* d, Window w);
int    XMoveWindow(Display* d, Window w, int x, int y);
int    XResizeWindow(Display* d, Window w, unsigned int width,
                     unsigned int height);
int    XMoveResizeWindow(Display* d, Window w, int x, int y,
                         unsigned int width, unsigned int height);
int    XConfigureWindow(Display* d, Window w, unsigned int mask,
                        XWindowChanges* ch);
int    XRaiseWindow(Display* d, Window w);
int    XLowerWindow(Display* d, Window w);
Status XGetWindowAttributes(Display* d, Window w, XWindowAttributes* wa);
int    XSelectInput(Display* d, Window w, long mask);
int    XStoreName(Display* d, Window w, const char* name);
int    XSetIconName(Display* d, Window w, const char* name);
void   XSetWMNormalHints(Display* d, Window w, XSizeHints* hints);
Status XSetWMProtocols(Display* d, Window w, Atom* protocols, int count);
Status XQueryTree(Display* d, Window w, Window* root, Window* parent,
                  Window** children, unsigned int* nchildren);
int    XClearArea(Display* d, Window w, int x, int y, unsigned int width,
                  unsigned int height, Bool exposures);
int    XSetInputFocus(Display* d, Window focus, int revert_to, Time time);
Bool   XQueryPointer(Display* d, Window w, Window* root, Window* child,
                     int* rx, int* ry, int* wx, int* wy, unsigned int* mask);
int    XGrabPointer(Display* d, Window grab, Bool owner_events,
                    unsigned int event_mask, int pointer_mode,
                    int keyboard_mode, Window confine_to, Cursor cursor,
                    Time time);
int    XUngrabPointer(Display* d, Time time);

/* properties and atoms */
Atom   XInternAtom(Display* d, const char* name, Bool only_if_exists);
char*  XGetAtomName(Display* d, Atom a);
int    XChangeProperty(Display* d, Window w, Atom property, Atom type,
                       int format, int mode, const unsigned char* data,
                       int nelements);
int    XGetWindowProperty(Display* d, Window w, Atom property, long off,
                          long len, Bool delete, Atom req_type,
                          Atom* actual_type, int* actual_format,
                          unsigned long* nitems, unsigned long* bytes_after,
                          unsigned char** prop);
int    XFree(void* data);

/* cursors: the font cursor shape codes the backend selects */
#define XC_left_ptr            68
#define XC_left_side           70
#define XC_right_side          96
#define XC_top_side            138
#define XC_bottom_side         16
#define XC_top_left_corner     134
#define XC_top_right_corner    136
#define XC_bottom_left_corner  12
#define XC_bottom_right_corner 14

Cursor XCreateFontCursor(Display* d, unsigned int shape);
int    XDefineCursor(Display* d, Window w, Cursor c);

/* pixmaps and GCs */
Pixmap XCreatePixmap(Display* d, Drawable ref, unsigned int w,
                     unsigned int h, unsigned int depth);
int    XFreePixmap(Display* d, Pixmap p);
GC     XCreateGC(Display* d, Drawable ref, unsigned long valuemask,
                 XGCValues* values);
int    XFreeGC(Display* d, GC gc);
int    XSetForeground(Display* d, GC gc, unsigned long fg);
int    XSetBackground(Display* d, GC gc, unsigned long bg);
int    XSetFunction(Display* d, GC gc, int function);
int    XSetLineAttributes(Display* d, GC gc, unsigned int width,
                          int line_style, int cap_style, int join_style);
int    XSetDashes(Display* d, GC gc, int dash_offset, const char* dash_list,
                  int n);
int    XSetFillStyle(Display* d, GC gc, int style);
int    XSetStipple(Display* d, GC gc, Pixmap stipple);
int    XSetTSOrigin(Display* d, GC gc, int x, int y);
int    XSetClipMask(Display* d, GC gc, Pixmap mask);
int    XSetArcMode(Display* d, GC gc, int mode);

/* drawing */
int    XDrawPoint(Display* d, Drawable dr, GC gc, int x, int y);
int    XDrawLine(Display* d, Drawable dr, GC gc, int x1, int y1, int x2,
                 int y2);
int    XDrawRectangle(Display* d, Drawable dr, GC gc, int x, int y,
                      unsigned int w, unsigned int h);
int    XFillRectangle(Display* d, Drawable dr, GC gc, int x, int y,
                      unsigned int w, unsigned int h);
int    XDrawArc(Display* d, Drawable dr, GC gc, int x, int y,
                unsigned int w, unsigned int h, int angle1, int angle2);
int    XFillArc(Display* d, Drawable dr, GC gc, int x, int y,
                unsigned int w, unsigned int h, int angle1, int angle2);
typedef struct { short x, y; } XPoint;
int    XFillPolygon(Display* d, Drawable dr, GC gc, XPoint* points,
                    int npoints, int shape, int mode);
int    XCopyArea(Display* d, Drawable src, Drawable dst, GC gc, int sx,
                 int sy, unsigned int w, unsigned int h, int dx, int dy);

/* images */
XImage* XCreateImage(Display* d, Visual* v, unsigned int depth, int format,
                     int offset, char* data, unsigned int width,
                     unsigned int height, int bitmap_pad,
                     int bytes_per_line);
XImage* XGetImage(Display* d, Drawable dr, int x, int y, unsigned int w,
                  unsigned int h, unsigned long plane_mask, int format);
int     XPutImage(Display* d, Drawable dr, GC gc, XImage* img, int sx,
                  int sy, int dx, int dy, unsigned int w, unsigned int h);
int     XDestroyImage(XImage* img);
unsigned long XGetPixel(XImage* img, int x, int y);
int     XPutPixel(XImage* img, int x, int y, unsigned long pixel);

/* events */
int    XPending(Display* d);
int    XNextEvent(Display* d, XEvent* e);
int    XPeekEvent(Display* d, XEvent* e);
Bool   XCheckTypedEvent(Display* d, int type, XEvent* e);
Bool   XCheckTypedWindowEvent(Display* d, Window w, int type, XEvent* e);
Status XSendEvent(Display* d, Window w, Bool propagate, long mask,
                  XEvent* e);
KeySym XLookupKeysym(XKeyEvent* e, int index);

/* Wayland-specific hooks the backend proper uses */

/* pump the wayland connection: read and dispatch anything pending without
   blocking; call when the display fd selects readable */
void   wlshim_pump(Display* d);

/* Declare a toplevel's frame regions, in surface pixels. A pointer press in
   the title rectangle starts a compositor-side interactive move; a press
   within borderw of the surface edge starts an interactive resize with the
   grabbed edges. Declare the title rectangle to exclude the frame buttons,
   whose presses must reach the application */
void   wlshim_frame(Display* d, Window w, int titx, int tity, int titw,
                    int tith, int borderw);

/* toplevel minimize and maximize through the shell */
void   wlshim_minimize(Display* d, Window w);
void   wlshim_maximize(Display* d, Window w, int on);
void   wlshim_frameevents(Display* d, Window w, int on);

/* dump a window's composed surface buffer to a PPM file; the test rig's
   capture path, independent of any compositor screenshot facility */
int    wlshim_dump(Display* d, Window w, const char* fn);

#endif /* WLSHIM_H */
