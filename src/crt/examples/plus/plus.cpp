/*PLUS.CPP IS A EXAMPLE OF VIDEO HANDLING FUNCTIONS                        */
/*PLUS.CPP illustrates the possibility to create enhanced versions of       */
/*VIDEO HANDLING FUNCTIONS.                                                */
/*To compile with command line compiler, type TCC -O -Z PLUS.CPP           */
/*To compile this you will need first to install VIDEO HANDLING FUNCTIONS  */
/*                                                                         */
/* FREEWARE/PUBLIC DOMAIN                                                  */
/*Copyright (c) by M rcio Afonso Arimura Fialho                            */
/*http://pessoal.iconet.com.br/jlfialho                                    */
/*E-mail:jlfialho@iconet.com.br                                            */
/*Alternate E-mail: jlfialho@yahoo.com (use only if the above doesn't work */

/*WARNING: THIS EXAMPLE REQUIRES AT LEAST AN EGA VIDEO ADAPTER             */

/*WARNING: When running this program under a MS-Windows MS-DOS prompt, be  */
/* sure that the MS-DOS prompt is in full screen mode, because some        */
/* functions used by this example aren't effective in a MS-DOS window      */

/*This example is based on VIDEOHLP.TXT Example 2. Notice the resemblance  */

/*For further information, read \EXAMPLES\README.TXT                       */

#include <crt.h>
#include <conio.h>
#include <stdlib.h>

#include "crtfont.c" //functions for selecting a font from disk
							//enhacement of CRT.H

#include "xmkline.c" //enhanced versions of mkline and mkline_aux

//crtframe, mkline, and macros examples.
//This example contains many sub-examples. Perhaps it is interesting
//to separate them.

#define VGA 9
#define EGA 3

#define VIDEOTYPE VGA  //if your video adapter is EGA, redefine this.

void pause ()
 {
	asm push ax;
	asm mov ah,0x08;
	asm int 0x21;
	asm pop ax;
 }

void initxmkline() //selects mkline font and updates crtframe_mat
 {
	char *p;
	setcrtmode (3);
	changechar_blk=1;
	if (changefnt ("linedrw2.fnt",0,0,0x100))
	 {
		prints ("ERROR: COULDN'T LOAD FONT FILE LINEDRW2.FNT",0,0,7);
		prints ("FONT MIGHT BE MISSING OR NOT ENOUGH MEMORY",0,1,7);
		crt_gotoxy(0,2);
		exit(1);
	 }
//	changechar(linedrw2_font,0,0x100);
	crtfontspec(4); //light foreground colored text will be written with linedrw2_font
						//dark foregound colored text will be written with standard font

	//it's not required to create a enhanced version of crtframe to display
	//characters with triple or quadruple outline, all that is needed is to
	//change crtframe_mat characters.
	p=(char *)crtframe_mat+16;

  //triple outline
	*p='ã'; p++;
	*p='Š'; p++;
	*p='ä'; p++;
	*p='¥'; p++;
	*p='ç'; p++;
	*p='Š'; p++;
	*p='æ'; p++;
	*p='¥'; p++;

  //quadruple outline
	*p='b'; p++;
	*p=3;   p++;
	*p='c'; p++;
	*p='('; p++;
	*p='f'; p++;
	*p=3;   p++;
	*p='e'; p++;
	*p='('; p++;

#if VIDEOTYPE==VGA
	setchrboxwidth (1);
#endif
 }

	//crtframe extended macros
#define moldurat(xi,yi,xf,yf,color)\
	crtframe(xi,yi,xf,yf,color,2)	//draws a text frame with triple outline
#define molduraq(xi,yi,xf,yf,color)\
	crtframe(xi,yi,xf,yf,color,3)	//draws a text frame with quadruple outline

	//crtframew extended macros
#define molduratw(color)\
	crtframew(color,2)
#define molduraqw(color)\
	crtframew(color,3)

	//linha... macros will be redefined, therefore they need to be undefined
#undef linha_hor
#undef linha_ver
#undef linhad_hor
#undef linhad_ver

	//mkline4 macros (redefining old and creating new linha... macros)
#define linha_hor(y,xi,xf,color)\
	mkline4(y,xi,xf,color,0) //draws a single horizontal line
#define linha_ver(x,yi,yf,color)\
	mkline4(x,yi,yf,color,1) //draws a single vertical line
#define linhad_hor(y,xi,xf,color)\
	mkline4(y,xi,xf,color,2) //draws a double horizontal line
#define linhad_ver(x,yi,yf,color)\
	mkline4(x,yi,yf,color,3) //draws a double horizontal line
#define linhat_hor(y,xi,xf,color)\
	mkline4(y,xi,xf,color,4) //draws a triple horizontal line
#define linhat_ver(x,yi,yf,color)\
	mkline4(x,yi,yf,color,5) //draws a triple horizontal line
#define linhaq_hor(y,xi,xf,color)\
	mkline4(y,xi,xf,color,6) //draws a quadruple horizontal line
#define linhaq_ver(x,yi,yf,color)\
	mkline4(x,yi,yf,color,7) //draws a quadruple horizontal line

  //program main function
void main ()
 {
	int i=0,j=0,k=0;
	int c0;

 //displays warning message
	fillscr (' ',0x07);
	crt_gotoxy(0,6);
	crtwin_just=0;
	printsj ("WARNING: If you are running this example from a MS-WINDOWS MS-DOS prompt and",0,0x07);
	printsj ("you want to see everything be sure that the MS-DOS prompt is running in full",1,0x07);
	printsj ("screen mode, because some functions in this example aren't effective in a",2,0x07);
	printsj ("MSDOS prompt window.",3,0x07);
	printsj ("To toggle a MS-WINDOWS MS-DOS prompt between full screen mode / window mode",4,0x07);
	printsj ("type ALT-ENTER.",5,0x07);
	pause ();
	crtwin_just=1;

 //selects mkline4 font and updates crtframe_mat
	initxmkline();

 //crtframe, crtframew and mkline4 macros examples
	fillscr ('°',0x19);
	prints ("* * * fillbar, crtframe and mkline4 example * * *",16,1,0x17);
	prints ("Using macros moldura... and linha...",21,2,0x17);

	setcrtwin (2,5,18,13);
	molduraw (0x1f);
	fillbarw (' ',0x19);
	pause ();

	setcrtwin (22,5,38,13);
	molduradw (0x1f);
	fillbarw (' ',0x19);
	pause ();

	setcrtwin (42,5,58,13);
	molduratw (0x1f);
	fillbarw (' ',0x19);
	pause ();

	setcrtwin (62,5,78,13);
	molduraqw (0x1f);
	fillbarw (' ',0x19);
	pause ();


	linha_hor  (9, 2,18,0x1e);
	linhad_hor (9,22,38,0x1e);
	linhat_hor (9,42,58,0x1e);
	linhaq_hor (9,62,78,0x1e);
	pause ();

	linha_ver  (10,5,13,0x1e);
	linhad_ver (30,5,13,0x1e);
	linhat_ver (50,5,13,0x1e);
	linhaq_ver (70,5,13,0x1e);
	pause ();

	prints ("- - - Hit any key to continue - - -",22,24,0x97);
	pause ();

//crtframe and moldura... example

	fillscr ('°',0x19);
	setcrtwin (-1,-1,80,25);
	printsj ("* * * Example of frames draw by * * *",1,0x17);
	printsj ("* * * crtframe with modified crtframe_mat * * *",2,0x17);
	pause ();

	moldura (10,5,20,8,0x1f);
	prints ("type = 0",11,9,0x17);
	pause ();

	moldurad (50,5,60,8,0x1f);
	prints ("type = 1",51,9,0x17);
	pause ();

	moldurat (10,15,20,18,0x1f);
	prints ("type = 2",11,19,0x17);
	pause ();

	//Normally user defined character, but now quadruple outline frame
	molduraq (50,15,60,18,0x1f);
	prints ("type = 3",51,19,0x17);
	pause ();

 //As any call to crtframe with "type" greater than 255 redefines crtframe_mat
 //positions 24 through 31 (that are being used by quadruple outline). A call
 //to crtframe with any value greater than 255 would destroy the quadruple
 //outline, therefore the lines below have been disabled.

 /*
	crtframe (37,15,47,18,0x1f,0x101);
	//user defined character is redefined as'þ'
	prints ("type = 101h",37,19,0x17);
	pause ();

	crtframe (63,15,73,18,0x1f,3);  //current user defined character='þ');
	prints ("type = 3",64,19,0x17);
	pause ();
	*/

//crtframe and mkline4 example

 #define L0 1      //column initial position
 #define L1 2      //row (line) initial position
 #define L2 12     //each x side size
 #define L3 4      //each y side size
 #define L4 6      //x median
 #define L5 2      //y median
 #define L6 20     //smaller spacing in x
 #define L7 6      //spacing in y
 #define L8 0      //bigger spacing in x
 #define CF 0x1e   //crtframe color
 #define CL 0x1f   //mkline4 color

	fillscr ('°',0x19);
	for (i=0;i<4;i++)
	 {
		fillscr ('°',0x19);
		prints ("* * * FRAME TYPES * * *",28,0,0x17);
		for (j=0;j<4;j++)
			for (k=0;k<4;k++)
			 {
				crtframe (L0+L6*k+L8*i,L1+L7*j,L0+L2+L6*k+L8*i,L1+L3+L7*j,CF,i);
				mkline4(L1+L5+L7*j,L0+L6*k+L8*i,L0+L2+L6*k+L8*i,CL,2*k);
				mkline4(L0+L4+L6*k+L8*i,L1+L7*j,L1+L3+L7*j,CL,2*j+1);
			 }
		pause ();
	 }

//Example of drawing a interesting text box, with many divisions inside,
//organized in a hierarchical way that resembles fractals.

	fillscr ('°',0x19);

	molduraq (8,0,72,24,0x1b);

	for (c0=12;c0<72;c0+=8)
		fillbar('l',c0,1,c0,23,0x1c);

	for (c0=0;c0<4;c0++)
	 {
		linha_hor (3+c0*6,8,72,0x1e);
		linha_ver (16+c0*16,0,24,0x1e);
	 }

	for (c0=0;c0<2;c0++)
	 {
		linhad_hor (6+c0*12,8,72,0x1f);
		linhad_ver (24+c0*32,0,24,0x1f);
	 }

	linhat_hor (12,8,72,0x1a);
	linhat_ver (40,0,24,0x1a);

	linhaq_hor(0,8,72,0x1b);
	linhaq_hor(24,8,72,0x1b);
	linhaq_ver(8,0,24,0x1b);
	linhaq_ver(72,0,24,0x1b);

	pause ();

//The End

	fillscr ('°',0x19);
	printsj(" * * * THE END * * * ",1,0x1f);
	printsj(" - - - Hit any key to return - - - ",24,0x97);
	pause();
	setcrtmode (3);
 }