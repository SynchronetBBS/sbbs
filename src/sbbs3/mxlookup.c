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


#include <stdio.h>
#include <winsock.h>

#define ERROR_VALUE			GetLastError()
#define USE_TCP				FALSE

#ifdef _WIN32
#pragma pack(push)
#pragma pack(1)
#endif

typedef struct {
#if USE_TCP
	WORD	length;
#endif
	WORD	id;
	WORD	bitfields;
	WORD	qdcount;
	WORD	ancount;
	WORD	nscount;
	WORD	arcount;
} dns_msghdr_t;

typedef struct {
	WORD	type;
	WORD	class;
} dns_query_t;

typedef struct {
	WORD	type;
	WORD	class;
	DWORD	ttl;
	WORD	length;
} dns_rr_t;

#ifdef _WIN32
#pragma pack(pop)		/* original packing */
#endif

#define DNS_QR			(1<<15)		// Query/Response (0=Query, 1=Response)
#define DNS_AA			(1<<10)		// Authoritavie Answer
#define DNS_TC			(1<<9)		// Truncation due to length limits
#define DNS_RD			(1<<8)		// Recursion Query desired
#define DNS_RA			(1<<7)		// Recursion Query available

#define DNS_RCODE_MASK	0x000f		// Response code bit field
enum {
	 DNS_RCODE_OK		// No error conidition
	,DNS_RCODE_FMT		// Format error
	,DNS_RCODE_SERVER	// Server Failure
	,DNS_RCODE_NAME		// Name error
	,DNS_RCODE_NI		// Not Implemented
	,DNS_RCODE_REFUSE	// Refused
};

#define DNS_A			1			// Host address Query Type
#define DNS_MX			15			// Mail Exchange Query Type
#define DNS_IN			1			// Internet Query Class

#ifdef DNSTEST

#define open_socket(type)	socket(AF_INET, type, IPPROTO_IP)
#define close_socket(sock)	closesocket(sock)

#else 

int open_socket(int type);
int close_socket(SOCKET sock);

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

int dns_getmx(char* name, char* mx, char* mx2, DWORD intf, DWORD ip_addr)
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
	int				result;
	int				answers;
	SOCKET			sock;
	SOCKADDR_IN		addr={0};
	BYTE			msg[512];
	dns_msghdr_t	msghdr;
	dns_query_t		query;
	dns_rr_t*		rr;


	mx[0]=0;
	mx2[0]=0;

#if USE_TCP
    sock = open_socket(SOCK_STREAM);
#else
    sock = open_socket(SOCK_DGRAM);
#endif

	if (sock == INVALID_SOCKET)
		return(ERROR_VALUE);
	
	addr.sin_addr.S_un.S_addr = htonl(intf);
    addr.sin_family = AF_INET;
    addr.sin_port   = htons (0);

    result = bind (sock, (struct sockaddr *) &addr,sizeof (addr));

	if (result != 0) {
		close_socket(sock);
		return(ERROR_VALUE);
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.S_un.S_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(53);
	
	if((result=connect(sock, (struct sockaddr *)&addr, sizeof(addr)))!=0) {
		close_socket(sock);
		return(ERROR_VALUE);
	}

	memset(&msghdr,0,sizeof(msghdr));
#if USE_TCP
	len=sizeof(msghdr)+strlen(name)+2+sizeof(query);
	msghdr.length=htons((WORD)(len-sizeof(msghdr.length)));
#endif
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
	printf("sending %d\n",len);
#if 0
	for(i=0;i<len;i++) 
		printf("%02X  ",msg[i]);
#endif
	printf("\nWaiting for response\n");
	send(sock,msg,len,0);
	rd=recv(sock,msg,512,0);
	printf("recv returned %d\n",rd);
	if(rd>0) {
#if 0      
		for(i=0;i<rd && i<200;i++) 
			printf("%02X  ",msg[i]);
		printf("\n");
#else	
		memcpy(&msghdr,msg,sizeof(msghdr));

#if 1
		printf("%-15s: %d\n","ID",ntohs(msghdr.id));
		printf("%-15s: %04X\n","Bitfields",ntohs(msghdr.bitfields));
		printf("%-15s: %d\n","QDCOUNT",ntohs(msghdr.qdcount));
		printf("%-15s: %d\n","ANCOUNT",ntohs(msghdr.ancount));
		printf("%-15s: %d\n","NSCOUNT",ntohs(msghdr.nscount));
		printf("%-15s: %d\n","ARCOUNT",ntohs(msghdr.arcount));
#endif
		answers=ntohs(msghdr.ancount);
		p=msg+len;

		for(i=0;i<answers;i++) {
			p+=dns_name(hostname,msg,p);
			printf("Answer %d: %s ",i+1, hostname);

			rr=(dns_rr_t*)p;
			p+=sizeof(dns_rr_t);
#if 0
			printf(" type=%d ",ntohs(rr->type));
			printf("class=%d ",ntohs(rr->class));
			printf("ttl=%ld ",ntohl(rr->ttl));
			printf("length=%d\n",ntohs(rr->length));
#endif

			len=ntohs(rr->length);
#if 0
			for(j=0;j<len;j++)
				printf("%02X  ",*(p+j));
#else
			if(ntohs(rr->type)==DNS_MX)  {
				pref=ntohs(*(WORD*)p);
				printf("MX %d: ",pref);
				p+=2;
				p+=dns_name(hostname, msg,p);
				if(pref<=highpref) {
					highpref=pref;
					if(mx[0])
						strcpy(mx2,mx);
					strcpy(mx,hostname);
				} else if(!mx2[0])
					strcpy(mx2,hostname);
				printf("%s",hostname);
			}
			else
#endif
				p+=len;
			printf("\n");
		}

#endif
			
	}

	if(!mx[0])
		strcpy(mx,name);
	close_socket(sock);
	return(0);
}

#ifdef RBLCHK

BOOL rblchk(ULONG mail_addr_n, char* rbl_addr, DWORD intf, DWORD dns_addr)
{
	char			name[128];
	char*			p;
	char*			tp;
	char			hostname[128];
	BYTE			namelen;
	WORD			highpref=0xffff;
	int				i;
	int				rd;
	int				len;
	int				result;
	int				answers;
	DWORD			mail_addr;
	SOCKET			sock;
	SOCKADDR_IN		addr={0};
	BYTE			msg[512];
	dns_msghdr_t	msghdr;
	dns_query_t		query;
	dns_rr_t*		rr;

	mail_addr=ntohl(mail_addr_n);
	sprintf(name,"%d.%d.%d.%d.%s"
		,mail_addr&0xff
		,(mail_addr>>8)&0xff
		,(mail_addr>>16)&0xff
		,(mail_addr>>24)&0xff
		,rbl_addr
		);
	printf("name: %s\n",name);

#if USE_TCP
    sock = open_socket(SOCK_STREAM);
#else
    sock = open_socket(SOCK_DGRAM);
#endif

	if (sock == INVALID_SOCKET) {
		fprintf(stderr,"!open_socket failed: %d\n",ERROR_VALUE);
		return(FALSE);
	}
	
	addr.sin_addr.S_un.S_addr = htonl(intf);
    addr.sin_family = AF_INET;
    addr.sin_port   = htons (0);

    result = bind (sock, (struct sockaddr *) &addr,sizeof (addr));

	if (result != 0) {
		fprintf(stderr,"!bind failed: %d\n",ERROR_VALUE);
		close_socket(sock);
		return(FALSE);
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.S_un.S_addr = dns_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(53);
	
	if((result=connect(sock, (struct sockaddr *)&addr, sizeof(addr)))!=0) {
		fprintf(stderr,"!connect failed: %d\n",ERROR_VALUE);
		close_socket(sock);
		return(FALSE);
	}

	memset(&msghdr,0,sizeof(msghdr));
	len=sizeof(msghdr)+strlen(name)+2+sizeof(query);
#if USE_TCP
	msghdr.length=htons((WORD)(len-sizeof(msghdr.length)));
#endif
	msghdr.bitfields=htons(DNS_RD);
	msghdr.qdcount=htons(1);
	query.type=htons(DNS_A);
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
	printf("sending %d\n",len);
#if 0
	for(i=0;i<len;i++) 
		printf("%02X  ",msg[i]);
#endif
	printf("\nWaiting for response\n");
	send(sock,msg,len,0);
	rd=recv(sock,msg,512,0);
	close_socket(sock);

	printf("recv returned %d\n",rd);
	if(rd<1) 
		return(FALSE);

	memcpy(&msghdr,msg,sizeof(msghdr));

	printf("%-15s: %d\n","ID",ntohs(msghdr.id));
	printf("%-15s: %04X\n","Bitfields",ntohs(msghdr.bitfields));
	printf("%-15s: %d\n","QDCOUNT",ntohs(msghdr.qdcount));
	printf("%-15s: %d\n","ANCOUNT",ntohs(msghdr.ancount));
	printf("%-15s: %d\n","NSCOUNT",ntohs(msghdr.nscount));
	printf("%-15s: %d\n","ARCOUNT",ntohs(msghdr.arcount));

		answers=ntohs(msghdr.ancount);
		p=msg+len;

		for(i=0;i<answers;i++) {
			p+=dns_name(hostname,msg,p);
			printf("Answer %d: %s ",i+1, hostname);

			rr=(dns_rr_t*)p;
			p+=sizeof(dns_rr_t);
#if 1
			printf(" type=%d ",ntohs(rr->type));
			printf("class=%d ",ntohs(rr->class));
			printf("ttl=%ld ",ntohl(rr->ttl));
			printf("length=%d\n",ntohs(rr->length));
#endif

			len=ntohs(rr->length);
			p+=len;
			printf("\n");
		}

	if(ntohs(msghdr.ancount))
		return(TRUE);

	return(FALSE);
}

#endif

#if 0
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

	if((result=dns_getmx(argv[1],mx,mx2,0,inet_addr(argv[2])))!=0) 
		printf("Error %d getting mx record\n",result);
	else {
		printf("MX1: %s\n",mx);
		printf("MX2: %s\n",mx2);
	}
	getch();

	WSACleanup();
}
#endif

#if defined(RBLCHK) && defined(DNSTEST)

void main(int argc, char **argv)
{
	int			result;
	WSADATA		WSAData;

	if(argc<4) {
		printf("usage: mxlookup hostname rbl dns\n");
		return;
	}

    if((result = WSAStartup(MAKEWORD(1,1), &WSAData))!=0) {
		printf("Error %d in WSAStartup",result);
		return;
	}

	printf("checking %s\n",argv[1]);
	if(rblchk(inet_addr(argv[1]),argv[2],0,inet_addr(argv[3]))==TRUE)
		printf("TRUE");
	else
		printf("FALSE");
	getch();

	WSACleanup();
}
#endif