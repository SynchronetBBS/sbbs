/* uifcfltk.c */

/* X/Windows Implementation of UIFC (user interface) library */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
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

/*********************************************/
/* UIFC Defines specific to the FLTK version */
/*********************************************/
#define	MAX_POPUPS			10		// Maximum number of popup()s
#define MAX_WINDOWS			100		// Maximum number of list()s
#define UIFC_CANCEL			-1		// Return value for Cancel
#define UIFC_MENU			-2		// Keep looping value
#define UIFC_INPUT			-3		// Input Box
#define UIFC_HELP			-4		// Return value for Help
#define UIFC_RCLICK			-5		// Right-click

/***********************
 * Fast Light Includes *
 ***********************/
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input_.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>		// fl_beep(), fl_message()

/******************/
/* Layout Defines */
/******************/
#define UIFC_CHAR_WIDTH			8		// Width of one "Character" (Assumes 80x25)
#define UIFC_CHAR_HEIGHT		12	// Height of one Character
#define UIFC_LINE_HEIGHT		16	// Height of one "Line"
#define UIFC_BORDER_WIDTH		2	// Width of a single border
#define UIFC_SCROLL_WIDTH		18	// Width of the scroll bar
#define UIFC_MAIN_BG_COLOR			FL_DARK_CYAN
#define UIFC_BORDER_COLOR			FL_YELLOW
#define UIFC_BUTTON_TEXT_COLOR		FL_WHITE
#define UIFC_BUTTON_COLOR			FL_DARK_BLUE
#define UIFC_BUTTON_SELECTED_COLOR  FL_BLUE
#define UIFC_WINDOW_TITLE_COLOR		FL_YELLOW
#define UIFC_INPUT_WIN_BG_COLOR		FL_DARK_BLUE
#define UIFC_INPUT_TEXT_COLOR		FL_WHITE
#define UIFC_INPUT_BOX_BG_COLOR		FL_DARK_BLUE
#define UIFC_INPUT_PROMPT_COLOR		FL_YELLOW
#define UIFC_HELP_PLAIN_COLOR		FL_WHITE
#define UIFC_HELP_HIGH_COLOR		FL_YELLOW
#define UIFC_HELP_INVERSE_COLOR		FL_CYAN
#define UIFC_HELP_BOTH_COLOR		FL_MAGENTA
#define UIFC_HELP_WIN_BG_COLOR		FL_DARK_BLUE
#define UIFC_LIST_WIN_BG_COLOR		FL_DARK_BLUE
#define UIFC_FOREGROUND				255,255,0
#define UIFC_BACKGROUND				0,64,64
#define UIFC_BACKGROUND2			0,0,255


#include <sys/types.h>
#include "uifc.h"

#ifdef __unix__
#include <unistd.h>
#else
#include <stdio.h>
#endif

static char *helpfile=0;
static uint helpline=0;
static uifcapi_t* api;

/* Typedefs */
typedef struct {
	int mode;
	int max;
	Fl_Input_	*InputBox;
} modes_t;

/* Prototypes */
static void help(void);
static void GenCallback(Fl_Widget *, void *);
static void LBCallback(Fl_Widget *, void *);

/* API routines */
static void uifcbail(void);
static int uscrn(char *str);
static int ulist(int mode, int left, int top, int width, int *dflt, int *bar
	,char *title, char **option);
static int uinput(int imode, int left, int top, char *prompt, char *str
	,int len ,int kmode);
static void umsg(char *str);
static void upop(char *str);
static void sethelp(int line, char* file);

/* Classes */
class UIFC_PopUp : public Fl_Double_Window  {
	int handle(int);
public:
	UIFC_PopUp(int w,int h,const char *title) : Fl_Double_Window(w,h,title) {
		border(FALSE);
		box(FL_UP_BOX);
		set_modal();
	}
};

class UIFC_Button : public Fl_Button  {
	char uifc_label[MAX_OPLN+1];
public:
	int retval;
	int mode;
	int handle(int);
	UIFC_Button(int x,int y,int w,int h,const char *label) : Fl_Button(x,y,w,h,label) {
		if(label==NULL)
			strcpy(uifc_label,"");
		else
			strcpy(uifc_label,label);
		this->label(uifc_label);
		labelfont(FL_COURIER);
		labelsize(UIFC_CHAR_HEIGHT);
		when(FL_WHEN_RELEASE|FL_WHEN_CHANGED);
		box(FL_FLAT_BOX);
		if(!(api->mode&UIFC_MONO))  {
			color(UIFC_BUTTON_COLOR,UIFC_BUTTON_SELECTED_COLOR);
			labelcolor(UIFC_BUTTON_TEXT_COLOR);
		}
	}
};

class UIFC_Window : public Fl_Double_Window  {
public:
	UIFC_Window(int x,int y,int w,int h,const char *title) : Fl_Double_Window(x,y,w,h,title) {
		box(FL_UP_BOX);
		if(!(api->mode&UIFC_MONO))
			color(UIFC_LIST_WIN_BG_COLOR);
		UIFC_Button *HButton = new UIFC_Button(
			w-(UIFC_LINE_HEIGHT*2)+UIFC_BORDER_WIDTH*3,
			UIFC_BORDER_WIDTH,
			UIFC_LINE_HEIGHT-(UIFC_BORDER_WIDTH*2),
			UIFC_LINE_HEIGHT-(UIFC_BORDER_WIDTH*2),
			"?");
		HButton->box(FL_NO_BOX);
		HButton->labelfont(FL_SCREEN);
		HButton->labelsize(UIFC_LINE_HEIGHT-(UIFC_BORDER_WIDTH*4));
		HButton->callback(GenCallback);
		HButton->retval=UIFC_HELP;
		HButton->when(FL_WHEN_CHANGED);
		if(!(api->mode&UIFC_MONO))  {
			HButton->color(FL_BLUE,FL_BLUE);
			HButton->labelcolor(UIFC_WINDOW_TITLE_COLOR);
		}
		UIFC_Button *CButton = new UIFC_Button(
			w-(UIFC_LINE_HEIGHT)+UIFC_BORDER_WIDTH,
			UIFC_BORDER_WIDTH,
			UIFC_LINE_HEIGHT-(UIFC_BORDER_WIDTH*2),	
			UIFC_LINE_HEIGHT-(UIFC_BORDER_WIDTH*2),
			"X");
		CButton->box(FL_NO_BOX);
		CButton->labelfont(FL_SCREEN);
		CButton->labelsize(UIFC_LINE_HEIGHT-(UIFC_BORDER_WIDTH*4));
		CButton->callback(GenCallback);
		CButton->retval=UIFC_CANCEL;
		CButton->when(FL_WHEN_CHANGED);
		if(!(api->mode&UIFC_MONO))  {
			CButton->color(FL_BLUE,FL_BLUE);
			CButton->labelcolor(UIFC_WINDOW_TITLE_COLOR);
		}
	}
};

class UIFC_Menu : public UIFC_Window  {
public:
	int *uifc_id;
	UIFC_Menu(int x,int y,int w,int h,const char *title,int mode,int opts,int *cur,char **option) : UIFC_Window(x,y,w,h,title)  {
		UIFC_Button *Button;
		Fl_Scroll *SGroup;
		int opt=0;
		int scrolloffset=0;
		
		uifc_id=cur;
		if(opts>20)
			scrolloffset=UIFC_SCROLL_WIDTH;

		SGroup = new Fl_Scroll(
				UIFC_CHAR_WIDTH-UIFC_BORDER_WIDTH,
				UIFC_LINE_HEIGHT-UIFC_BORDER_WIDTH,
				w-(2*UIFC_CHAR_WIDTH)+(UIFC_BORDER_WIDTH*2),
				UIFC_LINE_HEIGHT*(((opts<20)?opts:20))+(UIFC_BORDER_WIDTH*4),
				title);
		SGroup->box(FL_DOWN_BOX);
		SGroup->labelfont(FL_COURIER_BOLD);
		SGroup->labelsize(UIFC_CHAR_HEIGHT);
		if(!(api->mode&UIFC_MONO))  {
			SGroup->labelcolor(UIFC_WINDOW_TITLE_COLOR);
			SGroup->color(UIFC_BORDER_COLOR);
		}
		for(opt=0;option[opt][0];opt++)  {
			Button=new UIFC_Button(
					UIFC_CHAR_WIDTH+UIFC_BORDER_WIDTH,
					UIFC_LINE_HEIGHT*(opt+1)+UIFC_BORDER_WIDTH,
					w-(2*UIFC_CHAR_WIDTH)-(2*UIFC_BORDER_WIDTH)-scrolloffset,
					UIFC_LINE_HEIGHT,
					option[opt]);
			Button->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_CENTER);
			Button->callback(LBCallback);
			Button->retval=opt;
			Button->mode=mode;
		}
		if((mode&WIN_XTR)&&(mode&WIN_INS))  {
			Button=new UIFC_Button(
					UIFC_CHAR_WIDTH+UIFC_BORDER_WIDTH,
					UIFC_LINE_HEIGHT*(opt+1)+UIFC_BORDER_WIDTH,
					w-(2*UIFC_CHAR_WIDTH)-(2*UIFC_BORDER_WIDTH)-scrolloffset,
					UIFC_LINE_HEIGHT,
					NULL);
			Button->callback(LBCallback);
			Button->retval=opt|MSK_INS;
			Button->mode=mode;
			opt++;
		}
	}
};

class UIFC_Input_Box : public Fl_Input_  {
	int handle(int);
	void draw();
	int handle_key();
	int shift_position(int);
public:
	int mode;
	int max;
	UIFC_Input_Box(int x,int y,int w,int h,const char *title,int inmode,int inmax,char *outval) : Fl_Input_(x,y,w,h,title)  {
		mode=inmode;
		max=inmax;
		if(mode&K_EDIT)
			value(outval);
		else
			value("");
		position(strlen(value()));
		maximum_size(max);
		if(!(api->mode&UIFC_MONO))  {
			labelcolor(UIFC_INPUT_PROMPT_COLOR);
			color(UIFC_INPUT_BOX_BG_COLOR);
			textcolor(UIFC_INPUT_TEXT_COLOR);
		}
	}
};

class UIFC_Input : public UIFC_Window  {
public:
	char invalue[256];
	UIFC_Input(int x,int y,int w,int h,const char *title,int inmode,int inmax,int len,int width,char *outval) : UIFC_Window(x,y,w,h,title)  {
		box(FL_UP_BOX);
		new UIFC_Input_Box(
				UIFC_CHAR_WIDTH*(len+2),
				UIFC_LINE_HEIGHT,
				(width)*UIFC_CHAR_WIDTH,
				UIFC_LINE_HEIGHT,
				title,
				inmode,
				inmax,
				outval);
		int button_left=w/2-(UIFC_CHAR_WIDTH*10);
		UIFC_Button	*OkButton = new UIFC_Button(
				button_left,
				UIFC_LINE_HEIGHT*3,
				(8)*UIFC_CHAR_WIDTH,
				UIFC_LINE_HEIGHT,
				"Ok");
		OkButton->callback(GenCallback);
		OkButton->retval=0;
		UIFC_Button 	*CancelButton=new UIFC_Button(
				button_left+UIFC_CHAR_WIDTH*10,
				UIFC_LINE_HEIGHT*3,
				(10)*UIFC_CHAR_WIDTH
				,UIFC_LINE_HEIGHT
				,"Cancel");
		CancelButton->retval=UIFC_CANCEL;
		CancelButton->shortcut(FL_Escape);
		CancelButton->callback(GenCallback);
		if(inmode&K_EDIT)
			snprintf(invalue,inmax,"%s",outval);
		else
			invalue[0]=0;
	}
};

/* Globals */
Fl_Double_Window	*MainWin;
int			GUI_RetVal;
UIFC_Menu *Windows[MAX_WINDOWS];	// Array of windows for ulist
int CurrWin;				// Current window in array.
int *uifc_cur=NULL;				// Current selection.

/*****************************/
/* UIFC_Button ENTER catcher */
/*****************************/
int UIFC_Button::handle(int event)  {
	if (event == FL_FOCUS)  {
		if(uifc_cur != NULL)
			*uifc_cur=retval&MSK_OFF;

		if(!(api->mode&UIFC_MONO))
			color(UIFC_BUTTON_SELECTED_COLOR);
		Fl_Scroll *s=(Fl_Scroll *)parent();
		/******************************************************************************
		 * This works, it will always work if there is vertical room for at least one *
		 * button in the scroll box.  For gods sake, don't touch this unless you KNOW *
         * how it works, and can do it better.										  *
		 ******************************************************************************/
		if(s->type()==Fl_Scroll::BOTH)  {
			if(y() + h() + Fl::box_dy(box()) + Fl::box_dh(box()) > s->y()+Fl::box_dy(s->box())+s->h()-Fl::box_dh(s->box()))  {
				// Scroll Down
				s->position(0,s->yposition()-(s->y() + Fl::box_dy(s->box()) + s->h() - Fl::box_dh(s->box()) - y() - h() - Fl::box_dy(box()) - Fl::box_dh(box())));
			}
			if(y() < s->y()+Fl::box_dy(s->box()))  {
				// Scroll Up
				s->position(0,s->yposition()-(s->y() + Fl::box_dy(s->box()) - y()));
			}
		}
	}
	else if (event == FL_UNFOCUS)  {
		if(!(api->mode&UIFC_MONO))
			color(UIFC_BUTTON_COLOR);
	}
	else if (event == FL_KEYBOARD)  {
		int key=Fl::event_key();
		int i;
		if (key == FL_Enter || key == FL_KP_Enter) {
			do_callback();
			return 1;
		}
		if(key==FL_Page_Up)  {
			for(i=0;i<parent()->children() && this!=parent()->child(i);i++)  {}
			i=i-((parent()->h() - Fl::box_dh(parent()->box()))/UIFC_LINE_HEIGHT)+1;
			if(i<0)
				i=0;
			Fl::focus(parent()->child(i));
			parent()->child(i)->handle(FL_FOCUS);
			return(1);
		}
		if(key==FL_Page_Down)  {
			for(i=0;i<parent()->children() && this!=parent()->child(i);i++)  {}
			i=i+((parent()->h() - Fl::box_dh(parent()->box()))/UIFC_LINE_HEIGHT)-1;
			if(i>parent()->children()-3)
				i=parent()->children()-3;
			Fl::focus(parent()->child(i));
			parent()->child(i)->handle(FL_FOCUS);
			return(1);
		}
		if(key==FL_Up)  {
			for(i=0;i<parent()->children() && this!=parent()->child(i);i++)  {}
			i=i-1;
			if(i<0)
				i=parent()->children()-3;
			Fl::focus(parent()->child(i));
			parent()->child(i)->handle(FL_FOCUS);
			return(1);
		}
		if(key==FL_Down)  {
			for(i=0;i<parent()->children() && this!=parent()->child(i);i++)  {}
			i=i+1;
			if(i>parent()->children()-3)
				i=0;
			Fl::focus(parent()->child(i));
			parent()->child(i)->handle(FL_FOCUS);
			return(1);
		}
	}
	return Fl_Button::handle(event);
}

/**********************************/
/* right-click menu event handler */
/**********************************/
int UIFC_PopUp::handle(int event)  {
	int i;

	if(event == FL_SHORTCUT && Fl::test_shortcut(FL_Escape))  {
		GUI_RetVal=UIFC_CANCEL;
		return(1);
	}
	if(event==FL_PUSH)  {
		for(i=0;i<children();i++)  {
			if(Fl::event_inside(child(i)))  {
				GUI_RetVal=((UIFC_Button *)child(i))->retval;
				return(1);
			}
		}
		GUI_RetVal=UIFC_CANCEL;
		return(1);
	}
	return(0);
}

/*******************/
/* Input Box stuff */
/*******************/

void UIFC_Input_Box::draw() {
  if (input_type() == FL_HIDDEN_INPUT) return;
  Fl_Boxtype b = box();
  if (damage() & FL_DAMAGE_ALL) draw_box(b, color());
  Fl_Input_::drawtext(x()+Fl::box_dx(b), y()+Fl::box_dy(b),
		      w()-Fl::box_dw(b), h()-Fl::box_dh(b));
}

// kludge so shift causes selection to extend:
int UIFC_Input_Box::shift_position(int p) {
  return position(p, Fl::event_state(FL_SHIFT) ? mark() : p);
}

#define ctrl(x) (x^0x40)
int UIFC_Input_Box::handle_key() {
	char ascii = Fl::event_text()[0];
	int del;

	if (Fl::compose(del)) {
		// Insert characters into numeric fields after checking for legality:
		if (mode&K_NUMBER) {
			Fl::compose_reset(); // ignore any foreign letters...
			// This is complex to allow "0xff12" hex to be typed:
			if (ascii >= '0' && ascii <= '9') {
				if (readonly())
					fl_beep();
				else
					replace(position(), mark(), &ascii, 1);
      		}
			strcpy(((UIFC_Input *)parent())->invalue,value());
			return 1;
		}

		if (del || Fl::event_length()) {
			if (readonly())
				fl_beep();
			else
				replace(position(), del ? position()-del : mark(),
						Fl::event_text(), Fl::event_length());
		}
		strcpy(((UIFC_Input *)parent())->invalue,value());
		return 1;
	}

	switch (Fl::event_key()) {
		case FL_Insert:
			if (Fl::event_state() & FL_CTRL)
				ascii = ctrl('C');
			else
				if (Fl::event_state() & FL_SHIFT)
					ascii = ctrl('V');
			break;
		case FL_Delete:
			if (Fl::event_state() & FL_SHIFT)
				ascii = ctrl('X');
    		else
				ascii = ctrl('D');
			break;    
		case FL_Left:
			return shift_position(position()-1) + 1;
			break;
		case FL_Right:
			ascii = ctrl('F');
			break;
		case FL_Home:
			if (Fl::event_state() & FL_CTRL) {
				shift_position(0);
				return 1;
			}
			return shift_position(line_start(position())) + 1;
			break;
		case FL_End:
			if (Fl::event_state() & FL_CTRL) {
				shift_position(size());
				return 1;
			}
			ascii = ctrl('E'); break;
		case FL_BackSpace:
			ascii = ctrl('H');
			break;
		case FL_Enter:
		case FL_KP_Enter:
			GUI_RetVal=0;
			return 1;
	}

	int i;
	switch (ascii) {
		case ctrl('C'): // copy
			return copy(1);
		case ctrl('D'):
		case ctrl('?'):
			if (readonly()) {
				fl_beep();
				return 1;
			}
			if (mark() != position())  {
				i=cut();
				strcpy(((UIFC_Input *)parent())->invalue,value());
				return i;
			}
			else  {
				i=cut(1);
				strcpy(((UIFC_Input *)parent())->invalue,value());
				return i;
			}
		case ctrl('E'):
			return shift_position(line_end(position())) + 1;
		case ctrl('F'):
			return shift_position(position()+1) + 1;
		case ctrl('H'):
			if (readonly()) {
				fl_beep();
				return 1;
			}
			if (mark() != position())  {
				i=cut();
				strcpy(((UIFC_Input *)parent())->invalue,value());
			}
			else  {
				i=cut(-1);
				strcpy(((UIFC_Input *)parent())->invalue,value());
			}
    		return i;
		case ctrl('K'):
			if (readonly()) {
				fl_beep();
				return 1;
			}
			if (position()>=size()) return 0;
			i = line_end(position());
			if (i == position() && i < size())
				i++;
			cut(position(), i);
			i=copy_cuts();
			strcpy(((UIFC_Input *)parent())->invalue,value());
			return i;
		case ctrl('U'):
			if (readonly()) {
				fl_beep();
				return 1;
			}
			i=cut(0, size());
			strcpy(((UIFC_Input *)parent())->invalue,value());
			return i;
		case ctrl('V'):
		case ctrl('Y'):
			if (readonly()) {
				fl_beep();
				return 1;
			}
			Fl::paste(*this, 1);
			strcpy(((UIFC_Input *)parent())->invalue,value());
			return 1;
		case ctrl('X'):
		case ctrl('W'):
			if (readonly()) {
				fl_beep();
				return 1;
			}
			copy(1);
			i=cut();
			strcpy(((UIFC_Input *)parent())->invalue,value());
			return i;
		case ctrl('Z'):
		case ctrl('_'):
			if (readonly()) {
				fl_beep();
				return 1;
			}
			i=undo();
			strcpy(((UIFC_Input *)parent())->invalue,value());
			return i;
		case ctrl('A'):
		case ctrl('G'):
			if (readonly()) {
				fl_beep();
				return 1;
			}
		    // insert a few selected control characters literally:
			if (!(mode&K_NUMBER) && (mode&K_MSG))  {
				i=replace(position(), mark(), &ascii, 1);
				strcpy(((UIFC_Input *)parent())->invalue,value());
				return i;
			}
	}
	return 0;
}

int UIFC_Input_Box::handle(int event) {
	static int drag_start = -1;
	switch (event) {
		case FL_FOCUS:
			switch (Fl::event_key()) {
				case FL_Right:
					position(0);
					break;
				case FL_Left:
					position(size());
					break;
				case FL_Down:
					up_down_position(0);
					break;
				case FL_Up:
					up_down_position(line_start(size()));
					break;
				case FL_Tab:
				case 0xfe20: // XK_ISO_Left_Tab
					position(size(),0);
					break;
				default:
					position(position(),mark());// turns off the saved up/down arrow position
					break;
			}
			break;

		case FL_KEYBOARD:
			if (Fl::event_key() == FL_Tab && mark() != position()) {
				// Set the current cursor position to the end of the selection...
				if (mark() > position())
					position(mark());
				else
					position(position());
				return (1);
			}
			else
			return handle_key();
		case FL_PUSH:
			if (Fl::focus() != this) {
				Fl::focus(this);
				handle(FL_FOCUS);
			}
			break;

		case FL_RELEASE:
			if (Fl::event_button() == 2) {
				Fl::event_is_click(0); // stop double click from picking a word
				Fl::paste(*this, 0);
			}
			else if (!Fl::event_is_click()) {
				// copy drag-selected text to the clipboard.
				copy(0);
			}
			else if (Fl::event_is_click() && drag_start >= 0) {
				// user clicked in the field and wants to reset the cursor position...
				position(drag_start, drag_start);
				drag_start = -1;
			}
			// For output widgets, do the callback so the app knows the user
			// did something with the mouse...
			if (readonly())
				do_callback();
			return 1;

	}
	Fl_Boxtype b = box();
	return Fl_Input_::handletext(event,
			x()+Fl::box_dx(b), y()+Fl::box_dy(b),
			w()-Fl::box_dw(b), h()-Fl::box_dh(b));
}

/*********************/
/* top level handler */
/*********************/
static int handle_escape(int event)  {
	UIFC_Button *w=(UIFC_Button *)Fl::focus();
	int i,j,key;

	if(event == FL_SHORTCUT && Fl::test_shortcut(FL_Escape))  {
		GUI_RetVal=UIFC_CANCEL;
		return(1);
	}
	if(event == FL_SHORTCUT && Fl::focus()->type()==FL_NORMAL_BUTTON)  {
		int mode=((UIFC_Button *)Fl::focus())->mode;
		key=Fl::event_key();
		if(key==FL_Insert && (mode&WIN_INS))  {
			GUI_RetVal=w->retval|MSK_INS;
			return(1);
		}
		if(key==FL_Delete && (mode&WIN_DEL))  {
			GUI_RetVal=w->retval|MSK_DEL;
			return(1);
		}
		if(key==FL_F+5 && (mode&WIN_GET))  {
			GUI_RetVal=w->retval|MSK_GET;
			return(1);
		}
		if(key==FL_F+6 && (mode&WIN_PUT))  {
			GUI_RetVal=w->retval|MSK_PUT;
			return(1);
		}
		if(key==FL_F+1)  {
			GUI_RetVal=UIFC_HELP;
			return(1);
		}
		if((key>='a' && key<='z')||(key>='0' && key<='9'))  {
			for(i=0;i<w->parent()->children() && w!=w->parent()->child(i);i++)  {}
			if(i!=w->parent()->children())  {
				j=i;
				for(i++;i!=j;i++)  {
					if(i>=w->parent()->children())
						i=0;
					if(w->parent()->child(i)->type()==FL_NORMAL_BUTTON
							&& w->parent()->child(i)!=NULL
							&& w->parent()->child(i)->label()!=NULL)  {
						if(toupper(key)==toupper(*(char *)w->parent()->child(i)->label()))  {
							Fl::focus(w->parent()->child(i));
							w->parent()->child(i)->handle(FL_FOCUS);
							return(1);
						}
					}
				}
			}
		}
	}
	return(0);
}

/****************************************************************************/
/* Initialization function, see uifc.h for details.							*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uifcinifltk(uifcapi_t* uifcapi)
{
    if(uifcapi==NULL || uifcapi->size!=sizeof(uifcapi_t))
        return(-1);

    api=uifcapi;

    /* install function handlers */
    api->bail=uifcbail;
    api->scrn=uscrn;
    api->msg=umsg;
    api->pop=upop;
    api->list=ulist;
    api->input=uinput;
    api->sethelp=sethelp;

    api->scrn_len=24;
	api->mode |= UIFC_MOUSE;

//	Fl::scheme("plastic");
	Fl::add_handler(handle_escape);
	Fl::visible_focus(TRUE);
	Fl::visual(FL_DOUBLE|FL_INDEX);
	if(!(api->mode&UIFC_MONO))  {
		Fl::foreground(UIFC_FOREGROUND);
		Fl::background(UIFC_BACKGROUND);
		Fl::background2(UIFC_BACKGROUND2);
	}
	Windows[0]=NULL;

	CurrWin=0;
	MainWin=NULL;
    return(0);
}

/****************************************************************************/
/* Exit/uninitialize UIFC implementation.									*/
/****************************************************************************/
void uifcbail(void)
{
	delete MainWin;
}

/****************************************************************************/
/* Clear screen, fill with background attribute, display application title.	*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uscrn(char *str)
{
	if(MainWin==NULL)  {
		MainWin=new Fl_Double_Window(UIFC_CHAR_WIDTH*80,UIFC_LINE_HEIGHT*25,str);
		if(!(api->mode&UIFC_MONO))
			MainWin->color(UIFC_MAIN_BG_COLOR,UIFC_MAIN_BG_COLOR);
	}
	MainWin->show();
	return(0);
}

/**********************************/
/* Delete window and all children */
/**********************************/
void delwin(int WinNum)  {

	if(WinNum<0)
		return;
	if(Windows[WinNum]!=NULL)  {
		MainWin->remove(Windows[WinNum]);
		delete Windows[WinNum];
		Windows[WinNum]=NULL;
	}
}


/********************/
/* Generic callback */
/********************/
static void GenCallback(Fl_Widget *w, void *data)  {
	GUI_RetVal=((UIFC_Button *)w)->retval;
}

/*****************/
/* NULL callback */
/*****************/
static void NULLCallback(int i, void *data)  {
}

/***************/
/* Pop up menu */
/***************/
static void doPopUp(int mode)  {
	int height=0;
	int width=3;
	int curry=0;
	Fl_Widget *w=Fl::pushed();
	if(w->type()==FL_NORMAL_BUTTON)  {
		((Fl_Button *)w)->value(0);
		if(!(api->mode&UIFC_MONO))
			w->color(UIFC_BUTTON_SELECTED_COLOR);
	}

	// Right Click
	if(mode&WIN_INS)  {
		height++;
		width=7;
	}
	if(mode&WIN_DEL)  {
		height++;
		width=7;
	}
	if(mode&WIN_GET)  {
		height++;
	}
	if(mode&WIN_PUT)  {
		height++;
	}
	if(height)  {
		UIFC_PopUp *PopUp=new UIFC_PopUp(
				UIFC_CHAR_WIDTH*width+UIFC_BORDER_WIDTH*2,
				UIFC_LINE_HEIGHT*height+UIFC_BORDER_WIDTH*2,
				"PopUp");
		PopUp->position(Fl::event_x_root()+3,Fl::event_y_root()+3);
		if(mode&WIN_INS)  {
			UIFC_Button *InsertButton=new UIFC_Button(
					UIFC_BORDER_WIDTH,
					UIFC_LINE_HEIGHT*curry+UIFC_BORDER_WIDTH,
					UIFC_CHAR_WIDTH*width,
					UIFC_LINE_HEIGHT,
					"Insert");
			InsertButton->retval=((UIFC_Button *)w)->retval|MSK_INS;
			InsertButton->mode=0;
			InsertButton->box(FL_NO_BOX);
			curry++;
		}
		if(mode&WIN_DEL)  {
			UIFC_Button *DeleteButton=new UIFC_Button(
					UIFC_BORDER_WIDTH,
					UIFC_LINE_HEIGHT*curry+UIFC_BORDER_WIDTH,
					UIFC_CHAR_WIDTH*width,
					UIFC_LINE_HEIGHT,
					"Delete");
			DeleteButton->retval=((UIFC_Button *)w)->retval|MSK_DEL;
			DeleteButton->mode=0;
			DeleteButton->box(FL_NO_BOX);
			curry++;
		}
		if(mode&WIN_GET)  {
			UIFC_Button *GetButton=new UIFC_Button(
					UIFC_BORDER_WIDTH,
					UIFC_LINE_HEIGHT*curry+UIFC_BORDER_WIDTH,
					UIFC_CHAR_WIDTH*width,
					UIFC_LINE_HEIGHT,
					"Get");
			GetButton->retval=((UIFC_Button *)w)->retval|MSK_GET;
			GetButton->mode=0;
			GetButton->box(FL_NO_BOX);
			curry++;
		}
		if(mode&WIN_PUT)  {
			UIFC_Button *PutButton=new UIFC_Button(
					UIFC_BORDER_WIDTH,
					UIFC_LINE_HEIGHT*curry+UIFC_BORDER_WIDTH,
					UIFC_CHAR_WIDTH*width,
					UIFC_LINE_HEIGHT,
					"Put");
			PutButton->retval=((UIFC_Button *)w)->retval|MSK_PUT;
			PutButton->mode=0;
			PutButton->box(FL_NO_BOX);
			curry++;
		}
		PopUp->end();
		PopUp->show();
		PopUp->show();
		if(w->type()==FL_NORMAL_BUTTON)  {
			if(!(api->mode&UIFC_MONO))
				w->color(UIFC_BUTTON_SELECTED_COLOR);
			w->redraw();
		}
		Fl::grab(PopUp);
		while(GUI_RetVal==UIFC_RCLICK)  {
			Fl::wait();
		}
		Fl::grab(0);
		delete PopUp;
	}
	return;
}

/************************/
/* List button Callback */
/************************/
static void LBCallback(Fl_Widget *w, void *data)  {
	((Fl_Button *)w)->value(1);
	if(uifc_cur != NULL)
		*uifc_cur=((UIFC_Button *)w)->retval&MSK_OFF;
	if(Fl::event_key()==FL_Button+3)  {
		Fl::focus(w);
		w->handle(FL_FOCUS);
		GUI_RetVal=UIFC_RCLICK;
		return;
	}
	GUI_RetVal=((UIFC_Button *)w)->retval;
}	

/****************************************************************************/
/* General menu function, see uifc.h for details.							*/
/****************************************************************************/
int ulist(int mode, int left, int top, int width, int *cur, int *bar
	, char *title, char **option)
{
	int opts,i;
	int len;
	int scrolloffset=0;

	// Find WinID
	for(i=CurrWin;(i>=0)&&(Windows[i]==NULL || Windows[i]->uifc_id!=cur);i--) {}
	if(i>=0)  {
		for(;CurrWin>=i;CurrWin--)
			delwin(CurrWin);

	}
	CurrWin++;
	delwin(CurrWin);

	len=strlen(title)+6;
	if(width<len)
		width=len;
	for(opts=0;option[opts][0];opts++) {
		len=strlen(option[opts])+4;
		if(width<len)
			width=len;
	}
	if(mode&WIN_XTR)
		opts+=1;
	if(*cur>=opts)
		*cur=opts-1;
	if(*cur<0)
		*cur=0;

	if(opts>20)
		scrolloffset=UIFC_SCROLL_WIDTH;

	GUI_RetVal=UIFC_MENU;
	if(top==0)
		top=SCRN_TOP;
	if(left==0)
		left=SCRN_LEFT;
	if(mode&WIN_RHT)
		left=SCRN_RIGHT-width;
	while(left+width>79)
		left--;
	if(left<=0 || (mode&WIN_L2R))
		left=(80-width)/2;
	if(mode&WIN_BOT)
		top=23;
	while (top+((opts<20)?opts:20)>22)
		top--;
	if (mode&WIN_T2B)
		top=0;
	if(top<=0)
		top=(23-((opts<20)?opts:20))/2;

	Windows[CurrWin] = new UIFC_Menu(
			UIFC_CHAR_WIDTH*left-UIFC_BORDER_WIDTH-(scrolloffset/2),
			UIFC_LINE_HEIGHT*top-UIFC_BORDER_WIDTH,
			UIFC_CHAR_WIDTH*width+(UIFC_BORDER_WIDTH*2)+scrolloffset,
			UIFC_LINE_HEIGHT*(((opts<20)?opts:20)+1)+(UIFC_BORDER_WIDTH*6),
			title,
			mode,
			opts,
			cur,
			option);
	MainWin->add(Windows[CurrWin]);
	Windows[CurrWin]->end();
	Windows[CurrWin]->show();
	Windows[CurrWin]->show();
	/* This is kind of kludgy... it doesn't allow for adding of buttons to
	   the UIFC_Window before the Scroll Group */
	Fl::focus(((UIFC_Menu *)Windows[CurrWin]->child(2))->child(*cur));
	if(!(api->mode&UIFC_MONO))
		((UIFC_Menu *)Windows[CurrWin]->child(2))->child(*cur)->color(UIFC_BUTTON_SELECTED_COLOR);

	uifc_cur=cur;
	while(GUI_RetVal==UIFC_MENU)  {
		Fl::wait();
		if(GUI_RetVal==UIFC_HELP)  {
			help();
			GUI_RetVal=UIFC_MENU;
		}
		else if(GUI_RetVal==UIFC_RCLICK)  {
			doPopUp(mode);
			if(GUI_RetVal==UIFC_CANCEL)
				GUI_RetVal=UIFC_MENU;
		}
	}
	uifc_cur=NULL;
		
	Windows[CurrWin]->deactivate();
	return GUI_RetVal;
}

/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, int left, int top, char *prompt, char *outstr,
	int max, int kmode)
{
	int width;
	int wwidth;
	int	len;
	
	width=max;
	len=strlen(prompt);
	if(width>(76-len))
		width=76-len;
	wwidth=width+len+1;
	if(wwidth<18)  {
		len+=(18-wwidth)/2;
		wwidth=18;
	}

	GUI_RetVal=UIFC_INPUT;
	if(top==0)
		top=SCRN_TOP;
	if(left==0)
		left=SCRN_LEFT;
	if(mode&WIN_RHT)
		left=SCRN_RIGHT-width;
	while(left+wwidth>79)
		left--;
	if(left<=0 || (mode&WIN_L2R))
		left=(80-wwidth)/2;
	if(mode&WIN_BOT)
		top=23;
	while (top+5>22)
		top--;
	if (mode&WIN_T2B)
		top=0;
	if(top<=0)
		top=(23-5)/2;

	UIFC_Input *InputWin = new UIFC_Input(
			UIFC_CHAR_WIDTH*left,
			UIFC_LINE_HEIGHT*top,
			UIFC_CHAR_WIDTH*(wwidth+4),
			UIFC_LINE_HEIGHT*5,
			prompt,
			kmode,
			max,
			len,
			width,
			outstr);

	MainWin->add(InputWin);
	InputWin->end();
	InputWin->show();
	InputWin->show();
	/* This is kind of kludgy... it doesn't allow for adding of buttons to
	   the UIFC_Window before the UIFC_Input_Box */
	Fl::focus((UIFC_Menu *)InputWin->child(2));
	while(GUI_RetVal==UIFC_INPUT)  {
		Fl::wait();
		if(GUI_RetVal==UIFC_HELP)  {
			help();
			GUI_RetVal=UIFC_INPUT;
		}
		else if(GUI_RetVal==UIFC_RCLICK)
			GUI_RetVal=UIFC_INPUT;
	}
	if(GUI_RetVal==0)  {
		if(kmode&K_EDIT)  {
			if(strcmp(outstr,InputWin->invalue))  {
				api->changes=TRUE;
			}
		}
		else {
			if(strlen(InputWin->invalue))  {
				api->changes=TRUE;
			}
		}
		snprintf(outstr,max,"%s",InputWin->invalue);
	}
	MainWin->remove(InputWin);
	delete InputWin;

	if(GUI_RetVal==UIFC_CANCEL)
		return(-1);

    return(strlen(outstr));
}

/****************************************************************************/
/* Displays the message 'str' and waits for the user to select "OK"         */
/****************************************************************************/
void umsg(char *str)
{
    fl_message(str);
}

/****************************************************************************/
/* Status popup/down function, see uifc.h for details.						*/
/****************************************************************************/
void upop(char *str)
{
	static Fl_Double_Window *PopUp[MAX_POPUPS];
	static int CurrPop=0;
	int width=0;
	int height=1;

	if(str == NULL)  {
		for(;CurrPop>0;CurrPop--)  {
			delete PopUp[CurrPop-1];
		}
		return;
	}

	width=strlen(str);
	while(width>78)  {
		height+=1;
		width-=78;
	}
	PopUp[CurrPop]=new Fl_Double_Window((width+4)*UIFC_CHAR_WIDTH,(height+2)*UIFC_LINE_HEIGHT,NULL);
	PopUp[CurrPop]->border(FALSE);
	Fl_Box *box = new Fl_Box(
			UIFC_CHAR_WIDTH,UIFC_LINE_HEIGHT,
			UIFC_CHAR_WIDTH*width,UIFC_LINE_HEIGHT*height,"str");
	box->align(FL_ALIGN_CENTER|FL_ALIGN_WRAP|FL_ALIGN_INSIDE);
	PopUp[CurrPop]->set_modal();
	PopUp[CurrPop]->end();
	PopUp[CurrPop]->show();
	PopUp[CurrPop]->show();
	CurrPop++;
}

/****************************************************************************/
/* Sets the current help index by source code file and line number.			*/
/****************************************************************************/
void sethelp(int line, char* file)
{
    helpline=line;
    helpfile=file;
}

/****************************************************************************/
/* Help function.															*/
/****************************************************************************/
void help()
{
	char hbuf[HELPBUF_SIZE],str[256];
	char ch[2]={0,0};
    char *p;
	unsigned short line;
	size_t	len,j;
	int i=0;
	long l;
	FILE *fp;
	int inverse=0;
	int high=0;

	// Read help buffer
	if(!api->helpbuf) {
		if((fp=fopen(api->helpixbfile,"rb"))==NULL)
			sprintf(hbuf," ERROR  Cannot open help index:\n          %s"
				,api->helpixbfile);
		else {
			p=strrchr(helpfile,'/');
			if(p==NULL)
				p=strrchr(helpfile,'\\');
			if(p==NULL)
				p=helpfile;
			else
				p++;
			l=-1L;
			while(!feof(fp)) {
				if(!fread(str,12,1,fp))
					break;
				str[12]=0;
				fread(&line,2,1,fp);
				if(stricmp(str,p) || line != helpline) {
					fseek(fp,4,SEEK_CUR);
					continue;
				}
				fread(&l,4,1,fp);
				break;
			}
			fclose(fp);
			if(l==-1L)
				sprintf(hbuf," ERROR  Cannot locate help key (%s:%u) in:\n"
					"         %s",p,helpline,api->helpixbfile);
			else {
				if((fp=fopen(api->helpdatfile,"rb"))==NULL)
					sprintf(hbuf," ERROR  Cannot open help file:\r\n          %s"
						,api->helpdatfile);
				else {
					fseek(fp,l,SEEK_SET);
					fread(hbuf,HELPBUF_SIZE,1,fp);
					fclose(fp);
				}
			}
		}
	}
	else
		strcpy(hbuf,api->helpbuf);

	len=strlen(hbuf);


	UIFC_Window *HelpWin = new UIFC_Window(
			UIFC_CHAR_WIDTH*2-UIFC_BORDER_WIDTH,
			UIFC_LINE_HEIGHT*1-UIFC_BORDER_WIDTH,
			UIFC_CHAR_WIDTH*76+(UIFC_BORDER_WIDTH*2),
			UIFC_LINE_HEIGHT*22+(UIFC_BORDER_WIDTH*2),
			"Help Window");
	Fl_Text_Display *TextBox = new Fl_Text_Display(
			UIFC_BORDER_WIDTH,
			UIFC_LINE_HEIGHT+UIFC_BORDER_WIDTH,
			UIFC_CHAR_WIDTH*76,
			UIFC_LINE_HEIGHT*20+(UIFC_BORDER_WIDTH*6),
			"Help Window");
	TextBox->labelfont(FL_COURIER_BOLD);
	TextBox->labelsize(UIFC_CHAR_HEIGHT);
	if(!(api->mode&UIFC_MONO))  {
		TextBox->labelcolor(UIFC_WINDOW_TITLE_COLOR);
		TextBox->color(UIFC_HELP_WIN_BG_COLOR);
	}
	TextBox->textfont(FL_COURIER);
	TextBox->textsize(UIFC_CHAR_HEIGHT);
	TextBox->buffer(new Fl_Text_Buffer(HELPBUF_SIZE));
	Fl_Text_Display::Style_Table_Entry styletable[] = {     // Style table
		{ UIFC_HELP_PLAIN_COLOR,		FL_COURIER,				UIFC_CHAR_HEIGHT }, // A - Plain
		{ UIFC_HELP_HIGH_COLOR,	FL_COURIER_ITALIC,		UIFC_CHAR_HEIGHT }, // B - Bold
		{ UIFC_HELP_INVERSE_COLOR,		FL_COURIER_ITALIC,		UIFC_CHAR_HEIGHT }, // C - Inverse
		{ UIFC_HELP_BOTH_COLOR,	FL_COURIER_ITALIC,		UIFC_CHAR_HEIGHT }, // D - Both
	};
	Fl_Text_Buffer *StyleBuf=new Fl_Text_Buffer(HELPBUF_SIZE);
	TextBox->highlight_data(StyleBuf, styletable,
			(int)(sizeof(styletable) / sizeof(styletable[0])),
			'A',NULLCallback,(void *)NULL);
	MainWin->add(HelpWin);
	HelpWin->end();
	HelpWin->show();
	HelpWin->show();

	for(j=0;j<len;j++) {
		if(hbuf[j]==2 || hbuf[j]=='~') { /* Ctrl-b toggles inverse */
			if(inverse)
				inverse=0;
			else
				inverse=1;
		}
		else if(hbuf[j]==1 || hbuf[j]=='`') { /* Ctrl-a toggles high intensity */
			if(high)
				high=0;
			else
				high=1;
		}
		else  {
			ch[0]=hbuf[j];
			TextBox->insert(ch);
			ch[0]='A'+(high|(inverse<<1));
			StyleBuf->insert(i,ch);
			i++;
		}
	}
	while(GUI_RetVal==UIFC_HELP)  {
		Fl::wait();
		if(GUI_RetVal==UIFC_RCLICK)
			GUI_RetVal=UIFC_HELP;
	}
	delete StyleBuf;
	MainWin->remove(HelpWin);
	delete HelpWin;
}
