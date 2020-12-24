/**//***************************************************************************

Parse config file

Petit-ami pulls config files in plain text, in the order PUD or "Program, User,
current directory".
The file name is "petit_ami.cfg" or ".petit_ami.cfg". The first is visible, the
second is not. They are looked for in that order. Then, the order is to search:

Program:           The location of the program that includes petit_ami, that is,
                   the location of the binary.
User:              The location of the user main directory.
Current directory: The current directory.

Each of these are searched in the order given. Each petit_ami.cfg that is found
can have it's values overwritten by the new file, starting (typically) with an
empty config value tree. In this manner, the program installation point has the
master values, the user can override these values for their purposes, and
finally, a petit_ami.cfg file in the local directory can override that.

Petit_ami values are tree structured. The syntax of a petit_ami.cfg file is:

# comment
[<values>]...

<values> = begin <symbol>
                [<symbol> [<value>]]...
           end

<symbol> = a...z | A..Z | _ [a..z | A..Z | _ 0..9]...
<value> = ~ (space)

Each element of the .cfg file is on a separate line. Comment lines can be
interspersed in the file. The value of a symbol is the entire line contents
after the definition, up to eoln, minus the space that separates the symbol
from it's value. The value can be blank. This is common and means that the
appearance of the symbol itself is important (a flag).

A begin block will always have a symbol associated with it. Begin/end blocks can
nest to any level. A begin block can contain any number of values, which exist
as symbol/value pairs (including none). Thus a block can contain any number of
values or nested blocks, in any order. The block symbol has no value.

The values that are entered outside of any "begin"/"end" block have no sublist
name, and it is up to the client program to determine what this means.

Config values are passed via a tree structure:

typedef struct value {

    struct value next; /* next value in list */
    struct value sublist; /* new begin/end block */
    string name; /* name of node */
    string value; /* value of this node */

} value, *valptr;

Each node can be either a block (begin/end) or a value, or both. However, in the
current implementation blocks cannot also have values.

To read config files, the existing tree is passed, which may be populated or
empty. The new values replace the old values, which are free'ed if they are
replaced. Node types cannot be changed (symbol to block or vice versa), that is
an error.

Thus typically the tree is passed NULL on first call, then for each new file
read, the values are overwritten.

Once a config tree is read, it is typically maintained by the using program,
then disposed on exit. It can be locally edited. Thus the calling program can
either provide values before the config reads, in which case they will be
overwritten, or after the reads, in which case the app is effectively
overriding, or even editing the values.

The entire tree is typically considered (but not exclusively named) petit_ami.
The first subtrees are each module, such as graphics, sound, etc. Some modules
further divide the blocks from there, such as the sound module, which maintains
a block for each plugin.

*******************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <localdefs.h>
#include <services.h>

#define MAXSTR 250
#define MAXID  20

typedef struct value {

    struct value  next;    /* next value in list */
    struct value* sublist; /* new begin/end block */
    string name;           /* name of node */
    string value;          /* value of this node */

} value, *valptr;

/**//***************************************************************************

Process error

*******************************************************************************/

static void error(
    /* filename */      string fn,
    /* line count */    int lc,
    /* error message */ string es
}

{

    fprintf(stderr, "*** Config: File: %s Line: %d %s\n", fn, lc, es);
    exit(1);

}

/**//***************************************************************************

Get label from string

Parses a label from the string. Leading spaces are discarded. The format of the
label is:

    a...z | A..Z | _ [a..z | A..Z | _ | 0..9]...

The string pointer is advanced past the label.

*******************************************************************************/

static void parlab(
    /* source string to parse */ string* s,
    /* resulting id */           string  w,
}

{

    int wi;

    /* get id off line */
    wi = 0; /* set first character */
    while (isalnum(**s) || **s = '_') {

        if (wi > MAXID-1) {

            fprintf(stderr, "*** Config: File: %s Line: %d id too long\n", fn,
                    *lc);
            exit(1);

        }
        word[wi++] = (**s)++;

    }
    if (!wi) { /* no label found */

        fprintf(stderr, "*** Config: File: %s Line: %d missing id\n", fn, *lc);
        exit(1);

    }

}

/**//***************************************************************************

Parse config list

Parses a linear list of configuration lines.

*******************************************************************************/

static void parlst(
    /* name of file */        string fn,
    /* file handle */         FILE F,
    /* line counter */        int* lc,
    /* root of config tree */ valptr* root
)

{

    /* id buffer */                 char   word[MAXID];
    /* line buffer */               char   linbuf[MAXSTR];
    /* line buffer pointer */       char*  s;
    /* value constructor pointer */ valptr vp;
    /* end of block */              int    end;

    p = fgets(linbuf, MAXSTR, f); /* get next line */
    (*lc)++; /* increment line counter */
    end = FALSE; /* set not at end */
    if (p && !end) { /* not eof */

        s = linbuf; /* index line */
        while (isspace(*s)) s++; /* skip any spaces */
        if (*s && *s != '\n') { /* ignore blank lines */

            if (*s == "#") { /* comment */

                while (*s) s++; /* skip over line */

            } else if (isalnum(*s) || *s = '_') {

                parlab(&s, &word); /* get id off line */
                if (!strcmp(word, "begin")) {

                    /* nested sublist */
                    parlab(&s, &word); /* get symbol */
                    /* get sublist constructor */
                    vp = maloc(sizeof(value));
                    if (!vp) error(fn, *lc, "Out of memory");
                    /* push to list */
                    vp->next = *valptr;
                    *valptr = vp;
                    vp->sublist = NULL; /* set no subtree */
                    strcpy(vp->name, word); /* copy id into place */
                    /* parse sublist */
                    parlst(fn, f, lc, &(vp->sublist));

                } else if (!strcmp(word, "end")
                    /* end of list, we simply exit */
                    end = TRUE;
                else {

                    if (isspace(*s)) s++; /* skip space as separator */
                    /* valid id found, construct value entry */
                    vp = maloc(sizeof(value));
                    if (!vp) error(fn, *lc, "Out of memory");
                    /* push to list */
                    vp->next = *valptr;
                    *valptr = vp;
                    vp->sublist = NULL; /* set no subtree */
                    strcpy(vp->name, word); /* copy id into place */
                    strcpy(vp->value, s); /* copy value into place */

                }

            } else error(fn, *lc, "Missing id");

        }

    }

}

/**//***************************************************************************

Parse config tree from filename

Parses a configuration tree from the given filename/path to the given root tree.
The values are merged with the contents of the root passed.

This routine can be called directly to use alternative config file names.

*******************************************************************************/

void _pa_configfile(string fn, valptr* root)

{

    /* file id */          FILE*  f;
    /* line counter */     int    lc;
    /* new root pointer */ valptr np;

    f = fopen(fn, "r");
    lc = 0; /* clear line count */
    if (f) { /* file exists/can be opened */

        while (!feof()) {

            /* parse list of values */
            parlst(fn, f, &lc, &np);

        }
        fclose(f);

    }
    /* now merge old and new trees */
    merge(root, np);

}

/**//***************************************************************************

Parse config

This is the standard Petit-ami configuration parse. It visits each of the
standard PA configuration files in the order:

Program path:

These are typically fixed definitions. Since multiple users can use the same
program, these are options that are expected to be set for all invocations of
the program.

User path:

Holds settings for the current user.

Current directory:

Holds options to only be applied locally.

The files in each directory are:

petit_ami.cfg   The visible config file.
.petit_ami.cfg  The invisible config file.

Each new tree is merged with the last, with any duplicate values replacing the
previous ones. Thus user definitions can override program definitions, local
defintions can override those, etc.

If a root with existing definitions is passed in, the files here will be merged
with those. You can also perform a merge with other definitions after this
call. Thus you can either put extra definitions that will be overridden, or
new definitions that will override.

*******************************************************************************/

void _pa_config(valptr* root)

{

    char pgmpth[MAXSTR];
    char filnam[MAXSTR];

    /* config from program path */
    pa_getgm(pgmpth, MAXSTR); /* get program path */
    /* try both visible and invisible names */
    pa_maknam(filnam, MAXSTR, pgmpth, "petit_ami", "cfg");
    configfile(name, root);
    pa_maknam(filnam, MAXSTR, pgmpth, ".petit_ami", "cfg");
    configfile(name, root);

    /* config from user path */
    pa_getusr(pgmpth, MAXSTR); /* get user path */
    /* try both visible and invisible names */
    pa_maknam(filnam, MAXSTR, pgmpth, "petit_ami", "cfg");
    configfile(name, root);
    pa_maknam(filnam, MAXSTR, pgmpth, ".petit_ami", "cfg");
    configfile(name, root);

    /* config from current directory */
    pa_getcur(pgmpth, MAXSTR); /* get current path */
    /* try both visible and invisible names */
    pa_maknam(filnam, MAXSTR, pgmpth, "petit_ami", "cfg");
    configfile(name, root);
    pa_maknam(filnam, MAXSTR, pgmpth, ".petit_ami", "cfg");
    configfile(name, root);

}
