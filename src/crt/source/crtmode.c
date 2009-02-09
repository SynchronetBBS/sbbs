#include <ciolib.h>

extern int vmode_x;
extern int vmode_mode;
extern int crt_page;

int getcrtmode (void) //returns the current video mode and updates
//vmode_x and crt_page to current values.
//vmode_y is not updated to actual value.
 {
	struct text_info	ti;

	gettextinfo(&ti);
	crt_page=0;
	vmode_x=ti.screenwidth;
	vmode_mode=ti.currmode;
	return(ti.currmode);
 }

void setcrtmode (int mode) //changes video mode and calls getcrtmode
 {
	textmode(mode);
	getcrtmode();
 }
