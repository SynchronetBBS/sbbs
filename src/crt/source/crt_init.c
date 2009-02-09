extern int vmode_x;
extern int vmode_y;
extern int crtwin_just;

#include "crtwin.h"

extern void crt_detect(int mode);

void crt_init(int mode)
 {
	crt_detect(mode);
	xi=-1;
	yi=-1;
	xf=vmode_x;
	yf=vmode_y;
	crtwin_just=1; //CENTER_TEXT
 }
