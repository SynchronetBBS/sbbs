#include <ciolib.h>
#include <genwrap.h>

extern int vmode_x;

extern void printc(int c, int x, int y, int color);

void printx (char *s, int x, int y, int color)
//prints a string with special formats.
//eg. \n \t color change, position change, pause, beep, special character,
//and other features.
//Note: This function doesn't changes cursor position.
 {
	int flag,color2;
	int tabsizep=8; //default tab size
	int taboffst=0; //taboffset from left border
	flag=0;
	while (*s)
	 {
		if (*s>0x1b || *s<0x05)
			goto jp1;
		switch (*s) //verifies if it is a control byte (or character)
		 {
			case 0x05: //prints a character with the previous defined color
				color2=color; //format => 05h;color;char;
				s++;			  //prints any character, even if it is a format char.
				color=*s;
				flag=1;
				s++;
			 goto jp1;
			case 0x06: //changes current color	format => 06h;color
				s++;
				color=*s;
			 break;
			case 0x07: //sends a beep using a DOS call (INT 21h)
				beep();
			 break;
			case 0x08: //bkspace
				x--;
			 break;
			case 0x09: //tab
				x=x-taboffst;
				x=x-(x%tabsizep)+tabsizep+taboffst;
			 break;
			case 0x0a: //line feed and carriage return until column=taboffset
				y++;
				x=taboffst;
			 break;
			case 0x0b: //line feed
				y++;
			 break;
			case 0x0c: //goes to upper left corner
				y=0;
				x=0;
			 break;
			case 0x0d: //carriage return
				x=taboffst;
			 break;
			case 0x0e: //changes "x" value, which determines current column
				s++;
				x=*s;
			 break;
			case 0x0f: //changes "y" value, which determines current row
				s++;
				y=*s;
			 break;
			case 0x10: //changes tab size
				s++;
				tabsizep=*s;
			 break;
			case 0x11: //changes taboffset
				s++;
				taboffst=*s;
			 break;
			case 0x12: //increments x;
				x++;
			 break;
			case 0x13: //decrements y;
				y--;
			 break;
			case 0x14: //pauses until a keyboard character is available (reads a
						  //single character which is ignored). Checks CTRL-BREAK
				// TODO: Needs to check CTRL-BREAK!
				while(!kbhit()) SLEEP(1);
			 break;
			case 0x15: //pauses until a keyboard character is available,
						  //but doesn't check CRTL-BREAK.
				while(!kbhit()) SLEEP(1);
			 break;
			case 0x1b: //prints any ascii character, even if it is a format byte
				s++;
			default: //if it isn't any format character, prints character at
						//screen
			  jp1:
				while (x>=vmode_x)	//protection
				 {
					x=x-vmode_x;
					y++;
				 }
				while (x<0)				//protection
				 {
					x=x+vmode_x;
					y--;
				 }
				printc(*s,x,y,color);//character output at screen
				x++;
				if (flag)
				 {
					color=color2;
					flag=0;
				 }
		 }
		s++;
	 }
 }
