/* newuser.cpp */

/* Synchronet new user routine */

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
#include "cmdshell.h"

static char* ContinueText = "Continue";

/****************************************************************************/
/* This function is invoked when a user enters "NEW" at the NN: prompt		*/
/* Prompts user for personal information and then sends feedback to sysop.  */
/* Called from function waitforcall											*/
/****************************************************************************/
BOOL sbbs_t::newuser()
{
	char	c,str[512];
	char 	tmp[512];
	uint	i;
	long	kmode;
	bool	usa;

	if(cur_rate<cfg.node_minbps) {
		bprintf(text[MinimumModemSpeed],cfg.node_minbps);
		sprintf(str,"%stooslow.msg",cfg.text_dir);
		if(fexist(str))
			printfile(str,0);
		sprintf(str,"New user modem speed: %lu<%u"
			,cur_rate,cfg.node_minbps);
		logline("N!",str);
		hangup();
		return(FALSE); 
	}

	getnodedat(cfg.node_num,&thisnode,0);
	if(thisnode.misc&NODE_LOCK) {
		bputs(text[NodeLocked]);
		logline("N!","New user locked node logon attempt");
		hangup();
		return(FALSE); 
	}

	if(cfg.sys_misc&SM_CLOSED) {
		bputs(text[NoNewUsers]);
		hangup();
		return(FALSE); 
	}
	getnodedat(cfg.node_num,&thisnode,1);
	thisnode.status=NODE_NEWUSER;
	thisnode.connection=0xffff;
	putnodedat(cfg.node_num,&thisnode);
	memset(&useron,0,sizeof(user_t));	  /* Initialize user info to null */
	if(cfg.new_pass[0] && online==ON_REMOTE) {
		c=0;
		while(++c<4) {
			bputs(text[NewUserPasswordPrompt]);
			getstr(str,40,K_UPPER);
			if(!strcmp(str,cfg.new_pass))
				break;
			sprintf(tmp,"NUP Attempted: '%s'",str);
			logline("N!",tmp); 
		}
		if(c==4) {
			sprintf(str,"%snupguess.msg",cfg.text_dir);
			if(fexist(str))
				printfile(str,P_NOABORT);
			hangup();
			return(FALSE); 
		} 
	}

	/* Sets defaults per sysop config */
	useron.misc|=(cfg.new_misc&~(DELETED|INACTIVE|QUIET|NETMAIL));
	useron.qwk=(QWK_FILES|QWK_ATTACH|QWK_EMAIL|QWK_DELMAIL);
	useron.firston=useron.laston=useron.pwmod=time(NULL);
	if(cfg.new_expire) {
		now=time(NULL);
		useron.expire=now+((long)cfg.new_expire*24L*60L*60L); 
	} else
		useron.expire=0;
	useron.sex=SP;
	useron.prot=cfg.new_prot;
	strcpy(useron.note,cid);		/* Caller ID if supported, NULL otherwise */
	if((i=userdatdupe(0,U_NOTE,LEN_NOTE,cid,true))!=0) {	/* Duplicate IP address */
		sprintf(useron.comment,"Warning: same IP address as user #%d %s"
			,i,username(&cfg,i,str));
		logline("N!",useron.comment); 
	}

	strcpy(useron.alias,"New");     /* just for status line */
	strcpy(useron.modem,connection);
	if(!lastuser(&cfg)) {	/* Automatic sysop access for first user */
		useron.level=99;
		useron.exempt=useron.flags1=useron.flags2=0xffffffffUL;
		useron.flags3=useron.flags4=0xffffffffUL;
		useron.rest=0L; 
	} else {
		useron.level=cfg.new_level;
		useron.flags1=cfg.new_flags1;
		useron.flags2=cfg.new_flags2;
		useron.flags3=cfg.new_flags3;
		useron.flags4=cfg.new_flags4;
		useron.rest=cfg.new_rest;
		useron.exempt=cfg.new_exempt; 
	}

	useron.cdt=cfg.new_cdt;
	useron.min=cfg.new_min;
	useron.freecdt=cfg.level_freecdtperday[useron.level];

	if(cfg.total_fcomps)
		strcpy(useron.tmpext,cfg.fcomp[0]->ext);
	else
		strcpy(useron.tmpext,"ZIP");
	for(i=0;i<cfg.total_xedits;i++)
		if(!stricmp(cfg.xedit[i]->code,cfg.new_xedit) && chk_ar(cfg.xedit[i]->ar,&useron))
			break;
	if(i<cfg.total_xedits)
		useron.xedit=i+1;


	useron.shell=cfg.new_shell;

	useron.alias[0]=0;

	kmode=(cfg.uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL;
	if(!(cfg.uq&UQ_NOUPRLWR))
		kmode|=K_UPRLWR;

	while(online) {

		if(autoterm || yesno(text[AutoTerminalQ])) {
			useron.misc|=AUTOTERM;
			useron.misc|=autoterm; 
		} else
			useron.misc&=~AUTOTERM;

		if(!(useron.misc&AUTOTERM)) {
			if(yesno(text[AnsiTerminalQ]))
				useron.misc|=ANSI; 
			else
				useron.misc&=~ANSI;
		}

		if(useron.misc&ANSI) {
			useron.rows=0;	/* Auto-rows */
			if(useron.misc&(RIP|WIP|HTML) || yesno(text[ColorTerminalQ]))
				useron.misc|=COLOR; 
			else
				useron.misc&=~COLOR;
		}
		else
			useron.rows=24;
		if(!yesno(text[ExAsciiTerminalQ]))
			useron.misc|=NO_EXASCII;
		else
			useron.misc&=~NO_EXASCII;

		if(sys_status&SS_RLOGIN && rlogin_name[0])
			strcpy(useron.alias,rlogin_name);
		else {
			while(online) {
				if(cfg.uq&UQ_ALIASES)
					bputs(text[EnterYourAlias]);
				else
					bputs(text[EnterYourRealName]);
				getstr(useron.alias,LEN_ALIAS,kmode);
				truncsp(useron.alias);
				if(useron.alias[0]<=SP || !isalpha(useron.alias[0])
					|| !stricmp(useron.alias,cfg.sys_id)
					|| strchr(useron.alias,0xff)
					|| matchuser(&cfg,useron.alias,TRUE /* sysop_alias */) 
					|| trashcan(useron.alias,"name")
					|| (!(cfg.uq&UQ_ALIASES) && !strchr(useron.alias,SP))) {
					bputs(text[YouCantUseThatName]);
					if(!yesno(ContinueText))
						return(FALSE);
					continue; 
				}
				break; 
			}
		}
		if(!online) return(FALSE);
		if(cfg.uq&UQ_ALIASES && cfg.uq&UQ_REALNAME) {
			while(online) {
				bputs(text[EnterYourRealName]);
				if(!getstr(useron.name,LEN_NAME,kmode)
					|| trashcan(useron.name,"name")
					|| strchr(useron.name,0xff)
					|| !strchr(useron.name,SP)
					|| (cfg.uq&UQ_DUPREAL
						&& userdatdupe(useron.number,U_NAME,LEN_NAME
							,useron.name,0)))
					bputs(text[YouCantUseThatName]);
				else
					break; 
				if(!yesno(ContinueText))
					return(FALSE);
			} 
		}
		else if(cfg.uq&UQ_COMPANY) {
				bputs(text[EnterYourCompany]);
				getstr(useron.name,LEN_NAME,(cfg.uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL); 
		}
		if(!useron.name[0])
			strcpy(useron.name,useron.alias);
		if(!online) return(FALSE);
		if(!useron.handle[0])
			sprintf(useron.handle,"%.*s",LEN_HANDLE,useron.alias);
		while(cfg.uq&UQ_HANDLE && online) {
			bputs(text[EnterYourHandle]);
			if(!getstr(useron.handle,LEN_HANDLE
				,K_LINE|K_EDIT|K_AUTODEL|(cfg.uq&UQ_NOEXASC))
				|| strchr(useron.handle,0xff)
				|| (cfg.uq&UQ_DUPHAND
					&& userdatdupe(0,U_HANDLE,LEN_HANDLE,useron.handle,0))
				|| trashcan(useron.handle,"name"))
				bputs(text[YouCantUseThatName]);
			else
				break; 
			if(!yesno(ContinueText))
				return(FALSE);
		}
		if(!online) return(FALSE);
		if(cfg.uq&UQ_ADDRESS)
			while(online) { 	   /* Get address and zip code */
				bputs(text[EnterYourAddress]);
				if(getstr(useron.address,LEN_ADDRESS,kmode))
					break; 
			}
		if(!online) return(FALSE);
		while(cfg.uq&UQ_LOCATION && online) {
			bputs(text[EnterYourCityState]);
			if(getstr(useron.location,LEN_LOCATION,kmode)
				&& (cfg.uq&UQ_NOCOMMAS || strchr(useron.location,',')))
				break;
			bputs("\r\nYou must include a comma between the city and state.\r\n");
			useron.location[0]=0; 
		}
		if(cfg.uq&UQ_ADDRESS)
			while(online) {
				bputs(text[EnterYourZipCode]);
				if(getstr(useron.zipcode,LEN_ZIPCODE
					,K_UPPER|(cfg.uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL))
					break; 
			}
		if(!online) return(FALSE);
		if(cfg.uq&UQ_PHONE) {
			usa=yesno(text[CallingFromNorthAmericaQ]);
			while(online) {
				bputs(text[EnterYourPhoneNumber]);
				if(!usa) {
					if(getstr(useron.phone,LEN_PHONE
						,K_UPPER|K_LINE|(cfg.uq&UQ_NOEXASC)|K_EDIT|K_AUTODEL)<5)
						continue; 
				}
				else {
					if(gettmplt(useron.phone,cfg.sys_phonefmt
						,K_LINE|(cfg.uq&UQ_NOEXASC)|K_EDIT)<strlen(cfg.sys_phonefmt))
						continue; 
				}
				if(!trashcan(useron.phone,"phone"))
					break; 
			} 
		}
		if(!online) return(FALSE);
		if(cfg.uq&UQ_SEX) {
			bputs(text[EnterYourSex]);
			useron.sex=(char)getkeys("MF",0); 
		}
		while(cfg.uq&UQ_BIRTH && online) {
			bprintf(text[EnterYourBirthday]
				,cfg.sys_misc&SM_EURODATE ? "DD/MM/YY" : "MM/DD/YY");
			if(gettmplt(useron.birth,"nn/nn/nn",K_EDIT)==8 && getage(&cfg,useron.birth))
				break; 
		}
		if(!online) return(FALSE);
		while(!(sys_status&SS_RLOGIN) && !(cfg.uq&UQ_NONETMAIL) && online) {
			bputs(text[EnterNetMailAddress]);
			if(getstr(useron.netmail,LEN_NETMAIL,K_EDIT|K_AUTODEL|K_LINE)
				&& !trashcan(useron.netmail,"email"))
				break;
		}
		if(useron.netmail[0] && cfg.sys_misc&SM_FWDTONET && yesno(text[ForwardMailQ]))
			useron.misc|=NETMAIL;
		else 
			useron.misc&=~NETMAIL;
		if(yesno(text[UserInfoCorrectQ]))
			break; 
	}
	if(!online) return(FALSE);
	sprintf(str,"New user: %s",useron.alias);
	logline("N",str);
	if(!online) return(FALSE);
	CLS;
	sprintf(str,"%ssbbs.msg",cfg.text_dir);
	printfile(str,P_NOABORT);
	if(lncntr)
		pause();
	CLS;
	sprintf(str,"%ssystem.msg",cfg.text_dir);
	printfile(str,P_NOABORT);
	if(lncntr)
		pause();
	CLS;
	sprintf(str,"%snewuser.msg",cfg.text_dir);
	printfile(str,P_NOABORT);
	if(lncntr)
		pause();
	CLS;
	answertime=time(NULL);		/* could take 10 minutes to get this far */

	if(cfg.total_xedits && cfg.uq&UQ_XEDIT 
		&& !noyes("Use an external text editor")) {
		if(useron.xedit) useron.xedit--;
		for(i=0;i<cfg.total_xedits;i++)
			uselect(1,i,"External Editor",cfg.xedit[i]->name,cfg.xedit[i]->ar);
		if((int)(i=uselect(0,useron.xedit,0,0,0))>=0)
			useron.xedit=i+1; 
	}

	if(cfg.total_shells>1 && cfg.uq&UQ_CMDSHELL) {
		for(i=0;i<cfg.total_shells;i++)
			uselect(1,i,"Command Shell",cfg.shell[i]->name,cfg.shell[i]->ar);
		if((int)(i=uselect(0,useron.shell,0,0,0))>=0)
			useron.shell=i; 
	}

	c=0;
	while(c<LEN_PASS) { 				/* Create random password */
		useron.pass[c]=sbbs_random(43)+'0';
		if(isalnum(useron.pass[c]))
			c++; 
	}
	useron.pass[c]=0;

	if(!(sys_status&SS_RLOGIN)) {
		bprintf(text[YourPasswordIs],useron.pass);

		if(cfg.sys_misc&SM_PWEDIT && yesno(text[NewPasswordQ]))
			while(online) {
				bputs(text[NewPassword]);
				getstr(str,LEN_PASS,K_UPPER|K_LINE);
				truncsp(str);
				if(chkpass(str,&useron,true)) {
					strcpy(useron.pass,str);
					CRLF;
					bprintf(text[YourPasswordIs],useron.pass);
					break; 
				}
				CRLF; 
			}

		c=0;
		while(online) {
			bprintf(text[NewUserPasswordVerify]);
			console|=CON_R_ECHOX;
			if(!(cfg.sys_misc&SM_ECHO_PW))
				console|=CON_L_ECHOX;
			str[0]=0;
			getstr(str,LEN_PASS,K_UPPER);
			console&=~(CON_R_ECHOX|CON_L_ECHOX);
			if(!strcmp(str,useron.pass)) break;
			if(cfg.sys_misc&SM_ECHO_PW) 
				sprintf(tmp,"%s FAILED Password verification: '%s' instead of '%s'"
					,useron.alias
					,str
					,useron.pass);
			else
				sprintf(tmp,"%s FAILED Password verification"
					,useron.alias);
			logline(nulstr,tmp);
			if(++c==4) {
				logline("N!","Couldn't figure out password.");
				hangup(); 
			}
			bputs(text[IncorrectPassword]);
			bprintf(text[YourPasswordIs],useron.pass); 
		}
	}

	if(!online) return(FALSE);
	if(cfg.new_magic[0]) {
		bputs(text[MagicWordPrompt]);
		str[0]=0;
		getstr(str,50,K_UPPER);
		if(strcmp(str,cfg.new_magic)) {
			bputs(text[FailedMagicWord]);
			sprintf(tmp,"%s failed magic word: '%s'",useron.alias,str);
			logline("N!",tmp);
			hangup(); 
		}
		if(!online) return(FALSE); 
	}

	bputs(text[CheckingSlots]);

	if((i=newuserdat(&cfg,&useron))!=0) {
		sprintf(str,"user record #%u",useron.number);
		errormsg(WHERE,ERR_CREATE,str,i);
		hangup();
		return(FALSE); 
	}
	sprintf(str,"Created user record #%u: %s",useron.number,useron.alias);
	logline(nulstr,str);
	if(cfg.new_sif[0]) {
		sprintf(str,"%suser/%4.4u.dat",cfg.data_dir,useron.number);
		create_sif_dat(cfg.new_sif,str); 
	}
	if(!(cfg.uq&UQ_NODEF))
		maindflts(&useron);

	delallmail(useron.number);

	if(useron.number!=1 && cfg.node_valuser) {
		sprintf(str,"%sfeedback.msg",cfg.text_dir);
		CLS;
		printfile(str,P_NOABORT);
		sprintf(str,text[NewUserFeedbackHdr]
			,nulstr,getage(&cfg,useron.birth),useron.sex,useron.birth
			,useron.name,useron.phone,useron.comp,useron.modem);
		email(cfg.node_valuser,str,"New User Validation",WM_EMAIL);
		if(!useron.fbacks && !useron.emails) {
			if(online) {						/* didn't hang up */
				bprintf(text[NoFeedbackWarning],username(&cfg,cfg.node_valuser,tmp));
				email(cfg.node_valuser,str,"New User Validation",WM_EMAIL);
				} /* give 'em a 2nd try */
			if(!useron.fbacks && !useron.emails) {
        		bprintf(text[NoFeedbackWarning],username(&cfg,cfg.node_valuser,tmp));
				logline("N!","Aborted feedback");
				hangup();
				putuserrec(&cfg,useron.number,U_COMMENT,60,"Didn't leave feedback");
				putuserrec(&cfg,useron.number,U_MISC,8
					,ultoa(useron.misc|DELETED,tmp,16));
				putusername(&cfg,useron.number,nulstr);
				return(FALSE); 
			} 
		} 
	}

	answertime=starttime=time(NULL);	  /* set answertime to now */

#ifdef JAVASCRIPT
	js_create_user_objects();
#endif

	if(cfg.newuser_mod[0])
		exec_bin(cfg.newuser_mod,&main_csi);
	user_event(EVENT_NEWUSER);
	getuserdat(&cfg,&useron);	// In case event(s) modified user data
	logline("N+","Successful new user logon");
	sys_status|=SS_NEWUSER;

	return(TRUE);
}
