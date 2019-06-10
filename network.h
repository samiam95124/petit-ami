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
*******************************************************************************/

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdio.h>

typedef char* string;  /* general string type */

void opennet(FILE* infile, FILE* outfile, int addr, int port);
void addrnet(string name, int addr);

#endif /* __NETWORK_H__ */
