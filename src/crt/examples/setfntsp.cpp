//Simple program that selects which character sets (in map 3) are selectable
//by dark and light colors
//by Marcio A. A. Fialho

#include <crt.h>
#include <stdio.h>
#include <ctype.h>

#include "hextoi.c"
  //hextoi defined in HEXTOI.C converts a string in hexadecimal notation to a integer

void main (int n, char *ent[2])
 {
	int a0; //a0 = block specifier
	if (n<2)
	 {
		fputs("\n\
SETFNTSP ver 1.0 - Selects video character set blocks (in map 2)\n\n\
\tUsage: SETFNTSP <block specifier> \n\n\
\t<block specifier> => new block specifier value\n\
\t\t(must be in hexadecimal notation)\n\n\
 Bitfields for block specifier: R1|R0|L2|D2|L1|L0|D1|D0    where:\n\
\tR1|R0 => reserved\n\
\tL2|L1|L0 => specifies the block selected by light colors\n\
\tD2|D1|D0 => specifies the block selected by dark colors\n\
\tbits L2 and D2 are not used by EGA video adapters\n\n\
 To load fonts in blocks other than zero, use DOSFONT\\FONTSEL with /B option \n\n\
* * * WARNING: If the screen becomes unreadable, type VOLTA or SETFNTSP 0 * * *\
\n\nby M rcio Afonso Arimura Fialho\n\
Freeware / Public Domain",stdout);
		return;
	 }

	if (hextoi(&a0,ent[1]))
	 {
		fputs("ERROR:Input parameter is incorrect.\n\
Type SETFNTSP with no parameters to get help",stdout);
		return;
	 }
	crtfontspec(a0);
 }
