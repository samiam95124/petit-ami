/*******************************************************************************
*                                                                              *
*                              NETWORK TEST                                    *
*                                                                              *
* Automated test for the network library. Replaces the manual procedure in    *
* network_test.txt: each of the manual tests (gettys/telnet, msgserver/       *
* msgclient, prtcertnet, plain and secure) is run automatically against a     *
* loopback server run on a thread of this program, so no external server and  *
* no second console is needed.                                                 *
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
*    non-empty (the prtcertnet test automated against our own server), the    *
*    decoded certificate list (certlistnet) parses with the certificate       *
*    level signature value present, and certlistfree clears the list. The     *
*    server presents a two deep chain (localhost leaf signed by the           *
*    committed "Petit Ami Test CA"), so chain traversal is covered:           *
*    certificate 2 is the CA, with its critical basic constraints flag        *
*    propagated, and past the end of the chain gives empty/NULL results       *
*    without error. The DTLS run covers the message side queries             *
*    (certmsg, certlistmsg) directly.                                         *
* 8. The IPv6 forms of all of the above: addrnetv6, opennetv6, openmsgv6,     *
*    maxmsgv6 and relymsgv6, against the same loopback servers, which bind    *
*    dual stack.                                                              *
*                                                                              *
* Run from the petit-ami/amitk root so the TLS test certificates              *
* (client_tls_cert.pem and friends) are found in the current directory.      *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <localdefs.h>
#include <services.h>
#include <network.h>

#define BUFLEN 250

/* ports for the loopback servers; one per test to avoid reuse collisions */
#define PORT_TCP   42421
#define PORT_TLS   42422
#define PORT_MSG   42423
#define PORT_DTLS  42424
#define PORT_CERT  42425
#define PORT_TCP6  42426
#define PORT_TLS6  42427
#define PORT_MSG6  42428
#define PORT_DTLS6 42429

static int passes = 0;
static int fails  = 0;

static void result(const char* name, int pass)

{

    printf("%-40s %s\n", name, pass ? "pass" : "*** FAIL ***");
    if (pass) passes++; else fails++;

}

/* print a section banner: what is under test */
static void section(const char* title)

{

    printf("\n----- %s -----\n\n", title);

}

/*******************************************************************************

Loopback servers

Each server runs on a thread started with the services thread api: it serves
exactly one client, then flags completion. The client side hands the server
its parameters in the statics below before starting it (the tests run one at
a time, so one set suffices), and synchronizes with it by lock and signal.

*******************************************************************************/

static long lockid;   /* handshake lock */
static long sigid;    /* handshake signal */

static int srvport;   /* port for the server to serve */
static int srvsecure; /* serve secured */
static int clidone;   /* client is finished with the connection */
static int srvdone;   /* server has finished */

/* Let the server come up before connecting to it. waitnet/waitmsg bind,
   listen and accept in a single call, so there is no ready event the server
   could flag before it blocks; a delay is the only handshake available. */
static void waitsrv(void)

{

#ifdef _WIN32
    Sleep(1000);
#else
    sleep(1);
#endif

}

/* start a server thread on the given port */
static void startsrv(void (*srv)(void), int port, int secure)

{

    srvport = port;
    srvsecure = secure;
    clidone = FALSE;
    srvdone = FALSE;
    ami_newthread(srv);
    waitsrv(); /* let the server come up */

}

/* tell the server we are done with the connection */
static void clientdone(void)

{

    ami_lock(lockid);
    clidone = TRUE;
    ami_sendsig(sigid);
    ami_unlock(lockid);

}

/* flag server completion (run on the server thread) */
static void serverdone(void)

{

    ami_lock(lockid);
    srvdone = TRUE;
    ami_sendsig(sigid);
    ami_unlock(lockid);

}

/* finish with a server thread */
static void finish(void)

{

    ami_lock(lockid);
    while (!srvdone) ami_waitsig(lockid, sigid);
    ami_unlock(lockid);

}

/* TCP server: read a line from the client, send a line back */
static void tcpserver(void)

{

    FILE* f;
    char buff[BUFLEN];

    f = ami_waitnet(srvport, srvsecure);
    if (fgets(buff, BUFLEN, f))
        ; /* client line received */
    fputs("Hello, client\n", f);
    fflush(f);
    /* hold the connection up until the client has read and the certificate
       tests have inspected, then close it */
    ami_lock(lockid);
    while (!clidone) ami_waitsig(lockid, sigid);
    ami_unlock(lockid);
    fclose(f);
    serverdone();

}

/* message server: receive one message, send one back */
static void msgserver(void)

{

    long fn;
    long len;
    char buff[BUFLEN];

    fn = ami_waitmsg(srvport, srvsecure);
    len = ami_rdmsg(fn, buff, BUFLEN);
    ami_wrmsg(fn, "Hello, client", 13);
    ami_clsmsg(fn);
    serverdone();

}

/*******************************************************************************

Tests

*******************************************************************************/

/* find a named entry in a certificate list tree, all levels */
static ami_certptr fndnode(ami_certptr list, const char* name)

{

    ami_certptr cp;
    ami_certptr fp;

    for (cp = list; cp; cp = cp->next) {

        if (cp->name && !strcmp(cp->name, name)) return (cp);
        if (cp->fork) {

            fp = fndnode(cp->fork, name);
            if (fp) return (fp);

        }

    }

    return (NULL);

}

/* test 1: name lookup */
static void taddrnet(void)

{

    unsigned long addr;

    addr = 0;
    ami_addrnet("localhost", &addr);
    result("addrnet localhost (hosts file)", addr == 0x7f000001ul);
    printf("    localhost -> %lu.%lu.%lu.%lu\n", addr>>24 & 0xff,
           addr>>16 & 0xff, addr>>8 & 0xff, addr & 0xff);
    addr = 0;
    ami_addrnet("127.0.0.1", &addr);
    result("addrnet numeric literal", addr == 0x7f000001ul);
    printf("    \"127.0.0.1\" -> %lu.%lu.%lu.%lu\n", addr>>24 & 0xff,
           addr>>16 & 0xff, addr>>8 & 0xff, addr & 0xff);

}

/* test 1a: name lookup, IPv6 */
static void taddrnetv6(void)

{

    unsigned long long addrh, addrl;

    addrh = 1;
    addrl = 0;
    ami_addrnetv6("::1", &addrh, &addrl);
    result("addrnetv6 numeric loopback", addrh == 0 && addrl == 1);
    printf("    \"::1\" -> %llx:%llx (high:low 64 bit halves)\n", addrh, addrl);

}

/* Live DNS resolution, v4 and v6, against google.com (chosen as reliable).
   Note on a machine with no name service this errors out, which stops the
   program per the error model. The addresses themselves rotate; some
   non-zero, non-loopback answer is the check. */
static void taddrdns(void)

{

    unsigned long addr;
    unsigned long long addrh, addrl;

    addr = 0;
    ami_addrnet("google.com", &addr);
    result("addrnet live DNS (google.com)",
           addr != 0 && addr != 0x7f000001ul);
    printf("    google.com A    -> %lu.%lu.%lu.%lu\n", addr>>24 & 0xff,
           addr>>16 & 0xff, addr>>8 & 0xff, addr & 0xff);
    addrh = 0;
    addrl = 0;
    ami_addrnetv6("google.com", &addrh, &addrl);
    result("addrnetv6 live DNS (google.com)",
           (addrh != 0 || addrl != 0) && !(addrh == 0 && addrl == 1));
    printf("    google.com AAAA -> %llx:%llx (high:low 64 bit halves)\n",
           addrh, addrl);

}

/* tests 2 and 3: TCP line exchange, clear or secured */
static void ttcp(const char* name, int port, int secure)

{

    unsigned long addr;
    FILE* f;
    char  buff[BUFLEN];
    int   pass;

    pass = FALSE;
    startsrv(tcpserver, port, secure);
    ami_addrnet("localhost", &addr);
    f = ami_opennet(addr, port, secure);
    fputs("Hello, server\n", f);
    fflush(f);
    if (fgets(buff, BUFLEN, f))
        pass = !strcmp(buff, "Hello, client\n");
    fclose(f);
    clientdone();
    finish();
    result(name, pass);
    printf("    sent \"Hello, server\", received \"Hello, client\" over %s\n",
           secure ? "TLS" : "TCP in the clear");

}

/* IPv6 variant of the TCP line exchange, clear or secured */
static void ttcpv6(const char* name, int port, int secure)

{

    unsigned long long addrh, addrl;
    FILE* f;
    char  buff[BUFLEN];
    int   pass;

    pass = FALSE;
    startsrv(tcpserver, port, secure);
    ami_addrnetv6("::1", &addrh, &addrl);
    f = ami_opennetv6(addrh, addrl, port, secure);
    fputs("Hello, server\n", f);
    fflush(f);
    if (fgets(buff, BUFLEN, f))
        pass = !strcmp(buff, "Hello, client\n");
    fclose(f);
    clientdone();
    finish();
    result(name, pass);
    printf("    sent \"Hello, server\", received \"Hello, client\" over %s\n",
           secure ? "TLS on IPv6" : "TCP in the clear on IPv6");

}

/* tests 4 and 5: message exchange, clear or secured. The secured (DTLS) run
   also covers the message side certificate queries: the raw certificate,
   the decoded list, and the list free, against our own DTLS server */
static void tmsg(const char* name, int port, int secure)

{

    unsigned long addr;
    long fn;
    long len;
    char buff[BUFLEN];
    int  pass;
    char cert[10000];
    long r;
    ami_certptr list;
    ami_certptr cp;

    pass = FALSE;
    startsrv(msgserver, port, secure);
    ami_addrnet("localhost", &addr);
    fn = ami_openmsg(addr, port, secure);
    ami_wrmsg(fn, "Hello, server", 13);
    len = ami_rdmsg(fn, buff, BUFLEN);
    if (secure) {

        /* certificate queries on the message connection; the peer
           certificate is cached with the handshake, so these are valid on
           the open message file */
        memset(cert, 0, sizeof(cert));
        r = ami_certmsg(fn, 1, cert, sizeof(cert));
        result("certmsg raw certificate (DTLS)", strlen(cert) > 0);
        printf("    DTLS server certificate PEM: %ld bytes\n", r);
        list = NULL;
        ami_certlistmsg(fn, 1, &list);
        cp = fndnode(list, "Signature Value");
        result("certlistmsg decodes (DTLS)", list && list->name &&
               !strcmp(list->name, "Certificate") &&
               cp && cp->data && cp->data[0]);
        cp = fndnode(list, "Subject");
        printf("    DTLS certificate subject: %s\n",
               cp && cp->data ? cp->data : "<none>");
        ami_certlistfree(&list);
        result("certlistfree clears list (DTLS)", list == NULL);

    }
    if (len == 13) pass = !strncmp(buff, "Hello, client", 13);
    ami_clsmsg(fn);
    finish();
    result(name, pass);
    printf("    sent 13 byte message, received %ld byte reply over %s\n",
           len, secure ? "DTLS" : "UDP in the clear");

}

/* IPv6 variant of the message exchange, clear or secured */
static void tmsgv6(const char* name, int port, int secure)

{

    unsigned long long addrh, addrl;
    long fn;
    long len;
    char buff[BUFLEN];
    int  pass;

    pass = FALSE;
    startsrv(msgserver, port, secure);
    ami_addrnetv6("::1", &addrh, &addrl);
    fn = ami_openmsgv6(addrh, addrl, port, secure);
    ami_wrmsg(fn, "Hello, server", 13);
    len = ami_rdmsg(fn, buff, BUFLEN);
    if (len == 13) pass = !strncmp(buff, "Hello, client", 13);
    ami_clsmsg(fn);
    finish();
    result(name, pass);
    printf("    sent 13 byte message, received %ld byte reply over %s\n",
           len, secure ? "DTLS on IPv6" : "UDP in the clear on IPv6");

}

/* test 6: message limits */
static void tmsglim(void)

{

    unsigned long addr;
    long max;
    long rely;

    ami_addrnet("localhost", &addr);
    max = ami_maxmsg(addr);
    result("maxmsg sane", max > 0);
    printf("    maximum message for loopback: %ld bytes\n", max);
    rely = ami_relymsg(addr);
    result("relymsg sane", rely == 0 || rely == 1);
    printf("    loopback delivery reliable: %s\n", rely ? "yes" : "no");

}

/* IPv6 variant of the message limit tests */
static void tmsglimv6(void)

{

    unsigned long long addrh, addrl;
    long max;
    long rely;

    ami_addrnetv6("::1", &addrh, &addrl);
    max = ami_maxmsgv6(addrh, addrl);
    result("maxmsgv6 sane", max > 0);
    printf("    maximum message for v6 loopback: %ld bytes\n", max);
    rely = ami_relymsgv6(addrh, addrl);
    result("relymsgv6 sane", rely == 0 || rely == 1);
    printf("    v6 loopback delivery reliable: %s\n", rely ? "yes" : "no");

}

/* test 7: certificate of our own secure server */
static void tcert(void)

{

    unsigned long addr;
    FILE* f;
    char  cert[10000];
    char  buff[BUFLEN];
    long  r;
    ami_certptr list;
    ami_certptr cp;
    int   found;

    startsrv(tcpserver, PORT_CERT, TRUE);
    ami_addrnet("localhost", &addr);
    f = ami_opennet(addr, PORT_CERT, TRUE);

    /* raw certificate is retrievable and non-empty */
    memset(cert, 0, sizeof(cert));
    r = ami_certnet(f, 1, cert, sizeof(cert));
    result("certnet raw certificate", strlen(cert) > 0);
    printf("    TLS server certificate PEM: %ld bytes\n", r);

    /* decoded certificate list: the root is "Certificate" with a data fork,
       and the certificate level signature value is present and non-empty */
    list = NULL;
    ami_certlistnet(f, 1, &list);
    result("certlistnet decodes", list && list->name &&
           !strcmp(list->name, "Certificate") && list->fork);
    found = 0;
    if (list && list->fork)
        for (cp = list->fork; cp; cp = cp->next)
            if (cp->name && !strcmp(cp->name, "Signature Value") &&
                cp->data && cp->data[0]) found = 1;
    result("certlist signature value", found);
    cp = fndnode(list, "Subject");
    printf("    certificate 1 subject: %s\n",
           cp && cp->data ? cp->data : "<none>");
    cp = fndnode(list, "Signature Algorithm");
    printf("    certificate 1 signature algorithm: %s\n",
           cp && cp->data ? cp->data : "<none>");
    /* the leaf is issued by our test CA */
    cp = fndnode(list, "Issuer");
    result("certlist leaf issuer is test CA",
           cp && cp->data && strstr(cp->data, "Petit Ami Test CA") != NULL);
    printf("    certificate 1 issuer: %s\n",
           cp && cp->data ? cp->data : "<none>");
    ami_certlistfree(&list);
    result("certlistfree clears list", list == NULL);

    /* the server presents the chain: certificate 2 is the test CA */
    memset(cert, 0, sizeof(cert));
    r = ami_certnet(f, 2, cert, sizeof(cert));
    result("certnet chain CA raw", strlen(cert) > 0);
    list = NULL;
    ami_certlistnet(f, 2, &list);
    cp = fndnode(list, "Subject");
    result("certlistnet chain CA decodes",
           cp && cp->data && strstr(cp->data, "Petit Ami Test CA") != NULL);
    printf("    certificate 2 subject: %s\n",
           cp && cp->data ? cp->data : "<none>");
    /* the CA carries a critical basic constraints CA:TRUE; checks the
       critical flag propagates through the decode */
    cp = fndnode(list, "X509v3 Basic Constraints");
    result("certlist CA basic constraints critical",
           cp && cp->critical && cp->data && strstr(cp->data, "CA:TRUE") != NULL);
    ami_certlistfree(&list);

    /* past the end of the chain: raw form returns zero length, decoded form
       returns an empty list, neither errors */
    memset(cert, 0, sizeof(cert));
    r = ami_certnet(f, 3, cert, sizeof(cert));
    result("certnet past chain end is empty", r == 0 && cert[0] == 0);
    list = NULL;
    ami_certlistnet(f, 3, &list);
    result("certlistnet past chain end is NULL", list == NULL);

    /* complete the exchange so the server exits; the response content is
       not checked here, only consumed */
    fputs("Hello, server\n", f);
    fflush(f);
    if (!fgets(buff, BUFLEN, f)) buff[0] = 0;
    fclose(f);
    clientdone();
    finish();

}

int main(int argc, char **argv)

{

    printf("Network test vs. 0.1\n");
    printf("\n");

    lockid = ami_initlock();
    sigid = ami_initsig();

    section("Name resolution: addrnet (IPv4), addrnetv6 (IPv6); the live "
            "DNS checks need name service, no service stops the test here");
    taddrnet();
    taddrnetv6();
    taddrdns();
    section("Stream connections: opennet/waitnet (IPv4), opennetv6 (IPv6), "
            "clear and TLS");
    ttcp("TCP exchange in the clear", PORT_TCP, FALSE);
    ttcp("TCP exchange secured (TLS)", PORT_TLS, TRUE);
    ttcpv6("TCP exchange in the clear (IPv6)", PORT_TCP6, FALSE);
    ttcpv6("TCP exchange secured (TLS, IPv6)", PORT_TLS6, TRUE);
    section("Messages: openmsg/waitmsg/wrmsg/rdmsg/clsmsg (IPv4), openmsgv6 "
            "(IPv6), clear and DTLS; certmsg/certlistmsg on the DTLS run");
    tmsg("message exchange in the clear", PORT_MSG, FALSE);
    tmsg("message exchange secured (DTLS)", PORT_DTLS, TRUE);
    tmsgv6("message exchange in the clear (IPv6)", PORT_MSG6, FALSE);
    tmsgv6("message exchange secured (DTLS, IPv6)", PORT_DTLS6, TRUE);
    section("Message limits: maxmsg/relymsg (IPv4), maxmsgv6/relymsgv6 (IPv6)");
    tmsglim();
    tmsglimv6();
    section("Certificates: certnet/certlistnet/certlistfree, two deep chain");
    tcert();

    printf("\n");
    printf("Network test complete: %d passes, %d fails\n", passes, fails);

    return (fails != 0);

}
