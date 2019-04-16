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

    pa_maknam(fn, 100, "bark/", "sniff", "scratch");
    printf("Filename: %s\n", fn);

}
