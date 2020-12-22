/**//***************************************************************************

Parse config file

Petit-ami pulls config files in plain text, in the order PUD or "Program, User,
current directory".
The file name is "petit_ami.cfg" or ".petit_ami.cfg". The first is visible, the
second is not. They are looked for in that order. Then, the order is to search:

Program: The location of the program that includes petit_ami, that is, the
         location of the binary.
User: The location of the user main directory.
Current directory: The current directory.

Each of these are searched in the order given. Each petit_ami.cfg that is found
can have it's values overritten by the new file, starting (typically) with an
empty config value tree. In this manner, the program installation point has the
master values, the user can override these values for their purposes, and
finally, a petit_ami.cfg file in the local directory can override that.

Petit_ami values are tree structured. The syntax of a petit_ami.cfg file is:

# comment
[<values>]...

<values> = begin <symbol>
                       <values> | <symbol>'='<value>
                   end

<value> = <word> | "<text>" | '<text>'
<symbol> = a...z | A..Z | _ [a..z | A..Z | _ 0..9]...
<word> = ~ (space)
<text> = Any but CR/LF

Each element of the .cfg file is on a separate line. Comments start anywhere in
the line, and terminate at the end of the line.

A begin block will always have a symbol associated with it. Begin/end blocks can
nest to any level. A begin block can contain any number of values, which exist
as symbol/value pairs. Thus a block can contain any number of values or nested
blocks, in any order.

Each value can be a space delimited run of characters. Alternately, they can be
enclosed in quotes, either single or double (but must match). In this case,
spaces do not matter and form part of the symbol value. C style escapes are
supported, and quotes can be escaped or simply use the opposite type quote.

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

typedef struct value {

    struct value next; /* next value in list */
    struct value sublist; /* new begin/end block */
    string name; /* name of node */
    string value; /* value of this node */

} value, *valptr;

/**//***************************************************************************

Parse config tree from filename

Parses a configuration tree from the given filename/path to the given root tree.
The values are merged with the contents of the root passed.

This routine can be called directly to use alternative config file names.

*******************************************************************************/

void _pa_configfile(string fn, valptr* root)

{

    FILE* f;

    f = fopen(fn, "r");
    if (f) { /* file exists/can be opened */

        while (!feof()) {

            p = fgets(buff, MAXSTR, f); /* get next line */
            if (p) { /* not eof */

                while (isspace(*s)) s++; /* skip any spaces */
                if (*s == "#") { /* comment */

                    while (*s) s++; /* skip over line */

                } else

            }

        }
        fclose(f);

    }

}

/**//***************************************************************************

Parse config tree

Parses a configuration tree to the given root tree. The values are merged.

*******************************************************************************/

void _pa_config(valptr* root)

{

    char pgmpth[MAXSTR];
    char filnam[MAXSTR];

    /* config from program path */
    pa_getgm(pgmpth, MAXSTR); /* get program path */
    /* try both visible and invisible names */
    pa_maknam(filnam, MAXSTR, pgmpth, "petit_ami", "cfg");
    configtree(name, root);
    pa_maknam(filnam, MAXSTR, pgmpth, ".petit_ami", "cfg");
    configtree(name, root);

    /* config from user path */
    pa_getusr(pgmpth, MAXSTR); /* get user path */
    /* try both visible and invisible names */
    pa_maknam(filnam, MAXSTR, pgmpth, "petit_ami", "cfg");
    configtree(name, root);
    pa_maknam(filnam, MAXSTR, pgmpth, ".petit_ami", "cfg");
    configtree(name, root);

    /* config from current directory */
    pa_getcur(pgmpth, MAXSTR); /* get current path */
    /* try both visible and invisible names */
    pa_maknam(filnam, MAXSTR, pgmpth, "petit_ami", "cfg");
    configtree(name, root);
    pa_maknam(filnam, MAXSTR, pgmpth, ".petit_ami", "cfg");
    configtree(name, root);

}
