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
* contact.                                                                     *
*                                                                              *
* What this program tests is the event coming back: the widget reports         *
* with its own user defined event, ETKICKED, fired at the frame where the      *
* foot meets the ball. The main loop here waits on that event (or              *
* terminate), and answers it with nothing more than a printf() into the        *
* window's normal text stream -- each kick adds a line, and the text           *
* wraps and scrolls as any terminal stream does, flowing under the             *
* widget, which occludes it as any window occludes.                            *
*                                                                              *
* Resize the window and the widget is killed and recreated at the new          *
* center, exercising the create/kill lifecycle.                                *
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

/* place or replace the widget centered in the window */
static void place(ami_long widgeted)

{

    ami_long bs; /* button size */
    ami_long bx, by;

    if (widgeted) kickbuttonkill(stdout, BUTTON_ID);
    bs = ami_maxyg(stdout)/2; /* a square, half the window height */
    bx = ami_maxxg(stdout)/2-bs/2;
    by = ami_maxyg(stdout)/2-bs/2;
    kickbutton(stdout, bx, by, bx+bs, by+bs, BUTTON_ID);

}

int main(void)

{

    ami_evtrec er;

    ami_title(stdout, "Widget demonstrator: the kick button");
    printf("Press the button: the player kicks the ball.\n");
    place(FALSE);
    /* wait on the widget's own event, or terminate */
    do {

        ami_event(stdin, &er);
        if (er.etype == ETKICKED && er.butid == BUTTON_ID)
            /* The widget's event, back in the main loop. The answer is
               ordinary stream output: the lines accumulate and scroll
               as in any terminal window. */
            printf("He kicked the ball!\n");
        else if (er.etype == ami_etresize)
            /* recenter: kill and recreate the widget at the new middle,
               which also exercises the create/kill lifecycle */
            place(TRUE);

    } while (er.etype != ami_etterm);
    kickbuttonkill(stdout, BUTTON_ID);

    return (0);

}
