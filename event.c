/******************************************************************************
*                                                                             *
*                            EVENT DIAGNOSTIC                                 *
*                                                                             *
*                            10/02 S. A. Moore                                *
*                                                                             *
* Prints terminal level events.                                               *
*                                                                             *
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "terminal.h"

void main()

{

	pa_evtrec er;

    do {

    	pa_event(stdin, &er);
    	switch (er.etype) {

            case pa_etchar: {

                if (er.echar < ' ') er.echar = '.';
                printf("ANSI character returned '%c'\n", er.echar);
                break;

      	    }
            case pa_etup:      printf("up one line\n"); break;
            case pa_etdown:    printf("down one line\n"); break;
            case pa_etleft:    printf("left one character\n"); break;
            case pa_etright:   printf("right one character\n"); break;
            case pa_etleftw:   printf("left one word\n"); break;
            case pa_etrightw:  printf("right one word\n"); break;
            case pa_ethome:    printf("home of document\n"); break;
            case pa_ethomes:   printf("home of screen\n"); break;
            case pa_ethomel:   printf("home of line\n"); break;
            case pa_etend:     printf("end of document\n"); break;
            case pa_etends:    printf("end of screen\n"); break;
            case pa_etendl:    printf("end of line\n"); break;
            case pa_etscrl:    printf("scroll left one character\n"); break;
            case pa_etscrr:    printf("scroll right one character\n"); break;
            case pa_etscru:    printf("scroll up one line\n"); break;
            case pa_etscrd:    printf("scroll down one line\n"); break;
            case pa_etpagd:    printf("page down\n"); break;
            case pa_etpagu:    printf("page up\n"); break;
            case pa_ettab:     printf("tab\n"); break;
            case pa_etenter:   printf("enter line\n"); break;
            case pa_etinsert:  printf("insert block\n"); break;
            case pa_etinsertl: printf("insert line\n"); break;
            case pa_etinsertt: printf("insert toggle\n"); break;
            case pa_etdel:     printf("delete block\n"); break;
            case pa_etdell:    printf("delete line\n"); break;
            case pa_etdelcf:   printf("delete character forward\n"); break;
            case pa_etdelcb:   printf("delete character backward\n"); break;
            case pa_etcopy:    printf("copy block\n"); break;
            case pa_etcopyl:   printf("copy line\n"); break;
            case pa_etcan:     printf("cancel current operation\n"); break;
            case pa_etstop:    printf("stop current operation\n"); break;
            case pa_etcont:    printf("continue current operation\n"); break;
            case pa_etprint:   printf("print document\n"); break;
            case pa_etprintb:  printf("print block\n"); break;
            case pa_etprints:  printf("print screen\n"); break;
            case pa_etfun:     printf("Function key, number: %d", er.fkey); break;
            case pa_etmenu:    printf("display menu\n"); break;
            case pa_etmouba:   printf("mouse button assertion, mouse: %d button: %d\n",
                                      er.amoun, er.amoubn); break;
            case pa_etmoubd:   printf("mouse button deassertion, mouse: %d button: %d\n",
                                      er.dmoun, er.dmoubn); break;
            case pa_etmoumov:  printf("mouse move, mouse: %d x: %d y: %d\n",
                                      er.mmoun, er.moupx, er.moupy); break;
            case pa_ettim:     printf("timer matures, timer: %d\n", er.timnum); break;
            case pa_etjoyba:   printf("joystick button assertion, stick: %d button: %d", er.ajoyn, er.ajoybn); break;
            case pa_etjoybd:   printf("joystick button deassertion, stick: %d button: %d\n",
                                      er.djoyn, er.djoybn); break;
            case pa_etjoymov:  printf("joystick move, stick: %d x: %d y: %d z: %d",
                                      er.mjoyn, er.joypx, er.joypy, er.joypz); break;
            case pa_etresize:  printf("Window resized\n"); break;
            case pa_etterm:    printf("terminate program\n"); break;

        }

    } while (er.etype != pa_etterm);

}
