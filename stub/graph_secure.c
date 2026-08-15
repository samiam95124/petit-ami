/*******************************************************************************

Secure connection selector for remote-linked programs

Linking this stub makes a graph_client program connect securely: DTLS
on the message channels, TLS on the file connections. The client reads
GRAPH_SECURE at its constructor, which runs before main, so the choice
is made the same way, before anything of the program's own can run.

*******************************************************************************/

#include <stdlib.h>

static void graph_secure(void) __attribute__((constructor (104)));
static void graph_secure(void)

{

    setenv("GRAPH_SECURE", "1", 1);

}
