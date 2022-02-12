/* $Id: mouse.c,v 1.48 2020/06/27 00:04:45 deuce Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <genwrap.h>
#include <semwrap.h>
#include <threadwrap.h>
#include <link_list.h>

#include "ciolib.h"

#define MSEC_CLOCK()	xp_timer64()

enum {
	 MOUSE_NOSTATE
	,MOUSE_SINGLEPRESSED
	,MOUSE_CLICKED
	,MOUSE_DOUBLEPRESSED
	,MOUSE_DOUBLECLICKED
	,MOUSE_TRIPLEPRESSED
	,MOUSE_TRIPLECLICKED
	,MOUSE_QUADPRESSED
	,MOUSE_QUADCLICKED
	,MOUSE_DRAGSTARTED
};

struct in_mouse_event {
	int	event;
	int	x;
	int	y;
	int	x_res;
	int	y_res;
	long long ts;
	void	*nextevent;
};

struct out_mouse_event {
	int event;
	int bstate;
	int kbsm;		/* Known button state mask */
	int startx;
	int starty;
	int endx;
	int endy;
	int startx_res;
	int starty_res;
	int endx_res;
	int endy_res;
	void *nextevent;
};

struct mouse_state {
	int	buttonstate;			/* Current state of all buttons - bitmap */
	int	knownbuttonstatemask;	/* Mask of buttons that have done something since
								 * We started watching... the rest are actually in
								 * an unknown state */
	int	button_state[5];		/* Expanded state of each button */
	int	button_x[5];			/* Start X/Y position of the current state */
	int	button_y[5];
	int	button_x_res[5];			/* Start X/Y position of the current state */
	int	button_y_res[5];
	long long timeout[5];	/* Button event timeouts (timespecs ie: time of expiry) */
	int	curx;					/* Current X position */
	int	cury;					/* Current Y position */
	int	curx_res;					/* Current X position */
	int	cury_res;					/* Current Y position */
	int	events;					/* Currently enabled events */
	int	click_timeout;			/* Timeout between press and release events for a click (ms) */
	int	multi_timeout;			/* Timeout after a click for detection of multi clicks (ms) */
	int	click_drift;			/* Allowed "drift" during a click event */
	link_list_t	input;
	link_list_t	output;
};

struct mouse_state state;
uint64_t mouse_events=0;
int ciolib_mouse_initialized=0;
static int ungot=0;
pthread_mutex_t unget_mutex;

void init_mouse(void)
{
	memset(&state,0,sizeof(state));
	state.click_timeout=0;
	state.multi_timeout=300;
	listInit(&state.input,LINK_LIST_SEMAPHORE|LINK_LIST_MUTEX);
	listInit(&state.output,LINK_LIST_SEMAPHORE|LINK_LIST_MUTEX);
	pthread_mutex_init(&unget_mutex, NULL);
	ciolib_mouse_initialized=1;
}

void mousestate(int *x, int *y, uint8_t *buttons)
{
	if (!ciolib_mouse_initialized) {
		if (x)
			*x = -1;
		if (y)
			*y = -1;
		return;
	}
	if (x)
		*x = state.curx;
	if (y)
		*y = state.cury;
	if (buttons)
		*buttons = (state.buttonstate & 0xff);
	return;
}

void mousestate_res(int *x, int *y, uint8_t *buttons)
{
	if (!ciolib_mouse_initialized) {
		if (x)
			*x = -1;
		if (y)
			*y = -1;
		return;
	}
	if (x)
		*x = state.curx_res;
	if (y)
		*y = state.cury_res;
	if (buttons)
		*buttons = (state.buttonstate & 0xff);
	return;
}

uint64_t ciomouse_setevents(uint64_t events)
{
	mouse_events=events;
	return mouse_events;
}

uint64_t ciomouse_addevents(uint64_t events)
{
	mouse_events |= events;
	return mouse_events;
}

uint64_t ciomouse_delevents(uint64_t events)
{
	mouse_events &= ~events;
	return mouse_events;
}

uint64_t ciomouse_addevent(uint64_t event)
{
	mouse_events |= (UINT64_C(1)<<event);
	return mouse_events;
}

uint64_t ciomouse_delevent(uint64_t event)
{
	mouse_events &= ~(UINT64_C(1)<<event);
	return mouse_events;
}

void ciomouse_gotevent(int event, int x, int y, int x_res, int y_res)
{
	struct in_mouse_event *ime;

	while(!ciolib_mouse_initialized)
		SLEEP(1);
	ime=(struct in_mouse_event *)malloc(sizeof(struct in_mouse_event));
	if(ime) {
		ime->ts=MSEC_CLOCK();
		ime->event=event;
		ime->x=x;
		ime->y=y;
		ime->x_res=x_res;
		ime->y_res=y_res;
		ime->nextevent=NULL;

		listPushNode(&state.input,ime);
	}
}

void add_outevent(int event, int x, int y, int xres, int yres)
{
	struct out_mouse_event *ome;
	int	but;

	if(!(mouse_events & UINT64_C(1)<<event))
		return;
	ome=(struct out_mouse_event *)malloc(sizeof(struct out_mouse_event));

	if(ome) {
		but=CIOLIB_BUTTON_NUMBER(event);
		ome->event=event;
		ome->bstate=state.buttonstate;
		ome->kbsm=state.knownbuttonstatemask;
		ome->startx=but?state.button_x[but-1]:state.curx;
		ome->starty=but?state.button_y[but-1]:state.cury;
		ome->endx=x;
		ome->endy=y;
		ome->startx_res=but ? state.button_x_res[but-1] : state.curx_res;
		ome->starty_res=but ? state.button_y_res[but-1] : state.cury_res;
		ome->endx_res=xres;
		ome->endy_res=yres;

		ome->nextevent=(struct out_mouse_event *)NULL;

		listPushNode(&state.output,ome);
	}
}

int more_multies(int button, int clicks)
{
	switch(clicks) {
		case 0:
			if(mouse_events & (UINT64_C(1)<<CIOLIB_BUTTON_CLICK(button)))
				return(1);
			/* Fall-through */
		case 1:
			if(mouse_events & (UINT64_C(1)<<CIOLIB_BUTTON_DBL_CLICK(button)))
				return(1);
			/* Fall-through */
		case 2:
			if(mouse_events & (UINT64_C(1)<<CIOLIB_BUTTON_TRPL_CLICK(button)))
				return(1);
			/* Fall-through */
		case 3:
			if(mouse_events & (UINT64_C(1)<<CIOLIB_BUTTON_QUAD_CLICK(button)))
				return(1);
			/* Fall-through */
	}
	return(0);
}

void ciolib_mouse_thread(void *data)
{
	int	timedout;
	int timeout_button=0;
	int but;
	int delay;
	long long ttime=0;

	SetThreadName("Mouse");
	init_mouse();
	while(1) {
		timedout=0;
		if(timeout_button) {
			delay=state.timeout[timeout_button-1]-MSEC_CLOCK();
			if(delay<=0) {
				timedout=1;
			}
			else {
				timedout=!listSemTryWaitBlock(&state.input,delay);
			}
		}
		else {
			listSemWait(&state.input);
		}
		if(timedout) {
			state.timeout[timeout_button-1]=0;
			switch(state.button_state[timeout_button-1]) {
				case MOUSE_SINGLEPRESSED:
					/* Press event */
					add_outevent(CIOLIB_BUTTON_PRESS(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					break;
				case MOUSE_CLICKED:
					/* Click Event */
					add_outevent(CIOLIB_BUTTON_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					break;
				case MOUSE_DOUBLEPRESSED:
					/* Click event, then press event */
					add_outevent(CIOLIB_BUTTON_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					add_outevent(CIOLIB_BUTTON_PRESS(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					break;
				case MOUSE_DOUBLECLICKED:
					/* Double-click event */
					add_outevent(CIOLIB_BUTTON_DBL_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					break;
				case MOUSE_TRIPLEPRESSED:
					/* Double-click event, then press event */
					add_outevent(CIOLIB_BUTTON_DBL_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					add_outevent(CIOLIB_BUTTON_PRESS(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					break;
				case MOUSE_TRIPLECLICKED:
					/* Triple-click event */
					add_outevent(CIOLIB_BUTTON_TRPL_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					break;
				case MOUSE_QUADPRESSED:
					/* Triple-click evetn then press event */
					add_outevent(CIOLIB_BUTTON_TRPL_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					add_outevent(CIOLIB_BUTTON_PRESS(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					break;
				case MOUSE_QUADCLICKED:
					add_outevent(CIOLIB_BUTTON_QUAD_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1],state.button_x_res[timeout_button-1],state.button_y_res[timeout_button-1]);
					/* Quad click event (This doesn't need a timeout does it? */
					break;
			}
			state.button_state[timeout_button-1]=MOUSE_NOSTATE;
		}
		else {
			struct in_mouse_event *in;

			in=listShiftNode(&state.input);
			if(in==NULL) {
				YIELD();
				continue;
			}
			but=CIOLIB_BUTTON_NUMBER(in->event);
			if (in->x < 0)
				in->x = state.curx;
			if (in->y < 0)
				in->y = state.cury;
			if (in->x_res < 0)
				in->x_res = state.curx_res;
			if (in->y_res < 0)
				in->y_res = state.cury_res;

			switch(CIOLIB_BUTTON_BASE(in->event)) {
				case CIOLIB_MOUSE_MOVE:
					if(in->x==state.curx && in->y==state.cury &&
					    in->x_res==state.curx_res && in->y_res==state.cury_res)
						break;
					add_outevent(CIOLIB_MOUSE_MOVE,in->x,in->y,in->x_res, in->y_res);
					for(but=1;but<=5;but++) {
						switch(state.button_state[but-1]) {
							case MOUSE_NOSTATE:
								if(state.buttonstate & CIOLIB_BUTTON(but)) {
									add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
									add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),in->x,in->y, in->x_res, in->y_res);
									state.button_state[but-1]=MOUSE_DRAGSTARTED;
								}
								break;
							case MOUSE_SINGLEPRESSED:
								add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),in->x,in->y, in->x_res, in->y_res);
								state.button_state[but-1]=MOUSE_DRAGSTARTED;
								break;
							case MOUSE_CLICKED:
								add_outevent(CIOLIB_BUTTON_CLICK(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								state.button_state[but-1]=MOUSE_NOSTATE;
								break;
							case MOUSE_DOUBLEPRESSED:
								add_outevent(CIOLIB_BUTTON_CLICK(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),in->x,in->y, in->x_res, in->y_res);
								state.button_state[but-1]=MOUSE_DRAGSTARTED;
								break;
							case MOUSE_DOUBLECLICKED:
								add_outevent(CIOLIB_BUTTON_DBL_CLICK(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								state.button_state[but-1]=MOUSE_NOSTATE;
								break;
							case MOUSE_TRIPLEPRESSED:
								add_outevent(CIOLIB_BUTTON_DBL_CLICK(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),in->x,in->y, in->x_res, in->y_res);
								state.button_state[but-1]=MOUSE_DRAGSTARTED;
								break;
							case MOUSE_TRIPLECLICKED:
								add_outevent(CIOLIB_BUTTON_TRPL_CLICK(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								state.button_state[but-1]=MOUSE_NOSTATE;
								break;
							case MOUSE_QUADPRESSED:
								add_outevent(CIOLIB_BUTTON_TRPL_CLICK(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),in->x,in->y, in->x_res, in->y_res);
								state.button_state[but-1]=MOUSE_DRAGSTARTED;
								break;
							case MOUSE_DRAGSTARTED:
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),in->x,in->y, in->x_res, in->y_res);
								break;
						}
					}
					break;
				case CIOLIB_BUTTON_1_PRESS:
					state.buttonstate|=1<<(but-1);
					state.knownbuttonstatemask|=1<<(but-1);
					switch(state.button_state[but-1]) {
						case MOUSE_NOSTATE:
							state.button_state[but-1]=MOUSE_SINGLEPRESSED;
							state.button_x[but-1]=in->x;
							state.button_y[but-1]=in->y;
							state.button_x_res[but-1]=in->x_res;
							state.button_y_res[but-1]=in->y_res;
							state.timeout[but-1]=MSEC_CLOCK()+state.click_timeout;
							if(state.timeout[but-1]==0)
								state.timeout[but-1]=1;
							if(state.click_timeout==0)
								state.timeout[but-1]=0;
							if(!more_multies(but,0)) {
								add_outevent(CIOLIB_BUTTON_PRESS(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
								state.button_state[but-1]=MOUSE_NOSTATE;
								state.timeout[but-1]=0;
							}
							// Scroll "buttons"...
							if (but > 3)
								state.button_state[but-1] = MOUSE_NOSTATE;
							break;
						case MOUSE_CLICKED:
							state.button_state[but-1]=MOUSE_DOUBLEPRESSED;
							state.timeout[but-1]=MSEC_CLOCK()+state.click_timeout;
							if(state.timeout[but-1]==0)
								state.timeout[but-1]=1;
							if(state.click_timeout==0)
								state.timeout[but-1]=0;
							break;
						case MOUSE_DOUBLECLICKED:
							state.button_state[but-1]=MOUSE_TRIPLEPRESSED;
							state.timeout[but-1]=MSEC_CLOCK()+state.click_timeout;
							if(state.timeout[but-1]==0)
								state.timeout[but-1]=1;
							if(state.click_timeout==0)
								state.timeout[but-1]=0;
							break;
						case MOUSE_TRIPLECLICKED:
							state.button_state[but-1]=MOUSE_QUADPRESSED;
							state.timeout[but-1]=MSEC_CLOCK()+state.click_timeout;
							if(state.timeout[but-1]==0)
								state.timeout[but-1]=1;
							if(state.click_timeout==0)
								state.timeout[but-1]=0;
							break;
					}
					break;
				case CIOLIB_BUTTON_1_RELEASE:
					state.buttonstate&= ~(1<<(but-1));
					state.knownbuttonstatemask|=1<<(but-1);
					switch(state.button_state[but-1]) {
						case MOUSE_NOSTATE:
							state.button_x[but-1]=in->x;
							state.button_y[but-1]=in->y;
							state.button_x_res[but-1]=in->x_res;
							state.button_y_res[but-1]=in->y_res;
							add_outevent(CIOLIB_BUTTON_RELEASE(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
							break;
						case MOUSE_SINGLEPRESSED:
							state.button_state[but-1]=MOUSE_CLICKED;
							state.timeout[but-1]=more_multies(but,1)?MSEC_CLOCK()+state.multi_timeout:MSEC_CLOCK();
							if(state.timeout[but-1]==0)
								state.timeout[but-1]=1;
							break;
						case MOUSE_DOUBLEPRESSED:
							state.button_state[but-1]=MOUSE_DOUBLECLICKED;
							state.timeout[but-1]=more_multies(but,2)?MSEC_CLOCK()+state.multi_timeout:MSEC_CLOCK();
							if(state.timeout[but-1]==0)
								state.timeout[but-1]=1;
							break;
						case MOUSE_TRIPLEPRESSED:
							state.button_state[but-1]=MOUSE_TRIPLECLICKED;
							state.timeout[but-1]=more_multies(but,3)?MSEC_CLOCK()+state.multi_timeout:MSEC_CLOCK();
							if(state.timeout[but-1]==0)
								state.timeout[but-1]=1;
							break;
						case MOUSE_QUADPRESSED:
							state.button_state[but-1]=MOUSE_NOSTATE;
							add_outevent(CIOLIB_BUTTON_QUAD_CLICK(but),state.button_x[but-1],state.button_y[but-1],state.button_x_res[but-1],state.button_y_res[but-1]);
							state.timeout[but-1]=0;
							if(state.timeout[but-1]==0)
								state.timeout[but-1]=1;
							break;
						case MOUSE_DRAGSTARTED:
							add_outevent(CIOLIB_BUTTON_DRAG_END(but),in->x,in->y, in->x_res, in->y_res);
							state.button_state[but-1]=0;
					}
			}
			state.curx=in->x;
			state.cury=in->y;
			state.curx_res=in->x_res;
			state.cury_res=in->y_res;

			free(in);
		}

		timeout_button=0;
		for(but=1;but<=5;but++) {
			if(state.button_state[but-1]==MOUSE_DRAGSTARTED &&
			    (mouse_events & ((UINT64_C(1)<<CIOLIB_BUTTON_DRAG_START(but)) | (UINT64_C(1)<<CIOLIB_BUTTON_DRAG_MOVE(but)) | (UINT64_C(1)<<CIOLIB_BUTTON_DRAG_END(but)))) == 0)
				state.button_state[but-1] = MOUSE_NOSTATE;
		}

		for(but=1;but<=5;but++) {
			if(state.button_state[but-1]!=MOUSE_NOSTATE 
					&& state.button_state[but-1]!=MOUSE_DRAGSTARTED 
					&& state.timeout[but-1]!=0
					&& (timeout_button==0 || state.timeout[but-1]<ttime)) {
				ttime=state.timeout[but-1];
				timeout_button=but;
			}
		}
	}
}

int mouse_trywait(void)
{
	int	result;

	while(!ciolib_mouse_initialized)
		SLEEP(1);
	while(1) {
		result=listSemTryWait(&state.output);
		pthread_mutex_lock(&unget_mutex);
		if(ungot==0) {
			pthread_mutex_unlock(&unget_mutex);
			return(result);
		}
		ungot--;
		pthread_mutex_unlock(&unget_mutex);
	}
}

int mouse_wait(void)
{
	int result;

	while(!ciolib_mouse_initialized)
		SLEEP(1);
	while(1) {
		result=listSemWait(&state.output);
		pthread_mutex_lock(&unget_mutex);
		if(ungot==0) {
			pthread_mutex_unlock(&unget_mutex);
			return(result);
		}
		ungot--;
		pthread_mutex_unlock(&unget_mutex);
	}
}

int mouse_pending(void)
{
	while(!ciolib_mouse_initialized)
		SLEEP(1);
	return(listCountNodes(&state.output));
}

int ciolib_getmouse(struct mouse_event *mevent)
{
	int retval=0;

	while(!ciolib_mouse_initialized)
		SLEEP(1);
	if(listCountNodes(&state.output)) {
		struct out_mouse_event *out;
		out=listShiftNode(&state.output);
		if(out==NULL)
			return(-1);
		if(mevent != NULL) {
			mevent->event=out->event;
			mevent->bstate=out->bstate;
			mevent->kbsm=out->kbsm;
			mevent->startx=out->startx;
			mevent->starty=out->starty;
			mevent->endx=out->endx;
			mevent->endy=out->endy;
			mevent->startx_res=out->startx_res;
			mevent->starty_res=out->starty_res;
			mevent->endx_res=out->endx_res;
			mevent->endy_res=out->endy_res;
		}
		free(out);
	}
	else {
		fprintf(stderr,"WARNING: attempt to get a mouse key when none pending!\n");
		if(mevent != NULL)
			memset(mevent,0,sizeof(struct mouse_event));
		retval=-1;
	}
	return(retval);
}

int ciolib_ungetmouse(struct mouse_event *mevent)
{
	struct mouse_event *me;

	if((me=(struct mouse_event *)malloc(sizeof(struct mouse_event)))==NULL)
		return(-1);
	memcpy(me,mevent,sizeof(struct mouse_event));
	pthread_mutex_lock(&unget_mutex);
	if(listInsertNode(&state.output,me)==NULL) {
		pthread_mutex_unlock(&unget_mutex);
		return(FALSE);
	}
	ungot++;
	pthread_mutex_unlock(&unget_mutex);
	return(TRUE);
}
