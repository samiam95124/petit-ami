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

    pa_evtrec er;

    pa_home(stdout);

    printf("Terminal surface is: x: %d by y: %d\n", pa_maxx(stdout), pa_maxy(stdout));
    /* place message at middle of screeen */
    pa_cursor(stdout, pa_maxx(stdout)/2-strlen("Hello, Petit Ami")/2, pa_maxy(stdout)/2);
    printf("Hello, Petit Ami");

    /* set periodic timer for 10 seconds */
    pa_timer(stdin, 1, 3*SECOND, 1);
    do {

        pa_event(stdin, &er);
        printf("Event processed\n");
        if (er.etype == pa_ettim) {

            printf("Timer event: Timer: %d\n", er.timnum);

        }

    } while (er.etype != pa_etterm); /* until terminate */

}
