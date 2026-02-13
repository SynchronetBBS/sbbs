/* Synchronet node information retrieval functions */

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

/****************************************************************************/
/* Reads the data for node number 'number' into the structure 'node'        */
/* from NODE.DAB															*/
/* if lockit is non-zero, locks this node's record. putnodedat() unlocks it */
/****************************************************************************/
bool sbbs_t::getnodedat(uint number, node_t *node, bool lockit)
{
	int  rd = sizeof(node_t);
	int  count;

	if (node == NULL || number < 1)
		return false;

	if (number > cfg.sys_nodes) {
		errormsg(WHERE, ERR_CHK, "node number", number);
		return false;
	}

	if (node != &thisnode)
		memset(node, 0, sizeof(node_t));
	pthread_mutex_lock(&nodefile_mutex);
	if (nodefile == -1) {
		if ((nodefile = opennodedat(&cfg)) == -1) {
			pthread_mutex_unlock(&nodefile_mutex);
			errormsg(WHERE, ERR_OPEN, "nodefile", number);
			return false;
		}
	}
#if 0 // This leads to high disk (or Samba/file server) utilization as we call getnodedat() a lot
	else
		utime(str, NULL);       /* NFS fix... utime() forces a cache refresh */
#endif
	for (count = 0; count < LOOP_NODEDAB; count++) {
		if (count > 0)
			FILE_RETRY_DELAY(count + 1, LOCK_RETRY_DELAY);
		if (lockit && lock(nodefile, nodedatoffset(number), sizeof(node_t)) != 0) {
			unlock(nodefile, nodedatoffset(number), sizeof(node_t));
			continue;
		}
		if (seeknodedat(nodefile, number)) {
			rd = read(nodefile, node, sizeof(node_t));
			if (rd != sizeof(node_t))
				unlock(nodefile, nodedatoffset(number), sizeof(node_t));
			if (rd == sizeof(node_t))
				break;
		}
	}
	if (!lockit && cfg.node_misc & NM_CLOSENODEDAB) {
		close(nodefile);
		nodefile = -1;
	}

	if (count == LOOP_NODEDAB) {
		if (nodefile != -1)
			close(nodefile);
		nodefile = -1;
		pthread_mutex_unlock(&nodefile_mutex);
		errormsg(WHERE, rd == sizeof(node_t) ? ERR_LOCK : ERR_READ, "node.dab", number + 1);
		return false;
	}
	pthread_mutex_unlock(&nodefile_mutex);
	if (count > (LOOP_NODEDAB / 2)) {
		llprintf(LOG_WARNING, "!!", "NODE.DAB (node %d) COLLISION - Count: %d"
		            , number + 1, count);
	}

	return true;
}

static int getpagingnode(scfg_t* cfg)
{
	for (int i = 1; i <= cfg->sys_nodes; i++) {
		node_t node;
		if (i == cfg->node_num)
			continue;
		if (getnodedat(cfg, i, &node, FALSE, NULL) == 0 && node.action == NODE_PAGE && node.aux == cfg->node_num)
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
	int  atr = curatr;

	if (nodesync_inside || !online)
		return;
	nodesync_inside = 1;

	if (thisnode.action != action) {
		if (getnodedat(cfg.node_num, &thisnode, true)) {
			thisnode.action = action;
			putnodedat(cfg.node_num, &thisnode);
		}
	}

	criterrs = thisnode.errors;

	if (sys_status & SS_USERON) {

		if (thisnode.status == NODE_WFC) {
			lprintf(LOG_ERR, "Node %d NODE STATUS FIXUP", cfg.node_num);
			if (getnodedat(cfg.node_num, &thisnode, true)) {
				thisnode.status = NODE_INUSE;
				thisnode.useron = useron.number;
				putnodedat(cfg.node_num, &thisnode);
			}
		}

		if (!dates_are_same(time(NULL), useron.reset)) { /* New day, clear "today" user vars */
			sys_status |= SS_DATE_CHANGED; // Note: this could happen multiple times in a single (very long) user session
			lprintf(LOG_DEBUG, "New day detected");
			resetdailyuserdat(&cfg, &useron, /* write: */ true);
		}
		if ((thisnode.misc & NODE_UDAT) && !useron_is_guest()) {
			if (getuseron(WHERE) && getnodedat(cfg.node_num, &thisnode, true)) {
				thisnode.misc &= ~NODE_UDAT;
				putnodedat(cfg.node_num, &thisnode);
			}
		}
		if (useron.exempt & FLAG('H'))
			console |= CON_NO_INACT;
		else
			console &= ~CON_NO_INACT;

		if (!(sys_status & SS_MOFF)) {
			if (thisnode.misc & NODE_MSGW)
				getsmsg(useron.number, clearline);  /* getsmsg clears MSGW flag */
			if (thisnode.misc & NODE_NMSG)
				getnmsg(clearline);                 /* getnmsg clears NMSG flag */
		}
	}

	exec_mod("sync", cfg.sync_mod);

	if (thisnode.misc & NODE_INTR) {
		bputs(text[NodeLocked]);
		logline(LOG_NOTICE, nulstr, "Interrupted");
		hangup();
		nodesync_inside = 0;
		return;
	}

	if (thisnode.misc & NODE_LCHAT) { // pulled into local chat with sysop
		term->saveline();
		privchat(true);
		term->restoreline();
	}

	if (thisnode.misc & NODE_FCHAT) { // forced into private chat
		int n = getpagingnode(&cfg);
		if (n) {
			uint save_action = action;
			term->saveline();
			privchat(true, n);
			action = save_action;
			term->restoreline();
		}
		if (getnodedat(cfg.node_num, &thisnode, true)) {
			thisnode.action = action;
			thisnode.misc &= ~NODE_FCHAT;
			putnodedat(cfg.node_num, &thisnode);
		}
	}

	if (sys_status & SS_USERON && memcmp(&nodesync_user, &useron, sizeof(user_t))) {
		getusrdirs();
		getusrsubs();
		memcpy(&nodesync_user, &useron, sizeof(nodesync_user));
	}

	if (sys_status & SS_USERON && online && (timeleft / 60) < (5 - timeleft_warn)
	    && !useron_is_sysop()) {
		timeleft_warn = 5 - (timeleft / 60);
		if (!(sys_status & SS_MOFF)) {
			attr(LIGHTGRAY);
			bprintf(text[OnlyXminutesLeft]
			        , ((ushort)timeleft / 60) + 1, (timeleft / 60) ? "s" : nulstr);
		}
	}

	attr(atr);  /* replace original attributes */
	nodesync_inside = 0;
}

/****************************************************************************/
/* Prints short messages waiting for this node, if any...                   */
/****************************************************************************/
bool sbbs_t::getnmsg(bool clearline)
{
	char str[MAX_PATH + 1], *buf;
	int  file;
	long length;

	if (getnodedat(cfg.node_num, &thisnode, true)) {
		thisnode.misc &= ~NODE_NMSG;          /* clear the NMSG flag */
		putnodedat(cfg.node_num, &thisnode);
	}

	SAFEPRINTF2(str, "%smsgs/n%3.3u.msg", cfg.data_dir, cfg.node_num);
	if (flength(str) < 1L)
		return true;
	if ((file = nopen(str, O_RDWR)) == -1) {
		/**
			errormsg(WHERE,ERR_OPEN,str,O_RDWR);
		**/
		return false;
	}
	length = (long)filelength(file);
	if (length <= 0) {
		close(file);
		return true;
	}
	if ((buf = (char *)malloc(length + 1)) == NULL) {
		close(file);
		errormsg(WHERE, ERR_ALLOC, str, length + 1);
		return false;
	}
	if (read(file, buf, length) != length) {
		close(file);
		free(buf);
		errormsg(WHERE, ERR_READ, str, length);
		return false;
	}
	int retval = chsize(file, 0L);
	close(file);
	buf[length] = 0;

	if (clearline)
		term->clearline();
	else
		term->cond_newline();
	putmsg(buf, P_NOATCODES);
	free(buf);

	return retval == 0;
}

/****************************************************************************/
/* 'ext' must be at least 128 bytes!                                        */
/****************************************************************************/
bool sbbs_t::getnodeext(uint number, char *ext)
{
	int  rd, count;

	if (number < 1 || number > cfg.sys_nodes) {
		errormsg(WHERE, ERR_CHK, "node number", number);
		return false;
	}

	if ((node_ext = opennodeext(&cfg)) == -1) {
		memset(ext, 0, 128);
		errormsg(WHERE, ERR_OPEN, "node.exb", O_RDONLY | O_DENYNONE);
		return false;
	}

	number--;   /* make zero based */
	for (count = 0; count < LOOP_NODEDAB; count++) {
		if (count > 0)
			FILE_RETRY_DELAY(count + 1, LOCK_RETRY_DELAY);
		if (lock(node_ext, (long)number * 128L, 128) != 0)
			continue;
		lseek(node_ext, (long)number * 128L, SEEK_SET);
		rd = read(node_ext, ext, 128);
		unlock(node_ext, (long)number * 128L, 128);
		if (rd == 128)
			break;
	}
	close(node_ext);
	node_ext = -1;


	if (count == LOOP_NODEDAB) {
		errormsg(WHERE, ERR_READ, "node.exb", number + 1);
		return false;
	}
	if (count > (LOOP_NODEDAB / 2)) {
		llprintf("!!", "NODE.EXB (node %d) COLLISION - Count: %d"
		            , number + 1, count);
	}

	return true;
}

/****************************************************************************/
/* Prints short messages waiting for 'usernumber', if any...                */
/* then truncates the file.                                                 */
/****************************************************************************/
bool sbbs_t::getsmsg(int usernumber, bool clearline)
{
	char * buf;
	node_t node;
	int    i;

	for (i = 1; i <= cfg.sys_nodes; i++) { /* clear msg waiting flag */
		if (!getnodedat(i, &node, false) || node.useron != usernumber)
			continue;
		if (getnodedat(i, &node, true)) {
			if (node.useron == usernumber
			    && (node.status == NODE_INUSE || node.status == NODE_QUIET)
			    && node.misc & NODE_MSGW)
				node.misc &= ~NODE_MSGW;
			putnodedat(i, &node);
		}
	}

	if ((buf = readsmsg(&cfg, usernumber)) == NULL)
		return false;
	getnodedat(cfg.node_num, &thisnode);
	if (clearline)
		term->clearline();
	else
		term->cond_newline();
	putmsg(buf, P_NOATCODES | P_AUTO_UTF8);
	free(buf);

	return true;
}

/****************************************************************************/
/* This function lists users that are online.                               */
/* If listself is true, it will list the current node.                      */
/****************************************************************************/
void sbbs_t::whos_online(bool listself)
{
	int    i, j;
	node_t node;

	if(exec_mod("who's online", cfg.whosonline_mod) == 0)
		return;

	term->newline();
	bputs(text[NodeLstHdr]);
	for (j = 0, i = 1; i <= cfg.sys_nodes && i <= cfg.sys_lastnode; i++) {
		getnodedat(i, &node);
		if (i == cfg.node_num) {
			if (listself)
				printnodedat(i, &node);
			continue;
		}
		if (node.status == NODE_INUSE || (useron_is_sysop() && node.status == NODE_QUIET)) {
			printnodedat(i, &node);
			if (!lastnodemsg)
				lastnodemsg = i;
			j++;
		}
	}
	if (!j)
		bputs(text[NoOtherActiveNodes]);
}

void sbbs_t::nodelist(void)
{
	node_t node;

	if (exec_mod("list nodes", cfg.nodelist_mod) == 0)
		return;

	term->newline();
	bputs(text[NodeLstHdr]);
	for (int i = 1; i <= cfg.sys_nodes && i <= cfg.sys_lastnode; i++) {
		getnodedat(i, &node);
		printnodedat(i, &node);
	}
}

static char* node_connection_desc(sbbs_t* sbbs, ushort conn, char* str)
{
	switch (conn) {
		case NODE_CONNECTION_LOCAL:
			return (char*)" Locally";   /* obsolete */
		case NODE_CONNECTION_TELNET:
			return sbbs->text[NodeConnectionTelnet];
		case NODE_CONNECTION_RLOGIN:
			return sbbs->text[NodeConnectionRLogin];
		case NODE_CONNECTION_SSH:
			return sbbs->text[NodeConnectionSSH];
		case NODE_CONNECTION_SFTP:
			return sbbs->text[NodeConnectionSFTP];
		case NODE_CONNECTION_RAW:
			return sbbs->text[NodeConnectionRaw];
		default:
			snprintf(str, 16, sbbs->text[NodeConnectionModem], conn);
			break;
	}

	return str;
}

/****************************************************************************/
/* Displays the information for node number 'number' contained in 'node'    */
/****************************************************************************/
void sbbs_t::printnodedat(uint number, node_t* node)
{
	int  i;
	char hour, mer[3];
	char tmp[512];

	attr(cfg.color[clr_nodenum]);
	bprintf("%3d  ", number);
	attr(cfg.color[clr_nodestatus]);
	switch (node->status) {
		case NODE_WFC:
			bputs(text[NodeStatusWaitingForCall]);
			break;
		case NODE_OFFLINE:
			bputs(text[NodeStatusOffline]);
			break;
		case NODE_NETTING:
			bputs("Networking");    /* obsolete */
			break;
		case NODE_LOGON:
			bputs(text[NodeStatusLogon]);
			bputs(node_connection_desc(this, node->connection, tmp));
			break;
		case NODE_LOGOUT:
			bprintf(text[NodeStatusLogout]
			        , (node->misc & NODE_ANON) && !useron_is_sysop() ? text[UNKNOWN_USER] : username(&cfg, node->useron, tmp));
			break;
		case NODE_EVENT_WAITING:
			bputs(text[NodeStatusEventWaiting]);
			break;
		case NODE_EVENT_LIMBO:
			bprintf(text[NodeStatusEventLimbo], node->aux);
			break;
		case NODE_EVENT_RUNNING:
			bputs(text[NodeStatusEventRunning]);
			break;
		case NODE_NEWUSER:
			bputs(text[NodeStatusNewUser]);
			bputs(node_connection_desc(this, node->connection, tmp));
			break;
		case NODE_QUIET:
			if (!useron_is_sysop()) {
				bputs(text[NodeStatusWaitingForCall]);
				break;
			}
		case NODE_INUSE:
			attr(cfg.color[clr_nodeuser]);
			if (node->misc & NODE_ANON && !useron_is_sysop())
				bputs(text[UNKNOWN_USER]);
			else
				bputs(username(&cfg, node->useron, tmp));
			attr(cfg.color[clr_nodestatus]);
			bputs(" ");
			bputs(node_activity(&cfg, node, tmp, sizeof tmp, number));
			attr(cfg.color[clr_nodestatus]);
			bputs(node_connection_desc(this, node->connection, tmp));
			if (node->action == NODE_DLNG) {
				if (cfg.sys_misc & SM_MILITARY) {
					hour = node->aux / 60;
					mer[0] = 0;
				}
				else if ((node->aux / 60) >= 12) {
					if (node->aux / 60 == 12)
						hour = 12;
					else
						hour = (node->aux / 60) - 12;
					strcpy(mer, "pm");
				}
				else {
					if ((node->aux / 60) == 0)    /* 12 midnite */
						hour = 12;
					else
						hour = node->aux / 60;
					strcpy(mer, "am");
				}
				bprintf(" ETA %02d:%02d %s"
				        , hour, node->aux % 60, mer);
			}
			break;
	}
	i = NODE_LOCK;
	if (node->status == NODE_INUSE || useron_is_sysop())
		i |= NODE_POFF | NODE_AOFF | NODE_MSGW | NODE_NMSG;
	if (node->misc & i) {
		bputs(" (");
		if (node->misc & (i & NODE_AOFF))
			outchar('A');
		if (node->misc & NODE_LOCK)
			outchar('L');
		if (node->misc & (i & (NODE_MSGW)))
			outchar('M');
		if (node->misc & (i & (NODE_NMSG)))
			outchar('N');
		if (node->misc & (i & NODE_POFF))
			outchar('P');
		outchar(')');
	}
	if (useron_is_sysop() && ((node->misc
	               & (NODE_ANON | NODE_UDAT | NODE_INTR | NODE_RRUN | NODE_EVENT | NODE_DOWN | NODE_LCHAT | NODE_FCHAT))
	              || node->status == NODE_QUIET)) {
		bputs(" [");
		if (node->misc & NODE_ANON)
			outchar('A');
		if (node->misc & NODE_INTR)
			outchar('I');
		if (node->misc & NODE_RRUN)
			outchar('R');
		if (node->misc & NODE_UDAT)
			outchar('U');
		if (node->misc & NODE_EVENT)
			outchar('E');
		if (node->misc & NODE_DOWN)
			outchar('D');
		if (node->misc & NODE_LCHAT)
			outchar('C');
		if (node->misc & NODE_FCHAT)
			outchar('F');
		if (node->status == NODE_QUIET)
			outchar('Q');
		outchar(']');
	}
	if (node->errors && useron_is_sysop()) {
		attr(cfg.color[clr_err]);
		bprintf(" %d error%c", node->errors, node->errors > 1 ? 's' : '\0' );
	}
	attr(LIGHTGRAY);
	term->newline();
}

uint sbbs_t::count_nodes(bool self)
{
	uint count = 0;

	for (int i = 1; i <= cfg.sys_nodes && i <= cfg.sys_lastnode; i++) {
		node_t node;
		if (!getnodedat(i, &node))
			continue;
		if (!self && i == cfg.node_num)
			continue;
		if (node.status != NODE_INUSE)
			continue;
		count++;
	}
	return count;
}
