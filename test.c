/*******************************************************************************

Petit Ami test program

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "services.h"

int main(void)

{

    int err;

    pa_execw("ls", &err);
    printf("Command returned: %d\n", err);

}
