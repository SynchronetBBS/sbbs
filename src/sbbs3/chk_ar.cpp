/* chk_ar.cpp */

/* Synchronet ARS checking routine */

/* $Id: chk_ar.cpp,v 1.32 2020/03/19 18:50:51 rswindell Exp $ */

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

#include "sbbs.h"

bool sbbs_t::ar_exp(const uchar **ptrptr, user_t* user, client_t* client)
{
	bool	result,_not,_or,equal;
	uint	i,n,artype,age;
	ulong	l;
	struct tm tm;
	const char*	p;

	result = true;

	for(;(**ptrptr);(*ptrptr)++) {

		if((**ptrptr)==AR_ENDNEST)
			break;

		_not=_or=equal = false;

		if((**ptrptr)==AR_OR) {
			_or=true;
			(*ptrptr)++; 
		}
		
		if((**ptrptr)==AR_NOT) {
			_not=true;
			(*ptrptr)++; 
		}

		if((**ptrptr)==AR_EQUAL) {
			equal=true;
			(*ptrptr)++; 
		}

		if((result && _or) || (!result && !_or))
			break;

		if((**ptrptr)==AR_BEGNEST) {
			(*ptrptr)++;
			if(ar_exp(ptrptr,user,client))
				result=!_not;
			else
				result=_not;
			while((**ptrptr)!=AR_ENDNEST && (**ptrptr)) /* in case of early exit */
				(*ptrptr)++;
			if(!(**ptrptr))
				break;
			continue; 
		}

		artype=(**ptrptr);
		switch(artype) {
			case AR_ANSI:				/* No arguments */
			case AR_PETSCII:
			case AR_ASCII:
			case AR_UTF8:
			case AR_CP437:
			case AR_RIP:
			case AR_WIP:
			case AR_LOCAL:
			case AR_EXPERT:
			case AR_SYSOP:
			case AR_GUEST:
			case AR_QNODE:
			case AR_QUIET:
			case AR_OS2:
			case AR_DOS:
			case AR_WIN32:
			case AR_UNIX:
			case AR_LINUX:
			case AR_ACTIVE:
			case AR_INACTIVE:
			case AR_DELETED:
				break;
			default:
				(*ptrptr)++;
				break; 
		}

		n=(**ptrptr);
		i=(*(short *)*ptrptr);
		switch(artype) {
			case AR_LEVEL:
				if((equal && user->level!=n) || (!equal && user->level<n))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessLevel];
					noaccess_val=n; 
				}
				break;
			case AR_AGE:
				age=getage(&cfg,user->birth);
				if((equal && age!=n) || (!equal && age<n))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessAge];
					noaccess_val=n; 
				}
				break;
			case AR_BPS:
				if((equal && cur_rate!=i) || (!equal && cur_rate<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessBPS];
					noaccess_val=i; 
				}
				break;
			case AR_ANSI:
				if(!term_supports(ANSI))
					result=_not;
				else result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=ANSI; 
				}
				break;
			case AR_PETSCII:
				if((term_supports()&CHARSET_FLAGS) != CHARSET_PETSCII)
					result=_not;
				else result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=PETSCII; 
				}
				break;
			case AR_ASCII:
				if((term_supports()&CHARSET_FLAGS) != CHARSET_ASCII)
					result=_not;
				else result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=NO_EXASCII; 
				}
				break;
			case AR_UTF8:
				if((term_supports()&CHARSET_FLAGS) != CHARSET_UTF8)
					result=_not;
				else result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=UTF8; 
				}
				break;
			case AR_CP437:
				if((term_supports()&CHARSET_FLAGS) != CHARSET_CP437)
					result=_not;
				else result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=0; 
				}
				break;
			case AR_RIP:
				if(!term_supports(RIP))
					result=_not;
				else result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=RIP; 
				}
				break;
			case AR_WIP:
				if(!term_supports(WIP))
					result=_not;
				else result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=WIP; 
				}
				break;
			case AR_OS2:
				#ifndef __OS2__
					result=_not;
				#else
					result=!_not;
				#endif
				break;
			case AR_DOS:	/* DOS program support */
				result=_not;
				if(startup->options&BBS_OPT_NO_DOS)
					break;
				#if defined(_WIN32) || (defined(__linux__) && defined(USE_DOSEMU)) || defined(__FreeBSD__)
					result=!_not;
				#endif
				break;
			case AR_WIN32:
				#ifndef _WIN32
					result=_not;
				#else
					result=!_not;
				#endif
				break;
			case AR_UNIX:
				#ifndef __unix__
					result=_not;
				#else
					result=!_not;
				#endif
				break;
			case AR_LINUX:
				#ifndef __linux__
					result=_not;
				#else
					result=!_not;
				#endif
				break;
			case AR_ACTIVE:
				if(user->misc&(DELETED|INACTIVE))
					result=_not;
				else result=!_not;
				break;
			case AR_INACTIVE:
				if(!(user->misc&INACTIVE))
					result=_not;
				else result=!_not;
				break;
			case AR_DELETED:
				if(!(user->misc&DELETED))
					result=_not;
				else result=!_not;
				break;
			case AR_EXPERT:
				if(!(user->misc&EXPERT))
					result=_not;
				else result=!_not;
				break;
			case AR_SYSOP:
				if(!SYSOP)
					result=_not;
				else result=!_not;
				break;
			case AR_GUEST:
				if(!(user->rest&FLAG('G')))
					result=_not;
				else result=!_not;
				break;
			case AR_QNODE:
				if(!(user->rest&FLAG('Q')))
					result=_not;
				else result=!_not;
				break;
			case AR_QUIET:
				if(thisnode.status!=NODE_QUIET)
					result=_not;
				else result=!_not;
				break;
			case AR_LOCAL:
				if(online!=ON_LOCAL)
					result=_not;
				else result=!_not;
				break;
			case AR_DAY:
				now=time(NULL);
				localtime_r(&now,&tm);
				if((equal && tm.tm_wday!=(int)n) 
					|| (!equal && tm.tm_wday<(int)n))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessDay];
					noaccess_val=n; 
				}
				break;
			case AR_CREDIT:
				l=(ulong)i*1024UL;
				if((equal && user->cdt+user->freecdt!=l)
					|| (!equal && user->cdt+user->freecdt<l))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessCredit];
					noaccess_val=l; 
				}
				break;
			case AR_NODE:
				if((equal && cfg.node_num!=n) || (!equal && cfg.node_num<n))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessNode];
					noaccess_val=n; 
				}
				break;
			case AR_USER:
				if((equal && user->number!=i) || (!equal && user->number<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessUser];
					noaccess_val=i; 
				}
				break;
			case AR_GROUP:
				if((equal
					&& (cursubnum>=cfg.total_subs
						|| cfg.sub[cursubnum]->grp!=i))
					|| (!equal && cursubnum<cfg.total_subs
						&& cfg.sub[cursubnum]->grp<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessGroup];
					noaccess_val=i+1; 
				}
				break;
			case AR_SUB:
				if((equal && cursubnum!=i) || (!equal && cursubnum<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessSub];
					noaccess_val=i+1; 
				}
				break;
			case AR_SUBCODE:
				if(cursubnum>=cfg.total_subs
					|| !findstr_in_string(cfg.sub[cursubnum]->code,(char*)*ptrptr))
					result=_not;
				else
					result=!_not;
				while(*(*ptrptr))
					(*ptrptr)++;
				if(!result)
					noaccess_str=text[NoAccessSub];
				break;
			case AR_LIB:
				if((equal
					&& (curdirnum>=cfg.total_dirs
						|| cfg.dir[curdirnum]->lib!=i))
					|| (!equal && curdirnum<cfg.total_dirs
						&& cfg.dir[curdirnum]->lib<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessLib];
					noaccess_val=i+1; 
				}
				break;
			case AR_DIR:
				if((equal && curdirnum!=i) || (!equal && curdirnum<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessDir];
					noaccess_val=i+1; 
				}
				break;
			case AR_DIRCODE:
				if(curdirnum>=cfg.total_dirs
					|| !findstr_in_string(cfg.dir[curdirnum]->code,(char *)*ptrptr))
					result=_not;
				else
					result=!_not;
				while(*(*ptrptr))
					(*ptrptr)++;
				if(!result)
					noaccess_str=text[NoAccessSub];
				break;
			case AR_EXPIRE:
				now=time(NULL);
				if(!user->expire || now+((long)i*24L*60L*60L)>user->expire)
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessExpire];
					noaccess_val=i; 
				}
				break;
			case AR_RANDOM:
				n=sbbs_random(i+1);
				if((equal && n!=i) || (!equal && n<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_LASTON:
				now=time(NULL);
				if((now-user->laston)/(24L*60L*60L)<(long)i)
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_LOGONS:
				if((equal && user->logons!=i) || (!equal && user->logons<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_MAIN_CMDS:
				if((equal && main_cmds!=i) || (!equal && main_cmds<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_FILE_CMDS:
				if((equal && xfer_cmds!=i) || (!equal && xfer_cmds<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_TLEFT:
				if(timeleft/60<(ulong)n)
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTimeLeft];
					noaccess_val=n; 
				}
				break;
			case AR_TUSED:
				if((time(NULL)-logontime)/60<(long)n)
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTimeUsed];
					noaccess_val=n; 
				}
				break;
			case AR_TIME:
				now=time(NULL);
				localtime_r(&now,&tm);
				if((tm.tm_hour*60)+tm.tm_min<(int)i)
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessTime];
					noaccess_val=i; 
				}
				break;
			case AR_PCR:	/* post/call ratio (by percentage) */
				if(user->logons>user->posts
					&& (!user->posts || (100/((float)user->logons/user->posts))<(long)n))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessPCR];
					noaccess_val=n; 
				}
				break;
			case AR_UDR:	/* up/download byte ratio (by percentage) */
				l=user->dlb;
				if(!l) l=1;
				if(user->dlb>user->ulb
					&& (!user->ulb || (100/((float)l/user->ulb))<n))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessUDR];
					noaccess_val=n; 
				}
				break;
			case AR_UDFR:	/* up/download file ratio (in percentage) */
				i=user->dls;
				if(!i) i=1;
				if(user->dls>user->uls
					&& (!user->uls || (100/((float)i/user->uls))<n))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessUDFR];
					noaccess_val=n; 
				}
				break;
			case AR_ULS:
				if((equal && user->uls!=i) || (!equal && user->uls<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_ULK:
				if((equal && (user->ulb/1024)!=i) || (!equal && (user->ulb/1024)<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_ULM:
				if((equal && (user->ulb/(1024*1024))!=i) || (!equal && (user->ulb/(1024*1024))<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_DLS:
				if((equal && user->dls!=i) || (!equal && user->dls<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_DLK:
				if((equal && user->dlb/1024!=i) || (!equal && user->dlb/1024<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_DLM:
				if((equal && user->dlb/(1024*1024)!=i) || (!equal && user->dlb/(1024*1024)<i))
					result=_not;
				else
					result=!_not;
				(*ptrptr)++;
				break;
			case AR_FLAG1:
				if((!equal && !(user->flags1&FLAG(n)))
					|| (equal && user->flags1!=FLAG(n)))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessFlag1];
					noaccess_val=n; 
				}
				break;
			case AR_FLAG2:
				if((!equal && !(user->flags2&FLAG(n)))
					|| (equal && user->flags2!=FLAG(n)))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessFlag2];
					noaccess_val=n; 
				}
				break;
			case AR_FLAG3:
				if((!equal && !(user->flags3&FLAG(n)))
					|| (equal && user->flags3!=FLAG(n)))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessFlag3];
					noaccess_val=n; 
				}
				break;
			case AR_FLAG4:
				if((!equal && !(user->flags4&FLAG(n)))
					|| (equal && user->flags4!=FLAG(n)))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessFlag4];
					noaccess_val=n; 
				}
				break;
			case AR_REST:
				if((!equal && !(user->rest&FLAG(n)))
					|| (equal && user->rest!=FLAG(n)))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessRest];
					noaccess_val=n; 
				}
				break;
			case AR_EXEMPT:
				if((!equal && !(user->exempt&FLAG(n)))
					|| (equal && user->exempt!=FLAG(n)))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessExempt];
					noaccess_val=n; 
				}
				break;
			case AR_SEX:
				if(user->sex!=n)
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessSex];
					noaccess_val=n; 
				}
				break; 
			case AR_SHELL:
				if(user->shell>=cfg.total_shells
					|| !findstr_in_string(cfg.shell[user->shell]->code,(char*)*ptrptr))
					result=_not;
				else
					result=!_not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_PROT:
				if(client!=NULL)
					p=client->protocol;
				else
					p=user->modem;
				if(!findstr_in_string(p,(char*)*ptrptr))
					result=_not;
				else
					result=!_not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_HOST:
				if(client!=NULL)
					p=client->host;
				else
					p=user->comp;
				if(!findstr_in_string(p,(char*)*ptrptr))
					result=_not;
				else
					result=!_not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_IP:
				if(client!=NULL)
					p=client->addr;
				else
					p=user->ipaddr;
				if(!findstr_in_string(p,(char*)*ptrptr))
					result=_not;
				else
					result=!_not;
				while(*(*ptrptr))
					(*ptrptr)++;
				break;
			case AR_TERM:
				if(!findstr_in_string(terminal, (char*)*ptrptr))
					result=_not;
				else
					result=!_not;
				while(*(*ptrptr))
					(*ptrptr)++;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=0; 
				}
				break;
			case AR_COLS:
				if((equal && cols != (long)n) || (!equal && cols < (long)n))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=n; 
				}
				break;
			case AR_ROWS:
				if((equal && rows != (long)n) || (!equal && rows < (long)n))
					result=_not;
				else
					result=!_not;
				if(!result) {
					noaccess_str=text[NoAccessTerminal];
					noaccess_val=n; 
				}
				break;
		}
	}
	return(result);
}

bool sbbs_t::chk_ar(const uchar *ar, user_t* user, client_t* client)
{
	const uchar *p;

	if(ar==NULL)
		return(true);
	p=ar;
	return(ar_exp(&p,user,client));
}


/****************************************************************************/
/* This function fills the usrsub, usrsubs, usrgrps, curgrp, and cursub     */
/* variables based on the security clearance of the current user (useron)   */
/****************************************************************************/
void sbbs_t::getusrsubs()
{
    uint i,j,k,l;

	for(j=0,i=0;i<cfg.total_grps;i++) {
		if(!chk_ar(cfg.grp[i]->ar,&useron,&client))
			continue;
		for(k=0,l=0;l<cfg.total_subs;l++) {
			if(cfg.sub[l]->grp!=i) continue;
			if(!chk_ar(cfg.sub[l]->ar,&useron,&client))
				continue;
			usrsub[j][k++]=l; 
		}
		usrsubs[j]=k;
		if(!k)          /* No subs accessible in group */
			continue;
		usrgrp[j++]=i; 
	}
	usrgrps=j;
	if(usrgrps==0)
		return;
	while((curgrp>=usrgrps || !usrsubs[curgrp]) && curgrp) curgrp--;
	while(cursub[curgrp]>=usrsubs[curgrp] && cursub[curgrp]) cursub[curgrp]--;
}

/****************************************************************************/
/* This function fills the usrdir, usrdirs, usrlibs, curlib, and curdir     */
/* variables based on the security clearance of the current user (useron)   */
/****************************************************************************/
void sbbs_t::getusrdirs()
{
    uint i,j,k,l;

	if(useron.rest&FLAG('T')) {
		usrlibs=0;
		return; 
	}
	for(j=0,i=0;i<cfg.total_libs;i++) {
		if(!chk_ar(cfg.lib[i]->ar,&useron,&client))
			continue;
		for(k=0,l=0;l<cfg.total_dirs;l++) {
			if(cfg.dir[l]->lib!=i) continue;
			if(!chk_ar(cfg.dir[l]->ar,&useron,&client))
				continue;
			usrdir[j][k++]=l; 
		}
		usrdirs[j]=k;
		if(!k)          /* No dirs accessible in lib */
			continue;
		usrlib[j++]=i; 
	}
	usrlibs=j;
	if(usrlibs==0)
		return;
	while((curlib>=usrlibs || !usrdirs[curlib]) && curlib) curlib--;
	while(curdir[curlib]>=usrdirs[curlib] && curdir[curlib]) curdir[curlib]--;
}

uint sbbs_t::getusrgrp(uint subnum)
{
	uint	ugrp;

	if(subnum==INVALID_SUB)
		return(0);

	if(usrgrps<=0)
		return(0);

	for(ugrp=0;ugrp<usrgrps;ugrp++)
		if(usrgrp[ugrp]==cfg.sub[subnum]->grp)
			break;

	return(ugrp+1);
}

uint sbbs_t::getusrsub(uint subnum)
{
	uint	usub;
	uint	ugrp;

	ugrp = getusrgrp(subnum);
	if(ugrp<=0)
		return(0);
	ugrp--;
	for(usub=0;usub<usrsubs[ugrp];usub++)
		if(usrsub[ugrp][usub]==subnum)
			break;

	return(usub+1);
}

uint sbbs_t::getusrlib(uint dirnum)
{
	uint	ulib;

	if(dirnum == INVALID_DIR)
		return 0;

	if(usrlibs <= 0)
		return 0;

	for(ulib=0; ulib < usrlibs; ulib++)
		if(usrlib[ulib] == cfg.dir[dirnum]->lib)
			break;

	return ulib+1;
}

uint sbbs_t::getusrdir(uint dirnum)
{
	uint	udir;
	uint	ulib;

	ulib = getusrlib(dirnum);
	if(ulib <= 0)
		return 0;
	ulib--;
	for(udir=0; udir < usrdirs[ulib]; udir++)
		if(usrdir[ulib][udir] == dirnum)
			break;

	return udir+1;
}


int sbbs_t::dir_op(uint dirnum)
{
	return(SYSOP || (cfg.dir[dirnum]->op_ar!=NULL && cfg.dir[dirnum]->op_ar[0] && chk_ar(cfg.dir[dirnum]->op_ar,&useron,&client)));
}


