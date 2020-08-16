/* mxlookup.c */

/* Synchronet DNS MX-record lookup routines */

/* $Id: mxlookup.c,v 1.29 2018/03/19 16:36:33 deuce Exp $ */

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

/* Platform-specific */

/* ANSI */
#include <stdio.h>
#include <string.h>		/* strchr */

/* Synchronet-specific */
#include "sockwrap.h"
#include "gen_defs.h"
#include "smbdefs.h"		/* _PACK */
#include "mailsrvr.h"

#if defined(_WIN32) || defined(__BORLANDC__)
	#pragma pack(push,1)	/* Packet structures must be packed */
#endif

/* Per RFC 1035 */
typedef struct _PACK {
	WORD	length;		/* This field included when using TCP only */
	WORD	id;
	WORD	bitfields;
	WORD	qdcount;	/* number of entries in the question section */
	WORD	ancount;	/* number of resource records in the answer section */
	WORD	nscount;	/* number of name server resource records in the authority records section */
	WORD	arcount;	/* number of resource records in the additional records section */
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

#define DNS_QR			(1<<15)		/* Query/Response (0=Query, 1=Response) */
#define DNS_AA			(1<<10)		/* Authoritavie Answer */
#define DNS_TC			(1<<9)		/* Truncation due to length limits */
#define DNS_RD			(1<<8)		/* Recursion Query desired */
#define DNS_RA			(1<<7)		/* Recursion Query available */

#define DNS_RCODE_MASK	0x000f		/* Response code bit field */
enum {
	 DNS_RCODE_OK		/* No error condition */
	,DNS_RCODE_FMT		/* Format error */
	,DNS_RCODE_SERVER	/* Server Failure */
	,DNS_RCODE_NAME		/* Name error */
	,DNS_RCODE_NI		/* Not Implemented */
	,DNS_RCODE_REFUSE	/* Refused */
};

/* DNS Resource Record Types (from RFC 1035) - subset of query types */
enum {
	 DNS_A=1			/* 1 a host address */
	,DNS_NS             /* 2 an authoritative name server */
	,DNS_MD             /* 3 a mail destination (Obsolete - use MX) */
	,DNS_MF             /* 4 a mail forwarder (Obsolete - use MX) */
	,DNS_CNAME          /* 5 the canonical name for an alias */
	,DNS_SOA            /* 6 marks the start of a zone of authority */
	,DNS_MB             /* 7 a mailbox domain name (EXPERIMENTAL) */
	,DNS_MG             /* 8 a mail group member (EXPERIMENTAL) */
	,DNS_MR             /* 9 a mail rename domain name (EXPERIMENTAL) */
	,DNS_NULL           /* 10 a null RR (EXPERIMENTAL) */
	,DNS_WKS            /* 11 a well known service description */
	,DNS_PTR            /* 12 a domain name pointer */
	,DNS_HINFO          /* 13 host information */
	,DNS_MINFO          /* 14 mailbox or mail list information */
	,DNS_MX             /* 15 mail exchange */
	,DNS_TXT            /* 16 text strings */
};

#define DNS_IN			1			/* Internet Query Class */
#define DNS_ALL			255			/* Query all records */

size_t dns_name(char* name, size_t* namelen, size_t maxlen, BYTE* srcbuf, char* p)
{
	size_t	len=0;
	size_t	plen;
	WORD	offset;

	while(p && *p && (*namelen)<maxlen) {
		if(len) 
			name[(*namelen)++]='.';		/* insert between.names */
		if(((*p)&0xC0)==0xC0) {	/* Compresssed name */
			(*p)&=~0xC0;
			offset=ntohs(*(WORD*)p);
			(*p)|=0xC0;
			dns_name(name, namelen, maxlen, srcbuf, (char*)srcbuf+offset);
			return(len+2);
		}
		plen=(*p);
		if((*namelen)+plen>maxlen)
			break;
		memcpy(name+(*namelen),p+1,plen);	/* don't copy length byte */
		(*namelen)+=plen;
		plen++;		/* Increment past length byte */
		p+=plen;
		len+=plen;
	}
	name[(*namelen)++]=0;
	return(len+1);
}

#if defined(MX_LOOKUP_TEST)
void dump(BYTE* buf, int len)
{
	int i;

	printf("%d bytes:\n",len);
	for(i=0;i<len;i++) {
		printf("%02X ",buf[i]);
		if(!((i+1)%16))
			printf("\n");
		else if(!((i+1)%8))
			printf(" - ");
	}
	printf("\n");
}
#endif

int dns_getmx(char* name, char* mx, char* mx2
			  ,DWORD intf, DWORD ip_addr, BOOL use_tcp, int timeout)
{
	char*			p;
	char*			tp;
	char			hostname[128];
	size_t			namelen;
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
	int				sess = -1;

	mx[0]=0;
	mx2[0]=0;

	sock = socket(AF_INET, use_tcp ? SOCK_STREAM : SOCK_DGRAM, IPPROTO_IP);
	if(sock == INVALID_SOCKET)
		return(ERROR_VALUE);

	mail_open_socket(sock, "dns");
	
	addr.sin_addr.s_addr = htonl(intf);
    addr.sin_family = AF_INET;
    addr.sin_port   = 0;

    result = bind(sock,(struct sockaddr *)&addr, sizeof(addr));

	if(result != 0) {
		mail_close_socket(&sock, &sess);
		return(ERROR_VALUE);
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(53);
	
	if((result=connect(sock, (struct sockaddr *)&addr, sizeof(addr)))!=0) {
		mail_close_socket(&sock, &sess);
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
			namelen=tp-p;
		else
			namelen=strlen(p);
		*(msg+len)=(BYTE)namelen;
		len++;
		memcpy(msg+len,p,namelen);
		len+=namelen;
	}
	*(msg+len)=0;	/* terminator */
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
		mail_close_socket(&sock, &sess);
		return(result);
	}

	/* send query */
#if defined(MX_LOOKUP_TEST)
	printf("Sending ");
	dump(msg+offset,len);
#endif
	i=send(sock,msg+offset,len,0);
	if(i!=len) {
		if(i==SOCKET_ERROR)
			result=ERROR_VALUE;
		else
			result=-2;
		mail_close_socket(&sock, &sess);
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
		mail_close_socket(&sock, &sess);
		return(result);
	}

	/* receive response */
	rd=recv(sock,msg,sizeof(msg),0);
	if(rd>0) {

		memcpy(((BYTE*)(&msghdr))+offset,msg,sizeof(msghdr)-offset);

#if defined(MX_LOOKUP_TEST)
		printf("Received ");
		dump(msg,rd);

		printf("%-10s %lx\n","id",ntohs(msghdr.id));
		printf("%-10s %lx\n","bitfields",ntohs(msghdr.bitfields));
		printf("%-10s %lx\n","qdcount",ntohs(msghdr.qdcount));
		printf("%-10s %lx\n","ancount",ntohs(msghdr.ancount));
		printf("%-10s %lx\n","nscount",ntohs(msghdr.nscount));
		printf("%-10s %lx\n","arcount",ntohs(msghdr.arcount));

#endif

		if(!use_tcp)
			offset=0;
		else
			offset=sizeof(msghdr.length);

		answers=ntohs(msghdr.ancount);
		p=(char*)msg+len;	/* Skip the header and question portion */

		for(i=0;i<answers && p<(char*)msg+sizeof(msg);i++) {
			namelen=0;
			p+=dns_name(hostname, &namelen, sizeof(hostname)-1, msg+offset, p);

			rr=(dns_rr_t*)p;
			p+=sizeof(dns_rr_t);
#if defined(MX_LOOKUP_TEST)
			printf("answer #%d\n",i+1);
			printf("hostname='%s'\n",hostname);
			printf("type=%x ",ntohs(rr->type));
			printf("class=%x ",ntohs(rr->class));
			printf("ttl=%lu ",ntohl(rr->ttl));
			printf("length=%d\n",ntohs(rr->length));
#endif

			len=ntohs(rr->length);
			if(ntohs(rr->type)==DNS_MX)  {
				pref=ntohs(*(WORD*)p);
				p+=2;
				namelen=0;
				p+=dns_name(hostname, &namelen, sizeof(hostname)-1, msg+offset, p);
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

	if(!mx[0]) {
#if defined(MX_LOOKUP_TEST)
		printf("No MX record found, using default name.\n");
#endif
		strcpy(mx,name);
	}
	mail_close_socket(&sock, &sess);
	return(0);
}

#ifdef MX_LOOKUP_TEST
void main(int argc, char **argv)
{
	char		mx[128],mx2[128];
	int			result;
	DWORD		bindaddr=0;
#ifdef _WIN32
	WSADATA		WSAData;
#endif

	printf("sizeof(dns_msghdr_t)=%d\n",sizeof(dns_msghdr_t));
	printf("sizeof(dns_query_t)=%d\n",sizeof(dns_query_t));
	printf("sizeof(dns_rr_t)=%d\n",sizeof(dns_rr_t));

	if(argc<3) {
		printf("usage: mxlookup hostname dns [bindaddr]\n");
		return;
	}


#ifdef _WIN32
    if((result = WSAStartup(MAKEWORD(1,1), &WSAData))!=0) {
		printf("Error %d in WSAStartup",result);
		return;
	}
#endif

	if(argc > 3)
		bindaddr=ntohl(inet_addr(argv[3]));

	if((result=dns_getmx(argv[1],mx,mx2,bindaddr,inet_addr(argv[2]),FALSE,60))!=0) 
		printf("Error %d getting mx record\n",result);
	else {
		printf("MX1: %s\n",mx);
		printf("MX2: %s\n",mx2);
	}

#ifdef _WIN32
	WSACleanup();
#endif
	gets(mx);
}
#endif

