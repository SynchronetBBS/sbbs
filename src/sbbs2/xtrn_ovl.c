#line 1 "XTRN_OVL.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

char *hungupstr="\1n\1h%s\1n hung up on \1h%s\1n %s\r\n";

#ifndef __FLAT__
extern uint riobp;
#endif

extern mswtyp;
extern uint fakeriobp;

/****************************************************************************/
/* Convert C string to pascal string										*/
/****************************************************************************/
void str2pas(char *instr, char *outstr)
{
	int i;

outstr[0]=strlen(instr);
for(i=0;i<outstr[0];i++)
	outstr[i+1]=instr[i];
}

/****************************************************************************/
/* Convert from unix time (seconds since 1/70) to julian (days since 1900)	*/
/****************************************************************************/
int unixtojulian(time_t unix)
{
	int days[12]={0,31,59,90,120,151,181,212,243,273,304,334};
	long j;
	struct date d;
	struct time t;

unixtodos(unix,&d,&t);
j=36525L*d.da_year;
if(!(j%100) && d.da_mon<3)
	j--;
j=(j-(1900*36525))/100;
j+=d.da_day+days[d.da_mon-1];
return(j);
}

/****************************************************************************/
/* Convert julian date into unix format 									*/
/****************************************************************************/
time_t juliantounix(ulong j)
{
    int days[2][12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
                       0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
    long temp;
	int leap,counter;

	if(!j) return(0L);

	date.da_year=(100L*j)/36525L;
	temp=(long)date.da_year*36525L;
	date.da_year+=1900;
	j-=temp/100L;

	if (!(temp%100)) {
		j++;
		leap=1; }
	else leap=0;

	for(date.da_mon=counter=0;counter<12;counter++)
		if(days[leap][counter]<j)
			date.da_mon=counter;

	date.da_day=j-days[leap][date.da_mon];
	date.da_mon++;	/* go from 0 to 1 based */

	curtime.ti_hour=curtime.ti_min=curtime.ti_sec=0;
	return(dostounix(&date,&curtime));
}

/****************************************************************************/
/* Creates various types of xtrn (Doors, Chains, etc.) data files.		 */
/****************************************************************************/
void xtrndat(char *name, char *dropdir, uchar type, ulong tleft)
{
	char	str[1024],tmp2[128],c,*p;
	int		i,file;
	long	l;
	ushort	w;
	FILE *	stream;
	struct	time lastcall;
	stats_t stats;

if(type==XTRN_SBBS) {	/* SBBS XTRN.DAT file */
	sprintf(str,"%sXTRN.DAT",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return; }

	sprintf(str,"%s\r\n%s\r\n%s\r\n%s\r\n"
		,name								/* User name */
		,sys_name							/* System name */
		,sys_op 							/* Sysop name */
		,sys_guru); 						/* Guru name */
	write(file,str,strlen(str));

	sprintf(str,"%s\r\n%s\r\n%u\r\n%u\r\n%lu\r\n%s\r\n%lu\r\n%lu\r\n"
		,ctrl_dir							/* Ctrl dir */
		,data_dir							/* Data dir */
		,sys_nodes							/* Total system nodes */
		,node_num							/* Current node */
		,tleft								/* User Timeleft in seconds */
		,useron.misc&ANSI					/* User ANSI ? (Yes/Mono/No) */
			? useron.misc&COLOR
			? "Yes":"Mono":"No"
		,rows								/* User Screen lines */
		,useron.cdt+useron.freecdt);		/* User Credits */
	write(file,str,strlen(str));

	sprintf(str,"%u\r\n%u\r\n%s\r\n%c\r\n%u\r\n%s\r\n"
		,useron.level						/* User main level */
		,useron.level						/* User transfer level */
		,useron.birth						/* User birthday */
		,useron.sex 						/* User sex (M/F) */
		,useron.number						/* User number */
		,useron.phone); 					/* User phone number */
	write(file,str,strlen(str));

	sprintf(str,"%u\r\n%u\r\n%x\r\n%lu\r\n%s\r\n%s\r\n"
		"%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%lu\r\n"
		,online==ON_LOCAL ? 0:com_port		/* Com port or 0 if local */
		,com_irq							/* Com IRQ */
		,com_base							/* Com base in hex */
		,dte_rate							/* Com rate */
		,mdm_misc&MDM_CTS	   ? "Yes":"No" /* Hardware flow ctrl Y/N */
		,mdm_misc&MDM_STAYHIGH ? "Yes":"No" /* Locked DTE rate Y/N */
		,mdm_init							/* Modem initialization string */
		,mdm_spec							/* Modem special init string */
		,mdm_term							/* Modem terminal mode init str */
		,mdm_dial							/* Modem dial string */
		,mdm_offh							/* Modem off-hook string */
		,mdm_answ							/* Modem answer string */
#ifndef __FLAT__
		,sys_status&SS_DCDHIGH ? &fakeriobp : &riobp-1							 /* Modem status register */
#else
		,0
#endif
		);
	write(file,str,strlen(str));

	sprintf(str,"%u\r\n",total_xtrns);
	write(file,str,strlen(str));			/* Total external programs */

	for(i=0;i<total_xtrns;i++) {			/* Each program's name */
		if(SYSOP || chk_ar(xtrn[i]->ar,useron))
			strcpy(str,xtrn[i]->name);
		else
			str[0]=0;						/* Blank if no access */
		strcat(str,crlf);
		write(file,str,strlen(str)); }

	sprintf(str,"%s\r\n%s\r\n"
		,ltoaf(useron.flags1,tmp)			 /* Main flags */
		,ltoaf(useron.flags2,tmp2)			 /* Transfer flags */
		);
	write(file,str,strlen(str));

	sprintf(str,"%s\r\n%s\r\n%lx\r\n%s\r\n%s\r\n%s\r\n"
		,ltoaf(useron.exempt,tmp)			/* Exemptions */
		,ltoaf(useron.rest,tmp2)			/* Restrictions */
		,useron.expire						/* Expiration date in unix form */
		,useron.address 					/* Address */
		,useron.location					/* City/State */
		,useron.zipcode 					/* Zip/Postal code */
		);
	write(file,str,strlen(str));

	sprintf(str,"%s\r\n%s\r\n%d\r\n%s\r\n%lu\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
		"%lx\r\n%d\r\n"
		,ltoaf(useron.flags3,tmp)			/* Flag set #3 */
		,ltoaf(useron.flags4,tmp2)			/* Flag set #4 */
		,mswtyp 							/* Time-slice type */
		,useron.name						/* Real name/company */
		,cur_rate							/* DCE rate */
		,exec_dir
		,text_dir
		,temp_dir
		,sys_id
		,node_misc
#ifdef __FLAT__
		,rio_handle
#else
		,-1
#endif
		);
    write(file,str,strlen(str));

	close(file); }

else if(type==XTRN_WWIV) {	/*	WWIV CHAIN.TXT File */
	sprintf(str,"%sCHAIN.TXT",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return; }

	sprintf(str,"%u\r\n%s\r\n%s\r\n%s\r\n%u\r\n%c\r\n"
		,useron.number						/* User number */
		,name								/* User name */
		,useron.name						/* User real name */
		,nulstr 							/* User call sign */
		,getage(useron.birth)				/* User age */
		,useron.sex);						/* User sex (M/F) */
	strupr(str);
	write(file,str,strlen(str));

	sprintf(str,"%lu\r\n%s\r\n%u\r\n%lu\r\n%u\r\n%u\r\n%u\r\n%u\r\n%u\r\n"
		,useron.cdt+useron.freecdt			/* Gold */
		,unixtodstr(useron.laston,tmp)		/* User last on date */
		,80 								/* User screen width */
		,rows								/* User screen length */
		,useron.level						/* User SL */
		,0									/* Cosysop? */
		,SYSOP								/* Sysop? (1/0) */
		,(useron.misc&ANSI) ? 1:0			/* ANSI ? (1/0) */
		,online==ON_REMOTE);				/* Remote (1/0) */
	write(file,str,strlen(str));

	sprintf(str,"%lu\r\n%s\r\n%s\r\n%s\r\n%lu\r\n%d\r\n%s\r\n%s\r\n"
		"%u\r\n%u\r\n%lu\r\n%u\r\n%lu\r\n%u\r\n%s\r\n"
		,tleft								/* Time left in seconds */
		,node_dir							/* Gfiles dir (log dir) */
		,data_dir							/* Data dir */
		,"NODE.LOG"                         /* Name of log file */
		,dte_rate							/* DTE rate */
		,com_port							/* COM port number */
		,sys_name							/* System name */
		,sys_op 							/* Sysop name */
		,0									/* Logon time (sec past 12am) */
		,0									/* Current time (sec past 12am) */
		,useron.ulb/1024UL					/* User uploaded kbytes */
		,useron.uls 						/* User uploaded files */
		,useron.dlb/1024UL					/* User downloaded kbytes */
		,useron.dls 						/* User downloaded files */
		,"8N1");                            /* Data, parity, stop bits */
	write(file,str,strlen(str));

	close(file); }

else if(type==XTRN_GAP) {	/* Gap DOOR.SYS File */
	sprintf(str,"%sDOOR.SYS",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return; }

	sprintf(str,"COM%d:\r\n%lu\r\n%u\r\n%u\r\n%lu\r\n%c\r\n%c\r\n%c\r\n%c\r\n"
		,online==ON_REMOTE ? com_port:0 	/* 01: COM port - 0 if Local */
		,cur_rate							/* 02: DCE rate */
		,8									/* 03: Data bits */
		,node_num							/* 04: Node number */
		,dte_rate							/* 05: DTE rate */
		,console&CON_L_ECHO ? 'Y':'N'       /* 06: Screen display */
		,'Y'                                /* 07: Printer toggle */
		,'Y'                                /* 08: Page bell */
		,'Y');                              /* 09: Caller alarm */
	write(file,str,strlen(str));

	sprintf(str,"%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
		,name								/* 10: User name */
		,useron.location					/* 11: User location */
		,useron.phone						/* 12: User home phone */
		,useron.phone						/* 13: User work/data phone */
		,useron.pass);						/* 14: User password */
	write(file,str,strlen(str));

	sprintf(str,"%u\r\n%u\r\n%s\r\n%lu\r\n%lu\r\n%s\r\n"
		,useron.level						/* 15: User security level */
		,useron.logons						/* 16: User total logons */
		,unixtodstr(useron.laston,tmp)		/* 17: User last on date */
		,tleft								/* 18: User time left in sec */
		,tleft/60							/* 19: User time left in min */
		,useron.misc&NO_EXASCII 			/* 20: GR if COLOR ANSI */
			? "7E" : (useron.misc&(ANSI|COLOR))==(ANSI|COLOR) ? "GR" : "NG");
	write(file,str,strlen(str));

	sprintf(str,"%lu\r\n%c\r\n%s\r\n%u\r\n%s\r\n%u\r\n%c\r\n%u\r\n%u\r\n"
		,rows								/* 21: User screen length */
		,useron.misc&EXPERT ? 'Y':'N'       /* 22: Expert? (Y/N) */
		,ltoaf(useron.flags1,tmp2)			/* 23: Registered conferences */
		,0									/* 24: Conference came from */
		,unixtodstr(useron.expire,tmp)		/* 25: User expiration date */
		,useron.number						/* 26: User number */
		,'Y'                                /* 27: Default protocol */
		,useron.uls 						/* 28: User total uploads */
		,useron.dls);						/* 29: User total downloads */
	write(file,str,strlen(str));

	sprintf(str,"%u\r\n%lu\r\n%s\r\n%s\r\n%s\r\n%s"
		"\r\n%s\r\n%02d:%02d\r\n%c\r\n"
		,0									/* 30: Kbytes downloaded today */
		,(useron.cdt+useron.freecdt)/1024UL /* 31: Max Kbytes to download today */
		,useron.birth						/* 32: User birthday */
		,node_dir							/* 33: Path to MAIN directory */
		,data_dir							/* 34: Path to GEN directory */
		,sys_op 							/* 35: Sysop name */
		,nulstr 							/* 36: Alias name */
		,0 // sys_eventtime/60				/* 37: Event time HH:MM */
		,0 // sys_eventtime%60
		,'Y');                              /* 38: Error correcting connection */
	write(file,str,strlen(str));

	unixtodos(ns_time,&date,&curtime);
	sprintf(str,"%c\r\n%c\r\n%u\r\n%lu\r\n%02d/%02d/%02d\r\n"
		,(useron.misc&(NO_EXASCII|ANSI|COLOR))==ANSI
			? 'Y':'N'                       /* 39: ANSI supported but NG mode */
		,'Y'                                /* 40: Use record locking */
		,14 								/* 41: BBS default color */
		,useron.min 						/* 42: Time credits in minutes */
		,date.da_mon						/* 43: File new-scan date */
		,date.da_day
		,TM_YEAR(date.da_year-1900));
	write(file,str,strlen(str));

	unixtodos(logontime,&date,&curtime);
	unixtodos(useron.laston,&date,&lastcall);
	sprintf(str,"%02d:%02d\r\n%02d:%02d\r\n%u\r\n%u\r\n%lu\r\n"
		"%lu\r\n%s\r\n%u\r\n%u\r\n"
		,curtime.ti_hour					/* 44: Time of this call */
		,curtime.ti_min
		,lastcall.ti_hour					/* 45: Time of last call */
		,lastcall.ti_min
		,999								/* 46: Max daily files available */
		,0									/* 47: Files downloaded so far today */
		,useron.ulb/1024UL					/* 48: Total Kbytes uploaded */
		,useron.dlb/1024UL					/* 49: Total Kbytes downloaded */
		,useron.comment 					/* 50: User comment */
		,0									/* 51: Total doors opened */
		,useron.posts); 					/* 52: User message left */
	write(file,str,strlen(str));

	close(file); }

else if(type==XTRN_RBBS || type==XTRN_RBBS1) {
	if(type==XTRN_RBBS)
		sprintf(str,"%sDORINFO%X.DEF",dropdir,node_num);   /* support 1-F */
	else
		sprintf(str,"%sDORINFO1.DEF",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
        return; }

	strcpy(tmp,sys_op);
	p=strchr(tmp,SP);
	if(p)
		*(p++)=0;
	else
        p=nulstr;

	sprintf(str,"%s\r\n%s\r\n%s\r\nCOM%d\r\n%lu BAUD,N,8,1\r\n%u\r\n"
		,sys_name							/* Name of BBS */
		,tmp								/* Sysop's firstname */
		,p									/* Sysop's lastname */
		,online==ON_REMOTE ? com_port:0 	/* COM port number, 0 if local */
		,dte_rate							/* DTE rate */
		,0);								/* Network type */
	strupr(str);
	write(file,str,strlen(str));

	strcpy(tmp,name);
	p=strchr(tmp,SP);
	if(p)
		*(p++)=0;
	else
		p=nulstr;
	sprintf(str,"%s\r\n%s\r\n%s\r\n%u\r\n%u\r\n%lu\r\n"
		,tmp								/* User's firstname */
		,p									/* User's lastname */
		,useron.location					/* User's city */
		,(useron.misc&ANSI)==ANSI			/* 1=ANSI 0=ASCII */
		,useron.level						/* Security level */
		,tleft/60); 						/* Time left in minutes */
	strupr(str);
	write(file,str,strlen(str));

	close(file);

	sprintf(str,"%sEXITINFO.BBS",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
        return; }
	w=dte_rate;
	write(file,&w,sizeof(short));	  /* BaudRate */
	/* SysInfo */
	getstats(0,&stats);
	write(file,&stats.logons,sizeof(long)); /* CallCount */
	write(file,nulstr,36);					/* LastCallerName */
	write(file,nulstr,36);					/* LastCallerAlias */
	write(file,nulstr,92);					/* ExtraSpace */
	/* TimeLogInfo */
	write(file,nulstr,9);					/* StartDate */
	write(file,nulstr,24*sizeof(short));		/* BusyPerHour */
	write(file,nulstr,7*sizeof(short));		/* BusyPerDay */
	/* UserInfo */
	str2pas(name,str);				/* Name */
	write(file,str,36);
	str2pas(useron.location,str);
	write(file,str,26); 					/* City */
	str2pas(useron.pass,str);
	write(file,str,16); 					/* Pwd */
	str2pas(useron.phone,str);
	write(file,str,13); 					/* DataPhone */
	write(file,str,13); 					/* HomePhone */
	unixtodos(useron.laston,&date,&lastcall);
	sprintf(tmp,"%02d:%02d",lastcall.ti_hour,lastcall.ti_min);
	str2pas(tmp,str);
	write(file,str,6);						/* LastTime */
	unixtodstr(useron.laston,tmp);
	str2pas(tmp,str);
	write(file,str,9);						/* LastDate */
	c=0;
	if(useron.misc&DELETED) c|=(1<<0);
	if(useron.misc&CLRSCRN) c|=(1<<1);
	if(useron.misc&UPAUSE)	 c|=(1<<2);
	if(useron.misc&ANSI)	c|=(1<<3);
	if(useron.sex=='F')     c|=(1<<7);
	write(file,&c,1);						/* Attrib */
	write(file,&useron.flags1,4);			/* Flags */
	i=0;
	w=0;
	write(file,&w,sizeof(short)); 			/* Credit */
	write(file,&w,sizeof(short)); 			/* Pending */
	write(file,&useron.posts,sizeof(short));	/* TimesPosted */
	write(file,&w,sizeof(short)); 			/* HighMsgRead */
	w=useron.level;
	write(file,&w,sizeof(short)); 			/* SecLvl */
	w=0;
	write(file,&w,sizeof(short)); 			/* Times */
	write(file,&useron.uls,sizeof(short));	/* Ups */
	write(file,&useron.dls,sizeof(short));	/* Downs */
	w=useron.ulb/1024UL;
	write(file,&w,sizeof(short)); 			/* UpK */
	w=useron.dlb/1024UL;
	write(file,&w,sizeof(short)); 			/* DownK */
	w=logon_dlb/1024UL;
	write(file,&w,sizeof(short)); 			/* TodayK */
	w=0;
	write(file,&w,sizeof(short)); 			/* Elapsed */
	write(file,&w,sizeof(short)); 			/* Len */
	write(file,&w,sizeof(short)); 			/* CombinedPtr */
	write(file,&w,sizeof(short)); 			/* AliasPtr */
	l=0;
	write(file,&l,sizeof(long));			/* Birthday (as a long?) */
	/* EventInfo */
	c=0;
	write(file,&c,sizeof(char));			/* Status */
	write(file,&l /* sys_eventtime */,sizeof(long));	/* RunTime */
	write(file,&c,sizeof(char));			/* ErrorLevel */
	c=0xff;
	write(file,&c,sizeof(char));			/* Days */
	// c=sys_eventnode==node_num || sys_misc&SM_TIMED_EX ? 1 : 0;
	c=0;
	write(file,&c,sizeof(char));			/* Forced */
	if(!total_events)
		l=0;
	else
		l=event[0]->last;
	write(file,&l,sizeof(long));			/* LastTimeRun */
	memset(str,0,40);
	write(file,str,7);						/* Spare */

	c=0;
	write(file,&c,1);						/* NetMailEntered */
	write(file,&c,1);						/* EchoMailEntered */

	unixtodos(logontime,&date,&curtime);
	sprintf(tmp,"%02d:%02d",curtime.ti_hour,curtime.ti_min);
	str2pas(tmp,str);
	write(file,str,6);						/* LoginTime */
	unixtodstr(logontime,tmp);
	str2pas(tmp,str);
	write(file,str,9);						/* LoginDate */
	write(file,&level_timepercall[useron.level],sizeof(short));  /* TmLimit */
	write(file,&logontime,sizeof(long));	/* LoginSec */
	write(file,&useron.cdt,sizeof(long));	/* Credit */
	write(file,&useron.number,sizeof(short)); /* UserRecNum */
	write(file,&i,2);						/* ReadThru */
	write(file,&i,2);						/* PageTimes */
	write(file,&i,2);						/* DownLimit */
	c=sys_status&SS_SYSPAGE ? 1:0;
	write(file,&c,1);						/* WantChat */
	c=0;
	write(file,&c,1);						/* GosubLevel */

	memset(str,0,255);
	for(i=1;i<20;i++)
		write(file,str,9);					/* GosubData */
	write(file,str,9);						/* Menu */
	c=useron.misc&CLRSCRN ? 1:0;
	write(file,&c,1);						/* ScreenClear */
	c=useron.misc&UPAUSE ? 1:0;
	write(file,&c,1);						/* MorePrompts */
	c=useron.misc&NO_EXASCII ? 0:1;
	write(file,&c,1);						/* GraphicsMode */
	c=useron.xedit ? 1:0;
	write(file,&c,1);						/* ExternEdit */
	i=rows;
	write(file,&i,2);						/* ScreenLength */
	c=1;
	write(file,&c,1);						/* MNP_Connect */
	write(file,str,49); 					/* ChatReason */
	c=0;
	write(file,&c,1);						/* ExternLogoff */
	c=useron.misc&ANSI ? 1:0;
	write(file,&c,1);						/* ANSI_Capable */
	close(file);
	}

else if(type==XTRN_WILDCAT) { /* WildCat CALLINFO.BBS File */
	sprintf(str,"%sCALLINFO.BBS",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
        return; }

	if(online==ON_LOCAL) i=5;
	else
		switch(dte_rate) {
			case 300:
				i=1;
				break;
			case 1200:
				i=2;
				break;
			case 2400:
				i=0;
				break;
			case 9600:
				i=3;
				break;
			case 19200:
				i=4;
				break;
			case 38400:
				i=6;
				break;
			default:
				i=7;
				break; }
	sprintf(str,"%s\r\n%u\r\n%s\r\n%u\r\n%lu\r\n%s\r\n%s\r\n%u\r\n"
		,name								/* User name */
		,i									/* DTE rate */
		,useron.location					/* User location */
		,useron.level						/* Security level */
		,tleft/60							/* Time left in min */
		,useron.misc&ANSI ? "COLOR":"MONO"  /* ANSI ??? */
		,useron.pass						/* Password */
		,useron.number);					/* User number */
	write(file,str,strlen(str));

	unixtodos(now,&date,&curtime);
	sprintf(str,"%lu\r\n%02d:%02d\r\n%02d:%02d %02d/%02d/%02d\r\n%s\r\n"
		,tleft								/* Time left in seconds */
		,curtime.ti_hour,curtime.ti_min 	/* Current time HH:MM */
		,curtime.ti_hour,curtime.ti_min 	/* Current time and date HH:MM */
		,date.da_mon,date.da_day			/* MM/DD/YY */
		,TM_YEAR(date.da_year-1900)
		,nulstr);							/* Conferences with access */
	write(file,str,strlen(str));

	unixtodos(useron.laston,&date,&lastcall);
	sprintf(str,"%u\r\n%u\r\n%u\r\n%u\r\n%s\r\n%s %02u:%02u\r\n"
		,0									/* Daily download total */
		,0									/* Max download files */
		,0									/* Daily download k total */
		,0									/* Max download k total */
		,useron.phone						/* User phone number */
		,unixtodstr(useron.laston,tmp)		/* Last on date and time */
		,lastcall.ti_hour					/* MM/DD/YY  HH:MM */
		,lastcall.ti_min);
	write(file,str,strlen(str));

	unixtodos(ns_time,&date,&curtime);
	sprintf(str,"%s\r\n%s\r\n%02d/%02d/%02d\r\n%u\r\n%lu\r\n%u"
		"\r\n%u\r\n%u\r\n"
		,useron.misc&EXPERT 				/* Expert or Novice mode */
			? "EXPERT":"NOVICE"
		,"All"                              /* Transfer Protocol */
		,date.da_mon,date.da_day			/* File new-scan date */
		,TM_YEAR(date.da_year-1900)			/* in MM/DD/YY */
		,useron.logons						/* Total logons */
		,rows								/* Screen length */
		,0									/* Highest message read */
		,useron.uls 						/* Total files uploaded */
		,useron.dls);						/* Total files downloaded */
	write(file,str,strlen(str));

	sprintf(str,"%u\r\n%s\r\nCOM%u\r\n%s\r\n%lu\r\n%s\r\n%s\r\n"
		,8									/* Data bits */
		,online==ON_LOCAL?"LOCAL":"REMOTE"  /* Online local or remote */
		,com_port							/* COMx port */
		,useron.birth						/* User birthday */
		,dte_rate							/* DTE rate */
		,"FALSE"                            /* Already connected? */
		,"Normal Connection");              /* Normal or ARQ connect */
	write(file,str,strlen(str));

	sprintf(str,"%02d/%02d/%02d %02d:%02d\r\n%u\r\n%u\r\n"
		,date.da_mon,date.da_day			/* Current date MM/DD/YY */
		,TM_YEAR(date.da_year-1900)
		,curtime.ti_hour,curtime.ti_min 	/* Current time HH:MM */
		,node_num							/* Node number */
		,0);								/* Door number */
	write(file,str,strlen(str));

	close(file); }

else if(type==XTRN_PCBOARD) { /* PCBoard Files */
	sprintf(str,"%sPCBOARD.SYS",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
        return; }

	sprintf(str,"%2d%2d%2d%2d%c%2d%c%c%5u%-5.5s"
		,-1 								/* Display on/off */
		,0									/* Printer on/off */
		,sys_status&SS_SYSPAGE ? -1:0		/* Page Bell on/off */
		,node_misc&NM_ANSALARM ? -1:0		/* Caller Alarm on/off */
		,SP 								/* Sysop next flag */
		,0									/* Error corrected */
		,useron.misc&NO_EXASCII ? '7'       /* Graphics mode */
			: (useron.misc&(COLOR|ANSI))==(COLOR|ANSI) ? 'Y':'N'
		,'A'                                /* Node chat status */
		,(uint)dte_rate 					/* DTE Port Speed */
		,connection 						/* Connection description */
		);
	write(file,str,23);

	write(file,&useron.number,2);			/* User record number */

	strcpy(tmp,name);
	p=strchr(tmp,SP);
	if(p) *p=0;
	sprintf(str,"%-15.15s%-12s"
		,tmp								/* User's first name */
		,useron.pass);						/* User's password */
	write(file,str,27);

	unixtodos(logontime,&date,&curtime);
	i=(curtime.ti_hour*60)+curtime.ti_min;
	write(file,&i,2);						/* Logon time in min since mid */

	now=time(NULL);
	i=-(((now-starttime)/60)+useron.ttoday); /* Negative minutes used */
	write(file,&i,2);

	sprintf(str,"%02d:%02d",curtime.ti_hour,curtime.ti_min);
	write(file,str,5);

	i=level_timepercall[useron.level];		/* Time allowed on */
	write(file,&i,2);

	i=0;									/* Allowed K-bytes for D/L */
	write(file,&i,2);
	write(file,&i,1);						/* Conference user was in */
	write(file,&i,2);						/* Conferences joined */
	write(file,&i,2);						/* "" */
	write(file,&i,2);						/* "" */
	write(file,&i,2);						/* Conferences scanned */
	write(file,&i,2);						/* "" */
	write(file,&i,2);						/* Conference add time */
	write(file,&i,2);						/* Upload/Sysop Chat time min */

	strcpy(str,"    ");
	write(file,str,4);						/* Language extension */

	sprintf(str,"%-25.25s",name);           /* User's full name */
	write(file,str,25);

	i=(tleft/60);
	write(file,&i,2);						/* Minutes remaining */

	write(file,&node_num,1);				/* Node number */

	sprintf(str,"%02d:%02d%2d%2d"           /* Scheduled Event time */
		,0 // sys_eventtime/60
		,0 // sys_eventtime%60
		,0 // sys_timed[0] ? -1:0				 /* Event active ? */
		,0									/* Slide event ? */
		);
	write(file,str,9);

	l=0L;
	write(file,&l,4);						/* Memorized message number */

	sprintf(str,"%d%c%c%d%s%c%c%d%d%d%c%c"
		,com_port							/* COM Port number */
		,SP 								/* Reserved */
		,SP 								/* "" */
		,(useron.misc&ANSI)==ANSI			/* 1=ANSI 0=NO ANSI */
		,"01-01-80"                         /* last event date */
		,0,0								/* last event minute */
		,0									/* caller exited to dos */
		,sys_status&SS_EVENT ? 1:0			/* event up coming */
		,0									/* stop uploads */
		,0,0								/* conference user was in */
		);
	write(file,str,19);

	close(file);			/* End of PCBOARD.SYS creation */

	sprintf(str,"%sUSERS.SYS",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
        return; }

	/* Write goof-ball header */

	i=145;
	write(file,&i,2);		/* PCBoard version number (i.e. 145) */
	l=useron.number;
	write(file,&l,4);		/* Record number for USER's file */
	i=218;
	write(file,&i,2);		/* Size of "fixed" user record */
	i=1;
	write(file,&i,2);		/* Number of conference areas */
	i=7;
	write(file,&i,2);		/* Number of bit map fields for conferences */
	i=5;
	write(file,&i,2);		/* Size of each bit map field */
	memset(str,0,15);
	write(file,str,15); 	/* Name of Third Party Application (if any) */
	i=0;
	write(file,&i,2);		/* Version number for application (if any) */
	write(file,&i,2);		/* Size of a "fixed length" record (if any) */
	write(file,&i,2);		/* Size of conference record (if any) */
	l=0;
	write(file,&l,4);		/* Offset of AppRec into USERS.INF (if any) */
	i=0;
	write(file,&i,1);		/* 1, if USERS.SYS file has been updated */

	/* Write fixed record portion */

	write(file,name,26);			/* Name */
	sprintf(str,"%.24s",useron.location);
	write(file,str,25); 			/* Location */
	write(file,useron.pass,13); 	/* Password */
	write(file,useron.phone,14);	/* Business or Data Phone */
	write(file,useron.phone,14);	/* Home or Voice Phone */
	i=unixtojulian(useron.laston);
	write(file,&i,2);				/* Date last on */
	unixtodos(useron.laston,&date,&curtime);
	sprintf(str,"%02d:%02d",curtime.ti_hour,curtime.ti_min);
	write(file,str,6);				/* Last time on */
	if(useron.misc&EXPERT)
		i=1;
	else
		i=0;
	write(file,&i,1);				/* Expert mode */
	i='Z';
	write(file,&i,1);				/* Protocol (A-Z) */
	if(useron.misc&CLRSCRN)
		i=2;
	else
		i=0;
	write(file,&i,1);				/* bit packed flags */
	i=0;
	write(file,&i,2);				/* DOS date for last DIR Scan */
	i=useron.level;
	write(file,&i,2);				/* Security level */
	write(file,&useron.logons,2);	/* Number of times caller has connected */
	c=rows;
	write(file,&c,1);				/* Page length */
	write(file,&useron.uls,2);		/* Number of uploads */
	write(file,&useron.dls,2);		/* Number of downloads */
	l=0;
	write(file,&l,4);				/* Number of download bytes today */
	write(file,&useron.note,31);	/* Comment #1 */
	write(file,&useron.comp,31);	/* Comment #2 */
	i=(now-starttime)/60;
	write(file,&i,2);				/* Minutes online (this logon?) */
	i=unixtojulian(useron.expire);
	write(file,&i,2);				/* Expiration date */
	i=expired_level;
	write(file,&i,2);				/* Expired security level */
	i=1;
	write(file,&i,2);				/* Current conference */
	write(file,&useron.dlb,4);		/* Bytes downloaded */
	write(file,&useron.ulb,4);		/* Bytes uploaded */
	if(useron.misc&DELETED)
		i=1;
	else
		i=0;
	write(file,&i,1);				/* Deleted? */
	l=useron.number;
	write(file,&l,4);				/* Record number in USERS.INF file */
	l=0;
	memset(str,0,9);
	write(file,str,9);				/* Reserved */
	write(file,&l,4);				/* Number of messages read */
	l=useron.posts+useron.emails+useron.fbacks;
	write(file,&l,4);				/* Number of messages left */
	close(file);

	/* End of USERS.SYS creation */

	}

else if(type==XTRN_SPITFIRE) {	 /* SpitFire SFDOORS.DAT File */
	sprintf(str,"%sSFDOORS.DAT",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return; }

	now=time(NULL);
	unixtodos(now,&date,&curtime);
	l=((((long)curtime.ti_hour*60L)+(long)curtime.ti_min)*60L)
		+(long)curtime.ti_sec;

	strcpy(tmp,name);
	if((p=strchr(tmp,SP))!=NULL)
		*p=0;

	sprintf(str,"%u\r\n%s\r\n%s\r\n%s\r\n%lu\r\n%u\r\n%lu\r\n%lu\r\n"
		,useron.number						/* User number */
		,name								/* User name */
		,useron.pass						/* Password */
		,tmp								/* User's first name */
		,dte_rate							/* DTE Rate */
		,com_port							/* COM Port */
		,tleft/60							/* Time left in minutes */
		,l									/* Seconds since midnight (now) */
		);
	write(file,str,strlen(str));

	unixtodos(logontime,&date,&curtime);
	l=((((long)curtime.ti_hour*60L)+(long)curtime.ti_min)*60L)
		+(long)curtime.ti_sec;

	sprintf(str,"%s\r\n%s\r\n%u\r\n%u\r\n%u\r\n%u\r\n%lu\r\n%lu\r\n%s\r\n"
		"%s\r\n%s\r\n%lu\r\n%s\r\n%u\r\n%u\r\n%u\r\n%u\r\n%u\r\n%lu\r\n%u\r\n"
		"%lu\r\n%lu\r\n%s\r\n%s\r\n"
		,dropdir
		,useron.misc&ANSI ? "TRUE":"FALSE"  /* ANSI ? True or False */
		,useron.level						/* Security level */
		,useron.uls 						/* Total uploads */
		,useron.dls 						/* Total downloads */
		,level_timepercall[useron.level]	/* Minutes allowed this call */
		,l									/* Secs since midnight (logon) */
		,starttime-logontime				/* Extra time in seconds */
		,"FALSE"                            /* Sysop next? */
		,"FALSE"                            /* From Front-end? */
		,"FALSE"                            /* Software Flow Control? */
		,dte_rate							/* DTE Rate */
		,"FALSE"                            /* Error correcting connection? */
		,0									/* Current conference */
		,0									/* Current file dir */
		,node_num							/* Node number */
		,15 								/* Downloads allowed per day */
		,0									/* Downloads already this day */
		,100000 							/* Download bytes allowed/day */
		,0									/* Downloaded bytes already today */
		,useron.ulb/1024L					/* Kbytes uploaded */
		,useron.dlb/1024L					/* Kbytes downloaded */
		,useron.phone						/* Phone Number */
		,useron.location					/* City, State */
		);
	write(file,str,strlen(str));

    close(file); }

else if(type==XTRN_UTI) { /* UTI v2.1 - UTIDOOR.TXT */
	sprintf(str,"%sUTIDOOR.TXT",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
        return; }

	strcpy(tmp,name);
	strupr(tmp);
	sprintf(str,"%s\r\n%lu\r\n%u\r\n%lu\r\n%lu\r\n"
		,tmp								/* User name */
		,cur_rate							/* Actual BPS rate */
		,online==ON_LOCAL ? 0: com_port 	/* COM Port */
		,dte_rate							/* DTE rate */
		,tleft);							/* Time left in sec */
    write(file,str,strlen(str));

	close(file); }

else if(type==XTRN_SR) { /* Solar Realms DOORFILE.SR */
	sprintf(str,"%sDOORFILE.SR",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return; }

	sprintf(str,"%s\r\n%d\r\n%d\r\n%lu\r\n%lu\r\n%u\r\n%lu\r\n"
		,name								/* Complete name of user */
		,useron.misc&ANSI ? 1:0 			/* ANSI ? */
		,useron.misc&NO_EXASCII ? 0:1		/* IBM characters ? */
		,rows								/* Page length */
		,dte_rate							/* Baud rate */
		,online==ON_LOCAL ? 0:com_port		/* COM port */
		,tleft/60							/* Time left (in minutes) */
		);
	write(file,str,strlen(str));
	close(file); }

else if(type==XTRN_TRIBBS) { /* TRIBBS.SYS */
	sprintf(str,"%sTRIBBS.SYS",dropdir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return; }

	sprintf(str,"%u\r\n%s\r\n%s\r\n%u\r\n%c\r\n%c\r\n%lu\r\n%s\r\n%s\r\n%s\r\n"
		,useron.number						/* User's record number */
		,name								/* User's name */
		,useron.pass						/* User's password */
		,useron.level						/* User's level */
		,useron.misc&EXPERT ? 'Y':'N'       /* Expert? */
		,useron.misc&ANSI ? 'Y':'N'         /* ANSI? */
		,tleft/60							/* Minutes left */
		,useron.phone						/* User's phone number */
		,useron.location					/* User's city and state */
		,useron.birth						/* User's birth date */
		);
	write(file,str,strlen(str));

	sprintf(str,"%u\r\n%u\r\n%lu\r\n%lu\r\n%c\r\n%c\r\n%s\r\n%s\r\n%s\r\n"
		,node_num							/* Node number */
		,com_port							/* Serial port */
		,online==ON_LOCAL ? 0L:cur_rate 	/* Baud rate */
		,dte_rate							/* Locked rate */
		,mdm_misc&(MDM_RTS|MDM_CTS) ? 'Y':'N'
		,'Y'                                /* Error correcting connection */
		,sys_name							/* Board's name */
		,sys_op 							/* Sysop's name */
		,useron.handle						/* User's alias */
		);
	write(file,str,strlen(str));
	close(file); }

else if(type)
	errormsg(WHERE,ERR_CHK,"Drop file type",type);

}

/****************************************************************************/
/* Reads in MODUSER.DAT, EXITINFO.BBS, or DOOR.SYS and modify the current	*/
/* user's values.                                                           */
/****************************************************************************/
void moduserdat(uint xtrnnum)
{
	char str[256],path[256],c,startup[128];
	uint i;
	long mod;
    int file;
    FILE *stream;

sprintf(startup,"%s\\",xtrn[xtrnnum]->path);
if(xtrn[xtrnnum]->type==XTRN_RBBS) {
	sprintf(path,"%sEXITINFO.BBS"
		,xtrn[xtrnnum]->misc&STARTUPDIR ? startup : node_dir);
	if((file=nopen(path,O_RDONLY))!=-1) {
		lseek(file,361,SEEK_SET);
		read(file,&useron.flags1,4); /* Flags */
		putuserrec(useron.number,U_FLAGS1,8,ultoa(useron.flags1,tmp,16));
		lseek(file,373,SEEK_SET);
		read(file,&i,2);			/* SecLvl */
		if(i<90) {
			useron.level=i;
			putuserrec(useron.number,U_LEVEL,2,itoa(useron.level,tmp,10)); }
		close(file);
		remove(path); }
	return; }
else if(xtrn[xtrnnum]->type==XTRN_GAP) {
	sprintf(path,"%sDOOR.SYS"
		,xtrn[xtrnnum]->misc&STARTUPDIR ? startup : node_dir);
	if((stream=fopen(path,"rb"))!=NULL) {
		for(i=0;i<15;i++)			/* skip first 14 lines */
			if(!fgets(str,128,stream))
				break;
		if(i==15 && isdigit(str[0])) {
			mod=atoi(str);
			if(mod<90) {
				useron.level=mod;
				putuserrec(useron.number,U_LEVEL,2,itoa(useron.level,tmp,10)); } }

		for(;i<23;i++)
			if(!fgets(str,128,stream))
				break;
		if(i==23) { 					/* set main flags */
			useron.flags1=aftol(str);
			putuserrec(useron.number,U_FLAGS1,8,ultoa(useron.flags1,tmp,16)); }

		for(;i<25;i++)
			if(!fgets(str,128,stream))
				break;
		if(i==25 && isdigit(str[0]) && isdigit(str[1])
			&& (str[2]=='/' || str[2]=='-') /* xx/xx/xx or xx-xx-xx */
			&& isdigit(str[3]) && isdigit(str[4])
			&& (str[5]=='/' || str[5]=='-')
			&& isdigit(str[6]) && isdigit(str[7])) { /* valid expire date */
			useron.expire=dstrtounix(str);
			putuserrec(useron.number,U_EXPIRE,8,ultoa(useron.expire,tmp,16)); }

		for(;i<29;i++)					/* line 29, total downloaded files */
			if(!fgets(str,128,stream))
				break;
		if(i==29) {
			truncsp(str);
			useron.dls=atoi(str);
			putuserrec(useron.number,U_DLS,5,str); }

		if(fgets(str,128,stream)) { 	/* line 30, Kbytes downloaded today */
            i++;
			truncsp(str);
			mod=atol(str)*1024L;
			if(mod) {
				useron.dlb=adjustuserrec(useron.number,U_DLB,10,mod);
				subtract_cdt(mod); } }

		for(;i<42;i++)
			if(!fgets(str,128,stream))
				break;
		if(i==42 && isdigit(str[0])) {	/* Time Credits in Minutes */
			useron.min=atol(str);
			putuserrec(useron.number,U_MIN,10,ultoa(useron.min,tmp,10)); }

		fclose(stream); }
	return; }

else if(xtrn[xtrnnum]->type==XTRN_PCBOARD) {
	sprintf(path,"%sUSERS.SYS"
		,xtrn[xtrnnum]->misc&STARTUPDIR ? startup : node_dir);
	if((file=nopen(path,O_RDONLY))!=-1) {
		lseek(file,39,SEEK_SET);
		read(file,&c,1);
		if(c==1) {	 /* file has been updated */
			lseek(file,105,SEEK_CUR);	/* read security level */
			read(file,&i,2);
			if(i<90) {
				useron.level=i;
				putuserrec(useron.number,U_LEVEL,2,itoa(useron.level,tmp,10)); }
			lseek(file,75,SEEK_CUR);	/* read in expiration date */
			read(file,&i,2);	/* convert from julian to unix */
			useron.expire=juliantounix(i);
			putuserrec(useron.number,U_EXPIRE,8,ltoa(useron.expire,tmp,16)); }
		close(file); }
	return; }

sprintf(path,"%sMODUSER.DAT"
		,xtrn[xtrnnum]->misc&STARTUPDIR ? startup : node_dir);
if((stream=fopen(path,"rb"))!=NULL) {        /* File exists */
	if(fgets(str,81,stream) && (mod=atol(str))!=0) {
		ultoac(mod>0L ? mod : -mod,tmp);		/* put commas in the # */
		strcpy(str,"Credit Adjustment: ");
		if(mod<0L)
			strcat(str,"-");                    /* negative, put '-' */
		strcat(str,tmp);
		if(mod>0L)
			strcpy(tmp,"$+");
		else
			strcpy(tmp,"$-");
		logline(tmp,str);
		if(mod>0L)			/* always add to real cdt */
			useron.cdt=adjustuserrec(useron.number,U_CDT,10,mod);
		else
			subtract_cdt(-mod); }	/* subtract from free cdt first */
	if(fgets(str,81,stream)) {		/* main level */
		mod=atoi(str);
		if(isdigit(str[0]) && mod<90) {
			useron.level=mod;
			putuserrec(useron.number,U_LEVEL,2,itoa(useron.level,tmp,10)); } }
	fgets(str,81,stream);		 /* was transfer level, now ignored */
	if(fgets(str,81,stream)) {		/* flags #1 */
		if(strchr(str,'-'))         /* remove flags */
			useron.flags1&=~aftol(str);
		else						/* add flags */
			useron.flags1|=aftol(str);
		putuserrec(useron.number,U_FLAGS1,8,ultoa(useron.flags1,tmp,16)); }

	if(fgets(str,81,stream)) {		/* flags #2 */
		if(strchr(str,'-'))         /* remove flags */
			useron.flags2&=~aftol(str);
		else						/* add flags */
			useron.flags2|=aftol(str);
		putuserrec(useron.number,U_FLAGS2,8,ultoa(useron.flags2,tmp,16)); }

	if(fgets(str,81,stream)) {		/* exemptions */
		if(strchr(str,'-'))
			useron.exempt&=~aftol(str);
		else
			useron.exempt|=aftol(str);
		putuserrec(useron.number,U_EXEMPT,8,ultoa(useron.exempt,tmp,16)); }
	if(fgets(str,81,stream)) {		/* restrictions */
		if(strchr(str,'-'))
			useron.rest&=~aftol(str);
		else
			useron.rest|=aftol(str);
		putuserrec(useron.number,U_REST,8,ultoa(useron.rest,tmp,16)); }
	if(fgets(str,81,stream)) {		/* Expiration date */
		if(isxdigit(str[0]))
			putuserrec(useron.number,U_EXPIRE,8,str); }
	if(fgets(str,81,stream)) {		/* additional minutes */
		mod=atol(str);
		if(mod) {
			sprintf(str,"Minute Adjustment: %s",ultoac(mod,tmp));
			logline("*+",str);
			useron.min=adjustuserrec(useron.number,U_MIN,10,mod); } }

	if(fgets(str,81,stream)) {		/* flags #3 */
		if(strchr(str,'-'))         /* remove flags */
			useron.flags3&=~aftol(str);
		else						/* add flags */
			useron.flags3|=aftol(str);
		putuserrec(useron.number,U_FLAGS3,8,ultoa(useron.flags3,tmp,16)); }

	if(fgets(str,81,stream)) {		/* flags #4 */
		if(strchr(str,'-'))         /* remove flags */
			useron.flags4&=~aftol(str);
		else						/* add flags */
			useron.flags4|=aftol(str);
		putuserrec(useron.number,U_FLAGS4,8,ultoa(useron.flags4,tmp,16)); }

	if(fgets(str,81,stream)) {		/* flags #1 to REMOVE only */
		useron.flags1&=~aftol(str);
		putuserrec(useron.number,U_FLAGS1,8,ultoa(useron.flags1,tmp,16)); }
	if(fgets(str,81,stream)) {		/* flags #2 to REMOVE only */
		useron.flags2&=~aftol(str);
		putuserrec(useron.number,U_FLAGS2,8,ultoa(useron.flags2,tmp,16)); }
	if(fgets(str,81,stream)) {		/* flags #3 to REMOVE only */
		useron.flags3&=~aftol(str);
		putuserrec(useron.number,U_FLAGS3,8,ultoa(useron.flags3,tmp,16)); }
	if(fgets(str,81,stream)) {		/* flags #4 to REMOVE only */
		useron.flags4&=~aftol(str);
		putuserrec(useron.number,U_FLAGS4,8,ultoa(useron.flags4,tmp,16)); }
	if(fgets(str,81,stream)) {		/* exemptions to remove */
		useron.exempt&=~aftol(str);
		putuserrec(useron.number,U_EXEMPT,8,ultoa(useron.exempt,tmp,16)); }
	if(fgets(str,81,stream)) {		/* restrictions to remove */
		useron.rest&=~aftol(str);
		putuserrec(useron.number,U_REST,8,ultoa(useron.rest,tmp,16)); }

	fclose(stream);
	remove(path); }
}

/****************************************************************************/
/* This is the external programs (doors) section of the bbs                 */
/* Return 1 if no externals available, 0 otherwise. 						*/
/****************************************************************************/
char xtrn_sec()
{
	char str[256];
	int file,j,k,xsec,*usrxtrn,usrxtrns,*usrxsec,usrxsecs;
	uint i;

if(!total_xtrns || !total_xtrnsecs) {
	bputs(text[NoXtrnPrograms]);
	return(1); }

if((usrxtrn=(int *)MALLOC(total_xtrns*sizeof(int)))==NULL) {
	errormsg(WHERE,ERR_ALLOC,nulstr,total_xtrns);
	return(1); }
if((usrxsec=(int *)MALLOC(total_xtrnsecs*sizeof(int)))==NULL) {
	errormsg(WHERE,ERR_ALLOC,nulstr,total_xtrnsecs);
	FREE(usrxtrn);
    return(1); }

while(online) {
	for(i=0,usrxsecs=0;i<total_xtrnsecs;i++)
		if(chk_ar(xtrnsec[i]->ar,useron))
			usrxsec[usrxsecs++]=i;
	if(!usrxsecs) {
		bputs(text[NoXtrnPrograms]);
		FREE(usrxtrn);
		FREE(usrxsec);
		return(1); }
	if(usrxsecs>1) {
		sprintf(str,"%sMENU\\XTRN_SEC.*",text_dir);
		if(fexist(str)) {
			menu("XTRN_SEC");
			xsec=getnum(usrxsecs);
			if(xsec<=0)
				break;
			xsec--;
			xsec=usrxsec[xsec]; }
		else {
			for(i=0;i<total_xtrnsecs;i++)
				uselect(1,i,"External Program Section"
					,xtrnsec[i]->name,xtrnsec[i]->ar);
			xsec=uselect(0,0,0,0,0); }
		if(xsec==-1)
			break; }
	else
		xsec=0;

	while(!chk_ar(xtrnsec[xsec]->ar,useron))
		xsec++;

	if(xsec>=total_xtrnsecs) {
		bputs(text[NoXtrnPrograms]);
		FREE(usrxtrn);
		FREE(usrxsec);
        return(1); }

	while(online) {
		for(i=0,usrxtrns=0;i<total_xtrns; i++) {
			if(xtrn[i]->sec!=xsec)
				continue;
			if(xtrn[i]->misc&EVENTONLY)
				continue;
			if(!chk_ar(xtrn[i]->ar,useron))
				continue;
			usrxtrn[usrxtrns++]=i; }
		if(!usrxtrns) {
			bputs(text[NoXtrnPrograms]);
			pause();
			break; }
		sprintf(str,"%sMENU\\XTRN%u.*",text_dir,xsec+1);
		if(fexist(str)) {
			sprintf(str,"XTRN%u",xsec+1);
			menu(str); }
		else {
			bprintf(text[XtrnProgLstHdr],xtrnsec[xsec]->name);
			bputs(text[XtrnProgLstTitles]);
			if(usrxtrns>=10) {
				bputs("     ");
				bputs(text[XtrnProgLstTitles]); }
			CRLF;
			bputs(text[XtrnProgLstUnderline]);
			if(usrxtrns>=10) {
				bputs("     ");
				bputs(text[XtrnProgLstUnderline]); }
			CRLF;
			if(usrxtrns>=10)
				j=(usrxtrns/2)+(usrxtrns&1);
			else
				j=usrxtrns;
			for(i=0;i<j && !msgabort();i++) {
				bprintf(text[XtrnProgLstFmt],i+1
					,xtrn[usrxtrn[i]]->name,xtrn[usrxtrn[i]]->cost);
				if(usrxtrns>=10) {
					k=(usrxtrns/2)+i+(usrxtrns&1);
					if(k<usrxtrns) {
						bputs("     ");
						bprintf(text[XtrnProgLstFmt],k+1
							,xtrn[usrxtrn[k]]->name
							,xtrn[usrxtrn[k]]->cost); } }
				CRLF; }
			ASYNC;
			mnemonics(text[WhichXtrnProg]); }
		getnodedat(node_num,&thisnode,1);
		thisnode.aux=0; /* aux is 0, only if at menu */
		putnodedat(node_num,thisnode);
		action=NODE_XTRN;
		SYNC;
		if((j=getnum(usrxtrns))<1)
			break;
		exec_xtrn(usrxtrn[j-1]); }
	if(usrxsecs<2)
		break; }
FREE(usrxtrn);
FREE(usrxsec);
return(0);
}

/****************************************************************************/
/* This function handles configured external program execution. 			*/
/****************************************************************************/
void exec_xtrn(uint xtrnnum)
{
	char str[256],str2[256],path[256],dropdir[128],name[32],c,e,mode;
    int file;
	uint i;
	long mod;
	ulong tleft;
    FILE *stream;
    node_t node;
	time_t start,end;


if(!chk_ar(xtrn[xtrnnum]->run_ar,useron)
	|| !chk_ar(xtrnsec[xtrn[xtrnnum]->sec]->ar,useron)) {
	bputs(text[CantRunThatProgram]);
	return; }

if(xtrn[xtrnnum]->cost && !(useron.exempt&FLAG('X'))) {    /* costs */
	if(xtrn[xtrnnum]->cost>useron.cdt+useron.freecdt) {
		bputs(text[NotEnoughCredits]);
		pause();
		return; }
	subtract_cdt(xtrn[xtrnnum]->cost); }

if(!(xtrn[xtrnnum]->misc&MULTIUSER)) {
	for(i=1;i<=sys_nodes;i++) {
		getnodedat(i,&node,0);
		c=i;
		if((node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& node.action==NODE_XTRN && node.aux==(xtrnnum+1)) {
			if(node.status==NODE_QUIET) {
				strcpy(str,sys_guru);
				c=sys_nodes+1; }
			else if(node.misc&NODE_ANON)
				strcpy(str,"UNKNOWN USER");
			else
				username(node.useron,str);
			bprintf(text[UserRunningXtrn],str
				,xtrn[xtrnnum]->name,c);
			pause();
			break; } }
	if(i<=sys_nodes)
		return; }

sprintf(str,"%s\\",xtrn[xtrnnum]->path);
strcpy(path,xtrn[xtrnnum]->misc&STARTUPDIR ? str : node_dir);
strcpy(dropdir,xtrn[xtrnnum]->misc&STARTUPDIR ? str : node_dir);

switch(xtrn[xtrnnum]->type) {
	case XTRN_WWIV:
		strcat(path,"CHAIN.TXT");
		break;
	case XTRN_GAP:
		strcat(path,"DOOR.SYS");
		break;
	case XTRN_RBBS:
		sprintf(str,"DORINFO%X.DEF",node_num);
		strcat(path,str);
		break;
	case XTRN_RBBS1:
		strcat(path,"DORINFO1.DEF");
		break;
	case XTRN_WILDCAT:
		strcat(path,"CALLINFO.BBS");
		break;
	case XTRN_PCBOARD:
		strcat(path,"PCBOARD.SYS");
		break;
	case XTRN_UTI:
		strcat(path,"UTIDOOR.TXT");
		break;
	case XTRN_SR:
		strcat(path,"DOORFILE.SR");
		break;
	default:
		strcat(path,"XTRN.DAT");
		break; }
getnodedat(node_num,&thisnode,1);
thisnode.aux=xtrnnum+1;
thisnode.action=NODE_XTRN;
putnodedat(node_num,thisnode);

if(xtrn[xtrnnum]->misc&REALNAME)
    strcpy(name,useron.name);
else
    strcpy(name,useron.alias);

gettimeleft();
tleft=timeleft+(xtrn[xtrnnum]->textra*60);
if(xtrn[xtrnnum]->maxtime && tleft>xtrn[xtrnnum]->maxtime)
	tleft=(xtrn[xtrnnum]->maxtime*60);
xtrndat(name,dropdir,xtrn[xtrnnum]->type,tleft);
if(!online)
	return;
sprintf(str,"Ran external: %s",xtrn[xtrnnum]->name);
logline("X-",str);
if(xtrn[xtrnnum]->cmd[0]!='*' && sys_status&SS_LOGOPEN) {
	close(logfile);
	sys_status&=~SS_LOGOPEN; }

sprintf(str,"%sINTRSBBS.DAT"
		,xtrn[xtrnnum]->path[0] ? xtrn[xtrnnum]->path : node_dir);
remove(str);
sprintf(str,"%sHANGUP.NOW",node_dir);
remove(str);
sprintf(str,"%sFILE\\%04u.DWN",data_dir,useron.number);
remove(str);

mode=0; 	/* EX_CC */
if(xtrn[xtrnnum]->misc&IO_INTS)
	mode|=(EX_OUTR|EX_INR|EX_OUTL);
if(xtrn[xtrnnum]->misc&WWIVCOLOR)
	mode|=EX_WWIV;
if(xtrn[xtrnnum]->misc&SWAP)
	mode|=EX_SWAP;
if(xtrn[xtrnnum]->misc&MODUSERDAT) {	 /* Delete MODUSER.DAT */
	sprintf(str,"%sMODUSER.DAT",dropdir);       /* if for some weird  */
	remove(str); }								/* reason it's there  */

if(xtrn[xtrnnum]->path[0]) {
	if(xtrn[xtrnnum]->path[1]==':')                /* drive letter specified */
		setdisk(toupper(xtrn[xtrnnum]->path[0])-'A');
	if(chdir(xtrn[xtrnnum]->path))
		errormsg(WHERE,ERR_CHDIR,xtrn[xtrnnum]->path,0); }

start=time(NULL);
external(cmdstr(xtrn[xtrnnum]->cmd,path,dropdir,NULL),mode);
end=time(NULL);
if(xtrn[xtrnnum]->misc&FREETIME)
	starttime+=end-start;
if(xtrn[xtrnnum]->clean[0]) {
	if(xtrn[xtrnnum]->path[0]) {
		if(xtrn[xtrnnum]->path[1]==':')             /* drive letter specified */
			setdisk(toupper(xtrn[xtrnnum]->path[0])-'A');
		if(chdir(xtrn[xtrnnum]->path))
			errormsg(WHERE,ERR_CHDIR,xtrn[xtrnnum]->path,0); }
	external(cmdstr(xtrn[xtrnnum]->clean,path,nulstr,NULL)
		,mode&~EX_INR); }
/* Re-open the logfile */
if(!(sys_status&SS_LOGOPEN)) {
	sprintf(str,"%sNODE.LOG",node_dir);
	if((logfile=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1)
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
	else
		sys_status|=SS_LOGOPEN; }

sprintf(str,"%sFILE\\%04u.DWN",data_dir,useron.number);
batch_add_list(str);

sprintf(str,"%sHANGUP.NOW",node_dir);
if(fexist(str)) {
	remove(str);
	hangup(); }
if(online==ON_REMOTE) {
	checkline();
	if(!online) {
		sprintf(str,"%sHUNGUP.LOG",data_dir);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
			return; }
		getnodedat(node_num,&thisnode,0);
		now=time(NULL);
		sprintf(str,hungupstr,useron.alias,xtrn[thisnode.aux-1]->name
			,timestr(&now));
		write(file,str,strlen(str));
		close(file); } }
if(xtrn[xtrnnum]->misc&MODUSERDAT) {	/* Modify user data */
	moduserdat(xtrnnum);
	statusline(); }

getnodedat(node_num,&thisnode,1);
thisnode.aux=0; /* aux is 0, only if at menu */
putnodedat(node_num,thisnode);
}

/****************************************************************************/
/* This function will execute an external program if it is configured to    */
/* run during the event specified.                                          */
/****************************************************************************/
void user_event(char event)
{
    uint i;

for(i=0;i<total_xtrns;i++) {
	if(xtrn[i]->event!=event)
        continue;
	if(!chk_ar(xtrn[i]->ar,useron)
		|| !chk_ar(xtrnsec[xtrn[i]->sec]->ar,useron))
        continue;
    exec_xtrn(i); }
}


