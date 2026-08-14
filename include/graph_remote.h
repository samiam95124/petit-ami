/*******************************************************************************
*                                                                              *
*                        REMOTE DISPLAY PROTOCOL HEADER                        *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
*                            2026/08/14 S. A. Franco                           *
*                                                                              *
* The wire protocol for remote display mode, shared by the two modules that    *
* implement it:                                                                *
*                                                                              *
*     graph_client  exposes the complete graphics.h API to the program. Each   *
*                   call composes a message and sends it; event() returns the  *
*                   next inbound event. The program runs where the client is.  *
*                                                                              *
*     graph_server  owns the display. It plugs into the standard graphics      *
*                   package, executes inbound call messages against it, and    *
*                   sends the events back out.                                 *
*                                                                              *
* The display, mouse, keyboard and joystick face the user on the server side,  *
* while the program logic runs on the client side, which is why the            *
* arrangement is sometimes considered "upside down" relative to the usual      *
* client/server naming.                                                        *
*                                                                              *
* CHANNELS                                                                     *
*                                                                              *
* The link is two message channels of network.h, on adjacent ports:            *
*                                                                              *
*     command channel (port p):    client -> server call messages, and         *
*                                  server -> client replies to queries.        *
*     event channel   (port p+1):  server -> client events, continuously.      *
*                                                                              *
* The server runs the command channel on its main thread: wait for a message,  *
* execute it against the display library, reply if the call returns values.    *
* A second thread runs the event channel: call event() forever and send each   *
* event out, as soon as it is ready. Events are pushed, never polled: a pull   *
* model would cost a round trip per event. The client mirrors the push with a  *
* receiver thread of its own, which moves inbound events to the client's       *
* queue as they arrive; event() takes from the queue, and the program          *
* regulates its consumption there. The queue is also where the anticipated     *
* optimization of collapsing redundant events belongs. Calls stay on the       *
* program's thread.                                                            *
*                                                                              *
* Queries are synchronous: the client sends the request and waits for the      *
* reply on the command channel before issuing anything else. There is thus     *
* at most one request outstanding, and replies cannot interleave. The seq      *
* field carries a serial number that the reply echoes, as a consistency        *
* check.                                                                       *
*                                                                              *
* WIRE FORMAT                                                                  *
*                                                                              *
* Every message is one message-channel datagram, and must fit the channel      *
* maximum given by maxmsg() for the address; senders chunk the write stream    *
* as needed, and no other message can exceed the maximum with its payload      *
* strings, which bounds practical string lengths. All multibyte values are     *
* little endian. The notation used in the message catalog below:              *
*                                                                              *
*     i64   signed 64 bit integer (the API long)                               *
*     f64   IEEE 754 double (carries the API float values)                     *
*     str   i32 byte length, then the bytes, no terminator                     *
*     blk   i32 byte length, then the bytes                                    *
*     slst  string list: i32 count, then count str                             *
*     menu  menu tree: i32 count, then count entries, each entry               *
*           { i64 flags (bit 0 on/off capable, bit 1 oneof, bit 2 bar),        *
*             i64 id, str face, then a nested menu for the branch }            *
*     evt   event record: 11 i64: window handle, event code, handled, then     *
*           eight parameter slots p0..p7 holding the event's union fields in   *
*           declaration order (a character rides in p0)                        *
*                                                                              *
* Each message begins with the fixed header:                                   *
*                                                                              *
*     i32 len   total message length in bytes, header included                 *
*     i16 mid   message id, from the catalog below                             *
*     i16 seq   request serial; echoed by the reply; 0 where not meaningful    *
*     i64 wid   window handle the call acts on; 0 where not meaningful         *
*                                                                              *
* followed by the payload fields in the order the API call declares them,      *
* window file parameter excluded (it is the header wid).                       *
*                                                                              *
* WINDOW HANDLES                                                               *
*                                                                              *
* The client assigns window handles: handle 1 is the main window (the          *
* standard input/output pair), and openwin() takes the parent handle, the      *
* new handle, and the logical window id. The server maps handles to its own    *
* window files. A window closes with GR_MCLOSEWIN.                             *
*                                                                              *
* SCOPE                                                                        *
*                                                                              *
* The mouse, the keyboard and the joysticks are the server's; their events     *
* flow to the client, and the input device queries are synchronous queries     *
* like any other. eventover(), eventsover() and sendevent() act on the         *
* client's own event stream and put nothing on the wire. Print files           *
* (openprint()) are reserved for a later protocol version.                     *
*                                                                              *
* FILE TRANSFER                                                                *
*                                                                              *
* Files named in calls, the picture files of loadpict(), live where the        *
* program lives, on the client; the server holds a cache, its current          *
* directory. When a call names a file the server looks it up: first the name   *
* as given, then the base name in the cache. If it holds the file the call     *
* proceeds. If not, it requests the file from the client, on the model of      *
* ftp's passive form: the request goes by message inside the pending call's    *
* reply window, naming the file and the server's file port, the command       *
* port + 2. The client opens that port as a stream connection (opennet())     *
* and streams the file in, raw bytes, close delimited; an empty stream is a   *
* file the client does not hold either. The received file is stored in the    *
* cache under its base name, never under a path the client supplied, and the  *
* call then proceeds against the cache and sends its reply.                    *
*                                                                              *
* The passive form is used because the message channels do not disclose the    *
* client's address to the server, while the client always knows the            *
* server's; it is also the form that crosses address translation toward the   *
* server. One transfer runs at a time, which the one outstanding request       *
* rule already guarantees. The mechanism is not tied to pictures: any call     *
* that names a file uses the same exchange.                                    *
*                                                                              *
* RELIABILITY                                                                  *
*                                                                              *
* The protocol requires a reliable channel, in the sense of relymsg():         *
* no loss, no reordering. The local machine connection of the standard test    *
* setup is reliable by definition. Operation over an unreliable network      *
* needs a retransmission layer under this protocol, which is future work.      *
*                                                                              *
*******************************************************************************/

#ifndef __GRAPH_REMOTE_H__
#define __GRAPH_REMOTE_H__

#define GR_VERSION 1    /* protocol version, carried in the hello exchange */
#define GR_DEFPORT 4901 /* default command port; the event channel is +1,
                           the file transfer port +2 */

/* the fixed message header. The types are chosen for the wire widths on the
   supported platforms: int is 32 bits, short 16, long 64, little endian. */
typedef struct gr_msghdr {

    int   len; /* total message length, header included */
    short mid; /* message id */
    short seq; /* request serial, echoed by the reply */
    long  wid; /* window handle, 0 if not meaningful */

} gr_msghdr;

/* the event record as it rides the wire, after the header */
typedef struct gr_msgevt {

    long winid;  /* window handle the event belongs to */
    long etype;  /* event code */
    long handled;/* handled flag */
    long p[8];   /* the union fields, in declaration order */

} gr_msgevt;

/*
 * The message catalog. Each entry gives the payload after the header, and
 * the reply payload where the call returns values; calls without a reply
 * are not acknowledged. Replies use GR_MREPLY with the request's seq.
 */
typedef enum {

    /* --------------------------------------------------------- session */

    /** first on the command channel: i64 version -> i64 version. The
       server refuses a version it does not speak by replying its own and
       closing. */
    GR_MHELLO      = 1,
    /** first on the event channel, from the client, so the server learns
       where events go: i64 version. No reply. */
    GR_MEVOPEN     = 2,
    /** client is done; the server drops the connection. No payload. */
    GR_MBYE        = 3,
    /** server -> client on the event channel: str message. The client
       raises the error to the program. */
    GR_MERROR      = 4,
    /** server -> client on the command channel, inside the reply window
       of a pending call that names a file the server does not hold:
       str name, i64 port, the server's file port. The client opens the
       port as a stream connection and streams the file in, raw bytes,
       close delimited; the pending call then completes and replies. */
    GR_MFILEREQ    = 5,
    /** reserved: the active form of the transfer, where the server
       would connect to a port the client names. The passive form above
       is the form of this protocol version. */
    GR_MFILEPORT   = 6,
    /** the reply to any query: the request's seq, and the payload the
       request's catalog entry gives. */
    GR_MREPLY      = 9,

    /* ---------------------------------------------------- byte stream */

    /** the write stream of the window: blk of characters, as they would
       go to the window file. Chunked by the sender to the channel
       maximum. No reply. */
    GR_MWRITE      = 10,
    /** the window file closes: the server closes its side. No payload. */
    GR_MCLOSEWIN   = 11,

    /* ------------------------------------------------- terminal level */

    GR_MCURSOR     = 20,  /* i64 x, i64 y */
    GR_MMAXX       = 21,  /* -> i64 */
    GR_MMAXY       = 22,  /* -> i64 */
    GR_MHOME       = 23,  /* */
    GR_MDEL        = 24,  /* */
    GR_MUP         = 25,  /* */
    GR_MDOWN       = 26,  /* */
    GR_MLEFT       = 27,  /* */
    GR_MRIGHT      = 28,  /* */
    GR_MBLINK      = 29,  /* i64 e */
    GR_MREVERSE    = 30,  /* i64 e */
    GR_MUNDERLINE  = 31,  /* i64 e */
    GR_MSUPERSCRIPT= 32,  /* i64 e */
    GR_MSUBSCRIPT  = 33,  /* i64 e */
    GR_MITALIC     = 34,  /* i64 e */
    GR_MBOLD       = 35,  /* i64 e */
    GR_MSTRIKEOUT  = 36,  /* i64 e */
    GR_MSTANDOUT   = 37,  /* i64 e */
    GR_MFCOLOR     = 38,  /* i64 color */
    GR_MBCOLOR     = 39,  /* i64 color */
    GR_MAUTO       = 40,  /* i64 e */
    GR_MCURVIS     = 41,  /* i64 e */
    GR_MSCROLL     = 42,  /* i64 x, i64 y */
    GR_MCURX       = 43,  /* -> i64 */
    GR_MCURY       = 44,  /* -> i64 */
    GR_MCURBND     = 45,  /* -> i64 */
    GR_MSELECT     = 46,  /* i64 u, i64 d */
    GR_MTIMER      = 47,  /* i64 i, i64 t, i64 r */
    GR_MKILLTIMER  = 48,  /* i64 i */
    GR_MMOUSE      = 49,  /* -> i64 */
    GR_MMOUSEBUTTON= 50,  /* i64 m -> i64 */
    GR_MJOYSTICK   = 51,  /* -> i64 */
    GR_MJOYBUTTON  = 52,  /* i64 j -> i64 */
    GR_MJOYAXIS    = 53,  /* i64 j -> i64 */
    GR_MSETTAB     = 54,  /* i64 t */
    GR_MRESTAB     = 55,  /* i64 t */
    GR_MCLRTAB     = 56,  /* */
    GR_MFUNKEY     = 57,  /* -> i64 */
    GR_MFRAMETIMER = 58,  /* i64 e */
    GR_MAUTOHOLD   = 59,  /* i64 e; wid 0, the hold is global */
    GR_MWRTSTR     = 60,  /* str */
    GR_MWRTSTRN    = 61,  /* str */
    GR_MSIZBUF     = 62,  /* i64 x, i64 y */
    GR_MTITLE      = 63,  /* str */
    GR_MFCOLORC    = 64,  /* i64 r, i64 g, i64 b */
    GR_MBCOLORC    = 65,  /* i64 r, i64 g, i64 b */

    /* ------------------------------------------------ graphical level */

    GR_MMAXXG      = 100, /* -> i64 */
    GR_MMAXYG      = 101, /* -> i64 */
    GR_MCURXG      = 102, /* -> i64 */
    GR_MCURYG      = 103, /* -> i64 */
    GR_MLINE       = 104, /* i64 x1, y1, x2, y2 */
    GR_MLINEWIDTH  = 105, /* i64 w */
    GR_MLINESTYLE  = 106, /* i64 style */
    GR_MRECT       = 107, /* i64 x1, y1, x2, y2 */
    GR_MFRECT      = 108, /* i64 x1, y1, x2, y2 */
    GR_MRRECT      = 109, /* i64 x1, y1, x2, y2, xs, ys */
    GR_MFRRECT     = 110, /* i64 x1, y1, x2, y2, xs, ys */
    GR_MELLIPSE    = 111, /* i64 x1, y1, x2, y2 */
    GR_MFELLIPSE   = 112, /* i64 x1, y1, x2, y2 */
    GR_MARC        = 113, /* i64 x1, y1, x2, y2, sa, ea */
    GR_MFARC       = 114, /* i64 x1, y1, x2, y2, sa, ea */
    GR_MFCHORD     = 115, /* i64 x1, y1, x2, y2, sa, ea */
    GR_MFTRIANGLE  = 116, /* i64 x1, y1, x2, y2, x3, y3 */
    GR_MCURSORG    = 117, /* i64 x, i64 y */
    GR_MBASELINE   = 118, /* -> i64 */
    GR_MSETPIXEL   = 119, /* i64 x, i64 y */
    GR_MFOVER      = 120, /* */
    GR_MBOVER      = 121, /* */
    GR_MFINVIS     = 122, /* */
    GR_MBINVIS     = 123, /* */
    GR_MFXOR       = 124, /* */
    GR_MBXOR       = 125, /* */
    GR_MFAND       = 126, /* */
    GR_MBAND       = 127, /* */
    GR_MFOR        = 128, /* */
    GR_MBOR        = 129, /* */
    GR_MCHRSIZX    = 130, /* -> i64 */
    GR_MCHRSIZY    = 131, /* -> i64 */
    GR_MFONTS      = 132, /* -> i64 */
    GR_MFONT       = 133, /* i64 fc */
    GR_MFONTNAM    = 134, /* i64 fc -> str */
    GR_MFONTSIZ    = 135, /* i64 s */
    GR_MSETPOINTS  = 136, /* f64 ps */
    GR_MPOINTS     = 137, /* -> f64 */
    GR_MCHRSPCY    = 138, /* i64 s */
    GR_MCHRSPCX    = 139, /* i64 s */
    GR_MDPMX       = 140, /* -> i64 */
    GR_MDPMY       = 141, /* -> i64 */
    GR_MSTRSIZ     = 142, /* str -> i64 */
    GR_MCHRPOS     = 143, /* str, i64 p -> i64 */
    GR_MWRITEJUST  = 144, /* str, i64 n */
    GR_MJUSTPOS    = 145, /* str, i64 p, i64 n -> i64 */
    GR_MCONDENSED  = 146, /* i64 e */
    GR_MEXTENDED   = 147, /* i64 e */
    GR_MXLIGHT     = 148, /* i64 e */
    GR_MLIGHT      = 149, /* i64 e */
    GR_MXBOLD      = 150, /* i64 e */
    GR_MHOLLOW     = 151, /* i64 e */
    GR_MRAISED     = 152, /* i64 e */
    GR_MSETTABG    = 153, /* i64 t */
    GR_MRESTABG    = 154, /* i64 t */
    GR_MFCOLORG    = 155, /* i64 r, i64 g, i64 b */
    GR_MBCOLORG    = 156, /* i64 r, i64 g, i64 b */
    GR_MLOADPICT   = 157, /* i64 p, str name -> i64 0. The server fills
                             its cache by the file transfer exchange if
                             it does not hold the file; the reply comes
                             after the picture is loaded. */
    GR_MPICTSIZX   = 158, /* i64 p -> i64 */
    GR_MPICTSIZY   = 159, /* i64 p -> i64 */
    GR_MPICTURE    = 160, /* i64 p, x1, y1, x2, y2 */
    GR_MDELPICT    = 161, /* i64 p */
    GR_MSCROLLG    = 162, /* i64 x, i64 y */
    GR_MPATH       = 163, /* i64 a */
    GR_MVIEWOFFG   = 164, /* i64 x, i64 y */
    GR_MVIEWSCALE  = 165, /* f64 x, f64 y */
    GR_MSCALEX     = 166, /* i64 x -> i64 */
    GR_MSCALEY     = 167, /* i64 y -> i64 */
    GR_MBLOCKCOPYG = 168, /* i64 s, d, sx1, sy1, sx2, sy2, dx1, dy1, dx2,
                             dy2 */

    /* ----------------------------------------------- window management */

    /** open a window: i64 parent handle (0 for none), i64 new handle,
       i64 logical window id. The header wid is 0. */
    GR_MOPENWIN    = 200,
    GR_MBUFFER     = 201, /* i64 e */
    GR_MSIZBUFG    = 202, /* i64 x, i64 y */
    GR_MGETSIZ     = 203, /* -> i64 x, i64 y */
    GR_MGETSIZG    = 204, /* -> i64 x, i64 y */
    GR_MSETSIZ     = 205, /* i64 x, i64 y */
    GR_MSETSIZG    = 206, /* i64 x, i64 y */
    GR_MSETPOS     = 207, /* i64 x, i64 y */
    GR_MSETPOSG    = 208, /* i64 x, i64 y */
    GR_MDRAGWIN    = 209, /* */
    GR_MSCNSIZ     = 210, /* -> i64 x, i64 y */
    GR_MSCNSIZG    = 211, /* -> i64 x, i64 y */
    GR_MSCNCEN     = 212, /* -> i64 x, i64 y */
    GR_MSCNCENG    = 213, /* -> i64 x, i64 y */
    GR_MWINCLIENT  = 214, /* i64 cx, cy, ms -> i64 wx, wy */
    GR_MWINCLIENTG = 215, /* i64 cx, cy, ms -> i64 wx, wy */
    GR_MFRONT      = 216, /* */
    GR_MBACK       = 217, /* */
    GR_MFRAME      = 218, /* i64 e */
    GR_MSIZABLE    = 219, /* i64 e */
    GR_MSYSBAR     = 220, /* i64 e */
    GR_MMENU       = 221, /* menu; an empty tree removes the menu */
    GR_MMENUENA    = 222, /* i64 id, i64 e */
    GR_MMENUSEL    = 223, /* i64 id, i64 e */
    /** i64 standard menu set, menu the user additions -> menu, the
       combined menu as the server built it, which the client materializes
       for the program */
    GR_MSTDMENU    = 224,
    GR_MGETWINID   = 225, /* -> i64; wid 0, ids are global */
    GR_MFOCUS      = 226, /* */

    /* ----------------------------------------------------- widgets */

    GR_MGETWIGID   = 300, /* -> i64 */
    GR_MKILLWIDGET = 301, /* i64 id */
    GR_MSELECTWIDGET = 302, /* i64 id, i64 e */
    GR_MENABLEWIDGET = 303, /* i64 id, i64 e */
    GR_MGETWIDGETTEXT = 304, /* i64 id -> str */
    GR_MPUTWIDGETTEXT = 305, /* i64 id, str */
    GR_MSIZWIDGET  = 306, /* i64 id, i64 x, i64 y */
    GR_MSIZWIDGETG = 307, /* i64 id, i64 x, i64 y */
    GR_MPOSWIDGET  = 308, /* i64 id, i64 x, i64 y */
    GR_MPOSWIDGETG = 309, /* i64 id, i64 x, i64 y */
    GR_MBACKWIDGET = 310, /* i64 id */
    GR_MFRONTWIDGET= 311, /* i64 id */
    GR_MFOCUSWIDGET= 312, /* i64 id */
    GR_MBUTTONSIZ  = 313, /* str -> i64 w, i64 h */
    GR_MBUTTONSIZG = 314, /* str -> i64 w, i64 h */
    GR_MBUTTON     = 315, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MBUTTONG    = 316, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MCHECKBOXSIZ= 317, /* str -> i64 w, i64 h */
    GR_MCHECKBOXSIZG = 318, /* str -> i64 w, i64 h */
    GR_MCHECKBOX   = 319, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MCHECKBOXG  = 320, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MRADIOBUTTONSIZ = 321, /* str -> i64 w, i64 h */
    GR_MRADIOBUTTONSIZG = 322, /* str -> i64 w, i64 h */
    GR_MRADIOBUTTON = 323, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MRADIOBUTTONG = 324, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MGROUPSIZG  = 325, /* str, i64 cw, i64 ch -> i64 w, h, ox, oy */
    GR_MGROUPSIZ   = 326, /* str, i64 cw, i64 ch -> i64 w, h, ox, oy */
    GR_MGROUP      = 327, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MGROUPG     = 328, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MBACKGROUND = 329, /* i64 x1, y1, x2, y2, i64 id */
    GR_MBACKGROUNDG= 330, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLVERTSIZG = 331, /* -> i64 w, i64 h */
    GR_MSCROLLVERTSIZ = 332, /* -> i64 w, i64 h */
    GR_MSCROLLVERT = 333, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLVERTG= 334, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLHORIZSIZG = 335, /* -> i64 w, i64 h */
    GR_MSCROLLHORIZSIZ = 336, /* -> i64 w, i64 h */
    GR_MSCROLLHORIZ= 337, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLHORIZG = 338, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLPOS  = 339, /* i64 id, i64 r */
    GR_MSCROLLSIZ  = 340, /* i64 id, i64 r */
    GR_MNUMSELBOXSIZG = 341, /* i64 l, i64 u -> i64 w, i64 h */
    GR_MNUMSELBOXSIZ = 342, /* i64 l, i64 u -> i64 w, i64 h */
    GR_MNUMSELBOX  = 343, /* i64 x1, y1, x2, y2, l, u, id */
    GR_MNUMSELBOXG = 344, /* i64 x1, y1, x2, y2, l, u, id */
    GR_MEDITBOXSIZG= 345, /* str -> i64 w, i64 h */
    GR_MEDITBOXSIZ = 346, /* str -> i64 w, i64 h */
    GR_MEDITBOX    = 347, /* i64 x1, y1, x2, y2, i64 id */
    GR_MEDITBOXG   = 348, /* i64 x1, y1, x2, y2, i64 id */
    GR_MPROGBARSIZG= 349, /* -> i64 w, i64 h */
    GR_MPROGBARSIZ = 350, /* -> i64 w, i64 h */
    GR_MPROGBAR    = 351, /* i64 x1, y1, x2, y2, i64 id */
    GR_MPROGBARG   = 352, /* i64 x1, y1, x2, y2, i64 id */
    GR_MPROGBARPOS = 353, /* i64 id, i64 pos */
    GR_MLISTBOXSIZG= 354, /* slst -> i64 w, i64 h */
    GR_MLISTBOXSIZ = 355, /* slst -> i64 w, i64 h */
    GR_MLISTBOX    = 356, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MLISTBOXG   = 357, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MDROPBOXSIZG= 358, /* slst -> i64 cw, ch, ow, oh */
    GR_MDROPBOXSIZ = 359, /* slst -> i64 cw, ch, ow, oh */
    GR_MDROPBOX    = 360, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MDROPBOXG   = 361, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MDROPEDITBOXSIZG = 362, /* slst -> i64 cw, ch, ow, oh */
    GR_MDROPEDITBOXSIZ = 363, /* slst -> i64 cw, ch, ow, oh */
    GR_MDROPEDITBOX = 364, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MDROPEDITBOXG = 365, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MSLIDEHORIZSIZG = 366, /* -> i64 w, i64 h */
    GR_MSLIDEHORIZSIZ = 367, /* -> i64 w, i64 h */
    GR_MSLIDEHORIZ = 368, /* i64 x1, y1, x2, y2, i64 mark, i64 id */
    GR_MSLIDEHORIZG= 369, /* i64 x1, y1, x2, y2, i64 mark, i64 id */
    GR_MSLIDEVERTSIZG = 370, /* -> i64 w, i64 h */
    GR_MSLIDEVERTSIZ = 371, /* -> i64 w, i64 h */
    GR_MSLIDEVERT  = 372, /* i64 x1, y1, x2, y2, i64 mark, i64 id */
    GR_MSLIDEVERTG = 373, /* i64 x1, y1, x2, y2, i64 mark, i64 id */
    GR_MTABBARSIZG = 374, /* i64 tor, i64 cw, i64 ch -> i64 w, h, ox, oy */
    GR_MTABBARSIZ  = 375, /* i64 tor, i64 cw, i64 ch -> i64 w, h, ox, oy */
    GR_MTABBARCLIENTG = 376, /* i64 tor, i64 w, i64 h -> i64 cw, ch, ox, oy */
    GR_MTABBARCLIENT = 377, /* i64 tor, i64 w, i64 h -> i64 cw, ch, ox, oy */
    GR_MTABBAR     = 378, /* i64 x1, y1, x2, y2, slst, i64 tor, i64 id */
    GR_MTABBARG    = 379, /* i64 x1, y1, x2, y2, slst, i64 tor, i64 id */
    GR_MTABSEL     = 380, /* i64 id, i64 tn */

    /* ----------------------------------------------------- dialogs */

    GR_MALERT      = 420, /* str title, str message; wid 0 */
    GR_MQUERYCOLOR = 421, /* i64 r, g, b -> i64 r, g, b; wid 0 */
    GR_MQUERYOPEN  = 422, /* str -> str; wid 0 */
    GR_MQUERYSAVE  = 423, /* str -> str; wid 0 */
    GR_MQUERYFIND  = 424, /* str, i64 opt -> str, i64 opt; wid 0 */
    GR_MQUERYFINDREP = 425, /* str, str, i64 opt -> str, str, i64 opt;
                               wid 0 */
    GR_MQUERYFONT  = 426, /* i64 fc, s, fr, fg, fb, br, bg, bb, effect ->
                             the same nine, as chosen */

    /* ----------------------------------------------------- events */

    /** an event, server -> client on the event channel: evt */
    GR_MEVENT      = 500

} gr_msgcod;

#endif /* __GRAPH_REMOTE_H__ */
