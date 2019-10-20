/*******************************************************************************

Message client

Talks to a message server, giving a simple test message and receiving an answer
message.

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
    pa_addrnet(argv[1], &addr);
    /* if it is hostname, need to fix address */
    if (addr == 0x7f000101) addr = 0x7f000001;
    fn = pa_openmsg(addr, 4433, true);

    /* send message to server */
    pa_wrmsg(fn, "Hello, server", 13);

    /* receive message from server */
    len = pa_rdmsg(fn, buff, BUFLEN);
    buff[len] = 0; /* terminate */

    printf("The message from server was: %.*s\n", len, buff);

    pa_clsmsg(fn);

    return (0);

}
