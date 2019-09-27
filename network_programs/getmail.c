/*******************************************************************************
*                                                                              *
*                    SIMPLE NETWORK ACCESS TEST PROGRAM                        *
*                                                                              *
*                   2006/05/23 Copyright (C) S. A. Franco                      *
*                                                                              *
* Shows an example of network access via netlib. Accesses a mail server, and   *
* orders a list of outstanding mail. This is then dumped to the console.       *
*                                                                              *
* Netlib is a very simple method to access the Internet, because it simply     *
* connects to a server using the standard file methods.                        *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

#define BUFLEN 100

FILE* mail;
unsigned long addr;
char buff[BUFLEN];
char server[BUFLEN];
char user[BUFLEN];
char pass[BUFLEN];
int  msgnum;
int  msgseq;

void waitresp(void)

{

    char t[100];

    fgets(t, BUFLEN, mail); /* get response */
    if (t[0] != '+') {

      fprintf(stderr, "*** Error: protocol error\n");
      exit(1);

    }

}

int main(int argc, char **argv)

{

    printf("Mail server access test program\n");
    printf("\n");

    if (argc < 4) {

        fprintf(stderr, "Usage: getmail <server> <user> <pass>\n");
        exit(1);

    }
    pa_addrnet(argv[1], &addr);
    mail = pa_opennet(addr, 995, true);
    waitresp();
    fprintf(mail, "user %s\r\n", argv[2]);
    waitresp();
    fprintf(mail, "pass %s\r\n", argv[3]);
    waitresp();
    fprintf(mail, "list\r\n");
    waitresp();
    printf("Message Sequence\n");
    printf("----------------\n");
    do {

        if (fgets(buff, BUFLEN, mail)) {

            if (buff[0] != '.') {

                sscanf(buff, "%d %d\n", &msgnum, &msgseq);
                printf("%7d %8d\n", msgnum, msgseq);

            }

        }

   } while (buff[0] && buff[0] != '.');
   fclose(mail);

   return (0);

}
