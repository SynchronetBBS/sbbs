/* sockopts.c */

/* Set socket options based on contents of ctrl/sockopts.cfg */

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

typedef struct {
	char*	name;
	int		value;
} sockopt_name;

static const sockopt_name option_names[] = {
	{ "DEBUG",			SO_DEBUG		},
	{ "LINGER",			SO_LINGER		},
	{ "SNDBUF",			SO_SNDBUF		},
	{ "RCVBUF",			SO_RCVBUF		},
	{ "SNDLOWAT",		SO_SNDLOWAT		},
	{ "RCVLOWAT",		SO_RCVLOWAT		},
	{ "SNDTIMEO",		SO_SNDTIMEO		},
	{ "RCVTIMEO",		SO_RCVTIMEO		},
	{ "REUSEADDR",		SO_REUSEADDR	},	
	{ "KEEPALIVE",		SO_KEEPALIVE	},
	{ "DONTROUTE",		SO_DONTROUTE	},
	{ "BROADCAST",		SO_BROADCAST	},
	{ "OOBINLINE",		SO_OOBINLINE	},
	{ "ACCEPTCONN",		SO_ACCEPTCONN	},
	{ NULL }
};

int DLLCALL sockopt(char* str)
{
	int i;

	for(i=0;option_names[i].name;i++) {
		if(stricmp(str,option_names[i].name)==0)
			return(option_names[i].value);
	}
	return(strtoul(str,NULL,0));
}

int DLLCALL set_socket_options(scfg_t* cfg, SOCKET sock, char* error)
{
	char	cfgfile[MAX_PATH+1];
	char	str[256];
	char*	p;
	BYTE*	vp;
	FILE*	fp;
	int		option;
	int		value;
	int		len;
	int		result=0;
	LINGER	linger;

	/* Set user defined socket options */
	sprintf(cfgfile,"%ssockopts.cfg",cfg->ctrl_dir);
	if((fp=fopen(cfgfile,"r"))==NULL)
		return(0);
	while(!feof(fp)) {
		if(!fgets(str,sizeof(str)-1,fp))
			break;
		if(str[0]==';')
			continue;
		p=str;
		while(*p && *p>' ') p++;
		if(*p) *(p++)=0;
		option=sockopt(str);
		while(*p && *p<=' ') p++;
		len=sizeof(value);
		value=strtol(p,NULL,0);
		vp=(BYTE*)&value;
		while(*p && *p>' ') p++;
		if(*p) p++;
		while(*p && *p<=' ') p++;
		if(option==SO_LINGER) {
			linger.l_onoff = value;
			linger.l_linger = (int)strtol(p,NULL,0);
			vp=(BYTE*)&linger;
			len=sizeof(linger);
		}
#if 0
		lprintf("%04d setting socket option: %s to %d", sock, str, value);
#endif
		result=setsockopt(sock,SOL_SOCKET,option,vp,len);
		if(result) {
			sprintf(error,"%d (%d) setting socket option (%s) to %d"
				,result, ERROR_VALUE, str, value);
			break;
		}
#if 0
		len = sizeof(value);
		getsockopt(sock,SOL_SOCKET,option,(char*)&value,&len);
		lprintf("%04d socket option: %s set to %d", sock, str, value);
#endif
	}
	fclose(fp);
	return(result);
}
