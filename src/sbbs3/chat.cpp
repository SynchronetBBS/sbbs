/* Synchronet real-time chat functions */

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

#define PCHAT_LEN 1000		/* Size of Private chat file */

const char *weekday[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday"
				,"Saturday"};
const char *month[]={"January","February","March","April","May","June"
				,"July","August","September","October","November","December"};

/****************************************************************************/
/****************************************************************************/
void sbbs_t::multinodechat(int channel)
{
	char	line[256],str[256],ch,done
			,usrs,preusrs,qusrs,*gurubuf=NULL,savch,*p
			,pgraph[400],buf[400]
			,usr[MAX_NODES],preusr[MAX_NODES],qusr[MAX_NODES];
	char	guru_lastanswer[512];
	char 	tmp[512];
	int 	file;
	long	i,j,k,n;
	node_t 	node;

	if(useron.rest&FLAG('C')) {
		bputs(text[R_Chat]);
		return;
	}

	if(channel<1 || channel>cfg.total_chans)
		channel=1;

	if(!chan_access(channel-1))
		return;
	if(useron.misc&(RIP|WIP|HTML) ||!(useron.misc&EXPERT))
		menu("multchat");
	bputs(text[WelcomeToMultiChat]);
	if(getnodedat(cfg.node_num,&thisnode,true)==0) {
		thisnode.aux=channel;
		putnodedat(cfg.node_num,&thisnode);
	}
	bprintf(text[WelcomeToChannelN],channel,cfg.chan[channel-1]->name);
	if(cfg.chan[channel-1]->misc&CHAN_GURU && cfg.chan[channel-1]->guru<cfg.total_gurus
		&& chk_ar(cfg.guru[cfg.chan[channel-1]->guru]->ar,&useron,&client)) {
		sprintf(str,"%s%s.dat",cfg.ctrl_dir,cfg.guru[cfg.chan[channel-1]->guru]->code);
		if((file=nopen(str,O_RDONLY))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
			return;
		}
		if((gurubuf=(char *)malloc((size_t)filelength(file)+1))==NULL) {
			close(file);
			errormsg(WHERE,ERR_ALLOC,str,(size_t)filelength(file)+1);
			return;
		}
		(void)read(file,gurubuf,(size_t)filelength(file));
		gurubuf[filelength(file)]=0;
		close(file);
	}
	usrs=0;
	for(i=1;i<=cfg.sys_nodes && i<=cfg.sys_lastnode;i++) {
		if(i==cfg.node_num)
			continue;
		getnodedat(i,&node,0);
		if(node.action!=NODE_MCHT || node.status!=NODE_INUSE)
			continue;
		if(node.aux && (node.aux&0xff)!=channel)
			continue;
		printnodedat(i,&node);
		preusr[usrs]=(char)i;
		usr[usrs++]=(char)i;
	}
	preusrs=usrs;
	if(gurubuf)
		bprintf(text[NodeInMultiChatLocally]
			,cfg.sys_nodes+1,cfg.guru[cfg.chan[channel-1]->guru]->name,channel);
	bputs(text[YoureOnTheAir]);
	done=0;
	while(online && !done) {
		checkline();
		gettimeleft();
		action=NODE_MCHT;
		qusrs=usrs=0;
        for(i=1;i<=cfg.sys_nodes;i++) {
			if(i==cfg.node_num)
				continue;
			getnodedat(i,&node,0);
			if(node.action!=NODE_MCHT
				|| (node.aux && channel && (node.aux&0xff)!=channel))
				continue;
			if(node.status==NODE_QUIET)
				qusr[qusrs++]=(char)i;
			else if(node.status==NODE_INUSE)
				usr[usrs++]=(char)i;
		}
		if(preusrs>usrs) {
			if(!usrs && channel && cfg.chan[channel-1]->misc&CHAN_GURU
				&& cfg.chan[channel-1]->guru<cfg.total_gurus)
				bprintf(text[NodeJoinedMultiChat]
					,cfg.sys_nodes+1,cfg.guru[cfg.chan[channel-1]->guru]->name
					,channel);
			outchar(BEL);
			for(i=0;i<preusrs;i++) {
				for(j=0;j<usrs;j++)
					if(preusr[i]==usr[j])
						break;
				if(j==usrs) {
					getnodedat(preusr[i],&node,0);
					if(node.misc&NODE_ANON)
						sprintf(str,"%.80s",text[UNKNOWN_USER]);
					else
						username(&cfg,node.useron,str);
					bprintf(text[NodeLeftMultiChat]
						,preusr[i],str,channel);
				}
			}
		}
		else if(preusrs<usrs) {
			if(!preusrs && channel && cfg.chan[channel-1]->misc&CHAN_GURU
				&& cfg.chan[channel-1]->guru<cfg.total_gurus)
				bprintf(text[NodeLeftMultiChat]
					,cfg.sys_nodes+1,cfg.guru[cfg.chan[channel-1]->guru]->name
					,channel);
			outchar(BEL);
			for(i=0;i<usrs;i++) {
				for(j=0;j<preusrs;j++)
					if(usr[i]==preusr[j])
						break;
				if(j==preusrs) {
					getnodedat(usr[i],&node,0);
					if(node.misc&NODE_ANON)
						sprintf(str,"%.80s",text[UNKNOWN_USER]);
					else
						username(&cfg,node.useron,str);
					bprintf(text[NodeJoinedMultiChat]
						,usr[i],str,channel);
				}
			}
		}
		preusrs=usrs;
		for(i=0;i<usrs;i++)
			preusr[i]=usr[i];
		attr(cfg.color[clr_multichat]);
		SYNC;
		sys_status&=~SS_ABORT;
		if((ch=inkey(K_NONE,250))!=0 || wordwrap[0]) {
			if(ch=='/') {
				bputs(text[MultiChatCommandPrompt]);
				strcpy(str,"ACELWQ?*");
				if(SYSOP)
					SAFECAT(str,"0");
				i=getkeys(str,cfg.total_chans);
				if(i&0x80000000L) {  /* change channel */
					savch=(char)(i&~0x80000000L);
					if(savch==channel)
						continue;
					if(!chan_access(savch-1))
						continue;
					bprintf(text[WelcomeToChannelN]
						,savch,cfg.chan[savch-1]->name);

					usrs=0;
					for(i=1;i<=cfg.sys_nodes;i++) {
						if(i==cfg.node_num)
							continue;
						getnodedat(i,&node,0);
						if(node.action!=NODE_MCHT
							|| node.status!=NODE_INUSE)
							continue;
						if(node.aux && (node.aux&0xff)!=savch)
							continue;
						printnodedat(i,&node);
						if(node.aux&0x1f00) {	/* password */
							bprintf(text[PasswordProtected]
								,node.misc&NODE_ANON
								? text[UNKNOWN_USER]
								: username(&cfg,node.useron,tmp));
							if(!getstr(str,8,K_UPPER|K_ALPHA|K_LINE))
								break;
							if(strcmp(str,unpackchatpass(tmp,&node)))
								break;
							bputs(text[CorrectPassword]);
						}
						preusr[usrs]=(char)i;
						usr[usrs++]=(char)i;
					}
					if(i<=cfg.sys_nodes) {	/* failed password */
						bputs(text[WrongPassword]);
						continue;
					}
					if(gurubuf) {
						free(gurubuf);
						gurubuf=NULL;
					}
					if(cfg.chan[savch-1]->misc&CHAN_GURU
						&& cfg.chan[savch-1]->guru<cfg.total_gurus
						&& chk_ar(cfg.guru[cfg.chan[savch-1]->guru]->ar,&useron,&client
						)) {
						sprintf(str,"%s%s.dat",cfg.ctrl_dir
							,cfg.guru[cfg.chan[savch-1]->guru]->code);
						if((file=nopen(str,O_RDONLY))==-1) {
							errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
							break;
						}
						if((gurubuf=(char *)malloc((size_t)filelength(file)+1))==NULL) {
							close(file);
							errormsg(WHERE,ERR_ALLOC,str
								,(size_t)filelength(file)+1);
							break;
						}
						(void)read(file,gurubuf,(size_t)filelength(file));
						gurubuf[filelength(file)]=0;
						close(file);
					}
					preusrs=usrs;
					if(gurubuf)
						bprintf(text[NodeInMultiChatLocally]
							,cfg.sys_nodes+1
							,cfg.guru[cfg.chan[savch-1]->guru]->name
							,savch);
					channel=savch;
					if(!usrs && cfg.chan[savch-1]->misc&CHAN_PW
						&& !noyes(text[PasswordProtectChanQ])) {
						bputs(text[PasswordPrompt]);
						if(getstr(str,8,K_UPPER|K_ALPHA|K_LINE)) {
							getnodedat(cfg.node_num,&thisnode,true);
							thisnode.aux=channel;
							packchatpass(str,&thisnode);
						}
						else {
							getnodedat(cfg.node_num,&thisnode,true);
							thisnode.aux=channel;
						}
					}
					else {
						getnodedat(cfg.node_num,&thisnode,true);
						thisnode.aux=channel;
					}
					putnodedat(cfg.node_num,&thisnode);
					bputs(text[YoureOnTheAir]);
					if(cfg.chan[channel-1]->cost
						&& !(useron.exempt&FLAG('J')))
						subtract_cdt(&cfg,&useron,cfg.chan[channel-1]->cost);
				}
				else switch(i) {	/* other command */
					case '0':	/* Global channel */
						if(!SYSOP)
							break;
                        usrs=0;
						for(i=1;i<=cfg.sys_nodes;i++) {
							if(i==cfg.node_num)
								continue;
							getnodedat(i,&node,0);
							if(node.action!=NODE_MCHT
								|| node.status!=NODE_INUSE)
								continue;
							printnodedat(i,&node);
							preusr[usrs]=(char)i;
							usr[usrs++]=(char)i;
						}
						preusrs=usrs;
						if(getnodedat(cfg.node_num,&thisnode,true)==0) {
							thisnode.aux=channel=0;
							putnodedat(cfg.node_num,&thisnode);
						}
						break;
					case 'A':   /* Action commands */
						useron.chat^=CHAT_ACTION;
						bprintf("\r\nAction commands are now %s\r\n"
							,useron.chat&CHAT_ACTION
							? text[ON]:text[OFF]);
						putuserrec(&cfg,useron.number,U_CHAT,8
							,ultoa(useron.chat,str,16));
						break;
					case 'C':   /* List of action commands */
						CRLF;
						for(i=0;channel && i<cfg.total_chatacts;i++) {
							if(cfg.chatact[i]->actset
								!=cfg.chan[channel-1]->actset)
								continue;
							bprintf("%-*.*s",LEN_CHATACTCMD
								,LEN_CHATACTCMD,cfg.chatact[i]->cmd);
							if(!((i+1)%8)) {
								CRLF;
							}
							else
								bputs(" ");
						}
						CRLF;
						break;
					case 'E':   /* Toggle echo */
						useron.chat^=CHAT_ECHO;
						bprintf(text[EchoIsNow]
							,useron.chat&CHAT_ECHO
							? text[ON]:text[OFF]);
						putuserrec(&cfg,useron.number,U_CHAT,8
							,ultoa(useron.chat,str,16));
						break;
					case 'L':	/* list nodes */
						CRLF;
						for(i=1;i<=cfg.sys_nodes && i<=cfg.sys_lastnode;i++) {
							getnodedat(i,&node,0);
							printnodedat(i,&node);
						}
						CRLF;
						break;
					case 'W':   /* page node(s) */
						j=getnodetopage(0,0);
						if(!j)
							break;
						for(i=0;i<usrs;i++)
							if(usr[i]==j)
								break;
						if(i>=usrs) {
							bputs(text[UserNotFound]);
							break; 
						}

						bputs(text[NodeMsgPrompt]);
						if(!getstr(line,66,K_LINE|K_MSG))
							break;

						sprintf(buf,text[ChatLineFmt]
							,thisnode.misc&NODE_ANON
							? text[AnonUserChatHandle]
							: useron.handle
							,cfg.node_num,'*',line);
						SAFECAT(buf,crlf);
						if(useron.chat&CHAT_ECHO)
							bputs(buf);
						putnmsg(&cfg,j,buf);
						break;
					case 'Q':	/* quit */
						done=1;
						break;
					case '*':
						if(!menu("chan", P_NOERROR)) {
							bputs(text[ChatChanLstHdr]);
							bputs(text[ChatChanLstTitles]);
							if(cfg.total_chans>=10) {
								bputs("     ");
								bputs(text[ChatChanLstTitles]);
							}
							CRLF;
							bputs(text[ChatChanLstUnderline]);
							if(cfg.total_chans>=10) {
								bputs("     ");
								bputs(text[ChatChanLstUnderline]);
							}
							CRLF;
							if(cfg.total_chans>=10)
								j=(cfg.total_chans/2)+(cfg.total_chans&1);
							else
								j=cfg.total_chans;
							for(i=0;i<j && !msgabort();i++) {
								bprintf(text[ChatChanLstFmt],i+1
									,cfg.chan[i]->name
									,cfg.chan[i]->cost);
								if(cfg.total_chans>=10) {
									k=(cfg.total_chans/2)
										+i+(cfg.total_chans&1);
									if(k<cfg.total_chans) {
										bputs("     ");
										bprintf(text[ChatChanLstFmt]
											,k+1
											,cfg.chan[k]->name
											,cfg.chan[k]->cost);
									}
								}
								CRLF;
							}
							CRLF;
						}
						break;
					case '?':	/* menu */
						menu("multchat");
						break;
				}
			} else {
				ungetkey(ch);
				j=0;
				pgraph[0]=0;
				while(j<5) {
					if(!getstr(line,66,K_WRAP|K_MSG|K_CHAT))
						break;
					if(j) {
						sprintf(str,text[ChatLineFmt]
							,thisnode.misc&NODE_ANON
							? text[AnonUserChatHandle]
							: useron.handle
							,cfg.node_num,':',nulstr);
						sprintf(tmp,"%*s",(int)bstrlen(str),nulstr);
						SAFECAT(pgraph,tmp);
					}
					SAFECAT(pgraph,line);
					SAFECAT(pgraph,crlf);
					if(!wordwrap[0])
						break;
					j++;
				}
				if(pgraph[0]) {
					if(channel && useron.chat&CHAT_ACTION) {
						for(i=0;i<cfg.total_chatacts;i++) {
							if(cfg.chatact[i]->actset
								!=cfg.chan[channel-1]->actset)
								continue;
							sprintf(str,"%s ",cfg.chatact[i]->cmd);
							if(!strnicmp(str,pgraph,strlen(str)))
								break;
							sprintf(str,"%.*s"
								,LEN_CHATACTCMD+2,pgraph);
							str[strlen(str)-2]=0;
							if(!stricmp(cfg.chatact[i]->cmd,str))
								break;
						}

						if(i<cfg.total_chatacts) {
							p=pgraph+strlen(str);
							n=atoi(p);
							for(j=0;j<usrs;j++) {
								getnodedat(usr[j],&node,0);
								if(usrs==1) /* no need to search */
									break;
								if(n) {
									if(usr[j]==n)
										break;
									continue;
								}
								username(&cfg,node.useron,str);
								if(!strnicmp(str,p,strlen(str)))
									break;
								getuserrec(&cfg,node.useron,U_HANDLE
									,LEN_HANDLE,str);
								if(!strnicmp(str,p,strlen(str)))
									break;
							}
							if(!usrs
								&& cfg.chan[channel-1]->guru<cfg.total_gurus)
								strcpy(str
								,cfg.guru[cfg.chan[channel-1]->guru]->name);
							else if(j>=usrs)
								strcpy(str,"everyone");
							else if(node.misc&NODE_ANON)
								strcpy(str,text[UNKNOWN_USER]);
							else
								username(&cfg,node.useron,str);

							/* Display on same node */
							bprintf(cfg.chatact[i]->out
								,thisnode.misc&NODE_ANON
								? text[UNKNOWN_USER] : useron.alias
								,str);
							CRLF;

							if(usrs && j<usrs) {
								/* Display to dest user */
								sprintf(buf,cfg.chatact[i]->out
									,thisnode.misc&NODE_ANON
									? text[UNKNOWN_USER] : useron.alias
									,"you");
								SAFECAT(buf,crlf);
								putnmsg(&cfg,usr[j],buf);
							}


							/* Display to all other users */
							sprintf(buf,cfg.chatact[i]->out
								,thisnode.misc&NODE_ANON
								? text[UNKNOWN_USER] : useron.alias
								,str);
							SAFECAT(buf,crlf);

							for(i=0;i<usrs;i++) {
								if(i==j)
									continue;
								getnodedat(usr[i],&node,0);
								putnmsg(&cfg,usr[i],buf);
							}
							for(i=0;i<qusrs;i++) {
								getnodedat(qusr[i],&node,0);
								putnmsg(&cfg,qusr[i],buf);
							}
							continue;
						}
					}

					sprintf(buf,text[ChatLineFmt]
						,thisnode.misc&NODE_ANON
						? text[AnonUserChatHandle]
						: useron.handle
						,cfg.node_num,':',pgraph);
					if(useron.chat&CHAT_ECHO)
						bputs(buf);
					for(i=0;i<usrs;i++) {
						getnodedat(usr[i],&node,0);
						putnmsg(&cfg,usr[i],buf);
					}
					for(i=0;i<qusrs;i++) {
						getnodedat(qusr[i],&node,0);
						putnmsg(&cfg,qusr[i],buf);
					}
					if(!usrs && channel && gurubuf
						&& cfg.chan[channel-1]->misc&CHAN_GURU)
						guruchat(pgraph,gurubuf,cfg.chan[channel-1]->guru,guru_lastanswer);
				}
			}
		}
		if(sys_status&SS_ABORT)
			break;
	}
	lncntr=0;
	if(gurubuf != NULL)
		free(gurubuf);
}

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::guru_page(void)
{
	char	path[MAX_PATH+1];
	char*	gurubuf;
	int 	file;
	long	i;

	if(useron.rest&FLAG('C')) {
		bputs(text[R_Chat]);
		return(false);
	}

	if(!cfg.total_gurus) {
		bprintf(text[SysopIsNotAvailable],"The Guru");
		return(false);
	}
	if(cfg.total_gurus==1 && chk_ar(cfg.guru[0]->ar,&useron,&client))
		i=0;
	else {
		for(i=0;i<cfg.total_gurus;i++)
			uselect(1,i,nulstr,cfg.guru[i]->name,cfg.guru[i]->ar);
		i=uselect(0,0,0,0,0);
		if(i<0)
			return(false);
	}
	sprintf(path,"%s%s.dat",cfg.ctrl_dir,cfg.guru[i]->code);
	if((file=nopen(path,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,path,O_RDONLY);
		return(false);
	}
	if((gurubuf=(char *)malloc((size_t)filelength(file)+1))==NULL) {
		errormsg(WHERE,ERR_ALLOC,path,(size_t)filelength(file)+1);
		close(file);
		return(false);
	}
	(void)read(file,gurubuf,(size_t)filelength(file));
	gurubuf[filelength(file)]=0;
	close(file);
	localguru(gurubuf,i);
	free(gurubuf);
	return(true);
}

/****************************************************************************/
/* The chat section                                                         */
/****************************************************************************/
void sbbs_t::chatsection()
{
	char	str[256],ch,no_rip_menu;

	if(useron.rest&FLAG('C')) {
		bputs(text[R_Chat]);
		return;
	}

	action=NODE_CHAT;
	if(useron.misc&(RIP|WIP|HTML) || !(useron.misc&EXPERT))
		menu("chat");
	ASYNC;
	bputs(text[ChatPrompt]);
	while(online) {
		no_rip_menu=0;
		ch=(char)getkeys("ACDJPQST?\r",0);
		if(ch>' ')
			logch(ch,0);
		switch(ch) {
			case 'S':
				useron.chat^=CHAT_SPLITP;
				putuserrec(&cfg,useron.number,U_CHAT,8
					,ultoa(useron.chat,str,16));
				bprintf("\r\nPrivate split-screen chat is now: %s\r\n"
					,useron.chat&CHAT_SPLITP ? text[ON]:text[OFF]);
				break;
			case 'A':
				CRLF;
				useron.chat^=CHAT_NOACT;
				putuserrec(&cfg,useron.number,U_CHAT,8
					,ultoa(useron.chat,str,16));
				if(getnodedat(cfg.node_num,&thisnode,true)==0) {
					thisnode.misc^=NODE_AOFF;
					printnodedat(cfg.node_num,&thisnode);
				}
				putnodedat(cfg.node_num,&thisnode);
				no_rip_menu=true;
				break;
			case 'D':
				CRLF;
				useron.chat^=CHAT_NOPAGE;
				putuserrec(&cfg,useron.number,U_CHAT,8
					,ultoa(useron.chat,str,16));
				if(getnodedat(cfg.node_num,&thisnode,true)==0) {
					thisnode.misc^=NODE_POFF;
					printnodedat(cfg.node_num,&thisnode);
				}
				putnodedat(cfg.node_num,&thisnode);
				no_rip_menu=true;
				break;
			case 'J':
				multinodechat();
				break;
			case 'P':   /* private node-to-node chat */
				privchat();
				break;
			case 'C':
				no_rip_menu=1;
				if(sysop_page())
					break;
				if(cfg.total_gurus && chk_ar(cfg.guru[0]->ar,&useron,&client) && text[ChatWithGuruInsteadQ][0]) {
					SAFEPRINTF(str,text[ChatWithGuruInsteadQ],cfg.guru[0]->name);
					if(!yesno(str))
						break; 
				}
				else
					break;
				/* FALL-THROUGH */
			case 'T':
				guru_page();
				no_rip_menu=1;
				break;
			case '?':
				if(useron.misc&EXPERT)
					menu("chat");
				break;
			default:	/* 'Q' or <CR> */
				lncntr=0;
//				if(gurubuf)
//					free(gurubuf);
				return; 
		}
		action=NODE_CHAT;
		if(!(useron.misc&EXPERT) || useron.misc&(WIP|HTML)
			|| (useron.misc&RIP && !no_rip_menu)) {
			menu("chat"); 
		}
		ASYNC;
		bputs(text[ChatPrompt]); 
	}
//	if(gurubuf)
//		free(gurubuf);
}

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::sysop_page(void)
{
	char	str[256];
	int		i;

	if(useron.rest&FLAG('C')) {
		bputs(text[R_Chat]);
		return(false); 
	}

	if(sysop_available(&cfg)
		|| (cfg.sys_chat_ar[0] && chk_ar(cfg.sys_chat_ar,&useron,&client))
		|| useron.exempt&FLAG('C')) {

		logline("C", "paged sysop for chat");
		sprintf(str, "%s paged you to chat", useron.alias);
		notify(&cfg, 1, str, NULL);

		ftouch(syspage_semfile);
		for(i=0;i<cfg.total_pages;i++)
			if(chk_ar(cfg.page[i]->ar,&useron,&client))
				break;
		if(i<cfg.total_pages) {
			bprintf(text[PagingGuru],cfg.sys_op);
			long mode = 0;
			if(cfg.page[i]->misc&XTRN_STDIO)
				mode |= EX_STDIO;
			if(cfg.page[i]->misc&XTRN_NATIVE)
				mode|= EX_NATIVE;
			if(cfg.page[i]->misc&XTRN_SH)
				mode |= EX_SH;
			external(cmdstr(cfg.page[i]->cmd,nulstr,nulstr,NULL,mode), mode); 
		}
		else if(cfg.sys_misc&SM_SHRTPAGE) {
			bprintf(text[PagingGuru],cfg.sys_op);
			for(i=0;i<10 && !lkbrd(1);i++) {
				sbbs_beep(1000,200);
				mswait(200);
				outchar('.'); 
			}
			CRLF; 
		}
		else {
			sys_status^=SS_SYSPAGE;
			bprintf(text[SysopPageIsNow]
				,sys_status&SS_SYSPAGE ? text[ON] : text[OFF]);
			nosound();	
		}
		if(!(sys_status&SS_SYSPAGE))
			remove(syspage_semfile);

		return(true);
	}

	bprintf(text[SysopIsNotAvailable],cfg.sys_op);

	return(false);
}

/****************************************************************************/
/* Returns 1 if user online has access to channel "channum"                 */
/****************************************************************************/
bool sbbs_t::chan_access(uint cnum)
{

	if(!cfg.total_chans || cnum>=cfg.total_chans || !chk_ar(cfg.chan[cnum]->ar,&useron,&client)) {
		bputs(text[CantAccessThatChannel]);
		return(false); 
	}
	if(!(useron.exempt&FLAG('J')) && cfg.chan[cnum]->cost>useron.cdt+useron.freecdt) {
		bputs(text[NotEnoughCredits]);
		return(false); 
	}
	return(true);
}

/****************************************************************************/
/* Private split-screen (or interspersed) chat with node or local sysop		*/
/****************************************************************************/
void sbbs_t::privchat(bool forced, int node_num)
{
	char	str[128],c,*p,localbuf[5][81],remotebuf[5][81]
			,localline=0,remoteline=0,localchar=0,remotechar=0
			,*sep=text[PrivateChatSeparator]
			,*local_sep=text[SysopChatSeparator]
			;
	char 	tmp[512];
	char	outpath[MAX_PATH+1];
	char	inpath[MAX_PATH+1];
	uchar	ch;
	int 	in,out,i,n,echo=1,x,y,activity,remote_activity;
    int		local_y=1,remote_y=1;
	node_t	node;
	time_t	last_nodechk=0;

	if(forced)
		n = node_num;
	else {

		if(useron.rest&FLAG('C')) {
			bputs(text[R_Chat]);
			return; 
		}

		n=getnodetopage(0,0);
		if(!n)
			return;
		if(n==cfg.node_num) {
			bputs(text[NoNeedToPageSelf]);
			return; 
		}
		getnodedat(n,&node,0);
		if(node.action==NODE_PCHT && node.aux!=cfg.node_num) {
			bprintf(text[NodeNAlreadyInPChat],n);
			return; 
		}
		if(SYSOP && getnodedat(n, &node, true) == 0) {
			node.misc |= NODE_FCHAT;
			putnodedat(n, &node);
		} else {
			if((node.action!=NODE_PAGE || node.aux!=cfg.node_num)
				&& node.misc&NODE_POFF) {
				bprintf(text[CantPageNode],node.misc&NODE_ANON
					? text[UNKNOWN_USER] : username(&cfg,node.useron,tmp));
				return;
			}
			if(node.action!=NODE_PAGE) {
				bprintf(text[PagingUser]
					,node.misc&NODE_ANON ? text[UNKNOWN_USER] : username(&cfg,node.useron,tmp)
					,node.misc&NODE_ANON ? 0 : node.useron);
				sprintf(str,text[NodePChatPageMsg]
					,cfg.node_num,thisnode.misc&NODE_ANON
						? text[UNKNOWN_USER] : useron.alias);
				putnmsg(&cfg,n,str);
				sprintf(str,"paged %s on node %d to private chat"
					,username(&cfg,node.useron,tmp),n);
				logline("C",str);
			}
		}

		if(getnodedat(cfg.node_num,&thisnode,true)==0) {
			thisnode.action=action=NODE_PAGE;
			thisnode.aux=n;
			putnodedat(cfg.node_num,&thisnode);
		}

		if(node.action!=NODE_PAGE || node.aux!=cfg.node_num) {
			bprintf(text[WaitingForNodeInPChat],n);
			while(online && !(sys_status&SS_ABORT)) {
				getnodedat(n,&node,0);
				if((node.action==NODE_PAGE || node.action==NODE_PCHT)
					&& node.aux==cfg.node_num) {
					bprintf(text[NodeJoinedPrivateChat]
						,n,node.misc&NODE_ANON ? text[UNKNOWN_USER]
							: username(&cfg,node.useron,tmp));
					break; 
				}
				action=NODE_PAGE;
				checkline();
				gettimeleft();
				SYNC; 
				inkey(K_NONE,500);
			} 
		}
	}

	gettimeleft();

	if(getnodedat(cfg.node_num,&thisnode,true)==0) {
		thisnode.action=action=NODE_PCHT;
		thisnode.aux=n;
		thisnode.misc&=~ (NODE_LCHAT|NODE_FCHAT);
		putnodedat(cfg.node_num,&thisnode);
	}

	if(!online || (!forced && (sys_status&SS_ABORT)))
		return;

	if(forced && n == 0) {
		/* If an external sysop chat event handler is installed, just run that and do nothing else */
		if(user_event(EVENT_LOCAL_CHAT))
			return;
	}

	if(((sys_status&SS_USERON && useron.chat&CHAT_SPLITP) || !(sys_status&SS_USERON))
		&& term_supports(ANSI) && rows>=24 && cols>=80)
		sys_status|=SS_SPLITP;
	else
		sys_status&=~SS_SPLITP;
	/*
	if(!(useron.misc&EXPERT))
		menu("privchat");
	*/

	if(!(sys_status&SS_SPLITP)) {
		if(forced)
			bprintf(text[SysopIsHere],cfg.sys_op);
		else
			bputs(text[WelcomeToPrivateChat]);
	}

	sprintf(outpath,"%schat.dab",cfg.node_dir);
	if((out=sopen(outpath,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,DEFFILEMODE))==-1) {
		errormsg(WHERE,ERR_OPEN,outpath,O_RDWR|O_DENYNONE|O_CREAT);
		return; 
	}

	if(forced && n == 0)
		sprintf(inpath,"%slchat.dab",cfg.node_dir);
	else
		sprintf(inpath,"%schat.dab",cfg.node_path[n-1]);
	if(!fexist(inpath))		/* Wait while it's created for the first time */
		mswait(2000);
	if((in=sopen(inpath,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,DEFFILEMODE))==-1) {
		close(out);
		errormsg(WHERE,ERR_OPEN,inpath,O_RDWR|O_DENYNONE|O_CREAT);
		return; 
	}

	if((p=(char *)malloc(PCHAT_LEN))==NULL) {
		close(in);
		close(out);
		errormsg(WHERE,ERR_ALLOC,nulstr,PCHAT_LEN);
		return; 
	}
	memset(p,0,PCHAT_LEN);
	write(in,p,PCHAT_LEN);
	write(out,p,PCHAT_LEN);
	free(p);
	lseek(in,0L,SEEK_SET);
	lseek(out,0L,SEEK_SET);

	if(getnodedat(cfg.node_num,&thisnode,true)==0) {
		thisnode.misc&=~NODE_RPCHT; 		/* Clear "reset pchat flag" */
		putnodedat(cfg.node_num,&thisnode);
	}

	if(n) { // not local
		if(getnodedat(n,&node,true)==0) {
			node.misc|=NODE_RPCHT;				/* Set "reset pchat flag" */
			putnodedat(n,&node); 				/* on other node */
		}

											/* Wait for other node */
											/* to acknowledge and reset */
		while(online && !(sys_status&SS_ABORT)) {
			getnodedat(n,&node,0);
			if(!(node.misc&NODE_RPCHT))
				break;
			getnodedat(cfg.node_num,&thisnode,0);
			if(thisnode.misc&NODE_RPCHT)
				break;
			checkline();
			gettimeleft();
			SYNC;
			SLEEP(500); 
		}
	}

	action=NODE_PCHT;
	SYNC;

	if(sys_status&SS_SPLITP) {
		lncntr=0;
		CLS;
		ansi_save();
		ansi_gotoxy(1,13);
		remote_y=1;
		bprintf(forced ? local_sep : sep
			,thisnode.misc&NODE_MSGW ? 'T':' '
			,sectostr(timeleft,tmp)
			,thisnode.misc&NODE_NMSG ? 'M':' ');
		ansi_gotoxy(1,14);
		local_y=14; 
	}

	while(online && (forced || !(sys_status&SS_ABORT))) {
		lncntr=0;
		if(sys_status&SS_SPLITP)
			lbuflen=0;
		action=NODE_PCHT;
		activity=0;
		remote_activity=0;
		if((ch=inkey(K_GETSTR,100))!=0) {
			activity=1;
			if(echo)
				attr(cfg.color[clr_chatlocal]);
			if(ch==BS || ch==DEL) {
				if(localchar) {
					if(echo)
						backspace();
					localchar--;
					localbuf[localline][localchar]=0; 
				} 
			}
			else if(ch==TAB) {
				if(echo)
					outchar(' ');
				localbuf[localline][localchar]=' ';
				localchar++;
				while(localchar<78 && localchar%8) {
					if(echo)
						outchar(' ');
					localbuf[localline][localchar++]=' '; 
				} 
			}
			else if(ch==CTRL_R) {
				if(sys_status&SS_SPLITP) {
					CLS;
					attr(cfg.color[clr_chatremote]);
					remotebuf[remoteline][remotechar]=0;
					for(i=0;i<=remoteline;i++) {
						bputs(remotebuf[i]); 
						if(i!=remoteline) 
							bputs(crlf);
					}
					remote_y=1+remoteline;
					bputs("\1i_\1n");  /* Fake cursor */
					ansi_save();
					ansi_gotoxy(1,13);
					bprintf(forced ? local_sep : sep
						,thisnode.misc&NODE_MSGW ? 'T':' '
						,sectostr(timeleft,tmp)
						,thisnode.misc&NODE_NMSG ? 'M':' ');
					ansi_gotoxy(1,14);
					attr(cfg.color[clr_chatlocal]);
					localbuf[localline][localchar]=0;
					for(i=0;i<=localline;i++) {
						bputs(localbuf[i]);
						if(i!=localline) 
							bputs(crlf);
					}
					local_y=15+localline;
				}
				continue;
			}
			else if(ch>=' ' || ch==CR) {
				if(ch!=CR) {
					if(echo)
						outchar(ch);
					localbuf[localline][localchar]=ch; 
				}

				if(ch==CR || (localchar>68 && ch==' ') || ++localchar>78) {

					localbuf[localline][localchar]=0;
					localchar=0;

					if(sys_status&SS_SPLITP && local_y >= rows) {
						ansi_gotoxy(1,13);
						bprintf(forced ? local_sep : sep
							,thisnode.misc&NODE_MSGW ? 'T':' '
							,sectostr(timeleft,tmp)
							,thisnode.misc&NODE_NMSG ? 'M':' ');
						attr(cfg.color[clr_chatlocal]);
						for(x=13,y=0;x<rows;x++,y++) {
							comprintf("\x1b[%d;1H\x1b[K",x+1);
							if(y<=localline)
								bprintf("%s\r\n",localbuf[y]); 
						}
						ansi_gotoxy(1,local_y=(15+localline));
						localline=0; 
					}
					else {
						if(localline>=4)
							for(i=0;i<4;i++)
								memcpy(localbuf[i],localbuf[i+1],81);
						else
							localline++;
						if(echo) {
							CRLF;
		            		local_y++;
							if(sys_status&SS_SPLITP)
								cleartoeol(); 
						} 
					}
					// SYNC;
				} 
			} else { // illegal key
				continue;
			}

			(void)read(out,&c,1);
			(void)lseek(out,-1L,SEEK_CUR);
			if(!c)		/* hasn't wrapped */
				write(out,&ch,1);
			else {
				if(!tell(out))
					lseek(out,0L,SEEK_END);
				lseek(out,-1L,SEEK_CUR);
				ch=0;
				write(out,&ch,1);
				lseek(out,-1L,SEEK_CUR); 
			}
			utime(outpath,NULL);	/* update mod time for NFS/smbfs nodes */
			if(tell(out)>=PCHAT_LEN)
				lseek(out,0L,SEEK_SET);
		}
		else while(online) {
			if(!(sys_status&SS_SPLITP))
				remotechar=localchar;
			if(tell(in)>=PCHAT_LEN)
				lseek(in,0L,SEEK_SET);
			ch=0;
			utime(inpath,NULL);
			if(read(in,&ch,1) != 1)
				ch = 0;
			(void)lseek(in,-1L,SEEK_CUR);
			if(!ch) break;					  /* char from other node */
			activity=1;
			if(sys_status&SS_SPLITP && !remote_activity) {
				ansi_getxy(&x,&y);
				ansi_restore();
			}
			attr(cfg.color[clr_chatremote]);
			if(sys_status&SS_SPLITP && !remote_activity)
				backspace();             /* Delete fake cursor */
			remote_activity=1;
			if(ch==BS || ch==DEL) {
				if(remotechar) {
					backspace();
					remotechar--;
					remotebuf[remoteline][remotechar]=0; 
				} 
			}
			else if(ch==TAB) {
				outchar(' ');
				remotebuf[remoteline][remotechar]=' ';
				remotechar++;
				while(remotechar<78 && remotechar%8) {
					outchar(' ');
					remotebuf[remoteline][remotechar++]=' '; 
				} 
			}
			else if(ch>=' ' || ch==CR) {
				if(ch!=CR) {
					outchar(ch);
					remotebuf[remoteline][remotechar]=ch; 
				}

				if(ch==CR || (remotechar>68 && ch==' ') || ++remotechar>78) {

					remotebuf[remoteline][remotechar]=0;
					remotechar=0;

					if(sys_status&SS_SPLITP && remote_y==12) {
						CRLF;
						bprintf(forced ? local_sep : sep
							,thisnode.misc&NODE_MSGW ? 'T':' '
							,sectostr(timeleft,tmp)
							,thisnode.misc&NODE_NMSG ? 'M':' ');
						attr(cfg.color[clr_chatremote]);
						for(i=0;i<12;i++) {
							bprintf("\x1b[%d;1H\x1b[K",i+1);
							if(i<=remoteline)
								bprintf("%s\r\n",remotebuf[i]); 
						}
						remoteline=0;
						ansi_gotoxy(1, remote_y=6); 
					}
					else {
						if(remoteline>=4)
							for(i=0;i<4;i++)
								memcpy(remotebuf[i],remotebuf[i+1],81);
						else
							remoteline++;
						if(echo) {
							CRLF;
		            		remote_y++;
							if(sys_status&SS_SPLITP)
								cleartoeol(); 
						} 
					} 
				} 
			}
			ch=0;
			write(in,&ch,1);

			if(!(sys_status&SS_SPLITP))
				localchar=remotechar;
			}

		if(sys_status&SS_SPLITP && remote_activity) {
			bputs("\1i_\1n");  /* Fake cursor */
			ansi_save();
			ansi_gotoxy(x,y); 
		}

		now=time(NULL);
		if(!activity && now!=last_nodechk) {	/* no activity so chk node.dab */

			if(!localchar) {
				if(sys_status&SS_SPLITP) {
					getnodedat(cfg.node_num,&thisnode,0);
					if(thisnode.misc&NODE_INTR)
						break;
					if(thisnode.misc&NODE_UDAT && !(useron.rest&FLAG('G'))) {
						getuserdat(&cfg,&useron);
						if(getnodedat(cfg.node_num,&thisnode,true)==0) {
							thisnode.misc&=~NODE_UDAT;
							putnodedat(cfg.node_num,&thisnode); 
						}
					} 
				}
				else
					nodesync(); 
			}

			if(n != 0) {
				getnodedat(n,&node,0);
				if((node.action!=NODE_PCHT && node.action!=NODE_PAGE)
					|| node.aux!=cfg.node_num) {
					bprintf(text[NodeLeftPrivateChat]
						,n,node.misc&NODE_ANON ? text[UNKNOWN_USER]
						: username(&cfg,node.useron,tmp));
					break; 
				}
			}
			getnodedat(cfg.node_num,&thisnode,0);
			if(thisnode.action!=NODE_PCHT) {
				action=thisnode.action;
				bputs(text[EndOfChat]);
				break;
			}
			if(thisnode.misc&NODE_RPCHT) {		/* pchat has been reset */
				lseek(in,0L,SEEK_SET);			/* so seek to beginning */
				lseek(out,0L,SEEK_SET);
				if(getnodedat(cfg.node_num,&thisnode,true)==0) {
					thisnode.misc&=~NODE_RPCHT;
					putnodedat(cfg.node_num,&thisnode); 
				}
			}
			last_nodechk=now;
			gettimeleft(); 
		}
	}
	if(sys_status&SS_SPLITP)
		CLS;
	sys_status&=~(SS_SPLITP|SS_ABORT);
	close(in);
	close(out);
}


int sbbs_t::getnodetopage(int all, int telegram)
{
	char	str[128];
	char 	tmp[512];
	uint 	i,j;
	ulong	l;
	node_t	node;

	if(!lastnodemsg)
		lastnodemsguser[0]=0;
	if(lastnodemsg) {
		getnodedat(lastnodemsg,&node,0);
		if(node.status!=NODE_INUSE && !SYSOP)
			lastnodemsg=1; 
	}
	for(j=0,i=1;i<=cfg.sys_nodes && i<=cfg.sys_lastnode;i++) {
		getnodedat(i,&node,0);
		if(i==cfg.node_num)
			continue;
		if(node.status==NODE_INUSE || (SYSOP && node.status==NODE_QUIET)) {
			if(!lastnodemsg)
				lastnodemsg=i;
			j++; 
		} 
	}

	if(!lastnodemsguser[0])
		sprintf(lastnodemsguser,"%u",lastnodemsg);

	if(!j && !telegram) {
		bputs(text[NoOtherActiveNodes]);
		return(0); 
	}

	if(all)
		sprintf(str,text[NodeToSendMsgTo],lastnodemsg);
	else
		sprintf(str,text[NodeToPrivateChat],lastnodemsg);
	mnemonics(str);

	SAFECOPY(str,lastnodemsguser);
	getstr(str,LEN_ALIAS,K_LINE|K_EDIT|K_AUTODEL);
	if(sys_status&SS_ABORT) {
		sys_status&= ~SS_ABORT;
		return(0);
	}
	if(!str[0])
		return(0);

	j=atoi(str);
	if(j && j<=cfg.sys_lastnode && j<=cfg.sys_nodes) {
		getnodedat(j,&node,0);
		if(node.status!=NODE_INUSE && !SYSOP) {
			bprintf(text[NodeNIsNotInUse],j);
			return(0); 
		}
		if(telegram && node.misc&(NODE_POFF|NODE_ANON) && !SYSOP) {
			bprintf(text[CantPageNode],node.misc&NODE_ANON
				? text[UNKNOWN_USER] : username(&cfg,node.useron,tmp));
			return(0); 
		}
		SAFECOPY(lastnodemsguser,str);
		if(telegram)
			return(node.useron);
		return(j); 
	}
	if(all && !stricmp(str,"ALL"))
		return(-1);

	if(str[0]=='\'') {
		j=userdatdupe(0,U_HANDLE,LEN_HANDLE,str+1);
		if(!j) {
			bputs(text[UnknownUser]);
			return(0); 
		} 
	}
	else if(str[0]=='#')
		j=atoi(str+1);
	else
		j=finduser(str);
	if(!j)
		return(0);
	if(j>lastuser(&cfg))
		return(0);
	getuserrec(&cfg,j,U_MISC,8,tmp);
	l=ahtoul(tmp);
	if(l&(DELETED|INACTIVE)) {              /* Deleted or Inactive User */
		bputs(text[UnknownUser]);
		return(0); 
	}

	for(i=1;i<=cfg.sys_nodes && i<=cfg.sys_lastnode;i++) {
		getnodedat(i,&node,0);
		if((node.status==NODE_INUSE || (SYSOP && node.status==NODE_QUIET))
			&& node.useron==j) {
			if(telegram && node.misc&NODE_POFF && !SYSOP) {
				bprintf(text[CantPageNode],node.misc&NODE_ANON
					? text[UNKNOWN_USER] : username(&cfg,node.useron,tmp));
				return(0); 
			}
			if(telegram)
				return(j);
			SAFECOPY(lastnodemsguser,str);
			return(i); 
		} 
	}
	if(telegram) {
		SAFECOPY(lastnodemsguser,str);
		return(j); 
	}
	bputs(text[UserNotFound]);
	return(0);
}


/****************************************************************************/
/* Sending single line messages between nodes                               */
/****************************************************************************/
void sbbs_t::nodemsg()
{
	char	str[256],line[256],buf[512],logbuf[512],ch=0;
	char 	tmp[512];
	int 	i,usernumber,done=0;
	node_t	node,savenode;

	if(nodemsg_inside>1)	/* nested once only */
		return;
	nodemsg_inside++;
	if(cfg.privatemsg_mod[0] != '\0') {
		exec_bin(cfg.privatemsg_mod, &main_csi);
		nodemsg_inside--;
		return;
	}
	sys_status|=SS_IN_CTRLP;
	getnodedat(cfg.node_num,&savenode,0);
	wordwrap[0]=0;
	while(online && !done) {
		if(useron.rest&FLAG('C')) {
			bputs(text[R_SendMessages]);
			break; 
		}
		SYNC;
		mnemonics(text[PrivateMsgPrompt]);
		sys_status&=~SS_ABORT;
		while(online) { 	 /* Watch for incoming messages */
			ch=toupper(inkey(K_NONE,1000));
			if(ch && strchr("TMCQ\r",ch))
				break;
			if(sys_status&SS_ABORT)
				break;
			if(getnodedat(cfg.node_num,&thisnode,false)==0) {
				if(thisnode.misc&(NODE_MSGW|NODE_NMSG)) {
					lncntr=0;	/* prevent pause prompt */
					SAVELINE;
					CRLF;
					if(thisnode.misc&NODE_NMSG)
						getnmsg();
					if(thisnode.misc&NODE_MSGW)
						getsmsg(useron.number);
					CRLF;
					RESTORELINE; 
				}
				else
					nodesync();
			}
			gettimeleft();
		}

		if(!online || sys_status&SS_ABORT) {
			sys_status&=~SS_ABORT;
			CRLF;
			break; 
		}

		switch(toupper(ch)) {
			case 'T':   /* Telegram */
				bputs("Telegram\r\n");
				usernumber=getnodetopage(0,1);
				if(!usernumber)
					break;

				if(usernumber==1 && useron.rest&FLAG('S')) { /* ! val fback */
					bprintf(text[R_Feedback],cfg.sys_op);
					break; 
				}
				if(usernumber!=1 && useron.rest&FLAG('E')) {
					bputs(text[R_Email]);
					break; 
				}
				now=time(NULL);
				bprintf(text[SendingTelegramToUser]
					,username(&cfg,usernumber,tmp),usernumber);
				sprintf(buf,text[TelegramFmt]
					,thisnode.misc&NODE_ANON ? text[UNKNOWN_USER] : useron.alias
					,timestr(now));
				i=0;
				logbuf[0]=0;
				while(online && i<5) {
					bprintf("%4s",nulstr);
					if(!getstr(line,70,K_WRAP|K_MSG))
						break;
					SAFEPRINTF2(str,"%4s%s\r\n",nulstr,line);
					SAFECAT(buf,str);
					if(line[0]) {
						if(i)
							SAFECAT(logbuf,"   ");
						SAFECAT(logbuf,line);
					}
					i++; 
				}
				if(!i)
					break;
				if(sys_status&SS_ABORT) {
					CRLF;
					break; 
				}
				putsmsg(&cfg,usernumber,buf);
				sprintf(str,"sent telegram to %s #%u"
					,username(&cfg,usernumber,tmp),usernumber);
				logline("C",str);
				logline(nulstr,logbuf);
				bprintf(text[MsgSentToUser],"Telegram"
					,username(&cfg,usernumber,tmp),usernumber);
				break;
			case 'M':   /* Message */
				bputs("Message\r\n");
				i=getnodetopage(1,0);
				if(!i)
					break;
				if(i!=-1) {
					getnodedat(i,&node,0);
					usernumber=node.useron;
					if(node.misc&NODE_POFF && !SYSOP)
						bprintf(text[CantPageNode],node.misc&NODE_ANON
							? text[UNKNOWN_USER] : username(&cfg,node.useron,tmp));
					else {
						bprintf(text[SendingMessageToUser]
							,node.misc&NODE_ANON ? text[UNKNOWN_USER]
							: username(&cfg,node.useron,tmp)
							,node.misc&NODE_ANON ? 0 : node.useron);
						bputs(text[NodeMsgPrompt]);
						if(!getstr(line,69,K_LINE))
							break;
						sprintf(buf,text[NodeMsgFmt],cfg.node_num
							,thisnode.misc&NODE_ANON
								? text[UNKNOWN_USER] : useron.alias,line);
						putnmsg(&cfg,i,buf);
						if(!(node.misc&NODE_ANON))
							bprintf(text[MsgSentToUser],"Message"
								,username(&cfg,usernumber,tmp),usernumber);
						sprintf(str,"sent message to %s on node %d:"
							,username(&cfg,usernumber,tmp),i);
						logline("C",str);
						logline(nulstr,line); 
					} 
				}
				else {	/* ALL */
					bputs(text[NodeMsgPrompt]);
					if(!getstr(line,70,K_LINE))
						break;
					sprintf(buf,text[AllNodeMsgFmt],cfg.node_num
						,thisnode.misc&NODE_ANON
							? text[UNKNOWN_USER] : useron.alias,line);
					for(i=1;i<=cfg.sys_nodes;i++) {
						if(i==cfg.node_num)
							continue;
						getnodedat(i,&node,0);
						if((node.status==NODE_INUSE
							|| (SYSOP && node.status==NODE_QUIET))
							&& (SYSOP || !(node.misc&NODE_POFF)))
							putnmsg(&cfg,i,buf); 
					}
					SAFECOPY(str,"sent message to all nodes");
					logline("C",str);
					logline(nulstr,line); 
				}
				break;
			case 'C':   /* Chat */
				bputs("Chat\r\n");
				if(action==NODE_PCHT) { /* already in pchat */
					done=1;
					break; 
				}
				privchat();
				action=savenode.action;
				break;
			default:
				bputs("Quit\r\n");
				done=1;
				break; 
		} 
	}
	nodemsg_inside--;
	if(!nodemsg_inside)
		sys_status&=~SS_IN_CTRLP;
	if(getnodedat(cfg.node_num,&thisnode,true)==0) {
		thisnode.action=action=savenode.action;
		thisnode.aux=savenode.aux;
		thisnode.extaux=savenode.extaux;
		putnodedat(cfg.node_num,&thisnode);
	}
}

/****************************************************************************/
/* The guru will respond from the 'guru' buffer to 'line'                   */
/****************************************************************************/
void sbbs_t::guruchat(char* line, char* gurubuf, int gurunum, char* last_answer)
{
	char	str[512],cstr[512],*ptr,*answer[100],theanswer[1024]
			,mistakes=1,hu=0;
	char 	tmp[512];
	int		file;
	uint 	c,i,j,k,answers;
	long 	len;
	struct	tm tm;

	now=time(NULL);
	localtime_r(&now,&tm);

	for(i=0;i<100;i++) {
		if((answer[i]=(char *)malloc(512))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,512);
			while(i) {
				i--;
				free(answer[i]); 
			}
			sys_status&=~SS_GURUCHAT;
			return; 
		} 
	}
	ptr=gurubuf;
	len=strlen(gurubuf);
	strupr(line);
	j=strlen(line);
	k=0;
	for(i=0;i<j;i++) {
		if(line[i]<0 || !IS_ALPHANUMERIC(line[i])) {
			if(!k)	/* beginning non-alphanumeric */
				continue;
			if(line[i]==line[i+1])	/* redundant non-alnum */
				continue;
			if(line[i+1]=='?')	/* fix "WHAT ?" */
				continue;
		}
		cstr[k++]=line[i]; 
	}
	cstr[k]=0;
	while(k) {
		k--;
		if(!IS_ALPHANUMERIC(cstr[k]))
			continue;
		break; 
	}
	if(k<1) {
		for(i=0;i<100;i++)
			free(answer[i]);
		return; 
	}
	if(cstr[k+1]=='?')
		k++;
	cstr[k+1]=0;
	while(*ptr && ptr<gurubuf+len) {
		if(*ptr=='(') {
			ptr++;
			if(!guruexp(&ptr,cstr)) {
				while(*ptr && *ptr!='(' && ptr<gurubuf+len)
					ptr++;
				continue; 
			} 
		}
		else {
			while(*ptr && *ptr!=LF && ptr<gurubuf+len) /* skip LF after ')' */
				ptr++;
			ptr++;
			answers=0;
			while(*ptr && answers<100 && ptr<gurubuf+len) {
				i=0;
				while(*ptr && *ptr!=CR && *ptr!=LF && i<511 && ptr<gurubuf+len) {
					answer[answers][i]=*ptr;
					ptr++;
					i++;
					/* multi-line answer */
					if(*ptr=='\\' && (*(ptr+1)==CR || *(ptr+1)==LF)) {
						ptr++;	/* skip \ */
						while(*ptr && *ptr<' ') ptr++;	/* skip [CR]LF */
						answer[answers][i++]=CR;
						answer[answers][i++]=LF; 
					} 
				}
				answer[answers][i]=0;
				if(!strlen(answer[answers]) || answer[answers][0]=='(') {
					ptr-=strlen(answer[answers]);
					break; 
				}
				while(*ptr && *ptr<' ') ptr++;	/* skip [CR]LF */
				answers++; 
			}
			if(answers==100)
				while(*ptr && *ptr!='(' && ptr<gurubuf+len)
					ptr++;
			i=0;
			/* Try to not repeat yourself */
			for(j=0;j<answers;j++) {
				i=sbbs_random(answers);
				if(stricmp(answer[i],last_answer))
					break;
			}
			strcpy(last_answer,answer[i]);
			for(j=0,k=0;answer[i][j];j++) {
				if(answer[i][j]=='`') {
					j++;
					theanswer[k]=0;
					switch(toupper(answer[i][j])) {
						case 'A':
							if(sys_status&SS_USERON) {
								SAFECAT(theanswer,useron.alias);
							} else {
								SAFECAT(theanswer,text[UNKNOWN_USER]);
							}
							break;
						case 'B':
							if(sys_status&SS_USERON) {
								getbirthdstr(&cfg, useron.birth, theanswer, sizeof(theanswer));
							} else {
								SAFECAT(theanswer,"00/00/00");
							}
							break;
						case 'C':
                    		if(sys_status&SS_USERON) {
								SAFECAT(theanswer,useron.comp);
							} else {
								SAFECAT(theanswer,"PC Jr.");
							}
							break;
						case 'D':
                    		if(sys_status&SS_USERON) {
								SAFECAT(theanswer,ultoac(useron.dlb,tmp));
							} else {
								SAFECAT(theanswer,"0");
							}
							break;
						case 'G':
							SAFECAT(theanswer,cfg.guru[gurunum]->name);
							break;
						case 'H':
							hu=1;
							break;
						case 'I':
							SAFECAT(theanswer,cfg.sys_id);
							break;
						case 'J':
							sprintf(tmp,"%u",tm.tm_mday);
							break;
						case 'L':
                    		if(sys_status&SS_USERON) {
								SAFECAT(theanswer,ultoa(useron.level,tmp,10));
							} else {
								SAFECAT(theanswer,"0");
							}
							break;
						case 'M':
							SAFECAT(theanswer,month[tm.tm_mon]);
							break;
						case 'N':   /* Note */
                    		if(sys_status&SS_USERON) {
								SAFECAT(theanswer,useron.note);
							} else {
								SAFECAT(theanswer,text[UNKNOWN_USER]);
							}
							break;
						case 'O':
							SAFECAT(theanswer,cfg.sys_op);
							break;
						case 'P':
                    		if(sys_status&SS_USERON) {
								SAFECAT(theanswer,useron.phone);
							} else {
								SAFECAT(theanswer,"000-000-0000");
							}
							break;
						case 'Q':
								sys_status&=~SS_GURUCHAT;
							break;
						case 'R':
                    		if(sys_status&SS_USERON) {
								SAFECAT(theanswer,useron.name);
							} else {
								SAFECAT(theanswer,text[UNKNOWN_USER]);
							}
							break;
						case 'S':
							SAFECAT(theanswer,cfg.sys_name);
							break;
						case 'T':
							sprintf(tmp,"%u:%02u",tm.tm_hour>12 ? tm.tm_hour-12
								: tm.tm_hour,tm.tm_min);
							SAFECAT(theanswer,tmp);
							break;
						case 'U':
                    		if(sys_status&SS_USERON) {
								SAFECAT(theanswer,ultoac(useron.ulb,tmp));
							} else {
								SAFECAT(theanswer,"0");
							}
							break;
						case 'W':
							SAFECAT(theanswer,weekday[tm.tm_wday]);
							break;
						case 'Y':   /* Current year */
							sprintf(tmp,"%u",1900+tm.tm_year);
							SAFECAT(theanswer,tmp);
							break;
						case 'Z':
							if(sys_status&SS_USERON) {
								SAFECAT(theanswer,useron.zipcode);
							} else {
								SAFECAT(theanswer,"90210");
							}
							break;
						case '$':   /* Credits */
                    		if(sys_status&SS_USERON) {
								SAFECAT(theanswer,ultoac(useron.cdt,tmp));
							} else {
								SAFECAT(theanswer,"0");
							}
							break;
						case '#':
                    		if(sys_status&SS_USERON) {
								SAFECAT(theanswer,ultoa(getage(&cfg,useron.birth)
									,tmp,10));
							} else {
								SAFECAT(theanswer,"0");
							}
							break;
						case '!':
							mistakes=!mistakes;
							break;
						case '_':
							mswait(500);
							break; 
					}
					k=strlen(theanswer); 
				}
				else
					theanswer[k++]=answer[i][j]; 
			}
			theanswer[k]=0;
			mswait(500+sbbs_random(1000));	 /* thinking time */
			if(action!=NODE_MCHT) {
				for(i=0;i<k;i++) {
					if(i && mistakes && theanswer[i]!=theanswer[i-1] &&
						((!IS_ALPHANUMERIC(theanswer[i]) && !sbbs_random(100))
						|| (IS_ALPHANUMERIC(theanswer[i]) && !sbbs_random(30)))) {
						c=j=((uint)sbbs_random(3)+1);	/* 1 to 3 chars */
						if(c<strcspn(theanswer+(i+1),"\0., "))
							c=j=1;
						while(j) {
							outchar(97+sbbs_random(26));
							mswait(25+sbbs_random(150));
							j--; 
						}
						if(sbbs_random(100)) {
							mswait(100+sbbs_random(300));
							while(c) {
								backspace();
								mswait(50+sbbs_random(50));
								c--; 
							} 
						} 
					}
					outchar(theanswer[i]);
					if(theanswer[i]==theanswer[i+1])
						mswait(25+sbbs_random(50));
					else
						mswait(25+sbbs_random(150));
					if(theanswer[i]==' ')
						mswait(sbbs_random(50)); 
				} 
			}
			else {
				mswait(strlen(theanswer)*100);
				bprintf(text[ChatLineFmt],cfg.guru[gurunum]->name
					,cfg.sys_nodes+1,':',theanswer); 
			}
			CRLF;
			sprintf(str,"%sguru.log",cfg.logs_dir);
			if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1)
				errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
			else {
				xpDateTime_to_isoDateTimeStr(xpDateTime_now(), "-", " ", ":", 0, str, sizeof(str)-3);
				SAFECAT(str,"\r\n");
				write(file,str,strlen(str));
				if(action==NODE_MCHT) {
					sprintf(str,"[Multi] ");
					write(file,str,strlen(str)); 
				}
				sprintf(str,"%s:\r\n",sys_status&SS_USERON
					? useron.alias : "UNKNOWN");
				write(file,str,strlen(str));
				write(file,line,strlen(line));
				if(action!=NODE_MCHT)
					write(file,crlf,2);
				sprintf(str,"%s:\r\n",cfg.guru[gurunum]->name);
				write(file,str,strlen(str));
				write(file,theanswer,strlen(theanswer));
				write(file,crlf,2);
				close(file); 
			}
			if(hu)
				hangup();
			break; 
		} 
	}
	for(i=0;i<100;i++)
		free(answer[i]);
}

/****************************************************************************/
/* An expression from the guru's buffer 'ptrptr' is evaluated and true or   */
/* false is returned.                                                       */
/****************************************************************************/
bool sbbs_t::guruexp(char **ptrptr, char *line)
{
	char	*cp,str[256];
	int		nest;
	unsigned c;
	bool	result=false,_and=false,_or=false;
	uchar	*ar;

	if((**ptrptr)==')') {	/* expressions of () are always result */
		(*ptrptr)++;
		return(true); 
	}
	while((**ptrptr)!=')' && (**ptrptr)) {
		if((**ptrptr)=='[') {
			(*ptrptr)++;
			SAFECOPY(str,*ptrptr);
			while(**ptrptr && (**ptrptr)!=']')
				(*ptrptr)++;
			(*ptrptr)++;
			cp=strchr(str,']');
			if(cp) *cp=0;
			ar=arstr(NULL,str,&cfg,NULL);
			c=chk_ar(ar,&useron,&client);
			free(ar);
			if(!c && _and) {
				result=false;
				break; 
			}
			if(c && _or) {
				result=true;
				break; 
			}
			if(c)
				result=true;
			continue; 
		}
		if((**ptrptr)=='(') {
			(*ptrptr)++;
			c=guruexp(&(*ptrptr),line);
			if(!c && _and) {
				result=false;
				break; 
			}
			if(c && _or) {
				result=true;
				break; 
			}
			if(c)
				result=true; 
		}
		if((**ptrptr)==')')
			break;
		c=0;
		while((**ptrptr) && IS_WHITESPACE(**ptrptr))
			(*ptrptr)++;
		while((**ptrptr)!='|' && (**ptrptr)!='&' && (**ptrptr)!=')' &&(**ptrptr)) {
			str[c++]=(**ptrptr);
			(*ptrptr)++; 
		}
		str[c]=0;
		if((**ptrptr)=='|') {
			if(!c && result)
				break;
			_and=false;
			_or=true; 
		}
		else if((**ptrptr)=='&') {
			if(!c && !result)
				break;
			_and=true;
			_or=false; 
		}
		if(!c) {				/* support ((exp)op(exp)) */
			(*ptrptr)++;
			continue; 
		}
		if((**ptrptr)!=')')
			(*ptrptr)++;
		c=0;					/* c now used for start line flag */
		if(str[strlen(str)-1]=='^') {	/* ^signifies start of line only */
			str[strlen(str)-1]=0;
			c=true; 
		}
		if(str[strlen(str)-1]=='~') {	/* ~signifies non-isolated word */
			str[strlen(str)-1]=0;
			cp=strstr(line,str);
			if(c && cp!=line)
				cp=0; 
		}
		else {
			cp=strstr(line,str);
			if(cp && c) {
				if(cp!=line || IS_ALPHANUMERIC(*(cp+strlen(str))))
					cp=0; 
			}
			else {	/* must be isolated word */
				while(cp)
					if((cp!=line && IS_ALPHANUMERIC(*(cp-1)))
						|| IS_ALPHANUMERIC(*(cp+strlen(str))))
						cp=strstr(cp+strlen(str),str);
					else
						break; 
			} 
		}
		if(!cp && _and) {
			result=false;
			break; 
		}
		if(cp && _or) {
			result=true;
			break; 
		}
		if(cp)
			result=true; 
	}
	nest=0;
	while((**ptrptr)!=')' && (**ptrptr)) {		/* handle nested exp */
		if((**ptrptr)=='(')						/* (TRUE|(IGNORE)) */
			nest++;
		(*ptrptr)++;
		while((**ptrptr)==')' && nest && (**ptrptr)) {
			nest--;
			(*ptrptr)++; 
		} 
	}
	(*ptrptr)++;	/* skip over ')' */
	return(result);
}

/****************************************************************************/
/* Guru chat with the appearance of Local chat with sysop.                  */
/****************************************************************************/
void sbbs_t::localguru(char *gurubuf, int gurunum)
{
	char	ch,str[256];
	int 	con=console;		 /* save console state */
	char	lastanswer[512];

	if(sys_status&SS_GURUCHAT || !cfg.total_gurus)
		return;
	sys_status|=SS_GURUCHAT;
	console&=~(CON_L_ECHOX|CON_R_ECHOX);	/* turn off X's */
	console|=(CON_L_ECHO|CON_R_ECHO);					/* make sure echo is on */
	if(action==NODE_CHAT) {	/* only page if from chat section */
		bprintf(text[PagingGuru],cfg.guru[gurunum]->name);
		ch=sbbs_random(25)+25;
		while(ch--) {
			mswait(200);
			outchar('.'); 
		} 
	}
	bprintf(text[SysopIsHere],cfg.guru[gurunum]->name);
	if(getnodedat(cfg.node_num,&thisnode,true)==0) {
		thisnode.aux=gurunum;
		putnodedat(cfg.node_num,&thisnode);
	}
	attr(cfg.color[clr_chatlocal]);
	strcpy(str,"HELLO");
	guruchat(str,gurubuf,gurunum,lastanswer);
	str[0]=0;
	while(online && (sys_status&SS_GURUCHAT)) {
		checkline();
		action=NODE_GCHT;
		SYNC;
		if(wordwrap[0]==0) {
			if((ch=inkey(K_NONE,1000))==0) {
				if(str[0]) {
					attr(cfg.color[clr_chatlocal]);
					guruchat(str,gurubuf,gurunum,lastanswer); 
					str[0]=0;
				}
				continue;
			}
			ungetkey(ch);
		}
		attr(cfg.color[clr_chatremote]);
		getstr(str,78,K_WRAP|K_CHAT);
	}
	bputs(text[EndOfChat]);
	sys_status&=~SS_GURUCHAT;
	console=con;				/* restore console state */
}


