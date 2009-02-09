#include <ciolib.h>

extern int vmode_x;
extern int vmode_y;
extern int vmode_mode;
extern int crt_direct;
extern int crt_page;
extern int crt_EVGA_addr; //EGA/VGA base port address.
extern char*video_addr;

void crt_detect(int mode)
//mode values:
	//1 - CGA
	//2 - MCGA
	//3,4,5 - EGA
	//9 - VGA,SVGA or better
 {
	struct text_info	ti;

	gettextinfo(&ti);

	vmode_x=ti.screenwidth;
	vmode_y=ti.screenheight;
	vmode_mode=ti.currmode;
	crt_direct=1;
	crt_page=0;
 }
