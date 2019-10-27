/*******************************************************************************

Get web server page

Retrives the specified page from a web server. Either http or https can be
specified. The resulting page can either be just printed, or piped to a file to
be used.

Note that this version does not automatically clip the header info.

getpage [--secure|-s] [--v6] <website> <page> [<port>]

If secure is specified, an SSL connection will be created, otherwise plaintext.
the port number defaults to 443 for a https connection, otherwise 80.

Either the IPv4 address or IPv6 address can be selected. Note that the IPv6
address will only work if the server has IPv6 addressing (many servers will do
both).

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

#define BUFLEN 250

/* do/do not secure connection */
boolean secure = false;

/* use IPv6 or IPv4 */
boolean ipv6 = false;

optrec opttbl[] = {

    { "secure", &secure, NULL, NULL, NULL },
    { "s",      &secure, NULL, NULL, NULL },
    { "v6",     &ipv6,   NULL, NULL, NULL },
    { NULL,     NULL,    NULL, NULL, NULL }

};

int main(int argc, char **argv)
{

    FILE* fp;
    char buff[BUFLEN];
    unsigned long addr;
    unsigned long long addrh, addrl;
    int port;
    int argi = 1;

    /* parse user options */
    options(&argi, &argc, argv, opttbl, true);

    if (argc < 3) {

        fprintf(stderr, "Usage: getpage [--secure|-s] <website> <page> [<port>]\n");
        exit(1);
    }

    /* set default port */
    if (secure) port = 443; /* https standard port */
    else port = 80; /* http standard port */

    /* get user specified port */
    if (argc == 4) port = atoi(argv[argi+2]);

    /* open the server file */
    if (ipv6) {

printf("before addrnetv6\n");
        pa_addrnetv6(argv[argi], &addrh, &addrl);
printf("after addrnetv6\n");
printf("Address: %llx:%llx:%llx:%llx:%llx:%llx:%llx:%llx\n",
       addrh>>48&0xffff, addrh>>32&0xffff, addrh>>16&0xffff, addrh&0xffff,
       addrl>>48&0xffff, addrl>>32&0xffff, addrl>>16&0xffff, addrl&0xffff);
printf("Before open\n");
        fp = pa_opennetv6(addrh, addrl, port, secure);
printf("after open\n");

    } else {

        pa_addrnet(argv[argi], &addr);
        fp = pa_opennet(addr, port, secure);

    }

    /* send request to get root page */
    fprintf(fp, "GET %s HTTP/1.1\r\n", argv[argi+1]);
    fprintf(fp, "Host: %s\r\n\r\n", argv[argi]);

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
