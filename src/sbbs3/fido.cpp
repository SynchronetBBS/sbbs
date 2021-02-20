/* Synchronet FidoNet-related routines */

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

/* TODO: qwktonetmail() -> fidonet: write to SMB, not *.msg		*/

#include "sbbs.h"
#include "qwk.h"

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
