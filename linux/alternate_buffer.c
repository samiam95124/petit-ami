#include <stdio.h>

void main(void)

{

    int i;
    printf("\033[?1049h\033[H");
    printf("Alternate screen buffer\n");
    getchar();
    printf("\033[?1049l");

}
