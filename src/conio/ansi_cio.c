#include <fcntl.h>
#include <stdarg.h>

#include <genwrap.h>
#include <threadwrap.h>

#ifdef __unix__
#include <termios.h>
#endif

#include "ciolib.h"
#include "ansi_cio.h"
WORD	ansi_curr_attr=0x07<<8;

unsigned int ansi_rows=24;
unsigned int ansi_cols=80;
unsigned int ansi_nextchar;
int ansi_got_row=0;
int ansi_got_col=0;
int ansi_esc_delay=25;
int puttext_no_move=0;

const int 	ansi_tabs[10]={9,17,25,33,41,49,57,65,73,80};
const int 	ansi_colours[8]={0,4,2,6,1,5,3,7};
static WORD		ansi_inch;
static char		ansi_raw_inch;
struct termios tio_default;				/* Initial term settings */
WORD	*vmem;
int		ansi_row=0;
int		ansi_col=0;
int		force_move=1;

/* Control sequence table definitions. */
typedef struct
{
   char *pszSequence;
   int chExtendedKey;
} tODKeySequence;

#define ANSI_KEY_UP		72<<8
#define ANSI_KEY_DOWN	80<<8
#define ANSI_KEY_RIGHT	0x4d<<8
#define ANSI_KEY_LEFT	0x4b<<8
#define ANSI_KEY_HOME	0x47<<8
#define ANSI_KEY_END	0x4f<<8
#define ANSI_KEY_F1		0x3b<<8
#define ANSI_KEY_F2		0x3c<<8
#define ANSI_KEY_F3		0x3d<<8
#define ANSI_KEY_F4		0x3e<<8
#define ANSI_KEY_F5		0x3f<<8
#define ANSI_KEY_F6		0x40<<8
#define ANSI_KEY_F7		0x41<<8
#define ANSI_KEY_F8		0x42<<8
#define ANSI_KEY_F9		0x43<<8
#define ANSI_KEY_F10	0x44<<8
#define ANSI_KEY_PGUP	0x49<<8
#define ANSI_KEY_PGDN	0x51<<8
#define ANSI_KEY_INSERT	0x52<<8
#define ANSI_KEY_DELETE	0x53<<8

tODKeySequence aKeySequences[] =
{
   /* VT-52 control sequences. */
   {"\033A", ANSI_KEY_UP},
   {"\033B", ANSI_KEY_DOWN},
   {"\033C", ANSI_KEY_RIGHT},
   {"\033D", ANSI_KEY_LEFT},
   {"\033H", ANSI_KEY_HOME},
   {"\033K", ANSI_KEY_END},
   {"\033P", ANSI_KEY_F1},
   {"\033Q", ANSI_KEY_F2},
   {"\033?w", ANSI_KEY_F3},
   {"\033?x", ANSI_KEY_F4},
   {"\033?t", ANSI_KEY_F5},
   {"\033?u", ANSI_KEY_F6},
   {"\033?q", ANSI_KEY_F7},
   {"\033?r", ANSI_KEY_F8},
   {"\033?p", ANSI_KEY_F9},

   /* Control sequences common to VT-100/VT-102/VT-220/VT-320/ANSI. */
   {"\033[A", ANSI_KEY_UP},
   {"\033[B", ANSI_KEY_DOWN},
   {"\033[C", ANSI_KEY_RIGHT},
   {"\033[D", ANSI_KEY_LEFT},
   {"\033[M", ANSI_KEY_PGUP},
   {"\033[H\x1b[2J", ANSI_KEY_PGDN},
   {"\033[H", ANSI_KEY_HOME},
   {"\033[K", ANSI_KEY_END},
   {"\033OP", ANSI_KEY_F1},
   {"\033OQ", ANSI_KEY_F2},
   {"\033OR", ANSI_KEY_F3},
   {"\033OS", ANSI_KEY_F4},

   /* VT-220/VT-320 specific control sequences. */
   {"\033[17~", ANSI_KEY_F6},
   {"\033[18~", ANSI_KEY_F7},
   {"\033[19~", ANSI_KEY_F8},
   {"\033[20~", ANSI_KEY_F9},
   {"\033[21~", ANSI_KEY_F10},

   /* ANSI-specific control sequences. */
   {"\033[L", ANSI_KEY_HOME},
   {"\033Ow", ANSI_KEY_F3},
   {"\033Ox", ANSI_KEY_F4},
   {"\033Ot", ANSI_KEY_F5},
   {"\033Ou", ANSI_KEY_F6},
   {"\033Oq", ANSI_KEY_F7},
   {"\033Or", ANSI_KEY_F8},
   {"\033Op", ANSI_KEY_F9},

   /* PROCOMM-specific control sequences (non-keypad alternatives). */
   {"\033OA", ANSI_KEY_UP},
   {"\033OB", ANSI_KEY_DOWN},
   {"\033OC", ANSI_KEY_RIGHT},
   {"\033OD", ANSI_KEY_LEFT},
   {"\033OH", ANSI_KEY_HOME},
   {"\033OK", ANSI_KEY_END},
   
   /* Terminator */
   {"",0}
};

void ansi_sendch(char ch)
{
	struct text_info ti;

	if(!ch)
		ch=' ';
	if(ansi_row<ansi_rows-1 || (ansi_row==ansi_rows-1 && ansi_col<ansi_cols-1)) {
		ansi_col++;
		if(ansi_col>=ansi_cols) {
			ansi_col=0;
			ansi_row++;
			if(ansi_row>=ansi_rows) {
				ansi_col=ansi_cols-1;
				ansi_row=ansi_rows-1;
			}
		}
		fwrite(&ch,1,1,stdout);
		if(ch<' ')
			force_move=1;
	}
}

void ansi_sendstr(char *str,int len)
{
	if(len==-1)
		len=strlen(str);
	if(len) {
		fwrite(str,len,1,stdout);
	}
}

int ansi_puttext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;
	int		attrib;

	gettextinfo(&ti);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ti.screenwidth
			|| sy > ti.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > ti.screenwidth
			|| ey > ti.screenheight
			|| fill==NULL)
		return(0);

	out=fill;
	attrib=ti.attribute;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=*(out++);
			if(sch==27)
				sch=' ';
			if(sch==0)
				sch=' ';
			sch |= (*(out++))<<8;
			if(vmem[y*ansi_cols+x]==sch)
				continue;
			vmem[y*ansi_cols+x]=sch;
			ansi_gotoxy(x+1,y+1);
			if(attrib!=sch>>8) {
				textattr(sch>>8);
				attrib=sch>>8;
			}
			ansi_sendch(sch&0xff);
		}
	}

	if(!puttext_no_move)
		gotoxy(ti.curx,ti.cury);
	if(attrib!=ti.attribute)
		textattr(ti.attribute);
}

int ansi_gettext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;

	gettextinfo(&ti);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ti.screenwidth
			|| sy > ti.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > ti.screenwidth
			|| ey > ti.screenheight
			|| fill==NULL)
		return(0);

	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=vmem[y*ansi_cols+x];
			*(out++)=sch & 0xff;
			*(out++)=sch >> 8;
		}
	}
}

void ansi_textattr(unsigned char attr)
{
	char str[16];
	int fg,ofg;
	int bg,obg;
	int bl,obl;
	int br,obr;
	int oa;

	str[0]=0;
	if(ansi_curr_attr==attr<<8)
		return;

	bl=attr&0x80;
	bg=(attr>>4)&0x7;
	fg=attr&0x07;
	br=attr&0x04;

	oa=ansi_curr_attr>>8;
	obl=oa>>7;
	obg=(oa>>4)&0x7;
	ofg=oa&0x07;
	obr=(oa>>3)&0x01;

	ansi_curr_attr=attr<<8;

	strcpy(str,"\033[");
	if(obl!=bl) {
		if(!bl) {
			strcat(str,"0;");
			ofg=7;
			obg=0;
			obr=0;
		}
		else
			strcat(str,"5;");
	}
	if(br!=obr) {
		if(br)
			strcat(str,"1;");
		else
#if 0
			strcat(str,"2;");
#else
		{
			strcat(str,"0;");
			ofg=7;
			obg=0;
		}
#endif
	}
	if(fg!=ofg)
		sprintf(str+strlen(str),"3%d;",ansi_colours[fg]);
	if(bg!=obg)
		sprintf(str+strlen(str),"4%d;",ansi_colours[bg]);
	str[strlen(str)-1]='m';
	ansi_sendstr(str,-1);
}

static void ansi_keyparse(void *par)
{
	int		gotesc=0;
	char	seq[64];
	int		ch;
	int		waited=0;
	int		i;
	char	*p;

	for(;;) {
		while(!ansi_raw_inch
				&& (gotesc || (!gotesc && !seq[0]))) {
			waited++;
			if(waited>=ansi_esc_delay) {
				waited=0;
				gotesc=0;
			}
			else
				SLEEP(1);
		}
		if(!gotesc && seq[0]) {
			while(ansi_inch)
				SLEEP(1);
			ch=seq[0];
			for(p=seq;*p;*p=*(++p));
			ansi_inch=ch;
			continue;
		}
		else {
			ch=ansi_raw_inch;
			ansi_raw_inch=0;
		}
		switch(gotesc) {
			case 1:	/* Escape */
				waited=0;
				if(strlen(seq)>=sizeof(seq)-2) {
					gotesc=0;
					break;
				}
				seq[strlen(seq)+1]=0;
				seq[strlen(seq)]=ch;
				if((ch<'0' || ch>'9')		/* End of ESC sequence */
						&& ch!=';'
						&& ch!='?'
						&& (strlen(seq)==2?ch != '[':1)
						&& (strlen(seq)==2?ch != 'O':1)) {
					for(i=0;aKeySequences[i].pszSequence[0];i++) {
						if(!strcmp(seq,aKeySequences[i].pszSequence)) {
							gotesc=0;
							seq[0]=0;
							while(ansi_inch)
								SLEEP(1);
							ansi_inch=aKeySequences[i].chExtendedKey;
							break;
						}
					}
					if(!aKeySequences[i].pszSequence[0])
						gotesc=0;
				}
				break;
			default:
				if(ch==27) {
					seq[0]=27;
					seq[1]=0;
					gotesc=1;
					waited=0;
					break;
				}
				while(ansi_inch)
					SLEEP(1);
				ansi_inch=ch;
				break;
		}
	}
}

static void ansi_keythread(void *params)
{
	_beginthread(ansi_keyparse,1024,NULL);

	for(;;) {
		if(!ansi_raw_inch)
			ansi_raw_inch=fgetc(stdin);
		else
			SLEEP(1);
	}
}

int ansi_kbhit(void)
{
	return(ansi_inch);
}

void ansi_delay(long msec)
{
	SLEEP(msec);
}

int ansi_wherey(void)
{
	return(ansi_row+1);
}

int ansi_wherex(void)
{
	return(ansi_col+1);
}

/* Put the character _c on the screen at the current cursor position. 
 * The special characters return, linefeed, bell, and backspace are handled
 * properly, as is line wrap and scrolling. The cursor position is updated. 
 */
int ansi_putch(unsigned char ch)
{
	struct text_info ti;
	WORD sch;
	int i;
	char buf[2];

	buf[0]=ch;
	buf[1]=ansi_curr_attr>>8;

	gettextinfo(&ti);
	puttext_no_move=1;

	switch(ch) {
		case '\r':
			gotoxy(1,wherey());
			break;
		case '\n':
			if(wherey()==ti.winbottom-ti.wintop+1)
				wscroll();
			else
				gotoxy(wherex(),wherey()+1);
			break;
		case '\b':
			if(ansi_col>ti.winleft-1) {
				buf[0]=' ';
				gotoxy(wherex()-1,wherey());
				puttext(ansi_col+1,ansi_row+1,ansi_col+1,ansi_row+1,buf);
			}
			break;
		case 7:		/* Bell */
			ansi_sendch(7);
			break;
		case '\t':
			for(i=0;i<10;i++) {
				if(ansi_tabs[i]>ansi_col+1) {
					while(ansi_col+1<ansi_tabs[i]) {
						putch(' ');
					}
					break;
				}
			}
			if(i==10) {
				putch('\r');
				putch('\n');
			}
			break;
		default:
			if(wherey()==ti.winbottom-ti.wintop+1
					&& wherex()==ti.winright-ti.winleft+1) {
				gotoxy(1,wherey());
				puttext(ansi_col+1,ansi_row+1,ansi_col+1,ansi_row+1,buf);
				wscroll();
			}
			else {
				if(wherex()==ti.winright-ti.winleft+1) {
					gotoxy(1,ti.cury+1);
					puttext(ansi_col+1,ansi_row+1,ansi_col+1,ansi_row+1,buf);
				}
				else {
					puttext(ansi_col+1,ansi_row+1,ansi_col+1,ansi_row+1,buf);
					gotoxy(ti.curx+1,ti.cury);
				}
			}
			break;
	}

	puttext_no_move=0;
	return(ch);
}

void ansi_gotoxy(int x, int y)
{
	struct text_info ti;
	char str[16];

	if(x < 1
		|| x > ansi_cols
		|| y < 1
		|| y > ansi_rows)
		return;
	if(force_move) {
		force_move=0;
		sprintf(str,"\033[%d;%dH",y,x);
	}
	else {
		if(x==1 && ansi_col != 0 && ansi_row<ansi_row-1) {
			ansi_sendch('\r');
			force_move=0;
			ansi_col=0;
		}
		if(x==ansi_col+1) {
			if(y==ansi_row+1) {
				str[0]=0;
			}
			else {
				if(y<ansi_row+1) {
					if(y==ansi_row)
						strcpy(str,"\033[A");
					else
						sprintf(str,"\033[%dA",ansi_row+1-y);
				}
				else {
					if(y==ansi_row+2)
						strcpy(str,"\033[B");
					else
						sprintf(str,"\033[%dB",y-ansi_row-1);
				}
			}
		}
		else {
			if(y==ansi_row+1) {
				if(x<ansi_col+1) {
					if(x==ansi_col)
						strcpy(str,"\033[D");
					else
						sprintf(str,"\033[%dD",ansi_col+1-x);
				}
				else {
					if(x==ansi_col+2)
						strcpy(str,"\033[C");
					else
						sprintf(str,"\033[%dC",x-ansi_col-1);
				}
			}
			else {
				sprintf(str,"\033[%d;%dH",y,x);
			}
		}
	}

	ansi_sendstr(str,-1);
	ansi_row=y-1;
	ansi_col=x-1;
}

void ansi_gettextinfo(struct text_info *info)
{
	info->currmode=3;
	info->screenheight=ansi_rows;
	info->screenwidth=ansi_cols;
	info->curx=wherex();
	info->cury=wherey();
	info->attribute=ansi_curr_attr>>8;
}

void ansi_setcursortype(int type)
{
	switch(type) {
		case _NOCURSOR:
		case _SOLIDCURSOR:
		default:
			break;
	}
}

int ansi_getch(void)
{
	int ch;

	while(!ansi_inch)
		SLEEP(1);
	ch=ansi_inch&0xff;
	ansi_inch=ansi_inch>>8;
	return(ch);
}

int ansi_getche(void)
{
	int ch;

	if(ansi_nextchar)
		return(ansi_getch());
	ch=ansi_getch();
	if(ch)
		putch(ch);
	return(ch);
}

int ansi_beep(void)
{
	putch(7);
	return(0);
}

void ansi_textmode(int mode)
{
}

#ifdef __unix__
void ansi_fixterm(void)
{
	tcsetattr(STDIN_FILENO,TCSANOW,&tio_default);
}
#endif

int ansi_initciolib(long inmode)
{
	int i;
	char *init="\033[0m\033[2J\033[1;1H";
#ifdef _WIN32
	_setmode(fileno(stdout),_O_BINARY);
	_setmode(fileno(stdin),_O_BINARY);
	setvbuf(stdout, NULL, _IONBF, 0);
#else
	struct termios tio_raw;

	if (isatty(STDIN_FILENO))  {
		tcgetattr(STDIN_FILENO,&tio_default);
		tio_raw = tio_default;
		cfmakeraw(&tio_raw);
		tcsetattr(STDIN_FILENO,TCSANOW,&tio_raw);
		setvbuf(stdout, NULL, _IONBF, 0);
		atexit(ansi_fixterm);
	}
#endif
	vmem=(WORD *)malloc(ansi_rows*ansi_cols*sizeof(WORD));
	ansi_sendstr(init,-1);
	for(i=0;i<ansi_rows*ansi_cols;i++)
		vmem[i]=0x0720;
	_beginthread(ansi_keythread,1024,NULL);
	return(1);
}
