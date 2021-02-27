/* EXECDOS.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Execute DOS external BBS programs from OS/2 BBS */

#include "sbbs.h"

extern unsigned _heaplen=2048;

extern uint riobp;

ulong user_misc;

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	uint c;

str[strcspn(str,"\t")]=0;
c=strlen(str);
while(c && (uchar)str[c-1]<=SP) c--;
str[c]=0;
}

void interrupt (*oldfunc)(void);

/*****************************************************************************/
/* Interrupt routine to expand WWIV Ctrl-C# codes into ANSI escape sequences */
/*****************************************************************************/
void interrupt wwiv_expand()
{
	char str[256],al;
	static int ctrl_c;
	int i,j;

al=_AL;
if(al!=3 && !ctrl_c)
	oldfunc();
else if(al!=3 && ctrl_c) {
	ctrl_c=0;
	if(user_misc&ANSI) {
		switch(al) {
			default:
				strcpy(str,"\x1b[0m");          /* low grey */
				break;
			case '1':
				strcpy(str,"\x1b[0;1;36m");     /* high cyan */
				break;
			case '2':
				strcpy(str,"\x1b[0;1;33m");     /* high yellow */
				break;
			case '3':
				strcpy(str,"\x1b[0;35m");       /* low magenta */
				break;
			case '4':
				strcpy(str,"\x1b[0;1;44m");     /* white on blue */
				break;
			case '5':
				strcpy(str,"\x1b[0;32m");       /* low green */
				break;
			case '6':
				strcpy(str,"\x1b[0;1;5;31m");   /* high blinking red */
				break;
			case '7':
				strcpy(str,"\x1b[0;1;34m");     /* high blue */
				break;
			case '8':
				strcpy(str,"\x1b[0;34m");       /* low blue */
				break;
			case '9':
				strcpy(str,"\x1b[0;36m");       /* low cyan */
				break; }
		j=strlen(str);
		for(i=0;i<j;i++) {
			_AL=str[i];
			oldfunc(); } } }
else
	ctrl_c=1;
}

int main(int argc, char **argv)
{
	char str[256],commandline[128],sbbsnode[81],sbbsnnum[81],msr[31]="0"
		,user_name[LEN_NAME+1]
		,user_alias[LEN_ALIAS+1]
		,user_phone[LEN_PHONE+1]
		,user_location[LEN_LOCATION+1]
		,user_age
		,user_sex
		,user_level
		,node_scrnlen
		,*envvar[30],*arg[30],c,d;
	int i,file,base,com_base=0,com_irq,dte_rate,rmode,mode,node_num,col,row;
	FILE *stream;

if(argc<2) {
	printf("This program is for the internal use of Synchronet.\r\n");
	return(-1); }

sprintf(sbbsnode,"SBBSNODE=%s",argv[1]);
putenv(sbbsnode);
sprintf(str,"%sEXECDOS.DAT",argv[1]);
if((file=open(str,O_RDONLY|O_BINARY))!=-1) {
	stream=fdopen(file,"rb");
	str[0]=0;
	fgets(str,128,stream);
	truncsp(str);
	if(strcmp(str,"V1.00")) {
		printf("\7EXECDOS: SBBS VERSION MISMATCH!\7\r\n");
		delay(5000);
		return(-1); }
	str[0]=0;
	fgets(str,128,stream);
	com_base=strtoul(str,0,16);
	str[0]=0;
	fgets(str,128,stream);
	com_irq=atoi(str);
	str[0]=0;
	fgets(str,128,stream);
	dte_rate=atoi(str);
	str[0]=0;
    fgets(str,128,stream);
	rmode=strtoul(str,0,16);
	str[0]=0;
    fgets(str,128,stream);
	mode=strtoul(str,0,16);
	str[0]=0;
    fgets(str,128,stream);
	user_misc=strtoul(str,0,16);
	str[0]=0;
    fgets(str,128,stream);
	node_num=atoi(str);
	if(node_num) {
		sprintf(sbbsnnum,"SBBSNNUM=%u",node_num);
		putenv(sbbsnnum); }
	fgets(commandline,128,stream);
	truncsp(commandline);
	str[0]=0;
    fgets(str,128,stream);
	i=atoi(str);	/* total env vars */
	while(i--) {
		str[0]=0;
		fgets(str,128,stream);
		truncsp(str);
		if((envvar[i]=MALLOC(strlen(str)+1))!=NULL) {
			strcpy(envvar[i],str);
			putenv(envvar[i]); } }
	str[0]=0;
	fgets(str,128,stream);
	truncsp(str);
	sprintf(user_alias,"%.*s",LEN_ALIAS,str);
	str[0]=0;
	fgets(str,128,stream);
	truncsp(str);
	sprintf(user_name,"%.*s",LEN_NAME,str);
	str[0]=0;
	fgets(str,128,stream);
	user_level=atoi(str);
	str[0]=0;
	fgets(str,128,stream);
	user_age=atoi(str);
	str[0]=0;
	fgets(str,128,stream);
	user_sex=str[0];
	str[0]=0;
	fgets(str,128,stream);
	truncsp(str);
	sprintf(user_phone,"%.*s",LEN_PHONE,str);
	str[0]=0;
	fgets(str,128,stream);
	truncsp(str);
	sprintf(user_location,"%.*s",LEN_LOCATION,str);
	fclose(stream); }

printf("\nEXECDOS: %s\n",commandline);

if(rmode && com_base) { 	/* Capture the port and intercept I/O */
	base=0xffff;
	switch(com_base) {
		case 0xb:
			rioctl(I14PC);
			break;
		case 0xffff:
		case 0xd:
			rioctl(I14DB);
			break;
		case 0xe:
			rioctl(I14PS);
			break;
		case 0xf:
			rioctl(I14FO);
			break;
		default:
			base=com_base;
			break; }

	if(rioini(base,com_irq)) {
		printf("\7EXECDOS: Error initializing COM port (%x,%d)\7\r\n"
			,base,com_irq);
		return(-1); }
	rioctl(IOSM|CTSCK|RTSCK|PAUSE|ABORT);
	rioctl(CPTON);	/* Cvt ^p to ^^ */

	sprintf(msr,"%lu",&riobp-1);

	sprintf(str,"%sINTRSBBS.DAT",argv[1]);
	if((stream=fopen(str,"wb"))!=NULL) {
		fprintf(stream,"%lu\r\n",&riobp-1);
		fclose(stream); } }

setbaud(dte_rate);

if(rmode) {
	ivhctl(rmode);

	if(mode&EX_WWIV) {	/* WWIV code expansion */
		rioctl(CPTOFF); /* turn off ctrl-p translation */
		oldfunc=getvect(0x29);
		setvect(0x29,wwiv_expand); } }


if(rmode&INT29L && user_alias[0]) {
	node_scrnlen=lclini(0xd<<8);	  /* Tab expansion, no CRLF expansion */
	lclini(node_scrnlen-1);
	col=lclwx();
	row=lclwy();
	lclxy(1,node_scrnlen);
	lclatr(CYAN|HIGH|(BLUE<<4));
	lputs("  ");
	sprintf(str,"%-25.25s %02d %-25.25s  %02d %c %s"
		,user_alias,user_level,user_name[0] ? user_name : user_location
		,user_age
		,user_sex ? user_sex : SP
		,user_phone);
	lputs(str);
	lputc(CLREOL);
	lclatr(LIGHTGRAY);
	lclxy(col,row); }

/* separate args */

arg[0]=commandline; /* point to the beginning of the string */
for(c=0,d=1;commandline[c];c++)   /* Break up command line */
	if(commandline[c]==SP) {
		commandline[c]=0;		/* insert nulls */
		if(!strncmp(commandline+c+1,"%& ",3))
			arg[d++]=msr;
		else					/* point to the beginning of the next arg */
			arg[d++]=commandline+c+1; } 
arg[d]=0;

/* spawn it */

i=spawnvp(P_WAIT,arg[0],arg);

if(rmode) {
	rioctl(TXSYNC|(3<<8));
	rioctl(IOFB);
	if(mode&EX_WWIV)
		setvect(0x29,oldfunc);
	ivhctl(0);			/* replace DOS output interrupt vectors */
	if(com_base)
		rioini(0,0); }	/* replace com port */

//if(i) 			// debug
//	  getch();
return(i);
}
