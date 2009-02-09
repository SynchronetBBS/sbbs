/* PALETTE - DISPLAYS THE COLOR SET AVAILABLE FOR TEXT MODE TEXTS */
/*   ALSO AN EXAMPLE OF VIDEO HANDLING FUNCTIONS                  */

#include <crt.h>

#define CGA  1
#define MCGA 2
#define EGA  3
#define VGA  9

#define VIDEO_ADAPTER VGA
  //If your video adapter isn't VGA, redefine VIDEO_ADAPTER with one of
  //above adapter types.

void main ()
 {
	int c0,c1; //counters
	char s[5]; //A string that stores the text that is written with each color

	s[0]=' '; //Initializes string s.
	s[3]=' '; //s[1] and s[2] are loaded with values during conversion
	s[4]=0; //Null terminating character

	crt_detect(VIDEO_ADAPTER); //Gets current video status (it is, number of rows, columns, etc)

	fillscr (' ',0x07); //Clears the screen

  //Displays title
	prints ("* * * CURRENT TEXT COLORS * * *",25,1,0x1f);
	prints ("COLOR INDEX (hexadecimal):",5,3,0x0f);

  //Displays each color with it's value in hexadecimal notation
	for (c0=0;c0<16;c0++) //c0==background color
	 {
		//Converts background color to s[1]
		if (c0<10)
			s[1]=c0+0x30;
		 else
			s[1]=c0+0x37;

		for (c1=0;c1<16;c1++) //c1==foreground color
		 {
			//Converts foreground color to s[2]
			if (c1<10)
				s[2]=c1+0x30;
			else
				s[2]=c1+0x37;
			prints (s,4*c0+5,c1+5,c0*16u+(unsigned)c1); //outputs s
		 }
	 }

  //repositions the cursor at the begin of the immediate line that follows the
  //lowermost written line.
	crt_gotoxy (0,21);
 }

// By Marcio Afonso Arimura Fialho
// http://pessoal.iconet.com.br/jlfialho
// e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com