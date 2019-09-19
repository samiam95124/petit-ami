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

typedef char* string;  /* general string type */

unsigned long pa_addrnet(string name);
FILE* pa_opennet(unsigned long addr, int port, boolean secure);
int pa_maxmsg(void);
boolean pa_relymsg(void);
void pa_openmsg(int f, int addr, int port);
void pa_wrtmsg(int f, byte* msg, int len);
int pa_rdmsg(int f, byte* msg, int len);
void pa_clsmsg(int f);
FILE* pa_waitconn(void);

#endif /* __NETWORK_H__ */
