/* Network related wrapper functions */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sockwrap.h"
#include "genwrap.h"	/* truncsp */
#include "netwrap.h"	/* verify prototypes */

#include <stdlib.h>		/* malloc() */

#if defined(_WIN32)
	#include <iphlpapi.h>	/* GetNetworkParams */
#endif

str_list_t getNameServerList(void)
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

const char* getHostNameByAddr(const char* str)
{
	HOSTENT*	h;
	uint32_t	ip;

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	if(str==NULL)
		return NULL;
	if((ip=parseIPv4Address(str)) == INADDR_NONE)
		return str;
	if((h=gethostbyaddr((char *)&ip,sizeof(ip),AF_INET))==NULL)
		return NULL;

#ifdef _WIN32
	WSACleanup();
#endif

	return h->h_name;
}

/* In case we want to DLL-export getNameServerList in the future */
void freeNameServerList(str_list_t list)
{
	strListFree(&list);
}

// If the input is invalid, INADDR_NONE (usually -1) is returned
uint32_t parseIPv4Address(const char* value)
{
	uint32_t result = 0;

	if(strchr(value,'.') == NULL)
		return strtol(value, NULL, 10);

#if defined(__BORLANDC__) || defined(__MINGW32__)
	result = inet_addr(value); // deprecated function call
#else
	if(inet_pton(AF_INET, value, &result) != 1)
		result = INADDR_NONE;
#endif
	return ntohl(result);
}

struct in6_addr parseIPv6Address(const char* value)
{
	struct addrinfo hints = {0};
	struct addrinfo *res, *cur;
	struct in6_addr ret = {{{0}}};

	hints.ai_flags = AI_NUMERICHOST|AI_PASSIVE;
	if(getaddrinfo(value, NULL, &hints, &res))
		return ret;

	for(cur = res; cur; cur++) {
		if(cur->ai_addr->sa_family == AF_INET6)
			break;
	}
	if(!cur) {
		freeaddrinfo(res);
		return ret;
	}
	memcpy(&ret, &((struct sockaddr_in6 *)(cur->ai_addr))->sin6_addr, sizeof(ret));
	freeaddrinfo(res);
	return ret;
}

const char* IPv4AddressToStr(uint32_t addr, char* dest, size_t size)
{
	const char* result;
	struct in_addr in_addr;
	in_addr.s_addr = htonl(addr);
#if defined(__BORLANDC__) || defined(__MINGW32__)
	result = inet_ntoa(in_addr); // deprecated function call
	if(result == NULL)
		return NULL;
	strncpy(dest, result, size);
	result = dest;
#else
	result = inet_ntop(AF_INET, &in_addr, dest, size);
#endif
	return result;
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

