//Simple program that selects video mode.
//by Marcio A. A. Fialho

#include <stdio.h>
#include <ctype.h>

#include "hextoi.c"
  //hextoi defined in HEXTOI.C converts a string in hexadecimal notation to a integer

void main (int n, char *ent[2])
 {
	int a0; //a0 = mode
	if (n<2)
	 {
		fputs("\n\
SETMODE ver 1.0 - Selects current video mode\n\n\
\tUsage: SETMODE <videomode> \n\n\
\t<videomode> => new video mode (must be in hexadecimal notation)\n\n\
by M rcio Afonso Arimura Fialho\n\
Freeware / Public Domain",stdout);
		return;
	 }

	if (hextoi(&a0,ent[1]))
	 {
		fputs("ERROR:Input parameter is incorrect.\n\
Type SETMODE with no parameters to get help",stdout);
		return;
	 }

	_AH=0;
	_AL=a0;
   asm int 0x10;
 }
