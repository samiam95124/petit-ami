/*******************************************************************************

Secure connection selector for remote-linked programs

Secure channels are the default at both ends now, so nothing needs this
stub to get them; it is kept for a program that must have them whatever
the environment says, since the environment is where GRAPH_PLAIN would
otherwise turn them off. The variable is set from a constructor, which
runs before main, so the choice is made before anything of the program's
own can run.

*******************************************************************************/

#include <stdlib.h>

static void graph_secure(void) __attribute__((constructor (104)));
static void graph_secure(void)

{

    setenv("GRAPH_SECURE", "1", 1);

}
