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
int sbbs_t::putnodedat(uint number, node_t* node)
{
	char	str[256],firston[25];
	char	path[MAX_PATH+1];
	char	topic[128];
	int		wr=0;
	int		wrerr=0;
	int		attempts;

	if(!number)
		return(-1);

	if(number>cfg.sys_nodes) {
		errormsg(WHERE,ERR_CHK,"node number",number);
		return(-1); 
	}
	if(number==cfg.node_num) {
		if((node->status==NODE_INUSE || node->status==NODE_QUIET)
			&& node->action<NODE_LAST_ACTION
			&& text[NodeActionMain+node->action][0]) {
			node->misc|=NODE_EXT;
			memset(str,0,128);
			sprintf(str,text[NodeActionMain+node->action]
				,useron.alias
				,useron.level
				,getage(&cfg,useron.birth)
				,useron.sex
				,useron.comp
				,useron.ipaddr
				,unixtodstr(&cfg,useron.firston,firston)
				,node->aux&0xff
				,node->connection
				);
			putnodeext(number,str); 
		}
		else
			node->misc&=~NODE_EXT; 
	}

	sprintf(path,"%snode.dab",cfg.ctrl_dir);
	pthread_mutex_lock(&nodefile_mutex);
	if(nodefile==-1) {
		if((nodefile=nopen(path,O_CREAT|O_RDWR|O_DENYNONE))==-1) {
			errormsg(WHERE,ERR_OPEN,path,O_CREAT|O_RDWR|O_DENYNONE);
			pthread_mutex_unlock(&nodefile_mutex);
			return(errno); 
		}
	}

	number--;	/* make zero based */
	for(attempts=0;attempts<10;attempts++) {
		lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
		wr=write(nodefile,node,sizeof(node_t));
		if(wr==sizeof(node_t))
			break;
		wrerr=errno;	/* save write error */
		mswait(100);
	}
	unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
	if(cfg.node_misc&NM_CLOSENODEDAB) {
		close(nodefile);
		nodefile=-1;
	}
	pthread_mutex_unlock(&nodefile_mutex);

	snprintf(str, sizeof(str), "%u\t%u\t%u\t%u\t%x\t%u\t%u\t%u"
		,node->status
		,node->action
		,node->useron
		,node->connection
		,node->misc
		,node->aux
		,node->extaux
		,node->errors
		);
	SAFEPRINTF(topic, "node%u/status", number + 1);
	int result = mqtt_pub_strval(&startup->mqtt, TOPIC_BBS, topic, str);
	if(result != MQTT_SUCCESS)
		lprintf(LOG_ERR, "Error %d publishing node status: %s", result, topic);

	if(wr!=sizeof(node_t)) {
		errno=wrerr;
		errormsg(WHERE,ERR_WRITE,"nodefile",number+1);
		return(errno);
	}

	utime(path,NULL);	/* Update mod time for NFS/smbfs compatibility */

	return(0);
}

int sbbs_t::putnodeext(uint number, char *ext)
{
    char	str[MAX_PATH+1];
    int		count;
	int		wr;

	if(!number || number>cfg.sys_nodes) {
		errormsg(WHERE,ERR_CHK,"node number",number);
		return(-1); 
	}
	number--;   /* make zero based */

	sprintf(str,"%snode.exb",cfg.ctrl_dir);
	if((node_ext=nopen(str,O_CREAT|O_RDWR|O_DENYNONE))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_CREAT|O_RDWR|O_DENYNONE);
		return(errno); 
	}
	for(count=0;count<LOOP_NODEDAB;count++) {
		if(count)
			mswait(100);
		lseek(node_ext,(long)number*128L,SEEK_SET);
		if(lock(node_ext,(long)number*128L,128)==-1) 
			continue; 
		wr=write(node_ext,ext,128);
		unlock(node_ext,(long)number*128L,128);
		if(wr==128)
			break;
	}
	close(node_ext);
	node_ext=-1;

	if(count>(LOOP_NODEDAB/2) && count!=LOOP_NODEDAB) {
		sprintf(str,"NODE.EXB (node %d) COLLISION - Count: %d"
			,number+1, count);
		logline(LOG_NOTICE,"!!",str); 
	}
	if(count==LOOP_NODEDAB) {
		errormsg(WHERE,ERR_WRITE,"NODE.EXB",number+1);
		return(-2); 
	}

	return(0);
}
