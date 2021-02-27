#line 1 "PUTMSG.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

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
char putmsg(char HUGE16 *str, int mode)
{
	uchar	tmpatr,tmp2[256],tmp3[128],*p,exatr=0;
	int 	orgcon=console,i;
	ulong	l=0,sys_status_sav=sys_status;

tmpatr=curatr;	/* was lclatr(-1) */
if(!(mode&P_SAVEATR))
	attr(LIGHTGRAY);
while(str[l] && (mode&P_NOABORT || !msgabort()) && online) {
	if(str[l]==1) {		/* Ctrl-Ax sequence */
		if(str[l+1]=='"' && !(sys_status&SS_NEST_PF)) {  /* Quote a file */
			l+=2;
			i=0;
			while(i<12 && isprint(str[l]) && str[l]!='\\' && str[l]!='/')
				tmp2[i++]=str[l++];
			tmp2[i]=0;
			sys_status|=SS_NEST_PF; 	/* keep it only one message deep! */
			sprintf(tmp3,"%s%s",text_dir,tmp2);
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
	else if(sys_misc&SM_PCBOARD && str[l]=='@' && str[l+1]=='X'
		&& isxdigit(str[l+2]) && isxdigit(str[l+3])) {
		sprintf(tmp2,"%.2s",str+l+2);
		attr(ahtoul(tmp2));
		exatr=1;
		l+=4; }
	else if(sys_misc&SM_WILDCAT && str[l]=='@' && str[l+3]=='@'
		&& isxdigit(str[l+1]) && isxdigit(str[l+2])) {
		sprintf(tmp2,"%.2s",str+l+1);
		attr(ahtoul(tmp2));
		// exatr=1;
		l+=4; }
	else if(sys_misc&SM_RENEGADE && str[l]=='|' && isdigit(str[l+1])
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
	else if(sys_misc&SM_CELERITY && str[l]=='|' && isalpha(str[l+1])
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
	else if(sys_misc&SM_WWIV && str[l]==3 && isdigit(str[l+1])) {
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
				attr(LIGHTGRAY|HIGH|(BLUE<<4));
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
		if(exatr && str[l]==LF) 	/* clear at newline for extra attr codes */
			attr(LIGHTGRAY);
		if(lclaes()) {
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
			i=atcodes((char *)str+l);	/* returns 0 if not valid @ code */
			l+=i;					/* i is length of code string */
			if(i)					/* if valid string, go to top */
				continue; }
		if(str[l]!=26)
			outchar(str[l]);
		l++; } }
//curatr=lclatr(-1);		01/29/96
if(!(mode&P_SAVEATR)) {
	console=orgcon;
	attr(tmpatr); }

/* Restore original settings of Forced Pause On/Off */
sys_status&=~(SS_PAUSEOFF|SS_PAUSEON);
sys_status|=(sys_status_sav&(SS_PAUSEOFF|SS_PAUSEON));
return(str[l]);
}
