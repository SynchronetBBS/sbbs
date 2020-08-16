/* Upgrade Synchronet files from v3 to v4 */

/* $Id: v4upgrade.c,v 1.16 2018/07/24 01:11:08 rswindell Exp $ */

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
#include "sbbs4defs.h"
#include "ini_file.h"
#include "dat_file.h"
#include "datewrap.h"

scfg_t scfg;
BOOL overwrite_existing_files=TRUE;
ini_style_t style = { 25, NULL, NULL, " = ", NULL };

BOOL overwrite(const char* path)
{
	char	str[128];

	if(!overwrite_existing_files && fexist(path)) {
		printf("\n%s already exists, overwrite? ",path);
		fgets(str,sizeof(str),stdin);
		if(toupper(*str)!='Y')
			return(FALSE);
	}

	return(TRUE);
}

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
long DLLCALL dstrtodate(scfg_t* cfg, char *instr)
{
	char*	p;
	char*	day;
	char	str[16];
	struct tm tm;

	if(!instr[0] || !strncmp(instr,"00/00/00",8))
		return(0);

	if(isdigit(instr[0]) && isdigit(instr[1])
		&& isdigit(instr[3]) && isdigit(instr[4])
		&& isdigit(instr[6]) && isdigit(instr[7]))
		p=instr;	/* correctly formatted */
	else {
		p=instr;	/* incorrectly formatted */
		while(*p && isdigit(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		day=p;
		while(*p && isdigit(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		sprintf(str,"%02u/%02u/%02u"
			,atoi(instr)%100,atoi(day)%100,atoi(p)%100);
		p=str;
	}

	memset(&tm,0,sizeof(tm));
	tm.tm_year=((p[6]&0xf)*10)+(p[7]&0xf);
	if(cfg->sys_misc&SM_EURODATE) {
		tm.tm_mon=((p[3]&0xf)*10)+(p[4]&0xf);
		tm.tm_mday=((p[0]&0xf)*10)+(p[1]&0xf); }
	else {
		tm.tm_mon=((p[0]&0xf)*10)+(p[1]&0xf);
		tm.tm_mday=((p[3]&0xf)*10)+(p[4]&0xf); }

	return(((tm.tm_year+1900)*10000)+(tm.tm_mon*100)+tm.tm_mday);
}


BOOL upgrade_users(void)
{
	char	outpath[MAX_PATH+1];
	char	rec[USER_REC_LEN+1];
	FILE*	out;
	int		i,total;
	int		ret;
	size_t	len;
	user_t	user;

	printf("Upgrading user database...     ");

	sprintf(outpath,"%suser/user.tab",scfg.data_dir);
	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"wb"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	fprintf(out,"%-*.*s\r\n",USER_REC_LEN,USER_REC_LEN,tabLineCreator(user_dat_columns));

	total=lastuser(&scfg);
	for(i=1;i<=total;i++) {
		printf("\b\b\b\b\b%5u",total-i);
		memset(&user,0,sizeof(user));
		user.number=i;
		if((ret=getuserdat(&scfg,&user))!=0) {
			printf("\nError %d reading user.dat\n",ret);
			return(FALSE);
		}
		/******************************************/
		/* personal info */
		len=sprintf(rec,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t"
			,user.alias
			,user.name
			,user.handle
			,user.note
			,user.ipaddr
			,user.comp
			,user.comment
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* very personal info */
		len+=sprintf(rec+len,"%s\t%s\t%s\t%s\t%s\t%s\t%lu\t%c\t%s\t"
			,user.netmail
			,user.address
			,user.location
			,user.zipcode
			,user.pass
			,user.phone
			,dstrtodate(&scfg,user.birth)
			,user.sex
			,user.modem
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* date/times */
		len+=sprintf(rec+len,"%08lu%06u\t%08lu%06u\t%08lu%06u\t%08lu%06u\t%08lu%06u\t%08lu%06u\t"
			,time_to_isoDate(user.laston)
			,time_to_isoTime(user.laston)
			,time_to_isoDate(user.firston)
			,time_to_isoTime(user.firston)
			,time_to_isoDate(user.expire)
			,time_to_isoTime(user.expire)
			,time_to_isoDate(user.pwmod)
			,time_to_isoTime(user.pwmod)
			,time_to_isoDate(user.ns_time)
			,time_to_isoTime(user.ns_time)
			,time_to_isoDate(user.logontime)
			,time_to_isoTime(user.logontime)
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* counting stats */
		len+=sprintf(rec+len,"%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t"
			,user.logons
			,user.ltoday
			,user.timeon
			,user.textra
			,user.ttoday
			,user.tlast
			,user.posts
			,user.emails
			,user.fbacks
			,user.etoday
			,user.ptoday
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* file xfer stats, credits, minutes */
		len+=sprintf(rec+len,"%u\t%u\t%u\t%u\t%u\t%u\t%u\t"
			,user.ulb
			,user.uls
			,user.dlb
			,user.dls
			,user.cdt
			,user.freecdt
			,user.min
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* security */
		len+=sprintf(rec+len,"%u\t%lx\t%lx\t%lx\t%lx\t%lx\t%lx\t"
			,user.level
			,user.flags1
			,user.flags2
			,user.flags3
			,user.flags4
			,user.exempt
			,user.rest
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* settings (bit-fields) */
		len+=sprintf(rec+len,"%lx\t%lx\t%lx\t"
			,user.misc
			,user.qwk
			,user.chat
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* settings (strings and numbers) */
		len+=sprintf(rec+len,"%u\t%c\t%s\t%s\t%s\t%s\t%s\t%s\t"
			,user.rows
			,user.prot
			,user.xedit ? scfg.xedit[user.xedit]->code : ""
			,scfg.shell[user.shell]->code
			,user.tmpext
			,user.cursub
			,user.curdir
			,user.curxtrn
			);
		/* Message disabled.  Why?  ToDo */
		/* printf("reclen=%u\n",len); */
		if((ret=fprintf(out,"%-*.*s\r\n",USER_REC_LEN,USER_REC_LEN,rec))!=USER_REC_LINE_LEN) {
			printf("!Error %d (errno: %d) writing %u bytes to user.tab\n"
				,ret, errno, USER_REC_LINE_LEN);
			return(FALSE);
		}
	}
	fclose(out);

	printf("\n\tdata/user/user.dat -> %s (%u users)\n", outpath,total);

	return(TRUE);
}

typedef struct {
	time_t	time;
	ulong	ltoday;
	ulong	ttoday;
	ulong	uls;
	ulong	ulb;
	ulong	dls;
	ulong	dlb;
	ulong	ptoday;
	ulong	etoday;
	ulong	ftoday;
} csts_t;

BOOL upgrade_stats(void)
{
	char	inpath[MAX_PATH+1];
	char	outpath[MAX_PATH+1];
	BOOL	success;
	ulong	count;
	time32_t	t;
	stats_t	stats;
	FILE*	in;
	FILE*	out;
	csts_t	csts;
	str_list_t	list;

	printf("Upgrading statistics data...\n");

    sprintf(inpath,"%sdsts.dab",scfg.ctrl_dir);
	printf("\t%s ",inpath);
	if((in=fopen(inpath,"rb"))==NULL) {
		perror(inpath);
		return(FALSE);
	}
	fread(&t,sizeof(t),1,in);
    fread(&stats,sizeof(stats),1,in);
    fclose(in);

	sprintf(outpath,"%sstats.dat",scfg.ctrl_dir);
	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"w"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	if((list = strListInit())==NULL) {
		printf("!malloc failure\n");
		return(FALSE);
	}

	iniSetDateTime(&list,	ROOT_SECTION	,"TimeStamp"	,TRUE, t		,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"Logons"		,stats.logons	,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"LogonsToday"	,stats.ltoday	,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"Timeon"		,stats.timeon	,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"TimeonToday"	,stats.ttoday	,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"Uploads"		,stats.uls		,NULL);
	iniSetLongInt(&list,	ROOT_SECTION	,"UploadBytes"	,stats.ulb		,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"Downloads"	,stats.dls		,NULL);
	iniSetLongInt(&list,	ROOT_SECTION	,"DownloadBytes",stats.dlb		,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"PostsToday"	,stats.ptoday	,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"EmailToday"	,stats.etoday	,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"FeedbackToday",stats.ftoday	,NULL);
	iniSetInteger(&list,	ROOT_SECTION	,"NewUsersToday",stats.nusers	,NULL);

	success=iniWriteFile(out, list);

	fclose(out);
	strListFree(&list);
	printf("-> %s\n", outpath);

	if(!success) {
		printf("!iniWriteFile failure\n");
		return(FALSE);
	}

    sprintf(inpath,"%scsts.dab",scfg.ctrl_dir);
	printf("\t%s ",inpath);
	if((in=fopen(inpath,"rb"))==NULL) {
		perror(inpath);
		return(FALSE);
	}

	sprintf(outpath,"%sstats.tab",scfg.ctrl_dir);
	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"w"))==NULL) {
		perror(outpath);
		return(FALSE);
	}
#if 0
	fprintf(out,"Time Stamp\tLogons\tTimeon\tUploaded Files\tUploaded Bytes\t"
				"Downloaded Files\tDownloaded Bytes\tPosts\tEmail Sent\tFeedback Sent\r\n");
#else
	fprintf(out,"%s\n",tabLineCreator(stats_dat_columns));
#endif

	count=0;
	while(!feof(in)) {
		if(fread(&csts,1,sizeof(csts),in)!=sizeof(csts))
			break;
		fprintf(out,"%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t\n"
			,time_to_isoDate(csts.time)
			,csts.ltoday
			,csts.ttoday
			,csts.uls
			,csts.ulb
			,csts.dls
			,csts.dlb
			,csts.ptoday
			,csts.etoday
			,csts.ftoday
			);
		count++;
	}
	fclose(in);
	fclose(out);
	printf("-> %s (%u days)\n", outpath, count);

	return(success);
}

BOOL upgrade_event_data(void)
{
	char	inpath[MAX_PATH+1];
	char	outpath[MAX_PATH+1];
	BOOL	success;
	FILE*	in;
	FILE*	out;
	size_t	i;
	time32_t	t;
	str_list_t	list;

	printf("Upgrading event data...\n");

	sprintf(outpath,"%sevent.dat",scfg.ctrl_dir);
	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"w"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	if((list = strListInit())==NULL) {
		printf("!malloc failure\n");
		return(FALSE);
	}

	/* Read TIME.DAB */
	sprintf(inpath,"%stime.dab",scfg.ctrl_dir);
	printf("\t%s ",inpath);
	if((in=fopen(inpath,"rb"))==NULL) {
		perror("open failure");
		return(FALSE);
	}
	for(i=0;i<scfg.total_events;i++) {
		t=0;
		fread(&t,1,sizeof(t),in);
		iniSetHexInt(&list, "Events", scfg.event[i]->code, t, NULL);
	}
	t=0;
	fread(&t,1,sizeof(t),in);
	iniSetHexInt(&list,ROOT_SECTION,"QWKPrePack",t,NULL);
	fclose(in);

	printf("-> %s (%u timed events)\n", outpath, i);

	/* Read QNET.DAB */
	sprintf(inpath,"%sqnet.dab",scfg.ctrl_dir);
	printf("\t%s ",inpath);
	i=0;
	if((in=fopen(inpath,"rb"))==NULL)
		perror("open failure");
	else {
		for(i=0;i<scfg.total_qhubs;i++) {
			t=0;
			fread(&t,1,sizeof(t),in);
			iniSetHexInt(&list,"QWKNetworkHubs",scfg.qhub[i]->id,t,NULL);
		}
		fclose(in);
	}
	printf("-> %s (%u QWKnet hubs)\n", outpath, i);

	success=iniWriteFile(out, list);

	fclose(out);
	strListFree(&list);

	if(!success) {
		printf("!iniWriteFile failure\n");
		return(FALSE);
	}

	return(success);
}

BOOL upgrade_ip_filters(void)
{
	char	inpath[MAX_PATH+1];
	char	outpath[MAX_PATH+1];
	char	msgpath[MAX_PATH+1];
	char	str[INI_MAX_VALUE_LEN];
	char	estr[INI_MAX_VALUE_LEN];
	char*	p;
	FILE*	in;
	FILE*	out;
	BOOL	success;
	size_t	i;
	size_t	total;
	str_list_t	inlist;
	str_list_t	outlist;

	style.section_separator = NULL;
	iniSetDefaultStyle(style);

	printf("Upgrading IP Address filters...\n");

	sprintf(outpath,"%sip-filter.ini",scfg.ctrl_dir);
	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"w"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	if((outlist = strListInit())==NULL) {
		printf("!malloc failure\n");
		return(FALSE);
	}

	/* Read the message file (if present) */
	sprintf(msgpath,"%sbadip.msg",scfg.text_dir);
	if(fexist(msgpath)) {
		printf("\t%s ",msgpath);

		if((in=fopen(msgpath,"r"))==NULL) {
			perror("open failure");
			return(FALSE);
		}

		i=fread(str,1,INI_MAX_VALUE_LEN,in);
		str[i]=0;
		truncsp(str);
		fclose(in);

		if(strlen(str)) {
			c_escape_str(str,estr,sizeof(estr),/* ctrl_only? */TRUE);
			iniSetString(&outlist,ROOT_SECTION,"Message",estr,NULL);
		}

		printf("-> %s\n", outpath);
	}

	sprintf(inpath,"%sip.can",scfg.text_dir);
	printf("\t%s ",inpath);
	if((in=fopen(inpath,"r"))==NULL) {
		perror("open failure");
		return(FALSE);
	}

	if((inlist = strListReadFile(in,NULL,4096))==NULL) {
		printf("!failure reading %s\n",inpath);
		return(FALSE);
	}

	total=0;
	for(i=0;inlist[i]!=NULL;i++) {
		p=truncsp(inlist[i]);
		SKIP_WHITESPACE(p);
		if(*p==';')
			strListPush(&outlist,p);
		else if(*p) {
			iniAppendSection(&outlist,p,NULL);
			total++;
		}
	}

	printf("-> %s (%u IP Addresses)\n", outpath, total);
	fclose(in);
	strListFreeStrings(inlist);

	sprintf(inpath,"%sip-silent.can",scfg.text_dir);
	printf("\t%s ",inpath);
	if((in=fopen(inpath,"r"))==NULL) {
		perror("open failure");
		return(FALSE);
	}

	if((inlist = strListReadFile(in,NULL,4096))==NULL) {
		printf("!failure reading %s\n",inpath);
		return(FALSE);
	}

	total=0;
	for(i=0;inlist[i]!=NULL;i++) {
		p=truncsp(inlist[i]);
		SKIP_WHITESPACE(p);
		if(*p==';')
			strListPush(&outlist,p);
		else if(*p) {
			iniSetBool(&outlist,p,"Silent",TRUE,NULL);
			total++;
		}
	}

	printf("-> %s (%u IP Addresses)\n", outpath, total);
	fclose(in);
	strListFree(&inlist);

	success=iniWriteFile(out, outlist);

	fclose(out);

	if(!success) {
		printf("!iniWriteFile failure\n");
		return(FALSE);
	}

	printf("\tFiltering %u total IP Addresses\n", iniGetSectionCount(outlist,NULL));

	strListFree(&outlist);

	return(success);
}

BOOL upgrade_filter(const char* desc, const char* inpath, const char* msgpath, const char* outpath)
{
	char*	p;
	char	str[INI_MAX_VALUE_LEN];
	char	estr[INI_MAX_VALUE_LEN];
	FILE*	in;
	FILE*	out;
	BOOL	success;
	size_t	i;
	size_t	total;
	str_list_t	inlist;
	str_list_t	outlist;

	style.section_separator = NULL;
	iniSetDefaultStyle(style);

	printf("Upgrading %s filters...\n",desc);

	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"w"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	if((outlist = strListInit())==NULL) {
		printf("!malloc failure\n");
		return(FALSE);
	}

	/* Read the message file (if present) */
	if(msgpath!=NULL && fexist(msgpath)) {
		printf("\t%s ",msgpath);

		if((in=fopen(msgpath,"r"))==NULL) {
			perror("open failure");
			return(FALSE);
		}

		i=fread(str,1,INI_MAX_VALUE_LEN,in);
		str[i]=0;
		truncsp(str);
		fclose(in);

		if(strlen(str)) {
			c_escape_str(str,estr,sizeof(estr),/* ctrl_only? */TRUE);
			iniSetString(&outlist,ROOT_SECTION,"Message",estr,NULL);
		}

		printf("-> %s\n", outpath);
	}

	printf("\t%s ",inpath);
	if((in=fopen(inpath,"r"))==NULL) {
		perror("open failure");
		return(FALSE);
	}

	if((inlist = strListReadFile(in,NULL,4096))==NULL) {
		printf("!failure reading %s\n",inpath);
		return(FALSE);
	}

	total=0;
	for(i=0;inlist[i]!=NULL;i++) {
		p=truncsp(inlist[i]);
		SKIP_WHITESPACE(p);
		if(*p==';')
			strListPush(&outlist,p);
		else if(*p) {
			iniAppendSection(&outlist,p,NULL);
			total++;
		}
	}

	printf("-> %s (%u %ss)\n", outpath, total, desc);
	fclose(in);
	strListFree(&inlist);

	success=iniWriteFile(out, outlist);

	fclose(out);

	if(!success) {
		printf("!iniWriteFile failure\n");
		return(FALSE);
	}

	printf("\tFiltering %u total %ss\n", iniGetSectionCount(outlist,NULL),desc);

	strListFree(&outlist);

	return(success);
}

BOOL upgrade_list(const char* desc, const char* infile, const char* outfile
				  ,BOOL section_list, const char* key)
{
	char*	p;
	char*	vp;
	char	inpath[MAX_PATH+1];
	char	outpath[MAX_PATH+1];
	FILE*	in;
	FILE*	out;
	BOOL	success;
	size_t	i;
	size_t	total;
	str_list_t	inlist;
	str_list_t	outlist;

	style.section_separator = (section_list && key==NULL) ? NULL : "";
	iniSetDefaultStyle(style);

	SAFEPRINTF2(inpath,"%s%s",scfg.ctrl_dir,infile);
	SAFEPRINTF2(outpath,"%s%s",scfg.ctrl_dir,outfile);

	if(!fexistcase(inpath))
		return(TRUE);

	printf("Upgrading %s...\n",desc);

	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"w"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	if((outlist = strListInit())==NULL) {
		printf("!malloc failure\n");
		return(FALSE);
	}
	printf("\t%s ",inpath);
	if((in=fopen(inpath,"r"))==NULL) {
		perror("open failure");
		return(FALSE);
	}

	if((inlist = strListReadFile(in,NULL,4096))==NULL) {
		printf("!failure reading %s\n",inpath);
		return(FALSE);
	}

	total=0;
	for(i=0;inlist[i]!=NULL;i++) {
		p=truncsp(inlist[i]);
		SKIP_WHITESPACE(p);
		if(*p==';')
			strListPush(&outlist,p);
		else if(*p) {
			vp=NULL;
			if((!section_list || key!=NULL)
				&& ((vp=strchr(p,' '))!=NULL || ((vp=strchr(p,'\t'))!=NULL))) {
				*(vp++) = 0;
				SKIP_WHITESPACE(vp);
			}
			if(section_list) {
				iniAppendSection(&outlist,p,NULL);
				if(vp!=NULL && *vp)
					iniSetString(&outlist,p,key,vp,NULL);
			} else
				iniSetString(&outlist,ROOT_SECTION,p,vp,NULL);
			total++;
		}
	}

	printf("-> %s (%u)\n", outpath, total);
	fclose(in);
	strListFree(&inlist);

	success=iniWriteFile(out, outlist);

	fclose(out);

	if(!success) {
		printf("!iniWriteFile failure\n");
		return(FALSE);
	}

	printf("\tWrote %u items\n", iniGetSectionCount(outlist,NULL));

	strListFree(&outlist);

	return(success);
}

BOOL upgrade_filters()
{
	char	inpath[MAX_PATH+1];
	char	outpath[MAX_PATH+1];
	char	msgpath[MAX_PATH+1];

	if(!upgrade_ip_filters())
		return(FALSE);

	sprintf(inpath,"%shost.can",scfg.text_dir);
	sprintf(msgpath,"%sbadhost.msg",scfg.text_dir);
	sprintf(outpath,"%shost-filter.ini",scfg.ctrl_dir);
	if(!upgrade_filter("Hostname",inpath,msgpath,outpath))
		return(FALSE);

	sprintf(inpath,"%semail.can",scfg.text_dir);
	sprintf(msgpath,"%sbademail.msg",scfg.text_dir);
	sprintf(outpath,"%semail-filter.ini",scfg.ctrl_dir);
	if(!upgrade_filter("E-mail Address",inpath,msgpath,outpath))
		return(FALSE);

	sprintf(inpath,"%sname.can",scfg.text_dir);
	sprintf(msgpath,"%sbadname.msg",scfg.text_dir);
	sprintf(outpath,"%sname-filter.ini",scfg.ctrl_dir);
	if(!upgrade_filter("User Name",inpath,msgpath,outpath))
		return(FALSE);

	sprintf(inpath,"%sphone.can",scfg.text_dir);
	sprintf(msgpath,"%sbadphone.msg",scfg.text_dir);
	sprintf(outpath,"%sphone-filter.ini",scfg.ctrl_dir);
	if(!upgrade_filter("Phone Number",inpath,msgpath,outpath))
		return(FALSE);

	sprintf(inpath,"%ssubject.can",scfg.text_dir);
	sprintf(msgpath,"%sbadsubject.msg",scfg.text_dir);
	sprintf(outpath,"%ssubject-filter.ini",scfg.ctrl_dir);
	if(!upgrade_filter("Message Subject",inpath,msgpath,outpath))
		return(FALSE);

	return(TRUE);
}

#define BBS_VIRTUAL_PATH		"bbs:/""/"	/* this is actually bbs:<slash><slash> */

BOOL upgrade_ftp_aliases(void)
{
	char*	p;
	char*	path;
	char*	desc;
	char*	section;
	char	inpath[MAX_PATH+1];
	char	outpath[MAX_PATH+1];
	FILE*	in;
	FILE*	out;
	BOOL	success;
	size_t	i;
	size_t	total;
	str_list_t	inlist;
	str_list_t	outlist;

	style.section_separator = "";
	iniSetDefaultStyle(style);

	SAFEPRINTF(inpath,"%sftpalias.cfg",scfg.ctrl_dir);
	SAFEPRINTF(outpath,"%sftpalias.ini",scfg.ctrl_dir);

	if(!fexistcase(inpath))
		return(TRUE);

	printf("Upgrading FTP Aliases...\n");

	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"w"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	if((outlist = strListInit())==NULL) {
		printf("!malloc failure\n");
		return(FALSE);
	}
	printf("\t%s ",inpath);
	if((in=fopen(inpath,"r"))==NULL) {
		perror("open failure");
		return(FALSE);
	}

	if((inlist = strListReadFile(in,NULL,4096))==NULL) {
		printf("!failure reading %s\n",inpath);
		return(FALSE);
	}

	total=0;
	for(i=0;inlist[i]!=NULL;i++) {
		p=truncsp(inlist[i]);
		SKIP_WHITESPACE(p);
		if(*p==';') {
			strListPush(&outlist,p);
			continue;
		} else if(*p==0)
			continue;
		path=p;
		FIND_WHITESPACE(path);
		if(*path==0)
			continue;
		*(path++)=0;
		SKIP_WHITESPACE(path);
		desc=path;
		FIND_WHITESPACE(desc);
		if(*desc==0)
			continue;
		*(desc++)=0;
		SKIP_WHITESPACE(desc);
		iniAppendSection(&outlist,p,NULL);
		if(!strnicmp(path,BBS_VIRTUAL_PATH,strlen(BBS_VIRTUAL_PATH)))
			path+=strlen(BBS_VIRTUAL_PATH)-1;
		else
			iniSetBool(&outlist,p,"Local",TRUE,NULL);
		iniSetString(&outlist,p,"Path",path,NULL);
		if(!stricmp(desc,"hidden"))
			iniSetBool(&outlist,p,"Hidden",TRUE,NULL);
		else
			iniSetString(&outlist,p,"Description",desc,NULL);
		total++;
	}

	section="local";
	iniAppendSection(&outlist,section,NULL);
	iniSetString(&outlist,section,"Description","Local file system",NULL);
	iniSetString(&outlist,section,"Path","/",NULL);
	iniSetBool(&outlist,section,"Local",TRUE,NULL);
	iniSetString(&outlist,section,"AccessRequirements","SYSOP",NULL);

	printf("-> %s (%u FTP aliases)\n", outpath, total);
	fclose(in);
	strListFree(&inlist);

	success=iniWriteFile(out, outlist);

	fclose(out);

	if(!success) {
		printf("!iniWriteFile failure\n");
		return(FALSE);
	}

	printf("\tWrote %u total FTP aliases\n", iniGetSectionCount(outlist,NULL));

	strListFree(&outlist);

	return(success);
}

BOOL upgrade_socket_options(void)
{
	char*	p;
	char*	key;
	char*	val;
	char*	section;
	char	inpath[MAX_PATH+1];
	char	outpath[MAX_PATH+1];
	FILE*	in;
	FILE*	out;
	BOOL	success;
	size_t	i;
	size_t	total;
	str_list_t	inlist;
	str_list_t	outlist;

	style.section_separator = "";
	iniSetDefaultStyle(style);

	SAFEPRINTF(inpath,"%ssockopts.cfg",scfg.ctrl_dir);
	SAFEPRINTF(outpath,"%ssockopts.ini",scfg.ctrl_dir);

	if(!fexistcase(inpath))
		return(TRUE);

	printf("Upgrading Socket Options...\n");

	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"w"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	if((outlist = strListInit())==NULL) {
		printf("!malloc failure\n");
		return(FALSE);
	}
	printf("\t%s ",inpath);
	if((in=fopen(inpath,"r"))==NULL) {
		perror("open failure");
		return(FALSE);
	}

	if((inlist = strListReadFile(in,NULL,4096))==NULL) {
		printf("!failure reading %s\n",inpath);
		return(FALSE);
	}

	total=0;
	for(i=0;inlist[i]!=NULL;i++) {
		p=truncsp(inlist[i]);
		SKIP_WHITESPACE(p);
		if(*p==';') {
			strListPush(&outlist,p);
			continue;
		} else if(*p==0)
			continue;
		key=p;
		FIND_WHITESPACE(p);
		if(*p==0)
			continue;
		*(p++)=0;
		SKIP_WHITESPACE(p);
		val=p;
		section=ROOT_SECTION;
		if(!stricmp(key,"tcp_nodelay"))
			section="telnet|rlogin";
		else if(!stricmp(key,"keepalive"))
			section="tcp";
		iniSetString(&outlist,section,key,val,NULL);
		total++;
	}

	printf("-> %s (%u Socket Options)\n", outpath, total);
	fclose(in);
	strListFree(&inlist);

	success=iniWriteFile(out, outlist);

	fclose(out);

	if(!success) {
		printf("!iniWriteFile failure\n");
		return(FALSE);
	}

	strListFree(&outlist);

	return(success);
}



#define upg_iniSetString(list,section,key,val) \
		if(*val) iniSetString(list,section,key,val,NULL)

#define upg_iniSetInteger(list,section,key,val) \
		if(val) iniSetInteger(list,section,key,val,NULL)

BOOL upgrade_msg_areas(void)
{
	char	str[128];
	char	outpath[MAX_PATH+1];
	char	data_subs[MAX_PATH+1];
	FILE*	out;
	BOOL	success;
	size_t	i;
	str_list_t	outlist;

	style.section_separator = "";
	iniSetDefaultStyle(style);

	SAFEPRINTF(outpath,"%smsg_areas.ini",scfg.ctrl_dir);

	SAFEPRINTF(data_subs,"%ssubs",scfg.data_dir);
	backslash(data_subs);

	printf("Upgrading Message Area configuration...\n");

	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"w"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	if((outlist = strListInit())==NULL) {
		printf("!malloc failure\n");
		return(FALSE);
	}

	for(i=0; i<scfg.total_grps; i++) {
		SAFEPRINTF(str,"Group:%s",scfg.grp[i]->sname);
		iniAppendSection(&outlist,str,NULL);
		upg_iniSetString(&outlist,str,"Description",scfg.grp[i]->lname);
		upg_iniSetString(&outlist,str,"AccessRequirements",scfg.grp[i]->arstr);
		upg_iniSetString(&outlist,str,"CodePrefix",scfg.grp[i]->code_prefix);
	}
	for(i=0; i<scfg.total_subs; i++) {
		sprintf(str,"%s",scfg.sub[i]->code_suffix);
		iniAppendSection(&outlist,str,NULL);
		upg_iniSetString(&outlist,str,"Group",scfg.grp[scfg.sub[i]->grp]->sname);
		upg_iniSetString(&outlist,str,"Name",scfg.sub[i]->sname);
		upg_iniSetString(&outlist,str,"Newsgroup",scfg.sub[i]->newsgroup);
		upg_iniSetString(&outlist,str,"QwkName",scfg.sub[i]->qwkname);
		upg_iniSetInteger(&outlist,str,"QwkConference",scfg.sub[i]->qwkconf);
		upg_iniSetString(&outlist,str,"Description",scfg.sub[i]->lname);
		if(stricmp(scfg.sub[i]->data_dir, data_subs))
			iniSetString(&outlist,str,"DataDir",scfg.sub[i]->data_dir,NULL);
		if(strcmp(scfg.sub[i]->tagline, scfg.qnet_tagline))
			upg_iniSetString(&outlist,str,"TagLine",scfg.sub[i]->tagline);
		if(strcmp(scfg.origline, scfg.sub[i]->origline))
			upg_iniSetString(&outlist,str,"OriginLine",scfg.sub[i]->origline);
		upg_iniSetString(&outlist,str,"AccessRequirements",scfg.sub[i]->arstr);
		upg_iniSetString(&outlist,str,"ReadRequirements",scfg.sub[i]->read_arstr);
		upg_iniSetString(&outlist,str,"PostRequirements",scfg.sub[i]->post_arstr);
		upg_iniSetString(&outlist,str,"OperatorRequirements",scfg.sub[i]->op_arstr);
		upg_iniSetString(&outlist,str,"ModeratorRequirements",scfg.sub[i]->mod_arstr);
		upg_iniSetString(&outlist,str,"PostSemFile",scfg.sub[i]->post_sem);
		upg_iniSetInteger(&outlist,str,"MaxMessages",scfg.sub[i]->maxmsgs);
		upg_iniSetInteger(&outlist,str,"MaxMessageAge",scfg.sub[i]->maxage);
		upg_iniSetInteger(&outlist,str,"CrcHistory",scfg.sub[i]->maxcrcs);
		if(scfg.sub[i]->faddr.zone)
			iniSetString(&outlist,str,"FidoNetAddress",smb_faddrtoa(&scfg.sub[i]->faddr,NULL),NULL);
	}
	printf("-> %s (%u groups and %u sub-boards)\n", outpath, scfg.total_grps, scfg.total_subs);

	success=iniWriteFile(out, outlist);

	fclose(out);

	if(!success) {
		printf("!iniWriteFile failure\n");
		return(FALSE);
	}

	printf("\tWrote %u items\n", iniGetSectionCount(outlist,NULL));

	strListFree(&outlist);

	return(success);
}


char *usage="\nusage: v4upgrade [ctrl_dir]\n";

int main(int argc, char** argv)
{
	char	error[512];
	char	revision[16];
	char*	p;
	int		first_arg=1;

	sscanf("$Revision: 1.16 $", "%*s %s", revision);

	fprintf(stderr,"\nV4upgrade v%s-%s - Upgrade Synchronet files from v3 to v4\n"
		,revision
		,PLATFORM_DESC
		);

	if(argc>1 && strcspn(argv[first_arg],"/\\")!=strlen(argv[first_arg]))
		p=argv[first_arg++];
	else
		p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\nSBBSCTRL environment variable not set.\n");
		printf("\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
		exit(1); 
	}

	memset(&scfg,0,sizeof(scfg));
	scfg.size=sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir,p);

	if(chdir(scfg.ctrl_dir)!=0)
		fprintf(stderr,"!ERROR changing directory to: %s", scfg.ctrl_dir);

	printf("\nLoading configuration files from %s\n",scfg.ctrl_dir);
	if(!load_cfg(&scfg,NULL,TRUE,error)) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		exit(1);
	}

	iniSetDefaultStyle(style);

	if(!upgrade_users())
		return(1);

	if(!upgrade_stats())
		return(2);

	if(!upgrade_event_data())
		return(3);

	if(!upgrade_filters())
		return(4);

	if(!upgrade_list("Twits", "twitlist.cfg", "twitlist.ini", TRUE, NULL))
		return(5);

	if(!upgrade_list("RLogin allow", "rlogin.cfg", "rlogin.ini", TRUE, NULL))
		return(5);

	if(!upgrade_list("E-Mail aliases", "alias.cfg", "alias.ini", FALSE, NULL))
		return(5);
	
	if(!upgrade_list("E-Mail domains", "domains.cfg", "domains.ini", TRUE, NULL))
		return(5);

	if(!upgrade_list("Allowed mail relayers", "relay.cfg", "relay.ini", TRUE, NULL))
		return(5);

	if(!upgrade_list("SPAM bait addresses", "spambait.cfg", "spambait.ini", TRUE, NULL))
		return(5);

	if(!upgrade_list("Blocked spammers", "spamblock.cfg", "spamblock.ini", TRUE, NULL))
		return(5);

	if(!upgrade_list("DNS black-lists", "dns_blacklist.cfg", "dns_blacklist.ini", TRUE, "notice"))
		return(5);

	if(!upgrade_list("DNS black-list exemptions", "dnsbl_exempt.cfg", "dnsbl_exempt.ini", TRUE, NULL))
		return(5);

	if(!upgrade_ftp_aliases())
		return(-1);

	if(!upgrade_socket_options())
		return(-1);

	/* attr.cfg */

	if(!upgrade_msg_areas())
		return(-1);

	printf("Upgrade successful.\n");
    return(0);
}
