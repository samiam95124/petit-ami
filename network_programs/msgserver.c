/*******************************************************************************

Message server

Accept message file connections and give simple test messages.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <network.h>

#define BUFLEN 250

int main(int argc, char **argv)
{

    char buff[BUFLEN];
    unsigned long addr;
    int fn;
    int len;

    /* open the server file */
    fn = pa_waitmsg(4433, false);

    /* receive message from client */
    len = pa_rdmsg(fn, buff, BUFLEN);
    buff[len] = 0; /* terminate */

    printf("The message from client was: %.*s\n", len, buff);

    /* send message to client */
    pa_wrmsg(fn, "Hello, client again", 13);

    pa_clsmsg(fn);

    return (0);

}
