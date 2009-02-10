/* VIDEO HANDLING FUNCTIONS EXAMPLE/DEMO PROGRAM                          */
/* By M rcio Afonso Arimura Fialho                                        */
/* For further information read README.TXT in this subdirectory           */

/* IMPORTANT: If you are running this demo under a MS-Windows MS-DOS      */
/* prompt be sure that it is running in full screen mode to see the
/* effect of all functions, because some functions are not effective when */
/* they run in a MSDOS-prompt window */

#define CRT_FULL
#include <crt.h>
#include <ciolib.h>
#include <stdio.h>
#include <stdlib.h>

#undef savevideo
#undef savevideowin
#undef savevideow
#undef restorevideo
#undef restorevideowin
#undef restorevideow

#define LEFT_TEXT 0
#define CENTER_TEXT 1
#define RIGHT_TEXT 2

int video_adapter;

int pause () //similar to getch, but if user hits ESC, exits from program
 {
	int a0;
	a0=getch();
	if (a0==0x1b)
	 {
		setcrtmode (3);
		exit(0);
	 }
	return a0;
 }

void scrpause() //pauses and displays a message
 {
	crt_init (video_adapter);
	printsj ("- - - HIT ANY KEY TO CONTINUE, ESC EXITS - - -",vmode_y-1,0x9f);
	pause();
 }

void dispstatus () //displays current video status
 {
	char *msg[2]={"TEXT","GRAPHICS"};
	crt_detect (video_adapter); //updates vmode_mode,crt_direct,crt_page,... values to current video status
	fillscr (' ',0x19);
	prints ("CURRENT VIDEO STATUS:",2,2,0x1e);
	printxf(5,4,0x1b,"Current video mode\t\t= \6\37%d (%.2X)h",vmode_mode,vmode_mode);
	printxf(6,5,0x1b,"%s mode selected",msg[crt_direct]);
	printxf(5,7,0x1b,"Active display page\t= \6\37%d",crt_page);
	if (!crt_direct)
		printxf(5,8,0x1b,"Active display page base\n      \
address in text video RAM\t= \6\37%Fp",video_addr);
	printxf (5,10,0x1b,"number of screen columns\t= \6\37%d",vmode_x);
	printxf (5,11,0x1b,"number of screen rows\t= \6\37%d",vmode_y);

	printxf (2,(vmode_y*4)/5,0x1a,
	  "getcrtmode returned \6\37%d\6\32 after being called.", getcrtmode());
	  //getcrtmode example

	scrpause ();
 }

void disppalette (int xi, int yi, int tcolor, int fcolor) //displays the palette
//(xi,yi) == upper left corner
//tcolor = text color		fcolor = inside color
 {
	int c0;
	barcolor (xi,yi,xi+42,yi+17,fcolor);
	for (c0=0;c0<16;c0++)
	 {
		printsf (xi+1,yi+c0+1,tcolor,"palette register %.2d value = %.3d color =",
			c0,getpalreg(c0));
		printc ('Û',xi+41,yi+c0+1,c0);
	 }
 }

void dispbordercolor () //displays current border (overscan) color
 {
	unsigned a0;
	a0=getbordercolor();
	printsf (5,10,0x1f,"Current overscan color = %d (%.2X)h",a0,a0);
	scrpause ();
 }

void dispdacvalues () //display DAC register associated with each palette
	//register and it's value
 {
	unsigned u0,u1,u2,c0,red,green,blue;
	prints ("Selected      DAC colors   ",50,1,0x1f);
	printx ("DAC Reg.\5\x1c*    Red Green Blue",50,2,0x1f);

	u0=getdacpgstate();
	if (u0%256u)
		u1=16;
	 else
		u1=64;
	for (c0=0;c0<16;c0++)
	 {
		u2=(unsigned)getpalreg(c0)%u1+(u0/256u)*u1;
		//getdacreg(u2,&(char)red,&(char)green,&(char)blue);
		printsf(51,4+c0,0x1f," %.3d        %.2Xh  %.2Xh  %.2Xh ",
			u2,(unsigned char)red,(unsigned char)green,(unsigned char)blue);
	 }
	printx ("\5\x0c* by palette register on the left",10,21,0x0f);
	printsf (10,22,0x1f,"Current DAC page = %d.   Page size = %d (in DAC registers)",u0/256u,u1);
 }

void dispascii ()
 {
	int a0;
	int c0,c1;

	for (c0=0;c0<8;c0++)
		for (c1=0;c1<16;c1++)
		 {
			printc (c0*16+c1,c1+17,c0+8,0x17);
			printc (c0*16+c1+128,c1+42,c0+8,0x17);
		 }
		prints ("Current Font Character Set:",25,3,0x1e);
		for (c0=0;c0<8;c0++)
		 {
			prints ("(00)h ",10,8+c0,0x1f);
			prints ("(80)h ",60,8+c0,0x1f);
			printc (c0+0x30,11,8+c0,0x1f);
			printc (c0+0x3f,61,8+c0,0x1f);
			printc ('º',37,8+c0,0x1f);
		 }
		printc ('8',61,8,0x1f);
		printc ('9',61,9,0x1f);
		for (c0=0;c0<16;c0++)
		 {
			printc ('0',c0+17,5,0x1f);
			printc ('0',c0+42,5,0x1f);
			if (c0<10)
				a0=c0+'0';
			 else
				a0=c0+0x37;
			printc (a0,c0+17,6,0x1f);
			printc (a0,c0+42,6,0x1f);
		 }
 }

//this file contains the 8x16 handscript font patterns for characters
//0x20 through 0xAFh (font array) (used by changechar example)
#include "handscr.cpp"

//this file contains the 8x12 font information (used by changechar example)
#include "font12.cpp"

//this file contains the 8x8 font information for changecharg example
#include "crazy8.cpp"

extern "C" {
int main ()
 {
	int c0,c1,c2,c3;
	int a0,a1,a2,a3;
	unsigned u0,u1,u2;
	unsigned red,green,blue;

	char *ex_msg="EXAMPLE.";
	char buffer[4096];
	char buffer2[4096];
	char windowbuf[1000];
	char windowbuf2[1000];
	char *p;
	struct crtwin licos,deflt;
	struct crtwin_inp lelecos; //input values for crtwindow function

 //Displays a warning message
	cprintf ("\n\
WARNING: If you are running this demo from a MS-WINDOWS MS-DOS prompt and\n\
 you want to see everything be sure that the MS-DOS prompt is running in full\n\
 screen mode, because some functions in this demo aren't effective in a\n\
 MSDOS prompt window.\n\
\tTo toggle a MS-WINDOWS MS-DOS prompt between full screen mode / window\n\
 mode type ALT-ENTER.\n");

 //Startup, selects video adapter type
	cprintf ("\nWhat is your video adapter?\n\
\t1 - CGA\n\
\t2 - MCGA\n\
\t3 - EGA\n\
\t4 - EGA + UltraVision v2+\n\
\t9 - VGA\\SVGA+\n");
	video_adapter=getch()-0x30;
	if (video_adapter!=1 && video_adapter!=2 && video_adapter!=3 &&
		video_adapter!=4 && video_adapter!=9)
	 {
		cprintf ("Unknow adapter type. Aborting...");
		exit (1);
	 }
	crt_init(video_adapter); //autodetects number of rows,columns, active page, current
					//video mode, etc. And updates crtwin_dta. But not essential
					//to other Video Handling Functions

 //Introduction
	fillscr('°',BLUE*BKCOLOR+LIGHTBLUE);
	printsj ("* * * CRT FUNCTIONS TEST AND DEMO PROGRAM * * *",0,0x1e);
	pause ();

 //Example of capabilities to work in different screen size (EGA+)
	if (video_adapter >2) //if video_adapter >= EGA
	 {
		textmode (64);
		videomode (64); //obsolete function
		fillscr ('1',0x17);
		pause ();
	 }

 //Example of capability to work in many video modes
	dispstatus ();

	setcrtmode (1);
	setcrtpage (2);
	dispstatus (); //displays current status

	setcrtpage (0);//required due to a Windows 95 bug (see notice below)
	setcrtmode (3);
	setcrtpage (3);
	dispstatus ();

	setcrtpage (0);//required due to a Windows 95 bug (see notice below)
	setcrtmode (6);
	dispstatus ();

	//NOTICE: I had to include these calls to setcrtpage with zero
	//(setcrtpage(0);) before calling setcrtmode due to a Windows 95 BUG
	//that occured while I was running this program under Windows 95.
	//The bug is that if a program running in a MS-DOS prompt (in full
	//screen mode) calls INT 10h/AH=0 (this is the BIOS function that changes
	//the video mode and is used by setcrtmode) with active display page
	//other than zero, the system halts and the CPU seems to switch to sleep
	//mode, and all the user can do is to reset the machine.
	//Notice that this BUG doesn't occur in MSDOS, Windows 3.11 and Windows 95
	//running in real MSDOS mode. Notice also that this BUG may also be caused
	//by a misconfiguration of the computer or that you have a Windows 95/98
	//version free of this BUG. If your Windows 95/98 version is free of this
	//BUG the calls to setcrtpage(0) before setcrtmode will be redundant and
	//you may remove them. (unhapply there's no safe way to check your computer
	//for this BUG, but if you try, please report to me the results)

 //Cursor positioning functions
	setcrtmode(3);
	crt_detect(video_adapter);
	fillscr ('°',0x19);
	crt_gotoxy (37,16);
	pause ();
	crt_getxy(&a0,&a1);
	a0/=2; a1/=2;
	crt_gotoxy(a0,a1);
	pause ();

 //Character input/output functions
	a2=getcrtchar(a0,a1)-0x30;
	a3=getcrtcolor(a0,a1)+0x03;
	biosprintc (a2,10,5,0x3b,9);
	biosprintc (a2,15,5,0x3b,10);
	printcn (a2,20,5);
	printc (a2,25,5,a3);
	pause ();
	changecolor (25,5,0x1f);
	pause ();
	for (c0=0;c0<4;c0++)
		printct ('A'+c0,45+5*c0,5,0x3b,c0);
	pause ();

 //String output functions example
	printsn ("PRINTSN EXAMPLE",10,7);
	prints ("PRINTS EXAMPLE",30,7,0x1e);
	printxy ("PRINTXY EXAMPLE",50,7,2,0,0x1f);
	printx ("PRINTX EXAMPLE\6\33\n\tHIT ANY KEY TO SEE THE TEXT BOX EXAMPLE\25",
		10,8,0x1f);

  //String output inside a box
	setcrtwin (20,12,60,20); //defines box coordinates
	molduradw (0x1F); //draws the box
	fillbarw (' ',0x17); //fills the box internally
	//crtwin_just=CENTER_TEXT; //required for printsj...; CENTER_TEXT is default
	printtext ("PRINTTEXT EXAMPLE",30,0,0x1a);
	printsj ("PRINTSJ EXAMPLE",2,0x1e); //prints text in the center
	printsjc ("PRINTSJC \6\37EXAMPLE",3,0x1b);
	scrpause ();

 //Formatted string output functions example
	fillscr ('°',0x19);
	printsnf (10,2,"PRINTSNF %s",ex_msg);
	printsf (10,3,0x1f,"PRINTSF %s",ex_msg);
	printxyf (10,4,2,0,0x1e,"PRINTXYF %s",ex_msg);
	printxf (10,5,0x1e,"PRINTXF \6%c%s",YELLOW+BKCOLOR*BLUE,ex_msg);
	printx ("HIT ANY KEY TO SEE THE TEXT BOX EXAMPLE\25",10,8,0x1f);

  //Formatted string output example inside a box
	setcrtwin (20,10,60,20); //defines box coordinates
	molduradw (0x1F); //draws the box
	fillbarw (' ',0x17); //fills the box internally
	printtextf (29,0,0x1c,"PRINTTEXTF %s",ex_msg);
	printsjf (3,0x1e,"PRINTSJF AND PRINTSJCF %s",ex_msg);
	printsj ("CURRENT TEXT BOX COORDINATES",4,0x1f);
	printsjcf (5,0x1a,"(defined by crtwin_dta)=\6\%c(%d,%d),(%d,%d)",
		LIGHTCYAN+BKCOLOR*BLUE, crtwin_dta.left, crtwin_dta.top,
		crtwin_dta.right, crtwin_dta.bottom);
	scrpause ();

 //Screen filling functions
	fillscr ('A',0x2b);
	printsj ("- - - fillscr Example - - -",2,0x2f);
	scrpause ();
	crt_clrscr ();
	printsj ("- - - crt_clrscr Example - - -",2,0x0f);
	scrpause ();

 //Text window (or text box) manipulation functions
	fillscr ('°',0x19);
	moldurad(10,12,30,18,0x1f);
	fillbar('þ',11,13,29,17,0x18);
	pause ();
	linhad_hor(15,10,30,0x1e);
	linhad_ver(20,12,18,0x1e);
	barcolor(16,14,24,16,0x1a);
	printc('³',20,20,0x1f);
	mkline_aux(20,20,0,3,0x18);
	pause ();
	for (c0=0;c0<4;c0++)
	 {
		a0=(c0&0x01)*10;
		a1=((c0&0x02)>>1)*3;
		fillbox('Û',11+a0,13+a1,19+a0,14+a1,0x1b,c0);
	 }
	pause ();

  //Text window functions with coordinates given by crtwin_dta
	setcrtwin (50,12,70,18);
	molduradw (0x1e);
	fillbarw ('#',0x19);
	pause ();
	fillboxw (' ',0x1b,2);
	pause ();

  //Titled text box example
	lelecos.title="CRTWIN EXAMPLE"; //box title
	lelecos.tcolor=0x1b; //box title color
	lelecos.fchr='*'; //box internal character
	lelecos.fcolor=0x19; //box internal color
	lelecos.bcolor=0x1e; //border color
	lelecos.btype=1; //border type (same as crtframe)
	setcrtwin (20,5,60,10); //load coordinate values in crtwin_dta
	crtwindow (lelecos); //coordinates are given by crtwin_dta
	scrpause ();

 //Screen data save/restore functions
	savevideo (buffer);
	fillscr (' ',0x07);
	crt_gotoxy(0,0);
	pause ();
	restorevideo (buffer);
	pause ();
	savevideowin(windowbuf,10,12,30,18);
	fillscr (' ',0x07);
	pause ();
	restorevideowin(windowbuf,30,12,50,18);
	pause ();
	licos.left=31; //licos is a crtwin struct
	licos.top=13;
	licos.right=49;
	licos.bottom=17;
	crtwin_dta=licos;  //another way of assigning values to crtwin_dta, instead of using setcrtwin
	savevideow(windowbuf);
	fillscr(' ',0x07);
	pause ();
	crtwin_dta.top-=5;
	crtwin_dta.bottom-=5;
	restorevideow(windowbuf);
	scrpause ();

 //Screen data save/restore functions with mode
	crt_direct=0;
	video_addr=(char *)buffer2; //writes and reads buffer2 as if it where
	restorevideo(buffer);			//video RAM memory (this only happens when crt_direct==0)
	p=(char *)savecrt(buffer,2); //saves first the characters
	savecrt(p,3);	//then the attributes
	p=(char *)savecrtwin(windowbuf,10,12,30,18,2); //same as above, but
	savecrtwin(p,10,12,30,18,3); //now with a text window
	setcrtwin(50,12,70,18); //crtwin_dta=text window coordinates
	savecrtw(windowbuf2,0); //saves in windowbuf2 a text window
	crt_detect(video_adapter); //restores default video RAM address

	fillscr(' ',0x07);
	pause ();
	p=(char *)restorecrt(buffer,2);
	pause ();
	restorecrt(p,3);
	pause ();
	fillscr (' ',0x07);
	pause ();
	p=(char *)restorecrtwin(windowbuf,30,12,50,18,2);
	pause();
	restorecrtwin(p,30,12,50,18,3);
	pause ();
	setcrtwin(30,2,50,8);
	restorecrtw(windowbuf2,1); //ooops, character has been exchanged with color
	pause ();
	restorecrtw(windowbuf2,0); //now OK
	scrpause ();

 //Cursor functions
	fillscr('°',0x19);
	prints ("LOOK AT THE CURSOR = >  ",10,3,0x1f);
	crt_gotoxy(33,3);
	pause ();
	a0=getcursorsh();
	setcursorsh(0x020c);
	pause ();
	setcursorsh(a0);
	pause ();

  //These examples require an EGA (with UltraVision v2+) or VGA+ video adapter
	if (video_adapter>3)
	 {
		fillscr (' ',0x19);
 //Changes palette color
		disppalette (4,4,0x17,0x1f);
		scrpause ();
		for (c0=0;c0<16;c0++)
			setpalreg (c0,c0+16);
		disppalette (4,4,0x17,0x1f);
		scrpause ();
		setcrtmode (3); //a mode change restores default palette colors
 //Changes screen border (overscan) color
		fillscr ('°',0x19);
		a0=getbordercolor ();
		dispbordercolor ();//displays current screen border (overscan) color
		setbordercolor (1);
		dispbordercolor ();
		setbordercolor (15);
		dispbordercolor ();
		setbordercolor (a0);
		dispbordercolor ();
		setcrtmode (3); //a mode change restores default border color
	 }

	//P.S: In MCGA adapters, the palette color (value) is in fact the number
	//of a DAC register, that defines the actual color	for that palette
	//register. So in MCGA adapters, it's possible to change a color without
	//changing palette registers values, but changing the DAC register value
	//associated to that palette register.
	//In VGA+ adapters, the selected DAC register for a palette register is
	//also affected by DAC register paging. The MSB bits of selected DAC
	//register is given by current DAC page value and the LSB bits by the LSB
	//bits of palette register. And there exists two modes of paging,
	//mode 0: where DAC register number is given by the 6 LSB bits of palette value
	//  plus 2 lower bits of current page times 64
	//mode 1: where DAC register number is given by the 4 LSB bits of palette value
	//  plus 4 lower bits by current page times 16
	//Border (overscan) color register works just in the same way as palette
	//registers,except that they are not affected by DAC paging (in VGA+)
	//WARNING: This information may not be accurate.
	//The example below illustrates this.

 //Changes DAC registers values, paging mode, DAC page,...
 //This example requires a VGA+ video adapter
	if (video_adapter==9)
	 {
		fillscr (' ',0x07);
		setbordercolor(0xc0); //changes overscan color (DAC register)
		setpalreg(6,22); //changes palette register 6 DAC register
		disppalette (4,3,0x17,0x1f);
		setcrtwin(-1,-1,vmode_x,vmode_y);
		scrpause ();
		printsj (" - - - PLEASE WAIT, UPDATING VIDEO DAC REGISTERS - - - ",
			vmode_y-1,0x1E);
			//updates the 256 VGA DAC register in a way
			// to create a IIRRGGBB color set.
			// Where I = intensity bit R,G,B=red,green,blue bits
			// normally monitors use IRGB in text mode
		for (c0=0;c0<256;c0++)
		 {
			a0=(c0&0xc0)>>4u;
			setdacreg(c0,((c0&0x30u)>>4u)*17u+a0,
				((c0&0x0Cu)>>2u)*17u+a0,(c0&0x03u)*17u+a0);
		 }
		fillbar (' ',0,24,79,24,7);

		dispdacvalues ();//display DAC registers associated to palette register
			// on the left and their values

		scrpause ();

		for (c0=0;c0<2;c0++)
		 {
			setdacpgmode (c0); //selects DAC register paging mode
				//0 - 4 pages of 64
				//1 - 16 pages of 16
			for (c1=0;c1<16;c1++)
			 {
				if (!c0 && c1==8)
					break;
				setdacpage (c1);
				dispdacvalues ();
				pause ();
			 }
		 }
		setcrtmode (3); //a mode change restores default DAC, palette register values
	 }

 //setchrboxwidth example (requires a VGA+ adapter)
	if (video_adapter==9)
	 {
		fillscr ('°',0x19);
		setchrboxwidth(0);
		prints ("setchrboxwidth => Toggles character box width between 9 and 8 pixels wide",2,10,0x1f);
		prints ("in VGA/SVGA+ video adapters",24,11,0x1f);
		scrpause ();
		setchrboxwidth(1);
		pause ();
		setchrboxwidth(0);
		pause
		();
	 }

 //settextblink example (requires a EGA+ adapter)
	if (video_adapter>2)
	 {
		fillscr (' ',0x17);
		printsj ("settextblink => Toggles intensity/blinking bit",12,0x9e);
		scrpause ();
		settextblink(0);
		pause ();
		settextblink(1);
		pause ();

 //changechar example (requires a EGA+ adapter)
		setcrtmode (3);
		fillscr ('°',0x19);
		printsj ("* * * changechar Example - Default font character shape * * *",0,0x1e);
		dispascii (); //displays all ascii characters on screen
		scrpause ();
		changechar ((unsigned char *)handscr,32,144);
		printsj ("* * * changechar Example - Handscr character shape (20h - AFh) * * *",0,0x1e);
		dispascii ();
		pause ();

 //changechar example 2 (requires a EGA+ adapter)
		changechar_height=12;
		changechar_func=CHANGCHR_RECALC;
		setcrtmode (3);
		changechar((unsigned char *)font12,0,256);
		crt_detect (video_adapter);
		fillscr ('°',0x19);
		printsj ("* * * changechar Example - Loaded font 8x12 * * *",0,0x1e);
		dispascii ();
		scrpause ();

 //changecharg example (requires a EGA+ adapter)
		setcrtmode (6);
		crt_init (video_adapter);
		printsj ("* * * changecharg Example - Default font * * *",0,0x1e);
		dispascii ();
		scrpause ();
		setcrtmode (6);
		changecharg ((unsigned char *)crazy8,25);
		crt_init (video_adapter);
		printsj ("* * * changecharg Example - Crazy (8x8) font (00h - 7Fh) * * *",0,0x1e);
		dispascii ();
		scrpause ();

 //crtfontdisp example (requires a EGA+ adapter)
		setcrtmode (3);
		crt_init (video_adapter);
		changechar_blk=1;
		changechar_height=16;
		changechar_func=CHANGCHR_NORM;
		changechar((unsigned char *)handscr,32,144);
		crtfontspec(1);
		printsj("* * * crtfontspec Example - Two character sets displayed simultaneously * * *",0,0x1e);
		prints ("Character Set in block 0 (Default)",5,2,0x1f);
		prints ("Character Set in block 1",48,2,0x1f);
		for (c0=0;c0<256;c0++)
		 {
			printc(c0,c0%32+5,c0/32+5,0x1f);
			printc(c0,c0%32+45,c0/32+5,0x17);
		 }
		scrpause ();
	 }

 //The end
	setcrtmode (3);
	prints("THANKS FOR SEEING THIS EXAMPLE / DEMO PROGRAM",0,0,0x0E);
	prints("THE END",0,1,0x0F);
	crt_gotoxy(0,2);
	return(0);
 }
}

//By M rcio Afonso Arimura Fialho
//http://pessoal.iconet.com.br/jlfialho
//e**-mail: jlfialho@iconet.com.br OR jlfialho@yahoo.com (alternate)
