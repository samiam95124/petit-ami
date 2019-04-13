/*******************************************************************************

Petit Ami test program

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "services.h"

int main(void)

{

    pa_filptr fp;

    pa_list("*", &fp); /* get current directory listing */
    printf("Current directory:\n\n");
    while (fp) {

        printf("%s\n", fp->name);
        fp = fp->next;

    }

}
