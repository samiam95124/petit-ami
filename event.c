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
#include "pa_terminal.h"

main()

{

	evtrec er;

    do {

    	event(stdin, &er);
    	switch (er.etype) {

            case etchar: {

                if (er.echar < ' ') er.echar = '.';
                printf("ANSI character returned '%c'\n", er.echar);
                break;

      	    }
            case etup:      printf("cursor up one line\n"); break;
            case etdown:    printf("down one line\n"); break;
            case etleft:    printf("left one character\n"); break;
            case etright:   printf("right one character\n"); break;
            case etleftw:   printf("left one word\n"); break;
            case etrightw:  printf("right one word\n"); break;
            case ethome:    printf("home of document\n"); break;
            case ethomes:   printf("home of screen\n"); break;
            case ethomel:   printf("home of line\n"); break;
            case etend:     printf("end of document\n"); break;
            case etends:    printf("end of screen\n"); break;
            case etendl:    printf("end of line\n"); break;
            case etscrl:    printf("scroll left one character\n"); break;
            case etscrr:    printf("scroll right one character\n"); break;
            case etscru:    printf("scroll up one line\n"); break;
            case etscrd:    printf("scroll down one line\n"); break;
            case etpagd:    printf("page down\n"); break;
            case etpagu:    printf("page up\n"); break;
            case ettab:     printf("tab\n"); break;
            case etenter:   printf("enter line\n"); break;
            case etinsert:  printf("insert block\n"); break;
            case etinsertl: printf("insert line\n"); break;
            case etinsertt: printf("insert toggle\n"); break;
            case etdel:     printf("delete block\n"); break;
            case etdell:    printf("delete line\n"); break;
            case etdelcf:   printf("delete character forward\n"); break;
            case etdelcb:   printf("delete character backward\n"); break;
            case etcopy:    printf("copy block\n"); break;
            case etcopyl:   printf("copy line\n"); break;
            case etcan:     printf("cancel current operation\n"); break;
            case etstop:    printf("stop current operation\n"); break;
            case etcont:    printf("continue current operation\n"); break;
            case etprint:   printf("print document\n"); break;
            case etprintb:  printf("print block\n"); break;
            case etprints:  printf("print screen\n"); break;
            case etfun:     printf("Function key, number: %d", er.fkey); break;
            case etmenu:    printf("display menu\n"); break;
            case etmouba:   printf("mouse button assertion, mouse: %d button: %d\n",
                                   er.amoun, er.amoubn); break;
            case etmoubd:   printf("mouse button deassertion, mouse: %d button: %d\n",
                                   er.dmoun, er.dmoubn); break;
            case etmoumov:  printf("mouse move, mouse: %d x: %d y: %d\n",
                                   er.mmoun, er.moupx, er.moupy); break;
            case ettim:     printf("timer matures, timer: %d\n", er.timnum); break;
            case etjoyba:   printf("joystick button assertion, stick: %d button: %d", er.ajoyn, er.ajoybn); break;
            case etjoybd:   printf("joystick button deassertion, stick: %d button: %d\n",
                                   er.djoyn, er.djoybn); break;
            case etjoymov:  printf("joystick move, stick: %d x: %d y: %d z: %d",
                                   er.mjoyn, er.joypx, er.joypy, er.joypz); break;
            case etterm:    printf("terminate program\n"); break;

        }

    } while (er.etype != etterm);

}
