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
*******************************************************************************/

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdio.h>
#include <localdefs.h>

typedef char* string;  /* general string type */

void pa_addrnet(string name, unsigned long* addr);
void pa_addrnetv6(string name, unsigned long long* addrh,
                unsigned long long* addrl);
FILE* pa_opennet(unsigned long addr, int port, boolean secure);
FILE* pa_opennetv6(unsigned long long addrh, unsigned long long addrl,
                   int port, boolean secure);
int pa_maxmsg(void);
boolean pa_relymsg(void);
void pa_openmsg(int f, unsigned long addr, int port);
void pa_openmsgv6(int f, unsigned long long addrh, unsigned long long addrl,
                  int port);
void pa_wrmsg(int f, byte* msg, int len);
int pa_rdmsg(int f, byte* msg, int len);
void pa_clsmsg(int f);
FILE* pa_waitconn(int port, boolean secure);

#endif /* __NETWORK_H__ */
