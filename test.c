/*******************************************************************************

Petit Ami test program

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "terminal.h"
#include <stdlib.h>

#define SECOND 10000
int main(void)

{

    evtrec er;

    home(stdout);

    printf("Terminal surface is: x: %d by y: %d\n", maxx(stdout), maxy(stdout));
    /* place message at middle of screeen */
    cursor(stdout, maxx(stdout)/2-strlen("Hello, Petit Ami")/2, maxy(stdout)/2);
    printf("Hello, Petit Ami");

    /* set periodic timer for 10 seconds */
    timer(stdin, 1, 3*SECOND, 1);
    do {

        event(stdin, &er);
        printf("Event processed\n");
        if (er.etype == ettim) {

            printf("Timer event: Timer: %d\n", er.timnum);

        }

    } while (er.etype != etterm); /* until terminate */

}
