/* Synchronet node information writing routines */

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

/****************************************************************************/
/* Write the data from the structure 'node' into node.dab  					*/
/* getnodedat(num,&node,1); must have been called before calling this func  */
/*          NOTE: ------^   the indicates the node record has been locked   */
/****************************************************************************/
bool sbbs_t::putnodedat(uint number, node_t* node)
{
	char tmp[128];
	char path[MAX_PATH + 1];
	int  wr = 0;
	int  wrerr = 0;
	int  attempts;
	bool result = true;

	if (number < 1 || number > cfg.sys_nodes) {
		errormsg(WHERE, ERR_CHK, "node number", number);
		return false;
	}
	if (number == cfg.node_num) {
		if ((node->status == NODE_INUSE || node->status == NODE_QUIET)
		    && node->action < NODE_LAST_ACTION
		    && text[NodeActionMainMenu + node->action][0]) {
			node->misc |= NODE_EXT;
			putnodeext(number, expand_atcodes(text[NodeActionMainMenu + node->action], tmp, sizeof tmp));
		}
		else
			node->misc &= ~NODE_EXT;
	}

	snprintf(path, sizeof path, "%snode.dab", cfg.ctrl_dir);
	pthread_mutex_lock(&nodefile_mutex);
	if (nodefile == -1) {
		if ((nodefile = nopen(path, O_CREAT | O_RDWR | O_DENYNONE)) == -1) {
			pthread_mutex_unlock(&nodefile_mutex);
			errormsg(WHERE, ERR_OPEN, path, O_CREAT | O_RDWR | O_DENYNONE);
			return false;
		}
	}

	for (attempts = 0; attempts < 10; attempts++) {
		if (seeknodedat(nodefile, number)) {
			wr = write(nodefile, node, sizeof(node_t));
			if (wr == sizeof(node_t))
				break;
			wrerr = errno;    /* save write error */
		}
		FILE_RETRY_DELAY(attempts + 1, LOCK_RETRY_DELAY);
	}
	unlocknodedat(number);

	if (mqtt->connected) {
		int result = mqtt_putnodedat(mqtt, number, node);
		if (result != MQTT_SUCCESS)
			lprintf(LOG_WARNING, "ERROR %d (%d) publishing node status", result, errno);
	}
	if (wr != sizeof(node_t)) {
		errno = wrerr;
		errormsg(WHERE, ERR_WRITE, "nodefile", number);
		return false;
	}

	if (utime(path, NULL) != 0)   /* Update mod time for NFS/smbfs compatibility */
		return false;

	// Write to node#/status.ini
	if (number == cfg.node_num) {
		str_list_t ini = strListInit();
		iniSetUInteger(&ini, ROOT_SECTION, "status", node->status, nullptr);
		iniSetUInteger(&ini, ROOT_SECTION, "errors", node->errors, nullptr);
		iniSetUInteger(&ini, ROOT_SECTION, "action", node->action, nullptr);
		iniSetUInteger(&ini, ROOT_SECTION, "useron", node->useron, nullptr);
		iniSetHexInt(&ini, ROOT_SECTION, "conn", node->connection, nullptr);
		iniSetHexInt(&ini, ROOT_SECTION, "misc", node->misc, nullptr);
		iniSetUInteger(&ini, ROOT_SECTION, "aux", node->aux, nullptr);
		iniSetUInteger(&ini, ROOT_SECTION, "extaux", node->extaux, nullptr);
		if ((node->status == NODE_INUSE || node->status == NODE_QUIET)
			&& node->action == NODE_XTRN && xtrnnum_is_valid(&cfg, node->aux - 1))
			iniSetString(&ini, ROOT_SECTION, "xtrn", cfg.xtrn[node->aux - 1]->code, nullptr);

		snprintf(path, sizeof path, "%sstatus.ini", cfg.node_path[number - 1]);
		FILE* fp = iniOpenFile(path, /* modify */ true);
		if (fp == NULL)
			result = false;
		else {
			result = iniWriteFile(fp, ini);
			iniCloseFile(fp);
		}
		strListFree(&ini);
	}

	return result;
}

bool sbbs_t::unlocknodedat(uint number)
{
	if (number < 1 || number > cfg.sys_nodes) {
		pthread_mutex_unlock(&nodefile_mutex);
		errormsg(WHERE, ERR_CHK, "node number", number);
		return false;
	}
	int result = unlock(nodefile, (number - 1) * sizeof(node_t), sizeof(node_t));
	if (cfg.node_misc & NM_CLOSENODEDAB) {
		close(nodefile);
		nodefile = -1;
	}
	pthread_mutex_unlock(&nodefile_mutex);
	return result == 0;
}

bool sbbs_t::putnodeext(uint number, char *ext)
{
	int  count;
	int  wr;

	if (number < 1 || number > cfg.sys_nodes) {
		errormsg(WHERE, ERR_CHK, "node number", number);
		return false;
	}
	number--;   /* make zero based */

	if ((node_ext = opennodeext(&cfg)) == -1) {
		errormsg(WHERE, ERR_OPEN, "node.exb", O_CREAT | O_RDWR | O_DENYNONE);
		return false;
	}
	for (count = 0; count < LOOP_NODEDAB; count++) {
		lseek(node_ext, (long)number * 128L, SEEK_SET);
		if (lock(node_ext, (long)number * 128L, 128) == 0) {
			wr = write(node_ext, ext, 128);
			unlock(node_ext, (long)number * 128L, 128);
			if (wr == 128)
				break;
		}
		FILE_RETRY_DELAY(count + 1, LOCK_RETRY_DELAY);
	}
	close(node_ext);
	node_ext = -1;

	if (count > (LOOP_NODEDAB / 2) && count != LOOP_NODEDAB) {
		llprintf(LOG_NOTICE, "!!", "NODE.EXB (node %d) COLLISION - Count: %d"
		         , number + 1, count);
	}
	if (count == LOOP_NODEDAB) {
		errormsg(WHERE, ERR_WRITE, "NODE.EXB", number + 1);
		return false;
	}

	return true;
}

bool sbbs_t::putnode_downloading(off_t size)
{
	action = NODE_DLNG;

	/* calculate ETA */
	time_t t = now;
	if (size < cur_cps)
		++t;
	else if (cur_cps > 0)
		t += size / cur_cps;
	struct tm tm;
	if (localtime_r(&t, &tm) != NULL)
		thisnode.aux = (tm.tm_hour * 60) + tm.tm_min;
	if (!getnodedat(cfg.node_num, &thisnode, /* lock-it: */ true))
		return false;
	thisnode.action = action;
	return putnodedat(cfg.node_num, &thisnode);
}
