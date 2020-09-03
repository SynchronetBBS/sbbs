/* $Id: scfgchat.c,v 1.23 2018/06/21 20:22:07 rswindell Exp $ */

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

#include "scfg.h"

void page_cfg()
{
	static int dflt,bar;
	char str[81],done=0;
	int j,k;
	uint i;
	uint u;
	static page_t savpage;

	while(1) {
		for(i=0;i<cfg.total_pages && i<MAX_OPTS;i++)
			sprintf(opt[i],"%-40.40s %-.20s",cfg.page[i]->cmd,cfg.page[i]->arstr);
		opt[i][0]=0;
		j=WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT;
		if(cfg.total_pages)
			j|=WIN_DEL|WIN_COPY|WIN_CUT;
		if(cfg.total_pages<MAX_OPTS)
			j|=WIN_INS|WIN_INSACT|WIN_XTR;
		if(savpage.cmd[0])
			j|=WIN_PASTE;
		uifc.helpbuf=
			"`External Sysop Chat Pagers:`\n"
			"\n"
			"This is a list of the configured external sysop chat pagers.\n"
			"\n"
			"To add a pager, select the desired location and hit ~ INS ~.\n"
			"\n"
			"To delete a pager, select it and hit ~ DEL ~.\n"
			"\n"
			"To configure a pager, select it and hit ~ ENTER ~.\n"
		;
		i=uifc.list(j,0,0,45,&dflt,&bar,"External Sysop Chat Pagers",opt);
		if((signed)i==-1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			sprintf(str,"%%!tone +chatpage.ton");
			uifc.helpbuf=
				"`External Chat Pager Command Line:`\n"
				"\n"
				"This is the command line to execute for this external chat pager.\n"
				SCFG_CMDLINE_SPEC_HELP
			;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Command Line",str,50
				,K_EDIT)<1)
				continue;
			if((cfg.page=(page_t **)realloc(cfg.page,sizeof(page_t *)*(cfg.total_pages+1)))
				==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_pages+1);
				cfg.total_pages=0;
				bail(1);
				continue; 
			}
			if(cfg.total_pages)
				for(u=cfg.total_pages;u>i;u--)
					cfg.page[u]=cfg.page[u-1];
			if((cfg.page[i]=(page_t *)malloc(sizeof(page_t)))==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(page_t));
				continue; 
			}
			memset((page_t *)cfg.page[i],0,sizeof(page_t));
			strcpy(cfg.page[i]->cmd,str);
			cfg.total_pages++;
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if(msk == MSK_CUT)
				savpage = *cfg.page[i];
			free(cfg.page[i]);
			cfg.total_pages--;
			for(j=i;j<cfg.total_pages;j++)
				cfg.page[j]=cfg.page[j+1];
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_COPY) {
			savpage=*cfg.page[i];
			continue; 
		}
		if (msk == MSK_PASTE) {
			*cfg.page[i]=savpage;
			uifc.changes=1;
			continue; 
		}
		if (msk != 0)
			continue;
		j=0;
		done=0;
		while(!done) {
			k=0;
			sprintf(opt[k++],"%-27.27s%.40s","Command Line",cfg.page[i]->cmd);
			sprintf(opt[k++],"%-27.27s%.40s","Access Requirements",cfg.page[i]->arstr);
			sprintf(opt[k++],"%-27.27s%s","Intercept I/O"
				,(cfg.page[i]->misc&XTRN_STDIO) ? "Standard"
					:cfg.page[i]->misc&XTRN_CONIO ? "Console":"No");
			sprintf(opt[k++],"%-27.27s%s","Native Executable"
				,cfg.page[i]->misc&XTRN_NATIVE ? "Yes" : "No");
			sprintf(opt[k++],"%-27.27s%s","Use Shell to Execute"
				,cfg.page[i]->misc&XTRN_SH ? "Yes" : "No");
			opt[k][0]=0;
			sprintf(str,"Sysop Chat Pager #%d",i+1);
			switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&j,0,str,opt)) {
				case -1:
					done=1;
					break;
				case 0:
					uifc.helpbuf=
						"`External Chat Pager Command Line:`\n"
						"\n"
						"This is the command line to execute for this external chat pager.\n"
						SCFG_CMDLINE_SPEC_HELP
					;
					strcpy(str,cfg.page[i]->cmd);
					if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Command Line"
						,cfg.page[i]->cmd,sizeof(cfg.page[i]->cmd)-1,K_EDIT))
						strcpy(cfg.page[i]->cmd,str);
					break;
				case 1:
					getar(str,cfg.page[i]->arstr);
					break;
				case 2:
					switch(cfg.page[i]->misc&(XTRN_STDIO|XTRN_CONIO)) {
						case XTRN_STDIO:
							k=0;
							break;
						case XTRN_CONIO:
							k=1;
							break;
						default:
							k=2;
					}
					strcpy(opt[0],"Standard");
					strcpy(opt[1],"Console");
					strcpy(opt[2],"No");
					opt[3][0]=0;
					uifc.helpbuf=
						"`Intercept I/O:`\n"
						"\n"
						"If you wish the screen output and keyboard input to be intercepted\n"
						"when running this chat pager, set this option to either `Standard` or ~Console~.\n"
					;
					switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Intercept I/O"
						,opt)) {
					case 0:
						if((cfg.page[i]->misc&(XTRN_STDIO|XTRN_CONIO)) != XTRN_STDIO) {
							cfg.page[i]->misc|=XTRN_STDIO;
							cfg.page[i]->misc&=~XTRN_CONIO;
							uifc.changes=1; 
						}
						break;
					case 1:
						if((cfg.page[i]->misc&(XTRN_STDIO|XTRN_CONIO)) != XTRN_CONIO) {
							cfg.page[i]->misc|=XTRN_CONIO;
							cfg.page[i]->misc&=~XTRN_STDIO;
							uifc.changes=1; 
						}
						break;
					case 2:
						if((cfg.page[i]->misc&(XTRN_STDIO|XTRN_CONIO)) != 0) {
							cfg.page[i]->misc&=~(XTRN_STDIO|XTRN_CONIO);
							uifc.changes=1; 
						}
						break;
					}
					break;
				case 3:
					k=(cfg.page[i]->misc&XTRN_NATIVE) ? 0:1;
					uifc.helpbuf=
						"`Native Executable:`\n"
						"\n"
						"If this online program is a native (e.g. non-DOS) executable,\n"
						"set this option to `Yes`.\n"
					;
					k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
						,"Native",uifcYesNoOpts);
					if(!k && !(cfg.page[i]->misc&XTRN_NATIVE)) {
						cfg.page[i]->misc|=XTRN_NATIVE;
						uifc.changes=TRUE; 
					}
					else if(k==1 && (cfg.page[i]->misc&XTRN_NATIVE)) {
						cfg.page[i]->misc&=~XTRN_NATIVE;
						uifc.changes=TRUE; 
					}
					break;
				case 4:
					k=(cfg.page[i]->misc&XTRN_SH) ? 0:1;
					uifc.helpbuf=
						"`Use Shell to Execute Command:`\n"
						"\n"
						"If this command-line requires the system command shell to execute, (Unix\n"
						"shell script or DOS batch file), set this option to ~Yes~.\n"
					;
					k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
						,"Use Shell",uifcYesNoOpts);
					if(!k && !(cfg.page[i]->misc&XTRN_SH)) {
						cfg.page[i]->misc|=XTRN_SH;
						uifc.changes=TRUE; 
					}
					else if(k==1 && (cfg.page[i]->misc&XTRN_SH)) {
						cfg.page[i]->misc&=~XTRN_SH;
						uifc.changes=TRUE; 
					}
					break;

			} 
		} 
	}
}

void chan_cfg()
{
	static int chan_dflt,chan_bar,opt_dflt;
	char str[128],code[128],done=0;
	int j,k;
	uint i,u;
	static chan_t savchan;

	while(1) {
		for(i=0;i<cfg.total_chans && i<MAX_OPTS;i++)
			sprintf(opt[i],"%-25s",cfg.chan[i]->name);
		opt[i][0]=0;
		j=WIN_ACT|WIN_SAV|WIN_BOT|WIN_RHT;
		if(cfg.total_chans)
			j|=WIN_DEL|WIN_COPY|WIN_CUT;
		if(cfg.total_chans<MAX_OPTS)
			j|=WIN_INS|WIN_INSACT|WIN_XTR;
		if(savchan.name[0])
			j|=WIN_PASTE;
		uifc.helpbuf=
			"`Multinode Chat Channels:`\n"
			"\n"
			"This is a list of the configured multinode chat channels.\n"
			"\n"
			"To add a channel, select the desired location with the arrow keys and\n"
			"hit ~ INS ~.\n"
			"\n"
			"To delete a channel, select it with the arrow keys and hit ~ DEL ~.\n"
			"\n"
			"To configure a channel, select it with the arrow keys and hit ~ ENTER ~.\n"
		;
		i=uifc.list(j,0,0,45,&chan_dflt,&chan_bar,"Multinode Chat Channels",opt);
		if((signed)i==-1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			strcpy(str,"Open");
			uifc.helpbuf=
				"`Channel Name:`\n"
				"\n"
				"This is the name or description of the chat channel.\n"
			;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Chat Channel Name",str,25
				,K_EDIT)<1)
				continue;
			SAFECOPY(code,str);
			prep_code(code,/* prefix: */NULL);
			uifc.helpbuf=
				"`Chat Channel Internal Code:`\n"
				"\n"
				"Every chat channel must have its own unique code for Synchronet to refer\n"
				"to it internally. This code is usually an abbreviation of the chat\n"
				"channel name.\n"
			;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Internal Code"
				,code,LEN_CODE,K_EDIT|K_UPPER)<1)
				continue;
			if(!code_ok(code)) {
				uifc.helpbuf=invalid_code;
				uifc.msg("Invalid Code");
				uifc.helpbuf=0;
				continue; 
			}
			if((cfg.chan=(chan_t **)realloc(cfg.chan,sizeof(chan_t *)*(cfg.total_chans+1)))
				==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_chans+1);
				cfg.total_chans=0;
				bail(1);
				continue; 
			}
			if(cfg.total_chans)
				for(u=cfg.total_chans;u>i;u--)
					cfg.chan[u]=cfg.chan[u-1];
			if((cfg.chan[i]=(chan_t *)malloc(sizeof(chan_t)))==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(chan_t));
				continue; 
			}
			memset((chan_t *)cfg.chan[i],0,sizeof(chan_t));
			strcpy(cfg.chan[i]->name,str);
			strcpy(cfg.chan[i]->code,code);
			cfg.total_chans++;
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if(msk == MSK_CUT)
				savchan = *cfg.chan[i];
			free(cfg.chan[i]);
			cfg.total_chans--;
			for(j=i;j<cfg.total_chans;j++)
				cfg.chan[j]=cfg.chan[j+1];
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_COPY) {
			savchan=*cfg.chan[i];
			continue; 
		}
		if (msk == MSK_PASTE) {
			*cfg.chan[i]=savchan;
			uifc.changes=1;
			continue; 
		}
		if (msk != 0)
			continue;
		j=0;
		done=0;
		while(!done) {
			k=0;
			sprintf(opt[k++],"%-27.27s%s","Name",cfg.chan[i]->name);
			sprintf(opt[k++],"%-27.27s%s","Internal Code",cfg.chan[i]->code);
			sprintf(opt[k++],"%-27.27s%"PRIu32,"Cost in Credits",cfg.chan[i]->cost);
			sprintf(opt[k++],"%-27.27s%.40s","Access Requirements"
				,cfg.chan[i]->arstr);
			sprintf(opt[k++],"%-27.27s%s","Password Protection"
				,cfg.chan[i]->misc&CHAN_PW ? "Yes" : "No");
			sprintf(opt[k++],"%-27.27s%s","Guru Joins When Empty"
				,cfg.chan[i]->misc&CHAN_GURU ? "Yes" : "No");
			sprintf(opt[k++],"%-27.27s%s","Channel Guru"
				,cfg.chan[i]->guru<cfg.total_gurus ? cfg.guru[cfg.chan[i]->guru]->name : "");
			sprintf(opt[k++],"%-27.27s%s","Channel Action Set"
				,cfg.actset[cfg.chan[i]->actset]->name);
			opt[k][0]=0;
			uifc.helpbuf=
				"`Chat Channel Configuration:`\n"
				"\n"
				"This menu is for configuring the selected chat channel.\n"
			;
			sprintf(str,"%s Chat Channel",cfg.chan[i]->name);
			switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&opt_dflt,0,str,opt)) {
				case -1:
					done=1;
					break;
				case 0:
					uifc.helpbuf=
						"`Chat Channel Name:`\n"
						"\n"
						"This is the name or description of the chat channel.\n"
					;
					strcpy(str,cfg.chan[i]->name);
					if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Chat Channel Name"
						,cfg.chan[i]->name,sizeof(cfg.chan[i]->name)-1,K_EDIT))
						strcpy(cfg.chan[i]->name,str);
					break;
				case 1:
					uifc.helpbuf=
						"`Chat Channel Internal Code:`\n"
						"\n"
						"Every chat channel must have its own unique code for Synchronet to refer\n"
						"to it internally. This code is usually an abbreviation of the chat\n"
						"channel name.\n"
					;
					strcpy(str,cfg.chan[i]->code);
					if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Internal Code"
						,str,LEN_CODE,K_UPPER|K_EDIT))
						break;
					if(code_ok(str))
						strcpy(cfg.chan[i]->code,str);
					else {
						uifc.helpbuf=invalid_code;
						uifc.msg("Invalid Code");
						uifc.helpbuf=0; 
					}
					break;
				case 2:
					ultoa(cfg.chan[i]->cost,str,10);
					uifc.helpbuf=
						"`Chat Channel Cost to Join:`\n"
						"\n"
						"If you want users to be charged credits to join this chat channel, set\n"
						"this value to the number of credits to charge. If you want this channel\n"
						"to be free, set this value to `0`.\n"
					;
					uifc.input(WIN_MID|WIN_SAV,0,0,"Cost to Join (in Credits)"
						,str,10,K_EDIT|K_NUMBER);
					cfg.chan[i]->cost=atol(str);
					break;
				case 3:
					sprintf(str,"%s Chat Channel",cfg.chan[i]->name);
					getar(str,cfg.chan[i]->arstr);
					break;
				case 4:
					k=1;
					uifc.helpbuf=
						"`Allow Channel to be Password Protected:`\n"
						"\n"
						"If you want to allow the first user to join this channel to password\n"
						"protect it, set this option to `Yes`.\n"
					;
					k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
						,"Allow Channel to be Password Protected"
						,uifcYesNoOpts);
					if(!k && !(cfg.chan[i]->misc&CHAN_PW)) {
						cfg.chan[i]->misc|=CHAN_PW;
						uifc.changes=1; 
					}
					else if(k==1 && cfg.chan[i]->misc&CHAN_PW) {
						cfg.chan[i]->misc&=~CHAN_PW;
						uifc.changes=1; 
					}
					break;
				case 5:
					k=1;
					uifc.helpbuf=
						"`Guru Joins This Channel When Empty:`\n"
						"\n"
						"If you want the system guru to join this chat channel when there is\n"
						"only one user, set this option to `Yes`.\n"
					;
					k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
						,"Guru Joins This Channel When Empty"
						,uifcYesNoOpts);
					if(!k && !(cfg.chan[i]->misc&CHAN_GURU)) {
						cfg.chan[i]->misc|=CHAN_GURU;
						uifc.changes=1; 
					}
					else if(k==1 && cfg.chan[i]->misc&CHAN_GURU) {
						cfg.chan[i]->misc&=~CHAN_GURU;
						uifc.changes=1; 
					}
					break;
				case 6:
	uifc.helpbuf=
		"`Channel Guru:`\n"
		"\n"
		"This is a list of available chat Gurus.  Select the one that you wish\n"
		"to have available in this channel.\n"
	;
					k=0;
					for(j=0;j<cfg.total_gurus && j<MAX_OPTS;j++)
						sprintf(opt[j],"%-25s",cfg.guru[j]->name);
					opt[j][0]=0;
					k=uifc.list(WIN_SAV|WIN_RHT,0,0,25,&j,0
						,"Available Chat Gurus",opt);
					if(k==-1)
						break;
					cfg.chan[i]->guru=k;
					break;
				case 7:
	uifc.helpbuf=
		"`Channel Action Set:`\n"
		"\n"
		"This is a list of available chat action sets.  Select the one that you\n"
		"wish to have available in this channel.\n"
	;
					k=0;
					for(j=0;j<cfg.total_actsets && j<MAX_OPTS;j++)
						sprintf(opt[j],"%-25s",cfg.actset[j]->name);
					opt[j][0]=0;
					k=uifc.list(WIN_SAV|WIN_RHT,0,0,25,&j,0
						,"Available Chat Action Sets",opt);
					if(k==-1)
						break;
					uifc.changes=1;
					cfg.chan[i]->actset=k;
					break; 
			} 
		} 
	}
}

void chatact_cfg(uint setnum)
{
	static int chatact_dflt,chatact_bar;
	char str[128],cmd[128],out[128];
	int j;
	uint i,n,chatnum[MAX_OPTS+1];
	static chatact_t savchatact;

	while(1) {
		for(i=0,j=0;i<cfg.total_chatacts && j<MAX_OPTS;i++)
			if(cfg.chatact[i]->actset==setnum) {
				sprintf(opt[j],"%-*.*s %s",LEN_CHATACTCMD,LEN_CHATACTCMD
					,cfg.chatact[i]->cmd,cfg.chatact[i]->out);
				chatnum[j++]=i; 
			}
		chatnum[j]=cfg.total_chatacts;
		opt[j][0]=0;
		i=WIN_ACT|WIN_SAV;
		if(j)
			i|=WIN_DEL|WIN_COPY|WIN_CUT;
		if(j<MAX_OPTS)
			i|=WIN_INS|WIN_INSACT|WIN_XTR;
		if(savchatact.cmd[0])
			i|=WIN_PASTE;
		uifc.helpbuf=
			"`Multinode Chat Actions:`\n"
			"\n"
			"This is a list of the configured multinode chat actions.  The users can\n"
			"use these actions in multinode chat by turning on action commands with\n"
			"the `/A` command in multinode chat.  Then if a line is typed which\n"
			"begins with a valid `action command` and has a user name, chat handle,\n"
			"or node number following, the output string will be displayed replacing\n"
			"the `%s` symbols with the sending user's name and the receiving user's\n"
			"name (in that order).\n"
			"\n"
			"To add an action, select the desired location with the arrow keys and\n"
			"hit ~ INS ~.\n"
			"\n"
			"To delete an action, select it with the arrow keys and hit ~ DEL ~.\n"
			"\n"
			"To configure an action, select it with the arrow keys and hit ~ ENTER ~.\n"
		;
		sprintf(str,"%s Chat Actions",cfg.actset[setnum]->name);
		i=uifc.list(i,0,0,70,&chatact_dflt,&chatact_bar,str,opt);
		if((signed)i==-1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf=
				"`Chat Action Command:`\n"
				"\n"
				"This is the command word (normally a verb) to trigger the action output.\n"
			;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Action Command",cmd,LEN_CHATACTCMD
				,K_UPPER)<1)
				continue;
			uifc.helpbuf=
				"`Chat Action Output String:`\n"
				"\n"
				"This is the output string displayed with this action output.\n"
			;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"",out,LEN_CHATACTOUT
				,K_MSG)<1)
				continue;
			if((cfg.chatact=(chatact_t **)realloc(cfg.chatact
				,sizeof(chatact_t *)*(cfg.total_chatacts+1)))==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_chatacts+1);
				cfg.total_chatacts=0;
				bail(1);
				continue; 
			}
			if(j)
				for(n=cfg.total_chatacts;n>chatnum[i];n--)
					cfg.chatact[n]=cfg.chatact[n-1];
			if((cfg.chatact[chatnum[i]]=(chatact_t *)malloc(sizeof(chatact_t)))==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(chatact_t));
				continue; 
			}
			memset((chatact_t *)cfg.chatact[chatnum[i]],0,sizeof(chatact_t));
			strcpy(cfg.chatact[chatnum[i]]->cmd,cmd);
			strcpy(cfg.chatact[chatnum[i]]->out,out);
			cfg.chatact[chatnum[i]]->actset=setnum;
			cfg.total_chatacts++;
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if(msk == MSK_CUT)
				savchatact = *cfg.chatact[chatnum[i]];
			free(cfg.chatact[chatnum[i]]);
			cfg.total_chatacts--;
			for(j=chatnum[i];j<cfg.total_chatacts && j<MAX_OPTS;j++)
				cfg.chatact[j]=cfg.chatact[j+1];
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_COPY) {
			savchatact=*cfg.chatact[chatnum[i]];
			continue; 
		}
		if (msk == MSK_PASTE) {
			*cfg.chatact[chatnum[i]]=savchatact;
			cfg.chatact[chatnum[i]]->actset=setnum;
			uifc.changes=1;
			continue; 
		}
		if (msk != 0)
			continue;
		uifc.helpbuf=
			"`Chat Action Command:`\n"
			"\n"
			"This is the command that triggers this chat action.\n"
		;
		strcpy(str,cfg.chatact[chatnum[i]]->cmd);
		if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Chat Action Command"
			,cfg.chatact[chatnum[i]]->cmd,LEN_CHATACTCMD,K_EDIT|K_UPPER)) {
			strcpy(cfg.chatact[chatnum[i]]->cmd,str);
			continue; 
		}
		uifc.helpbuf=
			"`Chat Action Output String:`\n"
			"\n"
			"This is the output string that results from this chat action.\n"
		;
		strcpy(str,cfg.chatact[chatnum[i]]->out);
		if(!uifc.input(WIN_MID|WIN_SAV,0,10,""
			,cfg.chatact[chatnum[i]]->out,LEN_CHATACTOUT,K_EDIT|K_MSG))
			strcpy(cfg.chatact[chatnum[i]]->out,str); 
	}
}

void guru_cfg()
{
	static int guru_dflt,guru_bar,opt_dflt;
	char str[128],code[128],done=0;
	int j,k;
	uint i,u;
	static guru_t savguru;

	while(1) {
		for(i=0;i<cfg.total_gurus && i<MAX_OPTS;i++)
			sprintf(opt[i],"%-25s",cfg.guru[i]->name);
		opt[i][0]=0;
		j=WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT;
		if(cfg.total_gurus)
			j|=WIN_DEL|WIN_COPY|WIN_CUT;
		if(cfg.total_gurus<MAX_OPTS)
			j|=WIN_INS|WIN_INSACT|WIN_XTR;
		if(savguru.name[0])
			j|=WIN_PASTE;
		uifc.helpbuf=
			"`Gurus:`\n"
			"\n"
			"This is a list of the configured Gurus.\n"
			"\n"
			"To add a Guru, select the desired location with the arrow keys and\n"
			"hit ~ INS ~.\n"
			"\n"
			"To delete a Guru, select it with the arrow keys and hit ~ DEL ~.\n"
			"\n"
			"To configure a Guru, select it with the arrow keys and hit ~ ENTER ~.\n"
		;
		i=uifc.list(j,0,0,45,&guru_dflt,&guru_bar,"Artificial Gurus",opt);
		if((signed)i==-1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf=
				"`Guru Name:`\n"
				"\n"
				"This is the name of the selected Guru.\n"
			;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Guru Name",str,25
				,0)<1)
				continue;
			SAFECOPY(code,str);
			prep_code(code,/* prefix: */NULL);
			uifc.helpbuf=
				"`Guru Internal Code:`\n"
				"\n"
				"Every Guru must have its own unique code for Synchronet to refer to\n"
				"it internally. This code is usually an abbreviation of the Guru name.\n"
			;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Internal Code"
				,code,LEN_CODE,K_EDIT|K_UPPER)<1)
				continue;
			if(!code_ok(code)) {
				uifc.helpbuf=invalid_code;
				uifc.msg("Invalid Code");
				uifc.helpbuf=0;
				continue; 
			}
			if((cfg.guru=(guru_t **)realloc(cfg.guru,sizeof(guru_t *)*(cfg.total_gurus+1)))
				==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_gurus+1);
				cfg.total_gurus=0;
				bail(1);
				continue; 
			}
			if(cfg.total_gurus)
				for(u=cfg.total_gurus;u>i;u--)
					cfg.guru[u]=cfg.guru[u-1];
			if((cfg.guru[i]=(guru_t *)malloc(sizeof(guru_t)))==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(guru_t));
				continue; 
			}
			memset((guru_t *)cfg.guru[i],0,sizeof(guru_t));
			strcpy(cfg.guru[i]->name,str);
			strcpy(cfg.guru[i]->code,code);
			cfg.total_gurus++;
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if(msk == MSK_CUT)
				savguru = *cfg.guru[i];
			free(cfg.guru[i]);
			cfg.total_gurus--;
			for(j=i;j<cfg.total_gurus;j++)
				cfg.guru[j]=cfg.guru[j+1];
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_COPY) {
			savguru=*cfg.guru[i];
			continue; 
		}
		if (msk == MSK_PASTE) {
			*cfg.guru[i]=savguru;
			uifc.changes=1;
			continue; 
		}
		if (msk != 0)
			continue;
		j=0;
		done=0;
		while(!done) {
			k=0;
			sprintf(opt[k++],"%-27.27s%s","Guru Name",cfg.guru[i]->name);
			sprintf(opt[k++],"%-27.27s%s","Guru Internal Code",cfg.guru[i]->code);
			sprintf(opt[k++],"%-27.27s%.40s","Access Requirements",cfg.guru[i]->arstr);
			opt[k][0]=0;
			uifc.helpbuf=
				"`Guru Configuration:`\n"
				"\n"
				"This menu is for configuring the selected Guru.\n"
			;
			switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&opt_dflt,0,cfg.guru[i]->name
				,opt)) {
				case -1:
					done=1;
					break;
				case 0:
					uifc.helpbuf=
						"`Guru Name:`\n"
						"\n"
						"This is the name of the selected Guru.\n"
					;
					strcpy(str,cfg.guru[i]->name);
					if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Guru Name"
						,cfg.guru[i]->name,sizeof(cfg.guru[i]->name)-1,K_EDIT))
						strcpy(cfg.guru[i]->name,str);
					break;
				case 1:
	uifc.helpbuf=
		"`Guru Internal Code:`\n"
		"\n"
		"Every Guru must have its own unique code for Synchronet to refer to\n"
		"it internally. This code is usually an abbreviation of the Guru name.\n"
	;
					strcpy(str,cfg.guru[i]->code);
					if(!uifc.input(WIN_MID|WIN_SAV,0,0,"Guru Internal Code"
						,str,LEN_CODE,K_EDIT|K_UPPER))
						break;
					if(code_ok(str))
						strcpy(cfg.guru[i]->code,str);
					else {
						uifc.helpbuf=invalid_code;
						uifc.msg("Invalid Code");
						uifc.helpbuf=0; 
					}
					break;
				case 2:
					getar(cfg.guru[i]->name,cfg.guru[i]->arstr);
					break; 
			} 
		} 
	}
}

void actsets_cfg()
{
    static int actset_dflt,actset_bar,opt_dflt;
    char str[81];
    int j,k,done;
    uint i,u;
    static actset_t savactset;

	while(1) {
		for(i=0;i<cfg.total_actsets && i<MAX_OPTS;i++)
			sprintf(opt[i],"%-25s",cfg.actset[i]->name);
		opt[i][0]=0;
		j=WIN_ACT|WIN_RHT|WIN_BOT|WIN_SAV;
		if(cfg.total_actsets)
			j|=WIN_DEL|WIN_COPY|WIN_CUT;
		if(cfg.total_actsets<MAX_OPTS)
			j|=WIN_INS|WIN_INSACT|WIN_XTR;
		if(savactset.name[0])
			j|=WIN_PASTE;
		uifc.helpbuf=
			"`Chat Action Sets:`\n"
			"\n"
			"This is a list of the configured action sets.\n"
			"\n"
			"To add an action set, select the desired location with the arrow keys\n"
			"and hit ~ INS ~.\n"
			"\n"
			"To delete an action set, select it with the arrow keys and hit ~ DEL ~.\n"
			"\n"
			"To configure an action set, select it with the arrow keys and hit\n"
			"~ ENTER ~.\n"
		;
		i=uifc.list(j,0,0,45,&actset_dflt,&actset_bar,"Chat Action Sets",opt);
		if((signed)i==-1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf=
				"`Chat Action Set Name:`\n"
				"\n"
				"This is the name of the selected chat action set.\n"
			;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Chat Action Set Name",str,25
				,0)<1)
				continue;
			if((cfg.actset=(actset_t **)realloc(cfg.actset,sizeof(actset_t *)*(cfg.total_actsets+1)))
				==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_actsets+1);
				cfg.total_actsets=0;
				bail(1);
				continue; 
			}
			if(cfg.total_actsets)
				for(u=cfg.total_actsets;u>i;u--)
					cfg.actset[u]=cfg.actset[u-1];
			if((cfg.actset[i]=(actset_t *)malloc(sizeof(actset_t)))==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(actset_t));
				continue; 
			}
			memset((actset_t *)cfg.actset[i],0,sizeof(actset_t));
			strcpy(cfg.actset[i]->name,str);
			cfg.total_actsets++;
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if(msk == MSK_CUT)
				savactset = *cfg.actset[i];
			free(cfg.actset[i]);
			cfg.total_actsets--;
			for(j=i;j<cfg.total_actsets;j++)
				cfg.actset[j]=cfg.actset[j+1];
			uifc.changes=1;
			continue; 
		}
		if (msk == MSK_COPY) {
			savactset=*cfg.actset[i];
			continue; 
		}
		if (msk == MSK_PASTE) {
			*cfg.actset[i]=savactset;
			uifc.changes=1;
			continue; 
		}
		if (msk != 0)
			continue;

		j=0;
		done=0;
		while(!done) {
			k=0;
			sprintf(opt[k++],"%-27.27s%s","Action Set Name",cfg.actset[i]->name);
			sprintf(opt[k++],"%-27.27s","Configure Chat Actions...");
			opt[k][0]=0;
			uifc.helpbuf=
				"`Chat Action Set Configuration:`\n"
				"\n"
				"This menu is for configuring the selected chat action set.\n"
			;
			sprintf(str,"%s Chat Action Set",cfg.actset[i]->name);
			switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&opt_dflt,0,str
				,opt)) {
				case -1:
					done=1;
					break;
				case 0:
					uifc.helpbuf=
						"`Chat Action Set Name:`\n"
						"\n"
						"This is the name of the selected action set.\n"
					;
					strcpy(str,cfg.actset[i]->name);
					if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Action Set Name"
						,cfg.actset[i]->name,sizeof(cfg.actset[i]->name)-1,K_EDIT))
						strcpy(cfg.actset[i]->name,str);
					break;
				case 1:
					chatact_cfg(i);
					break; 
			} 
		} 
	}
}

