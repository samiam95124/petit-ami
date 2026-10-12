/* Client side of the screen capture test hook, for a program drawn on a
   graph_server: capture is a display side feature, and the display is the
   server's, so each call is asked of it through the client (see the test
   rig messages of graph_remote.h). */

extern void grx_rcapname(const char* fn);
extern void grx_rcaplabel(const char* s);
extern void grx_rcapture(void);

void screen_capture(void) { grx_rcapture(); }
void screen_capture_name(const char* fn) { if (fn && *fn) grx_rcapname(fn); }
void screen_capture_label(const char* s) { grx_rcaplabel(s); }
