/*******************************************************************************

Petit Ami test program

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "services.h"

int main(void)

{

    int time;

    time = pa_time();

    printf("Current time (GMT): ");
    pa_writetime(stdout, time);
    printf("\n");

    printf("current date (GMT): ");
    pa_writedate(stdout, time);
    printf("\n");

    printf("Current time (local): ");
    pa_writetime(stdout, pa_local(time));
    printf("\n");

    printf("current date (local): ");
    pa_writedate(stdout, pa_local(time));
    printf("\n");

}
