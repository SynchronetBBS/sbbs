/* ASCII - DISPLAYS CURRENT DOS CHARACTER FONT SELECTED           */
/*   ALSO AN EXAMPLE OF VIDEO HANDLING FUNCTIONS                  */

#include <crt.h>

#define CGA  1
#define MCGA 2
#define EGA  3
#define VGA  9

#define VIDEO_ADAPTER VGA
  //if your video adapter isn't VGA, redefine VIDEO_ADAPTER with one of
  //above adapter types.

void main ()
 {
	int c0,c1;

	crt_detect(VIDEO_ADAPTER);//gets current video status (it is, number of rows, columns, etc)

	fillscr (' ',7); //clears the screen

  //Writes all 256 ascii characters
	for (c0=0;c0<8;c0++)
		for (c1=0;c1<16;c1++)
		 {
			printc (c0*16+c1,c1+17,c0+6,0x0b);
			printc (c0*16+c1+128,c1+42,c0+6,0x0b);
		 }

  //displays title
	printsj ("Extended ASCII table (or current DOS character set):",1,0x0e);

  //writes the text beside and the line between the 256 characters
	for (c0=0;c0<8;c0++)
	 {
		prints ("(00)h ",10,6+c0,0x0f);
		prints ("(80)h ",60,6+c0,0x0f);
		printc (c0+0x30,11,6+c0,0x0f);
		printc (c0+0x3f,61,6+c0,0x0f);
		printc ('º',37,6+c0,0x0f);
	 }
	printc ('8',61,6,0x0f);
	printc ('9',61,7,0x0f);

  //Displays the numbers in white over the 256 characters
	for (c1=17;c1<60;c1+=25)
	 {
		for (c0=c1;c0<(c1+16);c0++)
			printc('0',c0,3,0x0f);
		prints ("0123456789ABCDEF",c1,4,0x0f);
	 }

  //repositions the cursor at the begin of the immediate line that follows the
  //lowermost written line.
	crt_gotoxy(0,14);
 }

// By Marcio Afonso Arimura Fialho
// http://pessoal.iconet.com.br/jlfialho
// e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com