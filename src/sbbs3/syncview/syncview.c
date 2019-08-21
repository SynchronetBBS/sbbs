#include <stdio.h>
#include <string.h>

#include "dirwrap.h"

#include "ciolib.h"
#include "cterm.h"

#define SCROLL_LINES	100000
#define BUF_SIZE		1024

struct cterminal *cterm;

void viewscroll(void)
{
	int	top;
	int key;
	int i;
	struct	text_info txtinfo;

	gettextinfo(&txtinfo);
	if(cterm->backpos>cterm->backlines) {
		memmove(cterm->scrollback,cterm->scrollback+cterm->width,cterm->width*sizeof(*cterm->scrollback)*(cterm->backlines-1));
		cterm->backpos=cterm->backlines;
	}
	vmem_gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,cterm->scrollback+(cterm->backpos)*cterm->width);
	top=cterm->backpos;
	for(i=0;!i;) {
		if(top<1)
			top=1;
		if(top>cterm->backpos)
			top=cterm->backpos;
		vmem_puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,cterm->scrollback+(txtinfo.screenwidth*top));
		key=getch();
		switch(key) {
			case 0xe0:
			case 0:
				switch(key|getch()<<8) {
					case CIO_KEY_UP:
						top--;
						break;
					case CIO_KEY_DOWN:
						top++;
						break;
					case CIO_KEY_PPAGE:
						top-=txtinfo.screenheight;
						break;
					case CIO_KEY_NPAGE:
						top+=txtinfo.screenheight;
						break;
				}
				break;
			case 'j':
			case 'J':
				top--;
				break;
			case 'k':
			case 'K':
				top++;
				break;
			case 'h':
			case 'H':
				top-=txtinfo.screenheight;
				break;
			case 'l':
			case 'L':
				top+=txtinfo.screenheight;
				break;
			case '\033':
				i=1;
				break;
		}
	}
	return;
}

void lfexpand(char *buf, int *len)
{
	char	newbuf[BUF_SIZE*2];
	int		newlen=0;
	int		i;

	for(i=0; i<*len; i++) {
		if(buf[i]=='\n') {
			newbuf[newlen++]='\r';
		}
		newbuf[newlen++]=buf[i];
	}
	*len=newlen;
	memcpy(buf, newbuf, newlen);

	return;
}

int main(int argc, char **argv)
{
	struct text_info	ti;
	FILE	*f;
	char	buf[BUF_SIZE*2];	/* Room for lfexpand */
	int		len;
	int		speed=0;
	struct vmem_cell	*scrollbuf;
	char	*infile=NULL;
	char	title[MAX_PATH+1];
	int		expand=0;
	int		ansi=0;
	int		i;

	/* Parse command line */
	for(i=1; i<argc; i++) {
		if(argv[i][0]=='-') {
			if(argv[i][1]=='l' && argv[i][2]==0)
				expand=1;
			else if(argv[i][1]=='a' && argv[i][2]==0)
				ansi=1;
			else
				goto usage;
		}
		else {
			if(infile==NULL)
				infile=argv[i];
			else
				goto usage;
		}
	}

	if(ansi) {
		initciolib(CIOLIB_MODE_ANSI);
		puts("START OF ANSI...");
	}

	textmode(C80);
	gettextinfo(&ti);
	if((scrollbuf=malloc(SCROLL_LINES*ti.screenwidth*sizeof(*scrollbuf)))==NULL) {
		cprintf("Cannot allocate memory\n\n\rPress any key to exit.");
		getch();
		return(-1);
	}

	cterm=cterm_init(ti.screenheight, ti.screenwidth, 1, 1, SCROLL_LINES, scrollbuf, CTERM_EMULATION_ANSI_BBS);
	if(!cterm) {
		fputs("ERROR Initializing CTerm!\n", stderr);
		return 1;
	}
	if(infile) {
		if((f=fopen(infile,"r"))==NULL) {
			cprintf("Cannot read %s\n\n\rPress any key to exit.",argv[1]);
			getch();
			return(-1);
		}
		sprintf(title,"SyncView: %s",getfname(argv[1]));
	}
	else {
		f=stdin;
		strcpy(title,"SyncView: [stdin]");
	}
	settitle(title);
	while((len=fread(buf, 1, BUF_SIZE, f))!=0) {
		if(expand)
			lfexpand(buf, &len);
		cterm_write(cterm, buf, len, NULL, 0, &speed);
	}
	if(ansi) {
		puts("");
		puts("END OF ANSI");
		if(cterm->backpos>cterm->backlines) {
			memmove(cterm->scrollback,cterm->scrollback+cterm->width,cterm->width*sizeof(*cterm->scrollback)*(cterm->backlines-1));
			cterm->backpos=cterm->backlines;
		}
		vmem_gettext(1,1,ti.screenwidth,ti.screenheight,cterm->scrollback+(cterm->backpos)*cterm->width);
		cterm->backpos += ti.screenheight;
		puttext_can_move=1;
		puts("START OF SCREEN DUMP...");
		for (i = 0; i < cterm->backpos; i += ti.screenheight) {
			clrscr();
			vmem_puttext(1,1,ti.screenwidth,cterm->backpos - i > ti.screenheight ? ti.screenheight : cterm->backpos - i,cterm->scrollback+(ti.screenwidth*i));
		}
	}
	else
		viewscroll();
	return(0);

usage:
	cprintf("Usage: %s [-l] [-a] [filename]\r\n\r\n"
			"Displays the ANSI file filename expanding \\n to \\r\\n if -l is specified.\r\n"
			"If no filename is specified, reads input from stdin\r\n"
			"If -a is specified, outputs ANSI to stdout\r\n"
			"\r\n"
			"Press any key to exit.");
	getch();
	return(-1);
}
