#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "../include/services.h"

#define MAXSTR 100

void main(void)

{

    char s[MAXSTR];

    pa_getenv("bark", s, MAXSTR);

    printf("This is svstst1 \"%s\"\n", s);

}