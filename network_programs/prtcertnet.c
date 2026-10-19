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
    FILE* rf;
    char cbuff[4096];
    char root[4096]; /* the root given, if any */
    int len;
    int rootlen;
    int port;
    int i;

    if (argc < 3 || argc > 4) {

        fprintf(stderr, "Usage: prtcertnet <server> <port> [<root pem file>]\n");
        exit(1);
    }

    port = atoi(argv[2]);

    /* A public server does not send its root, so the last link of its chain
       cannot be checked from the chain alone. With a root file given, each
       certificate is also checked against that root, which the program
       supplies itself */
    rootlen = 0;
    if (argc == 4) {

        rf = fopen(argv[3], "r");
        if (!rf) { perror(argv[3]); exit(1); }
        rootlen = fread(root, 1, sizeof(root), rf);
        fclose(rf);

    }

    fp = ami_opennetname(argv[1], port, TRUE);

    i = 1; /* set 1st certificate */
    do {

       len = ami_certnet(fp, i, cbuff, sizeof(cbuff));
       if (len) {

           /* each link checks under the key above it, the top under its own */
           printf("Certificate %d (signature %s):\n", i,
                  ami_certvalidnet(fp, i) ? "checks" : "does not check");
           printf("%.*s\n", (int)len, cbuff);
           if (rootlen)
               printf("Certificate %d signed by %s: %s\n", i, argv[3],
                      ami_certsigned(cbuff, len, root, rootlen) ? "yes" : "no");
           i++; /* next certificate */

       }

    } while (len);

    return (0);

}
