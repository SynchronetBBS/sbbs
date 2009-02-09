//Simple program that selects the active display page
//by Marcio A. A. Fialho

#include <crt.h>
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
SETPAGE ver 1.0 - Selects video active display page\n\n\
\tUsage: SETPAGE <pagenum> \n\n\
\t<pagenum> => new video page (must be in hexadecimal notation)\n\n\
by M rcio Afonso Arimura Fialho\n\
Freeware / Public Domain",stdout);
		return;
	 }

	if (hextoi(&a0,ent[1]))
	 {
		fputs("ERROR:Input parameter is incorrect.\n\
Type SETPAGE with no parameters to get help",stdout);
		return;
	 }
	setcrtpage (a0);
 }
