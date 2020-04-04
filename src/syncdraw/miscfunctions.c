#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ciolib.h>
#include <gen_defs.h>

#include "crt.h"
#include "syncdraw.h"
#include "key.h"
#include "miscfunctions.h"

char            nul[100];

void 
CopyScreen(int p1, int p2)
{
	int             x;
	for (x = 0; x < MaxLines; x++)
		memcpy(Screen[p2][x], Screen[p1][x], 160);
}
void 
SaveScreen(void)
{
	CopyScreen(ActivePage, UNDOPage);
	/*
	 * memcpy(&Screen[UNDOPage][0][0],
	 * &Screen[ActivePage][0][0],MaxLines*160);
	 */

}

void 
InsLine(void)
{
	int             b;
	for (b = MaxLines - 1; b > CursorY + FirstLine; b--)
		memcpy(Screen[ActivePage][b], Screen[ActivePage][b - 1], 160);
	for (b = 0; b < 80; b++) {
		Screen[ActivePage][CursorY + FirstLine][b * 2] = 32;
		Screen[ActivePage][CursorY + FirstLine][b * 2 + 1] = 7;
	}
	if (LastLine < MaxLines)
		LastLine++;
}

void 
DelLine(void)
{
	int             b;
	for (b = CursorY + FirstLine; b < MaxLines; b++)
		memcpy(Screen[ActivePage][b], Screen[ActivePage][b + 1], 160);
	for (b = 0; b < 80; b++) {
		Screen[ActivePage][MaxLines - 1][b << 1] = 32;
		Screen[ActivePage][MaxLines - 1][(b << 1) + 1] = 7;
	}
	if (LastLine > CursorY + FirstLine)
		LastLine--;
}

void 
InsCol(void)
{
	int             a;
	for (a = 0; a < MaxLines; a++) {
		memmove(&Screen[ActivePage][a][(CursorX << 1)],
			&Screen[ActivePage][a][(CursorX << 1) - 2], 160 - (CursorX << 1));
		Screen[ActivePage][a][CursorX << 1] = 32;
		Screen[ActivePage][a][(CursorX << 1) + 1] = 7;
	}
}
void 
DelCol(void)
{
	int             a;
	for (a = 0; a < MaxLines; a++) {
		memmove(&Screen[ActivePage][a][(CursorX << 1)],
			&Screen[ActivePage][a][(CursorX << 1) + 2], 160 - (CursorX << 1));
		Screen[ActivePage][a][158] = 32;
		Screen[ActivePage][a][159] = 7;
	}
}

void 
Colors(unsigned char col)
{
	char	buf[14]="  C o l o r   ";
	int	i;

	if (FullScreen)
		return;
	for(i=1;i<14;i+=2)
		buf[i]=col;
	puttext(11,1,17,1,buf);
}

void 
CoolWrite(int x, int y, char *a)
{
	int             b = 0, i = 0, j = 0;
	char			*buf;

	buf=(char *)malloc(strlen(a)*2);
	if (!buf)
		exit(1);
	while (a[b] != 0) {
		i++;
		if (a[b] == 32)
			i = 0;
		buf[j++]=a[b++];
		switch (i) {
		case 1:
			buf[j++]=8;
			break;
		case 3:
		case 2:
			buf[j++]=3;
			break;
		default:
			buf[j++]=11;
			break;
		}
	}
	puttext(x, y, x+strlen(a)-1,y,buf);
	free(buf);
}
void 
CodeWrite(int x, int y, char *a)
{
	int             b = 0, i = 0, attr=15;
	char			*buf;

	buf=(char *)malloc(strlen(a)*2);
	if (!buf)
		exit(1);
	while (a[b] != 0) {
		if (a[b] == ']')
			attr=15;
		buf[i++]=a[b];
		buf[i++]=attr;
		if (a[b] == '[')
			attr=7;
		b++;
	}
	puttext(x,y,x+strlen(a)-1,y,buf);
	free(buf);
}

unsigned char
QuickColor(void)
{
	unsigned char	buf[33*9*2];
	int	fg, bg, blink;
	int	ch, i, j, k;
	static unsigned char curch='#';
	struct	mouse_event me;

	fg = Attribute & 0x0f;
	bg = (Attribute & 0x70)>>4;
	blink = Attribute >> 7;
	memset(buf,0,sizeof(buf));
	DrawBox(7,2,41,12);
	do {
		Colors((blink<<7)|(bg<<4)|(fg));
		buf[0]='<';
		buf[1]=(bg==-1)?14:7;
		buf[32*2]='>';
		buf[32*2+1]=(bg==-1)?14:7;
		for(i=1;i<16;i++) {
			buf[i*2]=(curch-(16-i));
			buf[i*2+1]=(bg==-1)?14:7;
		}
		buf[16*2]=curch;
		buf[16*2+1]=0x60;
		for(i=17;i<32;i++) {
			buf[i*2]=curch+(i-16);
			buf[i*2+1]=(bg==-1)?14:7;
		}
		k=33*2;
		for(j=0;j<8;j++) {
			for(i=0;i<16;i++) {
				if(fg==i && bg == j)
					buf[k++]='>';
				else if(fg==i-1 && bg == j)
					buf[k++]='<';
				else
					buf[k++]=' ';
				buf[k++]=14;
				buf[k++]=curch;
				buf[k++]=(j<<4)|(i);
			}
			if(fg==i-1 && bg == j)
				buf[k++]='<';
			else
				buf[k++]=' ';
			buf[k++]=14;
		}
		puttext(8,3,40,11,buf);
		ch=newgetch();
		if(ch==CIO_KEY_MOUSE) {
			getmouse(&me);
			if(me.endx>=8 && me.endx<=40 && me.endy==3 
					&& me.event==CIOLIB_BUTTON_1_CLICK) {
				curch+=me.endx-8-16;
			}
			if(me.endx>=8 && me.endx<=40 && me.endy>=4 && me.endy<= 11 && me.endx % 2==1) {
				fg=(me.endx-8)/2;
				bg=me.endy-4;
				if(me.event==CIOLIB_BUTTON_1_CLICK) {
					ch=13;
				}
			}
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
		}
		switch(ch) {
		case CIO_KEY_UP:
			bg--;
			if(bg<-1)
				bg=7;
			break;
		case CIO_KEY_DOWN:
			bg++;
			if(bg>7)
				bg=-1;
			break;
		case CIO_KEY_LEFT:
			if(bg==-1)
				curch--;
			else {
				fg--;
				if(fg<0)
					fg=15;
			}
			break;
		case CIO_KEY_RIGHT:
			if(bg==-1)
				curch++;
			else {
				fg++;
				if(fg>15)
					fg=0;
			}
			break;
		case 13:
			if(bg==-1)
				ch=0;
			break;
		}
	} while(ch!=13 && ch!=27);
	if(ch==13)
		return((blink<<7)|(bg<<4)|(fg));
	return(Attribute);
}

unsigned char 
SelectColor(void)
{
	int             Blink, fx, bx, a, ch, i, bg=0, attr;
	struct			mouse_event	me;
	char			buf[51*7*2];
	char			cbuf[51*7*2];

	DrawBox(17, 16, 69, 24);
	memset(buf,0,sizeof(buf));
	bufprintf(buf+(5*51+21)*2,8,"B - BLINK");
	i=(1*51+2)*2;
	buf[i++]=176;
	buf[i++]=15;
	buf[i++]=176;
	buf[i++]=15;
	buf[i++]=32;
	buf[i++]=15;
	for (a = 1; a <= 15; a++) {
		buf[i++]=219;
		buf[i++]=a;
		buf[i++]=219;
		buf[i++]=a;
		buf[i++]=32;
		buf[i++]=a;
	}
	i=(3*51+13)*2;
	buf[i++]=176;
	buf[i++]=15;
	buf[i++]=176;
	buf[i++]=15;
	buf[i++]=32;
	buf[i++]=15;
	for (a = 1; a <= 7; a++) {
		buf[i++]=219;
		buf[i++]=a;
		buf[i++]=219;
		buf[i++]=a;
		buf[i++]=32;
		buf[i++]=a;
	}
	puttext(18,17,68,23,buf);
	memcpy(cbuf,buf,sizeof(buf));
	fx = 1 + ((Attribute & 15) * 3);
	bx = 12 + ((Attribute & 112) >> 4) * 3;
	Blink = Attribute & 128;
	do {
		memcpy(buf,cbuf,sizeof(buf));
		attr=bg?6:14;
		buf[(1*51+fx)*2]='<';
		buf[(1*51+fx)*2+1]=attr;
		buf[(1*51+fx+3)*2]='>';
		buf[(1*51+fx+3)*2+1]=attr;

		attr=bg?14:6;
		buf[(3*51+bx)*2]='<';
		buf[(3*51+bx)*2+1]=attr;
		buf[(3*51+bx+3)*2]='>';
		buf[(3*51+bx+3)*2+1]=attr;

		puttext(18,17,68,23,buf);
		Colors((((bx - 12) / 3) << 4) + (fx - 1) / 3 + Blink);
		ch = newgetch();
		switch (ch) {
		case CIO_KEY_MOUSE:
			getmouse(&me);
			if(me.endy == 18 
					&& me.endx>=20 && me.endx<=66
					&& ((me.endx-19)%3)) {
				bg=0;
				fx=me.endx-19;
				fx-=fx%3;
				fx++;
				if(me.event == CIOLIB_BUTTON_1_CLICK)
					ch=13;
			}
			if(me.endy == 20
					&& me.endx>=31 && me.endx<=52
					&& ((me.endx-30)%3)) {
				bg=1;
				bx=me.endx-30;
				bx-=bx%3;
				bx+=12;
				if(me.event == CIOLIB_BUTTON_1_CLICK)
					ch=13;
			}
			if(me.endy == 22
					&& me.endx >= 39 && me.endx <= 47
					&& me.event == CIOLIB_BUTTON_1_CLICK)
				Blink = Blink ^ 128;
			if(me.event == CIOLIB_BUTTON_3_CLICK)
				ch=27;
			break;
		case 98:
		case 66:
			Blink = Blink ^ 128;
			break;
		case 13:
			if(!bg) {
				bg=1;
				ch=0;
			}
			break;
		case CIO_KEY_UP:
		case CIO_KEY_DOWN:
			bg = !bg;
			break;
		case CIO_KEY_LEFT:
			if(bg)
				bx -= 3;
			else
				fx -= 3;
			break;
		case CIO_KEY_RIGHT:
			if(bg)
				bx += 3;
			else
				fx += 3;
			break;
		}
		if (fx > 46)
			fx = 1;
		if (fx < 1)
			fx = 46;
		if (bx > 33)
			bx = 12;
		if (bx < 12)
			bx = 33;
	} while (ch != 13 && ch != 27);
	if(ch==13)
		return ((((bx - 12) / 3) << 4) + (fx - 1) / 3 + Blink);
	return(Attribute);
}
char 
translate(char a)
{
	switch (a) {
	case 'e':
		return 'î';
	case 'E':
		return 'ä';
	case 'I':
		return '­';
	case 'r':
		return 'ç';
	case 'R':
		return 'ž';
	case 'F':
	case 'f':
		return 'Ÿ';
	case 'a':
		return 'à';
	case 'A':
		return '’';
	case 'b':
	case 'B':
		return 'á';
	case 'n':
	case 'N':
		return 'ã';
	case 'u':
		return 150;
	case 'U':
		return 'ï';
	case 'Y':
		return '';
	case 'o':
		return 'í';
	case 'O':
		return 'å';
	case 'L':
	case 'l':
		return 'œ';
	case 'X':
	case 'x':
		return '‘';
	case 'S':
	case 's':
		return '$';
	case 'C':
	case 'c':
		return '›';
	case 'D':
	case 'd':
		return 'ë';
	case 'y':
		return 'æ';
	case 't':
		return 'â';
	default:
		return a;
	}
}

char           *
inputfield(char *Str, int length, int x1, int y)
{
	int             ch, x, pos = 0;
	struct		mouse_event	me;
	char		*buf;
	int			oldx,oldy;

	buf=(char *)malloc(length*2);
	if (!buf)
		exit(1);
	strcpy(nul,Str);
	oldx=wherex();
	oldy=wherey();
	do {
		for (x = 1; x <= length; x++) {
			buf[(x-1)*2]=250;
			buf[(x-1)*2+1]=8;
		}
		bufprintf(buf,7,"%.*s",length,nul);
		puttext(x1+1, y+1, x1+length, y+1, buf);
		gotoxy( x1 + pos+1,y+1);
		ch = newgetch();
		switch (ch) {
		case CIO_KEY_MOUSE:
			getmouse(&me);
			/* ToDo Add Mouse Stuff */
			break;
		case CIO_KEY_LEFT:
			if (pos > 0)
				pos--;
			break;
		case CIO_KEY_RIGHT:
			if (pos < (int)strlen(nul))
				pos++;
			break;
		case CIO_KEY_HOME:
			pos = 0;
			break;
		case CIO_KEY_END:
			pos = strlen(nul);
			break;
		case 127:
			if (pos != (int)strlen(nul))
				memcpy(&nul[pos], &nul[pos + 1], sizeof(nul) - pos + 1);
			break;
		case 8:
			if (pos > 0) {
				memcpy(&nul[pos - 1], &nul[pos], sizeof(nul) - pos + 1);
				pos--;
			}
			break;
		default:
			if ((pos < length) & (ch >= 32) & (ch <= 127)) {
				if (pos == (int)strlen(nul))
					sprintf(strchr(nul,0), "%c", (ch & 255));
				else
					nul[pos] = ch;
				pos++;
			}
			break;
		}
	} while ((ch != 13) & (ch != 27));

	free(buf);
	gotoxy(oldx,oldy);
	if (ch == 27)
		return Str;
	return nul;
}

void 
about(void)
{
	struct		mouse_event	me;

	while(kbhit() && newgetch()==CIO_KEY_MOUSE)
		getmouse(&me);
	DrawBox(30, 11, 62, 16);
	CoolWrite(31,12,"coded 1996 by Mike Krueger     ");
	CoolWrite(31,13,"ansis made by Col. Blair^TUSCON");
	CoolWrite(31,14,"Fixes/Changes by Stephen Hurd  ");
	CoolWrite(31,15,"Version 1.5 (GPL)              ");
	while(newgetch()==CIO_KEY_MOUSE) {
		getmouse(&me);
		if(me.event==CIOLIB_BUTTON_1_CLICK || me.event==CIOLIB_BUTTON_3_CLICK)
			break;
	}
}

int bufprintf(char *buf, int attr, char *fmat, ...)
{
    va_list argptr;
	int		ret, i=0;
	char	*p;
#ifdef _WIN32			/* Can't figure out a way to allocate a "big enough" buffer for Win32. */
	char	str[16384];
#else
	char	*str;
#endif

    va_start(argptr,fmat);
#ifdef _WIN32
	ret=_vsnprintf(str,sizeof(str)-1,fmat,argptr);
#else
    ret=vsnprintf(NULL,0,fmat,argptr);
	va_end(argptr);
	str=(char *)malloc(ret+1);
	if(str==NULL)
		return(EOF);
    va_start(argptr,fmat);
	ret=vsprintf(str,fmat,argptr);
#endif
    va_end(argptr);
	if(ret>=0) {
		for(p=str;*p;p++) {
			buf[i++]=*p;
			buf[i++]=attr;
		}
	}
	else
		i=EOF;
#ifndef _WIN32
	free(str);
#endif
    return(i);
}
