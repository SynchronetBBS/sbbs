/* mailsrvr.c */

/* Synchronet Mail (SMTP/POP3) server and sendmail threads */

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

/* Platform-specific headers */
#ifdef _WIN32

	#include <io.h>			/* open/close */
	#include <share.h>		/* share open flags */
	#include <process.h>	/* _beginthread */
	#include <windows.h>	/* required for mmsystem.h */
	#include <mmsystem.h>	/* SND_ASYNC */

#endif


/* ANSI C Library headers */
#include <stdio.h>
#include <stdlib.h>			/* ltoa in GNU C lib */
#include <stdarg.h>			/* va_list */
#include <string.h>			/* strrchr */
#include <ctype.h>			/* isdigit */
#include <fcntl.h>			/* Open flags */
#include <errno.h>			/* errno */

/* Synchronet-specific headers */
#include "sbbs.h"
#include "mailsrvr.h"
#include "mime.h"
#include "crc32.h"
#include "base64.h"

/* Constants */
#define FORWARD			"forward:"
#define NO_FORWARD		"local:"

int dns_getmx(char* name, char* mx, char* mx2
			  ,DWORD intf, DWORD ip_addr, BOOL use_tcp, int timeout);

static char* ok_rsp		=	"250 OK";
static char* sys_error	=	"421 System error";
static char* badseq_rsp	=	"503 Bad sequence of commands";
static char* badauth_rsp=	"535 Authentication failure";
static char* badrsp_err	=	"%s replied with:\r\n\"%s\"\r\n"
							"instead of the expected reply:\r\n\"%s ...\"";

#define TIMEOUT_THREAD_WAIT		60		/* Seconds */

#define STATUS_WFC	"Listening"

static mail_startup_t* startup=NULL;
static scfg_t	scfg;
static SOCKET	server_socket=INVALID_SOCKET;
static SOCKET	pop3_socket=INVALID_SOCKET;
static DWORD	active_clients=0;
static int		active_sendmail=0;
static DWORD	thread_count=0;
static BOOL		sendmail_running=FALSE;
static DWORD	sockets=0;
static DWORD	served=0;
static BOOL		recycle_server=FALSE;
static char		revision[16];

typedef struct {
	SOCKET			socket;
	SOCKADDR_IN		client_addr;
} smtp_t,pop3_t;

static int lprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    if(startup==NULL || startup->lputs==NULL)
        return(0);

#if defined(_WIN32) && defined(_DEBUG)
	if(IsBadCodePtr((FARPROC)startup->lputs)) {
		DebugBreak();
		return(0);
	}
#endif

	va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(startup->lputs(sbuf));
}

#ifdef _WINSOCKAPI_

static WSADATA WSAData;
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		lprintf("%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized=TRUE;
		return (TRUE);
	}

    lprintf("!WinSock startup ERROR %d", status);
	return (FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)

#endif

static void update_clients(void)
{
	if(startup!=NULL && startup->clients!=NULL) {
#if defined(_WIN32) && defined(_DEBUG)
		if(IsBadCodePtr((FARPROC)startup->clients)) {
			DebugBreak();
			return;
		}
#endif
		startup->clients(active_clients+active_sendmail);
	}
}

static void client_on(SOCKET sock, client_t* client, BOOL update)
{
	if(startup!=NULL && startup->client_on!=NULL) {
#if defined(_WIN32) && defined(_DEBUG)
		if(IsBadCodePtr((FARPROC)startup->client_on)) {
			DebugBreak();
			return;
		}
#endif
		startup->client_on(TRUE,sock,client,update);
	}
}

static void client_off(SOCKET sock)
{
	if(startup!=NULL && startup->client_on!=NULL) {
#if defined(_WIN32) && defined(_DEBUG)
		if(IsBadCodePtr((FARPROC)startup->client_on)) {
			DebugBreak();
			return;
		}
#endif
		startup->client_on(FALSE,sock,NULL,FALSE);
	}
}

static void thread_up(BOOL setuid)
{
	thread_count++;
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(TRUE,setuid);
}

static void thread_down(void)
{
	if(thread_count>0)
		thread_count--;
	if(startup!=NULL && startup->thread_up!=NULL) {
#if defined(_WIN32) && defined(_DEBUG)
		if(IsBadCodePtr((FARPROC)startup->thread_up)) {
			DebugBreak();
			return;
		}
#endif
		startup->thread_up(FALSE,FALSE);
	}
}

SOCKET mail_open_socket(int type)
{
	char	error[256];
	SOCKET	sock;

	sock=socket(AF_INET, type, IPPROTO_IP);
	if(sock!=INVALID_SOCKET && startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(TRUE);
	if(sock!=INVALID_SOCKET) {
		if(set_socket_options(&scfg, sock,error))
			lprintf("%04d !ERROR %s",sock,error);

		sockets++;
#if 0 /*def _DEBUG */
		lprintf("%04d Socket opened (%d sockets in use)",sock,sockets);
#endif
	}
	return(sock);
}

int mail_close_socket(SOCKET sock)
{
	int		result;

	if(sock==INVALID_SOCKET)
		return(-1);

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	if(/* result==0 && */ startup!=NULL && startup->socket_open!=NULL) {
#if defined(_WIN32) && defined(_DEBUG)
		if(IsBadCodePtr((FARPROC)startup->socket_open)) {
			DebugBreak();
			return(-1);
		}
#endif
		startup->socket_open(FALSE);
	}
	sockets--;
	if(result!=0) {
		if(ERROR_VALUE!=ENOTSOCK)
			lprintf("%04d !ERROR %d closing socket",sock, ERROR_VALUE);
	}
#if 0 /*def _DEBUG */
	else 
		lprintf("%04d Socket closed (%d sockets in use)",sock,sockets);
#endif

	return(result);
}

static void status(char* str)
{
	if(startup!=NULL && startup->status!=NULL) {
#if defined(_WIN32) && defined(_DEBUG)
		if(IsBadCodePtr((FARPROC)startup->status)) {
			DebugBreak();
			return;
		}
#endif
	    startup->status(str);
	}
}

int sockprintf(SOCKET sock, char *fmt, ...)
{
	int		len;
	int		result;
	va_list argptr;
	char	sbuf[1024];
	fd_set	socket_set;
	struct timeval tv;

    va_start(argptr,fmt);
    len=vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
	if(startup->options&MAIL_OPT_DEBUG_TX)
		lprintf("%04d TX: %s", sock, sbuf);
	strcat(sbuf,"\r\n");
	len+=2;
    va_end(argptr);

	if(sock==INVALID_SOCKET) {
		lprintf("!INVALID SOCKET in call to sockprintf");
		return(0);
	}

	/* Check socket for writability (using select) */
	tv.tv_sec=300;
	tv.tv_usec=0;

	FD_ZERO(&socket_set);
	FD_SET(sock,&socket_set);

	if((result=select(sock+1,NULL,&socket_set,NULL,&tv))<1) {
		if(result==0)
			lprintf("%04d !TIMEOUT selecting socket for send"
				,sock);
		else
			lprintf("%04d !ERROR %d selecting socket for send"
				,sock, ERROR_VALUE);
		return(0);
	}

	while((result=sendsocket(sock,sbuf,len))!=len) {
		if(result==SOCKET_ERROR) {
			if(ERROR_VALUE==EWOULDBLOCK) {
				mswait(1);
				continue;
			}
			if(ERROR_VALUE==ECONNRESET) 
				lprintf("%04d Connection reset by peer on send",sock);
			else if(ERROR_VALUE==ECONNABORTED) 
				lprintf("%04d Connection aborted by peer on send",sock);
			else
				lprintf("%04d !ERROR %d sending on socket",sock,ERROR_VALUE);
			return(0);
		}
		lprintf("%04d !ERROR: short send on socket: %d instead of %d",sock,result,len);
	}
	return(len);
}

static time_t checktime(void)
{
	struct tm tm;

    memset(&tm,0,sizeof(tm));
    tm.tm_year=94;
    tm.tm_mday=1;
    return(mktime(&tm)-0x2D24BD00L);
}

static void recverror(SOCKET socket, int rd, int line)
{
	if(rd==0) 
		lprintf("%04d Socket closed by peer on receive (line %d)"
			,socket, line);
	else if(rd==SOCKET_ERROR) {
		if(ERROR_VALUE==ECONNRESET) 
			lprintf("%04d Connection reset by peer on receive (line %d)"
				,socket, line);
		else if(ERROR_VALUE==ECONNABORTED) 
			lprintf("%04d Connection aborted by peer on receive (line %d)"
				,socket, line);
		else
			lprintf("%04d !ERROR %d receiving on socket (line %d)"
				,socket, ERROR_VALUE, line);
	} else
		lprintf("%04d !ERROR: recv on socket returned unexpected value: %d (line %d)"
			,socket, rd, line);
}


static int sockreadline(SOCKET socket, char* buf, int len)
{
	char	ch;
	int		i,rd=0;
	fd_set	socket_set;
	struct	timeval	tv;
	time_t	start;

	buf[0]=0;

	start=time(NULL);

	if(socket==INVALID_SOCKET) {
		lprintf("!INVALID SOCKET in call to sockreadline");
		return(0);
	}
	
	while(rd<len-1) {

		if(server_socket==INVALID_SOCKET) {
			lprintf("%04d !ABORTING sockreadline",socket);
			return(-1);
		}

		tv.tv_sec=startup->max_inactivity;
		tv.tv_usec=0;

		FD_ZERO(&socket_set);
		FD_SET(socket,&socket_set);

		i=select(socket+1,&socket_set,NULL,NULL,&tv);

		if(i<1) {
			if(i==0) {
				if((time(NULL)-start)>startup->max_inactivity) {
					lprintf("%04d !SOCKET INACTIVE",socket);
					return(0);
				}
				mswait(1);
				continue;
			}
			recverror(socket,i,__LINE__);
			return(i);
		}
		i=recv(socket, &ch, 1, 0);
		if(i<1) {
			recverror(socket,i,__LINE__);
			return(i);
		}
		if(ch=='\n' /* && rd>=1 */ ) { /* Mar-9-2003: terminate on sole LF */
			break;
		}	
		buf[rd++]=ch;
	}
	if(rd>0 && buf[rd-1]=='\r')
		buf[--rd]=0;
	else
		buf[rd]=0;
	
	return(rd);
}

static BOOL sockgetrsp(SOCKET socket, char* rsp, char *buf, int len)
{
	int rd;

	while(1) {
		rd = sockreadline(socket, buf, len);
		if(rd<1) 
			return(FALSE);
		if(buf[3]=='-')	{ /* Multi-line response */
			if(startup->options&MAIL_OPT_DEBUG_RX_RSP) 
				lprintf("%04d RX: %s",socket,buf);
			continue;
		}
		if(strnicmp(buf,rsp,strlen(rsp))) {
			lprintf("%04d !INVALID RESPONSE: '%s' Expected: '%s'", socket, buf, rsp);
			return(FALSE);
		}
		break;
	}
	if(startup->options&MAIL_OPT_DEBUG_RX_RSP) 
		lprintf("%04d RX: %s",socket,buf);
	return(TRUE);
}

#define MAX_LINE_LEN	1000

static ulong sockmsgtxt(SOCKET socket, smbmsg_t* msg, char* msgtxt, ulong maxlines)
{
	char		toaddr[256]="";
	char		fromaddr[256]="";
	char		date[64];
	char		filepath[MAX_PATH+1]="";
	char*		p;
	char*		tp;
	char*		boundary=NULL;
	int			i;
	int			s;
	ulong		lines;

	/* HEADERS (in recommended order per RFC822 4.1) */
	if(!sockprintf(socket,"Date: %s",msgdate(msg->hdr.when_written,date)))
		return(0);

	if((p=smb_get_hfield(msg,RFC822FROM,NULL))!=NULL)
		s=sockprintf(socket,"From: %s",p);	/* use original RFC822 header field */
	else {
		if(msg->from_net.type==NET_INTERNET && msg->from_net.addr!=NULL)
			SAFECOPY(fromaddr,(char*)msg->from_net.addr);
		else if(msg->from_net.type==NET_QWK && msg->from_net.addr!=NULL)
			sprintf(fromaddr,"\"%s@%s\"@%s"
				,msg->from,(char*)msg->from_net.addr,scfg.sys_inetaddr);
		else 
			usermailaddr(&scfg,fromaddr,msg->from);
		if(fromaddr[0]=='<')
			s=sockprintf(socket,"From: \"%s\" %s",msg->from,fromaddr);
		else
			s=sockprintf(socket,"From: \"%s\" <%s>",msg->from,fromaddr);
	}
	if(!s)
		return(0);

	if(msg->from_org!=NULL || msg->from_net.type==NET_NONE)
		if(!sockprintf(socket,"Organization: %s"
			,msg->from_org==NULL ? scfg.sys_name : msg->from_org))
			return(0);

	if(!sockprintf(socket,"Subject: %s",msg->subj))
		return(0);

	if((p=smb_get_hfield(msg,RFC822TO,NULL))!=NULL)
		s=sockprintf(socket,"To: %s",p);	/* use original RFC822 header field */
	else {
		if(strchr(msg->to,'@')!=NULL || msg->to_net.addr==NULL)
			s=sockprintf(socket,"To: %s",msg->to);	/* Avoid double-@ */
		else if(msg->to_net.type==NET_INTERNET || msg->to_net.type==NET_QWK) {
			if(strchr((char*)msg->to_net.addr,'<')!=NULL)
				s=sockprintf(socket,"To: %s",(char*)msg->to_net.addr);
			else
				s=sockprintf(socket,"To: \"%s\" <%s>",msg->to,(char*)msg->to_net.addr);
		} else {
			usermailaddr(&scfg,toaddr,msg->to);
			s=sockprintf(socket,"To: \"%s\" <%s>",msg->to,toaddr);
		}
	}
	if(!s)
		return(0);
	if((p=smb_get_hfield(msg,RFC822REPLYTO,NULL))==NULL) {
		if(msg->replyto_net.type==NET_INTERNET)
			p=msg->replyto_net.addr;
		else if(msg->replyto!=NULL)
			p=msg->replyto;
	}
	if(p!=NULL)
		s=sockprintf(socket,"Reply-To: %s",p);	/* use original RFC822 header field */
	if(!s)
		return(0);
	if(!sockprintf(socket,"Message-ID: %s",get_msgid(&scfg,INVALID_SUB,msg)))
		return(0);
	if(msg->reply_id!=NULL)
		if(!sockprintf(socket,"In-Reply-To: %s",msg->reply_id))
			return(0);
    for(i=0;i<msg->total_hfields;i++) { 
		if(msg->hfield[i].type==RFC822HEADER) { 
			if(!sockprintf(socket,"%s",(char*)msg->hfield_dat[i]))
				return(0);
        } else if(msg->hdr.auxattr&MSG_FILEATTACH && msg->hfield[i].type==FILEATTACH) 
            strncpy(filepath,(char*)msg->hfield_dat[i],sizeof(filepath)-1);
    }
	if(msg->hdr.auxattr&MSG_FILEATTACH) {
		if(filepath[0]==0) { /* filename stored in subject */
			if(msg->idx.to!=0)
				snprintf(filepath,sizeof(filepath)-1,"%sfile/%04u.in/%s"
					,scfg.data_dir,msg->idx.to,msg->subj);
			else
				snprintf(filepath,sizeof(filepath)-1,"%sfile/%04u.out/%s"
					,scfg.data_dir,msg->idx.from,msg->subj);
		}
        boundary=mimegetboundary();
        mimeheaders(socket,boundary);
        sockprintf(socket,"");
        mimeblurb(socket,boundary);
        sockprintf(socket,"");
        mimetextpartheader(socket,boundary);
	}
	if(!sockprintf(socket,""))	/* Header Terminator */
		return(0);

	/* MESSAGE BODY */
	lines=0;
    for(i=0;i<msg->total_hfields;i++) {			/* delivery failure notification? */
		if(msg->hfield[i].type==SMTPSYSMSG || msg->hfield[i].type==SMB_COMMENT) { 
			if(!sockprintf(socket,"%s",(char*)msg->hfield_dat[i]))
				return(0);
			lines++;
		}
    }
	p=msgtxt;
	while(*p && lines<maxlines) {
		tp=strchr(p,'\n');
		if(tp) {
			if(tp-p>MAX_LINE_LEN)
				tp=p+MAX_LINE_LEN;
			*tp=0;
		}
		truncsp(p);	/* Takes care of '\r' or spaces */
		if(*p=='.')
			i=sockprintf(socket,".%.*s",MAX_LINE_LEN,p);
		else
			i=sockprintf(socket,"%.*s",MAX_LINE_LEN,p);
		if(!i)
			break;
		if(tp==NULL)
			break;
		p=tp+1;
		lines++;
		/* release time-slices every x lines */
		if(startup->lines_per_yield
			&& !(lines%startup->lines_per_yield))	
			mswait(1);
	}
    if(msg->hdr.auxattr&MSG_FILEATTACH) { 
	    sockprintf(socket,"");
		lprintf("%04u MIME Encoding and sending %s",socket,filepath);
        if(!mimeattach(socket,boundary,filepath))
			lprintf("%04u !ERROR opening/encoding/sending %s",socket,filepath);
		else {
			endmime(socket,boundary);
			if(msg->hdr.auxattr&MSG_KILLFILE)
				if(remove(filepath)!=0)
					lprintf("%04u !ERROR %d removing %s",socket,filepath);
		}
    }
    sockprintf(socket,".");	/* End of text */
    if(boundary) FREE(boundary);
	return(lines);
}

static u_long resolve_ip(char *addr)
{
	char*		p;
	HOSTENT*	host;

	if(*addr==0)
		return(INADDR_NONE);

	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;
	if(!(*p))
		return(inet_addr(addr));

	if((host=gethostbyname(addr))==NULL) {
		lprintf("0000 !ERROR resolving hostname: %s",addr);
		return(INADDR_NONE);
	}
	return(*((ulong*)host->h_addr_list[0]));
}


static void pop3_thread(void* arg)
{
	char*		p;
	char		str[128];
	char		buf[512];
	char		host_name[128];
	char		host_ip[64];
	char		username[LEN_ALIAS+1];
	char		password[LEN_PASS+1];
	char*		msgtxt;
	int			i;
	int			rd;
	BOOL		activity=FALSE;
	ulong		l;
	ulong		lines;
	ulong		lines_sent;
	ulong		msgs,bytes,msgnum,msgbytes;
	SOCKET		socket;
	HOSTENT*	host;
	smb_t		smb;
	smbmsg_t	msg;
	user_t		user;
	client_t	client;
	mail_t*		mail;
	pop3_t		pop3=*(pop3_t*)arg;

	thread_up(TRUE /* setuid */);

	free(arg);

	socket=pop3.socket;

	if(startup->options&MAIL_OPT_DEBUG_POP3)
		lprintf("%04d POP3 session thread started", socket);

#ifdef _WIN32
	if(startup->pop3_sound[0] && !(startup->options&MAIL_OPT_MUTE)) 
		PlaySound(startup->pop3_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

	SAFECOPY(host_ip,inet_ntoa(pop3.client_addr.sin_addr));

	if(startup->options&MAIL_OPT_DEBUG_POP3)
		lprintf("%04d POP3 connection accepted from: %s port %u"
			,socket, host_ip, ntohs(pop3.client_addr.sin_port));

	if(startup->options&MAIL_OPT_NO_HOST_LOOKUP)
		host=NULL;
	else
		host=gethostbyaddr((char *)&pop3.client_addr.sin_addr
			,sizeof(pop3.client_addr.sin_addr),AF_INET);

	if(host!=NULL && host->h_name!=NULL)
		SAFECOPY(host_name,host->h_name);
	else
		strcpy(host_name,"<no name>");

	if(startup->options&MAIL_OPT_DEBUG_POP3
		&& !(startup->options&MAIL_OPT_NO_HOST_LOOKUP)) {
		lprintf("%04d POP3 Hostname: %s", socket, host_name);
		for(i=0;host!=NULL && host->h_aliases!=NULL && host->h_aliases[i]!=NULL;i++)
			lprintf("%04d POP3 HostAlias: %s", socket, host->h_aliases[i]);
	}

	if(trashcan(&scfg,host_ip,"ip")) {
		lprintf("%04d !POP3 BLOCKED CLIENT IP ADDRESS: %s"
			,socket, host_ip);
		sockprintf(socket,"-ERR Access denied.");
		mail_close_socket(socket);
		thread_down();
		return;
	}

	if(trashcan(&scfg,host_name,"host")) {
		lprintf("%04d !POP3 BLOCKED CLIENT HOSTNAME: %s"
			,socket, host_name);
		sockprintf(socket,"-ERR Access denied.");
		mail_close_socket(socket);
		thread_down();
		return;
	}

	active_clients++, update_clients();

	/* Initialize client display */
	client.size=sizeof(client);
	client.time=time(NULL);
	SAFECOPY(client.addr,host_ip);
	SAFECOPY(client.host,host_name);
	client.port=ntohs(pop3.client_addr.sin_port);
	client.protocol="POP3";
	client.user="<unknown>";
	client_on(socket,&client,FALSE /* update */);

	sprintf(str,"POP3: %s", host_ip);
	status(str);

	mail=NULL;

	do {
		memset(&smb,0,sizeof(smb));
		memset(&msg,0,sizeof(msg));
		memset(&user,0,sizeof(user));

		sprintf(smb.file,"%smail",scfg.data_dir);
		smb.retry_time=scfg.smb_retry_time;
		smb.subnum=INVALID_SUB;
		if((i=smb_open(&smb))!=0) {
			lprintf("%04d !ERROR %d (%s) opening %s",socket,i,smb.last_error,smb.file);
			sockprintf(socket,"-ERR %d opening %s",i,smb.file);
			break;
		}

		sockprintf(socket,"+OK Synchronet POP3 Server %s-%s Ready"
			,revision,PLATFORM_DESC);

		/* Requires USER command first */
		for(i=3;i;i--) {
			if(sockgetrsp(socket,"USER ",buf,sizeof(buf)))
				break;
			sockprintf(socket,"-ERR USER command expected");
		}
		if(!i)	/* no USER command received */
			break;

		p=buf+5;
		while(*p && *p<=' ') p++;
		SAFECOPY(username,p);
		sockprintf(socket,"+OK");
		if(!sockgetrsp(socket,"PASS ",buf,sizeof(buf))) {
			sockprintf(socket,"-ERR PASS command expected");
			break;
		}
		p=buf+5;
		while(*p && *p<=' ') p++;
		SAFECOPY(password,p);
		user.number=matchuser(&scfg,username,FALSE /*sysop_alias*/);
		if(!user.number) {
			if(scfg.sys_misc&SM_ECHO_PW)
				lprintf("%04d !POP3 UNKNOWN USER: %s (password: %s)"
					,socket, username, password);
			else
				lprintf("%04d !POP3 UNKNOWN USER: %s"
					,socket, username);
			sockprintf(socket,"-ERR");
			break;
		}
		if((i=getuserdat(&scfg, &user))!=0) {
			lprintf("%04d !POP3 ERROR %d getting data on user (%s)"
				,socket, i, username);
			sockprintf(socket, "-ERR");
			break;
		}
		if(user.misc&(DELETED|INACTIVE)) {
			lprintf("%04d !POP3 DELETED or INACTIVE user #%u (%s)"
				,socket, user.number, username);
			sockprintf(socket, "-ERR");
			break;
		}
		if(stricmp(password,user.pass)) {
			if(scfg.sys_misc&SM_ECHO_PW)
				lprintf("%04d !POP3 FAILED Password attempt for user %s: '%s' expected '%s'"
					,socket, username, password, user.pass);
			else
				lprintf("%04d !POP3 FAILED Password attempt for user %s"
					,socket, username);
			sockprintf(socket, "-ERR");
			break;
		}
		putuserrec(&scfg,user.number,U_COMP,LEN_COMP,host_name);
		putuserrec(&scfg,user.number,U_NOTE,LEN_NOTE,host_ip);

		/* Update client display */
		client.user=user.alias;
		client_on(socket,&client,TRUE /* update */);

		if(startup->options&MAIL_OPT_DEBUG_POP3)		
			lprintf("%04d POP3 %s logged in", socket, user.alias);
		sprintf(str,"POP3: %s",user.alias);
		status(str);

		sockprintf(socket,"+OK User verified");

		mail=loadmail(&smb,&msgs,user.number,MAIL_YOUR,0);

		for(l=bytes=0;l<msgs;l++) {
			msg.hdr.number=mail[l].number;
			if((i=smb_getmsgidx(&smb,&msg))!=0) {
				lprintf("%04d !POP3 ERROR %d (%s) getting message index"
					,socket, i, smb.last_error);
				break;
			}
			if((i=smb_lockmsghdr(&smb,&msg))!=0) {
				lprintf("%04d !POP3 ERROR %d (%s) locking message header #%lu"
					,socket, i, smb.last_error, msg.hdr.number);
				break; 
			}
			i=smb_getmsghdr(&smb,&msg);
			smb_unlockmsghdr(&smb,&msg);
			if(i!=0) {
				lprintf("%04d !POP3 ERROR %d (%s) getting message header #%lu"
					,socket, i, smb.last_error, msg.hdr.number);
				break;
			}
			for(i=0;i<msg.hdr.total_dfields;i++)
				if(msg.dfield[i].type==TEXT_BODY || msg.dfield[i].type==TEXT_TAIL)
					bytes+=msg.dfield[i].length;
			smb_freemsgmem(&msg);
		}			

		while(1) {	/* TRANSACTION STATE */
			rd = sockreadline(socket, buf, sizeof(buf));
			if(rd<1) 
				break;
			truncsp(buf);
			if(startup->options&MAIL_OPT_DEBUG_POP3)
				lprintf("%04d POP3 RX: %s", socket, buf);
			if(!stricmp(buf, "NOOP")) {
				sockprintf(socket,"+OK");
				continue;
			}
			if(!stricmp(buf, "QUIT")) {
				sockprintf(socket,"+OK");
				break;
			}
			if(!stricmp(buf, "STAT")) {
				sockprintf(socket,"+OK %lu %lu",msgs,bytes);
				continue;
			}
			if(!stricmp(buf, "RSET")) {
				for(l=0;l<msgs;l++) {
					msg.hdr.number=mail[l].number;
					if((i=smb_getmsgidx(&smb,&msg))!=0) {
						lprintf("%04d !POP3 ERROR %d (%s) getting message index"
							,socket, i, smb.last_error);
						break;
					}
					if((i=smb_lockmsghdr(&smb,&msg))!=0) {
						lprintf("%04d !POP3 ERROR %d (%s) locking message header #%lu"
							,socket, i, smb.last_error, msg.hdr.number);
						break; 
					}
					if((i=smb_getmsghdr(&smb,&msg))!=0) {
						smb_unlockmsghdr(&smb,&msg);
						lprintf("%04d !POP3 ERROR %d (%s) getting message header #%lu"
							,socket, i, smb.last_error, msg.hdr.number);
						break;
					}
					msg.hdr.attr=mail[l].attr;
					msg.idx.attr=msg.hdr.attr;
					if((i=smb_putmsg(&smb,&msg))!=0)
						lprintf("%04d !POP3 ERROR %d (%s) updating message index"
							,socket, i, smb.last_error);
					smb_unlockmsghdr(&smb,&msg);
					smb_freemsgmem(&msg);
				}
				if(l<msgs)
					sockprintf(socket,"-ERR %d messages reset (ERROR: %d)",l,i);
				else
					sockprintf(socket,"+OK %lu messages (%lu bytes)",msgs,bytes);
				continue;
			}
			if(!strnicmp(buf, "LIST",4) || !strnicmp(buf,"UIDL",4)) {
				p=buf+4;
				while(*p && *p<=' ') p++;
				if(isdigit(*p)) {
					msgnum=atol(p);
					if(msgnum<1 || msgnum>msgs) {
						lprintf("%04d !POP3 INVALID message #%ld"
							,socket, msgnum);
						sockprintf(socket,"-ERR no such message");
						continue;
					}
					msg.hdr.number=mail[msgnum-1].number;
					if((i=smb_getmsgidx(&smb,&msg))!=0) {
						lprintf("%04d !POP3 ERROR %d (%s) getting message index"
							,socket, i, smb.last_error);
						sockprintf(socket,"-ERR %d getting message index",i);
						break;
					}
					if(msg.idx.attr&MSG_DELETE) {
						lprintf("%04d !POP3 ATTEMPT to list DELETED message"
							,socket);
						sockprintf(socket,"-ERR message deleted");
						continue;
					}
					if((i=smb_lockmsghdr(&smb,&msg))!=0) {
						lprintf("%04d !POP3 ERROR %d (%s) locking message header #%lu"
							,socket, i, smb.last_error, msg.hdr.number);
						sockprintf(socket,"-ERR %d locking message header",i);
						continue; 
					}
					i=smb_getmsghdr(&smb,&msg);
					smb_unlockmsghdr(&smb,&msg);
					if(i!=0) {
						smb_freemsgmem(&msg);
						lprintf("%04d !POP3 ERROR %d (%s) getting message header #%lu"
							,socket, i, smb.last_error, msg.hdr.number);
						sockprintf(socket,"-ERR %d getting message header",i);
						continue;
					}
					if(!strnicmp(buf, "LIST",4)) {
						msgbytes=0;
						for(i=0;i<msg.hdr.total_dfields;i++)
							if(msg.dfield[i].type==TEXT_BODY || msg.dfield[i].type==TEXT_TAIL)
								msgbytes+=msg.dfield[i].length;
						sockprintf(socket,"+OK %lu %lu",msgnum,msgbytes);
					} else /* UIDL */
						sockprintf(socket,"+OK %lu %lu",msgnum,msg.hdr.number);

					smb_freemsgmem(&msg);
					continue;
				}
				/* List ALL messages */
				sockprintf(socket,"+OK %lu messages (%lu bytes)",msgs,bytes);
				for(l=0;l<msgs;l++) {
					msg.hdr.number=mail[l].number;
					if((i=smb_getmsgidx(&smb,&msg))!=0) {
						lprintf("%04d !POP3 ERROR %d (%s) getting message index"
							,socket, i, smb.last_error);
						break;
					}
					if(msg.idx.attr&MSG_DELETE) 
						continue;
					if((i=smb_lockmsghdr(&smb,&msg))!=0) {
						lprintf("%04d !POP3 ERROR %d (%s) locking message header #%lu"
							,socket, i, smb.last_error, msg.hdr.number);
						break; 
					}
					i=smb_getmsghdr(&smb,&msg);
					smb_unlockmsghdr(&smb,&msg);
					if(i!=0) {
						smb_freemsgmem(&msg);
						lprintf("%04d !POP3 ERROR %d (%s) getting message header #%lu"
							,socket, i, smb.last_error, msg.hdr.number);
						break;
					}
					if(!strnicmp(buf, "LIST",4)) {
						msgbytes=0;
						for(i=0;i<msg.hdr.total_dfields;i++)
							if(msg.dfield[i].type==TEXT_BODY || msg.dfield[i].type==TEXT_TAIL)
								msgbytes+=msg.dfield[i].length;
						sockprintf(socket,"%lu %lu",l+1,msgbytes);
					} else /* UIDL */
						sockprintf(socket,"%lu %lu",l+1,msg.hdr.number);

					smb_freemsgmem(&msg);
				}			
				sockprintf(socket,".");
				continue;
			}
			activity=TRUE;
			if(!strnicmp(buf, "RETR ",5) || !strnicmp(buf,"TOP ",4)) {
				sprintf(str,"POP3: %s", user.alias);
				status(str);

				lines=-1;
				p=buf+4;
				while(*p && *p<=' ') p++;
				msgnum=atol(p);

				if(!strnicmp(buf,"TOP ",4)) {
					while(*p && isdigit(*p)) p++;
					while(*p && *p<=' ') p++;
					lines=atol(p);
				}
				if(msgnum<1 || msgnum>msgs) {
					lprintf("%04d !POP3 %s ATTEMPTED to retrieve an INVALID message #%ld"
						,socket, user.alias, msgnum);
					sockprintf(socket,"-ERR no such message");
					continue;
				}
				msg.hdr.number=mail[msgnum-1].number;

				lprintf("%04d POP3 %s retrieving message #%ld"
					,socket, user.alias, msg.hdr.number);

				if((i=smb_getmsgidx(&smb,&msg))!=0) {
					lprintf("%04d !POP3 ERROR %d (%s) getting message index"
						,socket, i, smb.last_error);
					sockprintf(socket,"-ERR %d getting message index",i);
					continue;
				}
				if(msg.idx.attr&MSG_DELETE) {
					lprintf("%04d !POP3 ATTEMPT to retrieve DELETED message"
						,socket);
					sockprintf(socket,"-ERR message deleted");
					continue;
				}
				if((i=smb_lockmsghdr(&smb,&msg))!=0) {
					lprintf("%04d !POP3 ERROR %d (%s) locking message header #%lu"
						,socket, i, smb.last_error, msg.hdr.number);
					sockprintf(socket,"-ERR %d locking message header",i);
					continue; 
				}
				i=smb_getmsghdr(&smb,&msg);
				smb_unlockmsghdr(&smb,&msg);
				if(i!=0) {
					lprintf("%04d !POP3 ERROR %d (%s) getting message header #%lu"
						,socket, i, smb.last_error, msg.hdr.number);
					sockprintf(socket,"-ERR %d getting message header",i);
					continue;
				}

				if((msgtxt=smb_getmsgtxt(&smb,&msg,GETMSGTXT_TAILS))==NULL) {
					smb_freemsgmem(&msg);
					lprintf("%04d !POP3 ERROR (%s) retrieving message %lu text"
						,socket, smb.last_error, msg.hdr.number);
					sockprintf(socket,"-ERR retrieving message text");
					continue;
				}

				sockprintf(socket,"+OK message follows");
				lprintf("%04d POP3 sending message text (%u bytes)"
					,socket,strlen(msgtxt));
				lines_sent=sockmsgtxt(socket,&msg,msgtxt,lines);
				/* if(startup->options&MAIL_OPT_DEBUG_POP3) */
				if(lines!=-1 && lines_sent<lines)	/* could send *more* lines */
					lprintf("%04d !POP3 ERROR sending message text (sent %ld of %ld lines)"
						,socket,lines_sent,lines);
				else {
					lprintf("%04d POP3 message transfer complete (%lu lines)"
						,socket,lines_sent);

					msg.hdr.attr|=MSG_READ;
					msg.idx.attr=msg.hdr.attr;
					msg.hdr.netattr|=MSG_SENT;

					if((i=smb_lockmsghdr(&smb,&msg))!=0) 
						lprintf("%04d !POP3 ERROR %d (%s) locking message header #%lu"
							,socket, i, smb.last_error, msg.hdr.number);
					if((i=smb_putmsg(&smb,&msg))!=0)
						lprintf("%04d !POP3 ERROR %d (%s) marking message #%lu as read"
							,socket, i, smb.last_error, msg.hdr.number);
					smb_unlockmsghdr(&smb,&msg);
				}
				smb_freemsgmem(&msg);
				smb_freemsgtxt(msgtxt);
				continue;
			}
			if(!strnicmp(buf, "DELE ",5)) {
				p=buf+5;
				while(*p && *p<=' ') p++;
				msgnum=atol(p);

				if(msgnum<1 || msgnum>msgs) {
					lprintf("%04d !POP3 %s ATTEMPTED to delete an INVALID message #%ld"
						,socket, user.alias, msgnum);
					sockprintf(socket,"-ERR no such message");
					continue;
				}
				msg.hdr.number=mail[msgnum-1].number;

				lprintf("%04d POP3 %s deleting message #%ld"
					,socket, user.alias, msg.hdr.number);

				if((i=smb_getmsgidx(&smb,&msg))!=0) {
					lprintf("%04d !POP3 ERROR %d (%s) getting message index"
						,socket, i, smb.last_error);
					sockprintf(socket,"-ERR %d getting message index",i);
					continue;
				}
				if((i=smb_lockmsghdr(&smb,&msg))!=0) {
					lprintf("%04d !POP3 ERROR %d (%s) locking message header #%lu"
						,socket, i, smb.last_error, msg.hdr.number);
					sockprintf(socket,"-ERR %d locking message header",i);
					continue; 
				}
				if((i=smb_getmsghdr(&smb,&msg))!=0) {
					smb_unlockmsghdr(&smb,&msg);
					lprintf("%04d !POP3 ERROR %d (%s) getting message header #%lu"
						,socket, i, smb.last_error, msg.hdr.number);
					sockprintf(socket,"-ERR %d getting message header",i);
					continue;
				}
				msg.hdr.attr|=MSG_DELETE;
				msg.idx.attr=msg.hdr.attr;
				if((i=smb_putmsg(&smb,&msg))!=0) {
					smb_unlockmsghdr(&smb,&msg);
					smb_freemsgmem(&msg);
					lprintf("%04d !POP3 ERROR %d (%s) marking message as read"
						, socket, i, smb.last_error);
					sockprintf(socket,"-ERR %d marking message for deletion",i);
					continue;
				}
				if(msg.hdr.auxattr&MSG_FILEATTACH)
					delfattach(&scfg,&msg);
				smb_unlockmsghdr(&smb,&msg);
				smb_freemsgmem(&msg);
				sockprintf(socket,"+OK");
				if(startup->options&MAIL_OPT_DEBUG_POP3)
					lprintf("%04d POP3 message deleted", socket);
				continue;
			}
			lprintf("%04d !POP3 UNSUPPORTED COMMAND from %s: '%s'"
				,socket, user.alias, buf);
			sockprintf(socket,"-ERR UNSUPPORTED COMMAND: %s",buf);
		}
		if(user.number)
			logoutuserdat(&scfg,&user,time(NULL),client.time);

	} while(0);

	if(activity) 
		lprintf("%04d POP3 %s logged out from port %u on %s [%s]"
			,socket, user.alias, ntohs(pop3.client_addr.sin_port), host_name, host_ip);

	status(STATUS_WFC);

	/* Free up resources here */
	if(mail!=NULL)
		freemail(mail);

	smb_freemsgmem(&msg);
	smb_close(&smb);

	active_clients--,update_clients();
	client_off(socket);

	thread_down();
	if(startup->options&MAIL_OPT_DEBUG_POP3)
		lprintf("%04d POP3 session thread terminated (%u threads remain, %lu clients served)"
			,socket, thread_count, served);

	/* Must be last */
	mail_close_socket(socket);
}

static ulong rblchk(SOCKET sock, DWORD mail_addr_n, const char* rbl_addr)
{
	char		name[256];
	DWORD		mail_addr;
	HOSTENT*	host;
	struct in_addr dnsbl_result;

	mail_addr=ntohl(mail_addr_n);
	sprintf(name,"%ld.%ld.%ld.%ld.%s"
		,mail_addr&0xff
		,(mail_addr>>8)&0xff
		,(mail_addr>>16)&0xff
		,(mail_addr>>24)&0xff
		,rbl_addr
		);

	if(startup->options&MAIL_OPT_DNSBL_DEBUG)
		lprintf("%04d SMTP DNSBL query: %s",sock,name);

	if((host=gethostbyname(name))==NULL)
		return(0);

	dnsbl_result.s_addr = *((ulong*)host->h_addr_list[0]);
	lprintf("%04d SMTP DNSBL query: %s resolved to: %s"
		,sock,name,inet_ntoa(dnsbl_result));

	return(dnsbl_result.s_addr);
}

static ulong dns_blacklisted(SOCKET sock, IN_ADDR addr, char* host_name, char* list)
{
	char	fname[MAX_PATH+1];
	char	str[256];
	char*	p;
	char*	tp;
	FILE*	fp;
	ulong	found=0;

	sprintf(fname,"%sdnsbl_exempt.cfg",scfg.ctrl_dir);
	if(findstr(inet_ntoa(addr),fname))
		return(FALSE);
	if(findstr(host_name,fname))
		return(FALSE);

	sprintf(fname,"%sdns_blacklist.cfg", scfg.ctrl_dir);
	if((fp=fopen(fname,"r"))==NULL)
		return(FALSE);

	while(!feof(fp) && !found) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		truncsp(str);

		p=str;
		while(*p && *p<=' ') p++;
		if(*p==';' || *p==0) /* comment or blank line */
			continue;

		sprintf(list,"%.100s",p);

		/* terminate */
		tp = p;
		while(*tp && *tp>' ') tp++;
		*tp=0;	

		found = rblchk(sock, addr.s_addr, p);
	}
	fclose(fp);

	return(found);
}


static BOOL chk_email_addr(SOCKET socket, char* p, char* host_name, char* host_ip
						   ,char* to, char* from)
{
	char	addr[64];
	char	tmp[128];

	while(*p && *p<=' ') p++;
	if(*p=='<') p++;		/* Skip '<' */
	SAFECOPY(addr,p);
	truncstr(addr,">( ");

	if(!trashcan(&scfg,addr,"email"))
		return(TRUE);

	lprintf("%04d !SMTP BLOCKED SOURCE: %s"
		,socket, addr);
	sprintf(tmp,"Blocked source e-mail address: %s", addr);
	spamlog(&scfg, "SMTP", "REFUSED", tmp, host_name, host_ip, to, from);
	sockprintf(socket, "554 Sender not allowed.");

	return(FALSE);
}

static void signal_smtp_sem(void)
{
	int file;

	if(scfg.smtpmail_sem[0]==0) 
		return; /* do nothing */

	if((file=open(scfg.smtpmail_sem,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
		close(file);
}

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
static char* cmdstr(char* instr, char* msgpath, char* lstpath, char* errpath, char* cmd)
{
	char	str[256];
    int		i,j,len;

    len=strlen(instr);
    for(i=j=0;i<len;i++) {
        if(instr[i]=='%') {
            i++;
            cmd[j]=0;
            switch(toupper(instr[i])) {
				case 'E':
					strcat(cmd,errpath);
					break;
                case 'G':   /* Temp directory */
                    strcat(cmd,scfg.temp_dir);
                    break;
                case 'J':
                    strcat(cmd,scfg.data_dir);
                    break;
                case 'K':
                    strcat(cmd,scfg.ctrl_dir);
                    break;
				case 'L':
					strcat(cmd,lstpath);
					break;
				case 'F':
				case 'M':
					strcat(cmd,msgpath);
					break;
                case 'O':   /* SysOp */
                    strcat(cmd,scfg.sys_op);
                    break;
                case 'Q':   /* QWK ID */
                    strcat(cmd,scfg.sys_id);
                    break;
                case 'V':   /* Synchronet Version */
                    sprintf(str,"%s%c",VERSION,REVISION);
                    break;
                case 'Z':
                    strcat(cmd,scfg.text_dir);
                    break;
                case '!':   /* EXEC Directory */
                    strcat(cmd,scfg.exec_dir);
                    break;
                case '%':   /* %% for percent sign */
                    strcat(cmd,"%");
                    break;
				case '?':	/* Platform */
#ifdef __OS2__
					SAFECOPY(str,"OS2");
#else
					SAFECOPY(str,PLATFORM_DESC);
#endif
					strlwr(str);
					strcat(cmd,str);
					break;
                default:    /* unknown specification */
                    break; 
			}
            j=strlen(cmd); 
		}
        else
            cmd[j++]=instr[i]; 
	}
    cmd[j]=0;

    return(cmd);
}


static int parse_header_field(char* buf, smbmsg_t* msg, ushort* type)
{
	char*	p;
	ushort	nettype;

	if(buf[0]<=' ' && *type!=UNKNOWN) {	/* folded header, append to previous */
		p=buf;
		truncsp(p);
		smb_hfield_append(msg,*type,2,"\r\n");
		return smb_hfield_append(msg,*type, (ushort)strlen(p), p);
	}

	if(!strnicmp(buf, "TO:",3)) {
		p=buf+3;
		while(*p && *p<=' ') p++;
		truncsp(p);
		return smb_hfield(msg, *type=RFC822TO, (ushort)strlen(p), p);
	}
	if(!strnicmp(buf, "REPLY-TO:",9)) {
		p=buf+9;
		while(*p && *p<=' ') p++;
		truncsp(p);
		smb_hfield(msg, *type=RFC822REPLYTO, (ushort)strlen(p), p);
		if(*p=='<')  {
			p++;
			truncstr(p,">");
		}
		nettype=NET_INTERNET;
		smb_hfield(msg, REPLYTONETTYPE, sizeof(nettype), &nettype);
		return smb_hfield(msg, *type=REPLYTONETADDR, (ushort)strlen(p), p);
	}
	if(!strnicmp(buf, "FROM:", 5)) {
		p=buf+5;
		while(*p && *p<=' ') p++;
		truncsp(p);
		return smb_hfield(msg, *type=RFC822FROM, (ushort)strlen(p), p);
	}
	if(!strnicmp(buf, "ORGANIZATION:",13)) {
		p=buf+13;
		while(*p && *p<=' ') p++;
		return smb_hfield(msg, *type=SENDERORG, (ushort)strlen(p), p);
	}
	if(!strnicmp(buf, "DATE:",5)) {
		p=buf+5;
		msg->hdr.when_written=rfc822date(p);
		*type=UNKNOWN;
		return(0);
	}
	if(!strnicmp(buf, "MESSAGE-ID:",11)) {
		p=buf+11;
		while(*p && *p<=' ') p++;
		return smb_hfield(msg, *type=RFC822MSGID, (ushort)strlen(p), p);
	}
	if(!strnicmp(buf, "IN-REPLY-TO:",12)) {
		p=buf+12;
		while(*p && *p<=' ') p++;
		return smb_hfield(msg, *type=RFC822REPLYID, (ushort)strlen(p), p);
	}
	/* Fall-through */
	return smb_hfield(msg, *type=RFC822HEADER, (ushort)strlen(buf), buf);
}

static void smtp_thread(void* arg)
{
	int			i;
	int			rd;
	char		str[512];
	char		tmp[128];
	char		buf[1024],*p,*tp,*cp;
	char		hdrfield[512];
	char		alias_buf[128];
	char		name_alias_buf[128];
	char		reverse_path[128];
	char		date[64];
	char		rcpt_name[128];
	char		rcpt_addr[128];
	char		sender[128];
	char		sender_addr[128];
	char		hello_name[128];
	char		user_name[LEN_ALIAS+1];
	char		user_pass[LEN_PASS+1];
	char		relay_list[MAX_PATH+1];
	char		domain_list[MAX_PATH+1];
	char		host_name[128];
	char		host_ip[64];
	char		dnsbl[256];
	char*		telegram_buf;
	char*		msgbuf;
	char		dest_host[128];
	ushort		dest_port;
	socklen_t	addr_len;
	ushort		hfield_type;
	ushort		nettype;
	uint		usernum;
	ulong		lines=0;
	ulong		hdr_lines=0;
	ulong		hdr_len=0;
	ulong		length;
	ulong		badcmds=0;
	BOOL		esmtp=FALSE;
	BOOL		telegram=FALSE;
	BOOL		forward=FALSE;
	BOOL		no_forward=FALSE;
	uint		subnum=INVALID_SUB;
	FILE*		msgtxt=NULL;
	char		msgtxt_fname[MAX_PATH+1];
	FILE*		rcptlst;
	char		rcptlst_fname[MAX_PATH+1];
	ushort		rcpt_count=0;
	FILE*		proc_cfg;
	FILE*		proc_err;
	char		proc_err_fname[MAX_PATH+1];
	char		session_id[MAX_PATH+1];
	FILE*		spy=NULL;
	SOCKET		socket;
	HOSTENT*	host;
	int			smb_error;
	smb_t		smb;
	smbmsg_t	msg;
	smbmsg_t	newmsg;
	user_t		user;
	user_t		relay_user;
	node_t		node;
	client_t	client;
	smtp_t		smtp=*(smtp_t*)arg;
	SOCKADDR_IN server_addr;
	IN_ADDR		dnsbl_result;
	enum {
			 SMTP_STATE_INITIAL
			,SMTP_STATE_HELO
			,SMTP_STATE_MAIL_FROM
			,SMTP_STATE_RCPT_TO
			,SMTP_STATE_DATA_HEADER
			,SMTP_STATE_DATA_BODY

	} state = SMTP_STATE_INITIAL;

	enum {
			 SMTP_CMD_NONE
			,SMTP_CMD_MAIL
			,SMTP_CMD_SEND
			,SMTP_CMD_SOML
			,SMTP_CMD_SAML

	} cmd = SMTP_CMD_NONE;

	thread_up(TRUE /* setuid */);

	free(arg);

	socket=smtp.socket;

	lprintf("%04d SMTP RX Session thread started", socket);

#ifdef _WIN32
	if(startup->inbound_sound[0] && !(startup->options&MAIL_OPT_MUTE)) 
		PlaySound(startup->inbound_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

	addr_len=sizeof(server_addr);
	if((i=getsockname(socket, (struct sockaddr *)&server_addr,&addr_len))!=0) {
		lprintf("%04d !SMTP ERROR %d (%d) getting address/port"
			,socket, i, ERROR_VALUE);
		sockprintf(socket,sys_error);
		mail_close_socket(socket);
		thread_down();
		return;
	} 

	memset(&smb,0,sizeof(smb));
	memset(&msg,0,sizeof(msg));
	memset(&user,0,sizeof(user));
	memset(&relay_user,0,sizeof(relay_user));

	SAFECOPY(host_ip,inet_ntoa(smtp.client_addr.sin_addr));

	lprintf("%04d SMTP connection accepted from: %s port %u"
		, socket, host_ip, ntohs(smtp.client_addr.sin_port));

	if(startup->options&MAIL_OPT_NO_HOST_LOOKUP)
		host=NULL;
	else
		host=gethostbyaddr ((char *)&smtp.client_addr.sin_addr
			,sizeof(smtp.client_addr.sin_addr),AF_INET);

	if(host!=NULL && host->h_name!=NULL)
		SAFECOPY(host_name,host->h_name);
	else
		strcpy(host_name,"<no name>");

	if(!(startup->options&MAIL_OPT_NO_HOST_LOOKUP)) {
		lprintf("%04d SMTP Hostname: %s", socket, host_name);
		for(i=0;host!=NULL && host->h_aliases!=NULL && host->h_aliases[i]!=NULL;i++)
			lprintf("%04d SMTP HostAlias: %s", socket, host->h_aliases[i]);
	}

	SAFECOPY(hello_name,host_name);

	if(trashcan(&scfg,host_ip,"ip")) {
		lprintf("%04d !SMTP BLOCKED SERVER IP ADDRESS: %s"
			,socket, host_ip);
		sockprintf(socket,"550 Access denied.");
		mail_close_socket(socket);
		thread_down();
		return;
	}

	if(trashcan(&scfg,host_name,"host")) {
		lprintf("%04d !SMTP BLOCKED SERVER HOSTNAME: %s"
			,socket, host_name);
		sockprintf(socket,"550 Access denied.");
		mail_close_socket(socket);
		thread_down();
		return;
	}

	active_clients++,update_clients();

	/*  SPAM Filters (mail-abuse.org) */
	dnsbl_result.s_addr = dns_blacklisted(socket,smtp.client_addr.sin_addr,host_name,dnsbl);
	if(dnsbl_result.s_addr) {
		lprintf("%04d !SMTP BLACKLISTED SERVER on %s: %s [%s] = %s"
			,socket, dnsbl, host_name, host_ip, inet_ntoa(dnsbl_result));
		if(startup->options&MAIL_OPT_DNSBL_REFUSE) {
			sprintf(str,"Listed on %s as %s", dnsbl, inet_ntoa(dnsbl_result));
			spamlog(&scfg, "SMTP", "SESSION REFUSED", str, host_name, host_ip, NULL, NULL);
			sockprintf(socket
				,"550 Mail from %s refused due to listing at %s"
				,host_ip, dnsbl);
			mail_close_socket(socket);
			lprintf("%04d !SMTP REFUSED SESSION from blacklisted server"
				,socket);
			thread_down();
			active_clients--,update_clients();
			return;
		}
	}

	sprintf(session_id,"%d.%lx",socket,clock()&0xffff);

	sprintf(rcptlst_fname,"%sSMTP.%s.lst", scfg.data_dir, session_id);
	rcptlst=fopen(rcptlst_fname,"w+");
	if(rcptlst==NULL) {
		lprintf("%04d !SMTP ERROR %d creating recipient list: %s"
			,socket, errno, rcptlst_fname);
		sockprintf(socket,sys_error);
		mail_close_socket(socket);
		thread_down();
		active_clients--,update_clients();
		return;
	}

	if(trashcan(&scfg,host_name,"smtpspy") 
		|| trashcan(&scfg,host_ip,"smtpspy")) {
		sprintf(str,"%sSMTPSPY.TXT", scfg.data_dir);
		spy=fopen(str,"a");
	}

	/* Initialize client display */
	client.size=sizeof(client);
	client.time=time(NULL);
	SAFECOPY(client.addr,host_ip);
	SAFECOPY(client.host,host_name);
	client.port=ntohs(smtp.client_addr.sin_port);
	client.protocol="SMTP";
	client.user="<unknown>";
	client_on(socket,&client,FALSE /* update */);

	sprintf(str,"SMTP: %s",host_ip);
	status(str);

	sockprintf(socket,"220 %s Synchronet SMTP Server %s-%s Ready"
		,startup->host_name,revision,PLATFORM_DESC);
	while(1) {
		rd = sockreadline(socket, buf, sizeof(buf));
		if(rd<1) 
			break;
		truncsp(buf);
		if(spy!=NULL)
			fprintf(spy,"%s\n",buf);
		if(state>=SMTP_STATE_DATA_HEADER) {
			if(!strcmp(buf,".")) {

				state=SMTP_STATE_HELO;	/* RESET state machine here in case of error */
				cmd=SMTP_CMD_NONE;

				if(msgtxt==NULL) {
					lprintf("%04d !SMTP NO MESSAGE TEXT FILE POINTER?", socket);
					sockprintf(socket,"554 No message text");
					continue;
				}

				if(ftell(msgtxt)<1) {
					lprintf("%04d !SMTP INVALID MESSAGE LENGTH: %ld (%lu lines)"
						, socket, ftell(msgtxt), lines);
					sockprintf(socket,"554 No message text");
					continue;
				}

				lprintf("%04d SMTP End of message (body: %lu lines, %lu bytes, header: %lu lines, %lu bytes)"
					, socket, lines, ftell(msgtxt)-hdr_len, hdr_lines, hdr_len);

				if(telegram==TRUE) {		/* Telegram */
					const char* head="\1n\1h\1cInstant Message\1n from \1h\1y";
					const char* tail="\1n:\r\n\1h";
					rewind(msgtxt);
					length=filelength(fileno(msgtxt));
					
					p=strchr(sender_addr,'@');
					if(p==NULL || resolve_ip(p+1)!=smtp.client_addr.sin_addr.s_addr) 
						/* Append real IP and hostname if different */
						sprintf(str,"%s%s\r\n\1w[\1n%s\1h] (\1n%s\1h)%s"
							,head,sender_addr,host_ip,host_name,tail);
					else
						sprintf(str,"%s%s%s",head,sender_addr,tail);
					
					if((telegram_buf=(char*)malloc(length+strlen(str)+1))==NULL) {
						lprintf("%04d !SMTP ERROR allocating %lu bytes of memory for telegram from %s"
							,socket,length+strlen(str)+1,sender_addr);
						sockprintf(socket, "452 Insufficient system storage");
						continue; 
					}
					strcpy(telegram_buf,str);	/* can't use SAFECOPY here */
					if(fread(telegram_buf+strlen(str),1,length,msgtxt)!=length) {
						lprintf("%04d !SMTP ERROR reading %lu bytes from telegram file"
							,socket,length);
						sockprintf(socket, "452 Insufficient system storage");
						free(telegram_buf);
						continue; 
					}
					telegram_buf[length+strlen(str)]=0;	/* Need ASCIIZ */

					/* Send telegram to users */
					rewind(rcptlst);
					rcpt_count=0;
					while(!feof(rcptlst)  && rcpt_count<startup->max_recipients) {
						if(fgets(str,sizeof(str),rcptlst)==NULL)
							break;
						usernum=atoi(str);
						if(fgets(rcpt_name,sizeof(rcpt_name),rcptlst)==NULL)
							break;
						truncsp(rcpt_name);
						if(fgets(rcpt_addr,sizeof(rcpt_addr),rcptlst)==NULL)
							break;
						truncsp(rcpt_addr);
						putsmsg(&scfg,usernum,telegram_buf);
						lprintf("%04d SMTP Created telegram (%ld/%u bytes) from %s to %s <%s>"
							,socket, length, strlen(telegram_buf), sender_addr, rcpt_name, rcpt_addr);
						rcpt_count++;
					}
					free(telegram_buf);
					sockprintf(socket,ok_rsp);
					telegram=FALSE;
					continue;
				}

				fclose(msgtxt);
				fclose(rcptlst);

				/* External Mail Processing here */
				if(startup->proc_cfg_file[0] 
					&& (proc_cfg=fopen(startup->proc_cfg_file,"r"))!=NULL) {
					sprintf(proc_err_fname,"%sSMTP.%s.err", scfg.data_dir, session_id);
					remove(proc_err_fname);

					while(!feof(proc_cfg)) {
						if(!fgets(tmp,sizeof(tmp),proc_cfg))
							break;
						truncsp(tmp);
						p=tmp;
						while(*p && *p<=' ') p++;
						if(*p==';' || *p==0)	/* comment or blank line */
							continue;
						lprintf("%04d SMTP executing external process: %s", socket, p);
						system(cmdstr(p, msgtxt_fname, rcptlst_fname, proc_err_fname, str));
						if(flength(proc_err_fname)>0)
							break;
						if(!fexist(msgtxt_fname) || !fexist(rcptlst_fname))
							break;
					}
					fclose(proc_cfg);
					if(flength(proc_err_fname)>0 
						&& (proc_err=fopen(proc_err_fname,"r"))!=NULL) {
						while(!feof(proc_err)) {
							if(!fgets(str,sizeof(str),proc_err))
								break;
							truncsp(str);
							lprintf("%04d !SMTP external process error: %s", socket, str);
							i=atoi(str);
							if(i>=100 && i<1000)
								sockprintf(socket,"%s", str);
							else
								sockprintf(socket,"554%c%s"
									,ftell(proc_err)<filelength(fileno(proc_err)) ? '-' : ' '
									,str);
						}
						fclose(proc_err);
						remove(proc_err_fname);
						continue;
					}
					if(!fexist(msgtxt_fname) || !fexist(rcptlst_fname)) {
						lprintf("%04d SMTP external process removed %s"
							,socket, fexist(msgtxt_fname)==FALSE ? "message text" : "recipient list");
						sockprintf(socket,ok_rsp);
						continue;
					}
				}

				/* Re-open files */
				if((rcptlst=fopen(rcptlst_fname,"r"))==NULL) {
					lprintf("%04d !SMTP ERROR %d re-opening recipient list: %s"
						,socket, errno, rcptlst_fname);
					sockprintf(socket,sys_error);
					continue;
				}
			
				if((msgtxt=fopen(msgtxt_fname,"rb"))==NULL) {
					lprintf("%04d !SMTP ERROR %d re-opening message file: %s"
						,socket, errno, msgtxt_fname);
					sockprintf(socket,sys_error);
					continue;
				}

				/* Initialize message header */
				smb_freemsgmem(&msg);
				memset(&msg,0,sizeof(smbmsg_t));		

				/* Parse message header here */
				hfield_type=UNKNOWN;
				smb_error=0; /* no SMB error */
				while(!feof(msgtxt)) {
					if(!fgets(buf,sizeof(buf),msgtxt))
						break;
					truncsp(buf);
					if(buf[0]==0)	/* blank line marks end of header */
						break;

					if(!strnicmp(buf, "SUBJECT:",8)) {
						p=buf+8;
						while(*p && *p<=' ') p++;
						if(dnsbl_result.s_addr && startup->dnsbl_tag[0]
							&& !(startup->options&MAIL_OPT_DNSBL_IGNORE)) {
							sprintf(str,"%.*s: %.*s"
								,(int)sizeof(str)/2, startup->dnsbl_tag
								,(int)sizeof(str)/2, p);
							p=str;
							lprintf("%04d !SMTP TAGGED MAIL SUBJECT from blacklisted server with: %s"
								,socket, startup->dnsbl_tag);
						}
						smb_hfield(&msg, SUBJECT, (ushort)strlen(p), p);
						msg.idx.subj=subject_crc(p);
						continue;
					}
					if(!strnicmp(buf, "FROM:", 5)
						&& !chk_email_addr(socket,buf+5,host_name,host_ip,rcpt_addr,reverse_path))
						break;
					if((smb_error=parse_header_field(buf,&msg,&hfield_type))!=0) {
						if(smb_error==SMB_ERR_HDR_LEN)
							lprintf("%04d !SMTP MESSAGE HEADER EXCEEDS %u BYTES"
								,socket, SMB_MAX_HDR_LEN);
						else
							lprintf("%04d !SMTP ERROR %d adding header field: %s"
								,socket, smb_error, buf);
						break;
					}
				}
				if(smb_error!=0) {	/* SMB Error */
					sockprintf(socket, "452 Insufficient system storage");
					continue;
				}
				if((p=smb_get_hfield(&msg, RFC822TO, NULL))!=NULL) {
					if(*p=='<')	p++;
					SAFECOPY(rcpt_name,p);
					truncstr(rcpt_name,">");
				}
				if((p=smb_get_hfield(&msg, RFC822FROM, NULL))!=NULL) {
					/* Get the sender's address */
					if((tp=strchr(p,'<'))!=NULL)
						tp++;
					else
						tp=p;
					while(*tp && *tp<=' ') tp++;
					SAFECOPY(sender_addr,tp);
					truncstr(sender_addr,">( ");

					SAFECOPY(tmp,p);
					p=tmp;
					/* Get the sender's "name" (if possible) */
					if((tp=strchr(p,'('))!=NULL) {			/* name in parenthesis? */
						p=tp+1;
						tp=strchr(p,')');
					} else if((tp=strchr(p,'"'))!=NULL) {	/* name in quotes? */
						p=tp+1;
						tp=strchr(p,'"');
					} else if(*p=='<') {					/* address in brackets? */
						p++;
						tp=strchr(p,'>');
					} else									/* name, then address in brackets */
						tp=strchr(p,'<');
					if(tp) *tp=0;
					SAFECOPY(sender,p);
					truncsp(sender);
				}

				/* SPAM Filtering/Logging */
				if(msg.subj!=NULL && trashcan(&scfg,msg.subj,"subject")) {
					lprintf("%04d !SMTP BLOCKED SUBJECT (%s) from: %s"
						,socket, msg.subj, reverse_path);
					sprintf(tmp,"Blocked subject (%s) from: %s"
						,msg.subj, reverse_path);
					spamlog(&scfg, "SMTP", "REFUSED"
						,tmp, host_name, host_ip, rcpt_addr, reverse_path);
					sockprintf(socket, "554 Subject not allowed.");
					continue;
				}
				if(dnsbl_result.s_addr) {
					if(startup->options&MAIL_OPT_DNSBL_IGNORE) {
						lprintf("%04d !SMTP IGNORED MAIL from blacklisted server"
							,socket);
						sprintf(str,"Listed on %s as %s", dnsbl, inet_ntoa(dnsbl_result));
						spamlog(&scfg, "SMTP", "IGNORED"
							,str, host_name, host_ip, rcpt_addr, reverse_path);
						/* pretend we received it */
						sockprintf(socket,ok_rsp);
						continue;
					}
					/* tag message as spam */
					if(startup->dnsbl_hdr[0]) {
						sprintf(str,"%s: %s is listed on %s as %s"
							,startup->dnsbl_hdr, host_ip
							,dnsbl, inet_ntoa(dnsbl_result));
						smb_hfield(&msg,RFC822HEADER,strlen(str),str);
						lprintf("%04d !SMTP TAGGED MAIL HEADER from blacklisted server with: %s"
							,socket, startup->dnsbl_hdr);
					}
					if(startup->dnsbl_hdr[0] || startup->dnsbl_tag[0]) {
						sprintf(str,"Listed on %s as %s", dnsbl, inet_ntoa(dnsbl_result));
						spamlog(&scfg, "SMTP", "TAGGED", str, host_name, host_ip, rcpt_addr, reverse_path);
					}
				}

				if(sender[0]==0) {
					lprintf("%04d !SMTP MISSING mail header 'FROM' field", socket);
					sockprintf(socket, "554 Mail header missing 'FROM' field");
					subnum=INVALID_SUB;
					continue;
				}
				nettype=NET_INTERNET;
				smb_hfield(&msg, SMTPREVERSEPATH, (ushort)strlen(reverse_path), reverse_path);
				smb_hfield(&msg, SENDER, (ushort)strlen(sender), sender);
				smb_hfield(&msg, SENDERNETTYPE, sizeof(nettype), &nettype);
				smb_hfield(&msg, SENDERNETADDR, (ushort)strlen(sender_addr), sender_addr);
				if(msg.idx.subj==0) {
					p="";
					smb_hfield(&msg, SUBJECT, 0, p);
					msg.idx.subj=subject_crc(p);
				}

				length=filelength(fileno(msgtxt))-ftell(msgtxt);

				if(startup->max_msg_size && length>startup->max_msg_size) {
					lprintf("%04d !SMTP message size (%lu) exceeds maximum: %lu bytes"
						,socket,length,startup->max_msg_size);
					sockprintf(socket, "552 Message size (%lu) exceeds maximum: %lu bytes"
						,length,startup->max_msg_size);
					subnum=INVALID_SUB;
					continue;
				}

				if((msgbuf=(char*)malloc(length+1))==NULL) {
					lprintf("%04d !SMTP ERROR allocating %d bytes of memory"
						,socket,length+1);
					sockprintf(socket, "452 Insufficient system storage");
					subnum=INVALID_SUB;
					continue;
				}
				fread(msgbuf,length,1,msgtxt);
				msgbuf[length]=0;	/* ASCIIZ */

				/* Do external JavaScript processing here? */

				if(subnum!=INVALID_SUB) {	/* Message Base */
					if(rcpt_name[0]==0)
						strcpy(rcpt_name,"All");
					smb_hfield(&msg, RECIPIENT, (ushort)strlen(rcpt_name), rcpt_name);

					smb.subnum=subnum;
					if((i=savemsg(&scfg, &smb, &msg, msgbuf))!=0) {
						lprintf("%04d !SMTP ERROR %d (%s) saving message"
							,socket,i,smb.last_error);
						sockprintf(socket, "452 ERROR %d (%s) saving message"
							,i,smb.last_error);
					} else {
						lprintf("%04d SMTP %s posted a message on %s"
							,socket, sender_addr, scfg.sub[subnum]->sname);
						sockprintf(socket,ok_rsp);
						signal_smtp_sem();
					}
					free(msgbuf);
					smb_close(&smb);
					subnum=INVALID_SUB;
					continue;
				}

				/* E-mail */
#if 0	/* old way */
				sprintf(smb.file,"%smail", scfg.data_dir);
				smb.retry_time=scfg.smb_retry_time;
				smb.subnum=INVALID_SUB;
				if((i=smb_open(&smb))!=0) {
					lprintf("%04d !SMTP ERROR %d (%s) opening %s"
						,socket, i, smb.last_error, smb.file);
					sockprintf(socket, "452 Insufficient system storage");
					continue;
				}

				if(smb_fgetlength(smb.shd_fp)<1) {	 /* Create it if it doesn't exist */
					smb.status.max_crcs=scfg.mail_maxcrcs;
					smb.status.max_age=scfg.mail_maxage;
					smb.status.max_msgs=MAX_SYSMAIL;
					smb.status.attr=SMB_EMAIL;
					if((i=smb_create(&smb))!=0) {
						smb_close(&smb);
						lprintf("%04d !SMTP ERROR %d (%s) creating %s"
							,socket, i, smb.last_error, smb.file);
						sockprintf(socket, "452 Insufficient system storage");
						continue;
					} 
				}

				if((i=smb_locksmbhdr(&smb))!=0) {
					smb_close(&smb);
					lprintf("%04d !SMTP ERROR %d (%s) locking %s"
						,socket, i, smb.last_error, smb.file);
					sockprintf(socket, "452 Insufficient system storage");
					continue; 
				}

				length+=sizeof(xlat);	 /* +2 for translation string */

				if((i=smb_open_da(&smb))!=0) {
					smb_unlocksmbhdr(&smb);
					smb_close(&smb);
					lprintf("%04d !SMTP ERROR %d (%s) opening %s.sda"
						,socket, i, smb.last_error, smb.file);
					sockprintf(socket, "452 Insufficient system storage");
					continue; 
				}

				if(scfg.sys_misc&SM_FASTMAIL)
					offset=smb_fallocdat(&smb,length,1);
				else
					offset=smb_allocdat(&smb,length,1);

				smb_fseek(smb.sdt_fp,offset,SEEK_SET);
				xlat=XLAT_NONE;
				smb_fwrite(&xlat,2,smb.sdt_fp);
				x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
				while(!feof(msgtxt)) {
					memset(buf,0,x);
					j=fread(buf,1,x,msgtxt);
					if(j<1)
						break;
					if(j>1 && (j!=x || feof(msgtxt)) && buf[j-1]=='\n' && buf[j-2]=='\r')
						buf[j-1]=buf[j-2]=0;
					if(scfg.mail_maxcrcs) {
						for(i=0;i<j;i++)
							crc=ucrc32(buf[i],crc); 
					}
					smb_fwrite(buf,j,smb.sdt_fp);
					x=SDT_BLOCK_LEN; 
				}
				smb_fflush(smb.sdt_fp);
				crc=~crc;

				if(scfg.mail_maxcrcs) {
					i=smb_addcrc(&smb,crc);
					if(i) {
						smb_freemsgdat(&smb,offset,length,1);
						smb_unlocksmbhdr(&smb);
						smb_close_da(&smb);
						smb_close(&smb);
						lprintf("%04d !SMTP ERROR %d ADDING MESSAGE: %s"
							, socket, i, smb.last_error);
						sockprintf(socket, "554 Duplicate Message");
						continue; 
					} 
				}

				msg.hdr.offset=offset;

				smb_dfield(&msg,TEXT_BODY,length);

				smb_unlocksmbhdr(&smb);

#else	/* new way */

				smb.subnum=INVALID_SUB;
				i=savemsg(&scfg, &smb, &msg, msgbuf);
				free(msgbuf);
				if(i!=0) {
					lprintf("%04d !SMTP ERROR %d (%s) saving message"
						,socket,i,smb.last_error);
					sockprintf(socket, "452 ERROR %d (%s) saving message"
						,i,smb.last_error);
					continue;
				}
#endif

				rcpt_count=0;
				while(!feof(rcptlst) && rcpt_count<startup->max_recipients) {
					if((i=smb_copymsgmem(&newmsg,&msg))!=0) {
						lprintf("%04d !SMTP ERROR %d (%s) copying message"
							,socket, i, smb.last_error);
						break;
					}
					if(fgets(str,sizeof(str),rcptlst)==NULL)
						break;
					usernum=atoi(str);
					if(fgets(rcpt_name,sizeof(rcpt_name),rcptlst)==NULL)
						break;
					truncsp(rcpt_name);
					if(fgets(rcpt_addr,sizeof(rcpt_addr),rcptlst)==NULL)
						break;
					truncsp(rcpt_addr);

					snprintf(hdrfield,sizeof(hdrfield),
						"Received: from %s (%s [%s])\r\n"
						"          by %s [%s] (Synchronet Mail Server %s-%s) with %s\r\n"
						"          for %s; %s"
						,host_name,hello_name,host_ip
						,startup->host_name,inet_ntoa(server_addr.sin_addr)
						,revision,PLATFORM_DESC
						,esmtp ? "ESMTP" : "SMTP"
						,rcpt_name,msgdate(msg.hdr.when_imported,date));
					smb_hfield(&newmsg, RFC822HEADER, (ushort)strlen(hdrfield), hdrfield);

					smb_hfield(&newmsg, RECIPIENT, (ushort)strlen(rcpt_name), rcpt_name);

					if(rcpt_addr[0]=='#') {	/* Local destination */
						newmsg.idx.to=atoi(rcpt_addr+1);
						smb_hfield(&newmsg, RECIPIENTEXT
							,(ushort)strlen(rcpt_addr+1), rcpt_addr+1);
					} else {
						newmsg.idx.to=0;
						nettype=NET_INTERNET;
						smb_hfield(&newmsg, RECIPIENTNETTYPE, sizeof(nettype), &nettype);
						smb_hfield(&newmsg, RECIPIENTNETADDR
							,(ushort)strlen(rcpt_addr), rcpt_addr);
					}

					i=smb_addmsghdr(&smb,&newmsg,SMB_SELFPACK);
					smb_freemsgmem(&newmsg);
					if(i!=0) {
						lprintf("%04d !SMTP ERROR %d (%s) adding message header"
							,socket, i, smb.last_error);
						break;
					}
					lprintf("%04d SMTP Created message #%ld from %s to %s <%s>"
						,socket, newmsg.hdr.number, sender, rcpt_name, rcpt_addr);
					if(usernum) {
						sprintf(str,"\7\1n\1hOn %.24s\r\n\1m%s \1n\1msent you e-mail from: "
							"\1h%s\1n\r\n"
							,timestr(&scfg,(time_t*)&newmsg.hdr.when_imported.time,tmp)
							,sender,sender_addr);
						if(!newmsg.idx.to) {	/* Forwarding */
							strcat(str,"\1mand it was automatically forwaded to: \1h");
							strcat(str,user.netmail);
							strcat(str,"\1n\r\n");
						}
						putsmsg(&scfg, usernum, str);
					}
					rcpt_count++;
				}
				if(rcpt_count<1) {
					smb_freemsgdat(&smb,msg.hdr.offset,length,0);
					sockprintf(socket, "452 Insufficient system storage");
				}
				else {
					if(rcpt_count>1)
						smb_incdat(&smb,msg.hdr.offset,length,(ushort)(rcpt_count-1));
					sockprintf(socket,ok_rsp);
					signal_smtp_sem();
				}
				smb_close_da(&smb);
				smb_close(&smb);
				continue;
			}
			if(buf[0]==0 && state==SMTP_STATE_DATA_HEADER) {	
				state=SMTP_STATE_DATA_BODY;	/* Null line separates header and body */
				lines=0;
				if(msgtxt!=NULL) {
					fprintf(msgtxt, "\r\n");
					hdr_len=ftell(msgtxt);
				}
				continue;
			}
			if(state==SMTP_STATE_DATA_BODY) {
				p=buf;
				if(*p=='.') p++;	/* Transparency (RFC821 4.5.2) */
				if(msgtxt!=NULL) 
					fprintf(msgtxt, "%s\r\n", p);
				lines++;
				/* release time-slices every x lines */
				if(startup->lines_per_yield &&
					!(lines%startup->lines_per_yield))	
					mswait(1);
				continue;
			}
			/* RFC822 Header parsing */
			if(startup->options&MAIL_OPT_DEBUG_RX_HEADER)
				lprintf("%04d SMTP %s",socket, buf);

			if(msgtxt!=NULL) 
				fprintf(msgtxt, "%s\r\n", buf);
			hdr_lines++;

#if 0 /* moved */
			if(!strnicmp(buf, "SUBJECT:",8)) {
				p=buf+8;
				while(*p && *p<=' ') p++;
				if(dnsbl_result.s_addr && startup->dnsbl_tag[0]
					&& !(startup->options&MAIL_OPT_DNSBL_IGNORE)) {
					sprintf(str,"%.*s: %.*s"
						,(int)sizeof(str)/2, startup->dnsbl_tag
						,(int)sizeof(str)/2, p);
					p=str;
					lprintf("%04d !SMTP TAGGED MAIL SUBJECT from blacklisted server with: %s"
						,socket, startup->dnsbl_tag);
				}
				smb_hfield(&msg, SUBJECT, (ushort)strlen(p), p);
				msg.idx.subj=subject_crc(p);
				continue;
			}
			if(!strnicmp(buf, "FROM:", 5)
				&& !chk_email_addr(socket,buf+5,host_name,host_ip,rcpt_addr,reverse_path))
				break;
			parse_header_field(buf,&msg);
#endif
			continue;
		}
		lprintf("%04d SMTP RX: %s", socket, buf);
		if(!strnicmp(buf,"HELO",4)) {
			p=buf+4;
			while(*p && *p<=' ') p++;
			SAFECOPY(hello_name,p);
			sockprintf(socket,"250 %s",startup->host_name);
			esmtp=FALSE;
			state=SMTP_STATE_HELO;
			cmd=SMTP_CMD_NONE;
			telegram=FALSE;
			subnum=INVALID_SUB;
			continue;
		}
		if(!strnicmp(buf,"EHLO",4)) {
			p=buf+4;
			while(*p && *p<=' ') p++;
			SAFECOPY(hello_name,p);
			sockprintf(socket,"250-%s",startup->host_name);
			sockprintf(socket,"250 AUTH LOGIN");
			esmtp=TRUE;
			state=SMTP_STATE_HELO;
			cmd=SMTP_CMD_NONE;
			telegram=FALSE;
			subnum=INVALID_SUB;
			continue;
		}
		/* This is a stupid protocol, but it's the only one Outlook Express supports */
		if(!stricmp(buf,"AUTH LOGIN")) {	
			sockprintf(socket,"334 VXNlcm5hbWU6");	/* Base64-encoded "Username:" */
			rd = sockreadline(socket, buf, sizeof(buf));
			if(rd<1) {
				sockprintf(socket,badauth_rsp);
				continue;
			}
			b64_decode(user_name,sizeof(user_name),buf,rd);
			sockprintf(socket,"334 UGFzc3dvcmQ6");	/* Base64-encoded "Password:" */
			rd = sockreadline(socket, buf, sizeof(buf));
			if(rd<1) {
				sockprintf(socket,badauth_rsp);
				continue;
			}
			b64_decode(user_pass,sizeof(user_pass),buf,rd);
			if((relay_user.number=matchuser(&scfg,user_name,FALSE))==0) {
				if(scfg.sys_misc&SM_ECHO_PW)
					lprintf("%04d !SMTP UNKNOWN USER: %s (password: %s)"
						,socket, user_name, user_pass);
				else
					lprintf("%04d !SMTP UNKNOWN USER: %s"
						,socket, user_name);
				sockprintf(socket,badauth_rsp);
				continue;
			}
			if((i=getuserdat(&scfg, &relay_user))!=0) {
				lprintf("%04d !SMTP ERROR %d getting data on user (%s)"
					,socket, i, user_name);
				sockprintf(socket,badauth_rsp);
				continue;
			}
			if(relay_user.misc&(DELETED|INACTIVE)) {
				lprintf("%04d !SMTP DELETED or INACTIVE user #%u (%s)"
					,socket, relay_user.number, user_name);
				sockprintf(socket,badauth_rsp);
				break;
			}
			if(stricmp(user_pass,relay_user.pass)) {
				if(scfg.sys_misc&SM_ECHO_PW)
					lprintf("%04d !SMTP FAILED Password attempt for user %s: '%s' expected '%s'"
						,socket, user_name, user_pass, relay_user.pass);
				else
					lprintf("%04d !SMTP FAILED Password attempt for user %s"
						,socket, user_name);
				sockprintf(socket,badauth_rsp);
				break;
			}
			lprintf("%04d SMTP %s authenticated using LOGIN protocol"
				,socket,relay_user.alias);
			sockprintf(socket,"235 User Authenticated");
			continue;
		}
		if(!stricmp(buf,"QUIT")) {
			sockprintf(socket,"221 %s Service closing transmission channel",startup->host_name);
			break;
		} 
		if(!stricmp(buf,"NOOP")) {
			sockprintf(socket, ok_rsp);
			badcmds=0;
			continue;
		}
		if(state<SMTP_STATE_HELO) {
			/* RFC 821 4.1.1 "The first command in a session must be the HELO command." */
			lprintf("%04d !SMTP MISSING 'HELO' command",socket);
			sockprintf(socket, badseq_rsp);
			continue;
		}
		if(!stricmp(buf,"TURN")) {
			sockprintf(socket,"502 command not supported");
			badcmds=0;
			continue;
		}
		if(!stricmp(buf,"RSET")) {
			smb_freemsgmem(&msg);
			memset(&msg,0,sizeof(smbmsg_t));		/* Initialize message header */
			reverse_path[0]=0;
			state=SMTP_STATE_HELO;
			cmd=SMTP_CMD_NONE;
			telegram=FALSE;
			subnum=INVALID_SUB;

			/* reset recipient list */
			if((rcptlst=freopen(rcptlst_fname,"w+",rcptlst))==NULL) {
				lprintf("%04d !SMTP ERROR %d re-opening %s"
					,socket, errno, rcptlst_fname);
				sockprintf(socket,sys_error);
				break;
			}
			rcpt_count=0;

			sockprintf(socket,ok_rsp);
			badcmds=0;
			lprintf("%04d SMTP session reset",socket);
			continue;
		}
		if(!strnicmp(buf,"MAIL FROM:",10)
			|| !strnicmp(buf,"SEND FROM:",10)	/* Send a Message (Telegram) to a local ONLINE user */
			|| !strnicmp(buf,"SOML FROM:",10)	/* Send OR Mail a Message to a local user */
			|| !strnicmp(buf,"SAML FROM:",10)	/* Send AND Mail a Message to a local user */
			) {
			p=buf+10;
			if(!chk_email_addr(socket,p,host_name,host_ip,NULL,NULL))
				break;
			while(*p && *p<=' ') p++;
			SAFECOPY(reverse_path,p);

			/* Update client display */
			client.user=reverse_path;
			client_on(socket,&client,TRUE /* update */);

			/* Setup state */
			state=SMTP_STATE_MAIL_FROM;
			if(!strnicmp(buf,"MAIL FROM:",10))
				cmd=SMTP_CMD_MAIL;
			else if(!strnicmp(buf,"SEND FROM:",10))
				cmd=SMTP_CMD_SEND;
			else if(!strnicmp(buf,"SOML FROM:",10))
				cmd=SMTP_CMD_SOML;
			else if(!strnicmp(buf,"SAML FROM:",10))
				cmd=SMTP_CMD_SAML;

			/* reset recipient list */
			/* reset recipient list */
			if((rcptlst=freopen(rcptlst_fname,"w+",rcptlst))==NULL) {
				lprintf("%04d !SMTP ERROR %d re-opening %s"
					,socket, errno, rcptlst_fname);
				sockprintf(socket,sys_error);
				break;
			}
			rcpt_count=0;

#if 0 /* moved */
			/* Initialize message header */
			smb_freemsgmem(&msg);
			memset(&msg,0,sizeof(smbmsg_t));		
			msg.hdr.version=smb_ver();
			msg.hdr.when_imported.time=time(NULL);
			msg.hdr.when_imported.zone=scfg.sys_timezone;
#endif
			sockprintf(socket,ok_rsp);
			badcmds=0;
			continue;
		}

#if 0	/* No one uses this command */
		if(!strnicmp(buf,"VRFY",4)) {
			p=buf+4;
			while(*p && *p<=' ') p++;
			if(*p==0) {
				sockprintf(socket,"550 No user specified.");
				continue;
			}
#endif

		/* Add to Recipient list */
		if(!strnicmp(buf,"RCPT TO:",8)) {

			if(state<SMTP_STATE_MAIL_FROM) {
				lprintf("%04d !SMTP MISSING 'MAIL' command",socket);
				sockprintf(socket, badseq_rsp);
				continue;
			}

			if(spy==NULL && trashcan(&scfg,reverse_path,"smtpspy")) {
				sprintf(str,"%sSMTPSPY.TXT", scfg.data_dir);
				spy=fopen(str,"a");
			}

			p=buf+8;
			while(*p && *p<=' ') p++;
			SAFECOPY(str,p);
			p=strrchr(str,'<');
			if(p==NULL)
				p=str;
			else
				p++;

			truncstr(str,">");	/* was truncating at space too */

			forward=FALSE;
			no_forward=FALSE;
			if(!strnicmp(p,FORWARD,strlen(FORWARD))) {
				forward=TRUE;		/* force forward to user's netmail address */
				p+=strlen(FORWARD);
			}
			if(!strnicmp(p,NO_FORWARD,strlen(NO_FORWARD))) {
				no_forward=TRUE;	/* do not forward to user's netmail address */
				p+=strlen(NO_FORWARD);
			}

			rcpt_name[0]=0;
			SAFECOPY(rcpt_addr,p);

			/* Check recipient counter */
			if(rcpt_count>=startup->max_recipients) {
				lprintf("%04d !SMTP MAXIMUM RECIPIENTS (%d) REACHED"
					,socket, startup->max_recipients);
				sprintf(tmp,"Maximum recipient count (%d)",startup->max_recipients);
				spamlog(&scfg, "SMTP", "REFUSED", tmp
					,host_name, host_ip, rcpt_addr, reverse_path);
				sockprintf(socket, "552 Too many recipients");
				continue;
			}

			/* Check for blocked recipients */
			if(trashcan(&scfg,rcpt_addr,"email")) {
				lprintf("%04d !SMTP BLOCKED RECIPIENT (%s) from: %s"
					,socket, rcpt_addr, reverse_path);
				spamlog(&scfg, "SMTP", "REFUSED", "Blocked recipient e-mail address"
					,host_name, host_ip, rcpt_addr, reverse_path);
				sockprintf(socket, "550 Unknown User:%s", buf+8);
				continue;
			}

			if(dnsbl_result.s_addr && startup->options&MAIL_OPT_DNSBL_BADUSER) {
				lprintf("%04d !SMTP REFUSED MAIL from blacklisted server"
					,socket);
				sprintf(str,"Listed on %s as %s", dnsbl, inet_ntoa(dnsbl_result));
				spamlog(&scfg, "SMTP", "REFUSED", str, host_name, host_ip, rcpt_addr, reverse_path);
				sockprintf(socket
					,"550 Mail from %s refused due to listing at %s"
					,host_ip, dnsbl);
				continue;
			}

			/* Check for full address aliases */
			p=alias(&scfg,p,alias_buf);
			if(p==alias_buf) 
				lprintf("%04d SMTP ADDRESS ALIAS: %s",socket,p);

			tp=strrchr(p,'@');
			if(cmd==SMTP_CMD_MAIL && tp!=NULL) {
				
				/* RELAY */
				dest_port=server_addr.sin_port;
				SAFECOPY(dest_host,tp+1);
				cp=strrchr(dest_host,':');
				if(cp!=NULL) {
					*cp=0;
					dest_port=atoi(cp+1);
				}
				sprintf(domain_list,"%sdomains.cfg",scfg.ctrl_dir);
				if((stricmp(dest_host,scfg.sys_inetaddr)!=0
						&& stricmp(dest_host,startup->host_name)!=0
						&& resolve_ip(dest_host)!=server_addr.sin_addr.s_addr
						&& findstr(dest_host,domain_list)==FALSE)
					|| dest_port!=server_addr.sin_port) {

					sprintf(relay_list,"%srelay.cfg",scfg.ctrl_dir);
					if(relay_user.number==0)
						relay_user.number=userdatdupe(&scfg, 0, U_NOTE, LEN_NOTE, host_ip, FALSE);
					if(relay_user.number!=0)
						getuserdat(&scfg,&relay_user);
					if(p!=alias_buf /* forced relay by alias */ &&
						(!(startup->options&MAIL_OPT_ALLOW_RELAY)
							|| relay_user.number==0
							|| relay_user.laston < time(NULL)-(60*60)
							|| relay_user.rest&(FLAG('G')|FLAG('M'))) &&
						!findstr(host_name,relay_list) && 
						!findstr(host_ip,relay_list)) {
						lprintf("%04d !SMTP ILLEGAL RELAY ATTEMPT from %s [%s] to %s"
							,socket, reverse_path, host_ip, p);
						sprintf(tmp,"Relay attempt to: %s", p);
						spamlog(&scfg, "SMTP", "REFUSED", tmp, host_name, host_ip, rcpt_addr, reverse_path);
						if(startup->options&MAIL_OPT_ALLOW_RELAY)
							sockprintf(socket, "553 Relaying through this server "
							"requires authentication.  "
							"Please authenticate with POP3 before sending.");
						else
							sockprintf(socket, "550 Relay not allowed.");
						break;
					}

					if(relay_user.number==0)
						SAFECOPY(relay_user.alias,"Unknown User");

					lprintf("%04d SMTP %s relaying to external mail service: %s"
						,socket, relay_user.alias, tp+1);

					fprintf(rcptlst,"0\n%.*s\n%.*s\n"
						,(int)sizeof(rcpt_name)-1,rcpt_addr
						,(int)sizeof(rcpt_addr)-1,p);
					
					sockprintf(socket,ok_rsp);
					state=SMTP_STATE_RCPT_TO;
					rcpt_count++;
					continue;
				}
			}
			if(tp!=NULL)
				*tp=0;	/* truncate at '@' */

			while(*p && !isalnum(*p)) p++;	/* Skip '<' or '"' */
			truncstr(p,"\"");	

			p=alias(&scfg,p,name_alias_buf);
			if(p==name_alias_buf) 
				lprintf("%04d SMTP NAME ALIAS: %s",socket,p);
		
			if(!strnicmp(p,"sub:",4)) {		/* Post on a sub-board */
				p+=4;
				for(i=0;i<scfg.total_subs;i++)
					if(!stricmp(p,scfg.sub[i]->code))
						break;
				if(i>=scfg.total_subs) {
					lprintf("%04d !SMTP UNKNOWN SUB-BOARD: %s", socket, p);
					sockprintf(socket, "550 Unknown sub-board: %s", p);
					continue;
				}
				subnum=i;
				sockprintf(socket,ok_rsp);
				state=SMTP_STATE_RCPT_TO;
				rcpt_count++;
				continue;
			}

			usernum=0;	/* unknown user at this point */
			if(startup->options&MAIL_OPT_ALLOW_RX_BY_NUMBER 
				&& isdigit(*p)) {
				usernum=atoi(p);			/* RX by user number */
				/* verify usernum */
				username(&scfg,usernum,str);
				if(!str[0] || !stricmp(str,"DELETED USER"))
					usernum=0;
				p=str;
			} else {
				/* RX by "user alias", "user.alias" or "user_alias" */
				usernum=matchuser(&scfg,p,TRUE /* sysop_alias */);	

				if(!usernum) { /* RX by "real name", "real.name", or "sysop.alias" */
					
					/* convert "user.name" to "user name" */
					SAFECOPY(rcpt_name,p);
					for(tp=rcpt_name;*tp;tp++)	
						if(*tp=='.') *tp=' ';

					if(!stricmp(p,scfg.sys_op) || !stricmp(rcpt_name,scfg.sys_op))
						usernum=1;			/* RX by "sysop.alias" */

					if(!usernum && scfg.msg_misc&MM_REALNAME)	/* RX by "real name" */
						usernum=userdatdupe(&scfg, 0, U_NAME, LEN_NAME, p, FALSE);

					if(!usernum && scfg.msg_misc&MM_REALNAME)	/* RX by "real.name" */
						usernum=userdatdupe(&scfg, 0, U_NAME, LEN_NAME, rcpt_name, FALSE);
				}
			}
			if(!usernum && startup->default_user[0]) {
				usernum=matchuser(&scfg,startup->default_user,TRUE /* sysop_alias */);
				if(usernum)
					lprintf("%04d SMTP Forwarding mail for UNKNOWN USER to default user: %s"
						,socket,startup->default_user);
				else
					lprintf("%04d !SMTP UNKNOWN DEFAULT USER: %s"
						,socket,startup->default_user);
			}

			if(!usernum) {
				lprintf("%04d !SMTP UNKNOWN USER:%s", socket, buf+8);
				sockprintf(socket, "550 Unknown User:%s", buf+8);
				continue;
			}
			user.number=usernum;
			if((i=getuserdat(&scfg, &user))!=0) {
				lprintf("%04d !SMTP ERROR %d getting data on user #%u (%s)"
					,socket, i, usernum, p);
				sockprintf(socket, "550 Unknown User:%s", buf+8);
				continue;
			}
			if(user.misc&(DELETED|INACTIVE)) {
				lprintf("%04d !SMTP DELETED or INACTIVE user #%u (%s)"
					,socket, usernum, p);
				sockprintf(socket, "550 Unknown User:%s", buf+8);
				continue;
			}
			if(cmd==SMTP_CMD_SEND) { /* Check if user online */
				for(i=0;i<scfg.sys_nodes;i++) {
					getnodedat(&scfg, i+1, &node, 0);
					if(node.status==NODE_INUSE && node.useron==user.number
						&& !(node.misc&NODE_POFF))
						break;
				}
				if(i>=scfg.sys_nodes) {
					lprintf("%04d !Attempt to send telegram to unavailable user #%u (%s)"
						,socket, user.number, user.alias);
					sockprintf(socket,"450 User unavailable");
					continue;
				}
			}
			if(cmd==SMTP_CMD_MAIL) {
#if 0	/* implement later */
				if(useron.etoday>=cfg.level_emailperday[useron.level]
					&& !(useron.rest&FLAG('Q'))) {
					bputs(text[TooManyEmailsToday]);
					continue; 
				}
#endif
			} else
				telegram=TRUE;

			fprintf(rcptlst,"%u\n%.*s\n"
				,user.number,(int)sizeof(rcpt_name)-1,rcpt_addr);

			/* Forward to Internet */
			tp=strrchr(user.netmail,'@');
			if(!telegram
				&& !no_forward
				&& scfg.sys_misc&SM_FWDTONET 
				&& (user.misc&NETMAIL || forward)
				&& tp && strchr(tp,'.') && !strchr(tp,'/') 
				&& !strstr(tp,scfg.sys_inetaddr)) {
				lprintf("%04d SMTP Forwarding to: %s"
					,socket, user.netmail);
				fprintf(rcptlst,"%s\n",user.netmail);
				sockprintf(socket,"251 User not local; will forward to %s", user.netmail);
			} else { /* Local (no-forward) */
				fprintf(rcptlst,"#%u\n",usernum);
				sockprintf(socket,ok_rsp);
			}
			state=SMTP_STATE_RCPT_TO;
			rcpt_count++;
			continue;
		}
		/* Message Data (header and body) */
		if(!strnicmp(buf,"DATA",4)) {
			if(state<SMTP_STATE_RCPT_TO) {
				lprintf("%04d !SMTP MISSING 'RCPT TO' command", socket);
				sockprintf(socket, badseq_rsp);
				continue;
			}
			if(msgtxt!=NULL) {
				fclose(msgtxt);
				if(!(startup->options&MAIL_OPT_DEBUG_RX_BODY))
					unlink(msgtxt_fname);
			}
			sprintf(msgtxt_fname,"%sSMTP.%s.msg", scfg.data_dir, session_id);
			if((msgtxt=fopen(msgtxt_fname,"w+b"))==NULL) {
				lprintf("%04d !SMTP ERROR %d opening %s"
					,socket, errno, msgtxt_fname);
				sockprintf(socket, "452 Insufficient system storage");
				continue;
			}
			/* These vars are potentially over-written by parsing an RFC822 header */
			/* get sender_addr */
			p=strchr(reverse_path,'<');
			if(p==NULL)	
				p=reverse_path;
			else 
				p++;
			SAFECOPY(sender_addr,p);
			truncstr(sender_addr,">");
			/* get sender */
			SAFECOPY(sender,sender_addr);
			if(truncstr(sender,"@")==NULL)
				sender[0]=0;

			sockprintf(socket, "354 send the mail data, end with <CRLF>.<CRLF>");
			if(telegram)
				state=SMTP_STATE_DATA_BODY;	/* No RFC headers in Telegrams */
			else
				state=SMTP_STATE_DATA_HEADER;
			hdr_lines=0;
			continue;
		}
		sockprintf(socket,"500 Syntax error");
		lprintf("%04d !SMTP UNSUPPORTED COMMAND: '%s'", socket, buf);
		if(++badcmds>9) {
			lprintf("%04d !TOO MANY INVALID COMMANDS (%u)",socket,badcmds);
			break;
		}
	}

	/* Free up resources here */
	smb_freemsgmem(&msg);

	if(msgtxt!=NULL) {
		fclose(msgtxt);
		if(!(startup->options&MAIL_OPT_DEBUG_RX_BODY))
			unlink(msgtxt_fname);
	}
	if(rcptlst!=NULL) {
		fclose(rcptlst);
		unlink(rcptlst_fname);
	}
	if(spy!=NULL)
		fclose(spy);

	status(STATUS_WFC);

	active_clients--,update_clients();
	client_off(socket);

	thread_down();
	lprintf("%04d SMTP RX Session thread terminated (%u threads remain, %lu clients served)"
		,socket, thread_count, served);

	/* Must be last */
	mail_close_socket(socket);
}

BOOL bounce(smb_t* smb, smbmsg_t* msg, char* err, BOOL immediate)
{
	char		str[128];
	char		attempts[64];
	int			i;
	ushort		agent=AGENT_SMTPSYSMSG;
	smbmsg_t	newmsg;

	if((i=smb_lockmsghdr(smb,msg))!=0) {
		lprintf("0000 !BOUNCE ERROR %d (%s) locking message header #%lu"
			,i, smb->last_error, msg->hdr.number);
		return(FALSE);
	}

	msg->hdr.delivery_attempts++;
	if((i=smb_putmsg(smb,msg))!=0) {
		lprintf("0000 !BOUNCE ERROR %d (%s) incrementing delivery attempt counter"
			,i, smb->last_error);
		smb_unlockmsghdr(smb,msg);
		return(FALSE);
	}

	lprintf("0000 !Delivery attempt #%u FAILED for message #%lu from %s to %s"
		,msg->hdr.delivery_attempts, msg->hdr.number
		,msg->from, msg->to_net.addr);

	if(!immediate && msg->hdr.delivery_attempts<startup->max_delivery_attempts) {
		smb_unlockmsghdr(smb,msg);
		return(TRUE);
	}

	newmsg=*msg;
	/* Mark original message as deleted */
	msg->hdr.attr|=MSG_DELETE;
	msg->idx.attr=msg->hdr.attr;
	if((i=smb_putmsg(smb,msg))!=0) {
		lprintf("0000 !BOUNCE ERROR %d (%s) deleting message"
			,i, smb->last_error);
		smb_unlockmsghdr(smb,msg);
		return(FALSE);
	}
	if(msg->hdr.auxattr&MSG_FILEATTACH)
		delfattach(&scfg,msg);
	smb_unlockmsghdr(smb,msg);

	if(msg->from_agent!=AGENT_PERSON	/* don't bounce 'bounce messages' */
		|| (msg->idx.from==0 && msg->from_net.type==NET_NONE)
		|| (msg->reverse_path!=NULL && *msg->reverse_path==0)) {
		lprintf("0000 !Deleted undeliverable message from %s", msg->from);
#if 0	/* this is actually wrong... whoever deletes the index will free the data fields */
		i=smb_freemsgdat(smb,msg->hdr.offset,smb_getmsgdatlen(msg),1);
		if(i!=SMB_SUCCESS)
			lprintf("0000 !ERROR %d (%s) freeing data blocks for undeliverable message"
				,i,smb->last_error);
#endif
		return(TRUE);
	}
	
	lprintf("0000 !Bouncing message back to %s", msg->from);

	newmsg.hfield=NULL;
	newmsg.hfield_dat=NULL;
	newmsg.total_hfields=0;
	newmsg.idx.to=newmsg.idx.from;
	newmsg.idx.from=0;
	newmsg.hdr.delivery_attempts=0;

	sprintf(str,"Delivery failure: %.100s",newmsg.subj);
	smb_hfield(&newmsg, SUBJECT, (ushort)strlen(str), str);
	smb_hfield(&newmsg, RECIPIENT, (ushort)strlen(newmsg.from), newmsg.from);
	if(newmsg.idx.to) {
		sprintf(str,"%u",newmsg.idx.to);
		smb_hfield(&newmsg, RECIPIENTEXT, (ushort)strlen(str), str);
	}
	if(newmsg.from_net.type!=NET_NONE && newmsg.from_net.type!=NET_FIDO
		&& newmsg.reverse_path!=NULL) {
		smb_hfield(&newmsg, RECIPIENTNETTYPE, sizeof(newmsg.from_net.type)
			,&newmsg.from_net.type);
		smb_hfield(&newmsg, RECIPIENTNETADDR, (ushort)strlen(newmsg.reverse_path)
			,newmsg.reverse_path);
	}
	strcpy(str,"Mail Delivery Subsystem");
	smb_hfield(&newmsg, SENDER, (ushort)strlen(str), str);
	smb_hfield(&newmsg, SENDERAGENT, sizeof(agent), &agent);
	
	/* Put error message in subject for now */
	if(msg->hdr.delivery_attempts>1)
		sprintf(attempts,"after %u attempts", msg->hdr.delivery_attempts);
	else
		attempts[0]=0;
	sprintf(str,"%s reporting delivery failure of message %s"
		,startup->host_name, attempts);
	smb_hfield(&newmsg, SMB_COMMENT, (ushort)strlen(str), str);
	sprintf(str,"from %s to %s\r\n"
		,msg->reverse_path==NULL ? msg->from : msg->reverse_path
		,(char*)msg->to_net.addr);
	smb_hfield(&newmsg, SMB_COMMENT, (ushort)strlen(str), str);
	strcpy(str,"Reason:");
	smb_hfield(&newmsg, SMB_COMMENT, (ushort)strlen(str), str);
	smb_hfield(&newmsg, SMB_COMMENT, (ushort)strlen(err), err);
	sprintf(str,"\r\nOriginal message text follows:\r\n");
	smb_hfield(&newmsg, SMB_COMMENT, (ushort)strlen(str), str);

	if((i=smb_addmsghdr(smb,&newmsg,SMB_SELFPACK))!=0)
		lprintf("0000 !BOUNCE ERROR %d (%s) adding message header"
			,i,smb->last_error);
	else {
		lprintf("0000 !Delivery failure notification (message #%ld) created for %s"
			,newmsg.hdr.number, newmsg.from);
		if((i=smb_incmsg(smb,&newmsg))!=0)
			lprintf("0000 !BOUNCE ERROR %d (%s) incrementing data allocation units"
				,i,smb->last_error);
	}

	newmsg.dfield=NULL;				/* Don't double-free the data fields */
	newmsg.hdr.total_dfields=0;
	smb_freemsgmem(&newmsg);

	return(TRUE);
}

#ifdef __BORLANDC__
#pragma argsused
#endif
static void sendmail_thread(void* arg)
{
	int			i,j;
	char		to[128];
	char		mx[128];
	char		mx2[128];
	char		err[128];
	char		buf[512];
	char		toaddr[256];
	char		fromaddr[256];
	char*		server;
	char*		msgtxt=NULL;
	char*		p;
	ushort		port;
	ulong		offset;
	ulong		last_msg=0;
	ulong		total_msgs;
	ulong		ip_addr;
	ulong		dns;
	ulong		lines;
	BOOL		success;
	BOOL		first_cycle=TRUE;
	SOCKET		sock=INVALID_SOCKET;
	SOCKADDR_IN	addr;
	SOCKADDR_IN	server_addr;
	time_t		last_scan=0;
	smb_t		smb;
	smbmsg_t	msg;

	sendmail_running=TRUE;

	thread_up(TRUE /* setuid */);

	lprintf("0000 SendMail thread started");

	memset(&msg,0,sizeof(msg));
	memset(&smb,0,sizeof(smb));

	while(server_socket!=INVALID_SOCKET) {

		if(startup->options&MAIL_OPT_NO_SENDMAIL) {
			mswait(1000);
			continue;
		}

		if(active_sendmail!=0)
			active_sendmail=0,update_clients();

		smb_close(&smb);

		if(sock!=INVALID_SOCKET) {
			mail_close_socket(sock);
			sock=INVALID_SOCKET;
		}

		if(msgtxt!=NULL) {
			smb_freemsgtxt(msgtxt);
			msgtxt=NULL;
		}

		smb_freemsgmem(&msg);

		/* Don't delay on first loop */
		if(first_cycle)
			first_cycle=FALSE;
		else
			mswait(3000);

		sprintf(smb.file,"%smail",scfg.data_dir);
		smb.retry_time=scfg.smb_retry_time;
		smb.subnum=INVALID_SUB;
		if((i=smb_open(&smb))!=0) 
			continue;
		if((i=smb_locksmbhdr(&smb))!=0)
			continue;
		i=smb_getstatus(&smb);
		smb_unlocksmbhdr(&smb);
		if(i!=0)
			continue;
		if(smb.status.last_msg==last_msg && time(NULL)-last_scan<startup->rescan_frequency)
			continue;
		last_msg=smb.status.last_msg;
		last_scan=time(NULL);
		total_msgs=smb.status.total_msgs;
		smb_rewind(smb.sid_fp);
		for(offset=0;offset<total_msgs;offset++) {

			if(active_sendmail!=0)
				active_sendmail=0,update_clients();

			if(server_socket==INVALID_SOCKET)	/* server stopped */
				break;

			if(sock!=INVALID_SOCKET) {
				mail_close_socket(sock);
				sock=INVALID_SOCKET;
			}

			if(msgtxt!=NULL) {
				smb_freemsgtxt(msgtxt);
				msgtxt=NULL;
			}

			smb_freemsgmem(&msg);

			smb_fseek(smb.sid_fp, offset*sizeof(msg.idx), SEEK_SET);
			if(smb_fread(&msg.idx, sizeof(msg.idx), smb.sid_fp) != sizeof(msg.idx))
				break;
			if(msg.idx.attr&MSG_DELETE)	/* Marked for deletion */
				continue;
			if(msg.idx.to)	/* Local */
				continue;
			msg.offset=offset;

			if((i=smb_lockmsghdr(&smb,&msg))!=0) {
				lprintf("0000 !SEND ERROR %d (%s) locking message header #%lu"
					,i, smb.last_error, msg.idx.number);
				continue;
			}
			i=smb_getmsghdr(&smb,&msg);
			smb_unlockmsghdr(&smb,&msg);
			if(i!=0) {
				lprintf("0000 !SEND ERROR %d (%s) reading message header #%lu"
					,i, smb.last_error, msg.idx.number);
				continue; 
			}

			if(msg.to_net.type!=NET_INTERNET || msg.to_net.addr==NULL) 
				continue;

			active_sendmail=1,update_clients();

			lprintf("0000 SEND Message #%lu from %s to %s"
				,msg.hdr.number, msg.from, msg.to_net.addr);
			status("Sending");
#ifdef _WIN32
			if(startup->outbound_sound[0] && !(startup->options&MAIL_OPT_MUTE)) 
				PlaySound(startup->outbound_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

			lprintf("0000 SEND getting message text");
			if((msgtxt=smb_getmsgtxt(&smb,&msg,GETMSGTXT_TAILS))==NULL) {
				lprintf("0000 !SEND ERROR (%s) retrieving message text",smb.last_error);
				continue;
			}

			port=0;
			if(startup->options&MAIL_OPT_RELAY_TX) { 
				server=startup->relay_server;
				port=startup->relay_port;
			} else {
				p=strrchr((char*)msg.to_net.addr,':');	/* non-standard SMTP port */
				if(p!=NULL) {
					*p=0;
					port=atoi(p+1);
				}
				SAFECOPY(to,(char*)msg.to_net.addr);
				truncstr(to,"> ");

				p=strrchr(to,'@');
				if(p==NULL) {
					lprintf("0000 !SEND INVALID destination address: %s", to);
					sprintf(err,"Invalid destination address: %s", to);
					bounce(&smb,&msg,err,TRUE);
					continue;
				}
				if((dns=resolve_ip(startup->dns_server))==INADDR_NONE) {
					lprintf("0000 !SEND INVALID DNS server address: %s"
						,startup->dns_server);
					continue;
				}
				p++;
				lprintf("0000 SEND getting MX records for %s from %s",p,startup->dns_server);
				if((i=dns_getmx(p, mx, mx2, startup->interface_addr, dns
					,startup->options&MAIL_OPT_USE_TCP_DNS ? TRUE : FALSE
					,TIMEOUT_THREAD_WAIT/2))!=0) {
					lprintf("0000 !SEND ERROR %d obtaining MX records for %s from %s"
						,i,p,startup->dns_server);
					sprintf(err,"Error %d obtaining MX record for %s",i,p);
					bounce(&smb,&msg,err,FALSE);
					continue;
				}
				server=mx;
			}
			if(!port)
				port=IPPORT_SMTP;


			if((sock=mail_open_socket(SOCK_STREAM))==INVALID_SOCKET) {
				lprintf("0000 !SEND ERROR %d opening socket", ERROR_VALUE);
				continue;
			}

			memset(&addr,0,sizeof(addr));
			addr.sin_addr.s_addr = htonl(startup->interface_addr);
			addr.sin_family = AF_INET;

			if(startup->seteuid!=NULL)
				startup->seteuid(FALSE);
			i=bind(sock,(struct sockaddr *)&addr, sizeof(addr));
			if(startup->seteuid!=NULL)
				startup->seteuid(TRUE);
			if(i!=0) {
				lprintf("%04d !SEND ERROR %d (%d) binding socket", sock, i, ERROR_VALUE);
				continue;
			}

			strcpy(err,"UNKNOWN ERROR");
			success=FALSE;
			for(j=0;j<2 && !success;j++) {
				if(j) {
					if(startup->options&MAIL_OPT_RELAY_TX || !mx2[0])
						break;
					server=mx2;	/* Give second mx record a try */
				}
				
				lprintf("%04d SEND resolving SMTP hostname: %s", sock, server);
				ip_addr=resolve_ip(server);
				if(ip_addr==INADDR_NONE) {
					sprintf(err,"Failed to resolve SMTP hostname: %s",server);
					continue;
				}

				memset(&server_addr,0,sizeof(server_addr));
				server_addr.sin_addr.s_addr = ip_addr;
				server_addr.sin_family = AF_INET;
				server_addr.sin_port = htons(port);
				
				lprintf("%04d SEND connecting to port %u on %s [%s]"
					,sock
					,ntohs(server_addr.sin_port)
					,server,inet_ntoa(server_addr.sin_addr));
				if((i=connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)))!=0) {
					i=ERROR_VALUE;
					lprintf("%04d !SEND ERROR %d connecting to SMTP server: %s"
						,sock
						,i, server);
					sprintf(err,"Error %d connecting to SMTP server: %s"
						,i, server);
					continue;
				}
				success=TRUE;
			}
			if(!success) {	/* Failed to send, so bounce */
				bounce(&smb,&msg,err,FALSE);	
				continue;
			}

			lprintf("%04d SEND connected to %s",sock,server);

			/* HELO */
			if(!sockgetrsp(sock,"220",buf,sizeof(buf))) {
				sprintf(err,badrsp_err,server,buf,"220");
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			sockprintf(sock,"HELO %s",startup->host_name);
			if(!sockgetrsp(sock,"250", buf, sizeof(buf))) {
				sprintf(err,badrsp_err,server,buf,"250");
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			/* MAIL */
			if(msg.from_net.type==NET_INTERNET && msg.reverse_path!=NULL)
				SAFECOPY(fromaddr,msg.reverse_path);
			else 
				usermailaddr(&scfg,fromaddr,msg.from);
			truncstr(fromaddr," ");
			if(fromaddr[0]=='<')
				sockprintf(sock,"MAIL FROM: %s",fromaddr);
			else
				sockprintf(sock,"MAIL FROM: <%s>",fromaddr);
			if(!sockgetrsp(sock,"250", buf, sizeof(buf))) {
				sprintf(err,badrsp_err,server,buf,"250");
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			/* RCPT */
			if((p=strrchr((char*)msg.to_net.addr,'<'))!=NULL)
				p++;
			else
				p=(char*)msg.to_net.addr;
			SAFECOPY(toaddr,p);
			truncstr(toaddr,"> ");
			sockprintf(sock,"RCPT TO: <%s>", toaddr);
			if(!sockgetrsp(sock,"25", buf, sizeof(buf))) {
				sprintf(err,badrsp_err,server,buf,"25*");
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			/* DATA */
			sockprintf(sock,"DATA");
			if(!sockgetrsp(sock,"354", buf, sizeof(buf))) {
				sprintf(err,badrsp_err,server,buf,"354");
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			lprintf("%04d SEND sending message text (%u bytes)"
				,sock, strlen(msgtxt));
			lines=sockmsgtxt(sock,&msg,msgtxt,-1);
			if(!sockgetrsp(sock,"250", buf, sizeof(buf))) {
				sprintf(err,badrsp_err,server,buf,"250");
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			lprintf("%04d SEND message transfer complete (%lu lines)", sock, lines);

			msg.hdr.attr|=MSG_DELETE;
			msg.idx.attr=msg.hdr.attr;
			if((i=smb_lockmsghdr(&smb,&msg))!=0) 
				lprintf("%04d !SEND ERROR %d (%s) locking message header #%lu"
					,sock
					,i, smb.last_error, msg.hdr.number);
			if((i=smb_putmsg(&smb,&msg))!=0)
				lprintf("%04d !SEND ERROR %d (%s) deleting message #%lu"
					,sock
					,i, smb.last_error, msg.hdr.number);
			if(msg.hdr.auxattr&MSG_FILEATTACH)
				delfattach(&scfg,&msg);
			smb_unlockmsghdr(&smb,&msg);

			/* QUIT */
			sockprintf(sock,"QUIT");
			sockgetrsp(sock,"221", buf, sizeof(buf));
			mail_close_socket(sock);
			sock=INVALID_SOCKET;
		}				
		status(STATUS_WFC);
	}
	if(sock!=INVALID_SOCKET)
		mail_close_socket(sock);

	smb_freemsgtxt(msgtxt);
	smb_freemsgmem(&msg);
	smb_close(&smb);

	if(active_sendmail!=0)
		active_sendmail=0,update_clients();

	thread_down();
	lprintf("0000 SendMail thread terminated (%u threads remain)", thread_count);

	sendmail_running=FALSE;
}

void DLLCALL mail_terminate(void)
{
	recycle_server=FALSE;
	if(server_socket!=INVALID_SOCKET) {
    	lprintf("%04d MAIL Terminate: closing socket",server_socket);
		mail_close_socket(server_socket);
	    server_socket=INVALID_SOCKET;
    }
}

static void cleanup(int code)
{
	free_cfg(&scfg);

	if(server_socket!=INVALID_SOCKET) {
		mail_close_socket(server_socket);
		server_socket=INVALID_SOCKET;
	}

	if(pop3_socket!=INVALID_SOCKET) {
		mail_close_socket(pop3_socket);
		pop3_socket=INVALID_SOCKET;
	}

	update_clients();

#ifdef _WINSOCKAPI_	
	if(WSAInitialized && WSACleanup()!=0) 
		lprintf("0000 !WSACleanup ERROR %d",ERROR_VALUE);
#endif

	thread_down();
	status("Down");
    lprintf("#### Mail Server thread terminated (%u threads remain, %lu clients served)"
		,thread_count, served);
	if(startup!=NULL && startup->terminated!=NULL)
		startup->terminated(code);
}

const char* DLLCALL mail_ver(void)
{
	static char ver[256];
	char compiler[32];

	DESCRIBE_COMPILER(compiler);

	sscanf("$Revision$", "%*s %s", revision);

	sprintf(ver,"Synchronet Mail Server %s%s  SMBLIB %s  "
		"Compiled %s %s with %s"
		,revision
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		,smb_lib_ver()
		,__DATE__, __TIME__, compiler
		);

	return(ver);
}

void DLLCALL mail_server(void* arg)
{
	char			path[MAX_PATH+1];
	char			str[256];
	char			error[256];
	char			compiler[32];
	SOCKADDR_IN		server_addr;
	SOCKADDR_IN		client_addr;
	socklen_t		client_addr_len;
	SOCKET			client_socket;
	int				i;
	int				result;
	ulong			l;
	time_t			t;
	time_t			start;
	time_t			initialized=0;
	fd_set			socket_set;
	SOCKET			high_socket_set;
	pop3_t*			pop3;
	smtp_t*			smtp;
	struct timeval	tv;

	mail_ver();

	startup=(mail_startup_t*)arg;

    if(startup==NULL) {
    	sbbs_beep(100,500);
    	fprintf(stderr, "No startup structure passed!\n");
    	return;
    }

	if(startup->size!=sizeof(mail_startup_t)) {	/* verify size */
		sbbs_beep(100,500);
		sbbs_beep(300,500);
		sbbs_beep(100,500);
		fprintf(stderr, "Invalid startup structure!\n");
		return;
	}

	/* Setup intelligent defaults */
	if(startup->relay_port==0)				startup->relay_port=IPPORT_SMTP;
	if(startup->smtp_port==0)				startup->smtp_port=IPPORT_SMTP;
	if(startup->pop3_port==0)				startup->pop3_port=IPPORT_POP3;
	if(startup->rescan_frequency==0)		startup->rescan_frequency=3600;	/* 60 minutes */
	if(startup->max_delivery_attempts==0)	startup->max_delivery_attempts=50;
	if(startup->max_inactivity==0) 			startup->max_inactivity=120; /* seconds */
	if(startup->max_recipients==0) 			startup->max_recipients=100;
	if(startup->proc_cfg_file[0]==0)			
		sprintf(startup->proc_cfg_file,"%smailproc.cfg",scfg.ctrl_dir);

	served=0;
	startup->recycle_now=FALSE;
	recycle_server=TRUE;
	do {

		thread_up(FALSE /* setuid */);

		status("Initializing");

		memset(&scfg, 0, sizeof(scfg));

		lprintf("Synchronet Mail Server Revision %s%s"
			,revision
#ifdef _DEBUG
			," Debug"
#else
			,""
#endif
			);

		DESCRIBE_COMPILER(compiler);

		lprintf("Compiled %s %s with %s", __DATE__, __TIME__, compiler);

		lprintf("SMBLIB %s (format %x.%02x)",smb_lib_ver(),smb_ver()>>8,smb_ver()&0xff);

		srand(time(NULL));

		if(!winsock_startup()) {
			cleanup(1);
			return;
		}

		t=time(NULL);
		lprintf("Initializing on %.24s with options: %lx"
			,CTIME_R(&t,str),startup->options);

		/* Initial configuration and load from CNF files */
		SAFECOPY(scfg.ctrl_dir,startup->ctrl_dir);
		lprintf("Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size=sizeof(scfg);
		SAFECOPY(error,UNKNOWN_LOAD_ERROR);
		if(!load_cfg(&scfg, NULL, TRUE, error)) {
			lprintf("!ERROR %s",error);
			lprintf("!Failed to load configuration files");
			cleanup(1);
			return;
		}

		if(startup->host_name[0]==0)
			SAFECOPY(startup->host_name,scfg.sys_inetaddr);

		if(!(scfg.sys_misc&SM_LOCAL_TZ) && !(startup->options&MAIL_OPT_LOCAL_TIMEZONE)) {
			if(putenv("TZ=UTC0"))
				lprintf("!putenv() FAILED");
			tzset();

			if((t=checktime())!=0) {   /* Check binary time */
				lprintf("!TIME PROBLEM (%ld)",t);
				cleanup(1);
				return;
			}
		}

		if(startup->max_clients==0) {
			startup->max_clients=scfg.sys_nodes;
			if(startup->max_clients<10)
				startup->max_clients=10;
		}

		lprintf("Maximum clients: %u",startup->max_clients);

		lprintf("Maximum inactivity: %u seconds",startup->max_inactivity);

		active_clients=0;
		update_clients();

		/* open a socket and wait for a client */

		server_socket = mail_open_socket(SOCK_STREAM);

		if(server_socket == INVALID_SOCKET) {
			lprintf("!ERROR %d opening socket", ERROR_VALUE);
			cleanup(1);
			return;
		}

		lprintf("%04d SMTP socket opened",server_socket);

		/*****************************/
		/* Listen for incoming calls */
		/*****************************/
		memset(&server_addr, 0, sizeof(server_addr));

		server_addr.sin_addr.s_addr = htonl(startup->interface_addr);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port   = htons(startup->smtp_port);

		if(startup->seteuid!=NULL)
			startup->seteuid(FALSE);
		result = bind(server_socket,(struct sockaddr *)&server_addr,sizeof(server_addr));
		if(startup->seteuid!=NULL)
			startup->seteuid(TRUE);
		if(result != 0) {
			lprintf("%04d !ERROR %d (%d) binding SMTP socket to port %u"
				,server_socket, result, ERROR_VALUE, startup->smtp_port);
			lprintf("%04d %s",server_socket, BIND_FAILURE_HELP);
			cleanup(1);
			return;
		}

		lprintf("%04d SMTP socket bound to port %u"
			,server_socket, startup->smtp_port);

		result = listen (server_socket, 1);

		if(result != 0) {
			lprintf("%04d !ERROR %d (%d) listening on socket"
				,server_socket, result, ERROR_VALUE);
			cleanup(1);
			return;
		}

		if(startup->options&MAIL_OPT_ALLOW_POP3) {

			/* open a socket and wait for a client */

			pop3_socket = mail_open_socket(SOCK_STREAM);

			if(pop3_socket == INVALID_SOCKET) {
				lprintf("!ERROR %d opening POP3 socket", ERROR_VALUE);
				cleanup(1);
				return;
			}

			lprintf("%04d POP3 socket opened",pop3_socket);

			/*****************************/
			/* Listen for incoming calls */
			/*****************************/
			memset(&server_addr, 0, sizeof(server_addr));

			server_addr.sin_addr.s_addr = htonl(startup->interface_addr);
			server_addr.sin_family = AF_INET;
			server_addr.sin_port   = htons(startup->pop3_port);

			if(startup->seteuid!=NULL)
				startup->seteuid(FALSE);
			result = bind(pop3_socket,(struct sockaddr *)&server_addr,sizeof(server_addr));
			if(startup->seteuid!=NULL)
				startup->seteuid(TRUE);
			if(result != 0) {
				lprintf("%04d !ERROR %d (%d) binding POP3 socket to port %u"
					,pop3_socket, result, ERROR_VALUE, startup->pop3_port);
				lprintf("%04d %s",pop3_socket,BIND_FAILURE_HELP);
				cleanup(1);
				return;
			}

			lprintf("%04d POP3 socket bound to port %u"
				,pop3_socket, startup->pop3_port);

			result = listen (pop3_socket, 1);

			if(result != 0) {
				lprintf("%04d !ERROR %d (%d) listening on POP3 socket"
					,pop3_socket, result, ERROR_VALUE);
				cleanup(1);
				return;
			}
		}

		/* signal caller that we've started up successfully */
		if(startup->started!=NULL)
    		startup->started();

		if(!(startup->options&MAIL_OPT_NO_SENDMAIL))
			_beginthread (sendmail_thread, 0, NULL);

		lprintf("%04d Mail Server thread started",server_socket);
		status(STATUS_WFC);

		if(initialized==0) {
			initialized=time(NULL);
			sprintf(path,"%smailsrvr.rec",scfg.ctrl_dir);
			t=fdate(path);
			if(t!=-1 && t>initialized)
				initialized=t;
		}

		while (server_socket!=INVALID_SOCKET) {

			if(!(startup->options&MAIL_OPT_NO_RECYCLE)) {
				sprintf(path,"%smailsrvr.rec",scfg.ctrl_dir);
				t=fdate(path);
				if(!active_clients && t!=-1 && t>initialized) {
					lprintf("0000 Recycle semaphore file (%s) detected",path);
					initialized=t;
					break;
				}
				if(!active_clients && startup->recycle_now==TRUE) {
					lprintf("0000 Recycle semaphore signaled");
					startup->recycle_now=FALSE;
					break;
				}
			}
			/* now wait for connection */

			FD_ZERO(&socket_set);
			FD_SET(server_socket,&socket_set);
			high_socket_set=server_socket+1;
			if(startup->options&MAIL_OPT_ALLOW_POP3 
				&& pop3_socket!=INVALID_SOCKET) {
				FD_SET(pop3_socket,&socket_set);
				if(pop3_socket+1>high_socket_set)
					high_socket_set=pop3_socket+1;
			}

			tv.tv_sec=2;
			tv.tv_usec=0;

			if((i=select(high_socket_set,&socket_set,NULL,NULL,&tv))<1) {
				if(i==0) {
					mswait(1);
					continue;
				}
				if(ERROR_VALUE==EINTR)
					lprintf("0000 Mail Server listening interrupted");
				else if(ERROR_VALUE == ENOTSOCK)
            		lprintf("0000 Mail Server sockets closed");
				else
					lprintf("0000 !ERROR %d selecting sockets",ERROR_VALUE);
				break;
			}

			if(server_socket!=INVALID_SOCKET
				&& FD_ISSET(server_socket,&socket_set)) {

				client_addr_len = sizeof(client_addr);
				client_socket = accept(server_socket, (struct sockaddr *)&client_addr
        			,&client_addr_len);

				if(client_socket == INVALID_SOCKET)
				{
					if(ERROR_VALUE == ENOTSOCK || ERROR_VALUE == EINVAL)
            			lprintf("0000 SMTP socket closed while listening");
					else
						lprintf("%04d !ERROR %d accept failed", server_socket, ERROR_VALUE);
					break;
				}
				if(startup->socket_open!=NULL)
					startup->socket_open(TRUE);
				sockets++;

				if(trashcan(&scfg,inet_ntoa(client_addr.sin_addr),"ip-silent")) {
					mail_close_socket(client_socket);
					continue;
				}

				if(active_clients>=startup->max_clients) {
					lprintf("%04d !MAXMIMUM CLIENTS (%u) reached, access denied"
						,client_socket, startup->max_clients);
					sockprintf(client_socket,"421 Maximum active clients reached, please try again later.");
					mswait(3000);
					mail_close_socket(client_socket);
					continue;
				}

				l=1;

				if((i=ioctlsocket(client_socket, FIONBIO, &l))!=0) {
					lprintf("%04d !ERROR %d (%d) disabling blocking on socket"
						,client_socket, i, ERROR_VALUE);
					mail_close_socket(client_socket);
					continue;
				}

				if((smtp=malloc(sizeof(smtp_t)))==NULL) {
					lprintf("%04d !ERROR allocating %u bytes of memory for smtp_t"
						,client_socket, sizeof(smtp_t));
					mail_close_socket(client_socket);
					continue;
				}

				smtp->socket=client_socket;
				smtp->client_addr=client_addr;
				_beginthread (smtp_thread, 0, smtp);
				served++;
			}

			if(pop3_socket!=INVALID_SOCKET
				&& FD_ISSET(pop3_socket,&socket_set)) {

				client_addr_len = sizeof(client_addr);
				client_socket = accept(pop3_socket, (struct sockaddr *)&client_addr
        			,&client_addr_len);

				if(client_socket == INVALID_SOCKET)
				{
					if(ERROR_VALUE == ENOTSOCK || ERROR_VALUE == EINVAL)
            			lprintf("%04d POP3 socket closed while listening",pop3_socket);
					else
						lprintf("%04d !ERROR %d accept failed", pop3_socket, ERROR_VALUE);
					break;
				}
				if(startup->socket_open!=NULL)
					startup->socket_open(TRUE);
				sockets++;

				if(trashcan(&scfg,inet_ntoa(client_addr.sin_addr),"ip-silent")) {
					mail_close_socket(client_socket);
					continue;
				}

				if(active_clients>=startup->max_clients) {
					lprintf("%04d !MAXMIMUM CLIENTS (%u) reached, access denied"
						,client_socket, startup->max_clients);
					sockprintf(client_socket,"-ERR Maximum active clients reached, please try again later.");
					mswait(3000);
					mail_close_socket(client_socket);
					continue;
				}


				l=1;

				if((i=ioctlsocket(client_socket, FIONBIO, &l))!=0) {
					lprintf("%04d !ERROR %d (%d) disabling blocking on socket"
						,client_socket, i, ERROR_VALUE);
					sockprintf(client_socket,"-ERR System error, please try again later.");
					mswait(3000);
					mail_close_socket(client_socket);
					continue;
				}

				if((pop3=malloc(sizeof(pop3_t)))==NULL) {
					lprintf("%04d !ERROR allocating %u bytes of memory for pop3_t"
						,client_socket,sizeof(pop3_t));
					sockprintf(client_socket,"-ERR System error, please try again later.");
					mswait(3000);
					mail_close_socket(client_socket);
					continue;
				}

				pop3->socket=client_socket;
				pop3->client_addr=client_addr;

				_beginthread (pop3_thread, 0, pop3);
				served++;
			}
		}

		if(active_clients) {
			lprintf("0000 Waiting for %d active clients to disconnect...", active_clients);
			start=time(NULL);
			while(active_clients) {
				if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
					lprintf("!TIMEOUT waiting for %u active clients ",active_clients);
					break;
				}
				mswait(100);
			}
		}

		if(sendmail_running) {
			mail_close_socket(server_socket);
			server_socket=INVALID_SOCKET; /* necessary to terminate sendmail_thread */
			mswait(2000);
		}
		if(sendmail_running) {
			lprintf("0000 Waiting for SendMail thread to terminate...");
			start=time(NULL);
			while(sendmail_running) {
				if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
					lprintf("!TIMEOUT waiting for sendmail thread to "
            			"terminate");
					break;
				}
				mswait(100);
			}
		}

		cleanup(0);

		if(recycle_server) {
			lprintf("Recycling server...");
			mswait(2000);
		}

	} while(recycle_server);
}
