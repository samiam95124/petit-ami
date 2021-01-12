/*******************************************************************************

                           PRINT CURRENT CONFIGURATION TREE

Parses and prints the current Petit-Ami configuration tree.

*******************************************************************************/

#include <stdio.h>

#include <config.h>

int main()

{

    pa_valptr root;

    printf("Petit-Ami configuration tree:\n");
    printf("\n");

    root = NULL; /* clear root */
    pa_config(&root); /* parse and load the tree */
    pa_prttre(root); /* print resulting tree */

}
