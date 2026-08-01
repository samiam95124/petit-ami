/** ****************************************************************************

\file

\brief WIDGET DEMONSTRATOR TEST

Exercises the kick button from widget_demo.c, the demonstrator for
building your own widgets on Petit-Ami. A single kick button appears,
wearing a still photo of a soccer player. Press it and the player kicks
the ball, with the kick sound at the moment of contact. The button
reports each press as a standard button event, counted below it.

Run from the repository root, so the demo finds its frames and sound
under tests/widget_demo/.

*******************************************************************************/

#include <stdio.h>

#include <localdefs.h>
#include <graphics.h>
#include <widget_demo.h>

#define BUTTON_ID 1

int main(void)

{

    ami_evtrec er;
    long       kicks = 0;
    long       bs;

    ami_title(stdout, "Widget demonstrator: the kick button");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    printf("\f");
    ami_font(stdout, AMI_FONT_SIGN);
    ami_fontsiz(stdout, 24);
    ami_cursorg(stdout, 40, 20);
    printf("Press the button: the player kicks the ball.");
    /* a square button, sized from the window */
    bs = ami_maxyg(stdout)/2;
    kickbutton(stdout, 40, 80, 40+bs, 80+bs, BUTTON_ID);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etbutton && er.butid == BUTTON_ID) {

            kicks++;
            ami_cursorg(stdout, 40, 100+bs);
            printf("Kicks: %ld   ", kicks);

        }

    } while (er.etype != ami_etterm);
    kickbuttonkill(stdout, BUTTON_ID);

    return (0);

}
