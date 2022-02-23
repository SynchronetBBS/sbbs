/* getnode.cpp */

/* Synchronet node information retrieval functions */

/* $Id: getnode.cpp,v 1.56 2020/08/01 22:04:03 rswindell Exp $ */

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
#include "cmdshell.h"

/****************************************************************************/
/* Reads the data for node number 'number' into the structure 'node'        */
/* from NODE.DAB															*/
/* if lockit is non-zero, locks this node's record. putnodedat() unlocks it */
/****************************************************************************/
int sbbs_t::getnodedat(uint number, node_t *node, bool lockit)
{
	char	str[MAX_PATH+1];
	int		rd=sizeof(node_t);
	int		count;

	if(node==NULL || number<1)
		return(-1);

	if(number>cfg.sys_nodes) {
		errormsg(WHERE,ERR_CHK,"node number",number);
		return(-1); 
	}

	if(node!=&thisnode)
		memset(node,0,sizeof(node_t));
	SAFEPRINTF(str,"%snode.dab",cfg.ctrl_dir);
	pthread_mutex_lock(&nodefile_mutex);
	if(nodefile==-1) {
		if((nodefile=nopen(str,O_RDWR|O_DENYNONE))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_RDWR|O_DENYNONE);
			pthread_mutex_unlock(&nodefile_mutex);
			return(errno); 
		}
	}
	else
		utime(str,NULL);		/* NFS fix... utime() forces a cache refresh */

	number--;	/* make zero based */
	for(count=0;count<LOOP_NODEDAB;count++) {
		if(count)
			mswait(100);
		if(lockit && lock(nodefile,(long)number*sizeof(node_t),sizeof(node_t))!=0) {
			unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
			continue;
		}
		lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
		rd=read(nodefile,node,sizeof(node_t));
		if(rd!=sizeof(node_t))
			unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
		if(rd==sizeof(node_t))
			break;
	}
	if(!lockit && cfg.node_misc&NM_CLOSENODEDAB) {
		close(nodefile);
		nodefile=-1;
	}

	if(count==LOOP_NODEDAB) {
		errormsg(WHERE,rd==sizeof(node_t) ? ERR_LOCK : ERR_READ,"node.dab",number+1);
		if(nodefile!=-1)
			close(nodefile);
		nodefile=-1;
		pthread_mutex_unlock(&nodefile_mutex);
		return(-2);
	}
	pthread_mutex_unlock(&nodefile_mutex);
	if(count>(LOOP_NODEDAB/2)) {
		SAFEPRINTF2(str,"NODE.DAB (node %d) COLLISION - Count: %d"
			,number+1, count);
		logline(LOG_WARNING,"!!",str); 
	}

	return(0);
}

static int getpagingnode(scfg_t* cfg)
{
	for(int i = 1; i <= cfg->sys_nodes; i++) {
		node_t node;
		if(i == cfg->node_num)
			continue;
		if(getnodedat(cfg, i, &node, FALSE, NULL) == 0 && node.action == NODE_PAGE && node.aux == cfg->node_num)
			return i;
	}
	return 0;
}

/****************************************************************************/
/* Synchronizes all the nodes knowledge of the other nodes' actions, mode,  */
/* status and other flags.                                                  */
/* Assumes that getnodedat(node_num,&thisnode,0) was called right before it */
/* is called.  																*/
/****************************************************************************/
void sbbs_t::nodesync(bool clearline)
{
	char	str[256],today[32];
	int		atr=curatr;

	if(nodesync_inside || !online) 
		return;
	nodesync_inside=1;

	if(thisnode.action!=action) {
		if(getnodedat(cfg.node_num,&thisnode,true)==0) {
			thisnode.action=action;
			putnodedat(cfg.node_num,&thisnode); 
		}
	}

	criterrs=thisnode.errors;

	if(sys_status&SS_USERON) {

		if(thisnode.status==NODE_WFC) {
			lprintf(LOG_ERR, "Node %d NODE STATUS FIXUP", cfg.node_num);
			if(getnodedat(cfg.node_num,&thisnode,true)==0) {
				thisnode.status=NODE_INUSE;
				thisnode.useron=useron.number;
				putnodedat(cfg.node_num,&thisnode); 
			}
		}

		if(!(sys_status&SS_NEWDAY)) {
			now=time(NULL);
			unixtodstr(&cfg,(time32_t)logontime,str);
			unixtodstr(&cfg,(time32_t)now,today);
			if(strcmp(str,today)) { /* New day, clear "today" user vars */
				sys_status|=SS_NEWDAY;	// So we don't keep doing this over&over
				resetdailyuserdat(&cfg, &useron,/* write: */true);
			} 
		}
		if(thisnode.misc&NODE_UDAT && !(useron.rest&FLAG('G'))) {   /* not guest */
			getuserdat(&cfg, &useron);
			if(getnodedat(cfg.node_num,&thisnode,true)==0) {
				thisnode.misc&=~NODE_UDAT;
				putnodedat(cfg.node_num,&thisnode); 
			}
		}
		if(!(sys_status&SS_MOFF)) {
			if(thisnode.misc&NODE_MSGW)
				getsmsg(useron.number, clearline); 	/* getsmsg clears MSGW flag */
			if(thisnode.misc&NODE_NMSG)
				getnmsg(clearline);					/* getnmsg clears NMSG flag */
		}
	}

	if(cfg.sync_mod[0])
		exec_bin(cfg.sync_mod,&main_csi);

	if(thisnode.misc&NODE_INTR) {
		bputs(text[NodeLocked]);
		logline(LOG_NOTICE,nulstr,"Interrupted");
		hangup();
		nodesync_inside=0;
		return; 
	}

	if(thisnode.misc&NODE_LCHAT) { // pulled into local chat with sysop
		SAVELINE;
		privchat(true);
		RESTORELINE;
	}

	if(thisnode.misc&NODE_FCHAT) { // forced into private chat
		int n = getpagingnode(&cfg);
		if(n) {
			uint save_action = action;
			SAVELINE;
			privchat(true, n);
			action = save_action;
			RESTORELINE;
		}
		if(getnodedat(cfg.node_num, &thisnode, true)==0) {
			thisnode.action = action;
			thisnode.misc &= ~NODE_FCHAT;
			putnodedat(cfg.node_num, &thisnode); 
		}
	}
		
	if(sys_status&SS_USERON && memcmp(&nodesync_user,&useron,sizeof(user_t))) {
		getusrdirs();
		getusrsubs();
		memcpy(&nodesync_user,&useron,sizeof(nodesync_user)); 
	}

	if(sys_status&SS_USERON && online && (timeleft/60)<(5-timeleft_warn)
		&& !SYSOP) {
		timeleft_warn=5-(timeleft/60);
		if(!(sys_status&SS_MOFF)) {
			attr(LIGHTGRAY);
			bprintf(text[OnlyXminutesLeft]
				,((ushort)timeleft/60)+1,(timeleft/60) ? "s" : nulstr); 
		}
	}

	attr(atr);	/* replace original attributes */
	nodesync_inside=0;
}

/****************************************************************************/
/* Prints short messages waiting for this node, if any...                   */
/****************************************************************************/
int sbbs_t::getnmsg(bool clearline)
{
	char	str[MAX_PATH+1], *buf;
	int		file;
	long	length;

	if(getnodedat(cfg.node_num,&thisnode,true)==0) {
		thisnode.misc&=~NODE_NMSG;          /* clear the NMSG flag */
		putnodedat(cfg.node_num,&thisnode);
	}

	SAFEPRINTF2(str,"%smsgs/n%3.3u.msg",cfg.data_dir,cfg.node_num);
	if(flength(str)<1L)
		return(0);
	if((file=nopen(str,O_RDWR))==-1) {
		/**
			errormsg(WHERE,ERR_OPEN,str,O_RDWR);
		**/
		return(errno); 
	}
	length=(long)filelength(file);
	if(!length) {
		close(file);
		return(0); 
	}
	if((buf=(char *)malloc(length+1))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,length+1);
		return(-1); 
	}
	if(lread(file,buf,length)!=length) {
		close(file);
		free(buf);
		errormsg(WHERE,ERR_READ,str,length);
		return(errno); 
	}
	chsize(file,0L);
	close(file);
	buf[length]=0;

	if(clearline)
		this->clearline();
	else if(column)
		CRLF; 
	putmsg(buf,P_NOATCODES);
	free(buf);

	return(0);
}

/****************************************************************************/
/* 'ext' must be at least 128 bytes!                                        */
/****************************************************************************/
int sbbs_t::getnodeext(uint number, char *ext)
{
    char	str[MAX_PATH+1];
    int		rd,count;

	if(!number || number>cfg.sys_nodes) {
		errormsg(WHERE,ERR_CHK,"node number",number);
		return(-1); 
	}

	SAFEPRINTF(str,"%snode.exb",cfg.ctrl_dir);
	if((node_ext=nopen(str,O_RDONLY|O_DENYNONE))==-1) {
		memset(ext,0,128);
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY|O_DENYNONE);
		return(errno); 
	}

	number--;   /* make zero based */
	for(count=0;count<LOOP_NODEDAB;count++) {
		if(count)
			mswait(100);
		if(lock(node_ext,(long)number*128L,128)!=0) 
			continue; 
		lseek(node_ext,(long)number*128L,SEEK_SET);
		rd=read(node_ext,ext,128);
		unlock(node_ext,(long)number*128L,128);
		if(rd==128)
			break;
	}
	close(node_ext);
	node_ext=-1;


	if(count==LOOP_NODEDAB) {
		errormsg(WHERE,ERR_READ,"node.exb",number+1);
		return(-2);
	}
	if(count>(LOOP_NODEDAB/2)) {
		SAFEPRINTF2(str,"NODE.EXB (node %d) COLLISION - Count: %d"
			,number+1, count);
		logline("!!",str); 
	}

	return(0);
}


/****************************************************************************/
/* Prints short messages waiting for 'usernumber', if any...                */
/* then truncates the file.                                                 */
/****************************************************************************/
int sbbs_t::getsmsg(int usernumber, bool clearline)
{
	char	*buf;
	node_t	node;
	int		i;

	for(i=1;i<=cfg.sys_nodes;i++) {	/* clear msg waiting flag */
		if(getnodedat(i,&node,false) != 0 || node.useron != usernumber)
			continue;
		if(getnodedat(i,&node,true)==0) {
			if(node.useron==usernumber
					&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
					&& node.misc&NODE_MSGW)
				node.misc&=~NODE_MSGW;
			putnodedat(i,&node); 
		} 
	}

	if((buf = readsmsg(&cfg, usernumber)) == NULL)
		return -1;
	getnodedat(cfg.node_num,&thisnode,0);
	if(clearline)
		this->clearline();
	else
		if(column)
			CRLF;
	putmsg(buf,P_NOATCODES);
	free(buf);

	return(0);
}

/****************************************************************************/
/* This function lists users that are online.                               */
/* If listself is true, it will list the current node.                      */
/* Returns number of active nodes (not including current node).             */
/****************************************************************************/
int sbbs_t::whos_online(bool listself)
{
    int		i,j;
    node_t	node;

	if(cfg.whosonline_mod[0] != '\0') {
		return exec_bin(cfg.whosonline_mod, &main_csi);
	}

	CRLF;
	bputs(text[NodeLstHdr]);
	for(j=0,i=1;i<=cfg.sys_nodes && i<=cfg.sys_lastnode;i++) {
		getnodedat(i,&node,false);
		if(i==cfg.node_num) {
			if(listself)
				printnodedat(i,&node);
			continue; 
		}
		if(node.status==NODE_INUSE || (SYSOP && node.status==NODE_QUIET)) {
			printnodedat(i,&node);
			if(!lastnodemsg)
				lastnodemsg=i;
			j++; 
		} 
	}
	if(!j)
		bputs(text[NoOtherActiveNodes]);
	return(j);
}

void sbbs_t::nodelist(void)
{
	node_t	node;

	if(cfg.nodelist_mod[0] != '\0') {
		exec_bin(cfg.nodelist_mod, &main_csi);
		return;
	}

	CRLF;
	bputs(text[NodeLstHdr]);
	for(int i=1;i<=cfg.sys_nodes && i<=cfg.sys_lastnode;i++) {
		getnodedat(i,&node,false);
		printnodedat(i,&node); 
	}
}

static char* node_connection_desc(sbbs_t* sbbs, ushort conn, char* str)
{
	switch(conn) {
		case NODE_CONNECTION_LOCAL:
			return (char*)" Locally";	/* obsolete */
		case NODE_CONNECTION_TELNET:
			return sbbs->text[NodeConnectionTelnet];
		case NODE_CONNECTION_RLOGIN:
			return sbbs->text[NodeConnectionRLogin];
		case NODE_CONNECTION_SSH:
			return sbbs->text[NodeConnectionSSH];
		case NODE_CONNECTION_RAW:
			return sbbs->text[NodeConnectionRaw];
		default:
			sprintf(str,sbbs->text[NodeConnectionModem],conn);
			break;
	}

	return str;
}

/****************************************************************************/
/* Displays the information for node number 'number' contained in 'node'    */
/****************************************************************************/
void sbbs_t::printnodedat(uint number, node_t* node)
{
    uint	i;
    char	hour,mer[3];
	char 	tmp[512];

	attr(cfg.color[clr_nodenum]);
	bprintf("%3d  ",number);
	attr(cfg.color[clr_nodestatus]);
	switch(node->status) {
		case NODE_WFC:
			bputs(text[NodeStatusWaitingForCall]);
			break;
		case NODE_OFFLINE:
			bputs(text[NodeStatusOffline]);
			break;
		case NODE_NETTING:
			bputs("Networking");	/* obsolete */
			break;
		case NODE_LOGON:
			bputs(text[NodeStatusLogon]);
			bputs(node_connection_desc(this, node->connection, tmp));
			break;
		case NODE_LOGOUT:
			bprintf(text[NodeStatusLogout]
				,(node->misc&NODE_ANON) && !SYSOP ? text[UNKNOWN_USER] : username(&cfg,node->useron,tmp));
			break;
		case NODE_EVENT_WAITING:
			bputs(text[NodeStatusEventWaiting]);
			break;
		case NODE_EVENT_LIMBO:
			bprintf(text[NodeStatusEventLimbo],node->aux);
			break;
		case NODE_EVENT_RUNNING:
			bputs(text[NodeStatusEventRunning]);
			break;
		case NODE_NEWUSER:
			bputs(text[NodeStatusNewUser]);
			bputs(node_connection_desc(this, node->connection, tmp));
			break;
		case NODE_QUIET:
			if(!SYSOP) {
				bputs(text[NodeStatusWaitingForCall]);
				break; 
			}
		case NODE_INUSE:
			if(node->misc&NODE_EXT) {
				getnodeext(number,tmp);
				bputs(tmp);
				break; 
			}
			attr(cfg.color[clr_nodeuser]);
			if(node->misc&NODE_ANON && !SYSOP)
				bputs(text[UNKNOWN_USER]);
			else
				bputs(username(&cfg,node->useron,tmp));
			attr(cfg.color[clr_nodestatus]);
			bputs(" ");
			switch(node->action) {
				case NODE_MAIN:
					bputs("at main menu");
					break;
				case NODE_RMSG:
					bputs("reading messages");
					break;
				case NODE_RMAL:
					bputs("reading mail");
					break;
				case NODE_RSML:
					bputs("reading sent mail");
					break;
				case NODE_RTXT:
					bputs("reading text files");
					break;
				case NODE_PMSG:
					bputs("posting message");
					break;
				case NODE_SMAL:
					bputs("sending mail");
					break;
				case NODE_AMSG:
					bputs("posting auto-message");
					break;
				case NODE_XTRN:
					if(node->aux<1 || node->aux>cfg.total_xtrns)
						bputs("at external program menu");
					else {
						bputs("running ");
						i=node->aux-1;
						if(SYSOP || chk_ar(cfg.xtrn[i]->ar,&useron,&client))
							bputs(cfg.xtrn[node->aux-1]->name);
						else
							bputs("external program"); 
					}
					break;
				case NODE_DFLT:
					bputs("changing defaults");
					break;
				case NODE_XFER:
					bputs("at transfer menu");
					break;
				case NODE_RFSD:
					bprintf("retrieving from device #%d",node->aux);
					break;
				case NODE_DLNG:
					bprintf("downloading");
					break;
				case NODE_ULNG:
					bputs("uploading");
					break;
				case NODE_BXFR:
					bputs("transferring bidirectional");
					break;
				case NODE_LFIL:
					bputs("listing files");
					break;
				case NODE_LOGN:
					bputs("logging on");
					break;
				case NODE_LCHT:
					bprintf("in local chat with %s",cfg.sys_op);
					break;
				case NODE_MCHT:
					if(node->aux) {
						bprintf("in multinode chat channel %d",node->aux&0xff);
						if(node->aux&0x1f00) { /* password */
							outchar('*');
							if(SYSOP)
								bprintf(" %s",unpackchatpass(tmp,node)); 
						} 
					}
					else
						bputs("in multinode global chat channel");
					break;
				case NODE_PAGE:
					bprintf("paging node %u for private chat",node->aux);
					break;
				case NODE_PCHT:
					if(node->aux)
						bprintf("in private chat with node %u",node->aux);
					else
						bprintf("in local chat with %s",cfg.sys_op);
					break;
				case NODE_GCHT:
					i=node->aux;
					if(i>=cfg.total_gurus)
						i=0;
					bprintf("chatting with %s",cfg.guru[i]->name);
					break;
				case NODE_CHAT:
					bputs("in chat section");
					break;
				case NODE_TQWK:
					bputs("transferring QWK packet");
					break;
				case NODE_SYSP:
					bputs("performing sysop activities");
					break;
				default:
					bputs(ultoa(node->action,tmp,10));
					break;  }
			bputs(node_connection_desc(this, node->connection, tmp));
			if(node->action==NODE_DLNG) {
				if(cfg.sys_misc&SM_MILITARY) {
					hour=node->aux/60;
					mer[0]=0; 
				}
				else if((node->aux/60)>=12) {
					if(node->aux/60==12)
						hour=12;
					else
						hour=(node->aux/60)-12;
					strcpy(mer,"pm"); 
				}
				else {
					if((node->aux/60)==0)    /* 12 midnite */
						hour=12;
					else hour=node->aux/60;
					strcpy(mer,"am"); 
				}
				bprintf(" ETA %02d:%02d %s"
					,hour,node->aux%60,mer); 
			}
			break; 
	}
	i=NODE_LOCK;
	if(node->status==NODE_INUSE || SYSOP)
		i|=NODE_POFF|NODE_AOFF|NODE_MSGW|NODE_NMSG;
	if(node->misc&i) {
		bputs(" (");
		if(node->misc&(i&NODE_AOFF))
			outchar('A');
		if(node->misc&NODE_LOCK)
			outchar('L');
		if(node->misc&(i&(NODE_MSGW)))
			outchar('M');
		if(node->misc&(i&(NODE_NMSG)))
			outchar('N');
		if(node->misc&(i&NODE_POFF))
			outchar('P');
		outchar(')'); 
	}
	if(SYSOP && ((node->misc
		&(NODE_ANON|NODE_UDAT|NODE_INTR|NODE_RRUN|NODE_EVENT|NODE_DOWN|NODE_LCHAT|NODE_FCHAT))
		|| node->status==NODE_QUIET)) {
		bputs(" [");
		if(node->misc&NODE_ANON)
			outchar('A');
		if(node->misc&NODE_INTR)
			outchar('I');
		if(node->misc&NODE_RRUN)
			outchar('R');
		if(node->misc&NODE_UDAT)
			outchar('U');
		if(node->misc&NODE_EVENT)
			outchar('E');
		if(node->misc&NODE_DOWN)
			outchar('D');
		if(node->misc&NODE_LCHAT)
			outchar('C');
		if(node->misc&NODE_FCHAT)
			outchar('F');
		if(node->status==NODE_QUIET)
			outchar('Q');
		outchar(']'); 
	}
	if(node->errors && SYSOP) {
		attr(cfg.color[clr_err]);
		bprintf(" %d error%c",node->errors, node->errors>1 ? 's' : '\0' ); 
	}
	attr(LIGHTGRAY);
	CRLF;
}

uint sbbs_t::count_nodes(bool self)
{
	uint count = 0;

	for(int i=1; i<=cfg.sys_nodes && i<=cfg.sys_lastnode; i++) {
	    node_t	node;
		if(getnodedat(i, &node, false) != 0)
			continue;
		if(!self && i==cfg.node_num)
			continue;
		if(node.status != NODE_INUSE)
			continue;
		count++;
	}
	return count;
}
