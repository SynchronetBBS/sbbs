#include <ciolib.h>

static int curr_shape=0x2f00;

//These functions works at text page defined by crt_page (default=0)
void setcursorsh (unsigned int shape) //changes the cursor shape.
	//bit 15-> must be 0; bits 14,13 -> cursor blink (invisible, blinking)
	//bits 12-8 -> upper scan line		bits 4-0 -> lower scan line
	//Data from Ralf Brown Files
 {
	if(((shape >> 8) & 0x1f) < (shape & 0x1f)) {
		_setcursortype(_NOCURSOR);
	}
	else if(((shape >> 8) & 0x1f) - (shape & 0x1f) <= 4) {
		_setcursortype(_NORMALCURSOR);
	}
	else {
		_setcursortype(_SOLIDCURSOR);
	}
	curr_shape=shape;
 }

int getcursorsh (void) //gets the current cursor shape.
 {
	return(curr_shape);
 }
