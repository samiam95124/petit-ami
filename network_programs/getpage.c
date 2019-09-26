#include <stdio.h>
#include <stdlib.h>

#include <network.h>

#define BUFLEN 250

int main(int argc, char **argv)
{

    FILE* fp;
    char buff[BUFLEN];
    unsigned long addr;

    pa_addrnet("example.com", &addr);
    fp = pa_opennet(addr, 80, false);

    /* send request to get root page */
    fprintf(fp, "GET / HTTP/1.1\r\n");
    fprintf(fp, "Host: example.com\r\n\r\n");

    /* print result */
    while (!feof(fp)) {

        if (fgets(buff, BUFLEN, fp)) printf("%s", buff);

    }

    return (0);

}
