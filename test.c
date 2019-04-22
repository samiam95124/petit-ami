/*******************************************************************************

Petit Ami test program

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "services.h"

int main(void)

{

    int t;

    while (1) {

        t = pa_clock();
        while (pa_elapsed(t) < 10000);
        printf("Second\n");

    }

}
