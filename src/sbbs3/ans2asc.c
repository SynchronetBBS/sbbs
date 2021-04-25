/* Convert ANSI messages to Synchronet Ctrl-A code format */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>		/* strcmp */
#include "sauce.h"
#include "git_branch.h"
#include "git_hash.h"

#ifndef CTRL_Z
#define CTRL_Z 0x1a
#endif

static void print_usage(const char* prog)
{
	fprintf(stderr,"\nSynchronet ANSI-Terminal-Sequence to Ctrl-A-Code Conversion Utility %s/%s\n"
		,GIT_BRANCH
		,GIT_HASH
	);
	fprintf(stderr,"\nusage: %s infile.ans [outfile.asc | outfile.msg] [[option] [...]]\n",prog);
	fprintf(stderr,"\noptions:\n\n");
	fprintf(stderr,"  -ice            treat blink as bright-background (iCE colors)\n");
	fprintf(stderr,"  -<columns>      insert conditional-newlines to force wrap (e.g. -80)\n");
	fprintf(stderr,"  -normal         use 'save/normal/restore' attributes for conditional new-lines\n");
	fprintf(stderr,"  -newline        append a newline (CRLF) sequence to output file\n");
	fprintf(stderr,"  -clear          insert a clear screen code at beginning of output file\n");
	fprintf(stderr,"  -pause          append a pause (hit a key) code to end of output file\n");
	fprintf(stderr,"  -space          use space characters for cursor-right movement/alignment\n");
	fprintf(stderr,"  -delay <int>    insert a 1/10th second delay code at output byte interval\n");
	fprintf(stderr,"                  (lower interval values result in more delays, slower display)\n");
}

int main(int argc, char **argv)
{
	unsigned char esc,n[25];
	int i,ch,ni;
	FILE *in=stdin;
	FILE *out=stdout;
	bool ice=false;
	bool normal=false;
	int cols=0;
	int column=0;
	int delay=0;
	int clear=0;
	int pause=0;
	int space=0;
	int newline=0;

	if(argc<2) {
		print_usage(argv[0]);
		return(0); 
	}

	for(i=1; i<argc; i++)  {
		if(argv[i][0]=='-') {
			if(strcmp(argv[i], "-delay") == 0) {
				if(i+1 == argc) {
					print_usage(argv[0]);
					return -1;
				}
				if((delay=atoi(argv[++i])) < 1) {
					print_usage(argv[0]);
					return -1;
				}
			}
			else if(strcmp(argv[i], "-ice") == 0)
				ice = true;
			else if(strcmp(argv[i], "-clear") == 0)
				clear = 1;
			else if(strcmp(argv[i], "-pause") == 0)
				pause = 1;
			else if(strcmp(argv[i], "-space") == 0)
				space = 1;
			else if(strcmp(argv[i], "-normal") == 0)
				normal = true;
			else if(strcmp(argv[i], "-newline") == 0)
				newline++;
			else if(IS_DIGIT(argv[i][1]))
				cols = atoi(argv[i] + 1);
			else {
				print_usage(argv[0]);
				return 0;
			}
		} else if(in==stdin) {
			if((in=fopen(argv[i],"rb"))==NULL) {
				perror(argv[i]);
				return(1); 
			}
		} else if(out==stdout) {
			if((out=fopen(argv[i],"wb"))==NULL) {
				perror(argv[i]);
				return(1);
			}
		}
	}

	if(in != stdin && cols < 1) {
		struct sauce_charinfo info;
		if(sauce_fread_charinfo(in, /* type: */NULL, &info)) {
			cols = info.width;
			ice = info.ice_color;
		}
	}

	const char* cond_newline = normal ? "\1+\1N\1/\1-" : "\1/";

	if(clear)
		fprintf(out,"\1N\1L");
	esc=0;
	while((ch=fgetc(in))!=EOF && ch != CTRL_Z) {
		if(ch=='[' && esc) {    /* ANSI escape sequence */
			if(cols && column >= cols) {
				fprintf(out, "%s", cond_newline);	// Conditional-newline
				column = 0;
			}
			ni=0;				/* zero number index */
			memset(n,1,sizeof(n));
			while((ch=fgetc(in))!=EOF) {
				if(IS_DIGIT(ch)) {			/* 1 digit */
					n[ni]=ch&0xf;
					ch=fgetc(in);
					if(ch==EOF)
						break;
					if(IS_DIGIT(ch)) {		/* 2 digits */
						n[ni]*=10;
						n[ni]+=ch&0xf;
						ch=fgetc(in);
						if(ch==EOF)
							break;
						if(IS_DIGIT(ch)) {	/* 3 digits */
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
						if(IS_DIGIT(ch)) ch=fgetc(in);	/* l or h ? */
						if(IS_DIGIT(ch)) ch=fgetc(in);
						if(IS_DIGIT(ch)) fgetc(in);
						break;
					case 'f':
					case 'H':
						if(n[0]<=1 && n[1]<=1)			/* home cursor */
							fputs("\1'", out);
						column = 0;
						break;
					case 'J':
						if(n[0]==2) 					/* clear screen */
							fputs("\1L",out);           /* ctrl-aL */
						else if(n[0]==0)				/* clear to EOS */
							fputs("\1J",out);           /* ctrl-aJ */
						column = 0;
						break;
					case 'K':
						fputs("\1>",out);               /* clear to eol */
						column = cols ? (cols - 1) : 79;
						break;
					case 'm':
						for(i=0;i<ni;i++) {
							fputc(1,out);				/* ctrl-ax */
							switch(n[i]) {
								case 0:
								case 2: 				/* no attribute */
									fputc('N',out);
									break;
								case 1: 				/* high intensity */
									fputc('H',out);
									break;
								case 3:
								case 4:
								case 5: 				/* blink */
								case 6:
								case 7:
									fputc(ice ? 'E': 'I',out);
									break;
								case 30:
									fputc('K',out);
									break;
								case 31:
									fputc('R',out);
									break;
								case 32:
									fputc('G',out);
									break;
								case 33:
									fputc('Y',out);
									break;
								case 34:
									fputc('B',out);
									break;
								case 35:
									fputc('M',out);
									break;
								case 36:
									fputc('C',out);
									break;
								case 37:
									fputc('W',out);
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
									break; 
								default:
									fprintf(stderr,"Unsupported ANSI color code: %u\n", (unsigned int)n[i]);
									break;
							} 
						}
						break;
					case 'B':	/* cursor down */
						while(n[0]) {
							fprintf(out,"\1]");	/* linefeed */
							n[0]--;
						}
						break;
					case 'C':	/* cursor right */
						if(space)
							fprintf(out, "%*s", n[0], " ");
						else
							fprintf(out,"\1%c",0x7f+n[0]);
						column += n[0];
						break;
					case 'D':	/* cursor left */
						column -= n[0];
						if(n[0] >= column)
							fprintf(out,"\1[");
						else
							while(n[0]) {
								fprintf(out,"\1<");
								n[0]--;
							}
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
			switch(ch) {
				case '\r':
				case '\n':
					fputc(ch,out);
					column = 0;
					break;
				default:
					if(cols && column >= cols) {
						fprintf(out, "%s", cond_newline);	// Conditional-newline
						column = 0;
					}
					fputc(ch,out);
					column++;
					break;
			}
			if(delay && (ftell(out)%delay)==0)
				fprintf(out,"\1,");
		} 
		if(column < 0)
			column = 0;
	}
	while(newline--)
		fprintf(out,"\r\n");
	if(pause)
		fprintf(out,"\1p");
	return(0);
}

