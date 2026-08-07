/** ****************************************************************************
 *
 * Network library interface C++ wrapper declarations
 *
 * See cpp/network.cpp for the rationale. The certificate field record
 * appears here with the ami_ prefix stripped, laid out identically to
 * its C form: the wrapper casts between them.
 *
 ******************************************************************************/

#ifndef __NETWORK_HPP__
#define __NETWORK_HPP__

extern "C" {

#include <stdio.h>

}

namespace network {

/* name - value pair list */
typedef struct certfield {

    char*             name;     /* name of field */
    char*             data;     /* content of field */
    long              critical; /* is a critical X509 field */
    struct certfield* fork;     /* sublist */
    struct certfield* next;     /* next entry in list */

} certfield, *certptr;

/* procedures and functions */
void  addrnet(const char* name, unsigned long* addr);
void  addrnetv6(const char* name, unsigned long long* addrh,
                unsigned long long* addrl);
FILE* opennet(unsigned long addr, long port, long secure = 0);
FILE* opennetv6(unsigned long long addrh, unsigned long long addrl,
                long port, long secure = 0);
long  maxmsg(unsigned long addr);
long  maxmsgv6(unsigned long long addrh, unsigned long long addrl);
long  relymsg(unsigned long addr);
long  relymsgv6(unsigned long long addrh, unsigned long long addrl);
long  openmsg(unsigned long addr, long port, long secure = 0);
long  openmsgv6(unsigned long long addrh, unsigned long long addrl,
                long port, long secure = 0);
void  wrmsg(long fn, void* msg, unsigned long len);
long  rdmsg(long fn, void* msg, unsigned long len);
void  clsmsg(long fn);
FILE* waitnet(long port, long secure = 0);
long  waitmsg(long port, long secure = 0);
long  certnet(FILE* f, long which, char* cert, long len);
long  certmsg(long fn, long which, char* cert, long len);
void  certlistnet(FILE* f, long which, certptr* list);
void  certlistmsg(long fn, long which, certptr* list);
void  certlistfree(certptr* list);

/*******************************************************************************

The net and msg objects

A net holds one stream connection, a msg one message channel. Opening
sets what the object holds -- as a client by address or, for a net, by
name, with the lookup made on the way -- or as a server with the wait
call. Every call after that leaves the connection off, and the
destructor closes whatever is still held, so a connection cannot leak
past an early return.

A net converts to FILE* wherever one is wanted, so the stdio calls
read and write it directly: fgets(buff, len, n) reads from the
connection n holds. Close it with the object -- close() or the
destructor -- never with fclose through the converted pointer, or the
destructor would close it a second time.

These hold handles, not hooks: any number of them may exist.

*******************************************************************************/

class net {

FILE* f; /* the connection held, NULL when closed */

public:

/* constructor */
net();

/* destructor */
~net();

/* methods */
void opennet(unsigned long addr, long port, long secure = 0);
void opennet(const char* name, long port, long secure = 0);
void opennetv6(unsigned long long addrh, unsigned long long addrl,
               long port, long secure = 0);
void opennetv6(const char* name, long port, long secure = 0);
void waitnet(long port, long secure = 0);
void close(void);
long certnet(long which, char* cert, long len);
void certlistnet(long which, certptr* list);

/* the connection, for the stdio calls */
operator FILE*(void);

}; /* class net */

class msg {

long fn;   /* the channel held */
long open; /* it is held */

public:

/* constructor */
msg();

/* destructor */
~msg();

/* methods */
void openmsg(unsigned long addr, long port, long secure = 0);
void openmsg(const char* name, long port, long secure = 0);
void openmsgv6(unsigned long long addrh, unsigned long long addrl,
               long port, long secure = 0);
void openmsgv6(const char* name, long port, long secure = 0);
void waitmsg(long port, long secure = 0);
void wrmsg(void* buff, unsigned long len);
long rdmsg(void* buff, unsigned long len);
void clsmsg(void);
long certmsg(long which, char* cert, long len);
void certlistmsg(long which, certptr* list);

}; /* class msg */

} /* namespace network */

#endif /* __NETWORK_HPP__ */
