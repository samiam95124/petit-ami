// @(#)root/x11:$Id$
// Author: O.Couet   17/11/93

#ifndef __ROTATED__
#define __ROTATED__

float   XRotVersion(char*, int);
void    XRotSetMagnification(float);
void    XRotSetBoundingBoxPad(int);
int     XRotDrawString(Display*, XFontStruct*, float,Drawable, GC, int, int, char*);
int     XRotDrawImageString(Display*, XFontStruct*, float,Drawable, GC, int, int, char*);
int     XRotDrawAlignedString(Display*, XFontStruct*, float,Drawable, GC, int, int, char*, int);
int     XRotDrawAlignedImageString(Display*, XFontStruct*, float,Drawable, GC, int, int, char*, int);
XPoint* XRotTextExtents(Display*, XFontStruct*, float,int, int, char*, int);

#endif
