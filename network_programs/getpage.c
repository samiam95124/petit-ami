/*******************************************************************************

Get web server page

Retrives the specified page from a web server. Either http or https can be
specified. The resulting page can either be just printed, or piped to a file to
be used.

Note that this version does not automatically clip the header info.

getpage [--secure|-s] <website> <page> [<port>]

If secure is specified, an SSL connection will be created, otherwise plaintext.
the port number defaults to 443 for a https connection, otherwise 80.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

#define BUFLEN 250

boolean secure = false;

optrec opttbl[] = {

    { "secure", &secure, NULL, NULL, NULL },
    { "s",      &secure, NULL, NULL, NULL },
    { NULL,     NULL,    NULL, NULL, NULL }

};

int main(int argc, char **argv)
{

    FILE* fp;
    char buff[BUFLEN];
    unsigned long addr;
    int port;
    int argi = 1;

    /* parse user options */
    options(&argi, &argc, argv, opttbl, true);

    if (argc < 3) {

        fprintf(stderr, "Usage: getpage [--secure|-s] <website> <page> [<port>]\n");
        exit(1);
    }

    /* set default port *
    if (secure) port = 443; /* https standard port */
    else port = 80; /* http standard port */

    /* get user specified port */
    if (argc == 4) port = atoi(argv[3]);

    /* open the server file */
    pa_addrnet(argv[1], &addr);
    fp = pa_opennet(addr, port, secure);

    /* send request to get root page */
    fprintf(fp, "GET %s HTTP/1.1\r\n", argv[2]);
    fprintf(fp, "Host: %s\r\n\r\n", argv[1]);

    /* print result */
    while (!feof(fp)) {

        if (fgets(buff, BUFLEN, fp)) {

            printf("%s", buff);
            /* this flush forces the printout even if we cancel out of the run.
               This is frequently necessary if the server does not time out
               automatically. This only matters if the output is piped. */
            fflush(stdout);

        }

    }

    return (0);

}
