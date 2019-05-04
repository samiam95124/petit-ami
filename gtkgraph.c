{******************************************************************************
*                                                                             *
*                        GRAPHICAL MODE LIBRARY FOR GTK                       *
*                                                                             *
*                       Copyright (C) 2019 Scott A. Franco                    *
*                                                                             *
*                            2019/05/03 S. A. Franco                          *
*                                                                             *
* Implements the graphical mode functions on GTK. Gralib is upward            *
* compatible with trmlib functions.                                           *
*                                                                             *
* Proposed improvements:                                                      *
*                                                                             *
* Move(f, d, dx, dy, s, sx1, sy1, sx2, sy2)                                   *
*                                                                             *
* Moves a block of pixels from one buffer to another, or to a different place *
* in the same buffer. Used to implement various features like intrabuffer     *
* moves, off screen image chaching, special clipping, etc.                    *
*                                                                             *
* fand, band                                                                  *
*                                                                             *
* Used with move to implement arbitrary clips usng move, above.               *
*                                                                             *
* History:                                                                    *
*                                                                             *
* Gralib started in 1996 as a graphical window demonstrator as a twin to      *
* ansilib, the ANSI control character based terminal mode library.            *
* In 2003, gralib was upgraded to the graphical terminal standard.            *
* In 2005, gralib was upgraded to include the window mangement calls, and the *
* widget calls.                                                               *
*                                                                             *
* Gralib uses three different tasks. The main task is passed on to the        *
* program, and two subthreads are created. The first one is to run the        *
* display, and the second runs widgets. The Display task both isolates the    *
* user interface from any hangs or slowdowns in the main thread, and also     *
* allows the display task to be a completely regular windows message loop     *
* with class handler, that just happens to communicate all of its results     *
* back to the main thread. This solves several small problems with adapting   *
* the X Windows/Mac OS style we use to Windows style. The main and the        *
* display thread are "joined" such that they can both access the same         *
* windows. The widget task is required because of this joining, and serves to *
* isolate the running of widgets from the main or display threads.            *
*                                                                             *
******************************************************************************}

module gralib;

uses windows, { uses windows system call wrapper }
     sysovr,  { system override library }
     winsup,  { windows support library }
     getfil;  { get logical file number from file }

const maxtim = 10; { maximum number of timers available }
      maxbuf = 10; { maximum number of buffers available }
      font_term = 1; { terminal font }
      font_book = 2; { book font }
      font_sign = 3; { sign font }
      font_tech = 4; { technical font (vector font) }
      iowin    = 1;  { logical window number of input/output pair }
      { standardized menu entries }
      smnew       = 1; { new file }
      smopen      = 2; { open file }
      smclose     = 3; { close file }
      smsave      = 4; { save file }
      smsaveas    = 5; { save file as name }
      smpageset   = 6; { page setup }
      smprint     = 7; { print }
      smexit      = 8; { exit program }
      smundo      = 9; { undo edit }
      smcut       = 10; { cut selection }
      smpaste     = 11; { paste selection }
      smdelete    = 12; { delete selection }
      smfind      = 13; { find text }
      smfindnext  = 14; { find next }
      smreplace   = 15; { replace text }
      smgoto      = 16; { goto line }
      smselectall = 17; { select all text }
      smnewwindow = 18; { new window }
      smtilehoriz = 19; { tile child windows horizontally }
      smtilevert  = 20; { tile child windows vertically }
      smcascade   = 21; { cascade windows }
      smcloseall  = 22; { close all windows }
      smhelptopic = 23; { help topics }
      smabout     = 24; { about this program }
      smmax       = 24; { maximum defined standard menu entries }

type { Colors displayable in text mode. Background is the color that will match
       widgets placed onto it. }
     color = (black, white, red, green, blue, cyan, yellow, magenta, backcolor);
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
     evtcod = (etchar,     { ANSI character returned }
               etup,       { cursor up one line }
               etdown,     { down one line }
               etleft,     { left one character }
               etright,    { right one character }
               etleftw,    { left one word }
               etrightw,   { right one word }
               ethome,     { home of document }
               ethomes,    { home of screen }
               ethomel,    { home of line }
               etend,      { end of document }
               etends,     { end of screen }
               etendl,     { end of line }
               etscrl,     { scroll left one character }
               etscrr,     { scroll right one character }
               etscru,     { scroll up one line }
               etscrd,     { scroll down one line }
               etpagd,     { page down }
               etpagu,     { page up }
               ettab,      { tab }
               etenter,    { enter line }
               etinsert,   { insert block }
               etinsertl,  { insert line }
               etinsertt,  { insert toggle }
               etdel,      { delete block }
               etdell,     { delete line }
               etdelcf,    { delete character forward }
               etdelcb,    { delete character backward }
               etcopy,     { copy block }
               etcopyl,    { copy line }
               etcan,      { cancel current operation }
               etstop,     { stop current operation }
               etcont,     { continue current operation }
               etprint,    { print document }
               etprintb,   { print block }
               etprints,   { print screen }
               etfun,      { function key }
               etmenu,     { display menu }
               etmouba,    { mouse button assertion }
               etmoubd,    { mouse button deassertion }
               etmoumov,   { mouse move }
               ettim,      { timer matures }
               etjoyba,    { joystick button assertion }
               etjoybd,    { joystick button deassertion }
               etjoymov,   { joystick move }
               etterm,     { terminate program }
               etmoumovg,  { mouse move graphical }
               etframe,    { frame sync }
               etresize,   { window was resized }
               etredraw,   { window redraw }
               etmin,      { window minimized }
               etmax,      { window maximized }
               etnorm,     { window normalized }
               etmenus,    { menu item selected }
               etbutton,   { button assert }
               etchkbox,   { checkbox click }
               etradbut,   { radio button click }
               etsclull,   { scroll up/left line }
               etscldrl,   { scroll down/right line }
               etsclulp,   { scroll up/left page }
               etscldrp,   { scroll down/right page }
               etsclpos,   { scroll bar position }
               etedtbox,   { edit box signals done }
               etnumbox,   { number select box signals done }
               etlstbox,   { list box selection }
               etdrpbox,   { drop box selection }
               etdrebox,   { drop edit box selection }
               etsldpos,   { slider position }
               ettabbar);  { tab bar select }
     { event record }
     evtrec = record

        winid: ss_filhdl; { identifier of window for event }
        case etype: evtcod of { event type }

           { ANSI character returned }
           etchar:   (char:                char);
           { timer handle that matured }
           ettim:     (timnum:              timhan);
           etmoumov:  (mmoun:               mouhan;   { mouse number }
                       moupx, moupy:        integer); { mouse movement }
           etmouba:   (amoun:               mouhan;   { mouse handle }
                       amoubn:              moubut);  { button number }
           etmoubd:   (dmoun:               mouhan;   { mouse handle }
                       dmoubn:              moubut);  { button number }
           etjoyba:   (ajoyn:               joyhan;   { joystick number }
                       ajoybn:              joybut);  { button number }
           etjoybd:   (djoyn:               joyhan;   { joystick number }
                       djoybn:              joybut);  { button number }
           etjoymov:  (mjoyn:               joyhan;   { joystick number }
                       joypx, joypy, joypz: integer); { joystick coordinates }
           etfun:     (fkey:                funky);   { function key }
           etmoumovg: (mmoung:              mouhan;   { mouse number }
                       moupxg, moupyg:      integer); { mouse movement }
           etredraw:  (rsx, rsy, rex, rey:  integer); { redraw screen }
           etmenus:   (menuid:              integer); { menu item selected }
           etbutton:  (butid:               integer); { button id }
           etchkbox:  (ckbxid:              integer); { checkbox }
           etradbut:  (radbid:              integer); { radio button }
           etsclull:  (sclulid:             integer); { scroll up/left line }
           etscldrl:  (scldlid:             integer); { scroll down/right line }
           etsclulp:  (sclupid:             integer); { scroll up/left page }
           etscldrp:  (scldpid:             integer); { scroll down/right page }
           etsclpos:  (sclpid:              integer;  { scroll bar }
                       sclpos:              integer); { scroll bar position }
           etedtbox:  (edtbid:              integer); { edit box complete }
           etnumbox:  (numbid:              integer;  { num sel box select }
                       numbsl:              integer); { num select value }
           etlstbox:  (lstbid:              integer;  { list box select }
                       lstbsl:              integer); { list box select number }
           etdrpbox:  (drpbid:              integer;  { drop box select }
                       drpbsl:              integer); { drop box select }
           etdrebox:  (drebid:              integer); { drop edit box select }
           etsldpos:  (sldpid:              integer;  { slider position }
                       sldpos:              integer); { slider position }
           ettabbar:  (tabid:               integer;  { tab bar }
                       tabsel:              integer); { tab select }
           etup, etdown, etleft, etright, etleftw, etrightw, ethome, ethomes,
           ethomel, etend, etends, etendl, etscrl, etscrr, etscru, etscrd,    
           etpagd, etpagu, ettab, etenter, etinsert, etinsertl, etinsertt, 
           etdel, etdell, etdelcf, etdelcb, etcopy, etcopyl, etcan, etstop,    
           etcont, etprint, etprintb, etprints, etmenu, etterm, etframe, 
           etresize, etmin, etmax, etnorm: (); { normal events }

        { end }

     end;
     { menu }
     menuptr = ^menurec;
     menurec = record

        next:   menuptr; { next menu item in list }
        branch: menuptr; { menu branch }
        onoff:  boolean; { on/off highlight }
        oneof:  boolean; { "one of" highlight }
        bar:    boolean; { place bar under }
        id:     integer; { id of menu item }
        face:   pstring  { text to place on button }

     end;
     { standard menu selector }
     stdmenusel = set of smnew..smmax;
     { windows mode sets }
     winmod = (wmframe,   { frame on/off }
               wmsize,    { size bars on/off }
               wmsysbar); { system bar on/off }
     winmodset = set of winmod;
     { string set for list box }
     strptr = ^strrec;
     strrec = record

        next: strptr; { next entry in list }
        str:  pstring { string }

     end;
     { orientation for tab bars }
     tabori = (totop, toright, tobottom, toleft); 
     { settable items in find query }
     qfnopt = (qfncase, qfnup, qfnre);
     qfnopts = set of qfnopt;
     { settable items in replace query }
     qfropt = (qfrcase, qfrup, qfrre, qfrfind, qfrallfil, qfralllin);
     qfropts = set of qfropt;
     { effects in font query }
     qfteffect = (qfteblink, qftereverse, qfteunderline, qftesuperscript,
                  qftesubscript, qfteitalic, qftebold, qftestrikeout,
                  qftestandout, qftecondensed, qfteextended, qftexlight,
                  qftelight, qftexbold, qftehollow, qfteraised);
     qfteffects = set of qfteffect;

{ functions at this level }

{ text }

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
overload procedure select(var f: text; d: integer); forward;
overload procedure select(d: integer); forward;
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
procedure autohold(e: boolean); forward;
procedure wrtstr(var f: text; view s: string); forward;
overload procedure wrtstr(view s: string); forward;

{ graphical }

function maxxg(var f: text): integer; forward;
overload function maxxg: integer; forward;
function maxyg(var f: text): integer; forward;
overload function maxyg: integer; forward;
function curxg(var f: text): integer; forward;
overload function curxg: integer; forward;
function curyg(var f: text): integer; forward;
overload function curyg: integer; forward;
procedure line(var f: text; x1, y1, x2, y2: integer); forward;
overload procedure line(x1, y1, x2, y2: integer); forward;
overload procedure line(var f: text; x2, y2: integer); forward;
overload procedure line(x2, y2: integer); forward;
procedure linewidth(var f: text; w: integer); forward;
overload procedure linewidth(w: integer); forward;
procedure rect(var f: text; x1, y1, x2, y2: integer); forward;
overload procedure rect(x1, y1, x2, y2: integer); forward;
procedure frect(var f: text; x1, y1, x2, y2: integer); forward;
overload procedure frect(x1, y1, x2, y2: integer); forward;
procedure rrect(var f: text; x1, y1, x2, y2, xs, ys: integer); forward;
overload procedure rrect(x1, y1, x2, y2, xs, ys: integer); forward;
procedure frrect(var f: text; x1, y1, x2, y2, xs, ys: integer); forward;
overload procedure frrect(x1, y1, x2, y2, xs, ys: integer); forward;
procedure ellipse(var f: text; x1, y1, x2, y2: integer); forward;
overload procedure ellipse(x1, y1, x2, y2: integer); forward;
procedure fellipse(var f: text; x1, y1, x2, y2: integer); forward;
overload procedure fellipse(x1, y1, x2, y2: integer); forward;
procedure arc(var f: text; x1, y1, x2, y2, sa, ea: integer); forward;
overload procedure arc(x1, y1, x2, y2, sa, ea: integer); forward;
procedure farc(var f: text; x1, y1, x2, y2, sa, ea: integer); forward;
overload procedure farc(x1, y1, x2, y2, sa, ea: integer); forward;
procedure fchord(var f: text; x1, y1, x2, y2, sa, ea: integer); forward;
overload procedure fchord(x1, y1, x2, y2, sa, ea: integer); forward;
procedure ftriangle(var f: text; x1, y1, x2, y2, x3, y3: integer); forward;
overload procedure ftriangle(x1, y1, x2, y2, x3, y3: integer); forward;
overload procedure ftriangle(var f: text; x2, y2, x3, y3: integer); forward;
overload procedure ftriangle(x2, y2, x3, y3: integer); forward;
overload procedure ftriangle(var f: text; x3, y3: integer); forward;
overload procedure ftriangle(x3, y3: integer); forward;
procedure cursorg(var f: text; x, y: integer); forward;
overload procedure cursorg(x, y: integer); forward;
function baseline(var f: text): integer; forward;
overload function baseline: integer; forward;
procedure setpixel(var f: text; x, y: integer); forward;
overload procedure setpixel(x, y: integer); forward;
procedure fover(var f: text); forward;
overload procedure fover; forward;
procedure bover(var f: text); forward;
overload procedure bover; forward;
procedure finvis(var f: text); forward;
overload procedure finvis; forward;
procedure binvis(var f: text); forward;
overload procedure binvis; forward;
procedure fxor(var f: text); forward;
overload procedure fxor; forward;
procedure bxor(var f: text); forward;
overload procedure bxor; forward;
function chrsizx(var f: text): integer; forward;
overload function chrsizx: integer; forward;
function chrsizy(var f: text): integer; forward;
overload function chrsizy: integer; forward;
function fonts(var f: text): integer; forward;
overload function fonts: integer; forward;
procedure font(var f: text; fc: integer); forward;
overload procedure font(fc: integer); forward;
procedure fontnam(var f: text; fc: integer; var fns: string); forward;
overload procedure fontnam(fc: integer; var fns: string); forward;
procedure fontsiz(var f: text; s: integer); forward;
overload procedure fontsiz(s: integer); forward;
procedure chrspcy(var f: text; s: integer); forward;
overload procedure chrspcy(s: integer); forward;
procedure chrspcx(var f: text; s: integer); forward;
overload procedure chrspcx(s: integer); forward;
function dpmx(var f: text): integer; forward;
overload function dpmx: integer; forward;
function dpmy(var f: text): integer; forward;
overload function dpmy: integer; forward;
function strsiz(var f: text; view s: string): integer; forward;
overload function strsiz(view s: string): integer; forward;
function strsizp(var f: text; view s: string): integer; forward;
overload function strsizp(view s: string): integer; forward;
function chrpos(var f: text; view s: string; p: integer): integer; forward;
overload function chrpos(view s: string; p: integer): integer; forward;
procedure writejust(var f: text; view s: string; n: integer); forward;
overload procedure writejust(view s: string; n: integer); forward;
function justpos(var f: text; view s: string; p, n: integer): integer; forward;
overload function justpos(view s: string; p, n: integer): integer; forward;
procedure condensed(var f: text; e: boolean); forward;
overload procedure condensed(e: boolean); forward;
procedure extended(var f: text; e: boolean); forward;
overload procedure extended(e: boolean); forward;
procedure xlight(var f: text; e: boolean); forward;
overload procedure xlight(e: boolean); forward;
procedure light(var f: text; e: boolean); forward;
overload procedure light(e: boolean); forward;
procedure xbold(var f: text; e: boolean); forward;
overload procedure xbold(e: boolean); forward;
procedure hollow(var f: text; e: boolean); forward;
overload procedure hollow(e: boolean); forward;
procedure raised(var f: text; e: boolean); forward;
overload procedure raised(e: boolean); forward;
procedure settabg(var f: text; t: integer); forward;
overload procedure settabg(t: integer); forward;
procedure restabg(var f: text; t: integer); forward;
overload procedure restabg(t: integer); forward;
{ Note that we overload the graphical color sets into the old text color sets.
  However, we also provide the "g" postfixed versions for compatability for
  pre-overload code. }
procedure fcolorg(var f: text; r, g, b: integer); forward;
overload procedure fcolor(var f: text; r, g, b: integer); forward;
overload procedure fcolor(r, g, b: integer); forward;
procedure bcolorg(var f: text; r, g, b: integer); forward;
overload procedure bcolorg(r, g, b: integer); forward;
overload procedure bcolor(var f: text; r, g, b: integer); forward;
overload procedure bcolor(r, g, b: integer); forward;
procedure loadpict(var f: text; p: integer; view fn: string); forward;
overload procedure loadpict(p: integer; view fn: string); forward;
function pictsizx(var f: text; p: integer): integer; forward;
overload function pictsizx(p: integer): integer; forward;
function pictsizy(var f: text; p: integer): integer; forward;
overload function pictsizy(p: integer): integer; forward;
procedure picture(var f: text; p: integer; x1, y1, x2, y2: integer); forward;
overload procedure picture(p: integer; x1, y1, x2, y2: integer); forward;
procedure delpict(var f: text; p: integer); forward;
overload procedure delpict(p: integer); forward;
procedure scrollg(var f: text; x, y: integer); forward;
overload procedure scrollg(x, y: integer); forward;
{ These are experimental, and are not part of the gralib standard }
procedure viewoffg(var f: text; x, y: integer); forward;
overload procedure viewoffg(x, y: integer); forward;
procedure viewscale(var f: text; x, y: real); forward;
overload procedure viewscale(x, y: real); forward;
overload procedure viewscale(var f: text; s: real); forward;
overload procedure viewscale(s: real); forward;

{ Window management functions }

procedure title(var f: text; view ts: string); forward;
overload procedure title(view ts: string); forward;
procedure openwin(var infile, outfile, parent: text; wid: ss_filhdl); forward;
overload procedure openwin(var infile, outfile: text; wid: ss_filhdl); forward;
procedure buffer(var f: text; e: boolean); forward;
overload procedure buffer(e: boolean); forward;
procedure sizbuf(var f: text; x, y: integer); forward;
overload procedure sizbuf(x, y: integer); forward;
procedure sizbufg(var f: text; x, y: integer); forward;
overload procedure sizbufg(x, y: integer); forward;
procedure getsiz(var f: text; var x, y: integer); forward;
overload procedure getsiz(var x, y: integer); forward;
procedure getsizg(var f: text; var x, y: integer); forward;
overload procedure getsizg(var x, y: integer); forward;
procedure setsiz(var f: text; x, y: integer); forward;
overload procedure setsiz(x, y: integer); forward;
procedure setsizg(var f: text; x, y: integer); forward;
overload procedure setsizg(x, y: integer); forward;
procedure setpos(var f: text; x, y: integer); forward;
overload procedure setpos(x, y: integer); forward;
procedure setposg(var f: text; x, y: integer); forward;
overload procedure setposg(x, y: integer); forward;
procedure scnsiz(var f: text; var x, y: integer); forward;
overload procedure scnsiz(var x, y: integer); forward;
procedure scnsizg(var f: text; var x, y: integer); forward;
overload procedure scnsizg(var x, y: integer); forward;
procedure winclient(var f: text; cx, cy: integer; var wx, wy: integer;
                    view ms: winmodset); forward;
overload procedure winclient(cx, cy: integer; var wx, wy: integer; 
                             view ms: winmodset); forward;
procedure winclientg(var f: text; cx, cy: integer; var wx, wy: integer;
                     view ms: winmodset); forward;
overload procedure winclientg(cx, cy: integer; var wx, wy: integer;
                              view ms: winmodset); forward;
procedure front(var f: text); forward;
overload procedure front; forward;
procedure back(var f: text); forward;
overload procedure back; forward;
procedure frame(var f: text; e: boolean); forward;
overload procedure frame(e: boolean); forward;
procedure sizable(var f: text; e: boolean); forward;
overload procedure sizable(e: boolean); forward;
procedure sysbar(var f: text; e: boolean); forward;
overload procedure sysbar(e: boolean); forward;
procedure menu(var f: text; m: menuptr); forward;
overload procedure menu(m: menuptr); forward;
procedure menuena(var f: text; id: integer; onoff: boolean); forward;
overload procedure menuena(id: integer; onoff: boolean); forward;
procedure menusel(var f: text; id: integer; select: boolean); forward;
overload procedure menusel(id: integer; select: boolean); forward;
procedure stdmenu(view sms: stdmenusel; var sm: menuptr; pm: menuptr); forward;

{ widgets/controls }

procedure killwidget(var f: text; id: integer); forward;
overload procedure killwidget(id: integer); forward;
procedure selectwidget(var f: text; id: integer; e: boolean); forward;
overload procedure selectwidget(id: integer; e: boolean); forward;
procedure enablewidget(var f: text; id: integer; e: boolean); forward;
overload procedure enablewidget(id: integer; e: boolean); forward;
procedure getwidgettext(var f: text; id: integer; var s: pstring); forward;
overload procedure getwidgettext(id: integer; var s: pstring); forward;
procedure putwidgettext(var f: text; id: integer; view s: string); forward;
overload procedure putwidgettext(id: integer; view s: string); forward;
procedure sizwidgetg(var f: text; id: integer; x, y: integer); forward;
overload procedure sizwidgetg(id: integer; x, y: integer); forward;
procedure poswidgetg(var f: text; id: integer; x, y: integer); forward;
overload procedure poswidgetg(id: integer; x, y: integer); forward;
procedure buttonsiz(var f: text; view s: string; var w, h: integer); forward;
overload procedure buttonsiz(view s: string; var w, h: integer); forward;
procedure buttonsizg(var f: text; view s: string; var w, h: integer); forward;
overload procedure buttonsizg(view s: string; var w, h: integer); forward;
procedure button(var f: text; x1, y1, x2, y2: integer; view s: string; 
                 id: integer); forward;
overload procedure button(x1, y1, x2, y2: integer; view s: string;
                          id: integer); forward;
procedure buttong(var f: text; x1, y1, x2, y2: integer; view s: string; 
                 id: integer); forward;
overload procedure buttong(x1, y1, x2, y2: integer; view s: string;
                          id: integer); forward;
procedure checkboxsiz(var f: text; view s: string; var w, h: integer); forward;
overload procedure checkboxsiz(view s: string; var w, h: integer); forward;
procedure checkboxsizg(var f: text; view s: string; var w, h: integer); forward;
overload procedure checkboxsizg(view s: string; var w, h: integer); forward;
procedure checkbox(var f: text; x1, y1, x2, y2: integer; view s: string; 
                   id: integer); forward;
overload procedure checkbox(x1, y1, x2, y2: integer; view s: string; 
                            id: integer); forward;
procedure checkboxg(var f: text; x1, y1, x2, y2: integer; view s: string; 
                   id: integer); forward;
overload procedure checkboxg(x1, y1, x2, y2: integer; view s: string; 
                            id: integer); forward;
procedure radiobuttonsiz(var f: text; view s: string; var w, h: integer); forward;
overload procedure radiobuttonsiz(view s: string; var w, h: integer); forward;
procedure radiobuttonsizg(var f: text; view s: string; var w, h: integer); forward;
overload procedure radiobuttonsizg(view s: string; var w, h: integer); forward;
procedure radiobutton(var f: text; x1, y1, x2, y2: integer; view s: string; 
                      id: integer); forward;
overload procedure radiobutton(x1, y1, x2, y2: integer; view s: string; 
                               id: integer); forward;
procedure radiobuttong(var f: text; x1, y1, x2, y2: integer; view s: string; 
                      id: integer); forward;
overload procedure radiobuttong(x1, y1, x2, y2: integer; view s: string; 
                               id: integer); forward;
procedure groupsizg(var f: text; view s: string; cw, ch: integer;
                    var w, h, ox, oy: integer); forward;
overload procedure groupsizg(view s: string; cw, ch: integer;
                             var w, h, ox, oy: integer); forward;
procedure groupsiz(var f: text; view s: string; cw, ch: integer;
                   var w, h, ox, oy: integer); forward;
overload procedure groupsiz(view s: string; cw, ch: integer;
                            var w, h, ox, oy: integer); forward;
procedure group(var f: text; x1, y1, x2, y2: integer; view s: string; 
                id: integer); forward;
overload procedure group(x1, y1, x2, y2: integer; view s: string; 
                         id: integer); forward;
procedure groupg(var f: text; x1, y1, x2, y2: integer; view s: string; 
                id: integer); forward;
overload procedure groupg(x1, y1, x2, y2: integer; view s: string; 
                         id: integer); forward;
procedure background(var f: text; x1, y1, x2, y2: integer; id: integer); 
   forward;
overload procedure background(x1, y1, x2, y2: integer; id: integer); forward;
procedure backgroundg(var f: text; x1, y1, x2, y2: integer; id: integer); 
   forward;
overload procedure backgroundg(x1, y1, x2, y2: integer; id: integer); forward;
procedure scrollvertsizg(var f: text; var w, h: integer); forward;
overload procedure scrollvertsizg(var w, h: integer); forward;
procedure scrollvertsiz(var f: text; var w, h: integer); forward;
overload procedure scrollvertsiz(var w, h: integer); forward;
procedure scrollvert(var f: text; x1, y1, x2, y2: integer; id: integer);
          forward;
overload procedure scrollvert(x1, y1, x2, y2: integer; id: integer);
          forward;
procedure scrollvertg(var f: text; x1, y1, x2, y2: integer; id: integer);
          forward;
overload procedure scrollvertg(x1, y1, x2, y2: integer; id: integer);
          forward;
procedure scrollhorizsizg(var f: text; var w, h: integer); forward;
overload procedure scrollhorizsizg(var w, h: integer); forward;
procedure scrollhorizsiz(var f: text; var w, h: integer); forward;
overload procedure scrollhorizsiz(var w, h: integer); forward;
procedure scrollhoriz(var f: text; x1, y1, x2, y2: integer; id: integer);
          forward;
overload procedure scrollhoriz(x1, y1, x2, y2: integer; id: integer);
          forward;
procedure scrollhorizg(var f: text; x1, y1, x2, y2: integer; id: integer);
          forward;
overload procedure scrollhorizg(x1, y1, x2, y2: integer; id: integer);
          forward;
procedure scrollpos(var f: text; id: integer; r: integer); forward;
overload procedure scrollpos(id: integer; r: integer); forward;
procedure scrollsiz(var f: text; id: integer; r: integer); forward;
overload procedure scrollsiz(id: integer; r: integer); forward;
procedure numselboxsizg(var f: text; l, u: integer; var w, h: integer); forward;
overload procedure numselboxsizg(l, u: integer; var w, h: integer); forward;
procedure numselboxsiz(var f: text; l, u: integer; var w, h: integer); forward;
overload procedure numselboxsiz(l, u: integer; var w, h: integer); forward;
procedure numselbox(var f: text; x1, y1, x2, y2: integer; l, u: integer;
                    id: integer); forward;
overload procedure numselbox(x1, y1, x2, y2: integer; l, u: integer;
                    id: integer); forward;
procedure numselboxg(var f: text; x1, y1, x2, y2: integer; l, u: integer;
                    id: integer); forward;
overload procedure numselboxg(x1, y1, x2, y2: integer; l, u: integer;
                    id: integer); forward;
procedure editboxsizg(var f: text; view s: string; var w, h: integer); forward;
overload procedure editboxsizg(view s: string; var w, h: integer); forward;
procedure editboxsiz(var f: text; view s: string; var w, h: integer); forward;
overload procedure editboxsiz(view s: string; var w, h: integer); forward;
procedure editbox(var f: text; x1, y1, x2, y2: integer; id: integer);
          forward;
overload procedure editbox(x1, y1, x2, y2: integer; id: integer);
          forward;
procedure editboxg(var f: text; x1, y1, x2, y2: integer; id: integer);
          forward;
overload procedure editboxg(x1, y1, x2, y2: integer; id: integer);
          forward;
procedure progbarsizg(var f: text; var w, h: integer); forward;
overload procedure progbarsizg(var w, h: integer); forward;
procedure progbarsiz(var f: text; var w, h: integer); forward;
overload procedure progbarsiz(var w, h: integer); forward;
procedure progbar(var f: text; x1, y1, x2, y2: integer; id: integer); forward;
overload procedure progbar(x1, y1, x2, y2: integer; id: integer); forward;
procedure progbarg(var f: text; x1, y1, x2, y2: integer; id: integer); forward;
overload procedure progbarg(x1, y1, x2, y2: integer; id: integer); forward;
procedure progbarpos(var f: text; id: integer; pos: integer); forward;
overload procedure progbarpos(id: integer; pos: integer); forward;
procedure listboxsizg(var f: text; sp: strptr; var w, h: integer); forward;
overload procedure listboxsizg(sp: strptr; var w, h: integer); forward;
procedure listboxsiz(var f: text; sp: strptr; var w, h: integer); forward;
overload procedure listboxsiz(sp: strptr; var w, h: integer); forward;
procedure listbox(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                  id: integer); forward;
overload procedure listbox(x1, y1, x2, y2: integer; sp: strptr; 
                  id: integer); forward;
procedure listboxg(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                  id: integer); forward;
overload procedure listboxg(x1, y1, x2, y2: integer; sp: strptr; 
                  id: integer); forward;
procedure dropboxsizg(var f: text; sp: strptr; var cw, ch, ow, oh: integer);
   forward;
overload procedure dropboxsizg(sp: strptr; var cw, ch, ow, oh: integer);
   forward;
procedure dropboxsiz(var f: text; sp: strptr; var cw, ch, ow, oh: integer);
   forward;
overload procedure dropboxsiz(sp: strptr; var cw, ch, ow, oh: integer);
   forward;
procedure dropbox(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                  id: integer); forward;
overload procedure dropbox(x1, y1, x2, y2: integer; sp: strptr; 
                  id: integer); forward;
procedure dropboxg(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                  id: integer); forward;
overload procedure dropboxg(x1, y1, x2, y2: integer; sp: strptr; 
                  id: integer); forward;
procedure dropeditboxsizg(var f: text; sp: strptr; var cw, ch, ow, oh: integer);
   forward;
overload procedure dropeditboxsizg(sp: strptr; var cw, ch, ow, oh: integer);
   forward;
procedure dropeditboxsiz(var f: text; sp: strptr; var cw, ch, ow, oh: integer);
   forward;
overload procedure dropeditboxsiz(sp: strptr; var cw, ch, ow, oh: integer); 
   forward;
procedure dropeditbox(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                      id: integer); forward;
overload procedure dropeditbox(x1, y1, x2, y2: integer; sp: strptr; 
                      id: integer); forward;
procedure dropeditboxg(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                      id: integer); forward;
overload procedure dropeditboxg(x1, y1, x2, y2: integer; sp: strptr; 
                      id: integer); forward;
procedure slidehorizsizg(var f: text; var w, h: integer); forward;
overload procedure slidehorizsizg(var w, h: integer); forward;
procedure slidehorizsiz(var f: text; var w, h: integer); forward;
overload procedure slidehorizsiz(var w, h: integer); forward;
procedure slidehoriz(var f: text; x1, y1, x2, y2: integer; mark: integer;
                     id: integer); forward;
overload procedure slidehoriz(x1, y1, x2, y2: integer; mark: integer;
                     id: integer); forward;
procedure slidehorizg(var f: text; x1, y1, x2, y2: integer; mark: integer;
                     id: integer); forward;
overload procedure slidehorizg(x1, y1, x2, y2: integer; mark: integer;
                     id: integer); forward;
procedure slidevertsizg(var f: text; var w, h: integer); forward;
overload procedure slidevertsizg(var w, h: integer); forward;
procedure slidevertsiz(var f: text; var w, h: integer); forward;
overload procedure slidevertsiz(var w, h: integer); forward;
procedure slidevert(var f: text; x1, y1, x2, y2: integer; mark: integer;
                    id: integer); forward;
overload procedure slidevert(x1, y1, x2, y2: integer; mark: integer;
                    id: integer); forward;
procedure slidevertg(var f: text; x1, y1, x2, y2: integer; mark: integer;
                    id: integer); forward;
overload procedure slidevertg(x1, y1, x2, y2: integer; mark: integer;
                    id: integer); forward;
procedure tabbarsizg(var f: text; tor: tabori; cw, ch: integer; 
                     var w, h, ox, oy: integer); forward;
overload procedure tabbarsizg(tor: tabori; cw, ch: integer; 
                              var w, h, ox, oy: integer); forward;
procedure tabbarsiz(var f: text; tor: tabori; cw, ch: integer; 
                    var w, h, ox, oy: integer); forward;
overload procedure tabbarsiz(tor: tabori; cw, ch: integer; 
                             var w, h, ox, oy: integer); forward;
procedure tabbarclientg(var f: text; tor: tabori; w, h: integer; 
                     var cw, ch, ox, oy: integer); forward;
overload procedure tabbarclientg(tor: tabori; w, h: integer; 
                                 var cw, ch, ox, oy: integer); forward;
procedure tabbarclient(var f: text; tor: tabori; w, h: integer; 
                    var cw, ch, ox, oy: integer); forward;
overload procedure tabbarclient(tor: tabori; w, h: integer; 
                                var cw, ch, ox, oy: integer); forward;
procedure tabbar(var f: text; x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                 id: integer); forward;
overload procedure tabbar(x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                 id: integer); forward;
procedure tabbarg(var f: text; x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                 id: integer); forward;
overload procedure tabbarg(x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                 id: integer); forward;
procedure tabsel(var f: text; id: integer; tn: integer); forward;
overload procedure tabsel(id: integer; tn: integer); forward;
procedure alert(view title, message: string); forward;
procedure querycolor(var r, g, b: integer); forward;
procedure queryopen(var s: pstring); forward;
procedure querysave(var s: pstring); forward;
procedure queryfind(var s: pstring; var opt: qfnopts); forward;
procedure queryfindrep(var s, r: pstring; var opt: qfropts); forward;
procedure queryfont(var f: text; var fc, s, fr, fg, fb, br, bg, bb: integer;
                    var effect: qfteffects); forward;
overload procedure queryfont(var f: text; var fc, s: integer; 
                             var fcl, bcl: color;
                             var effect: qfteffects); forward;
overload procedure queryfont(var fc, s, fr, fg, fb, br, bg, bb: integer; 
                             var effect: qfteffects); forward;
overload procedure queryfont(var fc, s: integer; var fcl, bcl: color;
                             var effect: qfteffects); forward;
{ These are experimental, and are not part of the gralib standard }
procedure backwidget(var f: text; id: integer); forward;
overload procedure backwidget(id: integer); forward;
procedure frontwidget(var f: text; id: integer); forward;
overload procedure frontwidget(id: integer); forward;

   errcod = (eftbful,  { File table full }
             ejoyacc,  { Joystick access }
             etimacc,  { Timer access }
             efilopr,  { Cannot perform operation on special file }
             einvscn,  { Invalid screen number }
             einvhan,  { Invalid handle }
             einvtab,  { Invalid tab position }
             eatopos,  { Cannot position text by pixel with auto on }
             eatocur,  { Cannot position outside screen with auto on }
             eatoofg,  { Cannot reenable auto off grid }
             eatoecb,  { Cannot reenable auto outside screen }
             einvftn,  { Invalid font number }
             etrmfnt,  { Valid terminal font not found }
             eatofts,  { Cannot resize font with auto enabled }
             eatoftc,  { Cannot change fonts with auto enabled }
             einvfnm,  { Invalid logical font number }
             efntemp,  { Empty logical font }
             etrmfts,  { Cannot size terminal font }
             etabful,  { Too many tabs set }
             eatotab,  { Cannot use graphical tabs with auto on }
             estrinx,  { String index out of range }
             epicfnf,  { Picture file not found }
             epicftl,  { Picture filename too large }
             etimnum,  { Invalid timer number }
             ejstsys,  { Cannot justify system font }
             efnotwin, { File is not attached to a window }
             ewinuse,  { Window id in use }
             efinuse,  { File already in use }
             einmode,  { Input side of window in wrong mode }
             edcrel,   { Cannot release Windows device context }
             einvsiz,  { Invalid buffer size }
             ebufoff,  { buffered mode not enabled }
             edupmen,  { Menu id was duplicated }
             emennf,   { Meny id was not found }
             ewignf,   { Widget id was not found }
             ewigdup,  { Widget id was duplicated }
             einvspos, { Invalid scroll bar slider position }
             einvssiz, { Invalid scroll bar size }
             ectlfal,  { Attempt to create control fails }
             eprgpos,  { Invalid progress bar position }
             estrspc,  { Out of string space }
             etabbar,  { Unable to create tab in tab bar }
             efildlg,  { Unable to create file dialog }
             efnddlg,  { Unable to create find dialog }
             efntdlg,  { Unable to create font dialog }
             efndstl,  { Find/replace string too long }
             einvwin,  { Invalid window number }
             einvjye,  { Invalid joystick event }
             ejoyqry,  { Could not get information on joystick }
             einvjoy,  { Invalid joystick ID }
             eclsinw,  { Cannot directly close input side of window }
             ewigsel,  { Widget is not selectable }
             ewigptxt, { Cannot put text in this widget }
             ewiggtxt, { Cannot get text from this widget }
             ewigdis,  { Cannot disable this widget }
             estrato,  { Cannot direct write string with auto on }
             etabsel,  { Invalid tab select }
             esystem); { System consistency check }

{******************************************************************************

Scroll screen

Scrolls the ANSI terminal screen by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source and destination rectangles, and use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

******************************************************************************}

procedure scrollg(var f: text; x, y: integer);

begin
end;

procedure scroll(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

******************************************************************************}

procedure cursor(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Position cursor graphical

Moves the cursor to the specified x and y location in pixels.

******************************************************************************}

procedure cursorg(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Find character baseline

Returns the offset, from the top of the current fonts character bounding box,
to the font baseline. The baseline is the line all characters rest on.

******************************************************************************}

function baseline(var f: text): integer;

begin

end;

{******************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

******************************************************************************}

function maxx(var f: text): integer;

begin

end;

{******************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

******************************************************************************}

function maxy(var f: text): integer;

begin

end;

{******************************************************************************

Return maximum x dimension graphical

Returns the maximum x dimension, which is the width of the client surface in
pixels.

******************************************************************************}

function maxxg(var f: text): integer;

begin

end;

{******************************************************************************

Return maximum y dimension graphical

Returns the maximum y dimension, which is the height of the client surface in
pixels.

******************************************************************************}

function maxyg(var f: text): integer;

begin
end;

{******************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

******************************************************************************}

procedure home(var f: text);

begin

end;

{******************************************************************************

Move cursor up internal

Moves the cursor position up one line.

******************************************************************************}

procedure up(var f: text);

begin

end;

{******************************************************************************

Move cursor down internal

Moves the cursor position down one line.

******************************************************************************}

procedure down(var f: text);

begin
end;

{******************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

******************************************************************************}

procedure left(var f: text);

begin
end;

{******************************************************************************

Move cursor right

Moves the cursor one character right.

******************************************************************************}

procedure right(var f: text);

begin

end;

{******************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

******************************************************************************}

procedure blink(var f: text; e: boolean);

begin

end;

{******************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

******************************************************************************}

procedure reverse(var f: text; e: boolean);

begin

end;

{******************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.
This is not implemented, but could be done by drawing a line under each
character drawn.

******************************************************************************}

procedure underline(var f: text; e: boolean);

begin

end;

{******************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

******************************************************************************}

procedure superscript(var f: text; e: boolean);

begin

end;

{******************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

******************************************************************************}

procedure subscript(var f: text; e: boolean);

begin

end;

{******************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

Italic is causing problems with fixed mode on some fonts, and Windows does not
seem to want to share with me just what the true width of an italic font is
(without taking heroic measures like drawing and testing pixels). So we disable
italic on fixed fonts.

******************************************************************************}

procedure italic(var f: text; e: boolean);

begin

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

end;

{******************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

******************************************************************************}

procedure standout(var f: text; e: boolean);

begin

end;

{******************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

******************************************************************************}

procedure fcolor(var f: text; c: color);

begin

end;

procedure fcolorc(var f: text; r, g, b: integer);

begin

end;

{******************************************************************************

Set foreground color graphical

Sets the foreground color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

Fcolor exists as an overload to the text version, but we also provide an
fcolorg for backward compatiblity to the days before overloads.

******************************************************************************}

procedure fcolorg(var f: text; r, g, b: integer);

begin

end;

{******************************************************************************

Set background color

Sets the background color from the universal primary code.

******************************************************************************}

procedure bcolor(var f: text; c: color);

begin

end;

procedure bcolorc(var f: text; r, g, b: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   ibcolorg(win, r, g, b); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set background color graphical

Sets the background color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

******************************************************************************}

procedure bcolorg(var f: text; r, g, b: integer);

begin

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

end;

{******************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

******************************************************************************}

procedure curvis(var f: text; e: boolean);

begin

end;

{******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

******************************************************************************}

function curx(var f: text): integer;

begin

end;

{******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

******************************************************************************}

function cury(var f: text): integer;

begin

end;

{******************************************************************************

Get location of cursor in x graphical

Returns the current location of the cursor in x, in pixels.

******************************************************************************}

function curxg(var f: text): integer;

begin

end;

{******************************************************************************

Get location of cursor in y graphical

Returns the current location of the cursor in y, in pixels.

******************************************************************************}

function curyg(var f: text): integer;

begin

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

procedure select(var f: text; u, d: integer);

begin

end;

{******************************************************************************

Write string to current cursor position

Writes a string to the current cursor position, then updates the cursor
position. This acts as a series of write character calls. However, it eliminates
several layers of protocol, and results in much faster write time for
applications that require it.

It is an error to call this routine with auto enabled, since it could exceed
the bounds of the screen.

No control characters or other interpretation is done, and invisible characters
such as controls are not suppressed.

******************************************************************************}

procedure wrtstr(var f: text; view s: string);

begin

end;

{******************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

******************************************************************************}

procedure del(var f: text);

begin

end;

{******************************************************************************

Draw line

Draws a single line in the foreground color.

******************************************************************************}

procedure line(var f: text; x1, y1, x2, y2: integer);

begin

end;

{******************************************************************************

Draw rectangle

Draws a rectangle in foreground color.

******************************************************************************}

procedure rect(var f: text; x1, y1, x2, y2: integer);

begin

end;

{******************************************************************************

Draw filled rectangle

Draws a filled rectangle in foreground color.

******************************************************************************}

procedure frect(var f: text; x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifrect(win, x1, y1, x2, y2); { draw rectangle }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw rounded rectangle

Draws a rounded rectangle in foreground color.

******************************************************************************}

procedure rrect(var f: text; x1, y1, x2, y2, xs, ys: integer);

begin

end;

{******************************************************************************

Draw filled rounded rectangle

Draws a filled rounded rectangle in foreground color.

******************************************************************************}

procedure frrect(var f: text; x1, y1, x2, y2, xs, ys: integer);

begin

end;

{******************************************************************************

Draw ellipse

Draws an ellipse with the current foreground color and line width.

******************************************************************************}

procedure ellipse(var f: text; x1, y1, x2, y2: integer);

begin

end;

{******************************************************************************

Draw filled ellipse

Draws a filled ellipse with the current foreground color.

******************************************************************************}

procedure fellipse(var f: text; x1, y1, x2, y2: integer);

begin

end;

{******************************************************************************

Draw arc

Draws an arc in the current foreground color and line width. The containing
rectangle of the ellipse is given, and the start and end angles clockwise from
0 degrees delimit the arc.

Windows takes the start and end delimited by a line extending from the center
of the arc. The way we do the convertion is to project the angle upon a circle
whose radius is the precision we wish to use for the calculation. Then that
point on the circle is found by triangulation.

The larger the circle of precision, the more angles can be represented, but
the trade off is that the circle must not reach the edge of an integer
(-maxint..maxint). That means that the total logical coordinate space must be
shortened by the precision. To find out what division of the circle precis
represents, use cd := precis*2*pi. So, for example, precis = 100 means 628
divisions of the circle.

The end and start points can be negative.
Note that Windows draws arcs counterclockwise, so our start and end points are
swapped.

Negative angles are allowed.

******************************************************************************}

procedure arc(var f: text; x1, y1, x2, y2, sa, ea: integer);

begin

end;

{******************************************************************************

Draw filled arc

Draws a filled arc in the current foreground color. The same comments apply
as for the arc function above.

******************************************************************************}

procedure farc(var f: text; x1, y1, x2, y2, sa, ea: integer);

begin

end;

{******************************************************************************

Draw filled cord

Draws a filled cord in the current foreground color. The same comments apply
as for the arc function above.

******************************************************************************}

procedure fchord(var f: text; x1, y1, x2, y2, sa, ea: integer);

begin

end;

{******************************************************************************

Draw filled triangle

Draws a filled triangle in the current foreground color.

******************************************************************************}

procedure ftriangle(var f: text; x1, y1, x2, y2, x3, y3: integer);

begin

end;

{******************************************************************************

Set pixel

Sets a single logical pixel to the foreground color.

******************************************************************************}

procedure setpixel(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Set foreground to overwrite

Sets the foreground write mode to overwrite.

******************************************************************************}

procedure fover(var f: text);

begin

end;

{******************************************************************************

Set background to overwrite

Sets the background write mode to overwrite.

******************************************************************************}

procedure bover(var f: text);

begin

end;

{******************************************************************************

Set foreground to invisible

Sets the foreground write mode to invisible.

******************************************************************************}

procedure finvis(var f: text);

begin

end;

{******************************************************************************

Set background to invisible

Sets the background write mode to invisible.

******************************************************************************}

procedure binvis(var f: text);

begin

end;

{******************************************************************************

Set foreground to xor

Sets the foreground write mode to xor.

******************************************************************************}

procedure fxor(var f: text);

begin

end;

{******************************************************************************

Set background to xor

Sets the background write mode to xor.

******************************************************************************}

procedure bxor(var f: text);

begin

end;

{******************************************************************************

Set line width

Sets the width of lines and several other figures.

******************************************************************************}

procedure linewidth(var f: text; w: integer);

begin

end;

{******************************************************************************

Find character size x

Returns the character width.

******************************************************************************}

function chrsizx(var f: text): integer;

begin

end;

{******************************************************************************

Find character size y

Returns the character height.

******************************************************************************}

function chrsizy(var f: text): integer;

begin

end;

{******************************************************************************

Find number of installed fonts

Finds the total number of installed fonts.

******************************************************************************}

function fonts(var f: text): integer; 

begin

end;

{******************************************************************************

Change fonts

Changes the current font to the indicated logical font number.

******************************************************************************}

procedure font(var f: text; fc: integer);

begin

end;

{******************************************************************************

Find name of font

Returns the name of a font by number.

******************************************************************************}

procedure fontnam(var f: text; fc: integer; var fns: string);

begin

end;

{******************************************************************************

Change font size

Changes the font sizing to match the given character height. The character
and line spacing are changed, as well as the baseline.

******************************************************************************}

procedure fontsiz(var f: text; s: integer); 

begin

end;

{******************************************************************************

Set character extra spacing y

Sets the extra character space to be added between lines, also referred to
as "ledding".

Not implemented yet.

******************************************************************************}

procedure chrspcy(var f: text; s: integer); 

begin

end;

{******************************************************************************

Sets extra character space x

Sets the extra character space to be added between characters, referred to
as "spacing".

Not implemented yet.

******************************************************************************}

procedure chrspcx(var f: text; s: integer); 

begin

end;

{******************************************************************************

Find dots per meter x

Returns the number of dots per meter resolution in x.

******************************************************************************}

function dpmx(var f: text): integer; 

begin

end;

{******************************************************************************

Find dots per meter y

Returns the number of dots per meter resolution in y.

******************************************************************************}

function dpmy(var f: text): integer; 

begin

end;

{******************************************************************************

Find string size in pixels

Returns the number of pixels wide the given string would be, considering
character spacing and kerning.

******************************************************************************}

function strsiz(var f: text; view s: string): integer; 

begin

end;

function strsizp(var f: text; view s: string): integer; 

begin

end;

{******************************************************************************

Find character position in string

Finds the pixel offset to the given character in the string.

******************************************************************************}

function chrpos(var f: text; view s: string; p: integer): integer; 

begin

end;

{******************************************************************************

Write justified text

Writes a string of text with justification. The string and the width in pixels
is specified. Auto mode cannot be on for this function, nor can it be used on
the system font.

******************************************************************************}

procedure writejust(var f: text; view s: string; n: integer); 

begin

end;

{******************************************************************************

Find justified character position

Given a string, a character position in that string, and the total length
of the string in pixels, returns the offset in pixels from the start of the
string to the given character, with justification taken into account.
The model used is that the extra space needed is divided by the number of
spaces, with the fractional part lost.

******************************************************************************}

function justpos(var f: text; view s: string; p, n: integer): integer; 

begin

end;

{******************************************************************************

Turn on condensed attribute

Turns on/off the condensed attribute. Condensed is a character set with a
shorter baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

******************************************************************************}

procedure condensed(var f: text; e: boolean); 

begin

end;

{******************************************************************************

Turn on extended attribute

Turns on/off the extended attribute. Extended is a character set with a
longer baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

******************************************************************************}

procedure extended(var f: text; e: boolean); 

begin

   refer(f, e) { stub out }

end;

{******************************************************************************

Turn on extra light attribute

Turns on/off the extra light attribute. Extra light is a character thiner than
light.

Note that the attributes can only be set singly.

Not implemented yet.

******************************************************************************}

procedure xlight(var f: text; e: boolean); 

begin

end;

{******************************************************************************

Turn on light attribute

Turns on/off the light attribute. Light is a character thiner than normal
characters in the currnet font.

Note that the attributes can only be set singly.

Not implemented yet.

******************************************************************************}

procedure light(var f: text; e: boolean); 

begin

end;

{******************************************************************************

Turn on extra bold attribute

Turns on/off the extra bold attribute. Extra bold is a character thicker than
bold.

Note that the attributes can only be set singly.

Not implemented yet.

******************************************************************************}

procedure xbold(var f: text; e: boolean); 

begin

end;

{******************************************************************************

Turn on hollow attribute

Turns on/off the hollow attribute. Hollow is an embossed or 3d effect that
makes the characters appear sunken into the page.

Note that the attributes can only be set singly.

Not implemented yet.

******************************************************************************}

procedure hollow(var f: text; e: boolean); 

begin

end;

{******************************************************************************

Turn on raised attribute

Turns on/off the raised attribute. Raised is an embossed or 3d effect that
makes the characters appear coming off the page.

Note that the attributes can only be set singly.

Not implemented yet.

******************************************************************************}

procedure raised(var f: text; e: boolean); 

begin

end;

{******************************************************************************

Delete picture

Deletes a loaded picture.

******************************************************************************}

procedure delpict(var f: text; p: integer);

begin

end;

{******************************************************************************

Load picture

Loads a picture into a slot of the loadable pictures array.

******************************************************************************}

procedure loadpict(var f: text; p: integer; view fn: string);

begin

end;

{******************************************************************************

Find size x of picture

Returns the size in x of the logical picture.

******************************************************************************}

function pictsizx(var f: text; p: integer): integer;

begin

end;

{******************************************************************************

Find size y of picture

Returns the size in y of the logical picture.

******************************************************************************}

function pictsizy(var f: text; p: integer): integer;

begin

end;

{******************************************************************************

Draw picture

Draws a picture from the given file to the rectangle. The picture is resized
to the size of the rectangle.

Images will be kept in a rotating cache to prevent repeating reloads.

******************************************************************************}

procedure picture(var f: text; p: integer; x1, y1, x2, y2: integer);

begin

end;

{******************************************************************************

Set viewport offset graphical

Sets the offset of the viewport in logical space, in pixels, anywhere from
-maxint to maxint.

******************************************************************************}

procedure viewoffg(var f: text; x, y: integer); 

begin

end;

{******************************************************************************

Set viewport scale

Sets the viewport scale in x and y. The scale is a real fraction between 0 and
1, with 1 being 1:1 scaling. Viewport scales are allways smaller than logical
scales, which means that there are more than one logical pixel to map to a
given physical pixel, but never the reverse. This means that pixels are lost
in going to the display, but the display never needs to interpolate pixels
from logical pixels.

Note:

Right now, symmetrical scaling (both x and y scales set the same) are all that
works completely, since we don't presently have a method to warp text to fit
a scaling process. However, this can be done by various means, including
painting into a buffer and transfering asymmetrically, or using outlines.

******************************************************************************}

procedure viewscale(var f: text; x, y: real);

begin

end;

{******************************************************************************

Aquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

******************************************************************************}

procedure event(var f: text; var er: evtrec);

begin

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

procedure timer(var f: text;     { file to send event to }
                    i: timhan;   { timer handle }
                    t: integer;  { number of tenth-milliseconds to run }
                    r: boolean); { timer is to rerun after completion }

begin

end;

{******************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

******************************************************************************}

procedure killtimer(var f: text;    { file to kill timer on }
                        i: timhan); { handle of timer }

begin

end;

{******************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

******************************************************************************}

procedure frametimer(var f: text; e: boolean);

begin

end;

{******************************************************************************

Set automatic hold state

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from gralib.
This exists to allow the results of gralib unaware programs to be viewed after
termination, instead of exiting an distroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fufills the requirement of
holding gralib unaware programs.

******************************************************************************}

procedure autohold(e: boolean);

begin

end;

{******************************************************************************

Return number of mice

Returns the number of mice implemented. Windows supports only one mouse.

******************************************************************************}

function mouse(var f: text): mounum;

begin

end;

{******************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

******************************************************************************}

function mousebutton(var f: text; m: mouhan): moubut;

begin

end;

{******************************************************************************

Return number of joysticks

Return number of joysticks attached.

******************************************************************************}

function joystick(var f: text): joynum;

begin

end;

{******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

******************************************************************************}

function joybutton(var f: text; j: joyhan): joybtn;

begin

end;

{******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

******************************************************************************}

function joyaxis(var f: text; j: joyhan): joyaxn;

begin

end;

{******************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

******************************************************************************}

procedure settabg(var f: text; t: integer);

begin

end;

{******************************************************************************

Set tab

Sets a tab at the indicated collumn number.

******************************************************************************}

procedure settab(var f: text; t: integer);

begin

end;

{******************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

******************************************************************************}

procedure restabg(var f: text; t: integer);

begin

end;

{******************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

******************************************************************************}

procedure restab(var f: text; t: integer);

begin

end;

{******************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

******************************************************************************}

procedure clrtab(var f: text);

begin

end;

{******************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

******************************************************************************}

function funkey(var f: text): funky;

begin

end;

{******************************************************************************

Set window title

Sets the title of the current window.

******************************************************************************}

procedure title(var f: text; view ts: string);

begin

end;

{******************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.
The window id can be from 1 to ss_maxhdl, but the input and output file ids
of 1 and 2 are reserved for the input and output files, and cannot be used
directly. These ids will be be opened as a pair anytime the "_input" or
"_output" file names are seen.

******************************************************************************}

procedure openwin(var infile, outfile, parent: text; wid: ss_filhdl);

begin

end;

{******************************************************************************

Size buffer pixel

Sets or resets the size of the buffer surface, in pixel units.

******************************************************************************}

procedure sizbufg(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

******************************************************************************}

procedure sizbuf(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

******************************************************************************}

procedure buffer(var f: text; e: boolean);

begin

end;

{******************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

******************************************************************************}

procedure menu(var f: text; m: menuptr);

begin

end;

{******************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

******************************************************************************}

procedure menuena(var f: text; id: integer; onoff: boolean);

begin

end;

{******************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

******************************************************************************}

procedure menusel(var f: text; id: integer; select: boolean);

begin

end;

{******************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

******************************************************************************}

procedure front(var f: text);

begin

end;

{******************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

******************************************************************************}

procedure back(var f: text);

begin

end;

{******************************************************************************

Get window size graphical

Gets the onscreen window size.

******************************************************************************}

procedure getsizg(var f: text; var x, y: integer);

begin

end;

{******************************************************************************

Get window size character

Gets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are returned. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

******************************************************************************}

procedure getsiz(var f: text; var x, y: integer);

begin

end;

{******************************************************************************

Set window size graphical

Sets the onscreen window to the given size.

******************************************************************************}

procedure setsizg(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Set window size character

Sets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

******************************************************************************}

procedure setsiz(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Set window position graphical

Sets the onscreen window to the given position in its parent.

******************************************************************************}

procedure setposg(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Set window position character

Sets the onscreen window position, in character terms. If the window has a
parent, the demensions are converted to the current character size there.
Otherwise, pixel based demensions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

******************************************************************************}

procedure setpos(var f: text; x, y: integer);

begin

end;

{******************************************************************************

Get screen size graphical

Gets the total screen size.

******************************************************************************}

procedure scnsizg(var f: text; var x, y: integer);

begin

end;

{******************************************************************************

Find window size from client

Finds the window size, in parent terms, needed to result in a given client
window size.

Note: this routine should be able to find the minimum size of a window using
the given style, and return the minimums if the input size is lower than this.
This does not seem to be obvious under Windows.

Do we also need a menu style type ?

******************************************************************************}

procedure winclient(var f: text; cx, cy: integer; var wx, wy: integer;
                    view ms: winmodset);

begin

end;

procedure winclientg(var f: text; cx, cy: integer; var wx, wy: integer;
                     view ms: winmodset);

begin

end;

{******************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

******************************************************************************}

procedure scnsiz(var f: text; var x, y: integer);

begin

end;

{******************************************************************************

Enable or disable window frame

Turns the window frame on and off.

******************************************************************************}

procedure frame(var f: text; e: boolean);

begin

end;

{******************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

******************************************************************************}

procedure sizable(var f: text; e: boolean);

begin

end;

{******************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

******************************************************************************}

procedure sysbar(var f: text; e: boolean);

begin

end;

{******************************************************************************

Create standard menu

Creates a standard menu set. Given a set of standard items selected in a set,
and a program added menu list, creates a new standard menu.

On this windows version, the standard lists are:

file edit <program> window help

That is, all of the standard items are sorted into the lists at the start and
end of the menu, then the program selections placed in the menu.

******************************************************************************}

procedure stdmenu(view sms: stdmenusel; var sm: menuptr; pm: menuptr);

begin

end;

{******************************************************************************

Kill widget

Removes the widget by id from the window.

******************************************************************************}

procedure killwidget(var f: text; id: integer);

begin

end;

{******************************************************************************

Select/deselect widget

Selects or deselects a widget.

******************************************************************************}

procedure selectwidget(var f: text; id: integer; e: boolean);

begin

end;

{******************************************************************************

Enable/disable widget

Enables or disables a widget.

******************************************************************************}

procedure enablewidget(var f: text; id: integer; e: boolean);

begin

end;

{******************************************************************************

Get widget text

Retrives the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

******************************************************************************}

procedure getwidgettext(var f: text; id: integer; var s: pstring);

begin

end;

{******************************************************************************

put edit box text

Places text into an edit box.

******************************************************************************}

procedure putwidgettext(var f: text; id: integer; view s: string);

begin

end;

{******************************************************************************

Resize widget

Changes the size of a widget.

******************************************************************************}

procedure sizwidgetg(var f: text; id: integer; x, y: integer);

begin

end;

{******************************************************************************

Reposition widget

Changes the parent position of a widget.

******************************************************************************}

procedure poswidgetg(var f: text; id: integer; x, y: integer);

begin

end;

{******************************************************************************

Place widget to back of Z order

******************************************************************************}

procedure backwidget(var f: text; id: integer);

begin

end;

{******************************************************************************

Place widget to back of Z order

******************************************************************************}

procedure frontwidget(var f: text; id: integer);

begin

end;

{******************************************************************************

Find minimum/standard button size

Finds the minimum size for a button. Given the face string, the minimum size of
a button is calculated and returned.

******************************************************************************}

procedure buttonsizg(var f: text; view s: string; var w, h: integer);

begin

end;

procedure buttonsiz(var f: text; view s: string; var w, h: integer);

begin

end;

{******************************************************************************

Create button

Creates a standard button within the specified rectangle, on the given window.

******************************************************************************}

procedure buttong(var f: text; x1, y1, x2, y2: integer; view s: string;
                 id: integer);

begin

end;

procedure button(var f: text; x1, y1, x2, y2: integer; view s: string; 
                 id: integer);

begin

end;

{******************************************************************************

Find minimum/standard checkbox size

Finds the minimum size for a checkbox. Given the face string, the minimum size of
a checkbox is calculated and returned.

******************************************************************************}

procedure checkboxsizg(var f: text; view s: string; var w, h: integer);

begin

end;

procedure checkboxsiz(var f: text; view s: string; var w, h: integer);

begin

end;

{******************************************************************************

Create checkbox

Creates a standard checkbox within the specified rectangle, on the given
window.

******************************************************************************}

procedure checkboxg(var f: text; x1, y1, x2, y2: integer; view s: string;
                   id: integer);

begin

end;

procedure checkbox(var f: text; x1, y1, x2, y2: integer; view s: string;
                   id: integer);

begin

end;

{******************************************************************************

Find minimum/standard radio button size

Finds the minimum size for a radio button. Given the face string, the minimum
size of a radio button is calculated and returned.

******************************************************************************}

procedure radiobuttonsizg(var f: text; view s: string; var w, h: integer);

begin

end;

procedure radiobuttonsiz(var f: text; view s: string; var w, h: integer);

begin

end;

{******************************************************************************

Create radio button

Creates a standard radio button within the specified rectangle, on the given
window.

******************************************************************************}

procedure radiobuttong(var f: text; x1, y1, x2, y2: integer; view s: string;
                      id: integer);

begin

end;

procedure radiobutton(var f: text; x1, y1, x2, y2: integer; view s: string;
                   id: integer);

begin

end;

{******************************************************************************

Find minimum/standard group size

Finds the minimum size for a group. Given the face string, the minimum
size of a group is calculated and returned.

******************************************************************************}

procedure groupsizg(var f: text; view s: string; cw, ch: integer;
                    var w, h, ox, oy: integer);

begin

end;

procedure groupsiz(var f: text; view s: string; cw, ch: integer;
                   var w, h, ox, oy: integer);

begin

end;

{******************************************************************************

Create group box

Creates a group box, which is really just a decorative feature that gererates
no messages. It is used as a background for other widgets.

******************************************************************************}

procedure groupg(var f: text; x1, y1, x2, y2: integer; view s: string;
                 id: integer);

begin

end;

procedure group(var f: text; x1, y1, x2, y2: integer; view s: string;
                   id: integer);

begin

end;

{******************************************************************************

Create background box

Creates a background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

******************************************************************************}

procedure backgroundg(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

procedure background(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

{******************************************************************************

Find minimum/standard vertical scrollbar size

Finds the minimum size for a vertical scrollbar. The minimum size of a vertical
scrollbar is calculated and returned.

******************************************************************************}

procedure scrollvertsizg(var f: text; var w, h: integer);

begin

end;

procedure scrollvertsiz(var f: text; var w, h: integer);

begin

end;

{******************************************************************************

Create vertical scrollbar

Creates a vertical scrollbar.

******************************************************************************}

procedure scrollvertg(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

procedure scrollvert(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

{******************************************************************************

Find minimum/standard horizontal scrollbar size

Finds the minimum size for a horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated and returned.

******************************************************************************}

procedure scrollhorizsizg(var f: text; var w, h: integer);

begin

end;

procedure scrollhorizsiz(var f: text; var w, h: integer);

begin

end;

{******************************************************************************

Create horizontal scrollbar

Creates a horizontal scrollbar.

******************************************************************************}

procedure scrollhorizg(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

procedure scrollhoriz(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

{******************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

******************************************************************************}

procedure scrollpos(var f: text; id: integer; r: integer);

begin

end;

{******************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

******************************************************************************}

procedure scrollsiz(var f: text; id: integer; r: integer);

begin

end;

{******************************************************************************

Find minimum/standard number select box size

Finds the minimum size for a number select box. The minimum size of a number
select box is calculated and returned.

******************************************************************************}

procedure numselboxsizg(var f: text; l, u: integer; var w, h: integer);

begin

end;

procedure numselboxsiz(var f: text; l, u: integer; var w, h: integer);

begin

end;

{******************************************************************************

Create number selector

Creates an up/down control for numeric selection.

******************************************************************************}

procedure numselboxg(var f: text; x1, y1, x2, y2: integer; l, u: integer;
                     id: integer);

begin

end;

procedure numselbox(var f: text; x1, y1, x2, y2: integer; l, u: integer;
                     id: integer);

begin

end;

{******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

******************************************************************************}

procedure editboxsizg(var f: text; view s: string; var w, h: integer);

begin

end;

procedure editboxsiz(var f: text; view s: string; var w, h: integer);

begin

end;

{******************************************************************************

Create edit box

Creates single line edit box

******************************************************************************}

procedure editboxg(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

procedure editbox(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

{******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

******************************************************************************}

procedure progbarsizg(var f: text; var w, h: integer);

begin

end;

procedure progbarsiz(var f: text; var w, h: integer);

begin

end;

{******************************************************************************

Create progress bar

Creates a progress bar.

******************************************************************************}

procedure progbarg(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

procedure progbar(var f: text; x1, y1, x2, y2: integer; id: integer);

begin

end;

{******************************************************************************

Set progress bar position

Sets the position of a progress bar, from 0 to maxint.

******************************************************************************}

procedure progbarpos(var f: text; id: integer; pos: integer);

begin

end;

{******************************************************************************

Find minimum/standard list box size

Finds the minimum size for an list box. Given a string list, the minimum size
of an list box is calculated and returned.

Windows listboxes pretty much ignore the size given. If you allocate more space
than needed, it will only put blank lines below if enough space for an entire
line is present. If the size does not contain exactly enough to display the
whole line list, the box will colapse to a single line with an up/down
control. The only thing that is garanteed is that the box will fit within the
specified rectangle, one way or another.

******************************************************************************}

procedure listboxsizg(var f: text; sp: strptr; var w, h: integer);

begin

end;

procedure listboxsiz(var f: text; sp: strptr; var w, h: integer);

begin

end;

{******************************************************************************

Create list box

Creates a list box. Fills it with the string list provided.

******************************************************************************}

procedure listboxg(var f: text; x1, y1, x2, y2: integer; sp: strptr;
                   id: integer);

begin

end;

procedure listbox(var f: text; x1, y1, x2, y2: integer; sp: strptr;
                   id: integer);

begin

end;

{******************************************************************************

Find minimum/standard dropbox size

Finds the minimum size for a dropbox. Given the face string, the minimum size of
a dropbox is calculated and returned, for both the "open" and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

******************************************************************************}

procedure dropboxsizg(var f: text; sp: strptr; var cw, ch, ow, oh: integer);

begin

end;

procedure dropboxsiz(var f: text; sp: strptr; var cw, ch, ow, oh: integer);

begin

end;

{******************************************************************************

Create dropdown box

Creates a dropdown box. Fills it with the string list provided.

******************************************************************************}

procedure dropboxg(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                   id: integer);

begin

end;

procedure dropbox(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                   id: integer);

begin

end;

{******************************************************************************

Find minimum/standard drop edit box size

Finds the minimum size for a drop edit box. Given the face string, the minimum
size of a drop edit box is calculated and returned, for both the "open" and 
"closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

******************************************************************************}

procedure dropeditboxsizg(var f: text; sp: strptr; var cw, ch, ow, oh: integer);

begin

end;

procedure dropeditboxsiz(var f: text; sp: strptr; var cw, ch, ow, oh: integer);

begin

end;

{******************************************************************************

Create dropdown edit box

Creates a dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

******************************************************************************}

procedure dropeditboxg(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                       id: integer);

begin

end;

procedure dropeditbox(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                       id: integer);

begin

end;

overload procedure dropeditbox(x1, y1, x2, y2: integer; sp: strptr; 
                                id: integer);

begin

end;

{******************************************************************************

Find minimum/standard horizontal slider size

Finds the minimum size for a horizontal slider. The minimum size of a horizontal
slider is calculated and returned.

******************************************************************************}

procedure slidehorizsizg(var f: text; var w, h: integer);

begin

end;

procedure slidehorizsiz(var f: text; var w, h: integer);

begin

end;

{******************************************************************************

Create horizontal slider

Creates a horizontal slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

******************************************************************************}

procedure slidehorizg(var f: text; x1, y1, x2, y2: integer; mark: integer;
                      id: integer);

begin

end;

procedure slidehoriz(var f: text; x1, y1, x2, y2: integer; mark: integer;
                      id: integer);

begin

end;

{******************************************************************************

Find minimum/standard vertical slider size

Finds the minimum size for a vertical slider. The minimum size of a vertical
slider is calculated and returned.

******************************************************************************}

procedure slidevertsizg(var f: text; var w, h: integer);

begin

end;

procedure slidevertsiz(var f: text; var w, h: integer);

begin

end;

{******************************************************************************

Create vertical slider

Creates a vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

******************************************************************************}

procedure slidevertg(var f: text; x1, y1, x2, y2: integer; mark: integer;
                     id: integer);

begin

end;

procedure slidevert(var f: text; x1, y1, x2, y2: integer; mark: integer;
                     id: integer);

begin

end;

{******************************************************************************

Find minimum/standard tab bar size

Finds the minimum size for a tab bar. The minimum size of a tab bar is
calculated and returned.

******************************************************************************}

procedure tabbarsizg(var f: text; tor: tabori; cw, ch: integer;
                     var w, h, ox, oy: integer);

begin

end;

procedure tabbarsiz(var f: text; tor: tabori; cw, ch: integer; 
                    var w, h, ox, oy: integer);

begin

end;

{******************************************************************************

Find client from tabbar size

Given a tabbar size and orientation, this routine gives the client size and 
offset. This is used where the tabbar size is fixed, but the client area is
flexible.

******************************************************************************}

procedure tabbarclientg(var f: text; tor: tabori; w, h: integer; 
                     var cw, ch, ox, oy: integer);

begin

end;

procedure tabbarclient(var f: text; tor: tabori; w, h: integer; 
                    var cw, ch, ox, oy: integer);

begin

end;

{******************************************************************************

Create tab bar

Creates a tab bar with the given orientation.

Bug: has strange overwrite mode where when the widget is first created, it
allows itself to be overwritten by the main window. This is worked around by
creating and distroying another widget.

******************************************************************************}

procedure tabbarg(var f: text; x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                  id: integer);

begin

end;

procedure tabbar(var f: text; x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                  id: integer);

begin

end;

{******************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

******************************************************************************}

procedure tabsel(var f: text; id: integer; tn: integer);

begin

end;

{******************************************************************************

Output message dialog

Outputs a message dialog with the given title and message strings.

******************************************************************************}

procedure alert(view title, message: string);

begin

end;

{******************************************************************************

Display choose color dialog

Presents the choose color dialog, then returns the resulting color.

Bug: does not take the input color as the default.

******************************************************************************}

procedure querycolor(var r, g, b: integer);

begin

end;

{******************************************************************************

Display choose file dialog for open

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

******************************************************************************}

procedure queryopen(var s: pstring);

begin

end;

{******************************************************************************

Display choose file dialog for save

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

******************************************************************************}

procedure querysave(var s: pstring);

begin

end;

{******************************************************************************

Display choose find text dialog

Presents the choose find text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: should return null string on cancel. Unlike other dialogs, windows
provides no indication of if the cancel button was pushed. To do this, we
would need to hook (or subclass) the find dialog.

After note: tried hooking the window. The issue is that the cancel button is
just a simple button that gets pressed. Trying to rely on the button id
sounds very system dependent, since that could change. One method might be
to retrive the button text, but this is still fairly system dependent. We
table this issue until later.

******************************************************************************}

procedure queryfind(var s: pstring; var opt: qfnopts);

begin

end;

{******************************************************************************

Display choose replace text dialog

Presents the choose replace text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: See comment, queryfind.

******************************************************************************}

procedure queryfindrep(var s, r: pstring; var opt: qfropts);

begin

end;

{******************************************************************************

Display choose font dialog

Presents the choose font dialog, then returns the resulting logical font
number, size, foreground color, background color, and effects (in a special
effects set for this routine).

The parameters are "flow through", meaning that they should be set to their
defaults before the call, and changes are made, then updated to the parameters.
During the routine, the state of the parameters given are presented to the
user as the defaults.

******************************************************************************}

procedure queryfont(var f: text; var fc, s, fr, fg, fb, br, bg, bb: integer;
                    var effect: qfteffects);

begin

end;

{******************************************************************************

Gralib startup

******************************************************************************}

begin

end;

{******************************************************************************

Gralib shutdown

******************************************************************************}

begin

end.
