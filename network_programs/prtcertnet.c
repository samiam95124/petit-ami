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
    char cbuff[4096];
    char vmsg[200]; /* verification message */
    int len;
    int port;
    int i;

    if (argc < 3) {

        fprintf(stderr, "Usage: prtcertnet <server> <port>\n");
        exit(1);
    }

    port = atoi(argv[2]);

    fp = ami_opennetname(argv[1], port, TRUE);

    i = 1; /* set 1st certificate */
    do {

       len = ami_certnet(fp, i, cbuff, sizeof(cbuff));
       if (len) {

           printf("Certificate %d:\n", i);
           printf("%.*s\n", (int)len, cbuff);
           i++; /* next certificate */

       }

    } while (len);
    /* say whether the chain verifies against the system's trusted roots and
       names the server asked for */
    if (ami_certverifynet(fp, vmsg, sizeof(vmsg)))
        printf("Not verified: %s\n", vmsg);
    else printf("Verified\n");

    return (0);

}
