/*
 * Socket.c program taken from the sockets example for linux
 * and refactored for IPv6.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>

void error(const char* es)

{

    fprintf(stderr, "Error: %s\n", es);
    exit(1);

}


void doconnect(struct sockaddr* saddr, int fn)

{

    char buff[1024];
    int r;

    /* connect to server */
    r = connect(fn, saddr, sizeof(saddr));
    if (r < 0) error("Connect Failed");

    /* send request */
    sprintf(buff, "GET %s HTTP/1.1\r\n", "/");
    write(fn, buff, strlen(buff));

    sprintf(buff, "Host: %s\r\n\r\n", "www.example.com");
    write(fn, buff, strlen(buff));
    do {

        r = read(fn, buff, sizeof(buff));
        if (r > 0) {

            buff[r] = 0;
            printf("%s", buff);

        }

    } while (r);

}

int main(int argc, char *argv[])
{
    int sockfd = 0, n = 0;
    char buff[1024];
    struct sockaddr_in6 serv_addr;
    int r;
    struct sockaddr* sap;
    struct addrinfo hints, *p;
    int i;

    if(argc != 3)
    {

        printf("Usage: socket6 <server> <page>\n");
        exit(1);

    }

    if((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
        error("Could not create socket");

    /* resolve address */
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    r = getaddrinfo(argv[1], NULL/*"80"*/, &hints, &p);
    if (r != 0) error(gai_strerror(r));

//    memcpy(&serv_addr, (struct sockaddr_in6*)p->ai_addr, sizeof(struct sockaddr_in6));
    freeaddrinfo(p);

    serv_addr.sin6_family = AF_INET6;

    serv_addr.sin6_port = htons(80);

    printf("flow info: %x\n", serv_addr.sin6_flowinfo);
    printf("scope id: %x\n", serv_addr.sin6_scope_id);

    serv_addr.sin6_addr.__in6_u.__u6_addr16[0] = htons(0x2606);
    serv_addr.sin6_addr.__in6_u.__u6_addr16[1] = htons(0x2800);
    serv_addr.sin6_addr.__in6_u.__u6_addr16[2] = htons(0x220);
    serv_addr.sin6_addr.__in6_u.__u6_addr16[3] = htons(0x1);
    serv_addr.sin6_addr.__in6_u.__u6_addr16[4] = htons(0x248);
    serv_addr.sin6_addr.__in6_u.__u6_addr16[5] = htons(0x1893);
    serv_addr.sin6_addr.__in6_u.__u6_addr16[6] = htons(0x25c8);
    serv_addr.sin6_addr.__in6_u.__u6_addr16[7] = htons(0x1946);

    /* Address: 2606:2800:220:1:248:1893:25c8:1946 (www.example.com) */

    printf("Address: ");
    for (i = 0; i < 8; i++) {

        printf("%x", ntohs(serv_addr.sin6_addr.__in6_u.__u6_addr16[i]));
        if (i < 7) printf(":");

    }
    printf("\n");

#if 1
    /* connect to server */
    r = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (r < 0) error("Connect Failed");

    /* send request */
    sprintf(buff, "GET %s HTTP/1.1\r\n", argv[2]);
    write(sockfd, buff, strlen(buff));

    sprintf(buff, "Host: %s\r\n\r\n", "www.example.com" /*argv[1]*/);
    write(sockfd, buff, strlen(buff));
    do {

        r = read(sockfd, buff, sizeof(buff));
        if (r > 0) {

            buff[r] = 0;
            printf("%s", buff);

        }

    } while (r);
#else
    doconnect((struct sockaddr*)&serv_addr, sockfd);
#endif

    return 0;

}
