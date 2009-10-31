#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "IO.h"
#include "doorIO.h"
#include "Config.h"

const char *black=		"`00";
const char *blue=		"`01";
const char *green=		"`02";
const char *cyan=		"`03";
const char *red=		"`04";
const char *magenta=	"`05";
const char *brown=		"`06";
const char *lgray=		"`07";
const char *dgray=		"`08";
const char *lblue=		"`09";
const char *lgreen=		"`10";
const char *lcyan=		"`11";
const char *lred=		"`12";
const char *lmagenta=	"`13";
const char *yellow=		"`14";
const char *white=		"`15";
const char *blred=		"`99";

int	lines=0;
int	cols=80;
static int col=0;
static int line=0;

void colorcode(const char *color)
{
	door_textattr(atoi(color+1));
}

void outstr(const char *str)
{
	char	*p,*start,*end;
	char	cc[4];
	char	*dup=strdup(str);
	int		before_end=0;
	int		after_end=0;

	cc[3]=0;
	for(p=dup, start=end=dup; *p; p++) {
		if(p[0]=='`' && isdigit(p[1]) && isdigit(p[2])) {
			strncpy(cc, p, 3);
			colorcode(cc);
			*p=0;
			if(*start)
				door_outstr(start);
			start=end=p+3;
			p+=2;
		}
		else {
			// Word Wrap
			after_end++;
			if(isspace(*p)) {
				end=p;
				before_end += after_end;
				after_end=0;
			}
			if(col+before_end+after_end >= cols) {
				*end=0;
				door_outstr(start);
				nl();
				end=start=end+1;
				before_end=0;
			}
		}
	}
	if(*start) {
		door_outstr(start);
		col+=before_end+after_end;
	}
	free(dup);
}

void halt(void)
{
	exit(1);
}

void normal_exit(void)
{
	exit(0);
}

void nl(void)
{
	door_nl();
	col=0;
	line++;
}

void d(const char *str, ...)
{
	va_list	ap;

	if(str==NULL) {
		fprintf(stderr,"NULL String Error!\r\n");
		abort();
	}
	colorcode(blred);
	va_start(ap, str);
	do {
		outstr(str);
		str=va_arg(ap, char *);
	} while(str != NULL);
	va_end(ap);
}

void f(const char *color, const char *fmt, ...)
{
	int		len;
	va_list	ap;
	char	*str;

	colorcode(color);
	va_start(ap, fmt);
	len=vsnprintf(NULL, 0, fmt, ap);
	str=alloca(len+1);
	vsnprintf(str, len+1, fmt, ap);
	va_end(ap);
	D(str);
}

void upause(void)
{
	D(dgray, "[Press a key]");
	if(door_readch()==-1)
		halt();
	nl();
}

char gchar(void)
{
	int ch;

	ch=door_readch();
	if(ch==-1)
		halt();
	return ch;
}

void clr(void)
{
	colorcode(black);
	door_clearscreen();
	col=line=0;
}

void menu2(const char *str)
{
	char	start[2]={str[0], 0};
	char	hot[2]={str[1], 0};
	char	end[2]={str[2], 0};

	D(config.bracketcolor, start, config.hotkeycolor, hot, config.bracketcolor, end, config.textcolor, str+3);
}

void menu(const char *str)
{
	menu2(str);
	nl();
}

bool confirm(const char *prompt, char dflt)
{
	char	ch;

	dflt=toupper(dflt);
	D(config.textcolor, prompt, dflt=='Y'?" ? ([Y]/N)":" ? (Y/[N])");
	do {
		ch=toupper(gchar());
		if(ch=='\r')
			ch=dflt;
	} while(ch != 'Y' && ch != 'N');
	if(ch=='Y') {
		DL(config.textcolor, " Yes");
		return(true);
	}
	DL(config.textcolor, " No");
	return(false);
}

void dbs(int count)
{
	for(;count;count--) {
		if(col > 0) {
			door_outstr("\b \b");
			col--;
		}
	}
}

long get_a_number(long min, long max, long dflt)
{
	int		len=0;
	char	in[17];
	char	ch;
	char	*p;
	long	ret;

	if(dflt > max)
		dflt=max;
	if(dflt < min)
		dflt=min;

	in[0]=0;
	for(;;) {
		ch=toupper(gchar());
		switch(ch) {
			case 'K':
				p=strchr(in, 0);
				strcat(in, "000");
				len+=3;
				if(len > 10) {
					len=10;
					in[len]=0;
				}
				D(config.textcolor, p);
				break;
			case 'M':
				p=strchr(in, 0);
				strcat(in, "000000");
				len+=6;
				if(len > 10) {
					len=10;
					in[len]=0;
				}
				D(config.textcolor, p);
				break;
			case '>':
				dbs(len);
				sprintf(in, "%ld", max);
				D(config.textcolor, max);
				break;
			case '\b':
			case 127:
				in[--len]=0;
				dbs(1);
				break;
			case '\r':
				if(len==0) {
					len=sprintf(in, "%ld", dflt);
					D(config.textcolor, in);
				}
				ret=atol(in);
				if(ret > max || ret < min) {
					nl();
					F(config.textcolor, "(a number in the range `07%ld%s .. `07%ld%s please!)", min, config.textcolor, max, config.textcolor);
					nl();
					D(config.textcolor, ":");
					in[0]=0;
					len=0;
				}
				else {
					goto end;
				}
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if(len < 10) {
					p=in+len;
					in[len++]=ch;
					in[len]=0;
					D(config.textcolor, p);
				}
				break;
		}
	}

end:
	nl();
	return ret;
}

long get_number(long min, long max)
{
	return(get_a_number(min, max, 0));
}
