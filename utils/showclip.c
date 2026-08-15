/*******************************************************************************
*                                                                              *
*                    SHOWCLIP - Show Clipboard Contents Utility                *
*                                                                              *
* Shows what the X clipboard actually contains: who owns it, what formats it   *
* offers, and the content itself. Text prints; an image reports its format,    *
* size and dimensions. The reason to exist: a screen capture that silently     *
* fails leaves the clipboard holding stale data, and a paste then delivers     *
* the previous capture with nothing to warn you. This answers "did the         *
* capture actually land?" before the paste.                                    *
*                                                                              *
* Usage:                                                                       *
*                                                                              *
* showclip [-p] [-t target] [-o file]                                          *
*                                                                              *
* Options:                                                                     *
*                                                                              *
* -p         Show the PRIMARY selection (the middle-click one) instead of      *
*            the CLIPBOARD selection (the ctrl-c/ctrl-v one).                  *
* -t target  Fetch the named target (format) instead of the default choice.    *
*            The available targets are always listed; pick from that list.     *
* -o file    Write the fetched content to the file, raw. This is the rescue    *
*            path: a capture that will not paste can still be saved.           *
*                                                                              *
* Returns:                                                                     *
*                                                                              *
* 0    The selection had an owner and content was fetched                      *
* 1    The selection is empty (no owner), or the transfer failed               *
*                                                                              *
* Large transfers use the INCR protocol, which is how screenshots move:        *
* a full screen of PNG is bigger than one X transfer, so the owner hands       *
* it over in installments. Without INCR this tool would fail on exactly        *
* the case it exists for.                                                      *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2026 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions are met:  *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright notice,    *
*    this list of conditions and the following disclaimer.                     *
* 2. Redistributions in binary form must reproduce the above copyright notice, *
*    this list of conditions and the following disclaimer in the documentation *
*    and/or other materials provided with the distribution.                    *
* 3. Neither the name of the project nor the names of its contributors may be  *
*    used to endorse or promote products derived from this software without    *
*    specific prior written permission.                                        *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND ANY  *
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED    *
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE       *
* DISCLAIMED. IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY  *
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES   *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; *
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  *
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT   *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF     *
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.            *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define WAITMS 3000 /* selection reply wait bound, in milliseconds */

static Display* dpy;      /* the display */
static Window   win;      /* our requestor window */
static Atom     seln;     /* the selection being shown */
static Atom     prop;     /* the property replies land on */
static Atom     incr;     /* the INCR type */

/*******************************************************************************

Wait for an event of the type on our window

Bounds the wait: a selection owner that never answers must not hang the
tool. Returns true with the event, or false on the bound.

*******************************************************************************/

static int waitevt(int type, XEvent* e)

{

    int ms;

    for (ms = 0; ms < WAITMS; ms++) {

        while (XPending(dpy)) {

            XNextEvent(dpy, e);
            if (e->type == type) return (1);

        }
        usleep(1000);

    }

    return (0);

}

/*******************************************************************************

Fetch a target of the selection

Converts the selection to the target and reads the reply property,
following the INCR protocol when the owner elects it: the first reply
carries only the promise, then the content arrives in installments,
each a property write, ended by an empty one. Returns the bytes, the
caller frees; NULL if the owner cannot supply the target.

*******************************************************************************/

static unsigned char* fetch(Atom target, unsigned long* len)

{

    XEvent         e;
    Atom           type;
    int            fmt;
    unsigned long  n, left;
    unsigned char* part;
    unsigned char* data = NULL;
    unsigned long  total = 0;

    *len = 0;
    XDeleteProperty(dpy, win, prop);
    XConvertSelection(dpy, seln, target, prop, win, CurrentTime);
    XFlush(dpy);
    if (!waitevt(SelectionNotify, &e)) return (NULL);
    if (e.xselection.property == None) return (NULL); /* cannot supply */

    XGetWindowProperty(dpy, win, prop, 0, LONG_MAX/4, True, AnyPropertyType,
                       &type, &fmt, &n, &left, &part);
    if (type != incr) {

        /* the whole answer in one property */
        *len = n*(fmt/8);
        data = malloc(*len+1);
        if (!data) { XFree(part); return (NULL); }
        memcpy(data, part, *len);
        data[*len] = 0;
        XFree(part);

        return (data);

    }
    XFree(part); /* the INCR promise; the delete above starts the flow */

    /* installments arrive as property writes, an empty one ends it */
    for (;;) {

        if (!waitevt(PropertyNotify, &e)) { free(data); return (NULL); }
        if (e.xproperty.state != PropertyNewValue) continue;
        XGetWindowProperty(dpy, win, prop, 0, LONG_MAX/4, True,
                           AnyPropertyType, &type, &fmt, &n, &left, &part);
        if (n == 0) { XFree(part); break; } /* the end marker */
        data = realloc(data, total+n*(fmt/8)+1);
        if (!data) { XFree(part); return (NULL); }
        memcpy(data+total, part, n*(fmt/8));
        total += n*(fmt/8);
        XFree(part);

    }
    if (data) data[total] = 0;
    *len = total;

    return (data);

}

/*******************************************************************************

Describe image data

Recognizes the formats screenshots travel in and reports dimensions from
the header, so "is this the capture I just took?" is answerable at a
glance. Returns false when the data matches no known image, so the
caller can fall back to text.

*******************************************************************************/

static int imginfo(unsigned char* d, unsigned long len)

{

    unsigned long w, h;

    if (len >= 24 && !memcmp(d, "\x89PNG\r\n\x1a\n", 8)) {

        /* PNG: IHDR width/height, big endian */
        w = d[16]<<24 | d[17]<<16 | d[18]<<8 | d[19];
        h = d[20]<<24 | d[21]<<16 | d[22]<<8 | d[23];
        printf("PNG image, %lux%lu pixels, %lu bytes\n", w, h, len);

    } else if (len >= 26 && d[0] == 'B' && d[1] == 'M') {

        /* BMP: info header width/height, little endian */
        w = d[18] | d[19]<<8 | d[20]<<16 | (unsigned long)d[21]<<24;
        h = d[22] | d[23]<<8 | d[24]<<16 | (unsigned long)d[25]<<24;
        printf("BMP image, %lux%lu pixels, %lu bytes\n", w, h, len);

    } else if (len >= 10 && !memcmp(d, "\xff\xd8\xff", 3))
        printf("JPEG image, %lu bytes\n", len);
    else return (0);

    return (1);

}

int main(int argc, char* argv[])

{

    Window         owner;
    Atom           targets, utf8, text;
    Atom           want = None;
    Atom*          tl;
    unsigned char* data;
    unsigned long  len, i, tn;
    char*          nm;
    char*          outfil = NULL;
    char*          tgtnam = NULL;
    int            primary = 0;
    int            istext;
    int            ai;
    FILE*          of;

    for (ai = 1; ai < argc; ai++) {

        if (!strcmp(argv[ai], "-p")) primary = 1;
        else if (!strcmp(argv[ai], "-t") && ai+1 < argc) tgtnam = argv[++ai];
        else if (!strcmp(argv[ai], "-o") && ai+1 < argc) outfil = argv[++ai];
        else {

            fprintf(stderr, "Usage: showclip [-p] [-t target] [-o file]\n");

            return (1);

        }

    }

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "showclip: cannot open display\n"); return (1); }
    seln = primary? XA_PRIMARY: XInternAtom(dpy, "CLIPBOARD", False);
    prop = XInternAtom(dpy, "SHOWCLIP_DATA", False);
    incr = XInternAtom(dpy, "INCR", False);
    targets = XInternAtom(dpy, "TARGETS", False);
    utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    text = XInternAtom(dpy, "TEXT", False);
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
    XSelectInput(dpy, win, PropertyChangeMask);

    owner = XGetSelectionOwner(dpy, seln);
    printf("%s: ", primary? "PRIMARY": "CLIPBOARD");
    if (owner == None) {

        printf("empty (no owner)\n");

        return (1);

    }
    printf("owned by window %lx\n", (unsigned long)owner);

    /* the offered formats */
    data = fetch(targets, &len);
    if (data) {

        int imgsel = 0;

        tl = (Atom*)data;
        tn = len/sizeof(Atom);
        printf("formats: ");
        for (i = 0; i < tn; i++) {

            nm = XGetAtomName(dpy, tl[i]);
            printf("%s%s", i? ", ": "", nm? nm: "?");
            /* choose the content to show: an image first, since a
               capture is the interesting case, then text */
            if (!tgtnam) {

                if (nm && !strncmp(nm, "image/", 6) && !imgsel)
                    { want = tl[i]; imgsel = 1; }
                else if (!imgsel && want == None &&
                         (tl[i] == utf8 || tl[i] == XA_STRING ||
                          tl[i] == text)) want = tl[i];

            }
            if (nm) XFree(nm);

        }
        printf("\n");
        free(data);

    } else printf("formats: (owner did not answer TARGETS)\n");
    if (tgtnam) want = XInternAtom(dpy, tgtnam, False);
    if (want == None) want = utf8; /* no better idea: try text */

    /* the content */
    data = fetch(want, &len);
    if (!data) {

        nm = XGetAtomName(dpy, want);
        printf("content: none for target %s\n", nm? nm: "?");
        if (nm) XFree(nm);

        return (1);

    }
    nm = XGetAtomName(dpy, want);
    istext = want == utf8 || want == XA_STRING || want == text ||
             (nm && !strncmp(nm, "text/", 5));
    printf("content (%s): ", nm? nm: "?");
    if (nm) XFree(nm);
    /* The data speaks before the label: some owners hand an image over
       under a text target, and binary sprayed at a terminal helps
       nobody. Image magic is recognized whatever the target said, and
       text with embedded zeros is reported, not printed. */
    if (imginfo(data, len)) ;
    else if (istext && !memchr(data, 0, len)) {

        printf("%lu characters\n", len);
        fwrite(data, 1, len, stdout);
        if (len && data[len-1] != '\n') printf("\n");

    } else printf("%lu bytes of data\n", len);
    if (outfil) {

        of = fopen(outfil, "wb");
        if (!of) { fprintf(stderr, "showclip: cannot write %s\n", outfil);
                   return (1); }
        fwrite(data, 1, len, of);
        fclose(of);
        printf("written to %s\n", outfil);

    }
    free(data);

    return (0);

}
