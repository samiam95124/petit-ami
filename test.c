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
    pa_envptr el;

    el = malloc(sizeof(pa_envrec));
    el->name = malloc(strlen("teststr")+1);
    strcpy(el->name, "teststr");
    el->data = malloc(strlen("hello, world")+1);
    strcpy(el->data, "hello, world");
    pa_execew("printenv", el, &err);
    printf("Command returned: %d\n", err);

}
