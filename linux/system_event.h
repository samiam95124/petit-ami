/*******************************************************************************
*                                                                              *
*                      SYSTEM EVENT HANDLER HEADER FILE                        *
*                                                                              *
*                       Copyright (C) 2021 Scott A. Franco                     *
*                                                                              *
* Presents the system event handler API.                                       *
*                                                                              *
*******************************************************************************/

#ifndef __SYSTEM_EVENT_H__
#define __SYSTEM_EVENT_H__

/* system event types */
typedef enum {

    se_none, /* no event */
    se_inp,  /* input file ready */
    se_tim,  /* timer fires */
    se_sig   /* signal event */

} sevttyp;

typedef struct sysevt* sevptr; /* pointer to system event */
typedef struct sysevt {

    sevttyp typ; /* system event type */
    int     lse; /* logical event number */

} sysevt;

int system_event_addseinp(int fid);
int system_event_addsesig(int sig);
int system_event_addsetim(int sid, int t, int r);
void system_event_deasetim(int sid);
void system_event_getsevt(sevptr ev);

#endif /* __SYSTEM_EVENT_H__ */
