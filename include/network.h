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

#include <stdio.h>
#include <localdefs.h>

/* name - value pair list */
typedef struct pa_certfield {

    string               name;     /* name of field */
    string               data;     /* content of field */
    int              critical; /* is a critical X509 field */
    struct pa_certfield* fork;     /* sublist */
    struct pa_certfield* next;     /* next entry in list */

} pa_certfield, *pa_certptr;

void pa_addrnet(const string name, unsigned long* addr);
void pa_addrnetv6(const string name, unsigned long long* addrh,
                  unsigned long long* addrl);
FILE* pa_opennet(unsigned long addr, int port, int secure);
FILE* pa_opennetv6(unsigned long long addrh, unsigned long long addrl,
                   int port, int secure);
int pa_maxmsg(unsigned long addr);
int pa_maxmsgv6(unsigned long long addrh, unsigned long long addrl);
int pa_relymsg(unsigned long addr);
int pa_relymsgv6(unsigned long long addrh, unsigned long long addrl);
int pa_openmsg(unsigned long addr, int port, int secure);
int pa_openmsgv6(unsigned long long addrh, unsigned long long addrl, int port,
                 int secure);
void pa_wrmsg(int fn, void* msg, unsigned long len);
int pa_rdmsg(int fn, void* msg, unsigned long len);
void pa_clsmsg(int f);
FILE* pa_waitnet(int port, int secure);
int pa_waitmsg(int port, int secure);
int pa_certnet(FILE* f, int which, string cert, int len);
int pa_certmsg(int fn, int which, string cert, int len);
void pa_certlistnet(FILE *f, int which, pa_certptr* list);
void pa_certlistmsg(int fn, int which, pa_certptr* list);
void pa_certlistfree(pa_certptr* list);

#endif /* __NETWORK_H__ */
