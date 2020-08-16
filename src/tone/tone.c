/* tone.c */

/* Tone Generation Utility (using PC speaker, not sound card) */

/* $Id: tone.c,v 1.11 2015/05/11 04:29:23 rswindell Exp $ */

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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "xpbeep.h"	/* BEEP */
#include "genwrap.h"
#include "dirwrap.h"	/* getfname */
#include "conwrap.h"	/* kbhit */

#define NOT_ABORTABLE	(1<<0)
#define SHOW_DOT		(1<<1)
#define SHOW_FREQ		(1<<2)
#define NO_VISUAL		(1<<3)

int mode=0; 	/* Optional modes */
int t=1;		/* Timing */
int s=0;		/* Stacato */
int octave=4;	/* Default octave */

double pitch=523.50/32.0;	 /* low 'C' */

BOOL use_xptone=FALSE;

enum WAVE_SHAPE wave_shape;

void play(char *freq, char *dur)
{
	char*	notes="c d ef g a b";
	char*	sharp="BC D EF G A ";
	int		i,n,o,d;
	int		len;
	double	f;

	if(dur==NULL)
		dur="0";

	if(freq==NULL)
		freq="0";

	d=atoi(dur);
	if(isdigit(*freq))
		f=atoi(freq);
  	else
		switch(toupper(*freq)) {
			case 'O':               /* default octave */
				if(isdigit(*dur))
					octave=d;
				else
					octave+=d;
				return;
			case 'P':               /* pitch variation */
				if(isdigit(*dur))
					pitch=atof(dur)/32.0;
				else
					pitch+=atof(dur);
				return;
			case 'Q':               /* quit */
				exit(0);
			case 'R':               /* rest */
				f=0;
				break;
			case 'S':               /* stacato */
				if(isdigit(*dur))
					s=d;
				else
					s+=d;
				return;
			case 'T':               /* time adjust */
				t=d;
				return;
			case 'V':
				if(mode&NO_VISUAL)
					return;
				n=strlen(dur);
				while(n && dur[n]<=' ')
					n--;
				dur[n+1]=0;
				if(dur[n]=='\\') {
					dur[n]=0;
					printf("%s",dur); 
				} else
					printf("%s\r\n",dur);
				return;
			case 'X':               /* exit */
				exit(1);
			default:
				for(n=0;notes[n];n++)
					if(*freq==notes[n] || *freq==sharp[n])
						break;
				if(isdigit(freq[1]))
					o=(freq[1]&0xf);
				else
					o=octave;
				f=pitch*pow(2,o+(double)n/12);
				break; 
	}

	if(f && mode&SHOW_FREQ) {
		for(i=0;freq[i]>' ';i++)
			;
		freq[i]=0;
		printf("%-4.4s",freq); 
	}
	if(mode&SHOW_DOT)
		printf(".");
	if(t>10)
		len=(d*t)-(d*s);
	else
		len=(d*t);
	if(f) {
		if(use_xptone)
			xptone(f,len,wave_shape);
		else
			BEEP(f,len);
	}
	else
		SLEEP(len);
	if(s) {
		if(t>10)
			SLEEP(d*s);
		else
			SLEEP(s);
	}
}

void usage(void)
{
	printf("usage: tone [-opts] [(note[oct]|freq) dur | (cmd val) [...]] "
		"[+filename]\n\n");
	printf("where: note  = a,b,c,d,e,f, or g (naturals) or A,B,C,D,E,F, or "
		"G (sharps)\n");
	printf("       oct   = octave 1 through 9 (default=%d)\n",octave);
	printf("       freq  = frequency (in Hz) or 0 for silence\n");
	printf("       dur   = duration (in timer counts)\n");
	printf("       cmd   = o set default octave (+/- to adjust) "
		"(default=%d)\n",octave);
	printf("               p set middle c pitch (+/- to adjust) "
		"(default=%.2f)\n",pitch*32.0);
	printf("               q quit program immediately\n");
	printf("               r rest (silence) for val timer counts\n");
	printf("               s set stacato duration (in ms) (+/- to adjust) "
		"(default=%d)\n",s);
	printf("               t set timer count value (in ms) "
		"(default=%d)\n",t);
	printf("               v visual text diplay of val (no val=cr/lf)\n");
	printf("               x quit program immediately (leave tone on)\n");
	printf("       opts  = d display dot for each note\n");
	printf("               f display frequency or note value\n");
	printf("               n not abortable with key-stroke\n");
	printf("               v disable visual text commands\n");
	printf("               x# use xptone (wave/dsp output) instead of beep macro\n");
	printf("                  optionally, specify wave shape number\n");
	exit(0);
}

int main(int argc, char **argv)
{
	char*	p;
	char	str[128];
	char	revision[16];
	int		i;
	FILE*	stream;

	sscanf("$Revision: 1.11 $", "%*s %s", revision);

	printf("\nTone Generation Utility  %s  Copyright %s Rob Swindell\n\n"
		,revision, __DATE__+7);

	if(argc<2)
		usage();

	setvbuf(stdout,NULL,_IONBF,0);
	for(i=1;i<argc;i++) {
		if(argv[i][0]=='-') {
			switch(toupper(argv[i][1])) {
				case 'D':
					mode^=SHOW_DOT;
					break;
				case 'F':
					mode^=SHOW_FREQ;
					break;
				case 'N':
					mode^=NOT_ABORTABLE;
					break;
				case 'V':
					mode^=NO_VISUAL;
					break;
				case 'X':
					use_xptone=TRUE;
					wave_shape=atoi(argv[i]+2);
					break;
				default:
					usage();
					break;
			}
			continue; 
		}
		if(use_xptone)
			xptone_open();
		if(argv[i][0]=='+') {
			if((stream=fopen(argv[i]+1,"rb"))==NULL) {
				/* Check directory of executable if file/path not found */
				strcpy(str,argv[0]);
				*getfname(str)=0;
				strcat(str,argv[i]+1);
				if((stream=fopen(str,"rb"))==NULL) {
					printf("\7Error opening %s\n",argv[i]+1);
					continue; 
				} 
			}
			while(mode&NOT_ABORTABLE || !kbhit()) {
				if(!fgets(str,sizeof(str),stream))
					break;
				if(!isalnum(*str))
					continue;
				p=str;
				FIND_WHITESPACE(p);
				SKIP_WHITESPACE(p);
				play(str,p); 
			}
			fclose(stream);
			continue; 
		}
		play(argv[i],argv[i+1]);
		if(use_xptone)
			xptone_close();
		i++;
	}

	return(0);
}

