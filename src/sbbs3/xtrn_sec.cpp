/* xtrn_sec.cpp */

/* Synchronet external program/door section and drop file routines */

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
/* This is the external programs (doors) section of the bbs                 */
/* Return 1 if no externals available, 0 otherwise. 						*/
/****************************************************************************/
int sbbs_t::xtrn_sec()
{
	char	str[MAX_PATH+1];
	int		xsec;
	uint	i,j,k,*usrxtrn,usrxtrns,*usrxsec,usrxsecs;
	long	l;

	if(useron.rest&FLAG('X')) {
		bputs(text[R_ExternalPrograms]);
		return(1);
	}

	if(!cfg.total_xtrns || !cfg.total_xtrnsecs) {
		bputs(text[NoXtrnPrograms]);
		return(1); 
	}

	if((usrxtrn=(uint *)MALLOC(cfg.total_xtrns*sizeof(int)))==NULL) {
		errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_xtrns);
		return(1); 
	}
	if((usrxsec=(uint *)MALLOC(cfg.total_xtrnsecs*sizeof(int)))==NULL) {
		errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_xtrnsecs);
		FREE(usrxtrn);
		return(1); 
	}

	while(online) {
		for(i=0,usrxsecs=0;i<cfg.total_xtrnsecs;i++)
			if(chk_ar(cfg.xtrnsec[i]->ar,&useron))
				usrxsec[usrxsecs++]=i;
		if(!usrxsecs) {
			bputs(text[NoXtrnPrograms]);
			FREE(usrxtrn);
			FREE(usrxsec);
			return(1); 
		}
		if(usrxsecs>1) {
			sprintf(str,"%smenu/xtrn_sec.*",cfg.text_dir);
			if(fexist(str)) {
				menu("xtrn_sec");
				xsec=getnum(usrxsecs);
				if(xsec<=0)
					break;
				xsec--;
				xsec=usrxsec[xsec]; 
			}
			else {
				for(i=0;i<cfg.total_xtrnsecs;i++)
					uselect(1,i,"External Program Section"
						,cfg.xtrnsec[i]->name,cfg.xtrnsec[i]->ar);
				xsec=uselect(0,0,0,0,0); 
			}
			if(xsec==-1)
				break; 
		}
		else
			xsec=0;

		while(!chk_ar(cfg.xtrnsec[xsec]->ar,&useron))
			xsec++;

		if(xsec>=cfg.total_xtrnsecs) {
			bputs(text[NoXtrnPrograms]);
			FREE(usrxtrn);
			FREE(usrxsec);
			return(1); 
		}

		while(online) {
			for(i=0,usrxtrns=0;i<cfg.total_xtrns; i++) {
				if(cfg.xtrn[i]->sec!=xsec)
					continue;
				if(cfg.xtrn[i]->misc&EVENTONLY)
					continue;
				if(!chk_ar(cfg.xtrn[i]->ar,&useron))
					continue;
				usrxtrn[usrxtrns++]=i; 
			}
			if(!usrxtrns) {
				bputs(text[NoXtrnPrograms]);
				pause();
				break; 
			}
			sprintf(str,"%smenu/xtrn%u.*",cfg.text_dir,xsec+1);
			if(fexist(str)) {
				sprintf(str,"xtrn%u",xsec+1);
				menu(str); 
			}
			else {
				bprintf(text[XtrnProgLstHdr],cfg.xtrnsec[xsec]->name);
				bputs(text[XtrnProgLstTitles]);
				if(usrxtrns>=10) {
					bputs("     ");
					bputs(text[XtrnProgLstTitles]); 
				}
				CRLF;
				bputs(text[XtrnProgLstUnderline]);
				if(usrxtrns>=10) {
					bputs("     ");
					bputs(text[XtrnProgLstUnderline]); 
				}
				CRLF;
				if(usrxtrns>=10)
					j=(usrxtrns/2)+(usrxtrns&1);
				else
					j=usrxtrns;
				for(i=0;i<j && !msgabort();i++) {
					bprintf(text[XtrnProgLstFmt],i+1
						,cfg.xtrn[usrxtrn[i]]->name,cfg.xtrn[usrxtrn[i]]->cost);
					if(usrxtrns>=10) {
						k=(usrxtrns/2)+i+(usrxtrns&1);
						if(k<usrxtrns) {
							bputs("     ");
							bprintf(text[XtrnProgLstFmt],k+1
								,cfg.xtrn[usrxtrn[k]]->name
								,cfg.xtrn[usrxtrn[k]]->cost); 
						}
					}
					CRLF; 
				}
				ASYNC;
				mnemonics(text[WhichXtrnProg]); 
			}
			getnodedat(cfg.node_num,&thisnode,1);
			thisnode.aux=0; /* aux is 0, only if at menu */
			putnodedat(cfg.node_num,&thisnode);
			action=NODE_XTRN;
			SYNC;
			if((l=getnum(usrxtrns))<1)
				break;
			l--;
			sprintf(str,"%smenu/xtrn/%s.*",cfg.text_dir,cfg.xtrn[usrxtrn[l]]->code);
			if(fexist(str)) {
				sprintf(str,"xtrn/%s",cfg.xtrn[usrxtrn[l]]->code);
				menu(str);
				lncntr=0;
			}
			exec_xtrn(usrxtrn[l]); 
		}
		if(usrxsecs<2)
			break; 
	}
	FREE(usrxtrn);
	FREE(usrxsec);
	return(0);
}


char *hungupstr="\1n\1h%s\1n hung up on \1h%s\1n %s\r\n";

#ifndef __FLAT__
extern uint riobp;
#endif

extern int mswtyp;
extern uint fakeriobp;

/****************************************************************************/
/* Convert C string to pascal string										*/
/****************************************************************************/
void str2pas(char *instr, char *outstr)
{
	int i;

	outstr[0]=(char)strlen(instr);
	for(i=0;i<outstr[0];i++)
		outstr[i+1]=instr[i];
}

/****************************************************************************/
/* Convert from unix time (seconds since 1/70) to julian (days since 1900)	*/
/****************************************************************************/
int unixtojulian(time_t unix_time)
{
	int days[12]={0,31,59,90,120,151,181,212,243,273,304,334};
	long j;
	struct tm tm;

	if(localtime_r(&unix_time,&tm)==NULL)
		return(0);
	j=36525L*(1900+tm.tm_year);
	if(!(j%100) && (tm.tm_mon+1)<3)
		j--;
	j=(j-(1900*36525))/100;
	j+=tm.tm_mday+days[tm.tm_mon];
	return(j);
}

/****************************************************************************/
/* Convert julian date into unix format 									*/
/****************************************************************************/
#ifdef __BORLANDC__
#pragma argsused
#endif
time_t juliantounix(ulong j)
{
#if 0 /* julian time */
    int days[2][12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
                       0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
    long temp;
	int leap,counter;
	struct tm tm;

	if(!j) return(0L);

	tm.tm_year=((100L*j)/36525L)-1900;
	temp=(long)date.da_year*36525L;
	date.da_year+=1900;
	j-=temp/100L;

	if (!(temp%100)) {
		j++;
		leap=1; 
	}
	else leap=0;

	for(date.da_mon=counter=0;counter<12;counter++)
		if(days[leap][counter]<j)
			date.da_mon=counter;

	date.da_day=j-days[leap][date.da_mon];
	date.da_mon++;	/* go from 0 to 1 based */

	curtime.ti_hour=curtime.ti_min=curtime.ti_sec=0;
	return(dostounix(&date,&curtime));
#else
	return((time_t)-1);
#endif
}

#ifdef __unix__
static void lfexpand(char *str, ulong misc)
{
	char *p;
	char newstr[1024];
	size_t len=0;

	if(misc&XTRN_NATIVE)
		return;

	for(p=str;*p && len < sizeof(newstr)-1;p++) {
		if(*p=='\n')
			newstr[len++]='\r';
		newstr[len++]=*p;
	}

	newstr[len]=0;
	strcpy(str,newstr);
}
#else
	#define lfexpand(str, misc)
#endif

/****************************************************************************/
/* Creates various types of xtrn (Doors, Chains, etc.) data (drop) files.	*/
/****************************************************************************/
void sbbs_t::xtrndat(char *name, char *dropdir, uchar type, ulong tleft
					,ulong misc)
{
	char	str[1024],tmp2[128],c,*p;
	char 	tmp[512];
	int		i,file;
	ushort	w;
	long	l;
	struct tm tm;
	struct tm tl;
	stats_t stats;

	if(type==XTRN_SBBS) {	/* SBBS XTRN.DAT file */
		strcpy(tmp,"XTRN.DAT");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

		sprintf(str,"%s\n%s\n%s\n%s\n"
			,name								/* User name */
			,cfg.sys_name						/* System name */
			,cfg.sys_op 						/* Sysop name */
			,cfg.sys_guru); 					/* Guru name */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%s\n%s\n%u\n%u\n%lu\n%s\n%lu\n%lu\n"
			,cfg.ctrl_dir						/* Ctrl dir */
			,cfg.data_dir						/* Data dir */
			,cfg.sys_nodes						/* Total system nodes */
			,cfg.node_num						/* Current node */
			,tleft								/* User Timeleft in seconds */
			,useron.misc&ANSI					/* User ANSI ? (Yes/Mono/No) */
				? useron.misc&COLOR
				? "Yes":"Mono":"No"
			,rows								/* User Screen lines */
			,useron.cdt+useron.freecdt);		/* User Credits */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%u\n%u\n%s\n%c\n%u\n%s\n"
			,useron.level						/* User main level */
			,useron.level						/* User transfer level */
			,useron.birth						/* User birthday */
			,useron.sex 						/* User sex (M/F) */
			,useron.number						/* User number */
			,useron.phone); 					/* User phone number */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%u\n%u\n%x\n%lu\n%s\n%s\n"
			"%s\n%s\n%s\n%s\n%s\n%s\n%lu\n"
			,misc&IO_INTS ? 0:cfg.com_port		/* Com port or 0 if !FOSSIL */
			,cfg.com_irq						/* Com IRQ */
			,cfg.com_base						/* Com base in hex */
			,dte_rate							/* Com rate */
			,"Yes"								/* Hardware flow ctrl Y/N */
			,"Yes"								/* Locked DTE rate Y/N */
			,cfg.mdm_init						/* Modem initialization string */
			,cfg.mdm_spec						/* Modem special init string */
			,cfg.mdm_term						/* Modem terminal mode init str */
			,cfg.mdm_dial						/* Modem dial string */
			,cfg.mdm_offh						/* Modem off-hook string */
			,cfg.mdm_answ						/* Modem answer string */
	#ifndef __FLAT__
			,sys_status&SS_DCDHIGH ? &fakeriobp : &riobp-1	/* Modem status register */
	#else
			,0L
	#endif
			);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%u\n",cfg.total_xtrns);
		lfexpand(str,misc);
		write(file,str,strlen(str));			/* Total external programs */

		for(i=0;i<cfg.total_xtrns;i++) {		/* Each program's name */
			if(SYSOP || chk_ar(cfg.xtrn[i]->ar,&useron))
				strcpy(str,cfg.xtrn[i]->name);
			else
				str[0]=0;						/* Blank if no access */
			strcat(str,"\n");
			lfexpand(str,misc);
			write(file,str,strlen(str)); 
		}

		sprintf(str,"%s\n%s\n"
			,ltoaf(useron.flags1,tmp)			/* Main flags */
			,ltoaf(useron.flags2,tmp2)			/* Transfer flags */
			);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%s\n%s\n%lx\n%s\n%s\n%s\n"
			,ltoaf(useron.exempt,tmp)			/* Exemptions */
			,ltoaf(useron.rest,tmp2)			/* Restrictions */
			,useron.expire						/* Expiration date in unix form */
			,useron.address 					/* Address */
			,useron.location					/* City/State */
			,useron.zipcode 					/* Zip/Postal code */
			);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%s\n%s\n%d\n%s\n%lu\n%s\n%s\n%s\n%s\n"
			"%lx\n%d\n"
			,ltoaf(useron.flags3,tmp)			/* Flag set #3 */
			,ltoaf(useron.flags4,tmp2)			/* Flag set #4 */
			,mswtyp 							/* Time-slice type */
			,useron.name						/* Real name/company */
			,cur_rate							/* DCE rate */
			,cfg.exec_dir
			,cfg.text_dir
			,cfg.temp_dir
			,cfg.sys_id
			,cfg.node_misc
	#if defined(__OS2__)
			,rio_handle
	#elif defined(_WIN32)
			,misc&IO_INTS ? INVALID_SOCKET : client_socket_dup
	#elif defined(__unix__)
			,misc&IO_INTS ? INVALID_SOCKET : client_socket
	#else
			,-1
	#endif
			);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		close(file); 
	}

	else if(type==XTRN_WWIV) {	/*	WWIV CHAIN.TXT File */
		strcpy(tmp,"CHAIN.TXT");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

		sprintf(str,"%u\n%s\n%s\n%s\n%u\n%c\n"
			,useron.number						/* User number */
			,name								/* User name */
			,useron.name						/* User real name */
			,nulstr 							/* User call sign */
			,getage(&cfg,useron.birth)			/* User age */
			,useron.sex);						/* User sex (M/F) */
		strupr(str);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%lu\n%s\n%u\n%lu\n%u\n%u\n%u\n%u\n%u\n"
			,useron.cdt+useron.freecdt			/* Gold */
			,unixtodstr(&cfg,useron.laston,tmp)	/* User last on date */
			,80 								/* User screen width */
			,rows								/* User screen length */
			,useron.level						/* User SL */
			,0									/* Cosysop? */
			,SYSOP								/* Sysop? (1/0) */
			,(useron.misc&ANSI) ? 1:0			/* ANSI ? (1/0) */
			,online==ON_REMOTE);				/* Remote (1/0) */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%lu\n%s\n%s\n%s\n%lu\n%d\n%s\n%s\n"
			"%u\n%u\n%lu\n%u\n%lu\n%u\n%s\n"
			,tleft								/* Time left in seconds */
			,cfg.node_dir						/* Gfiles dir (log dir) */
			,cfg.data_dir						/* Data dir */
			,"node.log"                         /* Name of log file */
			,dte_rate							/* DTE rate */
			,cfg.com_port						/* COM port number */
			,cfg.sys_name						/* System name */
			,cfg.sys_op 						/* Sysop name */
			,0									/* Logon time (sec past 12am) */
			,0									/* Current time (sec past 12am) */
			,useron.ulb/1024UL					/* User uploaded kbytes */
			,useron.uls 						/* User uploaded files */
			,useron.dlb/1024UL					/* User downloaded kbytes */
			,useron.dls 						/* User downloaded files */
			,"8N1");                            /* Data, parity, stop bits */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		close(file); 
	}

	else if(type==XTRN_GAP) {	/* Gap DOOR.SYS File */
		strcpy(tmp,"DOOR.SYS");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

		sprintf(str,"COM%d:\n%lu\n%u\n%u\n%lu\n%c\n%c\n%c\n%c\n"
			,online==ON_REMOTE ? cfg.com_port:0	/* 01: COM port - 0 if Local */
			,cur_rate							/* 02: DCE rate */
			,8									/* 03: Data bits */
			,cfg.node_num						/* 04: Node number */
			,dte_rate							/* 05: DTE rate */
			,console&CON_L_ECHO ? 'Y':'N'       /* 06: Screen display */
			,'Y'                                /* 07: Printer toggle */
			,'Y'                                /* 08: Page bell */
			,'Y');                              /* 09: Caller alarm */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%s\n%s\n%s\n%s\n%s\n"
			,name								/* 10: User name */
			,useron.location					/* 11: User location */
			,useron.phone						/* 12: User home phone */
			,useron.phone						/* 13: User work/data phone */
			,useron.pass);						/* 14: User password */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%u\n%u\n%s\n%lu\n%lu\n%s\n"
			,useron.level						/* 15: User security level */
			,useron.logons						/* 16: User total logons */
			,unixtodstr(&cfg,useron.laston,tmp)	/* 17: User last on date */
			,tleft								/* 18: User time left in sec */
			,tleft/60							/* 19: User time left in min */
			,useron.misc&NO_EXASCII 			/* 20: GR if COLOR ANSI */
				? "7E" : (useron.misc&(ANSI|COLOR))==(ANSI|COLOR) ? "GR" : "NG");
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%lu\n%c\n%s\n%u\n%s\n%u\n%c\n%u\n%u\n"
			,rows								/* 21: User screen length */
			,useron.misc&EXPERT ? 'Y':'N'       /* 22: Expert? (Y/N) */
			,ltoaf(useron.flags1,tmp2)			/* 23: Registered conferences */
			,0									/* 24: Conference came from */
			,unixtodstr(&cfg,useron.expire,tmp)	/* 25: User expiration date */
			,useron.number						/* 26: User number */
			,'Y'                                /* 27: Default protocol */
			,useron.uls 						/* 28: User total uploads */
			,useron.dls);						/* 29: User total downloads */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%u\n%lu\n%s\n%s\n%s\n%s"
			"\n%s\n%02d:%02d\n%c\n"
			,0									/* 30: Kbytes downloaded today */
			,(useron.cdt+useron.freecdt)/1024UL /* 31: Max Kbytes to download today */
			,useron.birth						/* 32: User birthday */
			,cfg.node_dir						/* 33: Path to MAIN directory */
			,cfg.data_dir						/* 34: Path to GEN directory */
			,cfg.sys_op 						/* 35: Sysop name */
			,nulstr 							/* 36: Alias name */
			,0 // sys_eventtime/60				/* 37: Event time HH:MM */
			,0 // sys_eventtime%60
			,'Y');                              /* 38: Error correcting connection */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		localtime_r(&ns_time,&tm);
		sprintf(str,"%c\n%c\n%u\n%lu\n%02d/%02d/%02d\n"
			,(useron.misc&(NO_EXASCII|ANSI|COLOR))==ANSI
				? 'Y':'N'                       /* 39: ANSI supported but NG mode */
			,'Y'                                /* 40: Use record locking */
			,14 								/* 41: BBS default color */
			,useron.min 						/* 42: Time credits in minutes */
			,tm.tm_mon+1						/* 43: File new-scan date */
			,tm.tm_mday
			,TM_YEAR(tm.tm_year));
		lfexpand(str,misc);
		write(file,str,strlen(str));

		localtime_r(&logontime,&tm);
		localtime_r(&useron.laston,&tl);
		sprintf(str,"%02d:%02d\n%02d:%02d\n%u\n%u\n%lu\n"
			"%lu\n%s\n%u\n%u\n"
			,tm.tm_hour						/* 44: Time of this call */
			,tm.tm_min
			,tl.tm_hour						/* 45: Time of last call */
			,tl.tm_min
			,999								/* 46: Max daily files available */
			,0									/* 47: Files downloaded so far today */
			,useron.ulb/1024UL					/* 48: Total Kbytes uploaded */
			,useron.dlb/1024UL					/* 49: Total Kbytes downloaded */
			,useron.comment 					/* 50: User comment */
			,0									/* 51: Total doors opened */
			,useron.posts); 					/* 52: User message left */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		close(file); 
	}

	else if(type==XTRN_RBBS || type==XTRN_RBBS1) {
		if(type==XTRN_RBBS)
			sprintf(tmp,"DORINFO%X.DEF",cfg.node_num);   /* support 1-F */
		else
			sprintf(tmp,"DORINFO1.DEF");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

		strcpy(tmp,cfg.sys_op);
		p=strchr(tmp,SP);
		if(p)
			*(p++)=0;
		else
			p=nulstr;

		sprintf(str,"%s\n%s\n%s\nCOM%d\n%lu BAUD,N,8,1\n%u\n"
			,cfg.sys_name						/* Name of BBS */
			,tmp								/* Sysop's firstname */
			,p									/* Sysop's lastname */
			,online==ON_REMOTE ? cfg.com_port:0 /* COM port number, 0 if local */
			,dte_rate							/* DTE rate */
			,0);								/* Network type */
		strupr(str);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		strcpy(tmp,name);
		p=strchr(tmp,SP);
		if(p)
			*(p++)=0;
		else
			p=nulstr;
		sprintf(str,"%s\n%s\n%s\n%u\n%u\n%lu\n"
			,tmp								/* User's firstname */
			,p									/* User's lastname */
			,useron.location					/* User's city */
			,(useron.misc&ANSI)==ANSI			/* 1=ANSI 0=ASCII */
			,useron.level						/* Security level */
			,tleft/60); 						/* Time left in minutes */
		strupr(str);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		close(file);

		strcpy(tmp,"EXITINFO.BBS");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
			return; 
		}
		w=(WORD)dte_rate;
		write(file,&w,sizeof(short));			/* BaudRate */
		/* SysInfo */
		getstats(&cfg,0,&stats);
		write(file,&stats.logons,sizeof(long)); /* CallCount */
		write(file,nulstr,36);					/* LastCallerName */
		write(file,nulstr,36);					/* LastCallerAlias */
		write(file,nulstr,92);					/* ExtraSpace */
		/* TimeLogInfo */
		write(file,nulstr,9);					/* StartDate */
		write(file,nulstr,24*sizeof(short));	/* BusyPerHour */
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
		localtime_r(&useron.laston,&tm);
		sprintf(tmp,"%02d:%02d",tm.tm_hour,tm.tm_min);
		str2pas(tmp,str);
		write(file,str,6);						/* LastTime */
		unixtodstr(&cfg,useron.laston,tmp);
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
		w=0;
		write(file,&w,sizeof(short)); 			/* Credit */
		write(file,&w,sizeof(short)); 			/* Pending */
		write(file,&useron.posts,sizeof(short));/* TimesPosted */
		write(file,&w,sizeof(short)); 			/* HighMsgRead */
		w=useron.level;
		write(file,&w,sizeof(short)); 			/* SecLvl */
		w=0;
		write(file,&w,sizeof(short)); 			/* Times */
		write(file,&useron.uls,sizeof(short));	/* Ups */
		write(file,&useron.dls,sizeof(short));	/* Downs */
		w=(ushort)(useron.ulb/1024UL);
		write(file,&w,sizeof(short)); 			/* UpK */
		w=(ushort)(useron.dlb/1024UL);
		write(file,&w,sizeof(short)); 			/* DownK */
		w=(ushort)(logon_dlb/1024UL);
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
		c='\xff';
		write(file,&c,sizeof(char));			/* Days */
		// c=sys_eventnode==node_num || sys_misc&SM_TIMED_EX ? 1 : 0;
		c=0;
		write(file,&c,sizeof(char));			/* Forced */
		if(!cfg.total_events)
			l=0;
		else
			l=cfg.event[0]->last;
		write(file,&l,sizeof(long));			/* LastTimeRun */
		memset(str,0,40);
		write(file,str,7);						/* Spare */

		c=0;
		write(file,&c,1);						/* NetMailEntered */
		write(file,&c,1);						/* EchoMailEntered */

		localtime_r(&logontime,&tm);
		sprintf(tmp,"%02d:%02d",tm.tm_hour,tm.tm_min);
		str2pas(tmp,str);
		write(file,str,6);						/* LoginTime */
		unixtodstr(&cfg,logontime,tmp);
		str2pas(tmp,str);
		write(file,str,9);						/* LoginDate */
		write(file,&cfg.level_timepercall[useron.level],sizeof(short));  /* TmLimit */
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
		strcpy(tmp,"CALLINFO.BBS");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

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
					break; 
		}
		sprintf(str,"%s\n%u\n%s\n%u\n%lu\n%s\n%s\n%u\n"
			,name								/* User name */
			,i									/* DTE rate */
			,useron.location					/* User location */
			,useron.level						/* Security level */
			,tleft/60							/* Time left in min */
			,useron.misc&ANSI ? "COLOR":"MONO"  /* ANSI ??? */
			,useron.pass						/* Password */
			,useron.number);					/* User number */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		localtime_r(&now,&tm);
		sprintf(str,"%lu\n%02d:%02d\n%02d:%02d %02d/%02d/%02d\n%s\n"
			,tleft								/* Time left in seconds */
			,tm.tm_hour,tm.tm_min 			/* Current time HH:MM */
			,tm.tm_hour,tm.tm_min 			/* Current time and date HH:MM */
			,tm.tm_mon+1,tm.tm_mday			/* MM/DD/YY */
			,TM_YEAR(tm.tm_year)
			,nulstr);							/* Conferences with access */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		localtime_r(&useron.laston,&tm);
		sprintf(str,"%u\n%u\n%u\n%u\n%s\n%s %02u:%02u\n"
			,0									/* Daily download total */
			,0									/* Max download files */
			,0									/* Daily download k total */
			,0									/* Max download k total */
			,useron.phone						/* User phone number */
			,unixtodstr(&cfg,useron.laston,tmp)	/* Last on date and time */
			,tm.tm_hour						/* MM/DD/YY  HH:MM */
			,tm.tm_min);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		localtime_r(&ns_time,&tm);
		sprintf(str,"%s\n%s\n%02d/%02d/%02d\n%u\n%lu\n%u"
			"\n%u\n%u\n"
			,useron.misc&EXPERT 				/* Expert or Novice mode */
				? "EXPERT":"NOVICE"
			,"All"                              /* Transfer Protocol */
			,tm.tm_mon+1,tm.tm_mday			/* File new-scan date */
			,TM_YEAR(tm.tm_year)				/* in MM/DD/YY */
			,useron.logons						/* Total logons */
			,rows								/* Screen length */
			,0									/* Highest message read */
			,useron.uls 						/* Total files uploaded */
			,useron.dls);						/* Total files downloaded */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%u\n%s\nCOM%u\n%s\n%lu\n%s\n%s\n"
			,8									/* Data bits */
			,online==ON_LOCAL?"LOCAL":"REMOTE"  /* Online local or remote */
			,cfg.com_port						/* COMx port */
			,useron.birth						/* User birthday */
			,dte_rate							/* DTE rate */
			,"FALSE"                            /* Already connected? */
			,"Normal Connection");              /* Normal or ARQ connect */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		localtime_r(&now,&tm);
		sprintf(str,"%02d/%02d/%02d %02d:%02d\n%u\n%u\n"
			,tm.tm_mon+1,tm.tm_mday			/* Current date MM/DD/YY */
			,TM_YEAR(tm.tm_year)
			,tm.tm_hour,tm.tm_min 			/* Current time HH:MM */
			,cfg.node_num						/* Node number */
			,0);								/* Door number */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		close(file); 
	}

	else if(type==XTRN_PCBOARD) { /* PCBoard Files */
		strcpy(tmp,"PCBOARD.SYS");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
			return; 
		}

		sprintf(str,"%2d%2d%2d%2d%c%2d%c%c%5u%-5.5s"
			,-1 								/* Display on/off */
			,0									/* Printer on/off */
			,sys_status&SS_SYSPAGE ? -1:0		/* Page Bell on/off */
			,cfg.node_misc&NM_ANSALARM ? -1:0	/* Caller Alarm on/off */
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

		if(localtime_r(&logontime,&tm)==NULL)
			i=0;
		else
			i=(tm.tm_hour*60)+tm.tm_min;
		write(file,&i,2);						/* Logon time in min since mid */

		now=time(NULL);
		i=-(((now-starttime)/60)+useron.ttoday);/* Negative minutes used */
		write(file,&i,2);

		sprintf(str,"%02d:%02d",tm.tm_hour,tm.tm_min);
		write(file,str,5);

		i=cfg.level_timepercall[useron.level];	/* Time allowed on */
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

		write(file,&cfg.node_num,1);			/* Node number */

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
			,cfg.com_port						/* COM Port number */
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

		strcpy(tmp,"USERS.SYS");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
			return; 
		}

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
		localtime_r(&useron.laston,&tm);
		sprintf(str,"%02d:%02d",tm.tm_hour,tm.tm_min);
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
		c=(char)rows;
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
		i=cfg.expired_level;
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
		strcpy(tmp,"SFDOORS.DAT");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

		now=time(NULL);
		if(localtime_r(&now,&tm)==NULL)
			l=0;
		else
			l=((((long)tm.tm_hour*60L)+(long)tm.tm_min)*60L)
				+(long)tm.tm_sec;

		strcpy(tmp,name);
		if((p=strchr(tmp,SP))!=NULL)
			*p=0;

		sprintf(str,"%u\n%s\n%s\n%s\n%lu\n%u\n%lu\n%lu\n"
			,useron.number						/* User number */
			,name								/* User name */
			,useron.pass						/* Password */
			,tmp								/* User's first name */
			,dte_rate							/* DTE Rate */
			,cfg.com_port						/* COM Port */
			,tleft/60							/* Time left in minutes */
			,l									/* Seconds since midnight (now) */
			);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		if(localtime_r(&logontime,&tm)==NULL)
			l=0;
		else
			l=((((long)tm.tm_hour*60L)+(long)tm.tm_min)*60L)
				+(long)tm.tm_sec;

		sprintf(str,"%s\n%s\n%u\n%u\n%u\n%u\n%lu\n%lu\n%s\n"
			"%s\n%s\n%lu\n%s\n%u\n%u\n%u\n%u\n%u\n%lu\n%u\n"
			"%lu\n%lu\n%s\n%s\n"
			,dropdir
			,useron.misc&ANSI ? "TRUE":"FALSE"  /* ANSI ? True or False */
			,useron.level						/* Security level */
			,useron.uls 						/* Total uploads */
			,useron.dls 						/* Total downloads */
			,cfg.level_timepercall[useron.level]/* Minutes allowed this call */
			,l									/* Secs since midnight (logon) */
			,starttime-logontime				/* Extra time in seconds */
			,"FALSE"                            /* Sysop next? */
			,"FALSE"                            /* From Front-end? */
			,"FALSE"                            /* Software Flow Control? */
			,dte_rate							/* DTE Rate */
			,"FALSE"                            /* Error correcting connection? */
			,0									/* Current conference */
			,0									/* Current file dir */
			,cfg.node_num						/* Node number */
			,15 								/* Downloads allowed per day */
			,0									/* Downloads already this day */
			,100000L 							/* Download bytes allowed/day */
			,0									/* Downloaded bytes already today */
			,useron.ulb/1024L					/* Kbytes uploaded */
			,useron.dlb/1024L					/* Kbytes downloaded */
			,useron.phone						/* Phone Number */
			,useron.location					/* City, State */
			);
		lfexpand(str,misc);
		write(file,str,strlen(str));

		close(file); 
	}

	else if(type==XTRN_UTI) { /* UTI v2.1 - UTIDOOR.TXT */
		strcpy(tmp,"UTIDOOR.TXT");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

		strcpy(tmp,name);
		strupr(tmp);
		sprintf(str,"%s\n%lu\n%u\n%lu\n%lu\n"
			,tmp								/* User name */
			,cur_rate							/* Actual BPS rate */
			,online==ON_LOCAL ? 0: cfg.com_port /* COM Port */
			,dte_rate							/* DTE rate */
			,tleft);							/* Time left in sec */
		lfexpand(str,misc);
		write(file,str,strlen(str));

		close(file); 
	}

	else if(type==XTRN_SR) { /* Solar Realms DOORFILE.SR */
		strcpy(tmp,"DOORFILE.SR");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

		sprintf(str,"%s\n%d\n%d\n%lu\n%lu\n%u\n%lu\n"
			,name								/* Complete name of user */
			,useron.misc&ANSI ? 1:0 			/* ANSI ? */
			,useron.misc&NO_EXASCII ? 0:1		/* IBM characters ? */
			,rows								/* Page length */
			,dte_rate							/* Baud rate */
			,online==ON_LOCAL ? 0:cfg.com_port	/* COM port */
			,tleft/60							/* Time left (in minutes) */
			);
		lfexpand(str,misc);
		write(file,str,strlen(str));
		close(file); 
	}

	else if(type==XTRN_TRIBBS) { /* TRIBBS.SYS */
		strcpy(tmp,"TRIBBS.SYS");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

		sprintf(str,"%u\n%s\n%s\n%u\n%c\n%c\n%lu\n%s\n%s\n%s\n"
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
		lfexpand(str,misc);
		write(file,str,strlen(str));

		sprintf(str,"%u\n%u\n%lu\n%lu\n%c\n%c\n%s\n%s\n%s\n"
			,cfg.node_num						/* Node number */
			,cfg.com_port						/* Serial port */
			,online==ON_LOCAL ? 0L:cur_rate 	/* Baud rate */
			,dte_rate							/* Locked rate */
			,'Y'
			,'Y'                                /* Error correcting connection */
			,cfg.sys_name						/* Board's name */
			,cfg.sys_op 						/* Sysop's name */
			,useron.handle						/* User's alias */
			);
		lfexpand(str,misc);
		write(file,str,strlen(str));
		close(file); 
	}

	else if(type==XTRN_DOOR32) { /* DOOR32.SYS */
		strcpy(tmp,"DOOR32.SYS");
		if(misc&XTRN_LWRCASE)
			strlwr(tmp);
		sprintf(str,"%s%s",dropdir,tmp);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC|O_TEXT);
			return; 
		}

		sprintf(str,"%d\n%d\n38400\n%s%c\n%d\n%s\n%s\n%d\n%ld\n"
			"%d\n%d\n"
			,misc&IO_INTS ? 0 /* Local */ : 2 /* Telnet */
#if defined(__unix__)
			,misc&IO_INTS ? INVALID_SOCKET : client_socket
#else
			,misc&IO_INTS ? INVALID_SOCKET : client_socket_dup
#endif
			,VERSION_NOTICE,REVISION
			,useron.number
			,useron.name
			,name
			,useron.level
			,timeleft/60
			,useron.misc&ANSI ? 1 : 0
			,cfg.node_num);
		lfexpand(str,misc);
		write(file,str,strlen(str));
		close(file);
	}

	else if(type)
		errormsg(WHERE,ERR_CHK,"Drop file type",type);

}

/****************************************************************************/
/* Reads in MODUSER.DAT, EXITINFO.BBS, or DOOR.SYS and modify the current	*/
/* user's values.                                                           */
/****************************************************************************/
void sbbs_t::moduserdat(uint xtrnnum)
{
	char	str[256],path[256],c,startup[128];
	char 	tmp[512];
	uint	i;
	long	mod;
    int		file;
    FILE *	stream;

	sprintf(startup,"%s/",cfg.xtrn[xtrnnum]->path);
	if(cfg.xtrn[xtrnnum]->type==XTRN_RBBS || cfg.xtrn[xtrnnum]->type==XTRN_RBBS1) {
		sprintf(path,"%sEXITINFO.BBS"
			,cfg.xtrn[xtrnnum]->misc&STARTUPDIR ? startup : cfg.node_dir);
		fexistcase(path);
		if((file=nopen(path,O_RDONLY))!=-1) {
			lseek(file,361,SEEK_SET);
			read(file,&useron.flags1,4); /* Flags */
			putuserrec(&cfg,useron.number,U_FLAGS1,8,ultoa(useron.flags1,tmp,16));
			lseek(file,373,SEEK_SET);
			read(file,&i,2);			/* SecLvl */
			if(i<SYSOP_LEVEL) {
				useron.level=i;
				putuserrec(&cfg,useron.number,U_LEVEL,2,ultoa(useron.level,tmp,10)); 
			}
			close(file);
			remove(path); 
		}
		return; 
	}
	else if(cfg.xtrn[xtrnnum]->type==XTRN_GAP) {
		sprintf(path,"%sDOOR.SYS"
			,cfg.xtrn[xtrnnum]->misc&STARTUPDIR ? startup : cfg.node_dir);
		fexistcase(path);
		if((stream=fopen(path,"rb"))!=NULL) {
			for(i=0;i<15;i++)			/* skip first 14 lines */
				if(!fgets(str,128,stream))
					break;
			if(i==15 && isdigit(str[0])) {
				mod=atoi(str);
				if(mod<SYSOP_LEVEL) {
					useron.level=(char)mod;
					putuserrec(&cfg,useron.number,U_LEVEL,2,ultoa(useron.level,tmp,10)); 
				} 
			}

			for(;i<23;i++)
				if(!fgets(str,128,stream))
					break;
			if(i==23) { 					/* set main flags */
				useron.flags1=aftol(str);
				putuserrec(&cfg,useron.number,U_FLAGS1,8,ultoa(useron.flags1,tmp,16)); 
			}

			for(;i<25;i++)
				if(!fgets(str,128,stream))
					break;
			if(i==25 && isdigit(str[0]) && isdigit(str[1])
				&& (str[2]=='/' || str[2]=='-') /* xx/xx/xx or xx-xx-xx */
				&& isdigit(str[3]) && isdigit(str[4])
				&& (str[5]=='/' || str[5]=='-')
				&& isdigit(str[6]) && isdigit(str[7])) { /* valid expire date */
				useron.expire=dstrtounix(&cfg,str);
				putuserrec(&cfg,useron.number,U_EXPIRE,8,ultoa(useron.expire,tmp,16)); 
			}

			for(;i<29;i++)					/* line 29, total downloaded files */
				if(!fgets(str,128,stream))
					break;
			if(i==29) {
				truncsp(str);
				useron.dls=atoi(str);
				putuserrec(&cfg,useron.number,U_DLS,5,str); 
			}

			if(fgets(str,128,stream)) { 	/* line 30, Kbytes downloaded today */
				i++;
				truncsp(str);
				mod=atol(str)*1024L;
				if(mod) {
					useron.dlb=adjustuserrec(&cfg,useron.number,U_DLB,10,mod);
					subtract_cdt(&cfg,&useron,mod); 
				} 
			}

			for(;i<42;i++)
				if(!fgets(str,128,stream))
					break;
			if(i==42 && isdigit(str[0])) {	/* Time Credits in Minutes */
				useron.min=atol(str);
				putuserrec(&cfg,useron.number,U_MIN,10,ultoa(useron.min,tmp,10)); 
			}

			fclose(stream); 
		}
		return; 
	}

	else if(cfg.xtrn[xtrnnum]->type==XTRN_PCBOARD) {
		sprintf(path,"%sUSERS.SYS"
			,cfg.xtrn[xtrnnum]->misc&STARTUPDIR ? startup : cfg.node_dir);
		fexistcase(path);
		if((file=nopen(path,O_RDONLY))!=-1) {
			lseek(file,39,SEEK_SET);
			read(file,&c,1);
			if(c==1) {	 /* file has been updated */
				lseek(file,105,SEEK_CUR);	/* read security level */
				read(file,&i,2);
				if(i<SYSOP_LEVEL) {
					useron.level=i;
					putuserrec(&cfg,useron.number,U_LEVEL,2,ultoa(useron.level,tmp,10)); 
				}
				lseek(file,75,SEEK_CUR);	/* read in expiration date */
				read(file,&i,2);			/* convert from julian to unix */
				useron.expire=juliantounix(i);
				putuserrec(&cfg,useron.number,U_EXPIRE,8,ultoa(useron.expire,tmp,16)); 
			}
			close(file); 
		}
		return; 
	}

	sprintf(path,"%sMODUSER.DAT"
			,cfg.xtrn[xtrnnum]->misc&STARTUPDIR ? startup : cfg.node_dir);
	fexistcase(path);
	if((stream=fopen(path,"rb"))!=NULL) {			/* File exists */
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
				useron.cdt=adjustuserrec(&cfg,useron.number,U_CDT,10,mod);
			else
				subtract_cdt(&cfg,&useron,-mod); /* subtract from free cdt first */
		}
		if(fgets(str,81,stream)) {		/* main level */
			mod=atoi(str);
			if(isdigit(str[0]) && mod<SYSOP_LEVEL) {
				useron.level=(uchar)mod;
				putuserrec(&cfg,useron.number,U_LEVEL,2,ultoa(useron.level,tmp,10)); 
			} 
		}
		fgets(str,81,stream);		 /* was transfer level, now ignored */
		if(fgets(str,81,stream)) {		/* flags #1 */
			if(strchr(str,'-'))         /* remove flags */
				useron.flags1&=~aftol(str);
			else						/* add flags */
				useron.flags1|=aftol(str);
			putuserrec(&cfg,useron.number,U_FLAGS1,8,ultoa(useron.flags1,tmp,16)); 
		}

		if(fgets(str,81,stream)) {		/* flags #2 */
			if(strchr(str,'-'))         /* remove flags */
				useron.flags2&=~aftol(str);
			else						/* add flags */
				useron.flags2|=aftol(str);
			putuserrec(&cfg,useron.number,U_FLAGS2,8,ultoa(useron.flags2,tmp,16)); 
		}

		if(fgets(str,81,stream)) {		/* exemptions */
			if(strchr(str,'-'))
				useron.exempt&=~aftol(str);
			else
				useron.exempt|=aftol(str);
			putuserrec(&cfg,useron.number,U_EXEMPT,8,ultoa(useron.exempt,tmp,16)); 
		}
		if(fgets(str,81,stream)) {		/* restrictions */
			if(strchr(str,'-'))
				useron.rest&=~aftol(str);
			else
				useron.rest|=aftol(str);
			putuserrec(&cfg,useron.number,U_REST,8,ultoa(useron.rest,tmp,16)); 
		}
		if(fgets(str,81,stream)) {		/* Expiration date */
			if(isxdigit(str[0]))
				putuserrec(&cfg,useron.number,U_EXPIRE,8,str); 
		}
		if(fgets(str,81,stream)) {		/* additional minutes */
			mod=atol(str);
			if(mod) {
				sprintf(str,"Minute Adjustment: %s",ultoac(mod,tmp));
				logline("*+",str);
				useron.min=adjustuserrec(&cfg,useron.number,U_MIN,10,mod); 
			} 
		}
		if(fgets(str,81,stream)) {		/* flags #3 */
			if(strchr(str,'-'))         /* remove flags */
				useron.flags3&=~aftol(str);
			else						/* add flags */
				useron.flags3|=aftol(str);
			putuserrec(&cfg,useron.number,U_FLAGS3,8,ultoa(useron.flags3,tmp,16)); 
		}

		if(fgets(str,81,stream)) {		/* flags #4 */
			if(strchr(str,'-'))         /* remove flags */
				useron.flags4&=~aftol(str);
			else						/* add flags */
				useron.flags4|=aftol(str);
			putuserrec(&cfg,useron.number,U_FLAGS4,8,ultoa(useron.flags4,tmp,16)); 
		}

		if(fgets(str,81,stream)) {		/* flags #1 to REMOVE only */
			useron.flags1&=~aftol(str);
			putuserrec(&cfg,useron.number,U_FLAGS1,8,ultoa(useron.flags1,tmp,16)); 
		}
		if(fgets(str,81,stream)) {		/* flags #2 to REMOVE only */
			useron.flags2&=~aftol(str);
			putuserrec(&cfg,useron.number,U_FLAGS2,8,ultoa(useron.flags2,tmp,16)); 
		}
		if(fgets(str,81,stream)) {		/* flags #3 to REMOVE only */
			useron.flags3&=~aftol(str);
			putuserrec(&cfg,useron.number,U_FLAGS3,8,ultoa(useron.flags3,tmp,16)); 
		}
		if(fgets(str,81,stream)) {		/* flags #4 to REMOVE only */
			useron.flags4&=~aftol(str);
			putuserrec(&cfg,useron.number,U_FLAGS4,8,ultoa(useron.flags4,tmp,16)); 
		}
		if(fgets(str,81,stream)) {		/* exemptions to remove */
			useron.exempt&=~aftol(str);
			putuserrec(&cfg,useron.number,U_EXEMPT,8,ultoa(useron.exempt,tmp,16)); 
		}
		if(fgets(str,81,stream)) {		/* restrictions to remove */
			useron.rest&=~aftol(str);
			putuserrec(&cfg,useron.number,U_REST,8,ultoa(useron.rest,tmp,16)); 
		}

		fclose(stream);
		remove(path); 
	}
}


/****************************************************************************/
/* This function handles configured external program execution. 			*/
/****************************************************************************/
bool sbbs_t::exec_xtrn(uint xtrnnum)
{
	char str[256],path[MAX_PATH+1],dropdir[MAX_PATH+1],name[32],c;
    int file;
	uint i;
	long tleft,mode;
    node_t node;
	time_t start,end;

	if(!chk_ar(cfg.xtrn[xtrnnum]->run_ar,&useron)
		|| !chk_ar(cfg.xtrnsec[cfg.xtrn[xtrnnum]->sec]->ar,&useron)) {
		bputs(text[CantRunThatProgram]);
		return(false); 
	}

	if(cfg.xtrn[xtrnnum]->cost && !(useron.exempt&FLAG('X'))) {    /* costs */
		if(cfg.xtrn[xtrnnum]->cost>useron.cdt+useron.freecdt) {
			bputs(text[NotEnoughCredits]);
			pause();
			return(false); 
		}
		subtract_cdt(&cfg,&useron,cfg.xtrn[xtrnnum]->cost); 
	}

	if(!(cfg.xtrn[xtrnnum]->misc&MULTIUSER)) {
		for(i=1;i<=cfg.sys_nodes;i++) {
			getnodedat(i,&node,0);
			c=i;
			if((node.status==NODE_INUSE || node.status==NODE_QUIET)
				&& node.action==NODE_XTRN && node.aux==(xtrnnum+1)) {
				if(node.status==NODE_QUIET) {
					strcpy(str,cfg.sys_guru);
					c=cfg.sys_nodes+1; 
				}
				else if(node.misc&NODE_ANON)
					strcpy(str,"UNKNOWN USER");
				else
					username(&cfg,node.useron,str);
				bprintf(text[UserRunningXtrn],str
					,cfg.xtrn[xtrnnum]->name,c);
				pause();
				break; 
			} 
		}
		if(i<=cfg.sys_nodes)
			return(false); 
	}

	sprintf(str,"%s/",cfg.xtrn[xtrnnum]->path);
	strcpy(path,cfg.xtrn[xtrnnum]->misc&STARTUPDIR ? str : cfg.node_dir);
	strcpy(dropdir,cfg.xtrn[xtrnnum]->misc&STARTUPDIR ? str : cfg.node_dir);

	switch(cfg.xtrn[xtrnnum]->type) {
		case XTRN_WWIV:
			strcpy(name,"CHAIN.TXT");
			break;
		case XTRN_GAP:
			strcpy(name,"DOOR.SYS");
			break;
		case XTRN_RBBS:
			sprintf(str,"DORINFO%X.DEF",cfg.node_num);
			strcpy(name,str);
			break;
		case XTRN_RBBS1:
			strcpy(name,"DORINFO1.DEF");
			break;
		case XTRN_WILDCAT:
			strcpy(name,"CALLINFO.BBS");
			break;
		case XTRN_PCBOARD:
			strcpy(name,"PCBOARD.SYS");
			break;
		case XTRN_UTI:
			strcpy(name,"UTIDOOR.TXT");
			break;
		case XTRN_SR:
			strcpy(name,"DOORFILE.SR");
			break;
		case XTRN_TRIBBS:
			strcpy(name,"TRIBBS.SYS");
			break;
		case XTRN_DOOR32:
			strcpy(name,"DOOR32.SYS");
			break;
		default:
			strcpy(name,"XTRN.DAT");
			break; 
	}
	if(cfg.xtrn[xtrnnum]->misc&XTRN_LWRCASE)
		strlwr(name);
	strcat(path,name);
	getnodedat(cfg.node_num,&thisnode,1);
	thisnode.aux=xtrnnum+1;
	thisnode.action=NODE_XTRN;
	putnodedat(cfg.node_num,&thisnode);
	putuserrec(&cfg,useron.number,U_CURXTRN,8,cfg.xtrn[xtrnnum]->code);

	if(cfg.xtrn[xtrnnum]->misc&REALNAME)
		strcpy(name,useron.name);
	else
		strcpy(name,useron.alias);

	gettimeleft();
	tleft=timeleft+(cfg.xtrn[xtrnnum]->textra*60);
	if(cfg.xtrn[xtrnnum]->maxtime && tleft>cfg.xtrn[xtrnnum]->maxtime)
		tleft=(cfg.xtrn[xtrnnum]->maxtime*60);
	xtrndat(name,dropdir,cfg.xtrn[xtrnnum]->type,tleft,cfg.xtrn[xtrnnum]->misc);
	if(!online)
		return(false);
	sprintf(str,"%s running external program: %s"
		,useron.alias,cfg.xtrn[xtrnnum]->name);
	logline("X-",str);
	if(cfg.xtrn[xtrnnum]->cmd[0]!='*' && logfile_fp!=NULL) {
		fclose(logfile_fp);
		logfile_fp=NULL;
	}

	sprintf(str,"%sINTRSBBS.DAT"
			,cfg.xtrn[xtrnnum]->path[0] ? cfg.xtrn[xtrnnum]->path : cfg.node_dir);
	if(fexistcase(str))
		remove(str);
	sprintf(str,"%shangup.now",cfg.node_dir);
	if(fexistcase(str))
		remove(str);
	sprintf(str,"%sfile/%04u.dwn",cfg.data_dir,useron.number);
	if(fexistcase(str))
		remove(str);

	mode=0; 	
	if(cfg.xtrn[xtrnnum]->misc&XTRN_SH)
		mode|=EX_SH;
	if(cfg.xtrn[xtrnnum]->misc&IO_INTS)
		mode|=(EX_OUTR|EX_INR|EX_OUTL);
	if(cfg.xtrn[xtrnnum]->misc&WWIVCOLOR)
		mode|=EX_WWIV;
	if(cfg.xtrn[xtrnnum]->misc&SWAP)
		mode|=EX_SWAP;
	if(cfg.xtrn[xtrnnum]->misc&XTRN_NATIVE)
		mode|=EX_NATIVE;
	if(cfg.xtrn[xtrnnum]->misc&MODUSERDAT) {	 /* Delete MODUSER.DAT */
		sprintf(str,"%sMODUSER.DAT",dropdir);       /* if for some weird  */
		remove(str); 								/* reason it's there  */
	}

	start=time(NULL);
	external(cmdstr(cfg.xtrn[xtrnnum]->cmd,path,dropdir,NULL),mode
		,cfg.xtrn[xtrnnum]->path);
	end=time(NULL);
	if(cfg.xtrn[xtrnnum]->misc&FREETIME)
		starttime+=end-start;
	if(cfg.xtrn[xtrnnum]->clean[0]) {
		external(cmdstr(cfg.xtrn[xtrnnum]->clean,path,nulstr,NULL)
			,mode&~EX_INR, cfg.xtrn[xtrnnum]->path); 
	}
	/* Re-open the logfile */
	if(logfile_fp==NULL) {
		sprintf(str,"%snode.log",cfg.node_dir);
		if((logfile_fp=fopen(str,"a+b"))==NULL)
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
	}

	sprintf(str,"%sfile/%04u.dwn",cfg.data_dir,useron.number);
	batch_add_list(str);

	sprintf(str,"%shangup.now",cfg.node_dir);
	if(fexistcase(str)) {
		remove(str);
		hangup(); 
	}
	if(online==ON_REMOTE) {
		checkline();
		if(!online) {
			sprintf(str,"%shungup.log",cfg.data_dir);
			if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
				errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
				return(false); 
			}
			getnodedat(cfg.node_num,&thisnode,0);
			now=time(NULL);
			sprintf(str,hungupstr,useron.alias,cfg.xtrn[thisnode.aux-1]->name
				,timestr(&now));
			write(file,str,strlen(str));
			close(file); 
		} 
	}
	if(cfg.xtrn[xtrnnum]->misc&MODUSERDAT) 	/* Modify user data */
		moduserdat(xtrnnum);

	getnodedat(cfg.node_num,&thisnode,1);
	thisnode.aux=0;
	putnodedat(cfg.node_num,&thisnode);

	return(true);
}

/****************************************************************************/
/* This function will execute an external program if it is configured to    */
/* run during the event specified.                                          */
/****************************************************************************/
bool sbbs_t::user_event(user_event_t event)
{
    uint	i;
	bool	success=false;

	for(i=0;i<cfg.total_xtrns;i++) {
		if(cfg.xtrn[i]->event!=event)
			continue;
		if(!chk_ar(cfg.xtrn[i]->ar,&useron)
			|| !chk_ar(cfg.xtrnsec[cfg.xtrn[i]->sec]->ar,&useron))
			continue;
		success=exec_xtrn(i); 
	}

	return(success);
}


