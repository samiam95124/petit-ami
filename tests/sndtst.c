/*******************************************************************************

Sound library test program

Goes through various test cases on the sound library.

*******************************************************************************/

#include <terminal.h> /* terminal level functions */
#include <sound.h>    /* sound library */

/*******************************************************************************

Wait user interaction

Wait return to be pressed, or handle terminate.

*******************************************************************************/

static void waitnext(void)
{

    pa_evtrec er; /* event record */

    do { pa_event(stdin, &er);
    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) { longjmp(terminate_buf, 1); }

}

int main(int argc, char *argv[])


{

    int dport; /* output port */

    dport = PA_SYNTH_OUT; /* index standard synth output port */
    pa_opensynthout(dport);

    printf("Sound library test\n");



    return (0); /* return no error */

}
