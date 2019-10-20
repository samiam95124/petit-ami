/*******************************************************************************

Message client

Talks to a message server, giving a simple test message and receiving an answer
message.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

#define BUFLEN 250

boolean secure = false;

optrec opttbl[] = {

    { "secure", &secure, NULL,   NULL, NULL },
    { "s",      &secure, NULL,   NULL, NULL },
    { NULL,     NULL,    NULL,   NULL, NULL }

};

int main(int argc, char **argv)
{

    char buff[BUFLEN];
    unsigned long addr;
    int fn;
    int len;
    int argi = 1;
    int port = 4433;

    /* parse user options */
    options(&argi, &argc, argv, opttbl, true);

    if (argc != 3) {

        fprintf(stderr, "Usage: msglclient [--secure|-s] servername port\n");
        exit(1);

    }

    /* get port number */
    port = atoi(argv[argi+1]);

    /* open the server file */
    pa_addrnet(argv[argi], &addr);
    /* if it is hostname, need to fix address */
    if (addr == 0x7f000101) addr = 0x7f000001;
    fn = pa_openmsg(addr, 4433, secure);

    /* send message to server */
    pa_wrmsg(fn, "Hello, server", 13);

    /* receive message from server */
    len = pa_rdmsg(fn, buff, BUFLEN);
    buff[len] = 0; /* terminate */

    printf("The message from server was: %.*s\n", len, buff);

    pa_clsmsg(fn);

    return (0);

}
