/* useredit.cpp */

/* Synchronet online sysop user editor */

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

/*******************************************************************/
/* The function useredit(), and functions that are closely related */
/*******************************************************************/

#include "sbbs.h"

#define SEARCH_TXT 0
#define SEARCH_ARS 1

/****************************************************************************/
/* Edits user data. Can only edit users with a Main Security Level less 	*/
/* than or equal to the current user's Main Security Level					*/
/* Called from functions waitforcall, main_sec, xfer_sec and inkey			*/
/****************************************************************************/
void sbbs_t::useredit(int usernumber)
{
	char	str[256],tmp2[256],tmp3[256],c,stype=SEARCH_TXT;
	char 	tmp[512];
	char	search[256]={""},artxt[128]={""};
	uchar	*ar=NULL;
	uint	i,j,k;
	long	l;
	user_t	user;
	struct	tm tm;

	if(online==ON_REMOTE && console&(CON_R_ECHO|CON_R_INPUT) && !chksyspass())
		return;
#if 0	/* no local logins in v3 */
	if(online==ON_LOCAL) {
		if(!(cfg.sys_misc&SM_L_SYSOP))
			return;
		if(cfg.node_misc&NM_SYSPW && !chksyspass())
			return; }
#endif
	if(usernumber)
		user.number=usernumber;
	else
		user.number=useron.number;
	action=NODE_SYSP;
	if(sys_status&SS_INUEDIT)
		return;
	sys_status|=SS_INUEDIT;
	while(online) {
		CLS;
		attr(LIGHTGRAY);
		getuserdat(&cfg,&user);
		if(!user.number) {
			user.number=1;
			getuserdat(&cfg,&user);
			if(!user.number) {
				bputs(text[NoUserData]);
				getkey(0);
				sys_status&=~SS_INUEDIT;
				return; } }
		unixtodstr(&cfg,time(NULL),str);
		unixtodstr(&cfg,user.laston,tmp);
		if(strcmp(str,tmp) && user.ltoday) {
			user.ltoday=user.ttoday=user.ptoday=user.etoday=user.textra=0;
			user.freecdt=cfg.level_freecdtperday[user.level];
			putuserdat(&cfg,&user); }	/* Leave alone */
		if(user.misc&DELETED)
			bputs(text[Deleted]);
		else if(user.misc&INACTIVE)
			bputs(text[Inactive]);
		bprintf(text[UeditAliasPassword]
			,user.alias, (user.level>useron.level && console&CON_R_ECHO)
			|| !(cfg.sys_misc&SM_ECHO_PW) ? "XXXXXXXX" : user.pass
			, unixtodstr(&cfg,user.pwmod,tmp));
		bprintf(text[UeditRealNamePhone]
			,user.level>useron.level && console&CON_R_ECHO
			? "XXXXXXXX" : user.name
			,user.level>useron.level && console&CON_R_ECHO
			? "XXX-XXX-XXXX" : user.phone);
		bprintf(text[UeditAddressBirthday]
			,user.address,getage(&cfg,user.birth),user.sex,user.birth);
		bprintf(text[UeditLocationZipcode],user.location,user.zipcode);
		bprintf(text[UeditNoteHandle],user.note,user.handle);
		bprintf(text[UeditComputerModem],user.comp,user.modem);
		if(user.netmail[0])
			bprintf(text[UserNetMail],user.netmail);

		sprintf(str,"%suser/%4.4u.msg", cfg.data_dir,user.number);
		i=fexist(str);
		if(user.comment[0] || i)
			bprintf(text[UeditCommentLine],i ? '+' : SP
				,user.comment);
		else
			CRLF;
		if(localtime_r(&user.laston,&tm)==NULL)
			return;
		bprintf(text[UserDates]
			,unixtodstr(&cfg,user.firston,str),unixtodstr(&cfg,user.expire,tmp)
			,unixtodstr(&cfg,user.laston,tmp2),tm.tm_hour, tm.tm_min);

		bprintf(text[UserTimes]
			,user.timeon,user.ttoday,cfg.level_timeperday[user.level]
			,user.tlast,cfg.level_timepercall[user.level],user.textra);
		if(user.posts)
			i=user.logons/user.posts;
		else
			i=0;
		bprintf(text[UserLogons]
			,user.logons,user.ltoday,cfg.level_callsperday[user.level],user.posts
			,i ? 100/i : user.posts>user.logons ? 100 : 0
			,user.ptoday);
		bprintf(text[UserEmails]
			,user.emails,user.fbacks,getmail(&cfg,user.number,0),user.etoday);

		bprintf(text[UserUploads],ultoac(user.ulb,tmp),user.uls);
		if(user.leech)
			sprintf(str,text[UserLeech],user.leech);
		else
			str[0]=0;
		bprintf(text[UserDownloads],ultoac(user.dlb,tmp),user.dls,str);
		bprintf(text[UserCredits],ultoac(user.cdt,tmp)
			,ultoac(user.freecdt,tmp2),ultoac(cfg.level_freecdtperday[user.level],str));
		bprintf(text[UserMinutes],ultoac(user.min,tmp));
		bprintf(text[UeditSecLevel],user.level);
		bprintf(text[UeditFlags],ltoaf(user.flags1,tmp),ltoaf(user.flags3,tmp2)
			,ltoaf(user.flags2,tmp3),ltoaf(user.flags4,str));
		bprintf(text[UeditExempts],ltoaf(user.exempt,tmp),ltoaf(user.rest,tmp2));
		l=lastuser(&cfg);
		ASYNC;
		if(lncntr>=rows-2)
			lncntr=0;
		bprintf(text[UeditPrompt],user.number,l);
		if(user.level>useron.level && console&CON_R_INPUT)
			strcpy(str,"QG[]?/{},");
		else
			strcpy(str,"ABCDEFGHIJKLMNOPQRSTUVWXYZ+[]?/{}~*$#");
		l=getkeys(str,l);
		if(l&0x80000000L) {
			user.number=(ushort)(l&~0x80000000L);
			continue; }
		switch(l) {
			case 'A':
				bputs(text[EnterYourAlias]);
				getstr(user.alias,LEN_ALIAS,K_LINE|K_EDIT|K_AUTODEL);
				putuserrec(&cfg,user.number,U_ALIAS,LEN_ALIAS,user.alias);
				if(!(user.misc&DELETED))
					putusername(&cfg,user.number,user.alias);
				bputs(text[EnterYourHandle]);
				getstr(user.handle,LEN_HANDLE,K_LINE|K_EDIT|K_AUTODEL);
				putuserrec(&cfg,user.number,U_HANDLE,LEN_HANDLE,user.handle);
				break;
			case 'B':
				bprintf(text[EnterYourBirthday]
					,cfg.sys_misc&SM_EURODATE ? "DD/MM/YY" : "MM/DD/YY");
				gettmplt(user.birth,"nn/nn/nn",K_LINE|K_EDIT|K_AUTODEL);
				putuserrec(&cfg,user.number,U_BIRTH,LEN_BIRTH,user.birth);
				break;
			case 'C':
				bputs(text[EnterYourComputer]);
				getstr(user.comp,LEN_COMP,K_LINE|K_EDIT|K_AUTODEL);
				putuserrec(&cfg,user.number,U_COMP,LEN_COMP,user.comp);
				break;
			case 'D':
				if(user.misc&DELETED) {
					if(!noyes(text[UeditRestoreQ])) {
						putuserrec(&cfg,user.number,U_MISC,8
							,ultoa(user.misc&~DELETED,str,16));
						putusername(&cfg,user.number,user.alias); }
					break; }
				if(user.misc&INACTIVE) {
					if(!noyes(text[UeditActivateQ]))
						putuserrec(&cfg,user.number,U_MISC,8
							,ultoa(user.misc&~INACTIVE,str,16));
					break; }
				if(!noyes(text[UeditDeleteQ])) {
					getsmsg(user.number);
					if(getmail(&cfg,user.number,0)) {
						if(yesno(text[UeditReadUserMailWQ]))
							readmail(user.number,MAIL_YOUR); }
					if(getmail(&cfg,user.number,1)) {
						if(yesno(text[UeditReadUserMailSQ]))
							readmail(user.number,MAIL_SENT); }
					putuserrec(&cfg,user.number,U_MISC,8
						,ultoa(user.misc|DELETED,str,16));
					putusername(&cfg,user.number,nulstr);
					break; }
				if(!noyes(text[UeditDeactivateUserQ])) {
					if(getmail(&cfg,user.number,0)) {
						if(yesno(text[UeditReadUserMailWQ]))
							readmail(user.number,MAIL_YOUR); }
					if(getmail(&cfg,user.number,1)) {
						if(yesno(text[UeditReadUserMailSQ]))
							readmail(user.number,MAIL_SENT); }
					putuserrec(&cfg,user.number,U_MISC,8
						,ultoa(user.misc|INACTIVE,str,16));
					break; }
				break;
			case 'E':
				if(!yesno(text[ChangeExemptionQ]))
					break;
				while(online) {
					bprintf(text[FlagEditing],ltoaf(user.exempt,tmp));
					c=(char)getkeys("ABCDEFGHIJKLMNOPQRSTUVWXYZ?\r",0);
					if(sys_status&SS_ABORT)
						break;
					if(c==CR) break;
					if(c=='?') {
						menu("exempt");
						continue; }
					if(user.level>useron.level 
						&& !(useron.exempt&FLAG(c)) && console&CON_R_INPUT)
						continue;
					user.exempt^=FLAG(c);
					putuserrec(&cfg,user.number,U_EXEMPT,8,ultoa(user.exempt,tmp,16)); }
				break;
			case 'F':
				i=1;
				while(online) {
					bprintf("\r\nFlag Set #%d\r\n",i);
					switch(i) {
						case 1:
							bprintf(text[FlagEditing],ltoaf(user.flags1,tmp));
							break;
						case 2:
							bprintf(text[FlagEditing],ltoaf(user.flags2,tmp));
							break;
						case 3:
							bprintf(text[FlagEditing],ltoaf(user.flags3,tmp));
							break;
						case 4:
							bprintf(text[FlagEditing],ltoaf(user.flags4,tmp));
							break; }
					c=(char)getkeys("ABCDEFGHIJKLMNOPQRSTUVWXYZ?1234\r",0);
					if(sys_status&SS_ABORT)
						break;
					if(c==CR) break;
					if(c=='?') {
						sprintf(str,"flags%d",i);
						menu(str);
						continue; }
					if(isdigit(c)) {
						i=c&0xf;
						continue; }
					if(user.level>useron.level && console&CON_R_INPUT)
						switch(i) {
							case 1:
								if(!(useron.flags1&FLAG(c)))
									continue;
								break;
							case 2:
								if(!(useron.flags2&FLAG(c)))
									continue;
								break;
							case 3:
								if(!(useron.flags3&FLAG(c)))
									continue;
								break;
							case 4:
								if(!(useron.flags4&FLAG(c)))
									continue;
								break; }
					switch(i) {
						case 1:
							user.flags1^=FLAG(c);
							putuserrec(&cfg,user.number,U_FLAGS1,8
								,ultoa(user.flags1,tmp,16));
							break;
						case 2:
							user.flags2^=FLAG(c);
							putuserrec(&cfg,user.number,U_FLAGS2,8
								,ultoa(user.flags2,tmp,16));
							break;
						case 3:
							user.flags3^=FLAG(c);
							putuserrec(&cfg,user.number,U_FLAGS3,8
								,ultoa(user.flags3,tmp,16));
							break;
						case 4:
							user.flags4^=FLAG(c);
							putuserrec(&cfg,user.number,U_FLAGS4,8
								,ultoa(user.flags4,tmp,16));
							break; } }
				break;
			case 'G':
				bputs(text[GoToUser]);
				if(getstr(str,LEN_ALIAS,K_UPPER|K_LINE)) {
					if(isdigit(str[0])) {
						i=atoi(str);
						if(i>lastuser(&cfg))
							break;
						if(i) user.number=i; }
					else {
						i=finduser(str);
						if(i) user.number=i; } }
				break;
			case 'H': /* edit user's information file */
				attr(LIGHTGRAY);
				sprintf(str,"%suser/%4.4u.msg", cfg.data_dir,user.number);
				editfile(str);
				break;
			case 'I':
				maindflts(&user);
				break;
			case 'J':   /* Edit Minutes */
				bputs(text[UeditMinutes]);
				ultoa(user.min,str,10);
				if(getstr(str,10,K_NUMBER|K_LINE))
					putuserrec(&cfg,user.number,U_MIN,10,str);
				break;
			case 'K':	/* date changes */
				bputs(text[UeditLastOn]);
				unixtodstr(&cfg,user.laston,str);
				gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
				if(sys_status&SS_ABORT)
					break;
				user.laston=dstrtounix(&cfg,str);
				putuserrec(&cfg,user.number,U_LASTON,8,ultoa(user.laston,tmp,16));
				bputs(text[UeditFirstOn]);
				unixtodstr(&cfg,user.firston,str);
				gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
				if(sys_status&SS_ABORT)
					break;
				user.firston=dstrtounix(&cfg,str);
				putuserrec(&cfg,user.number,U_FIRSTON,8,ultoa(user.firston,tmp,16));
				bputs(text[UeditExpire]);
				unixtodstr(&cfg,user.expire,str);
				gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
				if(sys_status&SS_ABORT)
					break;
				user.expire=dstrtounix(&cfg,str);
				putuserrec(&cfg,user.number,U_EXPIRE,8,ultoa(user.expire,tmp,16));
				bputs(text[UeditPwModDate]);
				unixtodstr(&cfg,user.pwmod,str);
				gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
				if(sys_status&SS_ABORT)
					break;
				user.pwmod=dstrtounix(&cfg,str);
				putuserrec(&cfg,user.number,U_PWMOD,8,ultoa(user.pwmod,tmp,16));
				break;
			case 'L':
				bputs(text[EnterYourAddress]);
				getstr(user.address,LEN_ADDRESS,K_LINE|K_EDIT|K_AUTODEL);
				if(sys_status&SS_ABORT)
					break;
				putuserrec(&cfg,user.number,U_ADDRESS,LEN_ADDRESS,user.address);
				bputs(text[EnterYourCityState]);
				getstr(user.location,LEN_LOCATION,K_LINE|K_EDIT|K_AUTODEL);
				if(sys_status&SS_ABORT)
					break;
				putuserrec(&cfg,user.number,U_LOCATION,LEN_LOCATION,user.location);
				bputs(text[EnterYourZipCode]);
				getstr(user.zipcode,LEN_ZIPCODE,K_LINE|K_EDIT|K_AUTODEL|K_UPPER);
				if(sys_status&SS_ABORT)
					break;
				putuserrec(&cfg,user.number,U_ZIPCODE,LEN_ZIPCODE,user.zipcode);
				break;
			case 'M':
				bputs(text[UeditML]);
				ultoa(user.level,str,10);
				if(getstr(str,2,K_NUMBER|K_LINE))
					if(!(atoi(str)>useron.level && console&CON_R_INPUT))
						putuserrec(&cfg,user.number,U_LEVEL,2,str);
				break;
			case 'N':
				bputs(text[UeditNote]);
				getstr(user.note,LEN_NOTE,K_LINE|K_EDIT|K_AUTODEL);
				putuserrec(&cfg,user.number,U_NOTE,LEN_NOTE,user.note);
				break;
			case 'O':
				bputs(text[UeditComment]);
				getstr(user.comment,60,K_LINE|K_EDIT|K_AUTODEL);
				putuserrec(&cfg,user.number,U_COMMENT,60,user.comment);
				break;
			case 'P':
				bputs(text[EnterYourPhoneNumber]);
				getstr(user.phone,LEN_PHONE,K_UPPER|K_LINE|K_EDIT|K_AUTODEL);
				putuserrec(&cfg,user.number,U_PHONE,LEN_PHONE,user.phone);
				break;
			case 'Q':
				CLS;
				sys_status&=~SS_INUEDIT;
				FREE_AR(ar);	/* assertion here */
				return;
			case 'R':
				bputs(text[EnterYourRealName]);
				getstr(user.name,LEN_NAME,K_LINE|K_EDIT|K_AUTODEL);
				putuserrec(&cfg,user.number,U_NAME,LEN_NAME,user.name);
				break;
			case 'S':
				bputs(text[EnterYourSex]);
				if(getstr(str,1,K_UPPER|K_LINE))
					putuserrec(&cfg,user.number,U_SEX,1,str);
				break;
			case 'T':   /* Text Search */
				bputs(text[SearchStringPrompt]);
				if(getstr(search,30,K_UPPER|K_LINE))
					stype=SEARCH_TXT;
				break;
			case 'U':
				bputs(text[UeditUlBytes]);
				ultoa(user.ulb,str,10);
				if(getstr(str,10,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
					putuserrec(&cfg,user.number,U_ULB,10,str);
				if(sys_status&SS_ABORT)
					break;
				bputs(text[UeditUploads]);
				sprintf(str,"%u",user.uls);
				if(getstr(str,5,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
					putuserrec(&cfg,user.number,U_ULS,5,str);
				if(sys_status&SS_ABORT)
					break;
				bputs(text[UeditDlBytes]);
				ultoa(user.dlb,str,10);
				if(getstr(str,10,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
					putuserrec(&cfg,user.number,U_DLB,10,str);
				if(sys_status&SS_ABORT)
					break;
				bputs(text[UeditDownloads]);
				sprintf(str,"%u",user.dls);
				if(getstr(str,5,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
					putuserrec(&cfg,user.number,U_DLS,5,str);
				break;
			case 'V':
				CLS;
				attr(LIGHTGRAY);
				for(i=0;i<10;i++) {
					bprintf(text[QuickValidateFmt]
						,i,cfg.val_level[i],ltoaf(cfg.val_flags1[i],str)
						,ltoaf(cfg.val_exempt[i],tmp)
						,ltoaf(cfg.val_rest[i],tmp3)); }
				ASYNC;
				bputs(text[QuickValidatePrompt]);
				c=getkey(0);
				if(!isdigit(c))
					break;
				i=c&0xf;
				user.level=cfg.val_level[i];
				user.flags1=cfg.val_flags1[i];
				user.flags2=cfg.val_flags2[i];
				user.flags3=cfg.val_flags3[i];
				user.flags4=cfg.val_flags4[i];
				user.exempt=cfg.val_exempt[i];
				user.rest=cfg.val_rest[i];
				user.cdt+=cfg.val_cdt[i];
				now=time(NULL);
				if(cfg.val_expire[i]) {
					if(user.expire<now)
						user.expire=now+((long)cfg.val_expire[i]*24L*60L*60L);
					else
						user.expire+=((long)cfg.val_expire[i]*24L*60L*60L); }
				putuserdat(&cfg,&user);
				break;
			case 'W':
				bputs(text[UeditPassword]);
				getstr(user.pass,LEN_PASS,K_UPPER|K_LINE|K_EDIT|K_AUTODEL);
				putuserrec(&cfg,user.number,U_PASS,LEN_PASS,user.pass);
				break;
			case 'X':
				attr(LIGHTGRAY);
				sprintf(str,"%suser/%4.4u.msg", cfg.data_dir,user.number);
				printfile(str,0);
				pause();
				break;
			case 'Y':
				if(!noyes(text[UeditCopyUserQ])) {
					bputs(text[UeditCopyUserToSlot]);
					i=getnum(lastuser(&cfg));
					if((int)i>0) {
						user.number=i;
						putusername(&cfg,user.number,user.alias);
						putuserdat(&cfg,&user); } }
				break;
			case 'Z':
				if(!yesno(text[ChangeRestrictsQ]))
					break;
				while(online) {
					bprintf(text[FlagEditing],ltoaf(user.rest,tmp));
					c=(char)getkeys("ABCDEFGHIJKLMNOPQRSTUVWXYZ?\r",0);
					if(sys_status&SS_ABORT)
						break;
					if(c==CR) break;
					if(c=='?') {
						menu("restrict");
						continue; }
					user.rest^=FLAG(c);
					putuserrec(&cfg,user.number,U_REST,8,ultoa(user.rest,tmp,16)); }
				break;
			case '?':
				CLS;
				menu("uedit");  /* Sysop Uedit Edit Menu */
				pause();
				break;
			case '~':
				bputs(text[UeditLeech]);
				if(getstr(str,2,K_NUMBER|K_LINE))
					putuserrec(&cfg,user.number,U_LEECH,2,ultoa(atoi(str),tmp,16));
				break;
			case '+':
				bputs(text[ModifyCredits]);
				getstr(str,10,K_UPPER|K_LINE);
				l=atol(str);
				if(strstr(str,"M"))
					l*=0x100000L;
				else if(strstr(str,"K"))
					l*=1024;
				else if(strstr(str,"$"))
					l*=cfg.cdt_per_dollar;
				if(l<0L && l*-1 > (long)user.cdt)
					user.cdt=0L;
				else
					user.cdt+=l;
				putuserrec(&cfg,user.number,U_CDT,10,ultoa(user.cdt,tmp,10));
				break;
			case '*':
				bputs(text[ModifyMinutes]);
				getstr(str,10,K_UPPER|K_LINE);
				l=atol(str);
				if(strstr(str,"H"))
					l*=60L;
				if(l<0L && l*-1 > (long)user.min)
					user.min=0L;
				else
					user.min+=l;
				putuserrec(&cfg,user.number,U_MIN,10,ultoa(user.min,tmp,10));
				break;
			case '#': /* read new user questionaire */
				sprintf(str,"%suser/%4.4u.dat", cfg.data_dir,user.number);
				if(!cfg.new_sof[0] || !fexist(str))
					break;
				read_sif_dat(cfg.new_sof,str);
				if(!noyes(text[DeleteQuestionaireQ]))
					remove(str);
				break;
			case '$':
				bputs(text[UeditCredits]);
				ultoa(user.cdt,str,10);
				if(getstr(str,10,K_NUMBER|K_LINE))
					putuserrec(&cfg,user.number,U_CDT,10,str);
				break;
			case '/':
				bputs(text[SearchStringPrompt]);
				if(getstr(artxt,40,K_UPPER|K_LINE))
					stype=SEARCH_ARS;
				FREE_AR(ar);
				ar=arstr(NULL,artxt,&cfg);
				break;
			case '{':
				if(stype==SEARCH_TXT)
					user.number=searchdn(search,user.number);
				else {
					if(!ar)
						break;
					k=user.number;
					for(i=k-1;i;i--) {
						user.number=i;
						getuserdat(&cfg,&user);
						if(chk_ar(ar,&user)) {
							outchar(BEL);
							break; } }
					if(!i)
						user.number=k; }
				break;
			case '}':
				if(stype==SEARCH_TXT)
					user.number=searchup(search,user.number);
				else {
					if(!ar)
						break;
					j=lastuser(&cfg);
					k=user.number;
					for(i=k+1;i<=j;i++) {
						user.number=i;
						getuserdat(&cfg,&user);
						if(chk_ar(ar,&user)) {
							outchar(BEL);
							break; } }
					if(i>j)
						user.number=k; }
				break;
			case ']':
				if(user.number==lastuser(&cfg))
					user.number=1;
				else user.number++;
				break;
			case '[':
				if(user.number==1)
					user.number=lastuser(&cfg);
				else user.number--;
				break; 
		} /* switch */
	} /* while */
	sys_status&=~SS_INUEDIT;
	FREE_AR(ar);
}

/****************************************************************************/
/* Seaches forward through the USER.DAT file for the ocurrance of 'search'  */
/* starting at the offset for usernum+1 and returning the usernumber of the */
/* record where the string was found or the original usernumber if the 		*/
/* string wasn't found														*/
/* Called from the function useredit										*/
/****************************************************************************/
int sbbs_t::searchup(char *search,int usernum)
{
	char userdat[U_LEN+1];
	int file,count;
	uint i=usernum+1;
	long flen;

	if(!search[0])
		return(usernum);
	sprintf(userdat,"%suser/user.dat", cfg.data_dir);
	if((file=nopen(userdat,O_RDONLY|O_DENYNONE))==-1)
		return(usernum);

	flen=filelength(file);
	lseek(file,(long)((long)usernum*U_LEN),0);

	while((i*U_LEN)<=(ulong)flen) {
		count=0;
		while(count<LOOP_NODEDAB
			&& lock(file,(long)((long)(i-1)*U_LEN),U_LEN)==-1) {
			if(count)
				mswait(100);
			count++; 
		}

		if(count>=LOOP_NODEDAB) {
			close(file);
			errormsg(WHERE,ERR_LOCK,"user.dat",i);
			return(usernum); 
		}

		if(read(file,userdat,U_LEN)!=U_LEN) {
			unlock(file,(long)((long)(i-1)*U_LEN),U_LEN);
			close(file);
			errormsg(WHERE,ERR_READ,"user.dat",U_LEN);
			return(usernum); 
		}

		unlock(file,(long)((long)(i-1)*U_LEN),U_LEN);
		userdat[U_LEN]=0;
		strupr(userdat);
		if(strstr(userdat,search)) {
			outchar(BEL);
			close(file);
			return(i); 
		}
		i++; 
	}
	close(file);
	return(usernum);
}

/****************************************************************************/
/* Seaches backward through the USER.DAT file for the ocurrance of 'search' */
/* starting at the offset for usernum-1 and returning the usernumber of the */
/* record where the string was found or the original usernumber if the 		*/
/* string wasn't found														*/
/* Called from the function useredit										*/
/****************************************************************************/
int sbbs_t::searchdn(char *search,int usernum)
{
	char userdat[U_LEN+1];
	int file,count;
	uint i=usernum-1;

	if(!search[0])
		return(usernum);
	sprintf(userdat,"%suser/user.dat", cfg.data_dir);
	if((file=nopen(userdat,O_RDONLY|O_DENYNONE))==-1)
		return(usernum);
	while(i) {
		lseek(file,(long)((long)(i-1)*U_LEN),0);
		count=0;
		while(count<LOOP_NODEDAB
			&& lock(file,(long)((long)(i-1)*U_LEN),U_LEN)==-1) {
			if(count)
				mswait(100);
			count++; 
		}

		if(count>=LOOP_NODEDAB) {
			close(file);
			errormsg(WHERE,ERR_LOCK,"user.dat",i);
			return(usernum); 
		}

		if(read(file,userdat,U_LEN)==-1) {
			unlock(file,(long)((long)(i-1)*U_LEN),U_LEN);
			close(file);
			errormsg(WHERE,ERR_READ,"USER.DAT",U_LEN);
			return(usernum); 
		}
		unlock(file,(long)((long)(i-1)*U_LEN),U_LEN);
		userdat[U_LEN]=0;
		strupr(userdat);
		if(strstr(userdat,search)) {
			outchar(BEL);
			close(file);
			return(i); 
		}
		i--; 
	}
	close(file);
	return(usernum);
}

/****************************************************************************/
/* This function view/edits the users main default settings.				*/
/****************************************************************************/
void sbbs_t::maindflts(user_t* user)
{
	char	str[256],ch;
	char 	tmp[512];
	int		i;

	action=NODE_DFLT;
	while(online) {
		CLS;
		getuserdat(&cfg,user);
		if(user->rows)
			rows=user->rows;
		bprintf(text[UserDefaultsHdr],user->alias,user->number);
		sprintf(str,"%s%s%s%s%s"
							,user->misc&AUTOTERM ? "Auto Detect ":nulstr
							,user->misc&ANSI ? "ANSI ":"TTY "
							,user->misc&COLOR ? "(Color) ":"(Mono) "
							,user->misc&WIP	? "WIP" : user->misc&RIP ? "RIP "
								: user->misc&HTML ? "HTML " : nulstr
							,user->misc&NO_EXASCII ? "ASCII Only":nulstr);
		bprintf(text[UserDefaultsTerminal],str);
		if(cfg.total_xedits)
			bprintf(text[UserDefaultsXeditor]
				,user->xedit ? cfg.xedit[user->xedit-1]->name : "None");
		if(user->rows)
			ultoa(user->rows,tmp,10);
		else
			sprintf(tmp,"Auto Detect (%ld)",rows);
		bprintf(text[UserDefaultsRows],tmp);
		if(cfg.total_shells>1)
			bprintf(text[UserDefaultsCommandSet]
				,cfg.shell[user->shell]->name);
		bprintf(text[UserDefaultsArcType]
			,user->tmpext);
		bprintf(text[UserDefaultsMenuMode]
			,user->misc&EXPERT ? text[On] : text[Off]);
		bprintf(text[UserDefaultsPause]
			,user->misc&UPAUSE ? text[On] : text[Off]);
		bprintf(text[UserDefaultsHotKey]
			,user->misc&COLDKEYS ? text[Off] : text[On]);
		bprintf(text[UserDefaultsCursor]
			,user->misc&SPIN ? text[On] : text[Off]);
		bprintf(text[UserDefaultsCLS]
			,user->misc&CLRSCRN ? text[On] : text[Off]);
		bprintf(text[UserDefaultsAskNScan]
			,user->misc&ASK_NSCAN ? text[On] : text[Off]);
		bprintf(text[UserDefaultsAskSScan]
			,user->misc&ASK_SSCAN ? text[On] : text[Off]);
		bprintf(text[UserDefaultsANFS]
			,user->misc&ANFSCAN ? text[On] : text[Off]);
		bprintf(text[UserDefaultsRemember]
			,user->misc&CURSUB ? text[On] : text[Off]);
		bprintf(text[UserDefaultsBatFlag]
			,user->misc&BATCHFLAG ? text[On] : text[Off]);
		if(cfg.sys_misc&SM_FWDTONET)
			bprintf(text[UserDefaultsNetMail]
				,user->misc&NETMAIL ? text[On] : text[Off]);
		if(startup->options&BBS_OPT_AUTO_LOGON && user->exempt&FLAG('V'))
			bprintf(text[UserDefaultsAutoLogon]
			,user->misc&AUTOLOGON ? text[On] : text[Off]);
		if(useron.exempt&FLAG('Q') || user->misc&QUIET)
			bprintf(text[UserDefaultsQuiet]
				,user->misc&QUIET ? text[On] : text[Off]);
		if(user->prot!=SP)
			sprintf(str,"%c",user->prot);
		else
			strcpy(str,"None");
		bprintf(text[UserDefaultsProtocol],str
			,user->misc&AUTOHANG ? "(Hang-up After Xfer)":nulstr);
		if(cfg.sys_misc&SM_PWEDIT && !(user->rest&FLAG('G')))
			bputs(text[UserDefaultsPassword]);

		ASYNC;
		bputs(text[UserDefaultsWhich]);
		strcpy(str,"HTBALPRSYFNCQXZ\r");
		if(cfg.sys_misc&SM_PWEDIT && !(user->rest&FLAG('G')))
			strcat(str,"W");
		if(useron.exempt&FLAG('Q') || user->misc&QUIET)
			strcat(str,"D");
		if(cfg.total_xedits)
			strcat(str,"E");
		if(cfg.sys_misc&SM_FWDTONET)
			strcat(str,"M");
		if(startup->options&BBS_OPT_AUTO_LOGON && user->exempt&FLAG('V'))
			strcat(str,"I");
		if(cfg.total_shells>1)
			strcat(str,"K");

		ch=(char)getkeys(str,0);
		switch(ch) {
			case 'T':
				if(yesno(text[AutoTerminalQ])) {
					user->misc|=AUTOTERM;
					user->misc&=~(ANSI|RIP|WIP|HTML);
					user->misc|=autoterm; }
				else
					user->misc&=~AUTOTERM;
				if(!(user->misc&AUTOTERM)) {
					if(yesno(text[AnsiTerminalQ]))
						user->misc|=ANSI;
					else
						user->misc&=~(ANSI|COLOR); }
				if(user->misc&ANSI) {
					if(yesno(text[ColorTerminalQ]))
						user->misc|=COLOR;
					else
						user->misc&=~COLOR; }
				if(!yesno(text[ExAsciiTerminalQ]))
					user->misc|=NO_EXASCII;
				else
					user->misc&=~NO_EXASCII;
				if(!(user->misc&AUTOTERM)) {
					if(!noyes(text[RipTerminalQ]))
						user->misc|=RIP;
					else
						user->misc&=~RIP; }
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'B':
				user->misc^=BATCHFLAG;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'E':
				if(noyes("Use an external editor")) {
					putuserrec(&cfg,user->number,U_XEDIT,8,nulstr);
					break; }
				if(user->xedit)
					user->xedit--;
				for(i=0;i<cfg.total_xedits;i++)
					uselect(1,i,"External Editor",cfg.xedit[i]->name, cfg.xedit[i]->ar);
				if((i=uselect(0,user->xedit,0,0,0))>=0)
					putuserrec(&cfg,user->number,U_XEDIT,8,cfg.xedit[i]->code);
				break;
			case 'K':   /* Command shell */
				for(i=0;i<cfg.total_shells;i++)
					uselect(1,i,"Command Shell",cfg.shell[i]->name,cfg.shell[i]->ar);
				if((i=uselect(0,user->shell,0,0,0))>=0)
					putuserrec(&cfg,user->number,U_SHELL,8,cfg.shell[i]->code);
				break;
			case 'A':
				for(i=0;i<cfg.total_fcomps;i++)
					uselect(1,i,"Archive Type",cfg.fcomp[i]->ext,cfg.fcomp[i]->ar);
				if((i=uselect(0,0,0,0,0))>=0)
					putuserrec(&cfg,user->number,U_TMPEXT,3,cfg.fcomp[i]->ext);
				break;
			case 'L':
				bputs(text[HowManyRows]);
				if((ch=(char)getnum(99))!=-1) {
					putuserrec(&cfg,user->number,U_ROWS,2,ultoa(ch,tmp,10));
					if(user==&useron) {
						useron.rows=ch;
						ansi_getlines();
					}
				}
				break;
			case 'P':
				user->misc^=UPAUSE;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'H':
				user->misc^=COLDKEYS;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'S':
				user->misc^=SPIN;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'F':
				user->misc^=ANFSCAN;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'X':
				user->misc^=EXPERT;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'R':   /* Remember current sub/dir */
				user->misc^=CURSUB;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'Y':   /* Prompt for scanning message to you */
				user->misc^=ASK_SSCAN;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'N':   /* Prompt for new message/files scanning */
				user->misc^=ASK_NSCAN;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'M':   /* NetMail address */
				if(noyes(text[ForwardMailQ]))
					user->misc&=~NETMAIL;
				else {
					user->misc|=NETMAIL;
					bputs(text[EnterNetMailAddress]);
					if(!getstr(user->netmail,LEN_NETMAIL,K_EDIT|K_AUTODEL|K_LINE))
						break;
					putuserrec(&cfg,user->number,U_NETMAIL,LEN_NETMAIL,user->netmail); }
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'C':
				user->misc^=CLRSCRN;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'D':
				user->misc^=QUIET;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'I':
				user->misc^=AUTOLOGON;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 'W':
				if(!noyes(text[NewPasswordQ])) {
					bputs(text[CurrentPassword]);
					console|=CON_R_ECHOX;
					if(!(cfg.sys_misc&SM_ECHO_PW))
						console|=CON_L_ECHOX;
					ch=getstr(str,LEN_PASS,K_UPPER);
					console&=~(CON_R_ECHOX|CON_L_ECHOX);
					if(strcmp(str,user->pass)) {
						bputs(text[WrongPassword]);
						pause();
						break; }
					bputs(text[NewPassword]);
					if(!getstr(str,LEN_PASS,K_UPPER|K_LINE))
						break;
					truncsp(str);
					if(!chkpass(str,user,false)) {
						CRLF;
						pause();
						break; }
					bputs(text[VerifyPassword]);
					console|=CON_R_ECHOX;
					if(!(cfg.sys_misc&SM_ECHO_PW))
						console|=CON_L_ECHOX;
					getstr(tmp,LEN_PASS,K_UPPER);
					console&=~(CON_R_ECHOX|CON_L_ECHOX);
					if(strcmp(str,tmp)) {
						bputs(text[WrongPassword]);
						pause();
						break; }
					if(!online)
						break;
					putuserrec(&cfg,user->number,U_PASS,LEN_PASS,str);
					now=time(NULL);
					putuserrec(&cfg,user->number,U_PWMOD,8,ultoa(now,tmp,16));
					bputs(text[PasswordChanged]);
					sprintf(str,"%s changed password",useron.alias);
					logline(nulstr,str);
				}
				sprintf(str,"%suser/%04u.sig",cfg.data_dir,user->number);
				if(fexist(str) && yesno("View signature"))
					printfile(str,P_NOATCODES);
				if(!noyes("Create/Edit signature"))
					editfile(str);
				else if(fexist(str) && !noyes("Delete signature"))
					remove(str);
				break;
			case 'Z':
				menu("dlprot");
				SYNC;
				mnemonics(text[ProtocolOrQuit]);
				strcpy(str,"Q");
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->dlcmd[0] && chk_ar(cfg.prot[i]->ar,&useron)) {
						sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
						strcat(str,tmp); }
				ch=(char)getkeys(str,0);
				if(ch=='Q' || sys_status&SS_ABORT) {
					ch=SP;
					putuserrec(&cfg,user->number,U_PROT,1,&ch); }
				else
					putuserrec(&cfg,user->number,U_PROT,1,&ch);
				if(yesno(text[HangUpAfterXferQ]))
					user->misc|=AUTOHANG;
				else
					user->misc&=~AUTOHANG;
				putuserrec(&cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			default:
				return; } }
}

void sbbs_t::purgeuser(int usernumber)
{
	char str[128];
	user_t user;

	user.number=usernumber;
	getuserdat(&cfg,&user);
	sprintf(str,"Purged %s #%u",user.alias,usernumber);
	logentry("!*",str);
	delallmail(usernumber);
	putusername(&cfg,usernumber,nulstr);
	putuserrec(&cfg,usernumber,U_MISC,8,ultoa(user.misc|DELETED,str,16));
}
