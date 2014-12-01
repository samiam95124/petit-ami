/*******************************************************************************

Print keyboard hex equivalents

Used to see what the exact codes being received from the keyboard are.

*******************************************************************************/

#include <stdio.h>

main()

{

    int c;

    printf("Key printer\n");
    while (1) {

        c = getchar();
        printf("Key was: %o\n", c);

    }

}
