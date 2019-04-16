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

    strcpy(fn, "here");
    pa_fulnam(fn, 100);
    printf("Filename: %s\n", fn);

}
