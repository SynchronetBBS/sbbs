#include "crtwin.h"

extern int crtwin_just; //default=1 (CENTER TEXT)

extern void	printc (int c, int x, int y, int color);

int printsjc_textlen (char *s)
//returns the size of the text excluding color "instruction" bytes.
 {
	int c0=0; //counts the number of chars to be written
	while (*s)
	 {
		if (*s==6 || *s==5 || *s==0x1b)
		 {
			if (*s==0x1b)
				c0++;
			s++;
		 }
		 else
			c0++;
		s++;
	 }
	return c0;
 }

void printsjc (char *s, int y, int color)
//Writes a string inside the box defined by crtwin_dta struct with justification
//defined by crtwin_just. Allows the user to change the color in the middle
//of the string, inserting special characters inside the string
 {
	int a0,a1; //auxiliar variables
	int color2; //stores the old color
	int rstcolor=0; //if ==1 restores old color

	if ((yi+1)>=yf) //protection
		return;

 //determines the row where the text will be written
	y=(unsigned)y%(yf-yi-1)+yi+1;

 //determines the position where the text will be written on screen
 //according to crtwin_dta and crtwin_just
	a0=printsjc_textlen(s);
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
		if (*s==5)
		 {
			if (!rstcolor)
			 {
				color2=color;
				s++;
				color=*s;
				rstcolor++;
			 }
			else
				s++;
		 }
		else if (*s==6)
		 {
			rstcolor=0;
			s++;
			color=*s;
		 }
		else
		 {
			if (*s==0x1b)
				s++;
			if ((a1>xi)&&(a1<xf)) //is char inside the box defined by crtwin_dta?
				printc(*s,a1,y,color); //if so, writes it.
			a1++;
			if (rstcolor) //restores old color
			 {
				rstcolor=0;
				color=color2;
			 }
		 }
		s++;
	 }
 }
