/* execmsg.cpp */

/* Synchronet message-related command shell/module routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2009 Rob Swindell - http://www.synchro.net/copyright.html		*
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

int sbbs_t::exec_msg(csi_t *csi)
{
	char	str[256],ch;
	ulong	i,j;
	
	switch(*(csi->ip++)) {

		case CS_MSG_SELECT_AREA:
			csi->logic=LOGIC_FALSE;
			if(!usrgrps) return(0);
			while(online) {
				j=0;
				if(usrgrps>1) {
					sprintf(str,"%smenu/grps.*", cfg.text_dir);
					if(fexist(str))
						menu("grps");
					else {
						bputs(text[CfgGrpLstHdr]);
						for(i=0;i<usrgrps && !msgabort();i++) {
							if(i==curgrp)
								outchar('*');
							else outchar(' ');
							if(i<9) outchar(' ');
							if(i<99) outchar(' ');
							bprintf(text[CfgGrpLstFmt]
								,i+1, cfg.grp[usrgrp[i]]->lname); } 
					}
					sprintf(str,text[JoinWhichGrp],curgrp+1);
					mnemonics(str);
					j=getnum(usrgrps);
					if((int)j==-1)
						return(0);
					if(!j)
						j=curgrp;
					else
						j--; 
				}
				sprintf(str,"%smenu/subs%u.*", cfg.text_dir, usrgrp[j]+1);
				if(fexist(str)) {
					sprintf(str,"subs%u",usrgrp[j]+1);
					menu(str); 
				}
				else {
					CLS;
					bprintf(text[SubLstHdr], cfg.grp[usrgrp[j]]->lname);
					for(i=0;i<usrsubs[j] && !msgabort();i++) {
						if(i==cursub[j]) outchar('*');
						else outchar(' ');
						sprintf(str,text[SubLstFmt],i+1
							,cfg.sub[usrsub[j][i]]->lname,nulstr
							,getposts(&cfg,usrsub[j][i]));
						if(i<9) outchar(' ');
						if(i<99) outchar(' ');
						bputs(str); } 
				}
				sprintf(str,text[JoinWhichSub],cursub[j]+1);
				mnemonics(str);
				i=getnum(usrsubs[j]);
				if((int)i==-1) {
					if(usrgrps==1)
						return(0);
					continue; 
				}
				if(!i)
					i=cursub[j];
				else
					i--;
				curgrp=j;
				cursub[curgrp]=i;
				csi->logic=LOGIC_TRUE;
				return(0); 
			}
			return(0);

		case CS_MSG_GET_SUB_NUM:

			if(useron.misc&COLDKEYS) {
				i=atoi(csi->str);
				if(i && usrgrps && i<=usrsubs[curgrp])
					cursub[curgrp]=i-1;
				return(0); 
			}

			ch=getkey(K_UPPER);
			outchar(ch);
			if(usrgrps && (ch&0xf)*10U<=usrsubs[curgrp] && (ch&0xf)) {
				i=(ch&0xf)*10;
				ch=getkey(K_UPPER);
				if(!isdigit(ch) && ch!=CR) {
					ungetkey(ch);
					cursub[curgrp]=(i/10)-1;
					return(0); 
				}
				outchar(ch);
				if(ch==CR) {
					cursub[curgrp]=(i/10)-1;
					return(0); 
				}
				logch(ch,0);
				i+=ch&0xf;
				if(i*10<=usrsubs[curgrp]) { 	/* 100+ subs */
					i*=10;
					ch=getkey(K_UPPER);
					if(!isdigit(ch) && ch!=CR) {
						ungetkey(ch);
						cursub[curgrp]=(i/10)-1;
						return(0); 
					}
					outchar(ch);
					if(ch==CR) {
						cursub[curgrp]=(i/10)-1;
						return(0); 
					}
					logch(ch,0);
					i+=ch&0xf; 
				}
				if(i<=usrsubs[curgrp])
					cursub[curgrp]=i-1;
				return(0); 
			}
			if((uint)(ch&0xf)<=usrsubs[curgrp] && (ch&0xf) && usrgrps)
				cursub[curgrp]=(ch&0xf)-1;
			return(0);

		case CS_MSG_GET_GRP_NUM:

			if(useron.misc&COLDKEYS) {
				i=atoi(csi->str);
				if(i && i<=usrgrps)
					curgrp=i-1;
				return(0); 
			}

			ch=getkey(K_UPPER);
			outchar(ch);
			if((ch&0xf)*10U<=usrgrps && (ch&0xf)) {
				i=(ch&0xf)*10;
				ch=getkey(K_UPPER);
				if(!isdigit(ch) && ch!=CR) {
					ungetkey(ch);
					curgrp=(i/10)-1;
					return(0); 
				}
				outchar(ch);
				if(ch==CR) {
					curgrp=(i/10)-1;
					return(0); 
				}
				logch(ch,0);
				i+=ch&0xf;
				if(i<=usrgrps)
					curgrp=i-1;
				return(0); 
			}
			if((uint)(ch&0xf)<=usrgrps && (ch&0xf))
				curgrp=(ch&0xf)-1;
			return(0);

		case CS_MSG_SET_GROUP:
			csi->logic=LOGIC_TRUE;
			for(i=0;i<usrgrps;i++)
				if(!stricmp(cfg.grp[usrgrp[i]]->sname,csi->str))
					break;
			if(i<usrgrps)
				curgrp=i;
			else
				csi->logic=LOGIC_FALSE;
			return(0);

		case CS_MSG_SHOW_GROUPS:
			if(!usrgrps) return(0);
			sprintf(str,"%smenu/grps.*", cfg.text_dir);
			if(fexist(str)) {
				menu("grps");
				return(0); 
			}
			bputs(text[GrpLstHdr]);
			for(i=0;i<usrgrps && !msgabort();i++) {
				if(i==curgrp)
					outchar('*');
				else outchar(' ');
				if(i<9) outchar(' ');
				bprintf(text[GrpLstFmt],i+1
					,cfg.grp[usrgrp[i]]->lname,nulstr,usrsubs[i]); 
			}
			return(0);

		case CS_MSG_SHOW_SUBBOARDS:
			if(!usrgrps) return(0);
			sprintf(str,"%smenu/subs%u.*", cfg.text_dir, usrgrp[curgrp]+1);
			if(fexist(str)) {
				sprintf(str,"subs%u",usrgrp[curgrp]+1);
				menu(str);
				return(0); 
			}
			CRLF;
			bprintf(text[SubLstHdr],cfg.grp[usrgrp[curgrp]]->lname);
			for(i=0;i<usrsubs[curgrp] && !msgabort();i++) {
				if(i==cursub[curgrp]) outchar('*');
				else outchar(' ');
				sprintf(str,text[SubLstFmt],i+1
					,cfg.sub[usrsub[curgrp][i]]->lname,nulstr
					,getposts(&cfg,usrsub[curgrp][i]));
				if(i<9) outchar(' ');
				if(i<99) outchar(' ');
				bputs(str); 
			}
			return(0);

		case CS_MSG_GROUP_UP:
			curgrp++;
			if(curgrp>=usrgrps)
				curgrp=0;
			return(0);
		case CS_MSG_GROUP_DOWN:
			if(!curgrp)
				curgrp=usrgrps-1;
			else curgrp--;
			return(0);
		case CS_MSG_SUBBOARD_UP:
			if(!usrgrps) return(0);
			cursub[curgrp]++;
			if(cursub[curgrp]>=usrsubs[curgrp])
				cursub[curgrp]=0;
			return(0);
		case CS_MSG_SUBBOARD_DOWN:
			if(!usrgrps) return(0);
			if(!cursub[curgrp])
				cursub[curgrp]=usrsubs[curgrp]-1;
			else cursub[curgrp]--;
			return(0);
		case CS_MSG_SET_AREA:
			csi->logic=LOGIC_TRUE;
			for(i=0;i<usrgrps;i++)
				for(j=0;j<usrsubs[i];j++)
					if(!stricmp(csi->str,cfg.sub[usrsub[i][j]]->code)) {
						curgrp=i;
						cursub[i]=j;
						return(0); 
					}
			csi->logic=LOGIC_FALSE;
			return(0);
		case CS_MSG_READ:
			if(!usrgrps) return(0);
			csi->logic=scanposts(usrsub[curgrp][cursub[curgrp]],0,nulstr);
			return(0);
		case CS_MSG_POST:
			if(!usrgrps) return(0);
			csi->logic=!postmsg(usrsub[curgrp][cursub[curgrp]],0,0);
			return(0);
		case CS_MSG_QWK:
			qwk_sec();
			return(0);
		case CS_MSG_PTRS_CFG:
			new_scan_ptr_cfg();
			return(0);
		case CS_MSG_PTRS_REINIT:
			for(i=0;i<cfg.total_subs;i++) {
				subscan[i].ptr=subscan[i].sav_ptr;
				subscan[i].last=subscan[i].sav_last; 
			}
			bputs(text[MsgPtrsInitialized]);
			return(0);
		case CS_MSG_NEW_SCAN_CFG:
			new_scan_cfg(SUB_CFG_NSCAN);
			return(0);
		case CS_MSG_NEW_SCAN:
			scansubs(SCAN_NEW);
			return(0);
		case CS_MSG_NEW_SCAN_SUB:
			csi->logic=scanposts(usrsub[curgrp][cursub[curgrp]],SCAN_NEW,nulstr);
			return(0);
		case CS_MSG_NEW_SCAN_ALL:
			scanallsubs(SCAN_NEW);
			return(0);
		case CS_MSG_CONT_SCAN:
			scansubs(SCAN_NEW|SCAN_CONST);
			return(0);
		case CS_MSG_CONT_SCAN_ALL:
			scanallsubs(SCAN_NEW|SCAN_CONST);
			return(0);
		case CS_MSG_BROWSE_SCAN:
			scansubs(SCAN_NEW|SCAN_BACK);
			return(0);
		case CS_MSG_BROWSE_SCAN_ALL:
			scanallsubs(SCAN_BACK|SCAN_NEW);
			return(0);
		case CS_MSG_FIND_TEXT:
			scansubs(SCAN_FIND);
			return(0);
		case CS_MSG_FIND_TEXT_ALL:
			scanallsubs(SCAN_FIND);
			return(0);
		case CS_MSG_YOUR_SCAN_CFG:
			new_scan_cfg(SUB_CFG_SSCAN);
			return(0);
		case CS_MSG_YOUR_SCAN:
			scansubs(SCAN_TOYOU);
			return(0);
		case CS_MSG_YOUR_SCAN_ALL:
			scanallsubs(SCAN_TOYOU);
			return(0); 
	}
	errormsg(WHERE,ERR_CHK,"shell function",*(csi->ip-1));
	return(0);
}
