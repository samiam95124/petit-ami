/*******************************************************************************

Petit Ami test program

*******************************************************************************/

//#include "stdio.h"
#include <stdio.h>
#include <string.h>
#include "pa_terminal.h"

main()

{

    home(stdout);
    printf("Terminal surface is: x: %d by y: %d\n", maxx(stdout), maxy(stdout));
    /* place message at middle of screeen */
    cursor(stdout, maxx(stdout)/2-strlen("Hello, Petit Ami")/2, maxy(stdout)/2);
    printf("Hello, Petit Ami");

}
