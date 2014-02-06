/* FONTEDIT ver 1.3 - Edits DOS character font files (.FNT)         */
/* Freeware version                                                 */
/* By Marcio Afonso Arimura Fialho                                  */
/* http://pessoal.iconet.com.br/jlfialho                            */
/* e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com */

//To select edited .FNT files, use FONTSEL

#include <stdio.h>
#include <crt.h>
#include <stdlib.h>
#include <string.h>
#include <ciolib.h>
#include <setjmp.h>
#include <mouse.h>
//#include <dos.h>

#define EGA 3 //EGA color adapters
#define VGA 9 //VGA/SVGA+ color adapters

#define intm(intnum) asm int intnum

typedef unsigned char byte;
typedef unsigned int  word;
typedef unsigned long dword;

//Disables CTRL-C

jmp_buf ctrl_c;

int c_break(void)
 {
	longjmp (ctrl_c,1);
	return 1;
 }

//Mouse routines:

int  mouserst (int *nbuttons); //resets and checks for mouse
void mouseon (void); //turns on mouse pointer
void mouseread(int *collum, int *line, int *buttom); //reads mouse position and button
void mousewrite (int collum, int line); //changes mouse pointer position
void mouseoff(void); //switch off mouse pointer

//Miscellaneous routines:
int  getchx(); //enhaced version of getch, reads extended characters
void readfname (char *target, char *source, char *ext);
	//reads filename and appends extension (ext) if none is given.

//Global variables
byte *buffer;
byte *aux,*aux2;
byte schar=0;
int c_height;
FILE *source;
byte bufchar[32]; //used by cut and paste tool (clipboard)

int prog_flags=0x19; //bit 0 => if set redraws miniature
							//bit 1 => if set means that font in memory has been changed
							//bit 2 => if set means that exit dialog box is active (user has pressed ESC key)
							//bit 3 => if set displays character set, thus allowing a character to be selected with mouse
							//bit 4 => if set displays clipboard
							//bit 5 => if set means mouse installed

#define target source  //the source file is also the target file

//Specific program routines
void redraw(); //redraws character pattern
void redrawmin(); //redraws the miniature of character pattern
void redrawclip(); //redraws the clipboard (displayed as a miniature)
void redrawascii(); //draws ascii character set
void filesave(char *name); //saves font file
void copychr();  //copyies the pattern of current character to clipboard
void pastechr(); //pastes the pattern stored in clipboard over current character pattern
void invertchr(); //inverts the pattern of current character
void cleanchr(); //clears the pattern of current character
void movepos(int dir); //moves the pattern of current character
void flipver (); //flips the pattern of current character vertically
void fliphor (); //flips the pattern of current character horizontally
void drawbtnsh (int c1, int c2, int xi, int xf); //draws the shadow of buttons in exit dialog window

void fontedithelp(); //fontedit help

#define E_XI 4 //pattern edit window coordinates
#define E_YI 5
#define M_XI 58 //miniature window coordinates
#define M_YI 10
#define B_XI 70 //clipboard window coordinates
#define B_YI 10
#define A_XI 18 //character set window coordinates
#define A_YI 8

	//coordinates for brief help window
#define BH_XI 16
#define BH_YI (vmode_y-20)
#define BH_XF 76
#define BH_YF (vmode_y-2)

  //coordinates for exit window dialog box
#define X_XI 18
#define X_YI 18
#define X_XF 65
#define X_YF 28

  //coordinates for help window
#define H_XI 20
#define H_YI 20
#define H_XF 60
#define H_YF 26

extern "C" {
int main (int n, char *ent[3])
  {
	int c0;
	int a0,a1,a2;
	int mbuttons; //number of mouse buttons
	int inpkey, mouseb=0, mousex=0, mousey=0;
	int mouseb2,mousex2, mousey2;
	char *msg_about="\nType FONTEDIT with no parameters to obtain help\n";
	char licos[128];
	char screenbuf[2*(1+X_XF-X_XI)*(1+X_YF-X_YI)];
	int cont;

	struct crtwin_inp exitmsgbox={"Message",0x7e,' ',0x18,0x7f,1};
/*  The above line is equivalent to the lines below
	struct crtwin_inp exitmsgbox;
	exitmsgbox.title="Message"; //title
	exitmsgbox.tcolor=0x7e; //title color
	exitmsgbox.fchr=' '; //fill character
	exitmsgbox.fcolor=0x18; //fill color
	exitmsgbox.bcolor=0x7f; //border color
	exitmsgbox.btype=1; //border type, same of crtframe
	*/

	struct crtwin_inp helpmsgbox={"Brief Help",0x1e,' ',0x18,0x1f,1};

	char *helpplus[]={"",", mouse\05\36*, enter\05\36*",
"","/mouse buttons","text mode","mouse",
"","\n\t\05\36* Works only when character set window is visible",
" ","\n",".\n","acter "};

	//Disables CTRL-C and assings for it a new function
//	ctrlbrk(c_break);
	if(setjmp(ctrl_c))
	 {
		crt_gotoxy (0,0);
		fillbar ('°',0,0,2,2,0x19);
		copychr ();
		goto pula2;
	 }

	if (n<2)
	 {
		printf ("FONTEDIT ver 1.3 - EDITS DOS CHARACTER FONT FILES (.FNT)\n\n\
Usage:   FONTEDIT <font name[.FNT]> [height]\n\n\
\theight is the character height in pixels  (valid: 1 - 32, default = 16)\n\
\nBy Marcio Afonso Arimura Fialho\nhttp://pessoal.iconet.com.br/jlfialho\n\
e-mail: jlfialho@iconet.com.br\n");
		return(1);
	 }

	if (n>=3) //reads the character height
	 {
		c_height=atoi(ent[2]);
		if((unsigned)c_height>32u)
		 {
			printf ("ERROR: Incorrect input parameters.%s",msg_about);
			return(1);
		 }
	 }
	 else
		c_height=16;

	readfname (licos,ent[1],".FNT");//appends the .FNT extension to licos
	  //if none is given

	source=fopen (licos,"rb");
	if(source==NULL)
	 {
		printf ("ERROR: File \"%s\" doesn't exist or couldnt be read.%s",licos,msg_about);
		fcloseall();
		return(1);
	 }

	buffer=(byte *)malloc(256*c_height); //allocates memory for character font

	if (buffer==NULL) //if not possible to allocate memory
	 {     //displays error message and returns
		printf ("ERROR: Not enough memory to store font.%s",msg_about);
		fcloseall ();
		return(1);
	 }
	for (aux=buffer,cont=0;cont<c_height*256;cont++) //loads character font into buffer
	 {
		*aux=(byte)fgetc (source);
		aux++;
	 }
	fclose (source);

 //detects mouse
	if (mouserst(&mbuttons)) //mouse installed
		prog_flags|=0x0020;
	 else //mouse not installed (mousex and mousey becomes text mode cursor coordinates)
	 {
		mousex=E_XI+1;
		mousey=E_YI+1;
	 }

 //Set mode and draws the background
	setcrtmode (3);
	textmode (64);
	crt_init (VGA); //if your monitor is EGA, replace crt_init argument
	crtfontspec(1); //enables the miniature to be displayed
	setpalreg(8,7); //makes the color 8 (dark gray) be light gray.
	setpalreg(13,0); //makes the color 13 (light magenta) be black.
	fillscr ('°',0x19);

  //Display messages on screen
	printsj ("* * * FONTEDIT ver 1.3 * * *",0,0x1e);
	printsj ("DOS character font editor for EGA/VGA/SVGA+ videos",2,0x18);

  //Brief Help window
	if (prog_flags&0x0020)
		a0=1;
	 else
		a0=0;
	setcrtwin (BH_XI,BH_YI,BH_XF,BH_YF);
	crtwindow(helpmsgbox);
	printxf (0,BH_YI+1,0x1f,"\21%c\n\
\tESC\06\30 - Exit\t\06\37F1\06\30 - Help\t\06\37F2\06\30 - Save\n\
\n\
\06\37PG-UP/PG-DN, CTRL-(PG-UP/PG-DN), ALT-\06\36\30\31\32\33\33\06\37\
%s\06\30%s- Select char%sto edit\n\
\06\37+/-/ENTER%s\06\30 - Draw character shape\n\
\06\37Arrows (\06\36\30\31\32\33\33\5\37)\6\30 - Move %s cursor\n\
\06\37Ctrl-C/Ins\06\30 - Copy character\t\06\37Ctrl-V\06\30 - Paste character\n\
\06\37Alt-I\06\30 - Invert character\t\06\37ALT-C\06\30 - Clear character\n\
\06\37Alt-V\06\30 - Flip Vertically \t\06\37Alt-H\06\30 - Flip Horizontally\n\
\06\37Alt-R\06\30 - Rotate 180 degrees (let pattern upside down)\n\
\06\37Ctrl-\06\36\30\31\32\33\33\06\30 - Move character pattern\n\
\06\37Alt-M\06\30 - Toggle On/Off \5\33Miniature display\n\
\06\37Alt-B\06\30 - Toggle On/Off clip\5\33Board display\n\
\06\37Alt-A\06\30 - Toggle On/Off ch\5\33Aracter set display\n\%s",BH_XI+2,
helpplus[a0],helpplus[a0+8],helpplus[a0+10],helpplus[a0+2],helpplus[a0+4],helpplus[a0+6]);

	moldurad (E_XI,E_YI,E_XI+9,E_YI+1+c_height,0x1f);
	printc ('1',E_XI+2,E_YI,0x1f);
	redraw();

	if (prog_flags&0x0020)
		mouseon ();
	do
	 {
		pula3:
		if (prog_flags&0x0002)
			printc (15,E_XI+2,E_YI+1+c_height,0x1f);
		 else
			printc ('Í',E_XI+2,E_YI+1+c_height,0x1f);

		if (kbhit()) //if is there a keystroke available and the control is not
			inpkey=getchx (); //exclusive of mouse
		 else
			inpkey=0;

		mouseb2=mouseb;
		mousex2=mousex;
		mousey2=mousey;
		if (!(prog_flags&0x0004)) //if the exit dialog window is not active (program running normally)
		 {
			switch (inpkey)
			 {
				case 0x1b: //if user hits esc key
					if (!(prog_flags&0x0002))
						goto fim; //if font not changed since last save

				  //draws exit window message box (if font changed and not saved)
					setcrtwin (X_XI,X_YI,X_XF,X_YF);
					if (prog_flags&0x0020)
						mouseoff();
					savevideow (screenbuf);
					crtwindow (exitmsgbox);
					fillbar (' ',X_XI+1,X_YI+7,X_XF-1,X_YF-1,0x7d);
					window(X_XI+2,X_YI+2,X_XF,X_YF);
					crtwin_just=0;
					printsj("WARNING:",0,0x1e);
					textattr(0x1e);
					gotoxy (4,2);
					cprintf("%s\15\12 not saved. Save?",licos);
					crt_gotoxy(0,0);

				  //draws the shadow of buttons
					drawbtnsh('ß','Ü',X_XI+6,X_XI+10);
					drawbtnsh('ß','Ü',X_XI+16,X_XI+19);
					drawbtnsh('ß','Ü',X_XI+31,X_XI+38);

				  //draws the text of buttons
					printx (" \5\57Yes ",X_XI+5,X_YI+8,0x2e);
					printx (" \5\57No ",X_XI+15,X_YI+8,0x2e);
					printx (" \5\57Cancel ",X_XI+30,X_YI+8,0x2e);
					if (prog_flags&0x0020)
						mouseon();

				  //informs that the exit window dialog box is active
					prog_flags|=0x0004;
				 goto pula3;

				case 0x3b00: fontedithelp(); break;
				case 0x3c00: filesave(licos); break;
				case 0x4800: mousey--; goto pula1;
				case 0x5000: mousey++; goto pula1;
				case 0x4b00: mousex--; goto pula1;
				case 0x4d00: mousex++; goto pula1;

				case 0x3000: prog_flags^=0x0010; redraw(); goto pula2; //clipboard displayment
				case 0x1e00: prog_flags^=0x0008; redraw(); goto pula2; //ascii character set window (with default patterns) displayment
				case 0x3200: prog_flags^=0x0001; redraw(); goto pula2; //miniature displayment

				case 0x9d00: //ALT + right
				case 0x4900: schar++; goto pula2; //PG UP
				case 0x9b00: //ALT + left
				case 0x5100: schar--; goto pula2; //PG DN
				case 0xA000: schar+=16; //ALT + down
				case 0x8400: schar+=16; goto pula2; //CTRL PG UP
				case 0x9800: schar-=16; //ALT + up
				case 0x7600: schar-=16; goto pula2; //CTRL PG DN
				case 0x9200: copychr(); goto pula2; //CTRL INS
				case 0x03  : copychr(); goto pula2; //CTRL-C (Don't need setjmp!)
				case 0x16  : pastechr(); goto pula2; //CTRL-V
				case 0x1700: invertchr(); goto pula2; //ALT-I
				case 0x2e00: cleanchr(); goto pula2;  //ALT-C
				case 0x8d00: movepos(2); goto pula2;  //CTRL up
				case 0x7400: movepos(0); goto pula2;  //CTRL right
				case 0x9100: movepos(3); goto pula2;  //CTRL down
				case 0x7300: movepos(1); goto pula2;  //CTRL left
				case 0x2F00: flipver(); goto pula2; //ALT V
				case 0x2300: fliphor(); goto pula2; //ALT H
				case 0x1300: flipver(); fliphor (); goto pula2; //ALT R

				pula1:
					if (prog_flags&0x0020)
						mousewrite (mousex*8, mousey*8);
					 else
					 {
						if (mousex<=E_XI)
							mousex+=8;
						if (mousey<=E_YI)
							mousey+=c_height;
						if (mousex>(E_XI+8))
							mousex-=8;
						if (mousey>(E_YI+c_height))
							mousey-=c_height;
					 }
				 break;
				pula2: redraw();
			 }
			if (prog_flags&0x0020)
			 {
				mouseread (&mousex,&mousey,&mouseb);
				mousex=mousex/8;
				mousey=mousey/8;
			 }
			 else
				crt_gotoxy(mousex,mousey);
			if ( (!mouseb2 && mouseb) || inpkey=='+' || inpkey=='-' || inpkey==0x0d || (mouseb && (mousex!=mousex2 || mousey!=mousey2)))
				//if is to change something
			 {
				if (mousex>E_XI && mousex<(E_XI+9) && mousey>E_YI &&
				mousey<=(E_YI+c_height))
				 {
					a0=mousey-E_YI-1;
					a1=E_XI+8-mousex;
					a2=1;
					for(c0=0;c0<a1;c0++,a2*=2);
					aux=buffer + schar * c_height + a0;
					if (inpkey=='+' || mouseb==0x01)
					 {
						prog_flags|=0x0002;
						(*aux)|=a2;
					 }
					if (inpkey=='-' || mouseb==0x02)
					 {
						prog_flags|=0x0002;
						(*aux)&=~a2;
					 }
					if (inpkey==0x0d || mouseb==0x03)
					 {
						prog_flags|=0x0002;
						(*aux)^=a2;
					 }
					redraw();
					//redraws the edit, character set and miniature windows.
				 }

				//if character set window is active and
				//mouse is inside character set window and left button of mouse or enter key has been pressed.
				//the character under mouse cursor is selected
				if (prog_flags&0x0008 && mousex>A_XI && mousex<(A_XI+33) &&
				mousey>A_YI && mousey<(A_YI+9) && (mouseb&0x01 || inpkey==0x0d))
				 {
					schar=(mousex-A_XI-1)+(mousey-A_YI-1)*32;
					redraw(); //redraws the edit, character set and miniature windows.
				 }

			 }
		 }
		else //if the exit dialog window is active
		 {
			if (prog_flags&0x0020)
			 {
				mouseread (&mousex,&mousey,&mouseb);
				mousex=mousex/8;
				mousey=mousey/8;
			 }
			if (mousey!=mousey2 || mousex!=mousex2 || mouseb!=mouseb2)
			 {
				if (prog_flags&0x0020)
					mouseoff ();
				drawbtnsh('ß','Ü',X_XI+6,X_XI+10);
				drawbtnsh('ß','Ü',X_XI+16,X_XI+19);
				drawbtnsh('ß','Ü',X_XI+31,X_XI+38);

				if (mousey==X_YI+8)
				 {
					if (mousex>=X_XI+5 && mousex<=X_XI+9 && mouseb&0x01)
						drawbtnsh(' ',' ',X_XI+6,X_XI+10);
					if (mousex>=X_XI+15 && mousex<=X_XI+18 && mouseb&0x01)
						drawbtnsh(' ',' ',X_XI+16,X_XI+19);
					if (mousex>=X_XI+30 && mousex<=X_XI+37 && mouseb&0x01)
						drawbtnsh(' ',' ',X_XI+31,X_XI+38);
					if (!(mouseb&0x01) && (mouseb2&0x01))
					 {
						if (mousex>=X_XI+5 && mousex<=X_XI+9)
							inpkey='Y';
						else if (mousex>=X_XI+15 && mousex<=X_XI+18)
							inpkey='N';
						else if (mousex>=X_XI+30 && mousex<=X_XI+37)
							inpkey=0x1b;
					 }
				 }
				if (prog_flags&0x0020)
					mouseon();
			 }

			switch (inpkey)
			 {
				case 'Y':
				case 'y': filesave(licos); goto fim;
				case 'N':
				case 'n': goto fim;
				case 0x1b:
				case 'C':
				case 'c':
					if (prog_flags&0x0020)
						mouseoff ();
					restorevideowin(screenbuf,X_XI,X_YI,X_XF,X_YF);
					if (prog_flags&0x0020)
						mouseon ();
					prog_flags&=0xFFFB;
					break;
			 }
		 }
	 }
	while (1);

  fim: //end of the program
  if (prog_flags&0x0020)
		mouseoff ();
	setcrtmode (3); //a mode set restores pallete colors and screen defaults.
	fcloseall();
	return(0);
 }
}

void redraw()
 {
	int a0,a2;
	int c0,c1,c2;
	if (prog_flags&0x0020)
		mouseoff ();
	for (c0=0;c0<c_height;c0++)
		for (c1=0;c1<8;c1++)
		 {
			a2=1;
			for(c2=0;c2<c1;c2++,a2*=2); //a2=2^c1
			a0=(*(buffer + schar * c_height + c0))&a2;
			if (a0)
				printc('Û',E_XI+8-c1,E_YI+1+c0,0x1f);
			else
				printc ('ú',E_XI+8-c1,E_YI+1+c0,0x1f);
		 }
	printsf (50,5,0x1f,"1 - Character => %02Xh = %03d",schar,schar);
	printsf (54,6,0x1f,"Default pattern = ' '");
	printc (schar,73,6,0x1e);
	if (prog_flags&0x0001)
		redrawmin();
	 else
		fillbar ('°',M_XI-2,M_YI-2,M_XI+7,M_YI+4+(c_height-1)/8,0x19);
	if (prog_flags&0x0010)
		redrawclip();
	 else
		fillbar ('°',B_XI-2,B_YI-2,B_XI+7,B_YI+4+(c_height-1)/8,0x19);
	if (prog_flags&0x0008)
		redrawascii();
	 else
		fillbar ('°',A_XI,A_YI,A_XI+33,A_YI+9,0x19);
	if (prog_flags&0x0020)
		mouseon ();
 }

void redrawmin()
 {
	int c0;
	int a0;
	char chrdisp[32];
	a0=(c_height-1)/8;
	prints ("Miniature:",M_XI-2,M_YI-2,0x1f);
	moldurad (M_XI,M_YI,M_XI+4,M_YI+4+a0,0x1f);
	fillbar (' ',M_XI+1,M_YI+1,M_XI+3,M_YI+3+a0,0x1f);

	for (c0=0;c0<c_height;c0++)
		chrdisp[c0]=*(buffer+schar*c_height+c0);
	for (;c0<32;c0++)
		chrdisp[c0]=0;
	changechar_height=8;
	changechar_blk=1;
	changechar ((unsigned char *)chrdisp,0,4);
	for (c0=0;c0<=a0;c0++)
		printc (c0,M_XI+2,M_YI+2+c0,0x17);
 }

void redrawclip()
 {
	int c0;
	int a0;
	char chrdisp[32];
	a0=(c_height-1)/8;
	prints ("clipBoard:",B_XI-2,B_YI-2,0x1f);
	moldurad (B_XI,B_YI,B_XI+4,B_YI+4+a0,0x1f);
	fillbar (' ',B_XI+1,B_YI+1,B_XI+3,B_YI+3+a0,0x1f);
	for (c0=0;c0<c_height;c0++)
		chrdisp[c0]=*(bufchar+c0);
	for (;c0<32;c0++)
		chrdisp[c0]=0;
	changechar_height=8;
	changechar_blk=1;
	changechar ((unsigned char *)chrdisp,4,4);
	for (c0=0;c0<=a0;c0++)
		printc (c0+4,B_XI+2,B_YI+2+c0,0x17);
 }

void redrawascii()
 {
	int c0;
	struct crtwin_inp asciiwindow={"CHAR. SET (DEFAULT PATTERNS)",
	0x1e,' ',0x19,0x1f,1};
	setcrtwin(A_XI,A_YI,A_XI+33,A_YI+9);
	crtwindow(asciiwindow);
	for (c0=0;c0<256;c0++)
		printc(c0,A_XI+(c0%32)+1,A_YI+(c0/32)+1,0x1b);
	changecolor(A_XI+(schar%32)+1,A_YI+(schar/32)+1,0x2e);
 }

void filesave(char *name)
 {
	int c0;
	target=fopen (name,"wb");
	if(target==NULL)
	 {
		prints ("ERROR: File can't be open for writing.",15,30,0x9e);
		putchar (0x07);
		getchx ();
		fillbar ('°',0,30,79,30,0x19);
		fcloseall();
		return;
	 }
	prog_flags&=0xFFFD;
	aux=buffer;
	for (c0=0;c0<c_height*256;c0++,aux++)
		fputc(*aux,target);
	fseek (target,-1,SEEK_END);
	fclose(target);
 }

void copychr()
 {
	int c0;
	aux=buffer + schar * c_height;
	aux2=bufchar;
	for (c0=0;c0<c_height;c0++,aux2++,aux++)
		*aux2=*aux;
 }

void pastechr()
 {
	int c0;
	prog_flags|=0x0002;
	aux=buffer + schar * c_height;
	aux2=bufchar;
	for (c0=0;c0<c_height;c0++,aux2++,aux++)
		*aux=*aux2;
 }

void invertchr()
 {
	int c0;
	prog_flags|=0x0002;
	aux=buffer + schar * c_height;
	for (c0=0;c0<c_height;c0++,aux++)
		*aux=~(*aux);
 }

void cleanchr()
 {
	int c0;
	prog_flags|=0x0002;
	aux=buffer + schar * c_height;
	for (c0=0;c0<c_height;c0++,aux++)
		*aux=0;
 }

void movepos(int dir)
 {
	int c0;
	aux=buffer + schar * c_height;
	if (dir<4)
	 {
		prog_flags|=0x0002;
		for (c0=0;c0<c_height;c0++,aux++)
		 {
			if (dir==0)	//moves to the right
				*aux=(*aux)/2;
			else if (dir==1) //moves to the left
				*aux=(*aux)*2;
			else if (dir==2) //moves to up
			 {
				if (c0!=(c_height-1))
					*aux=*(aux+1);
				else
					*aux=0;
			 }  		  }
	 }
	if(dir==3) //moves to down
	 {
		aux--;
		prog_flags|=0x0002;
		for (c0=c_height-1;c0>=0;c0--,aux--)
		 {
			if (c0)
				*aux=*(aux-1);
			else
				*aux=0;
		 }
	 }
 }

void flipver ()
 {
	int c0;
	char a0;
	prog_flags|=0x0002;
	aux=buffer + schar * c_height;
	for (c0=0;c0<(c_height/2);c0++)
	 {
		a0=*(aux+c_height-1-c0);
		*(aux+c_height-1-c0)=*(aux+c0);
		*(aux+c0)=a0;
	 }
 }

void fliphor ()
 {
	int c0,c1;
	int a0,a1;
	prog_flags|=0x0002;
	aux=buffer + schar * c_height;
	for (c0=0;c0<c_height;c0++,aux++)
		for (a1=1,c1=0;c1<4;c1++)
		 {
			if ((*aux)&a1)
				a0=1;
			 else
				a0=0;
			if ((*aux)&(0x80/a1))
				(*aux)|=a1;
			 else
				(*aux)&=~a1;
			if (a0)
				(*aux)|=(0x80/a1);
			 else
				(*aux)&=~(0x80/a1);
			a1*=2;
		 }
 }


void drawbtnsh (int c1, int c2, int xi, int xf)
	//draws the shadow of buttons in exit dialog window
 {
	fillbar (c1,xi,X_YI+9,xf,X_YI+9,0x7d);
	printc (c2,xf,X_YI+8,0x7d);
 }

void fontedithelp () //fontedit help window
 {
	struct crtwin_inp helpwindow={"Message",0x1e,' ',0x18,0x1f,1};
	char temp[2*(1+H_XF-H_XI)*(1+H_YF-H_YI)];
	setcrtwin(H_XI,H_YI,H_XF,H_YF);
	savevideow(temp);
	crtwindow(helpwindow);
	if (prog_flags&0x0020)
		mouseoff();
	printsjc("For better help, read \6\36FONTEDIT.DOC",1,0x1f);
	printsj("- - - Hit any key to continue - - -",4,0x1a);
	getchx();
	restorevideow(temp);
	if (prog_flags&0x0020)
		mouseon();

 }


/* MOUSE ROUTINES */
int ciomouse_x=0,ciomouse_y=0,ciomouse_buttons=0;

int mouserst (int *nbuttons)
 {
 	if(nbuttons)
		*nbuttons=3;
 	return(3);
 }

extern int mouse_events;

void mouseon (void)
 {
	ciomouse_setevents((1<<CIOLIB_MOUSE_MOVE)
			|(1<<CIOLIB_BUTTON_1_PRESS)
			|(1<<CIOLIB_BUTTON_2_PRESS)
			|(1<<CIOLIB_BUTTON_3_PRESS)
			|(1<<CIOLIB_BUTTON_1_RELEASE)
			|(1<<CIOLIB_BUTTON_2_RELEASE)
			|(1<<CIOLIB_BUTTON_3_RELEASE));
	showmouse();
 }

void mouseread(int *collum, int *line, int *buttom)
 {
	if(collum)
		*collum=ciomouse_x*8;
	if(line)
		*line=ciomouse_y*8;
	if(buttom)
		*buttom=ciomouse_buttons;
 }

void mousewrite (int collum, int line)
 {
	// TODO: Warp Mouse
 }

void mouseoff(void)
 {
	ciomouse_setevents(0);
	hidemouse();
 }

int getchx()
 {
	int i;
	i=getch();
	if(i==0 || i==0xe0)
		i=getch()<<8;
	if(i==CIO_KEY_MOUSE) {
		struct mouse_event mevent;

		getmouse(&mevent);
		ciomouse_x=mevent.endx-1;
		ciomouse_y=mevent.endy-1;
		ciomouse_buttons=mevent.bstate;
		/* Swap Second and Third mouse buttons */
		ciomouse_buttons = (ciomouse_buttons & 0x01)
							| ((ciomouse_buttons & 0x02) << 1)
							| ((ciomouse_buttons & 0x04) >> 1);
	}
	return(i);
 }

#include "readname.cpp"
