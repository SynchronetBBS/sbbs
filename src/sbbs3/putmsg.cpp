/* putmsg.cpp */

/* Synchronet message/menu display routine */
 
/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "sbbs.h"

/****************************************************************************/
/* Outputs a NULL terminated string locally and remotely (if applicable)	*/
/* checking for message aborts, pauses, ANSI escape and ^A sequences.		*/
/* Changes local text attributes if necessary. Max length of str is 4 gig	*/
/* Returns the last char of the buffer access.. 0 if not aborted.           */
/* If P_SAVEATR bit is set in mode, the attributes set by the message       */
/* will be the current attributes after the message is displayed, otherwise */
/* the attributes prior to diplaying the message are always restored.       */
/* Ignores Ctrl-Z's                                                         */
/****************************************************************************/
char sbbs_t::putmsg(char HUGE16 *str, long mode)
{
	char	tmpatr,tmp2[256],tmp3[128];
	uchar	exatr=0;
	int 	orgcon=console,i;
	ulong	l=0,sys_status_sav=sys_status;

	tmpatr=curatr;	/* was lclatr(-1) */
	if(!(mode&P_SAVEATR))
		attr(LIGHTGRAY);
	if(mode&P_NOPAUSE)
		sys_status|=SS_PAUSEOFF;
	if(mode&P_HTML)
		putcom("\x02\x02");
	while(str[l] && (mode&P_NOABORT || !msgabort()) && online) {
		if(str[l]==CTRL_A && str[l+1]!=0) {
			if(str[l+1]=='"' && !(sys_status&SS_NEST_PF)) {  /* Quote a file */
				l+=2;
				i=0;
				while(i<12 && isprint(str[l]) && str[l]!='\\' && str[l]!='/')
					tmp2[i++]=str[l++];
				tmp2[i]=0;
				sys_status|=SS_NEST_PF; 	/* keep it only one message deep! */
				sprintf(tmp3,"%s%s",cfg.text_dir,tmp2);
				printfile(tmp3,0);
				sys_status&=~SS_NEST_PF; }
			else if(toupper(str[l+1])=='Z')             /* Ctrl-AZ==EOF */
				break;
			else {
				ctrl_a(str[l+1]);
				l+=2; } }
		else if((str[l]=='`' || str[l]=='ú') && str[l+1]=='[') {   
			outchar(ESC); /* Convert `[ and ú[ to ESC[ */
			l++; }
		else if(cfg.sys_misc&SM_PCBOARD && str[l]=='@' && str[l+1]=='X'
			&& isxdigit(str[l+2]) && isxdigit(str[l+3])) {
			sprintf(tmp2,"%.2s",str+l+2);
			attr(ahtoul(tmp2));
			exatr=1;
			l+=4; }
		else if(cfg.sys_misc&SM_WILDCAT && str[l]=='@' && str[l+3]=='@'
			&& isxdigit(str[l+1]) && isxdigit(str[l+2])) {
			sprintf(tmp2,"%.2s",str+l+1);
			attr(ahtoul(tmp2));
			// exatr=1;
			l+=4; }
		else if(cfg.sys_misc&SM_RENEGADE && str[l]=='|' && isdigit(str[l+1])
			&& !(useron.misc&(RIP|WIP))) {
			sprintf(tmp2,"%.2s",str+l+1);
			i=atoi(tmp2);
			if(i>=16) { 				 /* setting background */
				i-=16;
				i<<=4;
				i|=(curatr&0x0f); } 	/* leave foreground alone */
			else
				i|=(curatr&0xf0);		/* leave background alone */
			attr(i);
			exatr=1;
			l+=2;	/* Skip |x */
			if(isdigit(str[l]))
				l++; }	/* Skip second digit if it exists */
		else if(cfg.sys_misc&SM_CELERITY && str[l]=='|' && isalpha(str[l+1])
			&& !(useron.misc&(RIP|WIP))) {
			switch(str[l+1]) {
				case 'k':
					attr((curatr&0xf0)|BLACK);
					break;
				case 'b':
					attr((curatr&0xf0)|BLUE);
					break;
				case 'g':
					attr((curatr&0xf0)|GREEN);
					break;
				case 'c':
					attr((curatr&0xf0)|CYAN);
					break;
				case 'r':
					attr((curatr&0xf0)|RED);
					break;
				case 'm':
					attr((curatr&0xf0)|MAGENTA);
					break;
				case 'y':
					attr((curatr&0xf0)|YELLOW);
					break;
				case 'w':
					attr((curatr&0xf0)|LIGHTGRAY);
					break;
				case 'd':
					attr((curatr&0xf0)|BLACK|HIGH);
					break;
				case 'B':
					attr((curatr&0xf0)|BLUE|HIGH);
					break;
				case 'G':
					attr((curatr&0xf0)|GREEN|HIGH);
					break;
				case 'C':
					attr((curatr&0xf0)|CYAN|HIGH);
					break;
				case 'R':
					attr((curatr&0xf0)|RED|HIGH);
					break;
				case 'M':
					attr((curatr&0xf0)|MAGENTA|HIGH);
					break;
				case 'Y':   /* Yellow */
					attr((curatr&0xf0)|YELLOW|HIGH);
					break;
				case 'W':
					attr((curatr&0xf0)|LIGHTGRAY|HIGH);
					break;
				case 'S':   /* swap foreground and background */
					attr((curatr&0x07)<<4);
					break; }
			exatr=1;
			l+=2;	/* Skip |x */
			}  /* Skip second digit if it exists */
		else if(cfg.sys_misc&SM_WWIV && str[l]==CTRL_C && isdigit(str[l+1])) {
			exatr=1;
			switch(str[l+1]) {
				default:
					attr(LIGHTGRAY);
					break;
				case '1':
					attr(CYAN|HIGH);
					break;
				case '2':
					attr(BROWN|HIGH);
					break;
				case '3':
					attr(MAGENTA);
					break;
				case '4':
					attr(LIGHTGRAY|HIGH|BG_BLUE);
					break;
				case '5':
					attr(GREEN);
					break;
				case '6':
					attr(RED|HIGH|BLINK);
					break;
				case '7':
					attr(BLUE|HIGH);
					break;
				case '8':
					attr(BLUE);
					break;
				case '9':
					attr(CYAN);
					break; }
			l+=2; }
		else {
			if(str[l]==LF) {
				if(exatr) 	/* clear at newline for extra attr codes */
					attr(LIGHTGRAY);
				if(l && str[l-1]!=CR)	/* convert sole LF to CR/LF */
					outchar(CR);
			}

			/* ansi escape sequence */
			if(outchar_esc) {
				if(str[l]=='A' || str[l]=='B' || str[l]=='H' || str[l]=='J'
					|| str[l]=='f' || str[l]=='u')    /* ANSI anim */
					lncntr=0;			/* so defeat pause */
				if(str[l]=='"') {
					l++;				/* don't pass on keyboard reassignment */
					continue; } }
			if(str[l]=='!' && str[l+1]=='|' && useron.misc&(RIP|WIP)) /* RIP */
				lncntr=0;				/* so defeat pause */
			if(str[l]==ESC && str[l+1]=='$')    /* WIP command */
				lncntr=0;
			if(str[l]=='@' && !(mode&P_NOATCODES)) {
				i=show_atcode((char *)str+l);	/* returns 0 if not valid @ code */
				l+=i;					/* i is length of code string */
				if(i)					/* if valid string, go to top */
					continue; }
			if(str[l]!=26)
				outchar(str[l]);
			l++; } }
	if(!(mode&P_SAVEATR)) {
		console=orgcon;
		attr(tmpatr); 
	}
	if(mode&P_HTML)
		putcom("\x02");

	/* Restore original settings of Forced Pause On/Off */
	sys_status&=~(SS_PAUSEOFF|SS_PAUSEON);
	sys_status|=(sys_status_sav&(SS_PAUSEOFF|SS_PAUSEON));
	return(str[l]);
}

/****************************************************************************/
/* Displays a text file to the screen, reading/display a block at time		*/
/****************************************************************************/
void sbbs_t::putmsg_fp(FILE *fp, long length, long mode)
{
	char *buf,tmpatr;
	int i,j,b=8192,orgcon=console;
	long l;

	tmpatr=curatr;	/* was lclatr(-1) */
	if((buf=(char *)MALLOC(b+1))==NULL) {
		errormsg(WHERE,ERR_ALLOC,nulstr,b+1L);
		return; }
	for(l=0;l<length;l+=b) {
		if(l+b>length)
			b=length-l;
		i=j=fread(buf,1,b,fp);
		if(!j) break;						/* No bytes read */
		if(l+i<length)						/* Not last block */
			while(i && buf[i-1]!=LF) i--;	/* Search for last LF */
		if(!i) i=j; 						/* None found */
		buf[i]=0;
		if(i<j)
			fseek(fp,(long)-(j-i),SEEK_CUR);
		b=i;
		if(putmsg(buf,mode|P_SAVEATR))
			break; }
	if(!(mode&P_SAVEATR)) {
		console=orgcon;
		attr(tmpatr); }
	FREE(buf);
}
