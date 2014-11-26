/** ***************************************************************************
*                                                                             *
*                      TRANSPARENT SCREEN CONTROL MODULE                      *
*                         FOR WINDOWS CONSOLE MODE                            *
*                                                                             *
*                              10/02 S. A. Moore                              *
*                                                                             *
* This module implementes the standard terminal calls for windows console     *
* mode.                                                                       *
*                                                                             *
* Windows console mode is fully buffered, with multiple buffering and buffer  *
* to display switching, with buffer parameters stored in each buffer. Because *
* of this, we let Windows manage the buffer operations, and mostly just       *
* reformat our calls to console mode.                                         *
*                                                                             *
* 2005/04/04 When running other tasks in the same console session from an     *
* exec, the other program moves the console position, but we don't see that,  *
* because we keep our own position. This is a side consequence of using the   *
* Windows direct console calls, which all require the write position to be    *
* specified. One way might be to return to using file I/O calls. However, we  *
* use the call "getpos" to reload Windows idea of the console cursor location *
* any time certain events occur. These events are:                            *
*                                                                             *
* 1. Writing of characters to the console buffer.                             *
*                                                                             *
* 2. Relative positioning (up/down/left/right).                               *
*                                                                             *
* 3. Reading the position.                                                    *
*                                                                             *
* This should keep conlib in sync with any changes in the Windows console,    *
* with the price that it slows down the execution. However, in character mode *
* console, this is deemed acceptable. Note that we ignore any other tasks     *
* changes of attributes and colors.                                           *
*                                                                             *
* Its certainly possible that our output, mixed with others, can be chaotic,  *
* but this happens in serial implementations as well. Most dangerous is our   *
* separation of character placement and move right after it. A character      *
* could come in the middle of that sequence.                                  *
*                                                                             *
* Remaining to be done:                                                       *
*                                                                             *
* 1. Make sure new buffers get proper coloring.                               *
*                                                                             *
* 2. We are getting "button 0" messages from the joystick on deassert during  *
*    fast repeated pushes.                                                    *
*                                                                             *
******************************************************************************/

module conlib;

#include <windows.h>
#include "pa_terminal.h"

uses windows, { uses windows 95 system call wrapper }
     sysovr;  { system override library }

const maxtim = 10; { maximum number of timers available }

type { colors displayable in text mode }
     color = (black, white, red, green, blue, cyan, yellow, magenta);
     joyhan = 1..4; { joystick handles }
     joynum = 0..4; { number of joysticks }
     joybut = 1..4; { joystick buttons }
     joybtn = 0..4; { joystick number of buttons }
     joyaxn = 0..3; { joystick axies }
     mounum = 0..4; { number of mice }
     mouhan = 1..4; { mouse handles }
     moubut = 1..4;  { mouse buttons }
     timhan = 1..maxtim; { timer handle }
     funky  = 1..100; { function keys }
     { events }
     evtcod = (etchar,    { ANSI character returned }
               etup,      { cursor up one line }
               etdown,    { down one line }
               etleft,    { left one character }
               etright,   { right one character }
               etleftw,   { left one word }
               etrightw,  { right one word }
               ethome,    { home of document }
               ethomes,   { home of screen }
               ethomel,   { home of line }
               etend,     { end of document }
               etends,    { end of screen }
               etendl,    { end of line }
               etscrl,    { scroll left one character }
               etscrr,    { scroll right one character }
               etscru,    { scroll up one line }
               etscrd,    { scroll down one line }
               etpagd,    { page down }
               etpagu,    { page up }
               ettab,     { tab }
               etenter,   { enter line }
               etinsert,  { insert block }
               etinsertl, { insert line }
               etinsertt, { insert toggle }
               etdel,     { delete block }
               etdell,    { delete line }
               etdelcf,   { delete character forward }
               etdelcb,   { delete character backward }
               etcopy,    { copy block }
               etcopyl,   { copy line }
               etcan,     { cancel current operation }
               etstop,    { stop current operation }
               etcont,    { continue current operation }
               etprint,   { print document }
               etprintb,  { print block }
               etprints,  { print screen }
               etfun,     { function key }
               etmenu,    { display menu }
               etmouba,   { mouse button assertion }
               etmoubd,   { mouse button deassertion }
               etmoumov,  { mouse move }
               ettim,     { timer matures }
               etjoyba,   { joystick button assertion }
               etjoybd,   { joystick button deassertion }
               etjoymov,  { joystick move }
               etterm);   { terminate program }
     { event record }
     evtrec = record

        { identifier of window for event, unused in terminal level }
        winid: ss_filhdl; { identifier of window for event }
        case etype: evtcod of { event type }

           { ANSI character returned }
           etchar:   (char:                char);
           { timer handle that matured }
           ettim:    (timnum:              timhan);
           etmoumov: (mmoun:               mouhan;   { mouse number }
                      moupx, moupy:        integer); { mouse movement }
           etmouba:  (amoun:               mouhan;   { mouse handle }
                      amoubn:              moubut);  { button number }
           etmoubd:  (dmoun:               mouhan;   { mouse handle }
                      dmoubn:              moubut);  { button number }
           etjoyba:  (ajoyn:               joyhan;   { joystick number }
                      ajoybn:              joybut);  { button number }
           etjoybd:  (djoyn:               joyhan;   { joystick number }
                      djoybn:              joybut);  { button number }
           etjoymov: (mjoyn:               joyhan;   { joystick number }
                      joypx, joypy, joypz: integer); { joystick coordinates }
           etfun:    (fkey:                funky);   { function key }
           etup, etdown, etleft, etright, etleftw, etrightw, ethome, ethomes,
           ethomel, etend, etends, etendl, etscrl, etscrr, etscru, etscrd,
           etpagd, etpagu, ettab, etenter, etinsert, etinsertl, etinsertt,
           etdel, etdell, etdelcf, etdelcb, etcopy, etcopyl, etcan, etstop,
           etcont, etprint, etprintb, etprints, etmenu,
           etterm: (); { normal events }

        { end }

     end;

{ functions at this level }

procedure cursor(var f: text; x, y: integer); forward;
overload procedure cursor(x, y: integer); forward;
function maxx(var f: text): integer; forward;
overload function maxx: integer; forward;
function maxy(var f: text): integer; forward;
overload function maxy: integer; forward;
procedure home(var f: text); forward;
overload procedure home; forward;
procedure del(var f: text); forward;
overload procedure del; forward;
procedure up(var f: text); forward;
overload procedure up; forward;
procedure down(var f: text); forward;
overload procedure down; forward;
procedure left(var f: text); forward;
overload procedure left; forward;
procedure right(var f: text); forward;
overload procedure right; forward;
procedure blink(var f: text; e: boolean); forward;
overload procedure blink(e: boolean); forward;
procedure reverse(var f: text; e: boolean); forward;
overload procedure reverse(e: boolean); forward;
procedure underline(var f: text; e: boolean); forward;
overload procedure underline(e: boolean); forward;
procedure superscript(var f: text; e: boolean); forward;
overload procedure superscript(e: boolean); forward;
procedure subscript(var f: text; e: boolean); forward;
overload procedure subscript(e: boolean); forward;
procedure italic(var f: text; e: boolean); forward;
overload procedure italic(e: boolean); forward;
procedure bold(var f: text; e: boolean); forward;
overload procedure bold(e: boolean); forward;
procedure strikeout(var f: text; e: boolean); forward;
overload procedure strikeout(e: boolean); forward;
procedure standout(var f: text; e: boolean); forward;
overload procedure standout(e: boolean); forward;
procedure fcolor(var f: text; c: color); forward;
overload procedure fcolor(c: color); forward;
procedure bcolor(var f: text; c: color); forward;
overload procedure bcolor(c: color); forward;
procedure auto(var f: text; e: boolean); forward;
overload procedure auto(e: boolean); forward;
procedure curvis(var f: text; e: boolean); forward;
overload procedure curvis(e: boolean); forward;
procedure scroll(var f: text; x, y: integer); forward;
overload procedure scroll(x, y: integer); forward;
function curx(var f: text): integer; forward;
overload function curx: integer; forward;
function cury(var f: text): integer; forward;
overload function cury: integer; forward;
function curbnd(var f: text): boolean; forward;
overload function curbnd: boolean; forward;
procedure select(var f: text; u, d: integer); forward;
overload procedure select(u, d: integer); forward;
procedure event(var f: text; var er: evtrec); forward;
overload procedure event(var er: evtrec); forward;
procedure timer(var f: text; i: timhan; t: integer; r: boolean); forward;
overload procedure timer(i: timhan; t: integer; r: boolean); forward;
procedure killtimer(var f: text; i: timhan); forward;
overload procedure killtimer(i: timhan); forward;
function mouse(var f: text): mounum; forward;
overload function mouse: mounum; forward;
function mousebutton(var f: text; m: mouhan): moubut; forward;
overload function mousebutton(m: mouhan): moubut; forward;
function joystick(var f: text): joynum; forward;
overload function joystick: joynum; forward;
function joybutton(var f: text; j: joyhan): joybtn; forward;
overload function joybutton(j: joyhan): joybtn; forward;
function joyaxis(var f: text; j: joyhan): joyaxn; forward;
overload function joyaxis(j: joyhan): joyaxn; forward;
procedure settab(var f: text; t: integer); forward;
overload procedure settab(t: integer); forward;
procedure restab(var f: text; t: integer); forward;
overload procedure restab(t: integer); forward;
procedure clrtab(var f: text); forward;
overload procedure clrtab; forward;
function funkey(var f: text): funky; forward;
overload function funkey: funky; forward;
procedure frametimer(var f: text; e: boolean); forward;
overload procedure frametimer(e: boolean); forward;

private

label 88; { abort label }

const

   { standard file handles }
   inpfil = 1;   { _input }
   outfil = 2;   { _output }
   maxlin = 250; { maximum length of input bufferred line }
   maxcon = 10;  { number of screen contexts }
   maxtab = 250; { maximum number of tabs (length of buffer in x) }
   frmtim = 11;  { handle number of framing timer }
   { special user events }
   uiv_tim = $8000; { timer matures }
   uiv_joy1move = $4001; { joystick 1 move }
   uiv_joy1zmove = $4002; { joystick 1 z move }
   uiv_joy2move = $2002; { joystick 2 move }
   uiv_joy2zmove = $2004; { joystick 2 z move }
   uiv_joy1buttondown = $1000; { joystick 1 button down }
   uiv_joy2buttondown = $0800; { joystick 2 button down }
   uiv_joy1buttonup = $0400; { joystick 1 button up }
   uiv_joy2buttonup = $0200; { joystick 2 button up }
   uiv_term = $0100; { terminate program }

type

   { screen attribute }
   scnatt = (sanone,    { no attribute }
             sablink,   { blinking text (foreground) }
             sarev,     { reverse video }
             saundl,    { underline }
             sasuper,   { superscript }
             sasubs,    { subscripting }
             saital,    { italic text }
             sabold,    { bold text }
             sastkout); { strikeout text }
   scncon = record { screen context }

      han:    integer; { screen buffer handle }
      maxx:   integer; { maximum x }
      maxy:   integer; { maximum y }
      curx:   integer; { current cursor location x }
      cury:   integer; { current cursor location y }
      conx:   integer; { windows console version of x }
      cony:   integer; { windows console version of y }
      curv:   boolean; { cursor visible }
      forec:  color;   { current writing foreground color }
      backc:  color;   { current writing background color }
      attr:   scnatt;  { current writing attribute }
      auto:   boolean; { current status of scroll and wrap }
      tab:    array [1..maxtab] of boolean; { tabbing array }
      sattr:  integer  { current character attributes }

   end;
   scnptr = ^scncon; { pointer to screen context block }
   errcod = (eftbful,  { file table full }
             ejoyacc,  { joystick access }
             etimacc,  { timer access }
             efilopr,  { cannot perform operation on special file }
             efilzer,  { filename is empty }
             einvscn,  { invalid screen number }
             einvhan,  { invalid handle }
             einvtab,  { invalid tab position }
             esbfcrt,  { cannot create screen buffer }
             ejoyqry,  { Could not get information on joystick }
             einvjoy,  { Invalid joystick ID }
             esystem); { System consistency check }

var

    { saves for hooked routines }
    sav_openread:  ss_pp;
    sav_openwrite: ss_pp;
    sav_close:     ss_pp;
    sav_read:      ss_pp;
    sav_write:     ss_pp;
    sav_position:  ss_pp;
    sav_location:  ss_pp;
    sav_length:    ss_pp;
    sav_eof:       ss_pp;

    inphdl:     integer; { "input" file handle }
    mb1:        boolean; { mouse assert status button 1 }
    mb2:        boolean; { mouse assert status button 2 }
    mb3:        boolean; { mouse assert status button 3 }
    mb4:        boolean; { mouse assert status button 4 }
    mpx, mpy:   integer; { mouse current position }
    nmb1:       boolean; { new mouse assert status button 1 }
    nmb2:       boolean; { new mouse assert status button 2 }
    nmb3:       boolean; { new mouse assert status button 3 }
    nmb4:       boolean; { new mouse assert status button 4 }
    nmpx, nmpy: integer; { new mouse current position }
    opnfil:     array [1..ss_maxhdl] of ss_filhdl; { open files table }
    fi:         1..ss_maxhdl; { index for files table }
    { we must open and process the _output file on our own, else we would
      recurse }
    inpbuf:     packed array [1..maxlin] of char; { input line buffer }
    inpptr:     0..maxlin; { input line index }
    screens:    array [1..maxcon] of scnptr; { screen contexts array }
    curdsp:     1..maxcon; { index for current display screen }
    curupd:     1..maxcon; { index for current update screen }
    timers:     array [1..10] of record

       han: sc_mmresult; { handle for timer }
       rep: boolean      { timer repeat flag }

    end;

    ti:         1..10; { index for repeat array }
    bi:         sc_console_screen_buffer_info; { screen buffer info structure }
    ci:         sc_console_cursor_info; { console cursor info structure }
    mode:       integer; { console mode }
    b:          boolean; { function return }
    i:          integer; { tab index }
    winhan:     integer; { main window id }
    r:          integer; { result holder }
    threadid:   integer; { dummy thread id (unused) }
    threadstart: boolean; { thread starts }
    joy1xs:     integer; { last joystick position 1x }
    joy1ys:     integer; { last joystick position 1y }
    joy1zs:     integer; { last joystick position 1z }
    joy2xs:     integer; { last joystick position 2x }
    joy2ys:     integer; { last joystick position 2y }
    joy2zs:     integer; { last joystick position 2z }
    numjoy:     integer; { number of joysticks found }
    { global sets. these are the global set parameters that apply to any new
      created screen buffer }
    gmaxx:      integer; { maximum x size }
    gmaxy:      integer; { maximum y size }
    gattr:      scnatt;  { current attribute }
    gauto:      boolean; { state of auto }
    gforec:     color;   { forground color }
    gbackc:     color;   { background color }
    gcurv:      boolean; { state of cursor visible }
    cix:        1..maxcon; { index for display screens }
    frmrun:     boolean; { framing timer is running }
    frmhan:     integer; { framing timer handle }

{******************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.
This needs to go to a dialog instead of the system error trap.

******************************************************************************}

procedure error(e: errcod);

procedure putstr(view s: string);

var i:     integer; { index for string }
    pream: packed array [1..9] of char; { preamble string }
    p:     pstring; { pointer to string }

begin

   pream := 'Console: '; { set preamble }
   new(p, max(s)+9); { get string to hold }
   for i := 1 to 9 do p^[i] := pream[i]; { copy preamble }
   for i := 1 to max(s) do p^[i+9] := s[i]; { copy string }
   ss_wrterr(p^); { output string }
   dispose(p) { release storage }

end;

begin

   case e of { error }

      eftbful: putstr('Too many files');
      ejoyacc: putstr('No joystick access available');
      etimacc: putstr('No timer access available');
      einvhan: putstr('Invalid handle');
      efilopr: putstr('Cannot perform operation on special file');
      efilzer: putstr('Filename is empty');
      einvscn: putstr('Invalid screen number');
      einvtab: putstr('Tab position specified off screen');
      esbfcrt: putstr('Cannot create screen buffer');
      einvjoy: putstr('Invalid joystick ID');
      ejoyqry: putstr('Could not get information on joystick');
      esystem: putstr('System consistency check, please contact vendor');

   end;
   goto 88 { abort module }

end;

{******************************************************************************

Make file entry

Indexes a present file entry or creates a new one. Looks for a free entry
in the files table, indicated by 0. If found, that is returned, otherwise
the file table is full.
Note that the "predefined" file slots are never allocated.

******************************************************************************}

procedure makfil(var fn: ss_filhdl); { file handle }

var fi: 1..ss_maxhdl; { index for files table }
    ff: 0..ss_maxhdl; { found file entry }

begin

   { find idle file slot (file with closed file entry) }
   ff := 0; { clear found file }
   for fi := outfil+1 to ss_maxhdl do { search all file entries }
      if opnfil[fi] = 0 then ff := fi; { found a slot }
   if ff = 0 then error(einvhan); { no empty file positions found }
   fn := ff { set file id number }

end;

{******************************************************************************

Remove leading and trailing spaces

Given a string, removes any leading and trailing spaces in the string. The
result is allocated and returned as an indexed buffer.

******************************************************************************}

procedure remspc(view nm: string;   { string }
                 var  rs: pstring); { result string }

var i1, i2: integer; { string indexes }
    n:      boolean; { string is null }
    s, e:   integer; { string start and end }

begin

   { first check if the string is empty or null }
   n := true; { set empty }
   for i1 := 1 to max(nm) do if nm[i1] <> ' ' then
      n := false; { set not empty }
   if n then new(rs, 0) { filename is empty, allocate null string strength }
   else begin

      s := 1; { set start of string }
      while (s < max(nm)) and (nm[s] = ' ') do s := s+1;
      e := max(nm); { set end of string }
      while (e > 1) and (nm[e] = ' ') do e := e-1;
      new(rs, e-s+1); { allocate result string }
      i2 := 1; { set 1st character of destination }
      for i1 := s to e do begin { copy to result }

         rs^[i2] := nm[i1]; { copy to result }
         i2 := i2+1 { next character }

      end

   end

end;

{******************************************************************************

Check system special file

Checks for one of the special files, and returns the handle of the special
file if found. Accepts a general string.

******************************************************************************}

function chksys(var fn: string) { file to check }
                : ss_filhdl;    { special file handle }

var hdl: ss_filhdl; { handle holder }

{ match strings }

function chkstr(view s: string): boolean;

var m: boolean; { match status }
    i: integer; { index for string }

{ find lower case }

function lcase(c: char): char;

begin

   { find lower case equivalent }
   if c in ['A'..'Z'] then c := chr(ord(c) - ord('A') + ord('a'));
   lcase := c { return as result }

end;

begin

   m := false; { set no match }
   if max(s) = max(fn) then begin { lengths match }

      m := true; { set strings match }
      for i := 1 to max(s) do if lcase(fn[i]) <> lcase(s[i]) then m := false

   end;
   chkstr := m { return match status }

end;

begin

   hdl := 0; { set not a special file }
   if chkstr('_input') then hdl := inpfil { check standard input }
   else if chkstr('_output') then hdl := outfil; { check standard output }
   chksys := hdl { return handle }

end;

{******************************************************************************

Load console status

Loads the current console status. Updates the size, cursor location, and
current attributes.

******************************************************************************}

procedure getpos;

var b:  boolean; { function return }
    bi: sc_console_screen_buffer_info; { screen buffer info structure }

begin

   if curdsp = curupd then { buffer is in display }
      with screens[curupd]^ do begin { with current update screen context }

      b := sc_getconsolescreenbufferinfo(han, bi);
      if (conx <> bi.dwcursorposition.x) or
         (cony <> bi.dwcursorposition.y) then begin

         conx := bi.dwcursorposition.x; { get new cursor position }
         cony := bi.dwcursorposition.y;
         screens[curupd]^.curx := conx+1; { place cursor position }
         screens[curupd]^.cury := cony+1

      end

   end

end;

{******************************************************************************

Construct packed coordinate word

Creates a Windows console coordinate word based on an X and Y value.

******************************************************************************}

function pcoord(x, y: integer): integer;

begin

   pcoord := y*65536+x

end;

{******************************************************************************

Set colors

Sets the current background and foreground colors in windows attribute format
from the coded colors and the "reverse" attribute.
Despite the name, also sets the attributes. We obey reverse coloring, and
set the following "substitute" attributes:

italics     Foreground half intensity.

underline   Background half intensity.

******************************************************************************}

procedure setcolor(sc: scnptr);

function colnum(c: color; i: boolean): integer;

var r: integer;

begin

   case c of { color }

      black:   r := $0000 or ord(i)*sc_foreground_intensity;
      white:   r := sc_foreground_blue or sc_foreground_green or
                    sc_foreground_red or ord(not i)*sc_foreground_intensity;
      red:     r := sc_foreground_red or ord(not i)*sc_foreground_intensity;
      green:   r := sc_foreground_green or ord(not i)*sc_foreground_intensity;
      blue:    r := sc_foreground_blue or ord(not i)*sc_foreground_intensity;
      cyan:    r := sc_foreground_blue or sc_foreground_green or
                    ord(not i)*sc_foreground_intensity;
      yellow:  r := sc_foreground_red or sc_foreground_green or
                    ord(not i)*sc_foreground_intensity;
      magenta: r := sc_foreground_red or sc_foreground_blue or
                    ord(not i)*sc_foreground_intensity

   end;
   colnum := r { return result }

end;

begin

   with sc^ do begin

      if attr = sarev then { set reverse colors }
         sattr := colnum(forec, (attr = saital) or (attr = sabold))*16+
                  colnum(backc, (attr = saundl) or (attr = sabold))
      else { set normal colors }
         sattr := colnum(backc, (attr = saundl) or (attr = sabold))*16+
                  colnum(forec, (attr = saital) or (attr = sabold))

   end

end;

{******************************************************************************

Find colors

Find colors from attribute word.

******************************************************************************}

procedure fndcolor(a: integer);

function numcol(a: integer): color;

var c: color;

begin

   case a mod 8 of { color }

       0: c := black;
       1: c := blue;
       2: c := green;
       3: c := cyan;
       4: c := red;
       5: c := magenta;
       6: c := yellow;
       7: c := white

   end;
   numcol := c

end;

begin

   screens[curupd]^.forec := numcol(a); { set foreground color }
   screens[curupd]^.backc := numcol(a div 16) { set background color }

end;

{******************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns true if so.

******************************************************************************}

function icurbnd(sc: scnptr): boolean;

begin

   icurbnd := (sc^.curx >= 1) and (sc^.curx <= sc^.maxx) and
              (sc^.cury >= 1) and (sc^.cury <= sc^.maxy)

end;

{******************************************************************************

Set cursor status

Sets the cursor visible or invisible. If the cursor is out of bounds, it is
invisible regardless. Otherwise, it is visible according to the state of the
current buffer's visible status.

******************************************************************************}

procedure cursts(sc: scnptr);

var b: boolean;
    ci: sc_console_cursor_info;
    cv: integer;

begin

   cv := ord(sc^.curv); { set current buffer status }
   if not icurbnd(sc) then cv := 0; { not in bounds, force off }
   { get current console information }
   b := sc_getconsolecursorinfo(sc^.han, ci);
   ci.bvisible := cv; { set cursor status }
   b := sc_setconsolecursorinfo(sc^.han, ci)

end;


{******************************************************************************

Position cursor

Positions the cursor (caret) image to the right location on screen, and handles
the visible or invisible status of that.

Windows has a nasty bug that setting the cursor position of a buffer that
isn't in display causes a cursor mark to be made at that position on the
active display. So we don't position if not in display.

******************************************************************************}

procedure setcur(sc: scnptr);

var b: boolean;

begin

   with sc^ do begin

      { check cursor in bounds, and buffer in display }
      if icurbnd(sc) and (sc = screens[curdsp]) then begin

         { set cursor position }
         b := sc_setconsolecursorposition(han, pcoord(curx-1, cury-1));
         conx := curx-1; { set our copy of that }
         cony := cury-1

      end;
      cursts(sc) { set new cursor status }

   end

end;

{******************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

******************************************************************************}

procedure iclear(sc: scnptr);

var x, y: integer;
    len:  integer;
    cb:   packed array [1..1] of char; { character output buffer }
    ab:   packed array [1..1] of integer; { attribute output buffer }
    b:    boolean;

begin

   with sc^ do begin

      cb[1] := ' '; { set space }
      ab[1] := sattr; { set attributes }
      for y := 0 to maxy-1 do begin

         for x := 0 to maxx-1 do begin

            b := sc_writeconsoleoutputcharacter(han, cb, pcoord(x, y), len);
            b := sc_writeconsoleoutputattribute(han, ab, pcoord(x, y), len)

         end

      end;
      cury := 1; { set cursor at home }
      curx := 1;
      setcur(sc)

   end;

end;

{******************************************************************************

Initalize screen

Clears all the parameters in the present screen context.

******************************************************************************}

procedure iniscn(sc: scnptr);

var i: integer;

begin

   sc^.maxx := gmaxx; { set size }
   sc^.maxy := gmaxy;
   b := sc_setconsolescreenbuffersize(sc^.han, pcoord(sc^.maxx, sc^.maxy));
   sc^.forec := gforec; { set colors and attributes }
   sc^.backc := gbackc;
   sc^.attr := gattr;
   sc^.auto := gauto; { set auto scroll and wrap }
   sc^.curv := gcurv; { set cursor visibility }
   setcolor(sc); { set current color }
   iclear(sc); { clear screen buffer with that }
   { set up tabbing to be on each 8th position }
   for i := 1 to sc^.maxx do sc^.tab[i] := (i-1) mod 8 = 0

end;

{******************************************************************************

Scroll screen

Scrolls the terminal screen by deltas in any given direction. Windows performs
scrolls as source->destination rectangle moves. This can be easily converted
from our differentials, but there is additional complexity in that both the
source and destination must be on screen, and not in negative space. So we
take each case of up, down, left right and break it out into moves that will
remain on the screen.
Because Windows (oddly) fills based on the source rectangle, we perform each
scroll as a separate sequence in each x and y direction. This makes the code
simpler, and lets Windows perform the fills for us.

******************************************************************************}

procedure iscroll(x, y: integer);

var sr: sc_small_rect; { scroll rectangle }
    f: sc_char_info;   { fill character info }
    b: boolean;        { return value }

begin

   with screens[curupd]^ do begin

      f.asciichar := ' '; { set fill values }
      f.attributes := screens[curupd]^.sattr;
      if (x <= -maxx) or (x >= maxx) or
         (y <= -maxy) or (y >= maxy) then
         { scroll would result in complete clear, do it }
         iclear(screens[curupd]) { clear the screen buffer }
      else begin { scroll }

         { perform y moves }
         if y >= 0 then begin { move text up }

            sr.left := 0;
            sr.right := screens[curupd]^.maxx-1;
            sr.top := y;
            sr.bottom := screens[curupd]^.maxy-1;
            b := sc_scrollconsolescreenbuffer_n(screens[curupd]^.han, sr,
                                                pcoord(0, 0), f)

         end else begin { move text down }

            sr.left := 0;
            sr.right := screens[curupd]^.maxx-1;
            sr.top := 0;
            sr.bottom := screens[curupd]^.maxy-1;
            b := sc_scrollconsolescreenbuffer_n(screens[curupd]^.han, sr,
                                                pcoord(0, abs(y)), f)

         end;
         { perform x moves }
         if x >= 0 then begin { move text left }

            sr.left := x;
            sr.right := screens[curupd]^.maxx-1;
            sr.top := 0;
            sr.bottom := screens[curupd]^.maxy-1;
            b := sc_scrollconsolescreenbuffer_n(screens[curupd]^.han, sr,
                                                pcoord(0, 0), f)

         end else begin { move text right }

            sr.left := 0;
            sr.right := screens[curupd]^.maxx-1;
            sr.top := 0;
            sr.bottom := screens[curupd]^.maxy-1;
            b := sc_scrollconsolescreenbuffer_n(screens[curupd]^.han, sr,
                                                pcoord(abs(x), 0), f)

         end

      end

   end

end;

procedure scroll(var f: text; x, y: integer);

begin

   refer(f); { file not used }
   iscroll(x, y) { process scroll }

end;

overload procedure scroll(x, y: integer);

begin

   iscroll(x, y) { process scroll }

end;

{******************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

******************************************************************************}

procedure icursor(x, y: integer);

begin

   screens[curupd]^.cury := y; { set new position }
   screens[curupd]^.curx := x;
   setcur(screens[curupd]) { set cursor on screen }

end;

procedure cursor(var f: text; x, y: integer);

begin

   refer(f); { file not used }
   icursor(x, y) { position cursor }

end;

overload procedure cursor(x, y: integer);

begin

   icursor(x, y) { position cursor }

end;

{******************************************************************************

Find if cursor is in screen bounds

This is the external interface to curbnd.

******************************************************************************}

function curbnd(var f: text): boolean;

begin

   refer(f); { file not used }
   curbnd := icurbnd(screens[curupd])

end;

overload function curbnd: boolean;

begin

   curbnd := icurbnd(screens[curupd])

end;

{******************************************************************************

Return maximum x demension

Returns the maximum x demension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

******************************************************************************}

function maxx(var f: text): integer;

begin

   refer(f); { file not used }
   maxx := screens[curupd]^.maxx { set maximum x }

end;

overload function maxx: integer;

begin

   maxx := screens[curupd]^.maxx { set maximum x }

end;

{******************************************************************************

Return maximum y demension

Returns the maximum y demension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

******************************************************************************}

function maxy(var f: text): integer;

begin

   refer(f); { file not used }
   maxy := screens[curupd]^.maxy { set maximum y }

end;

overload function maxy: integer;

begin

   maxy := screens[curupd]^.maxy { set maximum y }

end;

{******************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

******************************************************************************}

procedure home(var f: text);

begin

   refer(f); { file not used }
   screens[curupd]^.cury := 1; { set cursor at home }
   screens[curupd]^.curx := 1;
   setcur(screens[curupd]) { set cursor on screen }

end;

overload procedure home;

begin

   screens[curupd]^.cury := 1; { set cursor at home }
   screens[curupd]^.curx := 1;
   setcur(screens[curupd]) { set cursor on screen }

end;

{******************************************************************************

Move cursor up internal

Moves the cursor position up one line.

******************************************************************************}

procedure iup;

begin

   getpos; { update status }
   with screens[curupd]^ do begin

      { check not top of screen }
      if cury > 1 then cury := cury-1 { update position }
      { check auto mode }
      else if auto then iscroll(0, -1) { scroll up }
      { check won't overflow }
      else if cury > -maxint then cury := cury-1; { set new position }
      setcur(screens[curupd]) { set cursor on screen }

   end

end;

procedure up(var f: text);

begin

   refer(f); { file not used }
   iup { move up }

end;

overload procedure up;

begin

   iup { move up }

end;

{******************************************************************************

Move cursor down internal

Moves the cursor position down one line.

******************************************************************************}

procedure idown;

begin

   getpos; { update status }
   with screens[curupd]^ do begin

      { check not bottom of screen }
      if cury < maxy then cury := cury+1 { update position }
      { check auto mode }
      else if auto then iscroll(0, +1) { scroll down }
      else if cury < maxint then cury := cury+1; { set new position }
      setcur(screens[curupd]) { set cursor on screen }

   end

end;

procedure down(var f: text);

begin

   refer(f); { file not used }
   idown { move cursor down }

end;

overload procedure down;

begin

   idown { move cursor down }

end;

{******************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

******************************************************************************}

procedure ileft;

begin

   getpos; { update status }
   with screens[curupd]^ do begin

      { check not extreme left }
      if curx > 1 then curx := curx-1 { update position }
      else begin { wrap cursor motion }

         if auto then begin { autowrap is on }

            iup; { move cursor up one line }
            curx := screens[curupd]^.maxx { set cursor to extreme right }

         end else
            { check won't overflow }
            if curx > -maxint then curx := curx-1 { update position }

      end;
      setcur(screens[curupd]) { set cursor on screen }

   end

end;

procedure left(var f: text);

begin

   refer(f); { file not used }
   ileft { move cursor left }

end;

overload procedure left;

begin

   ileft { move cursor left }

end;

{******************************************************************************

Move cursor right internal

Moves the cursor one character right.

******************************************************************************}

procedure iright;

begin

   getpos; { update status }
   with screens[curupd]^ do begin

      { check not at extreme right }
      if curx < screens[curupd]^.maxx then curx := curx+1 { update position }
      else begin { wrap cursor motion }

         if auto then begin { autowrap is on }

            idown; { move cursor up one line }
            curx := 1 { set cursor to extreme left }

         end else
            { check won't overflow }
            if curx < maxint then curx := curx+1 { update position }

      end;
      setcur(screens[curupd]) { set cursor on screen }

   end

end;

procedure right(var f: text);

begin

   refer(f); { file not used }
   iright { move cursor right }

end;

overload procedure right;

begin

   iright { move cursor right }

end;

{******************************************************************************

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

******************************************************************************}

procedure itab;

var i: 1..maxtab;

begin

   getpos; { update status }
   { first, find if next tab even exists }
   i := screens[curupd]^.curx+1; { get the current x position +1 }
   if i < 1 then i := 1; { don't bother to search to left of screen }
   { find tab or end of screen }
   while (i < screens[curupd]^.maxx) and not screens[curupd]^.tab[i] do i := i+1;
   if screens[curupd]^.tab[i] then { we found a tab }
      while screens[curupd]^.curx < i do iright { go to the tab }

end;

{******************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

******************************************************************************}

procedure blink(var f: text; e: boolean);

begin

   { no capability }
   refer(f); { file not used }
   refer(e); { enable not used }
   screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

overload procedure blink(e: boolean);

begin

   { no capability }
   refer(e); { enable not used }
   screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

{******************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.
Note that the attributes can only be set singly.

******************************************************************************}

procedure reverse(var f: text; e: boolean);

begin

   refer(f); { file not used }
   if e then screens[curupd]^.attr := sarev { set reverse status }
   else screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

overload procedure reverse(e: boolean);

begin

   if e then screens[curupd]^.attr := sarev { set reverse status }
   else screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

{******************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.

******************************************************************************}

procedure underline(var f: text; e: boolean);

begin

   { substituted by background half intensity }
   refer(f); { file not used }
   if e then screens[curupd]^.attr := saundl { set underline status }
   else screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

overload procedure underline(e: boolean);

begin

   { substituted by background half intensity }
   if e then screens[curupd]^.attr := saundl { set underline status }
   else screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

{******************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

******************************************************************************}

procedure superscript(var f: text; e: boolean);

begin

   { no capability }
   refer(f); { file not used }
   refer(e); { enable not used }
   screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

overload procedure superscript(e: boolean);

begin

   { no capability }
   refer(e); { enable not used }
   screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

{******************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

******************************************************************************}

procedure subscript(var f: text; e: boolean);

begin

   { no capability }
   refer(f); { file not used }
   refer(e); { enable not used }
   screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

overload procedure subscript(e: boolean);

begin

   { no capability }
   refer(e); { enable not used }
   screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

{******************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

******************************************************************************}

procedure italic(var f: text; e: boolean);

begin

   { substituted by foreground half intensity }
   refer(f); { file not used }
   if e then screens[curupd]^.attr := saital { set italic status }
   else screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

overload procedure italic(e: boolean);

begin

   { substituted by foreground half intensity }
   if e then screens[curupd]^.attr := saital { set italic status }
   else screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

{******************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

******************************************************************************}

procedure bold(var f: text; e: boolean);

begin

   { substituted by foreground and background half intensity }
   refer(f); { file not used }
   if e then screens[curupd]^.attr := sabold { set bold status }
   else screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

overload procedure bold(e: boolean);

begin

   { substituted by foreground and background half intensity }
   if e then screens[curupd]^.attr := sabold { set bold status }
   else screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

{******************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

******************************************************************************}

procedure strikeout(var f: text; e: boolean);

begin

   { no capability }
   refer(f); { file not used }
   refer(e); { enable not used }
   screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

overload procedure strikeout(e: boolean);

begin

   { no capability }
   refer(e); { enable not used }
   screens[curupd]^.attr := sanone; { set attribute inactive }
   setcolor(screens[curupd]) { set those colors active }

end;

{******************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

******************************************************************************}

procedure standout(var f: text; e: boolean);

begin

   reverse(f, e) { implement as reverse }

end;

overload procedure standout(e: boolean);

begin

   reverse(e) { implement as reverse }

end;

{******************************************************************************

Set foreground color

Sets the foreground (text) color from the universal primary code.

******************************************************************************}

procedure fcolor(var f: text; c: color);

begin

   refer(f); { file not used }
   screens[curupd]^.forec := c; { set color status }
   setcolor(screens[curupd]) { activate }

end;

overload procedure fcolor(c: color);

begin

   screens[curupd]^.forec := c; { set color status }
   setcolor(screens[curupd]) { activate }

end;

{******************************************************************************

Set background color

Sets the background color from the universal primary code.

******************************************************************************}

procedure bcolor(var f: text; c: color);

begin

   refer(f); { file not used }
   screens[curupd]^.backc := c; { set color status }
   setcolor(screens[curupd]) { activate }

end;

overload procedure bcolor(c: color);

begin

   screens[curupd]^.backc := c; { set color status }
   setcolor(screens[curupd]) { activate }

end;

{******************************************************************************

Enable/disable automatic scroll and wrap


Enables or disables automatic screen scroll and end of line wrapping. When the
cursor leaves the screen in automatic mode, the following occurs:

up       Scroll down
down     Scroll up
right    Line down, start at left
left     Line up, start at right

These movements can be combined. Leaving the screen right from the lower right
corner will both wrap and scroll up. Leaving the screen left from upper left
will wrap and scroll down.

With auto disabled, no automatic scrolling will occur, and any movement of the
cursor off screen will simply cause the cursor to be undefined. In this
package that means the cursor is off, and no characters are written. On a
real terminal, it simply means that the position is undefined, and could be
anywhere.

******************************************************************************}

procedure auto(var f: text; e: boolean);

begin

   refer(f); { file not used }
   screens[curupd]^.auto := e { set line wrap status }

end;

overload procedure auto(e: boolean);

begin

   screens[curupd]^.auto := e { set line wrap status }

end;

{******************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

******************************************************************************}

procedure curvis(var f: text; e: boolean);

begin

   refer(f); { file not used }
   screens[curupd]^.curv := e; { set cursor visible status }
   cursts(screens[curupd]) { update cursor status }

end;

overload procedure curvis(e: boolean);

begin

   screens[curupd]^.curv := e; { set cursor visible status }
   cursts(screens[curupd]) { update cursor status }

end;

{******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

******************************************************************************}

function curx(var f: text): integer;

begin

   refer(f); { file not used }
   getpos; { update status }
   curx := screens[curupd]^.curx { return current location x }

end;

overload function curx: integer;

begin

   getpos; { update status }
   curx := screens[curupd]^.curx { return current location x }

end;

{******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

******************************************************************************}

function cury(var f: text): integer;

begin

   refer(f); { file not used }
   getpos; { update status }
   cury := screens[curupd]^.cury { return current location y }

end;

overload function cury: integer;

begin

   getpos; { update status }
   cury := screens[curupd]^.cury { return current location y }

end;

{******************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
then a new screen is allocated and cleared.
The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

******************************************************************************}

procedure iselect(u, d: integer);

var gr: integer;

begin

   gr := $8000000;
   gr := gr*16;
   if (u < 1) or (u > maxcon) or (d < 1) or (d > maxcon) then
      error(einvscn); { invalid screen number }
   curupd := u; { set current update screen }
   if screens[curupd] = nil then begin { no screen, create one }

      new(screens[curupd]); { get a new screen context }
      { create the screen }
      screens[curupd]^.han :=
         sc_createconsolescreenbuffer_nn(gr {sc_generic_read} or
                                      sc_generic_write,
                                      sc_file_share_read or sc_file_share_write,
                                      sc_console_textmode_buffer);
      { check handle is valid }
      if screens[curupd]^.han = sc_invalid_handle_value then error(esbfcrt);
      iniscn(screens[curupd]) { initalize that }

   end;
   curdsp := d; { set the current display screen }
   if screens[curdsp] = nil then begin { no screen, create one }

      new(screens[curdsp]); { get a new screen context }
      { create the screen }
      screens[curdsp]^.han :=
         sc_createconsolescreenbuffer_nn(gr {sc_generic_read} or
                                      sc_generic_write,
                                      sc_file_share_read or sc_file_share_write,
                                      sc_console_textmode_buffer);
      { check handle is valid }
      if screens[curdsp]^.han = sc_invalid_handle_value then error(esbfcrt);
      iniscn(screens[curdsp]) { initalize that }

   end;
   { set display buffer as active display console }
   b := sc_setconsoleactivescreenbuffer(screens[curdsp]^.han);
   getpos; { make sure we are synced with Windows }
   setcur(screens[curdsp]) { make sure the cursor is at correct point }

end;

procedure select(var f: text; u, d: integer);

begin

   refer(f); { file not used }
   iselect(u, d) { perform select }

end;

overload procedure select(u, d: integer);

begin

   iselect(u, d) { perform select }

end;

{******************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attributes.

******************************************************************************}

procedure plcchr(c: char);

var b:   boolean; { function return }
    cb:  packed array [1..1] of char; { character output buffer }
    ab:  packed array [1..1] of integer; { attribute output buffer }
    len: integer; { length dummy }

begin

   getpos; { update status }
   with screens[curupd]^ do begin

      { handle special character cases first }
      if c = '\cr' then { carriage return, position to extreme left }
         icursor(1, cury)
      else if c = '\lf' then idown { line feed, move down }
      else if c = '\bs' then ileft { back space, move left }
      else if c = '\ff' then iclear(screens[curupd]) { clear screen }
      else if c = '\ht' then itab { process tab }
      else if (c >= ' ') and (c <> chr($7f)) then begin { character is visible }

         if icurbnd(screens[curupd]) then begin { cursor in bounds }

            cb[1] := c; { place character in buffer }
            ab[1] := sattr; { place attribute in buffer }
            { write character }
            b := sc_writeconsoleoutputcharacter(han, cb, pcoord(curx-1, cury-1),
                                                len);
            b := sc_writeconsoleoutputattribute(han, ab, pcoord(curx-1, cury-1),
                                                len)

         end;
         iright { move cursor right }

      end

   end

end;

{******************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

******************************************************************************}

procedure del(var f: text);

begin

   left(f); { back up cursor }
   plcchr(' '); { blank out }
   left(f) { back up again }

end;

overload procedure del;

begin

   left; { back up cursor }
   plcchr(' '); { blank out }
   left { back up again }

end;

{******************************************************************************

Aquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

******************************************************************************}

procedure ievent(var er: evtrec);

var keep:       boolean; { event keep flag }
    b:          boolean; { function return value }
    ne:         integer; { number of events }
    x, y, z:    integer; { joystick readback }
    dx, dy, dz: integer; { joystick readback differences }
    inpevt:     array [1..1] of sc_input_record; { event read buffer }

{msg:  sc_msg;}
{

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

}

procedure keyevent;

{ check control key pressed }

function cntrl: boolean;

begin

   cntrl := inpevt[1].keyevent.dwcontrolkeystate and
            (sc_right_ctrl_pressed or sc_left_ctrl_pressed) <> 0

end;

{ check shift key pressed }

function shift: boolean;

begin

   shift := inpevt[1].keyevent.dwcontrolkeystate and sc_shift_pressed <> 0

end;

begin

   { we only take key down (pressed) events }
   if inpevt[1].keyevent.bkeydown <> 0 then begin

      { Check character is non-zero. The zero character occurs
        whenever a control feature is hit. }
      if ord(inpevt[1].keyevent.uchar.asciichar) <> 0 then begin

         if inpevt[1].keyevent.uchar.asciichar = '\cr' then
            er.etype := etenter { set enter line }
         else if inpevt[1].keyevent.uchar.asciichar = '\bs' then
            er.etype := etdelcb { set delete character backwards }
         else if inpevt[1].keyevent.uchar.asciichar = '\ht' then
            er.etype := ettab { set tab }
         else if inpevt[1].keyevent.uchar.asciichar = chr(ord('C')-64) then
            er.etype := etterm { set end program }
         else if inpevt[1].keyevent.uchar.asciichar = chr(ord('S')-64) then
            er.etype := etstop { set stop program }
         else if inpevt[1].keyevent.uchar.asciichar = chr(ord('Q')-64) then
            er.etype := etcont { set continue program }
         else begin { normal character }

            er.etype := etchar; { set character event }
            er.char := inpevt[1].keyevent.uchar.asciichar

         end;
         keep := true { set keep event }

      end else if inpevt[1].keyevent.wvirtualkeycode <= $ff then
         { limited to codes we can put in a set }
         if inpevt[1].keyevent.wvirtualkeycode in
            [sc_vk_home, sc_vk_end, sc_vk_left, sc_vk_right,
             sc_vk_up, sc_vk_down, sc_vk_insert, sc_vk_delete,
             sc_vk_prior, sc_vk_next, sc_vk_f1..sc_vk_f10,
             sc_vk_menu, sc_vk_cancel] then begin

         { this key is one we process }
         case inpevt[1].keyevent.wvirtualkeycode of { key }

            sc_vk_home: begin { home }

               if cntrl then er.etype := ethome { home document }
               else if shift then er.etype := ethomes { home screen }
               else er.etype := ethomel { home line }

            end;
            sc_vk_end: begin { end }

               if cntrl then er.etype := etend { end document }
               else if shift then er.etype := etends { end screen }
               else er.etype := etendl { end line }

            end;
            sc_vk_up: begin { up }

               if cntrl then er.etype := etscru { scroll up }
               else er.etype := etup { up line }

            end;
            sc_vk_down: begin { down }

               if cntrl then er.etype := etscrd { scroll down }
               else er.etype := etdown { up line }

            end;
            sc_vk_left: begin { left }

               if cntrl then er.etype := etscrl { scroll left one character }
               else if shift then er.etype := etleftw { left one word }
               else er.etype := etleft { left one character }

            end;
            sc_vk_right: begin { right }

               if cntrl then er.etype := etscrr { scroll right one character }
               else if shift then er.etype := etrightw { right one word }
               else er.etype := etright { left one character }

            end;
            sc_vk_insert: begin { insert }

               if cntrl then er.etype := etinsert { insert block }
               else if shift then er.etype := etinsertl { insert line }
               else er.etype := etinsertt { insert toggle }

            end;
            sc_vk_delete: begin { delete }

               if cntrl then er.etype := etdel { delete block }
               else if shift then er.etype := etdell { delete line }
               else er.etype := etdelcf { insert toggle }

            end;
            sc_vk_prior: er.etype := etpagu; { page up }
            sc_vk_next: er.etype := etpagd; { page down }
            sc_vk_f1: begin { f1 }

               if cntrl then er.etype := etcopy { copy block }
               else if shift then er.etype := etcopyl { copy line }
               else begin { f1 }

                  er.etype := etfun;
                  er.fkey := 1

               end

            end;
            sc_vk_f2: begin { f2 }

               if cntrl then er.etype := etprintb { print block }
               else if shift then er.etype := etprint { print document }
               else begin { f2 }

                  er.etype := etfun;
                  er.fkey := 2

               end

            end;
            sc_vk_f3: begin { f3 }

               if cntrl then er.etype := etprints { print screen }
               else begin { f3 }

                  er.etype := etfun;
                  er.fkey := 3

               end

            end;
            sc_vk_f4: begin { f4 }

               er.etype := etfun;
               er.fkey := 4

            end;
            sc_vk_f5: begin { f5 }

               er.etype := etfun;
               er.fkey := 5

            end;
            sc_vk_f6: begin { f6 }

               er.etype := etfun;
               er.fkey := 6

            end;
            sc_vk_f7: begin { f7 }

               er.etype := etfun;
               er.fkey := 7

            end;
            sc_vk_f8: begin { f8 }

               er.etype := etfun;
               er.fkey := 8

            end;
            sc_vk_f9: begin { f9 }

               er.etype := etfun;
               er.fkey := 9

            end;
            sc_vk_f10: begin { f10 }

               er.etype := etfun;
               er.fkey := 10

            end;
            sc_vk_f11: begin { f11 }

               er.etype := etfun;
               er.fkey := 11

            end;
            sc_vk_f12: begin { f12 }

               er.etype := etfun;
               er.fkey := 12

            end;
            sc_vk_menu: er.etype := etmenu; { alt }
            sc_vk_cancel: er.etype := etterm; { ctl-brk }

         end;
         keep := true { set keep event }

      end

   end

end;

{

Process mouse event.
Buttons are assigned to Win 95 as follows:

button 1: Windows left button
button 2: Windows right button
button 3: Windows 2nd from left button
button 4: Windows 3rd from left button

The Windows 4th from left button is unused. The double click events will
end up, as, well, two clicks of a single button, displaying my utter
contempt for the whole double click concept.

}

{ update mouse parameters }

procedure mouseupdate;

begin

   { we prioritize events by: movements 1st, button clicks 2nd }
   if (nmpx <> mpx) or (nmpy <> mpy) then begin { create movement event }

      er.etype := etmoumov; { set movement event }
      er.mmoun := 1; { mouse 1 }
      er.moupx := nmpx; { set new mouse position }
      er.moupy := nmpy;
      mpx := nmpx; { save new position }
      mpy := nmpy;
      keep := true { set to keep }

   end else if nmb1 > mb1 then begin

      er.etype := etmouba; { button 1 assert }
      er.amoun := 1; { mouse 1 }
      er.amoubn := 1; { button 1 }
      mb1 := nmb1; { update status }
      keep := true { set to keep }

   end else if nmb2 > mb2 then begin

      er.etype := etmouba; { button 2 assert }
      er.amoun := 1; { mouse 1 }
      er.amoubn := 2; { button 2 }
      mb2 := nmb2; { update status }
      keep := true { set to keep }

   end else if nmb3 > mb3 then begin

      er.etype := etmouba; { button 3 assert }
      er.amoun := 1; { mouse 1 }
      er.amoubn := 3; { button 3 }
      mb3 := nmb3; { update status }
      keep := true { set to keep }

   end else if nmb4 > mb4 then begin

      er.etype := etmouba; { button 4 assert }
      er.amoun := 1; { mouse 1 }
      er.amoubn := 4; { button 4 }
      mb4 := nmb4; { update status }
      keep := true { set to keep }

   end else if nmb1 < mb1 then begin

      er.etype := etmoubd; { button 1 deassert }
      er.dmoun := 1; { mouse 1 }
      er.dmoubn := 1; { button 1 }
      mb1 := nmb1; { update status }
      keep := true { set to keep }

   end else if nmb2 < mb2 then begin

      er.etype := etmoubd; { button 2 deassert }
      er.dmoun := 1; { mouse 1 }
      er.dmoubn := 2; { button 2 }
      mb2 := nmb2; { update status }
      keep := true { set to keep }

   end else if nmb3 < mb3 then begin

      er.etype := etmoubd; { button 3 deassert }
      er.dmoun := 1; { mouse 1 }
      er.dmoubn := 3; { button 3 }
      mb3 := nmb3; { update status }
      keep := true { set to keep }

   end else if nmb4 < mb4 then begin

      er.etype := etmoubd; { button 4 deassert }
      er.dmoun := 1; { mouse 1 }
      er.dmoubn := 4; { button 4 }
      mb4 := nmb4; { update status }
      keep := true { set to keep }

   end

end;

{ register mouse status }

procedure mouseevent;

begin

   { gather a new mouse status }
   nmpx := inpevt[1].mouseevent.dwmouseposition.x+1; { get mouse position }
   nmpy := inpevt[1].mouseevent.dwmouseposition.y+1;
   nmb1 := inpevt[1].mouseevent.dwbuttonstate and
           sc_from_left_1st_button_pressed <> 0;
   nmb2 := inpevt[1].mouseevent.dwbuttonstate
           and sc_rightmost_button_pressed <> 0;
   nmb3 := inpevt[1].mouseevent.dwbuttonstate and
           sc_from_left_2nd_button_pressed <> 0;
   nmb4 := inpevt[1].mouseevent.dwbuttonstate and
           sc_from_left_3rd_button_pressed <> 0

end;

{ process joystick messages }

procedure joymes;

{ issue event for changed button }

procedure updn(bn, bm: integer);

begin

   { Note that if there are multiple up/down bits, we only register the last
     one. Windows is ambiguous as to if it will combine such events }
   if (inpevt[1].keyevent.wvirtualkeycode and bm) <> 0 then begin { assert }

      er.etype := etjoyba; { set assert }
      if (inpevt[1].eventtype = uiv_joy1buttondown) or
         (inpevt[1].eventtype = uiv_joy1buttonup) then er.ajoyn := 1
      else er.ajoyn := 2;
      er.ajoybn := bn { set number }

   end else begin { deassert }

      er.etype := etjoybd; { set deassert }
      if (inpevt[1].eventtype = uiv_joy1buttondown) or
         (inpevt[1].eventtype = uiv_joy1buttonup) then er.ajoyn := 1
      else er.ajoyn := 2;
      er.djoybn := bn { set number }

   end;
   keep := true { set keep event }

end;

begin

   { register changes on each button }
   if (inpevt[1].keyevent.wvirtualkeycode and sc_joy_button1chg) <> 0 then
      updn(1, sc_joy_button1);
   if (inpevt[1].keyevent.wvirtualkeycode and sc_joy_button2chg) <> 0 then
      updn(2, sc_joy_button2);
   if (inpevt[1].keyevent.wvirtualkeycode and sc_joy_button3chg) <> 0 then
      updn(3, sc_joy_button3);
   if (inpevt[1].keyevent.wvirtualkeycode and sc_joy_button4chg) <> 0 then
      updn(4, sc_joy_button4)

end;

begin { event }

   repeat

      keep := false; { set don't keep by default }
      mouseupdate; { check any mouse details need processing }
      if not keep then begin { no, go ahead with event read }

         b := sc_readconsoleinput(inphdl, inpevt, ne); { get the next event }
         if b then begin { process valid event }

            { decode by event }
            if inpevt[1].eventtype = sc_key_event then keyevent { key event }
            else if inpevt[1].eventtype = sc_mouse_event then
               mouseevent { mouse event }
            else if inpevt[1].eventtype = uiv_tim then begin { timer event }

               er.etype := ettim; { set timer event occurred }
               { set what timer }
               er.timnum := inpevt[1].keyevent.wvirtualkeycode;
               keep := true { set to keep }

            end else if (inpevt[1].eventtype = uiv_joy1move) or
                        (inpevt[1].eventtype = uiv_joy1zmove) or
                        (inpevt[1].eventtype = uiv_joy2move) or
                        (inpevt[1].eventtype = uiv_joy2zmove) then begin

               { joystick move }
               er.etype := etjoymov; { set joystick moved }
               { set what joystick }
               if (inpevt[1].eventtype = uiv_joy1move) or
                  (inpevt[1].eventtype = uiv_joy1zmove) then er.mjoyn := 1
               else er.mjoyn := 2;
               { Set all variables to default to same. This way, only the joystick
                 axes that are actually set by the message are registered. }
               if (inpevt[1].eventtype = uiv_joy1move) or
                  (inpevt[1].eventtype = uiv_joy1zmove) then begin

                  x := joy1xs;
                  y := joy1ys;
                  z := joy1zs

               end else begin

                  x := joy2xs;
                  y := joy2ys;
                  z := joy2zs

               end;
               { If it's an x/y move, split the x and y axies parts of the message
                 up. }
               if (inpevt[1].eventtype = uiv_joy1move) or
                  (inpevt[1].eventtype = uiv_joy2move) then begin

                  { get x and y positions }
                  x := inpevt[1].keyevent.wvirtualkeycode;
                  y := inpevt[1].keyevent.wvirtualscancode

               end else { get z position }
                  z := inpevt[1].keyevent.wvirtualkeycode;
               { We perform thresholding on the joystick right here, which is
                 limited to 255 steps (same as joystick hardware. find joystick
                 diffs and update }
               if (inpevt[1].eventtype = uiv_joy1move) or
                  (inpevt[1].eventtype = uiv_joy1zmove) then begin

                  dx := abs(joy1xs-x); { find differences }
                  dy := abs(joy1ys-y);
                  dz := abs(joy1zs-z);
                  joy1xs := x; { place old values }
                  joy1ys := y;
                  joy1zs := z

               end else begin

                  dx := abs(joy2xs-x); { find differences }
                  dy := abs(joy2ys-y);
                  dz := abs(joy2zs-z);
                  joy2xs := x; { place old values }
                  joy2ys := y;
                  joy2zs := z

               end;
               { now reject moves below the threshold }
               if (dx > 65535 div 255) or (dy > 65535 div 255) or
                  (dz > 65535 div 255) then begin

                  { scale axies between -maxint..maxint and place }
                  er.joypx := (x - 32767)*(maxint div 32768);
                  er.joypy := (y - 32767)*(maxint div 32768);
                  er.joypz := (z - 32767)*(maxint div 32768);
                  keep := true { set keep event }

               end

            end else if (inpevt[1].eventtype = uiv_joy1buttondown) or
                        (inpevt[1].eventtype = uiv_joy2buttondown) or
                        (inpevt[1].eventtype = uiv_joy1buttonup) or
                        (inpevt[1].eventtype = uiv_joy2buttonup) then joymes
            else if inpevt[1].eventtype = uiv_term then begin

               er.etype := etterm; { set end program }
               keep := true { set keep event }

            end

         end

      end

   until keep { until an event we want occurs }

end; { event }

procedure event(var f: text; var er: evtrec);

begin

   refer(f); { file not used }
   ievent(er) { process event }

end;

overload procedure event(var er: evtrec);

begin

   ievent(er) { process event }

end;

{******************************************************************************

Timer handler procedure

Called when the windows multimedia event timer expires. We prepare a message
to send up to the console even handler. Since the console event system does not
have user defined messages, this is done by using a key event with an invalid
event code.

******************************************************************************}

procedure timeout(id, msg, usr, dw1, dw2: integer);

var inpevt: array [1..1] of sc_input_record; { windows event record array }
    ne:     integer;  { number of events written }
    b:      boolean;

begin

   refer(id, msg, dw1, dw2); { not used }
   inpevt[1].eventtype := uiv_tim; { set key event type }
   inpevt[1].keyevent.wvirtualkeycode := usr; { set timer handle }
   b := sc_writeconsoleinput(inphdl, inpevt, ne); { send }

end;

{******************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.
Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it sends a timer mature event to
the associated input file.

******************************************************************************}

procedure itimer(i: timhan;   { timer handle }
                 t: integer;  { number of tenth-milliseconds to run }
                 r: boolean); { timer is to rerun after completion }

var tf: integer; { timer flags }
    mt: integer; { millisecond time }

begin

   mt := t div 10; { find millisecond time }
   if mt = 0 then mt := 1; { fell below minimum, must be >= 1 }
   { set flags for timer }
   tf := sc_time_callback_function or sc_time_kill_synchronous;
   { set repeat/one shot status }
   if r then tf := tf or sc_time_periodic
   else tf := tf or sc_time_oneshot;
   timers[i].han := sc_timesetevent(mt, 0, timeout, i, tf);
   timers[i].rep := r; { set timer repeat flag }
   { should check and return an error }

end;

procedure timer(var f: text; i: timhan; t: integer; r: boolean);

begin

   refer(f); { file not used }
   itimer(i, t, r) { set timer }

end;

overload procedure timer(i: timhan; t: integer; r: boolean);

begin

   itimer(i, t, r) { set timer }

end;

{******************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

******************************************************************************}

procedure killtimer(var f: text;    { file to kill timer on }
                        i: timhan); { handle of timer }

var r: sc_mmresult; { return value }

begin

   refer(f); { file not used }
   r := sc_timekillevent(timers[i].han); { kill timer }
   { should check for return error }

end;

overload procedure killtimer(i: timhan); { handle of timer }

var r: sc_mmresult; { return value }

begin

   r := sc_timekillevent(timers[i].han); { kill timer }
   { should check for return error }

end;

{******************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

******************************************************************************}

procedure iframetimer(e: boolean);

begin

   if e then begin { enable timer }

      if not frmrun then begin { it is not running }

         { set timer to run, 17ms }
         frmhan := sc_timesetevent(17, 0, timeout, frmtim,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_periodic);
         if frmhan = 0 then error(etimacc); { error }
         frmrun := true { set timer running }

      end

   end else begin { disable framing timer }

      if frmrun then begin { it is currently running }

         r := sc_timekillevent(frmhan); { kill timer }
         if r <> 0 then error(etimacc); { error }
         frmrun := false { set timer not running }

      end

   end

end;

procedure frametimer(var f: text; e: boolean);

begin

   refer(f); { file not used }
   iframetimer(e); { execute }

end;

overload procedure frametimer(e: boolean);

begin

   iframetimer(e); { execute }

end;

{******************************************************************************

Return number of mice

Returns the number of mice implemented.

******************************************************************************}

function mouse(var f: text): mounum;

begin

   refer(f); { file not used }
   mouse := 1 { set single mouse }

end;

overload function mouse: mounum;

begin

   mouse := 1 { set single mouse }

end;

{******************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

******************************************************************************}

function mousebutton(var f: text; m: mouhan): moubut;

begin

   refer(f); { file not used }
   if m <> 1 then error(einvhan); { bad mouse number }
   mousebutton := 3 { set 3 buttons }

end;

overload function mousebutton(m: mouhan): moubut;

begin

   if m <> 1 then error(einvhan); { bad mouse number }
   mousebutton := 3 { set 3 buttons }

end;

{******************************************************************************

Return number of joysticks

Return number of joysticks attached.

******************************************************************************}

function joystick(var f: text): joynum;

begin

   refer(f); { file not used }
   joystick := numjoy { two }

end;

overload function joystick: joynum;

begin

   joystick := numjoy { two }

end;

{******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

******************************************************************************}

function ijoybutton(j: joyhan): joybtn;

var jc: sc_joycaps; { joystick characteristics data }
    nb: integer;    { number of buttons }
    r:  integer;    { return value }

begin

   if (j < 1) or (j > numjoy) then error(einvjoy); { bad joystick id }
   r := sc_joygetdevcaps(j-1, jc, sc_joycaps_len);
   if r <> 0 then error(ejoyqry); { could not access joystick }
   nb := jc.wnumbuttons; { set number of buttons }
   { We don't support more than 4 buttons. }
   if nb > 4 then nb := 4;
   ijoybutton := nb { set number of buttons }

end;

function joybutton(var f: text; j: joyhan): joybtn;

begin

   refer(f); { file not used }
   joybutton := ijoybutton(j)

end;

overload function joybutton(j: joyhan): joybtn;

begin

   joybutton := ijoybutton(j)

end;

{******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodementional
joystick can be considered a slider without positional meaning.

******************************************************************************}

function joyaxis(var f: text; j: joyhan): joyaxn;

begin

   refer(f); { file not used }
   refer(j); { joystick handle not used }
   joyaxis := 2 { 2d }

end;

overload function joyaxis(j: joyhan): joyaxn;

begin

   refer(j); { joystick handle not used }
   joyaxis := 2 { 2d }

end;

{******************************************************************************

Set tab

Sets a tab at the indicated collumn number.

******************************************************************************}

procedure settab(var f: text; t: integer);

begin

   refer(f); { file not used }
   if (t < 1) or (t > screens[curupd]^.maxx) then
      error(einvtab); { bad tab position }
   screens[curupd]^.tab[t] := true; { place tab at that position }

end;

overload procedure settab(t: integer);

begin

   if (t < 1) or (t > screens[curupd]^.maxx) then
      error(einvtab); { bad tab position }
   screens[curupd]^.tab[t] := true; { place tab at that position }

end;

{******************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

******************************************************************************}

procedure restab(var f: text; t: integer);

begin

   refer(f); { file not used }
   if (t < 1) or (t > screens[curupd]^.maxx) then
      error(einvtab); { bad tab position }
   screens[curupd]^.tab[t] := false; { clear tab at that position }

end;

overload procedure restab(t: integer);

begin

   if (t < 1) or (t > screens[curupd]^.maxx) then
      error(einvtab); { bad tab position }
   screens[curupd]^.tab[t] := false; { clear tab at that position }

end;

{******************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

******************************************************************************}

procedure clrtab(var f: text);

var i: integer;

begin

   refer(f); { file not used }
   for i := 1 to screens[curupd]^.maxx do screens[curupd]^.tab[i] := false

end;

overload procedure clrtab;

var i: integer;

begin

   for i := 1 to screens[curupd]^.maxx do screens[curupd]^.tab[i] := false

end;

{******************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

******************************************************************************}

function funkey(var f: text): funky;

begin

   refer(f); { file not used }
   funkey := 12 { number of function keys }

end;

overload function funkey: funky;

begin

   funkey := 12 { number of function keys }

end;

{******************************************************************************

Process input line

Reads an input line with full echo and editting. The line is placed into the
input line buffer.
The upgrade for this is to implement a full set of editting features.

******************************************************************************}

procedure readline;

var er: evtrec; { event record }

begin

   inpptr := 1; { set 1st character position }
   repeat { get line characters }

      { get events until an "interesting" event occurs }
      repeat ievent(er) until er.etype in [etchar, etenter, etterm, etdelcb];
      { if the event is line enter, place carriage return code,
        otherwise place real character. note that we emulate a
        terminal and return cr only, which is handled as appropriate
        by a higher level. if the event is program terminate, then we
        execute an organized halt }
      case er.etype of { event }

         etterm:  goto 88; { halt program }
         etenter: begin { line terminate }

            inpbuf[inpptr] := '\cr'; { return cr }
            plcchr('\cr'); { output newline sequence }
            plcchr('\lf')

         end;
         etchar: begin { character }

            if inpptr < maxlin then begin

               inpbuf[inpptr] := er.char; { place real character }
               plcchr(er.char) { echo the character }

            end;
            if inpptr < maxlin then inpptr := inpptr+1 { next character }

         end;
         etdelcb: begin { delete character backwards }

            if inpptr > 1 then begin { not at extreme left }

               plcchr('\bs'); { backspace, spaceout then backspace again }
               plcchr(' ');
               plcchr('\bs');
               inpptr := inpptr-1 { back up pointer }

            end

         end

      end

   until er.etype = etenter; { until line terminate }
   inpptr := 1 { set 1st position on active line }

end;

{******************************************************************************

Open file for read

Opens the file by name, and returns the file handle. If the file is the
"_input" file, then we give it our special handle. Otherwise, the entire
processing of the file is passed onto the system level.
We handle the entire processing of the input file here, by processing the
event queue.

******************************************************************************}

procedure fileopenread(var fn: ss_filhdl; view nm: string);

var fs: pstring; { filename buffer pointer }

begin

   remspc(nm, fs); { remove leading and trailing spaces }
   fn := chksys(fs^); { check it's a system file }
   if fn <> inpfil then begin { not "_input", process pass-on }

      makfil(fn); { create new file slot }
      ss_old_openread(opnfil[fn], fs^, sav_openread) { pass to lower level }

   end;
   dispose(fs) { release temp string }

end;

{******************************************************************************

Open file for write

Opens the file by name, and returns the file handle. If the file is the
"_output" file, then we give it our special handle. Otherwise, the file entire
processing of the file is passed onto the system level.

******************************************************************************}

procedure fileopenwrite(var fn: ss_filhdl; view nm: string);

var fs: pstring; { filename buffer pointer }

begin

   remspc(nm, fs); { remove leading and trailing spaces }
   fn := chksys(fs^); { check it's a system file }
   if fn <> outfil then begin { not "_output", process pass-on }

      makfil(fn); { create new file slot }
      ss_old_openwrite(opnfil[fn], fs^, sav_openwrite) { open at lower level }

   end;
   dispose(fs) { release temp string }

end;

{******************************************************************************

Close file

Closes the file. The file is closed at the system level, then we remove the
table entry for the file.

******************************************************************************}

procedure fileclose(fn: ss_filhdl);

begin

   if fn > outfil then begin { transparent file }

      if opnfil[fn] = 0 then error(einvhan); { invalid handle }
      ss_old_close(opnfil[fn], sav_close); { close at lower level }
      opnfil[fn] := 0 { clear out file table entry }

   end

end;

{******************************************************************************

Read file

If the file is the input file, we process that by reading from the event queue
and returning any characters found. Any events besides character events are
discarded, which is why reading from the "input" file is a downward compatible
operation.
All other files are passed on to the system level.

******************************************************************************}

procedure fileread(fn: ss_filhdl; var ba: bytarr);

var i: integer; { index for destination }
    l: integer; { length left on destination }

begin

   if fn > outfil then { transparent file }
      if opnfil[fn] = 0 then error(einvhan); { invalid handle }
   if fn = inpfil then begin { process input file }

      i := 1; { set 1st byte of destination }
      l := max(ba); { set length of destination }
      while l > 0 do begin { read input bytes }

         { if there is no line in the input buffer, get one }
         if inpptr = 0 then readline;
         ba[i] := ord(inpbuf[inpptr]); { get and place next character }
         if inpptr < maxlin then inpptr := inpptr+1; { next }
         { if we have just read the last of that line, then flag buffer empty }
         if ba[i] = ord('\cr') then inpptr := 0;
         i := i+1; { next character }
         l := l-1 { count characters }

      end

   end else ss_old_read(opnfil[fn], ba, sav_read) { pass to lower level }

end;

{******************************************************************************

Write file

Outputs to the given file. If the file is the "output" file, then we process
it specially.

******************************************************************************}

procedure filewrite(fn: ss_filhdl; view ba: bytarr);

var i: integer; { index for destination }
    l: integer; { length left on destination }

begin

   if fn > outfil then { transparent file }
      if opnfil[fn] = 0 then error(einvhan); { invalid handle }
   if fn = outfil then begin { process output file }

      i := 1; { set 1st byte of source }
      l := max(ba); { set length of source }
      while l > 0 do begin { write output bytes }

         plcchr(chr(ba[i])); { send character to terminal emulator }
         i := i+1; { next character }
         l := l-1 { count characters }

      end

   end else ss_old_write(opnfil[fn], ba, sav_write) { pass to lower level }

end;

{******************************************************************************

Position file

Positions the given file. If the file is one of "input" or "output", we fail
call, as positioning is not valid on a special file.

******************************************************************************}

procedure fileposition(fn: ss_filhdl; p: integer);

begin

   if fn > outfil then { transparent file }
      if opnfil[fn] = 0 then error(einvhan); { invalid handle }
   { check operation performed on special file }
   if (fn = inpfil) or (fn = outfil) then error(efilopr);
   ss_old_position(opnfil[fn], p, sav_position) { pass to lower level }

end;

{******************************************************************************

Find location of file

Find the location of the given file. If the file is one of "input" or "output",
we fail the call, as this is not valid on a special file.

******************************************************************************}

function filelocation(fn: ss_filhdl): integer;

begin

   if fn > outfil then { transparent file }
      if opnfil[fn] = 0 then error(einvhan); { invalid handle }
   { check operation performed on special file }
   if (fn = inpfil) or (fn = outfil) then error(efilopr);
   filelocation := ss_old_location(opnfil[fn], sav_location) { pass to lower level }

end;

{******************************************************************************

Find length of file

Find the length of the given file. If the file is one of "input" or "output",
we fail the call, as this is not valid on a special file.

******************************************************************************}

function filelength(fn: ss_filhdl): integer;

begin

   if fn > outfil then { transparent file }
      if opnfil[fn] = 0 then error(einvhan); { invalid handle }
   { check operation performed on special file }
   if (fn = inpfil) or (fn = outfil) then error(efilopr);
   filelength := ss_old_length(opnfil[fn], sav_length) { pass to lower level }

end;

{******************************************************************************

Check eof of file

Checks if a given file is at eof. On our special files "input" and "output",
the eof is allways false. On "input", there is no idea of eof on a keyboard
input file. On "output", eof is allways false on a write only file.

******************************************************************************}

function fileeof(fn: ss_filhdl): boolean;

begin

   if fn > outfil then { transparent file }
      if opnfil[fn] = 0 then error(einvhan); { invalid handle }
   { check operation performed on special file }
   if (fn = inpfil) or (fn = outfil) then fileeof := false { yes, allways true }
   else fileeof := ss_old_eof(opnfil[fn], sav_eof) { pass to lower level }

end;

{******************************************************************************

Place string in storage

Places the given string into dynamic storage, and returns that.

******************************************************************************}

function str(view s: string): pstring;

var p: pstring;

begin

   new(p, max(s));
   p^ := s;
   str := p

end;

{******************************************************************************

Windows class handler

Handles messages for the dummy window. The purpose of this routine is to relay
timer and joystick messages back to the main console queue.

******************************************************************************}

function wndproc(hwnd, msg, wparam, lparam: integer): integer;

var b:       boolean;  { result holder }
    r:       integer;  { result holder }
    ne:      integer;  { number of events written }
    x, y, z: integer;
    inpevt:  array [1..1] of sc_input_record; { event buffer }

begin

   ne := 1; { set one event to write }
   if msg = sc_wm_create then begin

      r := 0

   end else if msg = sc_mm_joy1move then begin

      inpevt[1].eventtype := uiv_joy1move; { set event type }
      sc_crkmsg(lparam, y, x); { get x and y for joystick }
      inpevt[1].keyevent.wvirtualkeycode := x; { place }
      inpevt[1].keyevent.wvirtualscancode := y; { place }
      b := sc_writeconsoleinput(inphdl, inpevt, ne) { send }

   end else if msg = sc_mm_joy1zmove then begin

      inpevt[1].eventtype := uiv_joy1zmove; { set event type }
      z := lparam and $ffff; { get z for joystick }
      inpevt[1].keyevent.wvirtualkeycode := z; { place }
      b := sc_writeconsoleinput(inphdl, inpevt, ne) { send }

   end else if msg = sc_mm_joy2move then begin

      inpevt[1].eventtype := uiv_joy2move; { set event type }
      sc_crkmsg(lparam, y, x); { get x and y for joystick }
      inpevt[1].keyevent.wvirtualkeycode := x; { place }
      inpevt[1].keyevent.wvirtualscancode := y; { place }
      b := sc_writeconsoleinput(inphdl, inpevt, ne) { send }

   end else if msg = sc_mm_joy2zmove then begin

      inpevt[1].eventtype := uiv_joy2zmove; { set event type }
      z := lparam and $ffff; { get z for joystick }
      inpevt[1].keyevent.wvirtualkeycode := z; { place }
      b := sc_writeconsoleinput(inphdl, inpevt, ne) { send }

   end else if msg = sc_mm_joy1buttondown then begin

      inpevt[1].eventtype := uiv_joy1buttondown; { set event type }
      inpevt[1].keyevent.wvirtualkeycode := wparam; { place buttons }
      b := sc_writeconsoleinput(inphdl, inpevt, ne) { send }

   end else if msg = sc_mm_joy2buttondown then begin

      inpevt[1].eventtype := uiv_joy2buttondown; { set event type }
      inpevt[1].keyevent.wvirtualkeycode := wparam; { place buttons }
      b := sc_writeconsoleinput(inphdl, inpevt, ne) { send }

   end else if msg = sc_mm_joy1buttonup then begin

      inpevt[1].eventtype := uiv_joy1buttonup; { set event type }
      inpevt[1].keyevent.wvirtualkeycode := wparam; { place buttons }
      b := sc_writeconsoleinput(inphdl, inpevt, ne) { send }

   end else if msg = sc_mm_joy2buttonup then begin

      inpevt[1].eventtype := uiv_joy2buttonup; { set event type }
      inpevt[1].keyevent.wvirtualkeycode := wparam; { place buttons }
      b := sc_writeconsoleinput(inphdl, inpevt, ne) { send }

   end else if msg = sc_wm_destroy then begin

      sc_postquitmessage(0);
      r := 0

   end else r := sc_defwindowproc(hwnd, msg, wparam, lparam);

   wndproc := r

end;

{******************************************************************************

Window handler task

Timers, joysticks and other toys won't work unless they have a window with
full class handling to send messages to. So we create an "invisible" window
by creating a window that never gets presented. The timer and other messages
are sent to that, then the windows procedure for that forwards them via special
queue messages back to the console input queue.
This procedure gets its own thread, and is fairly self contained. It starts
and runs the dummy window. The following variables are used to communicate
with the main thread:

winhan         Gives the window to send timer messages to.

threadstart    Flags that the thread has started. We can't set up events for
               the dummy task until it starts. This is a hard wait, I am
               searching for a better method.

timers         Contains the "repeat" flag for each timer. There can be race
               conditions for this, it needs to be studied. Plus, we can use
               some of Windows fancy lock mechanisims.

numjoy         Sets the number of joysticks. This is valid after wait for
               task start.

******************************************************************************}

procedure dummyloop;

var msg: sc_msg;
    wc:  sc_wndclassa; { windows class structure }
    b:   boolean; { function return }
    r:   integer; { result holder }
    v:   integer;

begin

   { there are a few resources that can only be used by windowed programs, such
     as timers and joysticks. to enable these, we start a false windows
     procedure with a window that is never presented }
   v := $8000000;
   v := v*16;
   { set windows class to a normal window without scroll bars,
     with a windows procedure pointing to the message mirror.
     The message mirror reflects messages that should be handled
     by the program back into the queue, sending others on to
     the windows default handler }
   wc.style         := sc_cs_hredraw or sc_cs_vredraw or sc_cs_owndc;
   wc.wndproc   := sc_wndprocadr(wndproc);
   wc.clsextra    := 0;
   wc.wndextra    := 0;
   wc.instance     := sc_getmodulehandle_n;
   wc.icon         := sc_loadicon_n(sc_idi_application);
   wc.cursor       := sc_loadcursor_n(sc_idc_arrow);
   wc.background := sc_getstockobject(sc_white_brush);
   wc.menuname  := nil;
   wc.classname := str('stdwin');
   { register that class }
   b := sc_registerclass(wc);
   { create the window }
   winhan := sc_createwindowex(
                0, 'StdWin', 'Dummy', sc_ws_overlappedwindow,
                v{sc_cw_usedefault}, v{sc_cw_usedefault},
                v{sc_cw_usedefault}, v{sc_cw_usedefault},
                0, 0, sc_getmodulehandle_n
         );
   { capture joysticks }
   r := sc_joysetcapture(winhan, sc_joystickid1, 33, false);
   if r = 0 then numjoy := numjoy+1; { count }
   r := sc_joysetcapture(winhan, sc_joystickid2, 33, false);
   if r = 0 then numjoy := numjoy+1; { count }
   { flag subthread has started up }
   threadstart := true; { set we started }
   { message handling loop }
   while sc_getmessage(msg, 0, 0, 0) > 0 do begin { not a quit message }

      b := sc_translatemessage(msg); { translate keyboard events }
      r := sc_dispatchmessage(msg);

   end;
   { release the joysticks }
   r := sc_joyreleasecapture(sc_joystickid1);
   r := sc_joyreleasecapture(sc_joystickid2);

end;

{******************************************************************************

Console control handler

This procedure gets activated as a callback when Windows flags a termination
event. We send a termination message back to the event queue, and flag the
event handled.
At the present time, we don't care what type of termination event it was,
all generate an etterm signal.

******************************************************************************}

function conhan(ct: sc_dword): boolean;

var b:  boolean; { result holder }
    ne: integer; { number of events written }
    inpevt: array [1..1] of sc_input_record; { event buffer }

begin

   refer(ct); { not used }
   inpevt[1].eventtype := uiv_term; { set key event type }
   b := sc_writeconsoleinput(inphdl, inpevt, ne); { send }
   conhan := true { set event handled }

end;

begin

   { these attributes are not used }
   refer(sasubs, sablink, sasuper, sastkout);
   { override our interdicted calls }
   ss_ovr_openread(fileopenread, sav_openread);
   ss_ovr_openwrite(fileopenwrite, sav_openwrite);
   ss_ovr_close(fileclose, sav_close);
   ss_ovr_read(fileread, sav_read);
   ss_ovr_write(filewrite, sav_write);
   ss_ovr_position(fileposition, sav_position);
   ss_ovr_location(filelocation, sav_location);
   ss_ovr_length(filelength, sav_length);
   ss_ovr_eof(fileeof, sav_eof);
   { get handle of input file }
   inphdl := sc_getstdhandle(sc_std_input_handle);
   mb1 := false; { set mouse as assumed no buttons down, at origin }
   mb2 := false;
   mb3 := false;
   mb4 := false;
   mpx := 1;
   mpy := 1;
   nmb1 := false;
   nmb2 := false;
   nmb3 := false;
   nmb4 := false;
   nmpx := 1;
   nmpy := 1;
   joy1xs := 0; { clear joystick saves }
   joy1ys := 0;
   joy1zs := 0;
   joy2xs := 0;
   joy2ys := 0;
   joy2zs := 0;
   numjoy := 0; { set number of joysticks 0 }
   inpptr := 0; { set no input line active }
   frmrun := false; { set framing timer not running }
   { clear timer repeat array }
   for ti := 1 to 10 do begin

      timers[ti].han := 0; { set no active timer }
      timers[ti].rep := false { set no repeat }

   end;
   { clear open files table }
   for fi := 1 to ss_maxhdl do opnfil[fi] := 0; { set unoccupied }
   for cix := 1 to maxcon do screens[cix] := nil;
   new(screens[1]); { get the default screen }
   curdsp := 1; { set current display screen }
   curupd := 1; { set current update screen }
   { point handle to present output screen buffer }
   screens[curupd]^.han := sc_getstdhandle(sc_std_output_handle);
   b := sc_getconsolescreenbufferinfo(screens[curupd]^.han, bi);
   screens[curupd]^.maxx := bi.dwsize.x; { place maximum sizes }
   screens[curupd]^.maxy := bi.dwsize.y;
   screens[curupd]^.curx := bi.dwcursorposition.x+1; { place cursor position }
   screens[curupd]^.cury := bi.dwcursorposition.y+1;
   screens[curupd]^.conx := bi.dwcursorposition.x; { set our copy of position }
   screens[curupd]^.cony := bi.dwcursorposition.y;
   screens[curupd]^.sattr := bi.wattributes; { place default attributes }
   { place max setting for all screens }
   gmaxx := screens[curupd]^.maxx;
   gmaxy := screens[curupd]^.maxy;
   screens[curupd]^.auto := true; { turn on auto scroll and wrap }
   gauto := true;
   fndcolor(screens[curupd]^.sattr); { set colors from that }
   gforec := screens[curupd]^.forec;
   gbackc := screens[curupd]^.backc;
   b := sc_getconsolecursorinfo(screens[curupd]^.han, ci); { get cursor mode }
   screens[curupd]^.curv := ci.bvisible <> 0; { set cursor visible from that }
   gcurv := ci.bvisible <> 0;
   screens[curupd]^.attr := sanone; { set no attribute }
   gattr := sanone;
   { set up tabbing to be on each 8th position }
   for i := 1 to screens[curupd]^.maxx do
      screens[curupd]^.tab[i] := (i-1) mod 8 = 0;
   { turn on mouse events }
   b := sc_getconsolemode(inphdl, mode);
   b := sc_setconsolemode(inphdl, mode or sc_enable_mouse_input);
   { capture control handler }
   b := sc_setconsolectrlhandler(conhan, true);
   { interlock to make sure that thread starts before we continue }
   threadstart := false;
   r := sc_createthread_nn(0, dummyloop, 0, threadid);
   while not threadstart do; { wait for thread to start }

end;

begin

   88: { abort module }
{ we need to terminate dummy window task by sending it a quit message }
   { close all open files }
   for fi := 1 to ss_maxhdl do if opnfil[fi] <> 0 then ss_close(opnfil[fi])

end.
