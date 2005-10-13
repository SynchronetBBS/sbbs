/* sockopts.c */

/* Set socket options based on contents of ctrl/sockopts.ini */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "ini_file.h"	/* ini file API */

int DLLCALL set_socket_options(scfg_t* cfg, SOCKET sock, const char* section, char* error, size_t errlen)
{
	char		cfgfile[MAX_PATH+1];
	FILE*		fp;
	int			type;
	int			result=0;
	str_list_t	list;
	socklen_t	len;

	/* Set user defined socket options */
	iniFileName(cfgfile,sizeof(cfgfile),cfg->ctrl_dir,"sockopts.ini");
	if((fp=iniOpenFile(cfgfile,FALSE))==NULL)
		return(0);
	list=iniReadFile(fp);
	fclose(fp);

	len = sizeof(type);
	result=getsockopt(sock,SOL_SOCKET,SO_TYPE,(void*)&type,&len);
	if(result) {
		sprintf(error,"%d getting socket option (TYPE, %d)"
			,ERROR_VALUE, SO_TYPE);
		return(result);
	}

	result=iniGetSocketOptions(list,ROOT_SECTION,sock,error,errlen);

	if(result==0)
		result=iniGetSocketOptions(list,type==SOCK_STREAM ? "tcp":"udp",sock,error,errlen);
	if(result==0 && section!=NULL)
		result=iniGetSocketOptions(list,section,sock,error,errlen);

	iniFreeStringList(list);

	return(result);
}
