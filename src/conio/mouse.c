#include <genwrap.h>
#include <semwrap.h>
#include <threadwrap.h>

#include "mouse.h"

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

pthread_mutex_t in_mutex;
pthread_mutex_t out_mutex;
sem_t in_sem;

struct in_mouse_event {
	int	event;
	int	x;
	int	y;
	clock_t	ts;
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
	void *nextevent;
};

struct mouse_state {
	int	buttonstate;			/* Current state of all buttons - bitmap */
	int	knownbuttonstatemask;	/* Mask of buttons that have done something since
								 * We started watching... the rest are actually in
								 * an unknown state */
	int	button_state[3];		/* Expanded state of each button */
	int	button_x[3];			/* Start X/Y position of the current state */
	int	button_y[3];
	clock_t	timeout[3];	/* Button event timeouts (timespecs ie: time of expiry) */
	int	curx;					/* Current X position */
	int	cury;					/* Current Y position */
	int	events;					/* Currently enabled events */
	int	click_timeout;			/* Timeout between press and release events for a click (ms) */
	int	multi_timeout;			/* Timeout after a click for detection of multi clicks (ms) */
	int	click_drift;			/* Allowed "drift" during a click event */
	struct in_mouse_event	*events_in;		/* Pointer to recevied events stack */
	struct out_mouse_event	*events_out;	/* Pointer to output events stack */
};

struct mouse_state state;
int mouse_events=0;

void init_mouse(void)
{
	memset(&state,0,sizeof(state));
	state.click_timeout=200;
	state.multi_timeout=300;
	state.events_in=(struct in_mouse_event *)NULL;
	state.events_out=(struct out_mouse_event *)NULL;
	pthread_mutex_init(&in_mutex,NULL);
	pthread_mutex_init(&out_mutex,NULL);
	sem_init(&in_sem,0,0);
}

int ciomouse_setevents(int events)
{
	mouse_events=events;
	return mouse_events;
}

int ciomouse_addevents(int events)
{
	mouse_events |= events;
	return mouse_events;
}

int ciomouse_delevents(int events)
{
	mouse_events &= ~events;
	return mouse_events;
}

void ciomouse_gotevent(int event, int x, int y)
{
	struct in_mouse_event *ime;
	struct in_mouse_event **lastevent;

	ime=(struct in_mouse_event *)malloc(sizeof(struct in_mouse_event));
	ime->ts=msclock();
	ime->event=event;
	ime->x=x;
	ime->y=y;
	ime->nextevent=NULL;

	pthread_mutex_lock(&in_mutex);

	for(lastevent=&state.events_in;*lastevent != NULL;lastevent=&(*lastevent)->nextevent);
	*lastevent=ime;

	pthread_mutex_unlock(&in_mutex);
	sem_post(&in_sem);
}

void add_outevent(int event, int x, int y)
{
	struct out_mouse_event *ome;
	struct out_mouse_event **lastevent;
	int	but=0;

	if(!(mouse_events & 1<<event))
		return;
	ome=(struct out_mouse_event *)malloc(sizeof(struct out_mouse_event));

	but=CIOLIB_BUTTON_NUMBER(event);
	ome->event=event;
	ome->bstate=state.buttonstate;
	ome->kbsm=state.knownbuttonstatemask;
	ome->startx=but?state.button_x[but-1]:state.curx;
	ome->starty=but?state.button_y[but-1]:state.cury;
	ome->endx=x;
	ome->endy=y;
	ome->nextevent=(struct out_mouse_event *)NULL;

	pthread_mutex_lock(&out_mutex);

	for(lastevent=&state.events_out;*lastevent != NULL;lastevent=&(*lastevent)->nextevent);
	*lastevent=ome;

	pthread_mutex_unlock(&out_mutex);
}

void ciolib_mouse_thread(void *data)
{
	struct in_mouse_event *old_in_event;
	int	timedout;
	int timeout_button=0;
	int but;
	int delay;
	clock_t	ttime=0;

	init_mouse();
	while(1) {
		timedout=0;
		if(timeout_button) {
			delay=state.timeout[timeout_button-1]-msclock();
			if(delay<=0) {
				timedout=1;
			}
			else {
				timedout=sem_trywait_block(&in_sem,delay);
			}
		}
		else
			sem_wait(&in_sem);
		if(timedout) {
			state.timeout[timeout_button-1]=0;
			switch(state.button_state[timeout_button-1]) {
				case MOUSE_SINGLEPRESSED:
					/* Press event */
					add_outevent(CIOLIB_BUTTON_PRESS(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					break;
				case MOUSE_CLICKED:
					/* Click Event */
					add_outevent(CIOLIB_BUTTON_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					break;
				case MOUSE_DOUBLEPRESSED:
					/* Click event, then press event */
					add_outevent(CIOLIB_BUTTON_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					add_outevent(CIOLIB_BUTTON_PRESS(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					break;
				case MOUSE_DOUBLECLICKED:
					/* Double-click event */
					add_outevent(CIOLIB_BUTTON_DBL_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					break;
				case MOUSE_TRIPLEPRESSED:
					/* Double-click event, then press event */
					add_outevent(CIOLIB_BUTTON_DBL_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					add_outevent(CIOLIB_BUTTON_PRESS(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					break;
				case MOUSE_TRIPLECLICKED:
					/* Triple-click event */
					add_outevent(CIOLIB_BUTTON_TRPL_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					break;
				case MOUSE_QUADPRESSED:
					/* Triple-click evetn then press event */
					add_outevent(CIOLIB_BUTTON_TRPL_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					add_outevent(CIOLIB_BUTTON_PRESS(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					break;
				case MOUSE_QUADCLICKED:
					add_outevent(CIOLIB_BUTTON_QUAD_CLICK(timeout_button),state.button_x[timeout_button-1],state.button_y[timeout_button-1]);
					/* Quad click event (This doesn't need a timeout does it? */
					break;
			}
			state.button_state[timeout_button-1]=MOUSE_NOSTATE;
		}
		else {
			but=CIOLIB_BUTTON_NUMBER(state.events_in->event);
			switch(CIOLIB_BUTTON_BASE(state.events_in->event)) {
				case CIOLIB_MOUSE_MOVE:
					add_outevent(CIOLIB_MOUSE_MOVE,state.events_in->x,state.events_in->y);
					for(but=1;but<=3;but++) {
						switch(state.button_state[but-1]) {
							case MOUSE_NOSTATE:
								if(state.buttonstate & CIOLIB_BUTTON(but)) {
									add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1]);
									add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),state.events_in->x,state.events_in->y);
									state.button_state[but-1]=MOUSE_DRAGSTARTED;
								}
								break;
							case MOUSE_SINGLEPRESSED:
								add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),state.events_in->x,state.events_in->y);
								state.button_state[but-1]=MOUSE_DRAGSTARTED;
								break;
							case MOUSE_CLICKED:
								add_outevent(CIOLIB_BUTTON_CLICK(but),state.button_x[but-1],state.button_y[but-1]);
								state.button_state[but-1]=MOUSE_NOSTATE;
								break;
							case MOUSE_DOUBLEPRESSED:
								add_outevent(CIOLIB_BUTTON_CLICK(but),state.button_x[but-1],state.button_y[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),state.events_in->x,state.events_in->y);
								state.button_state[but-1]=MOUSE_DRAGSTARTED;
								break;
							case MOUSE_DOUBLECLICKED:
								add_outevent(CIOLIB_BUTTON_DBL_CLICK(but),state.button_x[but-1],state.button_y[but-1]);
								state.button_state[but-1]=MOUSE_NOSTATE;
								break;
							case MOUSE_TRIPLEPRESSED:
								add_outevent(CIOLIB_BUTTON_DBL_CLICK(but),state.button_x[but-1],state.button_y[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),state.events_in->x,state.events_in->y);
								state.button_state[but-1]=MOUSE_DRAGSTARTED;
								break;
							case MOUSE_TRIPLECLICKED:
								add_outevent(CIOLIB_BUTTON_TRPL_CLICK(but),state.button_x[but-1],state.button_y[but-1]);
								state.button_state[but-1]=MOUSE_NOSTATE;
								break;
							case MOUSE_QUADPRESSED:
								add_outevent(CIOLIB_BUTTON_TRPL_CLICK(but),state.button_x[but-1],state.button_y[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_START(but),state.button_x[but-1],state.button_y[but-1]);
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),state.events_in->x,state.events_in->y);
								state.button_state[but-1]=MOUSE_DRAGSTARTED;
								break;
							case MOUSE_DRAGSTARTED:
								add_outevent(CIOLIB_BUTTON_DRAG_MOVE(but),state.events_in->x,state.events_in->y);
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
							state.button_x[but-1]=state.events_in->x;
							state.button_y[but-1]=state.events_in->y;
							state.timeout[but-1]=msclock()+(state.click_timeout*MSCLOCKS_PER_SEC)/1000;
							break;
						case MOUSE_CLICKED:
							state.button_state[but-1]=MOUSE_DOUBLEPRESSED;
							state.timeout[but-1]=msclock()+(state.click_timeout*MSCLOCKS_PER_SEC)/1000;
							break;
						case MOUSE_DOUBLECLICKED:
							state.button_state[but-1]=MOUSE_TRIPLEPRESSED;
							state.timeout[but-1]=msclock()+(state.click_timeout*MSCLOCKS_PER_SEC)/1000;
							break;
						case MOUSE_TRIPLECLICKED:
							state.button_state[but-1]=MOUSE_QUADPRESSED;
							state.timeout[but-1]=msclock()+(state.click_timeout*MSCLOCKS_PER_SEC)/1000;
							break;
					}
					break;
				case CIOLIB_BUTTON_1_RELEASE:
					state.buttonstate&= ~(1<<(but-1));
					state.knownbuttonstatemask|=1<<(but-1);
					switch(state.button_state[but-1]) {
						case MOUSE_NOSTATE:
							state.button_x[but-1]=state.events_in->x;
							state.button_y[but-1]=state.events_in->y;
							add_outevent(CIOLIB_BUTTON_RELEASE(but),state.button_x[but-1],state.button_y[but-1]);
							break;
						case MOUSE_SINGLEPRESSED:
							state.button_state[but-1]=MOUSE_CLICKED;
							state.timeout[but-1]=msclock()+(state.multi_timeout*MSCLOCKS_PER_SEC)/1000;
							break;
						case MOUSE_DOUBLEPRESSED:
							state.button_state[but-1]=MOUSE_DOUBLECLICKED;
							state.timeout[but-1]=msclock()+(state.multi_timeout*MSCLOCKS_PER_SEC)/1000;
							break;
						case MOUSE_TRIPLEPRESSED:
							state.button_state[but-1]=MOUSE_TRIPLECLICKED;
							state.timeout[but-1]=msclock()+(state.multi_timeout*MSCLOCKS_PER_SEC)/1000;
							break;
						case MOUSE_QUADPRESSED:
							state.button_state[but-1]=MOUSE_NOSTATE;
							add_outevent(CIOLIB_BUTTON_QUAD_CLICK(but),state.button_x[but-1],state.button_y[but-1]);
							state.timeout[but-1]=0;
							break;
						case MOUSE_DRAGSTARTED:
							add_outevent(CIOLIB_BUTTON_DRAG_END(but),state.events_in->x,state.events_in->y);
							state.button_state[but-1]=0;
					}
			}
			state.curx=state.events_in->x;
			state.cury=state.events_in->y;

			pthread_mutex_lock(&in_mutex);
			old_in_event=state.events_in;
			state.events_in=state.events_in->nextevent;
			free(old_in_event);
			pthread_mutex_unlock(&in_mutex);
		}

		ttime=-1;
		timeout_button=0;
		for(but=1;but<=3;but++) {
			if(state.button_state[but-1]!=MOUSE_NOSTATE && state.button_state[but-1]!=MOUSE_DRAGSTARTED && state.timeout[but-1]<ttime) {
				ttime=state.timeout[but-1];
				timeout_button=but;
			}
		}
	}
}

int mouse_pending(void)
{
	return(state.events_out!=NULL);
}

int ciolib_getmouse(struct mouse_event *mevent)
{
	int retval=0;
	struct out_mouse_event *old_out_event;

	pthread_mutex_lock(&out_mutex);

	if(state.events_out!=NULL) {
		mevent->event=state.events_out->event;
		mevent->bstate=state.events_out->bstate;
		mevent->kbsm=state.events_out->kbsm;
		mevent->startx=state.events_out->startx;
		mevent->starty=state.events_out->starty;
		mevent->endx=state.events_out->endx;
		mevent->endy=state.events_out->endy;
		old_out_event=state.events_out;
		state.events_out=state.events_out->nextevent;
		free(old_out_event);
	}
	else
		retval=-1;
	pthread_mutex_unlock(&out_mutex);
	return(retval);
}
