#line 1 "MSGTOQWK.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "qwk.h"
#include "etext.h"

#define MAX_MSGNUM	0x7FFFFFUL	// only 7 (decimal) digits allowed for msg num 

/****************************************************************************/
/* Converts message 'msg' to QWK format, writing to file 'qwk_fp'.          */
/* mode determines how to handle Ctrl-A codes								*/
/****************************************************************************/
ulong msgtoqwk(smbmsg_t msg, FILE *qwk_fp, int mode, int subnum
	, int conf)
{
	uchar	str[512],from[512],to[512],ch,tear=0,tearwatch=0,HUGE16 *buf,*p;
	ushort	xlat;
	long	l,size=0,offset;
	int 	i;

offset=ftell(qwk_fp);
memset(str,SP,128);
fwrite(str,128,1,qwk_fp);		/* Init header to space */

if(msg.from_net.addr && (uint)subnum==INVALID_SUB) {
	if(mode&TO_QNET)
		sprintf(from,"%.128s",msg.from);
	else if(msg.from_net.type==NET_FIDO)
		sprintf(from,"%.128s@%.128s"
			,msg.from,faddrtoa(*(faddr_t *)msg.from_net.addr));
	else if(msg.from_net.type==NET_INTERNET)
		sprintf(from,"%.128s",msg.from_net.addr);
	else
		sprintf(from,"%.128s@%.128s",msg.from,msg.from_net.addr);
	if(strlen(from)>25) {
		sprintf(str,"From: %.128s\xe3\xe3",from);
		fwrite(str,strlen(str),1,qwk_fp);
		size+=strlen(str);
		sprintf(from,"%.128s",msg.from); } }
else {
	sprintf(from,"%.128s",msg.from);
	if(msg.hdr.attr&MSG_ANONYMOUS && !SYSOP)	   /* from user */
		strcpy(from,text[Anonymous]); }

if(msg.to_net.addr && (uint)subnum==INVALID_SUB) {
	if(msg.to_net.type==NET_FIDO)
		sprintf(to,"%.128s@%s",msg.to,faddrtoa(*(faddr_t *)msg.to_net.addr));
	else if(msg.to_net.type==NET_INTERNET)
		sprintf(to,"%.128s",msg.to_net.addr);
	else if(msg.to_net.type==NET_QWK) {
		if(mode&TO_QNET) {
			p=strchr(msg.to_net.addr,'/');
			if(p) { 	/* Another hop */
				p++;
				strcpy(to,"NETMAIL");
				sprintf(str,"%.128s@%.128s\xe3",msg.to,p);
				fwrite(str,strlen(str),1,qwk_fp);
				size+=strlen(str); }
			else
				sprintf(to,"%.128s",msg.to); }
		else
			sprintf(to,"%.128s@%.128s",msg.to,msg.to_net.addr); }
	else
		sprintf(to,"%.128s@%.128s",msg.to,msg.to_net.addr);
	if(strlen(to)>25) {
		sprintf(str,"To: %.128s\xe3\xe3",to);
		fwrite(str,strlen(str),1,qwk_fp);
		size+=strlen(str);
		if(msg.to_net.type==NET_QWK)
			strcpy(to,"NETMAIL");
		else
			sprintf(to,"%.128s",msg.to); } }
else
	sprintf(to,"%.128s",msg.to);

if(msg.from_net.type==NET_QWK && mode&VIA && !msg.forwarded) {
	sprintf(str,"@VIA:%.128s\xe3",msg.from_net.addr);
	fwrite(str,strlen(str),1,qwk_fp);
	size+=strlen(str); }

if(msg.hdr.when_written.zone && mode&TZ) {
	sprintf(str,"@TZ:%04x\xe3",msg.hdr.when_written.zone);
	fwrite(str,strlen(str),1,qwk_fp);
	size+=strlen(str); }

p=0;
for(i=0;i<msg.total_hfields;i++) {
	if(msg.hfield[i].type==SENDER)
		p=msg.hfield_dat[i];
	if(msg.hfield[i].type==FORWARDED && p) {
		sprintf(str,"Forwarded from %s on %s\xe3",p
			,timestr((time_t *)msg.hfield_dat[i]));
		fwrite(str,strlen(str),1,qwk_fp);
		size+=strlen(str); } }

buf=smb_getmsgtxt(&smb,&msg,1);
if(!buf)
	return(0);

for(l=0;buf[l];l++) {
	ch=buf[l];

	if(ch==LF) {
		if(tear)
			tear++; 				/* Count LFs after tearline */
		if(tear>3)					/* more than two LFs after the tear */
			tear=0;
		if(tearwatch==4) {			/* watch for LF---LF */
			tear=1;
			tearwatch=0; }
		else if(!tearwatch)
			tearwatch=1;
		else
			tearwatch=0;
		ch=0xe3;
		fputc(ch,qwk_fp);		  /* Replace LF with funky char */
		size++;
		continue; }

	if(ch==CR) {					/* Ignore CRs */
		if(tearwatch<4) 			/* LF---CRLF is okay */
			tearwatch=0;			/* LF-CR- is not okay */
		continue; }

	if(ch==SP && tearwatch==4) {	/* watch for "LF--- " */
		tear=1;
		tearwatch=0; }

	if(ch=='-') {                   /* watch for "LF---" */
		if(l==0 || (tearwatch && tearwatch<4))
			tearwatch++;
		else
			tearwatch=0; }
	else
		tearwatch=0;

	if((uint)subnum!=INVALID_SUB && sub[subnum]->misc&SUB_ASCII) {
		if(ch<SP && ch!=1)
			ch='.';
		else if(ch>0x7f)
			ch='*'; }

	if(ch==0xE3)					/* funky char */
		ch='*';

	if(ch==1) {  /* ctrl-a */
		ch=buf[++l];
		if(!ch)
			break;
		if(mode&A_EXPAND) {
			str[0]=0;
			switch(toupper(ch)) {		/* non-color codes */
				case 'L':
					strcpy(str,"\x1b[2J");
					break;
				case 'W':
					strcpy(str,ansi(LIGHTGRAY));
					break;
				case 'K':
					strcpy(str,ansi(BLACK));
					break;
				case 'H':
					strcpy(str,ansi(HIGH));
					break;
				case 'I':
					strcpy(str,ansi(BLINK));
					break;
				case 'N':   /* Normal */
					strcpy(str,"\x1b[0m");
					break;
				case '0':
					strcpy(str,"\x1b[40m");
					break;
				case '7':
					strcpy(str,ansi(LIGHTGRAY<<4));
					break;
				case 'R':                               /* Color codes */
					strcpy(str,ansi(RED));
					break;
				case 'G':
					strcpy(str,ansi(GREEN));
					break;
				case 'B':
					strcpy(str,ansi(BLUE));
					break;
				case 'C':
					strcpy(str,ansi(CYAN));
					break;
				case 'M':
					strcpy(str,ansi(MAGENTA));
					break;
				case 'Y':   /* Yellow */
					strcpy(str,ansi(BROWN));
					break;
				case '1':
					strcpy(str,ansi(RED<<4));
					break;
				case '2':
					strcpy(str,ansi(GREEN<<4));
					break;
				case '3':
					strcpy(str,ansi(BROWN<<4));
					break;
				case '4':
					strcpy(str,ansi(BLUE<<4));
					break;
				case '5':
					strcpy(str,ansi(MAGENTA<<4));
					break;
				case '6':
					strcpy(str,ansi(CYAN<<4));
					break; }
			if(str[0]) {
				fwrite(str,strlen(str),1,qwk_fp);
				size+=strlen(str); }
			continue; } 						/* End Expand */

		if(mode&A_LEAVE) {
			fputc(1,qwk_fp);
			fputc(ch,qwk_fp);
			size+=2L; }
		else									/* Strip */
			if(toupper(ch)=='L') {
				fputc(FF,qwk_fp);
				size++; }
		continue; } 							/* End of Ctrl-A shit */
	fputc(ch,qwk_fp);
	size++; }

LFREE(buf);
if(ch!=0xE3) {
	fputc(0xE3,qwk_fp); 		/* make sure it ends in CRLF */
	size++; }

if(mode&TAGLINE && !(sub[subnum]->misc&SUB_NOTAG)) {
	if(!tear)										/* no tear line */
		strcpy(str,"\1n---\xe3");                   /* so add one */
    else
		strcpy(str,"\1n");
	if(sub[subnum]->misc&SUB_ASCII) ch='*';
	else ch='þ';
	sprintf(tmp," %c \1g%s\1n %c %.127s\xe3"
		,ch,decrypt(Synchronet,0),ch,sub[subnum]->tagline);
	strcat(str,tmp);
	if(!(mode&A_LEAVE))
		remove_ctrl_a(str);
	fwrite(str,strlen(str),1,qwk_fp);
	size+=strlen(str); }

while(size%128L) {				 /* Pad with spaces */
	size++;
	fputc(SP,qwk_fp); }

unixtodos(msg.hdr.when_written.time,&date,&curtime);

sprintf(tmp,"%02u-%02u-%02u%02u:%02u"
    ,date.da_mon,date.da_day,TM_YEAR(date.da_year-1900)
    ,curtime.ti_hour,curtime.ti_min);

if(msg.hdr.attr&MSG_PRIVATE) {
    if(msg.hdr.attr&MSG_READ)
        ch='*'; /* private, read */
    else
        ch='+'; /* private, unread */ }
else {
    if(msg.hdr.attr&MSG_READ)
        ch='-'; /* public, read */
    else
        ch=' '; /* public, unread */ }


sprintf(str,"%c%-7lu%-13.13s%-25.25s"
	"%-25.25s%-25.25s%12s%-8lu%-6lu\xe1%c%c%c%c%c"
    ,ch                     /* message status flag */
	,mode&REP ? (ulong)conf /* conference or */
		: msg.hdr.number&MAX_MSGNUM	/* message number */
	,tmp					/* date and time */
	,to 					/* To: */
	,from					/* From: */
    ,msg.subj               /* Subject */
    ,nulstr                 /* Password */
    ,msg.hdr.thread_orig&MAX_MSGNUM    /* Message Re: Number */
	,(size/128L)+1			/* Number of 128byte blocks */
    ,(char)conf&0xff        /* Conference number lo byte */
	,(ushort)conf>>8		/*					 hi byte */
    ,SP                     /* not used */
    ,SP                     /* not used */
    ,useron.rest&FLAG('Q') ? '*' : SP     /* Net tag line */
    );

fseek(qwk_fp,offset,SEEK_SET);
fwrite(str,128,1,qwk_fp);
fseek(qwk_fp,size,SEEK_CUR);

return(size);
}
