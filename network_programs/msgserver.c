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

ami_long secure = FALSE;

ami_optrec opttbl[] = {

    { "secure", &secure, NULL,   NULL, NULL },
    { "s",      &secure, NULL,   NULL, NULL },
    { NULL,     NULL,    NULL,   NULL, NULL }

};

int main(int argc, char **argv)
{

    char buff[BUFLEN];
    ami_ulong addr;
    ami_long fn;
    ami_long len;
    ami_long argi = 1;
    ami_long argcl = argc;
    ami_long port;

    /* parse user options */
    ami_options(&argi, &argcl, argv, opttbl, TRUE);

    if (argcl != 2) {

        fprintf(stderr, "Usage: msgserver [--secure|-s] port\n");
        exit(1);

    }

    /* get port number */
    port = atoi(argv[argi]);

    /* open the server file */
    fn = ami_waitmsg(port, secure);

    /* receive message from client */
    len = ami_rdmsg(fn, buff, BUFLEN);
    buff[len] = 0; /* terminate */

    printf("The message from client was: %.*s\n", (int)len, buff);

    /* send message to client */
    ami_wrmsg(fn, "Hello, client", 13);

    ami_clsmsg(fn);

    return (0);

}
