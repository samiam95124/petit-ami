/*******************************************************************************

Fake email server

Test server for the getmail program. Produces a fixed email message for testing.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

#define BUFLEN 250

char* mailmsg[] = {

    "From: <test@testserver.com>",
    "To: tester@test.com>",
    "Subject: Test success!",
    "",
    "This is a test email message",
    NULL

};

ami_long secure = FALSE;

ami_optrec opttbl[] = {

    { "secure", &secure, NULL, NULL, NULL },
    { "s",      &secure, NULL, NULL, NULL },
    { NULL,     NULL,    NULL, NULL, NULL }

};

int main(int argc, char **argv)
{

    FILE* fp;
    char buff[BUFLEN];
    ami_long port = 4433; /* note: can't use correct 110 port, it is denied */
    ami_long argi = 1;
    ami_long argcl = argc;
    int end;
    int i;

    /* parse user options */
    ami_options(&argi, &argcl, argv, opttbl, TRUE);

    if (argcl != 1) {

        fprintf(stderr, "Usage: fakemail [--secure|-s]\n");
        exit(1);
    }

    while (1) { /* serve this port until cancelled */

        printf("Fakemail server waits on port %lld for connections\n", AMI_LONG_CAST(port));
        fp = ami_waitnet(port, secure);

        printf("Inbound connection\n");

        fprintf(fp, "+OK POP3 Server ready.\n");
        /* run POP server command loop */
        end = FALSE;
        while (!feof(fp) && !end) {

            if (fgets(buff, BUFLEN, fp)) {

                /* we don't do anything with the user and password */
                if (!strncmp(buff, "user",4)) fprintf(fp, "+OK Password required.\n");
                else if (!strncmp(buff, "pass",4)) fprintf(fp, "+OK Mailbox open, 1 message.\n");
                else if (!strncmp(buff, "list",4)) {

                    fprintf(fp, "+OK Mailbox scan listing follows.\n");
                    fprintf(fp, "+OK 1 %d\n", 100);
                    fprintf(fp, ".\r\n");

                } else if (!strncmp(buff, "retr", 4)) {

                    fprintf(fp, "+OK 1 %d octets\n", 100);
                    i = 0;
                    do {

                        fprintf(fp, "%s\n", mailmsg[i]);
                        i++;

                    } while (mailmsg[i]);
                    fprintf(fp, ".\r\n");

                } if (!strncmp(buff, "quit",4)) end = TRUE;


            }

        }

        printf("Connection finished\n");

        /* close the connection and wait for next */
        fclose(fp);

    }

    return (0);

}
