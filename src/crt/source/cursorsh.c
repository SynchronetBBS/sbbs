#include <ciolib.h>
#include <vidmodes.h>

static int curr_shape=0x2f00;

//These functions works at text page defined by crt_page (default=0)
void setcursorsh (unsigned int shape) //changes the cursor shape.
	//bit 15-> must be 0; bits 14,13 -> cursor blink (invisible, blinking)
	//bits 12-8 -> upper scan line		bits 4-0 -> lower scan line
	//Data from Ralf Brown Files
 {
	struct text_info ti;
	int	s,e,r,vm;

	gettextinfo(&ti);

	if(((shape >> 8) & 0x1f) < (shape & 0x1f)) {
		_setcursortype(_NOCURSOR);
	}
	else if(((shape >> 8) & 0x1f) - (shape & 0x1f) <= 4) {
		_setcursortype(_NORMALCURSOR);
	}
	else {
		_setcursortype(_SOLIDCURSOR);
	}
	if((vm=find_vmode(ti.currmode))!=-1) {
		s=(shape & 0x1f00)>>8;
		e=shape & 0x1f, 0x1f;
		r=vparams[vm].charheight;
		if(s>=r)
			s=r-1;
		if(e>=r)
			e=r-1;
		
		setcustomcursor(s,e,r, shape & 0x20, (shape & 0x40?0:1));
	}
	curr_shape=shape;
 }

int getcursorsh (void) //gets the current cursor shape.
 {
	return(curr_shape);
 }
