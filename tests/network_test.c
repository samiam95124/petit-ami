/*******************************************************************************
*                                                                              *
*                              NETWORK TEST                                    *
*                                                                              *
* Automated test for the network library. Replaces the manual procedure in    *
* network_test.txt: each of the manual tests (gettys/telnet, msgserver/       *
* msgclient, prtcertnet, plain and secure) is run automatically against a     *
* loopback server forked from this program, so no external server and no      *
* second console is needed.                                                   *
*                                                                              *
* The tests:                                                                  *
*                                                                              *
* 1. addrnet: name lookup of localhost gives the loopback address.            *
* 2. TCP connection in the clear: server waitnet, client opennet, a line      *
*    exchanged in each direction (the gettys/telnet test automated).          *
* 3. TCP connection secured (TLS): the same exchange with secure on.          *
* 4. Messages in the clear: server waitmsg, client openmsg, a message         *
*    exchanged in each direction (the msgserver/msgclient test automated).    *
* 5. Messages secured (DTLS): the same exchange with secure on.               *
* 6. maxmsg/relymsg: sanity of the message limits for the loopback address.   *
* 7. certnet: the raw certificate of the secure server is retrievable and     *
*    non-empty (the prtcertnet test automated against our own server).        *
*                                                                              *
* The decoded certificate list (certlistnet) is not tested: per the note in   *
* network_test.txt, that decode path is not completely working yet.           *
*                                                                              *
* Run from the petit-ami/amitk root so the TLS test certificates              *
* (client_tls_cert.pem and friends) are found in the current directory.      *
*                                                                              *
* The IPV6 forms are not yet covered, matching the note in network_test.txt. *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <localdefs.h>
#include <network.h>

#define BUFLEN 250

/* ports for the loopback servers; one per test to avoid reuse collisions */
#define PORT_TCP   42421
#define PORT_TLS   42422
#define PORT_MSG   42423
#define PORT_DTLS  42424
#define PORT_CERT  42425

static int passes = 0;
static int fails  = 0;

static void result(const char* name, int pass)

{

    printf("%-40s %s\n", name, pass ? "pass" : "*** FAIL ***");
    if (pass) passes++; else fails++;

}

/*******************************************************************************

Loopback servers

Each server runs in a forked child: it serves exactly one client, then exits.
The parent gives the child a moment to come up before connecting.

*******************************************************************************/

/* TCP server: read a line from the client, send a line back */
static int tcpserver(int port, int secure)

{

    int  pid;
    FILE* f;
    char buff[BUFLEN];

    pid = fork();
    if (!pid) { /* child */

        f = ami_waitnet(port, secure);
        if (fgets(buff, BUFLEN, f))
            ; /* client line received */
        fputs("Hello, client\n", f);
        fflush(f);
        /* hold the connection up briefly so the client can read and the
           certificate tests can inspect, then exit (closing it) */
        sleep(2);
        exit(0);

    }
    sleep(1); /* let the server come up */

    return (pid);

}

/* message server: receive one message, send one back */
static int msgserver(int port, int secure)

{

    int  pid;
    int  fn;
    int  len;
    char buff[BUFLEN];

    pid = fork();
    if (!pid) { /* child */

        fn = ami_waitmsg(port, secure);
        len = ami_rdmsg(fn, buff, BUFLEN);
        ami_wrmsg(fn, "Hello, client", 13);
        ami_clsmsg(fn);
        exit(0);

    }
    sleep(1); /* let the server come up */

    return (pid);

}

/* finish with a server child */
static void finish(int pid)

{

    int st;

    waitpid(pid, &st, 0);

}

/*******************************************************************************

Tests

*******************************************************************************/

/* test 1: name lookup */
static void taddrnet(void)

{

    unsigned long addr;

    addr = 0;
    ami_addrnet("localhost", &addr);
    result("addrnet localhost", addr == 0x7f000001ul);

}

/* tests 2 and 3: TCP line exchange, clear or secured */
static void ttcp(const char* name, int port, int secure)

{

    unsigned long addr;
    FILE* f;
    char  buff[BUFLEN];
    int   pid;
    int   pass;

    pass = FALSE;
    pid = tcpserver(port, secure);
    ami_addrnet("localhost", &addr);
    f = ami_opennet(addr, port, secure);
    fputs("Hello, server\n", f);
    fflush(f);
    if (fgets(buff, BUFLEN, f))
        pass = !strcmp(buff, "Hello, client\n");
    fclose(f);
    finish(pid);
    result(name, pass);

}

/* tests 4 and 5: message exchange, clear or secured */
static void tmsg(const char* name, int port, int secure)

{

    unsigned long addr;
    int  fn;
    int  len;
    char buff[BUFLEN];
    int  pid;
    int  pass;

    pass = FALSE;
    pid = msgserver(port, secure);
    ami_addrnet("localhost", &addr);
    fn = ami_openmsg(addr, port, secure);
    ami_wrmsg(fn, "Hello, server", 13);
    len = ami_rdmsg(fn, buff, BUFLEN);
    if (len == 13) pass = !strncmp(buff, "Hello, client", 13);
    ami_clsmsg(fn);
    finish(pid);
    result(name, pass);

}

/* test 6: message limits */
static void tmsglim(void)

{

    unsigned long addr;
    int max;
    int rely;

    ami_addrnet("localhost", &addr);
    max = ami_maxmsg(addr);
    result("maxmsg sane", max > 0);
    rely = ami_relymsg(addr);
    result("relymsg sane", rely == 0 || rely == 1);

}

/* test 7: certificate of our own secure server */
static void tcert(void)

{

    unsigned long addr;
    FILE* f;
    char  cert[10000];
    char  buff[BUFLEN];
    int   pid;
    int   r;

    pid = tcpserver(PORT_CERT, TRUE);
    ami_addrnet("localhost", &addr);
    f = ami_opennet(addr, PORT_CERT, TRUE);

    /* raw certificate is retrievable and non-empty */
    memset(cert, 0, sizeof(cert));
    r = ami_certnet(f, 1, cert, sizeof(cert));
    result("certnet raw certificate", strlen(cert) > 0);

    /* complete the exchange so the server exits */
    fputs("Hello, server\n", f);
    fflush(f);
    if (fgets(buff, BUFLEN, f)) ; /* response */
    fclose(f);
    finish(pid);

}

int main(int argc, char **argv)

{

    printf("Network test vs. 0.1\n");
    printf("\n");

    taddrnet();
    ttcp("TCP exchange in the clear", PORT_TCP, FALSE);
    ttcp("TCP exchange secured (TLS)", PORT_TLS, TRUE);
    tmsg("message exchange in the clear", PORT_MSG, FALSE);
    tmsg("message exchange secured (DTLS)", PORT_DTLS, TRUE);
    tmsglim();
    tcert();

    printf("\n");
    printf("Network test complete: %d passes, %d fails\n", passes, fails);

    return (fails != 0);

}
