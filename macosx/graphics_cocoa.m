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
#include <IOKit/hid/IOHIDManager.h>
#include "pa_cocoa.h"

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
    int next = (evt_head + 1) % EVT_QUEUE_SIZE;
    if (next == evt_tail) return; /* full — drop oldest could be done here */
    evt_queue[evt_head] = *e;
    evt_head = next;
}

static int evt_pop(pa_rawevent* e)
{
    if (evt_head == evt_tail) return 0;
    *e = evt_queue[evt_tail];
    evt_tail = (evt_tail + 1) % EVT_QUEUE_SIZE;
    return 1;
}

static int evt_empty(void) { return evt_head == evt_tail; }

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
}
- (void)createBitmapWidth:(int)w height:(int)h;
- (void)destroyBitmap;
- (CGContextRef)ensureScreen:(int)idx;
@end

@implementation PAView

- (BOOL)isFlipped { return YES; } /* make (0,0) top-left */
- (BOOL)acceptsFirstResponder { return YES; }

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
    CGContextRef dsp = screens[dspscr];
    if (!dsp) return;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGImageRef   img = CGBitmapContextCreateImage(dsp);
    CGContextDrawImage(ctx, NSRectToCGRect(self.bounds), img);
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
    [self createBitmapWidth:(int)s.width height:(int)s.height];
    pa_rawevent e = {0};
    e.type       = PA_EVT_RESIZE;
    e.win        = owner;
    e.resize.w   = (int)s.width;
    e.resize.h   = (int)s.height;
    evt_push(&e);
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

    window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:style
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
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
    pa_cocoa_init();
    /* Cocoa screen coords: (0,0) is bottom-left; flip y */
    int screeny = pa_cocoa_screen_h() - y - h;
    PAWindow* pw = [[PAWindow alloc] initWithX:x y:screeny width:w height:h
                                         title:title];
    return (__bridge_retained pa_winhan)pw;
}

void pa_cocoa_destroy_window(pa_winhan win)
{
    PAWindow* pw = (__bridge_transfer PAWindow*)win;
    [pw->window close];
    (void)pw;
}

void pa_cocoa_show_window(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    [pw->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void pa_cocoa_hide_window(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    [pw->window orderOut:nil];
}

void pa_cocoa_set_title(pa_winhan win, const char* title)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    pw->window.title = [NSString stringWithUTF8String:title];
}

void pa_cocoa_move_window(pa_winhan win, int x, int y)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    int screeny = pa_cocoa_screen_h() - y - (int)pw->window.frame.size.height;
    [pw->window setFrameOrigin:NSMakePoint(x, screeny)];
}

void pa_cocoa_resize_window(pa_winhan win, int w, int h)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    NSRect f = pw->window.frame;
    CGFloat oldTop = f.origin.y + f.size.height;
    NSRect content = NSMakeRect(0, 0, w, h);
    NSRect newFrame = [pw->window frameRectForContentRect:content];
    newFrame.origin.x = f.origin.x;
    newFrame.origin.y = oldTop - newFrame.size.height;
    [pw->window setFrame:newFrame display:YES];
    [pw->view createBitmapWidth:w height:h];
}

void pa_cocoa_get_size(pa_winhan win, int* w, int* h)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    NSSize s = pw->view.bounds.size;
    *w = (int)s.width;
    *h = (int)s.height;
}

void pa_cocoa_front(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    [pw->window orderFront:nil];
}

void pa_cocoa_back(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    [pw->window orderBack:nil];
}

void pa_cocoa_focus(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    [pw->window makeKeyAndOrderFront:nil];
}

void pa_cocoa_set_frame(pa_winhan win, int on)
{
    PAWindow*       pw    = (__bridge PAWindow*)win;
    NSWindowStyleMask m   = pw->window.styleMask;
    if (on) m |=  NSWindowStyleMaskTitled;
    else    m &= ~NSWindowStyleMaskTitled;
    pw->window.styleMask = m;
}

void pa_cocoa_set_sizable(pa_winhan win, int on)
{
    PAWindow*       pw  = (__bridge PAWindow*)win;
    NSWindowStyleMask m = pw->window.styleMask;
    if (on) m |=  NSWindowStyleMaskResizable;
    else    m &= ~NSWindowStyleMaskResizable;
    pw->window.styleMask = m;
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
    [pw->view setNeedsDisplay:YES];
    /* pump the run loop briefly so the display actually updates */
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate dateWithTimeIntervalSinceNow:0]];
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
    if (changed) [v setNeedsDisplay:YES];
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
    if (dsp != old_dsp) [v setNeedsDisplay:YES];
}

/*----------------------------------------------------------------------------
 * Event processing
 *----------------------------------------------------------------------------*/

void pa_cocoa_process_ns_events(void)
{
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
    while (evt_empty()) {
        /* short timeout so timer events and signal-injected events get picked up */
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

/*----------------------------------------------------------------------------
 * Timers
 *----------------------------------------------------------------------------*/

void pa_cocoa_set_timer(pa_winhan win, int id, long us100, int repeat)
{
    PAWindow* pw  = (__bridge PAWindow*)win;
    if (id < 0 || id >= 10) return;
    if (pw->timers[id]) { [pw->timers[id] invalidate]; pw->timers[id] = nil; }
    /* PA timer units are 100 microseconds (0.0001s) */
    NSTimeInterval interval = us100 * 0.0001;
    NSNumber* tid  = [NSNumber numberWithInt:id];
    pw->timers[id] = [NSTimer scheduledTimerWithTimeInterval:interval
                                                      target:pw
                                                    selector:@selector(timerFired:)
                                                    userInfo:tid
                                                     repeats:(repeat ? YES : NO)];
}

void pa_cocoa_kill_timer(pa_winhan win, int id)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    if (id < 0 || id >= 10) return;
    [pw->timers[id] invalidate];
    pw->timers[id] = nil;
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
    PAWindow* pw = (__bridge PAWindow*)win;
    NSButton* b  = [[NSButton alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
    b.title      = [NSString stringWithUTF8String:label];
    b.bezelStyle = NSBezelStyleRounded;
    b.buttonType = NSButtonTypeMomentaryPushIn;
    b.tag        = id;
    b.target     = pw;
    b.action     = @selector(widgetAction:);
    [pw->view addSubview:b];
}

void pa_cocoa_checkbox(pa_winhan win, int x, int y, int w, int h,
                       const char* label, int id)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    NSButton* b  = [[NSButton alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
    b.title      = [NSString stringWithUTF8String:label];
    b.buttonType = NSButtonTypeSwitch;
    b.tag        = id;
    b.target     = pw;
    b.action     = @selector(widgetAction:);
    [pw->view addSubview:b];
}

void pa_cocoa_radiobutton(pa_winhan win, int x, int y, int w, int h,
                          const char* label, int id)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    NSButton* b  = [[NSButton alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
    b.title      = [NSString stringWithUTF8String:label];
    b.buttonType = NSButtonTypeRadio;
    b.tag        = id;
    b.target     = pw;
    b.action     = @selector(widgetAction:);
    [pw->view addSubview:b];
}

void pa_cocoa_editbox(pa_winhan win, int x, int y, int w, int h, int id)
{
    PAWindow*    pw = (__bridge PAWindow*)win;
    NSTextField* tf = [[NSTextField alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
    tf.tag = id;
    [pw->view addSubview:tf];
}

void pa_cocoa_scrollvert(pa_winhan win, int x, int y, int w, int h, int id)
{
    PAWindow*  pw = (__bridge PAWindow*)win;
    NSScroller* s = [[NSScroller alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
    s.tag = id;
    s.target  = pw;
    s.action  = @selector(scrollAction:);
    [pw->view addSubview:s];
}

void pa_cocoa_scrollhoriz(pa_winhan win, int x, int y, int w, int h, int id)
{
    pa_cocoa_scrollvert(win, x, y, w, h, id); /* same control, different orientation */
}

void pa_cocoa_slider_horiz(pa_winhan win, int x, int y, int w, int h,
                           int mark, int id)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    NSSlider* s  = [[NSSlider alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
    s.minValue   = 0;
    s.maxValue   = INT_MAX;
    s.intValue   = mark;
    s.tag        = id;
    s.target     = pw;
    s.action     = @selector(sliderAction:);
    [pw->view addSubview:s];
}

void pa_cocoa_slider_vert(pa_winhan win, int x, int y, int w, int h,
                          int mark, int id)
{
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
}

void pa_cocoa_progressbar(pa_winhan win, int x, int y, int w, int h, int id)
{
    PAWindow*        pw = (__bridge PAWindow*)win;
    NSProgressIndicator* p =
        [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
    p.style          = NSProgressIndicatorStyleBar;
    p.indeterminate  = NO;
    p.minValue       = 0;
    p.maxValue       = INT_MAX;
    pa_set_tag(p, id);
    [pw->view addSubview:p];
}

void pa_cocoa_kill_widget(pa_winhan win, int id)
{
    NSView* v = find_widget(win, id);
    if (v) [v removeFromSuperview];
}

void pa_cocoa_widget_text(pa_winhan win, int id, const char* s)
{
    NSView* v = find_widget(win, id);
    if (!v) return;
    NSString* ns = [NSString stringWithUTF8String:s];
    if ([v isKindOfClass:[NSControl class]])
        [(NSControl*)v setStringValue:ns];
}

void pa_cocoa_widget_get_text(pa_winhan win, int id, char* s, int sl)
{
    NSView* v = find_widget(win, id);
    if (!v) { if (sl > 0) s[0] = 0; return; }
    if ([v isKindOfClass:[NSControl class]]) {
        const char* cs = [[(NSControl*)v stringValue] UTF8String];
        strncpy(s, cs, sl-1);
        s[sl-1] = 0;
    }
}

void pa_cocoa_widget_enable(pa_winhan win, int id, int on)
{
    NSView* v = find_widget(win, id);
    if (v && [v isKindOfClass:[NSControl class]])
        [(NSControl*)v setEnabled:(on ? YES : NO)];
}

void pa_cocoa_widget_select(pa_winhan win, int id, int on)
{
    NSView* v = find_widget(win, id);
    if (v && [v isKindOfClass:[NSButton class]])
        [(NSButton*)v setState:(on ? NSControlStateValueOn : NSControlStateValueOff)];
}

void pa_cocoa_scrollbar_pos(pa_winhan win, int id, int pos)
{
    NSView* v = find_widget(win, id);
    if (v && [v isKindOfClass:[NSScroller class]])
        [(NSScroller*)v setFloatValue:(float)pos / INT_MAX];
}

void pa_cocoa_scrollbar_siz(pa_winhan win, int id, int range)
{
    /* NSScroller knob proportion — range is 0..INT_MAX */
    NSView* v = find_widget(win, id);
    if (v && [v isKindOfClass:[NSScroller class]])
        [(NSScroller*)v setKnobProportion:(float)range / INT_MAX];
}

void pa_cocoa_progressbar_pos(pa_winhan win, int id, int pos)
{
    NSView* v = find_widget(win, id);
    if (v && [v isKindOfClass:[NSProgressIndicator class]])
        [(NSProgressIndicator*)v setDoubleValue:(double)pos];
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
 * Dialogs
 *----------------------------------------------------------------------------*/

void pa_cocoa_alert(const char* title, const char* message)
{
    NSAlert* a    = [[NSAlert alloc] init];
    a.messageText = [NSString stringWithUTF8String:title   ? title   : ""];
    a.informativeText = [NSString stringWithUTF8String:message ? message : ""];
    [a addButtonWithTitle:@"OK"];
    [a runModal];
}

void pa_cocoa_query_open(char* path, int pathlen)
{
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
}

void pa_cocoa_query_save(char* path, int pathlen)
{
    NSSavePanel* p = [NSSavePanel savePanel];
    if ([p runModal] == NSModalResponseOK && p.URL) {
        const char* u = [p.URL.path UTF8String];
        strncpy(path, u, pathlen-1);
        path[pathlen-1] = 0;
    } else {
        path[0] = 0;
    }
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
