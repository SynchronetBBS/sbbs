/* Synchronet FidoNet-related routines */

/* $Id: fido.cpp,v 1.82 2020/07/15 06:12:56 rswindell Exp $ */

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

/* TODO: qwktonetmail() -> fidonet: write to SMB, not *.msg		*/

#include "sbbs.h"
#include "qwk.h"

void pt_zone_kludge(fmsghdr_t hdr,int fido)
{
	char str[256];

	sprintf(str,"\1INTL %hu:%hu/%hu %hu:%hu/%hu\r"
		,hdr.destzone,hdr.destnet,hdr.destnode
		,hdr.origzone,hdr.orignet,hdr.orignode);
	write(fido,str,strlen(str));

	if(hdr.destpoint) {
		sprintf(str,"\1TOPT %hu\r"
			,hdr.destpoint);
		write(fido,str,strlen(str)); 
	}

	if(hdr.origpoint) {
		sprintf(str,"\1FMPT %hu\r"
			,hdr.origpoint);
		write(fido,str,strlen(str)); 
	}
}

bool sbbs_t::lookup_netuser(char *into)
{
	char to[128],name[26],str[256],q[128];
	int i;
	FILE *stream;

	if(into == NULL || into[0] == 0 || strchr(into,'@'))
		return(false);
	SAFECOPY(to,into);
	strupr(to);
	sprintf(str,"%sqnet/users.dat", cfg.data_dir);
	if((stream=fnopen(&i,str,O_RDONLY))==NULL)
		return(false);
	while(!feof(stream)) {
		if(!fgets(str,sizeof(str),stream))
			break;
		str[25]=0;
		truncsp(str);
		SAFECOPY(name,str);
		strupr(name);
		str[35]=0;
		truncsp(str+27);
		SAFEPRINTF2(q,"Do you mean %s @%s",str,str+27);
		if(strstr(name,to) && yesno(q)) {
			fclose(stream);
			sprintf(into,"%s@%s",str,str+27);
			return(true); 
		}
		if(sys_status&SS_ABORT)
			break; 
	}
	fclose(stream);
	return(false);
}

/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/****************************************************************************/
faddr_t atofaddr(scfg_t* cfg, char *str)
{
	char *p;
	faddr_t addr;

	addr.zone=addr.net=addr.node=addr.point=0;
	if((p=strchr(str,':'))!=NULL) {
		addr.zone=atoi(str);
		addr.net=atoi(p+1); 
	}
	else {
		if(cfg->total_faddrs)
			addr.zone=cfg->faddr[0].zone;
		else
			addr.zone=1;
		addr.net=atoi(str); 
	}
	if(!addr.zone)              /* no such thing as zone 0 */
		addr.zone=1;
	if((p=strchr(str,'/'))!=NULL)
		addr.node=atoi(p+1);
	else {
		if(cfg->total_faddrs)
			addr.net=cfg->faddr[0].net;
		else
			addr.net=1;
		addr.node=atoi(str); 
	}
	if((p=strchr(str,'.'))!=NULL)
		addr.point=atoi(p+1);
	return(addr);
}
