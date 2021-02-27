/* netwrap.c */

/* Network related wrapper functions */

/* $Id: netwrap.c,v 1.8 2019/07/24 04:21:42 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

#include "sockwrap.h"
#include "genwrap.h"	/* truncsp */
#include "netwrap.h"	/* verify prototypes */

#include <stdlib.h>		/* malloc() */
#include <ctype.h>		/* isspace() */

#if defined(_WIN32)
	#include <iphlpapi.h>	/* GetNetworkParams */
#endif

str_list_t DLLCALL getNameServerList(void)
{
#ifdef __unix__	/* Look up DNS server address */
	FILE*	fp;
	char*	p;
	char	str[128];
	str_list_t	list;

	if((list=strListInit())==NULL)
		return(NULL);
	if((fp=fopen("/etc/resolv.conf","r"))!=NULL) {
		while(!feof(fp)) {
			if(fgets(str,sizeof(str),fp)==NULL)
				break;
			truncsp(str);
			p=str;
			SKIP_WHITESPACE(p);
			if(strnicmp(p,"nameserver",10)!=0) /* no match */
				continue;
			FIND_WHITESPACE(p);	/* skip "nameserver" */
			SKIP_WHITESPACE(p);	/* skip more white-space */
			strListPush(&list,p);
		}
		fclose(fp);
	}
	return(list);

#elif defined(_WIN32)
	FIXED_INFO* FixedInfo=NULL;
	ULONG    	FixedInfoLen=0;
	IP_ADDR_STRING* ip;
	str_list_t	list;

	if((list=strListInit())==NULL)
		return(NULL);
	if(GetNetworkParams(FixedInfo,&FixedInfoLen) == ERROR_BUFFER_OVERFLOW) {
        FixedInfo=(FIXED_INFO*)malloc(FixedInfoLen);
		if(GetNetworkParams(FixedInfo,&FixedInfoLen) == ERROR_SUCCESS) {
			ip=&FixedInfo->DnsServerList;
			for(; ip!=NULL; ip=ip->Next)
				strListPush(&list,ip->IpAddress.String);
		}
        if(FixedInfo!=NULL)
            free(FixedInfo);
    }
	return(list);
#else
	#error "Need a get_nameserver() implementation for this platform"
#endif
}

const char* DLLCALL getHostNameByAddr(const char* str)
{
	HOSTENT*	h;
	uint32_t	ip;

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	if(str==NULL)
		return NULL;
	if((ip=inet_addr(str)) == INADDR_NONE)
		return str;
	if((h=gethostbyaddr((char *)&ip,sizeof(ip),AF_INET))==NULL)
		return NULL;

#ifdef _WIN32
	WSACleanup();
#endif

	return h->h_name;
}

/* In case we want to DLL-export getNameServerList in the future */
void DLLCALL freeNameServerList(str_list_t list)
{
	strListFree(&list);
}

#if NETWRAP_TEST
int main(int argc, char** argv)
{
	size_t		i;
	str_list_t	list;

	if((list=getNameServerList())!=NULL) {
		for(i=0;list[i]!=NULL;i++)
			printf("%s\n",list[i]);
		freeNameServerList(list);
	}

	if(argc>1)
		printf("%s\n", getHostNameByAddr(argv[1]));
	return 0;
}
#endif

