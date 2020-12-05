/*******************************************************************************
*                                                                             *
*                          WIDGET TEST PROGRAM                                *
*                                                                             *
*                    Copyright (C) 2005 Scott A. Moore                        *
*                                                                             *
* Tests the widgets && dialogs available.                                    *
*                                                                             *
*******************************************************************************/

program wigtst(input, output, error);

uses stddef,
     gralib,
     strlib;

label 99;

const second == 10000; /* one second timer */

var er:              evtrec;
    chk, chk2, chk3: boolean;
    s:               pchar*;
    ss, rs:          pchar*;
    prog:            int;
    sp, lp:          strptr;
    x, y:            int;
    r, g, b:         int;
    optf:            qfnopts;
    optfr:           qfropts;
    fc:              int;
    fs:              int;
    fr, fg, fb:      int;
    br, bg, bb:      int;
    fe:              qfteffects; 
    cx, cy:          int;
    ox, oy:          int;

i, cnt: int;
fns: packed array [1..100] of char;

/* wait return to be pressed, || handle terminate */

void waitnext;

var er: evtrec; /* event record */

{

   repeat event(input, er) until (er.etype == etenter) || (er.etype == etterm);
   if er.etype == etterm then goto 99

};

/* draw a character grid */

void chrgrid;

var x, y: int;

{

   fcolor(yellow);
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

{

   writeln('Widget test vs. 0.1');
   writeln;
   writeln('Hit return in any window to continue for each test');
   waitnext;

;if false then {;
   /* ************************** Terminal Button test ************************* */

   bcolor(backcolor);
   page;
   writeln('Background color test');
   writeln;
   writeln('The background color should match widgets now.');
   waitnext;
   bcolor(white);

   /* ************************** Terminal Button test ************************* */

   page;
   chrgrid;
   binvis;
   writeln('Terminal buttons test');
   writeln;
   buttonsiz('Hello, there', x, y);
   button(10, 7, 10+x-1, 7+y-1, 'Hello, there', 1);
   buttonsiz('Bark!', x, y);
   button(10, 10, 10+x-1, 10+y-1, 'Bark!', 2);
   buttonsiz('Sniff', x, y);
   button(10, 13, 10+x-1, 13+y-1, 'Sniff', 3);
   writeln('Hit the buttons, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etbutton then {

         if er.butid == 1 then writeln('Hello to you, too')
         else if er.butid == 2 then writeln('Bark bark')
         else if er.butid == 3 then writeln('Sniff sniff')
         else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   enablewidget(2, false);
   writeln('Now the middle button is disabled, && should ! be able to');
   writeln('be pressed.');
   writeln('Hit the buttons, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etbutton then {

         if er.butid == 1 then writeln('Hello to you, too')
         else if er.butid == 2 then writeln('Bark bark')
         else if er.butid == 3 then writeln('Sniff sniff')
         else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);

   /* ************************* Graphical Button test ************************* */

   page;
   writeln('Graphical buttons test');
   writeln;
   buttonsizg('Hello, there', x, y);
   buttong(100, 100, 100+x, 100+y, 'Hello, there', 1);
   buttonsizg('Bark!', x, y);
   buttong(100, 150, 100+x, 150+y, 'Bark!', 2);
   buttonsizg('Sniff', x, y);
   buttong(100, 200, 100+x, 200+y, 'Sniff', 3);
   writeln('Hit the buttons, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etbutton then {

         if er.butid == 1 then writeln('Hello to you, too')
         else if er.butid == 2 then writeln('Bark bark')
         else if er.butid == 3 then writeln('Sniff sniff')
         else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   enablewidget(2, false);
   writeln('Now the middle button is disabled, && should ! be able to');
   writeln('be pressed.');
   writeln('Hit the buttons, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etbutton then {

         if er.butid == 1 then writeln('Hello to you, too')
         else if er.butid == 2 then writeln('Bark bark')
         else if er.butid == 3 then writeln('Sniff sniff')
         else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);

   /* ************************** Terminal Checkbox test ************************** */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal checkbox test');
   writeln;
   chk = false;
   chk2 = false;
   chk3 = false;
   checkboxsiz('Pick me', x, y);
   checkbox(10, 7, 10+x-1, 7+y-1, 'Pick me', 1);
   checkboxsiz('Or me', x, y);
   checkbox(10, 10, 10+x-1, 10+y-1, 'Or me', 2);
   checkboxsiz('No, me', x, y);
   checkbox(10, 13, 10+x-1, 13+y-1, 'No, me', 3);
   writeln('Hit the checkbox, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etchkbox then {

         if er.ckbxid == 1 then {

            writeln('You selected the top checkbox');
            chk = ! chk;
            selectwidget(1, chk)

         } else if er.ckbxid == 2 then {

            writeln('You selected the middle checkbox');
            chk2 = ! chk2;
            selectwidget(2, chk2)

         } else if er.ckbxid == 3 then {

            writeln('You selected the bottom checkbox');
            chk3 = ! chk3;
            selectwidget(3, chk3)

         } else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   enablewidget(2, false);
   writeln('Now the middle checkbox is disabled, && should ! be able to');
   writeln('be pressed.');
   writeln('Hit the checkbox, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etchkbox then {

         if er.ckbxid == 1 then {

            writeln('You selected the top checkbox');
            chk = ! chk;
            selectwidget(1, chk)

         } else if er.ckbxid == 2 then {

            writeln('You selected the middle checkbox');
            chk2 = ! chk2;
            selectwidget(2, chk2)

         } else if er.ckbxid == 3 then {

            writeln('You selected the bottom checkbox');
            chk3 = ! chk3;
            selectwidget(3, chk3)

         } else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);

   /* ************************** Graphical Checkbox test ************************** */
   
   page;
   writeln('Graphical checkbox test');
   writeln;
   chk = false;
   chk2 = false;
   chk3 = false;
   checkboxsizg('Pick me', x, y);
   checkboxg(100, 100, 100+x, 100+y, 'Pick me', 1);
   checkboxsizg('Or me', x, y);
   checkboxg(100, 150, 100+x, 150+y, 'Or me', 2);
   checkboxsizg('No, me', x, y);
   checkboxg(100, 200, 100+x, 200+y, 'No, me', 3);
   writeln('Hit the checkbox, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etchkbox then {

         if er.ckbxid == 1 then {

            writeln('You selected the top checkbox');
            chk = ! chk;
            selectwidget(1, chk)

         } else if er.ckbxid == 2 then {

            writeln('You selected the middle checkbox');
            chk2 = ! chk2;
            selectwidget(2, chk2)

         } else if er.ckbxid == 3 then {

            writeln('You selected the bottom checkbox');
            chk3 = ! chk3;
            selectwidget(3, chk3)

         } else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   enablewidget(2, false);
   writeln('Now the middle checkbox is disabled, && should ! be able to');
   writeln('be pressed.');
   writeln('Hit the checkbox, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etchkbox then {

         if er.ckbxid == 1 then {

            writeln('You selected the top checkbox');
            chk = ! chk;
            selectwidget(1, chk)

         } else if er.ckbxid == 2 then {

            writeln('You selected the middle checkbox');
            chk2 = ! chk2;
            selectwidget(2, chk2)

         } else if er.ckbxid == 3 then {

            writeln('You selected the bottom checkbox');
            chk3 = ! chk3;
            selectwidget(3, chk3)

         } else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);

   /* *********************** Terminal radio button test ********************* */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal radio button test');
   writeln;
   chk = false;
   chk2 = false;
   chk3 = false;
   radiobuttonsiz('Station 1', x, y);
   radiobutton(10, 7, 10+x-1, 7+y-1, 'Station 1', 1);
   radiobuttonsiz('Station 2', x, y);
   radiobutton(10, 10, 10+x-1, 10+y-1, 'Station 2', 2);
   radiobuttonsiz('Station 3', x, y);
   radiobutton(10, 13, 10+x-1, 13+y-1, 'Station 3', 3);
   writeln('Hit the radio button, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etradbut then {

         if er.radbid == 1 then {

            writeln('You selected the top checkbox');
            chk = ! chk;
            selectwidget(1, chk)

         } else if er.radbid == 2 then {

            writeln('You selected the middle checkbox');
            chk2 = ! chk2;
            selectwidget(2, chk2)

         } else if er.radbid == 3 then {

            writeln('You selected the bottom checkbox');
            chk3 = ! chk3;
            selectwidget(3, chk3)

         } else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   enablewidget(2, false);
   writeln('Now the middle radio button is disabled, && should ! be able');
   writeln('to be pressed.');
   writeln('Hit the radio button, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etradbut then {

         if er.radbid == 1 then {

            writeln('You selected the top checkbox');
            chk = ! chk;
            selectwidget(1, chk)

         } else if er.radbid == 2 then {

            writeln('You selected the middle checkbox');
            chk2 = ! chk2;
            selectwidget(2, chk2)

         } else if er.radbid == 3 then {

            writeln('You selected the bottom checkbox');
            chk3 = ! chk3;
            selectwidget(3, chk3)

         } else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);

   /* *********************** Graphical radio button test ********************* */
   
   page;
   writeln('Graphical radio button test');
   writeln;
   chk = false;
   chk2 = false;
   chk3 = false;
   radiobuttonsizg('Station 1', x, y);
   radiobuttong(100, 100, 100+x, 100+y, 'Station 1', 1);
   radiobuttonsizg('Station 2', x, y);
   radiobuttong(100, 150, 100+x, 150+y, 'Station 2', 2);
   radiobuttonsizg('Station 3', x, y);
   radiobuttong(100, 200, 100+x, 200+y, 'Station 3', 3);
   writeln('Hit the radio button, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etradbut then {

         if er.radbid == 1 then {

            writeln('You selected the top checkbox');
            chk = ! chk;
            selectwidget(1, chk)

         } else if er.radbid == 2 then {

            writeln('You selected the middle checkbox');
            chk2 = ! chk2;
            selectwidget(2, chk2)

         } else if er.radbid == 3 then {

            writeln('You selected the bottom checkbox');
            chk3 = ! chk3;
            selectwidget(3, chk3)

         } else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   enablewidget(2, false);
   writeln('Now the middle radio button is disabled, && should ! be able');
   writeln('to be pressed.');
   writeln('Hit the radio button, || return to continue');
   writeln;
   repeat

      event(er);
      if er.etype == etradbut then {

         if er.radbid == 1 then {

            writeln('You selected the top checkbox');
            chk = ! chk;
            selectwidget(1, chk)

         } else if er.radbid == 2 then {

            writeln('You selected the middle checkbox');
            chk2 = ! chk2;
            selectwidget(2, chk2)

         } else if er.radbid == 3 then {

            writeln('You selected the bottom checkbox');
            chk3 = ! chk3;
            selectwidget(3, chk3)

         } else writeln('!!! No button with id: ', er.butid:1, ' !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);


   /* *********************** Terminal Group box test ************************ */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal group box test');
   writeln;
   groupsiz('Hello there', 0, 0, x, y, ox, oy);
   group(10, 10, 10+x, 10+y, 'Hello there', 1);
   writeln('This is a group box with a null client area');
   writeln('Hit return to continue');
   waitnext;
   killwidget(1);
   groupsiz('Hello there', 20, 10, x, y, ox, oy);
   group(10, 10, 10+x, 10+y, 'Hello there', 1);
   writeln('This is a group box with a 20,10 client area');
   writeln('Hit return to continue');
   waitnext;
   killwidget(1);
   groupsiz('Hello there', 20, 10, x, y, ox, oy);
   group(10, 10, 10+x, 10+y, 'Hello there', 1);
   button(10+ox, 10+oy, 10+ox+20-1, 10+oy+10-1, 'Bark, bark!', 2);
   writeln('This is a group box with a 20,10 layered button');
   writeln('Hit return to continue');
   waitnext;
   killwidget(1);
   killwidget(2);

   /* *********************** Graphical Group box test ************************ */
   
   page;
   writeln('Graphical group box test');
   writeln;
   groupsizg('Hello there', 0, 0, x, y, ox, oy);
   groupg(100, 100, 100+x, 100+y, 'Hello there', 1);
   writeln('This is a group box with a null client area');
   writeln('Hit return to continue');
   waitnext;
   killwidget(1);
   groupsizg('Hello there', 200, 200, x, y, ox, oy);
   groupg(100, 100, 100+x, 100+y, 'Hello there', 1);
   writeln('This is a group box with a 200,200 client area');
   writeln('Hit return to continue');
   waitnext;
   killwidget(1);
   groupsizg('Hello there', 200, 200, x, y, ox, oy);
   groupg(100, 100, 100+x, 100+y, 'Hello there', 1);
   buttong(100+ox, 100+oy, 100+ox+200, 100+oy+200, 'Bark, bark!', 2);
   writeln('This is a group box with a 200,200 layered button');
   writeln('Hit return to continue');
   waitnext;
   killwidget(1);
   killwidget(2);

   /* *********************** Terminal background test ************************ */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal background test');
   writeln;
   background(10, 10, 40, 20, 1);
   writeln('Hit return to continue');
   waitnext;
   button(11, 11, 39, 19, 'Bark, bark!', 2);
   writeln('This is a background with a layered button');
   writeln('Hit return to continue');
   waitnext;
   killwidget(1);
   killwidget(2);

   /* *********************** Graphical background test ************************ */
   
   page;
   writeln('Graphical background test');
   writeln;
   backgroundg(100, 100, 400, 200, 1);
   writeln('Hit return to continue');
   waitnext;
   buttong(110, 110, 390, 190, 'Bark, bark!', 2);
   writeln('This is a background with a layered button');
   writeln('Hit return to continue');
   waitnext;
   killwidget(1);
   killwidget(2);

   /* *********************** Terminal scroll bar test *********************** */

   page;
   chrgrid;
   binvis;
   writeln('Terminal scroll bar test');
   writeln;
   scrollvertsiz(x, y);
   scrollvert(10, 10, 10+x-1, 20, 1);
   scrollhorizsiz(x, y);
   scrollhoriz(15, 10, 35, 10+y-1, 2);
   repeat

      event(er);
      if er.etype == etsclull then 
         writeln('Scrollbar: ', er.sclulid:1, ' up/left line');
      if er.etype == etscldrl then 
         writeln('Scrollbar: ', er.scldlid:1, ' down/right line');
      if er.etype == etsclulp then 
         writeln('Scrollbar: ', er.sclupid:1, ' up/left page');
      if er.etype == etscldrp then 
         writeln('Scrollbar: ', er.scldpid:1, ' down/right page');
      if er.etype == etsclpos then {

         scrollpos(er.sclpid, er.sclpos); /* set new position for scrollbar */
         writeln('Scrollbar: ', er.sclpid:1, ' position set: ', er.sclpos:1);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);

   /* ******************* Terminal scroll bar sizing test ******************** */

   page;
   chrgrid;
   binvis;
   writeln('Terminal scroll bar sizing test');
   writeln;
   scrollvert(10, 10, 12, 20, 1);
   scrollsiz(1, (INT_MAX / 4)*3);
   scrollvert(10+5, 10, 12+5, 20, 2);
   scrollsiz(2, INT_MAX / 2);
   scrollvert(10+10, 10, 12+10, 20, 3);
   scrollsiz(3, INT_MAX / 4);
   scrollvert(10+15, 10, 12+15, 20, 4);
   scrollsiz(4, INT_MAX / 8);
   writeln('Now should be four scrollbars, dec}ing in size to the right.');
   writeln('All of the scrollbars can be manipulated.');
   repeat

      event(er);
      if er.etype == etsclull then 
         writeln('Scrollbar: ', er.sclulid:1, ' up/left line');
      if er.etype == etscldrl then 
         writeln('Scrollbar: ', er.scldlid:1, ' down/right line');
      if er.etype == etsclulp then 
         writeln('Scrollbar: ', er.sclupid:1, ' up/left page');
      if er.etype == etscldrp then 
         writeln('Scrollbar: ', er.scldpid:1, ' down/right page');
      if er.etype == etsclpos then {

         scrollpos(er.sclpid, er.sclpos); /* set new position for scrollbar */
         writeln('Scrollbar: ', er.sclpid:1, ' position set: ', er.sclpos:1);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);

   /* ****************** Terminal scroll bar minimums test ******************* */

   page;
   chrgrid;
   binvis;
   writeln('Terminal scroll bar minimums test');
   writeln;
   scrollvertsiz(x, y);
   scrollvert(10, 10, 10+x-1, 10+y-1, 1);
   scrollhorizsiz(x, y);
   scrollhoriz(15, 10, 15+x-1, 10+y-1, 2);
   repeat

      event(er);
      if er.etype == etsclull then 
         writeln('Scrollbar: ', er.sclulid:1, ' up/left line');
      if er.etype == etscldrl then 
         writeln('Scrollbar: ', er.scldlid:1, ' down/right line');
      if er.etype == etsclulp then 
         writeln('Scrollbar: ', er.sclupid:1, ' up/left page');
      if er.etype == etscldrp then 
         writeln('Scrollbar: ', er.scldpid:1, ' down/right page');
      if er.etype == etsclpos then {

         scrollpos(er.sclpid, er.sclpos); /* set new position for scrollbar */
         writeln('Scrollbar: ', er.sclpid:1, ' position set: ', er.sclpos:1);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);

   /* ************ Terminal scroll bar fat && skinny bars test ************** */

   page;
   chrgrid;
   binvis;
   writeln('Terminal scroll bar fat && skinny bars test');
   writeln;
   scrollvertsiz(x, y);
   scrollvert(10, 10, 10, 10+10, 1);
   scrollvert(12, 10, 20, 10+10, 3);
   scrollhorizsiz(x, y);
   scrollhoriz(30, 10, 30+20, 10, 2);
   scrollhoriz(30, 12, 30+20, 20, 4);
   repeat

      event(er);
      if er.etype == etsclull then 
         writeln('Scrollbar: ', er.sclulid:1, ' up/left line');
      if er.etype == etscldrl then 
         writeln('Scrollbar: ', er.scldlid:1, ' down/right line');
      if er.etype == etsclulp then 
         writeln('Scrollbar: ', er.sclupid:1, ' up/left page');
      if er.etype == etscldrp then 
         writeln('Scrollbar: ', er.scldpid:1, ' down/right page');
      if er.etype == etsclpos then {

         scrollpos(er.sclpid, er.sclpos); /* set new position for scrollbar */
         writeln('Scrollbar: ', er.sclpid:1, ' position set: ', er.sclpos:1);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);
;};

   /* *********************** Graphical scroll bar test *********************** */

   page;
   writeln('Graphical scroll bar test');
   writeln;
   scrollvertsizg(x, y);
   scrollvertg(100, 100, 100+x, 300, 1);
   scrollhorizsizg(x, y);
   scrollhorizg(150, 100, 350, 100+y, 2);
   repeat

      event(er);
      if er.etype == etsclull then 
         writeln('Scrollbar: ', er.sclulid:1, ' up/left line');
      if er.etype == etscldrl then 
         writeln('Scrollbar: ', er.scldlid:1, ' down/right line');
      if er.etype == etsclulp then 
         writeln('Scrollbar: ', er.sclupid:1, ' up/left page');
      if er.etype == etscldrp then 
         writeln('Scrollbar: ', er.scldpid:1, ' down/right page');
      if er.etype == etsclpos then {

         scrollpos(er.sclpid, er.sclpos); /* set new position for scrollbar */
         writeln('Scrollbar: ', er.sclpid:1, ' position set: ', er.sclpos:1);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);

   /* ******************* Graphical scroll bar sizing test ******************** */

   page;
   writeln('Graphical scroll bar sizing test');
   writeln;
   scrollvertg(100, 100, 120, 300, 1);
   scrollsiz(1, (INT_MAX / 4)*3);
   scrollvertg(100+50, 100, 120+50, 300, 2);
   scrollsiz(2, INT_MAX / 2);
   scrollvertg(100+100, 100, 120+100, 300, 3);
   scrollsiz(3, INT_MAX / 4);
   scrollvertg(100+150, 100, 120+150, 300, 4);
   scrollsiz(4, INT_MAX / 8);
   writeln('Now should be four scrollbars, dec}ing in size to the right.');
   writeln('All of the scrollbars can be manipulated.');
   repeat

      event(er);
      if er.etype == etsclull then 
         writeln('Scrollbar: ', er.sclulid:1, ' up/left line');
      if er.etype == etscldrl then 
         writeln('Scrollbar: ', er.scldlid:1, ' down/right line');
      if er.etype == etsclulp then 
         writeln('Scrollbar: ', er.sclupid:1, ' up/left page');
      if er.etype == etscldrp then 
         writeln('Scrollbar: ', er.scldpid:1, ' down/right page');
      if er.etype == etsclpos then {

         scrollpos(er.sclpid, er.sclpos); /* set new position for scrollbar */
         writeln('Scrollbar: ', er.sclpid:1, ' position set: ', er.sclpos:1);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);

   /* ****************** Graphical scroll bar minimums test ******************* */

   page;
   writeln('Graphical scroll bar minimums test');
   writeln;
   scrollvertsizg(x, y);
   scrollvertg(100, 100, 100+x, 100+y, 1);
   scrollhorizsizg(x, y);
   scrollhorizg(150, 100, 150+x, 100+y, 2);
   repeat

      event(er);
      if er.etype == etsclull then 
         writeln('Scrollbar: ', er.sclulid:1, ' up/left line');
      if er.etype == etscldrl then 
         writeln('Scrollbar: ', er.scldlid:1, ' down/right line');
      if er.etype == etsclulp then 
         writeln('Scrollbar: ', er.sclupid:1, ' up/left page');
      if er.etype == etscldrp then 
         writeln('Scrollbar: ', er.scldpid:1, ' down/right page');
      if er.etype == etsclpos then {

         scrollpos(er.sclpid, er.sclpos); /* set new position for scrollbar */
         writeln('Scrollbar: ', er.sclpid:1, ' position set: ', er.sclpos:1);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);

   /* ************ Graphical scroll bar fat && skinny bars test ************** */

   page;
   writeln('Graphical scroll bar fat && skinny bars test');
   writeln;
   scrollvertsizg(x, y);
   scrollvertg(100, 100, 100+x / 2, 100+200, 1);
   scrollvertg(120, 100, 200, 100+200, 3);
   scrollhorizsizg(x, y);
   scrollhorizg(250, 100, 250+200, 100+y / 2, 2);
   scrollhorizg(250, 120, 250+200, 200, 4);
   repeat

      event(er);
      if er.etype == etsclull then 
         writeln('Scrollbar: ', er.sclulid:1, ' up/left line');
      if er.etype == etscldrl then 
         writeln('Scrollbar: ', er.scldlid:1, ' down/right line');
      if er.etype == etsclulp then 
         writeln('Scrollbar: ', er.sclupid:1, ' up/left page');
      if er.etype == etscldrp then 
         writeln('Scrollbar: ', er.scldpid:1, ' down/right page');
      if er.etype == etsclpos then {

         scrollpos(er.sclpid, er.sclpos); /* set new position for scrollbar */
         writeln('Scrollbar: ', er.sclpid:1, ' position set: ', er.sclpos:1);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);

   /* ******************** Terminal number select box test ******************* */

   page;
   chrgrid;
   binvis;
   writeln('Terminal number select box test');
   writeln;
   numselboxsiz(1, 10, x, y);
   numselbox(10, 10, 10+x-1, 10+y-1, 1, 10, 1);
   repeat

      event(er);
      if er.etype == etnumbox then writeln('You selected: ', er.numbsl:1);
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);


   /* ******************** Graphical number select box test ******************* */

   page;
   writeln('Graphical number select box test');
   writeln;
   numselboxsizg(1, 10, x, y);
   numselboxg(100, 100, 100+x, 100+y, 1, 10, 1);
   repeat

      event(er);
      if er.etype == etnumbox then writeln('You selected: ', er.numbsl:1);
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* ************************* Terminal edit box test ************************ */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal edit box test');
   writeln;
   editboxsiz('Hi there, george', x, y);
   editbox(10, 10, 10+x-1, 10+y-1, 1);
   putwidgettext(1, 'Hi there, george');
   repeat

      event(er);
      if er.etype == etedtbox then {

         getwidgettext(1, s);
         writeln('You entered: ', s^)

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* ************************* Graphical edit box test ************************ */
   
   page;
   writeln('Graphical edit box test');
   writeln;
   editboxsizg('Hi there, george', x, y);
   editboxg(100, 100, 100+x-1, 100+y-1, 1);
   putwidgettext(1, 'Hi there, george');
   repeat

      event(er);
      if er.etype == etedtbox then {

         getwidgettext(1, s);
         writeln('You entered: ', s^)

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* *********************** Terminal progress bar test ********************* */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal progress bar test');
   writeln;
   progbarsiz(x, y);
   progbar(10, 10, 10+x-1, 10+y-1, 1);
   timer(1, second, true);
   prog = 1;
   repeat

      event(er);
      if er.etype == ettim then {

         if prog < 20 then {

            progbarpos(1, INT_MAX-((20-prog)*(INT_MAX / 20)));
            prog = prog+1 /* next progress value */
      
         } else if prog == 20 then {

            progbarpos(1, INT_MAX);
            writeln('Done !');
            prog = 11;
            killtimer(1)

         }   

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* *********************** Graphical progress bar test ********************* */
   
   page;
   writeln('Graphical progress bar test');
   writeln;
   progbarsizg(x, y);
   progbarg(100, 100, 100+x-1, 100+y-1, 1);
   timer(1, second, true);
   prog = 1;
   repeat

      event(er);
      if er.etype == ettim then {

         if prog < 20 then {

            progbarpos(1, INT_MAX-((20-prog)*(INT_MAX / 20)));
            prog = prog+1 /* next progress value */
      
         } else if prog == 20 then {

            progbarpos(1, INT_MAX);
            writeln('Done !');
            prog = 11;
            killtimer(1)

         }   

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* ************************* Terminal list box test ************************ */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal list box test');
   writeln;
   writeln('Note that it is normal for this box to ! fill to exact');
   writeln('character cells.');
   writeln;
   new(lp);
   copy(lp->str, 'Blue');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Red');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Green');
   sp->next = lp;
   lp = sp;
   listboxsiz(lp, x, y);
   listbox(10, 10, 10+x-1, 10+y-1, lp, 1);
   repeat

      event(er);
      if er.etype == etlstbox then {

         case er.lstbsl of

            1: writeln('You selected green');
            2: writeln('You selected red');
            3: writeln('You selected blue');
            else writeln('!!! Bad select number !!!')

         }

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* ************************* Graphical list box test ************************ */
   
   page;
   writeln('Graphical list box test');
   writeln;
   new(lp);
   copy(lp->str, 'Blue');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Red');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Green');
   sp->next = lp;
   lp = sp;
   listboxsizg(lp, x, y);
   listboxg(100, 100, 100+x-1, 100+y-1, lp, 1);
   repeat

      event(er);
      if er.etype == etlstbox then {

         case er.lstbsl of

            1: writeln('You selected green');
            2: writeln('You selected red');
            3: writeln('You selected blue');
            else writeln('!!! Bad select number !!!')

         }

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* ************************* Terminal dropdown box test ************************ */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal dropdown box test');
   writeln;
   writeln('Note that it is normal for this box to ! fill to exact');
   writeln('character cells.');
   writeln;
   new(lp);
   copy(lp->str, 'Dog');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Cat');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Bird');
   sp->next = lp;
   lp = sp;
   dropboxsiz(lp, cx, cy, ox, oy);
   dropbox(10, 10, 10+ox-1, 10+oy-1, lp, 1);
   repeat

      event(er);
      if er.etype == etdrpbox then {

         case er.drpbsl of

            1: writeln('You selected Bird');
            2: writeln('You selected Cat');
            3: writeln('You selected Dog');
            else writeln('!!! Bad select number !!!')

         }

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* ************************* Graphical dropdown box test ************************ */
   
   page;
   writeln('Graphical dropdown box test');
   writeln;
   new(lp);
   copy(lp->str, 'Dog');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Cat');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Bird');
   sp->next = lp;
   lp = sp;
   dropboxsizg(lp, cx, cy, ox, oy);
   dropboxg(100, 100, 100+ox-1, 100+oy-1, lp, 1);
   repeat

      event(er);
      if er.etype == etdrpbox then {

         case er.drpbsl of

            1: writeln('You selected Bird');
            2: writeln('You selected Cat');
            3: writeln('You selected Dog');
            else writeln('!!! Bad select number !!!')

         }

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* ******************* Terminal dropdown edit box test ******************** */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal dropdown edit box test');
   writeln;
   writeln('Note that it is normal for this box to ! fill to exact');
   writeln('character cells.');
   writeln;
   new(lp);
   copy(lp->str, 'Corn');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Flower');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Tortillas');
   sp->next = lp;
   lp = sp;
   dropeditboxsiz(lp, cx, cy, ox, oy);
   dropeditbox(10, 10, 10+ox-1, 10+oy-1, lp, 1);
   repeat

      event(er);
      if er.etype == etdrebox then {

         getwidgettext(1, s);
         writeln('You selected: ', s^);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* ******************* Graphical dropdown edit box test ******************** */
   
   page;
   writeln('Graphical dropdown edit box test');
   writeln;
   new(lp);
   copy(lp->str, 'Corn');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Flower');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Tortillas');
   sp->next = lp;
   lp = sp;
   dropeditboxsizg(lp, cx, cy, ox, oy);
   dropeditboxg(100, 100, 100+ox-1, 100+oy-1, lp, 1);
   repeat

      event(er);
      if er.etype == etdrebox then {

         getwidgettext(1, s);
         writeln('You selected: ', s^);

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);

   /* ************************* Terminal slider test ************************ */
   
   page;
   chrgrid;
   binvis;
   writeln('Terminal slider test');
   slidehorizsiz(x, y);
   slidehoriz(10, 10, 10+x-1, 10+y-1, 10, 1);
   slidehoriz(10, 20, 10+x-1, 20+y-1, 0, 2);
   slidevertsiz(x, y);
   slidevert(40, 10, 40+x-1, 10+y-1, 10, 3);
   slidevert(50, 10, 50+x-1, 10+y-1, 0, 4);
   writeln('Bottom && right sliders should ! have tick marks');
   repeat

      event(er);
      if er.etype == etsldpos then 
         writeln('Slider id: ', er.sldpid:1, ' position: ', er.sldpos:1);
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);

   /* ************************* Graphical slider test ************************ */
   
   page;
   writeln('Graphical slider test');
   slidehorizsizg(x, y);
   slidehorizg(100, 100, 100+x-1, 100+y-1, 10, 1);
   slidehorizg(100, 200, 100+x-1, 200+y-1, 0, 2);
   slidevertsizg(x, y);
   slidevertg(400, 100, 400+x-1, 100+y-1, 10, 3);
   slidevertg(500, 100, 500+x-1, 100+y-1, 0, 4);
   writeln('Bottom && right sliders should ! have tick marks');
   repeat

      event(er);
      if er.etype == etsldpos then 
         writeln('Slider id: ', er.sldpid:1, ' position: ', er.sldpos:1);
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);

   /* ************************* Terminal tab bar test ************************ */

   page;
   chrgrid;
   binvis;
   writeln('Terminal tab bar test');
   writeln;
   new(lp);
   copy(lp->str, 'Right');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Left');
   sp->next = lp;
   lp = sp;
   tabbarsiz(totop, 20, 2, x, y, ox, oy);
   tabbar(15, 3, 15+x-1, 3+y-1, lp, totop, 1);

   new(lp);
   copy(lp->str, 'Bottom');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Top');
   sp->next = lp;
   lp = sp;
   tabbarsiz(toright, 2, 12, x, y, ox, oy);
   tabbar(40, 7, 40+x-1, 7+y-1, lp, toright, 2);

   new(lp);
   copy(lp->str, 'Right');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Left');
   sp->next = lp;
   lp = sp;
   tabbarsiz(tobottom, 20, 2, x, y, ox, oy);
   tabbar(15, 20, 15+x-1, 20+y-1, lp, tobottom, 3);

   new(lp);
   copy(lp->str, 'Bottom');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Top');
   sp->next = lp;
   lp = sp;
   tabbarsiz(toleft, 2, 12, x, y, ox, oy);
   tabbar(5, 7, 5+x-1, 7+y-1, lp, toleft, 4);

   repeat

      event(er);
      if er.etype == ettabbar then {

         if er.tabid == 1 then case er.tabsel of

            1: writeln('Top bar: You selected Left');
            2: writeln('Top bar: You selected Center');
            3: writeln('Top bar: You selected Right');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 2 then case er.tabsel of

            1: writeln('Right bar: You selected Top');
            2: writeln('Right bar: You selected Center');
            3: writeln('Right bar: You selected Bottom');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 3 then case er.tabsel of

            1: writeln('Bottom bar: You selected Left');
            2: writeln('Bottom bar: You selected Center');
            3: writeln('Bottom bar: You selected right');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 4 then case er.tabsel of

            1: writeln('Left bar: You selected Top');
            2: writeln('Left bar: You selected Center');
            3: writeln('Left bar: You selected Bottom');
            else writeln('!!! Bad select number !!!')

         } else writeln('!!! Bad tab id !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);

   /* ************************* Graphical tab bar test ************************ */

;bcolor(cyan);
   page;
   writeln('Graphical tab bar test');
   writeln;
   new(lp);
   copy(lp->str, 'Right');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Left');
   sp->next = lp;
   lp = sp;
   tabbarsizg(totop, 200, 20, x, y, ox, oy);
;line(1, 50, maxxg, 50);
;line(150, 1, 150, maxyg);
   tabbarg(150, 50, 150+x-1, 50+y-1, lp, totop, 1);

   new(lp);
   copy(lp->str, 'Bottom');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Top');
   sp->next = lp;
   lp = sp;
   tabbarsizg(toright, 20, 200, x, y, ox, oy);
   tabbarg(400, 100, 400+x-1, 100+y-1, lp, toright, 2);

   new(lp);
   copy(lp->str, 'Right');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Left');
   sp->next = lp;
   lp = sp;
   tabbarsizg(tobottom, 200, 20, x, y, ox, oy);
   tabbarg(150, 300, 150+x-1, 300+y-1, lp, tobottom, 3);

   new(lp);
   copy(lp->str, 'Bottom');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Top');
   sp->next = lp;
   lp = sp;
   tabbarsizg(toleft, 20, 200, x, y, ox, oy);
   tabbarg(50, 100, 50+x-1, 100+y-1, lp, toleft, 4);

   repeat

      event(er);
      if er.etype == ettabbar then {

         if er.tabid == 1 then case er.tabsel of

            1: writeln('Top bar: You selected Left');
            2: writeln('Top bar: You selected Center');
            3: writeln('Top bar: You selected Right');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 2 then case er.tabsel of

            1: writeln('Right bar: You selected Top');
            2: writeln('Right bar: You selected Center');
            3: writeln('Right bar: You selected Bottom');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 3 then case er.tabsel of

            1: writeln('Bottom bar: You selected Left');
            2: writeln('Bottom bar: You selected Center');
            3: writeln('Bottom bar: You selected right');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 4 then case er.tabsel of

            1: writeln('Left bar: You selected Top');
            2: writeln('Left bar: You selected Center');
            3: writeln('Left bar: You selected Bottom');
            else writeln('!!! Bad select number !!!')

         } else writeln('!!! Bad tab id !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);

   /* ************************* Terminal overlaid tab bar test ************************ */

   page;
   chrgrid;
   binvis;
   writeln('Terminal overlaid tab bar test');
   writeln;

   new(lp);
   copy(lp->str, 'Right');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Left');
   sp->next = lp;
   lp = sp;
   tabbarsiz(totop, 30, 12, x, y, ox, oy);
   tabbar(20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, totop, 1);

   new(lp);
   copy(lp->str, 'Bottom');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Top');
   sp->next = lp;
   lp = sp;
   tabbarsiz(toright, 30, 12, x, y, ox, oy);
   tabbar(20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, toright, 2);

   new(lp);
   copy(lp->str, 'Right');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Left');
   sp->next = lp;
   lp = sp;
   tabbarsiz(tobottom, 30, 12, x, y, ox, oy);
   tabbar(20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, tobottom, 3);

   new(lp);
   copy(lp->str, 'Bottom');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Top');
   sp->next = lp;
   lp = sp;
   tabbarsiz(toleft, 30, 12, x, y, ox, oy);
   tabbar(20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, toleft, 4);

   repeat

      event(er);
      if er.etype == ettabbar then {

         if er.tabid == 1 then case er.tabsel of

            1: writeln('Top bar: You selected Left');
            2: writeln('Top bar: You selected Center');
            3: writeln('Top bar: You selected Right');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 2 then case er.tabsel of

            1: writeln('Right bar: You selected Top');
            2: writeln('Right bar: You selected Center');
            3: writeln('Right bar: You selected Bottom');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 3 then case er.tabsel of

            1: writeln('Bottom bar: You selected Left');
            2: writeln('Bottom bar: You selected Center');
            3: writeln('Bottom bar: You selected riht');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 4 then case er.tabsel of

            1: writeln('Left bar: You selected Top');
            2: writeln('Left bar: You selected Center');
            3: writeln('Left bar: You selected Bottom');
            else writeln('!!! Bad select number !!!')

         } else writeln('!!! Bad tab id !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);

   /* ************************* Graphical overlaid tab bar test ************************ */

   page;
   writeln('Graphical overlaid tab bar test');
   writeln;
   new(lp);
   copy(lp->str, 'Right');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Left');
   sp->next = lp;
   lp = sp;
   tabbarsizg(totop, 200, 200, x, y, ox, oy);
   tabbarg(200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, totop, 1);

   new(lp);
   copy(lp->str, 'Bottom');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Top');
   sp->next = lp;
   lp = sp;
   tabbarsizg(toright, 200, 200, x, y, ox, oy);
   tabbarg(200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, toright, 2);

   new(lp);
   copy(lp->str, 'Right');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Left');
   sp->next = lp;
   lp = sp;
   tabbarsizg(tobottom, 200, 200, x, y, ox, oy);
   tabbarg(200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, tobottom, 3);

   new(lp);
   copy(lp->str, 'Bottom');
   lp->next = nil;
   new(sp);
   copy(sp->str, 'Center');
   sp->next = lp;
   lp = sp;
   new(sp);
   copy(sp->str, 'Top');
   sp->next = lp;
   lp = sp;
   tabbarsizg(toleft, 200, 200, x, y, ox, oy);
   tabbarg(200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, toleft, 4);

   repeat

      event(er);
      if er.etype == ettabbar then {

         if er.tabid == 1 then case er.tabsel of

            1: writeln('Top bar: You selected Left');
            2: writeln('Top bar: You selected Center');
            3: writeln('Top bar: You selected Right');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 2 then case er.tabsel of

            1: writeln('Right bar: You selected Top');
            2: writeln('Right bar: You selected Center');
            3: writeln('Right bar: You selected Bottom');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 3 then case er.tabsel of

            1: writeln('Bottom bar: You selected Left');
            2: writeln('Bottom bar: You selected Center');
            3: writeln('Bottom bar: You selected riht');
            else writeln('!!! Bad select number !!!')

         } else if er.tabid == 4 then case er.tabsel of

            1: writeln('Left bar: You selected Top');
            2: writeln('Left bar: You selected Center');
            3: writeln('Left bar: You selected Bottom');
            else writeln('!!! Bad select number !!!')

         } else writeln('!!! Bad tab id !!!')

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   killwidget(1);
   killwidget(2);
   killwidget(3);
   killwidget(4);

   /* ************************* Alert test ************************ */

   page;
   writeln('Alert test');
   writeln;
   writeln('There should be an alert dialog');
   writeln('Both the dialog && this window should be fully reactive');
   alert('This is an important message', 'There has been an event !');
   writeln;
   writeln('Alert dialog should have completed now');
   waitnext;

   /* ************************* Color query test ************************ */

   page;
   writeln('Color query test');
   writeln;
   writeln('There should be an color query dialog');
   writeln('Both the dialog && this window should be fully reactive');
   writeln('The color white should be the default selection');
   r = INT_MAX;
   g = INT_MAX;
   b = INT_MAX;
   querycolor(r, g, b);
   writeln;
   writeln('Dialog should have completed now');
   writeln('Colors are: red: ', r:1,  ' green: ', g:1, ' blue: ', b:1);
   waitnext;

   /* ************************* Open file query test ************************ */

   page;
   writeln('Open file query test');
   writeln;
   writeln('There should be an open file query dialog');
   writeln('Both the dialog && this window should be fully reactive');
   writeln('The dialog should have "myfile.txt" as the default filename');
   copy(s, 'myfile.txt');
   queryopen(s);
   writeln;
   writeln('Dialog should have completed now');
   writeln('Filename is: ', s^);
   waitnext;

   /* ************************* Save file query test ************************ */

   page;
   writeln('Save file query test');
   writeln;
   writeln('There should be an save file query dialog');
   writeln('Both the dialog && this window should be fully reactive');
   writeln('The dialog should have "myfile.txt" as the default filename');
   copy(s, 'myfile.txt');
   querysave(s);
   writeln;
   writeln('Dialog should have completed now');
   writeln('Filename is: ', s^);
   waitnext;

   /* ************************* Find query test ************************ */

   page;
   writeln('Find query test');
   writeln;
   writeln('There should be a find query dialog');
   writeln('Both the dialog && this window should be fully reactive');
   writeln('The dialog should have "mystuff" as the default search char*');
   copy(s, 'mystuff');
   optf = [];
   queryfind(s, optf);
   writeln;
   writeln('Dialog should have completed now');
   writeln('Search char* is: ''', s^, '''');
   if qfncase in optf then writeln('Case sensitive is on')
   else writeln('Case sensitive is off');
   if qfnup in optf then writeln('Search up')
   else writeln('Search down');
   if qfnre in optf then writeln('Use regular expression')
   else writeln('Use literal expression');
   waitnext;

   /* ************************* Find/replace query test ************************ */

   page;
   writeln('Find/replace query test');
   writeln;
   writeln('There should be a find/replace query dialog');
   writeln('Both the dialog && this window should be fully reactive');
   writeln('The dialog should have "bark" as the default search char*');
   writeln('and should have "sniff" as the default replacement char*');
   copy(ss, 'bark');
   copy(rs, 'sniff');
   optfr = [];
   queryfindrep(ss, rs, optfr);
   writeln;
   writeln('Dialog should have completed now');
   writeln('Search char* is: ''', ss^, '''');
   writeln('Replace char* is: ''', rs^, '''');
   if qfrcase in optfr then writeln('Case sensitive is on')
   else writeln('Case sensitive is off');
   if qfrup in optfr then writeln('Search/replace up')
   else writeln('Search/replace down');
   if qfrre in optfr then writeln('Regular expressions are on')
   else writeln('Regular expressions are off');
   if qfrfind in optfr then writeln('Mode is find')
   else writeln('Mode is find/replace');
   if qfrallfil in optfr then writeln('Mode is find/replace all in file')
   else writeln('Mode is find/replace first in file');
   if qfralllin in optfr then writeln('Mode is find/replace all on line(s)')
   else writeln('Mode is find/replace first on line(s)');
   waitnext;

   /* ************************* Font query test ************************ */

   page;
   writeln('Font query test');
   writeln;
   writeln('There should be a font query dialog');
   writeln('Both the dialog && this window should be fully reactive');
   fc = font_book;
   fs = chrsizy;
   fr = 0; /* set foreground to black */
   fg = 0;
   fb = 0;
   br = INT_MAX; /* set background to white */
   bg = INT_MAX;
   bb = INT_MAX;
   fe = [];
   queryfont(output, fc, fs, fr, fg, fb, br, bg, bb, fe);
   writeln;
   writeln('Dialog should have completed now');
   writeln('Font code: ', fc:1);
   writeln('Font size: ', fs:1);
   writeln('Foreground color: Red: ', fr:1, ' Green: ', fg:1, ' Blue: ', fb:1);
   writeln('Background color: Red: ', br:1, ' Green: ', bg:1, ' Blue: ', bb:1);
   if qfteblink in fe then writeln('Blink');
   if qftereverse in fe then writeln('Reverse');
   if qfteunderline in fe then writeln('Underline');
   if qftesuperscript in fe then writeln('Superscript');
   if qftesubscript in fe then writeln('Subscript');
   if qfteitalic in fe then writeln('Italic');
   if qftebold in fe then writeln('Bold');
   if qftestrikeout in fe then writeln('Strikeout');
   if qftestandout in fe then writeln('Standout');
   if qftecondensed in fe then writeln('Condensed');
   if qfteext}ed in fe then writeln('Ext}ed');
   if qftexlight in fe then writeln('Xlight');
   if qftelight in fe then writeln('Light');
   if qftexbold in fe then writeln('Xbold');
   if qftehollow in fe then writeln('Hollow');
   if qfteraised in fe then writeln('Raised');
   waitnext;

   99:;

   page;
   writeln('Test complete');

}.
