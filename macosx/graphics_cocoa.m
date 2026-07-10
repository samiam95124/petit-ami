/*******************************************************************************
*                                                                              *
*                       Cocoa/Quartz shim for Ami graphics                     *
*                                                                              *
* Objective-C side of the macOS graphics backend.  Implements window          *
* creation, event translation, and offscreen bitmap management using           *
* Cocoa (AppKit) and Quartz (CoreGraphics).                                    *
*                                                                              *
* Threading model: single-threaded poll.  User code runs on the main thread.  *
* ami_event() calls pa_cocoa_process_ns_events() which drains the NSApp       *
* event queue into our internal ring buffer, then returns one event.           *
*                                                                              *
*******************************************************************************/

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <pthread.h>
#include <dlfcn.h>
#include <crt_externs.h>
#include <IOKit/hid/IOHIDManager.h>
#include "pa_cocoa.h"

/*----------------------------------------------------------------------------
 * Threading state
 *
 * In threaded mode the Cocoa event loop runs on the main thread
 * ([NSApp run] or equivalent), and the user program runs on a worker
 * thread.  The two communicate through the event ring buffer protected
 * by evt_mutex / evt_cond.
 *----------------------------------------------------------------------------*/

static pthread_mutex_t evt_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  evt_cond  = PTHREAD_COND_INITIALIZER;
static int             threaded_mode = 0;

static void run_on_main(dispatch_block_t block) {
    if (!threaded_mode || [NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

/* Associated object key for storing tag on views that lack a settable tag */
static char kPATagKey;
static void  pa_set_tag(NSView* v, int t) {
    objc_setAssociatedObject(v, &kPATagKey, @(t), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
static int   pa_get_tag(NSView* v) {
    NSNumber* n = objc_getAssociatedObject(v, &kPATagKey);
    return n ? n.intValue : (int)v.tag;
}

/*----------------------------------------------------------------------------
 * Event ring buffer
 *----------------------------------------------------------------------------*/

#define EVT_QUEUE_SIZE 256

static pa_rawevent  evt_queue[EVT_QUEUE_SIZE];
static int          evt_head = 0;   /* next write position */
static int          evt_tail = 0;   /* next read  position */

static void evt_push(const pa_rawevent* e)
{
    if (threaded_mode) pthread_mutex_lock(&evt_mutex);

    /* coalesce resize events: overwrite an existing resize for the same window */
    if (e->type == PA_EVT_RESIZE) {
        for (int i = evt_tail; i != evt_head; i = (i + 1) % EVT_QUEUE_SIZE) {
            if (evt_queue[i].type == PA_EVT_RESIZE && evt_queue[i].win == e->win) {
                evt_queue[i] = *e;
                if (threaded_mode) {
                    pthread_cond_signal(&evt_cond);
                    pthread_mutex_unlock(&evt_mutex);
                }
                return;
            }
        }
    }

    int next = (evt_head + 1) % EVT_QUEUE_SIZE;
    if (next == evt_tail) {
        if (threaded_mode) pthread_mutex_unlock(&evt_mutex);
        return;
    }
    evt_queue[evt_head] = *e;
    evt_head = next;

    if (threaded_mode) {
        pthread_cond_signal(&evt_cond);
        pthread_mutex_unlock(&evt_mutex);
    }
}

static int evt_pop(pa_rawevent* e)
{
    if (threaded_mode) pthread_mutex_lock(&evt_mutex);
    if (evt_head == evt_tail) {
        if (threaded_mode) pthread_mutex_unlock(&evt_mutex);
        return 0;
    }
    *e = evt_queue[evt_tail];
    evt_tail = (evt_tail + 1) % EVT_QUEUE_SIZE;
    if (threaded_mode) pthread_mutex_unlock(&evt_mutex);
    return 1;
}

static int evt_empty(void) {
    if (threaded_mode) {
        pthread_mutex_lock(&evt_mutex);
        int empty = (evt_head == evt_tail);
        pthread_mutex_unlock(&evt_mutex);
        return empty;
    }
    return evt_head == evt_tail;
}

/*----------------------------------------------------------------------------
 * PAView — custom NSView, owns the offscreen bitmap
 *----------------------------------------------------------------------------*/

@interface PAView : NSView {
@public
    CGContextRef  bitmap;      /* offscreen drawing surface (update screen) */
    CGContextRef  screens[10]; /* per-screen bitmaps (0-based, lazily created) */
    int           updscr;      /* current update screen (0-based) */
    int           dspscr;      /* current display screen (0-based) */
    int           bmpW;        /* bitmap width  in points */
    int           bmpH;        /* bitmap height in points */
    pa_winhan     owner;       /* back-pointer to PAWindow */
    int           curVisible;  /* cursor visible flag */
    int           curX, curY;  /* cursor position (0-based pixels) */
    int           curW, curH;  /* cursor size (pixels) */
    int           bufmod;      /* buffered mode flag */
    float         bgR, bgG, bgB; /* background color for margins */
    CGImageRef    displayImage;  /* snapshot for drawRect (threaded mode) */
    int           dispW, dispH;  /* dimensions of displayImage */
}
- (void)createBitmapWidth:(int)w height:(int)h;
- (void)destroyBitmap;
- (CGContextRef)ensureScreen:(int)idx;
@end

@implementation PAView

- (BOOL)isFlipped { return YES; } /* make (0,0) top-left */
- (BOOL)acceptsFirstResponder { return YES; }

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    if ([self inLiveResize] && !bufmod) {
        int w = (int)newSize.width;
        int h = (int)newSize.height;
        if (w > 0 && h > 0 && (w != bmpW || h != bmpH)) {
            pa_rawevent e = {0};
            e.type     = PA_EVT_RESIZE;
            e.win      = owner;
            e.resize.w = w;
            e.resize.h = h;
            evt_push(&e);
            pa_rawevent r = {0};
            r.type     = PA_EVT_REDRAW;
            r.win      = owner;
            r.redraw.w = w;
            r.redraw.h = h;
            evt_push(&r);
        }
        [self setNeedsDisplay:YES];
    }
}

- (void)createBitmapWidth:(int)w height:(int)h
{
    /* release all existing screen bitmaps */
    for (int i = 0; i < 10; i++) {
        if (screens[i]) { CGContextRelease(screens[i]); screens[i] = NULL; }
    }
    bitmap = NULL;

    bmpW = w;
    bmpH = h;
    updscr = 0;
    dspscr = 0;

    /* create screen 0 as the default */
    screens[0] = [self createOneBitmapWidth:w height:h];
    bitmap = screens[0];
}

- (CGContextRef)createOneBitmapWidth:(int)w height:(int)h
{
    CGFloat       scale = self.window ? [self.window backingScaleFactor] : 1.0;
    int           pw    = (int)(w * scale);
    int           ph    = (int)(h * scale);
    CGColorSpaceRef cs  = CGColorSpaceCreateDeviceRGB();

    CGContextRef ctx = CGBitmapContextCreate(NULL, pw, ph, 8, pw * 4, cs,
                                             kCGImageAlphaPremultipliedFirst |
                                             kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(cs);
    CGContextScaleCTM(ctx, scale, scale);
    CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
    CGContextFillRect(ctx, CGRectMake(0, 0, w, h));
    return ctx;
}

- (CGContextRef)ensureScreen:(int)idx
{
    if (idx < 0 || idx >= 10) return screens[0];
    if (!screens[idx])
        screens[idx] = [self createOneBitmapWidth:bmpW height:bmpH];
    return screens[idx];
}

- (void)destroyBitmap
{
    for (int i = 0; i < 10; i++) {
        if (screens[i]) { CGContextRelease(screens[i]); screens[i] = NULL; }
    }
    bitmap = NULL;
}

- (void)drawRect:(NSRect)dirtyRect
{
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    int viewW = (int)self.bounds.size.width;
    int viewH = (int)self.bounds.size.height;

    /* In threaded mode, use the pre-rendered snapshot */
    CGImageRef img;
    int imgW, imgH;
    if (threaded_mode) {
        pthread_mutex_lock(&evt_mutex);
        img = displayImage;
        if (!img) { pthread_mutex_unlock(&evt_mutex); return; }
        CGImageRetain(img);
        imgW = dispW;
        imgH = dispH;
        pthread_mutex_unlock(&evt_mutex);
    } else {
        CGContextRef dsp = screens[dspscr];
        if (!dsp) return;
        img = CGBitmapContextCreateImage(dsp);
        imgW = bmpW;
        imgH = bmpH;
    }

    if ([self inLiveResize] && !bufmod && (imgW != viewW || imgH != viewH)) {
        /* scale last content to fill window during live resize */
        CGContextDrawImage(ctx, CGRectMake(0, 0, viewW, viewH), img);
    } else {
        /* draw bitmap at 1:1, clipped to view bounds */
        CGContextSaveGState(ctx);
        if (viewW < imgW || viewH < imgH) {
            int clipW = viewW < imgW ? viewW : imgW;
            int clipH = viewH < imgH ? viewH : imgH;
            CGContextClipToRect(ctx, CGRectMake(0, 0, clipW, clipH));
        }
        CGContextDrawImage(ctx, CGRectMake(0, 0, imgW, imgH), img);
        CGContextRestoreGState(ctx);

        if (imgW < viewW || imgH < viewH) {
            CGContextSetRGBFillColor(ctx, bgR, bgG, bgB, 1.0);
            if (imgW < viewW)
                CGContextFillRect(ctx, CGRectMake(imgW, 0,
                                                  viewW - imgW, viewH));
            if (imgH < viewH)
                CGContextFillRect(ctx, CGRectMake(0, imgH,
                                                  imgW, viewH - imgH));
        }
    }
    CGImageRelease(img);

    if (curVisible && curW > 0 && curH > 0) {
        CGContextSetBlendMode(ctx, kCGBlendModeDifference);
        CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
        CGContextFillRect(ctx, CGRectMake(curX, curY, curW, curH));
    }
}

- (void)viewDidEndLiveResize
{
    [super viewDidEndLiveResize];
    NSSize s = self.bounds.size;
    int w = (int)s.width;
    int h = (int)s.height;
    if (!threaded_mode && !bufmod && (w != bmpW || h != bmpH)) {
        [self createBitmapWidth:w height:h];
    }
    pa_rawevent e = {0};
    e.type       = PA_EVT_RESIZE;
    e.win        = owner;
    e.resize.w   = w;
    e.resize.h   = h;
    evt_push(&e);
    if (!bufmod) {
        pa_rawevent r = {0};
        r.type     = PA_EVT_REDRAW;
        r.win      = owner;
        r.redraw.w = w;
        r.redraw.h = h;
        evt_push(&r);
    }
    [self setNeedsDisplay:YES];
}

/*--- Keyboard ---*/

static uint32_t translate_key(NSEvent* event, pa_keycode* out_special)
{
    *out_special = 0;
    unsigned short kc = event.keyCode;
    /* map common special keys */
    switch (kc) {
        case 126: *out_special = PA_KEY_UP;       return 0;
        case 125: *out_special = PA_KEY_DOWN;      return 0;
        case 123: *out_special = PA_KEY_LEFT;      return 0;
        case 124: *out_special = PA_KEY_RIGHT;     return 0;
        case 115: *out_special = PA_KEY_HOME;      return 0;
        case 119: *out_special = PA_KEY_END;       return 0;
        case 116: *out_special = PA_KEY_PAGEUP;    return 0;
        case 121: *out_special = PA_KEY_PAGEDOWN;  return 0;
        case 117: *out_special = PA_KEY_DELETE;    return 0;
        case 51:  *out_special = PA_KEY_BACK;      return 0;
        case 36:  *out_special = PA_KEY_ENTER;     return 0;
        case 48:  *out_special = PA_KEY_TAB;       return 0;
        case 53:  *out_special = PA_KEY_ESC;       return 0;
        case 122: *out_special = PA_KEY_F1;        return 0;
        case 120: *out_special = PA_KEY_F2;        return 0;
        case 99:  *out_special = PA_KEY_F3;        return 0;
        case 118: *out_special = PA_KEY_F4;        return 0;
        case 96:  *out_special = PA_KEY_F5;        return 0;
        case 97:  *out_special = PA_KEY_F6;        return 0;
        case 98:  *out_special = PA_KEY_F7;        return 0;
        case 100: *out_special = PA_KEY_F8;        return 0;
        case 101: *out_special = PA_KEY_F9;        return 0;
        case 109: *out_special = PA_KEY_F10;       return 0;
        case 103: *out_special = PA_KEY_F11;       return 0;
        case 111: *out_special = PA_KEY_F12;       return 0;
    }
    /* regular character */
    NSString* chars = event.characters;
    if (chars.length > 0) return [chars characterAtIndex:0];
    return 0;
}

- (void)keyDown:(NSEvent*)event
{
    pa_keycode   special;
    uint32_t     ch = translate_key(event, &special);
    pa_rawevent  e  = {0};
    e.win = owner;
    if (special) {
        e.type         = PA_EVT_KEYDOWN;
        e.special.code = special;
    } else if (ch) {
        e.type   = PA_EVT_CHAR;
        e.key.ch = ch;
    } else return;
    evt_push(&e);
}

/*--- Mouse ---*/

- (NSPoint)flipPoint:(NSPoint)p
{
    /* isFlipped is YES so AppKit already gives us top-left coords */
    return p;
}

- (void)pushMouseEvent:(pa_evttype)type event:(NSEvent*)event
{
    NSPoint      p = [self convertPoint:event.locationInWindow fromView:nil];
    pa_rawevent  e = {0};
    e.type         = type;
    e.win          = owner;
    e.mouse.x      = (int)p.x + 1; /* PA is 1-based */
    e.mouse.y      = (int)p.y + 1;
    e.mouse.buttons = (int)event.buttonNumber + 1;
    evt_push(&e);
}

- (void)mouseMoved:(NSEvent*)e      { [self pushMouseEvent:PA_EVT_MOUSE_MOVE event:e]; }
- (void)mouseDragged:(NSEvent*)e    { [self pushMouseEvent:PA_EVT_MOUSE_MOVE event:e]; }
- (void)mouseDown:(NSEvent*)e       { [self pushMouseEvent:PA_EVT_MOUSE_DOWN event:e]; }
- (void)mouseUp:(NSEvent*)e         { [self pushMouseEvent:PA_EVT_MOUSE_UP   event:e]; }
- (void)rightMouseDown:(NSEvent*)e  { [self pushMouseEvent:PA_EVT_MOUSE_DOWN event:e]; }
- (void)rightMouseUp:(NSEvent*)e    { [self pushMouseEvent:PA_EVT_MOUSE_UP   event:e]; }

@end

/*----------------------------------------------------------------------------
 * PAKeyWindow — NSWindow subclass that accepts key status even when borderless
 *----------------------------------------------------------------------------*/

@interface PAKeyWindow : NSWindow
@property (nonatomic) BOOL suppressResignKey;
@end

@implementation PAKeyWindow
- (BOOL)canBecomeKeyWindow  { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
- (void)resignKeyWindow {
    if (!self.suppressResignKey) [super resignKeyWindow];
}
- (void)resignMainWindow {
    if (!self.suppressResignKey) [super resignMainWindow];
}
@end

/*----------------------------------------------------------------------------
 * PAWindow — wraps NSWindow + PAView + timers
 *----------------------------------------------------------------------------*/

@interface PAWindow : NSObject <NSWindowDelegate> {
@public
    NSWindow*  window;
    PAView*    view;
    int        winid;       /* PA window id */
    NSTimer*   timers[10];  /* up to 10 per-window timers */
}
- (instancetype)initWithX:(int)x y:(int)y width:(int)w height:(int)h
                    title:(const char*)title;
@end

@implementation PAWindow

- (instancetype)initWithX:(int)x y:(int)y width:(int)w height:(int)h
                    title:(const char*)title
{
    self = [super init];
    if (!self) return nil;

    NSRect frame = NSMakeRect(x, y, w, h);
    NSWindowStyleMask style = NSWindowStyleMaskTitled |
                              NSWindowStyleMaskClosable |
                              NSWindowStyleMaskMiniaturizable |
                              NSWindowStyleMaskResizable;

    window = [[PAKeyWindow alloc] initWithContentRect:frame
                                            styleMask:style
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
    window.releasedWhenClosed = NO;
    if (title) window.title = [NSString stringWithUTF8String:title];
    window.delegate = self;
    window.acceptsMouseMovedEvents = YES;

    view = [[PAView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    view->owner = (__bridge pa_winhan)self;
    [view createBitmapWidth:w height:h];
    window.contentView = view;

    memset(timers, 0, sizeof(timers));
    return self;
}

/*--- NSWindowDelegate ---*/

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    pa_rawevent e = {0};
    e.type = PA_EVT_CLOSE;
    e.win  = (__bridge pa_winhan)self;
    evt_push(&e);
    return NO; /* let PA handle it */
}

- (void)windowDidBecomeKey:(NSNotification*)n
{
    pa_rawevent e = {0};
    e.type = PA_EVT_FOCUS;
    e.win  = (__bridge pa_winhan)self;
    evt_push(&e);
}

- (void)windowDidResignKey:(NSNotification*)n
{
    pa_rawevent e = {0};
    e.type = PA_EVT_UNFOCUS;
    e.win  = (__bridge pa_winhan)self;
    evt_push(&e);
}

/*--- Timer support ---*/

- (void)timerFired:(NSTimer*)t
{
    int tid = [(NSNumber*)t.userInfo intValue];
    pa_rawevent e = {0};
    e.type    = PA_EVT_TIMER;
    e.win     = (__bridge pa_winhan)self;
    e.timer.id = tid;
    evt_push(&e);
}

@end

/*----------------------------------------------------------------------------
 * Global NSApplication setup
 *----------------------------------------------------------------------------*/

static BOOL app_inited = NO;

void pa_cocoa_init(void)
{
    if (app_inited) return;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];
    app_inited = YES;
}

void pa_cocoa_deinit(void) { /* nothing for now */ }

/*----------------------------------------------------------------------------
 * Thread transition — move user's main() to a worker thread so the
 * main thread can run the Cocoa event loop.  This mirrors the Windows
 * architecture where the display thread owns the message pump and the
 * user's thread blocks on WaitForSingleObject.
 *----------------------------------------------------------------------------*/

typedef int (*pa_main_func_t)(int, char**);

static void* pa_worker_func(void* arg)
{
    pa_main_func_t user_main = (pa_main_func_t)arg;
    int argc  = *_NSGetArgc();
    char** argv = *_NSGetArgv();
    int result = user_main(argc, argv);
    exit(result);
    return NULL;
}

void pa_cocoa_start_event_thread(void)
{
    pa_main_func_t user_main = (pa_main_func_t)dlsym(RTLD_DEFAULT, "main");
    if (!user_main) {
        fprintf(stderr, "pa_cocoa: cannot find main() — single-threaded fallback\n");
        return;
    }

    threaded_mode = 1;

    pthread_t worker;
    pthread_create(&worker, NULL, pa_worker_func, (void*)user_main);
    pthread_detach(worker);

    /* Main thread enters the Cocoa event loop — never returns.
       The worker thread calls main() which eventually calls exit(). */
    while (1) {
        @autoreleasepool {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantFuture]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
                [NSApp updateWindows];
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * Screen queries
 *----------------------------------------------------------------------------*/

int pa_cocoa_screen_w(void)
{
    return (int)[NSScreen mainScreen].frame.size.width;
}
int pa_cocoa_screen_h(void)
{
    return (int)[NSScreen mainScreen].frame.size.height;
}
int pa_cocoa_screen_wmm(void)
{
    /* use points-per-inch from screen description */
    CGSize sz = CGDisplayScreenSize(CGMainDisplayID());
    return (int)sz.width;
}
int pa_cocoa_screen_hmm(void)
{
    CGSize sz = CGDisplayScreenSize(CGMainDisplayID());
    return (int)sz.height;
}

/*----------------------------------------------------------------------------
 * Window management
 *----------------------------------------------------------------------------*/

pa_winhan pa_cocoa_create_window(int x, int y, int w, int h,
                                  const char* title)
{
    __block pa_winhan result;
    run_on_main(^{
        pa_cocoa_init();
        int screeny = pa_cocoa_screen_h() - y - h;
        PAWindow* pw = [[PAWindow alloc] initWithX:x y:screeny width:w height:h
                                             title:title];
        result = (__bridge_retained pa_winhan)pw;
    });
    return result;
}

void pa_cocoa_destroy_window(pa_winhan win)
{
    if (threaded_mode) {
        PAWindow* pw = (__bridge PAWindow*)win;
        pthread_mutex_lock(&evt_mutex);
        CGImageRef old = pw->view->displayImage;
        pw->view->displayImage = NULL;
        pthread_mutex_unlock(&evt_mutex);
        if (old) CGImageRelease(old);
        run_on_main(^{
            PAWindow* pw2 = (__bridge PAWindow*)win;
            [pw2->window close];
        });
        dispatch_async(dispatch_get_main_queue(), ^{
            CFRelease(win);
        });
    } else {
        PAWindow* pw = (__bridge PAWindow*)win;
        if (pw->view->displayImage) {
            CGImageRelease(pw->view->displayImage);
            pw->view->displayImage = NULL;
        }
        [pw->window close];
        CFRelease(win);
    }
}

void pa_cocoa_show_window(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        [pw->window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    });
}

void pa_cocoa_hide_window(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        [pw->window orderOut:nil];
    });
}

void pa_cocoa_set_title(pa_winhan win, const char* title)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        pw->window.title = [NSString stringWithUTF8String:title];
    });
}

void pa_cocoa_move_window(pa_winhan win, int x, int y)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        int screeny = pa_cocoa_screen_h() - y - (int)pw->window.frame.size.height;
        [pw->window setFrameOrigin:NSMakePoint(x, screeny)];
    });
}

void pa_cocoa_resize_window(pa_winhan win, int w, int h)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSRect f = pw->window.frame;
        CGFloat oldTop = f.origin.y + f.size.height;
        NSRect content = NSMakeRect(0, 0, w, h);
        NSRect newFrame = [pw->window frameRectForContentRect:content];
        newFrame.origin.x = f.origin.x;
        newFrame.origin.y = oldTop - newFrame.size.height;
        [pw->window setFrame:newFrame display:YES];
        [pw->view createBitmapWidth:w height:h];
    });
}

void pa_cocoa_get_size(pa_winhan win, int* w, int* h)
{
    __block int rw, rh;
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSSize s = pw->view.bounds.size;
        rw = (int)s.width;
        rh = (int)s.height;
    });
    *w = rw;
    *h = rh;
}

void pa_cocoa_front(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        [pw->window orderFront:nil];
    });
}

void pa_cocoa_back(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        [pw->window orderBack:nil];
    });
}

void pa_cocoa_focus(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        [pw->window makeKeyAndOrderFront:nil];
    });
}

void pa_cocoa_set_parent(pa_winhan child, pa_winhan parent)
{
    run_on_main(^{
        PAWindow* cw = (__bridge PAWindow*)child;
        PAWindow* pw = (__bridge PAWindow*)parent;
        [pw->window addChildWindow:cw->window ordered:NSWindowAbove];
    });
}

/* Move a child window to (x,y) relative to the parent's content area,
   PA convention: (0,0) = top-left of parent client, y increasing down. */
void pa_cocoa_move_window_child(pa_winhan win, pa_winhan parent, int x, int y)
{
    run_on_main(^{
        PAWindow* cw = (__bridge PAWindow*)win;
        PAWindow* pw = (__bridge PAWindow*)parent;
        NSRect pc = [pw->window contentRectForFrameRect:pw->window.frame];
        CGFloat sx = pc.origin.x + x;
        CGFloat sy = pc.origin.y + pc.size.height - y
                     - cw->window.frame.size.height;
        [cw->window setFrameOrigin:NSMakePoint(sx, sy)];
    });
}

static void refocus_window(NSWindow* w)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [w makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    });
}

void pa_cocoa_set_frame(pa_winhan win, int on)
{
    run_on_main(^{
        PAWindow*       pw    = (__bridge PAWindow*)win;
        PAKeyWindow*    kw    = (PAKeyWindow*)pw->window;
        NSWindowStyleMask all = NSWindowStyleMaskTitled |
                                NSWindowStyleMaskClosable |
                                NSWindowStyleMaskMiniaturizable |
                                NSWindowStyleMaskResizable;
        NSWindowStyleMask m   = kw.styleMask;
        if (on) m |=  all;
        else    m &= ~all;
        kw.suppressResignKey = YES;
        kw.styleMask = m;
        [kw makeKeyAndOrderFront:nil];
        dispatch_async(dispatch_get_main_queue(), ^{
            kw.suppressResignKey = NO;
            [kw makeKeyAndOrderFront:nil];
        });
    });
}

void pa_cocoa_set_sysbar(pa_winhan win, int on)
{
    run_on_main(^{
        PAWindow*       pw  = (__bridge PAWindow*)win;
        PAKeyWindow*    kw  = (PAKeyWindow*)pw->window;
        NSWindowStyleMask bar = NSWindowStyleMaskTitled |
                                NSWindowStyleMaskClosable |
                                NSWindowStyleMaskMiniaturizable;
        NSWindowStyleMask m = kw.styleMask;
        if (on) m |=  bar;
        else    m &= ~bar;
        kw.suppressResignKey = YES;
        kw.styleMask = m;
        [kw makeKeyAndOrderFront:nil];
        dispatch_async(dispatch_get_main_queue(), ^{
            kw.suppressResignKey = NO;
            [kw makeKeyAndOrderFront:nil];
        });
    });
}

void pa_cocoa_set_sizable(pa_winhan win, int on)
{
    run_on_main(^{
        PAWindow*       pw  = (__bridge PAWindow*)win;
        PAKeyWindow*    kw  = (PAKeyWindow*)pw->window;
        NSWindowStyleMask m = kw.styleMask;
        if (on) m |=  NSWindowStyleMaskResizable;
        else    m &= ~NSWindowStyleMaskResizable;
        kw.suppressResignKey = YES;
        kw.styleMask = m;
        [kw makeKeyAndOrderFront:nil];
        dispatch_async(dispatch_get_main_queue(), ^{
            kw.suppressResignKey = NO;
            [kw makeKeyAndOrderFront:nil];
        });
    });
}

/*----------------------------------------------------------------------------
 * Drawing context
 *----------------------------------------------------------------------------*/

CGContextRef pa_cocoa_get_context(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    return pw->view->screens[pw->view->updscr];
}

void pa_cocoa_flush(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    if (threaded_mode) {
        PAView* v = pw->view;
        CGContextRef ctx = v->screens[v->dspscr];
        if (!ctx) return;
        size_t w = CGBitmapContextGetWidth(ctx);
        size_t h = CGBitmapContextGetHeight(ctx);
        size_t bpr = CGBitmapContextGetBytesPerRow(ctx);
        void* data = CGBitmapContextGetData(ctx);
        if (!data || w == 0 || h == 0) return;
        CFDataRef pixelData = CFDataCreate(NULL, (const UInt8*)data, h * bpr);
        if (!pixelData) return;
        CGColorSpaceRef cs = CGBitmapContextGetColorSpace(ctx);
        CGDataProviderRef dp = CGDataProviderCreateWithCFData(pixelData);
        CFRelease(pixelData);
        CGImageRef snap = CGImageCreate(w, h,
            CGBitmapContextGetBitsPerComponent(ctx),
            CGBitmapContextGetBitsPerPixel(ctx),
            bpr, cs, CGBitmapContextGetBitmapInfo(ctx),
            dp, NULL, false, kCGRenderingIntentDefault);
        CGDataProviderRelease(dp);
        if (!snap) return;
        pthread_mutex_lock(&evt_mutex);
        CGImageRef old = v->displayImage;
        v->displayImage = snap;
        v->dispW = (int)w;
        v->dispH = (int)h;
        pthread_mutex_unlock(&evt_mutex);
        if (old) CGImageRelease(old);
        dispatch_async(dispatch_get_main_queue(), ^{
            [v setNeedsDisplay:YES];
        });
    } else {
        [pw->view setNeedsDisplay:YES];
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:0]];
    }
}

void pa_cocoa_set_cursor(pa_winhan win, int visible, int x, int y, int w, int h)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    PAView*   v  = pw->view;
    int changed = (v->curVisible != visible || v->curX != x || v->curY != y ||
                   v->curW != w || v->curH != h);
    v->curVisible = visible;
    v->curX = x;
    v->curY = y;
    v->curW = w;
    v->curH = h;
    if (changed) {
        if (threaded_mode) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [v setNeedsDisplay:YES];
            });
        } else {
            [v setNeedsDisplay:YES];
        }
    }
}

void pa_cocoa_resize_bitmap(pa_winhan win, int w, int h)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    PAView*   v  = pw->view;

    int oldW = v->bmpW;
    int oldH = v->bmpH;

    /* save old display screen bitmap */
    CGContextRef oldCtx = v->screens[v->dspscr];
    CGImageRef oldImg = oldCtx ? CGBitmapContextCreateImage(oldCtx) : NULL;

    /* release all screen bitmaps except detach the display one */
    v->screens[v->dspscr] = NULL;
    for (int i = 0; i < 10; i++) {
        if (v->screens[i]) { CGContextRelease(v->screens[i]); v->screens[i] = NULL; }
    }
    if (oldCtx) CGContextRelease(oldCtx);

    /* create new bitmaps at new size */
    v->bmpW = w;
    v->bmpH = h;
    v->screens[v->dspscr] = [v createOneBitmapWidth:w height:h];
    v->bitmap = v->screens[v->dspscr];
    if (v->updscr != v->dspscr) {
        v->screens[v->updscr] = [v createOneBitmapWidth:w height:h];
    }

    /* copy intersection of old content into new display bitmap */
    if (oldImg) {
        int copyW = oldW < w ? oldW : w;
        int copyH = oldH < h ? oldH : h;
        if (copyW > 0 && copyH > 0) {
            CGContextRef dst = v->screens[v->dspscr];
            CGContextSaveGState(dst);
            CGContextClipToRect(dst, CGRectMake(0, 0, copyW, copyH));
            CGContextDrawImage(dst, CGRectMake(0, 0, oldW, oldH), oldImg);
            CGContextRestoreGState(dst);
        }
        CGImageRelease(oldImg);
    }

    if (!threaded_mode) [v setNeedsDisplay:YES];
}

void pa_cocoa_set_bufmod(pa_winhan win, int on)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    pw->view->bufmod = on;
}

void pa_cocoa_set_background(pa_winhan win, float r, float g, float b)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    pw->view->bgR = r;
    pw->view->bgG = g;
    pw->view->bgB = b;
}

void pa_cocoa_select_screens(pa_winhan win, int upd, int dsp)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    PAView*   v  = pw->view;
    if (upd < 0 || upd >= 10 || dsp < 0 || dsp >= 10) return;
    [v ensureScreen:upd];
    [v ensureScreen:dsp];
    v->updscr = upd;
    int old_dsp = v->dspscr;
    v->dspscr = dsp;
    if (dsp != old_dsp) {
        if (threaded_mode) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [v setNeedsDisplay:YES];
            });
        } else {
            [v setNeedsDisplay:YES];
        }
    }
}

/*----------------------------------------------------------------------------
 * Event processing
 *----------------------------------------------------------------------------*/

void pa_cocoa_process_ns_events(void)
{
    if (threaded_mode) return;
    NSEvent* event;
    while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES])) {
        [NSApp sendEvent:event];
        [NSApp updateWindows];
    }
}

int pa_cocoa_dequeue(pa_rawevent* evt)
{
    return evt_pop(evt);
}

void pa_cocoa_wait(pa_rawevent* evt)
{
    if (threaded_mode) {
        pthread_mutex_lock(&evt_mutex);
        while (evt_tail == evt_head)
            pthread_cond_wait(&evt_cond, &evt_mutex);
        *evt = evt_queue[evt_tail];
        evt_tail = (evt_tail + 1) % EVT_QUEUE_SIZE;
        pthread_mutex_unlock(&evt_mutex);
    } else {
        while (evt_empty()) {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
                [NSApp updateWindows];
            }
        }
        evt_pop(evt);
    }
}

/*----------------------------------------------------------------------------
 * Timers
 *----------------------------------------------------------------------------*/

void pa_cocoa_set_timer(pa_winhan win, int id, long us100, int repeat)
{
    if (id < 0 || id >= 10) return;
    run_on_main(^{
        PAWindow* pw  = (__bridge PAWindow*)win;
        if (pw->timers[id]) { [pw->timers[id] invalidate]; pw->timers[id] = nil; }
        NSTimeInterval interval = us100 * 0.0001;
        NSNumber* tid  = [NSNumber numberWithInt:id];
        pw->timers[id] = [NSTimer scheduledTimerWithTimeInterval:interval
                                                          target:pw
                                                        selector:@selector(timerFired:)
                                                        userInfo:tid
                                                         repeats:(repeat ? YES : NO)];
    });
}

void pa_cocoa_kill_timer(pa_winhan win, int id)
{
    if (id < 0 || id >= 10) return;
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        [pw->timers[id] invalidate];
        pw->timers[id] = nil;
    });
}

/*----------------------------------------------------------------------------
 * Widgets
 *----------------------------------------------------------------------------*/

/* Helper: find a subview by tag */
static NSView* find_widget(pa_winhan win, int id)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    for (NSView* v in pw->view.subviews)
        if (pa_get_tag(v) == id) return v;
    return nil;
}

void pa_cocoa_button(pa_winhan win, int x, int y, int w, int h,
                     const char* label, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSButton* b  = [[NSButton alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        b.title      = [NSString stringWithUTF8String:label];
        b.bezelStyle = NSBezelStyleRounded;
        b.buttonType = NSButtonTypeMomentaryPushIn;
        b.tag        = id;
        b.target     = pw;
        b.action     = @selector(widgetAction:);
        [pw->view addSubview:b];
    });
}

void pa_cocoa_checkbox(pa_winhan win, int x, int y, int w, int h,
                       const char* label, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSButton* b  = [[NSButton alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        b.title      = [NSString stringWithUTF8String:label];
        b.buttonType = NSButtonTypeSwitch;
        b.tag        = id;
        b.target     = pw;
        b.action     = @selector(widgetAction:);
        [pw->view addSubview:b];
    });
}

void pa_cocoa_radiobutton(pa_winhan win, int x, int y, int w, int h,
                          const char* label, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSButton* b  = [[NSButton alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        b.title      = [NSString stringWithUTF8String:label];
        b.buttonType = NSButtonTypeRadio;
        b.tag        = id;
        b.target     = pw;
        b.action     = @selector(widgetAction:);
        [pw->view addSubview:b];
    });
}

void pa_cocoa_editbox(pa_winhan win, int x, int y, int w, int h, int id)
{
    run_on_main(^{
        PAWindow*    pw = (__bridge PAWindow*)win;
        NSTextField* tf = [[NSTextField alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        tf.tag = id;
        [pw->view addSubview:tf];
    });
}

void pa_cocoa_scrollvert(pa_winhan win, int x, int y, int w, int h, int id)
{
    run_on_main(^{
        PAWindow*  pw = (__bridge PAWindow*)win;
        NSScroller* s = [[NSScroller alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        s.tag = id;
        s.target  = pw;
        s.action  = @selector(scrollAction:);
        [pw->view addSubview:s];
    });
}

void pa_cocoa_scrollhoriz(pa_winhan win, int x, int y, int w, int h, int id)
{
    pa_cocoa_scrollvert(win, x, y, w, h, id);
}

void pa_cocoa_slider_horiz(pa_winhan win, int x, int y, int w, int h,
                           int mark, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSSlider* s  = [[NSSlider alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        s.minValue   = 0;
        s.maxValue   = INT_MAX;
        s.intValue   = mark;
        s.tag        = id;
        s.target     = pw;
        s.action     = @selector(sliderAction:);
        [pw->view addSubview:s];
    });
}

void pa_cocoa_slider_vert(pa_winhan win, int x, int y, int w, int h,
                          int mark, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSSlider* s  = [[NSSlider alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        s.vertical   = YES;
        s.minValue   = 0;
        s.maxValue   = INT_MAX;
        s.intValue   = mark;
        s.tag        = id;
        s.target     = pw;
        s.action     = @selector(sliderAction:);
        [pw->view addSubview:s];
    });
}

void pa_cocoa_progressbar(pa_winhan win, int x, int y, int w, int h, int id)
{
    run_on_main(^{
        PAWindow*        pw = (__bridge PAWindow*)win;
        NSProgressIndicator* p =
            [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        p.style          = NSProgressIndicatorStyleBar;
        p.indeterminate  = NO;
        p.minValue       = 0;
        p.maxValue       = INT_MAX;
        pa_set_tag(p, id);
        [pw->view addSubview:p];
    });
}

void pa_cocoa_kill_widget(pa_winhan win, int id)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v) [v removeFromSuperview];
    });
}

void pa_cocoa_widget_text(pa_winhan win, int id, const char* s)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (!v) return;
        NSString* ns = [NSString stringWithUTF8String:s];
        if ([v isKindOfClass:[NSControl class]])
            [(NSControl*)v setStringValue:ns];
    });
}

void pa_cocoa_widget_get_text(pa_winhan win, int id, char* s, int sl)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (!v) { if (sl > 0) s[0] = 0; return; }
        if ([v isKindOfClass:[NSControl class]]) {
            const char* cs = [[(NSControl*)v stringValue] UTF8String];
            strncpy(s, cs, sl-1);
            s[sl-1] = 0;
        }
    });
}

void pa_cocoa_widget_enable(pa_winhan win, int id, int on)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[NSControl class]])
            [(NSControl*)v setEnabled:(on ? YES : NO)];
    });
}

void pa_cocoa_widget_select(pa_winhan win, int id, int on)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[NSButton class]])
            [(NSButton*)v setState:(on ? NSControlStateValueOn : NSControlStateValueOff)];
    });
}

void pa_cocoa_scrollbar_pos(pa_winhan win, int id, int pos)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[NSScroller class]])
            [(NSScroller*)v setFloatValue:(float)pos / INT_MAX];
    });
}

void pa_cocoa_scrollbar_siz(pa_winhan win, int id, int range)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[NSScroller class]])
            [(NSScroller*)v setKnobProportion:(float)range / INT_MAX];
    });
}

void pa_cocoa_progressbar_pos(pa_winhan win, int id, int pos)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[NSProgressIndicator class]])
            [(NSProgressIndicator*)v setDoubleValue:(double)pos];
    });
}

/*----------------------------------------------------------------------------
 * Widget action callbacks (target/action from controls)
 *----------------------------------------------------------------------------*/

@implementation PAWindow (Actions)

- (void)widgetAction:(id)sender
{
    NSControl*  ctrl = (NSControl*)sender;
    pa_rawevent e    = {0};
    e.win            = (__bridge pa_winhan)self;

    e.type   = PA_EVT_CHAR; /* TODO: map to proper PA widget events */
    e.key.ch = (uint32_t)ctrl.tag; /* carry widget id */
    evt_push(&e);
}

- (void)scrollAction:(id)sender
{
    /* TODO: translate scroller position to PA scroll events */
}

- (void)sliderAction:(id)sender
{
    /* TODO: translate slider position to PA slider events */
}

@end

/*----------------------------------------------------------------------------
 * Menus — native macOS menu bar
 *----------------------------------------------------------------------------*/

/* Layout-compatible mirror of ami_menurec so we can walk the tree */
typedef struct pa_menu_node {
    struct pa_menu_node* next;
    struct pa_menu_node* branch;
    int onoff;
    int oneof;
    int bar;
    int id;
    char* face;
} pa_menu_node;

/* Menu action target — fires PA_EVT_MENU events */
@interface PAMenuTarget : NSObject
@property (assign) pa_winhan owner;
- (void)menuItemAction:(NSMenuItem*)sender;
@end

@implementation PAMenuTarget
- (void)menuItemAction:(NSMenuItem*)sender
{
    pa_rawevent e = {0};
    e.type    = PA_EVT_MENU;
    e.win     = self.owner;
    e.menu.id = (int)sender.tag;
    evt_push(&e);
}
@end

static PAMenuTarget* menuTarget = nil;
static NSMenu* savedAppMenu = nil;

static NSMenuItem* findMenuItemByTag(NSMenu* menu, int tag)
{
    for (NSInteger i = 0; i < menu.numberOfItems; i++) {
        NSMenuItem* item = [menu itemAtIndex:i];
        if (item.tag == tag && !item.hasSubmenu) return item;
        if (item.hasSubmenu) {
            NSMenuItem* found = findMenuItemByTag(item.submenu, tag);
            if (found) return found;
        }
    }
    return nil;
}

static void buildMenu(NSMenu* menu, pa_menu_node* list, PAMenuTarget* target)
{
    menu.autoenablesItems = NO;
    for (pa_menu_node* m = list; m; m = m->next) {
        NSString* title = [NSString stringWithUTF8String:m->face];
        if (m->branch) {
            NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                          action:nil
                                                   keyEquivalent:@""];
            item.tag = m->id;
            NSMenu* sub = [[NSMenu alloc] initWithTitle:title];
            buildMenu(sub, m->branch, target);
            item.submenu = sub;
            [menu addItem:item];
        } else {
            NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                          action:@selector(menuItemAction:)
                                                   keyEquivalent:@""];
            item.target = target;
            item.tag = m->id;
            [menu addItem:item];
        }
        if (m->bar) {
            [menu addItem:[NSMenuItem separatorItem]];
        }
    }
}

void pa_cocoa_menu(pa_winhan win, void* menu_list)
{
    run_on_main(^{
        pa_menu_node* list = (pa_menu_node*)menu_list;

        if (!menuTarget) {
            menuTarget = [[PAMenuTarget alloc] init];
        }
        menuTarget.owner = win;

        NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@""];
        mainMenu.autoenablesItems = NO;

        /* preserve app menu (first item), append PA items into its submenu */
        if (!savedAppMenu) {
            NSMenu* cur = [NSApp mainMenu];
            if (cur && cur.numberOfItems > 0) {
                savedAppMenu = [[cur itemAtIndex:0] copy];
            }
        }
        if (savedAppMenu) {
            NSMenuItem* appItem = [savedAppMenu copy];
            if (list) {
                NSMenu* appSub = appItem.submenu;
                appSub.autoenablesItems = NO;
                [appSub addItem:[NSMenuItem separatorItem]];
                buildMenu(appSub, list, menuTarget);
            }
            [mainMenu addItem:appItem];
        } else if (list) {
            NSString* progName = [NSProcessInfo processInfo].processName;
            NSMenuItem* appItem = [[NSMenuItem alloc] initWithTitle:progName
                                                              action:nil
                                                       keyEquivalent:@""];
            NSMenu* appSub = [[NSMenu alloc] initWithTitle:progName];
            appSub.autoenablesItems = NO;
            buildMenu(appSub, list, menuTarget);
            appItem.submenu = appSub;
            [mainMenu addItem:appItem];
        }

        [NSApp setMainMenu:mainMenu];
    });
}

void pa_cocoa_menu_enable(pa_winhan win, int id, int on)
{
    run_on_main(^{
        NSMenu* mainMenu = [NSApp mainMenu];
        if (!mainMenu) return;
        NSMenuItem* item = findMenuItemByTag(mainMenu, id);
        if (item) item.enabled = on ? YES : NO;
    });
}

void pa_cocoa_menu_check(pa_winhan win, int id, int on)
{
    run_on_main(^{
        NSMenu* mainMenu = [NSApp mainMenu];
        if (!mainMenu) return;
        NSMenuItem* item = findMenuItemByTag(mainMenu, id);
        if (item) item.state = on ? NSControlStateValueOn : NSControlStateValueOff;
    });
}

/*----------------------------------------------------------------------------
 * Dialogs
 *----------------------------------------------------------------------------*/

void pa_cocoa_alert(const char* title, const char* message)
{
    run_on_main(^{
        NSAlert* a    = [[NSAlert alloc] init];
        a.messageText = [NSString stringWithUTF8String:title   ? title   : ""];
        a.informativeText = [NSString stringWithUTF8String:message ? message : ""];
        [a addButtonWithTitle:@"OK"];
        [a runModal];
    });
}

void pa_cocoa_query_open(char* path, int pathlen)
{
    run_on_main(^{
        NSOpenPanel* p = [NSOpenPanel openPanel];
        p.canChooseFiles    = YES;
        p.canChooseDirectories = NO;
        p.allowsMultipleSelection = NO;
        if ([p runModal] == NSModalResponseOK && p.URLs.count > 0) {
            const char* u = [p.URLs[0].path UTF8String];
            strncpy(path, u, pathlen-1);
            path[pathlen-1] = 0;
        } else {
            path[0] = 0;
        }
    });
}

void pa_cocoa_query_save(char* path, int pathlen)
{
    run_on_main(^{
        NSSavePanel* p = [NSSavePanel savePanel];
        if ([p runModal] == NSModalResponseOK && p.URL) {
            const char* u = [p.URL.path UTF8String];
            strncpy(path, u, pathlen-1);
            path[pathlen-1] = 0;
        } else {
            path[0] = 0;
        }
    });
}

void pa_cocoa_inject_close(void)
{
    pa_rawevent e = {0};
    e.type = PA_EVT_CLOSE;
    e.win  = NULL;
    evt_push(&e);
}

/*----------------------------------------------------------------------------
 * Joystick support via IOKit HID
 *----------------------------------------------------------------------------*/

#define MAXJOY 10
#define MAXJAX 6

typedef struct {
    IOHIDDeviceRef dev;
    int            axes;
    int            buttons;
    int            axsav[MAXJAX];
    IOHIDElementRef axelem[MAXJAX];
    long           axmin[MAXJAX];
    long           axmax[MAXJAX];
} joyrec;

static joyrec          joytab[MAXJOY];
static int             numjoy;
static IOHIDManagerRef hidmgr;

static int joy_find(IOHIDDeviceRef dev)
{
    for (int i = 0; i < numjoy; i++)
        if (joytab[i].dev == dev) return i;
    return -1;
}

static int scale_hid(long val, long lo, long hi)
{
    if (hi <= lo) return 0;
    long mid = (lo + hi) / 2;
    long half = (hi - lo) / 2;
    if (half == 0) return 0;
    long long scaled = (long long)(val - mid) * INT_MAX / half;
    if (scaled > INT_MAX) return INT_MAX;
    if (scaled < -INT_MAX) return -INT_MAX;
    return (int)scaled;
}

static int axis_index_for_usage(uint32_t usage)
{
    switch (usage) {
    case kHIDUsage_GD_X:  return 0;
    case kHIDUsage_GD_Y:  return 1;
    case kHIDUsage_GD_Z:  return 2;
    case kHIDUsage_GD_Rx: return 3;
    case kHIDUsage_GD_Ry: return 4;
    case kHIDUsage_GD_Rz: return 5;
    default: return -1;
    }
}

static void hid_input_cb(void* ctx, IOReturn result, void* sender,
                          IOHIDValueRef value)
{
    IOHIDElementRef elem = IOHIDValueGetElement(value);
    IOHIDDeviceRef  dev  = IOHIDElementGetDevice(elem);
    int idx = joy_find(dev);
    if (idx < 0) return;
    joyrec* jp = &joytab[idx];

    uint32_t page  = IOHIDElementGetUsagePage(elem);
    uint32_t usage = IOHIDElementGetUsage(elem);
    long     raw   = IOHIDValueGetIntegerValue(value);

    if (page == kHIDPage_GenericDesktop) {
        int ai = axis_index_for_usage(usage);
        if (ai >= 0 && ai < jp->axes) {
            jp->axsav[ai] = scale_hid(raw, jp->axmin[ai], jp->axmax[ai]);
            pa_rawevent e = {0};
            e.type = PA_EVT_JOY_MOVE;
            e.win  = NULL;
            e.joymove.jn = idx;
            for (int i = 0; i < MAXJAX; i++)
                e.joymove.ax[i] = jp->axsav[i];
            evt_push(&e);
        }
        if (usage == kHIDUsage_GD_Hatswitch) {
            /* hat switch: map to axes 4,5 if not already used */
            /* for now just skip hat */
        }
    } else if (page == kHIDPage_Button) {
        int btn = (int)usage; /* 1-based */
        pa_rawevent e = {0};
        e.type = raw ? PA_EVT_JOY_DOWN : PA_EVT_JOY_UP;
        e.win  = NULL;
        e.joybtn.jn  = idx;
        e.joybtn.btn = btn;
        evt_push(&e);
    }
}

static void hid_match_cb(void* ctx, IOReturn result, void* sender,
                          IOHIDDeviceRef dev)
{
    if (numjoy >= MAXJOY) return;
    if (joy_find(dev) >= 0) return;

    joyrec* jp = &joytab[numjoy];
    memset(jp, 0, sizeof(*jp));
    jp->dev = dev;

    /* enumerate elements to count axes and buttons */
    CFArrayRef elems = IOHIDDeviceCopyMatchingElements(dev, NULL,
                           kIOHIDOptionsTypeNone);
    if (!elems) { jp->dev = NULL; return; }

    int nax = 0, nbtn = 0;
    CFIndex n = CFArrayGetCount(elems);
    for (CFIndex i = 0; i < n; i++) {
        IOHIDElementRef el = (IOHIDElementRef)CFArrayGetValueAtIndex(elems, i);
        IOHIDElementType etype = IOHIDElementGetType(el);
        if (etype != kIOHIDElementTypeInput_Misc &&
            etype != kIOHIDElementTypeInput_Axis &&
            etype != kIOHIDElementTypeInput_Button)
            continue;
        uint32_t pg = IOHIDElementGetUsagePage(el);
        uint32_t us = IOHIDElementGetUsage(el);

        if (pg == kHIDPage_GenericDesktop) {
            int ai = axis_index_for_usage(us);
            if (ai >= 0 && ai < MAXJAX) {
                jp->axelem[ai] = el;
                jp->axmin[ai]  = IOHIDElementGetLogicalMin(el);
                jp->axmax[ai]  = IOHIDElementGetLogicalMax(el);
                if (ai + 1 > nax) nax = ai + 1;
            }
        } else if (pg == kHIDPage_Button) {
            int bn = (int)us;
            if (bn > nbtn) nbtn = bn;
        }
    }
    CFRelease(elems);

    jp->axes    = nax > MAXJAX ? MAXJAX : nax;
    jp->buttons = nbtn;
    numjoy++;
}

static void hid_remove_cb(void* ctx, IOReturn result, void* sender,
                           IOHIDDeviceRef dev)
{
    int idx = joy_find(dev);
    if (idx < 0) return;
    joytab[idx].dev = NULL;
    joytab[idx].axes = 0;
    joytab[idx].buttons = 0;
}

void pa_cocoa_joy_init(void)
{
    numjoy = 0;
    hidmgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!hidmgr) return;

    /* match joysticks and gamepads */
    NSDictionary* joy = @{
        @(kIOHIDDeviceUsagePageKey): @(kHIDPage_GenericDesktop),
        @(kIOHIDDeviceUsageKey):     @(kHIDUsage_GD_Joystick)
    };
    NSDictionary* pad = @{
        @(kIOHIDDeviceUsagePageKey): @(kHIDPage_GenericDesktop),
        @(kIOHIDDeviceUsageKey):     @(kHIDUsage_GD_GamePad)
    };
    NSDictionary* multi = @{
        @(kIOHIDDeviceUsagePageKey): @(kHIDPage_GenericDesktop),
        @(kIOHIDDeviceUsageKey):     @(kHIDUsage_GD_MultiAxisController)
    };
    IOHIDManagerSetDeviceMatchingMultiple(hidmgr,
        (__bridge CFArrayRef)@[joy, pad, multi]);

    IOHIDManagerRegisterDeviceMatchingCallback(hidmgr, hid_match_cb, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(hidmgr, hid_remove_cb, NULL);
    IOHIDManagerRegisterInputValueCallback(hidmgr, hid_input_cb, NULL);
    IOHIDManagerScheduleWithRunLoop(hidmgr, CFRunLoopGetMain(),
                                    kCFRunLoopDefaultMode);
    IOHIDManagerOpen(hidmgr, kIOHIDOptionsTypeNone);

    /* pump the run loop once to pick up already-connected devices */
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
}

void pa_cocoa_joy_deinit(void)
{
    if (hidmgr) {
        IOHIDManagerClose(hidmgr, kIOHIDOptionsTypeNone);
        IOHIDManagerUnscheduleFromRunLoop(hidmgr, CFRunLoopGetMain(),
                                          kCFRunLoopDefaultMode);
        CFRelease(hidmgr);
        hidmgr = NULL;
    }
    for (int i = 0; i < numjoy; i++) {
        joytab[i].dev = NULL;
        joytab[i].axes = 0;
        joytab[i].buttons = 0;
    }
    numjoy = 0;
}

int pa_cocoa_joy_count(void)   { return numjoy; }

int pa_cocoa_joy_buttons(int j)
{
    if (j < 1 || j > numjoy) return 0;
    return joytab[j-1].buttons;
}

int pa_cocoa_joy_axes(int j)
{
    if (j < 1 || j > numjoy) return 0;
    int a = joytab[j-1].axes;
    return a > MAXJAX ? MAXJAX : a;
}

CTFontRef pa_cocoa_system_mono_font(CGFloat size)
{
    NSFont* f = [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightRegular];
    if (f) return CTFontCreateWithName((__bridge CFStringRef)[f fontName], size, NULL);
    return NULL;
}
