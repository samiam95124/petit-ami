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
* normal C read and write statements are used to access it.                    *
*                                                                              *
* Also implement message or fixed length packet service for use in high        *
* performance computing and applications that need to determine their own      *
* reliable delivery protocols like audio and video data.                       *
*                                                                              *
* The linux version, and in fact probably all versions, rely on stacking atop  *
* openssl.                                                                     *
*                                                                              *
*******************************************************************************/

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdio.h>
#include <localdefs.h>

/* name - value pair list */
typedef struct pa_certfield {

    string               name;  /* name of field */
    string               data;  /* content of field */
    struct pa_certfield* fork; /* sublist */
    struct pa_certfield* next; /* next entry in list */

} pa_certfield, *pa_certptr;

void pa_addrnet(string name, unsigned long* addr);
void pa_addrnetv6(string name, unsigned long long* addrh,
                unsigned long long* addrl);
FILE* pa_opennet(unsigned long addr, int port, boolean secure);
FILE* pa_opennetv6(unsigned long long addrh, unsigned long long addrl,
                   int port, boolean secure);
int pa_maxmsg(void);
boolean pa_relymsg(unsigned long addr);
boolean pa_relymsgv6(unsigned long long addrh, unsigned long long addrl);
int pa_openmsg(unsigned long addr, int port, boolean secure);
int pa_openmsgv6(unsigned long long addrh, unsigned long long addrl, int port,
                 boolean secure);
void pa_wrmsg(int fn, void* msg, unsigned long len);
int pa_rdmsg(int fn, void* msg, unsigned long len);
void pa_clsmsg(int f);
FILE* pa_waitnet(int port, boolean secure);
int pa_waitmsg(int port, boolean secure);
int pa_certnet(FILE* f, int which, string cert, int len);
int pa_certmsg(int fn, int which, string cert, int len);
void pa_certlistnet(FILE *f, int which, pa_certptr* list);
void pa_certlistmsg(int fn, int which, pa_certptr* list);
void pa_certcachenet(FILE *f);
void pa_certcachemsg(int fn);
boolean pa_certtestnet(FILE *f);
boolean pa_certtestmsg(int fn);

#endif /* __NETWORK_H__ */
