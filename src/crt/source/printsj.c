#include "crtwin.h"

extern int crtwin_just; //default=1 (CENTER TEXT)

extern void		printc	(int c, int x, int y, int color);
extern unsigned int strlen(const char *__s);

int  __abs__(int);

void printsj (char *s, int y, int color)
//Writes a string inside the box defined by crtwin_dta struct with justification
//defined by crtwin_just.
 {
	int a0,a1; //auxiliar variables

	if ((yi+1)>=yf) //protection
		return;

 //determines the row where the text will be written
	y=(unsigned)y%(yf-yi-1)+yi+1;

 //determines the position where the text will be written on screen
 //according to crtwin_dta and crtwin_just and its size
	a0=strlen(s);
	switch (crtwin_just)
	 {
		case 0: a1=xi+1; break;
		case 1: a1=(xi+xf-a0+1)/2; break;
		case 2: a1=xf-a0; break;
		default: return;
	 }

//writes the text
	while(*s)
	 {
		if ((a1>xi)&&(a1<xf)) //is char inside the box defined by crtwin_dta?
			printc(*s,a1,y,color); //if so, writes it.
		a1++;
		s++;
	 }
 }
