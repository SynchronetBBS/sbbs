/* putnode.cpp */

/* Synchronet node information writing routines */

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

/****************************************************************************/
/* Write the data from the structure 'node' into node.dab  					*/
/* getnodedat(num,&node,1); must have been called before calling this func  */
/*          NOTE: ------^   the indicates the node record has been locked   */
/****************************************************************************/
int sbbs_t::putnodedat(uint number, node_t* node)
{
	char	str[256],firston[25];
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
				,useron.note
				,unixtodstr(&cfg,useron.firston,firston)
				,node->aux&0xff
				,node->connection
				);
			putnodeext(number,str); 
		}
		else
			node->misc&=~NODE_EXT; 
	}

	if(nodefile==-1) {
		sprintf(str,"%snode.dab",cfg.ctrl_dir);
		if((nodefile=nopen(str,O_CREAT|O_RDWR|O_DENYNONE))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_CREAT|O_RDWR|O_DENYNONE);
			return(errno); 
		}
	}

	number--;	/* make zero based */
	lock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
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

	if(wr!=sizeof(node_t)) {
		errno=wrerr;
		errormsg(WHERE,ERR_WRITE,"nodefile",number+1);
		return(errno);
	}

	return(0);
}

int sbbs_t::putnodeext(uint number, char *ext)
{
    char	str[MAX_PATH+1];
    int		count;

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
		if(write(node_ext,ext,128)==128)
			break;
	}
	unlock(node_ext,(long)number*128L,128);
	close(node_ext);
	node_ext=-1;

	if(count>(LOOP_NODEDAB/2) && count!=LOOP_NODEDAB) {
		sprintf(str,"NODE.EXB COLLISION - Count: %d",count);
		logline("!!",str); 
	}
	if(count==LOOP_NODEDAB) {
		errormsg(WHERE,ERR_WRITE,"NODE.EXB",number+1);
		return(-2); 
	}

	return(0);
}
