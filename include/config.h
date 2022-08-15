/**//***************************************************************************

Parse config file

Parses config files into a tree structured database.

*******************************************************************************/

#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <localdefs.h>

/* Tree structured value record */

typedef struct pa_value {

    struct pa_value* next;    /* next value in list */
    struct pa_value* sublist; /* new begin/end block */
    string name;           /* name of node */
    string value;          /* value of this node */

} pa_value, *pa_valptr;

void pa_prttre(pa_valptr list); /* print tree structured config list */
pa_valptr pa_schlst(string id, pa_valptr root); /* search config list */
void pa_merge(pa_valptr* root, pa_valptr newroot); /* merge config trees */
void pa_configfile(string fn, pa_valptr* root); /* parse config file */
void pa_config(pa_valptr* root); /* parse config files */

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H__ */
