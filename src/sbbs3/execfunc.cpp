/* Hi-level command shell/module routines (functions) */

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

#include "sbbs.h"
#include "cmdshell.h"

int sbbs_t::exec_function(csi_t *csi)
{
	char	str[256];
	uchar*	p;
	int		s;
	uint 	i,j,k;
	long	l;
	node_t	node;
	struct	tm tm;
	uint8_t	cmd = *(csi->ip++);

	switch(cmd) {

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
			if(text[LogOffQ][0]==0 || !noyes(text[LogOffQ])) {
				if(cfg.logoff_mod[0])
					exec_bin(cfg.logoff_mod,csi);
				user_event(EVENT_LOGOFF);
				menu("logoff");
				SYNC;
				hangup(); 
			}
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
				netmail(csi->str); 
			}
			else if((i=finduser(csi->str))!=0 
				|| (cfg.msg_misc&MM_REALNAME && (i=userdatdupe(0,U_NAME,LEN_NAME,csi->str))!=0))
				email(i);
			csi->logic=!i;
			return(0);
		case CS_MAIL_SEND_FEEDBACK: 	  /* Feedback */
			if((i=finduser(csi->str))!=0)
				email(i,text[ReFeedback]);
			csi->logic=!i;
			return(0);
		case CS_MAIL_SEND_NETMAIL:
		case CS_MAIL_SEND_NETFILE:
		{
			bputs(text[EnterNetMailAddress]);
			csi->logic=LOGIC_FALSE;
			if(getstr(str,60,K_LINE)) {
				if(netmail(str, NULL, cmd == CS_MAIL_SEND_NETFILE ? WM_FILE : WM_NONE)) {
					csi->logic=LOGIC_TRUE; 
				}
			}
			return(0);
		}
		case CS_MAIL_SEND_FILE:   /* Upload Attached File to E-mail */
			if(strchr(csi->str,'@')) {
				i=1;
				netmail(csi->str,NULL,WM_FILE); 
			}
			else if((i=finduser(csi->str))!=0
				|| (cfg.msg_misc&MM_REALNAME && (i=userdatdupe(0,U_NAME,LEN_NAME,csi->str))!=0))
				email(i,NULL,NULL,WM_FILE);
			csi->logic=!i;
			return(0);
		case CS_MAIL_SEND_BULK:
			if(csi->str[0])
				p=arstr(NULL,csi->str, &cfg,NULL);
			else
				p=NULL;
			bulkmail(p);
			free(p);
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
			if(localtime_r(&now,&tm)==NULL)
				return(0);
			sprintf(str,"%slogs/%2.2d%2.2d%2.2d.log", cfg.logs_dir
				,tm.tm_mon+1,tm.tm_mday,TM_YEAR(tm.tm_year));
			printfile(str,0);
			return(0);
		case CS_SYSTEM_YLOG:                /* Yesterday's log */
			if(!chksyspass())
				return(0);
			now-=(ulong)60L*24L*60L;
			if(localtime_r(&now,&tm)==NULL)
				return(0);
			sprintf(str,"%slogs/%2.2d%2.2d%2.2d.log",cfg.logs_dir
				,tm.tm_mon+1,tm.tm_mday,TM_YEAR(tm.tm_year));
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
			return(0);
		case CS_ERROR_LOG:
			sprintf(str,"%serror.log", cfg.logs_dir);
			if(fexist(str)) {
				bputs(text[ErrorLogHdr]);
				printfile(str,0);
				if(text[DeleteErrorLogQ][0] && !noyes(text[DeleteErrorLogQ]))
					(void)remove(str); 
			}
			else
				bprintf(text[FileDoesNotExist],str);
			for(i=1;i<=cfg.sys_nodes;i++) {
				getnodedat(i,&node,0);
				if(node.errors)
					break; 
			}
			if(i<=cfg.sys_nodes || criterrs) {
				if(text[ClearErrCounter][0]==0 || !noyes(text[ClearErrCounter])) {
					for(i=1;i<=cfg.sys_nodes;i++) {
						if(getnodedat(i,&node,true)==0) {
							node.errors=0;
							putnodedat(i,&node); 
						}
					criterrs=0; 
					} 
				}
			}
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
			if(getstr(str,60,K_TRIM))
				printfile(str,0);
			return(0);
		case CS_EDIT_TEXT_FILE:              /* Edit ASCII/Ctrl-A file */
			if(!chksyspass())
				return(0);
			bputs(text[Filename]);
			if(getstr(str,60,K_TRIM))
				editfile(str);
			return(0);
		case CS_GURU_LOG:
			sprintf(str,"%sguru.log", cfg.logs_dir);
			if(fexist(str)) {
				printfile(str,0);
				CRLF;
				if(text[DeleteGuruLogQ][0] && !noyes(text[DeleteGuruLogQ]))
					remove(str); 
			}
			return(0);
		case CS_FILE_SET_ALT_PATH:
			bprintf("Alternate Upload Paths are Unsupported\r\n");
			return(0);
		case CS_FILE_RESORT_DIRECTORY:
			lprintf(LOG_WARNING, "deprecated function: RESORT_DIRECTORY");
			return 0;

		case CS_FILE_GET:
			if(!fexist(csi->str)) {
				bputs(text[FileNotFound]);
				return(0); 
			}
			if(!chksyspass())
				return(0);
			// fall-through
		case CS_FILE_SEND:

			csi->logic=sendfile(csi->str) ? LOGIC_TRUE:LOGIC_FALSE;
			return(0);

		case CS_FILE_PUT:
			csi->logic=LOGIC_FALSE;
			if(!chksyspass())
				return(0);
			// fall-through
		case CS_FILE_RECEIVE:
			csi->logic=recvfile(csi->str) ? LOGIC_TRUE:LOGIC_FALSE;
			return(0);

		case CS_FILE_UPLOAD_BULK:

			if(!usrlibs) return(0);

			if(!stricmp(csi->str,"ALL")) {     /* all libraries */
				for(i=0;i<usrlibs;i++)
					for(j=0;j<usrdirs[i];j++) {
						if(cfg.lib[i]->offline_dir==usrdir[i][j])
							continue;
						if(bulkupload(usrdir[i][j])) return(0); 
					}
				return(0); 
			}
			if(!stricmp(csi->str,"LIB")) {     /* current library */
				for(i=0;i<usrdirs[curlib];i++) {
					if(cfg.lib[usrlib[curlib]]->offline_dir
						==usrdir[curlib][i])
						continue;
					if(bulkupload(usrdir[curlib][i])) return(0); 
				}
				return(0); 
			}
			bulkupload(usrdir[curlib][curdir[curlib]]); /* current dir */
			return(0);

		case CS_FILE_FIND_OLD:
		case CS_FILE_FIND_OPEN:
		case CS_FILE_FIND_OFFLINE:
		case CS_FILE_FIND_OLD_UPLOADS:
			if(!usrlibs) return(0);
			if(!getfilespec(str))
				return(0);
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
				bprintf("uploaded before %s\r\n",timestr(ns_time)); 
			}
			else if(*(csi->ip-1)==CS_FILE_FIND_OLD) { /* go by download date */
				l=FI_OLD;
				bprintf("not downloaded since %s\r\n",timestr(ns_time)); 
			}
			else if(*(csi->ip-1)==CS_FILE_FIND_OFFLINE) {
				l=FI_OFFLINE;
				bputs("not online...\r\n"); 
			}
			else {
				// e.g. FI_CLOSE;
				return 0;
			}
			if(!stricmp(csi->str,"ALL")) {
				for(i=0;i<usrlibs;i++)
					for(j=0;j<usrdirs[i];j++) {
						if(cfg.lib[i]->offline_dir==usrdir[i][j])
							continue;
						if((s=listfileinfo(usrdir[i][j],str,(char)l))==-1)
							return(0);
						else k+=s; 
					} 
			}
			else if(!stricmp(csi->str,"LIB")) {
				for(i=0;i<usrdirs[curlib];i++) {
					if(cfg.lib[usrlib[curlib]]->offline_dir==usrdir[curlib][i])
							continue;
					if((s=listfileinfo(usrdir[curlib][i],str,(char)l))==-1)
						return(0);
					else k+=s; 
				} 
			}
			else {
				s=listfileinfo(usrdir[curlib][curdir[curlib]],str,(char)l);
				if(s==-1)
					return(0);
				k=s; 
			}
			if(k>1) {
				bprintf(text[NFilesListed],k); 
			}
			return(0); 
	}

	if(*(csi->ip-1)>=CS_MSG_SET_AREA && *(csi->ip-1)<=CS_MSG_UNUSED1) {
		csi->ip--;
		return(exec_msg(csi)); 
	}

	if(*(csi->ip-1)>=CS_FILE_SET_AREA && *(csi->ip-1)<=CS_FILE_RECEIVE) {
		csi->ip--;
		return(exec_file(csi)); 
	}

	errormsg(WHERE,ERR_CHK,"shell function",*(csi->ip-1));
	return(0);
}
