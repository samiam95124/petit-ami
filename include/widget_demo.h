/** ****************************************************************************

\file

\brief WIDGET DEMONSTRATOR DEFINITIONS

Entry calls for the widget demonstrator, widget_demo.c: one custom widget,
the kick button, showing how to build your own widgets on the Petit-Ami
drawing and event calls. See portable/widget_demo.c for the full story.

A widget package defines its own entry calls; a user widget kind has no
ami_ vector to override, and needs none.

*******************************************************************************/

#ifndef __WIDGET_DEMO_H__
#define __WIDGET_DEMO_H__

#include <stdio.h>

/* create a kick button over the rectangle, graphical coordinates */
void kickbutton(FILE* f, long x1, long y1, long x2, long y2, long id);
/* remove a kick button by id */
void kickbuttonkill(FILE* f, long id);
/* enable/disable a kick button */
void kickbuttonenable(FILE* f, long id, long e);

#endif /* __WIDGET_DEMO_H__ */
