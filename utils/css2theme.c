/*******************************************************************************
*                                                                              *
*                        GTK CSS THEME TO PETIT-AMI THEME                      *
*                                                                              *
* Reads the palette from a GTK CSS theme and writes the equivalent Petit-Ami   *
* widget theme into a petit_ami.cfg file, so the widget set takes the colors   *
* of the desktop theme in use.                                                 *
*                                                                              *
* Usage:                                                                       *
*                                                                              *
* css2theme [<theme>|<file.css>] [-o <config file>] [-p] [-n]                  *
*                                                                              *
* With no arguments the theme currently selected in the desktop is converted   *
* and written to the user's config file, which is what most users want:        *
*                                                                              *
*     css2theme                                                                *
*                                                                              *
* Arguments:                                                                   *
*                                                                              *
* <theme>       Name of an installed theme, as in /usr/share/themes, for       *
*               example "Yaru-dark". Default is the desktop's current theme.   *
* <file.css>    A CSS file to read instead, recognized by the .css ending.     *
* -o <file>     Config file to update. Default is petit_ami.cfg in the user's  *
*               home directory, which applies to all of the user's programs.   *
* -p            Print the theme block instead of updating any file.           *
* -n            Print the theme names found and what they map to, a           *
*               diagnostic.                                                    *
*                                                                              *
* How it works:                                                                *
*                                                                              *
* GTK themes declare their palette with @define-color lines. These are the     *
* theme's semantic colors: window background, text, selection and so on. The   *
* rest of a GTK stylesheet is thousands of lines of compiled rules carrying    *
* literal colors, which is not a palette and is not read here. The Petit-Ami   *
* theme points are derived from the palette, shading where GTK itself shades   *
* (a hovered button is the background lightened or darkened, and so on).       *
*                                                                              *
* Modern themes do not ship their CSS as a plain file: the theme directory     *
* holds a stub that imports from a compiled gresource blob. This program       *
* follows that automatically using the gresource tool. The stock GNOME theme   *
* (Adwaita) keeps its CSS inside the GTK library itself, with no theme         *
* directory content at all; there is nothing to read in that case, and the     *
* built in Petit-Ami theme remains in use.                                     *
*                                                                              *
* The color select dialog palette (querycolor1 through querycolor36) is not    *
* themed. That grid is there to offer the user a range of colors to pick       *
* from, which is not the same thing as the colors of the desktop.              *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXSTR   500  /* maximum string length */
#define MAXLIN   1000 /* maximum line length */
#define MAXPAL   100  /* maximum palette entries */
#define MAXCFG   100000 /* maximum config file size */

/* palette entry, as read from the theme */
typedef struct {

    char  name[MAXSTR];  /* GTK palette name */
    long  color;         /* packed rgb value */
    int   valid;         /* the value could be resolved */

} palrec;

/* theme point mapping: which palette entry gives which Petit-Ami theme
   point, and how it is shaded to get there. A shade of 1.0 is the palette
   color as is, below 1.0 is darker, above is lighter, the same convention
   GTK's own shade() function uses */
typedef struct {

    char*  thn;   /* Petit-Ami theme point name */
    char*  gtkn;  /* GTK palette name */
    double shade; /* shade factor */

} maprec;

/*
 * The mapping table.
 *
 * GTK offers about ten useful semantic colors; the Petit-Ami theme has 45
 * named points, so most points are derived by shading. Where GTK's own rules
 * shade the background for a state (hover, pressed, disabled), the same is
 * done here.
 */
static maprec maptbl[] = {

    /* buttons */
    { "back",              "theme_bg_color",          1.00 },
    { "backhover",         "theme_bg_color",          0.94 },
    { "backpressed",       "theme_bg_color",          0.86 },
    { "outline1",          "borders",                 1.00 },
    { "text",              "theme_fg_color",          1.00 },
    { "textdis",           "insensitive_fg_color",    1.00 },
    { "focus",             "theme_selected_bg_color", 1.00 },

    /* checkbox and radio button */
    { "chkrad",            "theme_selected_bg_color", 1.00 },
    { "chkradout",         "borders",                 1.00 },

    /* scrollbar */
    { "scrollback",        "theme_bg_color",          0.96 },
    { "scrollbar",         "theme_fg_color",          1.90 },
    { "scrollbarpressed",  "theme_selected_bg_color", 1.00 },

    /* number select box */
    { "numseldiv",         "borders",                 1.10 },
    { "numselud",          "theme_fg_color",          1.60 },
    { "outline2",          "borders",                 1.00 },

    /* error text */
    { "texterr",           "error_color",             1.00 },

    /* progress bar */
    { "proginacen",        "theme_bg_color",          0.90 },
    { "proginaedg",        "borders",                 1.00 },
    { "progactcen",        "theme_selected_bg_color", 1.00 },
    { "progactedg",        "theme_selected_bg_color", 0.88 },

    /* list box */
    { "lsthov",            "theme_bg_color",          0.94 },

    /* drop box */
    { "droparrow",         "theme_fg_color",          1.00 },
    { "droptext",          "theme_text_color",        1.00 },

    /* slider */
    { "sldint",            "theme_bg_color",          0.92 },

    /* tab bar */
    { "tabdis",            "insensitive_fg_color",    1.00 },
    { "tabback",           "theme_bg_color",          1.00 },
    { "tabsel",            "theme_selected_bg_color", 1.00 },
    { "tabfocus",          "theme_selected_bg_color", 1.10 },

    /* dialog title */
    { "title",             "theme_fg_color",          1.00 },

    /* dialog cancel button: a plain button in the theme's colors */
    { "cancelbackfocus",   "theme_bg_color",          0.86 },
    { "canceltextfocus",   "theme_fg_color",          1.00 },
    { "canceloutline",     "borders",                 0.90 },

    /* dialog select button: the theme's selection color, as GTK gives
       "suggested action" buttons */
    { "selectbackfocus",   "theme_selected_bg_color", 1.00 },
    { "selectback",        "theme_selected_bg_color", 0.94 },
    { "selecttextfocus",   "theme_selected_fg_color", 1.00 },
    { "selecttext",        "theme_selected_fg_color", 1.00 },
    { "selectoutline",     "theme_selected_bg_color", 0.90 },
    { "selectoutlinefocus","theme_selected_bg_color", 1.00 },

    /* dialog plus button: a plain button */
    { "plusbackfocus",     "theme_bg_color",          1.00 },
    { "plusback",          "theme_bg_color",          1.00 },
    { "plustextfocus",     "theme_fg_color",          1.00 },
    { "plustext",          "theme_fg_color",          1.00 },
    { "plusoutline",       "borders",                 1.00 },
    { "plusoutlinefocus",  "borders",                 1.00 },

    { NULL, NULL, 0.0 } /* end of table */

};

static palrec paltbl[MAXPAL]; /* palette read from the theme */
static int    palnum;         /* number of palette entries */

/*******************************************************************************

Report an error and stop

*******************************************************************************/

static void error(const char* es)

{

    fprintf(stderr, "*** css2theme: %s\n", es);

    exit(1);

}

/*******************************************************************************

Find a palette entry by name

Returns the entry, or NULL if the theme does not define it.

*******************************************************************************/

static palrec* fndpal(const char* name)

{

    int i;

    for (i = 0; i < palnum; i++)
        if (!strcmp(paltbl[i].name, name)) return (&paltbl[i]);

    return (NULL);

}

/*******************************************************************************

Shade a color

Multiplies the color by the given factor, the same operation GTK's shade()
performs: above 1.0 lightens, below 1.0 darkens. Components saturate at 255
rather than wrapping.

*******************************************************************************/

static long shdcol(long c, double f)

{

    long r, g, b;

    r = (c>>16 & 0xff)*f;
    g = (c>>8 & 0xff)*f;
    b = (c & 0xff)*f;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    return (r<<16 | g<<8 | b);

}

/*******************************************************************************

Parse a CSS color value

Handles the forms a GTK palette actually uses: #rgb and #rrggbb, rgb() and
rgba(), the handful of named colors that appear, shade() of another value,
and @name references to a previously defined palette entry. Returns TRUE if
the value resolved. alpha() forms do not resolve: they are transparencies,
which Petit-Ami colors cannot express, and every one of them in practice is
a window manager shadow that has no Petit-Ami equivalent.

*******************************************************************************/

static int cnvcol(const char* s, long* c)

{

    long    r, g, b;
    double  f;
    char    nam[MAXSTR];
    char    sub[MAXSTR];
    palrec* pp;
    int     i;
    const char* cp;

    while (isspace(*s)) s++; /* skip leading spaces */
    if (*s == '#') { /* hex form */

        s++;
        if (strlen(s) >= 6) { /* #rrggbb */

            if (sscanf(s, "%2lx%2lx%2lx", &r, &g, &b) != 3) return (0);

        } else if (strlen(s) >= 3) { /* #rgb, each digit doubled */

            if (sscanf(s, "%1lx%1lx%1lx", &r, &g, &b) != 3) return (0);
            r = r<<4|r; g = g<<4|g; b = b<<4|b;

        } else return (0);
        *c = r<<16 | g<<8 | b;

        return (1);

    }
    if (!strncmp(s, "rgba(", 5) || !strncmp(s, "rgb(", 4)) {

        cp = strchr(s, '(')+1;
        if (sscanf(cp, "%ld,%ld,%ld", &r, &g, &b) != 3) return (0);
        *c = r<<16 | g<<8 | b;

        return (1);

    }
    if (!strncmp(s, "shade(", 6)) { /* shade(color, factor) */

        cp = s+6;
        /* copy the inner color up to the comma */
        i = 0;
        while (*cp && *cp != ',' && i < MAXSTR-1) sub[i++] = *cp++;
        sub[i] = 0;
        if (*cp != ',') return (0);
        f = atof(cp+1);
        if (!cnvcol(sub, c)) return (0);
        *c = shdcol(*c, f);

        return (1);

    }
    if (*s == '@') { /* reference to another palette entry */

        s++;
        i = 0;
        while (*s && !isspace(*s) && *s != ';' && i < MAXSTR-1) nam[i++] = *s++;
        nam[i] = 0;
        pp = fndpal(nam);
        if (!pp || !pp->valid) return (0);
        *c = pp->color;

        return (1);

    }
    /* named colors: only the few that appear in themes */
    i = 0;
    while (*s && isalpha(*s) && i < MAXSTR-1) nam[i++] = *s++;
    nam[i] = 0;
    if (!strcmp(nam, "white")) { *c = 0xffffff; return (1); }
    if (!strcmp(nam, "black")) { *c = 0x000000; return (1); }
    if (!strcmp(nam, "red"))   { *c = 0xff0000; return (1); }
    if (!strcmp(nam, "green")) { *c = 0x008000; return (1); }
    if (!strcmp(nam, "blue"))  { *c = 0x0000ff; return (1); }

    return (0); /* alpha() and anything else does not resolve */

}

/*******************************************************************************

Read the palette from a CSS stream

Collects every @define-color line. Entries are kept in file order, so a later
entry can reference an earlier one.

*******************************************************************************/

static void rdpal(FILE* fp)

{

    char  lin[MAXLIN];
    char  nam[MAXSTR];
    char* cp;
    char* vp;
    int   i;

    palnum = 0;
    while (fgets(lin, MAXLIN, fp)) {

        cp = strstr(lin, "@define-color");
        if (!cp) continue; /* not a palette line */
        cp += 13; /* skip the keyword */
        while (isspace(*cp)) cp++;
        /* collect the name */
        i = 0;
        while (*cp && !isspace(*cp) && i < MAXSTR-1) nam[i++] = *cp++;
        nam[i] = 0;
        while (isspace(*cp)) cp++;
        /* the value runs to the terminating semicolon */
        vp = cp;
        cp = strchr(vp, ';');
        if (cp) *cp = 0;
        else continue; /* no terminator, skip the line */
        if (palnum >= MAXPAL) error("Too many palette entries");
        strncpy(paltbl[palnum].name, nam, MAXSTR-1);
        paltbl[palnum].name[MAXSTR-1] = 0;
        paltbl[palnum].valid = cnvcol(vp, &paltbl[palnum].color);
        palnum++;

    }

}

/*******************************************************************************

Find the desktop's current theme

Asks the desktop settings for the theme in use. Returns TRUE if a name was
found.

*******************************************************************************/

static int curthm(char* name, int len)

{

    FILE* fp;
    char  lin[MAXLIN];
    char* cp;
    int   i;

    fp = popen("gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null",
               "r");
    if (!fp) return (0);
    if (!fgets(lin, MAXLIN, fp)) { pclose(fp); return (0); }
    pclose(fp);
    /* the value comes back quoted, as 'Yaru' */
    cp = lin;
    while (*cp && *cp != '\'' && *cp != '"') cp++;
    if (!*cp) return (0);
    cp++;
    i = 0;
    while (*cp && *cp != '\'' && *cp != '"' && i < len-1) name[i++] = *cp++;
    name[i] = 0;

    return (i > 0);

}

/*******************************************************************************

Open the CSS for a theme by name

Modern themes ship a stub gtk.css that imports from a compiled gresource
blob. If the stub is a plain stylesheet, it is read directly; if it imports a
resource, the gresource tool extracts the real stylesheet and that is read
instead. Returns the stream, or NULL with the reason placed in the message
buffer.

*******************************************************************************/

static FILE* opnthm(const char* theme, char* msg, int msglen)

{

    char  path[MAXSTR*2];
    char  lin[MAXLIN];
    char  res[MAXSTR];
    char  cmd[MAXSTR*5];
    FILE* fp;
    char* cp;
    char* ep;
    int   i;

    snprintf(path, MAXSTR*2, "/usr/share/themes/%s/gtk-3.0/gtk.css", theme);
    fp = fopen(path, "r");
    if (!fp) {

        snprintf(msg, msglen, "Theme \"%s\" has no gtk-3.0/gtk.css", theme);

        return (NULL);

    }
    /* look for a resource import in the first few lines */
    res[0] = 0;
    for (i = 0; i < 10 && fgets(lin, MAXLIN, fp); i++) {

        cp = strstr(lin, "resource://");
        if (cp) {

            cp += 11; /* skip the scheme */
            ep = res;
            while (*cp && *cp != '"' && *cp != ')' && ep-res < MAXSTR-1)
                *ep++ = *cp++;
            *ep = 0;
            break;

        }

    }
    if (!res[0]) { /* a plain stylesheet, read it directly */

        rewind(fp);

        return (fp);

    }
    fclose(fp);
    /* extract the real stylesheet from the theme's resource blob */
    snprintf(path, MAXSTR*2, "/usr/share/themes/%s/gtk-3.0/gtk.gresource",
             theme);
    if (!fopen(path, "r")) {

        snprintf(msg, msglen,
                 "Theme \"%s\" imports a resource, but has no gtk.gresource. "
                 "The theme's stylesheet is inside the GTK library and cannot "
                 "be read", theme);

        return (NULL);

    }
    snprintf(cmd, MAXSTR*5, "gresource extract '%s' '%s' 2>/dev/null",
             path, res);
    fp = popen(cmd, "r");
    if (!fp) {

        snprintf(msg, msglen, "Cannot run the gresource tool");

        return (NULL);

    }

    return (fp);

}

/*******************************************************************************

Write the theme block

Writes the widgets/theme config block for the palette just read. Every theme
point that maps to a palette entry the theme defines is written; a point
whose palette entry is missing or unresolvable is left out, so the built in
default stands for it.

*******************************************************************************/

static int wrtthm(FILE* fp, const char* theme)

{

    maprec* mp;
    palrec* pp;
    long    c;
    int     cnt;

    cnt = 0;
    fprintf(fp, "#\n");
    fprintf(fp, "# Widget theme, generated by css2theme from the GTK theme\n");
    fprintf(fp, "# \"%s\". Edits to this block are replaced the next time\n",
            theme);
    fprintf(fp, "# css2theme runs. Remove the block to return to the built in\n");
    fprintf(fp, "# theme.\n");
    fprintf(fp, "#\n");
    fprintf(fp, "begin widgets\n");
    fprintf(fp, "\n");
    fprintf(fp, "    begin theme\n");
    fprintf(fp, "\n");
    for (mp = maptbl; mp->thn; mp++) {

        pp = fndpal(mp->gtkn);
        if (pp && pp->valid) {

            c = shdcol(pp->color, mp->shade);
            fprintf(fp, "        %-20s %ld %ld %ld\n", mp->thn,
                    c>>16 & 0xff, c>>8 & 0xff, c & 0xff);
            cnt++;

        }

    }
    fprintf(fp, "\n");
    fprintf(fp, "    end\n");
    fprintf(fp, "\n");
    fprintf(fp, "end\n");

    return (cnt);

}

/*******************************************************************************

Update a config file

Replaces the widgets block in the given config file with a freshly generated
one, leaving the rest of the file exactly as it was. If the file has no
widgets block, the block is appended. If the file does not exist, it is
created. Nested begin/end pairs inside the block are counted, so a block
containing subblocks is replaced whole.

*******************************************************************************/

static void updcfg(const char* fn, const char* theme)

{

    FILE* fp;
    char* buf;
    char* cp;
    char* bp;
    char* ep;
    long  len;
    int   dep;
    char  lin[MAXLIN];
    char  tmp[MAXSTR];

    buf = malloc(MAXCFG);
    if (!buf) error("Out of memory");
    buf[0] = 0;
    len = 0;
    fp = fopen(fn, "r");
    if (fp) { /* read the existing file */

        len = fread(buf, 1, MAXCFG-1, fp);
        if (len < 0) len = 0;
        buf[len] = 0;
        fclose(fp);

    }
    /* find an existing widgets block */
    bp = NULL;
    ep = NULL;
    cp = buf;
    while (*cp) {

        /* examine this line */
        sscanf(cp, "%499s", tmp);
        if (!strcmp(tmp, "begin")) {

            sscanf(cp, "%*s %499s", tmp);
            if (!strcmp(tmp, "widgets")) {

                bp = cp; /* block starts here */
                /* scan forward for the matching end */
                dep = 0;
                while (*cp) {

                    sscanf(cp, "%499s", tmp);
                    if (!strcmp(tmp, "begin")) dep++;
                    else if (!strcmp(tmp, "end")) {

                        dep--;
                        if (!dep) { /* matching end found */

                            while (*cp && *cp != '\n') cp++;
                            if (*cp) cp++; /* take the newline too */
                            ep = cp;
                            break;

                        }

                    }
                    while (*cp && *cp != '\n') cp++;
                    if (*cp) cp++;

                }
                break;

            }

        }
        while (*cp && *cp != '\n') cp++;
        if (*cp) cp++;

    }
    if (bp && !ep) error("Config file has an unterminated widgets block");
    /* write the file back with the block replaced */
    fp = fopen(fn, "w");
    if (!fp) {

        snprintf(lin, MAXLIN, "Cannot write config file \"%s\"", fn);
        error(lin);

    }
    if (bp) { /* replace in place */

        fwrite(buf, 1, bp-buf, fp);
        wrtthm(fp, theme);
        fputs(ep, fp);

    } else { /* append, keeping a blank line before the block */

        fputs(buf, fp);
        if (len && buf[len-1] != '\n') fputc('\n', fp);
        if (len) fputc('\n', fp);
        wrtthm(fp, theme);

    }
    fclose(fp);
    free(buf);

}

int main(int argc, char* argv[])

{

    char  theme[MAXSTR];  /* theme name */
    char  cfgfil[MAXSTR]; /* config file to update */
    char  msg[MAXSTR];    /* error message from the theme open */
    char* cssfil;         /* explicit css file, if given */
    char* home;
    FILE* fp;
    int   prtblk;         /* print the block instead of updating */
    int   prtnam;         /* print the palette diagnostic */
    int   i;
    int   cnt;
    maprec* mp;
    palrec* pp;

    theme[0] = 0;
    cssfil = NULL;
    cfgfil[0] = 0;
    prtblk = 0;
    prtnam = 0;
    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "-p")) prtblk = 1;
        else if (!strcmp(argv[i], "-n")) prtnam = 1;
        else if (!strcmp(argv[i], "-o")) {

            i++;
            if (i >= argc) error("-o needs a config file");
            strncpy(cfgfil, argv[i], MAXSTR-1);
            cfgfil[MAXSTR-1] = 0;

        } else if (strstr(argv[i], ".css")) cssfil = argv[i];
        else {

            strncpy(theme, argv[i], MAXSTR-1);
            theme[MAXSTR-1] = 0;

        }

    }
    /* find the theme to convert */
    if (cssfil) {

        fp = fopen(cssfil, "r");
        if (!fp) error("Cannot open the given CSS file");
        strncpy(theme, cssfil, MAXSTR-1);
        theme[MAXSTR-1] = 0;

    } else {

        if (!theme[0] && !curthm(theme, MAXSTR))
            error("Cannot find the desktop's current theme, name one");
        fp = opnthm(theme, msg, MAXSTR);
        if (!fp) error(msg);

    }
    rdpal(fp); /* read the palette */
    fclose(fp);
    if (!palnum) error("The theme declares no palette (@define-color) entries");
    if (prtnam) { /* palette diagnostic */

        printf("Palette read from \"%s\":\n\n", theme);
        for (i = 0; i < palnum; i++)
            if (paltbl[i].valid)
                printf("    %-36s %06lx\n", paltbl[i].name, paltbl[i].color);
            else
                printf("    %-36s (does not resolve, not used)\n",
                       paltbl[i].name);
        printf("\nTheme points that will be set:\n\n");
        for (mp = maptbl; mp->thn; mp++) {

            pp = fndpal(mp->gtkn);
            if (pp && pp->valid)
                printf("    %-20s from %-26s shade %.2f\n", mp->thn, mp->gtkn,
                       mp->shade);
            else
                printf("    %-20s from %-26s (not in theme, left at default)\n",
                       mp->thn, mp->gtkn);

        }

        return (0);

    }
    if (prtblk) { /* just print the block */

        wrtthm(stdout, theme);

        return (0);

    }
    /* default target is the user's config file, which applies to all of the
       user's programs */
    if (!cfgfil[0]) {

        home = getenv("HOME");
        if (!home) error("Cannot find the home directory, name a config file");
        snprintf(cfgfil, MAXSTR, "%.400s/petit_ami.cfg", home);

    }
    updcfg(cfgfil, theme);
    /* report what was done */
    cnt = 0;
    for (mp = maptbl; mp->thn; mp++) {

        pp = fndpal(mp->gtkn);
        if (pp && pp->valid) cnt++;

    }
    printf("css2theme: theme \"%s\", %d theme points written to %s\n",
           theme, cnt, cfgfil);

    return (0);

}
