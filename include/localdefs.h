/*******************************************************************************
*                                                                              *
*                      Standard definitions for Petit_ami                      *
*                                                                              *
* In most, but not all cases, C will simply treat duplicate definitions as a   *
* no-op if they are identical (the present exception being boolean). However,  *
* for the cases that is not so, we present standard used definitions here.     *
*                                                                              *
*******************************************************************************/

#ifndef __LOCALDEFS_H__
#define __LOCALDEFS_H__

#define FALSE 0
#define TRUE  1

#define BIT(b) (1<<b) /* set bit from bit number */
#define BITMSK(b) (~BIT(b)) /* mask out bit number */

typedef char* string;  /* general string type */
typedef unsigned char byte; /* byte */

#endif /* __LOCALDEFS_H__ */
