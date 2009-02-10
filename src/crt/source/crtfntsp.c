#include <stdlib.h>
#include <string.h>
#include <ciolib.h>
#include <allfonts.h>

int crt_main_font=0;
int crt_secondary_font=-99;

void crtfontspec (int blkspec) //enables the display of two character fonts
 {
	int new_main=(blkspec & 0x03) | ((blkspec & 0x10) >> 2);
	int new_second=((blkspec >> 2) & 0x03) | ((blkspec & 0x20) >> 3);

	// Load default charset into all 8 possible blocks
	if(crt_secondary_font == -99) {
		int i;
		int cfnt=getfont();

		for(i=0; i<8; i++) {
			conio_fontdata[255-i].eight_by_sixteen=(unsigned char *)malloc(16*256);
			memcpy(conio_fontdata[255-i].eight_by_sixteen,conio_fontdata[cfnt].eight_by_sixteen,16*256);
			conio_fontdata[255-i].eight_by_fourteen=(unsigned char *)malloc(14*256);
			memcpy(conio_fontdata[255-i].eight_by_fourteen,conio_fontdata[cfnt].eight_by_fourteen,16*256);
			conio_fontdata[255-i].eight_by_eight=(unsigned char *)malloc(8*256);
			memcpy(conio_fontdata[255-i].eight_by_eight,conio_fontdata[cfnt].eight_by_eight,16*256);
			conio_fontdata[255-i].desc="CRT Font";
		}
		i=getvideoflags();
		i|=CIOLIB_VIDEO_ALTCHARS;
		setvideoflags(i);
	}
	crt_main_font=new_main;
	setfont(255-crt_main_font, 0, 1);
	crt_secondary_font=new_second;
	setfont(255-crt_secondary_font, 0, 2);
 }
