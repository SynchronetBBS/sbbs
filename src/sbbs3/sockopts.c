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
	int		level;
	int		value;
} sockopt_name;

static const sockopt_name option_names[] = {
	{ "TYPE",				SOL_SOCKET,		SO_TYPE				},
	{ "DEBUG",				SOL_SOCKET,		SO_DEBUG			},
	{ "LINGER",				SOL_SOCKET,		SO_LINGER			},
	{ "SNDBUF",				SOL_SOCKET,		SO_SNDBUF			},
	{ "RCVBUF",				SOL_SOCKET,		SO_RCVBUF			},
	{ "SNDLOWAT",			SOL_SOCKET,		SO_SNDLOWAT			},
	{ "RCVLOWAT",			SOL_SOCKET,		SO_RCVLOWAT			},
	{ "SNDTIMEO",			SOL_SOCKET,		SO_SNDTIMEO			},
	{ "RCVTIMEO",			SOL_SOCKET,		SO_RCVTIMEO			},
	{ "REUSEADDR",			SOL_SOCKET,		SO_REUSEADDR		},	
	{ "KEEPALIVE",			SOL_SOCKET,		SO_KEEPALIVE		},
	{ "DONTROUTE",			SOL_SOCKET,		SO_DONTROUTE		},
	{ "BROADCAST",			SOL_SOCKET,		SO_BROADCAST		},
	{ "OOBINLINE",			SOL_SOCKET,		SO_OOBINLINE		},
#ifdef SO_ACCEPTCONN											
	{ "ACCEPTCONN",			SOL_SOCKET,		SO_ACCEPTCONN		},
#endif

	/* IPPROTO-level socket options */
	{ "TCP_NODELAY",		IPPROTO_TCP,	TCP_NODELAY			},
	/* The following are platform-specific */					
#ifdef TCP_MAXSEG											
	{ "TCP_MAXSEG",			IPPROTO_TCP,	TCP_MAXSEG			},
#endif															
#ifdef TCP_CORK													
	{ "TCP_CORK",			IPPROTO_TCP,	TCP_CORK			},
#endif															
#ifdef TCP_KEEPIDLE												
	{ "TCP_KEEPIDLE",		IPPROTO_TCP,	TCP_KEEPIDLE		},
#endif															
#ifdef TCP_KEEPINTVL											
	{ "TCP_KEEPINTVL",		IPPROTO_TCP,	TCP_KEEPINTVL		},
#endif															
#ifdef TCP_KEEPCNT												
	{ "TCP_KEEPCNT",		IPPROTO_TCP,	TCP_KEEPCNT			},
#endif															
#ifdef TCP_SYNCNT												
	{ "TCP_SYNCNT",			IPPROTO_TCP,	TCP_SYNCNT			},
#endif															
#ifdef TCP_LINGER2												
	{ "TCP_LINGER2",		IPPROTO_TCP,	TCP_LINGER2			},
#endif														
#ifdef TCP_DEFER_ACCEPT										
	{ "TCP_DEFER_ACCEPT",	IPPROTO_TCP,	TCP_DEFER_ACCEPT	},
#endif															
#ifdef TCP_WINDOW_CLAMP											
	{ "TCP_WINDOW_CLAMP",	IPPROTO_TCP,	TCP_WINDOW_CLAMP	},
#endif														
#ifdef TCP_QUICKACK											
	{ "TCP_QUICKACK",		IPPROTO_TCP,	TCP_QUICKACK		},
#endif						
#ifdef TCP_NOPUSH			
	{ "TCP_NOPUSH",			IPPROTO_TCP,	TCP_NOPUSH			},
#endif						
#ifdef TCP_NOOPT			
	{ "TCP_NOOPT",			IPPROTO_TCP,	TCP_NOOPT			},
#endif
	{ NULL }
};

int DLLCALL sockopt(char* str, int* level)
{
	int i;

	*level=SOL_SOCKET;	/* default option level */
	for(i=0;option_names[i].name;i++) {
		if(stricmp(str,option_names[i].name)==0) {
			*level = option_names[i].level;
			return(option_names[i].value);
		}
	}
	if(!isdigit(str[0]))	/* unknown option name */
		return(-1);
	return(strtoul(str,NULL,0));
}

int DLLCALL set_socket_options(scfg_t* cfg, SOCKET sock, char* error)
{
	char		cfgfile[MAX_PATH+1];
	char		str[256];
	char*		p;
	char*		name;
	BYTE*		vp;
	FILE*		fp;
	int			option;
	int			level;
	int			value;
	int			type;
	int			result=0;
	LINGER		linger;
	socklen_t	len;

	/* Set user defined socket options */
	sprintf(cfgfile,"%ssockopts.cfg",cfg->ctrl_dir);
	if((fp=fopen(cfgfile,"r"))==NULL)
		return(0);

	len = sizeof(type);
	result=getsockopt(sock,SOL_SOCKET,SO_TYPE,(void*)&type,&len);
	if(result) {
		sprintf(error,"%d getting socket option (TYPE, %d)"
			,ERROR_VALUE, SO_TYPE);
		return(result);
	}

	while(!feof(fp)) {
		if(!fgets(str,sizeof(str),fp))
			break;
		name=str;
		while(*name && *name<=' ') name++;
		if(*name==';' || *name==0)	/* blank line or comment */
			continue;
		p=name;
		while(*p && *p>' ') p++;
		if(*p) *(p++)=0;
		if((option=sockopt(name,&level))==-1)
			continue;
		if(level==IPPROTO_TCP && type!=SOCK_STREAM)
			continue;
		while(*p && *p<=' ') p++;
		len=sizeof(value);
		value=strtol(p,NULL,0);
		vp=(BYTE*)&value;
		while(*p && *p>' ') p++;
		if(*p) p++;
		while(*p && *p<=' ') p++;
		switch(option) {
			case SO_LINGER:
				linger.l_onoff = value;
				linger.l_linger = (int)strtol(p,NULL,0);
				vp=(BYTE*)&linger;
				len=sizeof(linger);
				break;
			case SO_KEEPALIVE:
				if(type!=SOCK_STREAM)
					continue;
				break;
		}
#if 0
		lprintf("%04d setting socket option: %s to %d", sock, str, value);
#endif
		result=setsockopt(sock,level,option,vp,len);
		if(result) {
			sprintf(error,"%d setting socket option (%s, %d) to %d"
				,ERROR_VALUE, name, option, value);
			break;
		}
#if 0
		len = sizeof(value);
		getsockopt(sock,level,option,(void*)&value,&len);
		lprintf("%04d socket option: %s set to %d", sock, name, value);
#endif
	}
	fclose(fp);
	return(result);
}
