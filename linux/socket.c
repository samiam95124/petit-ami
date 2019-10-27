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

void error(const char* es)

{

    fprintf(stderr, "Error: %s\n", es);
    exit(1);

}

 struct sockaddr* getadr(char* name)

{

	struct addrinfo *p;
	int r;
	int af;
	struct sockaddr* sap;

    af = 0; /* set address not found */
	r = getaddrinfo(name, NULL, NULL, &p);
	if (r) error(gai_strerror(r));
	while (p) {

    	/* traverse the available addresses */
        if (p->ai_family == AF_INET && p->ai_socktype == SOCK_STREAM) {

            /* get the IPv4 address */
	        sap = p->ai_addr;
	        af = 1; /* set an address found */

	    }
	    p = p->ai_next;

	}
	if (!af) error("No address found");

	return (sap);

}

int main(int argc, char *argv[])
{
    int sockfd = 0, n = 0;
    char buff[1024];
    struct sockaddr_in serv_addr;
    int r;
    struct sockaddr* sap;

    if(argc != 3)
    {

        printf("Usage: socket <ip of server> <page>\n");
        exit(1);

    }

    memset(buff, '0',sizeof(buff));

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("Could not create socket");

    sap = getadr(argv[1]);

    memcpy(&serv_addr, sap, sizeof(struct sockaddr));

//    memset(&serv_addr, '0', sizeof(serv_addr));

//    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(80);


//    r = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);
//    if (r <= 0) error("inet_pton error occured");

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("Connect Failed");

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
