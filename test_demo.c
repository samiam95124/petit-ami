/*******************************************************************************
*                                                                              *
*                    TEST FOR THE WIDGET DEMONSTRATOR                          *
*                                                                              *
* Build and run:                                                               *
*                                                                              *
*     make test_demo                                                           *
*     ./test_demo                                                              *
*                                                                              *
* A graphics window comes up with the demonstration widget from                *
* portable/widget_demo.c in the middle: the kick button, a button whose        *
* face is a still photo of a soccer player. Press it and the player kicks      *
* the ball, as a frame animation with the kick sound at the moment of          *
* contact. Each press reports a standard button event, counted under the       *
* widget.                                                                      *
*                                                                              *
* What to look at:                                                             *
*                                                                              *
* The widget is a frameless subwindow, so everything a window gets, it         *
* gets: it clips, occludes, and takes mouse events on its own. Hover over      *
* it and the outline lights; click it and the outline shows the focus          *
* color. Resize the window and the widget is killed and recreated at the       *
* new center, exercising the create/kill lifecycle.                            *
*                                                                              *
* Run from the repository root: the widget loads its animation frames and      *
* kick sound from tests/widget_demo/.                                          *
*                                                                              *
*******************************************************************************/

#include <stdio.h>

#include <localdefs.h>
#include <graphics.h>
#include <widget_demo.h>

#define BUTTON_ID 1

static long bx, by, bs; /* button place and size */

/* write the kick count under the widget */
static void count(long kicks)

{

    ami_cursorg(stdout, bx, by+bs+ami_chrsizy(stdout));
    printf("Kicks: %ld   ", kicks);

}

/* place or replace the widget centered in the window, with the banner
   and the count around it */
static void layout(long kicks, long widgeted)

{

    if (widgeted) kickbuttonkill(stdout, BUTTON_ID);
    printf("\f");
    bs = ami_maxyg(stdout)/2; /* a square, half the window height */
    bx = ami_maxxg(stdout)/2-bs/2;
    by = ami_maxyg(stdout)/2-bs/2;
    ami_cursorg(stdout, bx, by-ami_chrsizy(stdout)*2);
    printf("Press the button: the player kicks the ball.");
    kickbutton(stdout, bx, by, bx+bs, by+bs, BUTTON_ID);
    count(kicks);

}

int main(void)

{

    ami_evtrec er;
    long       kicks = 0;

    ami_title(stdout, "Widget demonstrator: the kick button");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_font(stdout, AMI_FONT_SIGN);
    ami_fontsiz(stdout, 24);
    layout(kicks, FALSE);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etbutton && er.butid == BUTTON_ID)
            count(++kicks); /* the widget reports as a stock button does */
        else if (er.etype == ami_etresize)
            /* recenter: kill and recreate the widget at the new middle,
               which also exercises the create/kill lifecycle */
            layout(kicks, TRUE);

    } while (er.etype != ami_etterm);
    kickbuttonkill(stdout, BUTTON_ID);

    return (0);

}
