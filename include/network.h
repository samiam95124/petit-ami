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
* Also implement message or fixed length packet service for use in high        *
* performance computing and applications that need to determine their own      *
* reliable delivery protocols like audio and video data.                       *
*                                                                              *
* String output buffers are "critical": a result may occupy the entire        *
* buffer, in which case the terminating zero is left off. Results shorter     *
* than the buffer are zero terminated. C callers should use length limited    *
* reads of such buffers (see strnlen and similar).                            *
*                                                                              *
* The Linux version, and in fact probably all versions, rely on stacking atop  *
* openssl.                                                                     *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
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
*******************************************************************************/

#ifndef __NETWORK_H__
#define __NETWORK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <localdefs.h>

/* name - value pair list */
typedef struct ami_certfield {

    string               name;     /* name of field */
    string               data;     /* content of field */
    ami_long          critical; /* is a critical X509 field */
    struct ami_certfield* fork;     /* sublist */
    struct ami_certfield* next;     /* next entry in list */

} ami_certfield, *ami_certptr;

void ami_addrnet(const string name, ami_ulong* addr);
void ami_addrnetv6(const string name, unsigned long long* addrh,
                  unsigned long long* addrl);
FILE* ami_opennet(ami_ulong addr, ami_long port, ami_long secure);
FILE* ami_opennetv6(unsigned long long addrh, unsigned long long addrl,
                   ami_long port, ami_long secure);
ami_long ami_maxmsg(ami_ulong addr, ami_long secure);
ami_long ami_maxmsgv6(unsigned long long addrh, unsigned long long addrl,
                  ami_long secure);
ami_long ami_relymsg(ami_ulong addr);
ami_long ami_relymsgv6(unsigned long long addrh, unsigned long long addrl);
ami_long ami_openmsg(ami_ulong addr, ami_long port, ami_long secure);
ami_long ami_openmsgv6(unsigned long long addrh, unsigned long long addrl, ami_long port,
                 ami_long secure);
void ami_wrmsg(ami_long fn, void* msg, ami_ulong len);
ami_long ami_rdmsg(ami_long fn, void* msg, ami_ulong len);
/* Is a message waiting on the channel? Waits up to usec microseconds for
   one, zero to poll. Nonzero when a read would not block. */
ami_long ami_rdymsg(ami_long fn, ami_long usec);
/* Bound the reads on a message channel: one that waits longer than usec
   microseconds fails. Zero clears the bound, for a channel that is kept. */
void ami_tmomsg(ami_long fn, ami_long usec);
/* Size the receive buffering on a message channel, in bytes. */
void ami_bufmsg(ami_long fn, ami_long len);
/* Shut a message channel under its reader: a read blocked on it fails.
   The channel still wants closing. */
void ami_shutmsg(ami_long fn);
void ami_clsmsg(ami_long fn);
FILE* ami_waitnet(ami_long port, ami_long secure);
/* Install a network error handler. Called with the error text before
   the abort; a handler that longjmps takes the error, one that returns
   lets the abort proceed. A server that recycles failed connections
   catches channel deaths this way: a vanished secure peer surfaces as
   an error where a clear channel just falls silent. */
typedef void (*ami_neterrhan_t)(const char* es);
void ami_neterror(ami_neterrhan_t handler);
ami_long ami_waitmsg(ami_long port, ami_long secure);
ami_long ami_certnet(FILE* f, ami_long which, string cert, ami_long len);
ami_long ami_certmsg(ami_long fn, ami_long which, string cert, ami_long len);
void ami_certlistnet(FILE *f, ami_long which, ami_certptr* list);
void ami_certlistmsg(ami_long fn, ami_long which, ami_certptr* list);
void ami_certlistfree(ami_certptr* list);

#ifdef __cplusplus
}
#endif

#endif /* __NETWORK_H__ */
