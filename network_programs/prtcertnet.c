/*******************************************************************************

Print server certificates

Gets and prints the certificate chain given by the server.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

int main(int argc, char **argv)
{

    FILE* fp;
    ami_ulong addr;
    char cbuff[4096];
    ami_long len;
    ami_long port;
    int i;

    if (argc < 3) {

        fprintf(stderr, "Usage: prtcertnet <server> <port>\n");
        exit(1);
    }

    port = atoi(argv[2]);

    ami_addrnet(argv[1], &addr);
    fp = ami_opennet(addr, port, TRUE);

    i = 1; /* set 1st certificate */
    do {

       len = ami_certnet(fp, i, cbuff, sizeof(cbuff));
       if (len) {

           printf("Certificate %d:\n", i);
           printf("%.*s\n", (int)len, cbuff);
           i++; /* next certificate */

       }

    } while (len);

    return (0);

}
