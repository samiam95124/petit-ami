/*******************************************************************************
*                                                                              *
*                           INTERNET ACCESS LIBRARY                            *
*                                                                              *
*                       Copyright (C) 2006 Scott A. Franco                     *
*                                                                              *
*                              5/06 S. A. Franco                               *
*                                                                              *
* Implements access to Internet functions, via tcp/ip. tcp/ip is implemented   *
* via the "file" paradigm. An address and port is used to create a file, then  *
* normal C read and write statements are used to access it.                    *
*                                                                              *
* Windows sockets are not CRT file descriptors, so unlike Linux the socket     *
* cannot itself back the FILE. Instead each connection parks a FILE on the     *
* "nul" device and records the socket in a table indexed by the CRT fd; the    *
* system call interdictions (read/write/close) then divert I/O on that fd to   *
* the socket. Security (TLS/DTLS) is provided by OpenSSL talking directly to   *
* the socket, so no shadow fd is needed as on Linux.                           *
*                                                                              *
* Note that I often use SSL to mean "secure socket attached to standard FILE   *
* structure, which nowadays means TLS. I also use DTLS to refer to message     *
* based sockets using DTLS on UDP.                                             *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* Portions of the code Copyright (C) 2009 - 2012 Robin Seggelmann,             *
* seggelmann@fh-muenster.de, Michael Tuexen, tuexen@fh-muenster.de             *
* 2019 Felix Weinrank, weinrank@fh-muenster.de, The OpenSSL Project Authors.   *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
* ============================================================================ *
*                                                                              *
* For the portions of the code covered by The OpenSSL License:                 *
*                                                                              *
* Licensed under the Apache License 2.0 (the "License").  You may not use      *
* this file except in compliance with the License.  You can obtain a copy      *
* in the file LICENSE in the source distribution or at                         *
* https://www.openssl.org/source/license.html                                  *
*                                                                              *
*******************************************************************************/

#define WINVER 0x0A00
#define _WIN32_WINNT 0xA00

/* Windows/mingw definitions */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
/* winsock2 must precede windows.h */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

/* OpenSSL */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/x509v3.h>

/* OpenSSL 3.0 renamed SSL_get_peer_certificate to SSL_get1_peer_certificate.
   The old name survives only as a deprecated alias, which mingw's OpenSSL 3.x
   build compiles out, so map it to the current name. Both return a reference
   the caller must X509_free. */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#ifndef SSL_get_peer_certificate
#define SSL_get_peer_certificate SSL_get1_peer_certificate
#endif
#endif

/* Petit-Ami definitions */
#include <localdefs.h>
#include <network.h>

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); } while (0)

#define MAXFIL 1000 /* maximum number of open files */
#define COOKIE_SECRET_LENGTH 16 /* length of secret cookie */

/* socket structures */
typedef union {

    struct sockaddr_storage ss;
    struct sockaddr_in6 s6;
    struct sockaddr_in s4;

} socket_struct;

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int     (*popen_t)(const char*, int, int);
typedef int     (*pclose_t)(int);
typedef off_t   (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

/* File tracking.
   Windows sockets cannot back a CRT fd, so each network connection parks a
   FILE (or bare fd for message sockets) on the "nul" device and the actual
   socket lives here, indexed by that fd. The interdicted read/write/close
   calls then divert to the socket. SSL and DTLS state also live here. */
typedef struct filrec {

    int           net;    /* it's a network file */
    int           sec;    /* it's a secure sockets (TLS) file */
    int           msg;    /* is a message socket (udp/dtls) */
    int           sudp;   /* is a secure udp (DTLS) */
    int           v6addr; /* is an IPv6 address */
    SOCKET        sock;   /* handle to network socket */
    SOCKET        sock2;  /* second socket (DTLS server connected side) */
    socket_struct saddr;  /* socket address structure */
    SSL*          ssl;    /* SSL data */
    X509*         cert;   /* peer certificate */
    BIO*          bio;    /* bio for DTLS */

} filrec, *filptr;

/* error codes */
typedef enum {

    ewskini,  /* cannot initialize winsock */
    einvhan,  /* invalid file handle */
    enetopn,  /* cannot open network file */
    enetpos,  /* cannot position network file */
    enetloc,  /* cannot find location network file */
    enetlen,  /* cannot find length network file */
    esckeof,  /* End encountered on socket */
    efinuse,  /* file already in use */
    enetwrt,  /* Attempt to write to input side of network pair */
    enetadr,  /* cannot determine address of server */
    einissl,  /* Cannot initialize OpenSSL library */
    esslnew,  /* Cannot create SSL */
    esslctx,  /* Cannot create SSL context */
    esslfid,  /* Cannot connect SSL to file id */
    esslses,  /* Cannot create SSL session */
    esslcer,  /* Cannot get SSL certificate */
    enodupf,  /* Cannot create duplicate fid */
    enoallc,  /* Cannot allocate file entry */
    enolcert, /* Cannot load certificate */
    enolpkey, /* Cannot load private key */
    enotmsg,  /* not a message file id */
    eismsg,   /* is a message file id */
    enotsec,  /* Not a secured file */
    enocert,  /* Cannot retrieve certificate */
    enobio,   /* Cannot create BIO in OpenSSL */
    ewrbio,   /* Cannot write to BIO in OpenSSL */
    erdbio,   /* Cannot read from BIO in OpenSSL */
    ecerttl,  /* PEM format certificate too large for buffer */
    einvctn,  /* invalid certificate number */
    enomem,   /* Out of memory */
    enoipv4,  /* Cannot find IPV4 address */
    enotimp,  /* function not implemented */
    esystem   /* System consistency check */

} errcod;

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overridden by this module.
 *
 */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static plseek_t  ofplseek;

static filptr opnfil[MAXFIL];      /* open files table */
static CRITICAL_SECTION netlck;    /* lock for files table and lazy contexts */

/* The double fault flag is set when exiting, so if we exit again, it
  is checked, then forces an immediate exit. This keeps faults from
  looping. */
static int dblflt; /* double fault flag */

/*
 * openSSL variables
 */

/* contexts */
static SSL_CTX* client_tls_ctx;
static SSL_CTX* client_dtls_ctx;
static SSL_CTX* server_tls_ctx;
static SSL_CTX* server_dtls_ctx;

/* Each SSL context loads certificate and key .pem files. Those files need not
   exist for a program that performs no secure networking, so the contexts are
   created lazily, on the first secure use of each, rather than in the startup
   constructor. Otherwise every program linking this library would have to carry
   the .pem files (and would abort at startup without them). The netlck guards
   make the lazy creation safe under the multithreaded server. */

/* server secret cookie */
static unsigned char cookie_secret[COOKIE_SECRET_LENGTH];
/* cookie has been initialized */
static int cookie_initialized;

/*******************************************************************************

Process network library error

Outputs an error message using the special syslib function, then halts.

*******************************************************************************/

static void netwrterr(const char* s)

{

    fprintf(stderr, "\nError: Network: %s\n", s);

    exit(1);

}

/*******************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.
This needs to go to a dialog instead of the system error trap.

*******************************************************************************/

static void error(errcod e)

{

    switch (e) { /* error */

        case ewskini:  netwrterr("Cannot initialize winsock"); break;
        case einvhan:  netwrterr("Invalid file number"); break;
        case enetopn:  netwrterr("Cannot reset or rewrite network file");
                       break;
        case enetpos:  netwrterr("Cannot position network file"); break;
        case enetloc:  netwrterr("Cannot find location network file"); break;
        case enetlen:  netwrterr("Cannot find length network file"); break;
        case esckeof:  netwrterr("End encountered on socket"); break;
        case efinuse:  netwrterr("File already in use"); break;
        case enetwrt:
            netwrterr("Attempt to write to input side of network pair"); break;
        case enetadr:  netwrterr("Cannot determine address of server"); break;
        case einissl:  netwrterr("Cannot initialize OpenSSL library"); break;
        case esslnew:  netwrterr("Cannot create SSL"); break;
        case esslctx:  netwrterr("Cannot create SSL context"); break;
        case esslfid:  netwrterr("Cannot connect SSL to file id"); break;
        case esslses:  netwrterr("Cannot create SSL session"); break;
        case esslcer:  netwrterr("Cannot get SSL certificate"); break;
        case enodupf:  netwrterr("Cannot create duplicate fid"); break;
        case enoallc:  netwrterr("Cannot allocate file entry"); break;
        case enolcert: netwrterr("Cannot load certificate"); break;
        case enolpkey: netwrterr("Cannot load private key"); break;
        case enotmsg:  netwrterr("Not a message file id"); break;
        case eismsg:   netwrterr("Is a message file id"); break;
        case enotsec:  netwrterr("Not a secured file"); break;
        case enocert:  netwrterr("Cannot retrieve certificate"); break;
        case enobio:   netwrterr("Cannot create BIO in OpenSSL"); break;
        case ewrbio:   netwrterr("Cannot write to BIO in OpenSSL"); break;
        case erdbio:   netwrterr("Cannot read from BIO in OpenSSL"); break;
        case ecerttl:  netwrterr("PEM format certificate too large for buffer");
                       break;
        case einvctn:  netwrterr("Invalid certificate number"); break;
        case enomem:   netwrterr("Out of memory"); break;
        case enoipv4:  netwrterr("Cannot find IPV4 address"); break;
        case enotimp:  netwrterr("Function not implemented"); break;
        case esystem:
            netwrterr("System consistency check, please contact vendor"); break;

    }

}

/*******************************************************************************

Print dotted address

Prints an address in the form 1.2.3.4. A diagnostic.

*******************************************************************************/

static void prtadr(unsigned long addr)

{

    fprintf(stderr, "%d.%d.%d.%d", (int)(addr >> 24 & 0xff),
            (int)(addr >> 16 & 0xff), (int)(addr >> 8 & 0xff),
            (int)(addr & 0xff));

}

/*******************************************************************************

Handle Winsock error

Only called if the last error variable is set. The text string for the error
is output, and then the program halted.

*******************************************************************************/

static void wskerr(void)
{

    LPVOID lpMsgBuf;

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, WSAGetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf,
                  0, NULL);
    fprintf(stderr, "\n*** Windows error: %s\n", (char*)lpMsgBuf);

    exit(1);

}

/*******************************************************************************

Handle SSL error queue

Dumps the SSL error queue.

*******************************************************************************/

static int sslerrcb(const char *str, size_t len, void *u)

{

    (void)u;
    fprintf(stderr, "%.*s", (int)len, str);
    return 1;

}

static void sslerrorqueue(void)

{

    ERR_print_errors_cb(sslerrcb, NULL);
    exit(1);

}

/*******************************************************************************

Handle SSL error

Handles an SSL layer error by looking up the error code and printing the
appropriate error message.

*******************************************************************************/

static void sslerror(SSL* ssl, int r)

{

    fprintf(stderr, "\nSSL Error: ");
    switch (SSL_get_error(ssl, r)) {

        case SSL_ERROR_NONE:
            fprintf(stderr, "The TLS/SSL I/O operation completed\n");
            break;
        case SSL_ERROR_ZERO_RETURN:
            fprintf(stderr, "The TLS/SSL connection has been closed\n");
            break;
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
        case SSL_ERROR_WANT_CONNECT:
        case SSL_ERROR_WANT_ACCEPT:
            fprintf(stderr, "The operation did not complete\n");
            break;
        case SSL_ERROR_WANT_X509_LOOKUP:
            fprintf(stderr, "The operation did not complete because an application "
                      "callback set by SSL_CTX_set_client_cert_cb() has asked "
                      "to be called again\n");
            break;
        case SSL_ERROR_SYSCALL:
            fprintf(stderr, "System I/O error\n");
            break;
        case SSL_ERROR_SSL:
            fprintf(stderr, "A failure in the SSL library occurred\n");
            sslerrorqueue();
            break;
        default:
            fprintf(stderr, "Unknown error code\n");
            break;

    }

    exit(1);

}

/*******************************************************************************

Get file entry

Gets a file entry by allocation. Clears the fields in the file structure.

*******************************************************************************/

static filptr getfil(void)

{

    filptr fp;

    /* get entry */
    fp = malloc(sizeof(filrec));
    if (!fp) error(enomem); /* didn't work */
    fp->net = FALSE;    /* set not network file */
    fp->sec = FALSE;    /* set ordinary socket */
    fp->msg = FALSE;    /* set not message port */
    fp->sudp = FALSE;   /* set not secure udp */
    fp->v6addr = FALSE; /* set v4 address */
    fp->sock = INVALID_SOCKET;  /* clear socket */
    fp->sock2 = INVALID_SOCKET; /* clear second socket */
    fp->ssl = NULL;     /* clear SSL data */
    fp->cert = NULL;    /* clear certificate data */
    fp->bio = NULL;     /* clear BIO data */

    return (fp);

}

/*******************************************************************************

Get new file entry

Checks the indicated file table entry, and allocates a new one if none is
allocated. Then the file entry is initialized.

*******************************************************************************/

static void newfil(int fn)

{

    filptr fp;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    EnterCriticalSection(&netlck); /* take the table lock */
    /* See if the file entry is undefined, then get a file entry if not */
    if (!opnfil[fn]) opnfil[fn] = getfil();
    else {

        fp = opnfil[fn]; /* index that */
        fp->net = FALSE;    /* set unoccupied */
        fp->sec = FALSE;    /* set ordinary socket */
        fp->msg = FALSE;    /* set not message port */
        fp->sudp = FALSE;   /* set not secure udp */
        fp->v6addr = FALSE; /* set v4 address */
        fp->sock = INVALID_SOCKET;  /* clear socket */
        fp->sock2 = INVALID_SOCKET; /* clear second socket */
        fp->ssl = NULL;     /* clear SSL data */
        fp->cert = NULL;    /* clear certificate data */
        fp->bio = NULL;     /* clear BIO data */

    }
    LeaveCriticalSection(&netlck); /* release the table lock */

}

/*******************************************************************************

Make file entry

Checks the indicated file table entry, and allocates a new one if none is
allocated. Unlike newfil, an existing entry is left as it stands.

*******************************************************************************/

static void makfil(int fn)

{

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    EnterCriticalSection(&netlck); /* take the table lock */
    if (!opnfil[fn]) opnfil[fn] = getfil();
    LeaveCriticalSection(&netlck); /* release the table lock */

}

/*******************************************************************************

Generate DTLS cookie

Generates a cookie for DTLS communications. A cookie is generated for the given
ssl socket, and placed into the given buffer. The length of the cookie is also
set.

This code comes from:

https://github.com/nplab/DTLS-Examples

*******************************************************************************/

static int generate_cookie(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len)

{

    unsigned char *buffer, result[EVP_MAX_MD_SIZE];
    unsigned int length = 0, resultlength;
    socket_struct peer;

    /* Initialize a random secret */
    if (!cookie_initialized)
        {
        if (!RAND_bytes(cookie_secret, COOKIE_SECRET_LENGTH))
            {
            printf("error setting random cookie secret\n");
            return 0;
            }
        cookie_initialized = TRUE;
        }

    /* Read peer information */
    (void) BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

    /* Create buffer with peer's address and port */
    length = 0;
    switch (peer.ss.ss_family) {
        case AF_INET:
            length += sizeof(struct in_addr);
            break;
        case AF_INET6:
            length += sizeof(struct in6_addr);
            break;
        default:
            OPENSSL_assert(0);
            break;
    }
    length += sizeof(unsigned short);
    buffer = (unsigned char*) OPENSSL_malloc(length);

    if (buffer == NULL)
        {
        printf("out of memory\n");
        return 0;
        }

    switch (peer.ss.ss_family) {
        case AF_INET:
            memcpy(buffer,
                   &peer.s4.sin_port,
                   sizeof(unsigned short));
            memcpy(buffer + sizeof(peer.s4.sin_port),
                   &peer.s4.sin_addr,
                   sizeof(struct in_addr));
            break;
        case AF_INET6:
            memcpy(buffer,
                   &peer.s6.sin6_port,
                   sizeof(unsigned short));
            memcpy(buffer + sizeof(unsigned short),
                   &peer.s6.sin6_addr,
                   sizeof(struct in6_addr));
            break;
        default:
            OPENSSL_assert(0);
            break;
    }

    /* Calculate HMAC of buffer using the secret */
    HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
         (const unsigned char*) buffer, length, result, &resultlength);
    OPENSSL_free(buffer);

    memcpy(cookie, result, resultlength);
    *cookie_len = resultlength;

    return 1;

}

/*******************************************************************************

Verify DTLS cookie

Verifies a cookie for DTLS communications. A cookie is checked for the given
ssl socket. The cookie string is expected in a buffer, and the length of the
string is also given.

This code comes from:

https://github.com/nplab/DTLS-Examples

*******************************************************************************/

static int verify_cookie(SSL *ssl, const unsigned char *cookie, unsigned int cookie_len)

{

    unsigned char *buffer, result[EVP_MAX_MD_SIZE];
    unsigned int length = 0, resultlength;
    socket_struct peer;

    /* If secret isn't initialized yet, the cookie can't be valid */
    if (!cookie_initialized)
        return 0;

    /* Read peer information */
    (void) BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

    /* Create buffer with peer's address and port */
    length = 0;
    switch (peer.ss.ss_family) {
        case AF_INET:
            length += sizeof(struct in_addr);
            break;
        case AF_INET6:
            length += sizeof(struct in6_addr);
            break;
        default:
            OPENSSL_assert(0);
            break;
    }
    length += sizeof(unsigned short);
    buffer = (unsigned char*) OPENSSL_malloc(length);

    if (buffer == NULL)
        {
        printf("out of memory\n");
        return 0;
        }

    switch (peer.ss.ss_family) {
        case AF_INET:
            memcpy(buffer,
                   &peer.s4.sin_port,
                   sizeof(unsigned short));
            memcpy(buffer + sizeof(unsigned short),
                   &peer.s4.sin_addr,
                   sizeof(struct in_addr));
            break;
        case AF_INET6:
            memcpy(buffer,
                   &peer.s6.sin6_port,
                   sizeof(unsigned short));
            memcpy(buffer + sizeof(unsigned short),
                   &peer.s6.sin6_addr,
                   sizeof(struct in6_addr));
            break;
        default:
            OPENSSL_assert(0);
            break;
    }

    /* Calculate HMAC of buffer using the secret */
    HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
         (const unsigned char*) buffer, length, result, &resultlength);
    OPENSSL_free(buffer);

    if (cookie_len == resultlength && memcmp(result, cookie, resultlength) == 0)
        return 1;

    return 0;

}

/*******************************************************************************

Verify certificate

Currently unused. Returns verified always.

*******************************************************************************/

static int dtls_verify_callback (int ok, X509_STORE_CTX *ctx)

{

    return 1;

}

/*******************************************************************************

Initialize SSL context

Note the .pem files are read with our own stdio and passed to OpenSSL as
memory BIOs. The prebuilt OpenSSL library was compiled against the Microsoft
C runtime FILE, which is not compatible with the petit-ami stdio this library
links, so OpenSSL's own file loading calls (SSL_CTX_use_certificate_file and
friends) cannot be used.

*******************************************************************************/

/* read a file into a memory BIO */

static BIO* filebio(const char* fn)

{

    FILE*  fp;
    BIO*   bp;
    char   buff[4096];
    size_t n;

    fp = fopen(fn, "rb");
    if (!fp) {

        fprintf(stderr, "\nError: Network: Cannot open certificate/key file: "
                        "%s\n", fn);
        exit(1);

    }
    bp = BIO_new(BIO_s_mem());
    if (!bp) error(enobio);
    while ((n = fread(buff, 1, sizeof(buff), fp)) > 0)
        BIO_write(bp, buff, (int)n);
    fclose(fp);

    return (bp);

}

static void initctx(
    /* context */ SSL_CTX** ctx,
    /* communications method */ const SSL_METHOD *method,
    /* certificate file */ string cert,
    /* key file */ string key
)

{

    int r;
    BIO* bp;
    X509* certx;
    EVP_PKEY* pkey;

    /* create new SSL context */
    *ctx = SSL_CTX_new(method);
    if (!*ctx) error(esslctx);

    /* Set the key and cert */
    bp = filebio(cert);
    certx = PEM_read_bio_X509(bp, NULL, NULL, NULL);
    if (!certx) sslerrorqueue();
    BIO_free(bp);
    r = SSL_CTX_use_certificate(*ctx, certx);
    X509_free(certx); /* the context holds its own reference */
    if (r <= 0) sslerrorqueue();

    bp = filebio(key);
    pkey = PEM_read_bio_PrivateKey(bp, NULL, NULL, NULL);
    if (!pkey) sslerrorqueue();
    BIO_free(bp);
    r = SSL_CTX_use_PrivateKey(*ctx, pkey);
    EVP_PKEY_free(pkey); /* the context holds its own reference */
    if (r <= 0) sslerrorqueue();

    r = SSL_CTX_check_private_key(*ctx);
    if (r != 1) sslerrorqueue();

}

/*******************************************************************************

Lazy SSL context creation

These create each SSL context on first secure use (guarded by the netlck),
loading the relevant certificate/key .pem files only then. A program that does
no secure networking never reaches these, so it never needs the .pem files.

*******************************************************************************/

static void ensure_client_tls(void)

{

    EnterCriticalSection(&netlck);
    if (!client_tls_ctx)
        initctx(&client_tls_ctx, TLS_client_method(), "client_tls_cert.pem",
                                                      "client_tls_key.pem");
    LeaveCriticalSection(&netlck);

}

static void ensure_client_dtls(void)

{

    EnterCriticalSection(&netlck);
    if (!client_dtls_ctx)
        initctx(&client_dtls_ctx, DTLS_client_method(), "client_dtls_cert.pem",
                                                        "client_dtls_key.pem");
    LeaveCriticalSection(&netlck);

}

static void ensure_server_tls(void)

{

    EnterCriticalSection(&netlck);
    if (!server_tls_ctx) {

        initctx(&server_tls_ctx, TLS_server_method(), "server_tls_cert.pem",
                                                      "server_tls_key.pem");
        SSL_CTX_set_ecdh_auto(server_tls_ctx, 1);

    }
    LeaveCriticalSection(&netlck);

}

static void ensure_server_dtls(void)

{

    EnterCriticalSection(&netlck);
    if (!server_dtls_ctx) {

        initctx(&server_dtls_ctx, DTLS_server_method(), "server_dtls_cert.pem",
                                                        "server_dtls_key.pem");
        SSL_CTX_set_ecdh_auto(server_dtls_ctx, 1);

        /* Client has to authenticate */
        SSL_CTX_set_verify(server_dtls_ctx,
                           SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE,
                           dtls_verify_callback);

        SSL_CTX_set_session_cache_mode(server_dtls_ctx, SSL_SESS_CACHE_OFF);
        SSL_CTX_set_cookie_generate_cb(server_dtls_ctx, generate_cookie);
        SSL_CTX_set_cookie_verify_cb(server_dtls_ctx, &verify_cookie);

    }
    LeaveCriticalSection(&netlck);

}

/*******************************************************************************

Get 128 bit v6 address to 64 bit high/low

Gets a high/low 64 bit address value from a 128 bit v6 address. Windows
accesses the v6 address by bytes, which is also endian independent.

*******************************************************************************/

static void get128t64(struct sockaddr_in6* sap, unsigned long long* addrh,
                      unsigned long long* addrl)

{

    unsigned char* bp;
    int i;

    bp = (unsigned char*)&sap->sin6_addr;
    *addrh = 0;
    *addrl = 0;
    for (i = 0; i < 8; i++) *addrh = *addrh << 8 | bp[i];
    for (i = 8; i < 16; i++) *addrl = *addrl << 8 | bp[i];

}

/*******************************************************************************

Get 64 bit high/low v6 address to 128 bit

Gets a 128 bit v6 address from a 64 bit high/low address.

*******************************************************************************/

static void get64t128(unsigned long long addrh, unsigned long long addrl,
                      struct sockaddr_in6* sap)

{

    unsigned char* bp;
    int i;

    bp = (unsigned char*)&sap->sin6_addr;
    for (i = 0; i < 8; i++) bp[i] = addrh >> (7-i)*8 & 0xff;
    for (i = 0; i < 8; i++) bp[i+8] = addrl >> (7-i)*8 & 0xff;

}

/*******************************************************************************

Get server address v4

Retrieves a v4 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnet(const string name, unsigned long* addr)

{

    struct addrinfo hints;
    struct addrinfo *p, *pl;
    int r;
    int af;

    /* Note that unlike Linux, Windows returns entries with unset (0) socket
       types unless the hints select them */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    af = FALSE; /* set address not found */
    r = getaddrinfo(name, NULL, &hints, &pl);
    if (r) netwrterr(gai_strerror(r));
    p = pl;
    while (p) {

        /* traverse the available addresses */
        if (p->ai_family == AF_INET) {

            /* get the IPv4 address */
            *addr =
                ntohl(((struct sockaddr_in*)(p->ai_addr))->sin_addr.s_addr);
            af = TRUE; /* set an address found */

        }
        p = p->ai_next;

    }
    freeaddrinfo(pl);
    if (!af) error(enoipv4); /* no address found */

}

/*******************************************************************************

Get server address v6

Retrieves a v6 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnetv6(string name, unsigned long long* addrh,
                unsigned long long* addrl)

{

    struct addrinfo hints;
    struct addrinfo *p, *pl;
    int r;
    int af;

    /* Note that unlike Linux, Windows returns entries with unset (0) socket
       types unless the hints select them */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    af = FALSE; /* set address not found */
    r = getaddrinfo(name, NULL, &hints, &pl);
    if (r) netwrterr(gai_strerror(r));
    p = pl;
    while (p) {

        /* traverse the available addresses */
        if (p->ai_family == AF_INET6) {

            /* get the IPv6 address */
            get128t64((struct sockaddr_in6*)(p->ai_addr), addrh, addrl);
            af = TRUE; /* set an address found */

        }
        p = p->ai_next;

    }
    freeaddrinfo(pl);
    if (!af) error(enetadr); /* no address found */

}

/*******************************************************************************

Open network file

Opens a network file with the given address and port. The file access is
broken up into a read and write side, that work independently, can can be
spread among different tasks. This is done because the Pascal file paradigm
does not handle read/write on the same file well.

*******************************************************************************/

/* park a FILE entry on "nul" and attach the given socket to it. Common to
   opennet and waitnet, and performs the TLS negotiation if selected. The
   parked open is serialized under netlck because server and client can run
   on simultaneous threads of this process. */

static FILE* sockfil(SOCKET sock, int secure, int server)

{

    FILE*  fp;  /* file pointer */
    int    fn;  /* file number */
    filptr fep; /* file tracking pointer */
    SSL*   ssl;
    X509*  cert;
    int    r;

    /* open file handle as null device to allocate a CRT fd for the socket */
    EnterCriticalSection(&netlck);
    fp = fopen("nul", "r+");
    LeaveCriticalSection(&netlck);
    if (!fp) error(enetopn);
    fn = fileno(fp); /* get logical file no. */
    newfil(fn); /* get/renew file entry */
    fep = opnfil[fn]; /* index that */
    fep->sock = sock; /* place socket */
    fep->net = TRUE; /* set as network file */

    /* check secure sockets layer, and negotiate if so */
    if (secure) {

        /* The SSL layer talks directly to the socket, not through the CRT
           fd, so unlike Linux no shadow fd is needed to keep the
           interdictions from looping */
        if (server) {

            ensure_server_tls(); /* ensure context */
            ssl = SSL_new(server_tls_ctx); /* create new ssl */
            if (!ssl) error(esslnew);
            r = SSL_set_fd(ssl, (int)sock); /* connect to socket */
            if (!r) error(esslfid);

            /* perform ssl server accept */
            r = SSL_accept(ssl);
            if (r <= 0) sslerrorqueue();

            fep->ssl = ssl;
            fep->sec = TRUE; /* turn TLS encode/decode on for ssl channel */

        } else {

            ensure_client_tls(); /* ensure context */
            ssl = SSL_new(client_tls_ctx); /* create new ssl */
            if (!ssl) error(esslnew);
            r = SSL_set_fd(ssl, (int)sock); /* connect to socket */
            if (!r) error(esslfid);

            /* initiate tls handshake */
            r = SSL_connect(ssl);
            if (r != 1) sslerror(ssl, r);

            /* Get the remote certificate into the X509 structure.
               Right now we don't do anything with this (don't verify it) */
            cert = SSL_get_peer_certificate(ssl);
            if (!cert) error(esslcer);

            fep->ssl = ssl;
            fep->cert = cert;
            fep->sec = TRUE; /* turn TLS encode/decode on for ssl channel */

        }

    }

    return (fp); /* exit with file */

}

FILE* ami_opennet(/* IP address */      unsigned long addr,
                 /* port */            int port,
                 /* link is secured */ int secure
)

{

    SOCKET sock;
    struct sockaddr_in saddr;
    int    r;

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    /* note, parameters specified in big endian */
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = htonl(addr);

    /* open socket as internet, stream */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) wskerr(); /* process Winsock error */
    r = connect(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));
    if (r == SOCKET_ERROR) wskerr(); /* process Winsock error */

    /* finish with general routine */
    return (sockfil(sock, secure, FALSE));

}

FILE* ami_opennetv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    SOCKET sock;
    struct sockaddr_in6 saddr;
    int    r;

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    get64t128(addrh, addrl, &saddr);
    saddr.sin6_port = htons(port);

    /* open socket as internet, stream */
    sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) wskerr(); /* process Winsock error */
    r = connect(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in6));
    if (r == SOCKET_ERROR) wskerr(); /* process Winsock error */

    /* finish with general routine */
    return (sockfil(sock, secure, FALSE));

}

/*******************************************************************************

Open message file

Opens a message file with the given address, port and security. The file can be
both written and read. The protocol used for the transfer is either UDP, or
DTLS, with fixed length messages.

*******************************************************************************/

/* allocate a CRT fd on the null device to serve as the logical id of a
   message socket. This keeps message ids in the same space as the stream
   file ids, so the certificate calls can share code */

static int msgfil(SOCKET sock)

{

    int fn;

    EnterCriticalSection(&netlck);
    fn = _open("nul", _O_RDWR);
    LeaveCriticalSection(&netlck);
    if (fn < 0) error(enetopn);
    newfil(fn); /* get/renew file entry */
    opnfil[fn]->sock = sock; /* place socket */
    opnfil[fn]->net = TRUE; /* set network (sockets) file */
    opnfil[fn]->msg = TRUE; /* set message socket */

    return (fn);

}

int ami_openmsg(
    /* ip address */      unsigned long addr,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    struct sockaddr_in saddr;
    SOCKET sock;
    int fn;
    socket_struct laddr;
    int r;
    struct timeval timeout;
    DWORD tv;

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(addr);
    saddr.sin_port = htons(port);

    /* create the socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    fn = msgfil(sock); /* park a fd entry on the socket */

    /* set up server address */
    memset(&opnfil[fn]->saddr.s4, 0, sizeof(struct sockaddr_in));

    /* copy address information from caller */
    memcpy(&opnfil[fn]->saddr.s4, &saddr, sizeof(struct sockaddr_in));

    opnfil[fn]->v6addr = FALSE; /* set v4 address */

    /* set up for DTLS operation if selected */
    if (secure) {

        /* clear local */
        memset((void *) &laddr, 0, sizeof(socket_struct));
        laddr.s4.sin_family = AF_INET;
        laddr.s4.sin_port = htons(0);
        r = bind(sock, (const struct sockaddr *) &laddr,
                 sizeof(struct sockaddr_in));
        if (r == SOCKET_ERROR) wskerr();

        /* create socket struct */
        ensure_client_dtls(); /* ensure context */
        opnfil[fn]->ssl = SSL_new(client_dtls_ctx);
        if (!opnfil[fn]->ssl) sslerrorqueue();

        /* Create BIO, connect and set to already connected */
        opnfil[fn]->bio = BIO_new_dgram((int)sock, BIO_NOCLOSE);
        r = connect(sock, (struct sockaddr *) &opnfil[fn]->saddr,
                    sizeof(struct sockaddr_in));
        if (r == SOCKET_ERROR) wskerr();

        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0,
                 &opnfil[fn]->saddr.ss);

        SSL_set_bio(opnfil[fn]->ssl, opnfil[fn]->bio, opnfil[fn]->bio);

        r = SSL_connect(opnfil[fn]->ssl);
        if (r <= 0) sslerror(opnfil[fn]->ssl, r);

        /* Set and activate timeouts */
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        /* set secure udp */
        opnfil[fn]->sudp = TRUE;

    } else {

        /* clear (plain UDP): bound the receive so a lost or late reply fails
           the read instead of blocking forever -- the secured path above gets
           this via BIO_CTRL_DGRAM_SET_RECV_TIMEOUT. Windows takes the timeout
           as a DWORD in milliseconds */
        tv = 3000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    }

    return (fn); /* return fid */

}

int ami_openmsgv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    struct sockaddr_in6 saddr;
    SOCKET sock;
    int fn;
    socket_struct laddr;
    int r;
    struct timeval timeout;
    DWORD tv;

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    get64t128(addrh, addrl, &saddr);
    saddr.sin6_port = htons(port);

    /* create the socket */
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    fn = msgfil(sock); /* park a fd entry on the socket */

    /* set up server address */
    memset(&opnfil[fn]->saddr.s6, 0, sizeof(struct sockaddr_in6));

    /* copy address information from caller */
    memcpy(&opnfil[fn]->saddr.s6, &saddr, sizeof(struct sockaddr_in6));

    opnfil[fn]->v6addr = TRUE; /* set v6 address */

    /* set up for DTLS operation if selected */
    if (secure) {

        /* clear local */
        memset((void *) &laddr, 0, sizeof(socket_struct));
        laddr.s6.sin6_family = AF_INET6;
        laddr.s6.sin6_port = htons(0);
        r = bind(sock, (const struct sockaddr *) &laddr,
                 sizeof(struct sockaddr_in6));
        if (r == SOCKET_ERROR) wskerr();

        /* create socket struct */
        ensure_client_dtls(); /* ensure context */
        opnfil[fn]->ssl = SSL_new(client_dtls_ctx);
        if (!opnfil[fn]->ssl) sslerrorqueue();

        /* Create BIO, connect and set to already connected */
        opnfil[fn]->bio = BIO_new_dgram((int)sock, BIO_NOCLOSE);
        r = connect(sock, (struct sockaddr *) &opnfil[fn]->saddr,
                    sizeof(struct sockaddr_in6));
        if (r == SOCKET_ERROR) wskerr();

        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0,
                 &opnfil[fn]->saddr.ss);

        SSL_set_bio(opnfil[fn]->ssl, opnfil[fn]->bio, opnfil[fn]->bio);

        r = SSL_connect(opnfil[fn]->ssl);
        if (r <= 0) sslerror(opnfil[fn]->ssl, r);

        /* Set and activate timeouts */
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        /* set secure udp */
        opnfil[fn]->sudp = TRUE;

    } else {

        /* clear (plain UDP): bound the receive so a lost or late reply fails
           the read instead of blocking forever */
        tv = 3000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    }

    return (fn); /* return fid */

}

/*******************************************************************************

Wait external message connection

Waits for an external message socket connection on a given port address. If an
external client connects to that port, then a socket file is opened and the file
number returned. Note that any number of such connections can be active at one
time. The program can invoke multiple tasks to handle each connection. If
another program tries to take the same port, it is blocked.

*******************************************************************************/

int ami_waitmsg(/* port number to wait on */ int port,
               /* secure mode */            int secure
               )

{

    socket_struct caddr;
    SOCKET sock;
    int fn;
    int r;
    int opt;
    int off;
    struct timeval timeout;
    DWORD tv;

    /* create the socket */
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    /* Windows defaults v6 sockets to v6 only; turn that off so we serve v4
       clients as well (dual stack, matching Linux) */
    off = 0;
    setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off));

    /* set socket options, multiple servers on address and same port */
    opt = 1;
    r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt,
                   sizeof(opt));
    if (r == SOCKET_ERROR) wskerr();

    fn = msgfil(sock); /* park a fd entry on the socket */

    /* clear server address */
    memset(&opnfil[fn]->saddr, 0, sizeof(socket_struct));

    /* set up address */
    opnfil[fn]->saddr.s6.sin6_family = AF_INET6;
    opnfil[fn]->saddr.s6.sin6_addr = in6addr_any;
    opnfil[fn]->saddr.s6.sin6_port = htons(port);

    opnfil[fn]->v6addr = TRUE; /* set v6 address */

    /* bind to socket */
    r = bind(sock, (struct sockaddr *)&opnfil[fn]->saddr.s6,
                 sizeof(struct sockaddr_in6));
    if (r == SOCKET_ERROR) wskerr();

    /* set up for DTLS operation if selected */
    if (secure) {

        memset(&caddr, 0, sizeof(socket_struct));

        /* Create BIO */
        opnfil[fn]->bio = BIO_new_dgram((int)sock, BIO_NOCLOSE);

        /* Set and activate timeouts */
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        ensure_server_dtls(); /* ensure context */
        opnfil[fn]->ssl = SSL_new(server_dtls_ctx);
        if (!opnfil[fn]->ssl) sslerrorqueue();

        SSL_set_bio(opnfil[fn]->ssl, opnfil[fn]->bio, opnfil[fn]->bio);
        SSL_set_options(opnfil[fn]->ssl, SSL_OP_COOKIE_EXCHANGE);

        while (DTLSv1_listen(opnfil[fn]->ssl, (BIO_ADDR *) &caddr) <= 0);

        /* Connect the listening socket itself to the client. Linux uses a
           second socket bound to the same port so it can keep listening, but
           Windows does not dispatch datagrams by connected tuple between
           sockets sharing a port, so the handshake would be delivered to the
           unread listening socket. We serve a single connection per wait, so
           the listening socket can simply become the connected one. */
        r = connect(sock, (struct sockaddr *) &caddr.s6,
                    sizeof(struct sockaddr_in6));
        if (r == SOCKET_ERROR) wskerr();

        /* set BIO to connected */
        BIO_ctrl(SSL_get_rbio(opnfil[fn]->ssl), BIO_CTRL_DGRAM_SET_CONNECTED,
                 0, &caddr.ss);

        /* Finish handshake */
        do { r = SSL_accept(opnfil[fn]->ssl); } while (r == 0);
        if (r < 0) sslerror(opnfil[fn]->ssl, r);

        /* Set and activate timeouts */
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        BIO_ctrl(SSL_get_rbio(opnfil[fn]->ssl), BIO_CTRL_DGRAM_SET_RECV_TIMEOUT,
                 0, &timeout);

        /* set secure udp */
        opnfil[fn]->sudp = TRUE;

    } else {

        /* clear (plain UDP): bound the receive so a dropped client message
           (e.g. sent before this server bound) times out and the server exits
           instead of lingering. Windows takes the timeout as a DWORD in
           milliseconds */
        tv = 5000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    }

    return (fn); /* return fid */

}

/*******************************************************************************

Get maximum message size v4

Returns the maximum size of messages in the system. This is typically 1500, or
the maximum size of a UDP message, but can be larger, a so called "jumbo packet"
at 64kb, or somewhere in between.

We return the MTU reported by the interface. For a reliable network, this is
the absolute packet size. For others, it will be the MTU of the interface, which
packet breakage is possible.

Note the Windows loopback interface reports an effectively unbounded MTU, so
the result is clamped to the maximum UDP payload.

*******************************************************************************/

static int clampmtu(int mtu)

{

    /* the UDP payload limit is the practical message maximum */
    if (mtu <= 0 || mtu > 65507) mtu = 65507;

    return (mtu);

}

int ami_maxmsg(unsigned long addr)

{

    struct sockaddr_in saddr;
    SOCKET sock;
    int r;
    int mtu;
    int mtulen;

    mtulen = sizeof(mtu); /* set length of word */

    /* set up target address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(addr);

    /* create socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    /* connect to address */
    r = connect(sock, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r == SOCKET_ERROR) wskerr();

    /* Find the mtu. Unix reads the connected socket's path mtu with
       getsockopt(IP_MTU); Windows has no such option, so fall back to the
       standard ethernet mtu (clampmtu bounds it to the udp payload). A later
       refinement can read the real interface mtu through the IP helper API
       (GetBestInterfaceEx + GetIpInterfaceEntry). */
#ifdef IP_MTU
    r = getsockopt(sock, IPPROTO_IP, IP_MTU, (char*)&mtu, &mtulen);
    if (r == SOCKET_ERROR) wskerr();
#else
    (void)mtulen; mtu = 1500;
#endif

    closesocket(sock);

    return (clampmtu(mtu)); /* return mtu */

}

/*******************************************************************************

Get maximum message size v6

Returns the maximum size of messages in the system. This is typically 1500, or
the maximum size of a UDP message, but can be larger, a so called "jumbo packet"
at 64kb, or somewhere in between.

We return the MTU reported by the interface. For a reliable network, this is
the absolute packet size. For others, it will be the MTU of the interface, which
packet breakage is possible.

*******************************************************************************/

int ami_maxmsgv6(unsigned long long addrh, unsigned long long addrl)

{

    struct sockaddr_in6 saddr;
    SOCKET sock;
    int r;
    int mtu;
    int mtulen;

    mtulen = sizeof(mtu); /* set length of word */

    /* set up target address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    get64t128(addrh, addrl, &saddr);

    /* create socket */
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    /* connect to address */
    r = connect(sock, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r == SOCKET_ERROR) wskerr();

    /* find the mtu; see ami_maxmsg for why Windows uses the default */
#ifdef IPV6_MTU
    r = getsockopt(sock, IPPROTO_IPV6, IPV6_MTU, (char*)&mtu, &mtulen);
    if (r == SOCKET_ERROR) wskerr();
#else
    (void)mtulen; mtu = 1500;
#endif

    closesocket(sock);

    return (clampmtu(mtu)); /* return mtu */

}

/*******************************************************************************

Write message to message file

Writes a message to the given message file. The message file must be open. Any
size (including 0) up to ami_maxmsg() is allowed.

*******************************************************************************/

void ami_wrmsg(int fn, void* msg, unsigned long len)

{

    int r;
    int sr;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    if (opnfil[fn]->sudp) { /* secure udp */

        sr = SSL_write(opnfil[fn]->ssl, msg, len);
        if (sr <= 0) sslerror(opnfil[fn]->ssl, sr);

    } else {

        /* write the message to socket */
        if (opnfil[fn]->v6addr)
            r = sendto(opnfil[fn]->sock, msg, len, 0,
                       (const struct sockaddr *) &opnfil[fn]->saddr.s6,
                       sizeof(struct sockaddr_in6));
        else
            r = sendto(opnfil[fn]->sock, msg, len, 0,
                       (const struct sockaddr *) &opnfil[fn]->saddr.s4,
                       sizeof(struct sockaddr_in));
        if (r == SOCKET_ERROR) wskerr();

    }

}

/*******************************************************************************

Read message from message file

Reads a message from the message file, of any length up to and including the
specified buffer length. The actual length transferred is returned. The length
of the buffer should be equal to maxmsg to pass all possible messages, unless it
is known that a given message size will never be exceeded.

*******************************************************************************/
int ami_rdmsg(int fn, void* msg, unsigned long len)

{

    int r;
    int al;
    int sr;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    if (opnfil[fn]->sudp) { /* secure udp */

        sr = SSL_read(opnfil[fn]->ssl, msg, len);
        if (sr <= 0) sslerror(opnfil[fn]->ssl, sr);
        r = sr; /* set length read */

    } else {

        /* read the message from socket, blocking (get full UDP message).
           The sender address is captured so a server can reply */
        if (opnfil[fn]->v6addr) {

            al = sizeof(struct sockaddr_in6);
            r = recvfrom(opnfil[fn]->sock, msg, len, 0,
                         (struct sockaddr *) &opnfil[fn]->saddr.s6, &al);

        } else {

            al = sizeof(struct sockaddr_in);
            r = recvfrom(opnfil[fn]->sock, msg, len, 0,
                         (struct sockaddr *) &opnfil[fn]->saddr.s4, &al);

        }
        if (r == SOCKET_ERROR) wskerr();

    }

    return (r); /* exit with length of message */

}

/*******************************************************************************

Close message file

Closes the given message file.

*******************************************************************************/

void ami_clsmsg(int fn)

{

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    /* if DTLS, free the ssl struct (frees the attached BIO as well) */
    if (opnfil[fn]->sudp) SSL_free(opnfil[fn]->ssl);
    opnfil[fn]->ssl = NULL;
    opnfil[fn]->bio = NULL;

    closesocket(opnfil[fn]->sock); /* close the socket */
    if (opnfil[fn]->sock2 != INVALID_SOCKET) closesocket(opnfil[fn]->sock2);
    opnfil[fn]->sock = INVALID_SOCKET;
    opnfil[fn]->sock2 = INVALID_SOCKET;
    opnfil[fn]->net = FALSE;
    opnfil[fn]->msg = FALSE;
    opnfil[fn]->sudp = FALSE;

    _close(fn); /* release the parked fd */

}

/*******************************************************************************

Wait external network connection

Waits for an external socket connection on a given port address. If an external
client connects to that port, then a socket file is opened and the file number
returned. Note that any number of such connections can be active at one time.
The program can invoke multiple tasks to handle each connection. If another
program tries to take the same port, it is blocked.

*******************************************************************************/

FILE* ami_waitnet(/* port number to wait on */ int port,
                 /* secure mode */            int secure
                )

{

    struct sockaddr_in6 saddr;
    SOCKET ls, as;
    int r;
    int opt;
    int off;

    /* create the listening socket */
    ls = socket(AF_INET6, SOCK_STREAM, 0);
    if (ls == INVALID_SOCKET) wskerr();

    /* Windows defaults v6 sockets to v6 only; turn that off so we serve v4
       clients as well (dual stack, matching Linux) */
    off = 0;
    setsockopt(ls, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off));

    /* set socket options, multiple servers on address and same port */
    opt = 1;
    r = setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt,
                   sizeof(opt));
    if (r == SOCKET_ERROR) wskerr();

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_addr = in6addr_any;
    saddr.sin6_port = htons(port);
    r = bind(ls, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r == SOCKET_ERROR) wskerr();

    /* wait on port */
    r = listen(ls, 3);
    if (r == SOCKET_ERROR) wskerr();

    /* accept connection, discard peer address */
    as = accept(ls, NULL, NULL);
    if (as == INVALID_SOCKET) wskerr();

    /* discard server port */
    closesocket(ls);

    /* finish with general routine */
    return (sockfil(as, secure, TRUE));

}

/*******************************************************************************

Message files are reliable

Returns true if the message files on this host are implemented with guaranteed
delivery and in order. This is a property of high performance compute clusters,
that the delivery of messages are guaranteed to arrive error free at their
destination. If this property appears, the program can skip providing it's own
retry or other error handling system.

There are two versions of this routine, depending on if ipv4 or ipv6 addresses
are used.

At the moment, this is a test to see if the address is the local address of
the host, meaning that it is a completely local connection that will not be
carried on the wire. Thus it is reliable by definition.

*******************************************************************************/

int ami_relymsg(unsigned long addr)

{

    return (addr == 0x7f000001);

}

int ami_relymsgv6(unsigned long long addrh, unsigned long long addrl)

{

    return (addrh == 0 && addrl == 1); /* test local host */

}

/*******************************************************************************

Get message certificate

Fetches the SSL certificate for an DTLS connection. The file must contain
an open and active DTLS connection. Retrieves on of the indicated
certificates by number, from 1 to N where N is the maximum certificate in the
chain. Certificate 1 is the certificate for the server connected. Certificate
N is the CA or Certificate Authority's certificate, AKA the root certificate.
The size of the certificate buffer is passed, and the actual length of the
certificate is returned. If there is no certificate by a given number, the
resulting length is zero.

Certificates are in base64 format, the same as a PEM file, starting with the
line "-----BEGIN CERTIFICATE-----" and ending with the line
"-----END CERTIFICATE----". The buffer contains a whole
certificate. Note that characters with value < 20 (control characters), may or
may not be included in the certificate. Typically carriage returns, line feed
or both may be used to break up lines in the certificate.

Certificates are normally retrieved in numerical order, that is, 1, 2, 3...N.
Thus the end of the certificate chain must be found by traversal.

Note that this routine retrieves the peer certificate, or other end of the
line. Servers are required to provide certificates. Clients are not.

*******************************************************************************/

int ami_certmsg(int fn, int which, string buff, int len)

{

    X509* cert;
    STACK_OF(X509)* certstk;
    BIO* cb;
    int r;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    if (which < 1) error(einvctn); /* invalid certificate number */

    makfil(fn); /* create file entry as required */
    if (!opnfil[fn]->sudp && !opnfil[fn]->sec) error(enotsec);

    /* get the certificate */
    cert = SSL_get_peer_certificate(opnfil[fn]->ssl);
    if (!cert) error(enocert);

    /* see if we need to get the chain */
    if (which > 1) {

        certstk = SSL_get_peer_cert_chain(opnfil[fn]->ssl);
        if (!certstk) {

            /* Zakir Durumeric says that it is possible to get a null chain,
               even though obviously a single certificate exists (it should
               be first in chain). So if this happens, we create our own chain
               and stuff the first certificate in it */
            certstk = sk_X509_new_null();
            sk_X509_push(certstk, cert);

        }
        /* if the certificate number is out of range, return nothing */
        if (which > sk_X509_num(certstk)) cert = NULL;
        else cert = sk_X509_value(certstk, which-1);

    }

    r = 0; /* set no certificate */
    if (cert) { /* there was a certificate, read to memory */

        /* make a bio to memory */
        cb = BIO_new(BIO_s_mem());
        if (!cb) error(enobio);

        /* write certificate to BIO */
        r = PEM_write_bio_X509(cb, cert);
        if (!r) error(ewrbio);

        /* read certificate back to memory */
        r = BIO_read(cb, buff, len);
        if (r < 0) error(erdbio);
        if (!BIO_eof(cb)) error(ecerttl);

        BIO_free(cb);

    }

    return (r);

}

/*******************************************************************************

Get network certificate

Fetches the SSL certificate for an SSL connection. The file must contain an
open and active SSL connection. Retrieves on of the indicated certificates by
number, from 1 to N where N is the maximum certificate in the chain.
Certificate 1 is the certificate for the server connected. Certificate N is the
CA or Certificate Authority's certificate, AKA the root certificate. The size
of the certificate buffer is passed, and the actual length of the certificate
is returned. If there is no certificate by a given number, the resulting length
is zero.

Certificates are in base64 format, the same as a PEM file, except without the
"BEGIN CERTIFICATE" and "END CERTIFICATE" lines. The buffer contains a whole
certificate. Note that characters with value < 20 (control characters), may or
may not be included in the certificate. Typically carriage returns, line feed
or both may be used to break up lines in the certificate.

Certificates are normally retrieved in numerical order, that is, 1, 2, 3...N.
Thus the end of the certificate chain must be found by traversal.

Note that this routine retrieves the peer certificate, or other end of the
line. Servers are required to provide certificates. Clients are not.

*******************************************************************************/

int ami_certnet(FILE* f, int which, string buff, int len)

{

    int fn;

    fn = fileno(f); /* get fid */

    /* with the fid, the call is the same as for messaging */
    return (ami_certmsg(fn, which, buff, len));

}

/*******************************************************************************

Get network certificate data list

Retrieves a list of data fields from the given file by number. The file
must contain an open and active SSL connection. The data list is a list of
name - data pairs, both strings. The list can also have branches or forks,
which make it able to contain complete trees. The certificate number is from
1 to N where N is the maximum certificate in the chain. Certificate 1 is the
certificate for the server connected. Certificate N is the CA or Certificate
Authority's certificate, AKA the root certificate. If there is no certificate
by that number, the resulting list is NULL.

Note that the decoded certificate list is not completely working on any
platform yet (see network_test.txt); like Linux this entry aborts as
unimplemented.

*******************************************************************************/

void ami_certlistnet(FILE *f, int which, ami_certptr* list)

{

    error(enotimp);

}

/*******************************************************************************

Get message certificate data list

Retrieves a list of data fields from the given file by number. The file
must contain an open and active DTLS connection. See the notes for
ami_certlistnet.

*******************************************************************************/

void ami_certlistmsg(int fn, int which, ami_certptr* list)

{

    error(enotimp);

}

/*******************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement stdio:

read
write
open
close
lseek

We use interdiction to allow network files to use stdio calls on network
streams, but allow enhanced features to be implemented as well.

*******************************************************************************/

/*******************************************************************************

Open

We don't do anything for this, so pass it on.

*******************************************************************************/

static int iopen(const char* pathname, int flags, int perm)

{

    return (*ofpopen)(pathname, flags, perm);

}

/*******************************************************************************

Close file

Closes the file. The file is closed at the system level, then we remove the
table entry for the file.

A file can be a normal file or a network connection. If its normal, it is passed
on to the operating system. If it's a network connection, then the network
connection is broken, and both the input and output sides are closed.

*******************************************************************************/

static int iclose(int fd)

{

    filptr fp;

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->net) {

        fp = opnfil[fd]; /* index that */
        if (fp->ssl) SSL_free(fp->ssl); /* free the ssl */
        fp->ssl = NULL;
        fp->bio = NULL; /* freed with the ssl */
        if (fp->cert) X509_free(fp->cert); /* free certificate */
        fp->cert = NULL;
        if (fp->sock != INVALID_SOCKET) closesocket(fp->sock);
        if (fp->sock2 != INVALID_SOCKET) closesocket(fp->sock2);
        fp->sock = INVALID_SOCKET;
        fp->sock2 = INVALID_SOCKET;
        fp->net = FALSE;
        fp->sec = FALSE;
        fp->msg = FALSE;
        fp->sudp = FALSE;

    }

    return (*ofpclose)(fd); /* normal file close */

}

/*******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

Note that on sockets a short read is normal; the stdio layer above simply
takes what arrives. A zero result (connection closed by the peer) is passed
through as end of file.

*******************************************************************************/

static ssize_t iread(int fd, void* buff, size_t count)

{

    int r; /* int result */
    int e;

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->net) {

        if (opnfil[fd]->sec) { /* secure (TLS) file */

            r = SSL_read(opnfil[fd]->ssl, buff, count);
            if (r <= 0) {

                e = SSL_get_error(opnfil[fd]->ssl, r);
                /* orderly shutdown by peer is end of file */
                if (e == SSL_ERROR_ZERO_RETURN) r = 0;
                else sslerror(opnfil[fd]->ssl, r);

            }

        } else {

            r = recv(opnfil[fd]->sock, buff, count, 0); /* receive network data */
            if (r == SOCKET_ERROR) wskerr(); /* flag read error */

        }

        return (r);

    }

    return (*ofpread)(fd, buff, count); /* standard file read */

}

/*******************************************************************************

Write file

Writes a byte buffer to the output file. If the file is normal, we pass it on.
If the file is a network file, we process a write on the associated socket.

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    int r;
    const char* p;
    size_t left;

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->net) {

        if (opnfil[fd]->sec) { /* secure (TLS) file */

            r = SSL_write(opnfil[fd]->ssl, buff, count);
            if (r <= 0) sslerror(opnfil[fd]->ssl, r);

        } else {

            /* transmit network data; send may take less than the whole
               buffer, so loop until everything is out */
            p = buff;
            left = count;
            while (left) {

                r = send(opnfil[fd]->sock, p, left, 0);
                if (r == SOCKET_ERROR) wskerr(); /* flag write error */
                p += r;
                left -= r;

            }

        }

        return (count);

    }

    /* standard file write */
    return (*ofpwrite)(fd, buff, count);

}

/*******************************************************************************

Lseek

Lseek is never possible on a network, so this just passed on.

*******************************************************************************/

static off_t ilseek(int fd, off_t offset, int whence)

{

    return (*ofplseek)(fd, offset, whence);

}

/*******************************************************************************

Console control handler

This procedure gets activated as a callback when Windows flags a termination
event to console. We immediately abort.

At the present time, we don't care what type of termination event it was,
all generate an etterm signal.

*******************************************************************************/

static BOOL WINAPI conhan(DWORD ct)

{

    int fi; /* index for file tables */

    WSACleanup();
    /* shutdown the open connections here, because the winsock dll is already
       shut down before the deinit handler gets executed */
    if (!dblflt) { /* we haven't already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->net &&
                opnfil[fi]->sock != INVALID_SOCKET)
                closesocket(opnfil[fi]->sock); /* close socket */

    }

    return (1); /* set event handled */

}

/*******************************************************************************

Network startup

*******************************************************************************/

static void ami_init_network (void) __attribute__((constructor (103)));
static void ami_init_network()

{

    int     fi;  /* index for file tables */
    int     r;   /* result code */
    WSADATA wsd; /* windows socket data */

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_lseek(ilseek, &ofplseek);

    dblflt = FALSE; /* set no double fault */

    /* clear open files table */
    for (fi = 0; fi < MAXFIL; fi++) opnfil[fi] = NULL; /* set unoccupied */

    /* initialize the table/context lock */
    InitializeCriticalSection(&netlck);

    /* perform winsock startup */
    r = WSAStartup(MAKEWORD(2,2), &wsd);
    if (r) wskerr(); /* can't initalize Winsock */

    /* initialize SSL library and register algorithms */
    if(SSL_library_init() < 0) error(einissl);
    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();

    /* The four SSL contexts are created lazily on first secure use (see the
       ensure_client_tls/ensure_client_dtls/ensure_server_tls/
       ensure_server_dtls functions above), so the certificate/key .pem files
       are not required by a program that does no secure networking. */

    /* set cookie uninitialized */
    cookie_initialized = FALSE;

    /* capture control handler so that ctl-c cancels properly. This is a
       workaround because the winsock dll gets shutdown before we reach
       the deinit function. */
    //SetConsoleCtrlHandler(conhan, TRUE);

}

/*******************************************************************************

Network shutdown

*******************************************************************************/

static void ami_deinit_network (void) __attribute__((destructor (103)));
static void ami_deinit_network()

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    plseek_t cpplseek;

    int fi; /* index for file tables */

    if (!dblflt) { /* we haven't already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->net) {

                if (opnfil[fi]->ssl) SSL_free(opnfil[fi]->ssl);
                if (opnfil[fi]->cert) X509_free(opnfil[fi]->cert);
                if (opnfil[fi]->sock != INVALID_SOCKET)
                    closesocket(opnfil[fi]->sock); /* close socket */
                if (opnfil[fi]->sock2 != INVALID_SOCKET)
                    closesocket(opnfil[fi]->sock2);

            }

    }
    /* free context structures */
    if (client_tls_ctx) SSL_CTX_free(client_tls_ctx);
    if (client_dtls_ctx) SSL_CTX_free(client_dtls_ctx);
    if (server_tls_ctx) SSL_CTX_free(server_tls_ctx);
    if (server_dtls_ctx) SSL_CTX_free(server_dtls_ctx);

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_lseek(ofplseek, &cpplseek);
    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose || cpplseek != ilseek)
        error(esystem);

    DeleteCriticalSection(&netlck);

    /* release control handler */
    SetConsoleCtrlHandler(NULL, FALSE);

}
