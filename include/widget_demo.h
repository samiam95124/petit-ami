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
#include <localdefs.h>

#include <stdio.h>
#include <graphics.h>

/* The kick button reports with a user defined event code. The codes from
   ami_etuser up are reserved for programs and packages to define; the
   union fields of the event record are theirs to assign for their own
   codes. This one carries the widget id in er.butid's position, and it
   fires not at the press but at the moment in the animation where the
   foot meets the ball. */
#define ETKICKED (ami_etuser+0) /* he kicked the ball */

/* create a kick button over the rectangle, graphical coordinates */
void kickbutton(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
/* remove a kick button by id */
void kickbuttonkill(FILE* f, ami_long id);
/* enable/disable a kick button */
void kickbuttonenable(FILE* f, ami_long id, ami_long e);

#endif /* __WIDGET_DEMO_H__ */
