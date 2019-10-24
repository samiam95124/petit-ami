#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>

static void error(const char* s)

{

    fprintf(stderr, "Error: %s\n", s);

    exit(1);

}

int main(int argc, char **argv)

{

	struct addrinfo *p;
	int r;
	unsigned long addr;
	unsigned long long addrl, addrh;
	struct sockaddr_in6* sap;

    if (argc != 2) {

        fprintf(stderr, "Usage: addr <host>\n");
        exit(1);

    }

	r = getaddrinfo(argv[1], NULL, NULL, &p);
	if (r) error(gai_strerror(r));
	printf("Addresses for host: %s\n", argv[1]);
	while (p) {

    	/* traverse the available addresses */
    	printf("Address: type: ");
    	switch (p->ai_family) {

            case AF_UNSPEC:     printf("AF_UNSPEC"); break;
            case AF_LOCAL:      printf("AF_LOCAL"); break;
            case AF_INET:       printf("AF_INET"); break;
            case AF_SNA:        printf("AF_SNA"); break;
            case AF_DECnet:     printf("AF_DECnet"); break;
            case AF_APPLETALK:  printf("AF_APPLETALK"); break;
            case AF_ROUTE:      printf("AF_ROUTE"); break;
            case AF_IPX:        printf("AF_IPX"); break;
            case AF_INET6:      printf("AF_INET6"); break;
            default:            printf("NONE"); break;

    	}
    	printf(" Socket type: ");
    	switch (p->ai_socktype) {

    	  case SOCK_STREAM:    printf("SOCK_STREAM"); break;
          case SOCK_DGRAM:     printf("SOCK_DGRAM"); break;
          case SOCK_RAW:       printf("SOCK_RAW"); break;
          case SOCK_RDM:       printf("SOCK_RDM"); break;
          case SOCK_SEQPACKET: printf("SOCK_SEQPACKET"); break;

    	}
        if (p->ai_family == AF_INET) {

            /* get the IPv4 address */
	        addr = ntohl(((struct sockaddr_in*)(p->ai_addr))->sin_addr.s_addr);
	        printf(" Address: %ld.%ld.%ld.%ld", addr>>24&0xff, addr>>16&0xff,
	               addr>>8&0xff, addr&0xff);

	    } else if (p->ai_family == AF_INET6) {

            /* get the IPv6 address */
            sap = (struct sockaddr_in6*)(p->ai_addr);
            addrh = (unsigned long long) ntohl(sap->sin6_addr.__in6_u.__u6_addr32[0]) << 32 |
                    (unsigned long long) ntohl(sap->sin6_addr.__in6_u.__u6_addr32[1]);
            addrl = (unsigned long long) ntohl(sap->sin6_addr.__in6_u.__u6_addr32[2]) << 32 |
                    (unsigned long long) ntohl(sap->sin6_addr.__in6_u.__u6_addr32[3]);
            printf(" Address: %llx:%llx:%llx:%llx:%llx:%llx:%llx:%llx",
                   addrh>>48&0xffff, addrh>>32&0xffff, addrh>>16&0xffff, addrh&0xffff,
                   addrl>>48&0xffff, addrl>>32&0xffff, addrl>>16&0xffff, addrl&0xffff);

	    }
	    printf("\n");
	    p = p->ai_next;

	}

}
