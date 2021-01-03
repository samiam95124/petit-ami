/*******************************************************************************
*                                                                             *
*                     WINDOW MANAGEMENT TEST PROGRAM                          *
*                                                                             *
*                    Copyright (C) 2005 Scott A. Moore                        *
*                                                                             *
* Tests text and graphical windows management calls.                          *
*                                                                             *
*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <services.h>
#include <graph.h>

#define OFF 0
#define ON 1

FILE*      win2;
FILE*      win3;
FILE*      win4;
int        x, x2, y, y2;
int        ox, oy;       /* original size of window */
int        fb;           /* front/back flipper */
pa_evtrec  er;
pa_menuptr mp;           /* menu pointer */
pa_menuptr ml;           /* menu list */
pa_menuptr sm;           /* submenu list */
int        sred;         /* state variables */
int        sgreen;
int        sblue;
int        mincnt;       /* minimize counter */
int        maxcnt;       /* maximize counter */
int        nrmcnt;       /* normalize counter */
int        i;

/* wait return to be pressed, or handle terminate */

static void waitnext(void)

{

    pa_evtrec er; /* event record */

    do { pa_event(stdin, &er); }
    while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* wait return to be pressed, or handle terminate, while printing characters */

void waitnextprint(void)

{

    pa_evtrec er; /* event record */

    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etchar)
            printf("Window: %d char: %c\n", er.winid, er.echar);

    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* print centered string */

static void prtcen(int y, const char* s)

{

   pa_cursor(stdout, (pa_maxx(stdout)/2)-(strlen(s)/2), y);
   printf("%s", s);

}

/* print centered string graphical */

static void prtceng(int y, const char* s)

{

   pa_cursorg(stdout, (pa_maxxg(stdout)/2)-(pa_strsiz(stdout, s)/2), y);
   printf("%s", s);

}

/* wait time in 100 microseconds */

void wait(int t)

{

    pa_evtrec er;

    pa_timer(stdout, 1, t, FALSE);
    do { pa_event(stdin, &er);
    } while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* append a new menu entry to the given list */

void appendmenu(pa_menuptr* list, pa_menuptr m)

{

    pa_menuptr lp;

    /* clear these links for insurance */
    m->next = NULL; /* clear next */
    m->branch = NULL; /* clear branch */
    if (!*list) *list = m; /* list empty, set as first entry */
    else { /* list non-empty */

        /* find last entry in list */
        lp = *list; /* index 1st on list */
        while (lp->next) lp = lp->next;
        lp->next = m; /* append at end */

    }

}

/* create menu entry */

void newmenu(pa_menuptr* mp, int onoff, int oneof, int bar,
             int id, const string face)

{

    *mp = malloc(sizeof(pa_menurec));
    if (!*mp) pa_alert("mantst", "Out of memory");
    (*mp)->onoff = onoff;
    (*mp)->oneof = oneof;
    (*mp)->bar = bar;
    (*mp)->id = id;
    new(*mp->face, max(face));
    (*mp->face^ = face

}

/* draw a character grid */

static void chrgrid(void)

{

    int x, y;

    pa_fcolor(stdout, pa_yellow);
    y = 1;
    while (y < pa_maxyg(stdout)) {

        pa_line(stdout, 1, y, pa_maxxg(stdout), y);
        y = y+pa_chrsizy(stdout);

    }
    x = 1;
    while (x < pa_maxxg(stdout)) {

        pa_line(stdout, x, 1, x, pa_maxyg(stdout));
        x = x+pa_chrsizx(stdout);

    }
    pa_fcolor(stdout, pa_black);

}

/* display frame test */

void frametest(const string s)

{

    pa_evtrec er;
    int       x, y;

    x = 1; /* set default size */
    y = 1;
    do {

        pa_event(stdin, &er); /* get next event */
        if (er.etype == pa_etredraw) {

            putchar('\f');
            pa_fcolor(stdout, pa_cyan);
            pa_rect(stdout, 1, 1, x, y);
            pa_line(stdout, 1, 1, x, y);
            pa_line(stdout, 1, y, x, 1);
            pa_fcolor(stdout, pa_black);
            puts(s);

        }
        if (er.etype == etresize then {

            /* Save the new demensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = pa_maxxg(stdout);
            y = pa_maxyg(stdout);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);

}

int main(void)

{

    if (setjmp(terminate_buf)) goto terminate;
    pa_auto(OFF);
    pa_curvis(OFF);
    writeln('Managed screen test vs. 0.1');
    writeln;
    pa_scnsiz(x, y);
    writeln('Screen size character: x: ', x:1, ' y: ', y:1);
    pa_scnsizg(x, y);
    writeln('Screen size pixel: x: ', x:1, ' y: ', y:1);
    writeln;
    pa_getsiz(x, y);
    writeln('Window size character: x: ', x:1, ' y: ', y:1);
    pa_getsizg(ox, oy);
    writeln('Window size graphical: x: ', ox:1, ' y: ', oy:1);
    writeln;
    writeln('Client size character: x: ', pa_maxx(stdout), ' y: ', pa_maxy(stdout));
    writeln('Client size graphical: x: ', pa_maxxg(stdout), ' y: ', pa_maxyg(stdout));
    writeln;
    writeln('Hit return in any window to continue for each test');
    waitnext();

    /* ************************** Window titling test ************************** */

    pa_title('This is a mangement test window');
    writeln('The title bar of this window should read: This is a mangement test window');
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), 'Window title test');
    waitnext();

    /* ************************** Multiple windows ************************** */

    putchar('\f');
    pa_curvis(on);
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), 'Multiple window test');
    pa_home;
    pa_auto(on);
    writeln('This is the main window');
    writeln;
    writeln('Select back and forth between each window, and make sure the');
    writeln('cursor follows');
    writeln;
    write('Here is the cursor->');
    pa_openwin(stdin, win2, 2);
    writeln(win2, 'This is the second window');
    writeln(win2);
    write(win2, 'Here is the cursor->');
    waitnext();
    writeln;
    writeln('Now enter characters to each window, then end with return');
    waitnextprint();
    fclose(win2);
    putchar('\f');
    writeln('Second window now closed');
    waitnext();
    pa_curvis(off);
    pa_auto(off);

    /* ********************* Resize buffer window character ******************** */

    ox = maxx;
    oy = maxy;
    pa_bcolor(cyan);
    pa_sizbuf(50, 50);
    putchar('\f');
    for (x = 1; x <= pa_maxx(stdout); x++) write('*');
    cursor(1, pa_maxy(stdout));
    for (x = 1; x <= pa_maxx(stdout); x++) write('*');
    for (y = 1; y <= pa_maxy(stdout); y++) { pa_cursor(stdout, 1, y); write('*') };
    for y = 1 to pa_maxy(stdout) do { pa_cursor(pa_maxx(stdout), y); write('*') };
    pa_home(stdout);
    writeln('Buffer should now be 50 by 50 characters, and');
    writeln('painted blue');
    writeln('maxx: %d maxy: %d\n", pa_maxx(stdout), pa_maxy(stdout));
    writeln('Open up window to verify this');
    prtcen(pa_maxy(stdout), 'Buffer resize character test');
    pa_bcolor(pa_white);
    waitnext();
    pa_sizbuf(stdout, ox, oy);

    /* *********************** Resize buffer window pixel ********************** */

    ox = pa_maxxg;
    oy = pa_maxyg;
    pa_bcolor(pa_cyan);
    pa_sizbufg(400, 400);
    putchar('\f');
    linewidth(20);
    line(1, 1, pa_maxxg(stdout), 1);
    line(1, 1, 1, pa_maxyg(stdout));
    line(1, pa_maxyg(stdout), pa_maxxg(stdout), pa_maxyg(stdout));
    line(pa_maxxg(stdout), 1, pa_maxxg(stdout), pa_maxyg(stdout));
    writeln('Buffer should now be 400 by 400 pixels, and');
    writeln('painted blue');
    writeln('maxxg: ', pa_maxxg(stdout), ' maxyg: ', pa_maxyg(stdout));
    writeln('Open up window to verify this');
    prtcen(pa_maxy(stdout), 'Buffer resize graphical test');
    pa_bcolor(pa_white);
    waitnext();
    pa_sizbufg(ox, oy);

    /* ****************** Resize screen with buffer on character *************** */

    ox = pa_maxxg(stdout);
    oy = pa_maxyg(stdout);
    for x = 20 to 80 do {

       pa_setsiz(x, 25);
       pa_getsiz(x2, y2);
       if (x2 <> x) || (y2 <> 25) then {

          pa_setsiz(80, 25);
          putchar('\f');
          writeln('*** Getsiz does ! match setsiz');
          waitnext();
          longjmp(terminate_buf, 1)

       };
       putchar('\f');
       writeln('Resize screen buffered character');
       writeln;
       writeln('Moving in x');
       wait(1000)

    };
    writeln;
    writeln('Complete');
    waitnext();
    for y = 10 to 80 do {

       pa_setsiz(80, y);
       pa_getsiz(x2, y2);
       if (x2 <> 80) || (y2 <> y) then {

          pa_setsiz(80, 25);
          putchar('\f');
          writeln('*** Getsiz does ! match setsiz');
          waitnext();
          longjmp(terminate_buf, 1)

       };
       putchar('\f');
       writeln('Resize screen buffered character');
       writeln;
       writeln('Moving in y');
       wait(1000)

    };
    writeln;
    writeln('Complete');
    waitnext();
    pa_winclientg(ox, oy, ox, oy, [pa_wmframe, pa_wmsize, pa_wmsysbar]);
    pa_setsizg(ox, oy);

    /* ******************** Resize screen with buffer on pixel ***************** */

    ox = pa_maxxg(stdout);
    oy = pa_maxyg(stdout);
    for x = 200 to 800 do {

       pa_setsizg(x, 200);
       pa_getsizg(x2, y2);
       if (x2 <> x) || (y2 <> 200) then {

          pa_setsiz(80, 25);
          putchar('\f');
          writeln('*** Getsiz does ! match setsiz');
          waitnext();
          longjmp(terminate_buf, 1)

       };
       putchar('\f');
       writeln('Resize screen buffered graphical');
       writeln;
       writeln('Moving in x');
       wait(100)

    };
    writeln;
    writeln('Complete');
    waitnext();
    for y = 100 to 800 do {

       pa_setsizg(300, y);
       pa_getsizg(x2, y2);
       if (x2 <> 300) || (y2 <> y) then {

          pa_setsiz(80, 25);
          putchar('\f');
          writeln('*** Getsiz does ! match setsiz');
          waitnext();
          longjmp(terminate_buf, 1)

       };
       putchar('\f');
       writeln('Resize screen buffered graphical');
       writeln;
       writeln('Moving in y');
       wait(100)

    };
    writeln;
    writeln('Complete');
    waitnext();
    pa_winclientg(ox, oy, ox, oy, [wmframe, wmsize, wmsysbar]);
    pa_setsizg(ox, oy);

    /* ********************************* Front/back test *********************** */

    putchar('\f');
    pa_auto(off);
    writeln('Position window for font/back test');
    writeln('Then hit space to flip font/back status, or return to stop');
    fb = false; /* clear front/back status */
    pa_font(font_sign);
    pa_fontsiz(50);
    repeat

       pa_event(stdin, er);
       if er.etype == pa_etchar then if er.char == ' ' then { /* flip front/back */

          fb = ! fb;
          if fb then {

             pa_front;
             pa_fcolor(pa_white);
             prtceng(pa_maxyg(stdout) / 2-pa_chrsizy(stdout) / 2, 'Back');
             pa_fcolor(pa_black);
             prtceng(pa_maxyg(stdout) / 2-pa_chrsizy / 2, 'Front')

          } else {

             pa_back(stdout);
             pa_fcolor(pa_white);
             prtceng(pa_maxyg(stdout) / 2-pa_chrsizy(stdout) / 2, 'Front');
             pa_fcolor(pa_black);
             prtceng(pa_maxyg(stdout) / 2-pa_chrsizy(stdout) / 2, 'Back')

          }

       };
       if er.etype == pa_etterm then longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_home(stdout);
    pa_font(font_term);
    pa_auto(on);

    /* ************************* Frame controls test buffered ****************** */

    putchar('\f');
    pa_fcolor(pa_cyan);
    pa_rect(1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(1, pa_maxyg(stdout), pa_maxxg(stdout), 1);
    pa_fcolor(pa_black);
    pa_binvis(stdout);
    writeln('Ready for frame controls buffered');
    waitnext();
    pa_frame(off);
    writeln('Entire frame off');
    waitnext();
    pa_frame(on);
    writeln('Entire frame on');
    waitnext();
    pa_sysbar(off);
    writeln('System bar off');
    waitnext();
    pa_sysbar(on);
    writeln('System bar on');
    waitnext();
    pa_sizable(off);
    writeln('Size bars off');
    waitnext();
    pa_sizable(on);
    writeln('Size bars on');
    waitnext();
    pa_bover;

    /* ************************* Frame controls test buffered ****************** */

    pa_buffer(off);
    frametest('Ready for frame controls unbuffered');
    frame(off);
    frametest('Entire frame off');
    frame(on);
    frametest('Entire frame on');
    pa_sysbar(off);
    frametest('System bar off');
    pa_sysbar(on);
    frametest('System bar on');
    pa_sizable(off);
    frametest('Size bars off');
    pa_sizable(on);
    frametest('Size bars on');
    pa_buffer(on);

    /* ********************************* Menu test ***************************** */

    putchar('\f');
    pa_fcolor(pa_cyan);
    pa_rect(1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(1, pa_maxyg(stdout), pa_maxxg(stdout), 1);
    pa_fcolor(pa_black);
    ml = nil; /* clear menu list */
    newmenu(mp, false, false, off, 1, 'Say hello');
    appendmenu(ml, mp);
    newmenu(mp, true, false,  on, 2, 'Bark');
    appendmenu(ml, mp);
    newmenu(mp, false, false, off, 3, 'Walk');
    appendmenu(ml, mp);
    newmenu(sm, false, false, off, 4, 'Sublist');
    appendmenu(ml, sm);
    /* these are one/of buttons */
    newmenu(mp, false, true,  off, 5, 'slow');
    appendmenu(sm->branch, mp);
    newmenu(mp, false, true,  off, 6, 'medium');
    appendmenu(sm->branch, mp);
    newmenu(mp, false, false, on, 7, 'fast');
    appendmenu(sm->branch, mp);
    /* these are on/off buttons */
    newmenu(mp, true, false,  off, 8, 'red');
    appendmenu(sm->branch, mp);
    newmenu(mp, true, false,  off, 9, 'green');
    appendmenu(sm->branch, mp);
    newmenu(mp, true, false,  off, 10, 'blue');
    appendmenu(sm->branch, mp);
    pa_menu(ml);
    pa_menuena(3, off); /* disable 'Walk' */
    pa_menusel(5, on); /* turn on 'slow' */
    pa_menusel(8, on); /* turn on 'red' */

    pa_home(stdout);
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

       pa_event(stdin, &er);
       if er.etype == pa_etterm then longjmp(terminate_buf, 1);
       if er.etype == pa_etmenus then {

          write('Menu select: ');
          case er.menuid of

             1:  writeln('Say hello');
             2:  writeln('Bark');
             3:  writeln('Walk');
             4:  writeln('Sublist');
             5:  { writeln('slow'); pa_menusel(5, on) };
             6:  { writeln('medium'); pa_menusel(6, on) };
             7:  { writeln('fast'); pa_menusel(7, on) };
             8:  { writeln('red'); sred = ! sred; pa_menusel(8, sred) };
             9:  { writeln('green'); sgreen = ! sgreen; pa_menusel(9, sgreen) };
             10: { writeln('blue'); sblue = ! sblue; pa_menusel(10, sblue) }

          }

       }

    until (er.etype == pa_etenter) || (er.etype == pa_etterm);
    pa_menu(nil);

    /* ****************************** Standard menu test *********************** */

    putchar('\f');
    pa_auto(on);
    ml = nil; /* clear menu list */
    newmenu(mp, false, false, off, smmax+1, 'one');
    appendmenu(ml, mp);
    newmenu(mp, true, false,  on, smmax+2, 'two');
    appendmenu(ml, mp);
    newmenu(mp, false, false, off, smmax+3, 'three');
    appendmenu(ml, mp);
    pa_stdmenu([pa_smnew, pa_smopen, pa_smclose, pa_smsave, pa_smsaveas, pa_smpageset, pa_smprint, pa_smexit,
             pa_smundo, pa_smcut, pa_smpaste, pa_smdelete, pa_smfind, pa_smfindnext, pa_smreplace,
             pa_smgoto, pa_smselectall, pa_smnewwindow, pa_smtilehoriz, pa_smtilevert,
             pa_smcascade, pa_smcloseall, pa_smhelptopic, pa_smabout], mp, ml);
    pa_menu(mp);
    writeln('Standard menu appears above');
    writeln('Check our ''one'', ''two'', ''three'' buttons are in the program');
    writeln('defined position');
    repeat

       pa_event(stdin, er);
       if er.etype == pa_etterm then longjmp(terminate_buf, 1);
       if er.etype == pa_etmenus then {

          write('Menu select: ');
          case er.menuid of

            pa_smnew:       writeln('new');
            pa_smopen:      writeln('open');
            pa_smclose:     writeln('close');
            pa_smsave:      writeln('save');
            pa_smsaveas:    writeln('saveas');
            pa_smpageset:   writeln('pageset');
            pa_smprint:     writeln('print');
            pa_smexit:      writeln('exit');
            pa_smundo:      writeln('undo');
            pa_smcut:       writeln('cut');
            pa_smpaste:     writeln('paste');
            pa_smdelete:    writeln('delete');
            pa_smfind:      writeln('find');
            pa_smfindnext:  writeln('findnext');
            pa_smreplace:   writeln('replace');
            pa_smgoto:      writeln('goto');
            pa_smselectall: writeln('selectall');
            pa_smnewwindow: writeln('newwindow');
            pa_smtilehoriz: writeln('tilehoriz');
            pa_smtilevert:  writeln('tilevert');
            pa_smcascade:   writeln('cascade');
            pa_smcloseall:  writeln('closeall');
            pa_smhelptopic: writeln('helptopic');
            pa_smabout:     writeln('about');
            smmax+1:     writeln('one');
            smmax+2:     writeln('two');
            smmax+3:     writeln('three');

         }

      }

    until (er.etype == pa_etenter) || (er.etype == pa_etterm);
    pa_menu(nil);

    /* ************************* Child windows test character ****************** */

    putchar('\f');
    chrgrid();
    prtcen(pa_maxy(stdout), 'Child windows test character');
    pa_openwin(stdin, win2, stdout, 2);
    pa_setpos(win2, 1, 10);
    pa_sizbuf(win2, 20, 10);
    pa_setsiz(win2, 20, 10);
    pa_openwin(stdin, win3, stdout, 3);
    pa_setpos(win3, 21, 10);
    pa_sizbuf(win3, 20, 10);
    pa_setsiz(win3, 20, 10);
    pa_openwin(stdin, win4, stdout, 4);
    pa_setpos(win4, 41, 10);
    pa_sizbuf(win4, 20, 10);
    pa_setsiz(win4, 20, 10);
    pa_bcolor(win2, pa_cyan);
    page(win2);
    writeln(win2, 'I am child window 1');
    pa_bcolor(win3, pa_yellow);
    page(win3);
    writeln(win3, 'I am child window 2');
    pa_bcolor(win4, pa_magenta);
    page(win4);
    writeln(win4, 'I am child window 3');
    pa_home;
    writeln('There should be 3 labeled child windows below, with frames   ');
    waitnext();
    pa_frame(win2, off);
    pa_frame(win3, off);
    pa_frame(win4, off);
    pa_home;
    writeln('There should be 3 labeled child windows below, without frames');
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_home;
    writeln('Child windows should all be closed                           ');
    waitnext();

    /* *************************** Child windows test pixel ******************** */

    putchar('\f');
    prtcen(maxy, 'Child windows test pixel');
    pa_openwin(stdin, win2, stdout, 2);
    pa_setposg(win2, 1, 100);
    pa_sizbufg(win2, 200, 200);
    pa_setsizg(win2, 200, 200);
    pa_openwin(stdin, win3, stdout, 3);
    pa_setposg(win3, 201, 100);
    pa_sizbufg(win3, 200, 200);
    pa_setsizg(win3, 200, 200);
    pa_openwin(stdin, win4, stdout, 4);
    pa_setposg(win4, 401, 100);
    pa_sizbufg(win4, 200, 200);
    pa_setsizg(win4, 200, 200);
    pa_bcolor(win2, cyan);
    page(win2);
    writeln(win2, 'I am child window 1');
    bcolor(win3, yellow);
    page(win3);
    writeln(win3, 'I am child window 2');
    pa_bcolor(win4, magenta);
    page(win4);
    writeln(win4, 'I am child window 3');
    pa_home(stdout);
    writeln('There should be 3 labled child windows below, with frames   ');
    waitnext();
    pa_frame(win2, off);
    pa_frame(win3, off);
    pa_frame(win4, off);
    pa_home(stdout);
    writeln('There should be 3 labled child windows below, without frames');
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_home(stdout);
    writeln('Child windows should all be closed                          ');
    waitnext();

    /* ******************* Child windows stacking test pixel ******************* */

    putchar('\f');
    prtcen(maxy, 'Child windows stacking test pixel');
    pa_openwin(stdin, win2, stdout, 2);
    pa_setposg(win2, 50, 50);
    pa_sizbufg(win2, 200, 200);
    pa_setsizg(win2, 200, 200);
    pa_openwin(stdin, win3, stdout, 3);
    pa_setposg(win3, 150, 100);
    pa_sizbufg(win3, 200, 200);
    pa_setsizg(win3, 200, 200);
    pa_openwin(stdin, win4, stdout, 4);
    pa_setposg(win4, 250, 150);
    pa_sizbufg(win4, 200, 200);
    pa_setsizg(win4, 200, 200);
    pa_bcolor(win2, cyan);
    page(win2);
    writeln(win2, 'I am child window 1');
    pa_bcolor(win3, yellow);
    page(win3);
    writeln(win3, 'I am child window 2');
    pa_bcolor(win4, magenta);
    page(win4);
    writeln(win4, 'I am child window 3');
    pa_home(stdout);
    writeln('There should be 3 labled child windows below, overlapped,   ');
    writeln('with child 1 on the bottom, child 2 middle, && child 3 top.');
    waitnext();
    pa_back(win2);
    pa_back(win3);
    pa_back(win4);
    pa_home(stdout);
    writeln('Now the windows are reordered, with child 1 on top, child 2 ');
    writeln('below that, && child 3 on the bottom.                      ');
    waitnext();
    pa_front(win2);
    pa_front(win3);
    pa_front(win4);
    pa_home(stdout);
    writeln('Now the windows are reordered, with child 3 on top, child 2 ');
    writeln('below that, && child 1 on the bottom.                      ');
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    putchar('\f');
    writeln('Child windows should all be closed                          ');
    waitnext();

    /* ************** Child windows stacking resize test pixel 1 *************** */

    pa_buffer(off);
    pa_auto(off);
    pa_openwin(stdin, win2, stdout, 2);
    pa_setposg(win2, 50-25, 50-25);
    pa_sizbufg(win2, 200, 200);
    pa_setsizg(win2, maxxg(stdout)-150, maxyg(stdout)-150);
    pa_openwin(stdin, win3, stdout, 3);
    pa_setposg(win3, 100-25, 100-25);
    pa_sizbufg(win3, 200, 200);
    pa_setsizg(win3, maxxg(stdout)-150, maxyg(stdout)-150);
    pa_openwin(stdin, win4, stdout, 4);
    pa_setposg(win4, 150-25, 150-25);
    pa_sizbufg(win4, 200, 200);
    pa_setsizg(win4, maxxg(stdout)-150, maxyg(stdout)-150);
    pa_bcolor(win2, pa_cyan);
    page(win2);
    writeln(win2, 'I am child window 1');
    pa_bcolor(win3, pa_yellow);
    page(win3);
    writeln(win3, 'I am child window 2');
    pa_bcolor(win4, pa_magenta);
    page(win4);
    writeln(win4, 'I am child window 3');
    repeat

        pa_event(er);
        if er.etype == pa_etredraw then {

            putchar('\f');
            prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), 'Child windows stacking resize test pixel 1');
            prtceng(1, 'move and resize');
            pa_setsizg(win3, pa_maxxg(stdout)-150, pa_maxyg(stdout)-150);
            pa_setsizg(win4, pa_maxxg(stdout)-150, pa_maxyg(stdout)-150);
            pa_setsizg(win2, pa_maxxg(stdout)-150, pa_maxyg(stdout)-150);

        }
        if er.etype == pa_etterm then longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    fclose(win2);
    fclose(win3);
    fclose(win4);
/*  ???????? There is a hole in the buffer after this gets enabled */
    pa_buffer(on);
    putchar('\f');
    writeln('Child windows should all be closed                          ');
    waitnext();

    /* ************** Child windows stacking resize test pixel 2 *************** */

    pa_buffer(off);
    pa_openwin(stdin, win2, stdout, 2);
    pa_setposg(win2, 50, 50);
    pa_sizbufg(win2, 200, 200);
    pa_setsizg(win2, maxxg(stdout)-100, maxyg(stdout)-100);
    pa_openwin(stdin, win3, stdout, 3);
    pa_setposg(win3, 100, 100);
    pa_sizbufg(win3, 200, 200);
    pa_setsizg(win3, maxxg(stdout)-200, maxyg(stdout)-200);
    pa_openwin(stdin, win4, stdout, 4);
    pa_setposg(win4, 150, 150);
    pa_sizbufg(win4, 200, 200);
    pa_setsizg(win4, maxxg(stdout)-300, maxyg(stdout)-300);
    pa_bcolor(win2, pa_cyan);
    page(win2);
    writeln(win2, 'I am child window 1');
    pa_bcolor(win3, pa_yellow);
    page(win3);
    writeln(win3, 'I am child window 2');
    pa_bcolor(win4, pa_magenta);
    page(win4);
    writeln(win4, 'I am child window 3');
    repeat

        pa_event(er);
        if er.etype == pa_etredraw then {

            putchar('\f');
            prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), 'Child windows stacking resize test pixel 2');
            prtceng(1, 'move and resize');
            pa_setsizg(win3, pa_maxxg(stdout)-200, pa_maxyg(stdout)-200);
            pa_setsizg(win4, pa_maxxg(stdout)-300, pa_maxyg(stdout)-300);
            pa_setsizg(win2, pa_maxxg(stdout)-100, pa_maxyg(stdout)-100);

        }
        if er.etype == pa_etterm then longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_buffer(on);
    putchar('\f');
    writeln('Child windows should all be closed                          ');
    waitnext();

    /* ******************************* Buffer off test *********************** */

    putchar('\f');
    pa_auto(off);
    pa_buffer(off);
    /* initalize prime size information */
    x = pa_maxxg(stdout);
    y = pa_maxyg(stdout);
    linewidth(5); /* set large lines */
    pa_font(font_sign);
    pa_binvis(stdout);
    repeat

       pa_event(er); /* get next event */
       if er.etype == pa_etredraw then {

          /* clear screen without overwriting frame */
          pa_fcolor(pa_white);
          pa_frect(1+5, 1+5, x-5, y-5);
          pa_fcolor(pa_black);
          pa_fontsiz(y / 10);
          prtceng(maxyg(stdout) / 2-pa_chrsizy(stdout) / 2, 'SIZE AND COVER ME !');
          pa_rect(1+2, 1+2, x-2, y-2); /* frame the window */

       };
       if er.etype == pa_etresize then {

          /* Save the new demensions, even if ! required. This way we must
            get a resize notification for this test to work. */
          x = pa_maxxg(stdout);
          y = pa_maxyg(stdout);

       };
       if er.etype == pa_etterm then longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_buffer(on);

    /* ******************************* min/max/norm test *********************** */

    putchar('\f');
    pa_auto(off);
    pa_buffer(off);
    pa_font(font_term);
    mincnt = 0; /* clear minimize counter */
    maxcnt = 0; /* clear maximize counter */
    nrmcnt = 0; /* clear normalize counter */
    repeat

       pa_event(er); /* get next event */
       if er.etype == pa_etredraw then {

          putchar('\f');
          writeln('Minimize, maximize && restore this window');
          writeln;
          writeln('Minimize count:  ', mincnt);
          writeln('Maximize count:  ', maxcnt);
          writeln('Normalize count: ', nrmcnt);

       };
       /* count minimize, maximize, normalize */
       if er.etype == pa_etmax then maxcnt = maxcnt+1;
       if er.etype == pa_etmin then mincnt = mincnt+1;
       if er.etype == pa_etnorm then nrmcnt = nrmcnt+1;

       if er.etype == pa_etterm then longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_buffer(on);

    /* ************************ Window size calculate character ******************** */

    putchar('\f');
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), 'Window size calculate character');
    pa_home(stdout);
    pa_openwin(stdin, win2, 2);
    pa_linewidth(1);

    pa_winclient(20, 10, x, y, [pa_wmframe, pa_wmsize, pa_wmsysbar]);
    writeln('For (20, 10) client, full frame, window size is: ', x:1, ',', y:1);
    pa_setsiz(win2, x, y);
    page(win2);
    pa_fcolor(win2, pa_black);
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
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, off);
    writeln('Check client window has (20, 10) surface');
    waitnext();

    writeln('System bar off');
    pa_sysbar(win2, off);
    pa_winclient(20, 10, x, y, [pa_wmframe, pa_wmsize]);
    writeln('For (20, 10) client, no system bar, window size is: ', x:1, ',', y:1);
    pa_setsiz(win2, x, y);
    page(win2);
    pa_fcolor(win2, pa_black);
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
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, off);
    writeln('Check client window has (20, 10) surface');
    waitnext();

    writeln('Sizing bars off');
    pa_sysbar(win2, on);
    pa_sizable(win2, off);
    pa_winclient(20, 10, x, y, [pa_wmframe, pa_wmsysbar]);
    writeln('For (20, 10) client, no size bars, window size is: ', x:1, ',', y:1);
    pa_setsiz(win2, x, y);
    page(win2);
    pa_fcolor(win2, pa_black);
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
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, off);
    writeln('Check client window has (20, 10) surface');
    waitnext();

    writeln('frame off');
    pa_sysbar(win2, on);
    pa_sizable(win2, on);
    pa_frame(win2, off);
    pa_winclient(20, 10, x, y, [pa_wmsize, pa_wmsysbar]);
    writeln('For (20, 10) client, no frame, window size is: ', x:1, ',', y:1);
    pa_setsiz(win2, x, y);
    page(win2);
    pa_fcolor(win2, pa_black);
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
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, off);
    writeln('Check client window has (20, 10) surface');
    waitnext();

    close(win2);

    /* ************************ Window size calculate pixel ******************** */

    putchar('\f');
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), 'Window size calculate pixel');
    pa_home(stdout);
    pa_openwin(stdin, win2, 2);
    pa_linewidth(1);
    pa_fcolor(win2, pa_cyan);
    pa_winclientg(200, 200, x, y, [pa_wmframe, pa_wmsize, pa_wmsysbar]);
    writeln('For (200, 200) client, full frame, window size is: ',
            x:1, ',', y:1);
    pa_setsizg(win2, x, y);
    pa_rect(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 200, 200, 1);
    pa_curvis(win2, off);
    writeln('Check client window has (200, 200) surface');
    waitnext();

    writeln('System bar off');
    pa_sysbar(win2, off);
    pa_winclientg(200, 200, x, y, [pa_wmframe, pa_wmsize]);
    writeln('For (200, 200) client, no system bar, window size is: ',
           x:1, ',', y:1);
    setsizg(win2, x, y);
    page(win2);
    rect(win2, 1, 1, 200, 200);
    line(win2, 1, 1, 200, 200);
    line(win2, 1, 200, 200, 1);
    writeln('Check client window has (200, 200) surface');
    waitnext();

    writeln('Sizing bars off');
    pa_sysbar(win2, on);
    pa_sizable(win2, off);
    pa_winclientg(200, 200, x, y, [pa_wmframe, pa_wmsysbar]);
    writeln('For (200, 200) client, no sizing, window size is: ',
            x:1, ',', y:1);
    pa_setsizg(win2, x, y);
    page(win2);
    pa_rect(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 200, 200, 1);
    writeln('Check client window has (200, 200) surface');
    waitnext();

    writeln('frame off');
    pa_sysbar(win2, on);
    pa_sizable(win2, on);
    pa_frame(win2, off);
    pa_winclientg(200, 200, x, y, [pa_wmsize, pa_wmsysbar]);
    writeln('For (200, 200) client, no frame, window size is: ',
            x:1, ',', y:1);
    pa_setsizg(win2, x, y);
    page(win2);
    pa_rect(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 200, 200, 1);
    writeln('Check client window has (200, 200) surface');
    waitnext();

    fclose(win2);

    /* ******************* Window size calculate minimums pixel *************** */

    /* this test does not work, winclient needs to return the minimums */

#if 0
    putchar('\f');
    prtceng(maxyg(stdout)-chrsizy(stdout), 'Window size calculate minimum pixel');
    pa_home(stdout);
    pa_openwin(stdin, win2, 2);
    pa_linewidth(1);
    pa_fcolor(win2, cyan);
    pa_winclientg(1, 1, x, y, [pa_wmframe, pa_wmsize, pa_wmsysbar]);
    writeln('For (200, 200) client, full frame, window size minimum is: ',
            x:1, ',', y:1);
    pa_setsizg(win2, 1, 1);
    pa_getsizg(win2, x2, y2);
    waitnext();

    fclose(win2);
#endif

   /* ********************** Child windows torture test pixel ***************** */

/* ?????? Overflows memory on this test */
    putchar('\f');
    writeln('Child windows torture test pixel');
    for (i = 1; i <= 100; i++) {

        openwin(stdin, win2, stdout, 2);
        setposg(win2, 1, 100);
        sizbufg(win2, 200, 200);
        setsizg(win2, 200, 200);
        openwin(stdin, win3, stdout, 3);
        setposg(win3, 201, 100);
        sizbufg(win3, 200, 200);
        setsizg(win3, 200, 200);
        openwin(stdin, win4, stdout, 4);
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
        fclose(win2);
        fclose(win3);
        fclose(win4);

    }
    pa_home(stdout);
    write('Child windows should all be closed');
    waitnext();

    terminate: /* terminate */

    putchar('\f');
    pa_auto(off);
    pa_font(font_sign);
    pa_fontsiz(50);
    prtceng(maxyg(stdout) / 2-chrsizy(stdout) / 2, 'Test complete');

}
