#include <stdlib.h>
#include <gen_defs.h>
#include <ciolib.h>
#include <vidmodes.h>
#include <allfonts.h>

int changechar_height=16;
int changechar_func=0x1100;
int changechar_blk=0;

void changechar (unsigned char *fmt, int ind, int qt)
 {
	unsigned char		*newfont;
	unsigned char		*currfont;
	struct text_info	ti;
	int					vm;
	int					fontsize;
	int					cfont;

	gettextinfo(&ti);

	if(ind < 0 || ind > 255 || ind+qt > 256)
		return;

	if((vm=find_vmode(ti.currmode))!=-1) {
		fontsize=vparams[vm].charheight*256;
		newfont=malloc(fontsize);
		cfont=getfont();
		switch(fontsize) {
			case 4096:
				currfont=conio_fontdata[cfont].eight_by_sixteen;
				break;
			case 3586:
				currfont=conio_fontdata[cfont].eight_by_fourteen;
				break;
			case 2048:
				currfont=conio_fontdata[cfont].eight_by_eight;
				break;
		}
		memcpy(newfont,currfont,fontsize);
		memcpy(newfont+(vparams[vm].charheight*ind), fmt, qt*vparams[vm].charheight);
		switch(fontsize) {
			case 4096:
				FREE_AND_NULL(conio_fontdata[255].eight_by_sixteen);
				conio_fontdata[255].eight_by_sixteen=newfont;
				break;
			case 3586:
				FREE_AND_NULL(conio_fontdata[255].eight_by_fourteen);
				conio_fontdata[255].eight_by_fourteen=newfont;
				break;
			case 2048:
				FREE_AND_NULL(conio_fontdata[255].eight_by_eight);
				conio_fontdata[255].eight_by_eight=newfont;
				break;
		}
		setfont(255,0);
	}
 }

void changecharg (unsigned char *fmt, int rows)
 {
#if 0
	asm push ax;
	asm push bx;
	asm push cx;
	asm push dx;
	asm push es;
	_BL=0x00;
	_CX=changechar_height;
	_DL=rows;
	asm push bp
	_ES=FP_SEG(fmt);
	_BP=FP_OFF(fmt);
	_AX=0x1121;
	intm(0x10);
	asm pop bp;
	asm pop es;
	asm pop dx;
	asm pop cx;
	asm pop bx;
	asm pop ax;
#endif
 }
