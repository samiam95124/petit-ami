/*******************************************************************************
*                                                                              *
*                      Standard definitions for Petit_ami                      *
*                                                                              *
* In most, but not all cases, C will simply treat duplicate definitions as a   *
* no-op if they are identical (the present exception being boolean). However,  *
* for the cases that is not so, we present standard used definitions here.     *
*                                                                              *
*******************************************************************************/

#ifndef __STDDEF_H__
#define __STDDEF_H__

typedef char* string;  /* general string type */
typedef enum { false, true } boolean; /* boolean */
typedef unsigned char byte; /* byte */

#endif /* __STDDEF_H__ */
