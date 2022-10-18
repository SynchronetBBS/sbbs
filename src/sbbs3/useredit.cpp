/* Synchronet online sysop user editor */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/*******************************************************************/
/* The function useredit(), and functions that are closely related */
/*******************************************************************/

#include "sbbs.h"
#include "petdefs.h"
#include "filedat.h"

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
	float	f;
	long	l;
	long	kmode = K_LINE|K_EDIT|K_AUTODEL|K_TRIM;
	int64_t adj;
	user_t	user;
	struct	tm tm;

	if(!chksyspass())
		return;
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
				return; 
			} 
		}
		unixtodstr(&cfg,time32(NULL),str);
		unixtodstr(&cfg,user.laston,tmp);
		if(strcmp(str,tmp) && user.ltoday) {
			user.ltoday=user.ttoday=user.ptoday=user.etoday=user.textra=0;
			user.freecdt=cfg.level_freecdtperday[user.level];
			putuserdat(&cfg,&user); 	/* Leave alone */
		}
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
			,user.address,getage(&cfg,user.birth),user.sex
			,format_birthdate(&cfg, user.birth, tmp, sizeof(tmp)));
		bprintf(text[UeditLocationZipcode],user.location,user.zipcode);
		bprintf(text[UeditNoteHandle],user.note,user.handle);
		bprintf(text[UeditComputerModem],user.comp,user.modem);
		bprintf(text[UserIpAddr],user.ipaddr);
		if(user.netmail[0])
			bprintf(text[UserNetMail],user.netmail);

		SAFEPRINTF2(str,"%suser/%4.4u.msg", cfg.data_dir,user.number);
		i=fexist(str);
		if(user.comment[0] || i)
			bprintf(text[UeditCommentLine],i ? '+' : ' '
				,user.comment);
		else
			CRLF;
		if(localtime32(&user.laston,&tm)==NULL)
			return;
		bprintf(text[UserDates]
			,unixtodstr(&cfg,user.firston,str),unixtodstr(&cfg,user.expire,tmp)
			,unixtodstr(&cfg,user.laston,tmp2),tm.tm_hour, tm.tm_min);

		bprintf(text[UserTimes]
			,user.timeon,user.ttoday,cfg.level_timeperday[user.level]
			,user.tlast,cfg.level_timepercall[user.level],user.textra);
		if(user.posts)
			f=(float)user.logons/user.posts;
		else
			f=0;
		bprintf(text[UserLogons]
			,user.logons,user.ltoday,cfg.level_callsperday[user.level],user.posts
			,f ? (uint)(100/f) : user.posts>user.logons ? 100 : 0
			,user.ptoday);
		bprintf(text[UserEmails]
			,user.emails,user.fbacks,getmail(&cfg,user.number,/* Sent: */FALSE, /* SPAM: */FALSE),user.etoday);

		bprintf(text[UserUploads], byte_estimate_to_str(user.ulb, tmp, sizeof(tmp), 1, 1), user.uls);
		if(user.leech)
			SAFEPRINTF(str,text[UserLeech],user.leech);
		else
			str[0]=0;
		bprintf(text[UserDownloads],byte_estimate_to_str(user.dlb, tmp, sizeof(tmp), 1, 1) ,user.dls,str);
		bprintf(text[UserCredits],byte_estimate_to_str(user.cdt, tmp, sizeof(tmp), 1, 1)
			,byte_estimate_to_str(user.freecdt, tmp2, sizeof(tmp2), 1, 1)
			,byte_estimate_to_str(cfg.level_freecdtperday[user.level], str, sizeof(str), 1, 1));
		bprintf(text[UserMinutes],ultoac(user.min,tmp));
		bprintf(text[UeditSecLevel],user.level);
		bprintf(text[UeditFlags],u32toaf(user.flags1,tmp),u32toaf(user.flags3,tmp2)
			,u32toaf(user.flags2,tmp3),u32toaf(user.flags4,str));
		bprintf(text[UeditExempts],u32toaf(user.exempt,tmp),u32toaf(user.rest,tmp2));
		if(lncntr>=rows-2)
			lncntr=0;
		if(user.misc&DELETED)
			center(text[Deleted]);
		else if(user.misc&INACTIVE)
			center(text[Inactive]);
		else
			CRLF;
		l=lastuser(&cfg);
		ASYNC;
		bprintf(text[UeditPrompt],user.number,l);
		SAFEPRINTF4(str, "QG[]?/{}%c%c%c%c", TERM_KEY_LEFT, TERM_KEY_RIGHT, TERM_KEY_HOME, TERM_KEY_END);
		if(user.level <= useron.level)
			SAFECAT(str, "ABCDEFHIJKLMNOPRSTUVWXYZ+~*$#");
		l=getkeys(str, l, K_UPPER|K_NOCRLF);
		if(l&0x80000000L) {
			user.number=(ushort)(l&~0x80000000L);
			continue; 
		}
		if(l >= ' ' && l != '[' && l != ']' && l != '{' && l != '}' && l != '?')
			newline();
		switch(l) {
			case 'A':
				bputs(text[EnterYourAlias]);
				getstr(user.alias,LEN_ALIAS,kmode);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_ALIAS, user.alias);
				if(!(user.misc&DELETED))
					putusername(&cfg,user.number,user.alias);
				bputs(text[EnterYourHandle]);
				getstr(user.handle,LEN_HANDLE,kmode);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_HANDLE, user.handle);
				break;
			case 'B':
				bprintf(text[EnterYourBirthday], birthdate_format(&cfg));
				format_birthdate(&cfg, user.birth, str, sizeof(str));
				if(gettmplt(str, "nn/nn/nnnn", kmode) == 10) {
					parse_birthdate(&cfg, str, user.birth, sizeof(user.birth));
					putuserstr(&cfg, user.number, USER_BIRTH, user.birth);
				}
				break;
			case 'C':
				bputs(text[EnterYourComputer]);
				getstr(user.comp,LEN_COMP,kmode);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_HOST, user.comp);
				break;
			case 'D':
				if(user.misc&DELETED) {
					if(!noyes(text[UeditRestoreQ])) {
						putusermisc(&cfg, user.number, user.misc &~ DELETED);
						putusername(&cfg,user.number,user.alias); 
					}
					break; 
				}
				if(user.misc&INACTIVE) {
					if(!noyes(text[UeditActivateQ]))
						putusermisc(&cfg, user.number, user.misc &~ INACTIVE);
					break; 
				}
				if(!noyes(text[UeditDeleteQ])) {
					getsmsg(user.number);
					if(getmail(&cfg,user.number, /* Sent: */FALSE, /* SPAM: */FALSE)) {
						if(yesno(text[UeditReadUserMailWQ]))
							readmail(user.number,MAIL_YOUR); 
					}
					if(getmail(&cfg,user.number, /* Sent: */TRUE, /* SPAM: */FALSE)) {
						if(yesno(text[UeditReadUserMailSQ]))
							readmail(user.number,MAIL_SENT); 
					}
					putusermisc(&cfg, user.number, user.misc | DELETED);
					putusername(&cfg,user.number,nulstr);
					break; 
				}
				if(!noyes(text[UeditDeactivateUserQ])) {
					if(getmail(&cfg,user.number, /* Sent: */FALSE, /* SPAM: */FALSE)) {
						if(yesno(text[UeditReadUserMailWQ]))
							readmail(user.number,MAIL_YOUR); 
					}
					if(getmail(&cfg,user.number, /* Sent: */TRUE, /* SPAM: */FALSE)) {
						if(yesno(text[UeditReadUserMailSQ]))
							readmail(user.number,MAIL_SENT); 
					}
					putusermisc(&cfg, user.number, user.misc | INACTIVE);
					break; 
				}
				break;
			case 'E':
				if(!yesno(text[ChangeExemptionQ]))
					break;
				while(online) {
					bprintf(text[FlagEditing], u32toaf(user.exempt,tmp));
					c=(char)getkeys("ABCDEFGHIJKLMNOPQRSTUVWXYZ?\r",0);
					if(sys_status&SS_ABORT)
						break;
					if(c==CR) break;
					if(c=='?') {
						menu("exempt");
						continue; 
					}
					user.exempt^=FLAG(c);
					putuserflags(&cfg, user.number, USER_EXEMPT, user.exempt); 
				}
				break;
			case 'F':
				i=1;
				while(online) {
					bprintf("\r\nFlag Set #%d\r\n",i);
					switch(i) {
						case 1:
							bprintf(text[FlagEditing], u32toaf(user.flags1,tmp));
							break;
						case 2:
							bprintf(text[FlagEditing], u32toaf(user.flags2,tmp));
							break;
						case 3:
							bprintf(text[FlagEditing], u32toaf(user.flags3,tmp));
							break;
						case 4:
							bprintf(text[FlagEditing], u32toaf(user.flags4,tmp));
							break; 
					}
					c=(char)getkeys("ABCDEFGHIJKLMNOPQRSTUVWXYZ?1234\r",0);
					if(sys_status&SS_ABORT)
						break;
					if(c==CR) break;
					if(c=='?') {
						SAFEPRINTF(str,"flags%d",i);
						menu(str);
						continue; 
					}
					if(IS_DIGIT(c)) {
						i=c&0xf;
						continue; 
					}
					switch(i) {
						case 1:
							user.flags1^=FLAG(c);
							putuserflags(&cfg, user.number, USER_FLAGS1, user.flags1);
							break;
						case 2:
							user.flags2^=FLAG(c);
							putuserflags(&cfg, user.number, USER_FLAGS2, user.flags2);
							break;
						case 3:
							user.flags3^=FLAG(c);
							putuserflags(&cfg, user.number, USER_FLAGS3, user.flags3);
							break;
						case 4:
							user.flags4^=FLAG(c);
							putuserflags(&cfg, user.number, USER_FLAGS4, user.flags4);
							break; 
					}
				}
				break;
			case 'G':
				bputs(text[GoToUser]);
				if(getstr(str,LEN_ALIAS,K_UPPER|K_LINE)) {
					if(IS_DIGIT(str[0])) {
						i=atoi(str);
						if(i>lastuser(&cfg))
							break;
						if(i) user.number=i; 
					}
					else {
						i=finduser(str);
						if(i) user.number=i; 
					} 
				}
				break;
			case 'H': /* edit user's information file */
				attr(LIGHTGRAY);
				SAFEPRINTF2(str,"%suser/%4.4u.msg", cfg.data_dir,user.number);
				editfile(str);
				break;
			case 'I':
				maindflts(&user);
				break;
			case 'J':   /* Edit Minutes */
				bputs(text[UeditMinutes]);
				ultoa(user.min,str,10);
				if(getstr(str,10,K_NUMBER|K_LINE))
					putuserstr(&cfg, user.number, USER_MIN, str);
				break;
			case 'K':	/* date changes */
				bputs(text[UeditLastOn]);
				unixtodstr(&cfg,user.laston,str);
				gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
				if(sys_status&SS_ABORT)
					break;
				user.laston=dstrtounix(&cfg,str);
				putuserdatetime(&cfg, user.number, USER_LASTON, user.laston);
				bputs(text[UeditFirstOn]);
				unixtodstr(&cfg,user.firston,str);
				gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
				if(sys_status&SS_ABORT)
					break;
				user.firston=dstrtounix(&cfg,str);
				putuserdatetime(&cfg, user.number, USER_FIRSTON, user.firston);
				bputs(text[UeditExpire]);
				unixtodstr(&cfg,user.expire,str);
				gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
				if(sys_status&SS_ABORT)
					break;
				user.expire=dstrtounix(&cfg,str);
				putuserdatetime(&cfg, user.number, USER_EXPIRE, user.expire);
				bputs(text[UeditPwModDate]);
				unixtodstr(&cfg,user.pwmod,str);
				gettmplt(str,"nn/nn/nn",K_LINE|K_EDIT);
				if(sys_status&SS_ABORT)
					break;
				user.pwmod=dstrtounix(&cfg,str);
				putuserdatetime(&cfg, user.number, USER_PWMOD, user.pwmod);
				break;
			case 'L':
				bputs(text[EnterYourAddress]);
				getstr(user.address,LEN_ADDRESS,kmode);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_ADDRESS, user.address);
				bputs(text[EnterYourCityState]);
				getstr(user.location,LEN_LOCATION,kmode);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_LOCATION, user.location);
				bputs(text[EnterYourZipCode]);
				getstr(user.zipcode,LEN_ZIPCODE,K_LINE|K_EDIT|K_AUTODEL|K_UPPER);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_ZIPCODE, user.zipcode);
				break;
			case 'M':
				bputs(text[UeditML]);
				ultoa(user.level,str,10);
				if(getstr(str,2,K_NUMBER|K_LINE))
					putuserstr(&cfg, user.number, USER_LEVEL, str);
				break;
			case 'N':
				bputs(text[UeditNote]);
				getstr(user.note,LEN_NOTE,K_LINE|K_EDIT|K_AUTODEL);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_NOTE, user.note);
				break;
			case 'O':
				bputs(text[UeditComment]);
				getstr(user.comment,60,kmode);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_COMMENT, user.comment);
				break;
			case 'P':
				bputs(text[EnterYourPhoneNumber]);
				getstr(user.phone,LEN_PHONE,kmode);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_PHONE, user.phone);
				break;
			case 'Q':
				lncntr = 0;
				CLS;
				sys_status&=~SS_INUEDIT;
				free(ar);	/* assertion here */
				return;
			case 'R':
				bputs(text[EnterYourRealName]);
				getstr(user.name,LEN_NAME,kmode);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_NAME, user.name);
				break;
			case 'S':
				bputs(text[EnterYourGender]);
				if(getstr(str,1,K_UPPER|K_LINE))
					putuserstr(&cfg, user.number, USER_GENDER, str);
				break;
			case 'T':   /* Text Search */
				bputs(text[SearchStringPrompt]);
				if(getstr(search,30,K_UPPER|K_LINE))
					stype=SEARCH_TXT;
				break;
			case 'U':
				bputs(text[UeditUlBytes]);
				_ui64toa(user.ulb,str,10);
				if(getstr(str,19,K_UPPER|K_LINE|K_EDIT|K_AUTODEL)) {
					user.ulb = parse_byte_count(str, 1);
					putuserdec64(&cfg, user.number, USER_ULB, user.ulb);
				}
				if(sys_status&SS_ABORT)
					break;
				bputs(text[UeditUploads]);
				SAFEPRINTF(str,"%u",user.uls);
				if(getstr(str,5,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
					putuserstr(&cfg, user.number, USER_ULS, str);
				if(sys_status&SS_ABORT)
					break;
				bputs(text[UeditDlBytes]);
				_ui64toa(user.dlb,str,10);
				if(getstr(str,19,K_UPPER|K_LINE|K_EDIT|K_AUTODEL)) {
					user.dlb = parse_byte_count(str, 1);
					putuserdec64(&cfg, user.number, USER_DLB, user.dlb);
				}
				if(sys_status&SS_ABORT)
					break;
				bputs(text[UeditDownloads]);
				SAFEPRINTF(str,"%u",user.dls);
				if(getstr(str,5,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL))
					putuserstr(&cfg, user.number, USER_DLS, str);
				break;
			case 'V':
				CLS;
				attr(LIGHTGRAY);
				for(i=0;i<10;i++) {
					bprintf(text[QuickValidateFmt]
						,i,cfg.val_level[i], u32toaf(cfg.val_flags1[i], str)
						,u32toaf(cfg.val_exempt[i], tmp)
						,u32toaf(cfg.val_rest[i], tmp3)); 
				}
				ASYNC;
				bputs(text[QuickValidatePrompt]);
				c=getkey(0);
				if(!IS_DIGIT(c))
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
						user.expire=(time32_t)(now+((long)cfg.val_expire[i]*24L*60L*60L));
					else
						user.expire+=((long)cfg.val_expire[i]*24L*60L*60L); 
				}
				putuserdat(&cfg,&user);
				break;
			case 'W':
				bputs(text[UeditPassword]);
				getstr(user.pass,LEN_PASS,K_UPPER|kmode);
				if(sys_status&SS_ABORT)
					break;
				putuserstr(&cfg, user.number, USER_PASS, user.pass);
				break;
			case 'X':
				attr(LIGHTGRAY);
				SAFEPRINTF2(str,"%suser/%4.4u.msg", cfg.data_dir,user.number);
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
						putuserdat(&cfg,&user); 
					} 
				}
				break;
			case 'Z':
				if(!yesno(text[ChangeRestrictsQ]))
					break;
				while(online) {
					bprintf(text[FlagEditing], u32toaf(user.rest, tmp));
					c=(char)getkeys("ABCDEFGHIJKLMNOPQRSTUVWXYZ?\r",0);
					if(sys_status&SS_ABORT)
						break;
					if(c==CR) break;
					if(c=='?') {
						menu("restrict");
						continue; 
					}
					user.rest^=FLAG(c);
					putuserflags(&cfg, user.number, USER_REST, user.rest); 
				}
				break;
			case '?':
				CLS;
				menu("uedit");  /* Sysop Uedit Edit Menu */
				pause();
				break;
			case '~':
				bputs(text[UeditLeech]);
				if(getstr(str,2,K_NUMBER|K_LINE))
					putuserdec32(&cfg, user.number, USER_LEECH, atoi(str));
				break;
			case '+':
				bputs(text[ModifyCredits]);
				if(getstr(str,10,K_UPPER|K_LINE)) {
					if(strstr(str,"$"))
						adj = strtoll(str, NULL, 10) * cfg.cdt_per_dollar;
					else
						adj = parse_byte_count(str, 1);
					adjustuserval(&cfg, user.number, USER_CDT, adj);
				}
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
				putuserdec32(&cfg, user.number, USER_MIN, user.min);
				break;
			case '#': /* read new user questionnaire */
				SAFEPRINTF2(str,"%suser/%4.4u.dat", cfg.data_dir,user.number);
				if(!cfg.new_sof[0] || !fexist(str))
					break;
				read_sif_dat(cfg.new_sof,str);
				if(!noyes(text[DeleteQuestionaireQ]))
					remove(str);
				break;
			case '$':
				bputs(text[UeditCredits]);
				_ui64toa(user.cdt,str,10);
				if(getstr(str,19,K_UPPER|K_LINE|K_EDIT|K_AUTODEL)) {
					user.cdt = parse_byte_count(str, 1);
					putuserdec64(&cfg, user.number, USER_CDT, user.cdt);
				}
				break;
			case '/':
				bputs(text[SearchStringPrompt]);
				if(getstr(artxt,40,K_UPPER|K_LINE))
					stype=SEARCH_ARS;
				ar=arstr(NULL,artxt,&cfg,NULL);
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
						if(chk_ar(ar,&user,/* client: */NULL)) {
							break; 
						} 
					}
					if(!i)
						user.number=k; 
				}
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
						if(chk_ar(ar,&user,/* client: */NULL)) {
							break; 
						} 
					}
					if(i>j)
						user.number=k; 
				}
				break;
			case TERM_KEY_RIGHT:
			case ']':
				if(user.number==lastuser(&cfg))
					user.number=1;
				else user.number++;
				break;
			case TERM_KEY_LEFT:
			case '[':
				if(user.number==1)
					user.number=lastuser(&cfg);
				else user.number--;
				break;
			case TERM_KEY_HOME:
				user.number = 1;
				break;
			case TERM_KEY_END:
				user.number = lastuser(&cfg);
				break;
		} /* switch */
	} /* while */
	sys_status&=~SS_INUEDIT;
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
	char userdat[USER_RECORD_LEN + 1];
	int file;
	uint i=usernum+1;

	if(!search[0])
		return(usernum);
	if((file=openuserdat(&cfg, /* for_modify: */FALSE))==-1)
		return(usernum);

	int last = (int)filelength(file) / USER_RECORD_LEN;

	while(readuserdat(&cfg, i, userdat, sizeof(userdat), file) == 0) {
		strupr(userdat);
		if(strstr(userdat,search)) {
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
	char userdat[USER_RECORD_LEN + 1];
	int file;
	uint i=usernum-1;

	if(!search[0])
		return(usernum);
	if((file = openuserdat(&cfg, /* for_modify: */FALSE)) == -1)
		return(usernum);

	while(i > 0 && readuserdat(&cfg, i, userdat, sizeof(userdat), file) == 0) {
		strupr(userdat);
		if(strstr(userdat,search)) {
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
		if(user->rows != TERM_ROWS_AUTO)
			rows = user->rows;
		if(user->cols != TERM_COLS_AUTO)
			cols = user->cols;
		bprintf(text[UserDefaultsHdr],user->alias,user->number);
		if(user == &useron)
			update_nodeterm();
		long term = (user == &useron) ? term_supports() : user->misc;
		if(term&PETSCII)
			safe_snprintf(str,sizeof(str),"%sCBM/PETSCII"
							,user->misc&AUTOTERM ? text[TerminalAutoDetect]:nulstr);
		else
			safe_snprintf(str,sizeof(str),"%s%s / %s %s%s%s"
							,user->misc&AUTOTERM ? text[TerminalAutoDetect]:nulstr
							,term_charset(term)
							,term_type(term)
							,term&COLOR ? (term&ICE_COLOR ? text[TerminalIceColor] : text[TerminalColor]) : text[TerminalMonochrome]
							,term&MOUSE ? text[TerminalMouse] : ""
							,term&SWAP_DELETE ? "DEL=BS" : nulstr);
		add_hotspot('T');
		bprintf(text[UserDefaultsTerminal], truncsp(str));
		safe_snprintf(str, sizeof(str), "%s%ld %s,", user->cols ? nulstr:text[TerminalAutoDetect], cols, text[TerminalColumns]);
		safe_snprintf(tmp, sizeof(tmp), "%s%ld %s", user->rows ? nulstr:text[TerminalAutoDetect], rows, text[TerminalRows]);
		add_hotspot('L');
		bprintf(text[UserDefaultsRows], str, tmp);
		if(cfg.total_shells>1) {
			add_hotspot('K');
			bprintf(text[UserDefaultsCommandSet]
				,cfg.shell[user->shell]->name);
		}
		if(cfg.total_xedits) {
			add_hotspot('E');
			bprintf(text[UserDefaultsXeditor]
				,user->xedit ? cfg.xedit[user->xedit-1]->name : "None");
		}
		add_hotspot('A');
		bprintf(text[UserDefaultsArcType]
			,user->tmpext);
		add_hotspot('X');
		bprintf(text[UserDefaultsMenuMode]
			,user->misc&EXPERT ? text[On] : text[Off]);
		add_hotspot('P');
		bprintf(text[UserDefaultsPause]
			,user->misc&UPAUSE ? text[On] : text[Off]);
		add_hotspot('H');
		bprintf(text[UserDefaultsHotKey]
			,user->misc&COLDKEYS ? text[Off] : text[On]);
		add_hotspot('S');
		bprintf(text[UserDefaultsCursor]
			,user->misc&SPIN ? text[On] : user->misc&NOPAUSESPIN ? text[Off] : "Pause Prompt Only");
		add_hotspot('C');
		bprintf(text[UserDefaultsCLS]
			,user->misc&CLRSCRN ? text[On] : text[Off]);
		add_hotspot('N');
		bprintf(text[UserDefaultsAskNScan]
			,user->misc&ASK_NSCAN ? text[On] : text[Off]);
		add_hotspot('Y');
		bprintf(text[UserDefaultsAskSScan]
			,user->misc&ASK_SSCAN ? text[On] : text[Off]);
		add_hotspot('F');
		bprintf(text[UserDefaultsANFS]
			,user->misc&ANFSCAN ? text[On] : text[Off]);
		add_hotspot('R');
		bprintf(text[UserDefaultsRemember]
			,user->misc&CURSUB ? text[On] : text[Off]);
		add_hotspot('B');
		bprintf(text[UserDefaultsBatFlag]
			,user->misc&BATCHFLAG ? text[On] : text[Off]);
		if(cfg.sys_misc&SM_FWDTONET) {
			add_hotspot('M');
			bprintf(text[UserDefaultsNetMail]
				,user->misc&NETMAIL ? text[On] : text[Off]
				,user->netmail);
		}
		if(startup->options&BBS_OPT_AUTO_LOGON && user->exempt&FLAG('V')) {
			add_hotspot('I');
			bprintf(text[UserDefaultsAutoLogon]
			,user->misc&AUTOLOGON ? text[On] : text[Off]);
		}
		if(user->exempt&FLAG('Q') || user->misc&QUIET) {
			add_hotspot('D');
			bprintf(text[UserDefaultsQuiet]
				,user->misc&QUIET ? text[On] : text[Off]);
		}
		SAFECOPY(str,"None");
		for(i=0;i<cfg.total_prots;i++) {
			if(user->prot==cfg.prot[i]->mnemonic) {
				SAFECOPY(str,cfg.prot[i]->name);
				break;
			}
		}
		add_hotspot('Z');
		bprintf(text[UserDefaultsProtocol],str
			,user->misc&AUTOHANG ? "(Auto-Hangup)":nulstr);
		add_hotspot('W');
		if(cfg.sys_misc&SM_PWEDIT && !(user->rest&FLAG('G')))
			bputs(text[UserDefaultsPassword]);

		ASYNC;
		bputs(text[UserDefaultsWhich]);
		SAFECOPY(str,"HTBALPRSYFNCQXZ\r");
		if(cfg.sys_misc&SM_PWEDIT && !(user->rest&FLAG('G')))
			strcat(str,"W");
		if(user->exempt&FLAG('Q') || user->misc&QUIET)
			strcat(str,"D");
		if(cfg.total_xedits)
			strcat(str,"E");
		if(cfg.sys_misc&SM_FWDTONET)
			strcat(str,"M");
		if(startup->options&BBS_OPT_AUTO_LOGON && user->exempt&FLAG('V'))
			strcat(str,"I");
		if(cfg.total_shells>1)
			strcat(str,"K");

		add_hotspot('Q');
		ch=(char)getkeys(str,0);
		switch(ch) {
			case 'T':
				if(yesno(text[AutoTerminalQ])) {
					user->misc |= AUTOTERM;
					user->misc &= ~(ANSI|RIP|WIP|HTML|PETSCII|UTF8);
				}
				else
					user->misc &= ~AUTOTERM;
				if(sys_status&SS_ABORT)
					break;
				if(!(user->misc&AUTOTERM)) {
					if(!noyes(text[Utf8TerminalQ]))
						user->misc |= UTF8;
					else
						user->misc &= ~UTF8;
					if(yesno(text[AnsiTerminalQ])) {
						user->misc |= ANSI;
						user->misc &= ~PETSCII;
					} else if(!(user->misc&UTF8)) {
						user->misc &= ~(ANSI|COLOR|ICE_COLOR);
						if(!noyes(text[PetTerminalQ]))
							user->misc |= PETSCII|COLOR;
						else
							user->misc &= ~PETSCII;
					}
				}
				if(sys_status&SS_ABORT)
					break;
				term = (user == &useron) ? term_supports() : user->misc;
				if(term&(AUTOTERM|ANSI) && !(term&PETSCII)) {
					user->misc |= COLOR;
					user->misc &= ~ICE_COLOR;
					if((user->misc&AUTOTERM) || yesno(text[ColorTerminalQ])) {
						if(!(console&(CON_BLINK_FONT|CON_HBLINK_FONT))
							&& !noyes(text[IceColorTerminalQ]))
							user->misc |= ICE_COLOR;
					} else
						user->misc &= ~COLOR;
				}
				if(sys_status&SS_ABORT)
					break;
				if(term&ANSI) {
					if(text[MouseTerminalQ][0] && yesno(text[MouseTerminalQ]))
						user->misc |= MOUSE;
					else
						user->misc &= ~MOUSE;
				}
				if(sys_status&SS_ABORT)
					break;
				if(!(term&PETSCII)) {
					if(!(term&UTF8) && !yesno(text[ExAsciiTerminalQ]))
						user->misc|=NO_EXASCII;
					else
						user->misc&=~NO_EXASCII;
					user->misc &= ~SWAP_DELETE;
					while(text[HitYourBackspaceKey][0] && !(user->misc&(PETSCII|SWAP_DELETE)) && online) {
						bputs(text[HitYourBackspaceKey]);
						uchar key = getkey(K_CTRLKEYS);
						bprintf(text[CharacterReceivedFmt], key, key);
						if(key == '\b')
							break;
						if(key == DEL) {
							if(text[SwapDeleteKeyQ][0] == 0 || yesno(text[SwapDeleteKeyQ]))
								user->misc |= SWAP_DELETE;
						}
						else if(key == PETSCII_DELETE) {
							autoterm |= PETSCII;
							user->misc |= PETSCII;
							outcom(PETSCII_UPPERLOWER);
							bputs(text[PetTerminalDetected]);
						}
						else
							bprintf(text[InvalidBackspaceKeyFmt], key, key);
					}
				}
				if(sys_status&SS_ABORT)
					break;
				if(!(user->misc&AUTOTERM) && (term&(ANSI|NO_EXASCII)) == ANSI) {
					if(!noyes(text[RipTerminalQ]))
						user->misc|=RIP;
					else
						user->misc&=~RIP; 
				}
				if(sys_status&SS_ABORT)
					break;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'B':
				user->misc^=BATCHFLAG;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'E':
				if(noyes(text[UseExternalEditorQ])) {
					putuserstr(&cfg, user->number, USER_XEDIT, nulstr);
					break; 
				}
				if(user->xedit)
					user->xedit--;
				for(i=0;i<cfg.total_xedits;i++)
					uselect(1,i,text[ExternalEditorHeading],cfg.xedit[i]->name, cfg.xedit[i]->ar);
				if((i=uselect(0,user->xedit,0,0,0))>=0)
					putuserstr(&cfg, user->number, USER_XEDIT, cfg.xedit[i]->code);
				break;
			case 'K':   /* Command shell */
				for(i=0;i<cfg.total_shells;i++)
					uselect(1,i,text[CommandShellHeading],cfg.shell[i]->name,cfg.shell[i]->ar);
				if((i=uselect(0,user->shell,0,0,0))>=0)
					putuserstr(&cfg, user->number, USER_SHELL, cfg.shell[i]->code);
				break;
			case 'A':
			{
				str_list_t ext_list = strListDup((str_list_t)supported_archive_formats);
				for(i=0; i < cfg.total_fcomps; i++) {
					if(strListFind(ext_list, cfg.fcomp[i]->ext, /* case-sensitive */FALSE) < 0
						&& chk_ar(cfg.fcomp[i]->ar, &useron, &client))
						strListPush(&ext_list, cfg.fcomp[i]->ext);
				}
				for(i=0; ext_list[i] != NULL; i++)
					uselect(1,i,text[ArchiveTypeHeading], ext_list[i], NULL);
				if((i=uselect(0,0,0,0,0))>=0)
					putuserstr(&cfg, user->number, USER_TMPEXT, ext_list[i]);
				strListFree(&ext_list);
				break;
			}
			case 'L':
				bputs(text[HowManyColumns]);
				if((i = getnum(TERM_COLS_MAX)) < 0)
					break;
				putuserdec32(&cfg, user->number, USER_COLS, i);
				if(user==&useron) {
					useron.cols = i;
					ansi_getlines();
				}
				bputs(text[HowManyRows]);
				if((i = getnum(TERM_ROWS_MAX)) < 0)
					break;
				putuserdec32(&cfg, user->number, USER_ROWS, i);
				if(user==&useron) {
					useron.rows = i;
					ansi_getlines();
				}
				break;
			case 'P':
				user->misc^=UPAUSE;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'H':
				user->misc^=COLDKEYS;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'S':
				user->misc^=SPIN;
				if(!(user->misc&SPIN)) {
					if(!yesno(text[SpinningCursorOnPauseQ]))
						user->misc|=NOPAUSESPIN;
					else
						user->misc&=~NOPAUSESPIN;
				}
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'F':
				user->misc^=ANFSCAN;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'X':
				user->misc^=EXPERT;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'R':   /* Remember current sub/dir */
				user->misc^=CURSUB;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'Y':   /* Prompt for scanning message to you */
				user->misc^=ASK_SSCAN;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'N':   /* Prompt for new message/files scanning */
				user->misc^=ASK_NSCAN;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'M':   /* NetMail address */
				bputs(text[EnterNetMailAddress]);
				getstr(user->netmail,LEN_NETMAIL,K_LINE|K_EDIT|K_AUTODEL|K_TRIM);
				if(sys_status&SS_ABORT)
					break;
				user->misc &= ~NETMAIL;
				if(is_supported_netmail_addr(&cfg, user->netmail) && !noyes(text[ForwardMailQ]))
					user->misc |= NETMAIL;
				putusermisc(&cfg, user->number, user->misc);
				putuserstr(&cfg, user->number, USER_NETMAIL, user->netmail); 
				break;
			case 'C':
				user->misc^=CLRSCRN;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'D':
				user->misc^=QUIET;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'I':
				user->misc^=AUTOLOGON;
				putusermisc(&cfg, user->number, user->misc);
				break;
			case 'W':
				if(!noyes(text[NewPasswordQ])) {
					bputs(text[CurrentPassword]);
					console|=CON_R_ECHOX;
					ch=(char)getstr(str,LEN_PASS,K_UPPER);
					console&=~(CON_R_ECHOX|CON_L_ECHOX);
					if(sys_status&SS_ABORT)
						break;
					if(stricmp(str,user->pass)) {
						bputs(text[WrongPassword]);
						pause();
						break; 
					}
					bprintf(text[NewPasswordPromptFmt], cfg.min_pwlen, LEN_PASS);
					if(!getstr(str,LEN_PASS,K_UPPER|K_LINE|K_TRIM))
						break;
					truncsp(str);
					if(!chkpass(str,user,false)) {
						CRLF;
						pause();
						break; 
					}
					bputs(text[VerifyPassword]);
					console|=CON_R_ECHOX;
					getstr(tmp,LEN_PASS*2,K_UPPER);
					if(sys_status&SS_ABORT)
						break;
					console&=~(CON_R_ECHOX|CON_L_ECHOX);
					if(strcmp(str,tmp)) {
						bputs(text[WrongPassword]);
						pause();
						break; 
					}
					if(!online)
						break;
					putuserstr(&cfg, user->number, USER_PASS, str);
					now=time(NULL);
					putuserdatetime(&cfg, user->number, USER_PWMOD, now);
					bputs(text[PasswordChanged]);
					logline(LOG_NOTICE,nulstr,"changed password");
				}
				SAFEPRINTF2(str,"%suser/%04u.sig",cfg.data_dir,user->number);
				if(fexist(str) && yesno(text[ViewSignatureQ]))
					printfile(str,P_NOATCODES);
				if(!noyes(text[CreateEditSignatureQ]))
					editfile(str);
				else if(fexist(str) && !noyes(text[DeleteSignatureQ]))
					remove(str);
				break;
			case 'Z':
				xfer_prot_menu(XFER_DOWNLOAD);
				SYNC;
				mnemonics(text[ProtocolOrQuit]);
				sprintf(str,"%c",quit_key());
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->dlcmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client)) {
						SAFEPRINTF(tmp,"%c",cfg.prot[i]->mnemonic);
						strcat(str,tmp); 
					}
				ch=(char)getkeys(str,0);
				if(ch==quit_key() || sys_status&SS_ABORT) {
					ch=' ';
					putuserstr(&cfg, user->number, USER_PROT, ""); 
				}
				else {
					tmp[0] = ch;
					tmp[1] = '\0';
					putuserstr(&cfg, user->number, USER_PROT, tmp);
				}
				if(yesno(text[HangUpAfterXferQ]))
					user->misc|=AUTOHANG;
				else
					user->misc&=~AUTOHANG;
				putusermisc(&cfg, user->number, user->misc);
				break;
			default:
				clear_hotspots();
				return; 
		}
	}
}

void sbbs_t::purgeuser(int usernumber)
{ 	char str[128];
	user_t user;

	user.number=usernumber;
	getuserdat(&cfg,&user);
	SAFEPRINTF2(str,"Purged %s #%u",user.alias,usernumber);
	logentry("!*",str);
	delallmail(usernumber, MAIL_ANY);
	putusername(&cfg,usernumber,nulstr);
	putusermisc(&cfg, usernumber, user.misc | DELETED);
}
