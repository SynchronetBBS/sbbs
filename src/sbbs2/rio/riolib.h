/* RIOLIB.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#ifndef _RIOLIB_H
#define _RIOLIB_H

#include "gen_defs.h"

int 	rioini(int com,int irq);		/* initialize com,irq */
int 	setbaud(int rate);				/* set baud rate */
int 	rioctl(ushort action);			/* remote i/o control */
int 	dtr(char onoff);				/* set/reset dtr */
int 	outcom(int ch); 				/* send character */
int 	incom(void);					/* receive character */
int 	ivhctl(int intcode);			/* local i/o redirection */

/* Win32 and OS/2 versions only */

long	rio_getbaud(void);				/* get current baud rate */
int 	rio_getstate(void); 			/* get current current state */
void	outcom_thread(void *);

extern	int rio_handle;

#endif	/* Don't add anything after this line */
