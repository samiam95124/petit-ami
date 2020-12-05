/*******************************************************************************
*                                                                             *
*                     WINDOW MANAGEMENT TEST PROGRAM                          *
*                                                                             *
*                    Copyright (C) 2005 Scott A. Moore                        *
*                                                                             *
* Tests text && graphical windows management calls.                          *
*                                                                             *
*******************************************************************************/

program mantst(input, output, error);

uses gralib/*, 
     extlib,
     strlib */;

label 99; /* terminate */

const off == false;
      on  == true;

var win2, win3, win4: text;
    x, x2, y, y2:     int;
    ox, oy:           int; /* original size of window */
    fb:               boolean; /* front/back flipper */
    er:               evtrec;
    mp:               menuptr; /* menu pointer */
    ml:               menuptr; /* menu list */
    sm:               menuptr; /* submenu list */
    sred:             boolean; /* state variables */
    sgreen:           boolean;
    sblue:            boolean;
    mincnt:           int; /* minimize counter */
    maxcnt:           int; /* maximize counter */
    nrmcnt:           int; /* normalize counter */
    i:                int;

/* wait return to be pressed, || handle terminate */

void waitnext;

var er: evtrec; /* event record */

{

   repeat event(input, er) until (er.etype == etenter) || (er.etype == etterm);
   if er.etype == etterm then goto 99

};

/* wait return to be pressed, || handle terminate, while printing characters */

void waitnextprint;

var er: evtrec; /* event record */

{

   repeat 

      event(input, er);
      if er.etype == etchar then 
         writeln('Window: ', er.winid:1, ' char: ', er.char);

   until (er.etype == etenter) || (er.etype == etterm);
   if er.etype == etterm then goto 99

};

/* print centered char* */

void prtcen(y: int; view s: char*);

{

   cursor(output, (maxx(output) / 2)-(max(s) / 2), y);
   write(s)

};

/* print centered char* graphical */

void prtceng(y: int; view s: char*);

{

   cursorg(output, (maxxg(output) / 2)-(strsiz(s) / 2), y);
   write(s)

};

/* wait time in 100 microseconds */

void wait(t: int);

var er: evtrec;

{

   timer(output, 1, t, false);
   repeat event(input, er) 
   until (er.etype == ettim) || (er.etype == etterm);
   if er.etype == etterm then goto 99

};

/* app} a new menu entry to the given list */

void app}menu(var list: menuptr; m: menuptr);

var lp: menuptr;

{

   /* clear these links for insurance */
   m->next = nil; /* clear next */
   m->branch = nil; /* clear branch */
   if list == nil then list = m /* list empty, set as first entry */
   else { /* list non-empty */

      /* find last entry in list */
      lp = list; /* index 1st on list */
      while lp->next <> nil do lp = lp->next;
      lp->next = m /* app} at } */

   }

};

/* create menu entry */

void newmenu(var mp: menuptr; onoff, oneof, bar: boolean; 
                  id: int; view face: char*);

{

   new(mp);
   mp->onoff = onoff;
   mp->oneof = oneof;
   mp->bar = bar;
   mp->id = id;
   new(mp->face, max(face));
   mp->face^ = face

};

/* draw a character grid */

void chrgrid;

var x, y: int;

{

   fcolor(yellow);
   linewidth(1);
   y = 1;
   while y < maxyg do {

      line(1, y, maxxg, y);
      y = y+chrsizy

   };
   x = 1;
   while x < maxxg do {

      line(x, 1, x, maxyg);
      x = x+chrsizx

   };
   fcolor(black)

};

/* display frame test */

void frametest(view s: char*);

var er:   evtrec;
    x, y: int;

{

   x = 1; /* set default size */
   y = 1;
   repeat

      event(er); /* get next event */
      if er.etype == etredraw then {

         page;
         fcolor(cyan);
         rect(1, 1, x, y);
         line(1, 1, x, y);
         line(1, y, x, 1);
         fcolor(black);
         write(s);

      };
      if er.etype == etresize then { 

         /* Save the new demensions, even if ! required. This way we must
           get a resize notification for this test to work. */
         x = maxxg; 
         y = maxyg 

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter

};

{

   auto(off);
   curvis(off);
   writeln('Managed screen test vs. 0.1');
   writeln;
   scnsiz(x, y);
   writeln('Screen size character: x: ', x:1, ' y: ', y:1);
   scnsizg(x, y);
   writeln('Screen size pixel: x: ', x:1, ' y: ', y:1);
   writeln;
   getsiz(x, y);
   writeln('Window size character: x: ', x:1, ' y: ', y:1);
   getsizg(ox, oy);
   writeln('Window size graphical: x: ', ox:1, ' y: ', oy:1);
   writeln;
   writeln('Client size character: x: ', maxx:1, ' y: ', maxy:1);
   writeln('Client size graphical: x: ', maxxg:1, ' y: ', maxyg:1);
   writeln;
   writeln('Hit return in any window to continue for each test');
   waitnext;
;if false then {

   /* ************************** Window titling test ************************** */

   title('This is a mangement test window');
   writeln('The title bar of this window should read: This is a mangement test window');
   prtceng(maxyg-chrsizy, 'Window title test');
   waitnext;
   
   /* ************************** Multiple windows ************************** */

   page;
   curvis(on);
   prtceng(maxyg-chrsizy, 'Multiple window test');
   home;
   auto(on);
   writeln('This is the main window');
   writeln;
   writeln('Select back && forth between each window, && make sure the'); 
   writeln('cursor follows');
   writeln;
   write('Here is the cursor->');
   openwin(input, win2, 2);
   writeln(win2, 'This is the second window'); 
   writeln(win2);
   write(win2, 'Here is the cursor->');
   waitnext;
   writeln;
   writeln('Now enter characters to each window, then } with return');
   waitnextprint;
   close(win2);
   page;
   writeln('Second window now closed');
   waitnext;
   curvis(off);
   auto(off);

   /* ********************* Resize buffer window character ******************** */

   ox = maxx;
   oy = maxy;
   bcolor(cyan);
   sizbuf(50, 50);
   page;
   for x = 1 to maxx do write('*');
   cursor(1, maxy);
   for x = 1 to maxx do write('*');
   for y = 1 to maxy do { cursor(1, y); write('*') };
   for y = 1 to maxy do { cursor(maxx, y); write('*') };
   home;       
   writeln('Buffer should now be 50 by 50 characters, and');
   writeln('painted blue');
   writeln('maxx: ', maxx:1, ' maxy: ', maxy:1);
   writeln('Open up window to verify this');
   prtcen(maxy, 'Buffer resize character test');
   bcolor(white);
   waitnext;
   sizbuf(ox, oy);

   /* *********************** Resize buffer window pixel ********************** */

   ox = maxxg;
   oy = maxyg;
   bcolor(cyan);
   sizbufg(400, 400);
   page;
   linewidth(20);
   line(1, 1, maxxg, 1);
   line(1, 1, 1, maxyg);
   line(1, maxyg, maxxg, maxyg);
   line(maxxg, 1, maxxg, maxyg);
   writeln('Buffer should now be 400 by 400 pixels, and');
   writeln('painted blue');
   writeln('maxxg: ', maxxg:1, ' maxyg: ', maxyg:1);
   writeln('Open up window to verify this');
   prtcen(maxy, 'Buffer resize graphical test');
   bcolor(white);
   waitnext;
   sizbufg(ox, oy);

   /* ****************** Resize screen with buffer on character *************** */

   ox = maxxg;
   oy = maxyg;
   for x = 20 to 80 do {

      setsiz(x, 25);
      getsiz(x2, y2);
      if (x2 <> x) || (y2 <> 25) then {

         setsiz(80, 25);
         page;
         writeln('*** Getsiz does ! match setsiz');
         waitnext;
         goto 99

      };
      page;
      writeln('Resize screen buffered character');
      writeln;
      writeln('Moving in x');
      wait(1000)

   };
   writeln;
   writeln('Complete');
   waitnext;
   for y = 10 to 80 do {

      setsiz(80, y);
      getsiz(x2, y2);
      if (x2 <> 80) || (y2 <> y) then {

         setsiz(80, 25);
         page;
         writeln('*** Getsiz does ! match setsiz');
         waitnext;
         goto 99

      };
      page;
      writeln('Resize screen buffered character');
      writeln;
      writeln('Moving in y');
      wait(1000)

   };
   writeln;
   writeln('Complete');
   waitnext;
   winclientg(ox, oy, ox, oy, [wmframe, wmsize, wmsysbar]);
   setsizg(ox, oy);
   
   /* ******************** Resize screen with buffer on pixel ***************** */

   ox = maxxg;
   oy = maxyg;
   for x = 200 to 800 do {

      setsizg(x, 200);
      getsizg(x2, y2);
      if (x2 <> x) || (y2 <> 200) then {

         setsiz(80, 25);
         page;
         writeln('*** Getsiz does ! match setsiz');
         waitnext;
         goto 99

      };
      page;
      writeln('Resize screen buffered graphical');
      writeln;
      writeln('Moving in x');
      wait(100)

   };
   writeln;
   writeln('Complete');
   waitnext;
   for y = 100 to 800 do {

      setsizg(300, y);
      getsizg(x2, y2);
      if (x2 <> 300) || (y2 <> y) then {

         setsiz(80, 25);
         page;
         writeln('*** Getsiz does ! match setsiz');
         waitnext;
         goto 99

      };
      page;
      writeln('Resize screen buffered graphical');
      writeln;
      writeln('Moving in y');
      wait(100)

   };
   writeln;
   writeln('Complete');
   waitnext;
   winclientg(ox, oy, ox, oy, [wmframe, wmsize, wmsysbar]);
   setsizg(ox, oy);

   /* ********************************* Front/back test *********************** */

   page;
   auto(off);
   writeln('Position window for font/back test');
   writeln('Then hit space to flip font/back status, || return to stop');
   fb = false; /* clear front/back status */
   font(font_sign);
   fontsiz(50);
   repeat 

      event(input, er);
      if er.etype == etchar then if er.char == ' ' then { /* flip front/back */

         fb = ! fb;
         if fb then {

            front;
            fcolor(white);
            prtceng(maxyg / 2-chrsizy / 2, 'Back');
            fcolor(black);
            prtceng(maxyg / 2-chrsizy / 2, 'Front')
  
         } else {

            back;
            fcolor(white);
            prtceng(maxyg / 2-chrsizy / 2, 'Front');
            fcolor(black);
            prtceng(maxyg / 2-chrsizy / 2, 'Back')

         }

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   home;
   font(font_term);
   auto(on);

   /* ************************* Frame controls test buffered ****************** */

   page;
   fcolor(cyan);
   rect(1, 1, maxxg, maxyg);
   line(1, 1, maxxg, maxyg);
   line(1, maxyg, maxxg, 1);
   fcolor(black);
   binvis;
   writeln('Ready for frame controls buffered');
   waitnext;
   frame(off);
   writeln('Entire frame off');
   waitnext;
   frame(on);
   writeln('Entire frame on');
   waitnext;
   sysbar(off);
   writeln('System bar off');
   waitnext;
   sysbar(on);
   writeln('System bar on');
   waitnext;
   sizable(off);
   writeln('Size bars off');
   waitnext;
   sizable(on);
   writeln('Size bars on');
   waitnext;
   bover;

   /* ************************* Frame controls test buffered ****************** */

   buffer(off);
   frametest('Ready for frame controls unbuffered');
   frame(off);
   frametest('Entire frame off');
   frame(on);
   frametest('Entire frame on');
   sysbar(off);
   frametest('System bar off');
   sysbar(on);
   frametest('System bar on');
   sizable(off);
   frametest('Size bars off');
   sizable(on);
   frametest('Size bars on');
   buffer(on);

   /* ********************************* Menu test ***************************** */

   page;
   fcolor(cyan);
   rect(1, 1, maxxg, maxyg);
   line(1, 1, maxxg, maxyg);
   line(1, maxyg, maxxg, 1);
   fcolor(black);
   ml = nil; /* clear menu list */
   newmenu(mp, false, false, off, 1, 'Say hello');
   app}menu(ml, mp); 
   newmenu(mp, true, false,  on, 2, 'Bark');
   app}menu(ml, mp); 
   newmenu(mp, false, false, off, 3, 'Walk');
   app}menu(ml, mp); 
   newmenu(sm, false, false, off, 4, 'Sublist');
   app}menu(ml, sm); 
   /* these are one/of buttons */
   newmenu(mp, false, true,  off, 5, 'slow');
   app}menu(sm->branch, mp); 
   newmenu(mp, false, true,  off, 6, 'medium');
   app}menu(sm->branch, mp); 
   newmenu(mp, false, false, on, 7, 'fast');
   app}menu(sm->branch, mp); 
   /* these are on/off buttons */
   newmenu(mp, true, false,  off, 8, 'red');
   app}menu(sm->branch, mp); 
   newmenu(mp, true, false,  off, 9, 'green');
   app}menu(sm->branch, mp); 
   newmenu(mp, true, false,  off, 10, 'blue');
   app}menu(sm->branch, mp); 
   menu(ml);
   menuena(3, off); /* disable 'Walk' */
   menusel(5, on); /* turn on 'slow' */
   menusel(8, on); /* turn on 'red' */

   home;
   writeln('Use sample menu above');
   writeln('''Walk'' is disabled');
   writeln('''Sublist'' is a dropdown');
   writeln('''slow'', ''medium'' && ''fast'' are a one/of list');
   writeln('''red'', ''green'' && ''blue'' are on/off');
   writeln('There should be a bar between slow-medium-fast groups and');
   writeln(' red-green-blue groups.');
   sred = on; /* set states */
   sgreen = off;
   sblue = off;
   repeat 

      event(input, er);
      if er.etype == etterm then goto 99;
      if er.etype == etmenus then {

         write('Menu select: ');
         case er.menuid of

            1:  writeln('Say hello');
            2:  writeln('Bark');
            3:  writeln('Walk');
            4:  writeln('Sublist');
            5:  { writeln('slow'); menusel(5, on) };
            6:  { writeln('medium'); menusel(6, on) };
            7:  { writeln('fast'); menusel(7, on) };
            8:  { writeln('red'); sred = ! sred; menusel(8, sred) };
            9:  { writeln('green'); sgreen = ! sgreen; menusel(9, sgreen) };
            10: { writeln('blue'); sblue = ! sblue; menusel(10, sblue) }

         }

      }

   until (er.etype == etenter) || (er.etype == etterm);
   menu(nil);

   /* ****************************** Standard menu test *********************** */

   page;
   auto(on);
   ml = nil; /* clear menu list */
   newmenu(mp, false, false, off, smmax+1, 'one');
   app}menu(ml, mp); 
   newmenu(mp, true, false,  on, smmax+2, 'two');
   app}menu(ml, mp); 
   newmenu(mp, false, false, off, smmax+3, 'three');
   app}menu(ml, mp); 
   stdmenu([smnew, smopen, smclose, smsave,smsaveas, smpageset, smprint, smexit,
            smundo, smcut, smpaste, smdelete, smfind, smfindnext, smreplace,
            smgoto, smselectall, smnewwindow, smtilehoriz, smtilevert,
            smcascade, smcloseall, smhelptopic, smabout], mp, ml);
   menu(mp);
   writeln('Standard menu appears above');
   writeln('Check our ''one'', ''two'', ''three'' buttons are in the program');
   writeln('defined position');
   repeat 

      event(input, er);
      if er.etype == etterm then goto 99;
      if er.etype == etmenus then {

         write('Menu select: ');
         case er.menuid of

            smnew:       writeln('new');
            smopen:      writeln('open');
            smclose:     writeln('close');
            smsave:      writeln('save');
            smsaveas:    writeln('saveas');
            smpageset:   writeln('pageset');
            smprint:     writeln('print');
            smexit:      writeln('exit');
            smundo:      writeln('undo');
            smcut:       writeln('cut');
            smpaste:     writeln('paste');
            smdelete:    writeln('delete');
            smfind:      writeln('find');
            smfindnext:  writeln('findnext');
            smreplace:   writeln('replace');
            smgoto:      writeln('goto');
            smselectall: writeln('selectall');
            smnewwindow: writeln('newwindow');
            smtilehoriz: writeln('tilehoriz');
            smtilevert:  writeln('tilevert');
            smcascade:   writeln('cascade');
            smcloseall:  writeln('closeall');
            smhelptopic: writeln('helptopic');
            smabout:     writeln('about');
            smmax+1:     writeln('one');
            smmax+2:     writeln('two');
            smmax+3:     writeln('three');

         }

      }

   until (er.etype == etenter) || (er.etype == etterm);
   menu(nil);

   /* ************************* Child windows test character ****************** */

   page;
   chrgrid;
   prtcen(maxy, 'Child windows test character');
   openwin(input, win2, output, 2);
   setpos(win2, 1, 10);
   sizbuf(win2, 20, 10);
   setsiz(win2, 20, 10);
   openwin(input, win3, output, 3);
   setpos(win3, 21, 10);
   sizbuf(win3, 20, 10);
   setsiz(win3, 20, 10);
   openwin(input, win4, output, 4);
   setpos(win4, 41, 10);
   sizbuf(win4, 20, 10);
   setsiz(win4, 20, 10);
   bcolor(win2, cyan);
   page(win2);
   writeln(win2, 'I am child window 1');
   bcolor(win3, yellow);
   page(win3);
   writeln(win3, 'I am child window 2');
   bcolor(win4, magenta);
   page(win4);
   writeln(win4, 'I am child window 3');
   home;
   writeln('There should be 3 labeled child windows below, with frames   ');
   waitnext;
   frame(win2, off);
   frame(win3, off);
   frame(win4, off);
   home;
   writeln('There should be 3 labeled child windows below, without frames');
   waitnext;
   close(win2);
   close(win3);
   close(win4);
   home;
   writeln('Child windows should all be closed                           ');
   waitnext;

   /* *************************** Child windows test pixel ******************** */

   page;
   prtcen(maxy, 'Child windows test pixel');
   openwin(input, win2, output, 2);
   setposg(win2, 1, 100);
   sizbufg(win2, 200, 200);
   setsizg(win2, 200, 200);
   openwin(input, win3, output, 3);
   setposg(win3, 201, 100);
   sizbufg(win3, 200, 200);
   setsizg(win3, 200, 200);
   openwin(input, win4, output, 4);
   setposg(win4, 401, 100);
   sizbufg(win4, 200, 200);
   setsizg(win4, 200, 200);
   bcolor(win2, cyan);
   page(win2);
   writeln(win2, 'I am child window 1');
   bcolor(win3, yellow);
   page(win3);
   writeln(win3, 'I am child window 2');
   bcolor(win4, magenta);
   page(win4);
   writeln(win4, 'I am child window 3');
   home;
   writeln('There should be 3 labled child windows below, with frames   ');
   waitnext;
   frame(win2, off);
   frame(win3, off);
   frame(win4, off);
   home;
   writeln('There should be 3 labled child windows below, without frames');
   waitnext;
   close(win2);
   close(win3);
   close(win4);
   home;
   writeln('Child windows should all be closed                          ');
   waitnext;

   /* ******************* Child windows stacking test pixel ******************* */

   page;
   prtcen(maxy, 'Child windows stacking test pixel');
   openwin(input, win2, output, 2);
   setposg(win2, 50, 50);
   sizbufg(win2, 200, 200);
   setsizg(win2, 200, 200);
   openwin(input, win3, output, 3);
   setposg(win3, 150, 100);
   sizbufg(win3, 200, 200);
   setsizg(win3, 200, 200);
   openwin(input, win4, output, 4);
   setposg(win4, 250, 150);
   sizbufg(win4, 200, 200);
   setsizg(win4, 200, 200);
   bcolor(win2, cyan);
   page(win2);
   writeln(win2, 'I am child window 1');
   bcolor(win3, yellow);
   page(win3);
   writeln(win3, 'I am child window 2');
   bcolor(win4, magenta);
   page(win4);
   writeln(win4, 'I am child window 3');
   home;
   writeln('There should be 3 labled child windows below, overlapped,   ');
   writeln('with child 1 on the bottom, child 2 middle, && child 3 top.');
   waitnext;
   back(win2);
   back(win3);
   back(win4);
   home;
   writeln('Now the windows are reordered, with child 1 on top, child 2 ');
   writeln('below that, && child 3 on the bottom.                      ');
   waitnext;
   front(win2);
   front(win3);
   front(win4);
   home;
   writeln('Now the windows are reordered, with child 3 on top, child 2 ');
   writeln('below that, && child 1 on the bottom.                      ');
   waitnext;
   close(win2);
   close(win3);
   close(win4);
   page;
   writeln('Child windows should all be closed                          ');
   waitnext;

   /* ************** Child windows stacking resize test pixel 1 *************** */

   buffer(off);
   auto(off);
   openwin(input, win2, output, 2);
   setposg(win2, 50-25, 50-25);
   sizbufg(win2, 200, 200);
   setsizg(win2, maxxg-150, maxyg-150);
   openwin(input, win3, output, 3);
   setposg(win3, 100-25, 100-25);
   sizbufg(win3, 200, 200);
   setsizg(win3, maxxg-150, maxyg-150);
   openwin(input, win4, output, 4);
   setposg(win4, 150-25, 150-25);
   sizbufg(win4, 200, 200);
   setsizg(win4, maxxg-150, maxyg-150);
   bcolor(win2, cyan);
   page(win2);
   writeln(win2, 'I am child window 1');
   bcolor(win3, yellow);
   page(win3);
   writeln(win3, 'I am child window 2');
   bcolor(win4, magenta);
   page(win4);
   writeln(win4, 'I am child window 3');
   repeat

      event(er);
      if er.etype == etredraw then {

         page;
         prtceng(maxyg-chrsizy, 'Child windows stacking resize test pixel 1');
         prtceng(1, 'move && resize');
         setsizg(win3, maxxg-150, maxyg-150);
         setsizg(win4, maxxg-150, maxyg-150);
         setsizg(win2, maxxg-150, maxyg-150);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   close(win2);
   close(win3);
   close(win4);
/* ???????? There is a hole in the buffer after this gets enabled */
   buffer(on);
   page;
   writeln('Child windows should all be closed                          ');
   waitnext;

   /* ************** Child windows stacking resize test pixel 2 *************** */

   buffer(off);
   openwin(input, win2, output, 2);
   setposg(win2, 50, 50);
   sizbufg(win2, 200, 200);
   setsizg(win2, maxxg-100, maxyg-100);
   openwin(input, win3, output, 3);
   setposg(win3, 100, 100);
   sizbufg(win3, 200, 200);
   setsizg(win3, maxxg-200, maxyg-200);
   openwin(input, win4, output, 4);
   setposg(win4, 150, 150);
   sizbufg(win4, 200, 200);
   setsizg(win4, maxxg-300, maxyg-300);
   bcolor(win2, cyan);
   page(win2);
   writeln(win2, 'I am child window 1');
   bcolor(win3, yellow);
   page(win3);
   writeln(win3, 'I am child window 2');
   bcolor(win4, magenta);
   page(win4);
   writeln(win4, 'I am child window 3');
   repeat

      event(er);
      if er.etype == etredraw then {

         page;
         prtceng(maxyg-chrsizy, 'Child windows stacking resize test pixel 2');
         prtceng(1, 'move && resize');
         setsizg(win3, maxxg-200, maxyg-200);
         setsizg(win4, maxxg-300, maxyg-300);
         setsizg(win2, maxxg-100, maxyg-100);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   close(win2);
   close(win3);
   close(win4);
   buffer(on);
   page;
   writeln('Child windows should all be closed                          ');
   waitnext;

   /* ******************************* Buffer off test *********************** */

   page;
   auto(off);
   buffer(off);
   /* initalize prime size information */
   x = maxxg; 
   y = maxyg; 
   linewidth(5); /* set large lines */
   font(font_sign);
   binvis;
   repeat

      event(er); /* get next event */
      if er.etype == etredraw then {

         /* clear screen without overwriting frame */
         fcolor(white);
         frect(1+5, 1+5, x-5, y-5);
         fcolor(black);
         fontsiz(y / 10);
         prtceng(maxyg / 2-chrsizy / 2, 'SIZE AND COVER ME !');
         rect(1+2, 1+2, x-2, y-2); /* frame the window */

      };
      if er.etype == etresize then { 

         /* Save the new demensions, even if ! required. This way we must
           get a resize notification for this test to work. */
         x = maxxg; 
         y = maxyg 

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   buffer(on);

   /* ******************************* min/max/norm test *********************** */

   page;
   auto(off);
   buffer(off);
   font(font_term);
   mincnt = 0; /* clear minimize counter */
   maxcnt = 0; /* clear maximize counter */
   nrmcnt = 0; /* clear normalize counter */
   repeat

      event(er); /* get next event */
      if er.etype == etredraw then {

         page;
         writeln('Minimize, maximize && restore this window');
         writeln;
         writeln('Minimize count:  ', mincnt);
         writeln('Maximize count:  ', maxcnt);
         writeln('Normalize count: ', nrmcnt);

      };
      /* count minimize, maximize, normalize */
      if er.etype == etmax then maxcnt = maxcnt+1; 
      if er.etype == etmin then mincnt = mincnt+1;
      if er.etype == etnorm then nrmcnt = nrmcnt+1;

      if er.etype == etterm then goto 99

   until er.etype == etenter;
   buffer(on);

   /* ************************ Window size calculate character ******************** */

   page;
   prtceng(maxyg-chrsizy, 'Window size calculate character');
   home;
   openwin(input, win2, 2);
   linewidth(1);

   winclient(20, 10, x, y, [wmframe, wmsize, wmsysbar]);
   writeln('For (20, 10) client, full frame, window size is: ', x:1, ',', y:1);
   setsiz(win2, x, y);
   page(win2);
   fcolor(win2, black);
   writeln(win2, '12345678901234567890');
   writeln(win2, '2');
   writeln(win2, '3');
   writeln(win2, '4');
   writeln(win2, '5');
   writeln(win2, '6');
   writeln(win2, '7');
   writeln(win2, '8');
   writeln(win2, '9');
   writeln(win2, '0');
   fcolor(win2, cyan);
   rect(win2, 1, 1, 20*chrsizx(win2), 10*chrsizy(win2));
   line(win2, 1, 1, 20*chrsizx(win2), 10*chrsizy(win2));
   line(win2, 1, 10*chrsizy(win2), 20*chrsizx(win2), 1);
   curvis(win2, off);
   writeln('Check client window has (20, 10) surface');
   waitnext;

   writeln('System bar off');
   sysbar(win2, off);
   winclient(20, 10, x, y, [wmframe, wmsize]);
   writeln('For (20, 10) client, no system bar, window size is: ', x:1, ',', y:1);
   setsiz(win2, x, y);
   page(win2);
   fcolor(win2, black);
   writeln(win2, '12345678901234567890');
   writeln(win2, '2');
   writeln(win2, '3');
   writeln(win2, '4');
   writeln(win2, '5');
   writeln(win2, '6');
   writeln(win2, '7');
   writeln(win2, '8');
   writeln(win2, '9');
   writeln(win2, '0');
   fcolor(win2, cyan);
   rect(win2, 1, 1, 20*chrsizx(win2), 10*chrsizy(win2));
   line(win2, 1, 1, 20*chrsizx(win2), 10*chrsizy(win2));
   line(win2, 1, 10*chrsizy(win2), 20*chrsizx(win2), 1);
   curvis(win2, off);
   writeln('Check client window has (20, 10) surface');
   waitnext;

   writeln('Sizing bars off');
   sysbar(win2, on);
   sizable(win2, off);
   winclient(20, 10, x, y, [wmframe, wmsysbar]);
   writeln('For (20, 10) client, no size bars, window size is: ', x:1, ',', y:1);
   setsiz(win2, x, y);
   page(win2);
   fcolor(win2, black);
   writeln(win2, '12345678901234567890');
   writeln(win2, '2');
   writeln(win2, '3');
   writeln(win2, '4');
   writeln(win2, '5');
   writeln(win2, '6');
   writeln(win2, '7');
   writeln(win2, '8');
   writeln(win2, '9');
   writeln(win2, '0');
   fcolor(win2, cyan);
   rect(win2, 1, 1, 20*chrsizx(win2), 10*chrsizy(win2));
   line(win2, 1, 1, 20*chrsizx(win2), 10*chrsizy(win2));
   line(win2, 1, 10*chrsizy(win2), 20*chrsizx(win2), 1);
   curvis(win2, off);
   writeln('Check client window has (20, 10) surface');
   waitnext;

   writeln('frame off');
   sysbar(win2, on);
   sizable(win2, on);
   frame(win2, off);
   winclient(20, 10, x, y, [wmsize, wmsysbar]);
   writeln('For (20, 10) client, no frame, window size is: ', x:1, ',', y:1);
   setsiz(win2, x, y);
   page(win2);
   fcolor(win2, black);
   writeln(win2, '12345678901234567890');
   writeln(win2, '2');
   writeln(win2, '3');
   writeln(win2, '4');
   writeln(win2, '5');
   writeln(win2, '6');
   writeln(win2, '7');
   writeln(win2, '8');
   writeln(win2, '9');
   writeln(win2, '0');
   fcolor(win2, cyan);
   rect(win2, 1, 1, 20*chrsizx(win2), 10*chrsizy(win2));
   line(win2, 1, 1, 20*chrsizx(win2), 10*chrsizy(win2));
   line(win2, 1, 10*chrsizy(win2), 20*chrsizx(win2), 1);
   curvis(win2, off);
   writeln('Check client window has (20, 10) surface');
   waitnext;

   close(win2);

   /* ************************ Window size calculate pixel ******************** */

   page;
   prtceng(maxyg-chrsizy, 'Window size calculate pixel');
   home;
   openwin(input, win2, 2);
   linewidth(1);
   fcolor(win2, cyan);
   winclientg(200, 200, x, y, [wmframe, wmsize, wmsysbar]);
   writeln('For (200, 200) client, full frame, window size is: ', 
           x:1, ',', y:1);
   setsizg(win2, x, y);
   rect(win2, 1, 1, 200, 200);
   line(win2, 1, 1, 200, 200);
   line(win2, 1, 200, 200, 1);
   curvis(win2, off);
   writeln('Check client window has (200, 200) surface');
   waitnext;

   writeln('System bar off');
   sysbar(win2, off);
   winclientg(200, 200, x, y, [wmframe, wmsize]);
   writeln('For (200, 200) client, no system bar, window size is: ', 
           x:1, ',', y:1);
   setsizg(win2, x, y);
   page(win2);
   rect(win2, 1, 1, 200, 200);
   line(win2, 1, 1, 200, 200);
   line(win2, 1, 200, 200, 1);
   writeln('Check client window has (200, 200) surface');
   waitnext;

   writeln('Sizing bars off');
   sysbar(win2, on);
   sizable(win2, off);
   winclientg(200, 200, x, y, [wmframe, wmsysbar]);
   writeln('For (200, 200) client, no sizing, window size is: ', 
           x:1, ',', y:1);
   setsizg(win2, x, y);
   page(win2);
   rect(win2, 1, 1, 200, 200);
   line(win2, 1, 1, 200, 200);
   line(win2, 1, 200, 200, 1);
   writeln('Check client window has (200, 200) surface');
   waitnext;

   writeln('frame off');
   sysbar(win2, on);
   sizable(win2, on);
   frame(win2, off);
   winclientg(200, 200, x, y, [wmsize, wmsysbar]);
   writeln('For (200, 200) client, no frame, window size is: ', 
           x:1, ',', y:1);
   setsizg(win2, x, y);
   page(win2);
   rect(win2, 1, 1, 200, 200);
   line(win2, 1, 1, 200, 200);
   line(win2, 1, 200, 200, 1);
   writeln('Check client window has (200, 200) surface');
   waitnext;

   close(win2);

   /* ******************* Window size calculate minimums pixel *************** */

   /* this test does ! work, winclient needs to return the minimums */
   if false then { 

   page;
   prtceng(maxyg-chrsizy, 'Window size calculate minimum pixel');
   home;
   openwin(input, win2, 2);
   linewidth(1);
   fcolor(win2, cyan);
   winclientg(1, 1, x, y, [wmframe, wmsize, wmsysbar]);
   writeln('For (200, 200) client, full frame, window size minimum is: ', 
           x:1, ',', y:1);
   setsizg(win2, 1, 1);
   getsizg(win2, x2, y2);
   waitnext;

   close(win2);

   };
;};

   /* ********************** Child windows torture test pixel ***************** */

/* ?????? Overflows memory on this test */
   page;
   writeln('Child windows torture test pixel');
   for i = 1 to 100 do {

      openwin(input, win2, output, 2);
      setposg(win2, 1, 100);
      sizbufg(win2, 200, 200);
      setsizg(win2, 200, 200);
      openwin(input, win3, output, 3);
      setposg(win3, 201, 100);
      sizbufg(win3, 200, 200);
      setsizg(win3, 200, 200);
      openwin(input, win4, output, 4);
      setposg(win4, 401, 100);
      sizbufg(win4, 200, 200);
      setsizg(win4, 200, 200);
      bcolor(win2, cyan);
      page(win2);
      writeln(win2, 'I am child window 1');
      bcolor(win3, yellow);
      page(win3);
      writeln(win3, 'I am child window 2');
      bcolor(win4, magenta);
      page(win4);
      writeln(win4, 'I am child window 3');
      close(win2);
      close(win3);
      close(win4);

   };
   home;
   write('Child windows should all be closed');
   waitnext;

   99: /* terminate */

   page;
   auto(off);
   font(font_sign);
   fontsiz(50);
   prtceng(maxyg / 2-chrsizy / 2, 'Test complete');

   refer(error)

}.
