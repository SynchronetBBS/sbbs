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

#elif defined(__unix__)

	#include <signal.h>		/* signal/SIGPIPE */

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
#include "crc32.h"

/* Constants */
#define MAIL_VERSION "1.11"

int dns_getmx(char* name, char* mx, char* mx2
			  ,DWORD intf, DWORD ip_addr, BOOL use_tcp, int timeout);

#define SMTP_OK		"250 OK"
#define SMTP_BADSEQ	"503 Bad sequence of commands"

#define TIMEOUT_THREAD_WAIT		30		/* Seconds */

#define STATUS_WFC	"Listening"

static const char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};

static mail_startup_t* startup=NULL;
static scfg_t	scfg;
static SOCKET	server_socket=INVALID_SOCKET;
static SOCKET	pop3_socket=INVALID_SOCKET;
static int		active_clients=0;
static int		active_sendmail=0;
static BOOL		sendmail_running=FALSE;
static DWORD	sockets=0;

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

	va_start(argptr,fmt);
    vsprintf(sbuf,fmt,argptr);
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
	if(startup!=NULL && startup->clients!=NULL)
		startup->clients(active_clients+active_sendmail);
}

static void client_on(SOCKET sock, client_t* client)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(TRUE,sock,client);
}

static void client_off(SOCKET sock)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(FALSE,sock,NULL);
}

static void thread_up(void)
{
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(TRUE);
}

static void thread_down(void)
{
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(FALSE);
}

SOCKET mail_open_socket(int type)
{
	SOCKET sock;

	sock=socket(AF_INET, type, IPPROTO_IP);
	if(sock!=INVALID_SOCKET && startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(TRUE);
	if(sock!=INVALID_SOCKET) {
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

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	if(/* result==0 && */ startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(FALSE);
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
	if(startup!=NULL && startup->status!=NULL)
	    startup->status(str);
}

static int sockprintf(SOCKET sock, char *fmt, ...)
{
	int		len;
	int		result;
	va_list argptr;
	char	sbuf[1024];
	fd_set	socket_set;
	struct timeval tv;

    va_start(argptr,fmt);
    len=vsprintf(sbuf,fmt,argptr);
	if(startup->options&MAIL_OPT_DEBUG_TX)
		lprintf("%04d TX: %s", sock, sbuf);
	strcat(sbuf,"\r\n");
	len+=2;
    va_end(argptr);

	/* Check socket for writability (using select) */
	tv.tv_sec=60;
	tv.tv_usec=0;

	FD_ZERO(&socket_set);
	FD_SET(sock,&socket_set);

	if((result=select(sock+1,NULL,&socket_set,NULL,&tv))<1) {
		lprintf("%04d !ERROR %d (%d) selecting socket for send"
			,sock, result, ERROR_VALUE, sock);
		return(0);
	}

	while((result=send(sock,sbuf,len,0))!=len) {
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

	start=time(NULL);
	
	while(rd<len-1) {

		tv.tv_sec=0;
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
		if(ch=='\n' && rd>=1) {
			break;
		}	
		buf[rd++]=ch;
	}
	buf[rd-1]=0;
	
	return(rd);
}

/************************************************/
/* Truncates white-space chars off end of 'str' */
/************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=' ') c--;
	str[c]=0;
}


static char* alias(char* name, char* alias)
{
	int		file;
	char*	p=name;
	char*	np;
	char*	tp;
	char	fname[MAX_PATH+1];
	FILE*	fp;

	sprintf(fname,"%salias.cfg",scfg.ctrl_dir);
	if((file=sopen(fname,O_RDONLY|O_BINARY,SH_DENYNO))==-1)
		return(name);

	if((fp=fdopen(file,"rb"))==NULL) {
		close(file);
		return(name);
	}

	while(!feof(fp)) {
		if(!fgets(alias,80,fp))
			break;
		np=alias;
		while(*np && *np<=' ') np++;
		if(*np==';')
			continue;
		tp=np;
		while(*tp && *tp>' ') tp++;
		if(*tp) *tp=0;
		if(!stricmp(np,name)) {
			np=tp+1;
			while(*np && *np<=' ') np++;
			p=np;
			truncsp(p);
			lprintf("0000 Alias: %s",p);
			break;
		}
	}
	fclose(fp);
	return(p);
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

static char *msgdate(when_t when, char* buf)
{
	struct tm	tm;
	struct tm*	tm_p;
	
	tm_p=localtime((const time_t*)&when.time);
	if(tm_p!=NULL)
		tm=*tm_p;
	else
		memset(&tm,0,sizeof(tm));
	sprintf(buf,"%s, %d %s %d %02d:%02d:%02d %s"
		,wday[tm.tm_wday]
		,tm.tm_mday
		,mon[tm.tm_mon]
		,1900+tm.tm_year
		,tm.tm_hour
		,tm.tm_min
		,tm.tm_sec
		,zonestr(when.zone)
		);
	return(buf);
}

#define MAX_LINE_LEN	1000

static void sockmsgtxt(SOCKET socket, smbmsg_t* msg, char* msgtxt, char* fromaddr
						,ulong maxlines)
{
	char		toaddr[256];
	char		date[64];
	char*		p;
	char*		tp;
	int			i;
	ulong		line;

	/* HEADERS */
	sockprintf(socket,"Date: %s",msgdate(msg->hdr.when_written,date));
	if(fromaddr[0]=='<')
		sockprintf(socket,"From: \"%s\" %s",msg->from,fromaddr);
	else
		sockprintf(socket,"From: \"%s\" <%s>",msg->from,fromaddr);
	sockprintf(socket,"Subject: %s",msg->subj);
	if(msg->to_net.type==NET_INTERNET || msg->to_net.type==NET_QWK) {
		if(*((char*)msg->to_net.addr)=='<')
			sockprintf(socket,"To: \"%s\" %s",msg->to,(char*)msg->to_net.addr);
		else
			sockprintf(socket,"To: \"%s\" <%s>",msg->to,(char*)msg->to_net.addr);
	} else {
		usermailaddr(&scfg,toaddr,msg->to);
		sockprintf(socket,"To: \"%s\" <%s>",msg->to,toaddr);
	}
	if(msg->replyto_net.type==NET_INTERNET)
		sockprintf(socket,"Reply-To: %s",msg->replyto_net.addr);
	else
		sockprintf(socket,"Reply-To: %s",fromaddr);
	sockprintf(socket,"Message-ID: <%08lX.%lu@%s>"
		,msg->hdr.when_written.time,msg->idx.number,scfg.sys_inetaddr);
	for(i=0;i<msg->total_hfields;i++) 
		if(msg->hfield[i].type==RFC822HEADER)
			sockprintf(socket,"%s",(char*)msg->hfield_dat[i]);
	sockprintf(socket,"");	/* Header Terminator */
	/* MESSAGE BODY */
	line=0;
	p=msgtxt;
	while(*p && line<maxlines) {
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
		line++;
		mswait(1);
	}
	sockprintf(socket,".");	/* End of text */
}

static u_long resolve_ip(char *addr)
{
	HOSTENT*	host;
	char*		p;

	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;
	if(!(*p))
		return(inet_addr(addr));

	if ((host=gethostbyname(addr))==NULL) {
		lprintf("0000 !ERROR resolving host name: %s",addr);
		return(0);
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
	char		fromaddr[256];
	char*		msgtxt;
	int			i;
	int			rd;
	BOOL		activity=FALSE;
	ulong		l;
	ulong		lines;
	ulong		msgs,bytes,msgnum,msgbytes;
	SOCKET		socket;
	HOSTENT*	host;
	smb_t		smb;
	smbmsg_t	msg;
	user_t		user;
	client_t	client;
	mail_t*		mail;
	pop3_t		pop3=*(pop3_t*)arg;

	thread_up();

	free(arg);

	socket=pop3.socket;

	if(startup->options&MAIL_OPT_DEBUG_POP3)
		lprintf("%04d POP3 session thread started", socket);

#ifdef _WIN32
	if(startup->pop3_sound[0] && !(startup->options&MAIL_OPT_MUTE)) 
		PlaySound(startup->pop3_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

	strcpy(host_ip,inet_ntoa(pop3.client_addr.sin_addr));

	if(startup->options&MAIL_OPT_DEBUG_POP3)
		lprintf("%04d POP3 connection accepted from: %s port %u"
			,socket, host_ip, ntohs(pop3.client_addr.sin_port));

	if(startup->options&MAIL_OPT_NO_HOST_LOOKUP)
		host=NULL;
	else
		host=gethostbyaddr((char *)&pop3.client_addr.sin_addr
			,sizeof(pop3.client_addr.sin_addr),AF_INET);

	if(host!=NULL && host->h_name!=NULL)
		sprintf(host_name,"%.*s",(int)sizeof(host_name)-1,host->h_name);
	else
		strcpy(host_name,"<no name>");

	if(startup->options&MAIL_OPT_DEBUG_POP3)
		lprintf("%04d POP3 client name: %s", socket, host_name);

	if(trashcan(&scfg,host_ip,"ip")) {
		lprintf("%04d !POP3 CLIENT BLOCKED in ip.can: %s"
			,socket, host_ip);
		sockprintf(socket,"-ERR Access denied.");
		mail_close_socket(socket);
		thread_down();
		return;
	}

	if(trashcan(&scfg,host_name,"host")) {
		lprintf("%04d !POP3 CLIENT BLOCKED in host.can: %s"
			,socket, host_name);
		sockprintf(socket,"-ERR Access denied.");
		mail_close_socket(socket);
		thread_down();
		return;
	}

	active_clients++;
	update_clients();

	/* Initialize client display */
	client.size=sizeof(client);
	client.time=time(NULL);
	sprintf(client.addr,"%.*s",(int)sizeof(client.addr)-1,host_ip);
	sprintf(client.host,"%.*s",(int)sizeof(client.host)-1,host_name);
	client.port=ntohs(pop3.client_addr.sin_port);
	client.protocol="POP3";
	client.user="<unknown>";
	client_on(socket,&client);

	sprintf(str,"POP3: %s", host_ip);
	status(str);

	mail=NULL;

	do {
		memset(&smb,0,sizeof(smb));
		memset(&msg,0,sizeof(msg));
		memset(&user,0,sizeof(user));

		sprintf(smb.file,"%smail",scfg.data_dir);
		if((i=smb_open(&smb))!=0) {
			lprintf("%04d !ERROR %d (%s) opening %s",socket,i,smb.last_error,smb.file);
			sockprintf(socket,"-ERR %d opening %s",i,smb.file);
			break;
		}

		sockprintf(socket,"+OK Synchronet POP3 Server for %s v%s Ready"
			,PLATFORM_DESC,MAIL_VERSION);

		if(!sockgetrsp(socket,"USER ",buf,sizeof(buf))) {
			sockprintf(socket,"-ERR USER command expected");
			break;
		}
		p=buf+5;
		while(*p && *p<=' ') p++;
		sprintf(username,"%.*s",(int)sizeof(username)-1,p);
		sockprintf(socket,"+OK");
		if(!sockgetrsp(socket,"PASS ",buf,sizeof(buf))) {
			sockprintf(socket,"-ERR PASS command expected");
			break;
		}
		p=buf+5;
		while(*p && *p<=' ') p++;
		sprintf(password,"%.*s",(int)sizeof(password)-1,p);
		user.number=matchuser(&scfg,username);
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

		/* Update client display */
		client.user=user.alias;
		client_on(socket,&client);

		if(startup->options&MAIL_OPT_DEBUG_POP3)		
			lprintf("%04d POP3 %s logged in", socket, user.alias);
		sprintf(str,"POP3: %s",user.alias);
		status(str);

		sockprintf(socket,"+OK User verified");

		mail=loadmail(&smb,&msgs,user.number,MAIL_YOUR,0);

		for(l=bytes=0;l<msgs;l++) {
			msg.hdr.number=mail[l].number;
			if((i=smb_getmsgidx(&smb,&msg))!=0) {
				lprintf("%04d !POP3 ERROR %d getting message index"
					,socket ,i);
				break;
			}
			if((i=smb_lockmsghdr(&smb,&msg))!=0) {
				lprintf("%04d !POP3 ERROR %d locking message header #%lu"
					,socket ,i ,msg.hdr.number);
				break; 
			}
			i=smb_getmsghdr(&smb,&msg);
			smb_unlockmsghdr(&smb,&msg);
			if(i!=0) {
				lprintf("%04d !POP3 ERROR %d getting message header #%lu"
					,socket, i, msg.hdr.number);
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
						lprintf("%04d !POP3 ERROR %d getting message index"
							,socket, i);
						break;
					}
					if((i=smb_lockmsghdr(&smb,&msg))!=0) {
						lprintf("%04d !POP3 ERROR %d locking message header #%lu"
							,socket, i,msg.hdr.number);
						break; 
					}
					if((i=smb_getmsghdr(&smb,&msg))!=0) {
						lprintf("%04d !POP3 ERROR %d getting message header #%lu"
							,socket, i, msg.hdr.number);
						break;
					}
					msg.hdr.attr=mail[l].attr;
					msg.idx.attr=msg.hdr.attr;
					if((i=smb_putmsg(&smb,&msg))!=0)
						lprintf("%04d !POP3 ERROR %d updating message index"
							,socket, i);
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
						lprintf("%04d !POP3 ERROR %d getting message index"
							,socket, i);
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
						lprintf("%04d !POP3 ERROR %d locking message header #%lu"
							,socket, i, msg.hdr.number);
						sockprintf(socket,"-ERR %d locking message header",i);
						continue; 
					}
					i=smb_getmsghdr(&smb,&msg);
					smb_unlockmsghdr(&smb,&msg);
					if(i!=0) {
						smb_freemsgmem(&msg);
						lprintf("%04d !POP3 ERROR %d getting message header #%lu"
							,socket, i,msg.hdr.number);
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
						lprintf("%04d !POP3 ERROR %d getting message index"
							,socket, i);
						break;
					}
					if(msg.idx.attr&MSG_DELETE) 
						continue;
					if((i=smb_lockmsghdr(&smb,&msg))!=0) {
						lprintf("%04d !POP3 ERROR %d locking message header #%lu"
							,socket, i,msg.hdr.number);
						break; 
					}
					i=smb_getmsghdr(&smb,&msg);
					smb_unlockmsghdr(&smb,&msg);
					if(i!=0) {
						smb_freemsgmem(&msg);
						lprintf("%04d !POP3 ERROR %d getting message header #%lu"
							,socket, i,msg.hdr.number);
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
					lprintf("%04d !POP3 ERROR %d getting message index"
						,socket, i);
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
					lprintf("%04d !POP3 ERROR %d locking message header #%lu"
						,socket, i, msg.hdr.number);
					sockprintf(socket,"-ERR %d locking message header",i);
					continue; 
				}
				if((i=smb_getmsghdr(&smb,&msg))!=0) {
					smb_unlockmsghdr(&smb,&msg);
					lprintf("%04d !POP3 ERROR %d getting message header #%lu"
						,socket, i, msg.hdr.number);
					sockprintf(socket,"-ERR %d getting message header",i);
					continue;
				}
				smb_unlockmsghdr(&smb,&msg);

				if((msgtxt=smb_getmsgtxt(&smb,&msg,GETMSGTXT_TAILS))==NULL) {
					smb_freemsgmem(&msg);
					lprintf("%04d !POP3 ERROR retrieving message text for message #%lu"
						,socket, msg.hdr.number);
					sockprintf(socket,"-ERR retrieving message text");
					continue;
				}

				sockprintf(socket,"+OK message follows");
				if(msg.from_net.type==NET_INTERNET)
					strcpy(fromaddr,msg.from_net.addr);
				else if(msg.from_net.type==NET_QWK)
					sprintf(fromaddr,"\"%s@%s\"@%s"
						,msg.from,(char*)msg.from_net.addr,scfg.sys_inetaddr);
				else 
					usermailaddr(&scfg,fromaddr,msg.from);	/* unresolved exception here Nov-06-2000 */
				lprintf("%04d POP3 sending message text (%u bytes)"
					,socket,strlen(msgtxt));
				sockmsgtxt(socket,&msg,msgtxt,fromaddr,lines);
				/* if(startup->options&MAIL_OPT_DEBUG_POP3) */
				lprintf("%04d POP3 message transfer complete",socket);

				msg.hdr.attr|=MSG_READ;
				msg.idx.attr=msg.hdr.attr;
				msg.hdr.netattr|=MSG_SENT;

				if((i=smb_lockmsghdr(&smb,&msg))!=0) 
					lprintf("%04d !POP3 ERROR %d locking message header #%lu"
						,socket, i, msg.hdr.number);
				if((i=smb_putmsg(&smb,&msg))!=0)
					lprintf("%04d !POP3 ERROR %d marking message #%lu as read"
						,socket, i, msg.hdr.number);
				smb_unlockmsghdr(&smb,&msg);
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
					lprintf("%04d !POP3 ERROR %d getting message index"
						,socket, i);
					sockprintf(socket,"-ERR %d getting message index",i);
					continue;
				}
				if((i=smb_lockmsghdr(&smb,&msg))!=0) {
					lprintf("%04d !POP3 ERROR %d locking message header #%lu"
						,socket, i,msg.hdr.number);
					sockprintf(socket,"-ERR %d locking message header",i);
					continue; 
				}
				if((i=smb_getmsghdr(&smb,&msg))!=0) {
					smb_unlockmsghdr(&smb,&msg);
					lprintf("%04d !POP3 ERROR %d getting message header #%lu"
						,socket, i,msg.hdr.number);
					sockprintf(socket,"-ERR %d getting message header",i);
					continue;
				}
				msg.hdr.attr|=MSG_DELETE;
				msg.idx.attr=msg.hdr.attr;
				if((i=smb_putmsg(&smb,&msg))!=0) {
					smb_unlockmsghdr(&smb,&msg);
					smb_freemsgmem(&msg);
					lprintf("%04d !POP3 ERROR %d marking message as read", socket, i);
					sockprintf(socket,"-ERR %d marking message for deletion",i);
					continue;
				}
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
	mail_close_socket(socket);

	if(mail!=NULL)
		freemail(mail);

	smb_freemsgmem(&msg);
	smb_close(&smb);

	if(startup->options&MAIL_OPT_DEBUG_POP3)
		lprintf("%04d POP3 session thread terminated", socket);

	active_clients--;
	update_clients();
	client_off(socket);

	thread_down();
}

BOOL rblchk(DWORD mail_addr_n, char* rbl_addr)
{
	char		name[256];
	DWORD		mail_addr;

	mail_addr=ntohl(mail_addr_n);
	sprintf(name,"%ld.%ld.%ld.%ld.%s"
		,mail_addr&0xff
		,(mail_addr>>8)&0xff
		,(mail_addr>>16)&0xff
		,(mail_addr>>24)&0xff
		,rbl_addr
		);

	if(gethostbyname(name)==NULL)
		return(FALSE);

	return(TRUE);
}


static void smtp_thread(void* arg)
{
	int			i,j,x;
	int			rd;
	char		str[512];
	char		buf[1024],*p,*tp;
	char		hdrfield[512];
	char		alias_buf[128];
	char		reverse_path[128];
	char		date[64];
	char		month[16];
	char		rcpt_name[128];
	char		rcpt_addr[128];
	char		sender[128]={0};
	char		sender_addr[128]={0};
	char		hello_name[128];
	char		relay_list[MAX_PATH+1];
	char		domain_list[MAX_PATH+1];
	char		host_name[128];
	char		host_ip[64];
	int			addr_len;
	ushort		xlat;
	ushort		nettype=NET_NONE;
	uint		usernum;
	ulong		crc;
	ulong		length;
	ulong		offset;
	BOOL		esmtp=FALSE;
	FILE*		msgtxt=NULL;
	char		msgtxt_fname[MAX_PATH+1];
	FILE*		rcptlst;
	char		rcptlst_fname[MAX_PATH+1];
	FILE*		spy=NULL;
	SOCKET		socket;
	HOSTENT*	host;
	smb_t		smb;
	smbmsg_t	msg;
	smbmsg_t	newmsg;
	user_t		user;
	client_t	client;
	struct tm	tm;
	smtp_t		smtp=*(smtp_t*)arg;
	SOCKADDR_IN server_addr;
	enum {
			 SMTP_STATE_INITIAL
			,SMTP_STATE_HELO
			,SMTP_STATE_MAIL_FROM
			,SMTP_STATE_RCPT_TO
			,SMTP_STATE_DATA_HEADER
			,SMTP_STATE_DATA_BODY

	} state = SMTP_STATE_INITIAL;

	thread_up();

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
		sockprintf(socket,"421 System error");
		mail_close_socket(socket);
		thread_down();
		return;
	} 

	memset(&msg,0,sizeof(msg));
	memset(&user,0,sizeof(user));

	strcpy(host_ip,inet_ntoa(smtp.client_addr.sin_addr));

	lprintf("%04d SMTP connection accepted from: %s port %u"
		, socket, host_ip, ntohs(smtp.client_addr.sin_port));

	if(startup->options&MAIL_OPT_NO_HOST_LOOKUP)
		host=NULL;
	else
		host=gethostbyaddr ((char *)&smtp.client_addr.sin_addr
			,sizeof(smtp.client_addr.sin_addr),AF_INET);

	if(host!=NULL && host->h_name!=NULL)
		sprintf(host_name,"%.*s",(int)sizeof(host_name)-1,host->h_name);
	else
		strcpy(host_name,"<no name>");

	lprintf("%04d SMTP host name: %s", socket, host_name);

	strcpy(hello_name,host_name);

	if(trashcan(&scfg,host_ip,"ip")) {
		lprintf("%04d !SMTP SERVER BLOCKED in ip.can: %s"
			,socket, host_ip);
		sockprintf(socket,"550 Access denied.");
		mail_close_socket(socket);
		thread_down();
		return;
	}

	if(trashcan(&scfg,host_name,"host")) {
		lprintf("%04d !SMTP SERVER BLOCKED in host.can: %s"
			,socket, host_name);
		sockprintf(socket,"550 Access denied.");
		mail_close_socket(socket);
		thread_down();
		return;
	}

	/*  SPAM Filters (mail-abuse.org) */
	if(startup->options&MAIL_OPT_USE_RBL
		&& rblchk(smtp.client_addr.sin_addr.s_addr
			,"blackholes.mail-abuse.org"
			)==TRUE) {
		lprintf("%04d !SMTP SPAM server filtered (RBL): %s [%s]"
			,socket, host_name, host_ip);
		spamlog(&scfg, "Listed on RBL (http://mail-abuse.org/rbl)"
			,host_name, host_ip);
		sockprintf(socket
			,"571 Mail from %s refused, see http://mail-abuse.org/rbl"
			,host_ip);
		mail_close_socket(socket);
		thread_down();
		return;
	}
	if(startup->options&MAIL_OPT_USE_DUL
		&& rblchk(smtp.client_addr.sin_addr.s_addr
			,"dialups.mail-abuse.org"
			)==TRUE) {
		lprintf("%04d !SMTP SPAM server filtered (DUL): %s [%s]"
			,socket, host_name, host_ip);
		spamlog(&scfg, "Listed on DUL (http://mail-abuse.org/dul)"
			,host_name, host_ip);
		sockprintf(socket
			,"571 Mail from %s refused, see http://mail-abuse.org/dul"
			,host_ip);
		mail_close_socket(socket);
		thread_down();
		return;
	}
	if(startup->options&MAIL_OPT_USE_RSS
		&& rblchk(smtp.client_addr.sin_addr.s_addr
			,"relays.mail-abuse.org"
			)==TRUE) {
		lprintf("%04d !SMTP SPAM server filtered (RSS): %s [%s]"
			,socket, host_name, host_ip);
		spamlog(&scfg, "Listed on RSS (http://mail-abuse.org/rss)"
			,host_name, host_ip);
		sockprintf(socket
			,"571 Mail from %s refused, see http://mail-abuse.org/rss"
			,host_ip);					
		mail_close_socket(socket);
		thread_down();
		return;
	}

	sprintf(rcptlst_fname,"%sSMTP%d.LST", scfg.data_dir, socket);
	rcptlst=fopen(rcptlst_fname,"w+");
	if(rcptlst==NULL) {
		lprintf("%04d !SMTP ERROR %d creating recipient list: %s"
			,socket, errno, rcptlst_fname);
		sockprintf(socket,"421 System error");
		mail_close_socket(socket);
		thread_down();
		return;
	}

	if(trashcan(&scfg,host_name,"smtpspy") 
		|| trashcan(&scfg,host_ip,"smtpspy")) {
		sprintf(str,"%sSMTPSPY.TXT", scfg.data_dir);
		spy=fopen(str,"a");
	}

	active_clients++;
	update_clients();

	/* Initialize client display */
	client.size=sizeof(client);
	client.time=time(NULL);
	sprintf(client.addr,"%.*s",(int)sizeof(client.addr)-1,host_ip);
	sprintf(client.host,"%.*s",(int)sizeof(client.host)-1,host_name);
	client.port=ntohs(smtp.client_addr.sin_port);
	client.protocol="SMTP";
	client.user="<unknown>";
	client_on(socket,&client);

	sprintf(str,"SMTP: %s",host_ip);
	status(str);

	sockprintf(socket,"220 %s Synchronet SMTP Server for %s v%s Ready"
		,scfg.sys_inetaddr,PLATFORM_DESC,MAIL_VERSION);
	while(1) {
		rd = sockreadline(socket, buf, sizeof(buf));
		if(rd<1) 
			break;
		truncsp(buf);
		if(spy!=NULL)
			fprintf(spy,"%s\n",buf);
		if(state>=SMTP_STATE_DATA_HEADER) {
			if(!strcmp(buf,".")) {
				lprintf("%04d SMTP End of message (%lu bytes)", socket, ftell(msgtxt));

				if(sender[0]==0) {
					lprintf("%04d !SMTP MISSING mail header 'FROM' field", socket);
					sockprintf(socket, "554 Mail header missing 'FROM' field");
					state=SMTP_STATE_HELO;
					continue;
				}

				if(msgtxt!=NULL) {
					rewind(msgtxt);
					sprintf(smb.file,"%smail", scfg.data_dir);
					smb.retry_time=scfg.smb_retry_time;
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
							lprintf("%04d !SMTP ERROR %d creating %s"
								,socket, i, smb.file);
							sockprintf(socket, "452 Insufficient system storage");
							continue;
						} 
					}

					if((i=smb_locksmbhdr(&smb))!=0) {
						smb_close(&smb);
						lprintf("%04d !SMTP ERROR %d locking %s"
							,socket, i, smb.file);
						sockprintf(socket, "452 Insufficient system storage");
						continue; }

					length=filelength(fileno(msgtxt))+2;	 /* +2 for translation string */

					if((i=smb_open_da(&smb))!=0) {
						smb_unlocksmbhdr(&smb);
						smb_close(&smb);
						lprintf("%04d !SMTP ERROR %d (%s) opening %s.sda"
							,socket, i, smb.last_error, smb.file);
						sockprintf(socket, "452 Insufficient system storage");
						continue; }

					if(scfg.sys_misc&SM_FASTMAIL)
						offset=smb_fallocdat(&smb,length,1);
					else
						offset=smb_allocdat(&smb,length,1);
					smb_close_da(&smb);

					smb_fseek(smb.sdt_fp,offset,SEEK_SET);
					xlat=XLAT_NONE;
					smb_fwrite(&xlat,2,smb.sdt_fp);
					x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
					while(!feof(msgtxt)) {
						memset(buf,0,x);
						j=fread(buf,1,x,msgtxt);
						if(j<1)
							break;
						if((j!=x || feof(msgtxt)) && buf[j-1]=='\n' && buf[j-2]=='\r')
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
							smb_close(&smb);
							lprintf("%04d !SMTP DUPLICATE MESSAGE", socket);
							sockprintf(socket, "554 Duplicate Message");
							continue; 
						} 
					}

					msg.hdr.offset=offset;

					smb_dfield(&msg,TEXT_BODY,length);

					smb_unlocksmbhdr(&smb);
					if(msg.idx.subj==0) {
						p="";
						smb_hfield(&msg, SUBJECT, 0, p);
						msg.idx.subj=crc16(p);
					}
					rewind(rcptlst);
					while(!feof(rcptlst)) {
						if((i=smb_copymsgmem(&newmsg,&msg))!=0) {
							lprintf("%04d !SMTP ERROR %d copying message"
								,socket, i);
							break;
						}
						if(fgets(str,sizeof(str)-1,rcptlst)==NULL)
							break;
						usernum=atoi(str);
						fgets(rcpt_name,sizeof(rcpt_name)-1,rcptlst);
						if(fgets(rcpt_addr,sizeof(rcpt_addr)-1,rcptlst)==NULL)
							break;
						truncsp(rcpt_name);
						truncsp(rcpt_addr);

						snprintf(hdrfield,sizeof(hdrfield),
							"Received: from %s (%s [%s])\r\n"
							"          by %s [%s] (Synchronet Mail Server for %s v%s) with %s\r\n"
							"          for %s; %s"
							,host_name,hello_name,host_ip
							,scfg.sys_inetaddr,inet_ntoa(server_addr.sin_addr)
							,PLATFORM_DESC,MAIL_VERSION
							,esmtp ? "ESMTP" : "SMTP"
							,rcpt_addr,msgdate(msg.hdr.when_imported,date));
						smb_hfield(&newmsg, RFC822HEADER, (ushort)strlen(hdrfield), hdrfield);

						smb_hfield(&newmsg, RECIPIENT, (ushort)strlen(rcpt_name), rcpt_name);

						if(rcpt_addr[0]=='#') {	/* Local destination */
							newmsg.idx.to=atoi(rcpt_addr+1);
							smb_hfield(&newmsg, RECIPIENTEXT
								,(ushort)strlen(rcpt_addr+1), rcpt_addr+1);
						} else {
							newmsg.idx.to=0;
							nettype=NET_INTERNET;
							smb_hfield(&newmsg, RECIPIENTNETTYPE, nettype, &nettype);
							smb_hfield(&newmsg, RECIPIENTNETADDR
								,(ushort)strlen(rcpt_addr), rcpt_addr);
						}

						i=smb_addmsghdr(&smb,&newmsg,SMB_SELFPACK);
						smb_freemsgmem(&newmsg);
						if(i!=0) {
							lprintf("%04d !SMTP ERROR %d adding message header"
								,socket, i);
							break;
						}
						lprintf("%04d SMTP Created message #%ld from %s to %s <%s>"
							,socket, newmsg.hdr.number, sender, rcpt_name, rcpt_addr);
						if(usernum) {
							sprintf(str,"\7\1n\1hOn %.24s\r\n\1m%s \1n\1msent you e-mail from: "
								"\1h%s\1n\r\n"
								,ctime((const time_t*)&newmsg.hdr.when_imported.time)
								,sender,sender_addr);
							if(!newmsg.idx.to) {	/* Forwarding */
								strcat(str,"\1mand it was automatically forwaded to: \1h");
								strcat(str,user.netmail);
								strcat(str,"\1n\r\n");
							}
							putsmsg(&scfg, usernum, str);
						}
					}
					smb_close(&smb);
					if(i) {
						smb_freemsgdat(&smb,offset,length,1);
						sockprintf(socket, "452 Insufficient system storage");
						continue;
					}
				}
				sockprintf(socket,SMTP_OK);
				state=SMTP_STATE_HELO;
				continue;
			}
			if(buf[0]==0 && state==SMTP_STATE_DATA_HEADER) {	
				state=SMTP_STATE_DATA_BODY;	/* Null line separates header and body */
				continue;
			}
			if(state==SMTP_STATE_DATA_BODY) {
				p=buf;
				if(*p=='.') p++;	/* Transparency (RFC821 4.5.2) */
				if(msgtxt!=NULL) 
					fprintf(msgtxt, "%s\r\n", p);
				continue;
			}
			/* RFC822 Header parsing */
			if(startup->options&MAIL_OPT_DEBUG_RX_HEADER)
				lprintf("%04d SMTP %s",socket, buf);

			if(!strnicmp(buf, "SUBJECT:",8)) {
				p=buf+8;
				while(*p && *p<=' ') p++;
				smb_hfield(&msg, SUBJECT, (ushort)strlen(p), p);
				strlwr(p);
				msg.idx.subj=crc16(p);
				continue;
			}
			if(!strnicmp(buf, "TO:",3)) {
				p=buf+3;
				while(*p && *p<=' ') p++;
				if(*p=='<')  {
					p++;
					tp=strrchr(p,'>');
					if(tp) *tp=0;
				}
				continue;
			}
			if(!strnicmp(buf, "REPLY-TO:",9)) {
				p=buf+9;
				while(*p && *p<=' ') p++;
				if(*p=='<')  {
					p++;
					tp=strchr(p,'>');
					if(tp) *tp=0;
				}
				nettype=NET_INTERNET;
				smb_hfield(&msg, REPLYTONETTYPE, nettype, &nettype);
				smb_hfield(&msg, REPLYTONETADDR, (ushort)strlen(p), p);
				continue;
			}
			if(!strnicmp(buf, "FROM:",5)) {
				p=strchr(buf+5,'<');
				if(p) {
					p++;
					tp=strchr(p,'>');
					if(tp) *tp=0;
					sprintf(sender_addr,"%.*s",(int)sizeof(sender_addr)-1,p);
				} else {
					p=buf+5;
					while(*p && *p<=' ') p++;
					sprintf(sender_addr,"%.*s",(int)sizeof(sender_addr)-1,p);
				}
				nettype=NET_INTERNET;
				smb_hfield(&msg, SENDERNETTYPE, sizeof(nettype), &nettype);
				smb_hfield(&msg, SENDERNETADDR, (ushort)strlen(sender_addr), sender_addr);
			
				p=buf+5;
				while(*p && *p<=' ') p++;
				if(*p=='"') { 
					p++;
					tp=strchr(p,'"');
				} else if(*p=='<') {
					p++;
					tp=strchr(p,'>');
				} else
					tp=strchr(p,'<');
				if(tp) *tp=0;
				truncsp(p);
				sprintf(sender,"%.*s",(int)sizeof(sender)-1,p);
				smb_hfield(&msg, SENDER, (ushort)strlen(sender), sender);
				continue;
			}
			if(!strnicmp(buf, "DATE:",5)) {
				p=buf+5;
				while(*p && *p<=' ') p++;
/*				lprintf("Sent: %s",p); */
				memset(&tm,0,sizeof(tm));
				while(*p && !isdigit(*p)) p++;
				/* DAY */
				tm.tm_mday=atoi(p);
				while(*p && isdigit(*p)) p++;
				/* MONTH */
				while(*p && *p<=' ') p++;
				sprintf(month,"%3.3s",p);
				if(!stricmp(month,"jan"))
					tm.tm_mon=0;
				else if(!stricmp(month,"feb"))
					tm.tm_mon=1;
				else if(!stricmp(month,"mar"))
					tm.tm_mon=2;
				else if(!stricmp(month,"apr"))
					tm.tm_mon=3;
				else if(!stricmp(month,"may"))
					tm.tm_mon=4;
				else if(!stricmp(month,"jun"))
					tm.tm_mon=5;
				else if(!stricmp(month,"jul"))
					tm.tm_mon=6;
				else if(!stricmp(month,"aug"))
					tm.tm_mon=7;
				else if(!stricmp(month,"sep"))
					tm.tm_mon=8;
				else if(!stricmp(month,"oct"))
					tm.tm_mon=9;
				else if(!stricmp(month,"nov"))
					tm.tm_mon=10;
				else
					tm.tm_mon=11;
				p+=4;
				/* YEAR */
				tm.tm_year=atoi(p);
				if(tm.tm_year>1900)
					tm.tm_year-=1900;
				while(*p && isdigit(*p)) p++;
				/* HOUR */
				while(*p && *p<=' ') p++;
				tm.tm_hour=atoi(p);
				while(*p && isdigit(*p)) p++;
				/* MINUTE */
				if(*p) p++;
				tm.tm_min=atoi(p);
				while(*p && isdigit(*p)) p++;
				/* SECONDS */
				if(*p) p++;
				tm.tm_sec=atoi(p);
				while(*p && isdigit(*p)) p++;
				/* TIME ZONE */
				while(*p && *p<=' ') p++;
				if(*p && (isdigit(*p) || *p=='-')) { /* HHMM or -HHMM format */
					sprintf(str,"%.*s",*p=='-'? 3:2,p);
					msg.hdr.when_written.zone=atoi(str)*60;
					p+=(*p=='-') ? 3:2;
					msg.hdr.when_written.zone+=atoi(p);
				}
				else if(!strnicmp(p,"PDT",3))
					msg.hdr.when_written.zone=(short)PDT;
				else if(!strnicmp(p,"MDT",3))
					msg.hdr.when_written.zone=(short)MDT;
				else if(!strnicmp(p,"CDT",3))
					msg.hdr.when_written.zone=(short)CDT;
				else if(!strnicmp(p,"EDT",3))
					msg.hdr.when_written.zone=(short)EDT;
				else if(!strnicmp(p,"PST",3))
					msg.hdr.when_written.zone=(short)PST;
				else if(!strnicmp(p,"MST",3))
					msg.hdr.when_written.zone=(short)MST;
				else if(!strnicmp(p,"CST",3))
					msg.hdr.when_written.zone=(short)CST;
				else if(!strnicmp(p,"EST",3))
					msg.hdr.when_written.zone=(short)EST;
				msg.hdr.when_written.time=mktime(&tm);
				continue;
			}
			if(!strnicmp(buf, "MESSAGE-ID:",11)) {
				p=buf+11;
				while(*p && *p<=' ') p++;
				smb_hfield(&msg, RFC822MSGID, (ushort)strlen(p), p);
				continue;
			}
			if(!strnicmp(buf, "IN-REPLY-TO:",12)) {
				p=buf+12;
				while(*p && *p<=' ') p++;
				smb_hfield(&msg, RFC822REPLYID, (ushort)strlen(p), p);
				continue;
			}
			/* Fall-through */
			smb_hfield(&msg, RFC822HEADER, (ushort)strlen(buf), buf);
			continue;
		}
		lprintf("%04d SMTP RX: %s", socket, buf);
		if(!strnicmp(buf,"HELO",4)) {
			p=buf+4;
			while(*p && *p<=' ') p++;
			sprintf(hello_name,"%.*s",(int)sizeof(hello_name)-1,p);
			sockprintf(socket,"250 %s",scfg.sys_inetaddr);
			esmtp=FALSE;
			state=SMTP_STATE_HELO;
			continue;
		}
		if(!strnicmp(buf,"EHLO",4)) {
			p=buf+4;
			while(*p && *p<=' ') p++;
			sprintf(hello_name,"%.*s",(int)sizeof(hello_name)-1,p);
			sockprintf(socket,"250 %s",scfg.sys_inetaddr);
			esmtp=TRUE;
			state=SMTP_STATE_HELO;
			continue;
		}
		if(!stricmp(buf,"QUIT")) {
			sockprintf(socket,"221 %s Service closing transmission channel",scfg.sys_inetaddr);
			break;
		} 
		if(!stricmp(buf,"NOOP")) {
			sockprintf(socket, SMTP_OK);
			continue;
		}
		if(state<SMTP_STATE_HELO) {
			/* RFC 821 4.1.1 "The first command in a session must be the HELO command." */
			lprintf("%04d !SMTP MISSING 'HELO' command",socket);
			sockprintf(socket, SMTP_BADSEQ);
			continue;
		}
		if(!stricmp(buf,"TURN")) {
			sockprintf(socket,"502 command not supported");
			continue;
		}
		if(!stricmp(buf,"RSET")) {
			smb_freemsgmem(&msg);
			memset(&msg,0,sizeof(smbmsg_t));		/* Initialize message header */
			reverse_path[0]=0;
			sender[0]=0;
			sender_addr[0]=0;
			sockprintf(socket,SMTP_OK);
			rewind(rcptlst);
			chsize(fileno(rcptlst),0);
			state=SMTP_STATE_HELO;
			continue;
		}
		if(!strnicmp(buf,"MAIL FROM:",10)) {
			smb_freemsgmem(&msg);
			memset(&msg,0,sizeof(smbmsg_t));		/* Initialize message header */
			memcpy(msg.hdr.id,"SHD\x1a",4);
			msg.hdr.version=smb_ver();
			msg.hdr.when_imported.time=time(NULL);
			msg.hdr.when_imported.zone=scfg.sys_timezone;
			p=buf+10;
			while(*p && *p<=' ') p++;
			sprintf(reverse_path,"%.*s",(int)sizeof(reverse_path)-1,p);
			sender[0]=0;
			sender_addr[0]=0;
			if(spy==NULL && trashcan(&scfg,reverse_path,"smtpspy")) {
				sprintf(str,"%sSMTPSPY.TXT", scfg.data_dir);
				spy=fopen(str,"a");
			}
			sockprintf(socket,SMTP_OK);
			state=SMTP_STATE_MAIL_FROM;
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



		if(!strnicmp(buf,"RCPT TO:",8)) {

			if(state<SMTP_STATE_MAIL_FROM) {
				lprintf("%04d !SMTP MISSING 'MAIL' command",socket);
				sockprintf(socket, SMTP_BADSEQ);
				continue;
			}

			sprintf(str,"%.*s",(int)sizeof(str)-1,buf+8);
			p=str;
			while(*p && *p<=' ') p++;

			if(*p=='<') p++;				/* Skip '<' */
			tp=strchr(str,'>');				/* Truncate '>' */
			if(tp!=NULL) *tp=0;

			tp=strrchr(str,'@');
			if(tp!=NULL) {
				
				/* RELAY */
				sprintf(domain_list,"%sdomains.cfg",scfg.ctrl_dir);
				sprintf(relay_list,"%srelay.cfg",scfg.ctrl_dir);
				if(stricmp(tp+1,scfg.sys_inetaddr)!=0
					&& resolve_ip(tp+1)!=server_addr.sin_addr.s_addr
					&& findstr(&scfg,tp+1,domain_list)==FALSE) {

					if(!findstr(&scfg,host_name,relay_list) && 
						!findstr(&scfg,host_ip,relay_list)) {
						lprintf("%04d !SMTP ILLEGAL RELAY ATTEMPT from %s [%s] to %s"
							,socket, host_name, host_ip, tp+1);
						sockprintf(socket, "550 Relay not allowed.");
						continue;
					}

					lprintf("%04d SMTP Relaying to external mail service: %s"
						,socket, tp+1);

					fprintf(rcptlst,"0\n%.*s\n%.*s\n"
						,(int)sizeof(rcpt_name)-1,p
						,(int)sizeof(rcpt_addr)-1,p);
					
					sockprintf(socket,SMTP_OK);
					state=SMTP_STATE_RCPT_TO;
					continue;
				}
				*tp=0;	/* truncate at '@' */
			}
			while(*p && !isalnum(*p)) p++;	/* Skip '<' or '"' */
			tp=strrchr(p,'"');	
			if(tp!=NULL) *tp=0;	/* truncate at '"' */

			p=alias(p,alias_buf);

											/* RX by sysop alias */
			if(!stricmp(p,"SYSOP") || !stricmp(p,scfg.sys_id) 
				|| !stricmp(p,"POSTMASTER") || !stricmp(p,scfg.sys_op))
				usernum=1;
			else if(startup->options&MAIL_OPT_ALLOW_RX_BY_NUMBER 
				&& isdigit(*p)) {
				usernum=atoi(p);			/* RX by user number */
				/* verify usernum */
				username(&scfg,usernum,str);
				if(!str[0] || !stricmp(str,"DELETED USER"))
					usernum=0;
				p=str;
			} else {
				usernum=matchuser(&scfg,p);	/* RX by "user alias", "user.alias" or "user_alias" */

				if(!usernum) { /* RX by "real name", "real.name", or "sysop.alias" */
					
					/* convert "user.name" to "user name" */
					sprintf(rcpt_name,"%.*s",sizeof(rcpt_name)-1,p);
					for(tp=rcpt_name;*tp;tp++)	
						if(*tp=='.') *tp=' ';

					if(!stricmp(rcpt_name,scfg.sys_op))
						usernum=1;			/* RX by "sysop.alias" */

					if(!usernum)			/* RX by "real name" */
						usernum=userdatdupe(&scfg, 0, U_NAME, LEN_NAME, p, FALSE);

					if(!usernum)			/* RX by "real.name" */
						usernum=userdatdupe(&scfg, 0, U_NAME, LEN_NAME, rcpt_name, FALSE);
				}
			}
			if(!usernum) {
				lprintf("%04d !SMTP UNKNOWN USER: %s", socket, buf+8);
				sockprintf(socket, "550 Unknown User: %s", buf+8);
				continue;
			}
			user.number=usernum;
			if((i=getuserdat(&scfg, &user))!=0) {
				lprintf("%04d !SMTP ERROR %d getting data on user #%u (%s)"
					,socket, i, usernum, p);
				sockprintf(socket, "550 Unknown User: %s", buf+8);
				continue;
			}
			if(user.misc&(DELETED|INACTIVE)) {
				lprintf("%04d !SMTP DELETED or INACTIVE user #%u (%s)"
					,socket, usernum, p);
				sockprintf(socket, "550 Unknown User: %s", buf+8);
				continue;
			}
#if 0	/* implement later */
			if(useron.etoday>=cfg.level_emailperday[useron.level]
				&& !(useron.rest&FLAG('Q'))) {
				bputs(text[TooManyEmailsToday]);
				continue; }
#endif

			fprintf(rcptlst,"%u\n%.*s\n",user.number,(int)sizeof(rcpt_name)-1,p);

			/* Forward to Internet */
			tp=strrchr(user.netmail,'@');
			if(scfg.sys_misc&SM_FWDTONET && user.misc&NETMAIL 
				&& tp && strchr(tp,'.') && !strchr(tp,'/') 
				&& !strstr(tp,scfg.sys_inetaddr)) {
				lprintf("%04d SMTP Forwarding to: %s"
					,socket, user.netmail);
				fprintf(rcptlst,"%s\n",user.netmail);
				sockprintf(socket,"251 User not local; will forward to %s", user.netmail);
			} else { /* Local (no-forward) */
				fprintf(rcptlst,"#%u\n",usernum);
				sockprintf(socket,SMTP_OK);
			}
			state=SMTP_STATE_RCPT_TO;
			continue;
		}
		if(!strnicmp(buf,"DATA",4)) {
			if(state<SMTP_STATE_RCPT_TO) {
				lprintf("%04d !SMTP MISSING 'RCPT TO' command", socket);
				sockprintf(socket, SMTP_BADSEQ);
				continue;
			}
			if(msgtxt!=NULL)
				fclose(msgtxt);
			sprintf(msgtxt_fname,"%sSMTP%d.RX", scfg.data_dir, socket);
			if((msgtxt=fopen(msgtxt_fname,"w+b"))==NULL) {
				lprintf("%04d !SMTP ERROR %d opening %s"
					,socket, errno, msgtxt_fname);
				sockprintf(socket, "452 Insufficient system storage");
				continue;
			}
			sockprintf(socket, "354 send the mail data, end with <CRLF>.<CRLF>");
			state=SMTP_STATE_DATA_HEADER;
			continue;
		}
		sockprintf(socket,"500 Syntax error");
		lprintf("%04d !SMTP UNSUPPORTED COMMAND: '%s'", socket, buf);
	}

	/* Free up resources here */
	mail_close_socket(socket);

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

	lprintf("%04d SMTP RX Session thread terminated", socket);
	active_clients--;
	update_clients();
	client_off(socket);

	thread_down();
}

BOOL bounce(smb_t* smb, smbmsg_t* msg, char* err, BOOL immediate)
{
	char		str[128],full_err[512];
	int			i;
	ushort		agent=AGENT_PROCESS;
	smbmsg_t	newmsg;

	if((i=smb_lockmsghdr(smb,msg))!=0) {
		lprintf("0000 !BOUNCE ERROR %d locking message header #%lu"
			,i,msg->hdr.number);
		return(FALSE);
	}

	msg->hdr.delivery_attempts++;
	if((i=smb_putmsg(smb,msg))!=0) {
		lprintf("0000 !BOUNCE ERROR %d incrementing delivery attempt counter",i);
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
	
	lprintf("0000 !Bouncing message back to %s", msg->from);

	newmsg=*msg;
	/* Mark original message as deleted */
	msg->hdr.attr|=MSG_DELETE;
	msg->idx.attr=msg->hdr.attr;
	if((i=smb_putmsg(smb,msg))!=0) {
		lprintf("0000 !BOUNCE ERROR %d deleting message",i);
		smb_unlockmsghdr(smb,msg);
		return(FALSE);
	}
	smb_unlockmsghdr(smb,msg);

	newmsg.hfield=NULL;
	newmsg.hfield_dat=NULL;
	newmsg.total_hfields=0;
	newmsg.idx.to=newmsg.idx.from;
	newmsg.idx.from=0;
	newmsg.hdr.delivery_attempts=0;

	smb_hfield(&newmsg, RECIPIENT, (ushort)strlen(newmsg.from), newmsg.from);
	if(newmsg.idx.to) {
		sprintf(str,"%u",newmsg.idx.to);
		smb_hfield(&newmsg, RECIPIENTEXT, (ushort)strlen(str), str);
	}
	smb_hfield(&newmsg, RECIPIENTAGENT, sizeof(ushort), &newmsg.from_agent);
	smb_hfield(&newmsg, RECIPIENTNETTYPE, sizeof(newmsg.from_net.type), &newmsg.from_net.type);
	if(newmsg.from_net.type) 
		smb_hfield(&newmsg, RECIPIENTNETADDR, (ushort)strlen(newmsg.from_net.addr)
			,newmsg.from_net.addr);
	strcpy(str,"Mail Delivery Subsystem");
	smb_hfield(&newmsg, SENDER, (ushort)strlen(str), str);
	smb_hfield(&newmsg, SENDERAGENT, sizeof(agent), &agent);
	
	/* Put error message in subject for now */
	sprintf(full_err,"Delivery failure of message to %s after %u attempts: %s"
		,(char*)msg->to_net.addr, msg->hdr.delivery_attempts, err);
	smb_hfield(&newmsg, SUBJECT, (ushort)strlen(full_err), full_err);

	if((i=smb_addmsghdr(smb,&newmsg,SMB_SELFPACK))!=0)
		lprintf("0000 !BOUNCE ERROR %d adding message header",i);

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
	char		fromaddr[256];
	char*		server;
	char*		msgtxt=NULL;
	char*		p;
	ulong		offset;
	ulong		last_msg=0;
	ulong		total_msgs;
	ulong		ip_addr;
	ulong		dns;
	BOOL		success;
	SOCKET		sock=INVALID_SOCKET;
	SOCKADDR_IN	addr;
	SOCKADDR_IN	server_addr;
	time_t		last_scan=0;
	smb_t		smb;
	smbmsg_t	msg;

	sendmail_running=TRUE;

	thread_up();

	lprintf("0000 SendMail thread started");

	memset(&msg,0,sizeof(msg));
	memset(&smb,0,sizeof(smb));

	while(server_socket!=INVALID_SOCKET) {

		if(startup->options&MAIL_OPT_NO_SENDMAIL) {
			mswait(1000);
			continue;
		}

		if(active_sendmail!=0) {
			active_sendmail=0;
			update_clients();
		}

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

		mswait(3000);

		sprintf(smb.file,"%smail",scfg.data_dir);
		if((i=smb_open(&smb))!=0) 
			continue;
		if((i=smb_locksmbhdr(&smb))!=0)
			continue;
		if((i=smb_getstatus(&smb))!=0) {
			smb_unlocksmbhdr(&smb);
			continue;
		}
		smb_unlocksmbhdr(&smb);
		if(smb.status.last_msg==last_msg && time(NULL)-last_scan<startup->rescan_frequency)
			continue;
		last_msg=smb.status.last_msg;
		last_scan=time(NULL);
		total_msgs=smb.status.total_msgs;
		smb_rewind(smb.sid_fp);
		for(offset=0;offset<total_msgs;offset++) {

			if(server_socket==INVALID_SOCKET)	/* server stopped */
				break;

			if(active_sendmail!=0) {
				active_sendmail=0;
				update_clients();
			}

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
				lprintf("0000 !SEND ERROR %d locking message header #%lu"
					,i,msg.idx.number);
				continue;
			}
			if((i=smb_getmsghdr(&smb,&msg))!=0) {
				smb_unlockmsghdr(&smb,&msg);
				lprintf("0000 !SEND ERROR %d reading message header #%lu"
					,i,msg.idx.number);
				continue; 
			}
			smb_unlockmsghdr(&smb,&msg);

			if(msg.to_net.type!=NET_INTERNET) 
				continue;

			active_sendmail=1;
			update_clients();

			lprintf("0000 SEND Message #%lu from %s to %s"
				,msg.hdr.number, msg.from, msg.to_net.addr);
			status("SendMail");
#ifdef _WIN32
			if(startup->outbound_sound[0] && !(startup->options&MAIL_OPT_MUTE)) 
				PlaySound(startup->outbound_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

			lprintf("0000 SEND getting message text");
			if((msgtxt=smb_getmsgtxt(&smb,&msg,GETMSGTXT_TAILS))==NULL) {
				lprintf("0000 !SEND ERROR retrieving message text");
				continue;
			}

			if(startup->options&MAIL_OPT_RELAY_TX)  
				server=startup->relay_server;
			else {
				sprintf(to,"%.*s",(int)sizeof(to)-1,(char*)msg.to_net.addr);
				p=strrchr(to,'>');	/* Truncate '>' */
				if(p!=NULL) *p=0;

				p=strrchr(to,'@');
				if(p==NULL) {
					lprintf("0000 !SEND INVALID destination address: %s", to);
					sprintf(err,"Invalid destination address: %s", to);
					bounce(&smb,&msg,err,TRUE);
					continue;
				}
				if((dns=resolve_ip(startup->dns_server))==0) 
					continue;
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


			lprintf("0000 SEND opening socket");
			if((sock=mail_open_socket(SOCK_STREAM))==INVALID_SOCKET) {
				lprintf("0000 !SEND ERROR %d opening socket", ERROR_VALUE);
				continue;
			}

			lprintf("%04d SEND socket opened",sock);

			memset(&addr,0,sizeof(addr));
			addr.sin_addr.s_addr = htonl(startup->interface_addr);
			addr.sin_family = AF_INET;

			lprintf("%04d SEND binding socket",sock);
			if((i=bind(sock, (struct sockaddr *) &addr, sizeof (addr)))!=0) {
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
				
				lprintf("%04d SEND resolving SMTP host name: %s", sock, server);
				ip_addr=resolve_ip(server);
				if(!ip_addr)  {
					sprintf(err,"Failed to resolve SMTP host name: %s",server);
					continue;
				}

				memset(&server_addr,0,sizeof(server_addr));
				server_addr.sin_addr.s_addr = ip_addr;
				server_addr.sin_family = AF_INET;
				if(startup->options&MAIL_OPT_RELAY_TX)
					server_addr.sin_port = htons(startup->relay_port);
				else
					server_addr.sin_port = htons(IPPORT_SMTP);
				
				lprintf("%04d SEND connecting to port %u on %s [%s]"
					,sock
					,ntohs(server_addr.sin_port)
					,server,inet_ntoa(server_addr.sin_addr));
				if((i=connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)))!=0) {
					lprintf("%04d !SEND ERROR %d (%d) connecting to SMTP server: %s"
						,sock
						,i,ERROR_VALUE, server);
					sprintf(err,"Error %d connecting to SMTP server: %s"
						,(int)ERROR_VALUE,server);
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
				sprintf(err,"%s replied with '%s' instead of 220",server,buf);
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			sockprintf(sock,"HELO %s",scfg.sys_inetaddr);
			if(!sockgetrsp(sock,"250", buf, sizeof(buf))) {
				sprintf(err,"%s replied with '%s' instead of 250",server,buf);
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			/* MAIL */
			if(msg.from_net.type==NET_INTERNET)
				strcpy(fromaddr,msg.from_net.addr);
			else 
				usermailaddr(&scfg,fromaddr,msg.from);
			if(fromaddr[0]=='<')
				sockprintf(sock,"MAIL FROM: %s",fromaddr);
			else
				sockprintf(sock,"MAIL FROM: <%s>",fromaddr);
			if(!sockgetrsp(sock,"250", buf, sizeof(buf))) {
				sprintf(err,"%s replied with '%s' instead of 250",server,buf);
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			/* RCPT */
			if(*((char*)msg.to_net.addr)=='<')
				sockprintf(sock,"RCPT TO: %s", (char*)msg.to_net.addr);
			else
				sockprintf(sock,"RCPT TO: <%s>", (char*)msg.to_net.addr);
			if(!sockgetrsp(sock,"25", buf, sizeof(buf))) {
				sprintf(err,"%s replied with '%s' instead of 25*",server,buf);
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			/* DATA */
			sockprintf(sock,"DATA");
			if(!sockgetrsp(sock,"354", buf, sizeof(buf))) {
				sprintf(err,"%s replied with '%s' instead of 354",server,buf);
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			lprintf("%04d SEND sending message text (%u bytes)"
				,sock, strlen(msgtxt));
			sockmsgtxt(sock,&msg,msgtxt,fromaddr,-1);
			if(!sockgetrsp(sock,"250", buf, sizeof(buf))) {
				sprintf(err,"%s replied with '%s' instead of 250",server,buf);
				bounce(&smb,&msg,err,buf[0]=='5');
				continue;
			}
			lprintf("%04d SEND message transfer complete", sock);

			msg.hdr.attr|=MSG_DELETE;
			msg.idx.attr=msg.hdr.attr;
			if((i=smb_lockmsghdr(&smb,&msg))!=0) 
				lprintf("%04d !SEND ERROR %d locking message header #%lu"
					,sock
					,i,msg.hdr.number);
			if((i=smb_putmsg(&smb,&msg))!=0)
				lprintf("%04d !SEND ERROR %d deleting message #%lu"
					,sock
					,i,msg.hdr.number);
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

	lprintf("0000 SendMail thread terminated");

	sendmail_running=FALSE;

	thread_down();
}

void DLLCALL mail_terminate(void)
{
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

    lprintf("#### Mail Server thread terminated");
	status("Down");
	if(startup!=NULL && startup->terminated!=NULL)
		startup->terminated(code);
	thread_down();
}

const char* DLLCALL mail_ver(void)
{
	static char ver[256];
	char compiler[32];

	COMPILER_DESC(compiler);

	sprintf(ver,"Synchronet Mail Server v%s%s  SMBLIB v%s  "
		"Compiled %s %s with %s"
		,MAIL_VERSION
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
	char			compiler[32];
	SOCKADDR_IN		server_addr;
	SOCKADDR_IN		client_addr;
	int				client_addr_len;
	SOCKET			client_socket;
	int				i;
	int				result;
	ulong			l;
	time_t			t;
	time_t			start;
	LINGER			linger;
	fd_set			socket_set;
	SOCKET			high_socket_set;
	pop3_t*			pop3;
	smtp_t*			smtp;
	struct timeval	tv;

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
	if(startup->pop3_port==0)				startup->pop3_port=110;
	if(startup->rescan_frequency==0)		startup->rescan_frequency=3600;	/* 60 minutes */
	if(startup->max_delivery_attempts==0)	startup->max_delivery_attempts=50;
	if(startup->max_inactivity==0) 			startup->max_inactivity=120; /* seconds */

	thread_up();

	status("Initializing");

    memset(&scfg, 0, sizeof(scfg));

#ifdef __unix__		/* Ignore "Broken Pipe" signal */
	signal(SIGPIPE,SIG_IGN);
#endif

	lprintf("Synchronet Mail Server Version %s%s"
		,MAIL_VERSION
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		);

	COMPILER_DESC(compiler);

	lprintf("Compiled %s %s with %s", __DATE__, __TIME__, compiler);

	lprintf("SMBLIB v%s (format %x.%02x)",smb_lib_ver(),smb_ver()>>8,smb_ver()&0xff);

	srand(time(NULL));

	if(!(startup->options&MAIL_OPT_LOCAL_TIMEZONE)) {
		if(PUTENV("TZ=UCT0"))
			lprintf("!putenv() FAILED");
		tzset();

		if((t=checktime())!=0) {   /* Check binary time */
			lprintf("!TIME PROBLEM (%ld)",t);
			cleanup(1);
			return;
		}
	}

	if(!winsock_startup()) {
		cleanup(1);
		return;
	}

	t=time(NULL);
	lprintf("Initializing on %.24s with options: %lx"
		,ctime(&t),startup->options);

	/* Initial configuration and load from CNF files */
    sprintf(scfg.ctrl_dir, "%.*s", (int)sizeof(scfg.ctrl_dir)-1
    	,startup->ctrl_dir);
    lprintf("Loading configuration files from %s", scfg.ctrl_dir);
	scfg.size=sizeof(scfg);
	if(!load_cfg(&scfg, NULL, TRUE)) {
		lprintf("!Failed to load configuration files");
		cleanup(1);
		return;
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

	if (server_socket == INVALID_SOCKET) {
		lprintf("!ERROR %d opening socket", ERROR_VALUE);
		cleanup(1);
		return;
	}

    lprintf("%04d SMTP socket opened",server_socket);

#if 1
	linger.l_onoff=TRUE;
    linger.l_linger=5;	/* seconds */

	result = setsockopt (server_socket, SOL_SOCKET, SO_LINGER
    	,(char *)&linger, sizeof(linger));

	if (result != 0) {
		lprintf("%04d !ERROR %d (%d) setting socket options"
			,server_socket, result, ERROR_VALUE);
		cleanup(1);
		return;
	}
#endif

	/*****************************/
	/* Listen for incoming calls */
	/*****************************/
    memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_addr.s_addr = htonl(startup->interface_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(startup->smtp_port);

    result = bind(server_socket, (struct sockaddr *) &server_addr
    	,sizeof (server_addr));

	if (result != 0) {
		lprintf("%04d !ERROR %d (%d) binding SMTP socket to port %u"
			,server_socket, result, ERROR_VALUE, startup->smtp_port);
		lprintf("!Another service may be using this port");
		cleanup(1);
		return;
	}

    lprintf("%04d SMTP socket bound to port %u"
		,server_socket, startup->smtp_port);

    result = listen (server_socket, 1);

	if (result != 0) {
		lprintf("%04d !ERROR %d (%d) listening on socket"
			,server_socket, result, ERROR_VALUE);
		cleanup(1);
		return;
	}

	if(startup->options&MAIL_OPT_ALLOW_POP3) {

		/* open a socket and wait for a client */

		pop3_socket = mail_open_socket(SOCK_STREAM);

		if (pop3_socket == INVALID_SOCKET) {
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

		result = bind(pop3_socket, (struct sockaddr *) &server_addr
    		,sizeof (server_addr));

		if (result != 0) {
			lprintf("%04d !ERROR %d (%d) binding POP3 socket to port %u"
				,pop3_socket, result, ERROR_VALUE, startup->pop3_port);
			lprintf("!Another service may be using this port");
			cleanup(1);
			return;
		}

	    lprintf("%04d POP3 socket bound to port %u"
			,pop3_socket, startup->pop3_port);

		result = listen (pop3_socket, 1);

		if (result != 0) {
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

	while (server_socket!=INVALID_SOCKET) {

		/* now wait for connection */

		FD_ZERO(&socket_set);
		FD_SET(server_socket,&socket_set);
		high_socket_set=server_socket+1;
		if(startup->options&MAIL_OPT_ALLOW_POP3) {
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


		if(FD_ISSET(server_socket,&socket_set)) {

			client_addr_len = sizeof (client_addr);
			client_socket = accept(server_socket, (struct sockaddr *)&client_addr
        		,&client_addr_len);

			if (client_socket == INVALID_SOCKET)
			{
				if(ERROR_VALUE == ENOTSOCK)
            		lprintf("%04d SMTP socket closed while listening",server_socket);
				else
					lprintf("%04d !ERROR %d accept failed", server_socket, ERROR_VALUE);
				break;
			}
			if(startup->socket_open!=NULL)
				startup->socket_open(TRUE);
			sockets++;

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
		}

		if(FD_ISSET(pop3_socket,&socket_set)) {

			client_addr_len = sizeof (client_addr);
			client_socket = accept(pop3_socket, (struct sockaddr *)&client_addr
        		,&client_addr_len);

			if (client_socket == INVALID_SOCKET)
			{
				if(ERROR_VALUE == ENOTSOCK)
            		lprintf("%04d POP3 socket closed while listening",pop3_socket);
				else
					lprintf("%04d !ERROR %d accept failed", pop3_socket, ERROR_VALUE);
				break;
			}
			if(startup->socket_open!=NULL)
				startup->socket_open(TRUE);
			sockets++;

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
}