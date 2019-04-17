/*******************************************************************************

Petit Ami test program

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "services.h"

int main(void)

{

    char fn[100];

    pa_getusr(fn, 100);
    printf("User path: %s\n", fn);

}
