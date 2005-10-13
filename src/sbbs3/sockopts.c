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

static struct {
	char*	name;
	int		level;
	int		value;
} option_names[] = {
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

/* This function is used by js_socket.c -> js_get/setsockopt() */
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

static int parse_sockopts_section(str_list_t list, SOCKET sock, int type, const char* section
						 ,char* error, size_t errlen)
{
	int			i;
	int			result;
	char*		name;
	BYTE*		vp;
	socklen_t	len;
	int			option;
	int			level;
	int			value;
	LINGER		linger;

	for(i=0;option_names[i].name!=NULL;i++) {
		name = option_names[i].name;
		if(!iniValueExists(list,section,name))
			continue;
		value=iniGetInteger(list, section, name, 0);

		vp=(BYTE*)&value;
		len=sizeof(value);

		level	= option_names[i].level;
		option	= option_names[i].value;

		switch(option) {
			case SO_LINGER:
				if(value) {
					linger.l_onoff = TRUE;
					linger.l_linger = value;
				} else {
					ZERO_VAR(linger);
				}
				vp=(BYTE*)&linger;
				len=sizeof(linger);
				break;
			case SO_KEEPALIVE:
				if(type!=SOCK_STREAM)
					continue;
				break;
		}

		if((result=setsockopt(sock,level,option,vp,len)) != 0) {
			safe_snprintf(error,errlen,"%d setting socket option (%s, %d) to %d"
				,ERROR_VALUE, name, option, value);
			return(result);
		}
	}

	return(0);
}


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

	result=parse_sockopts_section(list,sock,type,ROOT_SECTION,error,errlen);

	if(result==0 && section!=NULL)
		result=parse_sockopts_section(list,sock,type,section,error,errlen);

	iniFreeStringList(list);

	return(result);
}
