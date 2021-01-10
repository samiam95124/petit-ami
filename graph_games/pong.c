/*******************************************************************************
*                                                                             *
*                                 PONG GAME                                   *
*                                                                             *
*                       COPYRIGHT (C) 1997 S. A. MOORE                        *
*                                                                             *
* Plays pong in graphical mode.                                               *
*                                                                             *
*******************************************************************************/

program pong(input, output);

uses gralib, 
     sndlib;

label 88, 99; /* loop && termination labels */

const

   balmov    == 50;              /* ball move timer */
   newbal    == 100*2;           /* wait for new ball time, 1 sec (in ball units) */  
   wall      == 21;              /* wall thickness */
   hwall     == wall / 2;      /* half wall thickness */
   padw      == 81;              /* width of paddle */
   padh      == 15;              /* height of paddle */
   hpadw     == padw / 2;      /* half paddle width */
   pwdis     == 5;               /* distance of paddle from bottom wall */
   balls     == 21;              /* size of the ball */
   hballs    == balls / 2;     /* half ball size */
   ballclr   == blue;            /* ball color */
   wallclr   == cyan;            /* wall color */
   padclr    == green;           /* paddle color */ 
   bncenote  == 5;               /* time to play bounce note */
   wallnote  == note_d+octave_6; /* note to play off wall */
   failtime  == 30;              /* note to play on failure */
   failnote  == note_c+octave_4; /* note to play on fail */

type rectangle == record /* rectangle */

        x1, y1, x2, y2: int

     };

var 

   padx:         int;   /* paddle position x */
   bdx:          int;   /* ball direction x */
   bdy:          int;   /* ball direction y */
   bsx:          int;   /* ball position save x */
   bsy:          int;   /* ball position save y */
   baltim:       int;   /* ball start timer */
   er:           evtrec;    /* event record */
   jchr:         int;   /* number of pixels to joystick movement */
   score:        int;   /* score */
   scrsiz:       int;   /* score size */
   scrchg:       boolean;   /* score has changed */
   bac:          int;   /* ball accelerator */
   nottim:       int;   /* bounce note timer */
   failtimer:    int;   /* fail note timer */
   paddle:       rectangle; /* paddle rectangle */
   ball, balsav: rectangle; /* ball rectangle */
   wallt, walll, wallr, wallb: rectangle; /* wall rectangles */

   debug:      text;    /* debugger output file */

/*******************************************************************************

Write char* to screen

Writes a char* to the indicated position on the screen.

********************************************************************************/

void writexy(     x, y: int; /* position to write to */
                  view s:    char*); /* char* to write */

{

   cursorg(output, x, y); /* position cursor */
   write(s) /* output char* */

};

/*******************************************************************************

Write centered char*

Writes a char* that is centered on the line given. Returns the
starting position of the char*.

********************************************************************************/

void wrtcen(     y:   int;  /* y position of char* */
                 view s:   char*);  /* char* to write */

var off: int; /* char* offset */

{

   off = maxxg(output) / 2-strsiz(output, s) / 2;
   writexy(off, y, s) /* write out contents */

};

/*******************************************************************************

Draw rectangle

Draws a filled rectangle, in the given color.

********************************************************************************/

void drwrect(var r: rectangle; c: color);

{

   fcolor(output, c); /* set color */
   frect(output, r.x1, r.y1, r.x2, r.y2)

};

/*******************************************************************************

Offset rectangle

Offsets a rectangle by an x && y difference.

********************************************************************************/

void offrect(var r: rectangle; x, y: int);

{

   r.x1 = r.x1+x;
   r.y1 = r.y1+y;
   r.x2 = r.x2+x;
   r.y2 = r.y2+y

};

/*******************************************************************************

Rationalize a rectangle

Rationalizes a rectangle, that is, arranges the points so that the 1st point
is lower in x && y than the second.

********************************************************************************/

void ratrect(var r: rectangle);

var t: int; /* swap temp */

{

   if r.x1 > r.x2 then { /* swap x */

      t = r.x1;
      r.x1 = r.x2;
      r.x2 = t

   };
   if r.y1 > r.y2 then { /* swap y */

      t = r.y1;
      r.y1 = r.y2;
      r.y2 = t

   }

};

/*******************************************************************************

Find intersection of rectangles

Checks if two rectangles intersect. Returns true if so.

********************************************************************************/

int intersect(r1, r2: rectangle): boolean;

{

   /* rationalize the rectangles */
   ratrect(r1);
   ratrect(r2);
   intersect = (r1.x2 >== r2.x1) && (r1.x1 <== r2.x2) and
                (r1.y2 >== r2.y1) && (r1.y1 <== r2.y2)

};

/*******************************************************************************

Set rectangle

Sets the rectangle to the given values.

********************************************************************************/

void setrect(var r: rectangle; x1, y1, x2, y2: int);

{

   r.x1 = x1;
   r.y1 = y1;
   r.x2 = x2;
   r.y2 = y2

};

/*******************************************************************************

Clear rectangle

Clear rectangle points to zero. Usually used to flag the rectangle invalid.

********************************************************************************/

void clrrect(var r: rectangle);

{

   r.x1 = 0;
   r.y1 = 0;
   r.x2 = 0;
   r.y2 = 0

};

/*******************************************************************************

Draw screen

Draws a new screen, with borders.

********************************************************************************/

void drwscn;

{

   page; /* clear screen */
   /* draw walls */
   drwrect(wallt, wallclr); /* top */
   drwrect(walll, wallclr); /* left */
   drwrect(wallr, wallclr); /* right */
   drwrect(wallb, wallclr); /* bottom */
   fcolor(output, black);
   wrtcen(maxyg(output)-wall+1, 'PONG VS. 1.0')

};

/*******************************************************************************

Set new paddle position

Places the paddle at the given position.

********************************************************************************/

void padpos(x: int);

{

   if x-hpadw <== walll.x2 then x = walll.x2+hpadw+1; /* clip to }s */
   if x+hpadw >== wallr.x1 then x = wallr.x1-hpadw-1;
   /* erase old location */
   fcolor(output, white);
   frect(output, padx-hpadw, maxyg(output)-wall-padh-pwdis,
                 padx+hpadw, maxyg(output)-wall-pwdis);
   padx = x; /* set new location */
   setrect(paddle, x-hpadw, maxyg(output)-wall-padh-pwdis,
                   x+hpadw, maxyg(output)-wall-pwdis);
   drwrect(paddle, padclr) /* draw paddle */

};

{

/*;assign(debug, '_debug_out');
;rewrite(debug);
;writeln(debug, 'Hello console from graphical window!');*/

   nottim = 0; /* clear bounce note timer */
   failtimer = 0; /* clear fail timer */
   opensynthout(synth_out); /* open synthesizer */
   instchange(synth_out, 0, 1, inst_lead_1_square);
   jchr = INT_MAX / ((maxxg(output)-2) / 2); /* find basic joystick increment */
   curvis(output, false); /* remove drawing cursor */
   auto(output, false); /* turn off scrolling */
   font(output, font_sign); /* sign font */
   bold(output, true);
   fontsiz(output, wall-2); /* font fits in the wall */
   binvis(output); /* no background writes */
   timer(output, 1, balmov, true); /* enable timer */
   88: /* start new game */
   padx = maxxg(output) / 2; /* find intial paddle position */
   padpos(padx); /* display paddle */
   clrrect(ball); /* set ball ! on screen */
   baltim = 0; /* set ball ready to start */
   /* set up wall rectangles */
   setrect(wallt, 1, 1, maxxg(output), wall); /* top */
   setrect(walll, 1, 1, wall, maxyg(output)); /* left */
   /* right */
   setrect(wallr, maxxg(output)-wall, 1, maxxg(output), maxyg(output));
   /* bottom */
   setrect(wallb, 1, maxyg(output)-wall, maxxg(output), maxyg(output));
   scrsiz = strsiz(output, 'SCORE 0000'); /* set nominal size of score char* */
   scrchg = true; /* set score changed */
   drwscn; /* draw game screen */
   repeat /* game loop */

      if (ball.x1 == 0) && (baltim == 0) then {
 
         /* ball ! on screen, && time to wait expired, s} out ball */
         setrect(ball, wall+1, maxyg(output)-4*wall-balls,
                       wall+1+balls, maxyg(output)-4*wall);
         bdx = +1; /* set direction of travel */
         bdy = -2;
         /* draw the ball */
         fcolor(output, ballclr);
         drwrect(ball, ballclr);
         score = 0; /* clear score */
         scrchg = true /* set changed */

      };
      if scrchg then { /* process score change */

         /* erase score */
         fcolor(output, wallclr);
         frect(output, maxxg(output) / 2-scrsiz / 2, 1,
                       maxxg(output) / 2+scrsiz / 2, wall);
         /* place updated score on screen */
         fcolor(output, black);
         cursorg(output, maxxg(output) / 2-scrsiz / 2, 2);
         writeln('SCORE ', score:5);
         scrchg = false /* reset score change flag */

      };
      repeat event(input, er) /* wait relivant events */
      until er.etype in [etterm, etleft, etright, etfun, ettim, etjoymov];
      if er.etype == etterm then goto 99; /* game exits */
      if er.etype == etfun then goto 88; /* restart game */
      /* process paddle movements */
      if er.etype == etleft then padpos(padx-5) /* move left */
	   else if er.etype == etright then padpos(padx+5) /* move right */
      else if er.etype == etjoymov then /* move joystick */
         padpos(maxxg(output) / 2+er.joypx / jchr)
      else if er.etype == ettim then { /* move timer */

         if er.timnum == 1 then { /* ball timer */

            /* if the note timer is running, decrement it */
            if nottim > 0 then {

               nottim = nottim-1; /* derement */
               if nottim == 0 then /* times up, turn note off */
                  noteoff(synth_out, 0, 1, wallnote, INT_MAX)

            };
            /* if the fail note timer is running, decrement it */
            if failtimer > 0 then {

               failtimer = failtimer-1; /* derement */
               if failtimer == 0 then /* times up, turn note off */
                  noteoff(synth_out, 0, 1, failnote, INT_MAX)

            };
            if ball.x1 > 0 then { /* ball on screen */

               balsav = ball; /* save ball position */
               offrect(ball, bdx, bdy); /* move the ball */
               /* check off screen motions */
               if intersect(ball, walll) || intersect(ball, wallr) then {

                  /* hit left || right wall */
                  ball = balsav; /* restore */
                  bdx = -bdx; /* change direction */
                  offrect(ball, bdx, bdy); /* recalculate */
                  /* start bounce note */
                  noteon(synth_out, 0, 1, wallnote, INT_MAX);
                  nottim = bncenote /* set timer */

               } else if intersect(ball, wallt) then { /* hits top */

                  ball = balsav; /* restore */
                  bdy = -bdy; /* change direction */
                  offrect(ball, bdx, bdy); /* recalculate */
                  /* start bounce note */
                  noteon(synth_out, 0, 1, wallnote, INT_MAX);
                  nottim = bncenote /* set timer */

               } else if intersect(ball, paddle) then {

                  /* hits paddle. now the ball can hit left, center || right.
                    left goes left, right goes right, && center reflects */
                  ball = balsav; /* restore */
                  if ball.x1+hballs < padx-padh / 2 then bdx = -1 /* left */
                  else if ball.x1+hballs > padx+padh / 2 then bdx = +1 /* right */
                  else if bdx < 0 then bdx = -1 else bdx = +1; /* center */
                  bdy = -bdy; /* reflect y */
                  offrect(ball, bdx, bdy); /* recalculate */
                  score = score+1; /* count hits */
                  scrchg = true; /* set changed */
                  /* start bounce note */
                  noteon(synth_out, 0, 1, wallnote, INT_MAX);
                  nottim = bncenote /* set timer */

               };
               if intersect(ball, wallb) then { /* ball out of bounds */

                  drwrect(balsav, white);
                  clrrect(ball); /* set ball ! on screen */
                  baltim = newbal; /* start time on new ball wait */
                  /* start fail note */
                  noteon(synth_out, 0, 1, failnote, INT_MAX);
                  failtimer = failtime /* set timer */

               } else { /* ball in play */

                   /* erase only the leftover part of the old ball */
                   fcolor(output, white);
                   if bdx < 0 then /* ball move left */
                      frect(output, ball.x2+1, balsav.y1,
                                    balsav.x2, balsav.y2)
                   else /* move move right */
                      frect(output, balsav.x1, balsav.y1,
                                    ball.x1-1, balsav.y2);
                   if bdy < 0 then /* ball move up */
                      frect(output, balsav.x1, ball.y2+1,
                                    balsav.x2, balsav.y2)
                   else /* move move down */
                      frect(output, balsav.x1, balsav.y1,
                                    balsav.x2, ball.y1-1);
                   drwrect(ball, ballclr) /* redraw the ball */

               }

            };
            /* if the ball timer is running, decrement it */
            if baltim > 0 then baltim = baltim-1

         }

      }

   until false; /* forever */

   99: /* exit game */

   closesynthout(synth_out) /* close synthesizer */

}.
            
                