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

           /* each link checks under the key above it, the top under its own */
           printf("Certificate %d (signature %s):\n", i,
                  ami_certvalidnet(fp, i) ? "checks" : "does not check");
           printf("%.*s\n", (int)len, cbuff);
           i++; /* next certificate */

       }

    } while (len);

    return (0);

}
