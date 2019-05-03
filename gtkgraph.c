{******************************************************************************
*                                                                             *
*                        GRAPHICAL MODE LIBRARY FOR GTK                       *
*                                                                             *
*                       Copyright (C) 2019 Scott A. Franco                    *
*                                                                             *
*                            2019/05/03 S. A. Franco                          *
*                                                                             *
* Implements the graphical mode functions on GTK. Gralib is upward        *
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

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

******************************************************************************}


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
   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   ifcolorg(win, r, g, b); { execute }
   unlockmain { end exclusive access }

end;

overload procedure fcolor(var f: text; r, g, b: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   ifcolorg(win, r, g, b); { execute }
   unlockmain { end exclusive access }

end;

overload procedure fcolor(r, g, b: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   ifcolorg(win, r, g, b); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set background color

Sets the background color from the universal primary code.

******************************************************************************}

procedure ibcolor(win: winptr; c: color);

var r: integer;

begin

   with win^, screens[curupd]^ do begin { in window, screen contexts }

      bcrgb := colnum(c); { set color status }
      gbcrgb := bcrgb;
      { activate in buffer }
      if sarev in attr then begin

         r := sc_settextcolor(bdc, bcrgb);
         if r = -1 then winerr { process windows error }

      end else begin

         r := sc_setbkcolor(bdc, bcrgb);
         if r = -1 then winerr { process windows error }

      end;
      if indisp(win) then begin { activate on screen }

         { set screen color according to reverse }
         if sarev in attr then begin

            r := sc_settextcolor(devcon, bcrgb);
            if r = -1 then winerr { process windows error }

         end else begin

            r := sc_setbkcolor(devcon, bcrgb);
            if r = -1 then winerr { process windows error }

         end

      end

   end

end;

procedure bcolor(var f: text; c: color);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   ibcolor(win, c); { execute }
   unlockmain { end exclusive access }

end;

overload procedure bcolor(c: color);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   ibcolor(win, c); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set background color graphical

Sets the background color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

******************************************************************************}

procedure ibcolorg(win: winptr; r, g, b: integer); 

var rv: integer;

begin

   with win^, screens[curupd]^ do begin { in window, screen contexts }

      bcrgb := rgb2win(r, g, b); { set color status }
      gbcrgb := bcrgb;
      { activate in buffer }
      if sarev in attr then begin

         r := sc_settextcolor(bdc, bcrgb);
         if r = -1 then winerr { process windows error }

      end else begin

         rv := sc_setbkcolor(bdc, bcrgb);
         if rv = -1 then winerr { process windows error }

      end;
      if indisp(win) then begin { activate on screen }

         { set screen color according to reverse }
         if sarev in attr then begin

            rv := sc_settextcolor(devcon, bcrgb);
            if rv = -1 then winerr { process windows error }

         end else begin

            rv := sc_setbkcolor(devcon, bcrgb);
            if rv = -1 then winerr { process windows error }

         end

      end

   end

end;

procedure bcolorg(var f: text; r, g, b: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   ibcolorg(win, r, g, b); { execute }
   unlockmain { end exclusive access }

end;

overload procedure bcolorg(r, g, b: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   ibcolorg(win, r, g, b); { execute }
   unlockmain { end exclusive access }

end;

overload procedure bcolor(var f: text; r, g, b: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   ibcolorg(win, r, g, b); { execute }
   unlockmain { end exclusive access }

end;

overload procedure bcolor(r, g, b: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   ibcolorg(win, r, g, b); { execute }
   unlockmain { end exclusive access }

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

procedure iauto(win: winptr; e: boolean);

begin

   with win^, screens[curupd]^ do begin { in window, screen contexts }

      { check we are transitioning to auto mode }
      if e then begin

         { check display is on grid and in bounds }
         if (curxg-1) mod charspace <> 0 then error(eatoofg);
         if (curxg-1) mod charspace <> 0 then error(eatoofg);
         if not icurbnd(screens[curupd]) then error(eatoecb)

      end;
      screens[curupd]^.auto := e; { set auto status }
      gauto := e

   end

end;

procedure auto(var f: text; e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   iauto(win, e); { execute }
   unlockmain { end exclusive access }

end;

overload procedure auto(e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   iauto(win, e); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

******************************************************************************}

procedure icurvis(win: winptr; e: boolean);

begin

   with win^ do begin { in window context }

      screens[curupd]^.curv := e; { set cursor visible status }
      gcurv := e;
      cursts(win) { process any cursor status change }

   end

end;

procedure curvis(var f: text; e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   icurvis(win, e); { execute }
   unlockmain { end exclusive access }

end;

overload procedure curvis(e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   icurvis(win, e); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

******************************************************************************}

function curx(var f: text): integer;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   with win^ do { in window context }
      curx := screens[curupd]^.curx; { return current location x }
   unlockmain { end exclusive access }

end;

overload function curx: integer;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   with win^ do { in window context }
      curx := screens[curupd]^.curx; { return current location x }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

******************************************************************************}

function cury(var f: text): integer;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   with win^ do { in window context }
      cury := screens[curupd]^.cury; { return current location y }
   unlockmain { end exclusive access }

end;

overload function cury: integer;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   with win^ do { in window context }
      cury := screens[curupd]^.cury; { return current location y }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Get location of cursor in x graphical

Returns the current location of the cursor in x, in pixels.

******************************************************************************}

function curxg(var f: text): integer;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   with win^ do { in window context }
      curxg := screens[curupd]^.curxg; { return current location x }
   unlockmain { end exclusive access }

end;

overload function curxg: integer;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   with win^ do { in window context }
      curxg := screens[curupd]^.curxg; { return current location x }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Get location of cursor in y graphical

Returns the current location of the cursor in y, in pixels.

******************************************************************************}

function curyg(var f: text): integer;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   with win^ do { in window context }
      curyg := screens[curupd]^.curyg; { return current location y }
   unlockmain { end exclusive access }

end;

overload function curyg: integer;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   with win^ do { in window context }
      curyg := screens[curupd]^.curyg; { return current location y }
   unlockmain { end exclusive access }

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

procedure iselect(win: winptr; u, d: integer);

var ld: 1..maxcon; { last display screen number save }

begin

   with win^ do begin { in window context }

      if not bufmod then error(ebufoff); { error }
      if (u < 1) or (u > maxcon) or (d < 1) or (d > maxcon) then
         error(einvscn); { invalid screen number }
      ld := curdsp; { save the current display screen number }
      curupd := u; { set the current update screen }
      if screens[curupd] = nil then begin { no screen, create one }

         new(screens[curupd]); { get a new screen context }
         iniscn(win, screens[curupd]); { initalize that }

      end;
      curdsp := d; { set the current display screen }
      if screens[curdsp] = nil then begin { no screen, create one }

         { no current screen, create a new one }
         new(screens[curdsp]); { get a new screen context }
         iniscn(win, screens[curdsp]); { initalize that }
      
      end;
      { if the screen has changed, restore it }
      if curdsp <> ld then restore(win, true)

   end

end;

procedure select(var f: text; u, d: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   iselect(win, u, d); { execute }
   unlockmain { end exclusive access }

end;

overload procedure select(u, d: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   iselect(win, u, d); { execute }
   unlockmain { end exclusive access }

end;

overload procedure select(var f: text; d: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   iselect(win, d, d); { execute }
   unlockmain { end exclusive access }

end;

overload procedure select(d: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   iselect(win, d, d); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attributes.

Note: cannot place text with foreground for background xor modes, since there
is no direct windows feature for that. The only way to add this is to write
characters into a buffer DC, then blt them back to the main buffer and display
using ROP combinations.

******************************************************************************}

procedure plcchr(win: winptr; c: char);

var b:   boolean; { function return }
    cb:  packed array [1..1] of char; { character output buffer }
    off: integer;   { subscript offset }
    sz:  sc_size;   { size holder }

begin

   with win^ do begin { in window context }

      if not visible then winvis(win); { make sure we are displayed }
      { handle special character cases first }
      if c = '\cr' then with screens[curupd]^ do begin

         { carriage return, position to extreme left }
         curx := 1; { set to extreme left }
         curxg := 1;
         if indisp(win) then setcur(win) { set cursor on screen }

      end else if c = '\lf' then idown(win) { line feed, move down }
      else if c = '\bs' then ileft(win) { back space, move left }
      else if c = '\ff' then iclear(win) { clear screen }
      else if c = '\ht' then itab(win) { process tab }
      else if (c >= ' ') and (c <> chr($7f)) then { character is visible }
         with screens[curupd]^ do begin

         off := 0; { set no subscript offset }
         if sasubs in attr then off := trunc(linespace*0.35);
         { update buffer }
         cb[1] := c; { place character }
         if bufmod then begin { buffer is active }

            { draw character }
            b := sc_textout(bdc, curxg-1, curyg-1+off, cb);
            if not b then winerr { process windows error }

         end;
         if indisp(win) then begin { activate on screen }

            { draw character on screen }
            curoff(win); { hide the cursor }
            { draw character }
            b := sc_textout(devcon, curxg-1, curyg-1+off, cb);
            if not b then winerr; { process windows error }
            curon(win) { show the cursor }

         end;
         if cfont^.sys then iright(win) { move cursor right character }
         else begin { perform proportional version }

            b := sc_gettextextentpoint32(bdc, cb, sz); { get spacing }
            if not b then winerr; { process windows error }
            curxg := curxg+sz.cx; { advance the character width }
            curx := curxg div charspace+1; { recalculate character position }
            if indisp(win) then setcur(win) { set cursor on screen }

         end

      end

   end
      
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

procedure iwrtstr(win: winptr; view s: string);

var b:   boolean; { function return }
    off: integer; { subscript offset }
    sz:  sc_size; { size holder }

begin

   with win^, screens[curupd]^ do begin { in window, screen context }

      if auto then error(estrato); { autowrap is on }
      if not visible then winvis(win); { make sure we are displayed }
      off := 0; { set no subscript offset }
      if sasubs in attr then off := trunc(linespace*0.35);
      { update buffer }
      if bufmod then begin { buffer is active }

         { draw character }
         b := sc_textout(bdc, curxg-1, curyg-1+off, s);
         if not b then winerr { process windows error }

      end;
      if indisp(win) then begin { activate on screen }

         { draw character on screen }
         curoff(win); { hide the cursor }
         { draw character }
         b := sc_textout(devcon, curxg-1, curyg-1+off, s);
         if not b then winerr; { process windows error }
         curon(win) { show the cursor }

      end;
      if cfont^.sys then begin { perform fixed system advance }

            { should check if this exceeds maxint }
            curx := curx+max(s); { update position }
            curxg := curxg+charspace*max(s)

      end else begin { perform proportional version }

         b := sc_gettextextentpoint32(bdc, s, sz); { get spacing }
         if not b then winerr; { process windows error }
         curxg := curxg+sz.cx; { advance the character width }
         curx := curxg div charspace+1; { recalculate character position }
         if indisp(win) then setcur(win) { set cursor on screen }

      end

   end

end;

procedure wrtstr(var f: text; view s: string);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   iwrtstr(win, s); { perform string write }
   unlockmain { end exclusive access }

end;

overload procedure wrtstr(view s: string);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iwrtstr(win, s); { perform string write }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

******************************************************************************}

procedure idel(win: winptr);

begin

   with win^ do begin { in window context }

      ileft(win); { back up cursor }
      plcchr(win, ' '); { blank out }
      ileft(win) { back up again }

   end

end;

procedure del(var f: text);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   idel(win); { perform delete }
   unlockmain { end exclusive access }

end;

overload procedure del;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idel(win); { perform delete }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw line

Draws a single line in the foreground color.

******************************************************************************}

procedure iline(win: winptr; x1, y1, x2, y2: integer);

var b:      boolean; { results }
    tx, ty: integer; { temp holder }
    dx, dy: integer;

begin

   with win^, screens[curupd]^ do begin { in window, screen contexts }

      lcurx := x2; { place next progressive endpoint }
      lcury := y2;
      { rationalize the line to right/down }
      if (x1 > x2) or ((x1 = x2) and (y1 > y2)) then begin { swap }

         tx := x1;
         ty := y1;
         x1 := x2;
         y1 := y2;
         x2 := tx;
         y2 := ty

      end;
      { Try to compensate for windows not drawing line endings. }
      if y1 = y2 then dy := 0
      else if y1 < y2 then dy := +1
      else dy := -1;
      if x1 = x2 then dx := 0
      else dx := +1;
      if bufmod then begin { buffer is active }

         { set current position of origin }
         b := sc_movetoex_n(bdc, x1-1, y1-1);
         if not b then winerr; { process windows error }
         b := sc_lineto(bdc, x2-1+dx, y2-1+dy);
         if not b then winerr; { process windows error }

      end;
      if indisp(win) then begin { do it again for the current screen }

         if not visible then winvis(win); { make sure we are displayed }
         curoff(win);
         b := sc_movetoex_n(devcon, x1-1, y1-1);
         if not b then winerr; { process windows error }
         b := sc_lineto(devcon, x2-1+dx, y2-1+dy);
         if not b then winerr; { process windows error }
         curon(win)

      end

   end

end;

procedure line(var f: text; x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   iline(win, x1, y1, x2, y2); { draw line }
   unlockmain { end exclusive access }

end;

overload procedure line(x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iline(win, x1, y1, x2, y2); { draw line }
   unlockmain { end exclusive access }

end;

overload procedure line(var f: text; x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^, screens[curupd]^ do { in window, screen contexts }
      iline(win, lcurx, lcury, x2, y2); { draw line }
   unlockmain { end exclusive access }

end;

overload procedure line(x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^, screens[curupd]^ do { in window, screen contexts }
      iline(win, lcurx, lcury, x2, y2); { draw line }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw rectangle

Draws a rectangle in foreground color.

******************************************************************************}

procedure irect(win: winptr; x1, y1, x2, y2: integer);

var b:   boolean; { return value }

begin

   with win^ do begin { in window context }

      if bufmod then begin { buffer is active }

         { draw to buffer }
         b := sc_rectangle(screens[curupd]^.bdc, x1-1, y1-1, x2, y2);
         if not b then winerr { process windows error }

      end;
      if indisp(win) then begin

         { draw to screen }
         if not visible then winvis(win); { make sure we are displayed }
         curoff(win);
         b := sc_rectangle(devcon, x1-1, y1-1, x2, y2);
         if not b then winerr; { process windows error }
         curon(win)

      end

   end

end;

procedure rect(var f: text; x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   irect(win, x1, y1, x2, y2); { draw rectangle }
   unlockmain { end exclusive access }

end;

overload procedure rect(x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   irect(win, x1, y1, x2, y2); { draw line }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw filled rectangle

Draws a filled rectangle in foreground color.

******************************************************************************}

procedure ifrect(win: winptr; x1, y1, x2, y2: integer);

var b:   boolean; { return value }
    r:   integer; { result holder }

begin

   with win^, screens[curupd]^ do begin { in window, screen contexts }

      if bufmod then begin { buffer is active }

         { for filled ellipse, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these }
         r := sc_selectobject(bdc, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, fbrush);
         if r = -1 then winerr; { process windows error }
         { draw to buffer }
         b := sc_rectangle(bdc, x1-1, y1-1, x2, y2);
         if not b then winerr; { process windows error }
         { restore }
         r := sc_selectobject(bdc, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr { process windows error }

      end;
      { draw to screen }
      if indisp(win) then begin

         if not visible then winvis(win); { make sure we are displayed }
         r := sc_selectobject(devcon, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, fbrush);
         if r = -1 then winerr; { process windows error }
         curoff(win);
         b := sc_rectangle(devcon, x1-1, y1-1, x2, y2);
         if not b then winerr; { process windows error }
         curon(win);
         r := sc_selectobject(devcon, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr { process windows error }

      end

   end

end;

procedure frect(var f: text; x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifrect(win, x1, y1, x2, y2); { draw rectangle }
   unlockmain { end exclusive access }

end;

overload procedure frect(x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifrect(win, x1, y1, x2, y2); { draw line }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw rounded rectangle

Draws a rounded rectangle in foreground color.

******************************************************************************}

procedure irrect(win: winptr; x1, y1, x2, y2, xs, ys: integer);

var b: boolean; { return value }

begin

   with win^ do begin { in window context }

      if bufmod then begin { buffer is active }

         { draw to buffer }
         b := sc_roundrect(screens[curupd]^.bdc, x1-1, y1-1, x2, y2, xs, ys);
         if not b then winerr { process windows error }

      end;
      { draw to screen }
      if indisp(win) then begin

         if not visible then winvis(win); { make sure we are displayed }
         curoff(win);
         b := sc_roundrect(devcon, x1-1, y1-1, x2, y2, xs, ys);
         if not b then winerr; { process windows error }
         curon(win)

      end

   end

end;

procedure rrect(var f: text; x1, y1, x2, y2, xs, ys: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   irrect(win, x1, y1, x2, y2, xs, ys); { draw rectangle }
   unlockmain { end exclusive access }

end;

overload procedure rrect(x1, y1, x2, y2, xs, ys: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   irrect(win, x1, y1, x2, y2, xs, ys); { draw line }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw filled rounded rectangle

Draws a filled rounded rectangle in foreground color.

******************************************************************************}

procedure ifrrect(win: winptr; x1, y1, x2, y2, xs, ys: integer);

var b: boolean; { return value }
    r: integer; { result holder }

begin

   with win^, screens[curupd]^ do begin { in windows, screen contexts }

      if bufmod then begin { buffer is active }

         { for filled ellipse, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these }
         r := sc_selectobject(bdc, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, fbrush);
         if r = -1 then winerr; { process windows error }
         { draw to buffer }
         b := sc_roundrect(bdc, x1-1, y1-1, x2, y2, xs, ys);
         if not b then winerr; { process windows error }
         { restore }
         r := sc_selectobject(bdc, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr { process windows error }

      end;
      { draw to screen }
      if indisp(win) then begin

         if not visible then winvis(win); { make sure we are displayed }
         r := sc_selectobject(devcon, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, fbrush);
         if r = -1 then winerr; { process windows error }
         curoff(win);
         b := sc_roundrect(devcon, x1-1, y1-1, x2, y2, xs, ys);
         if not b then winerr; { process windows error }
         curon(win);
         r := sc_selectobject(devcon, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr; { process windows error }

      end

   end

end;

procedure frrect(var f: text; x1, y1, x2, y2, xs, ys: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifrrect(win, x1, y1, x2, y2, xs, ys); { draw rectangle }
   unlockmain { end exclusive access }

end;

overload procedure frrect(x1, y1, x2, y2, xs, ys: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifrrect(win, x1, y1, x2, y2, xs, ys); { draw line }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw ellipse

Draws an ellipse with the current foreground color and line width.

******************************************************************************}

procedure iellipse(win: winptr; x1, y1, x2, y2: integer);

var b: boolean; { return value }

begin

   with win^ do begin { in windows context }

      if bufmod then begin { buffer is active }

         { draw to buffer }
         b := sc_ellipse(screens[curupd]^.bdc, x1-1, y1-1, x2, y2);
         if not b then winerr { process windows error }

      end;
      { draw to screen }
      if indisp(win) then begin

         if not visible then winvis(win); { make sure we are displayed }
         curoff(win);
         b := sc_ellipse(devcon, x1-1, y1-1, x2, y2);
         if not b then winerr; { process windows error }
         curon(win)

      end

   end

end;

procedure ellipse(var f: text; x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   iellipse(win, x1, y1, x2, y2); { draw ellipse }
   unlockmain { end exclusive access }

end;

overload procedure ellipse(x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iellipse(win, x1, y1, x2, y2); { draw ellipse }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw filled ellipse

Draws a filled ellipse with the current foreground color.

******************************************************************************}

procedure ifellipse(win: winptr; x1, y1, x2, y2: integer);

var b: boolean; { return value }

begin

   with win^, screens[curupd]^ do begin { in windows, screen contexts }

      if bufmod then begin { buffer is active }

         { for filled ellipse, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these }
         r := sc_selectobject(bdc, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, fbrush);
         if r = -1 then winerr; { process windows error }
         { draw to buffer }
         b := sc_ellipse(bdc, x1-1, y1-1, x2, y2);
         if not b then winerr; { process windows error }
         { restore }
         r := sc_selectobject(bdc, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr { process windows error }

      end;
      { draw to screen }
      if indisp(win) then begin

         if not visible then winvis(win); { make sure we are displayed }
         r := sc_selectobject(devcon, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, fbrush);
         if r = -1 then winerr; { process windows error }
         curoff(win);
         b := sc_ellipse(devcon, x1-1, y1-1, x2, y2);
         if not b then winerr; { process windows error }
         curon(win);
         r := sc_selectobject(devcon, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr; { process windows error }

      end

   end

end;

procedure fellipse(var f: text; x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifellipse(win, x1, y1, x2, y2); { draw ellipse }
   unlockmain { end exclusive access }

end;

overload procedure fellipse(x1, y1, x2, y2: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifellipse(win, x1, y1, x2, y2); { draw ellipse }
   unlockmain { end exclusive access }

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

procedure iarc(win: winptr; x1, y1, x2, y2, sa, ea: integer);

const precis = 1000; { precision of circle calculation }

var saf, eaf:       real;    { starting angles in radian float }
    xs, ys, xe, ye: integer; { start and end coordinates }
    xc, yc:         integer; { center point }
    t:              integer; { swapper }
    b:              boolean; { return value }


begin

   with win^ do begin { in window context }

      { rationalize rectangle for processing }
      if x1 > x2 then begin t := x1; x1 := x2; x2 := t end;
      if y1 > y2 then begin t := y1; y1 := y2; y2 := t end;
      { convert start and end to radian measure }
      saf := sa*2.0*pi/maxint;
      eaf := ea*2.0*pi/maxint;
      { find center of ellipse }
      xc := (x2-x1) div 2+x1;
      yc := (y2-y1) div 2+y1;
      { resolve start to x, y }
      xs := round(xc+precis*cos(pi/2-saf));
      ys := round(yc-precis*sin(pi/2-saf));
      { resolve end to x, y }
      xe := round(xc+precis*cos(pi/2-eaf));
      ye := round(yc-precis*sin(pi/2-eaf));
      if bufmod then begin { buffer is active }

         b := sc_arc(screens[curupd]^.bdc, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
         if not b then winerr; { process windows error }

      end;
      if indisp(win) then begin { do it again for the current screen }

         if not visible then winvis(win); { make sure we are displayed }
         curoff(win);
         b := sc_arc(devcon, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
         if not b then winerr; { process windows error }
         curon(win);

      end

   end

end;

procedure arc(var f: text; x1, y1, x2, y2, sa, ea: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   iarc(win, x1, y1, x2, y2, sa, ea); { draw arc }
   unlockmain { end exclusive access }

end;

overload procedure arc(x1, y1, x2, y2, sa, ea: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iarc(win, x1, y1, x2, y2, sa, ea); { draw arc }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw filled arc

Draws a filled arc in the current foreground color. The same comments apply
as for the arc function above.

******************************************************************************}

procedure ifarc(win: winptr; x1, y1, x2, y2, sa, ea: integer);

const precis = 1000; { precision of circle calculation }

var saf, eaf:       real;    { starting angles in radian float }
    xs, ys, xe, ye: integer; { start and end coordinates }
    xc, yc:         integer; { center point }
    t:              integer; { swapper }
    b:              boolean; { return value }
    r:              integer; { result holder }

begin

   with win^, screens[curupd]^ do begin { in window, screen context }

      { rationalize rectangle for processing }
      if x1 > x2 then begin t := x1; x1 := x2; x2 := t end;
      if y1 > y2 then begin t := y1; y1 := y2; y2 := t end;
      { convert start and end to radian measure }
      saf := sa*2*pi/maxint;
      eaf := ea*2*pi/maxint;
      { find center of ellipse }
      xc := (x2-x1) div 2+x1;
      yc := (y2-y1) div 2+y1;
      { resolve start to x, y }
      xs := round(xc+precis*cos(pi/2-saf));
      ys := round(yc-precis*sin(pi/2-saf));
      { resolve end to x, y }
      xe := round(xc+precis*cos(pi/2-eaf));
      ye := round(yc-precis*sin(pi/2-eaf));
      if bufmod then begin { buffer is active }

         { for filled shape, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these }
         r := sc_selectobject(bdc, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, fbrush);
         if r = -1 then winerr; { process windows error }
         { draw shape }
         b := sc_pie(bdc, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
         if not b then winerr; { process windows error }
         { restore }
         r := sc_selectobject(bdc, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr { process windows error }

      end;
      if indisp(win) then begin { do it again for the current screen }

         if not visible then winvis(win); { make sure we are displayed }
         r := sc_selectobject(devcon, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, fbrush);
         if r = -1 then winerr; { process windows error }
         curoff(win);
         { draw shape }
         b := sc_pie(devcon, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
         if not b then winerr; { process windows error }
         curon(win);
         r := sc_selectobject(devcon, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr; { process windows error }

      end

   end

end;

procedure farc(var f: text; x1, y1, x2, y2, sa, ea: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifarc(win, x1, y1, x2, y2, sa, ea); { draw arc }
   unlockmain { end exclusive access }

end;

overload procedure farc(x1, y1, x2, y2, sa, ea: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifarc(win, x1, y1, x2, y2, sa, ea); { draw arc }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw filled cord

Draws a filled cord in the current foreground color. The same comments apply
as for the arc function above.

******************************************************************************}

procedure ifchord(win: winptr; x1, y1, x2, y2, sa, ea: integer);

const precis = 1000; { precision of circle calculation }

var saf, eaf:       real;    { starting angles in radian float }
    xs, ys, xe, ye: integer; { start and end coordinates }
    xc, yc:         integer; { center point }
    t:              integer; { swapper }
    b:              boolean; { return value }
    r:              integer; { result holder }

begin

   with win^, screens[curupd]^ do begin { in window, screen context }

      { rationalize rectangle for processing }
      if x1 > x2 then begin t := x1; x1 := x2; x2 := t end;
      if y1 > y2 then begin t := y1; y1 := y2; y2 := t end;
      { convert start and end to radian measure }
      saf := sa*2*pi/maxint;
      eaf := ea*2*pi/maxint;
      { find center of ellipse }
      xc := (x2-x1) div 2+x1;
      yc := (y2-y1) div 2+y1;
      { resolve start to x, y }
      xs := round(xc+precis*cos(pi/2-saf));
      ys := round(yc-precis*sin(pi/2-saf));
      { resolve end to x, y }
      xe := round(xc+precis*cos(pi/2-eaf));
      ye := round(yc-precis*sin(pi/2-eaf));
      if bufmod then begin { buffer is active }

         { for filled shape, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these }
         r := sc_selectobject(bdc, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, fbrush);
         if r = -1 then winerr; { process windows error }
         { draw shape }
         b := sc_chord(bdc, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
         if not b then winerr; { process windows error }
         { restore }
         r := sc_selectobject(bdc, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(bdc, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr { process windows error }

      end;
      if indisp(win) then begin { do it again for the current screen }

         if not visible then winvis(win); { make sure we are displayed }
         r := sc_selectobject(devcon, fspen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, fbrush);
         if r = -1 then winerr; { process windows error }
         curoff(win);
         { draw shape }
         b := sc_chord(devcon, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
         if not b then winerr; { process windows error }
         curon(win);
         r := sc_selectobject(devcon, fpen);
         if r = -1 then winerr; { process windows error }
         r := sc_selectobject(devcon, sc_getstockobject(sc_null_brush));
         if r = -1 then winerr { process windows error }

      end

   end

end;

procedure fchord(var f: text; x1, y1, x2, y2, sa, ea: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifchord(win, x1, y1, x2, y2, sa, ea); { draw cord }
   unlockmain { end exclusive access }

end;

overload procedure fchord(x1, y1, x2, y2, sa, ea: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifchord(win, x1, y1, x2, y2, sa, ea); { draw cord }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw filled triangle

Draws a filled triangle in the current foreground color.

******************************************************************************}

procedure iftriangle(win: winptr; x1, y1, x2, y2, x3, y3: integer);

var pa:  array [1..3] of sc_point; { points of triangle }
    b:   boolean; { return value }
    r:   integer; { result holder }

begin

   with win^ do begin { in window context }

      { place triangle points in array }
      pa[1].x := x1-1;
      pa[1].y := y1-1;
      pa[2].x := x2-1;
      pa[2].y := y2-1;
      pa[3].x := x3-1;
      pa[3].y := y3-1;
      with screens[curupd]^ do begin

         if bufmod then begin { buffer is active }

            { for filled shape, the pen and brush settings are all wrong. we need
              a single pixel pen and a background brush. we set and restore these }
            r := sc_selectobject(bdc, fspen);
            if r = -1 then winerr; { process windows error }
            r := sc_selectobject(bdc, fbrush);
            if r = -1 then winerr; { process windows error }
            { draw to buffer }
            b := sc_polygon(bdc, pa);
            if not b then winerr; { process windows error }
            { restore }
            r := sc_selectobject(bdc, fpen);
            if r = -1 then winerr; { process windows error }
            r := sc_selectobject(bdc, sc_getstockobject(sc_null_brush));
            if r = -1 then winerr { process windows error }

         end;
         { draw to screen }
         if indisp(win) then begin

            if not visible then winvis(win); { make sure we are displayed }
            r := sc_selectobject(devcon, fspen);
            if r = -1 then winerr; { process windows error }
            r := sc_selectobject(devcon, fbrush);
            if r = -1 then winerr; { process windows error }
            curoff(win);
            b := sc_polygon(devcon, pa);
            if not b then winerr; { process windows error }
            curon(win);
            r := sc_selectobject(devcon, fpen);
            if r = -1 then winerr; { process windows error }
            r := sc_selectobject(devcon, sc_getstockobject(sc_null_brush));
            if r = -1 then winerr; { process windows error }

         end;
         { The progressive points get shifted left one. This causes progressive
           single point triangles to become triangle strips. }
         if tcurs then begin { process odd strip flip }

            tcurx1 := x1; { place next progressive endpoint }
            tcury1 := y1;
            tcurx2 := x3;
            tcury2 := y3

         end else begin { process even strip flip }

            tcurx1 := x3; { place next progressive endpoint }
            tcury1 := y3;
            tcurx2 := x2;
            tcury2 := y2

         end

      end

   end

end;

procedure ftriangle(var f: text; x1, y1, x2, y2, x3, y3: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^, screens[curupd]^ do begin { in window, screen contexts }

      iftriangle(win, x1, y1, x2, y2, x3, y3); { draw triangle }
      tcurs := false { set even strip flip state }

   end;
   unlockmain { end exclusive access }

end;

overload procedure ftriangle(x1, y1, x2, y2, x3, y3: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^, screens[curupd]^ do begin { in window, screen contexts }

      iftriangle(win, x1, y1, x2, y2, x3, y3); { draw triangle }
      tcurs := false { set even strip flip state }

   end;
   unlockmain { end exclusive access }

end;

overload procedure ftriangle(var f: text; x2, y2, x3, y3: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^, screens[curupd]^ do begin { in window, screen contexts }

      iftriangle(win, tcurx1, tcury1, x2, y2, x3, y3); { draw triangle }
      tcurs := false { set even strip flip state }

   end;
   unlockmain { end exclusive access }

end;

overload procedure ftriangle(x2, y2, x3, y3: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^, screens[curupd]^ do begin { in window, screen contexts }

      iftriangle(win, tcurx1, tcury1, x2, y2, x3, y3); { draw triangle }
      tcurs := false { set even strip flip state }

   end;
   unlockmain { end exclusive access }

end;

overload procedure ftriangle(var f: text; x3, y3: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^, screens[curupd]^ do begin { in window, screen contexts }

      tcurs := not tcurs; { flip the strip flip state }
      iftriangle(win, tcurx1, tcury1, tcurx2, tcury2, x3, y3) { draw triangle }

   end;
   unlockmain { end exclusive access }

end;

overload procedure ftriangle(x3, y3: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^, screens[curupd]^ do begin { in window, screen contexts }

      tcurs := not tcurs; { flip the strip flip state }
      iftriangle(win, tcurx1, tcury1, tcurx2, tcury2, x3, y3) { draw triangle }

   end;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set pixel

Sets a single logical pixel to the foreground color.

******************************************************************************}

procedure isetpixel(win: winptr; x, y: integer);

var r: sc_colorref; { return value }

begin

   with win^ do begin { in window context }

      if bufmod then begin { buffer is active }

         { paint buffer }
         r := sc_setpixel(screens[curupd]^.bdc, x-1, y-1, screens[curupd]^.fcrgb);
         if r = -1 then winerr; { process windows error }

      end;
      { paint screen }
      if indisp(win) then begin

         if not visible then winvis(win); { make sure we are displayed }
         curoff(win);
         r := sc_setpixel(devcon, x-1, y-1, screens[curupd]^.fcrgb);
         if r = -1 then winerr; { process windows error }
         curon(win)

      end

   end

end;

procedure setpixel(var f: text; x, y: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   isetpixel(win, x, y); { set pixel }
   unlockmain { end exclusive access }

end;

overload procedure setpixel(x, y: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   isetpixel(win, x, y); { set pixel }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set foreground to overwrite

Sets the foreground write mode to overwrite.

******************************************************************************}

procedure ifover(win: winptr);

var r: integer;

begin

   with win^ do begin { in window context }

      gfmod := mdnorm; { set foreground mode normal }
      screens[curupd]^.fmod := mdnorm;
      r := sc_setrop2(screens[curupd]^.bdc, sc_r2_copypen);
      if r = 0 then winerr; { process windows error }
      if indisp(win) then r := sc_setrop2(devcon, sc_r2_copypen)

   end

end;

procedure fover(var f: text);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifover(win); { set overwrite }
   unlockmain { end exclusive access }

end;

overload procedure fover;

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifover(win); { set overwrite }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set background to overwrite

Sets the background write mode to overwrite.

******************************************************************************}

procedure ibover(win: winptr);

var r: integer;

begin

   with win^ do begin { in window context }

      gbmod := mdnorm; { set background mode normal }
      screens[curupd]^.bmod := mdnorm;
      r := sc_setbkmode(screens[curupd]^.bdc, sc_opaque);
      if r = 0 then winerr; { process windows error }
      if indisp(win) then r := sc_setbkmode(devcon, sc_opaque)

   end

end;

procedure bover(var f: text);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ibover(win); { set overwrite }
   unlockmain { end exclusive access }

end;

overload procedure bover;

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ibover(win); { set overwrite }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set foreground to invisible

Sets the foreground write mode to invisible.

******************************************************************************}

procedure ifinvis(win: winptr);

var r: integer;

begin

   with win^ do begin { in window context }

      gfmod := mdinvis; { set foreground mode invisible }
      screens[curupd]^.fmod := mdinvis;
      r := sc_setrop2(screens[curupd]^.bdc, sc_r2_nop);
      if r = 0 then winerr; { process windows error }
      if indisp(win) then r := sc_setrop2(devcon, sc_r2_nop)

   end

end;

procedure finvis(var f: text);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifinvis(win); { set invisible }
   unlockmain { end exclusive access }

end;

overload procedure finvis;

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifinvis(win); { set invisible }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set background to invisible

Sets the background write mode to invisible.

******************************************************************************}

procedure ibinvis(win: winptr);

var r: integer;

begin

   with win^ do begin { in window context }

      gbmod := mdinvis; { set background mode invisible }
      screens[curupd]^.bmod := mdinvis;
      r := sc_setbkmode(screens[curupd]^.bdc, sc_transparent);
      if r = 0 then winerr; { process windows error }
      if indisp(win) then r := sc_setbkmode(devcon, sc_transparent)

   end

end;

procedure binvis(var f: text);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ibinvis(win); { set invisible }
   unlockmain { end exclusive access }

end;

overload procedure binvis;

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ibinvis(win); { set invisible }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set foreground to xor

Sets the foreground write mode to xor.

******************************************************************************}

procedure ifxor(win: winptr);

var r: integer;

begin

   with win^ do begin { in window context }

      gfmod := mdxor; { set foreground mode xor }
      screens[curupd]^.fmod := mdxor;
      r := sc_setrop2(screens[curupd]^.bdc, sc_r2_xorpen);
      if r = 0 then winerr; { process windows error }
      if indisp(win) then r := sc_setrop2(devcon, sc_r2_xorpen)

   end

end;

procedure fxor(var f: text);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifxor(win); { set xor }
   unlockmain { end exclusive access }

end;

overload procedure fxor;

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifxor(win); { set xor }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set background to xor

Sets the background write mode to xor.

******************************************************************************}

procedure ibxor(win: winptr);

begin

   with win^ do begin { in window context }

      gbmod := mdxor; { set background mode xor }
      screens[curupd]^.bmod := mdxor

   end

end;

procedure bxor(var f: text);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ibxor(win); { set xor }
   unlockmain { end exclusive access }

end;

overload procedure bxor;

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ibxor(win); { set xor }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set line width

Sets the width of lines and several other figures.

******************************************************************************}

procedure ilinewidth(win: winptr; w: integer);

var oh: sc_hgdiobj; { old pen }
    b:  boolean;    { return value }
    lb: sc_logbrush;

begin

   with win^, screens[curupd]^ do begin { in window, screen context }

      lwidth := w; { set new width }
      { create new pen with desired width }
      b := sc_deleteobject(fpen); { remove old pen }
      if not b then winerr; { process windows error }
      { create new pen }
      lb.lbstyle := sc_bs_solid;
      lb.lbcolor := fcrgb;
      lb.lbhatch := 0;
      fpen := sc_extcreatepen_nn(fpenstl, lwidth, lb);
      if fpen = 0 then winerr; { process windows error }
      { select to buffer dc }
      oh := sc_selectobject(bdc, fpen);
      if r = -1 then winerr; { process windows error }
      if indisp(win) then begin { activate on screen }

         oh := sc_selectobject(devcon, fpen); { select pen to display }
         if r = -1 then winerr; { process windows error }

      end

   end

end;

procedure linewidth(var f: text; w: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ilinewidth(win, w); { set line width }
   unlockmain { end exclusive access }

end;

overload procedure linewidth(w: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ilinewidth(win, w); { set line width }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find character size x

Returns the character width.

******************************************************************************}

function chrsizx(var f: text): integer;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      chrsizx := charspace;
   unlockmain { end exclusive access }

end;

overload function chrsizx: integer;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      chrsizx := charspace;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find character size y

Returns the character height.

******************************************************************************}

function chrsizy(var f: text): integer;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      chrsizy := linespace;
   unlockmain { end exclusive access }

end;

overload function chrsizy: integer;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      chrsizy := linespace;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find number of installed fonts

Finds the total number of installed fonts.

******************************************************************************}

function fonts(var f: text): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      fonts := fntcnt; { return global font counter }
   unlockmain { end exclusive access }

end;

overload function fonts: integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      fonts := fntcnt; { return global font counter }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Change fonts

Changes the current font to the indicated logical font number.

******************************************************************************}

procedure ifont(win: winptr; fc: integer); 

var fp: fontptr;

begin

   with win^ do begin { in window context }

      if screens[curupd]^.auto then error(eatoftc); { cannot perform with auto on }
      if fc < 1 then error(einvfnm); { invalid font number }
      { find indicated font }
      fp := fntlst;
      while (fp <> nil) and (fc > 1) do begin { search }

         fp := fp^.next; { mext font entry }
         fc := fc-1 { count }

      end;
      if fc > 1 then error(einvfnm); { invalid font number }
      if max(fp^.fn^) = 0 then error(efntemp); { font is not assigned }
      screens[curupd]^.cfont := fp; { place new font }
      gcfont := fp;
      newfont(win); { activate font }
      chgcur(win) { change cursors }

   end

end;

procedure font(var f: text; fc: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifont(win, fc); { set font }
   unlockmain { end exclusive access }

end;

overload procedure font(fc: integer);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifont(win, fc); { set font }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find name of font

Returns the name of a font by number.

******************************************************************************}

procedure ifontnam(win: winptr; fc: integer; var fns: string);

var fp: fontptr; { pointer to font entries }
    i:  integer; { string index }

begin

   with win^ do begin { in window context }

      if fc <= 0 then error(einvftn); { invalid number }
      fp := fntlst; { index top of list }
      while fc > 1 do begin { walk fonts }

         fp := fp^.next; { next font }
         fc := fc-1; { count }
         if fp = nil then error(einvftn) { check null }

      end;
      for i := 1 to max(fns) do fns[i] := ' '; { clear result }
      for i := 1 to max(fp^.fn^) do fns[i] := fp^.fn^[i] { copy name }

   end

end;

procedure fontnam(var f: text; fc: integer; var fns: string);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifontnam(win, fc, fns); { find font name }
   unlockmain { end exclusive access }

end;

overload procedure fontnam(fc: integer; var fns: string);

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifontnam(win, fc, fns); { find font name }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Change font size

Changes the font sizing to match the given character height. The character
and line spacing are changed, as well as the baseline.

******************************************************************************}

procedure ifontsiz(win: winptr; s: integer); 

begin

   with win^ do begin { in window context }

      { cannot perform with system font }
      if screens[curupd]^.cfont^.sys then error(etrmfts);
      if screens[curupd]^.auto then error(eatofts); { cannot perform with auto on }
      gfhigh := s; { set new font height }
      newfont(win) { activate font }

   end

end;

procedure fontsiz(var f: text; s: integer); 

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ifontsiz(win, s); { set font size }
   unlockmain { end exclusive access }

end;

overload procedure fontsiz(s: integer); 

var win: winptr;  { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifontsiz(win, s); { set font size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set character extra spacing y

Sets the extra character space to be added between lines, also referred to
as "ledding".

Not implemented yet.

******************************************************************************}

procedure chrspcy(var f: text; s: integer); 

begin

   refer(f, s) { stub out }

end;

overload procedure chrspcy(s: integer); 

begin

   refer(s) { stub out }

end;

{******************************************************************************

Sets extra character space x

Sets the extra character space to be added between characters, referred to
as "spacing".

Not implemented yet.

******************************************************************************}

procedure chrspcx(var f: text; s: integer); 

begin

   refer(f, s) { stub out }

end;

overload procedure chrspcx(s: integer); 

begin

   refer(s) { stub out }

end;

{******************************************************************************

Find dots per meter x

Returns the number of dots per meter resolution in x.

******************************************************************************}

function dpmx(var f: text): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      dpmx := sdpmx;
   unlockmain { end exclusive access }

end;

overload function dpmx: integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      dpmx := sdpmx;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find dots per meter y

Returns the number of dots per meter resolution in y.

******************************************************************************}

function dpmy(var f: text): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      dpmy := sdpmy;
   unlockmain { end exclusive access }

end;

overload function dpmy: integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      dpmy := sdpmy;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find string size in pixels

Returns the number of pixels wide the given string would be, considering
character spacing and kerning.

******************************************************************************}

function istrsiz(win: winptr; view s: string): integer; 

var sz: sc_size; { size holder }
    b:  boolean; { return value }

begin

   with win^ do begin { in window context }

      b := sc_gettextextentpoint32(screens[curupd]^.bdc, s, sz); { get spacing }
      if not b then winerr; { process windows error }
      istrsiz := sz.cx { return that }

   end

end;

function istrsizp(win: winptr; view s: string): integer; 

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    sp: pstring; { string holder }

begin

   with win^ do begin { in window context }

      copy(sp, s); { create dynamic string of length }
      b := sc_gettextextentpoint32(screens[curupd]^.bdc, sp^, sz); { get spacing }
      if not b then winerr; { process windows error }
      dispose(sp); { release dynamic string }
      istrsizp := sz.cx { return that }

   end

end;

function strsiz(var f: text; view s: string): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   strsiz := istrsiz(win, s); { find string size }
   unlockmain { end exclusive access }

end;

overload function strsiz(view s: string): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   strsiz := istrsiz(win, s); { find string size }
   unlockmain { end exclusive access }

end;

function strsizp(var f: text; view s: string): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   strsizp := istrsizp(win, s); { find string size }
   unlockmain { end exclusive access }

end;

overload function strsizp(view s: string): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   strsizp := istrsizp(win, s); { find string size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find character position in string

Finds the pixel offset to the given character in the string.

******************************************************************************}

function ichrpos(win: winptr; view s: string; p: integer): integer; 

var sp:  pstring; { string holder }
    siz: integer; { size holder }
    sz:  sc_size; { size holder }
    b:   boolean; { return value }
    i:   integer; { index for string }

begin

   with win^ do begin { in window context }

      if (p < 1) or (p > max(s)) then error(estrinx); { out of range }
      if p = 1 then siz := 0 { its already at the position }
      else begin { find substring length }

         new(sp, p-1); { get a substring allocation }
         for i := 1 to p-1 do sp^[i] := s[i]; { copy substring into place }
         { get spacing }
         b := sc_gettextextentpoint32(screens[curupd]^.bdc, sp^, sz);
         if not b then winerr; { process windows error }
         dispose(sp); { release substring }
         siz := sz.cx { place size }

      end;
      ichrpos := siz { return result }

   end

end;

function chrpos(var f: text; view s: string; p: integer): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   chrpos := ichrpos(win, s, p); { find character position }
   unlockmain { end exclusive access }

end;

overload function chrpos(view s: string; p: integer): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   chrpos := ichrpos(win, s, p); { find character position }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Write justified text

Writes a string of text with justification. The string and the width in pixels
is specified. Auto mode cannot be on for this function, nor can it be used on
the system font.

******************************************************************************}

procedure iwritejust(win: winptr; view s: string; n: integer); 

var sz:  sc_size; { size holder }
    b:   boolean; { return value }
    off: integer; { subscript offset }
    ra:  sc_gcp_results; { placement info record }

begin

   with win^, screens[curupd]^ do begin { in window, screen contexts }

      if cfont^.sys then error(ejstsys); { cannot perform on system font }
      if auto then error(eatopos); { cannot perform with auto on }
      off := 0; { set no subscript offset }
      if sasubs in attr then off := trunc(linespace*0.35);
      { get minimum spacing for string }
      b := sc_gettextextentpoint32(bdc, s, sz);
      if not b then winerr; { process windows error }
      { if requested less than required, force required }
      if sz.cx > n then n := sz.cx;
      { find justified spacing }
      ra.lstructsize := sc_gcp_results_len; { set length of record }
      { new(ra.lpoutstring); }
      ra.lpoutstring := nil;
      ra.lporder := nil;
      new(ra.lpdx); { set spacing array }
      ra.lpcaretpos := nil;
      ra.lpclass := nil;
      new(ra.lpglyphs);
      ra.nglyphs := max(s);
      ra.nmaxfit := 0;
      r := sc_getcharacterplacement(screens[curupd]^.bdc, s, n, ra,
                                    sc_gcp_justify or sc_gcp_maxextent);
      if r = 0 then winerr; { process windows error }
      if bufmod then begin { draw to buffer }

         { draw the string to current position }
         b := sc_exttextout_n(bdc, curxg-1, curyg-1+off, 0, s, ra.lpdx);
         if not b then winerr { process windows error }

      end;
      if indisp(win) then begin

         if not visible then winvis(win); { make sure we are displayed }
         { draw character on screen }
         curoff(win); { hide the cursor }
         { draw the string to current position }
         b := sc_exttextout_n(devcon, curxg-1, curyg-1+off, 0, s, ra.lpdx);
         if not b then winerr; { process windows error }
         curon(win) { show the cursor }

      end;
      curxg := curxg+n; { advance the character width }
      curx := curxg div charspace+1; { recalculate character position }
      if indisp(win) then setcur(win) { set cursor on screen }

   end

end;

procedure writejust(var f: text; view s: string; n: integer); 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   iwritejust(win, s, n); { write justified text }
   unlockmain { end exclusive access }

end;

overload procedure writejust(view s: string; n: integer); 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iwritejust(win, s, n); { write justified text }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find justified character position

Given a string, a character position in that string, and the total length
of the string in pixels, returns the offset in pixels from the start of the
string to the given character, with justification taken into account.
The model used is that the extra space needed is divided by the number of
spaces, with the fractional part lost.

******************************************************************************}

function ijustpos(win: winptr; view s: string; p, n: integer): integer; 

var off: integer; { offset to character }
    w:   integer; { minimum string size }
    ra:  sc_gcp_results; { placement info record }
    i:   integer; { index for string }

begin

   with win^ do begin { in window context }

      if (p < 1) or (p > max(s)) then error(estrinx); { out of range }
      if p = 1 then off := 0 { its already at the position }
      else begin { find substring length }

         w := istrsiz(win, s); { find minimum character spacing }
         { if allowed is less than the min, return nominal spacing }
         if n <= w then off := ichrpos(win, s, p) else begin

            { find justified spacing }
            ra.lstructsize := sc_gcp_results_len; { set length of record }
            { new(ra.lpoutstring); }
            ra.lpoutstring := nil;
            ra.lporder := nil;
            new(ra.lpdx); { set spacing array }
            ra.lpcaretpos := nil;
            ra.lpclass := nil;
            new(ra.lpglyphs);
            ra.nglyphs := max(s);
            ra.nmaxfit := 0;
            r := sc_getcharacterplacement(screens[curupd]^.bdc, s, n, ra,
                                          sc_gcp_justify or sc_gcp_maxextent);
            if r = 0 then winerr; { process windows error }
            off := 0; { clear offset }
            { add in all widths to the left of position to offset }
            for i := 1 to p-1 do off := off+ra.lpdx^[i]

         end

      end;
      ijustpos := off { return result }

   end

end;

function justpos(var f: text; view s: string; p, n: integer): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   justpos := ijustpos(win, s, p, n); { find justified character position }
   unlockmain { end exclusive access }

end;

overload function justpos(view s: string; p, n: integer): integer; 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   justpos := ijustpos(win, s, p, n); { find justified character position }
   unlockmain { end exclusive access }

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

   refer(f, e) { stub out }

end;

overload procedure condensed(e: boolean); 

begin

   refer(e) { stub out }

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

overload procedure extended(e: boolean); 

begin

   refer(e) { stub out }

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

   refer(f, e) { stub out }

end;

overload procedure xlight(e: boolean); 

begin

   refer(e) { stub out }

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

   refer(f, e) { stub out }

end;

overload procedure light(e: boolean); 

begin

   refer(e) { stub out }

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

   refer(f, e) { stub out }

end;

overload procedure xbold(e: boolean); 

begin

   refer(e) { stub out }

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

   refer(f, e) { stub out }

end;

overload procedure hollow(e: boolean); 

begin

   refer(e) { stub out }

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

   refer(f, e) { stub out }

end;

overload procedure raised(e: boolean); 

begin

   refer(e) { stub out }

end;

{******************************************************************************

Delete picture

Deletes a loaded picture.

******************************************************************************}

procedure idelpict(win: winptr; p: integer);

var r: integer; { result holder }
    b: boolean; { result holder }

begin

   with win^ do begin { in window context }

      if (p < 1) or (p > maxpic) then error(einvhan); { bad picture handle }
      if pictbl[p].han = 0 then error(einvhan); { bad picture handle }
      r := sc_selectobject(pictbl[p].hdc, pictbl[p].ohn); { reselect old object }
      if r = -1 then winerr; { process windows error }
      b := sc_deletedc(pictbl[p].hdc); { delete device context }
      if not b then winerr; { process windows error }
      b := sc_deleteobject(pictbl[p].han); { delete bitmap }
      if not b then winerr; { process windows error }
      pictbl[p].han := 0 { set this entry free }

   end

end;

procedure delpict(var f: text; p: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   idelpict(win, p); { delete picture file }
   unlockmain { end exclusive access }

end;

overload procedure delpict(p: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idelpict(win, p); { delete picture file }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Load picture

Loads a picture into a slot of the loadable pictures array.

******************************************************************************}

procedure iloadpict(win: winptr; p: integer; view fn: string);

const maxfil = 250; { number of filename characters in buffer }

var r:   integer;   { result holder }
    bmi: sc_bitmap; { bit map information header }
    fnh: packed array [1..maxfil] of char; { file name holder }
    i:  integer;   { index for string }

procedure setext(view ext: string);

var i, x: integer; { index for string }
    f:    boolean; { found flag } 

begin

   f := false; { set no extention found }
   { search for extention }
   for i := 1 to maxfil do if fnh[i] = '.' then f := true; { found }
   if not f then begin { no extention, place one }

      i := maxfil; { index last character in string }
      while (i > 1) and (fnh[i] = ' ') do i := i-1;
      if maxfil-i < 4 then error(epicftl); { filename too large }
      for x := 1 to 4 do fnh[i+x] := ext[x] { place extention }

   end

end;

begin

   with win^ do begin { in window context }

      if len(fn) > maxfil then error(epicftl); { filename too large }
      clears(fnh); { clear filename holding }
      for i := 1 to len(fn) do fnh[i] := fn[i]; { copy }
      setext('.bmp'); { try bitmap first }
      if not exists(fnh) then begin { try dib }

         setext('.dib');
         if not exists(fnh) then error(epicfnf); { no file found }

      end;
      if (p < 1) or (p > maxpic) then error(einvhan); { bad picture handle }
      { if the slot is already occupied, delete that picture }
      if pictbl[p].han <> 0 then idelpict(win, p);
      { load the image into memory }
      pictbl[p].han :=
         sc_loadimage(0, fnh, sc_image_bitmap, 0, 0, sc_lr_loadfromfile);
      if pictbl[p].han = 0 then winerr; { process windows error }
      { put it into a device context }
      pictbl[p].hdc := sc_createcompatibledc(devcon);
      if pictbl[p].hdc = 0 then winerr; { process windows error }
      { select that to device context }
      pictbl[p].ohn := sc_selectobject(pictbl[p].hdc, pictbl[p].han);
      if pictbl[p].ohn = -1 then winerr; { process windows error }
      { get sizes }
      r := sc_getobject_bitmap(pictbl[p].han, sc_bitmap_len, bmi);
      if r = 0 then winerr; { process windows error }
      pictbl[p].sx := bmi.bmwidth; { set size x }
      pictbl[p].sy := bmi.bmheight { set size x }

   end

end;

procedure loadpict(var f: text; p: integer; view fn: string);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   iloadpict(win, p, fn); { load picture file }
   unlockmain { end exclusive access }

end;

overload procedure loadpict(p: integer; view fn: string);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iloadpict(win, p, fn); { load picture file }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find size x of picture

Returns the size in x of the logical picture.

******************************************************************************}

function pictsizx(var f: text; p: integer): integer;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do begin { in window context }

      if (p < 1) or (p > maxpic) then error(einvhan); { bad picture handle }
      if pictbl[p].han = 0 then error(einvhan); { bad picture handle }
      pictsizx := pictbl[p].sx { return x size }

   end;
   unlockmain { end exclusive access }

end;

overload function pictsizx(p: integer): integer;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do begin { in window context }

      if (p < 1) or (p > maxpic) then error(einvhan); { bad picture handle }
      if pictbl[p].han = 0 then error(einvhan); { bad picture handle }
      pictsizx := pictbl[p].sx { return x size }

   end;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find size y of picture

Returns the size in y of the logical picture.

******************************************************************************}

function pictsizy(var f: text; p: integer): integer;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do begin { in window context }

      if (p < 1) or (p > maxpic) then error(einvhan); { bad picture handle }
      if pictbl[p].han = 0 then error(einvhan); { bad picture handle }

      pictsizy := pictbl[p].sy { return x size }

   end;
   unlockmain { end exclusive access }

end;

overload function pictsizy(p: integer): integer;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do begin { in window context }

      if (p < 1) or (p > maxpic) then error(einvhan); { bad picture handle }
      if pictbl[p].han = 0 then error(einvhan); { bad picture handle }

      pictsizy := pictbl[p].sy { return x size }

   end;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Draw picture

Draws a picture from the given file to the rectangle. The picture is resized
to the size of the rectangle.

Images will be kept in a rotating cache to prevent repeating reloads.

******************************************************************************}

procedure ipicture(win: winptr; p: integer; x1, y1, x2, y2: integer);

var b:   boolean;  { result holder }
    rop: sc_dword; { rop holder }

begin

   with win^ do begin { in window context }

      if (p < 1) or (p > maxpic) then error(einvhan); { bad picture handle }
      if pictbl[p].han = 0 then error(einvhan); { bad picture handle }
      case screens[curupd]^.fmod of { rop }

         mdnorm:  rop := sc_srccopy; { straight }
         mdinvis: ; { no-op }
         mdxor:   rop := sc_srcinvert { xor }

      end;
      if screens[curupd]^.fmod <> mdinvis then begin { not a no-op }

         if bufmod then begin { buffer mode on }

            { paint to buffer }
            b := sc_stretchblt(screens[curupd]^.bdc, 
                               x1-1, y1-1, x2-x1+1, y2-y1+1,
                               pictbl[p].hdc, 0, 0, pictbl[p].sx, pictbl[p].sy,
                               rop);
            if not b then winerr; { process windows error }

         end;
         if indisp(win) then begin { paint to screen }

            if not visible then winvis(win); { make sure we are displayed }
            curoff(win);
            b := sc_stretchblt(devcon, x1-1, y1-1, x2-x1+1, y2-y1+1, 
                               pictbl[p].hdc, 0, 0, pictbl[p].sx, pictbl[p].sy,
                               rop);
            if not b then winerr; { process windows error }
            curon(win)

         end

      end

   end

end;

procedure picture(var f: text; p: integer; x1, y1, x2, y2: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   ipicture(win, p, x1, y1, x2, y2); { draw picture }
   unlockmain { end exclusive access }

end;

overload procedure picture(p: integer; x1, y1, x2, y2: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ipicture(win, p, x1, y1, x2, y2); { draw picture }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set viewport offset graphical

Sets the offset of the viewport in logical space, in pixels, anywhere from
-maxint to maxint.

******************************************************************************}

procedure iviewoffg(win: winptr; x, y: integer); 

begin

   with win^ do begin { in window context }

      { check change is needed }
      if (x <> screens[curupd]^.offx) and (y <> screens[curupd]^.offy) then begin

         screens[curupd]^.offx := x; { set offsets }
         screens[curupd]^.offy := y;
         goffx := x;
         goffy := y;
         iclear(win) { clear buffer }

      end

   end

end;

procedure viewoffg(var f: text; x, y: integer); 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   iviewoffg(win, x, y); { set viewport offset }
   unlockmain { end exclusive access }

end;

overload procedure viewoffg(x, y: integer); 

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iviewoffg(win, x, y); { set viewport offset }
   unlockmain { end exclusive access }

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

procedure iviewscale(win: winptr; x, y: real); 

begin
   
   with win^ do begin { in window context }

      { in this starting simplistic formula, the ratio is set x*maxint/maxint.
        it works, but can overflow for large coordinates or scales near 1 }
      screens[curupd]^.wextx := 100;
      screens[curupd]^.wexty := 100;
      screens[curupd]^.vextx := trunc(x*100);
      screens[curupd]^.vexty := trunc(y*100);
      gwextx := 100;
      gwexty := 100;
      gvextx := trunc(x*100);
      gvexty := trunc(y*100);
      iclear(win) { clear buffer }

   end

end;

procedure viewscale(var f: text; x, y: real);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   iviewscale(win, x, y); { set viewport scale }
   unlockmain { end exclusive access }

end;

overload procedure viewscale(x, y: real);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iviewscale(win, x, y); { set viewport scale }
   unlockmain { end exclusive access }

end;

overload procedure viewscale(var f: text; s: real);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   iviewscale(win, s, s); { set viewport scale }
   unlockmain { end exclusive access }

end;

overload procedure viewscale(s: real);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iviewscale(win, s, s); { set viewport scale }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Print message string

This routine is for diagnostic use. Comment it out on production builds.

******************************************************************************}

procedure prtmsgstr(mn: integer);

begin

   prtnum(mn, 4, 16);
   prtstr(': ');
   if (mn >= $800) and (mn <= $bfff) then prtstr('User message')
   else if (mn >= $c000) and (mn <= $ffff) then prtstr('Registered message')
   else case mn of

      $0000: prtstr('WM_NULL');                 
      $0001: prtstr('WM_CREATE');               
      $0002: prtstr('WM_DESTROY');              
      $0003: prtstr('WM_MOVE');                 
      $0005: prtstr('WM_SIZE');                 
      $0006: prtstr('WM_ACTIVATE');             
      $0007: prtstr('WM_SETFOCUS');             
      $0008: prtstr('WM_KILLFOCUS');            
      $000A: prtstr('WM_ENABLE');               
      $000B: prtstr('WM_SETREDRAW');            
      $000C: prtstr('WM_SETTEXT');              
      $000D: prtstr('WM_GETTEXT');              
      $000E: prtstr('WM_GETTEXTLENGTH');        
      $000F: prtstr('WM_PAINT');                
      $0010: prtstr('WM_CLOSE');                
      $0011: prtstr('WM_QUERYENDSESSION');      
      $0012: prtstr('WM_QUIT');                 
      $0013: prtstr('WM_QUERYOPEN');            
      $0014: prtstr('WM_ERASEBKGND');           
      $0015: prtstr('WM_SYSCOLORCHANGE');       
      $0016: prtstr('WM_ENDSESSION');           
      $0018: prtstr('WM_SHOWWINDOW');           
      $001A: prtstr('WM_WININICHANGE');         
      $001B: prtstr('WM_DEVMODECHANGE');        
      $001C: prtstr('WM_ACTIVATEAPP');          
      $001D: prtstr('WM_FONTCHANGE');           
      $001E: prtstr('WM_TIMECHANGE');           
      $001F: prtstr('WM_CANCELMODE');           
      $0020: prtstr('WM_SETCURSOR');            
      $0021: prtstr('WM_MOUSEACTIVATE');        
      $0022: prtstr('WM_CHILDACTIVATE');        
      $0023: prtstr('WM_QUEUESYNC');            
      $0024: prtstr('WM_GETMINMAXINFO');        
      $0026: prtstr('WM_PAINTICON');            
      $0027: prtstr('WM_ICONERASEBKGND');       
      $0028: prtstr('WM_NEXTDLGCTL');           
      $002A: prtstr('WM_SPOOLERSTATUS');        
      $002B: prtstr('WM_DRAWITEM');             
      $002C: prtstr('WM_MEASUREITEM');          
      $002D: prtstr('WM_DELETEITEM');           
      $002E: prtstr('WM_VKEYTOITEM');           
      $002F: prtstr('WM_CHARTOITEM');           
      $0030: prtstr('WM_SETFONT');              
      $0031: prtstr('WM_GETFONT');              
      $0032: prtstr('WM_SETHOTKEY');            
      $0033: prtstr('WM_GETHOTKEY');            
      $0037: prtstr('WM_QUERYDRAGICON');        
      $0039: prtstr('WM_COMPAREITEM');          
      $0041: prtstr('WM_COMPACTING');           
      $0042: prtstr('WM_OTHERWINDOWCREATED');   
      $0043: prtstr('WM_OTHERWINDOWDESTROYED'); 
      $0044: prtstr('WM_COMMNOTIFY');           
      $0045: prtstr('WM_HOTKEYEVENT');          
      $0046: prtstr('WM_WINDOWPOSCHANGING');    
      $0047: prtstr('WM_WINDOWPOSCHANGED');     
      $0048: prtstr('WM_POWER');                
      $004A: prtstr('WM_COPYDATA');             
      $004B: prtstr('WM_CANCELJOURNAL');        
      $004E: prtstr('WM_NOTIFY');
      $0050: prtstr('WM_INPUTLANGCHANGEREQUEST');
      $0051: prtstr('WM_INPUTLANGCHANGE');              
      $0052: prtstr('WM_TCARD');                        
      $0053: prtstr('WM_HELP');                         
      $0054: prtstr('WM_USERCHANGED');                  
      $0055: prtstr('WM_NOTIFYFORMAT');                 
      $007B: prtstr('WM_CONTEXTMENU');                  
      $007C: prtstr('WM_STYLECHANGING');                
      $007D: prtstr('WM_STYLECHANGED');                 
      $007E: prtstr('WM_DISPLAYCHANGE');                
      $007F: prtstr('WM_GETICON');                      
      $0080: prtstr('WM_SETICON');                      
      $0081: prtstr('WM_NCCREATE');             
      $0082: prtstr('WM_NCDESTROY');            
      $0083: prtstr('WM_NCCALCSIZE');           
      $0084: prtstr('WM_NCHITTEST');            
      $0085: prtstr('WM_NCPAINT');              
      $0086: prtstr('WM_NCACTIVATE');           
      $0087: prtstr('WM_GETDLGCODE');           
      $00A0: prtstr('WM_NCMOUSEMOVE');          
      $00A1: prtstr('WM_NCLBUTTONDOWN');        
      $00A2: prtstr('WM_NCLBUTTONUP');          
      $00A3: prtstr('WM_NCLBUTTONDBLCLK');      
      $00A4: prtstr('WM_NCRBUTTONDOWN');        
      $00A5: prtstr('WM_NCRBUTTONUP');          
      $00A6: prtstr('WM_NCRBUTTONDBLCLK');      
      $00A7: prtstr('WM_NCMBUTTONDOWN');        
      $00A8: prtstr('WM_NCMBUTTONUP');          
      $00A9: prtstr('WM_NCMBUTTONDBLCLK');      
      {$0100: prtstr('WM_KEYFIRST');}             
      $0100: prtstr('WM_KEYDOWN');
      $0101: prtstr('WM_KEYUP');                
      $0102: prtstr('WM_CHAR');                 
      $0103: prtstr('WM_DEADCHAR');             
      $0104: prtstr('WM_SYSKEYDOWN');           
      $0105: prtstr('WM_SYSKEYUP');             
      $0106: prtstr('WM_SYSCHAR');              
      $0107: prtstr('WM_SYSDEADCHAR');          
      $0108: prtstr('WM_KEYLAST');              
      $0109: prtstr('WM_UNICHAR');
      $0110: prtstr('WM_INITDIALOG');           
      $0111: prtstr('WM_COMMAND');              
      $0112: prtstr('WM_SYSCOMMAND');           
      $0113: prtstr('WM_TIMER');                
      $0114: prtstr('WM_HSCROLL');              
      $0115: prtstr('WM_VSCROLL');              
      $0116: prtstr('WM_INITMENU');             
      $0117: prtstr('WM_INITMENUPOPUP');        
      $011F: prtstr('WM_MENUSELECT');           
      $0120: prtstr('WM_MENUCHAR');             
      $0121: prtstr('WM_ENTERIDLE');            
      $0132: prtstr('WM_CTLCOLORMSGBOX');       
      $0133: prtstr('WM_CTLCOLOREDIT');         
      $0134: prtstr('WM_CTLCOLORLISTBOX');      
      $0135: prtstr('WM_CTLCOLORBTN');          
      $0136: prtstr('WM_CTLCOLORDLG');          
      $0137: prtstr('WM_CTLCOLORSCROLLBAR');    
      $0138: prtstr('WM_CTLCOLORSTATIC');       
      { $0200: prtstr('WM_MOUSEFIRST'); }
      $0200: prtstr('WM_MOUSEMOVE');
      $0201: prtstr('WM_LBUTTONDOWN');          
      $0202: prtstr('WM_LBUTTONUP');            
      $0203: prtstr('WM_LBUTTONDBLCLK');        
      $0204: prtstr('WM_RBUTTONDOWN');          
      $0205: prtstr('WM_RBUTTONUP');            
      $0206: prtstr('WM_RBUTTONDBLCLK');        
      $0207: prtstr('WM_MBUTTONDOWN');          
      $0208: prtstr('WM_MBUTTONUP');            
      $0209: prtstr('WM_MBUTTONDBLCLK');        
      { $0209: prtstr('WM_MOUSELAST'); }           
      $0210: prtstr('WM_PARENTNOTIFY');         
      $0211: prtstr('WM_ENTERMENULOOP');        
      $0212: prtstr('WM_EXITMENULOOP');         
      $0220: prtstr('WM_MDICREATE');            
      $0221: prtstr('WM_MDIDESTROY');           
      $0222: prtstr('WM_MDIACTIVATE');          
      $0223: prtstr('WM_MDIRESTORE');           
      $0224: prtstr('WM_MDINEXT');              
      $0225: prtstr('WM_MDIMAXIMIZE');          
      $0226: prtstr('WM_MDITILE');              
      $0227: prtstr('WM_MDICASCADE');           
      $0228: prtstr('WM_MDIICONARRANGE');       
      $0229: prtstr('WM_MDIGETACTIVE');         
      $0230: prtstr('WM_MDISETMENU');           
      $0231: prtstr('WM_ENTERSIZEMOVE');           
      $0232: prtstr('WM_EXITSIZEMOVE');           
      $0233: prtstr('WM_DROPFILES');            
      $0234: prtstr('WM_MDIREFRESHMENU');       
      $0300: prtstr('WM_CUT');                  
      $0301: prtstr('WM_COPY');                 
      $0302: prtstr('WM_PASTE');                
      $0303: prtstr('WM_CLEAR');                
      $0304: prtstr('WM_UNDO');                 
      $0305: prtstr('WM_RENDERFORMAT');         
      $0306: prtstr('WM_RENDERALLFORMATS');     
      $0307: prtstr('WM_DESTROYCLIPBOARD');     
      $0308: prtstr('WM_DRAWCLIPBOARD');        
      $0309: prtstr('WM_PAINTCLIPBOARD');       
      $030A: prtstr('WM_VSCROLLCLIPBOARD');     
      $030B: prtstr('WM_SIZECLIPBOARD');        
      $030C: prtstr('WM_ASKCBFORMATNAME');      
      $030D: prtstr('WM_CHANGECBCHAIN');        
      $030E: prtstr('WM_HSCROLLCLIPBOARD');     
      $030F: prtstr('WM_QUERYNEWPALETTE');      
      $0310: prtstr('WM_PALETTEISCHANGING');    
      $0311: prtstr('WM_PALETTECHANGED');       
      $0312: prtstr('WM_HOTKEY');               
      $0380: prtstr('WM_PENWINFIRST');          
      $038F: prtstr('WM_PENWINLAST');           
      $03A0: prtstr('MM_JOY1MOVE');
      $03A1: prtstr('MM_JOY2MOVE');
      $03A2: prtstr('MM_JOY1ZMOVE');
      $03A3: prtstr('MM_JOY2ZMOVE');
      $03B5: prtstr('MM_JOY1BUTTONDOWN');
      $03B6: prtstr('MM_JOY2BUTTONDOWN');
      $03B7: prtstr('MM_JOY1BUTTONUP');
      $03B8: prtstr('MM_JOY2BUTTONUP');
      else prtstr('???')

   end

end;

{******************************************************************************

Print message diagnostic

This routine is for diagnostic use. Comment it out on production builds.

******************************************************************************}

procedure prtmsg(var m: sc_msg);

begin

   prtstr('handle: ');
   prtnum(m.hwnd, 8, 16); 
   prtstr(' message: '); 
   prtmsgstr(m.message); 
   prtstr(' wparam: '); 
   prtnum(m.wparam, 8, 16); 
   prtstr(' lparam: ');
   prtnum(m.lparam, 8, 16);
   prtstr('\cr\lf')

end;

{******************************************************************************

Print unpacked message diagnostic

This routine is for diagnostic use. Comment it out on production builds.

******************************************************************************}

procedure prtmsgu(hwnd, imsg, wparam, lparam: integer);

begin

   prtstr('handle: ');
   prtnum(hwnd, 8, 16); 
   prtstr(' message: '); 
   prtmsgstr(imsg); 
   prtstr(' wparam: '); 
   prtnum(wparam, 8, 16); 
   prtstr(' lparam: ');
   prtnum(lparam, 8, 16);
   prtstr('\cr\lf')

end;

{******************************************************************************

Aquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

******************************************************************************}

procedure ievent(ifn: ss_filhdl; var er: evtrec);

var msg:    sc_msg;    { windows message }
    keep:   boolean;   { keep event flag }
    win:    winptr;    { pointer to windows structure }
    ofn:    ss_filhdl; { file handle from incoming message }
    ep:     eqeptr;    { event queuing pointer }
    b:      boolean;   { return value }

{

Process keyboard event.
The events are mapped from IBM-PC keyboard keys as follows:

etup      up arrow            cursor up one line 
etdown    down arrow          down one line 
etleft    left arrow          left one character 
etright   right arrow         right one character 
etleftw   cntrl-left arrow    left one word 
etrightw  cntrl-right arrow   right one word 
ethome    cntrl-home          home of document 
ethomes   shift-home          home of screen 
ethomel   home                home of line 
etend     cntrl-end           end of document 
etends    shift-end           end of screen 
etendl    end                 end of line 
etscrl    shift-left arrow    scroll left one character 
etscrr    shift-right arrow   scroll right one character 
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
etfun     f1-f12              function keys
etmenu    alt                 display menu 
etend     cntrl-c             terminate program 

This is equivalent to the CUA or Common User Access keyboard mapping with 
various local extentions.

}

procedure keyevent;

begin

   if chr(msg.wparam) = '\cr' then er.etype := etenter { set enter line }
   else if chr(msg.wparam) = '\bs' then 
      er.etype := etdelcb { set delete character backwards }
   else if chr(msg.wparam) = '\ht' then er.etype := ettab { set tab }
   else if chr(msg.wparam) = '\etx' then begin

      er.etype := etterm; { set end program }
      fend := true { set end was ordered }

   end else if chr(msg.wparam) = '\xoff' then
      er.etype := etstop { set stop program }
   else if chr(msg.wparam) = '\xon' then
      er.etype := etcont { set continue program }
   else if chr(msg.wparam) = '\esc' then
      er.etype := etcan { set cancel operation }
   else begin { normal character }

      er.etype := etchar; { set character event }
      er.char := chr(msg.wparam)

   end;
   keep := true { set keep event }

end;

procedure ctlevent;

begin

   with win^ do begin { in window context }

      if msg.wparam <= $ff then
         { limited to codes we can put in a set }
         if msg.wparam in 
            [sc_vk_home, sc_vk_end, sc_vk_left, sc_vk_right,
             sc_vk_up, sc_vk_down, sc_vk_insert, sc_vk_delete,
             sc_vk_prior, sc_vk_next, sc_vk_f1..sc_vk_f12,
             sc_vk_menu, sc_vk_cancel] then begin

         { this key is one we process }
         case msg.wparam of { key }

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

               if cntrl then er.etype := etleftw { left one word }
               else if shift then er.etype := etscrl { scroll left one character }
               else er.etype := etleft { left one character }

            end;
            sc_vk_right: begin { right }

               if cntrl then er.etype := etrightw { right one word }
               else if shift then er.etype := etscrr { scroll right one character }
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
button 3: Windows middle from left button

Button 4 is unused. Double clicks will be ignored, displaying my utter
contempt for the whole double click concept.

}

{ update mouse parameters }

procedure mouseupdate;

begin

   with win^ do begin { in window context }

      { we prioritize events by: movements 1st, button clicks 2nd }
      if (nmpx <> mpx) or (nmpy <> mpy) then begin { create movement event }

         er.etype := etmoumov; { set movement event }
         er.mmoun := 1; { mouse 1 }
         er.moupx := nmpx; { set new mouse position }
         er.moupy := nmpy;
         mpx := nmpx; { save new position }
         mpy := nmpy;
         keep := true { set to keep }

      end else if (nmpxg <> mpxg) or (nmpyg <> mpyg) then begin

         { create graphical movement event }
         er.etype := etmoumovg; { set movement event }
         er.mmoung := 1; { mouse 1 }
         er.moupxg := nmpxg; { set new mouse position }
         er.moupyg := nmpyg;
         mpxg := nmpxg; { save new position }
         mpyg := nmpyg;
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

      end

   end

end;

{ register mouse status }

procedure mouseevent;

begin

   with win^ do begin { in window context }

      nmpx := msg.lparam mod 65536 div charspace+1; { get mouse x }
      nmpy := msg.lparam div 65536 div linespace+1; { get mouse y }
      nmpxg := msg.lparam mod 65536+1; { get mouse graphical x }
      nmpyg := msg.lparam div 65536+1; { get mouse graphical y }
      { set new button statuses }
      if msg.message = sc_wm_lbuttondown then nmb1 := true;
      if msg.message = sc_wm_lbuttonup then nmb1 := false;
      if msg.message = sc_wm_mbuttondown then nmb2 := true;
      if msg.message = sc_wm_mbuttonup then nmb2 := false;
      if msg.message = sc_wm_rbuttondown then nmb3 := true;
      if msg.message = sc_wm_rbuttonup then nmb3 := false

   end

end;

{ queue event to window }

procedure enqueue(var el: eqeptr; var er: evtrec);

var ep: eqeptr; { pointer to queue entries }

begin

   geteqe(ep); { get a new event container }
   ep^.evt := er; { copy event to container }
   { insert into bubble list }
   if el = nil then begin { list empty, place as first entry }

      ep^.last := ep; { tie entry to itself }
      ep^.next := ep

   end else begin { list has entries }

      ep^.last := el; { link last to current }
      ep^.next := el^.next; { link next to next of current }
      el^.next := ep { link current to new }

   end;
   el := ep { set that as new root }
   { ok, new entries are moving to the last direction, and we remove entries
     from the next direction. }

end;

{ process joystick messages }

procedure joymes;

{ issue event for changed button }

procedure updn(bn, bm: integer);

begin

   { if there is already a message processed, enqueue that }
   if keep then enqueue(opnfil[opnfil[ofn]^.inl]^.evt, er); { queue it }
   if (msg.wparam and bm) <> 0 then begin { assert }

      er.etype := etjoyba; { set assert }
      if (msg.message = sc_mm_joy1buttondown) or 
         (msg.message = sc_mm_joy1buttonup) then er.ajoyn := 1
      else er.ajoyn := 2;
      er.ajoybn := bn { set number }

   end else begin { deassert }

      er.etype := etjoybd; { set deassert }
      if (msg.message = sc_mm_joy1buttondown) or 
         (msg.message = sc_mm_joy1buttonup) then er.ajoyn := 1
      else er.ajoyn := 2;
      er.djoybn := bn { set number }
   
   end;
   keep := true { set keep event }

end;

begin

   { register changes on each button }
   if (msg.wparam and sc_joy_button1chg) <> 0 then updn(1, sc_joy_button1);
   if (msg.wparam and sc_joy_button2chg) <> 0 then updn(2, sc_joy_button2);
   if (msg.wparam and sc_joy_button3chg) <> 0 then updn(3, sc_joy_button3);
   if (msg.wparam and sc_joy_button4chg) <> 0 then updn(4, sc_joy_button4)

end;

{ process windows messages to event }

procedure winevt;

var cr:         sc_rect; { client rectangle }
    wp:         wigptr;  { widget entry pointer }
    r:          integer; { result holder }
    b:          boolean; { boolean result }
    v:          integer; { value }
    x, y, z:    integer; { joystick readback }
    dx, dy, dz: integer; { joystick readback differences }
    nm:         integer; { notification message }
    f:          real;    { floating point temp }
    i2nmhdrp: record { convertion record }

       case boolean of

          true:  (i:  integer);
          false: (rp: ^sc_nmhdr);

    end;

begin

   with win^ do begin { in window context }

      if msg.message = sc_wm_paint then begin { window paint }

         if not bufmod then begin { our client handles it's own redraws }

            { form redraw request }
            b := sc_getupdaterect(winhan, cr, false);
            er.etype := etredraw; { set redraw message }
            er.rsx := msg.wparam div $10000; { fill the rectangle with update }
            er.rsy := msg.wparam mod $10000;
            er.rex := msg.lparam div $10000;
            er.rey := msg.lparam mod $10000;
            keep := true { set keep event }

         end

      end else if msg.message = sc_wm_size then begin { resize }

         if not bufmod then begin { main thread handles resizes }

            { check if maximize, minimize, or exit from either mode }
            if msg.wparam = sc_size_maximized then begin

               er.etype := etmax; { set maximize event }
               { save the event ahead of the resize }
               enqueue(opnfil[opnfil[ofn]^.inl]^.evt, er) { queue it }

            end else if msg.wparam = sc_size_minimized then begin

               er.etype := etmin; { set minimize event }
               { save the event ahead of the resize }
               enqueue(opnfil[opnfil[ofn]^.inl]^.evt, er) { queue it }

            end else if (msg.wparam = sc_size_restored) and
                        ((sizests = sc_size_minimized) or
                         (sizests = sc_size_maximized)) then begin
               
               { window is restored, and was minimized or maximized }    
               er.etype := etnorm; { set normalize event }
               { save the event ahead of the resize }
               enqueue(opnfil[opnfil[ofn]^.inl]^.evt, er) { queue it }

            end;
            sizests := msg.wparam; { save size status }
            { process resize message }
            gmaxxg := msg.lparam and $ffff; { set x size }
            gmaxyg := msg.lparam div 65536 and $ffff; { set y size }
            gmaxx := gmaxxg div charspace; { find character size x }
            gmaxy := gmaxyg div linespace; { find character size y }
            screens[curdsp]^.maxx := gmaxx; { copy to screen control }
            screens[curdsp]^.maxy := gmaxy;
            screens[curdsp]^.maxxg := gmaxxg;
            screens[curdsp]^.maxyg := gmaxyg;
            { place the resize message }
            er.etype := etresize; { set resize message }
            keep := true; { set keep event }

         end

      end else if msg.message = sc_wm_char then
         keyevent { process characters }
      else if msg.message = sc_wm_keydown then begin

         if msg.wparam = sc_vk_shift then shift := true; { set shift active }
         if msg.wparam = sc_vk_control then
            cntrl := true; { set control active }
         ctlevent { process control character }

      end else if msg.message = sc_wm_keyup then begin

         if msg.wparam = sc_vk_shift then
            shift := false; { set shift inactive }
         if msg.wparam = sc_vk_control then
            cntrl := false { set control inactive }

      end else if (msg.message = sc_wm_quit) or (msg.message = sc_wm_close) then
         begin

         er.etype := etterm; { set terminate }
         fend := true; { set end of program ordered }
         keep := true { set keep event }

      end else if (msg.message = sc_wm_mousemove) or
                  (msg.message = sc_wm_lbuttondown) or
                  (msg.message = sc_wm_lbuttonup) or
                  (msg.message = sc_wm_mbuttondown) or
                  (msg.message = sc_wm_mbuttonup) or
                  (msg.message = sc_wm_rbuttondown) or
                  (msg.message = sc_wm_rbuttonup) then begin

         mouseevent; { mouse event }
         mouseupdate { check any mouse details need processing }

      end else if msg.message = sc_wm_timer then begin

         { check its a standard timer }
         if (msg.wparam > 0) and (msg.wparam <= maxtim) then begin

            er.etype := ettim; { set timer event occurred }
            er.timnum := msg.wparam; { set what timer }
            keep := true { set keep event }

         end else if msg.wparam = frmtim then begin { its the framing timer }

            er.etype := etframe; { set frame event occurred }
            keep := true { set keep event }

         end

      end else if (msg.message = sc_mm_joy1move) or
                  (msg.message = sc_mm_joy2move) or
                  (msg.message = sc_mm_joy1zmove) or
                  (msg.message = sc_mm_joy2zmove) then begin

         er.etype := etjoymov; { set joystick moved }
         { set what joystick }
         if (msg.message = sc_mm_joy1move) or 
            (msg.message = sc_mm_joy1zmove) then er.mjoyn := 1
         else er.mjoyn := 2;
         { Set all variables to default to same. This way, only the joystick
           axes that are actually set by the message are registered. }
         if (msg.message = sc_mm_joy1move) or 
            (msg.message = sc_mm_joy1zmove) then begin

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
         if (msg.message = sc_mm_joy1move) or 
            (msg.message = sc_mm_joy2move) then sc_crkmsg(msg.lparam, y, x)
         { For z axis, get a single variable. }
         else z := msg.lparam and $ffff;
         { We perform thresholding on the joystick right here, which is
           limited to 255 steps (same as joystick hardware. find joystick
           diffs and update }
         if (msg.message = sc_mm_joy1move) or 
            (msg.message = sc_mm_joy1zmove) then begin

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
   
      end else if (msg.message = sc_mm_joy1buttondown) or
                  (msg.message = sc_mm_joy2buttondown) or
                  (msg.message = sc_mm_joy1buttonup) or
                  (msg.message = sc_mm_joy2buttonup) then joymes
      else if msg.message = sc_wm_command then begin

         if msg.lparam <> 0 then begin { it's a widget }

            wp := fndwig(win, msg.wparam and $ffff); { find the widget }
            if wp = nil then error(esystem); { should be in the list }
            nm := msg.wparam div $10000; { get notification message }
            case wp^.typ of { widget type }

               wtbutton: begin { button }

                  if nm = sc_bn_clicked then begin
   
                     er.etype := etbutton; { set button assert event }
                     er.butid := wp^.id; { get widget id }
                     keep := true { set keep event }

                  end

               end;
               wtcheckbox: begin { checkbox }
               
                  er.etype := etchkbox; { set checkbox select event }
                  er.ckbxid := wp^.id; { get widget id }
                  keep := true { set keep event }

               end;
               wtradiobutton: begin { radio button }

                  er.etype := etradbut; { set checkbox select event }
                  er.radbid := wp^.id; { get widget id }
                  keep := true { set keep event }

               end;
               wtgroup: ; { group box, gives no messages }
               wtbackground: ; { background box, gives no messages }
               wtscrollvert: ; { scrollbar, gives no messages }
               wtscrollhoriz: ; { scrollbar, gives no messages }
               wteditbox: ; { edit box, requires no messages }
               wtlistbox: begin { list box }

                  if nm = sc_LBN_DBLCLK then begin

                     unlockmain; { end exclusive access }
                     r := sc_sendmessage(wp^.han, sc_lb_getcursel, 0, 0);
                     lockmain; { start exclusive access }
                     if r = -1 then error(esystem); { should be a select }
                     er.etype := etlstbox; { set list box select event }
                     er.lstbid := wp^.id; { get widget id }
                     er.lstbsl := r+1; { set selection }
                     keep := true { set keep event }

                  end

               end;
               wtdropbox: begin { drop box }

                  if nm = sc_CBN_SELENDOK then begin

                     unlockmain; { end exclusive access }
                     r := sc_sendmessage(wp^.han, sc_cb_getcursel, 0, 0);
                     lockmain; { start exclusive access }
                     if r = -1 then error(esystem); { should be a select }
                     er.etype := etdrpbox; { set list box select event }
                     er.drpbid := wp^.id; { get widget id }
                     er.drpbsl := r+1; { set selection }
                     keep := true { set keep event }

                  end

               end;
               wtdropeditbox: begin { drop edit box }

                  if nm = sc_CBN_SELENDOK then begin

                     er.etype := etdrebox; { set list box select event }
                     er.drebid := wp^.id; { get widget id }
                     keep := true { set keep event }

                  end

               end;
               wtslidehoriz: ; 
               wtslidevert: ; 
               wtnumselbox: ;

            end

         end else begin { it's a menu select }

            er.etype := etmenus; { set menu select event }
            er.menuid := msg.wparam and $ffff; { get menu id }
            keep := true { set keep event }

         end

      end else if msg.message = sc_wm_vscroll then begin

         v := msg.wparam and $ffff; { find subcommand }
         if (v = sc_sb_thumbtrack) or
            (v = sc_sb_lineup) or (v = sc_sb_linedown) or
            (v = sc_sb_pageup) or (v = sc_sb_pagedown) then begin 

            { position request }
            wp := fndwighan(win, msg.lparam); { find widget tracking entry }
            if wp = nil then error(esystem); { should have been found }
            if wp^.typ = wtscrollvert then begin { scroll bar }

               if v = sc_sb_lineup then begin

                  er.etype := etsclull; { line up }
                  er.sclulid := wp^.id

               end else if v = sc_sb_linedown then begin

                  er.etype := etscldrl; { line down }
                  er.scldlid := wp^.id

               end else if v = sc_sb_pageup then begin

                  er.etype := etsclulp; { page up }
                  er.sclupid := wp^.id

               end else if v = sc_sb_pagedown then begin

                  er.etype := etscldrp; { page down }
                  er.scldpid := wp^.id

               end else begin

                  er.etype := etsclpos; { set scroll position event }
                  er.sclpid := wp^.id; { set widget id }
                  f := msg.wparam div $10000; { get current position to float }
                  { clamp to maxint }
                  if f*maxint/(255-wp^.siz) > maxint then er.sclpos := maxint
                  else er.sclpos := round(f*maxint/(255-wp^.siz));
                  {er.sclpos := msg.wparam div 65536*$800000} { get position }

               end;
               keep := true { set keep event }

            end else if wp^.typ = wtslidevert then begin { slider }

               er.etype := etsldpos; { set scroll position event }
               er.sldpid := wp^.id; { set widget id }
               { get position }
               if v = sc_sb_thumbtrack then { message includes position }
                  er.sldpos := msg.wparam div 65536*(maxint div 100)
               else begin { must retrive the position by message }

                  unlockmain; { end exclusive access }
                  r := sc_sendmessage(wp^.han, sc_tbm_getpos, 0, 0);
                  lockmain; { start exclusive access }
                  er.sldpos := r*(maxint div 100) { set position }

               end;
               keep := true { set keep event }
               
            end else error(esystem) { should be one of those }

         end

      end else if msg.message = sc_wm_hscroll then begin

         v := msg.wparam and $ffff; { find subcommand }
         if (v = sc_sb_thumbtrack) or 
            (v = sc_sb_lineleft) or (v = sc_sb_lineright) or
            (v = sc_sb_pageleft) or (v = sc_sb_pageright) then begin 

            { position request }
            wp := fndwighan(win, msg.lparam); { find widget tracking entry }
            if wp = nil then error(esystem); { should have been found }
            if wp^.typ = wtscrollhoriz then begin { scroll bar }

               if v = sc_sb_lineleft then begin

                  er.etype := etsclull; { line up }
                  er.sclulid := wp^.id

               end else if v = sc_sb_lineright then begin

                  er.etype := etscldrl; { line down }
                  er.scldlid := wp^.id

               end else if v = sc_sb_pageleft then begin

                  er.etype := etsclulp; { page up }
                  er.sclupid := wp^.id

               end else if v = sc_sb_pageright then begin

                  er.etype := etscldrp; { page down }
                  er.scldpid := wp^.id

               end else begin

                  er.etype := etsclpos; { set scroll position event }
                  er.sclpid := wp^.id; { set widget id }
                  er.sclpos := msg.wparam div 65536*$800000 { get position }

               end;
               keep := true { set keep event }

            end else if wp^.typ = wtslidehoriz then begin { slider }

               er.etype := etsldpos; { set scroll position event }
               er.sldpid := wp^.id; { set widget id }
               { get position }
               if v = sc_sb_thumbtrack then { message includes position }
                  er.sldpos := msg.wparam div 65536*(maxint div 100)
               else begin { must retrive the position by message }

                  unlockmain; { end exclusive access }
                  r := sc_sendmessage(wp^.han, sc_tbm_getpos, 0, 0);
                  lockmain; { start exclusive access }
                  er.sldpos := r*(maxint div 100) { set position }

               end;
               keep := true { set keep event }

            end else error(esystem) { should be one of those }

         end

      end else if msg.message = sc_wm_notify then begin

         wp := fndwig(win, msg.wparam); { find widget tracking entry }
         if wp = nil then error(esystem); { should have been found }
         i2nmhdrp.i := msg.lparam; { convert lparam to record pointer }
         v := i2nmhdrp.rp^.code; { get code }
         { no, I don't know why this works, or what the -2 code is. Tab controls
           are giving me multiple indications, and the -2 code is more reliable
           as a selection indicator. }
         if v = -2 {sc_tcn_selchange} then begin

            unlockmain; { end exclusive access }
            r := sc_sendmessage(wp^.han, sc_tcm_getcursel, 0, 0);
            lockmain; { start exclusive access }
            er.etype := ettabbar; { set tab bar type }
            er.tabid := wp^.id; { set id }
            er.tabsel := r+1; { set tab number }
            keep := true { keep event }

         end

      end else if msg.message = umeditcr then begin

         wp := fndwig(win, msg.wparam); { find widget tracking entry }
         if wp = nil then error(esystem); { should have been found }
         er.etype := etedtbox; { set edit box complete event }
         er.edtbid := wp^.id; { get widget id }
         keep := true { set keep event }

      end else if msg.message = umnumcr then begin

         wp := fndwig(win, msg.wparam); { find widget tracking entry }
         if wp = nil then error(esystem); { should have been found }
         er.etype := etnumbox; { set number select box complete event }
         er.numbid := wp^.id; { get widget id }
         er.numbsl := msg.lparam; { set number selected }
         keep := true { set keep event }

      end

   end

end;

procedure sigevt;

begin

   if (msg.message = sc_wm_quit) or (msg.message = sc_wm_close) then begin

      er.etype := etterm; { set terminate }
      fend := true; { set end of program ordered }
      keep := true { set keep event }

   end

end;

begin { ievent }

   { Windows gdi caches, which can cause written graphics to pause uncompleted
     while we await user input. This next causes a sync-up. }
   b := sc_gdiflush;
   { check there are events waiting on the input queue }
   if opnfil[ifn]^.evt <> nil then with opnfil[ifn]^ do begin

      { We pick one, and only one, event off the input queue. The messages are
        removed in fifo order. }
      ep := evt^.next; { index the entry to dequeue }
      er := ep^.evt; { get the queued event }
      { if this is the last entry in the queue, just empty the list }
      if ep^.next = ep then evt := nil
      else begin { not last entry }

         ep^.next^.last := ep^.last; { link next to last }
         ep^.last^.next := ep^.next; { link last to next }
         puteqe(ep) { release entry }

      end

   end else repeat

      keep := false; { set don't keep by default }
      { get next message }
      getmsg(msg);
      { get the logical output file from Windows handle }
      ofn := hwn2lfn(msg.hwnd);
{;prtstr('ofn: '); prtnum(ofn, 8, 16); prtstr(' '); prtmsg(msg);}
      { A message can have a window associated with it, or be sent anonymously.
        Anonymous messages are typically intertask housekeeping signals. }
      if ofn > 0 then begin

         win := lfn2win(ofn); { index window from output file }
         er.winid := filwin[ofn]; { set window id }
         winevt; { process messsage }
         if not keep then sigevt { if not found, try intertask signal }

      end else sigevt; { process signal }
      if keep and (ofn > 0) then begin { we have an event, and not main }

         { check and error if no logical input file is linked to this output
           window }
         if opnfil[ofn]^.inl = 0 then error(esystem);
         if opnfil[ofn]^.inl <> ifn then begin
             
            { The event is not for the input agent that is calling us. We will
              queue the message up on the input file it is intended for. Why
              would this happen ? Only if the program is switching between
              input channels, since each input is locked to a task. }
            enqueue(opnfil[opnfil[ofn]^.inl]^.evt, er);
            { Now, keep receiving events until one matches the input file we
              were looking for. }
            keep := false

         end

      end

   until keep { until an event we want occurs }

end; { ievent }

{ external event interface }

procedure event(var f: text; var er: evtrec);

begin

   lockmain; { start exclusive access }
   { get logical input file number for input, and get the event for that. }
   ievent(txt2lfn(f), er); { process event }
   unlockmain { end exclusive access }

end;

overload procedure event(var er: evtrec);

begin

   lockmain; { start exclusive access }
   { get logical input file number for input, and get the event for that. }
   ievent(inpfil, er); { process event }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Wait for intratask message

Waits for the given intratask message. Discards any other messages or
intratask messages. The im is returned back to free, which matches the common
use of im to just return the entry as acknowledgement.

******************************************************************************}

procedure waitim(m: imcode; var ip: imptr);

var done: boolean; { done flag }
    msg:  sc_msg;  { message }

begin

   done := false; { set not done }
   repeat 

      igetmsg(msg); { get next message }
      if msg.message = umim then begin { receive im }
  
         ip := int2itm(msg.wparam); { get im pointer }
         if ip^.im = m then done := true; { found it }
         putitm(ip) { release im entry }

      end

   until done { until message found }

end;

{******************************************************************************

Timer handler procedure

Called by the callback thunk set with timesetevent. We are given a user word
of data, in which we multiplex the two peices of data we need, the logical
file number for the window file, from which we get the windows handle, and
the timer number.

With that data, we then post a message back to the queue, containing the
gralib number of the timer that went off.

The reason we multiplex the logical file number is because the windows handle,
which we need, has a range determined by Windows, and we have to assume it
occupies a full word. The alternatives to multiplexing were to have the timer
callback thunk be customized, which is a non-standard solution, or use one
of the other parameters Windows passes to this function, which are not
documented.

******************************************************************************}

procedure timeout(id, msg, usr, dw1, dw2: integer);

var fn: ss_filhdl; { logical file number }
    wh: integer;   { window handle }

begin

   lockmain; { start exclusive access }
   refer(id, dw1, dw2, msg); { not used }
   fn := usr div maxtim; { get lfn multiplexed in user data }
   { Validate it, but do nothing if wrong. We just don't want to crash on
     errors here. }
   if (fn >= 1) and (fn <= ss_maxhdl) then { valid lfn }
      if opnfil[fn] <> nil then { file is defined }
         if opnfil[fn]^.win <> nil then begin { file has window context }

      wh := opnfil[fn]^.win^.winhan; { get window handle }
      unlockmain; { end exclusive access }
      putmsg(wh, sc_wm_timer, usr mod maxtim { multiplexed timer number}, 0)

   end else unlockmain { end exclusive access }

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

procedure itimer(win: winptr;    { file to send event to }
                 lf:  ss_filhdl; { logical file number }
                 i:   timhan;    { timer handle }
                 t:   integer;   { number of tenth-milliseconds to run }
                 r:   boolean);  { timer is to rerun after completion }

var tf: integer; { timer flags }
    mt: integer; { millisecond time }

begin

   with win^ do begin { in window context }

      if (i < 1) or (i > maxtim) then error(etimnum); { bad timer number }
      mt := t div 10; { find millisecond time }
      if mt = 0 then mt := 1; { fell below minimum, must be >= 1 }
      { set flags for timer }
      tf := sc_time_callback_function or sc_time_kill_synchronous;
      { set repeat/one shot status }
      if r then tf := tf or sc_time_periodic
      else tf := tf or sc_time_oneshot;
      { We need both the timer number, and the window number in the handler,
        but we only have a single callback parameter available. So we mux
        them together in a word. }
      timers[i].han := 
         sc_timesetevent(mt, 0, timeout, lf*maxtim+i, tf);
      if timers[i].han = 0 then error(etimacc); { no timer available }
      timers[i].rep := r; { set timer repeat flag }
      { should check and return an error }

   end

end;

procedure timer(var f: text;     { file to send event to }
                    i: timhan;   { timer handle }
                    t: integer;  { number of tenth-milliseconds to run }
                    r: boolean); { timer is to rerun after completion }

var win: winptr;  { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { index output file }
   itimer(win, txt2lfn(f), i, t, r); { execute }
   unlockmain { end exclusive access }

end;

overload procedure timer(i: timhan;   { timer handle }
                t: integer;  { number of tenth-milliseconds to run }
                r: boolean); { timer is to rerun after completion }

var win: winptr;  { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { index standard output file }
   itimer(win, outfil, i, t, r); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

******************************************************************************}

procedure ikilltimer(win: winptr;  { file to kill timer on }
                     i:   timhan); { handle of timer }

var r: sc_mmresult; { return value }

begin

   with win^ do begin { in window context }

      if (i < 1) or (i > maxtim) then error(etimnum); { bad timer number }
      r := sc_timekillevent(timers[i].han); { kill timer }
      if r <> 0 then error(etimacc) { error }

   end

end;

procedure killtimer(var f: text;    { file to kill timer on }
                        i: timhan); { handle of timer }

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { index output file }
   ikilltimer(win, i); { execute }
   unlockmain { end exclusive access }

end;

overload procedure killtimer(i: timhan); { handle of timer }

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { index output file }
   ikilltimer(win, i); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

******************************************************************************}

procedure iframetimer(win: winptr; lf: ss_filhdl; e: boolean);

begin

   with win^ do begin { in window context }

      if e then begin { enable framing timer }

         if not frmrun then begin { it is not running }

            { set timer to run, 17ms }
            frmhan := sc_timesetevent(17, 0, timeout, lf*maxtim+frmtim,
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

   end

end;

procedure frametimer(var f: text; e: boolean);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { index output file }
   iframetimer(win, txt2lfn(f), e); { execute }
   unlockmain { end exclusive access }

end;

overload procedure frametimer(e: boolean);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { index output file }
   iframetimer(win, outfil, e); { execute }
   unlockmain { end exclusive access }

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

   fautohold := e { set new state of autohold }

end;

{******************************************************************************

Return number of mice

Returns the number of mice implemented. Windows supports only one mouse.

******************************************************************************}

function mouse(var f: text): mounum;

var rv: integer; { return value }

begin

   refer(f); { stub out }
   rv := sc_getsystemmetrics(sc_sm_mousepresent); { find mouse present }
   mouse := ord(rv <> 0) { set single mouse }

end;

overload function mouse: mounum;

var rv: integer; { return value }

begin

   rv := sc_getsystemmetrics(sc_sm_mousepresent); { find mouse present }
   mouse := ord(rv <> 0) { set single mouse }

end;

{******************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

******************************************************************************}

function mousebutton(var f: text; m: mouhan): moubut;

begin

   refer(f); { stub out }
   if m <> 1 then error(einvhan); { bad mouse number }
   mousebutton := 
      sc_getsystemmetrics(sc_sm_cmousebuttons) { find mouse buttons }

end;

overload function mousebutton(m: mouhan): moubut;

begin

   if m <> 1 then error(einvhan); { bad mouse number }
   mousebutton := 
      sc_getsystemmetrics(sc_sm_cmousebuttons) { find mouse buttons }

end;

{******************************************************************************

Return number of joysticks

Return number of joysticks attached.

******************************************************************************}

function joystick(var f: text): joynum;

var win: winptr; { window pointer }

begin
  
   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      joystick := numjoy; { two }
   unlockmain { end exclusive access }

end;

overload function joystick: joynum;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      joystick := numjoy; { two }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

******************************************************************************}

function joybutton(var f: text; j: joyhan): joybtn;

var jc:  sc_joycaps; { joystick characteristics data }
    win: winptr;     { window pointer }
    nb:  integer;    { number of buttons }
    r:   integer;    { return value }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do begin { in window context }

      if (j < 1) or (j > numjoy) then error(einvjoy); { bad joystick id }
      r := sc_joygetdevcaps(j-1, jc, sc_joycaps_len);
      if r <> 0 then error(ejoyqry); { could not access joystick }
      nb := jc.wnumbuttons; { set number of buttons }
      { We don't support more than 4 buttons. }
      if nb > 4 then nb := 4;
      joybutton := nb { set number of buttons }

   end;
   unlockmain { end exclusive access }

end;

overload function joybutton(j: joyhan): joybtn;

var jc:  sc_joycaps; { joystick characteristics data }
    win: winptr;     { window pointer }
    nb:  integer;    { number of buttons }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do begin { in window context }

      if (j < 1) or (j > numjoy) then error(einvjoy); { bad joystick id }
      r := sc_joygetdevcaps(j-1, jc, sc_joycaps_len);
      if r <> 0 then error(ejoyqry); { could not access joystick } 
      nb := jc.wnumbuttons; { set number of buttons }
      { We don't support more than 4 buttons. }
      if nb > 4 then nb := 4;
      joybutton := nb { set number of buttons }

   end;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

******************************************************************************}

function ijoyaxis(win: winptr; j: joyhan): joyaxn;

var jc: sc_joycaps; { joystick characteristics data }
    na: integer;    { number of axes }

begin

   with win^ do begin { in window context }

      if (j < 1) or (j > numjoy) then error(einvjoy); { bad joystick id }
      r := sc_joygetdevcaps(j-1, jc, sc_joycaps_len);
      if r <> 0 then error(ejoyqry); { could not access joystick } 
      na := jc.wnumaxes; { set number of axes }
      { We don't support more than 3 axes. }
      if na > 3 then na := 3;
      ijoyaxis := na { set number of axes }

   end;

end;

function joyaxis(var f: text; j: joyhan): joyaxn;

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   joyaxis := ijoyaxis(win, j); { find joystick axes }
   unlockmain { end exclusive access }

end;

overload function joyaxis(j: joyhan): joyaxn;

var win: winptr;     { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   joyaxis := ijoyaxis(win, j); { find joystick axes }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

******************************************************************************}

procedure isettabg(win: winptr; t: integer);

var i, x: 1..maxtab; { tab index }

begin

   with win^, screens[curupd]^ do begin { window, screen context }

      if auto and (((t-1) mod charspace) <> 0) then
         error(eatotab); { cannot perform with auto on }
      if (t < 1) or (t > maxxg) then error(einvtab); { bad tab position }
      { find free location or tab beyond position }
      i := 1;
      while (i < maxtab) and (tab[i] <> 0) and (t > tab[i]) do i := i+1;
      if (i = maxtab) and (t < tab[i]) then error(etabful); { tab table full }
      if t <> tab[i] then begin { not the same tab yet again }

         if tab[maxtab] <> 0 then error(etabful); { tab table full }
         { move tabs above us up }
         for x := maxtab downto i+1 do tab[x] := tab[x-1];
         tab[i] := t { place tab in order }    
 
      end

   end

end;

procedure settabg(var f: text; t: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      isettabg(win, t); { translate to graphical call }
   unlockmain { end exclusive access }

end;

overload procedure settabg(t: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      isettabg(win, t); { translate to graphical call }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set tab

Sets a tab at the indicated collumn number.

******************************************************************************}

procedure settab(var f: text; t: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      isettabg(win, (t-1)*charspace+1); { translate to graphical call }
   unlockmain { end exclusive access }

end;

overload procedure settab(t: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      isettabg(win, (t-1)*charspace+1); { translate to graphical call }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

******************************************************************************}

procedure irestabg(win: winptr; t: integer);

var i:  1..maxtab; { tab index }
    ft: 0..maxtab; { found tab }

begin

   with win^, screens[curupd]^ do begin { in windows, screen context }

      if (t < 1) or (t > maxxg) then error(einvtab); { bad tab position }
      { search for that tab }
      ft := 0; { set not found }
      for i := 1 to maxtab do if tab[i] = t then ft := i; { found }
      if ft <> 0 then begin { found the tab, remove it }

         { move all tabs down to gap out }
         for i := ft to maxtab-1 do tab[i] := tab[i+1];
         tab[maxtab] := 0 { clear any last tab }

      end

   end

end;

procedure restabg(var f: text; t: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      irestabg(win, t); { translate to graphical call }
   unlockmain { end exclusive access }

end;

overload procedure restabg(t: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      irestabg(win, t); { translate to graphical call }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

******************************************************************************}

procedure restab(var f: text; t: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      irestabg(win, (t-1)*charspace+1); { translate to graphical call }
   unlockmain { end exclusive access }

end;

overload procedure restab(t: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      irestabg(win, (t-1)*charspace+1); { translate to graphical call }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

******************************************************************************}

procedure clrtab(var f: text);

var i:   1..maxtab;
    win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      for i := 1 to maxtab do screens[curupd]^.tab[i] := 0;
   unlockmain { end exclusive access }

end;

overload procedure clrtab;

var i:   1..maxtab;
    win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      for i := 1 to maxtab do screens[curupd]^.tab[i] := 0;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

******************************************************************************}

function funkey(var f: text): funky;

begin

   refer(f); { stub out }
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

Now edits multiple buffers in multiple windows. Each event is received from
ievent, then dispatched to the buffer whose window it belongs to. When a
buffer is completed by hitting "enter", then we return.

******************************************************************************}

procedure readline(fn: ss_filhdl);

var er:  evtrec; { event record } 
    win: winptr; { window pointer }

begin

   repeat { get line characters }

      { get events until an "interesting" event occurs }
      repeat ievent(fn, er) 
         until er.etype in [etchar, etenter, etterm, etdelcb];
      win := lfn2win(xltwin[er.winid]); { get the window from the id }
      with win^ do begin { in windows context }

         { if the event is line enter, place carriage return code, 
           otherwise place real character. note that we emulate a
           terminal and return cr only, which is handled as appropriate
           by a higher level. if the event is program terminate, then we
           execute an organized halt }
         case er.etype of { event }

            etterm:  goto 88; { halt program }
            etenter: begin { line terminate }

               inpbuf[inpptr] := '\cr'; { return cr }
               plcchr(win, '\cr'); { output newline sequence }
               plcchr(win, '\lf');
               inpend := true { set line was terminated }

            end;
            etchar: begin { character }

               if inpptr < maxlin then begin

                  inpbuf[inpptr] := er.char; { place real character }
                  plcchr(win, er.char) { echo the character }

               end;
               if inpptr < maxlin then inpptr := inpptr+1 { next character }

            end;
            etdelcb: begin { delete character backwards }

               if inpptr > 1 then begin { not at extreme left }

                  plcchr(win, '\bs'); { backspace, spaceout then backspace again }
                  plcchr(win, ' ');
                  plcchr(win, '\bs');
                  inpptr := inpptr-1 { back up pointer }

               end

            end

         end

      end

   until er.etype = etenter; { until line terminate }
   { note we still are indexing the last window that gave us the enter }
   win^.inpptr := 1 { set 1st position on active line }

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

Get program name

Retrieves the program name off the Win95 command line. This routine is very
dependent on the structure of a windows command line. The name is enclosed
in quotes, has a path, and is terminated by '.'.
A variation on this is a program that executes us directly may not have the
quotes, and we account for this. I have also accounted for there being no
extention, just in case these kinds of things turn up.

Notes:

1. Win98 drops the quotes.
2. Win XP/2000 drops the path and the extention.

******************************************************************************}

procedure getpgm;

var l:  integer; { length of program name string }
    cp: pstring; { holds command line pointer }
    i:  integer; { index for string }
    s:  integer; { index save }

{ This causes CE to fail }
{ fixed fini: string = 'Finished - ';} { termination message }

{ termination message }

fixed fini: packed array [1..11] of char = 'Finished - ';

{ check next command line character }

function chknxt: char;

var c: char;

begin

   if i > max(cp^) then c := ' ' else c := cp^[i];
   chknxt := c

end;

begin

   cp := sc_getcommandline; { get command line }
   i := 1; { set 1st character }
   if cp^[1] = '"' then i := 2; { set character after quote }
   { find last '\' in quoted section }
   s := 0; { flag not found }
   while (chknxt <> '"') and (chknxt <> ' ') and (i < max(cp^)) do
      begin if chknxt = '\\' then s := i; i := i+1 end;
   s := s+1; { skip '\' }
   i := s; { index 1st character }   
   { count program name length }
   l := 0; { clear length }
   while (chknxt <> '.') and (chknxt <> ' ') do begin l := l+1; i := i+1 end;
   new(pgmnam, l); { get a string for that }
   i := s; { index 1st character }
   while (chknxt <> '.') and (chknxt <> ' ') do begin

      pgmnam^[i-s+1] := chknxt; { place character }
      i := i+1 { next character }

   end;
   { form the name for termination }
   new(trmnam, max(pgmnam^)+11); { get the holding string }
   for i := 1 to 11 do trmnam^[i] := fini[i]; { place first part }
   { place program name }
   for i := 1 to max(pgmnam^) do trmnam^[i+11] := pgmnam^[i]

end;

{******************************************************************************

Sort font list

Sorts the font list for alphabetical order, a-z. The font list does not need
to be in a particular order (and indeed, can't be absolutely in order, because
of the first 4 reserved entries), but sorting it makes listings neater if a
program decides to dump the font names in order.

******************************************************************************}

procedure sortfont(var fp: fontptr);

var nl, p, c, l: fontptr;

function gtr(view d, s: string): boolean;

var i:      integer; { index for string }
    l:      integer; { common region length }
    r:      boolean; { result save }

begin

   { check case where one string is null, and compare lengths if so }
   if (max(s) = 0) or (max(d) = 0) then r := max(d) < max(s)
   else begin

      { find the shorter of the strings }
      if max(s) > max(d) then l := max(d) else l := max(s);
      { compare the overlapping region }
      i := 1; { set 1st character }
      { find first non-matching character }
      while (i < l) and (lcase(s[i]) = lcase(d[i])) do i := i+1;
      { check not equal, and return status based on that character }
      if lcase(s[i]) <> lcase(d[i]) then r := lcase(d[i]) < lcase(s[i])
      { if the entire common region matched, then we base the result on
        length }
      else r := max(d) < max(s)

   end;
   gtr := r { return match status }

end;

begin

   nl := nil; { clear destination list }
   while fp <> nil do begin { insertion sort }

      p := fp; { index top }
      fp := fp^.next; { remove that }
      p^.next := nil; { clear next }
      c := nl; { index new list }
      l := nil; { clear last }
      while c <> nil do begin { find insertion point }

         { compare strings }
         if gtr(p^.fn^, c^.fn^) then
            c := nil { terminate }
         else begin

            l := c; { set last }
            c := c^.next { advance }

         end

      end;
      if l = nil then begin

         { if no last, insert to start }
         p^.next := nl;
         nl := p

      end else begin

         { insert to middle/end }
         p^.next := l^.next; { set next }
         l^.next := p { link to last }

      end

   end;
   fp := nl { place result }

end;

{******************************************************************************

Font list callback

This routine is called by windows when we register it from getfonts. It is
called once for each installed font. We ignore non-TrueType fonts, then place
the found fonts on the global fonts list in no particular order.

Note that OpenType fonts are also flagged as TrueType fonts.

We remove any "bold", "italic" or "oblique" descriptor word from the end of
the font string, as these are attributes, not part of the name.

******************************************************************************}

function enumfont(var lfd: sc_enumlogfontex; var pfd: sc_newtextmetricex;
                  ft: sc_dword; ad: sc_lparam): boolean;

var fp:   fontptr; { pointer to font entry }
    c, i: integer; { indexes }

{ get the last word in the font name }

procedure getlst(view s: string; var d: string);

var i, x: integer;

begin

   clears(d); { clear result }
   { find end }
   i := 1;
   while s[i] <> chr(0) do i := i+1;
   if i > 1 then begin

      i := i-1; { back up to last character }
      { back up to non-space }
      while (s[i] = ' ') and (i > 1) do i := i-1;
      if s[i] <> ' ' then begin { at end of word }

         { back up to space }
         while (s[i] <> ' ') and (i > 1) do i := i-1;
         if s[i] = ' ' then begin { at start of word }

            i := i+1; { index start of word }
            x := 1; { index start of output }
            while (s[i] <> chr(0)) and (s[i] <> ' ') do begin
           
               d[x] := s[i]; { transfer character }
               i := i+1; { next }
               x := x+1
           
            end

         end

      end

   end

end;

{ clear last word in font name }

procedure clrlst(var s: string);

var i: integer;

begin

   { find end }
   i := 1;
   while s[i] <> chr(0) do i := i+1;
   if i > 1 then begin { string isn't null }

      i := i-1; { back up to last character }
      { back up to non-space }
      while (s[i] = ' ') and (i > 1) do i := i-1;
      if s[i] <> ' ' then begin { at end of word }

         { back up to space }
         while (s[i] <> ' ') and (i > 1) do i := i-1;
         if s[i] = ' ' then begin { at start of word }

            { back up to non-space }
            while (s[i] = ' ') and (i > 1) do i := i-1;
            if s[i] <> ' ' then { at end of word before that }
               s[i+1] := chr(0) { now terminate string there }

         end

      end

   end

end;

{ replace attribute word }

procedure repatt(var s: string);

var buff: packed array [1..sc_lf_fullfacesize] of char;
    done: boolean;

begin

   repeat

      done := true; { default to no more }
      getlst(s, buff); { get any last word }

      if compp(buff, 'bold') or compp(buff, 'italic') or 
         compp(buff, 'oblique') then begin

         clrlst(s); { clear last word }
         done := false { signal not complete }

      end

   until done

end;
   
begin

   refer(pfd, ad); { unused }
   if (ft and sc_truetype_fonttype <> 0) and
      ((lfd.elflogfont.lfcharset = sc_ansi_charset) or 
       (lfd.elflogfont.lfcharset = sc_symbol_charset) or
       (lfd.elflogfont.lfcharset = sc_default_charset)) then begin

      { ansi character set, record it }
      repatt(lfd.elffullname); { remove any attribute word }
      new(fp); { get new font entry }
      fp^.next := fntlst; { push to list }
      fntlst := fp;
      fntcnt := fntcnt+1; { count font }
      c := 0; { count font name characters }
      while lfd.elffullname[c+1] <> chr(0) do c := c+1;
      if c > 0 then begin { not null }

         new(fp^.fn, c); { get a string of that length }
         for i := 1 to c do fp^.fn^[i] := ascii2chr(ord(lfd.elffullname[i]));
         
      end;
      fp^.fix := (lfd.elflogfont.lfpitchandfamily and 3) = sc_fixed_pitch;
      fp^.sys := false { set not system }

   end;
   enumfont := true { set continue }

end;

{******************************************************************************

Get fonts list

Loads the windows font list. The list is filtered for TrueType/OpenType 
only. We also retrive the fixed pitch status.

Because of the callback arrangement, we have to pass the font list and count
through globals, which are then placed into the proper window.

******************************************************************************}

procedure getfonts(win: winptr);

var r:  integer;
    lf: sc_logfont;

begin

   fntlst := nil; { clear fonts list }
   fntcnt := 0; { clear fonts count }
   lf.lfheight := 0; { use default height }
   lf.lfwidth := 0; { use default width }
   lf.lfescapement := 0; { no escapement }
   lf.lforientation := 0; { orient to x axis }
   lf.lfweight := sc_fw_dontcare; { default weight }
   lf.lfitalic := 0;  { no italic }
   lf.lfunderline := 0; { no underline }
   lf.lfstrikeout := 0; { no strikeout }
   lf.lfcharset := sc_default_charset; { use default characters }
   lf.lfoutprecision := sc_out_default_precis; { use default precision }
   lf.lfclipprecision := sc_clip_default_precis; { use default clipping }
   lf.lfquality := sc_default_quality; { use default quality }
   lf.lfpitchandfamily := 0; { must be zero }
   lf.lffacename[1] := chr(0); { match all typeface names }
   r := sc_enumfontfamiliesex(win^.devcon, lf, enumfont, 0, 0);
   win^.fntlst := fntlst; { place into windows record }
   win^.fntcnt := fntcnt;
   sortfont(win^.fntlst) { sort into alphabetical order }

end;

{******************************************************************************

Remove font from font list

Removes the indicated font from the font list. Does not dispose of the entry.

******************************************************************************}

procedure delfnt(win: winptr; fp: fontptr);

var p: fontptr;

begin

   p := win^.fntlst; { index top of list }
   if win^.fntlst = nil then error(esystem); { should not be null }
   if win^.fntlst = fp then win^.fntlst := fp^.next { gap first entry }
   else begin { mid entry }

      { find entry before ours }
      while (p^.next <> fp) and (p^.next <> nil) do p := p^.next;
      if p^.next = nil then error(esystem); { not found }
      p^.next := fp^.next { gap out }

   end

end;

{******************************************************************************

Search for font by name

Finds a font in the list of fonts. Also matches fixed/no fixed pitch status.

******************************************************************************}

procedure fndfnt(win: winptr; view fn: string; fix: boolean; var fp: fontptr);

var p: fontptr;

begin

   fp := nil; { set no font found }
   p := win^.fntlst; { index top of font list }
   while p <> nil do begin { traverse font list }

      if compp(p^.fn^, fn) and (p^.fix = fix) then fp := p; { found, set }
      p := p^.next { next entry }

   end

end;

{******************************************************************************

Separate standard fonts

Finds the standard fonts, in order, and either moves these to the top of the
table or creates a blank entry.

Note: could also default to style searching for book and sign fonts.

******************************************************************************}

procedure stdfont(win: winptr);

var termfp, bookfp, signfp, techfp: fontptr; { standard font slots }

{ place font entry in list }

procedure plcfnt(fp: fontptr);

begin

   if fp = nil then begin { no entry, create a dummy }

      new(fp); { get a new entry }
      new(fp^.fn, 0); { place empty string }
      fp^.fix := false; { set for cleanlyness }
      fp^.sys := false { set not system }
      
   end;
   fp^.next := win^.fntlst; { push onto list }
   win^.fntlst := fp

end;
     
begin

   { clear font pointers }
   termfp := nil;
   bookfp := nil;
   signfp := nil;
   techfp := nil;
   { set up terminal font. terminal font is set to system default }
   new(termfp); { get a new entry }
   termfp^.fix := true; { set fixed }
   termfp^.sys := true; { set system }
   new(termfp^.fn, 12); { place name }
   termfp^.fn^ := 'System Fixed';
   win^.fntcnt := win^.fntcnt+1; { add to font count }
   { find book fonts }
   fndfnt(win, 'Times New Roman', false, bookfp);
   if bookfp = nil then begin

      fndfnt(win, 'Garamond', false, bookfp);
      if bookfp = nil then begin

         fndfnt(win, 'Book Antiqua', false, bookfp);
         if bookfp = nil then begin
      
            fndfnt(win, 'Georgia', false, bookfp);
            if bookfp = nil then begin

               fndfnt(win, 'Palatino Linotype', false, bookfp);
               if bookfp = nil then fndfnt(win, 'Verdana', false, bookfp)

            end

         end

      end

   end;
   { find sign fonts }

   fndfnt(win, 'Tahoma', false, signfp);
   if signfp = nil then begin

      fndfnt(win, 'Microsoft Sans Serif', false, signfp);
      if signfp = nil then begin

         fndfnt(win, 'Arial', false, signfp);
         if signfp = nil then begin

            fndfnt(win, 'News Gothic MT', false, signfp);
            if signfp = nil then begin

               fndfnt(win, 'Century Gothic', false, signfp);
               if signfp = nil then begin

                  fndfnt(win, 'Franklin Gothic', false, signfp);
                  if signfp = nil then begin

                     fndfnt(win, 'Trebuchet MS', false, signfp);
                     if signfp = nil then fndfnt(win, 'Verdana', false, signfp)

                  end

               end

            end

         end

      end

   end;
   { delete found fonts from the list }
   if bookfp <> nil then delfnt(win, bookfp);
   if signfp <> nil then delfnt(win, signfp);
   { now place the fonts in the list backwards }
   plcfnt(techfp);
   plcfnt(signfp);
   plcfnt(bookfp);
   termfp^.next := win^.fntlst;
   win^.fntlst := termfp

end;

{******************************************************************************

Set window title

Sets the title of the current window.

******************************************************************************}

procedure title(var f: text; view ts: string);

var b:   boolean; { result code }
    win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do begin { in window context }

      { Set title is actually done via a "wm_settext" message to the display
        window, so we have to remove the lock to allow that to be processed.
        Otherwise, setwindowtext will wait for acknoledgement of the message
        and lock us. }
      unlockmain; { end exclusive access }
      { set window title text }
      b := sc_setwindowtext(winhan, ts);
      lockmain { start exclusive access }

   end;
   if not b then winerr; { process windows error }
   unlockmain { end exclusive access }

end;

overload procedure title(view ts: string);

var b:   boolean; { result code }
    win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do begin { in window context }

      { Set title is actually done via a "wm_settext" message to the display
        window, so we have to remove the lock to allow that to be processed.
        Otherwise, setwindowtext will wait for acknoledgement of the message
        and lock us. }
      unlockmain; { end exclusive access }
      { Set window title text }
      b := sc_setwindowtext(winhan, ts);
      lockmain { start exclusive access }

   end;
   if not b then winerr; { process windows error }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Register standard window class

Registers the "stdwin" class. All of gralib's normal windows use the same
class, which has the name "stdwin". This class only needs to be registered
once, and is thereafter referenced by name.

******************************************************************************}

procedure regstd;

var wc: sc_wndclassa; { windows class structure }
    b:  boolean;      { boolean }

begin

   { set windows class to a normal window without scroll bars,
     with a windows procedure pointing to the message mirror.
     The message mirror reflects messages that should be handled
     by the program back into the queue, sending others on to
     the windows default handler }
   wc.style      := sc_cs_hredraw or sc_cs_vredraw or sc_cs_owndc;
   wc.wndproc    := sc_wndprocadr(wndproc);
   wc.clsextra   := 0;
   wc.wndextra   := 0;
   wc.instance   := sc_getmodulehandle_n;
   if wc.instance = 0 then winerr; { process windows error }
   wc.icon       := sc_loadicon_n(sc_idi_application);
   if wc.icon = 0 then winerr; { process windows error }
   wc.cursor     := sc_loadcursor_n(sc_idc_arrow);
   if wc.cursor = 0 then winerr; { process windows error }
   wc.background := sc_getstockobject(sc_white_brush);
   if wc.background = 0 then winerr; { process windows error }
   wc.menuname   := nil;
   wc.classname  := str('stdwin');
   { register that class }
   b := sc_registerclass(wc);
   if not b then winerr { process windows error }

end;

{******************************************************************************

Kill window

Sends a destroy window command to the window. We can't directly kill a window
from the main thread, so we send a message to the window to kill it for us.

******************************************************************************}

procedure kilwin(wh: integer);

var msg: sc_msg; { intertask message }

begin

   stdwinwin := wh; { place window handle }
   { order window to close }
   b := sc_postmessage(dispwin, umclswin, 0, 0);
   if not b then winerr; { process windows error }
   { Wait for window close. } 
   repeat igetmsg(msg) until msg.message = umwincls

end;

{******************************************************************************

Open and present window

Given a windows record, opens and presents the window associated with it. All
of the screen buffer data is cleared, and a single buffer assigned to the
window.

******************************************************************************}

procedure opnwin(fn, pfn: ss_filhdl);
           
var v:     integer;       { used to construct $80000000 value }
    cr:    sc_rect;       { client rectangle holder }
    r:     integer;       { result holder }
    b:     boolean;       { boolean result holder }
    er:    evtrec;        { event holding record }
    ti:    1..10;         { index for repeat array }
    pin:   1..maxpic;     { index for loadable pictures array }
    si:    1..maxcon;     { index for current display screen }
    tm:    sc_textmetric; { true type text metric structure }
    win:   winptr;        { window pointer }
    pwin:  winptr;        { parent window pointer }
    f:     integer;       { window creation flags }
    msg:   sc_msg;        { intertask message }

begin

   win := lfn2win(fn); { get a pointer to the window }
   with win^ do begin { process within window }

      { find parent }
      parlfn := pfn; { set parent logical number }
      if pfn <> 0 then begin

         pwin := lfn2win(pfn); { index parent window }
         parhan := pwin^.winhan { set parent window handle }

      end else parhan := 0; { set no parent }
      mb1 := false; { set mouse as assumed no buttons down, at origin }
      mb2 := false;
      mb3 := false;
      mpx := 1;
      mpy := 1;
      mpxg := 1;
      mpyg := 1;
      nmb1 := false;
      nmb2 := false;
      nmb3 := false;
      nmpx := 1;
      nmpy := 1;
      nmpxg := 1;
      nmpyg := 1;
      shift := false; { set no shift active }
      cntrl := false; { set no control active }
      fcurdwn := false; { set cursor is not down }
      focus := false; { set not in focus }
      joy1xs := 0; { clear joystick saves }
      joy1ys := 0;
      joy1zs := 0;
      joy2xs := 0;
      joy2ys := 0;
      joy2zs := 0;
      numjoy := 0; { set number of joysticks 0 }
      inpptr := 1; { set 1st character }
      inpend := false; { set no line ending }
      frmrun := false; { set framing timer not running }
      bufmod := true; { set buffering on }
      menhan := 0; { set no menu }
      metlst := nil; { clear menu tracking list }
      wiglst := nil; { clear widget list }
      frame := true; { set frame on }
      size := true; { set size bars on }
      sysbar := true; { set system bar on }
      sizests := 0; { clear last size status word }
      { clear timer repeat array }
      for ti := 1 to 10 do begin 

         timers[ti].han := 0; { set no active timer }
         timers[ti].rep := false { set no repeat }

      end;
      { clear loadable pictures table }
      for pin := 1 to maxpic do pictbl[pin].han := 0; 
      for si := 1 to maxcon do screens[si] := nil;
      new(screens[1]); { get the default screen }
      curdsp := 1; { set current display screen }
      curupd := 1; { set current update screen }
      visible := false; { set not visible }
      { now perform windows setup }
      v := $8000000;
      v := v*16;
      { set flags for window create }
      f := sc_ws_overlappedwindow or sc_ws_clipchildren;
      { add flags for child window }
      if parhan <> 0 then f := f or sc_ws_child or sc_ws_clipsiblings;
      { Create the window, using display task. }
      stdwinflg := f;
      stdwinx := v;
      stdwiny := v;
      stdwinw := v;
      stdwinh := v;
      stdwinpar := parhan;
      { order window to start }
      b := sc_postmessage(dispwin, ummakwin, 0, 0);
      if not b then winerr; { process windows error }
      { Wait for window start. } 
      repeat igetmsg(msg) until msg.message = umwinstr;
      winhan := stdwinwin; { get the new handle }
      if winhan = 0 then winerr; { process windows error }

      { Joysticks were captured with the window open. Set status of joysticks.

        Do we need to release and recapture the joysticks each time we gain and
        loose focus ? Windows could have easily outgrown that need by copying
        the joystick messages. This needs testing. }

      numjoy := 0; { clear joystick counter }
      joy1cap := stdwinj1c; { set joystick 1 capture status }
      numjoy := numjoy+ord(joy1cap); { count that }
      joy2cap := stdwinj2c; { set joystick 1 capture status }
      numjoy := numjoy+ord(joy2cap); { count that }

      { create a device context for the window }
      devcon := sc_getdc(winhan); { get device context }
      if devcon = 0 then winerr; { process windows error }
      { set rescalable mode }
      r := sc_setmapmode(devcon, sc_mm_anisotropic);
      if r = 0 then winerr; { process windows error }
      { set non-braindamaged stretch mode }
      r := sc_setstretchbltmode(devcon, sc_halftone);
      if r = 0 then winerr; { process windows error }
      { remove fills }
      r := sc_selectobject(devcon, sc_getstockobject(sc_null_brush));
      if r = -1 then winerr; { process windows error }
      { because this is an "open ended" (no feedback) emulation, we must bring
        the terminal to a known state }
      gfhigh := fheight; { set default font height }
      getfonts(win); { get the global fonts list }
      stdfont(win); { mark the standard fonts }
      gcfont := fntlst; { index top of list as terminal font }
      { set up system default parameters }
      r := sc_selectobject(devcon, sc_getstockobject(sc_system_fixed_font));
      if r = -1 then winerr; { process windows error }
      b := sc_gettextmetrics(devcon, tm); { get the standard metrics }
      if not b then winerr; { process windows error }
      { calculate line spacing }
      linespace := tm.tmheight;
      { calculate character spacing }
      charspace := tm.tmmaxcharwidth;
      { set cursor width }
      curspace := tm.tmavecharwidth; 
      { find screen device parameters for dpm calculations }
      shsize := sc_getdevicecaps(devcon, sc_horzsize); { size x in millimeters }
      svsize := sc_getdevicecaps(devcon, sc_vertsize); { size y in millimeters }
      shres := sc_getdevicecaps(devcon, sc_horzres); { pixels in x }
      svres := sc_getdevicecaps(devcon, sc_vertres); { pixels in y }
      sdpmx := round(shres/shsize*1000); { find dots per meter x }
      sdpmy := round(svres/svsize*1000); { find dots per meter y }
      { find client area size }
      gmaxxg := maxxd*charspace;
      gmaxyg := maxyd*linespace;
      cr.left := 0; { set up desired client rectangle }
      cr.top := 0;
      cr.right := gmaxxg;
      cr.bottom := gmaxyg;
      { find window size from client size }
      b := sc_adjustwindowrectex(cr, sc_ws_overlappedwindow, false, 0);
      if not b then winerr; { process windows error }
      { now, resize the window to just fit our character mode }
      unlockmain; { end exclusive access }
      b := sc_setwindowpos(winhan, 0, 0, 0, cr.right-cr.left, cr.bottom-cr.top,
                           sc_swp_nomove or sc_swp_nozorder);
      if not b then winerr; { process windows error }
{ now handled in winvis }
;if false then begin
      { present the window }
      b := sc_showwindow(winhan, sc_sw_showdefault);
      { send first paint message }
      b := sc_updatewindow(winhan);
;end;
      lockmain; { start exclusive access }
      { set up global buffer parameters }
      gmaxx := maxxd; { character max dimensions }
      gmaxy := maxyd;
      gattr := []; { no attribute }
      gauto := true; { auto on }
      gfcrgb := colnum(black); {foreground black }
      gbcrgb := colnum(white); { background white }
      gcurv := true; { cursor visible }
      gfmod := mdnorm; { set mix modes }
      gbmod := mdnorm;
      goffx := 0;  { set 0 offset }
      goffy := 0;
      gwextx := 1; { set 1:1 extents }
      gwexty := 1; 
      gvextx := 1; 
      gvexty := 1; 
      iniscn(win, screens[1]); { initalize screen buffer }
      restore(win, true); { update to screen }
{ This next is taking needed messages out of the queue, and I don't believe it
  is needed anywmore with display tasking. }
;if false then begin
      { have seen problems with actions being performed before events are pulled
        from the queue, like the focus event. the answer to this is to wait a short
        delay until these messages clear. in fact, all that is really required is
        to reenter the OS so it can do the callback }
      frmhan := sc_timesetevent(10, 0, timeout, fn*maxtim+1,
                                sc_time_callback_function or
                                sc_time_kill_synchronous or
                                sc_time_oneshot);
      if frmhan = 0 then error(etimacc); { no timer available }
      repeat ievent(opnfil[fn]^.inl, er) 
         until (er.etype = ettim) or (er.etype = etterm);
      if er.etype = etterm then goto 88
;end;

   end

end;

{******************************************************************************

Close window

Shuts down, removes and releases a window.

******************************************************************************}

procedure clswin(fn: ss_filhdl);

var r:   integer; { result holder }
    b:   boolean; { boolean result holder }
    win: winptr;  { window pointer }

begin

   win := lfn2win(fn); { get a pointer to the window }
   with win^ do begin { in windows context }

      b := sc_releasedc(winhan, devcon); { release device context }
      if not b then winerr; { process error }
      { release the joysticks }
      if joy1cap then begin

         r := sc_joyreleasecapture(sc_joystickid1);
         if r <> 0 then error(ejoyacc) { error }

      end;
      if joy2cap then begin

         r := sc_joyreleasecapture(sc_joystickid2);
         if r <> 0 then error(ejoyacc) { error }

      end;
      kilwin(winhan) { kill window }

   end

end;

{******************************************************************************

Close window

Closes an open window pair. Accepts an output window. The window is closed, and
the window and file handles are freed. The input file is freed only if no other
window also links it.

******************************************************************************}

procedure closewin(ofn: ss_filhdl);

var ifn: ss_filhdl; { input file id }
    wid: ss_filhdl; { window id }

{ flush and close file }

procedure clsfil(fn: ss_filhdl);

var ep: eqeptr;    { pointer for event containers }
    si: 1..maxcon; { index for screens }

begin

   with opnfil[fn]^ do begin

      { release all of the screen buffers }
      for si := 1 to maxcon do 
         if win^.screens[si] <> nil then dispose(win^.screens[si]);
      dispose(win); { release the window data }
      win := nil; { set not open }
      han := 0;
      inw := false;
      inl := 0;
      while evt <> nil do begin 

         ep := evt; { index top }
         if evt^.next = evt then evt := nil { last entry, clear list }
         else evt := evt^.next; { gap out entry }
         dispose(ep) { release }

      end

   end

end;

function inplnk(fn: ss_filhdl): integer;

var fi: ss_filhdl;    { index for files }
    fc: 0..ss_maxhdl; { counter for files }

begin

   fc := 0; { clear count }
   for fi := 1 to ss_maxhdl do { traverse files }
      if opnfil[fi] <> nil then { entry is occupied }
         if opnfil[fi]^.inl = fn then fc := fc+1; { count the file link }

   inplnk := fc { return result }

end;

begin

   wid := filwin[ofn]; { get window id }
   ifn := opnfil[ofn]^.inl; { get the input file link }
   clswin(ofn); { close the window }
   clsfil(ofn); { flush and close output file }
   { if no remaining links exist, flush and close input file }
   if inplnk(ifn) = 0 then clsfil(ifn);
   filwin[ofn] := 0; { clear file to window translation }
   xltwin[wid] := 0; { clear window to file translation }

end;

{******************************************************************************

Open an input and output pair

Creates, opens and initalizes an input and output pair of files.

******************************************************************************}

procedure openio(ifn, ofn, pfn, wid: ss_filhdl);

begin

   { if output was never opened, create it now }
   if opnfil[ofn] = nil then getfet(opnfil[ofn]);
   { if input was never opened, create it now }
   if opnfil[ifn] = nil then getfet(opnfil[ifn]);
   opnfil[ofn]^.inl := ifn; { link output to input }
   opnfil[ifn]^.inw := true; { set input is window handler }
   { now see if it has a window attached }
   if opnfil[ofn]^.win = nil then begin

      { Haven't already started the main input/output window, so allocate
        and start that. We tolerate multiple opens to the output file. }
      new(opnfil[ofn]^.win); { get a new window record }
      opnwin(ofn, pfn) { and start that up }

   end;
   { check if the window has been pinned to something else }
   if (xltwin[wid] <> 0) and (xltwin[wid] <> ofn) then 
      error(ewinuse); { flag error }
   xltwin[wid] := ofn; { pin the window to the output file }
   filwin[ofn] := wid

end;

{******************************************************************************

Alias file number

Aliases a top level (application program) file number to its syslib equivalent
number. Paslib passes this information down the stack when it opens a file.
Having both the top level number and the syslib equivalent allows us to be
called by the program, as well as interdicting the syslib calls.

******************************************************************************}

procedure filealias(fn, fa: ss_filhdl);

begin

   lockmain; { start exclusive access }
   if r = -1 then winerr; { process windows error }
   { check file has entry }
   if opnfil[fn] = nil then error(einvhan); { invalid handle }
   { check is either normal or window file, and is active }
   { if (opnfil[fn]^.han = 0) and (opnfil[fn]^.win = nil) and 
      not opnfil[fn]^.inw then error(einvhan);} { invalid handle }
   { throw consistentcy check if alias is bad }
   if (fa < 1) or (fa > ss_maxhdl) then error(esystem);
   xltfil[fa] := fn; { place translation entry }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Resolve filename

Resolves header file parameters. If the filename is one of our special window
input or output identifiers, we do nothing. Otherwise, it's passed through to
the lower level.

Files that attach to the input or output side must be opened as standard
files because standard Pascal I/O operations, like read and write, are used
on them. Sending them through paslib allows it to set up normal tables and
associations for the file.

******************************************************************************}

procedure fileresolve(view nm: string; var fs: pstring);

begin

   { check its our special window identifier }
   if not compp(fs^, '_input_window') and 
      not compp(fs^, '_output_window') then begin

      { its one of our specials, just transfer it }
      new(fs, max(nm)); { get space for string }
      fs^ := nm { copy }

   end else ss_old_resolve(nm, fs, sav_resolve) { pass it on }

end;

{******************************************************************************

Open file for read

Opens the file by name, and returns the file handle. If the file is the
"_input" file, then we give it our special handle. Otherwise, the entire
processing of the file is passed onto the system level.
We handle the entire processing of the input file here, by processing the
event queue.

Allows "_debug_in" to override to "_input" for debugging in a console window.

If the input file is "_input_window" then its just the marker for the input
side of a window. We create an entry for it, but otherwise ignore it, since
that will be handled by openwin.

******************************************************************************}

procedure fileopenread(var fn: ss_filhdl; view nm: string);

var fs: pstring; { filename buffer pointer }

begin

   lockmain; { start exclusive access }
   remspc(nm, fs); { remove leading and trailing spaces }
   fn := chksys(fs^); { check it's a system file }
   if fn = inpfil then 
      openio(inpfil, outfil, 0, iowin) { it's the "input" file }
   else begin { not "_input", process pass-on }

      makfil(fn); { find file slot }
      if compp(fs^, '_debug_in') then { open debug file }
         ss_old_openread(opnfil[fn]^.han, '_input', sav_openread)
      else if not compp(fs^, '_input_window') then { open regular }
         ss_old_openread(opnfil[fn]^.han, fs^, sav_openread);

   end;
   dispose(fs); { release temp string }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Open file for write

Opens the file by name, and returns the file handle. If the file is the
"_output" file, then we give it our special handle. Otherwise, the file entire
processing of the file is passed onto the system level.

Allows "_debug_out" to override to "_output" for debugging in a console window.

If the output file is "_output_window" then its just the marker for the output
side of a window. We create an entry for it, but otherwise ignore it, since
that will be handled by openwin.

******************************************************************************}

procedure fileopenwrite(var fn: ss_filhdl; view nm: string);

var fs: pstring; { filename buffer pointer }

begin

   lockmain; { start exclusive access }
   remspc(nm, fs); { remove leading and trailing spaces }
   fn := chksys(fs^); { check it's a system file }
   if fn = outfil then 
      openio(inpfil, outfil, 0, iowin) { its the "output" file }
   else begin { not "_output", process pass-on }

      makfil(fn); { find file slot }
      if compp(fs^, '_debug_out') then { open debug file }
         ss_old_openwrite(opnfil[fn]^.han, '_output', sav_openwrite)
      else if not compp(fs^, '_output_window') then { open regular }
         ss_old_openwrite(opnfil[fn]^.han, fs^, sav_openwrite)

   end;
   dispose(fs); { release temp string }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Close file

Closes the file. The file is closed at the system level, then we remove the
table entry for the file.

A file can be a normal file, the input side of a window, or the output side
of a window. If its normal, it is passed on to the operating system. If it's
the output side of a window, then the window is closed, and the link count of
the associated input file has its link count decremented. If it's an input
side of a window, then it's an error, since the input side is automatically
closed when there are no output windows that reference it.

******************************************************************************}

procedure fileclose(fn: ss_filhdl);

begin

   lockmain; { start exclusive access }
   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   if fn > outfil then begin { transparent file }

      if opnfil[fn]^.win <> nil then closewin(fn) { it's an output window }
      else if opnfil[fn]^.inw then { it's input linked to window }
         error(eclsinw) { cannot directly close this }
      else begin { standard file }

         chkopn(fn); { check valid handle }
         ss_old_close(opnfil[fn]^.han, sav_close); { close at lower level }
         opnfil[fn]^.han := 0 { clear out file table entry }

      end

   end;
   unlockmain { end exclusive access }

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

var i:   integer;   { index for destination }
    l:   integer;   { length left on destination }
    win: winptr;    { pointer to window data }
    ofn: ss_filhdl; { output file handle }

{ find window with non-zero input buffer }

function fndful: ss_filhdl;

var fi: ss_filhdl; { file index }
    ff: ss_filhdl; { found file }

begin

   ff := 0; { set no file found }
   for fi := 1 to ss_maxhdl do if opnfil[fi] <> nil then begin

      if (opnfil[fi]^.inl = fn) and (opnfil[fi]^.win <> nil) then
         { links the input file, and has a window }
         if opnfil[fi]^.win^.inpend then ff := fi; { found one }

   end;

   fndful := ff { return result }

end;

begin

   lockmain; { start exclusive access }
   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   if opnfil[fn]^.inw then begin { process input file }

      i := 1; { set 1st byte of destination }
      l := max(ba); { set length of destination }
      while l > 0 do { while there is space left in the buffer }
            begin { read input bytes }

         { find any window with a buffer with data that points to this input
           file }
         ofn := fndful;
         if ofn = 0 then readline(fn) { none, read a buffer }
         else begin { read characters }

            win := lfn2win(ofn); { get the window }
            with win^ do { in window context }
               while (inpptr > 0) and (l > 0) do begin

               { there is data in the buffer, and we need that data }
               ba[i] := chr2ascii(inpbuf[inpptr]); { get and place next character }
               if inpptr < maxlin then inpptr := inpptr+1; { next }
               { if we have just read the last of that line, then flag buffer
                 empty }
               if ba[i] = chr2ascii('\cr') then begin

                  inpptr := 1; { set 1st character }
                  inpend := false { set no ending }

               end;
               i := i+1; { next character }
               l := l-1 { count characters }

            end

         end

      end
      
   end else begin

      chkopn(fn); { check valid handle }
      ss_old_read(opnfil[fn]^.han, ba, sav_read) { pass to lower level }

   end;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Write file

Outputs to the given file. If the file is the "output" file, then we process
it specially.

******************************************************************************}

procedure filewrite(fn: ss_filhdl; view ba: bytarr);

var i:   integer; { index for destination }
    l:   integer; { length left on destination }
    win: winptr;  { pointer to window data }

begin

   lockmain; { start exclusive access }
   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   if opnfil[fn]^.win <> nil then begin { process window output file }

      win := lfn2win(fn); { index window }
      i := 1; { set 1st byte of source }
      l := max(ba); { set length of source }
      while l > 0 do begin { write output bytes }

         plcchr(win, ascii2chr(ba[i])); { send character to terminal emulator }
         i := i+1; { next character }
         l := l-1 { count characters }

      end
      
   end else begin { standard file }

      chkopn(fn); { check valid handle }
      ss_old_write(opnfil[fn]^.han, ba, sav_write) { pass to lower level }

   end;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Position file

Positions the given file. If the file is one of "input" or "output", we fail
call, as positioning is not valid on a special file.

******************************************************************************}

procedure fileposition(fn: ss_filhdl; p: integer);

begin

   lockmain; { start exclusive access }
   if fn > outfil then { transparent file }
      chkopn(fn); { check valid handle }
   { check operation performed on special file }
   if (fn = inpfil) or (fn = outfil) then error(efilopr);
   ss_old_position(opnfil[fn]^.han, p, sav_position); { pass to lower level }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find location of file

Find the location of the given file. If the file is one of "input" or "output",
we fail the call, as this is not valid on a special file.

******************************************************************************}

function filelocation(fn: ss_filhdl): integer;

begin

   lockmain; { start exclusive access }
   if r = -1 then winerr; { process windows error }
   if fn > outfil then { transparent file }
      chkopn(fn); { check valid handle }
   { check operation performed on special file }
   if (fn = inpfil) or (fn = outfil) then error(efilopr);
   filelocation := 
      ss_old_location(opnfil[fn]^.han, sav_location); { pass to lower level }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find length of file

Find the length of the given file. If the file is one of "input" or "output",
we fail the call, as this is not valid on a special file.

******************************************************************************}

function filelength(fn: ss_filhdl): integer;

begin

   lockmain; { start exclusive access }
   if fn > outfil then { transparent file }
      chkopn(fn); { check valid handle }
   { check operation performed on special file }
   if (fn = inpfil) or (fn = outfil) then error(efilopr);
   { pass to lower level }
   filelength := ss_old_length(opnfil[fn]^.han, sav_length);
   unlockmain { end exclusive access }

end;

{******************************************************************************

Check eof of file

Checks if a given file is at eof. On our special files "input" and "output",
the eof is allways false. On "input", there is no idea of eof on a keyboard
input file. On "output", eof is allways false on a write only file.

******************************************************************************}

function fileeof(fn: ss_filhdl): boolean;

begin

   lockmain; { start exclusive access }
   if fn > outfil then { transparent file }
      chkopn(fn); { check valid handle }
   { check operation performed on special file }
   if (fn = inpfil) or (fn = outfil) then fileeof := false { yes, allways true }
   else fileeof := ss_old_eof(opnfil[fn]^.han, sav_eof); { pass to lower level }
   unlockmain { end exclusive access }

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

procedure iopenwin(var infile, outfile: text; pfn, wid: ss_filhdl);

var ifn, ofn: ss_filhdl; { file logical handles }

begin

   { check valid window handle }
   if (wid < 1) or (wid > ss_maxhdl) then error(einvwin);
   { check if the window id is already in use }
   if xltwin[wid] <> 0 then error(ewinuse); { error }
   { if input is not open, open it now }
   if getlfn(infile) = 0 then begin

      unlockmain; { end exclusive access }
      assign(infile, '_input_window'); { assign to window input }
      reset(infile); { activate it }
      lockmain { start exclusive access }

   end;
   { output should not be open }
   if getlfn(outfile) <> 0 then error(efinuse) { file in use }
   else begin { open it }

      unlockmain; { end exclusive access }
      assign(outfile, '_output_window'); { assign to window output }
      rewrite(outfile); { activate it }
      lockmain { start exclusive access }
      
   end;
   { get our level of the file handles }
   ifn := txt2lfn(infile); { get input file }
   ofn := txt2lfn(outfile); { get output file } 
   { check either input is unused, or is already an input side of a window }
   if opnfil[ifn] <> nil then { entry exists }
      if not opnfil[ifn]^.inw or (opnfil[ifn]^.han <> 0) or 
         (opnfil[ifn]^.win <> nil) then error(einmode); { wrong mode }
   { check output file is in use }
   if opnfil[ofn] <> nil then { entry exists }
      if (opnfil[ofn]^.han <> 0) or (opnfil[ofn]^.win <> nil) or 
          opnfil[ofn]^.inw then error(efinuse); { file in use }
   { establish all logical files and links, translation tables, and open
     window }
   openio(ifn, ofn, pfn, wid)

end;

procedure openwin(var infile, outfile, parent: text; wid: ss_filhdl);

var win: winptr; { window context pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(parent); { validate parent is a window file }
   iopenwin(infile, outfile, txt2lfn(parent), wid); { process open }
   unlockmain { end exclusive access }

end;

overload procedure openwin(var infile, outfile: text; wid: ss_filhdl);

begin

   lockmain; { start exclusive access }
   iopenwin(infile, outfile, 0, wid); { process open }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Size buffer pixel

Sets or resets the size of the buffer surface, in pixel units.

******************************************************************************}

procedure isizbufg(win: winptr; x, y: integer);

var cr:  sc_rect;   { client rectangle holder }
    si:  1..maxcon; { index for current display screen }

begin

   if (x < 1) or (y < 1) then error(einvsiz); { invalid buffer size }
   with win^ do begin { in windows context }

      { set buffer size }
      gmaxx := x div charspace; { find character size x }
      gmaxy := y div linespace; { find character size y }
      gmaxxg := x; { set size in pixels x }
      gmaxyg := y; { set size in pixels y }
      cr.left := 0; { set up desired client rectangle }
      cr.top := 0;
      cr.right := gmaxxg;
      cr.bottom := gmaxyg;
      { find window size from client size }
      b := sc_adjustwindowrectex(cr, sc_ws_overlappedwindow, false, 0);
      if not b then winerr; { process windows error }
      { now, resize the window to just fit our new buffer size }
      unlockmain; { end exclusive access }
      b := sc_setwindowpos(winhan, 0, 0, 0, cr.right-cr.left, cr.bottom-cr.top,
                           sc_swp_nomove or sc_swp_nozorder);
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }
      { all the screen buffers are wrong, so tear them out }
      for si := 1 to maxcon do disscn(win, screens[si]);
      new(screens[curdsp]); { get the display screen }
      iniscn(win, screens[curdsp]); { initalize screen buffer }
      restore(win, true); { update to screen }
      if curdsp <> curupd then begin { also create the update buffer }

         new(screens[curupd]); { get the display screen }
         iniscn(win, screens[curupd]); { initalize screen buffer }

      end

   end

end;

procedure sizbufg(var f: text; x, y: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window pointer from text file }
   with win^ do { in window context }
      isizbufg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

overload procedure sizbufg(x, y: integer);

var win: winptr; { window pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in window context }
      isizbufg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

******************************************************************************}

procedure sizbuf(var f: text; x, y: integer);

var win: winptr;    { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window context }
   with win^ do { in windows context }
      { just translate from characters to pixels and do the resize in pixels. }
      isizbufg(win, x*charspace, y*linespace);
   unlockmain { end exclusive access }

end;

overload procedure sizbuf(x, y: integer);

var win: winptr; { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in windows context }
      { just translate from characters to pixels and do the resize in pixels. }
      isizbufg(win, x*charspace, y*linespace);
   unlockmain { end exclusive access }

end;

{******************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

******************************************************************************}

procedure ibuffer(win: winptr; e: boolean);

var si:  1..maxcon; { index for current display screen }
    b:   boolean;   { result }
    r:   sc_rect;   { rectangle }

begin

   with win^ do begin { in windows context }

      if e then begin { perform buffer on actions }

         bufmod := true; { turn buffer mode on }
         { restore size from current buffer }
         gmaxxg := screens[curdsp]^.maxxg; { pixel size }
         gmaxyg := screens[curdsp]^.maxyg;
         gmaxx := screens[curdsp]^.maxx; { character size }
         gmaxy := screens[curdsp]^.maxy;
         r.left := 0; { set up desired client rectangle }
         r.top := 0;
         r.right := gmaxxg;
         r.bottom := gmaxyg;
         { find window size from client size }
         b := sc_adjustwindowrectex(r, sc_ws_overlappedwindow, false, 0);
         if not b then winerr; { process windows error }
         { resize the window to just fit our buffer size }
         unlockmain; { end exclusive access }
         b := sc_setwindowpos(winhan, 0, 0, 0, r.right-r.left, r.bottom-r.top,
                              sc_swp_nomove or sc_swp_nozorder);
         lockmain; { start exclusive access }
         if not b then winerr; { process windows error }
         restore(win, true) { restore buffer to screen }

      end else if bufmod then begin { perform buffer off actions }

         { The screen buffer contains a lot of drawing information, so we have
           to keep one of them. We keep the current display, and force the
           update to point to it as well. This single buffer then serves as
           a "template" for the real pixels on screen. }
         bufmod := false; { turn buffer mode off }
         for si := 1 to maxcon do if si <> curdsp then disscn(win, screens[si]);
         { dispose of screen data structures }
         for si := 1 to maxcon do if si <> curdsp then 
            if screens[si] <> nil then dispose(screens[si]);
         curupd := curdsp; { unify the screens }
         { get actual size of onscreen window, and set that as client space }
         b := sc_getclientrect(winhan, r);
         if not b then winerr; { process windows error }
         gmaxxg := r.right-r.left; { return size }
         gmaxyg := r.bottom-r.top;
         gmaxx := gmaxxg div charspace; { find character size x }
         gmaxy := gmaxyg div linespace; { find character size y }
         { tell the window to resize }
         b := sc_postmessage(win^.winhan, sc_wm_size, sc_size_restored, 
                             gmaxyg*65536+gmaxxg);
         if not b then winerr; { process windows error }
         { tell the window to repaint }
         {b := sc_postmessage(win^.winhan, sc_wm_paint, 0, 0);}
         putmsg(win^.winhan, sc_wm_paint, 0, 0);
         if not b then winerr { process windows error }

      end

   end

end;

procedure buffer(var f: text; e: boolean);

var win: winptr; { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window context }
   with win^ do { in windows context }
      ibuffer(win, e); { execute }
   unlockmain { end exclusive access }

end;

overload procedure buffer(e: boolean);

var win: winptr; { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in windows context }
      ibuffer(win, e); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

******************************************************************************}

procedure imenu(win: winptr; m: menuptr);

var b:  boolean; { function result }
    mp: metptr;  { tracking entry pointer }

{ create menu tracking entry }

procedure mettrk(han: integer; inx: integer; m: menuptr);

var mp: metptr; { menu tracking entry pointer }

begin

   with win^ do begin { in window context }

      new(mp); { get a new tracking entry }
      mp^.next := metlst; { push onto tracking list }
      metlst := mp;
      mp^.han := han; { place menu handle }
      mp^.inx := inx; { place menu index }
      mp^.onoff := m^.onoff; { place on/off highlighter }
      mp^.select := false; { place status of select (off) }
      mp^.id := m^.id; { place id }
      mp^.oneof := nil; { set no "one of" }
      { We are walking backwards in the list, and we need the next list entry
        to know the "one of" chain. So we tie the entry to itself as a flag
        that it chains to the next entry. That chain will get fixed on the
        next entry. }
      if m^.oneof then mp^.oneof := mp;
      { now tie the last entry to this if indicated }
      if mp^.next <> nil then { there is a next entry }
         if mp^.next^.oneof = mp^.next then mp^.next^.oneof := mp

   end

end;

{ create menu list }

procedure createmenu(m: menuptr; var mh: integer);

var sm:  integer; { submenu handle }
    f:   integer;  { menu flags }
    inx: integer; { index number for this menu }

begin

   mh := sc_createmenu; { create new menu }
   if mh = 0 then winerr; { process windows error }
   inx := 0; { set first in sequence }
   while m <> nil do begin { add menu item }

      f := sc_mf_string or sc_mf_enabled; { set string and enable }
      if m^.branch <> nil then begin { handle submenu }

         createmenu(m^.branch, sm); { create submenu }
         b := sc_appendmenu(mh, f or sc_mf_popup, sm, m^.face^);
         if not b then winerr; { process windows error }
         mettrk(mh, inx, m) { enter that into tracking }
         
      end else begin { handle terminal menu }

         b := sc_appendmenu(mh, f, m^.id, m^.face^);
         if not b then winerr; { process windows error }
         mettrk(mh, inx, m) { enter that into tracking }

      end;
      if m^.bar then begin { add separator bar }

         { a separator bar is a blank entry that will never be referenced }
         b := sc_appendmenu(mh, sc_mf_separator, 0, '');
         if not b then winerr; { process windows error }
         inx := inx+1 { next in sequence }

      end;
      m := m^.next; { next menu entry }
      inx := inx+1 { next in sequence }

   end

end;

begin

   with win^ do begin { in windows context }
   
      if menhan <> 0 then begin { distroy previous menu }

         b := sc_destroymenu(menhan); { destroy it }
         if not b then winerr; { process windows error }
         { dispose of menu tracking entries }
         while metlst <> nil do begin

            mp := metlst; { remove top entry }
            metlst := metlst^.next; { gap out }
            dispose(mp) { free the entry }

         end;
         menhan := 0 { set no menu active }

      end;
      if m <> nil then { there is a new menu to activate }
         createmenu(m, menhan);
      unlockmain; { end exclusive access }
      b := sc_setmenu(winhan, menhan); { set the menu to the window }
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }
      unlockmain; { end exclusive access }
      b := sc_drawmenubar(winhan); { display menu }
      lockmain; { start exclusive access }
      if not b then winerr { process windows error }

   end

end;

procedure menu(var f: text; m: menuptr);

var win: winptr; { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window context }
   with win^ do { in windows context }
      imenu(win, m); { execute }
   unlockmain { end exclusive access }

end;

overload procedure menu(m: menuptr);

var win: winptr; { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in windows context }
      imenu(win, m); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find menu entry

Finds a menu entry by id. If the entry is not found, it generates an error.
If the entry exists more than once, it generates an error.

******************************************************************************}

function fndmenu(win: winptr; id: integer): metptr;

var mp: metptr; { tracking entry pointer }
    fp: metptr; { found entry pointer }

begin

   with win^ do begin { in window context }

      fp := nil; { set no entry found }
      mp := metlst; { index top of list }
      while mp <> nil do begin { traverse }

         if mp^.id = id then begin { found }

            if fp <> nil then error(edupmen); { menu entry is duplicated }
            fp := mp { set found entry }

         end;
         mp := mp^.next { next entry }
            
      end;
      if fp = nil then error(emennf) { no menu entry found with id }

   end;

   fndmenu := fp { return entry }

end;

{******************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

******************************************************************************}

procedure imenuena(win: winptr; id: integer; onoff: boolean);

var fl: integer; { flags }
    mp: metptr;  { tracking pointer }
    r:  integer; { result }
    b:  boolean; { result }

begin

   with win^ do begin { in windows context }
   
      mp := fndmenu(win, id); { find the menu entry }
      fl := sc_mf_byposition; { place position select flag }
      if onoff then fl := fl or sc_mf_enabled { enable it }
      else fl := fl or sc_mf_grayed; { disable it }
      r := sc_enablemenuitem(mp^.han, mp^.inx, fl); { perform that }
      if r = -1 then error(esystem); { should not happen }
      unlockmain; { end exclusive access }
      b := sc_drawmenubar(winhan); { display menu }
      lockmain; { start exclusive access }
      if not b then winerr { process windows error }

   end

end;

procedure menuena(var f: text; id: integer; onoff: boolean);

var win: winptr; { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window context }
   with win^ do { in windows context }
      imenuena(win, id, onoff); { execute }
   unlockmain { end exclusive access }

end;

overload procedure menuena(id: integer; onoff: boolean);

var win: winptr; { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in windows context }
      imenuena(win, id, onoff); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

******************************************************************************}

procedure imenusel(win: winptr; id: integer; select: boolean);

var fl: integer; { flags }
    mp: metptr;  { tracking pointer }
    b:  boolean; { result }
    r:  integer; { result }

{ find top of "one of" list }

function fndtop(mp: metptr): metptr;

begin

   if mp^.next <> nil then { there is a next }
      if mp^.next^.oneof = mp then begin { next entry links this one }

      mp := mp^.next; { go to next entry }
      mp := fndtop(mp) { try again }

   end;

   fndtop := mp { return result }

end;

{ clear "one of" select list }

procedure clrlst(mp: metptr);

begin

   repeat { items in list }

      fl := sc_mf_byposition or sc_mf_unchecked; { place position select flag }
      r := sc_checkmenuitem(mp^.han, mp^.inx, fl); { perform that }
      if r = -1 then error(esystem); { should not happen }
      mp := mp^.oneof { link next }

   until mp = nil { no more }

end;

begin

   with win^ do begin { in windows context }
   
      mp := fndmenu(win, id); { find the menu entry }
      clrlst(fndtop(mp)); { clear "one of" group }
      mp^.select := select; { set the new select }
      fl := sc_mf_byposition; { place position select flag }
      if mp^.select then fl := fl or sc_mf_checked { select it }
      else fl := fl or sc_mf_unchecked; { deselect it }
      r := sc_checkmenuitem(mp^.han, mp^.inx, fl); { perform that }
      if r = -1 then error(esystem); { should not happen }
      unlockmain; { end exclusive access }
      b := sc_drawmenubar(winhan); { display menu }
      lockmain; { start exclusive access }
      if not b then winerr { process windows error }

   end

end;

procedure menusel(var f: text; id: integer; select: boolean);

var win: winptr; { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window context }
   with win^ do { in windows context }
      imenusel(win, id, select); { execute }
   unlockmain { end exclusive access }

end;

overload procedure menusel(id: integer; select: boolean);

var win: winptr; { pointer to windows context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   with win^ do { in windows context }
      imenusel(win, id, select); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

******************************************************************************}

procedure ifront(win: winptr);

var b:  boolean; { result holder }
    fl: integer;

begin

   with win^ do begin { in windows context }

      fl := 0;
      fl := not fl;
      unlockmain; { end exclusive access }
      b := sc_setwindowpos(winhan, 0{fl} {sc_hwnd_topmost}, 0, 0, 0, 0, 
                           sc_swp_nomove or sc_swp_nosize);
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }

;if false then begin
      fl := 1;
      fl := not fl;
      unlockmain; { end exclusive access }
      b := sc_setwindowpos(winhan, fl {sc_hwnd_notopmost}, 0, 0, 0, 0, 
                           sc_swp_nomove or sc_swp_nosize);
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }
;end;

      unlockmain; { end exclusive access }
      b := sc_postmessage(winhan, sc_wm_paint, 0, 0);
      if not b then winerr; { process windows error }
      lockmain; { start exclusive access }

      if parhan <> 0 then begin

         unlockmain; { end exclusive access }
         b := sc_postmessage(parhan, sc_wm_paint, 0, 0);
         if not b then winerr; { process windows error }
         lockmain; { start exclusive access }

      end

   end
   
end;

procedure front(var f: text);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   ifront(win); { execute }
   unlockmain { end exclusive access }

end;

overload procedure front;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   ifront(win); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

******************************************************************************}

procedure iback(win: winptr);

var b: boolean; { result holder }

begin

   with win^ do begin { in windows context }

      unlockmain; { end exclusive access }
      b := sc_setwindowpos(winhan, sc_hwnd_bottom, 0, 0, 0, 0, 
                           sc_swp_nomove or sc_swp_nosize);
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }

   end
   
end;

procedure back(var f: text);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   iback(win); { execute }
   unlockmain { end exclusive access }

end;

overload procedure back;

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   iback(win); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Get window size graphical

Gets the onscreen window size.

******************************************************************************}

procedure igetsizg(win: winptr; var x, y: integer);

var b: boolean; { result holder }
    r: sc_rect; { rectangle }

begin

   with win^ do begin { in windows context }

      b := sc_getwindowrect(winhan, r);
      if not b then winerr; { process windows error }
      x := r.right-r.left; { return size }
      y := r.bottom-r.top

   end
   
end;

procedure getsizg(var f: text; var x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   igetsizg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

overload procedure getsizg(var x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   igetsizg(win, x, y); { execute }
   unlockmain { end exclusive access }

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

var win, par: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   igetsizg(win, x, y); { execute }
   if win^.parlfn <> 0 then begin { has a parent }

      par := lfn2win(win^.parlfn); { index the parent }
      { find character based sizes }
      x := (x-1) div par^.charspace+1;
      y := (y-1) div par^.linespace+1

   end else begin

      { find character based sizes }
      x := (x-1) div stdchrx+1;
      y := (y-1) div stdchry+1

   end;
   unlockmain { end exclusive access }

end;

overload procedure getsiz(var x, y: integer);

var win, par: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   igetsizg(win, x, y); { execute }
   if win^.parlfn <> 0 then begin { has a parent }

      par := lfn2win(win^.parlfn); { index the parent }
      { find character based sizes }
      x := (x-1) div par^.charspace+1;
      y := (y-1) div par^.linespace+1

   end else begin

      { find character based sizes }
      x := (x-1) div stdchrx+1;
      y := (y-1) div stdchry+1

   end;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set window size graphical

Sets the onscreen window to the given size.

******************************************************************************}

procedure isetsizg(win: winptr; x, y: integer);

var b: boolean; { result holder }

begin

   with win^ do begin { in windows context }

      unlockmain; { end exclusive access }
      b := sc_setwindowpos(winhan, 0, 0, 0, x, y, 
                           sc_swp_nomove or sc_swp_nozorder);
      lockmain; { start exclusive access }
      if not b then winerr { process windows error }

   end
   
end;

procedure setsizg(var f: text; x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   isetsizg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

overload procedure setsizg(x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   isetsizg(win, x, y); { execute }
   unlockmain { end exclusive access }

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

var win, par: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   if win^.parlfn <> 0 then begin { has a parent }

      par := lfn2win(win^.parlfn); { index the parent }
      { find character based sizes }
      x := x*par^.charspace;
      y := y*par^.linespace

   end else begin

      { find character based sizes }
      x := x*stdchrx;
      y := y*stdchry

   end;
   isetsizg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

overload procedure setsiz(x, y: integer);

var win, par: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   if win^.parlfn <> 0 then begin { has a parent }

      par := lfn2win(win^.parlfn); { index the parent }
      { find character based sizes }
      x := x*par^.charspace;
      y := y*par^.linespace

   end else begin

      { find character based sizes }
      x := x*stdchrx;
      y := y*stdchry

   end;
   isetsizg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set window position graphical

Sets the onscreen window to the given position in its parent.

******************************************************************************}

procedure isetposg(win: winptr; x, y: integer);

var b: boolean; { result holder }

begin

   with win^ do begin { in windows context }

      unlockmain; { end exclusive access }
      b := sc_setwindowpos(winhan, 0, x-1, y-1, 0, 0, sc_swp_nosize);
      lockmain; { start exclusive access }
      if not b then winerr { process windows error }

   end
   
end;

procedure setposg(var f: text; x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   isetposg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

overload procedure setposg(x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   isetposg(win, x, y); { execute }
   unlockmain { end exclusive access }

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

var win, par: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   if win^.parlfn <> 0 then begin { has a parent }

      par := lfn2win(win^.parlfn); { index the parent }
      { find character based sizes }
      x := (x-1)*par^.charspace+1;
      y := (y-1)*par^.linespace+1

   end else begin

      { find character based sizes }
      x := (x-1)*stdchrx+1;
      y := (y-1)*stdchry+1

   end;
   isetposg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

overload procedure setpos(x, y: integer);

var win, par: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   if win^.parlfn <> 0 then begin { has a parent }

      par := lfn2win(win^.parlfn); { index the parent }
      { find character based sizes }
      x := (x-1)*par^.charspace+1;
      y := (y-1)*par^.linespace+1

   end else begin

      { find character based sizes }
      x := (x-1)*stdchrx+1;
      y := (y-1)*stdchry+1

   end;
   isetposg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Get screen size graphical

Gets the total screen size.

******************************************************************************}

procedure iscnsizg(win: winptr; var x, y: integer);

var b:      boolean; { result holder }
    r:      sc_rect; { rectangle }
    scnhan: integer; { desktop handle }

begin

   with win^ do begin { in windows context }

      scnhan := sc_getdesktopwindow;
      b := sc_getwindowrect(scnhan, r);
      if not b then winerr; { process windows error }
      x := r.right-r.left; { return size }
      y := r.bottom-r.top

   end
   
end;

procedure scnsizg(var f: text; var x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   iscnsizg(win, x, y); { execute }
   unlockmain { end exclusive access }

end;

overload procedure scnsizg(var x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   iscnsizg(win, x, y); { execute }
   unlockmain { end exclusive access }

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

procedure iwinclientg(win: winptr; cx, cy: integer; var wx, wy: integer; 
                      view ms: winmodset);

var cr: sc_rect; { client rectangle holder }
    fl: integer; { flag }

begin

   lockmain; { start exclusive access }
   with win^ do begin

      cr.left := 0; { set up desired client rectangle }
      cr.top := 0;
      cr.right := cx;
      cr.bottom := cy;
      { set minimum style }
      fl := sc_ws_overlapped or sc_ws_clipchildren;
      { add flags for child window }
      if parhan <> 0 then fl := fl or sc_ws_child or sc_ws_clipsiblings;
      if wmframe in ms then begin { add frame features }

         if wmsize in ms then fl := fl or sc_ws_thickframe;

         if wmsysbar in ms then fl := fl or sc_ws_caption or sc_ws_sysmenu or
                                      sc_ws_minimizebox or sc_ws_maximizebox;

      end;
      { find window size from client size }
      b := sc_adjustwindowrectex(cr, fl, false, 0);
      if not b then winerr; { process windows error }
      wx := cr.right-cr.left; { return window size }
      wy := cr.bottom-cr.top

   end;
   unlockmain { end exclusive access }

end;

procedure winclient(var f: text; cx, cy: integer; var wx, wy: integer;
                    view ms: winmodset);

var win, par: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   { execute }
   iwinclientg(win, cx*win^.charspace, cy*win^.linespace, wx, wy, ms);
   { find character based sizes }
   if win^.parlfn <> 0 then begin { has a parent }

      par := lfn2win(win^.parlfn); { index the parent }
      { find character based sizes }
      wx := (wx-1) div par^.charspace+1;
      wy := (wy-1) div par^.linespace+1

   end else begin

      { find character based sizes }
      wx := (wx-1) div stdchrx+1;
      wy := (wy-1) div stdchry+1

   end;
   unlockmain { end exclusive access }

end;

overload procedure winclient(cx, cy: integer; var wx, wy: integer;
                             view ms: winmodset);

var win, par: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   { execute }
   iwinclientg(win, cx*win^.charspace, cy*win^.linespace, wx, wy, ms);
   { find character based sizes }
   if win^.parlfn <> 0 then begin { has a parent }

      par := lfn2win(win^.parlfn); { index the parent }
      { find character based sizes }
      wx := (wx-1) div par^.charspace+1;
      wy := (wy-1) div par^.linespace+1;

   end else begin

      { find character based sizes }
      wx := (wx-1) div stdchrx+1;
      wy := (wy-1) div stdchry+1

   end;
   unlockmain { end exclusive access }

end;

procedure winclientg(var f: text; cx, cy: integer; var wx, wy: integer;
                     view ms: winmodset);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   iwinclientg(win, cx, cy, wx, wy, ms); { execute }
   unlockmain { end exclusive access }

end;

overload procedure winclientg(cx, cy: integer; var wx, wy: integer;
                              view ms: winmodset);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   iwinclientg(win, cx, cy, wx, wy, ms); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

******************************************************************************}

procedure scnsiz(var f: text; var x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   iscnsizg(win, x, y); { execute }
   x := x div stdchrx; { convert to "standard character" size }
   y := y div stdchry;
   unlockmain { end exclusive access }

end;

overload procedure scnsiz(var x, y: integer);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   iscnsizg(win, x, y); { execute }
   x := x div stdchrx; { convert to "standard character" size }
   y := y div stdchry;
   unlockmain { end exclusive access }

end;

{******************************************************************************

Enable or disable window frame

Turns the window frame on and off.

******************************************************************************}

procedure iframe(win: winptr; e: boolean);

var fl1, fl2: integer; { flag }
    cr:       sc_rect; { client rectangle holder }

begin

   with win^ do begin { in windows context }

      frame := e; { set new status of frame }
      fl1 := sc_ws_overlapped or sc_ws_clipchildren; { set minimum style }
      { add flags for child window }
      if parhan <> 0 then fl1 := fl1 or sc_ws_child or sc_ws_clipsiblings;
      { if we are enabling frames, add the frame parts back }
      if e then begin

         if size then fl1 := fl1 or sc_ws_thickframe;
         if sysbar then fl1 := fl1 or sc_ws_caption or sc_ws_sysmenu or
                               sc_ws_minimizebox or sc_ws_maximizebox;

      end;
      fl2 := $f; { build the gwl_style value }
      fl2 := not fl2;
      unlockmain; { end exclusive access }
      r := sc_setwindowlong(winhan, fl2 {sc_gwl_style}, fl1);
      lockmain; { start exclusive access }
      if r = 0 then winerr; { process windows error }
      unlockmain; { end exclusive access }
      b := sc_setwindowpos(winhan, 0, 0, 0, 0, 0, 
                           sc_swp_nosize or sc_swp_nomove or 
                           sc_swp_framechanged);
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }
      { present the window }
      unlockmain; { end exclusive access }
      b := sc_showwindow(winhan, sc_sw_showdefault);
      lockmain; { start exclusive access }
      if bufmod then begin { in buffer mode }

         { change window size to match new mode }
         cr.left := 0; { set up desired client rectangle }
         cr.top := 0;
         cr.right := gmaxxg;
         cr.bottom := gmaxyg;
         { find window size from client size }
         b := sc_adjustwindowrectex(cr, fl1, false, 0);
         if not b then winerr; { process windows error }
         unlockmain; { end exclusive access }
         b := sc_setwindowpos(winhan, 0, 0, 0, 
                              cr.right-cr.left, cr.bottom-cr.top,
                              sc_swp_nomove or sc_swp_nozorder);
         lockmain; { start exclusive access }
         if not b then winerr { process windows error }

      end

   end

end;

procedure frame(var f: text; e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   iframe(win, e); { execute }
   unlockmain { end exclusive access }

end;

overload procedure frame(e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   iframe(win, e); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

******************************************************************************}

procedure isizable(win: winptr; e: boolean);

var fl1, fl2: integer; { flag }
    cr:       sc_rect; { client rectangle holder }

begin

   with win^ do begin { in windows context }

      size := e; { set new status of size bars }
      { no point in making the change of framing is off entirely }
      if frame then begin

         { set minimum style }
         fl1 := sc_ws_overlapped or sc_ws_clipchildren;
         { add frame features }
         if size then fl1 := fl1 or sc_ws_thickframe
         else fl1 := fl1 or sc_ws_border;
         if sysbar then fl1 := fl1 or sc_ws_caption or sc_ws_sysmenu or
                               sc_ws_minimizebox or sc_ws_maximizebox;
         { add flags for child window }
         if parhan <> 0 then fl1 := fl1 or sc_ws_child or sc_ws_clipsiblings;
         { if we are enabling frames, add the frame parts back }
         if e then fl1 := fl1 or sc_ws_thickframe;
         fl2 := $f; { build the gwl_style value }
         fl2 := not fl2;
         unlockmain; { end exclusive access }
         r := sc_setwindowlong(winhan, fl2 {sc_gwl_style}, fl1);
         lockmain; { start exclusive access }
         if r = 0 then winerr; { process windows error }
         unlockmain; { end exclusive access }
         b := sc_setwindowpos(winhan, 0, 0, 0, 0, 0, 
                              sc_swp_nosize or sc_swp_nomove or 
                              sc_swp_framechanged);
         lockmain; { start exclusive access }
         if not b then winerr; { process windows error }
         { present the window }
         unlockmain; { end exclusive access }
         b := sc_showwindow(winhan, sc_sw_showdefault);
         lockmain; { start exclusive access }
         if bufmod then begin { in buffer mode }

            { change window size to match new mode }
            cr.left := 0; { set up desired client rectangle }
            cr.top := 0;
            cr.right := gmaxxg;
            cr.bottom := gmaxyg;
            { find window size from client size }
            b := sc_adjustwindowrectex(cr, fl1, false, 0);
            if not b then winerr; { process windows error }
            unlockmain; { end exclusive access }
            b := sc_setwindowpos(winhan, 0, 0, 0, 
                                 cr.right-cr.left, cr.bottom-cr.top,
                                 sc_swp_nomove or sc_swp_nozorder);
            lockmain; { start exclusive access }
            if not b then winerr { process windows error }

         end

      end

   end

end;

procedure sizable(var f: text; e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   isizable(win, e); { execute }
   unlockmain { end exclusive access }

end;

overload procedure sizable(e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   isizable(win, e); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

******************************************************************************}

procedure isysbar(win: winptr; e: boolean);

var fl1, fl2: integer; { flag }
    cr:       sc_rect; { client rectangle holder }

begin

   with win^ do begin { in windows context }

      sysbar := e; { set new status of size bars }
      { no point in making the change of framing is off entirely }
      if frame then begin

         { set minimum style }
         fl1 := sc_ws_overlapped or sc_ws_clipchildren;
         { add frame features }
         if size then fl1 := fl1 or sc_ws_thickframe
         else fl1 := fl1 or sc_ws_border;
         if sysbar then fl1 := fl1 or sc_ws_caption or sc_ws_sysmenu or
                               sc_ws_minimizebox or sc_ws_maximizebox;
         { add flags for child window }
         if parhan <> 0 then fl1 := fl1 or sc_ws_child or sc_ws_clipsiblings;
         { if we are enabling frames, add the frame parts back }
         if e then fl1 := fl1 or sc_ws_thickframe;
         fl2 := $f; { build the gwl_style value }
         fl2 := not fl2;
         unlockmain; { end exclusive access }
         r := sc_setwindowlong(winhan, fl2 {sc_gwl_style}, fl1);
         lockmain; { start exclusive access }
         if r = 0 then winerr; { process windows error }
         unlockmain; { end exclusive access }
         b := sc_setwindowpos(winhan, 0, 0, 0, 0, 0,
                              sc_swp_nosize or sc_swp_nomove or 
                              sc_swp_framechanged);
         lockmain; { start exclusive access }
         if not b then winerr; { process windows error }
         { present the window }
         unlockmain; { end exclusive access }
         b := sc_showwindow(winhan, sc_sw_showdefault);
         lockmain; { start exclusive access }
         if bufmod then begin { in buffer mode }

            { change window size to match new mode }
            cr.left := 0; { set up desired client rectangle }
            cr.top := 0;
            cr.right := gmaxxg;
            cr.bottom := gmaxyg;
            { find window size from client size }
            b := sc_adjustwindowrectex(cr, fl1, false, 0);
            if not b then winerr; { process windows error }
            unlockmain; { end exclusive access }
            b := sc_setwindowpos(winhan, 0, 0, 0, 
                                 cr.right-cr.left, cr.bottom-cr.top,
                                 sc_swp_nomove or sc_swp_nozorder);
            lockmain; { start exclusive access }
            if not b then winerr { process windows error }

         end

      end

   end

end;

procedure sysbar(var f: text; e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get window from file }
   isysbar(win, e); { execute }
   unlockmain { end exclusive access }

end;

overload procedure sysbar(e: boolean);

var win: winptr; { windows record pointer }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   isysbar(win, e); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Append menu entry

Appends a new menu entry to the given list.

******************************************************************************}

procedure appendmenu(var list: menuptr; m: menuptr);

var lp: menuptr;

begin

   { clear these links for insurance }
   m^.next := nil; { clear next }
   m^.branch := nil; { clear branch }
   if list = nil then list := m { list empty, set as first entry }
   else begin { list non-empty }

      { find last entry in list }
      lp := list; { index 1st on list }
      while lp^.next <> nil do lp := lp^.next;
      lp^.next := m { append at end }

   end

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

var m, hm: menuptr; { pointer for menu entries }

{ get menu entry }

procedure getmenu(var m: menuptr; id: integer; view face: string);

begin

   new(m); { get new menu element }
   m^.next   := nil; { no next }
   m^.branch := nil; { no branch }
   m^.onoff  := false; { not an on/off value }
   m^.oneof  := false; { not a "one of" value }
   m^.bar    := false; { no bar under }
   m^.id     := id;    { no id }
   new(m^.face, max(face)); { place face string }
   m^.face^ := face

end;

{ add standard list item }

procedure additem(i: integer; var m, l: menuptr; view s: string; 
                  b: boolean);

begin

   if i in sms then begin { this item is active }

      getmenu(m, i, s); { get menu item }
      appendmenu(l, m); { add to list }
      m^.bar := b       { set bar status }

   end

end;

begin

   refer(pm); { we don't implement menu addition yet }
   sm := nil; { clear menu }

   { check and perform "file" menu }

   if sms*[smnew, smopen, smclose, smsave, smsaveas, smpageset, smprint, 
           smexit] <> [] then begin { file menu }

      getmenu(hm, 0, 'File'); { get entry }
      appendmenu(sm, hm);

      additem(smnew, m, hm^.branch, 'New', false);
      additem(smopen, m, hm^.branch, 'Open', false);
      additem(smclose, m, hm^.branch, 'Close', false);
      additem(smsave, m, hm^.branch, 'Save', false);
      additem(smsaveas, m, hm^.branch, 'Save As', true);
      additem(smpageset, m, hm^.branch, 'Page Setup', false);
      additem(smprint, m, hm^.branch, 'Print', true);
      additem(smexit, m, hm^.branch, 'Exit', false)

   end;

   { check and perform "edit" menu }

   if sms*[smundo, smcut, smpaste, smdelete, smfind, smfindnext, 
           smreplace, smgoto, smselectall] <> [] then begin { file menu }

      getmenu(hm, 0, 'Edit'); { get entry }
      appendmenu(sm, hm);

      additem(smundo, m, hm^.branch, 'Undo', true);
      additem(smcut, m, hm^.branch, 'Cut', false);
      additem(smpaste, m, hm^.branch, 'Paste', false);
      additem(smdelete, m, hm^.branch, 'Delete', true);
      additem(smfind, m, hm^.branch, 'Find', false);
      additem(smfindnext, m, hm^.branch, 'Find Next', false);
      additem(smreplace, m, hm^.branch, 'Replace', false);
      additem(smgoto, m, hm^.branch, 'Goto', true);
      additem(smselectall, m, hm^.branch, 'Select All', false)

   end;

   { insert custom menu }

   while pm <> nil do begin { insert entries }

      m := pm; { index top button }
      pm := pm^.next; { next button }
      appendmenu(sm, m);

   end;

   { check and perform "window" menu }

   if sms*[smnewwindow, smtilehoriz, smtilevert, smcascade, 
           smcloseall] <> [] then begin { file menu }

      getmenu(hm, 0, 'Window'); { get entry }
      appendmenu(sm, hm);

      additem(smnewwindow, m, hm^.branch, 'New Window', true);
      additem(smtilehoriz, m, hm^.branch, 'Tile Horizontally', false);
      additem(smtilevert, m, hm^.branch, 'Tile Vertically', false);
      additem(smcascade, m, hm^.branch, 'Cascade', true);
      additem(smcloseall, m, hm^.branch, 'Close All', false)

   end;

   { check and perform "help" menu }

   if sms*[smhelptopic, smabout] <> [] then begin { file menu }

      getmenu(hm, 0, 'Help'); { get entry }
      appendmenu(sm, hm);

      additem(smhelptopic, m, hm^.branch, 'Help Topics', true);
      additem(smabout, m, hm^.branch, 'About', false)

   end

end;

{******************************************************************************

Create widget

Creates a widget within the given window, within the specified bounding box,
and using the face string and type, and the given id. The string may or may not
be used.

Widgets use the subthread to buffer them. There were various problems from
trying to start them on the main window.

******************************************************************************}

procedure widget(win: winptr; x1, y1, x2, y2: integer; view s: string; 
                 id: integer; typ: wigtyp; exfl: integer; var wp: wigptr);

var fl:     integer; { creation flag }
    clsstr: packed array [1..20] of char; { class name }
    b:      boolean; { return value }

{ create widget according to type }

function createwidget(win: winptr; typ: wigtyp; x1, y1, x2, y2: integer;
                      view s: string; id: integer): integer;

var wh: integer; { handle to widget }
    ip: imptr;   { intertask message pointer }

begin

   with win^ do begin

      { search previous definition with same id }
      if fndwig(win, id) <> nil then error(ewigdup); { duplicate widget id }
      case typ of { widget type }

         wtbutton: begin

            clsstr := 'button              ';
            fl := sc_bs_pushbutton or exfl;  { button }

         end;
         wtcheckbox: begin

            clsstr := 'button              ';
            fl := sc_bs_checkbox or exfl;    { checkbox }

         end;
         wtradiobutton: begin

            clsstr := 'button              ';
            fl := sc_bs_radiobutton or exfl; { radio button }

         end;
         wtgroup: begin

            clsstr := 'button              ';
            fl := sc_bs_groupbox or exfl;    { group box }

         end;
         wtbackground: begin

            clsstr := 'static              ';
            fl := 0 or exfl;   { background }

         end;
         wtscrollvert: begin { vertical scroll bar }

            clsstr := 'scrollbar           ';
            fl := sc_sbs_vert or exfl;

         end;
         wtscrollhoriz: begin { horizontal scrollbar }

            clsstr := 'scrollbar           ';
            fl := sc_sbs_horz or exfl;

         end;
         wteditbox: begin { single line edit }

            clsstr := 'edit                ';
            fl := sc_ws_border or sc_es_left or sc_es_autohscroll or exfl;

         end;
         wtprogressbar: begin { progress bar }
 
            clsstr := 'msctls_progress32   ';
            fl := 0 or exfl;

         end;
         wtlistbox: begin { list box }
 
            clsstr := 'listbox             ';
            fl := sc_lbs_standard-sc_lbs_sort or exfl;

         end;
         wtdropbox: begin { list box }
 
            clsstr := 'combobox            ';
            fl := sc_cbs_dropdownlist or exfl;

         end;
         wtdropeditbox: begin { list box }
 
            clsstr := 'combobox            ';
            fl := sc_cbs_dropdown or exfl;

         end;
         wtslidehoriz: begin { horizontal slider }
 
            clsstr := 'msctls_trackbar32   ';
            fl := sc_tbs_horz or sc_tbs_autoticks or exfl;

         end;
         wtslidevert: begin { vertical slider }
 
            clsstr := 'msctls_trackbar32   ';
            fl := sc_tbs_vert or sc_tbs_autoticks or exfl;

         end;
         wttabbar: begin { tab bar }
 
            clsstr := 'systabcontrol32     ';
            fl := sc_ws_visible or exfl;

         end;

      end;
      { create an intertask message to start the widget }
      getitm(ip); { get a im pointer }
      ip^.im := imwidget; { set type is widget }
      copy(ip^.wigcls, clsstr); { place class string }
      copy(ip^.wigtxt, s); { place face text }
      ip^.wigflg := sc_ws_child or sc_ws_visible or fl;
      ip^.wigx := x1-1; { place origin }
      ip^.wigy := y1-1;
      ip^.wigw := x2-x1+1; { place size }
      ip^.wigh := y2-y1+1;
      ip^.wigpar := winhan; { place parent }
      ip^.wigid := id; { place id }
      ip^.wigmod := sc_getmodulehandle_n; { place module }
      { order widget to start }
      b := sc_postmessage(dispwin, umim, itm2int(ip), 0);
      if not b then winerr; { process windows error }
      { Wait for widget start, this also keeps our window going. } 
      waitim(imwidget, ip); { wait for the return }
      wh := ip^.wigwin; { place handle to widget }
      dispose(ip^.wigcls); { release class string }
      dispose(ip^.wigtxt); { release face text string }
      putitm(ip) { release im }

   end;

   createwidget := wh { return handle }

end;

begin

   getwig(win, wp); { get new widget }
   { Group widgets don't have a background, so we pair it up with a background
     widget. }
   if typ = wtgroup then { create buddy for group }
      wp^.han2 := createwidget(win, wtbackground, x1, y1, x2, y2, '', id);
   wp^.han := createwidget(win, typ, x1, y1, x2, y2, s, id); { create widget }
   wp^.id := id; { place button id }
   wp^.typ := typ { place type }

end;

{******************************************************************************

Kill widget

Removes the widget by id from the window.

******************************************************************************}

procedure ikillwidget(win: winptr; id: integer);

var wp: wigptr; { widget pointer }

begin

   with win^ do begin { in windows context }

      if not visible then winvis(win); { make sure we are displayed }
      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      kilwin(wp^.han); { kill window }
      if wp^.han2 <> 0 then kilwin(wp^.han2); { distroy buddy window }
      putwig(win, wp) { release widget entry }

   end;

end;

procedure killwidget(var f: text; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ikillwidget(win, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure killwidget(id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ikillwidget(win, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Select/deselect widget

Selects or deselects a widget.

******************************************************************************}

procedure iselectwidget(win: winptr; id: integer; e: boolean);

var wp: wigptr;  { widget entry }
    r:  integer; { return value }

begin

   with win^ do begin

      if not visible then winvis(win); { make sure we are displayed }
      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      { check this widget is selectable }
      if not (wp^.typ in [wtcheckbox, wtradiobutton]) then error(ewigsel);
      unlockmain; { end exclusive access }
      r := sc_sendmessage(wp^.han, sc_bm_setcheck, ord(e), 0);
      lockmain { start exclusive access }
   
   end

end;

procedure selectwidget(var f: text; id: integer; e: boolean);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iselectwidget(win, id, e); { execute }
   unlockmain { end exclusive access }

end;

overload procedure selectwidget(id: integer; e: boolean);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iselectwidget(win, id, e); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Enable/disable widget

Enables or disables a widget.

******************************************************************************}

procedure ienablewidget(win: winptr; id: integer; e: boolean);

var wp:  wigptr;  { widget entry }
    b:   boolean; { return value }

begin

   with win^ do begin

      if not visible then winvis(win); { make sure we are displayed }
      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      { check this widget can get text }
      if not (wp^.typ in [wtbutton, wtcheckbox, wtradiobutton, wtgroup,
                          wtscrollvert, wtscrollhoriz, wtnumselbox,
                          wteditbox, wtlistbox, wtdropbox, wtdropeditbox,
                          wtslidehoriz, wtslidevert, 
                          wttabbar]) then error(ewigdis);
      unlockmain; { end exclusive access }
      b := sc_enablewindow(wp^.han, e); { perform }
      lockmain; { start exclusive access }
      wp^.enb := e { save enable/disable status }
   
   end;

end;

procedure enablewidget(var f: text; id: integer; e: boolean);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ienablewidget(win, id, e); { execute }
   unlockmain { end exclusive access }

end;

overload procedure enablewidget(id: integer; e: boolean);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ienablewidget(win, id, e); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Get widget text

Retrives the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

******************************************************************************}

procedure igetwidgettext(win: winptr; id: integer; var s: pstring);

var wp:  wigptr;  { widget pointer }
    ls:  integer; { length of text }
    sp:  pstring; { pointer to string }
    i:   integer; { index for string }
    r:   integer; { return value }

begin

   with win^ do begin

      if not visible then winvis(win); { make sure we are displayed }
      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      { check this widget can get text }
      if not (wp^.typ in [wteditbox, wtdropeditbox]) then error(ewiggtxt);
      unlockmain; { end exclusive access }
      ls := sc_getwindowtextlength(wp^.han); { get text length }
      lockmain; { start exclusive access }
      { There is no real way to process an error, as listed in the 
        documentation, for getwindowtextlength. The docs define
        a zero return as being for a zero length string, but also apparently
        uses that value for errors. }
      new(sp, ls+1); { get a string for that, with zero terminate }
      unlockmain; { end exclusive access }
      r := sc_getwindowtext(wp^.han, sp^); { get the text }
      lockmain; { start exclusive access }
      { Getwindowtext has the same issue as getwindowtextlength, with the
        exception that, since we already have the length of data, if the
        length is wrong AND the return is zero, its an error. This leaves
        the case of an error on a zero length return. }
      if (r = 0) and (r <> ls) then winerr; { process windows error }
      if r <> ls then error(esystem); { lengths should match }
      new(s, r); { get final string }
      for i := 1 to r do s^[i] := sp^[i]; { copy into place }
      dispose(sp) { release temp buffer }

   end

end;

procedure getwidgettext(var f: text; id: integer; var s: pstring);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   igetwidgettext(win, id, s); { execute }
   unlockmain { end exclusive access }

end;

overload procedure getwidgettext(id: integer; var s: pstring);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   igetwidgettext(win, id, s); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

put edit box text

Places text into an edit box.

******************************************************************************}

procedure iputwidgettext(win: winptr; id: integer; view s: string);

var wp: wigptr;  { widget pointer }
    b:  boolean; { return value }

begin

   with win^ do begin

      if not visible then winvis(win); { make sure we are displayed }
      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      { check this widget can put text }
      if not (wp^.typ in [wteditbox, wtdropeditbox]) then error(ewigptxt);
      unlockmain; { end exclusive access }
      b := sc_setwindowtext(wp^.han, s); { get the text }
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }

   end;

end;

procedure putwidgettext(var f: text; id: integer; view s: string);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iputwidgettext(win, id, s); { execute }
   unlockmain { end exclusive access }

end;

overload procedure putwidgettext(id: integer; view s: string);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iputwidgettext(win, id, s); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Resize widget

Changes the size of a widget.

******************************************************************************}

procedure isizwidgetg(win: winptr; id: integer; x, y: integer);

var wp: wigptr; { widget pointer }

begin

   with win^ do begin { in windows context }

      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      unlockmain; { end exclusive access }
      b := sc_setwindowpos(wp^.han, 0, 0, 0, x, y, 
                           sc_swp_nomove or sc_swp_nozorder);
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }
      if wp^.han2 <> 0 then begin { also resize the buddy }

         { Note, the buddy needs to be done differently for a numselbox }
         unlockmain; { end exclusive access }
         b := sc_setwindowpos(wp^.han2, 0, 0, 0, x, y, 
                              sc_swp_nomove or sc_swp_nozorder);
         lockmain; { start exclusive access }
         if not b then winerr; { process windows error }

      end

   end;

end;

procedure sizwidgetg(var f: text; id: integer; x, y: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   isizwidgetg(win, id, x, y); { execute }
   unlockmain { end exclusive access }

end;

overload procedure sizwidgetg(id: integer; x, y: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   isizwidgetg(win, id, x, y); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Reposition widget

Changes the parent position of a widget.

******************************************************************************}

procedure iposwidgetg(win: winptr; id: integer; x, y: integer);

var wp: wigptr; { widget pointer }

begin

   with win^ do begin { in windows context }

      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      unlockmain; { end exclusive access }
      b := sc_setwindowpos(wp^.han, 0, x-1, y-1, 0, 0, sc_swp_nosize);
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }
      if wp^.han2 <> 0 then begin { also reposition the buddy }

         { Note, the buddy needs to be done differently for a numselbox }
         unlockmain; { end exclusive access }
         b := sc_setwindowpos(wp^.han2, 0, x-1, y-1, 0, 0, sc_swp_nosize);
         lockmain; { start exclusive access }
         if not b then winerr; { process windows error }

      end

   end;

end;

procedure poswidgetg(var f: text; id: integer; x, y: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iposwidgetg(win, id, x, y); { execute }
   unlockmain { end exclusive access }

end;

overload procedure poswidgetg(id: integer; x, y: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iposwidgetg(win, id, x, y); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Place widget to back of Z order

******************************************************************************}

procedure ibackwidget(win: winptr; id: integer);

var wp: wigptr;  { widget pointer }
    b:  boolean; { result holder }

begin

   with win^ do begin { in windows context }

      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      unlockmain; { end exclusive access }
      b := sc_setwindowpos(wp^.han, sc_hwnd_bottom, 0, 0, 0, 0, 
                           sc_swp_nomove or sc_swp_nosize);
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }
      if wp^.han2 <> 0 then begin { also reposition the buddy }

         { Note, the buddy needs to be done differently for a numselbox }
         unlockmain; { end exclusive access }
         b := sc_setwindowpos(wp^.han2, sc_hwnd_bottom, 0, 0, 0, 0, 
                              sc_swp_nomove or sc_swp_nosize);
         lockmain; { start exclusive access }
         if not b then winerr; { process windows error }

      end

   end;

end;

procedure backwidget(var f: text; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ibackwidget(win, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure backwidget(id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ibackwidget(win, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Place widget to back of Z order

******************************************************************************}

procedure ifrontwidget(win: winptr; id: integer);

var wp: wigptr;  { widget pointer }
    b:  boolean; { result holder }
    fl: integer;

begin

   with win^ do begin { in windows context }

      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      fl := 0;
      fl := not fl;
      unlockmain; { end exclusive access }
      b := sc_setwindowpos(wp^.han, fl {sc_hwnd_topmost}, 0, 0, 0, 0, 
                           sc_swp_nomove or sc_swp_nosize);
      lockmain; { start exclusive access }
      if not b then winerr; { process windows error }
      if wp^.han2 <> 0 then begin { also reposition the buddy }

         { Note, the buddy needs to be done differently for a numselbox }
         unlockmain; { end exclusive access }
         b := sc_setwindowpos(wp^.han2, fl {sc_hwnd_topmost}, 0, 0, 0, 0, 
                              sc_swp_nomove or sc_swp_nosize);
         lockmain; { start exclusive access }
         if not b then winerr; { process windows error }

      end

   end;

end;

procedure frontwidget(var f: text; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ifrontwidget(win, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure frontwidget(id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ifrontwidget(win, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard button size

Finds the minimum size for a button. Given the face string, the minimum size of
a button is calculated and returned.

******************************************************************************}

procedure ibuttonsizg(win: winptr; view s: string; var w, h: integer);

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    dc: integer; { dc for screen }

begin

   refer(win); { don't need the window data }

   dc := sc_getwindowdc(0); { get screen dc }
   if dc = 0 then winerr; { process windows error }
   b := sc_gettextextentpoint32(dc, s, sz); { get sizing }
   if not b then winerr; { process windows error }
   { add button borders to size }
   w := sz.cx+sc_getsystemmetrics(sc_sm_cxedge)*2;
   h := sz.cy+sc_getsystemmetrics(sc_sm_cyedge)*2

end;

procedure ibuttonsiz(win: winptr; view s: string; var w, h: integer);

begin

   ibuttonsizg(win, s, w, h); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1

end;

procedure buttonsizg(var f: text; view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ibuttonsizg(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure buttonsizg(view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ibuttonsizg(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure buttonsiz(var f: text; view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ibuttonsiz(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure buttonsiz(view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ibuttonsiz(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create button

Creates a standard button within the specified rectangle, on the given window.

******************************************************************************}

procedure ibuttong(win: winptr; x1, y1, x2, y2: integer; view s: string; 
                  id: integer);

var wp: wigptr; { widget pointer }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, s, id, wtbutton, 0, wp)

end;

procedure ibutton(win: winptr; x1, y1, x2, y2: integer; view s: string; 
                  id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   ibuttong(win, x1, y1, x2, y2, s, id) { create button graphical }

end;

procedure buttong(var f: text; x1, y1, x2, y2: integer; view s: string; 
                 id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ibuttong(win, x1, y1, x2, y2, s, id);
   unlockmain { end exclusive access }

end;

overload procedure buttong(x1, y1, x2, y2: integer; view s: string;
                          id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ibuttong(win, x1, y1, x2, y2, s, id);
   unlockmain { end exclusive access }

end;

procedure button(var f: text; x1, y1, x2, y2: integer; view s: string; 
                 id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ibutton(win, x1, y1, x2, y2, s, id);
   unlockmain { end exclusive access }

end;

overload procedure button(x1, y1, x2, y2: integer; view s: string;
                          id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window from file }
   ibutton(win, x1, y1, x2, y2, s, id);
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard checkbox size

Finds the minimum size for a checkbox. Given the face string, the minimum size of
a checkbox is calculated and returned.

******************************************************************************}

procedure icheckboxsizg(win: winptr; view s: string; var w, h: integer);

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    dc: integer; { dc for screen }

begin

   refer(win); { don't need the window data }

   dc := sc_getwindowdc(0); { get screen dc }
   if dc = 0 then winerr; { process windows error }
   b := sc_gettextextentpoint32(dc, s, sz); { get sizing }
   if not b then winerr; { process windows error }
   { We needed to add a fudge factor for the space between the checkbox, the
     left edge of the widget, and the left edge of the text. }
   w := sz.cx+sc_getsystemmetrics(sc_sm_cxmenucheck)+6; { return size }
   h := sz.cy

end;

procedure icheckboxsiz(win: winptr; view s: string; var w, h: integer);

begin

   icheckboxsizg(win, s, w, h); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1

end;

procedure checkboxsizg(var f: text; view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   icheckboxsizg(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure checkboxsizg(view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   icheckboxsizg(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure checkboxsiz(var f: text; view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   icheckboxsiz(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure checkboxsiz(view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   icheckboxsiz(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create checkbox

Creates a standard checkbox within the specified rectangle, on the given
window.

******************************************************************************}

procedure icheckboxg(win: winptr; x1, y1, x2, y2: integer; view s: string; 
                     id: integer);

var wp: wigptr; { widget pointer }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, s, id, wtcheckbox, 0, wp)

end;

procedure icheckbox(win: winptr; x1, y1, x2, y2: integer; view s: string; 
                    id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   icheckboxg(win, x1, y1, x2, y2, s, id) { create button graphical }

end;

procedure checkboxg(var f: text; x1, y1, x2, y2: integer; view s: string; 
                   id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   icheckboxg(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure checkboxg(x1, y1, x2, y2: integer; view s: string; 
                            id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   icheckboxg(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

procedure checkbox(var f: text; x1, y1, x2, y2: integer; view s: string; 
                   id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   icheckbox(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure checkbox(x1, y1, x2, y2: integer; view s: string; 
                            id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   icheckbox(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard radio button size

Finds the minimum size for a radio button. Given the face string, the minimum
size of a radio button is calculated and returned.

******************************************************************************}

procedure iradiobuttonsizg(win: winptr; view s: string; var w, h: integer);

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    dc: integer; { dc for screen }

begin

   refer(win); { don't need the window data }

   dc := sc_getwindowdc(0); { get screen dc }
   if dc = 0 then winerr; { process windows error }
   b := sc_gettextextentpoint32(dc, s, sz); { get sizing }
   if not b then winerr; { process windows error }
   { We needed to add a fudge factor for the space between the checkbox, the
     left edge of the widget, and the left edge of the text. }
   w := sz.cx+sc_getsystemmetrics(sc_sm_cxmenucheck)+6; { return size }
   h := sz.cy

end;

procedure iradiobuttonsiz(win: winptr; view s: string; var w, h: integer);

begin

   iradiobuttonsizg(win, s, w, h); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1

end;

procedure radiobuttonsizg(var f: text; view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iradiobuttonsizg(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure radiobuttonsizg(view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iradiobuttonsizg(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure radiobuttonsiz(var f: text; view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iradiobuttonsiz(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure radiobuttonsiz(view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iradiobuttonsiz(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create radio button

Creates a standard radio button within the specified rectangle, on the given
window.

******************************************************************************}

procedure iradiobuttong(win: winptr; x1, y1, x2, y2: integer; view s: string; 
                     id: integer);

var wp: wigptr; { widget pointer }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, s, id, wtradiobutton, 0, wp)

end;

procedure iradiobutton(win: winptr; x1, y1, x2, y2: integer; view s: string; 
                    id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   iradiobuttong(win, x1, y1, x2, y2, s, id) { create button graphical }

end;

procedure radiobuttong(var f: text; x1, y1, x2, y2: integer; view s: string; 
                      id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iradiobuttong(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure radiobuttong(x1, y1, x2, y2: integer; view s: string;
                               id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iradiobuttong(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

procedure radiobutton(var f: text; x1, y1, x2, y2: integer; view s: string; 
                   id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iradiobutton(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure radiobutton(x1, y1, x2, y2: integer; view s: string; 
                            id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iradiobutton(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard group size

Finds the minimum size for a group. Given the face string, the minimum
size of a group is calculated and returned.

******************************************************************************}

procedure igroupsizg(win: winptr; view s: string; cw, ch: integer;
                     var w, h, ox, oy: integer);

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    dc: integer; { dc for screen }

begin

   refer(win); { don't need the window data }

   dc := sc_getwindowdc(0); { get screen dc }
   if dc = 0 then winerr; { process windows error }
   b := sc_gettextextentpoint32(dc, s, sz); { get sizing }
   if not b then winerr; { process windows error }
   { Use the string sizing, and rules of thumb for the edges }
   w := sz.cx+7*2; { return size }
   { if string is greater than width plus edges, use the string. }
   if cw+7*2 > w then w := cw+7*2;
   h := sz.cy+ch+5*2;
   { set offset to client area }
   ox := 5;
   oy := sz.cy

end;

procedure igroupsiz(win: winptr; view s: string; cw, ch: integer;
                    var w, h, ox, oy: integer);

begin

   { convert client sizes to graphical }
   cw := cw*win^.charspace;
   ch := ch*win^.linespace;
   igroupsizg(win, s, cw, ch, w, h, ox, oy); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1;
   ox := (ox-1) div win^.charspace+1;
   oy := (oy-1) div win^.linespace+1

end;

procedure groupsizg(var f: text; view s: string; cw, ch: integer;
                    var w, h, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   igroupsizg(win, s, cw, ch, w, h, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

overload procedure groupsizg(view s: string; cw, ch: integer;
                             var w, h, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   igroupsizg(win, s, cw, ch, w, h, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

procedure groupsiz(var f: text; view s: string; cw, ch: integer;
                   var w, h, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   igroupsiz(win, s, cw, ch, w, h, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

overload procedure groupsiz(view s: string; cw, ch: integer;
                            var w, h, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   igroupsiz(win, s, cw, ch, w, h, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create group box

Creates a group box, which is really just a decorative feature that gererates
no messages. It is used as a background for other widgets.

******************************************************************************}

procedure igroupg(win: winptr; x1, y1, x2, y2: integer; view s: string; 
                     id: integer);

var wp: wigptr; { widget pointer }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, s, id, wtgroup, 0, wp)

end;

procedure igroup(win: winptr; x1, y1, x2, y2: integer; view s: string; 
                    id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   igroupg(win, x1, y1, x2, y2, s, id) { create button graphical }

end;

procedure groupg(var f: text; x1, y1, x2, y2: integer; view s: string; 
                 id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   igroupg(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure groupg(x1, y1, x2, y2: integer; view s: string; 
                         id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   igroupg(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

procedure group(var f: text; x1, y1, x2, y2: integer; view s: string; 
                   id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   igroup(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure group(x1, y1, x2, y2: integer; view s: string; 
                            id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   igroup(win, x1, y1, x2, y2, s, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create background box

Creates a background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

******************************************************************************}

procedure ibackgroundg(win: winptr; x1, y1, x2, y2: integer; id: integer);

var wp: wigptr; { widget pointer }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, '', id, wtbackground, 0, wp)

end;

procedure ibackground(win: winptr; x1, y1, x2, y2: integer; id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   ibackgroundg(win, x1, y1, x2, y2, id) { create button graphical }

end;

procedure backgroundg(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ibackgroundg(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure backgroundg(x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ibackgroundg(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

procedure background(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ibackground(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure background(x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ibackground(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard vertical scrollbar size

Finds the minimum size for a vertical scrollbar. The minimum size of a vertical
scrollbar is calculated and returned.

******************************************************************************}

procedure iscrollvertsizg(win: winptr; var w, h: integer);

begin

   refer(win); { don't need the window data }
   { get system values for scroll bar arrow width and height, for which there
     are two. }
   w := sc_getsystemmetrics(sc_sm_cxvscroll);
   h := sc_getsystemmetrics(sc_sm_cyvscroll)*2

end;

procedure iscrollvertsiz(win: winptr; var w, h: integer);

begin

   refer(win); { not used }

   { Use fixed sizes, as this looks best }
   w := 2;
   h := 2

end;

procedure scrollvertsizg(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollvertsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure scrollvertsizg(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollvertsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure scrollvertsiz(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollvertsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure scrollvertsiz(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollvertsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create vertical scrollbar

Creates a vertical scrollbar.

******************************************************************************}

procedure iscrollvertg(win: winptr; x1, y1, x2, y2: integer; id: integer);

var wp: wigptr;        { widget pointer }
    si: sc_scrollinfo; { scroll information structure }
    b:  boolean;       { return value }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, '', id, wtscrollvert, 0, wp);
   { The scroll set for windows is arbitrary. We expand that to 0..maxint on
     messages. }
   unlockmain; { end exclusive access }
   b := sc_setscrollrange(wp^.han, sc_sb_ctl, 0, 255, false);
   lockmain; { start exclusive access }
   if not b then winerr; { process windows error }
   { retrieve the default size of slider }
   si.cbsize := sc_scrollinfo_len; { set size }
   si.fmask := sc_sif_page; { set page size }
   unlockmain; { end exclusive access }
   b := sc_getscrollinfo(wp^.han, sc_sb_ctl, si);
   lockmain; { start exclusive access }
   if not b then winerr; { process windows error }
   wp^.siz := si.npage { get size }

end;

procedure iscrollvert(win: winptr; x1, y1, x2, y2: integer; id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   iscrollvertg(win, x1, y1, x2, y2, id) { create button graphical }

end;

procedure scrollvertg(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollvertg(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure scrollvertg(x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollvertg(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

procedure scrollvert(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollvert(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure scrollvert(x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollvert(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard horizontal scrollbar size

Finds the minimum size for a horizontal scrollbar. The minimum size of a 
horizontal scrollbar is calculated and returned.

******************************************************************************}

procedure iscrollhorizsizg(win: winptr; var w, h: integer);

begin

   refer(win); { don't need the window data }
   { get system values for scroll bar arrow width and height, for which there
     are two. }
   w := sc_getsystemmetrics(sc_sm_cxhscroll)*2;
   h := sc_getsystemmetrics(sc_sm_cyhscroll)

end;

procedure iscrollhorizsiz(win: winptr; var w, h: integer);

begin

   refer(win); { not used }

   { Use fixed sizes, as this looks best }
   w := 2;
   h := 1

end;

procedure scrollhorizsizg(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollhorizsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure scrollhorizsizg(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollhorizsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure scrollhorizsiz(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollhorizsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure scrollhorizsiz(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollhorizsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create horizontal scrollbar

Creates a horizontal scrollbar.

******************************************************************************}

procedure iscrollhorizg(win: winptr; x1, y1, x2, y2: integer; id: integer);

var wp: wigptr;        { widget pointer }
    si: sc_scrollinfo; { scroll information structure }
    b:  boolean;       { return value }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, '', id, wtscrollhoriz, 0, wp);
   { The scroll set for windows is arbitrary. We expand that to 0..maxint on
     messages. }
   unlockmain; { end exclusive access }
   b := sc_setscrollrange(wp^.han, sc_sb_ctl, 0, 255, false);
   lockmain; { start exclusive access }
   if not b then winerr; { process windows error }
   { retrieve the default size of slider }
   si.cbsize := sc_scrollinfo_len; { set size }
   si.fmask := sc_sif_page; { set page size }
   unlockmain; { end exclusive access }
   b := sc_getscrollinfo(wp^.han, sc_sb_ctl, si);
   lockmain; { start exclusive access }
   if not b then winerr; { process windows error }
   wp^.siz := si.npage { get size }

end;

procedure iscrollhoriz(win: winptr; x1, y1, x2, y2: integer; id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   iscrollhorizg(win, x1, y1, x2, y2, id) { create button graphical }

end;

procedure scrollhorizg(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollhorizg(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure scrollhorizg(x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollhorizg(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

procedure scrollhoriz(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollhoriz(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure scrollhoriz(x1, y1, x2, y2: integer; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollhoriz(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

******************************************************************************}

procedure iscrollpos(win: winptr; id: integer; r: integer);

var wp: wigptr;  { widget pointer }
    rv: integer; { return value }
    f:  real;    { floating temp }
    p:  integer; { calculated position to set }

begin

   if r < 0 then error(einvspos); { invalid position }
   with win^ do begin { in windows context }

      if not visible then winvis(win); { make sure we are displayed }
      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      f := r; { place position in float }
      { clamp to max }
      if f*(255-wp^.siz)/maxint > 255 then p := 255
      else p := round(f*(255-wp^.siz)/maxint);
      unlockmain; { end exclusive access }
      rv := sc_setscrollpos(wp^.han, sc_sb_ctl, p, true);
      lockmain { start exclusive access }

   end

end;

procedure scrollpos(var f: text; id: integer; r: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollpos(win, id, r); { execute }
   unlockmain { end exclusive access }

end;

overload procedure scrollpos(id: integer; r: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollpos(win, id, r); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

******************************************************************************}

procedure iscrollsiz(win: winptr; id: integer; r: integer);

var wp:  wigptr;        { widget pointer }
    rv:  integer;       { return value }
    si:  sc_scrollinfo; { scroll information structure }

begin

   if r < 0 then error(einvssiz); { invalid scrollbar size }
   with win^ do begin { in windows context }

      if not visible then winvis(win); { make sure we are displayed }
      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      si.cbsize := sc_scrollinfo_len; { set size }
      si.fmask := sc_sif_page; { set page size }
      si.nmin := 0; { no min }
      si.nmax := 0; { no max }
      si.npage := r div $800000; { set size }
      si.npos := 0; { no position }
      si.ntrackpos := 0; { no track position }
      unlockmain; { end exclusive access }
      rv := sc_setscrollinfo(wp^.han, sc_sb_ctl, si, true);
      lockmain; { start exclusive access }
      wp^.siz := r div $800000; { set size }

   end

end;

procedure scrollsiz(var f: text; id: integer; r: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iscrollsiz(win, id, r); { execute }
   unlockmain { end exclusive access }

end;

overload procedure scrollsiz(id: integer; r: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iscrollsiz(win, id, r); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Control window procedure for number edit box

This message handler is to allow us to capture the carriage return from an
number edit box, and turn that into a message. It also restricts input to the
box to numeric characters.

******************************************************************************}

function wndprocnum(hwnd, imsg, wparam, lparam: integer): integer;

var r:   integer;   { result }
    wh:  integer;   { parent window handle }
    lfn: ss_filhdl; { logical number for parent window }
    win: winptr;    { parent window data }
    wp:  wigptr;    { widget pointer }
    s:   packed array [1..100] of char; { buffer for edit string }
    v:   integer;   { value from edit control }
    err: boolean;   { error parsing value }

{i: integer;}

begin                                              

   refer(hwnd, imsg, wparam, lparam); { these aren't presently used }
{;prtstr('wndprocnum: msg: ');
;prtmsgu(hwnd, imsg, wparam, lparam);}

   { We need to find out who we are talking to. }
   lockmain; { start exclusive access }
   wh := sc_getparent(hwnd); { get the widget parent handle }
   lfn := hwn2lfn(wh); { get the logical window number }
   win := lfn2win(lfn); { index window from logical number }
   wp := fndwighan(win, hwnd); { find the widget from that }
   unlockmain; { end exclusive access }
   r := 0; { set no error }
   { check its a character }
   if imsg = sc_wm_char then begin

      if wp^.enb then begin { is the widget enabled ? }

         { check control is receiving a carriage return }
         if wparam = ord('\cr') then begin

            r := sc_getwindowtext(wp^.han2, s); { get contents of edit }
            v := intv(s, err); { get value }
             { Send edit sends cr message to parent window, with widget logical
               number embedded as wparam. }
            if not err and (v >= wp^.low) and (v <= wp^.high) then
               putmsg(wh, umnumcr, wp^.id, v) { send select message }
            else
               { Send the message on to its owner, this will ring the bell in
                 Windows XP. }
               r := sc_callwindowproc(wp^.wprc, hwnd, imsg, wparam, lparam)

         end else begin

            { Check valid numerical character, with backspace control. If not,
              replace with carriage return. This will cause the control to emit
              an error, a bell in Windows XP. }
            if not (chr(wparam) in ['0'..'9', '+', '-', '\bs']) then 
               wparam := ord('\cr');
            r := sc_callwindowproc(wp^.wprc, hwnd, imsg, wparam, lparam)

         end

      end

   end else
      { send the message on to its owner }
      r := sc_callwindowproc(wp^.wprc, hwnd, imsg, wparam, lparam);

   wndprocnum := r { return result }

end;

{******************************************************************************

Find minimum/standard number select box size

Finds the minimum size for a number select box. The minimum size of a number
select box is calculated and returned.

******************************************************************************}

procedure inumselboxsizg(win: winptr; l, u: integer; var w, h: integer);

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    dc: integer; { dc for screen }

begin

   refer(win); { don't need the window data }
   refer(l); { don't need lower bound }

   { get size of text }
   dc := sc_getwindowdc(0); { get screen dc }
   if dc = 0 then winerr; { process windows error }
   if u > 9 then b := sc_gettextextentpoint32(dc, '00', sz) { get sizing }
   else b := sc_gettextextentpoint32(dc, '0', sz); { get sizing }
   if not b then winerr; { process windows error }
   { width of text, plus up/down arrows, and border and divider lines }
   w := sz.cx+sc_getsystemmetrics(sc_sm_cxvscroll)+4;
   h := sz.cy+2 { height of text plus border lines }

end;

procedure inumselboxsiz(win: winptr; l, u: integer; var w, h: integer);

begin

   inumselboxsizg(win, l, u, w, h); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1

end;

procedure numselboxsizg(var f: text; l, u: integer; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   inumselboxsizg(win, l, u, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure numselboxsizg(l, u: integer; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   inumselboxsizg(win, l, u, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure numselboxsiz(var f: text; l, u: integer; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   inumselboxsiz(win, l, u, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure numselboxsiz(l, u: integer; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   inumselboxsiz(win, l, u, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create number selector

Creates an up/down control for numeric selection.

******************************************************************************}

procedure inumselboxg(win: winptr; x1, y1, x2, y2: integer; l, u: integer;
                      id: integer);

var ip:  imptr;   { intratask message pointer }
    wp:  wigptr;  { widget pointer }
    br:  boolean; { result }
    udw: integer; { up/down control width }

begin

   with win^ do begin

      if not visible then winvis(win); { make sure we are displayed }
      { search previous definition with same id }
      if fndwig(win, id) <> nil then error(ewigdup); { duplicate widget id }

      { Number select is a composite control, and it will send messages
        immediately after creation, as the different components talk to each
        other. Because of this, we must create a widget entry first, even if
        it is incomplete. }
      getwig(win, wp); { get new widget }
      wp^.id := id; { place button id }
      wp^.typ := wtnumselbox; { place type }
      wp^.han := 0; { clear handles }
      wp^.han2 := 0;
      wp^.low := l; { place limits }
      wp^.high := u;
      { get width of up/down control (same as scroll arrow) }
      udw := sc_getsystemmetrics(sc_sm_cxhscroll);
      { If the width is not enough for the control to appear, force it. }
      if x2-x1+1 < udw then x2 := x1+udw-1;
      getitm(ip); { get a im pointer }
      ip^.im := imupdown; { set is up/down control }
      ip^.udflg := sc_ws_child or sc_ws_visible or sc_ws_border or sc_uds_setbuddyint;
      ip^.udx := x1-1;   
      ip^.udy := y1-1;   
      ip^.udcx := x2-x1+1;   
      ip^.udcy := y2-y1+1;  
      ip^.udpar := winhan; 
      ip^.udid := id;  
      ip^.udinst := sc_getmodulehandle_n;
      ip^.udup := u;  
      ip^.udlow := l; 
      ip^.udpos := l; 
      br := sc_postmessage(dispwin, umim, itm2int(ip), 0);
      if not br then winerr; { process windows error }
      waitim(imupdown, ip); { wait for the return }
      wp^.han := ip^.udhan; { place control handle }
      wp^.han2 := ip^.udbuddy; { place buddy handle }
      putitm(ip); { release im }
      { place our subclass handler for the edit control }
      wp^.wprc := sc_getwindowlong(wp^.han2, sc_gwl_wndproc); 
      if wp^.wprc = 0 then winerr; { process windows error }
      r := sc_setwindowlong(wp^.han2, sc_gwl_wndproc, sc_wndprocadr(wndprocnum)); 
      if r = 0 then winerr; { process windows error }

   end

end;

procedure inumselbox(win: winptr; x1, y1, x2, y2: integer; l, u: integer; 
                     id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := x2*win^.charspace;
   y2 := y2*win^.linespace;
   inumselboxg(win, x1, y1, x2, y2, l, u, id) { create button graphical }

end;

procedure numselboxg(var f: text; x1, y1, x2, y2: integer; l, u: integer;
                     id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   inumselboxg(win, x1, y1, x2, y2, l, u, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure numselboxg(x1, y1, x2, y2: integer; l, u: integer;
                              id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   inumselboxg(win, x1, y1, x2, y2, l, u, id); { execute }
   unlockmain { end exclusive access }

end;

procedure numselbox(var f: text; x1, y1, x2, y2: integer; l, u: integer;
                     id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   inumselbox(win, x1, y1, x2, y2, l, u, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure numselbox(x1, y1, x2, y2: integer; l, u: integer;
                              id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   inumselbox(win, x1, y1, x2, y2, l, u, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Control window procedure for edit box

This message handler is to allow us to capture the carriage return from an
edit box, and turn that into a message.

******************************************************************************}

function wndprocedit(hwnd, imsg, wparam, lparam: integer): integer;

var r:   integer;   { result }
    wh:  integer;   { parent window handle }
    lfn: ss_filhdl; { logical number for parent window }
    win: winptr;    { parent window data }
    wp:  wigptr;    { widget pointer }

begin                                              

   refer(hwnd, imsg, wparam, lparam); { these aren't presently used }
{;prtstr('wndprocedit: msg: ');
;prtmsgu(hwnd, imsg, wparam, lparam);}

   { We need to find out who we are talking to. }
   wh := sc_getparent(hwnd); { get the widget parent handle }
   lfn := hwn2lfn(wh); { get the logical window number }
   win := lfn2win(lfn); { index window from logical number }
   wp := fndwighan(win, hwnd); { find the widget from that }
   { check control is receiving a carriage return }
   if (imsg = sc_wm_char) and (wparam = ord('\cr')) then
       { Send edit sends cr message to parent window, with widget logical
         number embedded as wparam. }
      putmsg(wh, umeditcr, wp^.id, 0)
   else
      { send the message on to its owner }
      r := sc_callwindowproc(wp^.wprc, hwnd, imsg, wparam, lparam);

   wndprocedit := r { return result }

end;

{******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

******************************************************************************}

procedure ieditboxsizg(win: winptr; view s: string; var w, h: integer);

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    dc: integer; { dc for screen }

begin

   refer(win); { don't need the window data }

   dc := sc_getwindowdc(0); { get screen dc }
   if dc = 0 then winerr; { process windows error }
   b := sc_gettextextentpoint32(dc, s, sz); { get sizing }
   if not b then winerr; { process windows error }
   { add borders to size }
   w := sz.cx+4;
   h := sz.cy+2

end;

procedure ieditboxsiz(win: winptr; view s: string; var w, h: integer);

begin

   ieditboxsizg(win, s, w, h); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1

end;

procedure editboxsizg(var f: text; view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ieditboxsizg(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure editboxsizg(view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ieditboxsizg(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure editboxsiz(var f: text; view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ieditboxsiz(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure editboxsiz(view s: string; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ieditboxsiz(win, s, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create edit box

Creates single line edit box

******************************************************************************}

procedure ieditboxg(win: winptr; x1, y1, x2, y2: integer; id: integer);

var wp: wigptr; { widget pointer }
    r: integer;

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, '', id, wteditbox, 0, wp);
   { get the windows internal procedure for subclassing }
   wp^.wprc := sc_getwindowlong(wp^.han, sc_gwl_wndproc); 
   if wp^.wprc = 0 then winerr; { process windows error }
   r := sc_setwindowlong(wp^.han, sc_gwl_wndproc, sc_wndprocadr(wndprocedit)); 
   if r = 0 then winerr; { process windows error }

end;

procedure ieditbox(win: winptr; x1, y1, x2, y2: integer; id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   ieditboxg(win, x1, y1, x2, y2, id) { create button graphical }

end;

procedure editboxg(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ieditboxg(win, x1,y1, x2,y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure editboxg(x1, y1, x2, y2: integer; id: integer);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ieditboxg(win, x1,y1, x2,y2, id); { execute }
   unlockmain { end exclusive access }

end;

procedure editbox(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ieditbox(win, x1,y1, x2,y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure editbox(x1, y1, x2, y2: integer; id: integer);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ieditbox(win, x1,y1, x2,y2, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

******************************************************************************}

procedure iprogbarsizg(win: winptr; var w, h: integer);

begin

   refer(win); { don't need the window data }
   { Progress bars are arbitrary for sizing. We choose a size that allows for
     20 bar elements. Note that the size of the blocks in a Windows progress
     bar are ratioed to the height. }
   w := 20*14+2;
   h := 20+2

end;

procedure iprogbarsiz(win: winptr; var w, h: integer);

begin

   iprogbarsizg(win, w, h); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1

end;

procedure progbarsizg(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iprogbarsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure progbarsizg(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iprogbarsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure progbarsiz(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iprogbarsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure progbarsiz(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iprogbarsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create progress bar

Creates a progress bar.

******************************************************************************}

procedure iprogbarg(win: winptr; x1, y1, x2, y2: integer; id: integer);

var wp:  wigptr;  { widget pointer }
    r:   integer; { return value }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   { create the progress bar }
   widget(win, x1, y1, x2, y2, '', id, wtprogressbar, 0, wp);
   { use 0..maxint ratio }
   unlockmain; { end exclusive access }
   r := sc_sendmessage(wp^.han, sc_pbm_setrange32, 0, maxint);
   lockmain { start exclusive access }

end;

procedure iprogbar(win: winptr; x1, y1, x2, y2: integer; id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   iprogbarg(win, x1, y1, x2, y2, id) { create button graphical }

end;

procedure progbarg(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iprogbarg(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure progbarg(x1, y1, x2, y2: integer; id: integer);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iprogbarg(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

procedure progbar(var f: text; x1, y1, x2, y2: integer; id: integer);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iprogbar(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure progbar(x1, y1, x2, y2: integer; id: integer);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iprogbar(win, x1, y1, x2, y2, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set progress bar position

Sets the position of a progress bar, from 0 to maxint.

******************************************************************************}

procedure iprogbarpos(win: winptr; id: integer; pos: integer);

var wp:  wigptr; { widget pointer }
    r:  integer;

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   if pos < 0 then error(eprgpos); { bad position }
   with win^ do begin

      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      { set the range }
      unlockmain; { end exclusive access }
      r := sc_sendmessage(wp^.han, sc_pbm_setpos, pos, 0);
      lockmain { start exclusive access }

   end;

end;

procedure progbarpos(var f: text; id: integer; pos: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iprogbarpos(win, id, pos); { execute }
   unlockmain { end exclusive access }

end;

overload procedure progbarpos(id: integer; pos: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iprogbarpos(win, id, pos); { execute }
   unlockmain { end exclusive access }

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

procedure ilistboxsizg(win: winptr; sp: strptr; var w, h: integer);

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    dc: integer; { dc for screen }
    mw: integer; { max width }

begin

   refer(win); { don't need the window data }

   w := 4; { set minimum overhead }
   h := 2;
   while sp <> nil do begin { traverse string list }

      dc := sc_getwindowdc(0); { get screen dc }
      if dc = 0 then winerr; { process windows error }
      b := sc_gettextextentpoint32(dc, sp^.str^, sz); { get sizing }
      if not b then winerr; { process windows error }
      { add borders to size }
      mw := sz.cx+4;
      if mw > w then w := mw; { set new maximum }
      h := h+sz.cy;
      sp := sp^.next { next string }

   end

end;

procedure ilistboxsiz(win: winptr; sp: strptr; var w, h: integer);

begin

   ilistboxsizg(win, sp, w, h); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1

end;

procedure listboxsizg(var f: text; sp: strptr; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ilistboxsizg(win, sp, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure listboxsizg(sp: strptr; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ilistboxsizg(win, sp, w, h); { get size }
   unlockmain { end exclusive access }

end;
                              
procedure listboxsiz(var f: text; sp: strptr; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ilistboxsiz(win, sp, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure listboxsiz(sp: strptr; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ilistboxsiz(win, sp, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create list box

Creates a list box. Fills it with the string list provided.

******************************************************************************}

procedure ilistboxg(win: winptr; x1, y1, x2, y2: integer; sp: strptr; 
                    id: integer);

var wp: wigptr;  { widget pointer }
    r:  integer; { return value }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, '', id, wtlistbox, 0, wp);
   while sp <> nil do begin { add strings to list }

      unlockmain; { end exclusive access }
      r := sc_sendmessage(wp^.han, sc_lb_addstring, sp^.str^); { add string }
      lockmain; { start exclusive access }
      if r = -1 then error(estrspc); { out of string space }
      sp := sp^.next { next string }

   end

end;

procedure ilistbox(win: winptr; x1, y1, x2, y2: integer; sp: strptr; 
                   id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   ilistboxg(win, x1, y1, x2, y2, sp, id) { create button graphical }

end;

procedure listboxg(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                   id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ilistboxg(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure listboxg(x1, y1, x2, y2: integer; sp: strptr; 
                            id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ilistboxg(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

procedure listbox(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                   id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   ilistbox(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure listbox(x1, y1, x2, y2: integer; sp: strptr; 
                            id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   ilistbox(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard dropbox size

Finds the minimum size for a dropbox. Given the face string, the minimum size of
a dropbox is calculated and returned, for both the "open" and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

******************************************************************************}

procedure idropboxsizg(win: winptr; sp: strptr; 
                       var cw, ch, ow, oh: integer);

{ I can't find a reasonable system metrics version of the drop arrow demensions,
  so they are hardcoded here. }
const darrowx = 17;
      darrowy = 20;

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    dc: integer; { dc for screen }

{ find size for single line }

procedure getsiz(view s: string);

begin

   dc := sc_getwindowdc(0); { get screen dc }
   if dc = 0 then winerr; { process windows error }
   b := sc_gettextextentpoint32(dc, s, sz); { get sizing }
   if not b then winerr { process windows error }

end;

begin

   refer(win); { don't need the window data }

   { calculate first line }
   getsiz(sp^.str^); { find sizing for line }
   { Find size of string x, drop arrow width, box edges, and add fudge factor
     to space text out. }
   cw := sz.cx+darrowx+sc_getsystemmetrics(sc_sm_cxedge)*2+4;
   ow := cw; { open is the same }
   { drop arrow height+shadow overhead+drop box bounding }
   oh := darrowy+sc_getsystemmetrics(sc_sm_cyedge)*2+2;
   { drop arrow height+shadow overhead }
   ch := darrowy+sc_getsystemmetrics(sc_sm_cyedge)*2;
   { add all lines to drop box section }
   while sp <> nil do begin { traverse string list }

      getsiz(sp^.str^); { find sizing for this line }
      { find open width on this string only }
      ow := sz.cx+darrowx+sc_getsystemmetrics(sc_sm_cxedge)*2+4;
      if ow > cw then cw := ow; { larger than closed width, set new max }
      oh := oh+sz.cy; { add to open height }
      sp := sp^.next; { next string }

   end;
   ow := cw { set maximum open width }

end;

procedure idropboxsiz(win: winptr; sp: strptr; var cw, ch, ow, oh: integer);

begin

   idropboxsizg(win, sp, cw, ch, ow, oh); { get size }
   { change graphical size to character }
   cw := (cw-1) div win^.charspace+1;
   ch := (ch-1) div win^.linespace+1;
   ow := (ow-1) div win^.charspace+1;
   oh := (oh-1) div win^.linespace+1

end;

procedure dropboxsizg(var f: text; sp: strptr; var cw, ch, ow, oh: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   idropboxsizg(win, sp, cw, ch, ow, oh); { get size }
   unlockmain { end exclusive access }

end;

overload procedure dropboxsizg(sp: strptr; var cw, ch, ow, oh: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idropboxsizg(win, sp, cw, ch, ow, oh); { get size }
   unlockmain { end exclusive access }

end;

procedure dropboxsiz(var f: text; sp: strptr; var cw, ch, ow, oh: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   idropboxsiz(win, sp, cw, ch, ow, oh); { get size }
   unlockmain { end exclusive access }

end;

overload procedure dropboxsiz(sp: strptr; var cw, ch, ow, oh: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idropboxsiz(win, sp, cw, ch, ow, oh); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create dropdown box

Creates a dropdown box. Fills it with the string list provided.

******************************************************************************}

procedure idropboxg(win: winptr; x1, y1, x2, y2: integer; sp: strptr; 
                    id: integer);

var wp:  wigptr;  { widget pointer }
    sp1: strptr;  { string pointer }
    r:   integer; { return value }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, '', id, wtdropbox, 0, wp);
   sp1 := sp; { index top of string list }
   while sp1 <> nil do begin { add strings to list }

      unlockmain; { end exclusive access }
      r := sc_sendmessage(wp^.han, sc_cb_addstring, sp1^.str^); { add string }
      lockmain; { start exclusive access }
      if r = -1 then error(estrspc); { out of string space }
      sp1 := sp1^.next { next string }

   end;
   unlockmain; { end exclusive access }
   r := sc_sendmessage(wp^.han, sc_cb_setcursel, 0, 0);
   lockmain; { start exclusive access }
   if r = -1 then error(esystem) { should not happen }

end;

procedure idropbox(win: winptr; x1, y1, x2, y2: integer; sp: strptr; 
                   id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   idropboxg(win, x1, y1, x2, y2, sp, id) { create button graphical }

end;

procedure dropboxg(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                   id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   idropboxg(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure dropboxg(x1, y1, x2, y2: integer; sp: strptr; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idropboxg(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

procedure dropbox(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                   id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   idropbox(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure dropbox(x1, y1, x2, y2: integer; sp: strptr; id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idropbox(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

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

procedure idropeditboxsizg(win: winptr; sp: strptr; 
                           var cw, ch, ow, oh: integer);

{ I can't find a reasonable system metrics version of the drop arrow demensions,
  so they are hardcoded here. }
const darrowx = 17;
      darrowy = 20;

var sz: sc_size; { size holder }
    b:  boolean; { return value }
    dc: integer; { dc for screen }

{ find size for single line }

procedure getsiz(view s: string);

begin

   dc := sc_getwindowdc(0); { get screen dc }
   if dc = 0 then winerr; { process windows error }
   b := sc_gettextextentpoint32(dc, s, sz); { get sizing }
   if not b then winerr { process windows error }

end;

begin

   refer(win); { don't need the window data }

   { calculate first line }
   getsiz(sp^.str^); { find sizing for line }
   { Find size of string x, drop arrow width, box edges, and add fudge factor
     to space text out. }
   cw := sz.cx+darrowx+sc_getsystemmetrics(sc_sm_cxedge)*2+4;
   ow := cw; { open is the same }
   { drop arrow height+shadow overhead+drop box bounding }
   oh := darrowy+sc_getsystemmetrics(sc_sm_cyedge)*2+2;
   { drop arrow height+shadow overhead }
   ch := darrowy+sc_getsystemmetrics(sc_sm_cyedge)*2;
   { add all lines to drop box section }
   while sp <> nil do begin { traverse string list }

      getsiz(sp^.str^); { find sizing for this line }
      { find open width on this string only }
      ow := sz.cx+darrowx+sc_getsystemmetrics(sc_sm_cxedge)*2+4;
      if ow > cw then cw := ow; { larger than closed width, set new max }
      oh := oh+sz.cy; { add to open height }
      sp := sp^.next; { next string }

   end;
   ow := cw { set maximum open width }

end;

procedure idropeditboxsiz(win: winptr; sp: strptr; var cw, ch, ow, oh: integer);

begin

   idropeditboxsizg(win, sp, cw, ch, ow, oh); { get size }
   { change graphical size to character }
   cw := (cw-1) div win^.charspace+1;
   ch := (ch-1) div win^.linespace+1;
   ow := (ow-1) div win^.charspace+1;
   oh := (oh-1) div win^.linespace+1

end;

procedure dropeditboxsizg(var f: text; sp: strptr; var cw, ch, ow, oh: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   idropeditboxsizg(win, sp, cw, ch, ow, oh); { get size }
   unlockmain { end exclusive access }

end;

overload procedure dropeditboxsizg(sp: strptr; var cw, ch, ow, oh: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idropeditboxsizg(win, sp, cw, ch, ow, oh); { get size }
   unlockmain { end exclusive access }

end;

procedure dropeditboxsiz(var f: text; sp: strptr; var cw, ch, ow, oh: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   idropeditboxsiz(win, sp, cw, ch, ow, oh); { get size }
   unlockmain { end exclusive access }

end;

overload procedure dropeditboxsiz(sp: strptr; var cw, ch, ow, oh: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idropeditboxsiz(win, sp, cw, ch, ow, oh); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create dropdown edit box

Creates a dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

******************************************************************************}

procedure idropeditboxg(win: winptr; x1, y1, x2, y2: integer; sp: strptr; 
                        id: integer);

var wp:  wigptr;  { widget pointer }
    sp1: strptr;  { string pointer }
    r:   integer; { return value }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   widget(win, x1, y1, x2, y2, '', id, wtdropeditbox, 0, wp);
   sp1 := sp; { index top of string list }
   while sp1 <> nil do begin { add strings to list }

      unlockmain; { end exclusive access }
      r := sc_sendmessage(wp^.han, sc_cb_addstring, sp1^.str^); { add string }
      lockmain; { start exclusive access }
      if r = -1 then error(estrspc); { out of string space }
      sp1 := sp1^.next { next string }

   end

end;

procedure idropeditbox(win: winptr; x1, y1, x2, y2: integer; sp: strptr; 
                       id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   idropeditboxg(win, x1, y1, x2, y2, sp, id) { create button graphical }

end;

procedure dropeditboxg(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                       id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   idropeditboxg(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure dropeditboxg(x1, y1, x2, y2: integer; sp: strptr; 
                                id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idropeditboxg(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

procedure dropeditbox(var f: text; x1, y1, x2, y2: integer; sp: strptr; 
                       id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   idropeditbox(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure dropeditbox(x1, y1, x2, y2: integer; sp: strptr; 
                                id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   idropeditbox(win, x1, y1, x2, y2, sp, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard horizontal slider size

Finds the minimum size for a horizontal slider. The minimum size of a horizontal
slider is calculated and returned.

******************************************************************************}

procedure islidehorizsizg(win: winptr; var w, h: integer);

begin

   refer(win); { don't need the window data }

   { The width is that of an average slider. The height is what is needed to
     present the slider, tick marks, and 2 pixels of spacing around it. }
   w := 200;
   h := 32

end;

procedure islidehorizsiz(win: winptr; var w, h: integer);

begin

   islidehorizsizg(win, w, h); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1

end;

procedure slidehorizsizg(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   islidehorizsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure slidehorizsizg(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   islidehorizsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure slidehorizsiz(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   islidehorizsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure slidehorizsiz(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   islidehorizsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create horizontal slider

Creates a horizontal slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

******************************************************************************}

procedure islidehorizg(win: winptr; x1, y1, x2, y2: integer; mark: integer;
                       id: integer);

var wp: wigptr;  { widget pointer }
    r:  integer; { return value }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   if mark = 0 then  { tick marks enabled }
      widget(win, x1, y1, x2, y2, '', id, wtslidehoriz, sc_tbs_noticks, wp)
   else { tick marks enabled }
      widget(win, x1, y1, x2, y2, '', id, wtslidehoriz, 0, wp);
   { set tickmark frequency }
   unlockmain; { end exclusive access }
   r := sc_sendmessage(wp^.han, sc_tbm_setticfreq, mark, 0);
   lockmain { start exclusive access }

end;

procedure islidehoriz(win: winptr; x1, y1, x2, y2: integer; mark: integer; 
                      id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   islidehorizg(win, x1, y1, x2, y2, mark, id) { create button graphical }

end;

procedure slidehorizg(var f: text; x1, y1, x2, y2: integer; mark: integer;
                      id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   islidehorizg(win, x1, y1, x2, y2, mark, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure slidehorizg(x1, y1, x2, y2: integer; mark: integer;
                              id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   islidehorizg(win, x1, y1, x2, y2, mark, id); { execute }
   unlockmain { end exclusive access }

end;

procedure slidehoriz(var f: text; x1, y1, x2, y2: integer; mark: integer;
                      id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   islidehoriz(win, x1, y1, x2, y2, mark, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure slidehoriz(x1, y1, x2, y2: integer; mark: integer;
                              id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   islidehoriz(win, x1, y1, x2, y2, mark, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find minimum/standard vertical slider size

Finds the minimum size for a vertical slider. The minimum size of a vertical
slider is calculated and returned.

******************************************************************************}

procedure islidevertsizg(win: winptr; var w, h: integer);

begin

   refer(win); { don't need the window data }

   { The height is that of an average slider. The width is what is needed to
     present the slider, tick marks, and 2 pixels of spacing around it. }
   w := 32;
   h := 200

end;

procedure islidevertsiz(win: winptr; var w, h: integer);

begin

   islidevertsizg(win, w, h); { get size }
   { change graphical size to character }
   w := (w-1) div win^.charspace+1;
   h := (h-1) div win^.linespace+1

end;

procedure slidevertsizg(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   islidevertsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure slidevertsizg(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   islidevertsizg(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

procedure slidevertsiz(var f: text; var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   islidevertsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

overload procedure slidevertsiz(var w, h: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   islidevertsiz(win, w, h); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create vertical slider

Creates a vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

******************************************************************************}

procedure islidevertg(win: winptr; x1, y1, x2, y2: integer; mark: integer;
                      id: integer);

var wp:  wigptr;  { widget pointer }
    r:   integer; { return value }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   if mark = 0 then { tick marks off }
      widget(win, x1, y1, x2, y2, '', id, wtslidevert, sc_tbs_noticks, wp)
   else { tick marks enabled }
      widget(win, x1, y1, x2, y2, '', id, wtslidevert, 0, wp);
   { set tickmark frequency }
   unlockmain; { end exclusive access }
   r := sc_sendmessage(wp^.han, sc_tbm_setticfreq, mark, 0);
   lockmain { start exclusive access }

end;

procedure islidevert(win: winptr; x1, y1, x2, y2: integer; mark: integer; 
                      id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   islidevertg(win, x1, y1, x2, y2, mark, id) { create button graphical }

end;

procedure slidevertg(var f: text; x1, y1, x2, y2: integer; mark: integer;
                     id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   islidevertg(win, x1, y1, x2, y2, mark, id);
   unlockmain { end exclusive access }

end;

overload procedure slidevertg(x1, y1, x2, y2: integer; mark: integer;
                              id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   islidevertg(win, x1, y1, x2, y2, mark, id);
   unlockmain { end exclusive access }

end;

procedure slidevert(var f: text; x1, y1, x2, y2: integer; mark: integer;
                     id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   islidevert(win, x1, y1, x2, y2, mark, id);
   unlockmain { end exclusive access }

end;

overload procedure slidevert(x1, y1, x2, y2: integer; mark: integer;
                              id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   islidevert(win, x1, y1, x2, y2, mark, id);
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create and distroy useless widget

This appears to be a windows bug. When tab bars are created, they allow
themselves to be overritten by the parent. This only occurs on tab bars.
The workaround is to create a distroy a widget right after creating the
tab bar, since only the last widget created has this problem.

Bug: this is not getting deleted. I fixed it temporarily by making it
invisible.

******************************************************************************}

procedure uselesswidget(win: winptr);

var ip: imptr; { intratask message pointer }

begin

   with win^ do begin

      getitm(ip); { get a im pointer }
      ip^.im := imwidget; { set type is widget }
      copy(ip^.wigcls, 'static');
      copy(ip^.wigtxt, '');
      ip^.wigflg := sc_ws_child {or sc_ws_visible};
      ip^.wigx := 50;
      ip^.wigy := 50;
      ip^.wigw := 50;
      ip^.wigh := 50;
      ip^.wigpar := winhan;
      ip^.wigid := 0;
      ip^.wigmod := sc_getmodulehandle_n;
      { order widget to start }
      b := sc_postmessage(dispwin, umim, itm2int(ip), 0);
      if not b then winerr; { process windows error }
      { Wait for widget start, this also keeps our window going. } 
      waitim(imwidget, ip); { wait for the return }
      kilwin(ip^.wigwin); { kill widget }
      dispose(ip^.wigcls); { release class string }
      dispose(ip^.wigtxt); { release face text string }
      putitm(ip) { release im }

   end

end;

{******************************************************************************

Find minimum/standard tab bar size

Finds the minimum size for a tab bar. The minimum size of a tab bar is
calculated and returned.

******************************************************************************}

procedure itabbarsizg(win: winptr; tor: tabori; cw, ch: integer;
                      var w, h, ox, oy: integer);

begin

   refer(win); { don't need the window data }
   
   if (tor = toright) or (tor = toleft) then begin { vertical bar }

      w := 32; { set minimum width }
      h := 2+20*2; { set minimum height }
      w := w+cw; { add client space to width }
      if ch+4 > h then h := ch+4; { set to client if greater }
      if tor = toleft then begin

         ox := 28; { set offsets }
         oy := 4

      end else begin

         ox := 4; { set offsets }
         oy := 4

      end

   end else begin { horizontal bar }

      w := 2+20*2; { set minimum width, edges, arrows }
      h := 32; { set minimum height }
      if cw+4 > w then w := cw+4; { set to client if greater }
      h := h+ch; { add client space to height }
      if tor = totop then begin

         ox := 4; { set offsets }
         oy := 28

      end else begin

         ox := 4; { set offsets }
         oy := 4

      end

   end

end;

procedure itabbarsiz(win: winptr; tor: tabori; cw, ch: integer; 
                     var w, h, ox, oy: integer);

var gw, gh, gox, goy: integer;

begin

   { convert client sizes to graphical }
   cw := cw*win^.charspace;
   ch := ch*win^.linespace;
   itabbarsizg(win, tor, cw, ch, gw, gh, gox, goy); { get size }
   { change graphical size to character }
   w := (gw-1) div win^.charspace+1;
   h := (gh-1) div win^.linespace+1;
   ox := (gox-1) div win^.charspace+1;
   oy := (goy-1) div win^.linespace+1;
   { must make sure that client dosen't intrude on edges }
   if gw-gox-4 mod win^.charspace <> 0 then w := w+1;
   if gh-goy-4 mod win^.charspace <> 0 then h := h+1

end;

procedure tabbarsizg(var f: text; tor: tabori; cw, ch: integer; 
                     var w, h, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   itabbarsizg(win, tor, cw, ch, w, h, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

overload procedure tabbarsizg(tor: tabori; cw, ch: integer; 
                              var w, h, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   itabbarsizg(win, tor, cw, ch, w, h, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

procedure tabbarsiz(var f: text; tor: tabori; cw, ch: integer; 
                    var w, h, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   itabbarsiz(win, tor, cw, ch, w, h, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

overload procedure tabbarsiz(tor: tabori; cw, ch: integer; 
                             var w, h, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   itabbarsiz(win, tor, cw, ch, w, h, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Find client from tabbar size

Given a tabbar size and orientation, this routine gives the client size and 
offset. This is used where the tabbar size is fixed, but the client area is
flexible.

******************************************************************************}

procedure itabbarclientg(win: winptr; tor: tabori; w, h: integer; 
                         var cw, ch, ox, oy: integer);

begin

   refer(win); { don't need the window data }

   if (tor = toright) or (tor = toleft) then begin { vertical bar }

      { Find client height and width from total height and width minus
        tabbar overhead. }
      cw := w-32;
      ch := h-8;
      if tor = toleft then begin

         ox := 28; { set offsets }
         oy := 4

      end else begin

         ox := 4; { set offsets }
         oy := 4

      end

   end else begin { horizontal bar }

      { Find client height and width from total height and width minus
        tabbar overhead. }
      cw := w-8;
      ch := h-32;
      if tor = totop then begin

         ox := 4; { set offsets }
         oy := 28

      end else begin

         ox := 4; { set offsets }
         oy := 4

      end

   end

end;

procedure itabbarclient(win: winptr; tor: tabori; w, h: integer; 
                        var cw, ch, ox, oy: integer);

var gw, gh, gox, goy: integer;

begin

   { convert sizes to graphical }
   w := w*win^.charspace;
   h := h*win^.linespace;
   itabbarsizg(win, tor, w, h, gw, gh, gox, goy); { get size }
   { change graphical size to character }
   cw := (gw-1) div win^.charspace+1;
   ch := (gh-1) div win^.linespace+1;
   ox := (gox-1) div win^.charspace+1;
   oy := (goy-1) div win^.linespace+1;
   { must make sure that client dosen't intrude on edges }
   if gw-gox-4 mod win^.charspace <> 0 then w := w+1;
   if gh-goy-4 mod win^.charspace <> 0 then h := h+1

end;

procedure tabbarclientg(var f: text; tor: tabori; w, h: integer; 
                     var cw, ch, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   itabbarclientg(win, tor, w, h, cw, ch, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

overload procedure tabbarclientg(tor: tabori; w, h: integer; 
                                 var cw, ch, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   itabbarclientg(win, tor, w, h, cw, ch, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

procedure tabbarclient(var f: text; tor: tabori; w, h: integer; 
                    var cw, ch, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   itabbarclient(win, tor, w, h, cw, ch, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

overload procedure tabbarclient(tor: tabori; w, h: integer; 
                                var cw, ch, ox, oy: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   itabbarclient(win, tor, w, h, cw, ch, ox, oy); { get size }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Create tab bar

Creates a tab bar with the given orientation.

Bug: has strange overwrite mode where when the widget is first created, it
allows itself to be overwritten by the main window. This is worked around by
creating and distroying another widget.

******************************************************************************}

procedure itabbarg(win: winptr; x1, y1, x2, y2: integer; sp: strptr; 
                   tor: tabori; id: integer);

var wp:  wigptr;    { widget pointer }
    inx: integer;   { index for tabs }
    tcr: sc_tcitem; { tab attributes record }
    bs:  pstring;   { string buffer }
    i:   integer;   { idnex for string }
    m:   integer;   { maximum length of string }
    fl:  integer;   { flags }

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   fl := 0; { clear parameter flags }
   if (tor = toright) or (tor = toleft) then fl := fl+sc_tcs_vertical;
   if tor = toright then fl := fl+sc_tcs_right;
   if tor = tobottom then fl := fl+sc_tcs_bottom;
   widget(win, x1, y1, x2, y2, '', id, wttabbar, fl, wp);
   inx := 0; { set index }
   while sp <> nil do begin { add strings to list }

      { create a string buffer with space for terminating zero }
      m := max(sp^.str^); { get length }
      new(bs, m+1); { create buffer string }
      for i := 1 to m do bs^[i] := chr(chr2ascii(sp^.str^[i])); { copy }
      bs^[m+1] := chr(0); { place terminator }
      tcr.mask := sc_tcif_text; { set record contains text label }
      tcr.dwstate := 0; { clear state }
      tcr.dwstatemask := 0; { clear state mask }
      tcr.psztext := bs; { place string }
      tcr.iimage := -1; { no image }
      tcr.lparam := 0; { no parameter }
      unlockmain; { end exclusive access }
      r := sc_sendmessage(wp^.han, sc_tcm_insertitem, inx, tcr); { add string }
      lockmain; { start exclusive access }
      if r = -1 then error(etabbar); { can't create tab }
      dispose(bs); { release string buffer }
      sp := sp^.next; { next string }
      inx := inx+1 { next index }

   end;

   uselesswidget(win) { stop overwrite bug }

end;

procedure itabbar(win: winptr; x1, y1, x2, y2: integer; sp: strptr;
                  tor: tabori; id: integer);

begin

   { form graphical from character coordinates }
   x1 := (x1-1)*win^.charspace+1;
   y1 := (y1-1)*win^.linespace+1;
   x2 := (x2)*win^.charspace;
   y2 := (y2)*win^.linespace;
   itabbarg(win, x1, y1, x2, y2, sp, tor, id) { create button graphical }

end;

procedure tabbarg(var f: text; x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                  id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   itabbarg(win, x1, y1, x2, y2, sp, tor, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure tabbarg(x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                           id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   itabbarg(win, x1, y1, x2, y2, sp, tor, id); { execute }
   unlockmain { end exclusive access }

end;

procedure tabbar(var f: text; x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                  id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   itabbar(win, x1, y1, x2, y2, sp, tor, id); { execute }
   unlockmain { end exclusive access }

end;

overload procedure tabbar(x1, y1, x2, y2: integer; sp: strptr; tor: tabori;
                           id: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   itabbar(win, x1, y1, x2, y2, sp, tor, id); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

******************************************************************************}

procedure itabsel(win: winptr; id: integer; tn: integer);

var wp:  wigptr; { widget pointer }
    r:  integer;

begin

   if not win^.visible then winvis(win); { make sure we are displayed }
   if tn < 1 then error(etabsel); { bad tab select }
   with win^ do begin

      wp := fndwig(win, id); { find widget }
      if wp = nil then error(ewignf); { not found }
      { set the range }
      unlockmain; { end exclusive access }
      r := sc_sendmessage(wp^.han, sc_tcm_setcursel, tn-1, 0);
      lockmain { start exclusive access }

   end;

end;

procedure tabsel(var f: text; id: integer; tn: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   itabsel(win, id, tn); { execute }
   unlockmain { end exclusive access }

end;

overload procedure tabsel(id: integer; tn: integer);

var win: winptr; { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   itabsel(win, id, tn); { execute }
   unlockmain { end exclusive access }

end;

{******************************************************************************

Output message dialog

Outputs a message dialog with the given title and message strings.

******************************************************************************}

procedure alert(view title, message: string);

var ip: imptr;   { intratask message pointer }
    b:  boolean; { result }

procedure copy(var d: pstring; view s: string);

begin

   new(d, max(s));
   d^ := s

end;

begin

   lockmain; { start exclusive access }
   getitm(ip); { get a im pointer }
   ip^.im := imalert; { set is alert }
   copy(ip^.alttit, title); { copy strings }
   copy(ip^.altmsg, message);
   b := sc_postmessage(dialogwin, umim, itm2int(ip), 0);
   if not b then winerr; { process windows error }
   waitim(imalert, ip); { wait for the return }
   dispose(ip^.alttit); { free strings }
   dispose(ip^.altmsg);
   unlockmain { end exclusive access }

end;

{******************************************************************************

Display choose color dialog

Presents the choose color dialog, then returns the resulting color.

Bug: does not take the input color as the default.

******************************************************************************}

procedure querycolor(var r, g, b: integer);

var ip: imptr;   { intratask message pointer }
    br: boolean; { result }

begin

   lockmain; { start exclusive access }
   getitm(ip); { get a im pointer }
   ip^.im := imqcolor; { set is color query }
   ip^.clrred := r; { set colors }
   ip^.clrgreen := g;
   ip^.clrblue := b;
   br := sc_postmessage(dialogwin, umim, itm2int(ip), 0);
   if not br then winerr; { process windows error }
   waitim(imqcolor, ip); { wait for the return }
   r := ip^.clrred; { set new colors }
   g := ip^.clrgreen;
   b := ip^.clrblue;
   putitm(ip); { release im }
   unlockmain { end exclusive access }

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

var ip: imptr;   { intratask message pointer }
    br: boolean; { result }

begin

   lockmain; { start exclusive access }
   getitm(ip); { get a im pointer }
   ip^.im := imqopen; { set is open file query }
   ip^.opnfil := s; { set input string }
   br := sc_postmessage(dialogwin, umim, itm2int(ip), 0);
   if not br then winerr; { process windows error }
   waitim(imqopen, ip); { wait for the return }
   s := ip^.opnfil; { set output string }
   putitm(ip); { release im }
   unlockmain { end exclusive access }

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

var ip: imptr;   { intratask message pointer }
    br: boolean; { result }

begin

   lockmain; { start exclusive access }
   getitm(ip); { get a im pointer }
   ip^.im := imqsave; { set is open file query }
   ip^.opnfil := s; { set input string }
   br := sc_postmessage(dialogwin, umim, itm2int(ip), 0);
   if not br then winerr; { process windows error }
   waitim(imqsave, ip); { wait for the return }
   s := ip^.savfil; { set output string }
   putitm(ip); { release im }
   unlockmain { end exclusive access }

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

var ip: imptr;   { intratask message pointer }
    br: boolean; { result }

begin

   lockmain; { start exclusive access }
   { check string to large for dialog, accounting for trailing zero }
   if max(s^) > sc_findreplace_str_len-1 then error(efndstl);
   getitm(ip); { get a im pointer }
   ip^.im := imqfind; { set is find query }
   ip^.fndstr := s; { set input string }
   ip^.fndopt := opt; { set options }
   br := sc_postmessage(dialogwin, umim, itm2int(ip), 0);
   if not br then winerr; { process windows error }
   waitim(imqfind, ip); { wait for the return }
   s := ip^.fndstr; { set output string }
   opt := ip^.fndopt; { set output options }
   putitm(ip); { release im }
   unlockmain { end exclusive access }

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

var ip: imptr;   { intratask message pointer }
    br: boolean; { result }

begin

   lockmain; { start exclusive access }
   { check string to large for dialog, accounting for trailing zero }
   if (max(s^) > sc_findreplace_str_len-1) or
      (max(r^) > sc_findreplace_str_len-1) then error(efndstl);
   getitm(ip); { get a im pointer }
   ip^.im := imqfindrep; { set is find/replace query }
   ip^.fnrsch := s; { set input find string }
   ip^.fnrrep := r; { set input replace string }
   ip^.fnropt := opt; { set options }
   br := sc_postmessage(dialogwin, umim, itm2int(ip), 0);
   if not br then winerr; { process windows error }
   waitim(imqfindrep, ip); { wait for the return }
   s := ip^.fnrsch; { set output find string }
   r := ip^.fnrrep;
   opt := ip^.fnropt; { set output options }
   putitm(ip); { release im }
   unlockmain { end exclusive access }

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

procedure iqueryfont(win: winptr; var fc, s, fr, fg, fb, br, bg, bb: integer;
                     var effect: qfteffects);

var ip:  imptr;   { intratask message pointer }
    b:   boolean; { result }
    fns: packed array [1..sc_lf_facesize] of char; { name of font }
    fs:  pstring; { save for input string }

{ find font in fonts list }

function fndfnt(win: winptr; view fns: string): integer;

var fp:     fontptr; { pointer for fonts list }
    fc, ff: integer; { font counters }

begin

   fp := win^.fntlst; { index top of fonts }
   fc := 1; { set 1st font }
   ff := 0; { set no font found }
   while fp <> nil do begin { traverse }

      if compp(fns, fp^.fn^) then ff := fc; { found }
      fp := fp^.next; { next entry }
      fc := fc+1 { count }

   end;
   { The font string should match one from the list, since that list was itself
     formed from the system font list. }
   if ff = 0 then error(esystem); { should have found matching font }

   fndfnt := ff { return font }

end;

begin

   getitm(ip); { get a im pointer }
   ip^.im := imqfont; { set is font query }
   ifontnam(win, fc, fns); { get the name of the font }
   copy(ip^.fntstr, fns); { place in record }
   fs := ip^.fntstr; { and save a copy }
   ip^.fnteff := effect; { copy effects }
   ip^.fntfr := fr; { copy colors }
   ip^.fntfg := fg;
   ip^.fntfb := fb;
   ip^.fntbr := br;
   ip^.fntbg := bg;
   ip^.fntbb := bb;
   ip^.fntsiz := s; { place font size }
   { send request } 
   b := sc_postmessage(dialogwin, umim, itm2int(ip), 0);
   if not b then winerr; { process windows error }
   waitim(imqfont, ip); { wait for the return }
   { pull back the output parameters }
   fc := fndfnt(win, ip^.fntstr^); { find font from list }
   effect := ip^.fnteff; { effects }
   fr := ip^.fntfr; { colors }
   fg := ip^.fntfg;
   fb := ip^.fntfb;
   br := ip^.fntbr;
   bg := ip^.fntbg;
   bb := ip^.fntbb;
   s := ip^.fntsiz; { font size }
   dispose(ip^.fntstr); { release returned copy of font string }
   putitm(ip); { release im }
   dispose(fs) { release our copy of the input font name }

end;

procedure queryfont(var f: text; var fc, s, fr, fg, fb, br, bg, bb: integer;
                    var effect: qfteffects);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   iqueryfont(win, fc, s, fr, fg, fb, br, bg, bb, effect); { execute }
   unlockmain { end exclusive access }

end;

overload procedure queryfont(var f: text; var fc, s: integer; 
                             var fcl, bcl: color;
                             var effect: qfteffects);

var win: winptr;  { window context }
    fr, fg, fb, br, bg, bb: integer; { colors }

begin
                          
   lockmain; { start exclusive access }
   win := txt2win(f); { get windows context }
   { convert colors }
   colrgb(fcl, fr, fg, fb);
   colrgb(bcl, br, bg, bb);
   iqueryfont(win, fc, s, fr, fg, fb, br, bg, bb, effect);
   { convert back again }
   rgbcol(fr, fg, fb, fcl); 
   rgbcol(br, bg, bb, bcl);
   unlockmain { end exclusive access }
   
end;

overload procedure queryfont(var fc, s, fr, fg, fb, br, bg, bb: integer;
                             var effect: qfteffects);

var win: winptr;  { window context }

begin

   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   iqueryfont(win, fc, s, fr, fg, fb, br, bg, bb, effect); { execute }
   unlockmain { end exclusive access }

end;

overload procedure queryfont(var fc, s: integer; var fcl, bcl: color;
                             var effect: qfteffects);

var win: winptr;  { window context }
    fr, fg, fb, br, bg, bb: integer; { colors }

begin
                          
   lockmain; { start exclusive access }
   win := lfn2win(outfil); { get window pointer from text file }
   { convert colors }
   colrgb(fcl, fr, fg, fb);
   colrgb(bcl, br, bg, bb);
   iqueryfont(win, fc, s, fr, fg, fb, br, bg, bb, effect);
   { convert back again }
   rgbcol(fr, fg, fb, fcl); 
   rgbcol(br, bg, bb, bcl);
   unlockmain { end exclusive access }
   
end;

{******************************************************************************

Window procedure for display thread

This is the window handler callback for all display windows.

******************************************************************************}

function wndproc(hwnd, imsg, wparam, lparam: integer): integer;

var r:   integer;   { result holder }
    b:   boolean;
    ofn: ss_filhdl; { output file handle }
    win: winptr;    { pointer to windows structure }
    ip:  imptr;     { intratask message pointer }
    udw: integer;   { up/down control width }
    cr:  sc_rect;   { client rectangle }

begin

{print('wndproc: msg: ', msgcnt); print(' ');
;prtmsgu(hwnd, imsg, wparam, lparam);
;msgcnt := msgcnt+1;}
   if imsg = sc_wm_create then begin

      r := 0

   end else if imsg = sc_wm_paint then begin

      lockmain; { start exclusive access }
      { get the logical output file from Windows handle }
      ofn := hwn2lfn(hwnd);
      if ofn <> 0 then begin { there is a window }

         win := lfn2win(ofn); { index window from output file }
         if win^.bufmod then restore(win, false) { perform selective update }
         else begin { main task will handle it }

            { get update region }
            b := sc_getupdaterect(hwnd, cr, false);
            { validate it so windows won't send multiple notifications }
            b := sc_validatergn_n(hwnd); { validate region }
            { Pack the update region in the message. This limits the update
              region to 16 bit coordinates. We need an im to fix this. }
            wparam := cr.left*$10000+cr.top;
            lparam := cr.right*$10000+cr.bottom;
            unlockmain; { end exclusive access }
            putmsg(hwnd, imsg, wparam, lparam); { send message up }
            lockmain { start exclusive access }

         end;
         r := 0

      end else r := sc_defwindowproc(hwnd, imsg, wparam, lparam);
      unlockmain; { end exclusive access }
      r := 0

   end else if imsg = sc_wm_setfocus then begin

      lockmain; { start exclusive access }
      { get the logical output file from Windows handle }
      ofn := hwn2lfn(hwnd);
      if ofn <> 0 then begin { there is a window }

         win := lfn2win(ofn); { index window from output file }
         with win^ do begin

            b := sc_createcaret(winhan, 0, curspace, 3); { activate caret }
            { set caret (text cursor) position at bottom of bounding box }
            b := sc_setcaretpos(screens[curdsp]^.curxg-1,
                                screens[curdsp]^.curyg-1+linespace-3);
            focus := true; { set screen in focus }
            curon(win) { show the cursor }

         end

      end;
      unlockmain; { end exclusive access }
      putmsg(hwnd, imsg, wparam, lparam); { copy to main thread }
      r := 0

   end else if imsg = sc_wm_killfocus then begin

      lockmain; { start exclusive access }
      { get the logical output file from Windows handle }
      ofn := hwn2lfn(hwnd);
      if ofn <> 0 then begin { there is a window }

         win := lfn2win(ofn); { index window from output file }
         with win^ do begin

            focus := false; { set screen not in focus }
            curoff(win); { hide the cursor }
            b := sc_destroycaret; { remove text cursor }

         end

      end;
      unlockmain; { end exclusive access }
      putmsg(hwnd, imsg, wparam, lparam); { copy to main thread }
      r := 0

   end else if imsg = ummakwin then begin { create standard window }

      { create the window }
      stdwinwin := sc_createwindow('StdWin', pgmnam^, stdwinflg,
                                stdwinx, stdwiny, stdwinw, stdwinh,
                                stdwinpar, 0, sc_getmodulehandle_n);

      stdwinj1c := false; { set no joysticks }
      stdwinj2c := false;
      if joyenb then begin

         r := sc_joysetcapture(stdwinwin, sc_joystickid1, 33, false);
         stdwinj1c := r = 0; { set joystick 1 was captured }
         r := sc_joysetcapture(stdwinwin, sc_joystickid2, 33, false);
         stdwinj2c := r = 0; { set joystick 1 was captured }

      end;

      { signal we started window }
      iputmsg(0, umwinstr, 0, 0);
      r := 0

   end else if imsg = umclswin then begin { close standard window }

      b := sc_destroywindow(stdwinwin); { remove window from screen }

      { signal we closed window }
      iputmsg(0, umwincls, 0, 0);
      r := 0

   end else if imsg = sc_wm_erasebkgnd then begin

     { Letting windows erase the background is not good, because it flashes, and
       is redundant in any case, because we handle that. }
     r := 1 { say we are handling the erase }

   end else if imsg = sc_wm_close then begin

      { we handle our own window close, so don't pass this on }
      putmsg(0, imsg, wparam, lparam);
      r := 0

   end else if imsg = sc_wm_destroy then begin

      { here's a problem. Posting quit causes the thread/process to terminate,
        not just the window. MSDN says to quit only the main window, but what
        is the main window here ? We send our terminate message based on
        wm_quit, and the window, even the main, does get closed. The postquit
        message appears to only be for closing down the program, which our
        program does by exiting. }
      { sc_postquitmessage(0); }
      r := 0

   end else if (imsg = sc_wm_lbuttondown) or (imsg = sc_wm_mbuttondown) or
               (imsg = sc_wm_rbuttondown) then begin

      { Windows allows child windows to capture the focus, but they don't
        give it up (its a feature). We get around this by  returning the
        focus back to any window that is clicked by the mouse, but does
        not have the focus. }
      r := sc_setfocus(hwnd);
      putmsg(hwnd, imsg, wparam, lparam);
      r := sc_defwindowproc(hwnd, imsg, wparam, lparam);

   end else if imsg = umim then begin { intratask message }

      ip := int2itm(wparam); { get im pointer }
      case ip^.im of { im type }

         imupdown: begin { create up/down control }

            { get width of up/down control (same as scroll arrow) }
            udw := sc_getsystemmetrics(sc_sm_cxhscroll);
            ip^.udbuddy := 
               sc_createwindow('edit', '', 
                               sc_ws_child or sc_ws_visible or sc_ws_border or
                               sc_es_left or sc_es_autohscroll, 
                               ip^.udx, ip^.udy, ip^.udcx-udw-1, ip^.udcy,
                               ip^.udpar, ip^.udid, ip^.udinst);
            ip^.udhan := 
               sc_createupdowncontrol(ip^.udflg, ip^.udx+ip^.udcx-udw-2, ip^.udy, udw,
                                      ip^.udcy, ip^.udpar, ip^.udid, ip^.udinst,
                                      ip^.udbuddy, ip^.udup, ip^.udlow, 
                                      ip^.udpos);
            { signal complete }
            iputmsg(0, umim, wparam, 0)

         end;

         imwidget: begin { create widget }

            { start widget window }
            ip^.wigwin := sc_createwindow(ip^.wigcls^, ip^.wigtxt^, ip^.wigflg,
                                          ip^.wigx, ip^.wigy, ip^.wigw,
                                          ip^.wigh, ip^.wigpar, ip^.wigid,
                                          ip^.wigmod);
            { signal we started widget }
            iputmsg(0, umim, wparam, 0)

         end

      end;
      r := 0 { clear result }

   end else begin { default handling }

      { Copy messages we are interested in to the main thread. By keeping the
        messages passed down to only the interesting ones, we help prevent
        queue "flooding". This is done with a case, and not a set, because sets
        are limited to 256 elements. }
      case imsg of

         sc_wm_paint, sc_wm_lbuttondown, sc_wm_lbuttonup, sc_wm_mbuttondown,
         sc_wm_mbuttonup, sc_wm_rbuttondown, sc_wm_rbuttonup, sc_wm_size,
         sc_wm_char, sc_wm_keydown, sc_wm_keyup, sc_wm_quit, sc_wm_close,
         sc_wm_mousemove, sc_wm_timer, sc_wm_command, sc_wm_vscroll, 
         sc_wm_hscroll, sc_wm_notify: begin

{print('wndproc: passed to main: msg: ', msgcnt); print(' ');
;prtmsgu(hwnd, imsg, wparam, lparam);
;msgcnt := msgcnt+1;}
            putmsg(hwnd, imsg, wparam, lparam);

         end;
         else { ignore the rest }

      end;
      r := sc_defwindowproc(hwnd, imsg, wparam, lparam);

   end;

   wndproc := r;

end;

{******************************************************************************

Create dummy window

Create window to pass messages only. The window will have no display.

******************************************************************************}

procedure createdummy(function wndproc(hwnd, imsg, wparam, lparam: integer)
                      : integer; view name: string; var dummywin: integer);

var wc:  sc_wndclassa; { windows class structure }
    b:   boolean; { function return }
    v:   integer;

begin

   { create dummy class for message handling }
   wc.style      := 0;
   wc.wndproc    := sc_wndprocadr(wndproc);
   wc.clsextra   := 0;
   wc.wndextra   := 0;
   wc.instance   := sc_getmodulehandle_n;
   wc.icon       := 0;
   wc.cursor     := 0;
   wc.background := 0;
   wc.menuname   := nil;
   wc.classname  := str(name);
   { register that class }
   b := sc_registerclass(wc);
   { create the window }
   v := 2; { construct sc_hwnd_message, $fffffffd }
   v := not v;
   dummywin := sc_createwindow(name, '', 0, 0, 0, 0, 0,
                              v {sc_hwnd_message}, 0, sc_getmodulehandle_n)

end;

{******************************************************************************

Window display thread

Handles the actual display of all windows and input queues associated with
them. We create a dummy window to allow "headless" communication with the
thread, but any number of subwindows will be started in the thread.

******************************************************************************}

procedure dispthread;

var msg: sc_msg;
    b:   boolean; { function return }
    r:   integer; { result holder }

begin

   { create dummy window for message handling }
   createdummy(wndproc, 'dispthread', dispwin);

   b := sc_setevent(threadstart); { flag subthread has started up }

   { message handling loop }
   while sc_getmessage(msg, 0, 0, 0) <> 0 do begin { not a quit message }

      b := sc_translatemessage(msg); { translate keyboard events }
      r := sc_dispatchmessage(msg)

   end

end;

{******************************************************************************

Window procedure

This is the event handler for the main thread. It's a dummy, and we handle
everything by sending it on.

******************************************************************************}

function wndprocmain(hwnd, imsg, wparam, lparam: integer): integer;

var r: integer; { result holder }

begin

{;prtstr('wndprocmain: msg: ');
;prtmsgu(hwnd, imsg, wparam, lparam);}
   if imsg = sc_wm_create then begin

      r := 0

   end else if imsg = sc_wm_destroy then begin

      sc_postquitmessage(0);
      r := 0

   end else r := sc_defwindowproc(hwnd, imsg, wparam, lparam);

   wndprocmain := r

end;

{******************************************************************************

Dialog query window procedure

This message handler is to allow is to fix certain features of dialogs, like
the fact that they come up behind the main window.

******************************************************************************}

function wndprocfix(hwnd, imsg, wparam, lparam: integer): integer;

var b: boolean; { return value }

begin

   refer(hwnd, imsg, wparam, lparam); { these aren't presently used }
{;prtstr('wndprocfix: msg: ');
;prtmsgu(hwnd, imsg, wparam, lparam);}

   { If dialog is focused, send it to the foreground. This solves the issue
     where dialogs are getting parked behind the main window. }
   if imsg = sc_wm_setfocus then b := sc_setforegroundwindow(hwnd);

   wndprocfix := 0 { tell callback to handle own messages }

end;

{******************************************************************************

Dialog procedure

Runs the various dialogs.

******************************************************************************}

function wndprocdialog(hwnd, imsg, wparam, lparam: integer): integer;

type fnrptr = ^sc_findreplace; { pointer to find replace entry }

var r:    integer;                { result holder }
    ip:   imptr;                  { intratask message pointer }
    cr:   sc_choosecolor_rec;     { color select structure }
    b:    boolean;                { result holder }
    i:    integer;                { index for string }
    fr:   sc_openfilename;        { file select structure }
    bs:   pstring;                { filename holder }
    frrp: fnrptr;                 { dialog control record for find/replace }
    fs:   sc_findreplace_str_ptr; { pointer to finder string }
    rs:   sc_findreplace_str_ptr; { pointer to replacement string }
    fl:   integer;                { flags }
    fns:  sc_choosefont_rec;      { font select structure }
    lf:   sc_lplogfont;           { logical font structure }
    frcr: record { find/replace record pointer convertion }

       case boolean of

          false: (i: integer);
          true:  (p: fnrptr)

    end;

begin

{;prtstr('wndprocdialog: msg: ');
;prtmsgu(hwnd, imsg, wparam, lparam);
;printn('');}
   if imsg = sc_wm_create then begin

      r := 0

   end else if imsg = sc_wm_destroy then begin

      sc_postquitmessage(0);
      r := 0

   end else if imsg = umim then begin { intratask message }

      ip := int2itm(wparam); { get im pointer }
      case ip^.im of { im type }

         imalert: begin { it's an alert }

            r := sc_messagebox(0, ip^.altmsg^, ip^.alttit^, 
                               sc_mb_ok or sc_mb_setforeground);
            { signal complete }
            iputmsg(0, umim, wparam, 0)

         end;

         imqcolor: begin

            { set starting color }
            cr.rgbresult := rgb2win(ip^.clrred, ip^.clrgreen, ip^.clrblue);
            cr.lstructsize := 9*4; { set size }
            cr.hwndowner := 0; { set no owner }
            cr.hinstance := 0; { no instance }
            cr.rgbresult := 0; { clear color }
            cr.lpcustcolors := gcolorsav; { set global color save }
            { set display all colors, start with initalized color }
            cr.flags := sc_cc_anycolor or sc_cc_rgbinit or sc_cc_enablehook;
            cr.lcustdata := 0; { no data }
            cr.lpfnhook := sc_wndprocadr(wndprocfix); { hook to force front }
            cr.lptemplatename := nil; { set no template name }
            b := sc_choosecolor(cr); { perform choose color }
            { set resulting color }
            win2rgb(cr.rgbresult, ip^.clrred, ip^.clrgreen, ip^.clrblue);
            { signal complete }
            iputmsg(0, umim, wparam, 0)

         end;

         imqopen, imqsave: begin

            new(bs, 200); { allocate result string buffer }
            { copy input string to buffer }
            for r:= 1 to max(ip^.opnfil^) do 
               bs^[r] := chr(chr2ascii(ip^.opnfil^[r]));
            { place terminator }
            bs^[max(ip^.opnfil^)+1] := chr(0);
            ip^.opnfil := bs; { now index the temp buffer }
            fr.lstructsize := 21*4+2*2; { set size }
            fr.hwndowner := 0;
            fr.hinstance := 0;
            fr.lpstrfilter := nil;
            fr.lpstrcustomfilter := nil;
            fr.nfilterindex := 0;
            fr.lpstrfile := bs;
            fr.lpstrfiletitle := nil;
            fr.lpstrinitialdir := nil;
            fr.lpstrtitle := nil;
            fr.flags := sc_ofn_hidereadonly or sc_ofn_enablehook;
            fr.nfileoffset := 0;
            fr.nfileextension := 0;
            fr.lpstrdefext := nil;
            fr.lcustdata := 0;
            fr.lpfnhook := sc_wndprocadr(wndprocfix); { hook to force front }
            fr.lptemplatename := nil;
            fr.pvreserved := 0;
            fr.dwreserved := 0;
            fr.flagsex := 0;
            if ip^.im = imqopen then { it's open }
               b := sc_getopenfilename(fr) { perform choose file }
            else { it's save }
               b := sc_getsavefilename(fr); { perform choose file }
            if not b then begin
         
               { Check was a cancel. If the user canceled, return a null 
                 string. }
               r := sc_commdlgextendederror;
               if r <> 0 then error(efildlg); { error }
               { Since the main code is expecting us to make a new string for
                 the result, we must copy the input to the output so that it's
                 disposal will be valid. }
               if ip^.im = imqopen then new(ip^.opnfil, 0)
               else new(ip^.savfil, 0)
               
            end else begin { create result string }

               if ip^.im = imqopen then begin { it's open }

                  i := 1; { set 1st character }
                  while bs^[i] <> chr(0) do i := i+1; { find terminator }
                  new(ip^.opnfil, i-1); { create result string }
                  for i := 1 to i-1 do ip^.opnfil^[i] := ascii2chr(ord(bs^[i]))

               end else begin { it's save }

                  i := 1; { set 1st character }
                  while bs^[i] <> chr(0) do i := i+1; { find terminator }
                  new(ip^.savfil, i-1); { create result string }
                  for i := 1 to i-1 do ip^.savfil^[i] := ascii2chr(ord(bs^[i]))

               end

            end;
            dispose(bs); { release temp string }
            { signal complete }
            iputmsg(0, umim, wparam, 0)

         end;

         imqfind: begin

            new(fs); { get a new finder string }
            { copy string to destination }
            for i := 1 to max(ip^.fndstr^) do 
               fs^[i] := chr(chr2ascii(ip^.fndstr^[i]));
            fs^[max(ip^.fndstr^)+1] := chr(0); { terminate }
            new(frrp); { get a find/replace data record }
            frrp^.lstructsize := sc_findreplace_len; { set size }
            frrp^.hwndowner := dialogwin; { set owner }
            frrp^.hinstance := 0; { no instance }
            { set flags }
            fl := sc_fr_hidewholeword {or sc_fr_enablehook};
            { set status of down }
            if not (qfnup in ip^.fndopt) then fl := fl+sc_fr_down;
            { set status of case }
            if qfncase in ip^.fndopt then fl := fl+sc_fr_matchcase;
            frrp^.flags := fl;
            frrp^.lpstrfindwhat := fs; { place finder string }
            frrp^.lpstrreplacewith := nil; { set no replacement string }
            frrp^.wfindwhatlen := sc_findreplace_str_len; { set length }
            frrp^.wreplacewithlen := 0; { set null replacement string }
            frrp^.lcustdata := itm2int(ip); { send ip with this record }
            frrp^.lpfnhook := 0; { no callback }
            frrp^.lptemplatename := nil; { set no template }
            { start find dialog }
            fndrepmsg := sc_registerwindowmessage('commdlg_FindReplace');
            ip^.fndhan := sc_findtext(frrp^); { perform dialog }
            { now bring that to the front }
            fl := 0;
            fl := not fl;
            b := sc_setwindowpos(ip^.fndhan, fl {sc_hwnd_top}, 0, 0, 0, 0,
                                 sc_swp_nomove or sc_swp_nosize); 
            b := sc_setforegroundwindow(ip^.fndhan);
            r := 0

         end;

         imqfindrep: begin

            new(fs); { get a new finder string }
            { copy string to destination }
            for i := 1 to max(ip^.fnrsch^) do 
               fs^[i] := chr(chr2ascii(ip^.fnrsch^[i]));
            fs^[max(ip^.fnrsch^)+1] := chr(0); { terminate }
            new(rs); { get a new finder string }
            { copy string to destination }
            for i := 1 to max(ip^.fnrrep^) do 
               rs^[i] := chr(chr2ascii(ip^.fnrrep^[i]));
            rs^[max(ip^.fnrrep^)+1] := chr(0); { terminate }
            new(frrp); { get a find/replace data record }
            frrp^.lstructsize := sc_findreplace_len; { set size }
            frrp^.hwndowner := dialogwin; { set owner }
            frrp^.hinstance := 0; { no instance }
            { set flags }
            fl := sc_fr_hidewholeword;
            if not (qfrup in ip^.fnropt) then fl := fl+sc_fr_down; { set status of down }
            if qfrcase in ip^.fnropt then fl := fl+sc_fr_matchcase; { set status of case }
            frrp^.flags := fl;
            frrp^.lpstrfindwhat := fs; { place finder string }
            frrp^.lpstrreplacewith := rs; { place replacement string }
            frrp^.wfindwhatlen := sc_findreplace_str_len; { set length }
            frrp^.wreplacewithlen := sc_findreplace_str_len; { set null replacement string }
            frrp^.lcustdata := itm2int(ip); { send ip with this record }
            frrp^.lpfnhook := 0; { clear these }
            frrp^.lptemplatename := nil;
            { start find dialog }
            fndrepmsg := sc_registerwindowmessage('commdlg_FindReplace');
            ip^.fnrhan := sc_replacetext(frrp^); { perform dialog }
            { now bring that to the front }
            fl := 0;
            fl := not fl;
            b := sc_setwindowpos(ip^.fnrhan, fl {sc_hwnd_top}, 0, 0, 0, 0,
                                 sc_swp_nomove or sc_swp_nosize); 
            b := sc_setforegroundwindow(ip^.fnrhan);
            r := 0

         end;

         imqfont: begin
           
            new(lf); { get a logical font structure }
            { initalize logical font structure }
            lf^.lfheight := ip^.fntsiz; { use default height }
            lf^.lfwidth := 0; { use default width }
            lf^.lfescapement := 0; { no escapement }
            lf^.lforientation := 0; { orient to x axis }
            if qftebold in ip^.fnteff then lf^.lfweight := sc_fw_bold { set bold }
            else lf^.lfweight := sc_fw_dontcare; { default weight }
            lf^.lfitalic := ord(qfteitalic in ip^.fnteff);  { italic }
            lf^.lfunderline := ord(qfteunderline in ip^.fnteff); { underline }
            lf^.lfstrikeout := ord(qftestrikeout in ip^.fnteff); { strikeout }
            lf^.lfcharset := sc_default_charset; { use default characters }
            { use default precision }
            lf^.lfoutprecision := sc_out_default_precis;
            { use default clipping }
            lf^.lfclipprecision := sc_clip_default_precis;
            lf^.lfquality := sc_default_quality; { use default quality }
            lf^.lfpitchandfamily := 0; { must be zero }
            copys2z(lf^.lffacename, ip^.fntstr^); { place face name }
            { initalize choosefont structure }
            fns.lstructsize := sc_choosefont_len; { set size }
            fns.hwndowner := 0; { set no owner }
            fns.hdc := 0; { no device context }
            fns.lplogfont := lf; { logical font }
            fns.ipointsize := 0; { no point size }
            { set flags }
            fns.flags := sc_cf_screenfonts or sc_cf_effects or
                         sc_cf_noscriptsel or sc_cf_forcefontexist or 
                         sc_cf_ttonly or sc_cf_inittologfontstruct or
                         sc_cf_enablehook;
            { color }
            fns.rgbcolors := rgb2win(ip^.fntfr, ip^.fntfg, ip^.fntfb);
            fns.lcustdata := 0; { no data }
            fns.lpfnhook := sc_wndprocadr(wndprocfix); { hook to force front }
            fns.lptemplatename := nil; { no template name }
            fns.hinstance := 0; { no instance }
            fns.lpszstyle := nil; { no style }
            fns.nfonttype := 0; { no font type }
            fns.nsizemin := 0; { no minimum size }
            fns.nsizemax := 0; { no maximum size }
            b := sc_choosefont(fns); { perform choose font }
            if not b then begin
            
               { Check was a cancel. If the user canceled, just return the string 
                 empty. }
               r := sc_commdlgextendederror;
               if r <> 0 then error(efnddlg); { error }
               { Since the main code is expecting us to make a new string for
                 the result, we must copy the input to the output so that it's
                 disposal will be valid. }
               copy(ip^.fntstr, ip^.fntstr^)
               
            end else begin { set what the dialog changed }

               ip^.fnteff := []; { clear what was set }
               if lf^.lfitalic <> 0 then ip^.fnteff := ip^.fnteff+[qfteitalic] { italic }
               else ip^.fnteff := ip^.fnteff-[qfteitalic];
               if fns.nfonttype and sc_bold_fonttype <> 0 then 
                  ip^.fnteff := ip^.fnteff+[qftebold] { bold }
               else
                  ip^.fnteff := ip^.fnteff-[qftebold];
               if lf^.lfunderline <> 0 then 
                  ip^.fnteff := ip^.fnteff+[qfteunderline] { underline }
               else ip^.fnteff := ip^.fnteff-[qfteunderline];
               if lf^.lfstrikeout <> 0 then 
                  ip^.fnteff := ip^.fnteff+[qftestrikeout] { strikeout }
               else ip^.fnteff := ip^.fnteff-[qftestrikeout];
               { place foreground colors }
               win2rgb(fns.rgbcolors, ip^.fntfr, ip^.fntfg, ip^.fntfb);
               copyz2s(ip^.fntstr, lf^.lffacename); { copy font string back }
               ip^.fntsiz := abs(lf^.lfheight); { set size }

            end;
            { signal complete }
            iputmsg(0, umim, wparam, 0)

         end;

      end;
      r := 0 { clear result }

   end else if imsg = fndrepmsg then begin { find is done }

      { Here's a series of dirty tricks. The find/replace record pointer is given
        back to us as an integer by windows. We also stored the ip as "customer
        data" in integer. }
      frcr.i := lparam; { get find/replace data pointer }
      frrp := frcr.p;
      ip := int2itm(frrp^.lcustdata); { get im pointer }
      with frrp^, ip^ do { in find/replace context do }
         if ip^.im = imqfind then begin { it's a find }

         b := sc_destroywindow(ip^.fndhan); { destroy the dialog }
         { check and set case match mode }
         if flags and sc_fr_matchcase <> 0 then fndopt := fndopt+[qfncase];
         { check and set cas up/down mode }
         if flags and sc_fr_down <> 0 then fndopt := fndopt-[qfnup]
         else fndopt := fndopt+[qfnup];
         i := 1; { set 1st character }
         while lpstrfindwhat^[i] <> chr(0) do i := i+1; { find terminator }
         new(fndstr, i-1); { create result string }
         for i := 1 to i-1 do fndstr^[i] := ascii2chr(ord(lpstrfindwhat^[i]));
         dispose(lpstrfindwhat) { release temp string }

      end else begin { it's a find/replace }

         b := sc_destroywindow(ip^.fnrhan); { destroy the dialog }
         { check and set case match mode }
         if flags and sc_fr_matchcase <> 0 then fnropt := fnropt+[qfrcase];
         { check and set find mode }
         if flags and sc_fr_findnext <> 0 then fnropt := fnropt+[qfrfind];
         { check and set replace mode }
         if flags and sc_fr_replace <> 0 then 
            fnropt := fnropt-[qfrfind]-[qfrallfil];
         { check and set replace all mode }
         if flags and sc_fr_replaceall <> 0 then 
            fnropt := fnropt-[qfrfind]+[qfrallfil];
         i := 1; { set 1st character }
         while lpstrfindwhat^[i] <> chr(0) do i := i+1; { find terminator }
         new(fnrsch, i-1); { create result string }
         for i := 1 to i-1 do fnrsch^[i] := ascii2chr(ord(lpstrfindwhat^[i]));
         i := 1; { set 1st character }
         while lpstrreplacewith^[i] <> chr(0) do i := i+1; { find terminator }
         new(fnrrep, i-1); { create result string }
         for i := 1 to i-1 do 
            fnrrep^[i] := ascii2chr(ord(lpstrreplacewith^[i]));
         dispose(lpstrfindwhat); { release temp strings }
         dispose(lpstrreplacewith)

      end;
      dispose(frrp); { release find/replace record entry }
      { signal complete }
      iputmsg(0, umim, itm2int(ip), 0)

   end else r := sc_defwindowproc(hwnd, imsg, wparam, lparam);

   wndprocdialog := r

end;

{******************************************************************************

Dialog thread

******************************************************************************}

procedure dialogthread;

var msg: sc_msg;
    b:   boolean; { function return }
    r:   integer; { result holder }

begin

   { create dummy window for message handling }
   createdummy(wndprocdialog, 'dialogthread', dialogwin);

   b := sc_setevent(threadstart); { flag subthread has started up }

   { message handling loop }
   while sc_getmessage(msg, 0, 0, 0) <> 0 do begin { not a quit message }

      b := sc_translatemessage(msg); { translate keyboard events }
      r := sc_dispatchmessage(msg)

   end

end;

{******************************************************************************

Gralib startup

******************************************************************************}

begin

   { override our interdicted calls }
   ss_ovr_alias(filealias, sav_alias);
   ss_ovr_resolve(fileresolve, sav_resolve);
   ss_ovr_openread(fileopenread, sav_openread);
   ss_ovr_openwrite(fileopenwrite, sav_openwrite);
   ss_ovr_close(fileclose, sav_close);
   ss_ovr_read(fileread, sav_read);
   ss_ovr_write(filewrite, sav_write);
   ss_ovr_position(fileposition, sav_position);
   ss_ovr_location(filelocation, sav_location);
   ss_ovr_length(filelength, sav_length);
   ss_ovr_eof(fileeof, sav_eof);
   ss_ovr_wrterr(wrterr, sav_wrterr);
   fend := false; { set no end of program ordered }
   fautohold := true; { set automatically hold self terminators }
   eqefre := nil; { set free event queuing list empty }
   dblflt := false; { set no double fault }
   wigfre := nil; { set free widget tracking list empty }
   freitm := nil; { clear intratask message free list }
   msgcnt := 1; { clear message counter }
   { Form character to ASCII value translation array from ASCII value to 
     character translation array. }
   for ti := 1 to 255 do trnchr[chr(ti)] := 0; { null out array }
   for ti := 1 to 127 do trnchr[chrtrn[ti]] := ti; { form translation }

   { Set up private message queuing }

   msginp := 1; { clear message input queue }
   msgout := 1;
   msgrdy := sc_createevent(true, false); { create message event }
   imsginp := 1; { clear control message message input queue }
   imsgout := 1;
   imsgrdy := sc_createevent(true, false); { create message event }
   sc_initializecriticalsection(mainlock); { initialize the sequencer lock }
   { mainlock := sc_createmutex(false); } { create mutex with no owner }
   { if mainlock = 0 then winerr; } { process windows error }
   new(gcolorsav); { get the color pick array }
   fndrepmsg := 0; { set no find/replace message active }
   for i := 1 to 16 do gcolorsav^[i] := $ffffff; { set all to white }
   { clear open files table }
   for fi := 1 to ss_maxhdl do opnfil[fi] := nil; { set unoccupied }
   { clear top level translator table }
   for fi := 1 to ss_maxhdl do xltfil[fi] := 0; { set unoccupied }
   { clear window logical number translator table }
   for fi := 1 to ss_maxhdl do xltwin[fi] := 0; { set unoccupied }
   { clear file to window logical number translator table }
   for fi := 1 to ss_maxhdl do filwin[fi] := 0; { set unoccupied }

   { Create dummy window for message handling. This is only required so that
     the main thread can be attached to the display thread }
   createdummy(wndprocmain, 'mainthread', mainwin);
   mainthreadid := sc_getcurrentthreadid;

   getpgm; { get the program name from the command line }
   { Now start the display thread, which both manages all displays, and sends us
     all messages from those displays. }
   threadstart := sc_createevent(true, false);
   if threadstart = 0 then winerr; { process windows error }
   { create subthread }
   b := sc_resetevent(threadstart); { clear event }
   r := sc_createthread_nn(0, dispthread, 0, threadid);
   r := sc_waitforsingleobject(threadstart, -1); { wait for thread to start }
   if r = -1 then winerr; { process windows error }
   { Past this point, we need to lock for access between us and the thread. }

   { Now attach the main thread to the display thread. This is required for the
     main thread to have access to items like the display window caret. }
   b := sc_attachthreadinput(mainthreadid, threadid, true);
   if not b then winerr; { process windows error }

   { Start widget thread }
   b := sc_resetevent(threadstart); { clear event }
   r := sc_createthread_nn(0, dialogthread, 0, threadid);
   r := sc_waitforsingleobject(threadstart, -1); { wait for thread to start }
   if r = -1 then winerr; { process windows error }

   { register the stdwin class used to create all windows }
   regstd;

   { unused attribute codes }
   refer(sablink);

   { currently unused routines }
   refer(lwn2win);

   { diagnostic routines (come in and out of use) }
   refer(print);
   refer(printn);
   refer(prtstr);
   refer(prtnum);
   refer(prtmsg);
   refer(prtmsgu);
   refer(prtfil);
   refer(prtmenu);
   refer(prtwiglst);
   refer(diastr);

refer(intv);

end;

{******************************************************************************

Gralib shutdown

******************************************************************************}

begin

   lockmain; { start exclusive access }
   { if the program tries to exit when the user has not ordered an exit, it
     is assumed to be a windows "unaware" program. We stop before we exit
     these, so that their content may be viewed }
   if not fend and fautohold then begin { process automatic exit sequence }

      { See if output is open at all }
      if opnfil[outfil] <> nil then { output is allocated }
         if opnfil[outfil]^.win <> nil then with opnfil[outfil]^.win^ do begin

         { make sure we are displayed }
         if not visible then winvis(opnfil[outfil]^.win);
         { If buffering is off, turn it back on. This will cause the screen
           to come up clear, but this is better than an unrefreshed "hole"
           in the screen. }
         if not bufmod then ibuffer(opnfil[outfil]^.win, true);
         { Check framed. We don't want that off, either, since the user cannot
           close the window via the system close button. }
         if not frame then iframe(opnfil[outfil]^.win, true);
         { Same with system bar }
         if not sysbar then isysbar(opnfil[outfil]^.win, true);
         { change window label to alert user }
         unlockmain; { end exclusive access }
         b := sc_setwindowtext(winhan, trmnam^);
         lockmain; { start exclusive access }
         { wait for a formal end }
         while not fend do ievent(inpfil, er)

      end

   end;
   { close any open windows }
   88: { abort module }
   if not dblflt then begin { we haven't already exited }

      dblflt := true; { set we already exited }
      { close all open files and windows }
      for fi := 1 to ss_maxhdl do
         if opnfil[fi] <> nil then with opnfil[fi]^ do begin

         if han <> 0 then ss_old_close(han, sav_close); { close at lower level }
         if win <> nil then clswin(fi) { close open window }

      end

   end;
   unlockmain { end exclusive access }

end.
