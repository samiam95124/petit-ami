/*******************************************************************************

Message server

Accept message file connections and give simple test messages.

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
    int port;

    /* parse user options */
    options(&argi, &argc, argv, opttbl, true);

    if (argc != 2) {

        fprintf(stderr, "Usage: msgserver [--secure|-s] port\n");
        exit(1);

    }

    /* get port number */
    port = atoi(argv[argi]);

    /* open the server file */
    fn = pa_waitmsg(port, secure);

    /* receive message from client */
    len = pa_rdmsg(fn, buff, BUFLEN);
    buff[len] = 0; /* terminate */

    printf("The message from client was: %.*s\n", len, buff);

    /* send message to client */
    pa_wrmsg(fn, "Hello, client", 13);

    pa_clsmsg(fn);

    return (0);

}
