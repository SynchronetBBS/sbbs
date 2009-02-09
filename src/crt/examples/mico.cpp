/* MICO - SIMPLE PROGRAM THAT FILLS THE SCREEN WITH A CHARACTER   */
/*        AND A COLOR                                             */
/*   ALSO AN EXAMPLE OF VIDEO HANDLING FUNCTIONS                  */

//Usage: MICO <input character> <input color>
// Both character and color must be entered in hexadecimal notation.

#include <crt.h>
#include <ctype.h>
#include <stdio.h>

#define CGA  1
#define MCGA 2
#define EGA  3
#define VGA  9

#define VIDEO_ADAPTER VGA
	//replace VIDEO_ADAPTER value if your video adapter is not VGA/SVGA+

#include "hextoi.c"
  //hextoi defined in HEXTOI.CPP converts a string (in hexadecimal notation) to a integer

//pause pauses the screen

#define pause()\
asm mov ah,8;\
asm int 0x21

void main (int n, char *ptr[])
 {
	int color,chr;
	if (n<3) //displays help if missing input parameters
	 {
		fputs ("\n\
MICO ver 1.0 - Utility that fills the screen with a given character and color\n\
and after pauses until user hits a key\n\n\
\tUsage: MICO <char> <color>\n\n\
\t<char> is the character ASCII code (must be in hexadecimal notation)\n\
\t<color> is the fill color/attribute (must be in hexadecimal notation)\n\n\
by M rcio Afonso Arimura Fialho\n\
Freeware",stdout);
		return;
	 }

  //converts input parameters to chr and color
	if(hextoi(&chr,ptr[1]) || hextoi(&color,ptr[2]))
	 {
	  //if input parameters are incorrect
		fputs ("\nIncorrect input. Type MICO with no parameters to get help",stdout);
		return;
	 }

  //detects number of rows and columns in current video mode
	crt_detect(VIDEO_ADAPTER);

  //fills the screen with character given by chr and color color
	fillscr(chr,color);

  //pauses
	pause();
 }

// By Marcio Afonso Arimura Fialho
// http://pessoal.iconet.com.br/jlfialho
// e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com
