/* SCB */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "xsdk.h"
#include "scb.h"

/* RCIOLL.ASM */

int  rioini(int com,int irq);          /* initialize com,irq */
int  setbaud(int rate);                /* set baud rate */
int  rioctl(int action);               /* remote i/o control */
int  dtr(char onoff);                  /* set/reset dtr */
int  outcom(int ch);                   /* send character */
int  incom(void);                      /* receive character */
int  ivhctl(int intcode);              /* local i/o redirection */

/************************/
/* Remote I/O Constants */
/************************/

							/* i/o mode and state flags */
#define CTSCK 0x1000     	/* check cts (mode only) */
#define RTSCK 0x2000		/* check rts (mode only) */
#define TXBOF 0x0800		/* transmit buffer overflow (outcom only) */
#define ABORT 0x0400     	/* check for ^C (mode), aborting (state) */
#define PAUSE 0x0200     	/* check for ^S (mode), pausing (state) */
#define NOINP 0x0100     	/* input buffer empty (incom only) */

							/* status flags */
#define RIODCD	0x80		/* DCD on */
#define RI		0x40		/* Ring indicate */
#define DSR 	0x20		/* Dataset ready */
#define CTS 	0x10       	/* CTS on */
#define FERR 	0x08		/* Frameing error */
#define PERR 	0x04		/* Parity error */
#define OVRR 	0x02		/* Overrun */
#define RXLOST 	0x01       	/* Receive buffer overflow */

/* rioctl() arguments */
/* returns mode or state flags in high 8 bits, status flags in low 8 bits */

							/* the following return mode in high 8 bits */
#define IOMODE 0        	/* no operation */
#define IOSM 1          	/* i/o set mode flags */
#define IOCM 2          	/* i/o clear mode flags */
							/* the following return state in high 8 bits */
#define IOSTATE 4       	/* no operation */
#define IOSS 5          	/* i/o set state flags */
#define IOCS 6          	/* i/o clear state flags */
#define IOFB 0x308      	/* i/o buffer flush */
#define IOFI 0x208      	/* input buffer flush */
#define IOFO 0x108      	/* output buffer flush */
#define IOCE 9          	/* i/o clear error flags */

							/* return count (16bit)	*/
#define RXBC	0x0a		/* get receive buffer count */
#define TXBC	0x0b		/* get transmit buffer count */
#define TXSYNC  0x0c        /* sync transmition (seconds<<8|0x0c) */
#define IDLE	0x0d		/* suspend communication routines */
#define RESUME  0x10d		/* return from suspended state */
#define RLERC	0x000e		/* read line error count and clear */
#define CPTON	0x0110		/* set input translation flag for ctrl-p on */
#define CPTOFF	0x0010		/* set input translation flag for ctrl-p off */
#define GETCPT	0x8010		/* return the status of ctrl-p translation */
#define MSR 	0x0011		/* read modem status register */
#define FIFOCTL 0x0012		/* FIFO UART control */
#define TSTYPE	0x0013		/* Time-slice API type */
#define GETTST  0x8013      /* Get Time-slice API type */


#define I14DB	0x001d		/* DigiBoard int 14h driver */
#define I14PC	0x011d		/* PC int 14h driver */
#define I14PS	0x021d		/* PS/2 int 14h driver */
#define I14FO   0x031d      /* FOSSIL int 14h driver */


							/* ivhctl() arguments */
#define INT29R 1         	/* copy int 29h output to remote */
#define INT29L 2			/* Use _putlc for int 29h */
#define INT16  0x10      	/* return remote chars to int 16h calls */
#define INTCLR 0            /* release int 16h, int 29h */

#define LEN_ALIAS		25	/* User alias								*/
#define LEN_NAME		25	/* User name								*/
#define LEN_HANDLE		8	/* User chat handle 						*/
#define LEN_NOTE		30	/* User note								*/
#define LEN_COMP		30	/* User computer description				*/
#define LEN_COMMENT 	60	/* User comment 							*/
#define LEN_NETMAIL 	60	/* NetMail forwarding address				*/
#define LEN_PASS		 8	/* User password							*/
#define LEN_PHONE		12	/* User phone number						*/
#define LEN_BIRTH		 8	/* Birthday in MM/DD/YY format				*/
#define LEN_ADDRESS 	30	/* User address 							*/
#define LEN_LOCATION	30	/* Location (City, State)					*/
#define LEN_ZIPCODE 	10	/* Zip/Postal code							*/
#define LEN_MODEM		 8	/* User modem type description				*/
#define LEN_FDESC		58	/* File description 						*/
#define LEN_TITLE		70	/* Message title							*/
#define LEN_MAIN_CMD	40	/* Storage in user.dat for custom commands	*/
#define LEN_XFER_CMD	40
#define LEN_SCAN_CMD	40
#define LEN_MAIL_CMD	40
#define LEN_CID 		25	/* Caller ID (phone number) 				*/
#define LEN_ARSTR		40	/* Max length of Access Requirement string	*/

/****************************************************************************/
/* This is a list of offsets into the USER.DAT file for different variables */
/* that are stored (for each user)											*/
/****************************************************************************/
#define U_ALIAS 	0					/* Offset to alias */
#define U_NAME		(U_ALIAS+LEN_ALIAS) /* Offset to name */
#define U_HANDLE	(U_NAME+LEN_NAME)
#define U_NOTE		(U_HANDLE+LEN_HANDLE+2)
#define U_COMP		(U_NOTE+LEN_NOTE)
#define U_COMMENT	(U_COMP+LEN_COMP+2)

#define U_NETMAIL	(U_COMMENT+LEN_COMMENT+2)

#define U_ADDRESS	(U_NETMAIL+LEN_NETMAIL+2)
#define U_LOCATION	(U_ADDRESS+LEN_ADDRESS)
#define U_ZIPCODE	(U_LOCATION+LEN_LOCATION)

#define U_PASS		(U_ZIPCODE+LEN_ZIPCODE+2)
#define U_PHONE  	(U_PASS+8) 			/* Offset to phone-number */
#define U_BIRTH  	(U_PHONE+12)		/* Offset to users birthday	*/
#define U_MODEM     (U_BIRTH+8)
#define U_LASTON	(U_MODEM+8)
#define U_FIRSTON	(U_LASTON+8)
#define U_EXPIRE    (U_FIRSTON+8)
#define U_PWMOD     (U_EXPIRE+8)

#define U_LOGONS    (U_PWMOD+8+2)
#define U_LTODAY    (U_LOGONS+5)
#define U_TIMEON    (U_LTODAY+5)
#define U_TEXTRA  	(U_TIMEON+5)
#define U_TTODAY    (U_TEXTRA+5)
#define U_TLAST     (U_TTODAY+5)
#define U_POSTS     (U_TLAST+5)
#define U_EMAILS    (U_POSTS+5)
#define U_FBACKS    (U_EMAILS+5)
#define U_ETODAY	(U_FBACKS+5)
#define U_PTODAY	(U_ETODAY+5)

#define U_ULB       (U_PTODAY+5+2)
#define U_ULS       (U_ULB+10)
#define U_DLB       (U_ULS+5)
#define U_DLS       (U_DLB+10)
#define U_CDT		(U_DLS+5)
#define U_MIN		(U_CDT+10)

#define U_LEVEL 	(U_MIN+10+2)	/* Offset to Security Level    */
#define U_FLAGS1	(U_LEVEL+2) 	/* Offset to Flags */
#define U_TL		(U_FLAGS1+8)	/* Offset to unused field */
#define U_FLAGS2	(U_TL+2)		/* Offset to unused field */
#define U_EXEMPT	(U_FLAGS2+8)
#define U_REST		(U_EXEMPT+8)
#define U_ROWS		(U_REST+8+2)	/* Number of Rows on user's monitor */
#define U_SEX		(U_ROWS+2)		/* Sex, Del, ANSI, color etc.		*/
#define U_MISC		(U_SEX+1)		/* Miscellaneous flags in 8byte hex */
#define U_XEDIT 	(U_MISC+8)		/* External editor */
#define U_LEECH 	(U_XEDIT+2) 	/* two hex digits - leech attempt count */
#define U_CURGRP	(U_LEECH+2) 	/* Current group */
#define U_CURSUB	(U_CURGRP+4)	/* Current sub-board */
#define U_CURLIB	(U_CURSUB+4)	/* Current library */
#define U_CURDIR	(U_CURLIB+4)	/* Current directory */
#define U_CMDSET	(U_CURDIR+4)	/* User's command set */
#define U_MAIN_CMD	(U_CMDSET+2+2)	/* Custom main command set */
#define U_XFER_CMD	(U_MAIN_CMD+LEN_MAIN_CMD)
#define U_SCAN_CMD	(U_XFER_CMD+LEN_XFER_CMD+2)
#define U_MAIL_CMD	(U_SCAN_CMD+LEN_SCAN_CMD)
#define U_FREECDT	(U_MAIL_CMD+LEN_MAIL_CMD+2)  /* Unused bytes */
#define U_FLAGS3	(U_FREECDT+10)	/* Unused bytes */
#define U_FLAGS4	(U_FLAGS3+8)
#define U_UNUSED1	(U_FLAGS4+8)
#define U_UNUSED2	(U_UNUSED1+22+2)
#define U_LEN       (U_UNUSED2+48+2)

enum {
	 PHONE_OKAY
	,PHONE_INVALID
	,PHONE_LD
	};

unsigned _stklen=16000; 		  /* Set stack size in code, not header */

uint asmrev,options=0;
char canfile[256],result[256]={NULL},tmp[256],
	 addfile[256],phone_number[81]={NULL},flags1[81],flags2[81],flags3[81],
	 flags4[81],exempt[81],restrict[81],expiration[81],credits[81],minutes[81],
	 level[81],bbs_ac[81];
char *crlf="\r\n";
extern uint riobp;
extern int mswtyp;
char io_int=0,validated=0;		/* i/o interrupts intercepted? yes/no */
int sysop=0;
int ldstart[7],ldend[7],min_phone_len,max_phone_len,hangup_time;

/***********************/
/* Function Prototypes */
/***********************/
int lprintf(char *fmat, ...);
void bail();
int main();
void phone_val();
char *get_user_pw();
char check_phone(char *insearch);
void mdmcmd(char *str);
void putcomch(char ch);
char getmdmstr(char *str, int sec);
void mswait(int);
void moduser();
void write_info();
char long_distance(char *number);

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(char *fmat, ...) {
	char sbuf[256];
	int chcount;

chcount=vsprintf(sbuf,fmat,_va_ptr);
lputs(sbuf);
return(chcount);
}

void bail()
{
	char str[256];
	int i;

if(io_int)
	ivhctl(0);
else if(!(options&STAY_CONNECTED)) { /* exiting without re-connecting */
	dtr(10);
	sprintf(str,"%sHANGUP.NOW",node_dir);
	if((i=nopen(str,O_CREAT|O_RDWR))!=-1)
		close(i); }
if(com_port) {
	for(i=0;i<5;i++)
		if(!rioctl(TXBC))		/* wait for rest of output */
			break;
		else
			mswait(1000);
	rioini(0,0); }
if(options&ALWAYS_VALIDATE || validated)
	moduser();
write_info();
lputc(FF);
}

int main()
{
	void *v;
	uchar far *s;
	char str[128],ch,*p;
	int i,file;
	uint base=0xffff;
	FILE *stream;

node_dir[0]=0;

p=getenv("SBBSNODE");
if(p)
	strcpy(node_dir,p);

if(!node_dir[0]) {	  /* node directory not specified */
	printf("\n\7SBBSNODE environment variable must be set.\n");
	printf("\nExample: SET SBBSNODE=C:\\SBBS\\NODE1\n");
	getch();
	return(1); }

if(node_dir[strlen(node_dir)-1]!='\\')  /* make sure node_dir ends in '\' */
	strcat(node_dir,"\\");

initdata();                                 /* read XTRN.DAT and more */

if((asmrev=*(&riobp-1))!=23) {
    printf("Wrong rciol.obj\n");
    exit(1); }

lclini(0xd<<8); 	 /* Tab expansion, no CRLF expansion */

if(com_port) {
	lprintf("\r\nInitializing COM port %u: ",com_port);
	switch(com_base) {
		case 0xb:
			lputs("PC BIOS");
			rioctl(I14PC);
			break;
		case 0xffff:
		case 0xd:
			lputs("DigiBoard");
			rioctl(I14DB);
			break;
		case 0xe:
			lputs("PS/2 BIOS");
			rioctl(I14PS);
			break;
		case 0xf:
			lputs("FOSSIL");
			rioctl(I14FO);
			break;
		case 0:
			base=com_port;
			lputs("UART I/O (BIOS), ");
			if(com_irq)
				lprintf("IRQ %d",com_irq);
			else lputs("default IRQ");
			break;
		default:
			base=com_base;
			lprintf("UART I/O %Xh, ",com_base);
			if(com_irq)
				lprintf("IRQ %d",com_irq);
			else lputs("default IRQ");
			break; }

	if(base==0xffff)
		lprintf(" channel %u",com_irq);
	i=rioini(base,com_irq);
	if(i) {
		lprintf(" - Failed! (%d)\r\n",i);
		exit(1); }
	if(mdm_misc&MDM_FLOWCTRL)
		rioctl(IOSM|CTSCK|RTSCK); /* set rts/cts chk */
	setbaud((uint)(com_rate&0xffffL));
	msr=&riobp-1; }

rioctl(TSTYPE|mswtyp);	 /* set time-slice API type */

rioctl(CPTON);          /* ctrl-p translation */

i=INT29L;
if(com_port)
	i|=(INT29R|INT16);
ivhctl(i);
io_int=1;

atexit(bail);

phone_val();

return(0);
}

/******************************************************************************
 Main phone validation loop.
******************************************************************************/
void phone_val()
{
	FILE *stream;
	char user_password[9],str[256],init_attempts=0,callout_attempts=0,*p;
	int i=0,j,file;

	if((file=nopen("SCB.CFG",O_RDONLY))==-1) {
		bprintf("ERROR: Opening configuration file\r\n"); exit(1); }
	if((stream=fdopen(file,"rb"))==NULL) {
		bprintf("ERROR: Converting configuration file to a stream\r\n");
		exit(1); }
	fgets(str,81,stream); truncsp(str); callout_attempts=atoi(str);
	fgets(str,81,stream); truncsp(str);
	if(str[0]=='Y') options|=ALWAYS_VALIDATE;
	if(str[1]=='Y') options|=MODIFY_USER_NOTE;
	if(str[2]=='Y') options|=START_WITH_0;
	if(str[3]=='Y') options|=START_WITH_1;
	if(str[4]=='Y') options|=STAY_CONNECTED;
	if(str[5]=='Y') options|=SC_LOCAL_ONLY;
	if(str[6]=='Y') options|=US_PHONE_FORMAT;
	if(str[7]=='Y') options|=ALLOWED_ONLY;
	if(str[8]!='N') options|=SAME_AREA_LD;
	fgets(canfile,81,stream); truncsp(canfile);
	fgets(addfile,81,stream); truncsp(addfile);
	fgets(credits,81,stream); truncsp(credits);
	fgets(str,81,stream); truncsp(str); sysop=atoi(str);
	fgets(level,81,stream); truncsp(level);
	fgets(flags1,81,stream); truncsp(flags1);
	fgets(flags2,81,stream); truncsp(flags2);
	fgets(exempt,81,stream); truncsp(exempt);
	fgets(restrict,81,stream); truncsp(restrict);
	fgets(expiration,81,stream); truncsp(expiration);
	fgets(minutes,81,stream); truncsp(minutes);
	fgets(flags3,81,stream); truncsp(flags3);
	fgets(flags4,81,stream); truncsp(flags4);
	for(i=0;i<7;i++) {
		fgets(str,81,stream); ldstart[i]=atoi(str);    /* min since midnight */
		fgets(str,81,stream); ldend[i]=atoi(str); }
	fgets(str,81,stream); min_phone_len=atoi(str);
	fgets(str,81,stream); max_phone_len=atoi(str);
	fgets(bbs_ac,81,stream); truncsp(bbs_ac);
	if(fgets(str,81,stream)) {
		hangup_time=atoi(str);
		if(hangup_time>90)
			hangup_time=90; }
	else
		hangup_time=30;

	fgets(str,81,stream);		// regnum

	fclose(stream);

	cls();
	strcpy(result,"Hung up");
	strcpy(user_password,get_user_pw());
	bprintf("\1c\1hSynchronet Callback v%s  "
		"Developed 1995-1997 Rob Swindell\r\n",VERSION);
	sprintf(str,"%s..\\EXEC\\",ctrl_dir);
	printfile("SCB.MSG");
	if(yesno("\r\nDo you need instructions"))
		printfile("INSTRUCT.MSG");
	if(!yesno("\r\nContinue with verification")) {
        options&=~ALWAYS_VALIDATE;
        cls(); printfile("REFUSED.MSG"); pause();
        strcpy(result,"Refused");
        return; }
/***
	if(!(options&ALLOW_LD) && yesno("Are you calling long distance")) {
		bprintf("\r\n\1n\1cSorry, \1h%s \1n\1cwill verify \1y\1hLOCAL "
			"\1n\1ccalls only!\r\n",sys_name);
			strcpy(result,"Long Dist"); pause();
		return; }
***/

	while(1) {
		while(1) {
			bprintf("\r\n\r\n\1n\1cEnter your \1h\1yCOMPLETE \1n\1cphone "
				"number now. If you are calling long distance, enter\r\n\1h\1y"
				"ALL \1n\1cof the digits necessary to reach your phone number. "
				"If you are a \1h\1yLOCAL \1n\1ccall from the BBS, \1h\1yDO NOT"
				"\1n\1c enter unnecessary digits (your area code, for example)."
				"\r\n:");
			getstr(phone_number,20,K_LINE|K_NUMBER);
			if(yesno("\r\nIs this correct")) break; }

		if(bbs_ac[0]) { 							/* Strip off area code */
			if(!strncmp(phone_number,bbs_ac,strlen(bbs_ac)))
				strcpy(phone_number,phone_number+strlen(bbs_ac));
			else {
				sprintf(tmp,"1%s",bbs_ac);
				if(!strncmp(phone_number,tmp,strlen(tmp)))
					strcpy(phone_number,phone_number+strlen(tmp)); } }

		if(options&US_PHONE_FORMAT &&				/* Add 1 to number */
			(phone_number[1]=='0' || phone_number[1]=='1') &&
			strlen(phone_number)>7) {
			sprintf(tmp,"1%s",phone_number);
			strcpy(phone_number,tmp); }

		j=check_phone(phone_number);

		if(j==PHONE_LD) {
			strcpy(result,"Long Dist");
			pause();
			return; }
		if(j==PHONE_OKAY)
			break;
		options&=~ALWAYS_VALIDATE;
		strcpy(result,"Invalid #");
		bprintf("\r\n\1n\1cReturning you to \1h%s.\1n\r\n",sys_name);
		return; }

	bprintf("\r\n\r\n\1n\1cDropping Carrier, \1h%s \1n\1cwill call you back "
		"now.\r\nType \1h\1yATA \1n\1cto answer when your modem rings!\r\n\r\n"
		,sys_name);
	if(!com_port) exit(0);
	mswait(1000);
	ivhctl(0);		/* put intercepted i/o vectors back */
	io_int=0;

	for(init_attempts=0;init_attempts<4;init_attempts++) {	 /* 4 attempts */
		dtr(5);
		if(!init_attempts) mswait(1000);
		dtr(1);
		rioctl(IOFB);
		for(i=0;i<4;i++) {
			mdmcmd(mdm_init);
			if(!getmdmstr(str,10))
				continue;
			if(!stricmp(str,mdm_init)) {	/* Echo on? */
				getmdmstr(str,10);			/* Get OK */
				mdmcmd("ATE0");             /* Turn echo off */
				if(!getmdmstr(str,10))
                    continue; }
			if(!strcmp(str,"OK")) {         /* Verbal response? */
				mdmcmd("ATV0");             /* Turn verbal off */
				if(!getmdmstr(str,10))
					continue; }
			if(!strcmp(str,"0"))
				break;
			rioctl(IOFB);		/* Send fool-proof init string */
			mdmcmd("ATE0V0");
			if(getmdmstr(str,10) && !strcmp(str,"0"))
				break; }
		if(i==4)
			continue;
		mswait(100);
		if(mdm_spec[0]) {
			for(i=0;i<4;i++) {
				mdmcmd(mdm_spec);
				if(!getmdmstr(str,10)) continue;
				if(!strcmp(str,"0")) break; }
			if(i==4)
				continue; }
		break; }

	if(init_attempts==4) {	 /* couldn't init */
		strcpy(result,"No Init");
		exit(1); }

	mswait(1000);
	mdmcmd("ATH");
	str[0]=0;
	getmdmstr(str,10);
	if(strcmp(str,"0")) {
		strcpy(result,"No Init");
		exit(1); }
	if(hangup_time)
		bprintf("\r\n\1n\1cWaiting \1h%u\1n\1c seconds before dialing..."
			"\r\n\r\n",hangup_time);
	mswait(hangup_time*1000);			/* Wait xx seconds before dialing */

	for(i=0;i<callout_attempts;i++) {
		mswait(5000);
		rioctl(IOFB);
		sprintf(str,"%s%s",mdm_dial,phone_number);
		mdmcmd(str);
		if(!getmdmstr(str,60)) {
			mdmcmd("");         /* send CR to abort dial */
			continue; }
		if(strcmp(str,"0") && strcmp(str,"2") && strcmp(str,"3")
			&& strcmp(str,"4") && strcmp(str,"6") && strcmp(str,"7")
			&& strcmp(str,"8") && strcmp(str,"9"))
		   break; }

	if(i==callout_attempts) {			/* Couldn't connect */
		strcpy(result,"No Connect");
		exit(1); }

	i=INT29L;		/* intercept i/o vectors again */
	if(com_port)
		i|=(INT29R|INT16);
	ivhctl(i);
	io_int=1;

	if(rioctl(IOSTATE|DCD)) {
		mswait(5000);	/* wait 5 seconds for MNP to Non-MNP connect */
		rioctl(IOFB);
		lncntr=0;
		i=0; cls();
		bprintf("\r\n\r\n\1n\1cThis is \1h%s \1n\1ccalling for \1m%s.",
			sys_name,user_name);
		while(1) {
			bprintf("\r\n\r\n\1h\1yEnter your password for verification: ");
			getstr(str,8,K_UPPER|K_LINE);
			if(!stricmp(str,user_password)) {
				validated=1;
				bprintf("\r\n\r\n\1n\1cYou have now been verified on \1h%s!",
					sys_name);
				printfile("VERIFIED.MSG");
				strcpy(result,"Verified");
				break; }
			else {
				bprintf("\r\n\1r\1hINCORRECT!\1n");
				if(++i>=4) {
					bprintf("\r\n\r\n\1r\1hYou have entered an incorrect "
						"password.  Goodbye.\r\n\r\n"); dtr(5);
						strcpy(result,"Bad Pass"); return; } } }

		if(!(options&STAY_CONNECTED)) {
			mswait(2000);
			for(i=0;i<5;i++) {
				dtr(5);
				if(!(rioctl(IOSTATE)&DCD))	/* no carrier detect */
					break;
				lprintf("SCB: Dropping DTR failed to lower DCD.\r\n");
				dtr(1);
				mswait(2000); }
			sprintf(str,"%sHANGUP.NOW",node_dir);
			if((i=nopen(str,O_CREAT|O_RDWR))!=-1)
				close(i); } }
}

/******************************************************************************
 Writes the MODUSER.DAT file.
******************************************************************************/
void moduser()
{
	FILE *stream;
	char str[512];
	int file;
	long expire;
	time_t now;

	now=time(NULL);

	if(user_expire>now)
		expire=(user_expire+(atol(expiration)*24L*60L*60L));
	else
		expire=(now+(atol(expiration)*24L*60L*60L));

	sprintf(str,"%sMODUSER.DAT",node_dir);
	if((stream=fopen(str,"wb"))!=NULL) {
		fprintf(stream,"%s\r\n",credits);
		fprintf(stream,"%s\r\n",level);
		fprintf(stream,"\r\n");
		fprintf(stream,"%s\r\n",flags1);
		fprintf(stream,"%s\r\n",flags2);
		fprintf(stream,"%s\r\n",exempt);
		fprintf(stream,"%s\r\n",restrict);
		fprintf(stream,"%s\r\n",atol(expiration) ? ultoa(expire,str,16) : "");
		fprintf(stream,"%s\r\n",minutes);
		fprintf(stream,"%s\r\n",flags3);
		fprintf(stream,"%s\r\n",flags4);
		fclose(stream);
		}
	else
        bprintf("\7\r\nError opening %s for write.\r\n",str);
	if(addfile[0] && phone_number[0]) {
        if((file=nopen(addfile,O_WRONLY|O_APPEND|O_CREAT))!=-1) {
            sprintf(str,"%s^\r\n",phone_number);
            write(file,str,strlen(str));
            close(file); }
        else
            bprintf("\7\r\nError opening %s for write.\r\n",addfile); }
	if(sysop) {
		sprintf(str,"\1c\1hSCB: \1y%s \1n\1cwas validated \1h%.24s\1n\r\n"
			,user_name,ctime(&now));
		putsmsg(sysop,str); }
}

/******************************************************************************
 Writes the log file.
******************************************************************************/
void write_info()
{
	char str[512];
	int file;
	time_t now;

	now=time(NULL);
	if((file=nopen("SCB.LOG",O_WRONLY|O_APPEND|O_CREAT))!=-1) {
		sprintf(str,
			"Node %-3d     : %.24s\r\n"
			"User Name    : %s\r\n"
			"Voice Number : %s\r\n"
			"Modem Number : %s\r\n"
			"Result       : %s\r\n\r\n"
			,node_num,ctime(&now),user_name,user_phone,phone_number,result);
		write(file,str,strlen(str));
		close(file); }
	else
		bprintf("\7\r\nError opening SCB.LOG for write.\r\n");
	sprintf(str,"%sNODE.LOG",node_dir);
	if(com_port && (file=nopen(str,O_WRONLY|O_APPEND|O_CREAT))!=-1) {
		sprintf(str,"cb Result: %s %s\r\n",result,phone_number);
		write(file,str,strlen(str));
		close(file); }
	if(options&MODIFY_USER_NOTE) {
        sprintf(str,"%sUSER\\USER.DAT",data_dir);
		if((file=nopen(str,O_WRONLY|O_DENYNONE))!=-1) {
			lseek(file,(long)((((long)user_number-1L)*U_LEN)+U_NOTE),SEEK_SET);
            memset(str,'\3',30);
			sprintf(str,"%s %s",result,phone_number);
			str[strlen(str)]=3;
			write(file,str,30);
            close(file); } }
}

/******************************************************************************
 Returns the users' password.
******************************************************************************/
char *get_user_pw()
{
	static char pw[9];
	char str[256];
	int file,x;

	sprintf(str,"%sUSER\\USER.DAT",data_dir);
	if((file=nopen(str,O_RDONLY))==-1) {
		printf("Unable to open %s.",str); exit(1); }
	lseek(file,(long)((((long)user_number-1L)*U_LEN)+U_PASS),SEEK_SET);
	read(file,pw,8);
	for(x=0;x<8;x++)
		if(pw[x]==3) break;
	pw[x]=0;
	close(file);
	return(pw);
}

/******************************************************************************
 Checks the phone number entered.  0=Good, 1=Bad.
******************************************************************************/
char check_phone(char *insearch)
{
	char str[256],search[256],c,found=0,allowed=0,long_distance=0;
	int file,day,t;
	FILE *stream;
	time_t now;
	struct date date;
	struct time curtime;
	struct tm *tblock;

if(strlen(insearch)<min_phone_len) {
	printfile("TOOSHORT.MSG");
	return(PHONE_INVALID); }
if(strlen(insearch)>max_phone_len) {
	printfile("TOOLONG.MSG");
    return(PHONE_INVALID); }

strcpy(str,"ALLOWED.DAT");
if((file=nopen(str,O_RDONLY|O_TEXT))!=-1)
	if((stream=fdopen(file,"rt"))!=NULL) {
		strcpy(search,insearch);
		strupr(search);
		while(!feof(stream) && !ferror(stream)) {
			if(!fgets(str,81,stream))
				break;
			truncsp(str);
			c=strlen(str);
			if(c && !strncmp(str,search,c)) {
				allowed=1;
				break; } }
	fclose(stream); }

if(!allowed) {

	if(options&ALLOWED_ONLY)
		long_distance=1;
	else {
		strcpy(str,"LDPREFIX.DAT");
		if((file=nopen(str,O_RDONLY|O_TEXT))!=-1)
			if((stream=fdopen(file,"rt"))!=NULL) {
				strcpy(search,insearch);
				strupr(search);
				while(!feof(stream) && !ferror(stream)) {
					if(!fgets(str,81,stream))
						break;
					truncsp(str);
					c=strlen(str);
					if(c && !strncmp(str,search,c)) {
						long_distance=1;
						break; } }
			fclose(stream); } }

	if(!(options&START_WITH_0) && insearch[0]=='0') {
		printfile("NO_ZERO.MSG");
		return(PHONE_LD); }
	if(!(options&START_WITH_1) && insearch[0]=='1') {
		printfile("NO_ONE.MSG");
		return(PHONE_LD); }
	if(!(options&SAME_AREA_LD) && insearch[0]!='0' && insearch[0]!='1'
		&& long_distance) {
		printfile("NO_LD.MSG");
		return(PHONE_LD); }
	if((insearch[0]=='1' || insearch[0]=='0' || long_distance) &&
		(ldstart[day] || ldend[day])) {
		now=time(NULL);
		tblock=localtime(&now);
		day=tblock->tm_wday;
		unixtodos(now,&date,&curtime);
		t=(curtime.ti_hour*60)+curtime.ti_min;

		if((ldstart[day]<ldend[day]
			&& (t<ldstart[day] || t>ldend[day]))

		|| (ldstart[day]>ldend[day]
			&& (t<ldstart[day] && t>ldend[day]))

			) {

			bprintf("\7\r\n\1n\1gLong distance calls are only allowed between "
				"\1h%02d:%02d \1n\1gand \1h%02d:%02d\1n\1g.\r\n"
				,ldstart[day]/60,ldstart[day]%60
				,ldend[day]/60,ldend[day]%60);
			printfile("LD_TIME.MSG");
			return(PHONE_LD); } } }

strcpy(str,canfile);
if((file=nopen(str,O_RDONLY|O_TEXT))==-1) {
	if(fexist(str))
		printf("ERROR: Unable to open %s for read.",str);
	return(PHONE_OKAY); }
if((stream=fdopen(file,"rt"))==NULL) {
	printf("ERROR: Converting %s to a stream.",str);
	return(PHONE_OKAY); }
strcpy(search,insearch);
strupr(search);
while(!feof(stream) && !ferror(stream) && !found) {
	if(!fgets(str,81,stream))
		break;
	truncsp(str);
	c=strlen(str);
	if(c) {
		c--;
		strupr(str);
		if(str[c]=='~') {
			str[c]=0;
			if(strstr(search,str))
				found=1; }

		else if(str[c]=='^') {
			str[c]=0;
			if(!strncmp(str,search,c))
				found=1; }

		else if(!strcmp(str,search))
			found=1; } }
fclose(stream);
if(found) {
	printfile("PHONECAN.MSG");
	return(PHONE_INVALID); }
if(!allowed && options&STAY_CONNECTED && options&SC_LOCAL_ONLY &&
	(insearch[0]=='1' || insearch[0]=='0'))
    options&=~STAY_CONNECTED;
return(PHONE_OKAY);
}

/****************************************************************************/
/* Outputs a single character to the COM port								*/
/****************************************************************************/
void putcomch(char ch)
{
	char 	lch;
	int 	i=0;

if(!com_port)
	return;
while(outcom(ch)&TXBOF && i<1440) {	/* 3 minute pause delay */
    if(lkbrd(1)) {
		lch=lkbrd(0);	/* ctrl-c */
		if(lch==3) {
			lputs("local abort (putcomch)\r\n");
			i=1440;
			break; }
		ungetkey(lch); }
    if(!(rioctl(IOSTATE)&DCD))
		break;
	i++;
	mswait(80); }
if(i==1440) {						/* timeout - beep flush outbuf */
	i=rioctl(TXBC);
	lprintf("timeout(putcomch) %04X %04X\r\n",i,rioctl(IOFO));
	outcom(7);
	lputc(7);
	rioctl(IOCS|PAUSE); }
}

/****************************************************************************/
/* Sends string of characters to COM port. Interprets ^M and ~ (pause)		*/
/* Called from functions waitforcall and offhook							*/
/****************************************************************************/
void mdmcmd(char *str)
{
	int 	i=0;
	uint	lch;

lputs("\r\nModem command  : ");
while(str[i]) {
	if(str[i]=='~')
		mswait(500);
	else {
		if(i && str[i-1]=='^' && str[i]!='^')  /* control character */
			putcomch(toupper(str[i])-64);
		else if(str[i]!='^' || (i && str[i-1]=='^'))
			putcomch(str[i]);
		lputc(str[i]); }
	i++; }
putcomch(CR);
i=0;
while(rioctl(TXSYNC) && i<10) {   /* wait for modem to receive all chars */
	if(lkbrd(1)) {
		lch=lkbrd(0);	/* ctrl-c */
		if(lch==0x2e03) {
			lputs("local abort (mdmcmd)\r\n");
			break; }
		if(lch==0xff00)
			bail(1);
		ungetkey(lch); }
	i++; }
if(i==10) {
	i=rioctl(TXBC);
	lprintf("\r\n\7timeout(mdmcmd) %04X %04X\r\n",i,rioctl(IOFO)); }
lputs(crlf);
}
/****************************************************************************/
/* Returns CR terminated string from the COM port. 							*/
/* Called from function waitforcall 										*/
/****************************************************************************/
char getmdmstr(char *str, int sec)
{
	uchar	j=0,ch;
	uint	lch;
	time_t	start;

lputs("Modem response : ");
start=time(NULL);
while(time(NULL)-start<sec && j<81) {
	if(lkbrd(1)) {
		lch=lkbrd(0);	/* ctrl-c */
		if(lch==0x2e03) {
			lputs("local abort (getmdmstr)\r\n");
			break; }
		if(lch==0xff00)
			bail(1);
		ungetkey(lch); }
	if((ch=incom())==CR && j)
		break;
	if(ch && (ch<0x20 || ch>0x7f)) /* ignore control characters and ex-ascii */
		continue;
	if(ch) {
		str[j++]=ch;
		lputc(ch); }
	else mswait(0); }
mswait(500);
str[j]=0;
lputs(crlf);
return(j);
}
