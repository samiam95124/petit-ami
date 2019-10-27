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

int main(int argc, char *argv[])
{
    int sockfd = 0, n = 0;
    char buff[1024];
    struct sockaddr_in6 serv_addr;
    int r;
    struct sockaddr* sap;
    struct addrinfo hints, *p;

    if(argc != 3)
    {

        printf("Usage: socket <server> <page>\n");
        exit(1);

    }

    if((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
        error("Could not create socket");

    /* resolve address */
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_protocol = IPPROTO_TCP;

    r = getaddrinfo(argv[1], "80", &hints, &p);
    if (r != 0) error(gai_strerror(r));

    memcpy(&serv_addr, (struct sockaddr_in6*)p->ai_addr, sizeof(struct sockaddr_in6));
    freeaddrinfo(p);

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

    return 0;

}
