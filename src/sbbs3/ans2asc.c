/* ans2asc.c */

/* Convert ANSI messages to Synchronet .asc (Ctrl-A code) format */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>		/* isdigit */

int main(int argc, char **argv)
{
	char esc,n[25];
	char revision[16];
	int i,ch,ni;
	FILE *in,*out;

	sscanf("$Revision$", "%*s %s", revision);

	if(argc<3) {
		fprintf(stderr,"\nans2asc %s\n",revision);
		fprintf(stderr,"\nusage: %s infile.ans outfile.asc\n",argv[0]);
		return(0); 
	}

	if((in=fopen(argv[1],"rb"))==NULL) {
		perror(argv[1]);
		return(1); 
	}

	if((out=fopen(argv[2],"wb"))==NULL) {
		perror(argv[2]);
		return(1); 
	}

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
							ch=fgetc(in); 
						} 
					}
					ni++; 
				}
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
						fprintf(stderr,"Unsupported ANSI code '%c' (0x%02X)\r\n",ch,ch);
						break; 
				}
				break; 
			}	/* end of while */
			esc=0;
			continue; 
		} 	/* end of ANSI expansion */
		if(ch=='\x1b')
			esc=1;
		else {
			esc=0;
			fputc(ch,out); 
		} 
	}
	return(0);
}

