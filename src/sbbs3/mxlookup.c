/* mxlookup.c */

/* Synchronet DNS MX-record lookup routines */

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

/* Platform-specific */
#ifdef _WIN32
	#include <windows.h>	/* avoid DWORD conflict */
#endif

/* ANSI */
#include <stdio.h>
#include <string.h>		/* strchr */

/* Synchronet-specific */
#include "sockwrap.h"
#include "gen_defs.h"
#include "smbdefs.h"		/* _PACK */

#if defined(_WIN32) || defined(__BORLANDC__)
#pragma pack(push)
#pragma pack(1)
#endif

typedef struct _PACK {
	WORD	length;			/* This field included when using TCP only */
	WORD	id;
	WORD	bitfields;
	WORD	qdcount;
	WORD	ancount;
	WORD	nscount;
	WORD	arcount;
} dns_msghdr_t;

typedef struct _PACK {
	WORD	type;
	WORD	class;
} dns_query_t;

typedef struct _PACK {
	WORD	type;
	WORD	class;
	DWORD	ttl;
	WORD	length;
} dns_rr_t;

#if defined(_WIN32) || defined(__BORLANDC__)
#pragma pack(pop)		/* original packing */
#endif

#define DNS_QR			(1<<15)		// Query/Response (0=Query, 1=Response)
#define DNS_AA			(1<<10)		// Authoritavie Answer
#define DNS_TC			(1<<9)		// Truncation due to length limits
#define DNS_RD			(1<<8)		// Recursion Query desired
#define DNS_RA			(1<<7)		// Recursion Query available

#define DNS_RCODE_MASK	0x000f		// Response code bit field
enum {
	 DNS_RCODE_OK		// No error condition
	,DNS_RCODE_FMT		// Format error
	,DNS_RCODE_SERVER	// Server Failure
	,DNS_RCODE_NAME		// Name error
	,DNS_RCODE_NI		// Not Implemented
	,DNS_RCODE_REFUSE	// Refused
};

#define DNS_A			1			// Host address Query Type
#define DNS_MX			15			// Mail Exchange Query Type
#define DNS_IN			1			// Internet Query Class

#ifdef MX_LOOKUP_TEST
	#define mail_open_socket(type)	socket(AF_INET, type, IPPROTO_IP)
	#define mail_close_socket(sock)	closesocket(sock)
#else
	int mail_open_socket(int type);
	int mail_close_socket(SOCKET sock);
#endif

int dns_name(char* name, char* srcbuf, char* p)
{
	char*	np=name;
	int		len=0;
	int		plen;
	WORD	offset;

	while(p && *p) {
		if(np!=name) 
			*(np++)='.';		// insert between.names
		if(((*p)&0xC0)==0xC0) {	// Compresssed name
			(*p)&=~0xC0;
			offset=ntohs(*(WORD*)p);
			(*p)|=0xC0;
			dns_name(np, srcbuf,srcbuf+offset);
			len+=2;
			return(len); //continue;
		}
		plen=(*p);
		memcpy(np,p+1,plen);	// don't copy length byte
		np+=plen;
		plen++;		// Increment past length byte
		p+=plen;
		len+=plen;
	}
	*np=0;
	return(len+1);
}

int dns_getmx(char* name, char* mx, char* mx2
			  ,DWORD intf, DWORD ip_addr, BOOL use_tcp, int timeout)
{
	char*			p;
	char*			tp;
	char			hostname[128];
	BYTE			namelen;
	WORD			pref;
	WORD			highpref=0xffff;
	int				i;
	int				rd;
	int				len;
	int				offset;
	int				result;
	int				answers;
	SOCKET			sock;
	SOCKADDR_IN		addr={0};
	BYTE			msg[512];
	dns_msghdr_t	msghdr;
	dns_query_t		query;
	dns_rr_t*		rr;
	struct timeval	tv;
	fd_set			socket_set;

	mx[0]=0;
	mx2[0]=0;

	if(use_tcp) 
		sock = mail_open_socket(SOCK_STREAM);
	else
		sock = mail_open_socket(SOCK_DGRAM);

	if (sock == INVALID_SOCKET)
		return(ERROR_VALUE);
	
	addr.sin_addr.s_addr = htonl(intf);
    addr.sin_family = AF_INET;
    addr.sin_port   = htons (0);

    result = bind (sock, (struct sockaddr *) &addr,sizeof (addr));

	if (result != 0) {
		mail_close_socket(sock);
		return(ERROR_VALUE);
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(53);
	
	if((result=connect(sock, (struct sockaddr *)&addr, sizeof(addr)))!=0) {
		mail_close_socket(sock);
		return(ERROR_VALUE);
	}

	memset(&msghdr,0,sizeof(msghdr));

	len=sizeof(msghdr)+strlen(name)+sizeof(query);
	msghdr.length=htons((WORD)len);

	msghdr.bitfields=htons(DNS_RD);
	msghdr.qdcount=htons(1);
	query.type=htons(DNS_MX);
	query.class=htons(DNS_IN);
	/* Build the message */
	memcpy(msg,&msghdr,sizeof(msghdr));
	len=sizeof(msghdr);
	for(p=name;*p;p+=namelen) {
		if(*p=='.')
			p++;
		tp=strchr(p,'.');
		if(tp)
			namelen=(BYTE)(tp-p);
		else
			namelen=(BYTE)strlen(p);
		*(msg+len)=namelen;
		len++;
		memcpy(msg+len,p,namelen);
		len+=namelen;
	}
	*(msg+len)=0;	// terminator
	len++;
	memcpy(msg+len,&query,sizeof(query));
	len+=sizeof(query);

	if(use_tcp)
		offset=0;
	else {	/* UDP */
		offset=sizeof(msghdr.length);
		len-=sizeof(msghdr.length);
	}

	/* check for writability (using select) */
	tv.tv_sec=timeout;
	tv.tv_usec=0;

	FD_ZERO(&socket_set);
	FD_SET(sock,&socket_set);

	i=select(sock+1,NULL,&socket_set,NULL,&tv);
	if(i<1) {
		if(i==SOCKET_ERROR)
			result=ERROR_VALUE;
		else 
			result=-1;
		mail_close_socket(sock);
		return(result);
	}

	/* send query */
	i=send(sock,msg+offset,len,0);
	if(i!=len) {
		if(i==SOCKET_ERROR)
			result=ERROR_VALUE;
		else
			result=-2;
		return(result);
	}

	/* check for readability (using select) */
	tv.tv_sec=timeout;
	tv.tv_usec=0;

	FD_ZERO(&socket_set);
	FD_SET(sock,&socket_set);

	i=select(sock+1,&socket_set,NULL,NULL,&tv);
	if(i<1) {
		if(i==SOCKET_ERROR)
			result=ERROR_VALUE;
		else 
			result=-1;
		mail_close_socket(sock);
		return(result);
	}

	/* receive response */
	rd=recv(sock,msg,sizeof(msg),0);
	if(rd>0) {

		memcpy(&msghdr,msg+offset,sizeof(msghdr)-offset);

		if(!use_tcp)
			offset=0;
		else
			offset=sizeof(msghdr.length);

		answers=ntohs(msghdr.ancount);
		p=msg+len;	/* Skip the header and question portion */

		for(i=0;i<answers;i++) {
			p+=dns_name(hostname, msg+offset, p);

			rr=(dns_rr_t*)p;
			p+=sizeof(dns_rr_t);

			len=ntohs(rr->length);
			if(ntohs(rr->type)==DNS_MX)  {
				pref=ntohs(*(WORD*)p);
				p+=2;
				p+=dns_name(hostname, msg+offset, p);
				if(pref<=highpref) {
					highpref=pref;
					if(mx[0])
						strcpy(mx2,mx);
					strcpy(mx,hostname);
				} else if(!mx2[0])
					strcpy(mx2,hostname);
			}
			else
				p+=len;
		}
	}

	if(!mx[0])
		strcpy(mx,name);
	mail_close_socket(sock);
	return(0);
}

#ifdef MX_LOOKUP_TEST
void main(int argc, char **argv)
{
	char		mx[128],mx2[128];
	int			result;
	WSADATA		WSAData;

	if(argc<3) {
		printf("usage: mxlookup hostname dns\n");
		return;
	}


    if((result = WSAStartup(MAKEWORD(1,1), &WSAData))!=0) {
		printf("Error %d in WSAStartup",result);
		return;
	}

	if((result=dns_getmx(argv[1],mx,mx2,0,inet_addr(argv[2]),FALSE,60))!=0) 
		printf("Error %d getting mx record\n",result);
	else {
		printf("MX1: %s\n",mx);
		printf("MX2: %s\n",mx2);
	}

	WSACleanup();
}
#endif

