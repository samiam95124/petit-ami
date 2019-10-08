/*******************************************************************************
*                                                                              *
*                           INTERNET ACCESS LIBRARY                            *
*                                                                              *
*                       Copyright (C) 2019 Scott A. Franco                     *
*                                                                              *
* Implements access to internet functions, via tcp/ip. tcp/ip is implemented   *
* via the "file" paradygm. An address and port is used to create a file, then  *
* normal stdio read and write statements are used to access it.                *
*                                                                              *
* Linux (as well as all socket based Unix clones), already treats connections  *
* as files. What the network library does for you is make it portable to       *
* systems that cannot do that (Windows), and also give a standard method to    *
* open such files.                                                             *
*                                                                              *
*******************************************************************************/

/* Linux definitions */
#include <sys/types.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <stdio.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>

/* Petit-Ami definitions */
#include <localdefs.h>
#include <network.h>

#define MAXFIL 1000 /* maximum number of open files */

/* File tracking.
   Files could be transparent in the case of plain text, but SSL and advanced
   layering needs special handling. So we translate the file descriptors and
   flag if they need special handling. */
typedef struct filrec {

   pthread_mutex_t lock; /* lock for this structure */
   /* we don't use the network flag for much right now, except to indicate that
      the file is a socket. Linux treats them equally */
   boolean net;  /* it's a network file */
   boolean sec;  /* its a secure sockets file */
   SSL*    ssl;  /* SSL data */
   X509*   cert; /* peer certificate */
   int     sfn;  /* shadow fid */
   boolean opn;  /* file is open with Linux */
   struct filrec* next; /* next entry (when placed on free list) */

} filrec;
typedef filrec* filptr; /* pointer to file record */

/* error codes */
typedef enum {
    ewskini,  /* cannot initialize winsock */
    einvhan,  /* invalid file handle */
    enetopn,  /* cannot reset or rewrite network file */
    enetpos,  /* cannot position network file */
    enetloc,  /* cannot find location network file */
    enetlen,  /* cannot find length network file */
    esckeof,  /* } encountered on socket */
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
    esystem   /* System consistency check */
} errcod;

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef off_t (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_read_nocancel(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_write_nocancel(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_open_nocancel(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_close_nocancel(pclose_t nfp, pclose_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t   ofpread;
static pread_t   ofpread_nocancel;
static pwrite_t  ofpwrite;
static pwrite_t  ofpwrite_nocancel;
static popen_t   ofpopen;
static popen_t   ofpopen_nocancel;
static pclose_t  ofpclose;
static pclose_t  ofpclose_nocancel;
static plseek_t  ofplseek;

/*
 * Variables
 */
static filptr opnfil[MAXFIL];  /* open files table */
static pthread_mutex_t oflock; /* lock for this structure */
static filptr frefil;          /* free file entries list */
static pthread_mutex_t fflock; /* lock for this structure */


/*
 * openSSL variables
 */
static SSL_CTX* client_ctx;
static SSL_CTX* server_ctx;

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

        case ewskini:  netwrterr("Cannot initalize winsock"); break;
        case einvhan:  netwrterr("Invalid file number"); break;
        case enetopn:  netwrterr("Cannot reset or rewrite network file"); break;
        case enetpos:  netwrterr("Cannot position network file"); break;
        case enetloc:  netwrterr("Cannot find location network file"); break;
        case enetlen:  netwrterr("Cannot find length network file"); break;
        case esckeof:  netwrterr("end encountered on socket"); break;
        case efinuse:  netwrterr("File already in use"); break;
        case enetwrt:  netwrterr("Attempt to write to input side of network "
                                 "pair"); break;
        case enetadr:  netwrterr("Cannot determine address of server"); break;
        case einissl:  netwrterr("Cannot initialize OpenSSL library"); break;
        case esslnew:  netwrterr("Cannot create SSL"); break;
        case esslctx:  netwrterr("Cannot create SSL context"); break;
        case esslfid:  netwrterr("Cannot connect SSL to file id"); break;
        case esslses:  netwrterr("Cannot create SSL session"); break;
        case esslcer:  netwrterr("Cannot get SSL certificate"); break;
        case enodupf:  netwrterr("Cannot create duplicate fid"); break;
        case enoallc:  netwrterr("Cannot allocate file entry"); break;
        case esystem:  netwrterr("System consistency check, please contact "
                                 "vendor"); break;

    }

}

/*******************************************************************************

Handle Linux error

Handles error in errno. Prints the string assocated with the error.

*******************************************************************************/

static void linuxerror(void)

{

    fprintf(stderr, "\nLinux Error: %s\n", strerror(errno));

    exit(1);

}
/*******************************************************************************

Handle SSL error queue

Dumps the SSL error queue. I don't know what the difference is between this and
SSL errors at this time.

*******************************************************************************/

static void sslerrorqueue(void)

{

    ERR_print_errors_fp(stderr);
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

Gets a file entry, either from the free stack or by allocation. Clears the
fields in the file structure.

Note that we assume the malloc() calls kernel functions, and so we should not
call it with a lock.

*******************************************************************************/

static filptr getfil(void)

{

    filptr fp;

    if (frefil) { /* get existing free entry */

        pthread_mutex_lock(&fflock); /* take free file lock */
        fp = frefil; /* index top */
        frefil = fp->next; /* gap */
        pthread_mutex_unlock(&fflock); /* release */

    } else { /* allocate new one */

        /* get entry */
        fp = malloc(sizeof(filrec));
        if (!fp) error(enoallc); /* didn't work */
        /* allocate a lock for that (this is permanent) */
        pthread_mutex_init(&fp->lock, NULL);

    }
    fp->net = false; /* set not network file */
    fp->sec = false;  /* set ordinary socket */
    fp->ssl = NULL;   /* clear SSL data */
    fp->cert = NULL;  /* clear certificate data */
    fp->sfn = -1;     /* set no shadow */
    fp->opn = false;  /* set not open */
    fp->next = NULL; /* set no next */

    return (fp);

}

/*******************************************************************************

Put file entry

Places the given file entry on the free list, using the free list lock. This
routine is callable with a (unrelated) lock by the rules of structured locking,
but note it's sister function getfil is not lock callable.

*******************************************************************************/

static void putfil(filptr fp)

{

    pthread_mutex_lock(&fflock); /* take free file lock */
    fp->next = frefil; /* place on free list */
    frefil = fp;
    pthread_mutex_unlock(&fflock); /* release */

}

/*******************************************************************************

Make file entry

Checks the indicated file table entry, and allocates a new one if none is
allocated.

*******************************************************************************/

static void makfil(int fn)

{

    filptr fp;

    /* See if the file entry is undefined, then get a file entry if not.
       This could be lapped by another thread, but we are saving the effort
       of taking a lock on each file entry validation. This is why we use
       a free list, even though we don't dispose of entries in the files
       array, just so we can have a supply of ready to use file entries. */
    if (!opnfil[fn]) {

        fp = getfil();
        pthread_mutex_lock(&oflock); /* take the table lock */
        if (!opnfil[fn]) opnfil[fn] = fp; /* still undefined, place */
        else putfil(fp); /* otherwise we got lapped, save the entry */
        pthread_mutex_unlock(&oflock); /* release the table lock */

    }

}

/*******************************************************************************

Get new file entry

Checks the indicated file table entry, and allocates a new one if none is
allocated. Then the file entry is initialized.

*******************************************************************************/

void newfil(int fn)

{

    filptr fp;

    /* See if the file entry is undefined, then get a file entry if not.
       This could be lapped by another thread, but we are saving the effort
       of taking a lock on each file entry validation. This is why we use
       a free list, even though we don't dispose of entries in the files
       array, just so we can have a supply of ready to use file entries. */
    if (!opnfil[fn]) {

        fp = getfil();
        pthread_mutex_lock(&oflock); /* take the table lock */
        if (!opnfil[fn]) opnfil[fn] = fp; /* still undefined, place */
        else putfil(fp); /* otherwise we got lapped, save the entry */
        pthread_mutex_unlock(&oflock); /* release the table lock */

    }
    pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
    opnfil[fn]->net = false; /* set unoccupied */
    opnfil[fn]->sec = false;  /* set ordinary socket */
    opnfil[fn]->ssl = NULL;   /* clear SSL data */
    opnfil[fn]->cert = NULL;  /* clear certificate data */
    opnfil[fn]->sfn = -1;     /* set no shadow */
    opnfil[fn]->opn = false;  /* set not open */
    pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

}

/*******************************************************************************

Get server address

Retrives a server address by name. The name is given as a string. The address
is returned as an integer.

*******************************************************************************/

void pa_addrnet(string name, unsigned long* addr)

{

	struct addrinfo *p;
	int r;
	boolean af;

    af = false; /* set address not found */
	r = getaddrinfo(name, NULL, NULL, &p);
	if (r) netwrterr(gai_strerror(r));
	while (p) {

    	/* traverse the available addresses */
        if (p->ai_family == AF_INET && p->ai_socktype == SOCK_STREAM) {

            /* get the IPv4 address */
	        *addr =
	            ntohl(((struct sockaddr_in*)(p->ai_addr))->sin_addr.s_addr);
	        af = true; /* set an address found */

	    }
	    p = p->ai_next;

	}
	if (!af) error(enetadr); /* no address found */

}

/*******************************************************************************

Open network file

Opens a network file with the given address, port and security. The file can be
both written and read. The protocol used for the transfer is either
TCP/IP or TCP/IP with a security layer, typically TLS 1.3 at this writing.

*******************************************************************************/

FILE* pa_opennet(/* IP address */      unsigned long addr,
                 /* port */            int port,
                 /* link is secured */ boolean secure
                )

{

    struct sockaddr_in saddr;
    int   fn;
    int   r;
    FILE* fp;
    int   sfn;
    SSL*  ssl;
    X509* cert;

    /* set up address */
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(addr);
    saddr.sin_port = htons(port);
    /* connect the socket */
    fn = socket(AF_INET, SOCK_STREAM, 0);
    if (fn < 0) linuxerror();
    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    r = connect(fn, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r < 0) linuxerror();
    /* connect fid to FILE pointer for glibc use */
    fp = fdopen(fn, "r+");
    if (!fp) linuxerror();
    newfil(fn); /* get/renew file entry */
    pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
    opnfil[fn]->net = true; /* set network (sockets) file */
    opnfil[fn]->sec = false; /* set not secure */
    opnfil[fn]->opn = true;
    pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

    /* check secure sockets layer, and negotiate if so */
    if (secure) {

        /* to keep ssl handling from looping, we create a shadow fid so we
           can talk to the underlying socket */
        sfn = dup(fn); /* create second side for ssl operations */
        if (sfn == -1) error(enodupf);
        if (sfn < 0 || sfn >= MAXFIL) error(einvhan); /* invalid file handle */
        newfil(sfn); /* init the shadow */

        pthread_mutex_lock(&opnfil[sfn]->lock); /* take file entry lock */
        opnfil[sfn]->net = true; /* set network file */
        opnfil[sfn]->opn = true;
        pthread_mutex_unlock(&opnfil[sfn]->lock); /* release file entry lock */

        ssl = SSL_new(client_ctx); /* create new ssl */
        if (!ssl) error(esslnew);
        /* connect the ssl side to the shadow fid */
        r = SSL_set_fd(ssl, sfn); /* connect to fid */
        if (!r) error(esslfid);

        /* initiate tls handshake */
        r = SSL_connect(ssl);
        if (r != 1) sslerror(ssl, r);

        /* Get the remote certificate into the X509 structure.
           Right now we don't do anything with this (don't verify it) */
        cert = SSL_get_peer_certificate(ssl);
        if (!cert) error(esslcer);

        /* Update to file entry. The semantics of socket() and dup() dictate
           that no other open takes the fid, but I prefer to update the file
           entry atomically. */
        pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
        opnfil[fn]->sfn = sfn;
        opnfil[fn]->cert = cert;
        opnfil[fn]->ssl = ssl;
        opnfil[fn]->sec = true; /* turn TLS encode/decode on for ssl channel */
        pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

    }

    return (fp);

}

/*******************************************************************************

Open message file

Opens a message file with the given address, port and security. The file can be
both written and read. The protocol used for the transfer is either UDP, with
fixed length messages.

*******************************************************************************/

int openmsg(unsigned long addr, int port, boolean secure)

{

}

/*******************************************************************************

Get maximum message size

Returns the maximum size of messages in the system. This is typically 1500, or
the maximum size of a UDP message, but can be larger, a so called "jumbo packet"
at 64kb.

*******************************************************************************/

int pa_maxmsg(void)

{

    return (1500); /* MTU */

}

/*******************************************************************************

Write message to message file

Writes a message to the given message file. The message file must be open. Any
size (including 0) up to pa_maxmsg() is allowed.

*******************************************************************************/

void pa_wrmsg(int fn, byte* msg, int len)

{

}

/*******************************************************************************

Read message from message file

Reads a message from the message file, of any length up to and including the
specified buffer length. The actual length transferred is returned. The length
of the buffer should be equal to maxmsg to pass all possible messages, unless it
is known that a given message size will never be exceeded.

*******************************************************************************/

int pa_rdmsg(int f, byte* msg, int len)

{

}

/*******************************************************************************

Close message file

Closes the given message file.

*******************************************************************************/

void pa_clsmsg(int f)

{

}

/*******************************************************************************

Wait external network connection

Waits for an external socket connection on a given port address. If an external
client connects to that port, then a socket file is opened and the file number
returned. Note that any number of such connections can be active at one time.
The program can invoke multiple tasks to handle each connection. If another
program tries to take the same port, it is blocked.

*******************************************************************************/

FILE* pa_waitconn(/* port number to wait on */ int port,
                  /* secure mode */            boolean secure
                 )

{

    struct sockaddr_in saddr;
    int sfn, fn;
    int r;
    FILE* fp;
    int opt;
    SSL*  ssl;
    X509* cert;

    /* connect the socket */
    sfn = socket(AF_INET, SOCK_STREAM, 0);
    if (sfn < 0) linuxerror();
    if (sfn < 0 || sfn >= MAXFIL) error(einvhan); /* invalid file handle */
    newfil(sfn); /* clear the fid entry */

    /* set socket options, multiple servers on address and same port */
    opt = 1;
    r = setsockopt(sfn, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt));
    if (r < 0) linuxerror();

    /* set up address */
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);
    r = bind(sfn, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r < 0) linuxerror();

    /* wait on port */
    r = listen(sfn, 3);
    if (r < 0) linuxerror();

    /* accept connection, discard peer address */
    fn = accept(sfn, NULL, NULL);
    if (r < 0) linuxerror();
    if (sfn < 0 || sfn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* discard server port */
    close(sfn);

    /* set up as FILE* accessable */
    fp = fdopen(fn, "r+");
    if (!fp) linuxerror();
    newfil(fn); /* initialize file entry */
    opnfil[fn]->net = true; /* set network (sockets) file */

    /* check secure sockets layer, and negotiate if so */
    if (secure) {

        /* to keep ssl handling from looping, we create a shadow fid so we
           can talk to the underlying socket */
        sfn = dup(fn); /* create second side for ssl operations */
        if (sfn == -1) error(enodupf);
        if (sfn < 0 || sfn >= MAXFIL) error(einvhan); /* invalid file handle */
        newfil(sfn); /* init the shadow */

        pthread_mutex_lock(&opnfil[sfn]->lock); /* take file entry lock */
        opnfil[sfn]->net = true; /* set network file */
        opnfil[sfn]->opn = true;
        pthread_mutex_unlock(&opnfil[sfn]->lock); /* release file entry lock */

        ssl = SSL_new(server_ctx); /* create new ssl */
        if (!ssl) error(esslnew);
        /* connect the ssl side to the shadow fid */
        r = SSL_set_fd(ssl, sfn); /* connect to fid */
        if (!r) error(esslfid);

        /* perform ssl server accept */
        r = SSL_accept(ssl);
        if (r <= 0) sslerrorqueue();

        /* Update to file entry. The semantics of socket() and dup() dictate
           that no other open takes the fid, but I prefer to update the file
           entry atomically. */
        pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
        opnfil[fn]->sfn = sfn;
        opnfil[fn]->cert = cert;
        opnfil[fn]->ssl = ssl;
        opnfil[fn]->sec = true; /* turn TLS encode/decode on for ssl channel */
        pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

    }

    return (fp);

}

/*******************************************************************************

Message files are reliable

Returns true if the message files on this host are implemented with guaranteed
delivery and in order. This is a property of high performance compute clusters,
that the delivery of messages are guaranteed to arrive error free at their
destination. If this property appears, the program can skip providing it's own
retry or other error handling system.

*******************************************************************************/

boolean pa_relymsg(void)

{

    return (false);

}

/*******************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement stdio:

read
write
open
close
lseek

We use interdiction to filter standard I/O calls towards the terminal. The
0 (input) and 1 (output) files are interdicted. In ANSI terminal, we act as a
filter, so this does not change the user ability to redirect the file handles
elsewhere.

*******************************************************************************/

/*******************************************************************************

Open

Terminal is assumed to be opened when the system starts, and closed when it
shuts down. Thus we do nothing for this.

*******************************************************************************/

static int ivopen(popen_t opendc, const char* pathname, int flags, int perm)

{

    int r;

    /* open with passdown */
    r = (*opendc)(pathname, flags, perm);
    if (r >= 0) {

    	if (r < 0 || r > MAXFIL) error(einvhan); /* invalid file handle */
    	if (opnfil[r]) { /* if not tracked, don't touch it */

    	    makfil(r); /* create file entry as required */
    	    /* open to close arguments on opposing threads can leave the open
    	       indeterminate, but this is just a state issue. */
    	    opnfil[r]->opn = true; /* set open */

    	}

    }

    return (r);

}

static int iopen(const char* pathname, int flags, int perm)

{

    ivopen(ofpopen, pathname, flags, perm);

}

static int iopen_nocancel(const char* pathname, int flags, int perm)

{

    ivopen(ofpopen_nocancel, pathname, flags, perm);

}

/*******************************************************************************

Close

Does nothing but pass on.

*******************************************************************************/

static int ivclose(pclose_t closedc, int fd)

{

    filrec fr; /* shadow of file entries */
    int r;

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */

    if (opnfil[fd]) { /* if not tracked, don't touch it */

        makfil(fd); /* create file entry as required */
        pthread_mutex_lock(&opnfil[fd]->lock); /* acquire lock */
        /* copy entry out and clear original. This keeps us from traveling with
           the lock and means we own the file data as locals */
        memcpy(&fr, opnfil[fd], sizeof(filrec));
        opnfil[fd]->ssl = NULL;
        opnfil[fd]->cert = NULL;
        opnfil[fd]->sfn = -1;
        pthread_mutex_unlock(&opnfil[fd]->lock); /* release lock */
        if (fr.sec) {

            if (fr.ssl) SSL_free(fr.ssl); /* free the ssl */
            if (fr.cert) X509_free(fr.cert); /* free certificate */
            (*closedc)(fr.sfn); /* close the shadow as well */

        }

    }
    r = (*closedc)(fd); /* normal file and socket close */

    /* open to close arguments on opposing threads can leave the open
       indeterminate, but this is just a state issue. */
    if (opnfil[fd]) opnfil[fd]->opn = false;

    return (r);

}

static int iclose(int fd)

{

    ivclose(ofpclose, fd);

}

static int iclose_nocancel(int fd)

{

    ivclose(ofpclose_nocancel, fd);

}

/*******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

*******************************************************************************/

static ssize_t ivread(pread_t readdc, int fd, void* buff, size_t count)

{

    ssize_t r;

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */

    if (opnfil[fd]) { /* if not tracked, don't touch it */

        makfil(fd); /* create file entry as required */
        if (opnfil[fd]->sec)
            /* perform ssl decoded read */
            r = SSL_read(opnfil[fd]->ssl, buff, count);
        else r = (*readdc)(fd, buff, count); /* standard file and socket read */

    } else r = (*readdc)(fd, buff, count); /* standard file and socket read */

    return (r);

}

static ssize_t iread(int fd, void* buff, size_t count)

{

    ivread(ofpread, fd, buff, count);

}

static ssize_t iread_nocancel(int fd, void* buff, size_t count)

{

    ivread(ofpread_nocancel, fd, buff, count);

}

/*******************************************************************************

Write

*******************************************************************************/

static ssize_t ivwrite(pwrite_t writedc, int fd, const void* buff, size_t count)

{

    ssize_t r;

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */

    if (opnfil[fd]) { /* if not tracked, don't touch it */

        makfil(fd); /* create file entry as required */
        if (opnfil[fd]->sec)
            /* perform ssl encoded write */
            r = SSL_write(opnfil[fd]->ssl, buff, count);
        else
            /* standard file and socket write */
            r = (*writedc)(fd, buff, count);

    } else
        /* standard file and socket write */
        r = (*writedc)(fd, buff, count);

    return (r);

}

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    ivwrite(ofpwrite, fd, buff, count);

}

static ssize_t iwrite_nocancel(int fd, const void* buff, size_t count)

{

    ivwrite(ofpwrite_nocancel, fd, buff, count);

}

/*******************************************************************************

Lseek

Lseek is never possible on a terminal, so this is always an error on the stdin
or stdout handle.

*******************************************************************************/

static off_t ivlseek(plseek_t lseekdc, int fd, off_t offset, int whence)

{

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */

    return (*lseekdc)(fd, offset, whence);

}

static off_t ilseek(int fd, off_t offset, int whence)

{

    ivlseek(ofplseek, fd, offset, whence);

}

/*******************************************************************************

Network startup

*******************************************************************************/

static void pa_init_network (void) __attribute__((constructor (103)));
static void pa_init_network()

{

    int fi;
    int r;

    frefil = NULL; /* clear free files list */
    /* clear open files table */
    for (fi = 0; fi < MAXFIL; fi++) opnfil[fi] = NULL;

    /* initialize the open files lock */
    pthread_mutex_init(&oflock, NULL);
    /* initialize the free files lock */
    pthread_mutex_init(&fflock, NULL);

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_read_nocancel(iread_nocancel, &ofpread_nocancel);
    ovr_write(iwrite, &ofpwrite);
    ovr_write(iwrite_nocancel, &ofpwrite_nocancel);
    ovr_open(iopen, &ofpopen);
    ovr_open(iopen_nocancel, &ofpopen_nocancel);
    ovr_close(iclose, &ofpclose);
    ovr_close(iclose_nocancel, &ofpclose_nocancel);
    ovr_lseek(ilseek, &ofplseek);

    /* initialize SSL library and register algorithms */
    if(SSL_library_init() < 0) error(einissl);

    /* create new client SSL context */
    client_ctx = SSL_CTX_new(TLS_client_method());
    //client_ctx = SSL_CTX_new(TLS_server_method());
    if (!client_ctx) error(esslctx);

    /* create new server SSL context */
    server_ctx = SSL_CTX_new(TLS_server_method());
    if (!server_ctx) error(esslctx);

    /* configure server context */
    SSL_CTX_set_ecdh_auto(server_ctx, 1);

    /* Set the server key and cert */
    r = SSL_CTX_use_certificate_file(server_ctx, "cert.pem", SSL_FILETYPE_PEM);
    if (r <= 0) sslerrorqueue();

    r = SSL_CTX_use_PrivateKey_file(server_ctx, "key.pem", SSL_FILETYPE_PEM);
    if (r <= 0) sslerrorqueue();

};

/*******************************************************************************

Netlib shutdown

*******************************************************************************/

static void pa_deinit_network (void) __attribute__((destructor (103)));
static void pa_deinit_network()

{

    int fi;

    /* holding copies of system vectors */
    pread_t cppread;
    pread_t cppread_nocancel;
    pwrite_t cppwrite;
    pwrite_t cppwrite_nocancel;
    popen_t cppopen;
    popen_t cppopen_nocancel;
    pclose_t cppclose;
    pclose_t cppclose_nocancel;
    plseek_t cpplseek;

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_read(ofpread_nocancel, &cppread_nocancel);
    ovr_write(ofpwrite, &cppwrite);
    ovr_write(ofpwrite_nocancel, &cppwrite_nocancel);
    ovr_open(ofpopen, &cppopen);
    ovr_open(ofpopen_nocancel, &cppopen_nocancel);
    ovr_close(ofpclose, &cppclose);
    ovr_close(ofpclose_nocancel, &cppclose_nocancel);
    ovr_lseek(ofplseek, &cpplseek);

    /* close out open files and release space */
    for (fi = 0; fi < MAXFIL; fi++) if (opnfil[fi]) {

        pthread_mutex_destroy(&opnfil[fi]->lock);
        if (opnfil[fi]->opn) close(fi);
        if (opnfil[fi]->ssl) SSL_free(opnfil[fi]->ssl);
        if (opnfil[fi]->cert) X509_free(opnfil[fi]->cert);
        free(opnfil[fi]);

    }
    /* free context structure */
    SSL_CTX_free(client_ctx);

    /* release the open files lock */
    pthread_mutex_destroy(&oflock);
    /* release the free files lock */
    pthread_mutex_destroy(&fflock);

}
