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
int secure = FALSE;

/* use IPv6 or IPv4 */
int ipv6 = FALSE;

pa_optrec opttbl[] = {

    { "secure", &secure, NULL, NULL, NULL },
    { "s",      &secure, NULL, NULL, NULL },
    { "v6",     &ipv6,   NULL, NULL, NULL },
    { NULL,     NULL,    NULL, NULL, NULL }

};

int main(int argc, char **argv)
{

    char buff[BUFLEN];
    unsigned long addr;
    unsigned long long addrh, addrl;
    int fn;
    int len;
    int argi = 1;
    int port;

    /* parse user options */
    pa_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 3) {

        fprintf(stderr, "Usage: msgclient [--secure|-s] [--v6] servername port\n");
        exit(1);

    }

    /* get port number */
    port = atoi(argv[argi+1]);

    /* open the server file */
    if (ipv6) {

        pa_addrnetv6(argv[argi], &addrh, &addrl);
        fn = pa_openmsgv6(addrh, addrl, port, secure);

    } else {

        pa_addrnet(argv[argi], &addr);
        fn = pa_openmsg(addr, port, secure);

    }

    /* send message to server */
    pa_wrmsg(fn, "Hello, server", 13);

    /* receive message from server */
    len = pa_rdmsg(fn, buff, BUFLEN);
    buff[len] = 0; /* terminate */

    printf("The message from server was: %.*s\n", len, buff);

    pa_clsmsg(fn);

    return (0);

}
