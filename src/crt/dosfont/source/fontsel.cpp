/* FONTSEL - Selects temporarily another DOS character font         */
/* Freeware version                                                 */
/* By Marcio Afonso Arimura Fialho                                  */
/* http://pessoal.iconet.com.br/jlfialho                            */
/* e-mail: jlfialho@iconet.com.br or (alternate) jlfialho@yahoo.com */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <crt.h>
#include <ctype.h>
#include <conio.h>

#define EGA 3 //EGA color video adapters
#define VGA 9 //VGA/SVGA+ color video adapters

#define VIDEOADAPTER VGA
	//if your adapter is EGA, replace VIDEOADAPTER

int changechar_rows=25;

int chrup(int chr) //upcases input letter
 {
	if (chr>0x60 && chr<0x7b)
		chr-=0x20;
	return chr;
 }

int asctoi (int *rtn, char *s, unsigned radix)
//converts a string (in base "radix") to integer
//radix must be between 2 and 36, inclusive.
 {
	unsigned register a0; //converted value
	unsigned register a1,a2; //auxiliar variables
	a0=0;
	if (radix<2 || radix >36)
	 {
		*rtn=a0;
		return -3; //radix value is incorrect
	 }
	while (*s)
	 {
		a1=toupper (*s);
		if(a1>='0' && a1<='9')
			a1-='0';
		else if(a1>='A' && a1<='Z')
			a1-='7';
		else
		 {
			*rtn=a0;
			return -1; //unexpected character
		 }
		if (a1>=radix)
		 {
			*rtn=a0;
			return -2; //digit is out of range
		 }
		a2=a0;
		a0*=radix;
		a0+=a1;
		if ((a0/radix)!=a2)
		 {
			*rtn=a0;
			return -4; //overflow
		 }
		s++;
	 }
	*rtn=a0;
	return 0;
 }
// rtn is the converted value (until the point the error occured).
// hextoi returns 0 if sucessful. On error returns:
// -1 -> unexpected character in "s"
// -2 -> digit is out of range given by radix
// -3 -> radix value is incorrect
// -4 -> overflow

int hextobin (int c) //converts an hexadecimal character to integer
 {
	c=chrup (c);
	if(c>='0' && c<='9')
		return c-'0';
	if(c>='A' && c<='F')
		return c-('A'-10);
	return -1; //error code
 }

#include "readname.cpp"

unsigned char *buffer;

void main (int n, char *ent[4])
 {
	int c0,c1;
	int cm;
	int a0,a1;
	int flags=0;
	int progflag=0;
	int newmode; //new video mode
	long src_offset=0; //start loading font from offset in font file
	int startpos=0; //index of the first character to be replaced
	int endpos=255; //index of the last character to be replaced
	int blkspec=0; //value for block specifier in call to crtfontspec
	FILE *source;
	int cont;
	char licos[128];
	char temp [128];
	char *errormsg="\nType\tFONTSEL /?\t for help\n";
	char *errormsg2="\nERROR: Invalid ";
	unsigned char *aux;
	char c;

	printf ("FONTSEL ver 1.7 - DOS FONT SELECTOR - By M rcio Afonso Arimura Fialho\n");
	if (n<2)
	 {
		printf (errormsg);
		return;
	 }

	changechar_height=16;

	for (cm=1;cm<n;cm++)
	 {
		if (*ent[cm]=='/')
		 {
			c=chrup(*(ent[cm]+1));
			switch (c)
			 {
				case 'I': flags|=0x0001; break;
				case '8': flags|=0x000a; break;
				case '9': flags|=0x0008; break;
				case 'A':
				case 'D': flags|=0x0010; break;
				case 'R': flags|=0x0020; break;
				case 'M':
					flags|=0x0040;
					a0=strlen(ent[cm]);
					if (a0>2)
					 {
						if (asctoi(&newmode,ent[cm]+2,16))
							goto m_error;
						if (newmode&0xFF00) //if (unsigned)a1>256
							goto m_error;
					 }
					 else
					 {
					  m_error:
						printf ("%smode value. Expected => (/M0 - /MFF)hex%s",errormsg2,errormsg);
						return;
					 }
				 break;
				case 'G': flags|=0x0080; break;
				case 'L':
					flags|=0x0100;
					a0=strlen(ent[cm]);
					if (a0>2)
					 {
						if (asctoi(&changechar_rows,ent[cm]+2,10))
							goto l_error;
						if (changechar_rows&0xFF00)
							goto l_error;
					 }
					 else
					 {
					  l_error:
						printf ("%svalue for rows. Expected => (/L0 - /L255)%s",errormsg2,errormsg);
						return;
					 }
					break;
				case 'H':
					a0=strlen(ent[cm]);
					if (a0==2)
					 {
						printf ("\nWarning: Box height assumed to be 16 (default)");
						break;
					 }

					if (asctoi(&changechar_height,ent[cm]+2,10))
						goto h_error;
					if (changechar_height&0xFF00)
						goto h_error;
					break;
				  h_error:
					printf ("%sbox height. Expected => (/H0 - /H255)%s",errormsg2,errormsg);
					return;

				case 'O':
					a0=strlen(ent[cm]);
					if (a0==2)
					 {
						printf ("\nWarning: Offset assumed to be 0 (default).");
						break;
					 }
					for (c0=2;c0<a0;c0++)
					 {
						a1=*(ent[cm]+c0);
						if (a1<'0' || a1>'9')
							goto o_error;
						src_offset*=10L;
						src_offset+=(long)(a1-'0');
					 }
					ltoa(src_offset,temp,10);
					if(strcmp(temp,ent[cm]+2))
					 {
					  o_error:
						printf ("%soffset value. Expected => (/O0 - /O2147483647)%s",errormsg2,errormsg);
						return;
					 }
				 break;
				case 'S':
					a0=strlen(ent[cm]);
					if (a0>2)
					 {
						if (asctoi(&startpos,ent[cm]+2,16))
							goto s_error;
						if (startpos&0xFF00)
							goto s_error;
					 }
					 else
					 {
					  s_error:
						printf ("%svalue for the index of the first character to be loaded.\nExpected => (/S0 - /SFF)%s",errormsg2,errormsg);
						return;
					 }
				 break;
				case 'E':
					a0=strlen(ent[cm]);
					if (a0>2)
					 {
						if (asctoi(&endpos,ent[cm]+2,16))
							goto e_error;
						if (endpos&0xFF00)
							goto e_error;
					 }
					 else
					 {
					  e_error:
						printf ("%svalue for the index of the last character to be loaded.\nExpected => (/E0 - /EFF)%s",errormsg2,errormsg);
						return;
					 }
				 break;
				case 'F':
					a0=chrup(*(ent[cm]+2));
					switch (a0)
					 {
						case 'H':
						case 'h': flags|=0x0200; break;
						case 'V':
						case 'v': flags|=0x0400; break;
						default:	goto unknow_op;
					 }
				 break;
				case 'C':
					flags|=0x0800;
					a0=strlen(ent[cm]);
					if (a0>2)
					 {
						if (asctoi(&blkspec,ent[cm]+2,16))
							goto c_error;
						if (blkspec&0xFF00)
							goto c_error;
					 }
					 else
					 {
					  c_error:
						printf ("%svalue for the block specifier.\nExpected => (/C0 - /CFF)%s",errormsg2,errormsg);
						return;
					 }
				 break;
				case 'B':
					a0=strlen(ent[cm]);
					if (a0>2)
					 {
						if (asctoi(&changechar_blk,ent[cm]+2,16))
							goto b_error;
						if (changechar_blk&0xFF00)
							goto b_error;
					 }
					 else
					 {
					  b_error:
						printf ("%svalue for the block to load font.\nExpected => (/B0 - /BFF)%s",errormsg2,errormsg);
						return;
					 }
				 break;
				hlp:
				case '?':
					printf ("\
 * * * FREEWARE VERSION - MAY BE DISTRIBUTED FREELY * * *\n\
Syntax: FONTSEL [option [option...]] <fontname[.fnt]> [option [option...]]\n\n\
fontname is the file containing the DOS-FONT in raw format (no control data)\n\
\t(default extension => .fnt)\n\
options:\n\
/I\t   => selects the negative of the font\n\
/8\t   => box width 8 (VGA/SVGA only)\n\
/9\t   => box width 9 (VGA/SVGA only)\n\
/D or /A   => displays the font's character set\n\
/R\t * => recalculates the box height\n\
/M??\t   => selects a video mode (must be in hexadecimal notation)\n\
/G\t   => use when graphics mode is selected\n\
/L???\t   => selects number of scrool lines (valid => 0-255)\n\
/H???\t   => font character height (default=16 valid => 0-255)\n\
/O???...   => load font file from offset (default=0 valid => 0-2147483647)\n\
/S??\t * => first character to be loaded (valid => 0-FF (in hexadecimal))\n\
/E??\t * => last character to be loaded (valid => 0-FF (in hexadecimal))\n\
/FH\t   => Flips patterns Horizontally\n\
/FV\t   => Flips patterns Vertically\n\
/B??\t * => Block to load font patterns (valid => 0-FF (in hexadecimal)\n\
/C??\t * => Block (Font) specifier (controls the display of fonts\n\
\t      stored in video's RAM) (valid => 0-FF (in hexadecimal))\n\
\t\t- - - Hit any key to continue - - -");
					getch ();
					printf("\n\n\
/?\t   => this help screen\n\n\
\t\t * => disabled when option /G is selected\n\n\
\tFor further help, read DOSFONT.TXT\n");
					return;
				unknow_op:
				default:
					printf ("\nERROR: Unknow option%s",errormsg);
					return;
			 }
		 }
		 else
		 {
			if (progflag&0x0001)
			 {
				printf ("\nERROR: Unexpected char '%c' in command line%s",*(ent[cm]),errormsg);
				return;
			 }
			progflag|=0x0001;
			readfname (licos,ent[cm],".FNT"); //appends extension .FNT in file
							  //name, if filename has no extension
		 }
	 }
	if (!(progflag&0x0001))
	 {
		printf ("\nERROR: No font file supplied%s",errormsg);
		return;
	 }

	buffer=(unsigned char*)malloc(256*changechar_height);
	source=fopen (licos,"rb");
	if(source==NULL)
	 {
		printf ("\nERROR: FILE DOES NOT EXIST OR COULDN'T BE OPEN FOR READ%s",errormsg);
		fcloseall();
		return;
	 }
	fseek(source,src_offset,0);
	for (cont=0;cont<changechar_height*256;cont++)
	 {
		if (!(flags&0x0001))
			buffer[cont]=(unsigned char)fgetc (source);
		 else
			buffer[cont]=~(unsigned char)fgetc (source);
	 }
	fclose (source);

	if (flags&0x0200) //flips patterns horizontally
	 {
		aux=buffer;
		for (c0=0;c0<changechar_height*256;c0++,aux++)
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

	if (flags&0x0400) //flips patterns vertically
	 {
		for (c1=0;c1<256;c1++)
		 {
			aux=buffer + c1 * changechar_height;
			for (c0=0;c0<(changechar_height/2);c0++)
			 {
				a0=*(aux+changechar_height-1-c0);
				*(aux+changechar_height-1-c0)=*(aux+c0);
				*(aux+c0)=a0;
			 }
		 }
	 }

	if (flags&0x0020)
	 {
		setcrtmode (3);
		changechar_func=CHANGCHR_RECALC;
	 }

	if (flags&0x0040)
	 {
		setcrtmode (newmode);
		crt_detect (VIDEOADAPTER);
	 }

	if (!(flags&0x0080))
	 {
		if (startpos>endpos)
		 {
			printf ("\nERROR: First character to be loaded index is greater than\
 last character to be\nloaded index.%s",errormsg);
		 }
		changechar (buffer,startpos,(endpos-startpos)+1);
	 }
	 else
	 {
		crt_direct=1;
		if (!(flags&0x0100))
			changechar_rows=vmode_y;
		goto jmp_100h;
	 }

	if (flags&0x0100)
	  jmp_100h:
		changecharg(buffer,changechar_rows);

	if (flags&0x0008)
		setchrboxwidth((flags&0x0007)/2);

	if (flags&0x0800)
		crtfontspec(blkspec);

	if (flags&0x0010)
	 {
		crt_detect(VIDEOADAPTER);
		fillscr (0x20,0x1e);
		for (c0=0;c0<8;c0++)
			for (c1=0;c1<16;c1++)
			 {
				printc (c0*16+c1,c1+17,c0+8,0x17);
				printc (c0*16+c1+128,c1+42,c0+8,0x17);
			 }
		prints ("FONT CHARACTER SET:",25,3,0x1e);
		for (c0=0;c0<8;c0++)
		 {
			prints ("(00)h ",10,8+c0,0x1f);
			prints ("(80)h ",60,8+c0,0x1f);
			printc (c0+0x30,11,8+c0,0x1f);
			printc (c0+0x5f,61,8+c0,0x1f);
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
				a0=c0+0x57;
			printc (a0,c0+17,6,0x1f);
			printc (a0,c0+42,6,0x1f);
		 }
		crt_gotoxy(0,16);
	 }
	free(buffer);
 }
//0123456789êëìíîï