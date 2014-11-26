{******************************************************************************
*                                                                             *
*                           SCREEN TEST PROGRAM                               *
*                                                                             *
*                    Copyright (C) 1997 Scott A. Moore                        *
*                                                                             *
* This program performs a reasonably complete test of common features in the  *
* terminal level standard.                                                    *
*                                                                             *
* Tests performed:                                                            *
*                                                                             *
* 1. Row id - number each row with a digit in turn. This test uncovers        *
* positioning errors.                                                         *
* 2. Collumn id - Same for collums.                                           *
* 3. Fill test - fills the screen with the printable ascii characters, and    *
* "elided" control characters. Tests ability to print standard ASCII set.     *
* 4. Sidewinder - Fills the screen starting from the edges in. Tests          *
* positioning.                                                                *
* 5. Bounce - A ball bounces off the walls for a while. Tests positioning.    *
* 6. Scroll - A pattern that is recognizable if shifted is written, then the  *
* display successively scrolled until blank, in each of four directions.      *
* Tests the scrolling ability.                                                *
*                                                                             *
* Notes:                                                                      *
*                                                                             *
* Should have speed tests adjust their length according to actual process     *
* time to prevent tests from taking too long on slow cpu/display.             *
*                                                                             *
* Benchmark results:                                                          *
*                                                                             *
* Windows console library (conlib):                                           *
*                                                                             *
* Character write speed: 0.000031 Sec. Per character.                         *
* Scrolling speed:       0.00144 Sec. Per scroll.                             *
* Buffer switch speed:   0.00143 Sec. per switch.                             *
*                                                                             *
* Windows graphical library (gralib):                                         *
*                                                                             *
* Character write speed: 0.0000075 Sec. Per character.                        *
* Scrolling speed:       0.000192 Sec. Per scroll.                            *
* Buffer switch speed:   0.000126 Sec. per switch.                            *
*                                                                             *
******************************************************************************}

program scntst(input, output);

uses trmlib,
     extlib;

label 99;

var x, y, lx, ly, tx, ty: integer;
    dx, dy: integer;
    c: char;
    top, bottom, lside, rside: integer; { borders }
    direction: (dup, ddown, dleft, dright); { writting direction }
    count, t1, t2: integer;
    delay: integer;
    minlen: integer; { minimum direction, x or y }
    er: evtrec; { event record }
    i, b: integer;
    tc: integer;
    clk: integer;
    cnt: integer;
    tf: text; { test file }

{ draw box }

procedure box(sx, sy, ex, ey: integer; c: char);

var x, y: integer;

begin

   { top }
   cursor(output, sx, sy);
   for x := sx to ex do write(c);
   { bottom }
   cursor(output, sx, ey);
   for x := sx to ex do write(c);
   { left }
   for y := sy to ey do begin cursor(output, sx, y); write(c) end;
   { right }
   for y := sy to ey do begin cursor(output, ex, y); write(c) end

end;

{ wait time in 100 microseconds }

procedure wait(t: integer);

begin

   timer(output, 1, t, false);
   repeat event(input, er) until (er.etype = ettim) or (er.etype = etterm);
   if er.etype = etterm then goto 99

end;

{ wait return to be pressed, or handle terminate }

procedure waitnext;

begin

   repeat event(input, er) until (er.etype = etenter) or (er.etype = etterm);
   if er.etype = etterm then goto 99

end;
   
procedure timetest;

var i, t, et, total, max, min: integer;
    er: evtrec;

begin

   writeln('Timer test, measuring minimum timer resolution, 100 samples');
   writeln;
   max := 0;
   min := maxint;
   for i := 1 to 100 do begin

      t := clock;
      timer(output, 1, 1, false);
      repeat write('*'); event(input, er) until er.etype = ettim;
      et := elapsed(t);
      total := total+elapsed(t);
      if et > max then max := et;
      if et < min then min := et

   end;
   writeln;
   writeln;
   writeln('Average time was: ', total div 100:1, '00 Microseconds');
   writeln('Minimum time was: ', min:1, '00 Microseconds');
   writeln('Maximum time was: ', max:1, '00 Microseconds');
   write('This timer supports frame rates up to ', 10000 div (total div 100):1);
   writeln(' frames per second');
   t := clock;
   timer(output, 1, 10000, false);
   repeat event(input, er) until er.etype = ettim;
   writeln('1 second time, was: ', elapsed(t):1, '00 Microseconds');
   writeln;
   writeln('30 seconds of 1 second ticks:');
   writeln;
   for i := 1 to 30 do begin

      timer(output, 1, 10000, false);
      repeat event(input, er) until (er.etype = ettim) or (er.etype = etterm);
      if er.etype = etterm then goto 99;
      write('.')

   end

end;

{ plot joystick on screen }

procedure plotjoy(line, joy: integer);

var i, x: integer;
    r:    real;

begin

   cursor(output, 1, line);
   for i := 1 to maxx(output) do write(' '); { clear line }
   if joy < 0 then begin { plot left }

      r := abs(joy);
      x := (maxx(output) div 2)-round(r*(maxx(output) div 2)/maxint);
      cursor(output, x, line);
      while x <= maxx(output) div 2 do begin

         write('*');
         x := x+1

      end
   
   end else begin { plot right }

      r := joy;
      x := round(r*(maxx(output) div 2)/maxint+(maxx(output) div 2));
      i := maxx(output) div 2;
      cursor(output, i, line);
      while i <= x do begin

         write('*');
         i := i+1

      end

   end

end;

{ print centered string }

procedure prtcen(y: integer; view s: string);

begin

   cursor(output, (maxx(output) div 2)-(max(s) div 2), y);
   write(s)

end;

{ print center banner string }

procedure prtban(view s: string);

var i: integer;

begin

   cursor(output, (maxx(output) div 2)-(max(s) div 2)-1, maxy(output) div 2-1);
   for i := 1 to max(s)+2 do write(' ');
   cursor(output, (maxx(output) div 2)-(max(s) div 2)-1, maxy(output) div 2);
   write(' ');
   prtcen(maxy(output) div 2, s);
   write(' ');
   cursor(output, (maxx(output) div 2)-(max(s) div 2)-1, maxy(output) div 2+1);
   for i := 1 to max(s)+2 do write(' ');

end;

begin

   select(output, 2, 2); { move off the display buffer }
   { set black on white text }
   fcolor(output, black);
   bcolor(output, white);
   page;
   curvis(output, false);
   prtban('Terminal mode screen test vs. 1.0');
   prtcen(maxy(output), 'Press return to continue');
   waitnext;
   page; { clear screen }
   writeln('Screen size: x -> ', maxx(output):1, ' y -> ', maxy(output):1);
   writeln;
   writeln('Number of joysticks: ', joystick(output):1);
   for i := 1 to joystick(output) do begin

      writeln;
      writeln('Number of axes on joystick: ', i:1, ' is: ', 
              joyaxis(output, i):1);
      writeln('Number of buttons on joystick: ', i:1, ' is: ', 
              joybutton(output, i):1)

   end;
   writeln;
   writeln('Number of mice: ', mouse(output):1);
   for i := 1 to mouse(output) do begin

      writeln;
      writeln('Number of buttons on mouse: ', i:1, ' is: ', 
              mousebutton(output, i):1)

   end;
   prtcen(maxy(output), 'Press return to continue');
   waitnext;
   page;
   timetest;
   prtcen(maxy(output), 'Press return to continue');
   waitnext;
   page;
   curvis(output, true);
   write('Cursor should be [on ], press return ->');
   waitnext;
   curvis(output, false);
   write('\crCursor should be [off], press return ->');
   waitnext;
   curvis(output, true);
   write('\crCursor should be [on ], press return ->');
   waitnext;
   curvis(output, false);
   writeln;
   writeln;
   prtcen(maxy(output), 
          'Press return to start test (and to pass each pattern)');
   waitnext;

   { ************************* Test last line problem ************************ }

   page;
   curvis(output, false); { remove cursor }
   auto(output, false); { turn off auto scroll }
   prtcen(1, 'Last line blank out test');
   cursor(output, 1, 3);
   writeln('If this terminal is not capable of showing the last character on');
   writeln('the last line, the "*" character pointed to by the arrow below');
   writeln('will not appear (probally blank). This should be noted for each');
   writeln('of the following test patterns.');
   cursor(output, 1, maxy(output));
   for i := 1 to maxx(output)-2 do write('-');
   write('>*');
   waitnext;
   
   { ************************** Cursor movements test ************************ }

   { First, do it with automatic scrolling on. The pattern will rely on scroll
     up, down, left wrap and right wrap working correctly. }

   page;
   auto(output, true); { set auto on }
   curvis(output, false); { remove cursor }
   { top of left lower }
   cursor(output, 1, maxy(output));
   write('\\/');
   { top of right lower, bottom of left lower, and move it all up }
   cursor(output, maxx(output)-1, maxy(output));
   write('\\//\\');
   { finish right lower }
   up(output);
   left(output);
   left(output);
   left(output);
   left(output);
   down(output);
   down(output);
   write('/\\');
   { now move it back down }
   home(output);
   left(output);
   { upper left hand cross }
   cursor(output, 1, 1);
   write('\\/');
   cursor(output, maxx(output), 1);
   right(output);
   write('/\\');
   { upper right hand cross }
   cursor(output, maxx(output)-1, 2);
   write('/\\');
   cursor(output, 1, 2);
   left(output);
   left(output);
   write('\\/');
   { test delete works }
   prtcen(1, 'BARK!');
   del(output);
   del(output);
   del(output);
   del(output);
   del(output);
   prtcen(maxy(output) div 2-1, 'Cursor movements test, automatic scroll ON');
   prtcen(maxy(output) div 2+1, 'Should be a double line X in each corner');
   waitnext;

   { Now do it with automatic scrolling off. The pattern will rely on the
     ability of the cursor to go into "negative" space. }

   page;
   auto(output, false); { disable automatic screen scroll/wrap }
   { upper left }
   home(output);
   write('\\/');
   up(output);
   left(output);
   left(output);
   left(output);
   left(output);
   down(output);
   down(output);
   right(output);
   right(output);
   write('/\\');
   { upper right }
   cursor(output, maxx(output)-1, 1);
   write('\\/');
   down(output);
   del(output);
   del(output);
   write('/\\');
   { lower left }
   cursor(output, 1, maxy(output));
   write('/\\');
   down(output);
   left(output);
   left(output);
   left(output);
   up(output);
   up(output);
   right(output);
   write('\\/');
   { lower right }
   cursor(output, maxx(output), maxy(output)-1);
   write('/');
   left(output);
   left(output);
   write('\\');
   down(output);
   del(output);
   write('/\\');
   prtcen(maxy(output) div 2-1, 'Cursor movements test, automatic scroll OFF');
   prtcen(maxy(output) div 2+1, 'Should be a double line X in each corner');
   waitnext;

   { **************************** Scroll cursor test ************************* }

   page;
   curvis(output, true);
   prtcen(maxy(output) div 2, 'Scroll cursor test, cursor should be here ->');
   up(output);
   scroll(output, 0, 1);
   waitnext;
   curvis(output, false);

   { ******************************* Row ID test ***************************** }

   page;
   { perform row id test }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do write(c); { output characters }
      if c <> '9' then c := succ(c) { next character }
      else c := '0' { start over }

   end;
   prtban('Row ID test, all rows should be numbered');
   waitnext;

   { *************************** Collumn ID test ***************************** }

   page;
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do begin

         write(c); { output characters }
         if c <> '9' then c := succ(c) { next character }
         else c := '0' { start over }

      end

   end;
   prtban('Collumn ID test, all collumns should be numbered');
   waitnext;

   { ****************************** Fill test ******************************** }

   page;
   c := chr(0); { initalize character value }
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do begin

         if (c >= ' ') and (c <> chr($7f)) then write(c)
         else write('\\');
         if c <> chr($7f) then c := succ(c) { next character }
         else c := chr(0) { start over }

      end

   end;
   prtban('Fill test, all printable characters should appear');
   waitnext;

   { **************************** Sidewinder test **************************** }

   page;
   { perform sidewinder }
   x := 1; { set origin }
   y := 1;
   top := 1; { set borders }
   bottom := maxy(output);
   lside := 2;
   rside := maxx(output);
   direction := ddown; { start down }
   t1 := maxx(output);
   t2 := maxy(output);
   tc := 0;
   for count := 1 to t1 * t2 do begin { for all screen characters }

      cursor(output, x, y); { place character }
      write('*');
      tc := tc+1;
      if tc >= 10 then begin

         wait(50); { 50 milliseconds }
         tc := 0

      end;
      case direction of

         ddown:  begin

                   y := y + 1; { next }
                   if y = bottom then begin { change }

                      direction := dright;
                      bottom := bottom - 1

                   end

                end;

         dright: begin

                   x := x + 1; { next }
                   if x = rside then begin

                      direction := dup;
                      rside := rside - 1

                   end

                end;

         dup:    begin

                   y := y - 1;
                   if y = top then begin

                      direction := dleft;
                      top := top + 1

                   end

                end;

         dleft:  begin

                   x := x - 1;
                   if x = lside then begin

                      direction := ddown;
                      lside := lside + 1

                   end

                end

      end

   end;
   prtcen(maxy(output)-1, '                 ');
   prtcen(maxy(output), ' Sidewinder test ');
   waitnext;

   { *************************** Bouncing ball test ************************** }

   page;
   x := 10; { set origin }
   y := 20;
   lx := 10; { set last }
   ly := 20;
   dx := -1; { set initial directions }
   dy := -1;
   for count := 1 to 1000 do begin

      cursor(output, x, y); { place character }
      write('*');
      cursor(output, lx, ly); { place character }
      write(' ');
      lx := x; { set last }
      ly := y;
      x := x + dx; { find next x }
      y := y + dy; { find next y }
      tx := x;
      ty := y;
      if (x = 1) or (tx = maxx(output)) then dx := -dx; { find new dir x }
      if (y = 1) or (ty = maxy(output)) then dy := -dy; { find new dir y }
      wait(100) { slow this down }

   end;
   prtcen(maxy(output)-1, '                    ');
   prtcen(maxy(output), ' Bouncing ball test ');
   waitnext;

   { *************************** Attributes test ************************** }

   page;
   if maxy(output) < 20 then write('Not enough lines for attributes test')
   else begin

      blink(output, true);
      writeln('Blinking text');
      blink(output, false);
      reverse(output, true);
      writeln('Reversed text');
      reverse(output, false);
      underline(output, true);
      writeln('Underlined text');
      underline(output, false);
      write('Superscript ');
      superscript(output, true);
      writeln('text');
      superscript(output, false);
      write('Subscript ');
      subscript(output, true);
      writeln('text');
      subscript(output, false);
      italic(output, true);
      writeln('Italic text');
      italic(output, false);
      bold(output, true);
      writeln('Bold text');
      bold(output, false);
      standout(output, true);
      writeln('Standout text');
      standout(output, false);
      fcolor(output, red);
      writeln('Red text');
      fcolor(output, green);
      writeln('Green text');
      fcolor(output, blue);
      writeln('Blue text');
      fcolor(output, cyan);
      writeln('Cyan text');
      fcolor(output, yellow);
      writeln('Yellow text');
      fcolor(output, magenta);
      writeln('Magenta text');
      fcolor(output, black);
      bcolor(output, red);
      writeln('Red background text');
      bcolor(output, green);
      writeln('Green background text');
      bcolor(output, blue);
      writeln('Blue background text');
      bcolor(output, cyan);
      writeln('Cyan background text');
      bcolor(output, yellow);
      writeln('Yellow background text');
      bcolor(output, magenta);
      writeln('Magenta background text');
      bcolor(output, white);
      prtcen(maxy(output), 'Attributes test')

   end;
   waitnext;

   { ***************************** Scrolling test **************************** }

   page;
   { fill screen with row order data }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do write(c); { output characters }
      if c <> '9' then c := succ(c) { next character }
      else c := '0' { start over }

   end;
   for y := 1 to maxy(output) do begin wait(200); scroll(output, 0, 1) end;
   prtcen(maxy(output), 'Scroll up');
   waitnext;
   page;
   { fill screen with row order data }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do write(c); { output characters }
      if c <> '9' then c := succ(c) { next character }
      else c := '0' { start over }

   end;
   for y := 1 to maxy(output) do begin wait(200); scroll(output, 0, -1) end;
   prtcen(maxy(output), 'Scroll down');
   waitnext;
   page;
   { fill screen with collumn order data }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do begin

         write(c); { output characters }
         if c <> '9' then c := succ(c) { next character }
         else c := '0' { start over }

      end

   end;
   for x := 1 to maxx(output) do begin wait(200); scroll(output, 1, 0) end;
   prtcen(maxy(output), 'Scroll left');
   waitnext;
   page;
   { fill screen with collumn order data }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do begin

         write(c); { output characters }
         if c <> '9' then c := succ(c) { next character }
         else c := '0' { start over }

      end

   end;
   for x := 1 to maxx(output) do begin wait(200); scroll(output, -1, 0) end;
   { find minimum direction, x or y }
   if x < y then minlen := x else minlen := y;
   prtcen(maxy(output), 'Scroll right');
   waitnext;
   page;
   { fill screen with uni data }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do begin

         write(c); { output characters }
         if c <> '9' then c := succ(c) { next character }
         else c := '0' { start over }

      end

   end;
   for i := 1 to minlen do begin wait(200); scroll(output, 1, 1) end;
   prtcen(maxy(output), 'Scroll up/left');
   waitnext;
   page;
   { fill screen with uni data }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do begin

         write(c); { output characters }
         if c <> '9' then c := succ(c) { next character }
         else c := '0' { start over }

      end

   end;
   for i := 1 to minlen do begin wait(200); scroll(output, 1, -1) end;
   prtcen(maxy(output), 'Scroll down/left');
   waitnext;
   page;
   { fill screen with uni data }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do begin

         write(c); { output characters }
         if c <> '9' then c := succ(c) { next character }
         else c := '0' { start over }

      end

   end;
   for i := 1 to minlen do begin wait(200); scroll(output, -1, 1) end;
   prtcen(maxy(output), 'Scroll up/right');
   waitnext;
   page;
   { fill screen with uni data }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do begin

         write(c); { output characters }
         if c <> '9' then c := succ(c) { next character }
         else c := '0' { start over }

      end

   end;
   for i := 1 to minlen do begin wait(200); scroll(output, -1, -1) end;
   prtcen(maxy(output), 'Scroll down/right');
   waitnext;

   { ******************************** Tab test ******************************* }

   page;
   for y := 1 to maxy(output) do begin

      for i := 1 to y-1 do write('\ht');
      writeln('>Tab ', y-1:3);

   end;
   prtcen(maxy(output), 'Tabbing test');
   waitnext;

   { ************************** Buffer switching test ************************ }

   page;
   for b := 2 to 10 do begin { prepare buffers }

      select(output, b, 2); { select buffer }
      { write a shinking box pattern }
      box(b-1, b-1, maxx(output)-(b-2), maxy(output)-(b-2), '*');
      prtcen(maxy(output), 'Buffer switching test')

   end;
   for i := 1 to 30 do { flip buffers }
      for b := 2 to 10 do begin wait(300); select(output, 2, b) end;
   select(output, 2, 2); { restore buffer select }

   { **************************** Writethrough test ************************** }

   page;
   prtcen(maxy(output), 'File writethrough test');
   home(output);
   rewrite(tf);
   writeln(tf, 'This is a test file');
   reset(tf);
   while not eoln(tf) do begin

      read(tf, c);
      write(c)

   end;
   readln(tf);
   writeln;
   writeln;
   writeln('s/b');
   writeln;
   writeln('This is a test file');
   waitnext;

   { ****************************** Joystick test **************************** }

   if joystick(output) > 0 then begin { joystick test }

      page;
      prtcen(1, 'Move the joystick(s) X, Y and Z, and hit buttons');
      prtcen(maxy(output), 'Joystick test test');
      repeat { gather joystick events }
    
         { we do up to 4 joysticks }
         event(input, er);
         if er.etype = etjoymov then begin { joystick movement }

            if er.mjoyn = 1 then begin { joystick 1 }

               cursor(output, 1, 3);
               write('joystick: ', er.mjoyn:1, ' x: ', er.joypx, ' y: ',
                     er.joypy, ' z: ', er.joypz);
               plotjoy(4, er.joypx);
               plotjoy(5, er.joypy);
               plotjoy(6, er.joypz);

            end else if er.mjoyn = 2 then begin { joystick 2 }

               cursor(output, 1, 7);
               write('joystick: ', er.mjoyn:1, ' x: ', er.joypx, ' y: ',
                     er.joypy, ' z: ', er.joypz);
               plotjoy(8, er.joypx);
               plotjoy(9, er.joypy);
               plotjoy(10, er.joypz);

            end else if er.mjoyn = 3 then begin { joystick 3 }

               cursor(output, 1, 11);
               write('joystick: ', er.mjoyn:1, ' x: ', er.joypx, ' y: ',
                     er.joypy, ' z: ', er.joypz);
               plotjoy(11, er.joypx);
               plotjoy(12, er.joypy);
               plotjoy(13, er.joypz);

            end else if er.mjoyn = 4 then begin { joystick 4 }

               cursor(output, 1, 14);
               write('joystick: ', er.mjoyn:1, ' x: ', er.joypx, ' y: ',
                     er.joypy, ' z: ', er.joypz);
               plotjoy(15, er.joypx);
               plotjoy(16, er.joypy);
               plotjoy(17, er.joypz);

            end

         end else if er.etype = etjoyba then begin { joystick button assert }

            if er.ajoyn = 1 then begin { joystick 1 }

               cursor(output, 1, 18);
               write('joystick: ', er.ajoyn:1, ' button assert:   ', er.ajoybn:1);

            end else if er.ajoyn = 2 then begin { joystick 2 }

               cursor(output, 1, 19);
               write('joystick: ', er.ajoyn:1, ' button assert:   ', er.ajoybn:1);

            end else if er.ajoyn = 3 then begin { joystick 3 }

               cursor(output, 1, 20);
               write('joystick: ', er.ajoyn:1, ' button assert:   ', er.ajoybn:1);

            end else if er.ajoyn = 4 then begin { joystick 4 }

               cursor(output, 1, 21);
               write('joystick: ', er.ajoyn:1, ' button assert:   ', er.ajoybn:1);

            end

         end else if er.etype = etjoybd then begin { joystick button deassert }

            if er.djoyn = 1 then begin { joystick 1 }

               cursor(output, 1, 18);
               write('joystick: ', er.djoyn:1, ' button deassert: ', er.djoybn:1);

            end else if er.djoyn = 2 then begin { joystick 2 }

               cursor(output, 1, 19);
               write('joystick: ', er.djoyn:1, ' button deassert: ', er.djoybn:1);

            end else if er.djoyn = 3 then begin { joystick 3 }

               cursor(output, 1, 20);
               write('joystick: ', er.djoyn:1, ' button deassert: ', er.djoybn:1);

            end else if er.djoyn = 4 then begin { joystick 4 }

               cursor(output, 1, 21);
               write('joystick: ', er.djoyn:1, ' button deassert: ', er.djoybn:1);

            end

         end

      until (er.etype = etenter) or (er.etype = etterm)

   end;

   { **************************** Mouse test ********************************* }

   if mouse(input) > 0 then begin { mouse test }

      page;
      prtcen(1, 'Move the mouse, and hit buttons');
      prtcen(maxy(output), 'Mouse test');
      repeat { gather mouse events }
    
         { we only one mouse, all mice equate to that (multiple controls) }
         event(input, er);
         if er.etype = etmoumov then begin

            cursor(output, x, y);
            writeln('          ');
            cursor(output, er.moupx, er.moupy);
            x := curx(output);
            y := cury(output);
            writeln('<- Mouse ', er.mmoun:1)

         end;
         { blank out button status line }
         cursor(output, 1, maxy(output)-2);
         for i := 1 to maxx(output) do write(' ');
         if er.etype = etmouba then begin { mouse button assert }

            cursor(output, 1, maxy(output)-2);
            writeln('Mouse button assert, mouse: ', er.amoun:1, 
                    ' button: ', er.amoubn:1)

         end;
         if er.etype = etmoubd then begin { mouse button assert }

            cursor(output, 1, maxy(output)-2);
            writeln('Mouse button deassert, mouse: ', er.dmoun:1, 
                    ' button: ', er.dmoubn:1)

         end;

      until (er.etype = etenter) or (er.etype = etterm);
      if er.etype = etterm then goto 99

   end;

   { ********************** Character write speed test *********************** }

   page;
   clk := clock; { get reference time }
   c := chr(0); { initalize character value }
   cnt := 0; { clear character count }
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do begin

         if (c >= ' ') and (c <> chr($7f)) then write(c)
         else write('\\');
         if c <> chr($7f) then c := succ(c) { next character }
         else c := chr(0); { start over }
         cnt := cnt+1 { count characters }

      end

   end;
   clk := elapsed(clk); { find elapsed time }
   page;
   writeln('Character write speed: ', clk/cnt*0.0001, 
           ' average seconds per character');
   waitnext;

   { ************************** Scrolling speed test ************************* }

   page;
   { fill screen so we aren't moving blanks (could be optimized) }
   c := '1';
   for y := 1 to maxy(output) do begin

      cursor(output, 1, y); { index start of line }
      for x := 1 to maxx(output) do write(c); { output characters }
      if c <> '9' then c := succ(c) { next character }
      else c := '0' { start over }

   end;
   prtban('Scrolling speed test');
   clk := clock; { get reference time }
   cnt := 0; { clear count }
   for i := 1 to 1000 do begin { scroll various directions }

      scroll(output, 0, -1); { up }
      scroll(output, -1, 0); { left }
      scroll(output, 0, +1); { down }
      scroll(output, 0, +1); { down }
      scroll(output, +1, 0); { right }
      scroll(output, +1, 0); { right }
      scroll(output, 0, -1); { up }
      scroll(output, 0, -1); { up }
      scroll(output, -1, 0); { left }
      scroll(output, 0, +1); { down }
      scroll(output, -1, -1); { up/left }
      scroll(output, +1, +1); { down/right }
      scroll(output, +1, +1); { down/right }
      scroll(output, -1, -1); { up/left }
      scroll(output, +1, -1); { up/right }
      scroll(output, -1, +1); { down/left }
      scroll(output, -1, +1); { down/left }
      scroll(output, +1, -1); { up/right }
      cnt := cnt+19 { count all scrolls }

   end;
   clk := elapsed(clk); { find elapsed time }
   page;
   writeln('Scrolling speed: ', clk/cnt*0.0001, 
           ' average seconds per scroll');
   waitnext;

   { ************************** Buffer flip speed test ************************* }

   page;
   cnt := 0; { clear count }

   for b := 2 to 10 do begin { prepare buffers }

      select(output, b, 2); { select buffer }
      { write a shinking box pattern }
      box(b-1, b-1, maxx(output)-(b-2), maxy(output)-(b-2), '*')

   end;
   clk := clock; { get reference time }
   for i := 1 to 1000 do { flip buffers }
      for b := 2 to 10 do begin 

      select(output, 2, b);
      cnt := cnt+1

   end;
   clk := elapsed(clk); { find elapsed time }
   select(output, 2, 2); { restore buffer select }
   page;
   writeln('Buffer switch speed: ', clk/cnt*0.0001, 
           ' average seconds per switch');
   waitnext;

   99: ; { terminate }

   { test complete }
   select(output, 1, 1); { back to display buffer }
   curvis(output, true); { restore cursor }
   auto(output, true); { enable automatic screen wrap }
   writeln;
   writeln('Test complete')

end.
