/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _CIOLIB_MOUSE_H_
#define _CIOLIB_MOUSE_H_

struct mouse_event {
	int event;
	int bstate;
	int kbsm;
	int startx;
	int starty;
	int endx;
	int endy;
};

#define CIOLIB_BUTTON_1	1
#define CIOLIB_BUTTON_2	2
#define CIOLIB_BUTTON_3	4

#define CIOLIB_BUTTON(x)	(1<<(x-1))

enum {
	 CIOLIB_MOUSE_MOVE				/* 0 */
	,CIOLIB_BUTTON_1_PRESS
	,CIOLIB_BUTTON_1_RELEASE
	,CIOLIB_BUTTON_1_CLICK
	,CIOLIB_BUTTON_1_DBL_CLICK
	,CIOLIB_BUTTON_1_TRPL_CLICK
	,CIOLIB_BUTTON_1_QUAD_CLICK
	,CIOLIB_BUTTON_1_DRAG_START
	,CIOLIB_BUTTON_1_DRAG_MOVE
	,CIOLIB_BUTTON_1_DRAG_END
	,CIOLIB_BUTTON_2_PRESS			/* 10 */
	,CIOLIB_BUTTON_2_RELEASE
	,CIOLIB_BUTTON_2_CLICK
	,CIOLIB_BUTTON_2_DBL_CLICK
	,CIOLIB_BUTTON_2_TRPL_CLICK
	,CIOLIB_BUTTON_2_QUAD_CLICK
	,CIOLIB_BUTTON_2_DRAG_START
	,CIOLIB_BUTTON_2_DRAG_MOVE
	,CIOLIB_BUTTON_2_DRAG_END
	,CIOLIB_BUTTON_3_PRESS
	,CIOLIB_BUTTON_3_RELEASE		/* 20 */
	,CIOLIB_BUTTON_3_CLICK
	,CIOLIB_BUTTON_3_DBL_CLICK
	,CIOLIB_BUTTON_3_TRPL_CLICK
	,CIOLIB_BUTTON_3_QUAD_CLICK
	,CIOLIB_BUTTON_3_DRAG_START
	,CIOLIB_BUTTON_3_DRAG_MOVE
	,CIOLIB_BUTTON_3_DRAG_END		/* 27 */
};

#define CIOLIB_BUTTON_PRESS(x)		((x-1)*9+1)
#define CIOLIB_BUTTON_RELEASE(x)	((x-1)*9+2)
#define CIOLIB_BUTTON_CLICK(x)		((x-1)*9+3)
#define CIOLIB_BUTTON_DBL_CLICK(x)	((x-1)*9+4)
#define CIOLIB_BUTTON_TRPL_CLICK(x)	((x-1)*9+5)
#define CIOLIB_BUTTON_QUAD_CLICK(x)	((x-1)*9+6)
#define CIOLIB_BUTTON_DRAG_START(x)	((x-1)*9+7)
#define CIOLIB_BUTTON_DRAG_MOVE(x)	((x-1)*9+8)
#define CIOLIB_BUTTON_DRAG_END(x)	((x-1)*9+9)

#define CIOLIB_BUTTON_NUMBER(x)		((x+8)/9)

#define CIOLIB_BUTTON_BASE(x)		(x!=CIOLIB_MOUSE_MOVE?x-9*(CIOLIB_BUTTON_NUMBER(x)-1):CIOLIB_MOUSE_MOVE)

#ifdef __cplusplus
extern "C" {
#endif
void ciomouse_gotevent(int event, int x, int y);
int mouse_trywait(void);
int mouse_wait(void);
int mouse_pending(void);
int ciolib_getmouse(struct mouse_event *mevent);
int ciolib_ungetmouse(struct mouse_event *mevent);
void ciolib_mouse_thread(void *data);
int ciomouse_setevents(int events);
int ciomouse_addevents(int events);
int ciomouse_delevents(int events);
int ciomouse_addevent(int event);
int ciomouse_delevent(int event);
#ifdef __cplusplus
}
#endif

#endif
