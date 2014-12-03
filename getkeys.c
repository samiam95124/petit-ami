/*******************************************************************************

Print keyboard hex equivalents

Used to see what the exact codes being received from the keyboard are.

*******************************************************************************/

#include <termios.h>
#include <stdio.h>

static struct termios trmsav;

static void set_tty_raw(void)

{

    struct termios raw;

    raw = trmsav;

    /* input modes - clear indicated ones giving: no break, no CR to NL,
       no parity check, no strip char, no start/stop output (sic) control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* output modes - clear giving: no post processing such as NL to CR+NL */
    raw.c_oflag &= ~(OPOST);

    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);

    /* local modes - clear giving: echoing off, canonical off (no erase with
       backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* put terminal in raw mode after flushing */
    tcsetattr(0,TCSAFLUSH,&raw);

}

main()

{

    char c;

    tcgetattr(0,&trmsav); /* save original state of terminal */
    set_tty_raw();
    printf("Key printer, input keys, control-c to stop\r\n");
    do {

        //c = getchar();
        read(0, &c, 1);
        printf("Key was: %o\r\n", c);

    } while (c != 3);
    tcsetattr(0,TCSAFLUSH,&trmsav);

}
