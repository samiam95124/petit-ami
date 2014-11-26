/* Output from p2c 1.21alpha-07.Dec.93, the Pascal-to-C translator */
/* From input file "ansilib.pas" */


/*******************************************************************************
*                                                                             *
*                      TRANSPARENT SCREEN CONTROL MODULE                      *
*                     FOR ANSI COMPLIANT TERMINALS (IBMPC)                    *
*                                                                             *
*                              4/96 S. A. Moore                               *
*                                                                             *
* This module implements the level 1 section of the standard terminal calls   *
* for an ANSI compliant terminal running under windows 95. Although this is   *
* dependent on Windows 95, theoretically, it should run via a serial console  *
* port. Mouse control is enabled, but this is very unlikely to be able to run *
* in such a configuration. It will work in a local window, however. I don't   *
* know what Windows does if the console is serial.                            *
* Note: This package won't work if the actual screen size does not match the  *
* set screen size, because we rely on the ANSI downward scroll.               *
*                                                                             *
* Things to do:                                                               *
*                                                                             *
* 1. Right now, the input and output files as passed to the screen control    *
* routines are ignored. Instead, they should be checked to actually contain   *
* those files.                                                                *
*                                                                             *
* 2. Ansilib should parse command line parameters. These will appear right    *
* after the command so that the program itself does not need to know about    *
* them:                                                                       *
*                                                                             *
*    comm #opt [other program options and files]                              *
*                                                                             *
* The options are:                                                            *
*                                                                             *
*    #bc=c or #backcolor=c - Set inital background color.                     *
*    #fc=c or #forecolor=c - Set inital foreground color.                     *
*    #at=a or #attribute=a - Set inital drawing attribute.                    *
*    #xs=x or #xsize=x     - Set size of x demension.                         *
*    #ys=y or #ysize=y     - Set size of y demension.                         *
*    #sw   or #smallwindow - Assume not using all of terminal.                *
*                                                                             *
*******************************************************************************/


#include <p2c/p2c.h>


#define maxtim          10   /* maximum number of timers available */
#define ss_maxhdl       10


typedef long ss_filhdl;

typedef uchar byte;

typedef byte bytarr[40];
typedef Char string[40];
/* colors displayable in text mode */
typedef enum {
  black, white, red, green, blue, cyan, yellow, magenta
} color;
typedef char joyhan;
   /* joystick handles */
typedef char joynum;
   /* number of joysticks */
typedef char joybut;
   /* joystick buttons */
typedef char joybtn;
   /* joystick number of buttons */
typedef char joyaxn;
   /* joystick axies */
typedef char timhan;
   /* timer handle */
/* events */
/* ANSI character returned */
/* cursor up one line */
/* down one line */
/* left one character */
/* right one character */
/* left one word */
/* right one word */
/* home of document */
/* home of screen */
/* home of line */
/* end of document */
/* end of screen */
/* end of line */
/* scroll left one character */
/* scroll right one character */
/* scroll up one line */
/* scroll down one line */
/* page down */
/* page up */
/* tab */
/* enter line */
/* insert block */
/* insert line */
/* insert toggle */
/* delete block */
/* delete line */
/* delete character forward */
/* delete character backward */
/* copy block */
/* copy line */
/* cancel current operation */
/* stop current operation */
/* continue current operation */
/* print document */
/* print block */
/* print screen */
/* function key 1 */
/* function key 2 */
/* function key 3 */
/* function key 4 */
/* function key 5 */
/* function key 6 */
/* function key 7 */
/* function key 8 */
/* function key 9 */
/* function key 10 */
/* display menu */
/* mouse button 1 assertion */
/* mouse button 2 assertion */
/* mouse button 3 assertion */
/* mouse button 4 assertion */
/* mouse button 1 deassertion */
/* mouse button 2 deassertion */
/* mouse button 3 deassertion */
/* mouse button 4 deassertion */
/* mouse move */
/* timer matures */
/* joystick button assertion */
/* joystick button deassertion */
/* joystick move */
typedef enum {
  etchar, etup, etdown, etleft, etright, etleftw, etrightw, ethome, ethomes,
  ethomel, etend, etends, etendl, etscrl, etscrr, etscru, etscrd, etpagd,
  etpagu, ettab, etenter, etinsert, etinsertl, etinsertt, etdel, etdell,
  etdelcf, etdelcb, etcopy, etcopyl, etcan, etstop, etcont, etprint, etprintb,
  etprints, etf1, etf2, etf3, etf4, etf5, etf6, etf7, etf8, etf9, etf10,
  etmenu, etmoub1a, etmoub2a, etmoub3a, etmoub4a, etmoub1d, etmoub2d,
  etmoub3d, etmoub4d, etmoumov, ettim, etjoyba, etjoybd, etjoymov, etterm
} evtcod;   /* terminate program */
/* p2c: ansilib.pas, line 120:
 * Note: Line breaker spent 0.0 seconds, 5000 tries on line 142 [251] */
/* event record */

typedef struct evtrec {

  /* event type */

  /* ANSI character returned */
  evtcod etype;
  union {
    Char char_;
    /* timer handle that matured */
    timhan timnum;
    struct {
      long moupx, moupy;   /* mouse movement */
    } U55;
    struct {
      joyhan ajoyn;   /* joystick number */
      joybut ajoybn;   /* button number */
    } U57;
    struct {
      joyhan djoyn;   /* joystick number */
      joybut djoybn;   /* button number */
    } U58;
    struct {
      joyhan mjoyn;   /* joystick number */
      long joypx, joypy, joypz;   /* joystick coordinates */
    } U59;
  } UU;
} evtrec;

/* lower level interdiction funtions */


Static Void ts_openread PP((ss_filhdl *fn, Char *nm));

Static Void ts_openwrite PP((ss_filhdl *fn, Char *nm));

Static Void ts_close PP((ss_filhdl fn));

Static Void ts_read PP((ss_filhdl fn, byte *ba));

Static Void ts_write PP((ss_filhdl fn, byte *ba));

Static Void ts_position PP((ss_filhdl fn, long p));

Static long ts_location PP((ss_filhdl fn));

Static long ts_length PP((ss_filhdl fn));

Static boolean ts_eof PP((ss_filhdl fn));

/* functions at this level */

Static Void cursor PP((FILE **f, long x, long y));

Static long maxx PP((FILE **f));

Static long maxy PP((FILE **f));

Static Void home PP((FILE **f));

Static Void del PP((FILE **f));

Static Void up PP((FILE **f));

Static Void down PP((FILE **f));

Static Void left PP((FILE **f));

Static Void right PP((FILE **f));

Static Void blink PP((FILE **f, int e));

Static Void reverse PP((FILE **f, int e));

Static Void underline PP((FILE **f, int e));

Static Void superscript PP((FILE **f, int e));

Static Void subscript PP((FILE **f, int e));

Static Void italic PP((FILE **f, int e));

Static Void bold PP((FILE **f, int e));

Static Void standout PP((FILE **f, int e));

Static Void fcolor PP((FILE **f, color c));

Static Void bcolor PP((FILE **f, color c));

Static Void ascroll PP((FILE **f, int e));

Static Void curvis PP((FILE **f, int e));

Static Void scroll PP((FILE **f, long x, long y));

Static long curx PP((FILE **f));

Static long cury PP((FILE **f));

Static Void select PP((FILE **f, long s));

Static Void event PP((FILE **f, evtrec *er));

Static Void timer PP((FILE **f, int i, long t, int r));

Static Void killtimer PP((FILE **f, int i));

Static boolean mouse PP((FILE **f));

Static joynum joystick PP((FILE **f));

Static joybtn joybutton PP((FILE **f, int j));

Static joyaxn joyaxis PP((FILE **f, int j));


/* abort label */


#define maxxd           80   /* standard terminal, 80x50 */
#define maxyd           50
/* standard file handles */
#define inpfil          1   /* _input */
#define outfil          2   /* _output */
#define maxlin          250   /* maximum length of input bufferred line */
#define maxcon          10   /* number of screen contexts */


/* screen attribute */
/* no attribute */
/* blinking text (foreground) */
/* reverse video */
/* underline */
/* superscript */
/* subscripting */
/* italic text */
typedef enum {
  sanone, sablink, sarev, saundl, sasuper, sasubs, saital, sabold
} scnatt;   /* bold text */
/* single character on screen container. note that not all the attributes
   that appear here can be changed */

typedef struct scnrec {

  Char ch;   /* character at location */
  color forec;   /* foreground color at location */
  color backc;   /* background color at location */
  /* active attribute at location */

  scnatt attr;
} scnrec;

typedef scnrec scnbuf[maxyd][maxxd];

typedef struct scncon {
  /* screen context */

  scnbuf buf;   /* screen buffer */
  long curx;   /* current cursor location x */
  long cury;   /* current cursor location y */
  color forec;   /* current writing foreground color */
  color backc;   /* current writing background color */
  scnatt attr;   /* current writing attribute */
  /* current status of scroll */

  boolean scroll;
} scncon;

/* pointer to screen context block */
/* file table full */
/* joystick access */
/* timer access */
/* cannot perform operation on special file */
/* invalid screen position */
/* filename is empty */
/* invalid screen number */
typedef enum {
  eftbful, ejoyacc, etimacc, efilopr, einvpos, efilzer, einvscn, einvhan
} errcod;   /* invalid handle */


Static long inphdl;   /* "input" file handle */
Static boolean mb1;   /* mouse assert status button 1 */
Static boolean mb2;   /* mouse assert status button 2 */
Static boolean mb3;   /* mouse assert status button 3 */
Static boolean mb4;   /* mouse assert status button 4 */
Static long mpx, mpy;   /* mouse current position */
Static boolean nmb1;   /* new mouse assert status button 1 */
Static boolean nmb2;   /* new mouse assert status button 2 */
Static boolean nmb3;   /* new mouse assert status button 3 */
Static boolean nmb4;   /* new mouse assert status button 4 */
Static long nmpx, nmpy;   /* new mouse current position */
Static ss_filhdl opnfil[ss_maxhdl];   /* open files table */
Static char fi;   /* index for files table */
/* we must open and process the _output file on our own, else we would
   recurse */
Static ss_filhdl trmfil;   /* output file */
Static byte chrbuf[1];   /* single character output buffer */
Static Char inpbuf[maxlin];   /* input line buffer */
Static uchar inpptr;   /* input line index */
Static scncon *screens[maxcon];   /* screen contexts array */
Static char curscn;   /* index for current screen */


Static jmp_buf _JL88;

Local Void putstr(s)
Char *s;
{
  long i;   /* index for string */
  Char pream[9];   /* preamble string */
  Char *p;   /* pointer to string */


  memcpy(pream, "Ansilib: ", 9L);   /* set preamble */
/* p2c: ansilib.pas, line 295: Warning: Expected a ')', found a '(' [227] */
/* p2c: ansilib.pas, line 295:
 * Note: No SpecialMalloc form known for STRING.MAX [187] */
  p = (Char *)Malloc(sizeof(string));   /* get string to hold */
  for (i = 0; i <= 8; i++)   /* copy preamble */
    p[i] = pream[i];
  for (i = 1; i <= s; i++)   /* copy string */
    p[i+8] = s[i-1];
  ss_wrterr(p);   /* output string */
/* p2c: ansilib.pas, line 298:
 * Warning: Symbol 'SS_WRTERR' is not defined [221] */
  /* release storage */

  Free(p);
}



/*******************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.

*******************************************************************************/

Static Void error(e)
errcod e;
{
  switch (e) {   /* error */

  case eftbful:
    putstr("Too many files                          ");
    break;

  case ejoyacc:
    putstr("No joystick access available            ");
    break;

  case etimacc:
    putstr("No timer access available               ");
    break;

  case einvhan:
    putstr("Invalid handle                          ");
    break;

  case efilopr:
    putstr("Cannot perform operation on special file");
    break;

  case einvpos:
    putstr("Invalid screen position                 ");
    break;

  case efilzer:
    putstr("Filename is empty                       ");
    break;

  case einvscn:
    putstr("Invalid screen number                   ");
    break;

  case einvhan:
    putstr("Invalid file handle                     ");
    break;

  }
  longjmp(_JL88, 1);   /* abort module */

}



/*******************************************************************************

Make file entry

Indexes a present file entry or creates a new one. Looks for a free entry
in the files table, indicated by 0. If found, that is returned, otherwise
the file table is full.
Note that the "predefined" file slots are never allocated.

*******************************************************************************/

Static Void makfil(fn)
ss_filhdl *fn;
{
  /* file handle */
  char fi;   /* index for files table */
  char ff = 0;   /* found file entry */


  /* find idle file slot (file with closed file entry) */
  /* clear found file */
  for (fi = outfil + 1; fi <= ss_maxhdl; fi++) {
	/* search all file entries */
	  if (opnfil[fi-1] == 0)
      ff = fi;
  }
  /* found a slot */
  if (ff == 0)   /* no empty file positions found */
    error(einvhan);
  *fn = ff;   /* set file id number */

}



/*******************************************************************************

Remove leading and trailing spaces

Given a string, removes any leading and trailing spaces in the string. The
result is allocated and returned as an indexed buffer.
The input string must not be null.

*******************************************************************************/

Static Void remspc(nm, rs)
Char *nm, **rs;
{
  /* string */
  /* result string */
  long i1;
  long i2 = 1;   /* string indexes */
  boolean n = true;   /* string is null */
  long s = 1;
  long e;   /* string start and end */


  /* first check if the string is empty or null */
  /* set empty */
  for (i1 = 0; i1 <= (Char *)(nm - 1); i1++) {   /* set not empty */
    if (nm[i1] != ' ')
      n = false;
  }
  if (n)   /* filename is empty */
    error(efilzer);
  /* set start of string */
  while (s < nm && nm[s-1] == ' ')
    s++;
  e = nm;   /* set end of string */
  while (e > 1 && nm[e-1] == ' ')
    e--;
/* p2c: ansilib.pas, line 377: Warning: Expected a ')', found a '-' [227] */
/* p2c: ansilib.pas, line 377:
 * Note: No SpecialMalloc form known for STRING.E [187] */
  *rs = (Char *)Malloc(sizeof(string));   /* allocate result string */
  /* set 1st character of destination */
  for (i1 = s - 1; i1 < e; i1++) {  /* copy to result */
    (*rs)[i2-1] = nm[i1];   /* copy to result */
    i2++;   /* next character */

  }


}


/* Local variables for chksys: */
struct LOC_chksys {
  Char *fn;
} ;

/* find lower case */

Local Char lcase(c)
Char c;
{
  /* find lower case equivalent */
  if (isupper(c))
    c = _tolower(c);
  return c;   /* return as result */

}

/* match strings */

Local boolean chkstr(s, LINK)
Char *s;
struct LOC_chksys *LINK;
{
  boolean m = true;   /* match status */
  long i;   /* index for string */


  /* set no match */
/* p2c: ansilib.pas: Note: Eliminated unused assignment statement [338] */
  if (strncmp(s, LINK->fn, sizeof(string)))  /* lengths match */
    return false;

  /* set strings match */
  for (i = 0; i <= (Char *)(s - 1); i++) {
    if (lcase(LINK->fn[i]) != lcase(s[i])) {
      m = false;

    }
  }
  return m;   /* return match status */

}



/*******************************************************************************

Check system special file

Checks for one of the special files, and returns the handle of the special
file if found. Accepts a general string.

*******************************************************************************/

Static ss_filhdl chksys(fn_)
Char *fn_;
{
  /* file to check */
  /* special file handle */
  struct LOC_chksys V;
  ss_filhdl hdl = 0;   /* handle holder */


  V.fn = fn_;
  /* set not a special file */
  if (chkstr("_input                                  ", &V))
    hdl = inpfil;   /* check standard input */
  else if (chkstr("_output                                 ", &V))
    hdl = outfil;
  /* check standard output */
  return hdl;   /* return handle */

}



/*******************************************************************************

Write character to output file

Writes a single character to the output file. Used to write to the output file
directly, instead of via the paslib functions.

*******************************************************************************/

Static Void wrtchr(c)
Char c;
{
  chrbuf[0] = c;   /* place character in buffer */
  ss_write(trmfil, chrbuf);   /* output character */

/* p2c: ansilib.pas, line 459:
 * Warning: Symbol 'SS_WRITE' is not defined [221] */
}



/*******************************************************************************

Write string to output file

Writes a string to the output file.

*******************************************************************************/

Static Void wrtstr(s)
Char *s;
{
  long i;   /* index for string */


  for (i = 0; i <= (Char *)(s - 1); i++)   /* output characters */
    wrtchr(s[i]);

}


#define maxpwr          1000000000L   /* maximum power that fits in integer */



/*******************************************************************************

Write integer to output file

Writes a simple unsigned integer to the output file.

*******************************************************************************/

Static Void wrtint(i)
long i;
{
  long p = maxpwr;   /* power holder */
  Char digit;   /* digit holder */
  boolean leading = false;   /* leading digit flag */


  /* set power */
  /* set no leading digit encountered */
  while (p != 0) {  /* output digits */
    digit = (Char)(i / p % 10 + '0');   /* get next digit */
/* p2c: ansilib.pas, line 501:
 * Note: Using % for possibly-negative arguments [317] */
    p /= 10;   /* next digit */
    if (digit != '0' || p == 0)   /* leading digit or end of number */
      leading = true;
    /* set leading digit found */
    if (leading)   /* output digit */
      wrtchr(digit);

  }


}

#undef maxpwr



/*******************************************************************************

Translate colors code

Translates an idependent to a terminal specific primary color code for an
ANSI compliant terminal..

*******************************************************************************/

Static long colnum(c)
color c;
{
  long n;


  /* translate color number */
  switch (c) {   /* color */

  case black:
    n = 0;
    break;

  case white:
    n = 7;
    break;

  case red:
    n = 1;
    break;

  case green:
    n = 2;
    break;

  case blue:
    n = 4;
    break;

  case cyan:
    n = 6;
    break;

  case yellow:
    n = 3;
    break;

  case magenta:
    n = 5;

    break;
  }
  return n;   /* return number */

}



/*******************************************************************************

Basic terminal controls

These routines control the basic terminal functions. They exist just to
ecapsulate this information. All of these functions are specific to ANSI
compliant terminals.
ANSI is able to set more than one attribute at a time, but under windows 95
there are no two attributes that you can detect together ! This is because
win95 modifies the attributes quite a bit (there is no blink). This capability
can be replaced later if needed.
Other notes: underline only works on monocrome terminals. On color, it makes
the text turn blue.

*******************************************************************************/

/* clear screen and home cursor */

Static Void trm_clear()
{
  wrtstr("\\esc[2J                                 ");
}


/* home cursor */

Static Void trm_home()
{
  wrtstr("\\esc[H                                  ");
}


/* move cursor up */

Static Void trm_up()
{
  wrtstr("\\esc[A                                  ");
}


/* move cursor down */

Static Void trm_down()
{
  wrtstr("\\esc[B                                  ");
}


/* move cursor left */

Static Void trm_left()
{
  wrtstr("\\esc[D                                  ");
}


/* move cursor right */

Static Void trm_right()
{
  wrtstr("\\esc[C                                  ");
}


/* turn on blink attribute */

Static Void trm_blink()
{
  wrtstr("\\esc[5m                                 ");
}


/* turn on reverse video */

Static Void trm_rev()
{
  wrtstr("\\esc[7m                                 ");
}


/* turn on underline */

Static Void trm_undl()
{
  wrtstr("\\esc[4m                                 ");
}


/* turn on bold attribute */

Static Void trm_bold()
{
  wrtstr("\\esc[1m                                 ");
}


/* turn off all attributes */

Static Void trm_attroff()
{
  wrtstr("\\esc[0m                                 ");
}


/* turn on cursor wrap */

Static Void trm_wrapon()
{
  wrtstr("\\esc[=7h                                ");
}


/* turn off cursor wrap */

Static Void trm_wrapoff()
{
  wrtstr("\\esc[=7l                                ");
}


/* position cursor */

Static Void trm_cursor(x, y)
long x, y;
{
  wrtstr("\\esc[                                   ");
  wrtint(y);
  wrtstr(";                                       ");
  wrtint(x);

  wrtstr("H                                       ");
}


/* set foreground color */

Static Void trm_fcolor(c)
color c;
{
  wrtstr("\\esc[                                   ");
  wrtint(colnum(c) + 30);
  wrtstr("m                                       ");
}


/* set background color */

Static Void trm_bcolor(c)
color c;
{
  wrtstr("\\esc[                                   ");
  wrtint(colnum(c) + 40);
  wrtstr("m                                       ");
}



/*******************************************************************************

Set attribute from attribute code

Accepts a "universal" attribute code, and executes the attribute set required
to make that happen onscreen.

*******************************************************************************/

Static Void setattr(a)
scnatt a;
{
  switch (a) {   /* attribute */

  case sanone:   /* no attribute */
    trm_attroff();
    break;

  case sablink:   /* blinking text (foreground) */
    trm_blink();
    break;

  case sarev:   /* reverse video */
    trm_rev();
    break;

  case saundl:   /* underline */
    trm_undl();
    break;

  case sasuper:   /* superscript */
    break;

  case sasubs:   /* subscripting */
    break;

  case saital:   /* italic text */
    break;

  case sabold:   /* bold text */
    trm_bold();
    break;

  }

}



/*******************************************************************************

Clear screen buffer

Clears the entire screen buffer to spaces with the current colors and
attributes.

*******************************************************************************/

Static Void clrbuf()
{
  long x, y;   /* screen indexes */
  scnrec *WITH;


  /* clear the screen buffer */
  for (y = 0; y < maxyd; y++) {
    for (x = 0; x < maxxd; x++) {
      WITH = &screens[curscn-1]->buf[y][x];


      WITH->ch = ' ';   /* clear to spaces */
      WITH->forec = screens[curscn-1]->forec;
      WITH->backc = screens[curscn-1]->backc;
      WITH->attr = screens[curscn-1]->attr;

    }
  }
}



/*******************************************************************************

Initalize screen

Clears all the parameters in the present screen context, and updates the
display to match.

*******************************************************************************/

Static Void iniscn()
{
  screens[curscn-1]->cury = 1;   /* set cursor at home */
  screens[curscn-1]->curx = 1;
  /* these attributes and colors are pretty much windows 95 specific. The
     bazzare setting of "blink" actually allows access to bright white */
  screens[curscn-1]->forec = black;   /* set colors and attributes */
  screens[curscn-1]->backc = white;
  screens[curscn-1]->attr = sablink;
  screens[curscn-1]->scroll = true;   /* turn on autoscroll */
  clrbuf();   /* clear screen buffer with that */
  setattr(screens[curscn-1]->attr);   /* set current attribute */
  trm_fcolor(screens[curscn-1]->forec);   /* set current colors */
  trm_bcolor(screens[curscn-1]->backc);   /* clear screen, home cursor */

  trm_clear();
}



/*******************************************************************************

Restore screen

Updates all the buffer and screen parameters to the terminal.

*******************************************************************************/

Static Void restore()
{
  long xi, yi;   /* screen indexes */
  color fs, bs;   /* color saves */
  scnatt as;   /* attribute saves */
  scnrec *WITH;


  trm_home();   /* restore cursor to upper left to start */
  /* set colors and attributes */
  trm_fcolor(screens[curscn-1]->forec);   /* restore colors */
  trm_bcolor(screens[curscn-1]->backc);
  setattr(screens[curscn-1]->attr);   /* restore attributes */
  fs = screens[curscn-1]->forec;   /* save current colors and attributes */
  bs = screens[curscn-1]->backc;
  as = screens[curscn-1]->attr;
  /* copy buffer to screen */
  for (yi = 1; yi <= maxyd; yi++) {  /* lines */
    for (xi = 0; xi < maxxd; xi++) {   /* characters */
      WITH = &screens[curscn-1]->buf[yi-1][xi];

      /* for each new character, we compare the attributes and colors
         with what is set. if a new color or attribute is called for,
         we set that, and update the saves. this technique cuts down on
         the amount of output characters */
      if (WITH->forec != fs) {  /* new foreground color */
	trm_fcolor(WITH->forec);   /* set the new color */
	fs = WITH->forec;   /* set save */

      }

      if (WITH->backc != bs) {  /* new foreground color */
	trm_bcolor(WITH->backc);   /* set the new color */
	bs = WITH->backc;   /* set save */

      }

      if (WITH->attr != as)   /* now output the actual character */
      {  /* new attribute */
	setattr(WITH->attr);   /* set the new attribute */
	as = WITH->attr;   /* set save */

      }


      wrtchr(WITH->ch);
    }
    if (yi < maxyd)
      wrtstr("\\cr\\lf                                  ");
    /* output next line sequence on all lines but the last. this is
       because the last one would cause us to scroll */

  }

  /* restore cursor positition */
  trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
  trm_fcolor(screens[curscn-1]->forec);   /* restore colors */
  trm_bcolor(screens[curscn-1]->backc);   /* restore attributes */

  setattr(screens[curscn-1]->attr);
}



/*******************************************************************************

Scroll screen

Scrolls the ANSI terminal screen by deltas in any given direction. For an ANSI
terminal, we special case any scroll that is downward only, without any
movement in x. These are then done by an arbitrary number of line feeds
executed at the bottom of the screen.
For all other scrolls, we do this by completely refreshing the contents of the
screen, including blank lines or collumns for the "scrolled in" areas. The
blank areas are all given the current attributes and colors.
The cursor allways remains in place for these scrolls, even though the text
is moving under it.

*******************************************************************************/

Static Void scrolls(x, y)
long x, y;
{
  long xi, yi;   /* screen counters */
  color fs, bs;   /* color saves */
  scnatt as;   /* attribute saves */
  scnbuf scnsav;   /* full screen buffer save */
  long lx;   /* last unmatching character index */
  boolean m;   /* match flag */
  long FORLIM, FORLIM1;
  scnrec *WITH;


  if (y > 0 && x == 0) {
    /* downward straight scroll, we can do this with native scrolling */
    trm_cursor(1L, (long)maxyd);   /* position to bottom of screen */
    /* use linefeed to scroll. linefeeds work no matter the state of
       wrap, and use whatever the current background color is */
    yi = y;   /* set line count */
    while (yi > 0) {  /* scroll down requested lines */
/* p2c: ansilib.pas, line 825:
 * Warning: Char constant with more than one character [154] */
      wrtchr('\\');   /* scroll down */
      yi--;   /* count lines */

    }

    /* restore cursor positition */
    trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
    /* now, adjust the buffer to be the same */
    for (yi = 0; yi <= maxyd - 2; yi++) {   /* move any lines up */
      if (yi + y + 1 <= maxyd) {   /* still within buffer */
	/* move lines up */
	memcpy(screens[curscn-1]->buf[yi], screens[curscn-1]->buf[yi + y],
	       maxxd * sizeof(scnrec));
      }
    }
    for (yi = maxyd - y; yi < maxyd; yi++) {   /* clear blank lines at end */
      for (xi = 0; xi < maxxd; xi++) {
	WITH = &screens[curscn-1]->buf[yi][xi];


	WITH->ch = ' ';   /* clear to blanks at colors and attributes */
	WITH->forec = screens[curscn-1]->forec;
	WITH->backc = screens[curscn-1]->backc;
	WITH->attr = screens[curscn-1]->attr;

      }
    }
    return;
  }

  /* when the scroll is arbitrary, we do it by completely refreshing the
     contents of the screen from the buffer */
  if (x <= -maxxd || x >= maxxd || y <= -maxyd || y >= maxyd) {
    /* scroll would result in complete clear, do it */
    trm_clear();   /* scroll would result in complete clear, do it */
    clrbuf();   /* clear the screen buffer */
    /* restore cursor positition */

    trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
    /* odd direction scroll */


    return;
  }

  /* true scroll is done in two steps. first, the contents of the buffer
     are adjusted to read as after the scroll. then, the contents of the
     buffer are output to the terminal. before the buffer is changed,
     we perform a full save of it, which then represents the "current"
     state of the real terminal. then, the new buffer contents are
     compared to that while being output. this saves work when most of
     the screen is spaces anyways */
  memcpy(scnsav, screens[curscn-1]->buf, sizeof(scnbuf));
      /* save the entire buffer */
  if (y > 0) {  /* move text up */
    for (yi = 0; yi <= maxyd - 2; yi++) {   /* move any lines up */
      if (yi + y + 1 <= maxyd) {   /* still within buffer */
	/* move lines up */
	memcpy(screens[curscn-1]->buf[yi], screens[curscn-1]->buf[yi + y],
	       maxxd * sizeof(scnrec));
      }
    }
    for (yi = maxyd - y; yi < maxyd; yi++) {   /* clear blank lines at end */
      for (xi = 0; xi < maxxd; xi++) {
	WITH = &screens[curscn-1]->buf[yi][xi];


	WITH->ch = ' ';   /* clear to blanks at colors and attributes */
	WITH->forec = screens[curscn-1]->forec;
	WITH->backc = screens[curscn-1]->backc;
	WITH->attr = screens[curscn-1]->attr;

      }
    }
  }

  else if (y < 0) {  /* move text down */

    for (yi = maxyd - 1; yi >= 1; yi--) {   /* move any lines up */
      if (yi + y + 1 >= 1) {   /* still within buffer */
	/* move lines up */
	memcpy(screens[curscn-1]->buf[yi], screens[curscn-1]->buf[yi + y],
	       maxxd * sizeof(scnrec));
      }
    }
    FORLIM = labs(y);
    for (yi = 0; yi < FORLIM; yi++) {   /* clear blank lines at start */
      for (xi = 0; xi < maxxd; xi++) {
	WITH = &screens[curscn-1]->buf[yi][xi];


	WITH->ch = ' ';   /* clear to blanks at colors and attributes */
	WITH->forec = screens[curscn-1]->forec;
	WITH->backc = screens[curscn-1]->backc;
	WITH->attr = screens[curscn-1]->attr;

      }
    }
  }
  if (x > 0) {  /* move text left */
    for (yi = 0; yi < maxyd; yi++) {  /* move text left */
      for (xi = 0; xi <= maxxd - 2; xi++) {   /* move left */
	if (xi + x + 1 <= maxxd) {   /* still within buffer */
	  /* move characters left */
	  screens[curscn-1]->buf[yi][xi] = screens[curscn-1]->buf[yi][xi + x];
	}
      }
      /* clear blank spaces at right */
      for (xi = maxxd - x; xi < maxxd; xi++) {
	WITH = &screens[curscn-1]->buf[yi][xi];


	WITH->ch = ' ';   /* clear to blanks at colors and attributes */
	WITH->forec = screens[curscn-1]->forec;
	WITH->backc = screens[curscn-1]->backc;
	WITH->attr = screens[curscn-1]->attr;

      }
    }


  }

  else if (x < 0) {  /* move text right */

    for (yi = 0; yi < maxyd; yi++) {  /* move text right */
      for (xi = maxxd - 1; xi >= 1; xi--) {   /* move right */
	if (xi + x + 1 >= 1) {   /* still within buffer */
	  /* move characters left */
	  screens[curscn-1]->buf[yi][xi] = screens[curscn-1]->buf[yi][xi + x];
	}
      }
      FORLIM1 = labs(x);
      /* clear blank spaces at left */
      for (xi = 0; xi < FORLIM1; xi++) {
	WITH = &screens[curscn-1]->buf[yi][xi];


	WITH->ch = ' ';   /* clear to blanks at colors and attributes */
	WITH->forec = screens[curscn-1]->forec;
	WITH->backc = screens[curscn-1]->backc;
	WITH->attr = screens[curscn-1]->attr;

      }
    }


  }
  /* the buffer is adjusted. now just copy the complete buffer to the
     screen */
  trm_home();   /* restore cursor to upper left to start */
  fs = screens[curscn-1]->forec;   /* save current colors and attributes */
  bs = screens[curscn-1]->backc;
  as = screens[curscn-1]->attr;
  for (yi = 0; yi < maxyd; yi++) {  /* lines */
    /* find the last unmatching character between real and new buffers.
       Then, we only need output the leftmost non-matching characters
       on the line. note that it does not really help us that characters
       WITHIN the line match, because a character output is as or more
       efficient as a cursor movement. if, however, you want to get
       SERIOUSLY complex, we could check runs of matching characters,
       then check if performing a direct cursor position is less output
       characters than just outputing data :) */
    lx = maxxd;   /* set to end */
    do {   /* check matches */

      m = true;   /* set match */
      /* check all elements match */
      if (screens[curscn-1]->buf[yi][lx-1].ch != scnsav[yi][lx-1].ch)
	m = false;
      if (screens[curscn-1]->buf[yi][lx-1].forec != scnsav[yi][lx-1].forec)
	m = false;
      if (screens[curscn-1]->buf[yi][lx-1].backc != scnsav[yi][lx-1].backc)
	m = false;
      if (screens[curscn-1]->buf[yi][lx-1].attr != scnsav[yi][lx-1].attr)
	m = false;
      if (m) {
	lx--;   /* next character */

      }
    } while (m && lx != 0);   /* until match or no more */
    for (xi = 0; xi < lx; xi++) {   /* characters */
      WITH = &screens[curscn-1]->buf[yi][xi];

      /* for each new character, we compare the attributes and colors
         with what is set. if a new color or attribute is called for,
         we set that, and update the saves. this technique cuts down on
         the amount of output characters */
      if (WITH->forec != fs) {  /* new foreground color */
	trm_fcolor(WITH->forec);   /* set the new color */
	fs = WITH->forec;   /* set save */

      }

      if (WITH->backc != bs) {  /* new foreground color */
	trm_bcolor(WITH->forec);   /* set the new color */
	bs = WITH->backc;   /* set save */

      }

      if (WITH->attr != as)   /* now output the actual character */
      {  /* new attribute */
	setattr(WITH->attr);   /* set the new attribute */
	as = WITH->attr;   /* set save */

      }


      wrtchr(WITH->ch);
    }
    if (yi + 1 < maxyd)
      wrtstr("\\cr\\lf                                  ");
    /* output next line sequence on all lines but the last. this is
       because the last one would cause us to scroll */

  }

  /* restore cursor positition */
  trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
  trm_fcolor(screens[curscn-1]->forec);   /* restore colors */
  trm_bcolor(screens[curscn-1]->backc);   /* restore attributes */

  setattr(screens[curscn-1]->attr);
  /* scroll */


}



/*******************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

Static Void iclear()
{
  trm_clear();   /* erase screen */
  clrbuf();   /* clear the screen buffer */
  screens[curscn-1]->cury = 1;   /* set cursor at home */
  screens[curscn-1]->curx = 1;

}



/*******************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

Static Void icursor(x, y)
long x, y;
{
  scncon *WITH;

  if (x < 1 || x > maxxd || y < 1 || y > maxyd) {
    error(einvpos);
    /* invalid position */

    return;
  }

  WITH = screens[curscn-1];   /* with current buffer */
  if (x == WITH->curx && y == WITH->cury)
    return;


  trm_cursor(x, y);   /* position cursor */
  WITH->cury = y;   /* set new position */
  WITH->curx = x;

}



/*******************************************************************************

Position cursor

This is the external interface to cursor.

*******************************************************************************/

Static Void cursor(f, x, y)
FILE **f;
long x, y;
{   /* position cursor */
  icursor(x, y);
}



/*******************************************************************************

Return maximum x demension

Returns the maximum x demension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

Static long maxx(f)
FILE **f;
{
  return maxxd;   /* set maximum x */

}



/*******************************************************************************

Return maximum y demension

Returns the maximum y demension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

Static long maxy(f)
FILE **f;
{
  return maxyd;   /* set maximum y */

}



/*******************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

Static Void home(f)
FILE **f;
{
  trm_home();   /* home cursor */
  screens[curscn-1]->cury = 1;   /* set cursor at home */
  screens[curscn-1]->curx = 1;

}



/*******************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

Static Void iup()
{
  if (screens[curscn-1]->cury > 1) {  /* not at top of screen */
    trm_up();   /* move up */
    screens[curscn-1]->cury--;   /* update position */

    return;
  }

  if (screens[curscn-1]->scroll)   /* scroll enabled */
    scrolls(0L, -1L);
  /* at top already, scroll up */
  else {
    screens[curscn-1]->cury = maxyd;   /* set new position */
    /* update on screen */

    trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
    /* wrap cursor around to screen bottom */


  }
}



/*******************************************************************************

Move cursor up

This is the external interface to up.

*******************************************************************************/

Static Void up(f)
FILE **f;
{   /* move up */
  iup();
}



/*******************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

Static Void idown()
{
  if (screens[curscn-1]->cury < maxyd) {  /* not at bottom of screen */
    trm_down();   /* move down */
    screens[curscn-1]->cury++;   /* update position */

    return;
  }

  if (screens[curscn-1]->scroll)   /* wrap enabled */
    scrolls(0L, 1L);
  /* already at bottom, scroll down */
  else {
    screens[curscn-1]->cury = 1;   /* set new position */
    /* update on screen */

    trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
    /* wrap cursor around to screen top */


  }
}



/*******************************************************************************

Move cursor down

This is the external interface to down.

*******************************************************************************/

Static Void down(f)
FILE **f;
{   /* move cursor down */
  idown();
}



/*******************************************************************************

Move cursor left internal

Moves the cursor one character left.

*******************************************************************************/

Static Void ileft()
{
  if (screens[curscn-1]->curx > 1) {  /* not at extreme left */
    trm_left();   /* move left */
    screens[curscn-1]->curx--;   /* update position */

    return;
  }

  iup();   /* move cursor up one line */
  screens[curscn-1]->curx = maxxd;   /* set cursor to extreme right */
  /* position on screen */

  trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
  /* wrap cursor motion */


}



/*******************************************************************************

Move cursor left

This is the external interface to left.

*******************************************************************************/

Static Void left(f)
FILE **f;
{   /* move cursor left */
  ileft();
}



/*******************************************************************************

Move cursor right internal

Moves the cursor one character right.

*******************************************************************************/

Static Void iright()
{
  if (screens[curscn-1]->curx < maxxd) {  /* not at extreme right */
    trm_right();   /* move right */
    screens[curscn-1]->curx++;   /* update position */

    return;
  }

  idown();   /* move cursor up one line */
  screens[curscn-1]->curx = 1;   /* set cursor to extreme left */
/* p2c: ansilib.pas, line 1297:
 * Warning: Char constant with more than one character [154] */
  /* position on screen */

  wrtchr('\\');
  /* wrap cursor motion */


}



/*******************************************************************************

Move cursor right

This is the external interface to right.

*******************************************************************************/

Static Void right(f)
FILE **f;
{   /* move cursor right */
  iright();
}



/*******************************************************************************

Turn on blink attribute

Turns on/off the blink attribute. Note that under windows 95 in a shell window,
blink does not mean blink, but instead "bright". We leave this alone because
we are supposed to also work over a com interface.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

Static Void blink(f, e)
FILE **f;
boolean e;
{
  trm_attroff();   /* turn off attributes */
  /* either on or off leads to blink, so we just do that */
  screens[curscn-1]->attr = sablink;   /* set attribute active */
  setattr(screens[curscn-1]->attr);   /* set current attribute */
  trm_fcolor(screens[curscn-1]->forec);   /* set current colors */

  trm_bcolor(screens[curscn-1]->backc);
}



/*******************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

Static Void reverse(f, e)
FILE **f;
boolean e;
{
  trm_attroff();   /* turn off attributes */
  if (e) {  /* reverse on */
    screens[curscn-1]->attr = sarev;   /* set attribute active */
    setattr(screens[curscn-1]->attr);   /* set current attribute */
    trm_fcolor(screens[curscn-1]->forec);   /* set current colors */

    trm_bcolor(screens[curscn-1]->backc);
    return;
  }

  screens[curscn-1]->attr = sablink;   /* set attribute active */
  setattr(screens[curscn-1]->attr);   /* set current attribute */
  trm_fcolor(screens[curscn-1]->forec);   /* set current colors */

  trm_bcolor(screens[curscn-1]->backc);
  /* turn it off */


}



/*******************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

Static Void underline(f, e)
FILE **f;
boolean e;
{
  trm_attroff();   /* turn off attributes */
  if (e) {  /* underline on */
    screens[curscn-1]->attr = saundl;   /* set attribute active */
    setattr(screens[curscn-1]->attr);   /* set current attribute */
    trm_fcolor(screens[curscn-1]->forec);   /* set current colors */

    trm_bcolor(screens[curscn-1]->backc);
    return;
  }

  screens[curscn-1]->attr = sablink;   /* set attribute active */
  setattr(screens[curscn-1]->attr);   /* set current attribute */
  trm_fcolor(screens[curscn-1]->forec);   /* set current colors */

  trm_bcolor(screens[curscn-1]->backc);
  /* turn it off */


}



/*******************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

Static Void superscript(f, e)
FILE **f;
boolean e;
{
  /* no capability */

}



/*******************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

Static Void subscript(f, e)
FILE **f;
boolean e;
{
  /* no capability */

}



/*******************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

Static Void italic(f, e)
FILE **f;
boolean e;
{
  /* no capability */

}



/*******************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

Static Void bold(f, e)
FILE **f;
boolean e;
{
  trm_attroff();   /* turn off attributes */
  if (e) {  /* bold on */
    screens[curscn-1]->attr = sabold;   /* set attribute active */
    setattr(screens[curscn-1]->attr);   /* set current attribute */
    trm_fcolor(screens[curscn-1]->forec);   /* set current colors */

    trm_bcolor(screens[curscn-1]->backc);
    return;
  }

  screens[curscn-1]->attr = sablink;   /* set attribute active */
  setattr(screens[curscn-1]->attr);   /* set current attribute */
  trm_fcolor(screens[curscn-1]->forec);   /* set current colors */

  trm_bcolor(screens[curscn-1]->backc);
  /* turn it off */


}



/*******************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

Static Void standout(f, e)
FILE **f;
boolean e;
{   /* implement as reverse */
  reverse(f, e);
}



/*******************************************************************************

Set foreground color

Sets the foreground (text) color from the universal primary code.

*******************************************************************************/

Static Void fcolor(f, c)
FILE **f;
color c;
{
  trm_fcolor(c);   /* set color */
  screens[curscn-1]->forec = c;

}



/*******************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

Static Void bcolor(f, c)
FILE **f;
color c;
{
  trm_bcolor(c);   /* set color */
  screens[curscn-1]->backc = c;

}



/*******************************************************************************

Enable/disable automatic scroll

Enables or disables automatic screen scroll. With automatic scroll on, moving
off the screen at the top or bottom will scroll up or down, respectively.

*******************************************************************************/

Static Void ascroll(f, e)
FILE **f;
boolean e;
{
  screens[curscn-1]->scroll = e;   /* set line wrap status */

}



/*******************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility. We don't have a capability for this.

*******************************************************************************/

Static Void curvis(f, e)
FILE **f;
boolean e;
{
  /* no capability */

}



/*******************************************************************************

Scroll screen

Process full delta scroll on screen. This is the external interface to this
function.

*******************************************************************************/

Static Void scroll(f, x, y)
FILE **f;
long x, y;
{   /* process scroll */
  scrolls(x, y);
}



/*******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

Static long curx(f)
FILE **f;
{
  return (screens[curscn-1]->curx);   /* return current location x */

}



/*******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

Static long cury(f)
FILE **f;
{
  return (screens[curscn-1]->cury);   /* return current location y */

}



/*******************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
then a new screen is allocated and cleared.
The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

*******************************************************************************/

Static Void select(f, s)
FILE **f;
long s;
{
  if (s < 1 || s > maxcon)   /* invalid screen number */
    error(einvscn);
  curscn = s;   /* set the current screen */
  if (screens[curscn-1] != NULL)   /* restore current screen */
    restore();
  else {
    screens[curscn-1] = (scncon *)Malloc(sizeof(scncon));
	/* get a new screen context */
    /* initalize that */

    iniscn();
    /* no current screen, create a new one */


  }
}



/*******************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attribute.

*******************************************************************************/

Static Void plcchr(c)
Char c;
{
  scnrec *WITH;

  /* handle special character cases first */
  if (c == '\\') {   /* carriage return, position to extreme left */
    icursor(1L, screens[curscn-1]->cury);
    return;
  }
/* p2c: ansilib.pas, line 1679:
 * Warning: Char constant with more than one character [154] */
  if (c == '\\') {   /* line feed, move down */
    idown();
    return;
  }
/* p2c: ansilib.pas, line 1681:
 * Warning: Char constant with more than one character [154] */
  if (c == '\\') {   /* back space, move left */
    ileft();
    return;
  }
/* p2c: ansilib.pas, line 1682:
 * Warning: Char constant with more than one character [154] */
  if (c == '\\') {   /* clear screen */
    iclear();
    return;
  }
/* p2c: ansilib.pas, line 1683:
 * Warning: Char constant with more than one character [154] */
  if (c < ' ' || c == '\177')
    return;


  /* normal character case, not control character */
  wrtchr(c);   /* output character to terminal */
  WITH = &screens[curscn-1]->buf[screens[curscn-1]->cury - 1]
    [screens[curscn-1]->curx - 1];
  /* update buffer */

  WITH->ch = c;   /* place character */
  WITH->forec = screens[curscn-1]->forec;   /* place colors */
  WITH->backc = screens[curscn-1]->backc;
  WITH->attr = screens[curscn-1]->attr;   /* place attribute */

  /* finish cursor right processing */
  if (screens[curscn-1]->curx < maxxd)   /* not at extreme right */
    screens[curscn-1]->curx++;   /* update position */
  else {
    iright();
    /* Wrap being off, ANSI left the cursor at the extreme right. So now
       we can process our own version of move right */

  }
}



/*******************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

Static Void del(f)
FILE **f;
{
  left(f);   /* back up cursor */
  plcchr(' ');   /* blank out */
  /* back up again */

  left(f);
}


/* Local variables for ievent: */
struct LOC_ievent {
  evtrec *er;
  boolean keep;   /* event keep flag */
/* p2c: ansilib.pas, line 1741:
 * Warning: Symbol 'SC_INPUT_RECORD' is not defined [221] */
  long ser;   /* windows event record */
} ;

/* Local variables for keyevent: */
struct LOC_keyevent {
  struct LOC_ievent *LINK;
} ;

Local boolean cntrl(LINK)
struct LOC_keyevent *LINK;
{
  return ((LINK->LINK->ser.controlkeystate.controlkeystate &
	   (sc_right_ctrl_pressed | sc_left_ctrl_pressed)) != 0);
/* p2c: ansilib.pas, line 1806:
 * Warning: No field called CONTROLKEYSTATE in that record [288] */
/* p2c: ansilib.pas, line 1806:
 * Warning: No field called CONTROLKEYSTATE in that record [288] */
/* p2c: ansilib.pas, line 1807:
 * Warning: Symbol 'SC_RIGHT_CTRL_PRESSED' is not defined [221] */
/* p2c: ansilib.pas, line 1807:
 * Warning: Symbol 'SC_LEFT_CTRL_PRESSED' is not defined [221] */

}

/* check shift key pressed */

Local boolean shift(LINK)
struct LOC_keyevent *LINK;
{
  return ((LINK->LINK->ser.controlkeystate.controlkeystate & sc_shift_pressed) != 0);
/* p2c: ansilib.pas, line 1817:
 * Warning: No field called CONTROLKEYSTATE in that record [288] */
/* p2c: ansilib.pas, line 1817:
 * Warning: No field called CONTROLKEYSTATE in that record [288] */
/* p2c: ansilib.pas, line 1817:
 * Warning: Symbol 'SC_SHIFT_PRESSED' is not defined [221] */

}

/*

Process keyboard event.
The events are mapped from IBM-PC keyboard keys as follows:

etup      up arrow            cursor up one line
etdown    down arrow          down one line
etleft    left arrow          left one character
etright   right arrow         right one character
etleftw   shift-left arrow    left one word
etrightw  shift-right arrow   right one word
ethome    cntrl-home          home of document
ethomes   shift-home          home of screen
ethomel   home                home of line
etend     cntrl-end           end of document
etends    shift-end           end of screen
etendl    end                 end of line
etscrl    cntrl-left arrow    scroll left one character
etscrr    cntrl-right arrow   scroll right one character
etscru    cntrl-up arrow      scroll up one line
etscrd    cntrl-down arrow    scroll down one line
etpagd    page down           page down
etpagu    page up             page up
ettab     tab                 tab
etenter   enter               enter line
etinsert  cntrl-insert        insert block
etinsertl shift-insert        insert line
etinsertt insert              insert toggle
etdel     cntrl-del           delete block
etdell    shift-del           delete line
etdelcf   del                 delete character forward
etdelcb   backspace           delete character backward
etcopy    cntrl-f1            copy block
etcopyl   shift-f1            copy line
etcan     esc                 cancel current operation
etstop    cntrl-s             stop current operation
etcont    cntrl-q             continue current operation
etprint   shift-f2            print document
etprintb  cntrl-f2            print block
etprints  cntrl-f3            print screen
etf1      f1                  function key 1
etf2      f2                  function key 2
etf3      f3                  function key 3
etf4      f4                  function key 4
etf5      f5                  function key 5
etf6      f6                  function key 6
etf7      f7                  function key 7
etf8      f8                  function key 8
etf9      f9                  function key 9
etf10     f10                 function key 10
etmenu    alt                 display menu
etend     cntrl-c             terminate program

*/

Local Void keyevent(LINK)
struct LOC_ievent *LINK;
{

  /* check control key pressed */
  struct LOC_keyevent V;


  V.LINK = LINK;
  /* we only take key down (pressed) events */
  if (LINK->ser.keydown.keydown == 0)
    return;
/* p2c: ansilib.pas, line 1824:
 * Warning: No field called KEYDOWN in that record [288] */
/* p2c: ansilib.pas, line 1824:
 * Warning: No field called KEYDOWN in that record [288] */


  /* Check character is non-zero. The zero character occurs
     whenever a control feature is hit. */
  if (LINK->ser.char.char.asciichar != 0) {
    if (LINK->ser.char.char.asciichar == "\\cr") {
/* p2c: ansilib.pas, line 1830:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1830:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1830:
 * Warning: No field called ASCIICHAR in that record [288] */
      LINK->er->etype = etenter;   /* set enter line */
    } else if (LINK->ser.char.char.asciichar == "\\bs") {
/* p2c: ansilib.pas, line 1832:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1832:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1832:
 * Warning: No field called ASCIICHAR in that record [288] */
      LINK->er->etype = etdelcb;   /* set delete character backwards */
    } else if (LINK->ser.char.char.asciichar == "\\ht") {
/* p2c: ansilib.pas, line 1834:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1834:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1834:
 * Warning: No field called ASCIICHAR in that record [288] */
      LINK->er->etype = ettab;   /* set tab */
    } else if (LINK->ser.char.char.asciichar == 'C' - 64) {
/* p2c: ansilib.pas, line 1836:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1836:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1836:
 * Warning: No field called ASCIICHAR in that record [288] */
      LINK->er->etype = etterm;   /* set end program */
    } else if (LINK->ser.char.char.asciichar == 'S' - 64) {
/* p2c: ansilib.pas, line 1838:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1838:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1838:
 * Warning: No field called ASCIICHAR in that record [288] */
      LINK->er->etype = etstop;   /* set stop program */
    } else if (LINK->ser.char.char.asciichar == 'Q' - 64) {
/* p2c: ansilib.pas, line 1840:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1840:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1840:
 * Warning: No field called ASCIICHAR in that record [288] */
      LINK->er->etype = etcont;   /* set continue program */
    } else {
      LINK->er->etype = etchar;   /* set character event */
      LINK->er->UU.char_ = LINK->ser.char.char.asciichar;
      /* normal character */

/* p2c: ansilib.pas, line 1845:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1845:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1845:
 * Warning: No field called ASCIICHAR in that record [288] */

    }
    LINK->keep = true;   /* set keep event */

    return;
  }
/* p2c: ansilib.pas, line 1828:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1828:
 * Warning: No field called CHAR in that record [288] */
/* p2c: ansilib.pas, line 1828:
 * Warning: No field called ASCIICHAR in that record [288] */

  if (LINK->ser.virtualkeycode.virtualkeycode > 0xff)
    return;
/* p2c: ansilib.pas, line 1850:
 * Warning: No field called VIRTUALKEYCODE in that record [288] */
/* p2c: ansilib.pas, line 1850:
 * Warning: No field called VIRTUALKEYCODE in that record [288] */
  /* limited to codes we can put in a set */
  if (LINK->ser.virtualkeycode.virtualkeycode != sc_vk_cancel &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_menu &&
      (LINK->ser.virtualkeycode.virtualkeycode < sc_vk_f1 ||
       LINK->ser.virtualkeycode.virtualkeycode > sc_vk_f10) &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_next &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_prior &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_delete &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_insert &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_down &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_up &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_right &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_left &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_end &&
      LINK->ser.virtualkeycode.virtualkeycode != sc_vk_home)
    return;
/* p2c: ansilib.pas, line 1852:
 * Warning: No field called VIRTUALKEYCODE in that record [288] */
/* p2c: ansilib.pas, line 1852:
 * Warning: No field called VIRTUALKEYCODE in that record [288] */
/* p2c: ansilib.pas, line 1853:
 * Warning: Symbol 'SC_VK_HOME' is not defined [221] */
/* p2c: ansilib.pas, line 1853:
 * Warning: Symbol 'SC_VK_END' is not defined [221] */
/* p2c: ansilib.pas, line 1853:
 * Warning: Symbol 'SC_VK_LEFT' is not defined [221] */
/* p2c: ansilib.pas, line 1853:
 * Warning: Symbol 'SC_VK_RIGHT' is not defined [221] */
/* p2c: ansilib.pas, line 1854:
 * Warning: Symbol 'SC_VK_UP' is not defined [221] */
/* p2c: ansilib.pas, line 1854:
 * Warning: Symbol 'SC_VK_DOWN' is not defined [221] */
/* p2c: ansilib.pas, line 1854:
 * Warning: Symbol 'SC_VK_INSERT' is not defined [221] */
/* p2c: ansilib.pas, line 1854:
 * Warning: Symbol 'SC_VK_DELETE' is not defined [221] */
/* p2c: ansilib.pas, line 1855:
 * Warning: Symbol 'SC_VK_PRIOR' is not defined [221] */
/* p2c: ansilib.pas, line 1855:
 * Warning: Symbol 'SC_VK_NEXT' is not defined [221] */
/* p2c: ansilib.pas, line 1855:
 * Warning: Symbol 'SC_VK_F1' is not defined [221] */
/* p2c: ansilib.pas, line 1855:
 * Warning: Symbol 'SC_VK_F10' is not defined [221] */
/* p2c: ansilib.pas, line 1856:
 * Warning: Symbol 'SC_VK_MENU' is not defined [221] */
/* p2c: ansilib.pas, line 1856:
 * Warning: Symbol 'SC_VK_CANCEL' is not defined [221] */


  /* this key is one we process */
/* p2c: ansilib.pas, line 1859:
 * Warning: No field called VIRTUALKEYCODE in that record [288] */
/* p2c: ansilib.pas, line 1859:
 * Warning: No field called VIRTUALKEYCODE in that record [288] */
  switch (LINK->ser.virtualkeycode.virtualkeycode) {   /* key */

  case sc_vk_home:  /* home */
    if (cntrl(&V))
      LINK->er->etype = ethome;   /* home document */
    else if (shift(&V))
      LINK->er->etype = ethomes;   /* home screen */
    else {
      LINK->er->etype = ethomel;   /* home line */

    }
    break;
/* p2c: ansilib.pas, line 1861:
 * Warning: Symbol 'SC_VK_HOME' is not defined [221] */

  case sc_vk_end:  /* end */
    if (cntrl(&V))
      LINK->er->etype = etend;   /* end document */
    else if (shift(&V))
      LINK->er->etype = etends;   /* end screen */
    else {
      LINK->er->etype = etendl;   /* end line */

    }
    break;
/* p2c: ansilib.pas, line 1868:
 * Warning: Symbol 'SC_VK_END' is not defined [221] */

  case sc_vk_up:  /* up */
    if (cntrl(&V))
      LINK->er->etype = etscru;   /* scroll up */
    else {
      LINK->er->etype = etup;   /* up line */

    }
    break;
/* p2c: ansilib.pas, line 1875:
 * Warning: Symbol 'SC_VK_UP' is not defined [221] */

  case sc_vk_down:  /* down */
    if (cntrl(&V))
      LINK->er->etype = etscrd;   /* scroll down */
    else {
      LINK->er->etype = etdown;   /* up line */

    }
    break;
/* p2c: ansilib.pas, line 1881:
 * Warning: Symbol 'SC_VK_DOWN' is not defined [221] */

  case sc_vk_left:  /* left */
    if (cntrl(&V))
      LINK->er->etype = etscrl;   /* scroll left one character */
    else if (shift(&V))
      LINK->er->etype = etleftw;   /* left one word */
    else {
      LINK->er->etype = etleft;   /* left one character */

    }
    break;
/* p2c: ansilib.pas, line 1887:
 * Warning: Symbol 'SC_VK_LEFT' is not defined [221] */

  case sc_vk_right:  /* right */
    if (cntrl(&V))
      LINK->er->etype = etscrr;   /* scroll right one character */
    else if (shift(&V))
      LINK->er->etype = etrightw;   /* right one word */
    else {
      LINK->er->etype = etright;   /* left one character */

    }
    break;
/* p2c: ansilib.pas, line 1894:
 * Warning: Symbol 'SC_VK_RIGHT' is not defined [221] */

  case sc_vk_right:  /* right */
    if (cntrl(&V))
      LINK->er->etype = etscrr;   /* scroll right one character */
    else if (shift(&V))
      LINK->er->etype = etrightw;   /* right one word */
    else {
      LINK->er->etype = etright;   /* left one character */

    }
    break;
/* p2c: ansilib.pas, line 1901:
 * Warning: Symbol 'SC_VK_RIGHT' is not defined [221] */

  case sc_vk_insert:  /* insert */
    if (cntrl(&V))
      LINK->er->etype = etinsert;   /* insert block */
    else if (shift(&V))
      LINK->er->etype = etinsertl;   /* insert line */
    else {
      LINK->er->etype = etinsertt;   /* insert toggle */

    }
    break;
/* p2c: ansilib.pas, line 1908:
 * Warning: Symbol 'SC_VK_INSERT' is not defined [221] */

  case sc_vk_delete:  /* delete */
    if (cntrl(&V))
      LINK->er->etype = etdel;   /* delete block */
    else if (shift(&V))
      LINK->er->etype = etdell;   /* delete line */
    else {
      LINK->er->etype = etdelcf;   /* insert toggle */

    }
    break;
/* p2c: ansilib.pas, line 1915:
 * Warning: Symbol 'SC_VK_DELETE' is not defined [221] */

  case sc_vk_prior:   /* page up */
/* p2c: ansilib.pas, line 1922:
 * Warning: Symbol 'SC_VK_PRIOR' is not defined [221] */
    LINK->er->etype = etpagu;
    break;

  case sc_vk_next:   /* page down */
/* p2c: ansilib.pas, line 1923:
 * Warning: Symbol 'SC_VK_NEXT' is not defined [221] */
    LINK->er->etype = etpagd;
    break;

  case sc_vk_f1:  /* f1 */
    if (cntrl(&V))
      LINK->er->etype = etcopy;   /* copy block */
    else if (shift(&V))
      LINK->er->etype = etcopyl;   /* copy line */
    else {
      LINK->er->etype = etf1;   /* f1 */

    }
    break;
/* p2c: ansilib.pas, line 1924:
 * Warning: Symbol 'SC_VK_F1' is not defined [221] */

  case sc_vk_f2:  /* f2 */
    if (cntrl(&V))
      LINK->er->etype = etprintb;   /* print block */
    else if (shift(&V))
      LINK->er->etype = etprint;   /* print document */
    else {
      LINK->er->etype = etf2;   /* f2 */

    }
    break;
/* p2c: ansilib.pas, line 1931:
 * Warning: Symbol 'SC_VK_F2' is not defined [221] */

  case sc_vk_f3:  /* f3 */
    if (cntrl(&V))
      LINK->er->etype = etprints;   /* print screen */
    else {
      LINK->er->etype = etf3;   /* f3 */

    }
    break;
/* p2c: ansilib.pas, line 1938:
 * Warning: Symbol 'SC_VK_F3' is not defined [221] */

  case sc_vk_f4:   /* f4 */
/* p2c: ansilib.pas, line 1944:
 * Warning: Symbol 'SC_VK_F4' is not defined [221] */
    LINK->er->etype = etf4;
    break;

  case sc_vk_f5:   /* f5 */
/* p2c: ansilib.pas, line 1945:
 * Warning: Symbol 'SC_VK_F5' is not defined [221] */
    LINK->er->etype = etf5;
    break;

  case sc_vk_f6:   /* f6 */
/* p2c: ansilib.pas, line 1946:
 * Warning: Symbol 'SC_VK_F6' is not defined [221] */
    LINK->er->etype = etf6;
    break;

  case sc_vk_f7:   /* f7 */
/* p2c: ansilib.pas, line 1947:
 * Warning: Symbol 'SC_VK_F7' is not defined [221] */
    LINK->er->etype = etf7;
    break;

  case sc_vk_f8:   /* f8 */
/* p2c: ansilib.pas, line 1948:
 * Warning: Symbol 'SC_VK_F8' is not defined [221] */
    LINK->er->etype = etf8;
    break;

  case sc_vk_f9:   /* f9 */
/* p2c: ansilib.pas, line 1949:
 * Warning: Symbol 'SC_VK_F9' is not defined [221] */
    LINK->er->etype = etf9;
    break;

  case sc_vk_f10:   /* f10 */
/* p2c: ansilib.pas, line 1950:
 * Warning: Symbol 'SC_VK_F10' is not defined [221] */
    LINK->er->etype = etf10;
    break;

  case sc_vk_menu:   /* alt */
/* p2c: ansilib.pas, line 1951:
 * Warning: Symbol 'SC_VK_MENU' is not defined [221] */
    LINK->er->etype = etmenu;
    break;

  case sc_vk_cancel:   /* ctl-brk */
/* p2c: ansilib.pas, line 1952:
 * Warning: Symbol 'SC_VK_CANCEL' is not defined [221] */
    LINK->er->etype = etterm;
    break;

  }
  LINK->keep = true;   /* set keep event */

}

/*

Process mouse event.
Buttons are assigned to Win 95 as follows:

button 1: Windows left button
button 2: Windows right button
button 3: Windows 2nd from left button
button 4: Windows 3rd from left button

The Windows 4th from left button is unused. The double click events will
end up, as, well, two clicks of a single button, displaying my utter
contempt for the whole double click concept.

*/

/* update mouse parameters */

Local Void mouseupdate(LINK)
struct LOC_ievent *LINK;
{
  /* we prioritize events by: movements 1st, button clicks 2nd */
  if (nmpx != mpx || nmpy != mpy) {  /* create movement event */
    LINK->er->etype = etmoumov;   /* set movement event */
    LINK->er->UU.U55.moupx = nmpx;   /* set new mouse position */
    LINK->er->UU.U55.moupy = nmpy;
    mpx = nmpx;   /* save new position */
    mpy = nmpy;
    LINK->keep = true;   /* set to keep */

    return;
  }

  if (nmb1 > mb1) {
    LINK->er->etype = etmoub1a;   /* button 1 assert */
    mb1 = nmb1;   /* update status */
    LINK->keep = true;   /* set to keep */

    return;
  }

  if (nmb2 > mb2) {
    LINK->er->etype = etmoub2a;   /* button 2 assert */
    mb2 = nmb2;   /* update status */
    LINK->keep = true;   /* set to keep */

    return;
  }

  if (nmb3 > mb3) {
    LINK->er->etype = etmoub3a;   /* button 3 assert */
    mb3 = nmb3;   /* update status */
    LINK->keep = true;   /* set to keep */

    return;
  }

  if (nmb4 > mb4) {
    LINK->er->etype = etmoub4a;   /* button 4 assert */
    mb4 = nmb4;   /* update status */
    LINK->keep = true;   /* set to keep */

    return;
  }

  if (nmb1 < mb1) {
    LINK->er->etype = etmoub1d;   /* button 1 deassert */
    mb1 = nmb1;   /* update status */
    LINK->keep = true;   /* set to keep */

    return;
  }

  if (nmb2 < mb2) {
    LINK->er->etype = etmoub2d;   /* button 2 deassert */
    mb2 = nmb2;   /* update status */
    LINK->keep = true;   /* set to keep */

    return;
  }

  if (nmb3 < mb3) {
    LINK->er->etype = etmoub3d;   /* button 3 deassert */
    mb3 = nmb3;   /* update status */
    LINK->keep = true;   /* set to keep */

    return;
  }

  if (nmb4 >= mb4)
    return;


  LINK->er->etype = etmoub4d;   /* button 4 deassert */
  mb4 = nmb4;   /* update status */
  LINK->keep = true;   /* set to keep */

}

/* register mouse status */

Local Void mouseevent(LINK)
struct LOC_ievent *LINK;
{
  /* gather a new mouse status */
  nmpx = LINK->ser.mouseposition.mouseposition.x + 1;
      /* get mouse position */
/* p2c: ansilib.pas, line 2054:
 * Warning: No field called MOUSEPOSITION in that record [288] */
/* p2c: ansilib.pas, line 2054:
 * Warning: No field called MOUSEPOSITION in that record [288] */
/* p2c: ansilib.pas, line 2054:
 * Warning: No field called X in that record [288] */
  nmpy = LINK->ser.mouseposition.mouseposition.y + 1;
/* p2c: ansilib.pas, line 2055:
 * Warning: No field called MOUSEPOSITION in that record [288] */
/* p2c: ansilib.pas, line 2055:
 * Warning: No field called MOUSEPOSITION in that record [288] */
/* p2c: ansilib.pas, line 2055:
 * Warning: No field called Y in that record [288] */
  nmb1 = ((LINK->ser.buttonstate.buttonstate & sc_from_left_1st_button_pressed) !=
	  0);
/* p2c: ansilib.pas, line 2056:
 * Warning: No field called BUTTONSTATE in that record [288] */
/* p2c: ansilib.pas, line 2056:
 * Warning: No field called BUTTONSTATE in that record [288] */
/* p2c: ansilib.pas, line 2056:
 * Warning: Symbol 'SC_FROM_LEFT_1ST_BUTTON_PRESSED' is not defined [221] */
  nmb2 = ((LINK->ser.buttonstate.buttonstate & sc_rightmost_button_pressed) != 0);
/* p2c: ansilib.pas, line 2057:
 * Warning: No field called BUTTONSTATE in that record [288] */
/* p2c: ansilib.pas, line 2057:
 * Warning: No field called BUTTONSTATE in that record [288] */
/* p2c: ansilib.pas, line 2057:
 * Warning: Symbol 'SC_RIGHTMOST_BUTTON_PRESSED' is not defined [221] */
  nmb3 = ((LINK->ser.buttonstate.buttonstate & sc_from_left_2nd_button_pressed) !=
	  0);
/* p2c: ansilib.pas, line 2058:
 * Warning: No field called BUTTONSTATE in that record [288] */
/* p2c: ansilib.pas, line 2058:
 * Warning: No field called BUTTONSTATE in that record [288] */
/* p2c: ansilib.pas, line 2058:
 * Warning: Symbol 'SC_FROM_LEFT_2ND_BUTTON_PRESSED' is not defined [221] */
  nmb4 = ((LINK->ser.buttonstate.buttonstate & sc_from_left_3rd_button_pressed) !=
	  0);
/* p2c: ansilib.pas, line 2059:
 * Warning: No field called BUTTONSTATE in that record [288] */
/* p2c: ansilib.pas, line 2059:
 * Warning: No field called BUTTONSTATE in that record [288] */
/* p2c: ansilib.pas, line 2059:
 * Warning: Symbol 'SC_FROM_LEFT_3RD_BUTTON_PRESSED' is not defined [221] */

}



/*******************************************************************************

Aquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

*******************************************************************************/

Static Void ievent(er_)
evtrec *er_;
{  /* event */
  struct LOC_ievent V;
  long r;   /* function return value */


  V.er = er_;
  do {

    V.keep = false;   /* set don't keep by default */
    mouseupdate(&V);   /* check any mouse details need processing */
    if (!V.keep)   /* until an event we want occurs */
    {  /* no, go ahead with event read */
      r = sc_readconsoleinputa(inphdl, V.ser);   /* get the next event */
/* p2c: ansilib.pas, line 2071:
 * Warning: Symbol 'SC_READCONSOLEINPUTA' is not defined [221] */
      if (r == 1) {  /* process valid event */
	/* decode by event */
	if (V.ser.eventtype == sc_key_event)   /* key event */
	  keyevent(&V);
/* p2c: ansilib.pas, line 2075:
 * Warning: No field called EVENTTYPE in that record [288] */
/* p2c: ansilib.pas, line 2075:
 * Warning: Symbol 'SC_KEY_EVENT' is not defined [221] */
	else if (V.ser.eventtype == sc_mouse_event) {   /* mouse event */
/* p2c: ansilib.pas, line 2076:
 * Warning: No field called EVENTTYPE in that record [288] */
/* p2c: ansilib.pas, line 2076:
 * Warning: Symbol 'SC_MOUSE_EVENT' is not defined [221] */
	  /* ansi mode has no window size, menu or focus events */

	  mouseevent(&V);
	}
      }


    }



  } while (!V.keep);   /* event */
}


/* external event interface */

Static Void event(f, er)
FILE **f;
evtrec *er;
{   /* process event */
  ievent(er);
}



/*******************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.
Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it sends a timer mature event to
the associated input file.
Note that it is an error to set a timer without repeat, then kill that same
timer. This is because the kill call could reference an event that has expired.
Note: timers are not implemented. I tried this under win95, it was
uncooperative.

*******************************************************************************/

Static Void timer(f, i, t, r)
FILE **f;
timhan i;
long t;
boolean r;
{   /* no timers available */
  /* file to send event to */
  /* timer handle */
  /* number of milliseconds to run */
  /* timer is to rerun after completion */
  error(etimacc);
}



/*******************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.
Note: timers are not implemented. I tried this under win95, it was
uncooperative.

*******************************************************************************/

Static Void killtimer(f, i)
FILE **f;
timhan i;
{   /* no timers available */
  /* file to kill timer on */
  /* handle of timer */
  error(etimacc);
}



/*******************************************************************************

Return mouse existance

Returns true if a mouse is implemented, which it is.

*******************************************************************************/

Static boolean mouse(f)
FILE **f;
{
  return true;   /* set mouse exists */

}



/*******************************************************************************

Return number of joysticks

Return number of joysticks attached.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

Static joynum joystick(f)
FILE **f;
{
  return 0;   /* none */

}



/*******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

Static joybtn joybutton(f, j)
FILE **f;
joyhan j;
{
  error(ejoyacc);   /* there are no joysticks */
  return 0;   /* shut up compiler */

}



/*******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodementional
joystick can be considered a slider without positional meaning.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

Static joyaxn joyaxis(f, j)
FILE **f;
joyhan j;
{
  error(ejoyacc);   /* there are no joysticks */
  return 0;   /* shut up compiler */

}



/*******************************************************************************

Process input line

Reads an input line with full echo and editting. The line is placed into the
input line buffer.
The upgrade for this is to implement a full set of editting features.

*******************************************************************************/

Static Void readline()
{
  evtrec er;   /* event record */


  inpptr = 1;   /* set 1st character position */
  do {   /* get line characters */

    /* get events until an "interesting" event occurs */
    do {
      ievent(&er);
    } while (er.etype != (int)etdelcb && er.etype != (int)etterm &&
	     er.etype != (int)etenter && er.etype != (int)etchar);
    /* if the event is line enter, place carriage return code,
       otherwise place real character. note that we emulate a
       terminal and return cr only, which is handled as appropriate
       by a higher level. if the event is program terminate, then we
       execute an organized halt */
    switch (er.etype) {   /* event */

    case etterm:   /* halt program */
      _Escape(0);
      break;

    case etenter:  /* line terminate */
      inpbuf[inpptr-1] = '\\';   /* return cr */
/* p2c: ansilib.pas, line 2248:
 * Warning: Char constant with more than one character [154] */
/* p2c: ansilib.pas, line 2249:
 * Warning: Char constant with more than one character [154] */
      plcchr('\\');   /* output newline sequence */
/* p2c: ansilib.pas, line 2250:
 * Warning: Char constant with more than one character [154] */

      plcchr('\\');
      break;

    case etchar:  /* character */
      if (inpptr < maxlin) {
	inpbuf[inpptr-1] = er.UU.char_;   /* place real character */
	/* echo the character */

	plcchr(er.UU.char_);
      }

      if (inpptr < maxlin) {
	inpptr++;   /* next character */

      }
      break;

    case etdelcb:  /* delete character backwards */
      if (inpptr > 1) {  /* not at extreme left */
/* p2c: ansilib.pas, line 2268:
 * Warning: Char constant with more than one character [154] */
	plcchr('\\');   /* backspace, spaceout then backspace again */
	plcchr(' ');
/* p2c: ansilib.pas, line 2270:
 * Warning: Char constant with more than one character [154] */
	plcchr('\\');
	inpptr--;   /* back up pointer */

      }


      break;


    }

  } while (er.etype != etenter);   /* until line terminate */
  inpptr = 1;   /* set 1st position on active line */

}



/*******************************************************************************

Open file for read

Opens the file by name, and returns the file handle. If the file is the
"_input" file, then we give it our special handle. Otherwise, the entire
processing of the file is passed onto the system level.
We handle the entire processing of the input file here, by processing the
event queue.

*******************************************************************************/

Static Void ts_openread(fn, nm)
ss_filhdl *fn;
Char *nm;
{
  Char *fs;   /* filename buffer pointer */


  remspc(nm, &fs);   /* remove leading and trailing spaces */
  *fn = chksys(fs);   /* check it's a system file */
  if (*fn != inpfil)   /* release temp string */
  {  /* not "_input", process pass-on */
    makfil(fn);   /* create new file slot */
    ss_openread(opnfil[*fn - 1], fs);   /* pass to lower level */

/* p2c: ansilib.pas, line 2309:
 * Warning: Symbol 'SS_OPENREAD' is not defined [221] */
  }


  Free(fs);
}



/*******************************************************************************

Open file for write

Opens the file by name, and returns the file handle. If the file is the
"_output" file, then we give it our special handle. Otherwise, the file entire
processing of the file is passed onto the system level.

*******************************************************************************/

Static Void ts_openwrite(fn, nm)
ss_filhdl *fn;
Char *nm;
{
  Char *fs;   /* filename buffer pointer */


  remspc(nm, &fs);   /* remove leading and trailing spaces */
  *fn = chksys(fs);   /* check it's a system file */
  if (*fn != outfil)   /* release temp string */
  {  /* not "_output", process pass-on */
    makfil(fn);   /* create new file slot */
    ss_openwrite(opnfil[*fn - 1], fs);   /* open at lower level */

/* p2c: ansilib.pas, line 2337:
 * Warning: Symbol 'SS_OPENWRITE' is not defined [221] */
  }


  Free(fs);
}



/*******************************************************************************

Close file

Closes the file. The file is closed at the system level, then we remove the
table entry for the file.

*******************************************************************************/

Static Void ts_close(fn)
ss_filhdl fn;
{
  if (fn <= outfil)  /* transparent file */
    return;


  if (opnfil[fn-1] == 0)   /* invalid handle */
    error(einvhan);
  ss_close(opnfil[fn-1]);   /* close at lower level */
/* p2c: ansilib.pas, line 2358:
 * Warning: Symbol 'SS_CLOSE' is not defined [221] */
  opnfil[fn-1] = 0;   /* clear out file table entry */

}



/*******************************************************************************

Read file

If the file is the input file, we process that by reading from the event queue
and returning any characters found. Any events besides character events are
discarded, which is why reading from the "input" file is a downward compatible
operation.
All other files are passed on to the system level.

*******************************************************************************/

Static Void ts_read(fn, ba)
ss_filhdl fn;
byte *ba;
{
  long i = 1;   /* index for destination */
  long l;   /* length left on destination */


  if (fn > outfil) {   /* transparent file */
    if (opnfil[fn-1] == 0)
      error(einvhan);
  }
  /* invalid handle */
  if (fn != inpfil) {  /* process input file */
    ss_read(opnfil[fn-1], ba);   /* pass to lower level */

/* p2c: ansilib.pas, line 2405:
 * Warning: Symbol 'SS_READ' is not defined [221] */
    return;
  }

  /* set 1st byte of destination */
  l = ba;   /* set length of destination */
  while (l > 0) {  /* read input bytes */
    /* if there is no line in the input buffer, get one */
    if (inpptr == 0)
      readline();
    ba[i-1] = inpbuf[inpptr-1];   /* get and place next character */
    if (inpptr < maxlin)   /* next */
      inpptr++;
    /* if we have just read the last of that line, then flag buffer empty */
    if (ba[i-1] == "\\cr")
      inpptr = 0;
    i++;   /* next character */
    l--;   /* count characters */

  }


}



/*******************************************************************************

Write file

Outputs to the given file. If the file is the "output" file, then we process
it specially.

*******************************************************************************/

Static Void ts_write(fn, ba)
ss_filhdl fn;
byte *ba;
{
  long i = 1;   /* index for destination */
  long l;   /* length left on destination */


  if (fn > outfil) {   /* transparent file */
    if (opnfil[fn-1] == 0)
      error(einvhan);
  }
  /* invalid handle */
  if (fn != outfil) {  /* process output file */
    ss_write(opnfil[fn-1], ba);   /* pass to lower level */

/* p2c: ansilib.pas, line 2439:
 * Warning: Symbol 'SS_WRITE' is not defined [221] */
    return;
  }

  /* set 1st byte of source */
  l = ba;   /* set length of source */
  while (l > 0) {  /* write output bytes */
    plcchr(ba[i-1]);   /* send character to terminal emulator */
    i++;   /* next character */
    l--;   /* count characters */

  }


}



/*******************************************************************************

Position file

Positions the given file. If the file is one of "input" or "output", we fail
call, as positioning is not valid on a special file.

*******************************************************************************/

Static Void ts_position(fn, p)
ss_filhdl fn;
long p;
{
  if (fn > outfil) {   /* transparent file */
    if (opnfil[fn-1] == 0)
      error(einvhan);
  }
  /* invalid handle */
  /* check operation performed on special file */
  if (fn == inpfil || fn == outfil)
    error(efilopr);
  ss_position(opnfil[fn-1], p);   /* pass to lower level */

/* p2c: ansilib.pas, line 2460:
 * Warning: Symbol 'SS_POSITION' is not defined [221] */
}



/*******************************************************************************

Find location of file

Find the location of the given file. If the file is one of "input" or "output",
we fail the call, as this is not valid on a special file.

*******************************************************************************/

Static long ts_location(fn)
ss_filhdl fn;
{
  if (fn > outfil) {   /* transparent file */
    if (opnfil[fn-1] == 0)
      error(einvhan);
  }
  /* invalid handle */
  /* check operation performed on special file */
  if (fn == inpfil || fn == outfil)
    error(efilopr);
  return ss_location(opnfil[fn-1]);   /* pass to lower level */

/* p2c: ansilib.pas, line 2481:
 * Warning: Symbol 'SS_LOCATION' is not defined [221] */
}



/*******************************************************************************

Find length of file

Find the length of the given file. If the file is one of "input" or "output",
we fail the call, as this is not valid on a special file.

*******************************************************************************/

Static long ts_length(fn)
ss_filhdl fn;
{
  if (fn > outfil) {   /* transparent file */
    if (opnfil[fn-1] == 0)
      error(einvhan);
  }
  /* invalid handle */
  /* check operation performed on special file */
  if (fn == inpfil || fn == outfil)
    error(efilopr);
  return ss_length(opnfil[fn-1]);   /* pass to lower level */

/* p2c: ansilib.pas, line 2502:
 * Warning: Symbol 'SS_LENGTH' is not defined [221] */
}



/*******************************************************************************

Check eof of file

Checks if a given file is at eof. On our special files "input" and "output",
the eof is allways false. On "input", there is no idea of eof on a keyboard
input file. On "output", eof is allways false on a write only file.

*******************************************************************************/

Static boolean ts_eof(fn)
ss_filhdl fn;
{
  if (fn > outfil) {   /* transparent file */
    if (opnfil[fn-1] == 0)
      error(einvhan);
  }
  /* invalid handle */
  /* check operation performed on special file */
  if (fn == inpfil || fn == outfil)
    return false;   /* yes, allways true */
  else {
    return ss_eof(opnfil[fn-1]);   /* pass to lower level */

/* p2c: ansilib.pas, line 2524:
 * Warning: Symbol 'SS_EOF' is not defined [221] */
  }
}



main(argc, argv)
int argc;
Char *argv[];
{
  PASCAL_MAIN(argc, argv);
  if (setjmp(_JL88))
    goto _L88;
  inphdl = sc_getstdhandle(sc_std_input_handle);
/* p2c: ansilib.pas, line 2528:
 * Warning: Symbol 'SC_STD_INPUT_HANDLE' is not defined [221] */
/* p2c: ansilib.pas, line 2528:
 * Warning: Symbol 'SC_GETSTDHANDLE' is not defined [221] */
  mb1 = false;   /* set mouse as assumed no buttons down, at origin */
  mb2 = false;
  mb3 = false;
  mb4 = false;
  mpx = 1;
  mpy = 1;
  nmb1 = false;
  nmb2 = false;
  nmb3 = false;
  nmb4 = false;
  nmpx = 1;
  nmpy = 1;
  inpptr = 0;   /* set no input line active */
  /* clear open files table */
  for (fi = 1; fi <= ss_maxhdl; fi++)   /* set unoccupied */
    opnfil[fi-1] = 0;
  ss_openwrite(trmfil, "_output");   /* open the output file */
/* p2c: ansilib.pas, line 2544:
 * Warning: Symbol 'SS_OPENWRITE' is not defined [221] */
  /* because this is an "open ended" (no feedback) emulation, we must bring
     the terminal to a known state */
  /* clear active screens */
  for (curscn = 1; curscn <= maxcon; curscn++)
    screens[curscn-1] = NULL;
  screens[0] = (scncon *)Malloc(sizeof(scncon));
      /* get the default screen */
  curscn = 1;   /* set current screen */
  trm_wrapoff();   /* wrap is allways off */
  /* initalize screen */

  iniscn();
  exit(EXIT_SUCCESS);
}
/* p2c: ansilib.pas, line 2556: 
 * Warning: Junk at end of input file ignored [277] */




/* End. */
