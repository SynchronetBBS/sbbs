/* execfunc.cpp */

/* Hi-level command shell/module routines (functions) */

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

int sbbs_t::exec_function(csi_t *csi)
{
	char	str[256],ch;
	char 	tmp[512];
	uchar*	p;
	int		s;
	uint 	i,j,k;
	long	l;
	node_t	node;
	struct	tm * tm;

	switch(*(csi->ip++)) {


		case CS_PRINTFILE_STR:
			printfile(csi->str,P_NOATCODES);
			return(0);

	/********************************/
	/* General Main Menu Type Stuff */
	/********************************/
		case CS_AUTO_MESSAGE:
			automsg();
			return(0);
		case CS_MINUTE_BANK:
			time_bank();
			return(0);
		case CS_CHAT_SECTION:
			chatsection();
			return(0);
		case CS_USER_DEFAULTS:
			maindflts(&useron);
			if(!(useron.rest&FLAG('G')))    /* not guest */
				getuserdat(&cfg,&useron);
			return(0);
		case CS_TEXT_FILE_SECTION:
			text_sec();
			return(0);
		case CS_INFO_SYSTEM:   /* System information */
			sys_info();
			return(0);
		case CS_INFO_SUBBOARD:	 /* Sub-board information */
			if(!usrgrps) return(0);
			subinfo(usrsub[curgrp][cursub[curgrp]]);
			return(0);
		case CS_INFO_DIRECTORY:   /* Sub-board information */
			if(!usrlibs) return(0);
			dirinfo(usrdir[curlib][curdir[curlib]]);
			return(0);
		case CS_INFO_VERSION:	/* Version */
			ver();
			return(0);
		case CS_INFO_USER:	 /* User's statistics */
			user_info();
			return(0);
		case CS_INFO_XFER_POLICY:
			xfer_policy();
			return(0);
		case CS_XTRN_EXEC:
			csi->logic=LOGIC_TRUE;
			for(i=0;i<cfg.total_xtrns;i++)
				if(!stricmp(cfg.xtrn[i]->code,csi->str))
					break;
			if(i<cfg.total_xtrns)
				exec_xtrn(i);
			else
				csi->logic=LOGIC_FALSE;
			return(0);
		case CS_XTRN_SECTION:
			xtrn_sec();
			return(0);
		case CS_LOGOFF:
			if(!noyes(text[LogOffQ])) {
				if(cfg.logoff_mod[0])
					exec_bin(cfg.logoff_mod,csi);
				user_event(EVENT_LOGOFF);
				menu("logoff");
				SYNC;
				hangup(); }
			return(0);
		case CS_LOGOFF_FAST:
			SYNC;
			hangup();
			return(0);
		case CS_NODELIST_ALL:
			nodelist();
			return(0);
		case CS_NODELIST_USERS:
			whos_online(true);
			return(0);
		case CS_USERLIST_SUB:
			userlist(UL_SUB);
			return(0);
		case CS_USERLIST_DIR:
			userlist(UL_DIR);
			return(0);
		case CS_USERLIST_ALL:
			userlist(UL_ALL);
			return(0);
		case CS_USERLIST_LOGONS:
			logonlist();
			return(0);
		case CS_PAGE_SYSOP:
			csi->logic=sysop_page() ? LOGIC_TRUE:LOGIC_FALSE;
			return(0);
		case CS_PAGE_GURU:
			csi->logic=guru_page() ? LOGIC_TRUE:LOGIC_FALSE;
			return(0);
		case CS_SPY:
			csi->logic=spy(atoi(csi->str)) ? LOGIC_TRUE:LOGIC_FALSE;
			return(0);
		case CS_PRIVATE_CHAT:
			privchat();
			return(0);
		case CS_PRIVATE_MESSAGE:
			nodemsg();
			return(0);

	/*******************/
	/* Sysop Functions */
	/*******************/
		case CS_USER_EDIT:
			useredit(csi->str[0] ? finduser(csi->str) : 0);
			return(0);


	/******************/
	/* Mail Functions */
	/******************/

		case CS_MAIL_READ:	 /* Read E-mail */
			readmail(useron.number,MAIL_YOUR);
			return(0);
		case CS_MAIL_READ_SENT: 	  /* Kill/read sent mail */
			readmail(useron.number,MAIL_SENT);
			return(0);
		case CS_MAIL_READ_ALL:
			readmail(useron.number,MAIL_ALL);
			return(0);
		case CS_MAIL_SEND:		 /* Send E-mail */
			if(strchr(csi->str,'@')) {
				i=1;
				netmail(csi->str,nulstr,0); }
			else if((i=finduser(csi->str))!=0 
				|| (cfg.msg_misc&MM_REALNAME && (i=userdatdupe(0,U_NAME,LEN_NAME,csi->str,false))!=0))
				email(i,nulstr,nulstr,WM_EMAIL);
			csi->logic=!i;
			return(0);
		case CS_MAIL_SEND_FEEDBACK: 	  /* Feedback */
			if((i=finduser(csi->str))!=0)
				email(i,text[ReFeedback],nulstr,WM_EMAIL);
			csi->logic=!i;
			return(0);
		case CS_MAIL_SEND_NETMAIL:
			bputs(text[EnterNetMailAddress]);
			if(getstr(str,60,K_LINE)) {
				netmail(str,nulstr,0);
				csi->logic=LOGIC_TRUE; }
			else
				csi->logic=LOGIC_FALSE;
			return(0);

		case CS_MAIL_SEND_NETFILE:
			bputs(text[EnterNetMailAddress]);
			if(getstr(str,60,K_LINE)) {
				netmail(str,nulstr,WM_FILE);
				csi->logic=LOGIC_TRUE; }
			else
				csi->logic=LOGIC_FALSE;
			return(0);

		case CS_MAIL_SEND_FILE:   /* Upload Attached File to E-mail */
			if(strchr(csi->str,'@')) {
				i=1;
				netmail(csi->str,nulstr,WM_FILE); }
			else if((i=finduser(csi->str))!=0
				|| (cfg.msg_misc&MM_REALNAME && (i=userdatdupe(0,U_NAME,LEN_NAME,csi->str,false))!=0))
				email(i,nulstr,nulstr,WM_EMAIL|WM_FILE);
			csi->logic=!i;
			return(0);
		case CS_MAIL_SEND_BULK:
			if(csi->str[0])
				p=arstr(NULL,csi->str, &cfg);
			else
				p=(uchar *)nulstr;
			bulkmail(p);
			if(p && p[0])
				FREE(p);
			return(0);

		case CS_INC_MAIN_CMDS:
			main_cmds++;
			return(0);

		case CS_INC_FILE_CMDS:
			xfer_cmds++;
			return(0);

		case CS_SYSTEM_LOG:                 /* System log */
			if(!chksyspass())
				return(0);
			tm=localtime(&now);
			if(tm==NULL)
				return(0);
			sprintf(str,"%slogs/%2.2d%2.2d%2.2d.log", cfg.data_dir
				,tm->tm_mon+1,tm->tm_mday,TM_YEAR(tm->tm_year));
			printfile(str,0);
			return(0);
		case CS_SYSTEM_YLOG:                /* Yesterday's log */
			if(!chksyspass())
				return(0);
			now-=(ulong)60L*24L*60L;
			tm=localtime(&now);
			if(tm==NULL)
				return(0);
			sprintf(str,"%slogs/%2.2d%2.2d%2.2d.log",cfg.data_dir
				,tm->tm_mon+1,tm->tm_mday,TM_YEAR(tm->tm_year));
			printfile(str,0);
			return(0);
		case CS_SYSTEM_STATS:               /* System Statistics */
			sys_stats();
			return(0);
		case CS_NODE_STATS:              /* Node Statistics */
			node_stats(atoi(csi->str));
			return(0);
		case CS_CHANGE_USER:                 /* Change to another user */
			change_user();
			return(0);
		case CS_SHOW_MEM:
	#ifdef __MSDOS__
			 bprintf(text[NBytesFreeMemory],farcoreleft());
	#endif
			return(0);
		case CS_ERROR_LOG:
			sprintf(str,"%serror.log", cfg.data_dir);
			if(fexist(str)) {
				bputs(text[ErrorLogHdr]);
				printfile(str,0);
				if(!noyes(text[DeleteErrorLogQ]))
					remove(str); }
			else
				bputs(text[NoErrorLogExists]);
			for(i=1;i<=cfg.sys_nodes;i++) {
				getnodedat(i,&node,0);
				if(node.errors)
					break; }
			if(i<=cfg.sys_nodes || criterrs) {
				if(!noyes(text[ClearErrCounter])) {
					for(i=1;i<=cfg.sys_nodes;i++) {
						getnodedat(i,&node,1);
						node.errors=0;
						putnodedat(i,&node); }
					criterrs=0; } }
			return(0);
		case CS_ANSI_CAPTURE:           /* Capture ANSI codes */
			sys_status^=SS_ANSCAP;
			bprintf(text[ANSICaptureIsNow]
				,sys_status&SS_ANSCAP ? text[ON] : text[OFF]);
			return(0);
		case CS_LIST_TEXT_FILE:              /* View ASCII/ANSI/Ctrl-A file */
			if(!chksyspass())
				return(0);
			bputs(text[Filename]);
			if(getstr(str,60,0))
				printfile(str,0);
			return(0);
		case CS_EDIT_TEXT_FILE:              /* Edit ASCII/Ctrl-A file */
			if(!chksyspass())
				return(0);
			bputs(text[Filename]);
			if(getstr(str,60,0))
				editfile(str);
			return(0);
		case CS_GURU_LOG:
			sprintf(str,"%sguru.log", cfg.data_dir);
			if(fexist(str)) {
				printfile(str,0);
				CRLF;
				if(!noyes(text[DeleteGuruLogQ]))
					remove(str); }
			return(0);
		case CS_FILE_SET_ALT_PATH:
			altul=atoi(csi->str);
			if(altul>cfg.altpaths)
				altul=0;
			bprintf(text[AltULPathIsNow],altul ? cfg.altpath[altul-1] : text[OFF]);
			return(0);
		case CS_FILE_RESORT_DIRECTORY:
			for(i=1;i<=cfg.sys_nodes;i++)
				if(i!=cfg.node_num) {
					getnodedat(i,&node,0);
					if(node.status==NODE_INUSE
						|| node.status==NODE_QUIET)
						break; }

			if(i<=cfg.sys_nodes) {
				bputs(text[ResortWarning]);
				return(0); }

			if(!stricmp(csi->str,"ALL")) {     /* all libraries */
				for(i=0;i<usrlibs;i++)
					for(j=0;j<usrdirs[i];j++)
						resort(usrdir[i][j]);
				return(0); }
			if(!stricmp(csi->str,"LIB")) {     /* current library */
				for(i=0;i<usrdirs[curlib];i++)
					resort(usrdir[curlib][i]);
				return(0); }
			resort(usrdir[curlib][curdir[curlib]]);
			return(0);

		case CS_FILE_GET:
			if(!fexist(csi->str)) {
				bputs(text[FileNotFound]);
				return(0); }
			if(!chksyspass())
				return(0);

		case CS_FILE_SEND:

			csi->logic=LOGIC_FALSE;
			menu("dlprot");
			mnemonics(text[ProtocolOrQuit]);
			strcpy(str,"Q");
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->dlcmd[0] && chk_ar(cfg.prot[i]->ar,&useron)) {
					sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
					strcat(str,tmp); }
			ch=(char)getkeys(str,0);
			if(ch=='Q' || sys_status&SS_ABORT) {
				return(0); }
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->mnemonic==ch && chk_ar(cfg.prot[i]->ar,&useron))
					break;
			if(i<cfg.total_prots) {
				if(protocol(cmdstr(cfg.prot[i]->dlcmd,csi->str,csi->str,str),false)==0)
					csi->logic=LOGIC_TRUE;
				autohangup(); 
			}
			return(0);

		case CS_FILE_PUT:
			csi->logic=LOGIC_FALSE;
			if(!chksyspass())
				return(0);

		case CS_FILE_RECEIVE:
			csi->logic=LOGIC_FALSE;
			menu("ulprot");
			mnemonics(text[ProtocolOrQuit]);
			strcpy(str,"Q");
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->ulcmd[0] && chk_ar(cfg.prot[i]->ar,&useron)) {
					sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
					strcat(str,tmp); }
			ch=(char)getkeys(str,0);
			if(ch=='Q' || sys_status&SS_ABORT) {
				lncntr=0;
				return(0); }
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->mnemonic==ch && chk_ar(cfg.prot[i]->ar,&useron))
					break;
			if(i<cfg.total_prots) {
				if(protocol(cmdstr(cfg.prot[i]->ulcmd,csi->str,csi->str,str),true)==0)
					csi->logic=LOGIC_TRUE;
				autohangup(); 
			}
			return(0);

		case CS_FILE_UPLOAD_BULK:

			if(!usrlibs) return(0);

			if(!stricmp(csi->str,"ALL")) {     /* all libraries */
				for(i=0;i<usrlibs;i++)
					for(j=0;j<usrdirs[i];j++) {
						if(cfg.lib[i]->offline_dir==usrdir[i][j])
							continue;
						if(bulkupload(usrdir[i][j])) return(0); }
				return(0); }
			if(!stricmp(csi->str,"LIB")) {     /* current library */
				for(i=0;i<usrdirs[curlib];i++) {
					if(cfg.lib[usrlib[curlib]]->offline_dir
						==usrdir[curlib][i])
						continue;
					if(bulkupload(usrdir[curlib][i])) return(0); }
				return(0); }
			bulkupload(usrdir[curlib][curdir[curlib]]); /* current dir */
			return(0);

		case CS_FILE_FIND_OLD:
		case CS_FILE_FIND_OPEN:
		case CS_FILE_FIND_OFFLINE:
		case CS_FILE_FIND_OLD_UPLOADS:
			if(!usrlibs) return(0);
			if(!getfilespec(tmp))
				return(0);
			padfname(tmp,str);
			k=0;
			bputs("\r\nSearching ");
			if(!stricmp(csi->str,"ALL"))
				bputs("all libraries");
			else if(!stricmp(csi->str,"LIB"))
				bputs("library");
			else
				bputs("directory");
			bputs(" for files ");
			if(*(csi->ip-1)==CS_FILE_FIND_OLD_UPLOADS) {
				l=FI_OLDUL;
				bprintf("uploaded before %s\r\n",timestr(&ns_time)); }
			else if(*(csi->ip-1)==CS_FILE_FIND_OLD) { /* go by download date */
				l=FI_OLD;
				bprintf("not downloaded since %s\r\n",timestr(&ns_time)); }
			else if(*(csi->ip-1)==CS_FILE_FIND_OFFLINE) {
				l=FI_OFFLINE;
				bputs("not online...\r\n"); }
			else {
				l=FI_CLOSE;
				bputs("currently open...\r\n"); }
			if(!stricmp(csi->str,"ALL")) {
				for(i=0;i<usrlibs;i++)
					for(j=0;j<usrdirs[i];j++) {
						if(cfg.lib[i]->offline_dir==usrdir[i][j])
							continue;
						if((s=listfileinfo(usrdir[i][j],str,(char)l))==-1)
							return(0);
						else k+=s; } }
			else if(!stricmp(csi->str,"LIB")) {
				for(i=0;i<usrdirs[curlib];i++) {
					if(cfg.lib[usrlib[curlib]]->offline_dir==usrdir[curlib][i])
							continue;
					if((s=listfileinfo(usrdir[curlib][i],str,(char)l))==-1)
						return(0);
					else k+=s; } }
			else {
				s=listfileinfo(usrdir[curlib][curdir[curlib]],str,(char)l);
				if(s==-1)
					return(0);
				k=s; }
			if(k>1) {
				bprintf(text[NFilesListed],k); }
			return(0); }

	if(*(csi->ip-1)>=CS_MSG_SET_AREA && *(csi->ip-1)<=CS_MSG_UNUSED1) {
		csi->ip--;
		return(exec_msg(csi)); }

	if(*(csi->ip-1)>=CS_FILE_SET_AREA && *(csi->ip-1)<=CS_FILE_RECEIVE) {
		csi->ip--;
		return(exec_file(csi)); }

	errormsg(WHERE,ERR_CHK,"shell function",*(csi->ip-1));
	return(0);
}
