/* Synchronet (oh, so old) data access routines */

/* $Id: data.cpp,v 1.32 2020/04/27 07:42:23 rswindell Exp $ */

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

/**************************************************************/
/* Functions that store and retrieve data from disk or memory */
/**************************************************************/

#include "sbbs.h"

/****************************************************************************/
/* Looks for close or perfect matches between str and valid usernames and  	*/
/* numbers and prompts user for near perfect matches in names.				*/
/* Returns the number of the matched user or 0 if unsuccessful				*/
/* Called from functions main_sec, useredit and readmailw					*/
/****************************************************************************/
uint sbbs_t::finduser(char *instr, bool silent_failure)
{
	int file,i;
	char str[128],str2[256],str3[256],ynq[25],c,pass=1;
	long l,length;
	FILE *stream;

	i=atoi(instr);
	if(i>0) {
		username(&cfg, i,str2);
		if(str2[0] && strcmp(str2,"DELETED USER"))
			return(i); 
	}
	strcpy(str,instr);
	strupr(str);
	SAFEPRINTF(str3,"%suser/name.dat",cfg.data_dir);
	if(flength(str3)<1L)
		return(0);
	if((stream=fnopen(&file,str3,O_RDONLY))==NULL) {
		errormsg(WHERE,ERR_OPEN,str3,O_RDONLY);
		return(0); 
	}
	SAFEPRINTF(ynq,"%.3s",text[YNQP]);
	length=(long)filelength(file);
	while(pass<3) {
		fseek(stream,0L,SEEK_SET);	/* seek to beginning for each pass */
		for(l=0;l<length;l+=LEN_ALIAS+2) {
			if(!online) break;
			fread(str2,LEN_ALIAS+2,1,stream);
			for(c=0;c<LEN_ALIAS;c++)
				if(str2[c]==ETX) break;
			str2[c]=0;
			if(!c)		/* deleted user */
				continue;
			strcpy(str3,str2);
			strupr(str2);
			if(pass==1 && !strcmp(str,str2)) {
				fclose(stream);
				return((l/(LEN_ALIAS+2))+1); 
			}
			if(pass==2 && strstr(str2,str)) {
				bprintf(text[DoYouMeanThisUserQ],str3
					,(uint)(l/(LEN_ALIAS+2))+1);
				c=(char)getkeys(ynq,0);
				if(sys_status&SS_ABORT) {
					fclose(stream);
					return(0); 
				}
				if(c==text[YNQP][0]) {
					fclose(stream);
					return((l/(LEN_ALIAS+2))+1); 
				}
				if(c==text[YNQP][2]) {
					fclose(stream);
					return(0); 
				} 
			} 
		}
		pass++; 
	}
	if(!silent_failure)
		bputs(text[UnknownUser]);
	fclose(stream);
	return(0);
}

/****************************************************************************/
/* Returns the number of user transfers in XFER.IXT for either a dest user  */
/* source user, or filename.												*/
/****************************************************************************/
int sbbs_t::getuserxfers(int fromuser, int destuser, char *fname)
{
	char str[256];
	int file,found=0;
	FILE *stream;

	SAFEPRINTF(str,"%sxfer.ixt",cfg.data_dir);
	if(!fexist(str))
		return(0);
	if(!flength(str)) {
		remove(str);
		return(0); 
	}
	if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		return(0); 
	}
	while(!ferror(stream)) {
		if(!fgets(str,81,stream))
			break;
		str[22]=0;
		if(fname!=NULL && fname[0] && !strncmp(str+5,fname,12))
				found++;
		else if(fromuser && atoi(str+18)==fromuser)
				found++;
		else if(destuser && atoi(str)==destuser)
				found++; 
	}
	fclose(stream);
	return(found);
}

/****************************************************************************/
/* Return time of next forced timed event									*/
/* 'event' may be NULL														*/
/****************************************************************************/
extern "C" time_t DLLCALL getnextevent(scfg_t* cfg, event_t* event)
{
    int     i;
	time_t	now=time(NULL);
	time_t	event_time=0;
	time_t	thisevent;
	time_t	tmptime;
    struct  tm tm, last_tm;

	if(localtime_r(&now,&tm) == NULL)
		return 0;

	for(i=0;i<cfg->total_events;i++) {
		if(!cfg->event[i]->node || cfg->event[i]->node>cfg->sys_nodes
			|| cfg->event[i]->misc&EVENT_DISABLED)
			continue;
		if(!(cfg->event[i]->misc&EVENT_FORCE)
			|| (!(cfg->event[i]->misc&EVENT_EXCL) && cfg->event[i]->node!=cfg->node_num)
			|| !(cfg->event[i]->days&(1<<tm.tm_wday))
			|| (cfg->event[i]->mdays!=0 && !(cfg->event[i]->mdays&(1<<tm.tm_mday)))
			|| (cfg->event[i]->months!=0 && !(cfg->event[i]->months&(1<<tm.tm_mon)))) 
			continue;

		tm.tm_hour=cfg->event[i]->time/60;
		tm.tm_min=cfg->event[i]->time%60;
		tm.tm_sec=0;
		tm.tm_isdst=-1;	/* Do not adjust for DST */
		thisevent=mktime(&tm);
		if(thisevent == -1)
			continue;

		tmptime=cfg->event[i]->last;
		if(localtime_r(&tmptime,&last_tm) == NULL)
			memset(&last_tm,0,sizeof(last_tm));

		if(tm.tm_mday==last_tm.tm_mday && tm.tm_mon==last_tm.tm_mon)
			thisevent+=24L*60L*60L;     /* already ran today, so add 24hrs */
		if(!event_time || thisevent<event_time) {
			event_time=thisevent;
			if(event!=NULL)
				*event=*cfg->event[i];
		}
	}

	return event_time;
}

/****************************************************************************/
/* Fills the timeleft variable with the correct value. Hangs up on the      */
/* user if their time is up.                                                */
/* Called from functions main_sec and xfer_sec                              */
/****************************************************************************/
ulong sbbs_t::gettimeleft(bool handle_out_of_time)
{
    char    str[128];
	char 	tmp[512];
	event_t	nextevent;

	now=time(NULL);

	timeleft = (ulong)::gettimeleft(&cfg, &useron, starttime);

	/* Timed event time reduction handler */
	event_time=getnextevent(&cfg, &nextevent);
	if(event_time)
		event_code=nextevent.code;

	if(event_time && now+(time_t)timeleft>event_time) {    /* less time, set flag */
		if(event_time<now)
			timeleft=0;
		else
			timeleft=(ulong)(event_time-now); 
		if(!(sys_status&SS_EVENT)) {
			lprintf(LOG_NOTICE,"Node %d Time reduced (to %s) due to upcoming event (%s) on %s"
				,cfg.node_num,sectostr(timeleft,tmp),event_code,timestr(event_time));
			sys_status|=SS_EVENT;
		}
	}

	if((long)timeleft<0)  /* timeleft can't go negative */
		timeleft=0;
	if(thisnode.status==NODE_NEWUSER) {
		timeleft=cfg.level_timepercall[cfg.new_level];
		if(timeleft<10*60L)
			timeleft=10*60L; 
	}

	if(handle_out_of_time && !gettimeleft_inside)			/* The following code is not recursive */
	{
		gettimeleft_inside=1;

		if(!timeleft && !SYSOP && !(sys_status&SS_LCHAT)) {
			logline(LOG_NOTICE,nulstr,"Ran out of time");
			SAVELINE;
			if(sys_status&SS_EVENT)
				bprintf(text[ReducedTime],timestr(event_time));
			bputs(text[TimesUp]);
			if(!(sys_status&(SS_EVENT|SS_USERON)) && useron.cdt>=100L*1024L
				&& !(cfg.sys_misc&SM_NOCDTCVT)) {
				SAFEPRINTF(tmp,text[Convert100ktoNminQ],cfg.cdt_min_value);
				if(yesno(tmp)) {
					logline("  ","Credit to Minute Conversion");
					useron.min=adjustuserrec(&cfg,useron.number,U_MIN,10,cfg.cdt_min_value);
					useron.cdt=adjustuserrec(&cfg,useron.number,U_CDT,10,-(102400L));
					SAFEPRINTF(str,"Credit Adjustment: %ld",-(102400L));
					logline("$-",str);
					SAFEPRINTF(str,"Minute Adjustment: %u",cfg.cdt_min_value);
					logline("*+",str);
					RESTORELINE;
					gettimeleft();
					gettimeleft_inside=0;
					return timeleft; 
				} 
			}
			if(cfg.sys_misc&SM_TIME_EXP && !(sys_status&SS_EVENT)
				&& !(useron.exempt&FLAG('E'))) {
												/* set to expired values */
				bputs(text[AccountHasExpired]);
				SAFEPRINTF2(str,"%s #%u Expired",useron.alias,useron.number);
				logentry("!%",str);
				if(cfg.level_misc[useron.level]&LEVEL_EXPTOVAL
					&& cfg.level_expireto[useron.level]<10) {
					useron.flags1=cfg.val_flags1[cfg.level_expireto[useron.level]];
					useron.flags2=cfg.val_flags2[cfg.level_expireto[useron.level]];
					useron.flags3=cfg.val_flags3[cfg.level_expireto[useron.level]];
					useron.flags4=cfg.val_flags4[cfg.level_expireto[useron.level]];
					useron.exempt=cfg.val_exempt[cfg.level_expireto[useron.level]];
					useron.rest=cfg.val_rest[cfg.level_expireto[useron.level]];
					if(cfg.val_expire[cfg.level_expireto[useron.level]])
						useron.expire=(time32_t)now
							+(cfg.val_expire[cfg.level_expireto[useron.level]]*24*60*60);
					else
						useron.expire=0;
					useron.level=cfg.val_level[cfg.level_expireto[useron.level]]; 
				}
				else {
					if(cfg.level_misc[useron.level]&LEVEL_EXPTOLVL)
						useron.level=cfg.level_expireto[useron.level];
					else
						useron.level=cfg.expired_level;
					useron.flags1&=~cfg.expired_flags1; /* expired status */
					useron.flags2&=~cfg.expired_flags2; /* expired status */
					useron.flags3&=~cfg.expired_flags3; /* expired status */
					useron.flags4&=~cfg.expired_flags4; /* expired status */
					useron.exempt&=~cfg.expired_exempt;
					useron.rest|=cfg.expired_rest;
					useron.expire=0; 
				}
				putuserrec(&cfg,useron.number,U_LEVEL,2,ultoa(useron.level,str,10));
				putuserrec(&cfg,useron.number,U_FLAGS1,8,ultoa(useron.flags1,str,16));
				putuserrec(&cfg,useron.number,U_FLAGS2,8,ultoa(useron.flags2,str,16));
				putuserrec(&cfg,useron.number,U_FLAGS3,8,ultoa(useron.flags3,str,16));
				putuserrec(&cfg,useron.number,U_FLAGS4,8,ultoa(useron.flags4,str,16));
				putuserrec(&cfg,useron.number,U_EXPIRE,8,ultoa((ulong)useron.expire,str,16));
				putuserrec(&cfg,useron.number,U_EXEMPT,8,ultoa(useron.exempt,str,16));
				putuserrec(&cfg,useron.number,U_REST,8,ultoa(useron.rest,str,16));
				if(cfg.expire_mod[0])
					exec_bin(cfg.expire_mod,&main_csi);
				RESTORELINE;
				gettimeleft();
				gettimeleft_inside=0;
				return timeleft; 
			}
			SYNC;
			hangup(); 
		}
		gettimeleft_inside=0;
	}
	return timeleft;
}
