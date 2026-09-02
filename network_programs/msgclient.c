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

/* do/do not secure connection */
ami_long secure = FALSE;

/* use IPv6 or IPv4 */
ami_long ipv6 = FALSE;

ami_optrec opttbl[] = {

    { "secure", &secure, NULL, NULL, NULL },
    { "s",      &secure, NULL, NULL, NULL },
    { "v6",     &ipv6,   NULL, NULL, NULL },
    { NULL,     NULL,    NULL, NULL, NULL }

};

int main(int argc, char **argv)
{

    char buff[BUFLEN];
    ami_ulong addr;
    unsigned long long addrh, addrl;
    int fn;
    int len;
    ami_long argi = 1;
    ami_long argcl = argc;
    int port;

    /* parse user options */
    ami_options(&argi, &argcl, argv, opttbl, TRUE);

    if (argcl != 3) {

        fprintf(stderr, "Usage: msgclient [--secure|-s] [--v6] servername port\n");
        exit(1);

    }

    /* get port number */
    port = atoi(argv[argi+1]);

    /* open the server file */
    if (ipv6) {

        ami_addrnetv6(argv[argi], &addrh, &addrl);
        fn = ami_openmsgv6(addrh, addrl, port, secure);

    } else {

        ami_addrnet(argv[argi], &addr);
        fn = ami_openmsg(addr, port, secure);

    }

    /* send message to server */
    ami_wrmsg(fn, "Hello, server", 13);

    /* receive message from server */
    len = ami_rdmsg(fn, buff, BUFLEN);
    buff[len] = 0; /* terminate */

    printf("The message from server was: %.*s\n", (int)len, buff);

    ami_clsmsg(fn);

    return (0);

}
