/*******************************************************************************
*                                                                              *
*                             POP EMAIL READER                                 *
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
#include <string.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

#define BUFLEN 100
#define NUMMSG 1000 /* maximum messages to store */

FILE* mail;
unsigned long addr;
char buff[BUFLEN];
char server[BUFLEN];
char user[BUFLEN];
char pass[BUFLEN];
int  msgnum;
int  msgsiz;
int  msgarr[NUMMSG];

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

    int i, top;

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
    top = 0; /* set top of message array */
    /* gather message numbers */
    do {

        if (fgets(buff, BUFLEN, mail)) {

            if (strcmp(buff, ".\r\n")) {

                sscanf(buff, "%d %d\n", &msgnum, &msgsiz);
                msgarr[top] = msgnum; /* save message number */
                if (top < NUMMSG) top++; /* advance */
                //printf("%7d %8d\n", msgnum, msgsiz);

            }

        }

   } while (buff[0] && strcmp(buff, ".\r\n"));
   /* now print those messages */
   for (i = 0; i < top; i++) {

       printf("Message: %d\n\n", msgarr[i]);
       fprintf(mail, "retr %d\r\n", msgarr[i]);
       waitresp();
       do {

           if (fgets(buff, BUFLEN, mail))
               if (strcmp(buff, ".\r\n")) printf("%s", buff);

       } while (buff[0] && strcmp(buff, ".\r\n"));

   }

   fclose(mail);

   return (0);

}
