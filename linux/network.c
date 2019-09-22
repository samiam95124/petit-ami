/*******************************************************************************
*                                                                              *
*                           INTERNET ACCESS LIBRARY                            *
*                                                                              *
*                       Copyright (C) 2006 Scott A. Moore                      *
*                                                                              *
*                              5/06 S. A. Moore                                *
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

/* Petit-Ami definitions */
#include <localdefs.h>
#include <network.h>

#define MAXFIL 100 /* maximum number of open files */

/* File tracking.
   Files could be transparent in the case of plain text, but SSL and advanced
   layering needs special handling. So we translate the file descriptors and
   flag if they need special handling. */
typedef struct filrec {

   boolean         net;   /* it's a network file */

} filrec;
typedef filrec* filptr; /* pointer to file record */

/* error codes */
typedef enum {
    ewskini,  /* cannot initalize winsock */
    einvhan,  /* invalid file handle */
    enetopn,  /* cannot reset or rewrite network file */
    enetpos,  /* cannot position network file */
    enetloc,  /* cannot find location network file */
    enetlen,  /* cannot find length network file */
    esckeof,  /* } encountered on socket */
    efinuse,  /* file already in use */
    enetwrt,  /* Attempt to write to input side of network pair */
    esystem   /* System consistency check */
} errcod;

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef int (*punlink_t)(const char*);
typedef off_t (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_unlink(punlink_t nfp, punlink_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static punlink_t ofpunlink;
static plseek_t  ofplseek;

/*
 * Variables
 */
filrec opnfil[MAXFIL]; /* open files table */
int fi; /* index for files table */
int r; /* result */

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
 
void error(errcod e)

{

    switch (e) { /* error */

        case ewskini:  netwrterr("Cannot initalize winsock"); break;
        case einvhan:  netwrterr("Invalid file number"); break;
        case enetopn:  netwrterr("Cannot reset or rewrite network file"); break;
        case enetpos:  netwrterr("Cannot position network file"); break;
        case enetloc:  netwrterr("Cannot find location network file"); break;
        case enetlen:  netwrterr("Cannot find length network file"); break;
        case esckeof:  netwrterr("} encountered on socket"); break;
        case efinuse:  netwrterr("File already in use"); break;
        case enetwrt:  netwrterr("Attempt to write to input side of network pair"); break;
        case esystem:  netwrterr("System consistency check, please contact v}or"); break;

    }

    exit(1);

}

/*******************************************************************************

Handle Linux error

Handles error in errno. Prints the string assocated with the error.

*******************************************************************************/

void linuxerror(void)

{

    fprintf(stderr, "\nLinux Error: %s\n", strerror(errno));

}

/*******************************************************************************

Get server address

Retrives a server address by name. The name is given as a string. The address
is returned as an integer.

*******************************************************************************/

void pa_addrnet(string name, unsigned long long* addrh,
                unsigned long long* addrl)

{

	struct addrinfo *p;
	int r;

	r = getaddrinfo(name, NULL, NULL, &p);
	if (r) netwrterr(gai_strerror(r));
	/* note that if more than one address is returned for this server, we only
	   use the first one */
    if (p->ai_family == AF_INET) printf("IPv4 address\n");
    else if (p->ai_family == AF_INET6) printf("IPv6 address\n");
    else netwrterr("Cannot use this address type");
	if (p->ai_socktype != SOCK_STREAM) netwrterr("Cannot use this prototol");


}

/*******************************************************************************

Open network file

Opens a network file with the given address, port and security. The file can be
both written and read. The protocol used for the transfer is either
TCP/IP or TCP/IP with a security layer, typically TLS 1.3 at this writing.

*******************************************************************************/

FILE* pa_opennet(/* IP address high */ unsigned long long addrh,
                 /* IP address low */  unsigned long long addrl,
                 /* port */            int port,
                 /* link is secured */ boolean secure
                )

{

    struct sockaddr_in saddr;
    int fn;
    int r;
    FILE* fp;
    in_addr_t ia;

    /* not sure what is wanted here */
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(addrl);
    saddr.sin_port = htons(port);
    /* connect the socket */
    fn = socket(AF_INET, SOCK_STREAM, 0);
    if (fn < 0) linuxerror();
    r = connect(fn, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r < 0) linuxerror();
    fp = fdopen(fn, "r+");
    if (!fp) linuxerror();
    opnfil[fn].net = true;

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

}

/*******************************************************************************

Write message to message file

Writes a message to the given message file. The message file must be open. Any
size (including 0) up to pa_maxmsg() is allowed.

*******************************************************************************/

void pa_wrtmsg(int fn, byte* msg, int len)

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
The program can invoke multiple tasks to handle each connection.

*******************************************************************************/

FILE* pa_waitconn(void)

{

}

/*******************************************************************************

Message files are reliable

Returns true if the message files on this host are implemented with garanteed
delivery. This is a property of high performance compute clusters, that the
delivery of messages are garanteed to arrive error free at their destination.
If this property appears, the program can skip providing it's own retry or other
error handling system.

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
unlink
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

static int iopen(const char* pathname, int flags, int perm)

{

    /* open with passdown */
    r = (*ofpopen)(pathname, flags, perm);
    if (r >= 0)
    	if (r < 1 || r > MAXFIL) error(einvhan); /* invalid file handle */

    return (r);

}

/*******************************************************************************

Close

Does nothing but pass on.

*******************************************************************************/

static int iclose(int fd)

{

    if (fd < 1 || fd > MAXFIL) error(einvhan); /* invalid file handle */

    return (*ofpclose)(fd);

}

/*******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

*******************************************************************************/

static ssize_t iread(int fd, void* buff, size_t count)

{

    ssize_t r;

    if (fd < 1 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    r = (*ofpread)(fd, buff, count);

    return (r);

}

/*******************************************************************************

Write

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    ssize_t r;

    if (fd < 1 || fd > MAXFIL) error(einvhan); /* invalid file handle */
    r = (*ofpwrite)(fd, buff, count);

    return (r);

}

/*******************************************************************************

Unlink

Unlink has nothing to do with us, so we just pass it on.

*******************************************************************************/

static int iunlink(const char* pathname)

{

    return (*ofpunlink)(pathname);

}

/*******************************************************************************

Lseek

Lseek is never possible on a terminal, so this is always an error on the stdin
or stdout handle.

*******************************************************************************/

static off_t ilseek(int fd, off_t offset, int whence)

{

    if (fd < 1 || fd > MAXFIL) error(einvhan); /* invalid file handle */

    return (*ofplseek)(fd, offset, whence);

}

/*******************************************************************************

Network startup

*******************************************************************************/

static void pa_init_network (void) __attribute__((constructor (103)));
static void pa_init_network()

{

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
//    ovr_unlink(iunlink, &ofpunlink);
    ovr_lseek(ilseek, &ofplseek);

    /* clear open files table */
    for (fi = 0; fi < MAXFIL; fi++) opnfil[fi].net = false; /* set unoccupied */

};

/*******************************************************************************

Netlib shutdown

*******************************************************************************/

static void pa_deinit_network (void) __attribute__((destructor (103)));
static void pa_deinit_network()

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    punlink_t cppunlink;
    plseek_t cpplseek;

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
//    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);

}
