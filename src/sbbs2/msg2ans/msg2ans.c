/* msg2ans.c */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Converts Synchronet Ctrl-A codes into ANSI escape sequences */

#include <stdio.h>
#include <conio.h>

#define ANSI fprintf(out,"\x1b[")

int main(int argc, char **argv)
{
	int i,ch;
	FILE *in,*out;

if(argc<3) {
	printf("\nMSG2ANS v1.03\n");
	printf("\nusage: msg2ans infile.msg outfile.ans\n");
	exit(0); }

if((in=fopen(argv[1],"rb"))==NULL) {
	printf("\nerror opening %s\n",argv[1]);
	exit(1); }

if((out=fopen(argv[2],"wb"))==NULL) {
	printf("\nerror opening %s\n",argv[2]);
	exit(1); }

while((ch=fgetc(in))!=EOF) {
	if(ch==1) { /* ctrl-a */
		ch=fgetc(in);
		if(ch==EOF)
			break;
		if(ch>=0x7f) {					/* move cursor right x columns */
			ANSI;
			fprintf(out,"%uC",ch-0x7f);
			continue; }
		switch(toupper(ch)) {
			case 'A':
				fputc('\1',out);
				break;
			case '<':
				fputc('\b',out);
				break;
			case '>':
				ANSI;
				fputc('K',out);
				break;
			case '[':
				fputc('\r',out);
				break;
			case ']':
				fputc('\n',out);
				break;
			case 'L':
				ANSI;
				fprintf(out,"2J");
				break;
			case '-':
			case '_':
			case 'N':
				ANSI;
				fprintf(out,"0m");
				break;
			case 'H':
				ANSI;
				fprintf(out,"1m");
                break;
			case 'I':
				ANSI;
				fprintf(out,"5m");
				break;
			case 'K':
				ANSI;
				fprintf(out,"30m");
				break;
			case 'R':
				ANSI;
				fprintf(out,"31m");
                break;
			case 'G':
				ANSI;
				fprintf(out,"32m");
                break;
			case 'Y':
				ANSI;
				fprintf(out,"33m");
                break;
			case 'B':
				ANSI;
				fprintf(out,"34m");
                break;
			case 'M':
				ANSI;
				fprintf(out,"35m");
                break;
			case 'C':
				ANSI;
				fprintf(out,"36m");
                break;
			case 'W':
				ANSI;
				fprintf(out,"37m");
                break;
			case '0':
				ANSI;
				fprintf(out,"40m");
                break;
			case '1':
				ANSI;
				fprintf(out,"41m");
                break;
			case '2':
				ANSI;
				fprintf(out,"42m");
                break;
			case '3':
				ANSI;
				fprintf(out,"43m");
                break;
			case '4':
				ANSI;
				fprintf(out,"44m");
                break;
			case '5':
				ANSI;
				fprintf(out,"45m");
                break;
			case '6':
				ANSI;
				fprintf(out,"46m");
                break;
			case '7':
				ANSI;
				fprintf(out,"47m");
				break;
			default:
				fprintf(out,"\1%c",ch);
				break; } }
	else
		fputc(ch,out); }
}




