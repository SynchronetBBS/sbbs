/* ANS2MSG.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Convert ANSI messages to Synchronet .MSG (Ctrl-A code) format */

#include <stdio.h>
#include <conio.h>

int main(int argc, char **argv)
{
	char esc,n[25],ni;
	int i,ch;
	FILE *in,*out;

if(argc<3) {
	printf("\nANS2MSG v1.06\n");
	printf("\nusage: ans2msg infile.ans outfile.msg\n");
	exit(0); }

if((in=fopen(argv[1],"rb"))==NULL) {
	printf("\nerror opening %s\n",argv[1]);
	exit(1); }

if((out=fopen(argv[2],"wb"))==NULL) {
	printf("\nerror opening %s\n",argv[2]);
	exit(1); }

esc=0;
while((ch=fgetc(in))!=EOF) {
	if(ch=='[' && esc) {    /* ANSI escape sequence */
		ni=0;				/* zero number index */
		while((ch=fgetc(in))!=EOF) {
			if(isdigit(ch)) {			/* 1 digit */
				n[ni]=ch&0xf;
				ch=fgetc(in);
				if(ch==EOF)
					break;
				if(isdigit(ch)) {		/* 2 digits */
					n[ni]*=10;
					n[ni]+=ch&0xf;
					ch=fgetc(in);
					if(ch==EOF)
						break;
					if(isdigit(ch)) {	/* 3 digits */
						n[ni]*=10;
						n[ni]+=ch&0xf;
						ch=fgetc(in); } }
				ni++; }
			if(ch==';')
				continue;
			switch(ch) {
				case '=':
				case '?':
					ch=fgetc(in);	   /* First digit */
					if(isdigit(ch)) ch=fgetc(in);	/* l or h ? */
					if(isdigit(ch)) ch=fgetc(in);
					if(isdigit(ch)) fgetc(in);
					break;
				case 'J':
					if(n[0]==2) 					/* clear screen */
						fputs("\1l",out);           /* ctrl-al */
					break;
				case 'K':
					fputs("\1>",out);               /* clear to eol */
					break;
				case 'm':
					for(i=0;i<ni;i++) {
						fputc(1,out);				/* ctrl-ax */
						switch(n[i]) {
							default:
							case 0:
							case 2: 				/* no attribute */
								fputc('n',out);
								break;
							case 1: 				/* high intensity */
								fputc('h',out);
								break;
							case 3:
							case 4:
							case 5: 				/* blink */
							case 6:
							case 7:
								fputc('i',out);
								break;
							case 8: 				/* concealed */
								fputc('e',out);
								break;
							case 30:
								fputc('k',out);
								break;
							case 31:
								fputc('r',out);
								break;
							case 32:
								fputc('g',out);
								break;
							case 33:
								fputc('y',out);
								break;
							case 34:
								fputc('b',out);
								break;
							case 35:
								fputc('m',out);
								break;
							case 36:
								fputc('c',out);
								break;
							case 37:
								fputc('w',out);
								break;
							case 40:
								fputc('0',out);
								break;
							case 41:
								fputc('1',out);
								break;
							case 42:
								fputc('2',out);
								break;
							case 43:
								fputc('3',out);
								break;
							case 44:
								fputc('4',out);
								break;
							case 45:
								fputc('5',out);
								break;
							case 46:
								fputc('6',out);
								break;
							case 47:
								fputc('7',out);
								break; } }
					break;
				case 'C':
					fprintf(out,"\1%c",0x7f+n[0]);
					break;
				default:
					printf("Unsupported ANSI code '%c'\r\n",ch);
					break; }
			break; }	/* end of while */
		esc=0;
		continue; } 	/* end of ANSI expansion */
	if(ch=='\x1b')
		esc=1;
	else {
		esc=0;
		fputc(ch,out); } }
fcloseall();
return(0);
}

