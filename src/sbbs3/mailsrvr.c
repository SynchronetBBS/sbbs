/* Synchronet Mail (SMTP/POP3) server and sendmail threads */

/* $Id$ */
// vi: tabstop=4

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

/* ANSI C Library headers */
#include <limits.h>			/* UINT_MAX */
#include <stdio.h>
#include <stdlib.h>			/* ltoa in GNU C lib */
#include <stdarg.h>			/* va_list */
#include <string.h>			/* strrchr */
#include <ctype.h>			/* isdigit */
#include <fcntl.h>			/* Open flags */
#include <errno.h>			/* errno */

/* Synchronet-specific headers */
#undef SBBS	/* this shouldn't be defined unless building sbbs.dll/libsbbs.so */
#include "sbbs.h"
#include "mailsrvr.h"
#include "mime.h"
#include "md5.h"
#include "crc32.h"
#include "base64.h"
#include "ini_file.h"
#include "netwrap.h"	/* getNameServerList() */
#include "xpendian.h"
#include "js_rtpool.h"
#include "js_request.h"
#include "multisock.h"
#include "ssl.h"
#include "cryptlib.h"

/* Constants */
static const char*	server_name="Synchronet Mail Server";
#define FORWARD			"forward:"
#define NO_FORWARD		"local:"
#define NO_SPAM			"#nospam"

int dns_getmx(char* name, char* mx, char* mx2
			  ,DWORD intf, DWORD ip_addr, BOOL use_tcp, int timeout);

static char* pop_err	=	"-ERR";
static char* ok_rsp		=	"250 OK";
static char* auth_ok	=	"235 User Authenticated";
static char* sys_error	=	"421 System error";
static char* sys_unavail=	"421 System unavailable, try again later";
static char* insuf_stor =	"452 Insufficient system storage";
static char* badarg_rsp =	"501 Bad argument";
static char* badseq_rsp	=	"503 Bad sequence of commands";
static char* badauth_rsp=	"535 Authentication failure";
static char* badrsp_err	=	"%s replied with:\r\n\"%s\"\r\n"
							"instead of the expected reply:\r\n\"%s ...\"";

#define TIMEOUT_THREAD_WAIT		60		/* Seconds */
#define DNSBL_THROTTLE_VALUE	1000	/* Milliseconds */

#define STATUS_WFC	"Listening"

static mail_startup_t* startup=NULL;
static scfg_t	scfg;
static struct xpms_set	*mail_set=NULL;
static BOOL terminated=FALSE;
static protected_uint32_t active_clients;
static protected_uint32_t thread_count;
static volatile int		active_sendmail=0;
static volatile BOOL	sendmail_running=FALSE;
static volatile BOOL	terminate_server=FALSE;
static volatile BOOL	terminate_sendmail=FALSE;
static sem_t	sendmail_wakeup_sem;
static char		revision[16];
static volatile time_t	uptime;
static str_list_t recycle_semfiles;
static str_list_t shutdown_semfiles;
static int		mailproc_count;
static js_server_props_t js_server_props;

struct {
	volatile ulong	sockets;
	volatile ulong	errors;
	volatile ulong	crit_errors;
	volatile ulong	connections_ignored;
	volatile ulong	connections_refused;
	volatile ulong	connections_served;
	volatile ulong	pop3_served;
	volatile ulong	smtp_served;
	/* SMTP: */
	volatile ulong	sessions_refused;
	volatile ulong	msgs_ignored;
	volatile ulong	msgs_refused;
	volatile ulong	msgs_received;
} stats;

struct mailproc {
	char		name[INI_MAX_VALUE_LEN];
	char		cmdline[INI_MAX_VALUE_LEN];
	char		eval[INI_MAX_VALUE_LEN];
	str_list_t	to;
	str_list_t	from;
	BOOL		passthru;
	BOOL		native;
	BOOL		ignore_on_error;	/* Ignore mail message if cmdline fails */
	BOOL		disabled;
	BOOL		process_spam;
	BOOL		process_dnsbl;
	uint8_t*	ar;
	ulong		handled;			/* counter (for stats display) */
} *mailproc_list;

typedef struct {
	SOCKET			socket;
	union xp_sockaddr	client_addr;
	socklen_t		client_addr_len;
	BOOL			tls_port;
} smtp_t,pop3_t;

static int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

	va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);

	if(level <= LOG_ERR) {
		char errmsg[sizeof(sbuf)+16];
		SAFEPRINTF(errmsg, "mail %s", sbuf);
		errorlog(&scfg,startup==NULL ? NULL:startup->host_name,errmsg), stats.errors++;
		if(startup!=NULL && startup->errormsg!=NULL)
			startup->errormsg(startup->cbdata,level,errmsg);
	}

	if(level <= LOG_CRIT)
		stats.crit_errors++;

    if(startup==NULL || startup->lputs==NULL || level > startup->log_level)
		return(0);

#if defined(_WIN32)
	if(IsBadCodePtr((FARPROC)startup->lputs))
		return(0);
#endif

    return(startup->lputs(startup->cbdata,level,sbuf));
}

#ifdef _WINSOCKAPI_

static WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		lprintf(LOG_DEBUG,"%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized=TRUE;
		return (TRUE);
	}

    lprintf(LOG_CRIT,"!WinSock startup ERROR %d", status);
	return (FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)
#define SOCKLIB_DESC NULL

#endif

static void update_clients(void)
{
	if(startup!=NULL && startup->clients!=NULL)
		startup->clients(startup->cbdata,protected_uint32_value(active_clients)+active_sendmail);
}

static void client_on(SOCKET sock, client_t* client, BOOL update)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(startup->cbdata,TRUE,sock,client,update);
}

static void client_off(SOCKET sock)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(startup->cbdata,FALSE,sock,NULL,FALSE);
}

static void thread_up(BOOL setuid)
{
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,TRUE,setuid);
}

static int32_t thread_down(void)
{
	int32_t count = protected_uint32_adjust(&thread_count,-1);
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,FALSE,FALSE);
	return count;
}

void mail_open_socket(SOCKET sock, void* cb_protocol)
{
	char	*protocol=(char *)cb_protocol;
	char	error[256];
	char	section[128];

	if(startup!=NULL && startup->socket_open!=NULL)
		startup->socket_open(startup->cbdata,TRUE);
	SAFEPRINTF(section,"mail|%s",protocol);
	if(set_socket_options(&scfg, sock, section, error, sizeof(error)))
		lprintf(LOG_ERR,"%04d !ERROR %s",sock,error);

	stats.sockets++;
}

void mail_close_socket_cb(SOCKET sock, void* cb_protocol)
{
	if(startup!=NULL && startup->socket_open!=NULL)
		startup->socket_open(startup->cbdata,FALSE);
	stats.sockets--;
}

int mail_close_socket(SOCKET sock)
{
	int		result;

	if(sock==INVALID_SOCKET)
		return(-1);

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	if(startup!=NULL && startup->socket_open!=NULL)
		startup->socket_open(startup->cbdata,FALSE);
	stats.sockets--;
	if(result!=0) {
		if(ERROR_VALUE!=ENOTSOCK)
			lprintf(LOG_WARNING,"%04d !ERROR %d closing socket",sock, ERROR_VALUE);
	}
#if 0 /*def _DEBUG */
	else 
		lprintf(LOG_DEBUG,"%04d Socket closed (%d sockets in use)",sock,stats.sockets);
#endif

	return(result);
}

static void status(char* str)
{
	if(startup!=NULL && startup->status!=NULL)
	    startup->status(startup->cbdata,str);
}

int sockprintf(SOCKET sock, CRYPT_SESSION sess, char *fmt, ...)
{
	int		len;
	int		maxlen;
	int		result;
	va_list argptr;
	char	sbuf[1024];
	fd_set	socket_set;
	struct timeval tv;

    va_start(argptr,fmt);
    len=vsnprintf(sbuf,maxlen=sizeof(sbuf)-2,fmt,argptr);
    va_end(argptr);

	if(len<0 || len > maxlen) /* format error or output truncated */
		len=maxlen;
	if(startup->options&MAIL_OPT_DEBUG_TX)
		lprintf(LOG_DEBUG,"%04d TX%s: %.*s", sock, sess != -1 ? "S" : "", len, sbuf);
	memcpy(sbuf+len,"\r\n",2);
	len+=2;

	if(sock==INVALID_SOCKET) {
		lprintf(LOG_WARNING,"!INVALID SOCKET in call to sockprintf");
		return(0);
	}

	/* Check socket for writability (using select) */
	tv.tv_sec=300;
	tv.tv_usec=0;

	FD_ZERO(&socket_set);
	FD_SET(sock,&socket_set);

	if((result=select(sock+1,NULL,&socket_set,NULL,&tv))<1) {
		if(result==0)
			lprintf(LOG_NOTICE,"%04d !TIMEOUT selecting socket for send"
				,sock);
		else
			lprintf(LOG_NOTICE,"%04d !ERROR %d selecting socket for send"
				,sock, ERROR_VALUE);
		return(0);
	}

	if (sess != -1) {
		int tls_sent;
		int sent = 0;
		while (sent < len) {
			result = cryptPushData(sess, sbuf+sent, len-sent, &tls_sent);
			if (result == CRYPT_OK)
				sent += tls_sent;
			else
				return 0;
		}
		if (cryptFlushData(sess) != CRYPT_OK)
			return 0;
	}
	else {
		// It looks like this could stutter on partial sends -- Deuce
		while((result=sendsocket(sock,sbuf,len))!=len) {
			if(result==SOCKET_ERROR) {
				if(ERROR_VALUE==EWOULDBLOCK) {
					YIELD();
					continue;
				}
				if(ERROR_VALUE==ECONNRESET) 
					lprintf(LOG_NOTICE,"%04d Connection reset by peer on send",sock);
				else if(ERROR_VALUE==ECONNABORTED) 
					lprintf(LOG_NOTICE,"%04d Connection aborted by peer on send",sock);
				else
					lprintf(LOG_NOTICE,"%04d !ERROR %d sending on socket",sock,ERROR_VALUE);
				return(0);
			}
			lprintf(LOG_WARNING,"%04d !ERROR: short send on socket: %d instead of %d",sock,result,len);
		}
	}
	return(len);
}

static void sockerror(SOCKET socket, int rd, const char* action)
{
	if(rd==0) 
		lprintf(LOG_NOTICE,"%04d Socket closed by peer on %s"
			,socket, action);
	else if(rd==SOCKET_ERROR) {
		if(ERROR_VALUE==ECONNRESET) 
			lprintf(LOG_NOTICE,"%04d Connection reset by peer on %s"
				,socket, action);
		else if(ERROR_VALUE==ECONNABORTED) 
			lprintf(LOG_NOTICE,"%04d Connection aborted by peer on %s"
				,socket, action);
		else
			lprintf(LOG_NOTICE,"%04d !SOCKET ERROR %d on %s"
				,socket, ERROR_VALUE, action);
	} else
		lprintf(LOG_WARNING,"%04d !SOCKET ERROR: unexpected return value %d from %s"
			,socket, rd, action);
}

static int sock_recvbyte(SOCKET sock, CRYPT_SESSION sess, char *buf, time_t start)
{
	int len=0;
	fd_set	socket_set;
	struct	timeval	tv;
	int ret;
	int i;

	if (sess > -1) {
		while (1) {
			ret = cryptPopData(sess, buf, 1, &len);
			switch(ret) {
				case CRYPT_OK:
					break;
				case CRYPT_ERROR_TIMEOUT:
					lprintf(LOG_WARNING,"%04d !TIMEOUT in sock_recvbyte (%u seconds):  INACTIVE SOCKET"
						,sock, startup->max_inactivity);
					return -1;
				case CRYPT_ERROR_COMPLETE:
					return 0;
				default:
					lprintf(LOG_WARNING,"%04d !Cryptlib error in sock_recvbyte:  %d", sock, ret);
					if (ret < -1)
						return ret;
					return -2;
			}
			if (len)
				return len;
			if(startup->max_inactivity && (time(NULL)-start)>startup->max_inactivity) {
				lprintf(LOG_WARNING,"%04d !TIMEOUT in sock_recvbyte (%u seconds):  INACTIVE SOCKET"
					,sock, startup->max_inactivity);
				return(-1);
			}
		}
	}
	else {
		tv.tv_sec=startup->max_inactivity;
		tv.tv_usec=0;

		FD_ZERO(&socket_set);
		FD_SET(sock,&socket_set);

		i=select(sock+1,&socket_set,NULL,NULL,&tv);

		if(i<1) {
			if(i==0) {
				if(startup->max_inactivity && (time(NULL)-start)>startup->max_inactivity) {
					lprintf(LOG_WARNING,"%04d !TIMEOUT in sock_recvbyte (%u seconds):  INACTIVE SOCKET"
						,sock, startup->max_inactivity);
					return(-1);
				}
				return 0;
			}
			sockerror(sock,i,"select");
			return 0;
		}
		i=recv(sock, buf, 1, 0);
		if(i<1)
			sockerror(sock,i,"receive");
		return i;
	}
}

static int sockreadline(SOCKET socket, CRYPT_SESSION sess, char* buf, int len)
{
	char	ch;
	int		i,rd=0;
	time_t	start;

	buf[0]=0;

	start=time(NULL);

	if(socket==INVALID_SOCKET) {
		lprintf(LOG_WARNING,"!INVALID SOCKET in call to sockreadline");
		return(-1);
	}
	
	while(rd<len-1) {

		if(terminated || terminate_server) {
			lprintf(LOG_WARNING,"%04d !ABORTING sockreadline",socket);
			return(-1);
		}

		i = sock_recvbyte(socket, sess, &ch, start);

		if(i<1)
			return(-1);

		if(ch=='\n' /* && rd>=1 */ ) { /* Mar-9-2003: terminate on sole LF */
			break;
		}
		buf[rd++]=ch;
	}
	if(rd>0 && buf[rd-1]=='\r')
		rd--;
	buf[rd]=0;
	
	return(rd);
}

static BOOL sockgetrsp(SOCKET socket, CRYPT_SESSION sess, char* rsp, char *buf, int len)
{
	int rd;

	while(1) {
		rd = sockreadline(socket, sess, buf, len);
		if(rd<1) {
			if(rd==0 && rsp != NULL)
				lprintf(LOG_WARNING,"%04d !RECEIVED BLANK RESPONSE, Expected '%s'", socket, rsp);
			return(FALSE);
		}
		if(buf[3]=='-')	{ /* Multi-line response */
			if(startup->options&MAIL_OPT_DEBUG_RX_RSP) 
				lprintf(LOG_DEBUG,"%04d RX%s: %s",socket, sess != -1 ? "S" : "", buf);
			continue;
		}
		if(rsp!=NULL && strnicmp(buf,rsp,strlen(rsp))) {
			lprintf(LOG_WARNING,"%04d !INVALID RESPONSE: '%s' Expected: '%s'", socket, buf, rsp);
			return(FALSE);
		}
		break;
	}
	if(startup->options&MAIL_OPT_DEBUG_RX_RSP) 
		lprintf(LOG_DEBUG,"%04d RX%s: %s",socket,sess != -1 ? "S" : "", buf);
	return(TRUE);
}

static int sockgetrsp_opt(SOCKET socket, CRYPT_SESSION sess, char* rsp, char *opt, char *buf, int len)
{
	int rd;
	int ret = 0;
	size_t moptlen;
	char *mopt;

	moptlen = strlen(rsp)+strlen(opt) + 1;
	mopt = malloc(moptlen+1);
	if (mopt == NULL)
		return -1;
	sprintf(mopt, "%s-%s", rsp, opt);
	while(1) {
		rd = sockreadline(socket, sess, buf, len);
		if(rd<1) {
			if(rd==0)
				lprintf(LOG_WARNING,"%04d !RECEIVED BLANK RESPONSE, Expected '%s'", socket, rsp);
			free(mopt);
			return(-1);
		}
		if(buf[3]=='-')	{ /* Multi-line response */
			if (strncmp(buf, mopt, moptlen) == 0)
				ret = 1;
			if(startup->options&MAIL_OPT_DEBUG_RX_RSP) 
				lprintf(LOG_DEBUG,"%04d RX%s: %s",socket, sess != -1 ? "S" : "", buf);
			continue;
		}
		if(rsp!=NULL && strnicmp(buf,rsp,strlen(rsp))) {
			lprintf(LOG_WARNING,"%04d !INVALID RESPONSE: '%s' Expected: '%s'", socket, buf, rsp);
			free(mopt);
			return(-1);
		}
		break;
	}
	mopt[3] = ' ';
	if (strncmp(buf, mopt, moptlen) == 0)
	free(mopt);
	if(startup->options&MAIL_OPT_DEBUG_RX_RSP)
		lprintf(LOG_DEBUG,"%04d RX%s: %s",socket,sess != -1 ? "S" : "", buf);
	return(ret);
}

/* non-standard, but documented (mostly) in draft-newman-msgheader-originfo-05 */
void originator_info(SOCKET socket, CRYPT_SESSION sess, smbmsg_t* msg)
{
	char* user		= msg->from_ext;
	char* login		= smb_get_hfield(msg,SENDERUSERID,NULL);
	char* server	= smb_get_hfield(msg,SENDERSERVER,NULL);
	char* client	= smb_get_hfield(msg,SENDERHOSTNAME,NULL);
	char* addr		= smb_get_hfield(msg,SENDERIPADDR,NULL);
	char* prot		= smb_get_hfield(msg,SENDERPROTOCOL,NULL);
	char* port		= smb_get_hfield(msg,SENDERPORT,NULL);
	char* time		= smb_get_hfield(msg,SENDERTIME,NULL);

	if(user || login || server || client || addr || prot || port || time)
		sockprintf(socket, sess
			,"X-Originator-Info: account=%s; login-id=%s; server=%s; client=%s; addr=%s; prot=%s; port=%s; time=%s"
			,user
			,login
			,server
			,client
			,addr
			,prot
			,port
			,time
			);
}

/* RFC822: The maximum total length of a text line including the
   <CRLF> is 1000 characters (but not counting the leading
   dot duplicated for transparency). 

   POP3 (RFC1939) actually calls for a 512 byte line length limit!
*/
#define MAX_LINE_LEN	998		

static ulong sockmimetext(SOCKET socket, CRYPT_SESSION sess, smbmsg_t* msg, char* msgtxt, ulong maxlines
						  ,str_list_t file_list, char* mime_boundary)
{
	char		toaddr[256]="";
	char		fromaddr[256]="";
	char		fromhost[256];
	char		msgid[256];
	char		date[64];
	uchar*		p;
	char*		np;
	char*		content_type=NULL;
	int			i;
	int			s;
	ulong		lines;
	int			len,tlen;

	/* HEADERS (in recommended order per RFC822 4.1) */

	if(msg->reverse_path!=NULL)
		if(!sockprintf(socket,sess,"Return-Path: %s", msg->reverse_path))
			return(0);

	for(i=0;i<msg->total_hfields;i++)
		if(msg->hfield[i].type == SMTPRECEIVED && msg->hfield_dat[i]!=NULL) 
			if(!sockprintf(socket,sess,"Received: %s", msg->hfield_dat[i]))
				return(0);

	if(!sockprintf(socket,sess,"Date: %s",msgdate(msg->hdr.when_written,date)))
		return(0);

	if((p=smb_get_hfield(msg,RFC822FROM,NULL))!=NULL)
		s=sockprintf(socket,sess,"From: %s",p);	/* use original RFC822 header field */
	else {
		char fromname[256];
		SAFEPRINTF(fromname, "\"%s\"", msg->from);
		if(msg->from_net.type==NET_QWK && msg->from_net.addr!=NULL)
			SAFEPRINTF2(fromaddr,"%s!%s"
				,(char*)msg->from_net.addr
				,usermailaddr(&scfg,fromhost,msg->from));
		else if(msg->from_net.type==NET_FIDO && msg->from_net.addr!=NULL) {
			faddr_t* faddr = (faddr_t *)msg->from_net.addr;
			char faddrstr[128];
			SAFEPRINTF2(fromname,"\"%s\" (%s)", msg->from, smb_faddrtoa(faddr, NULL));
			if(faddr->point)
				SAFEPRINTF4(faddrstr,"p%hu.f%hu.n%hu.z%hu"FIDO_TLD
					,faddr->point, faddr->node, faddr->net, faddr->zone);
			else
				SAFEPRINTF3(faddrstr,"f%hu.n%hu.z%hu"FIDO_TLD
					,faddr->node, faddr->net, faddr->zone);
			SAFEPRINTF2(fromaddr,"%s@%s", usermailaddr(NULL,fromhost,msg->from), faddrstr);
		} else if(msg->from_net.type!=NET_NONE && msg->from_net.addr!=NULL)
			SAFECOPY(fromaddr,(char*)msg->from_net.addr);
		else 
			usermailaddr(&scfg,fromaddr,msg->from);
		if(fromaddr[0]=='<')
			s=sockprintf(socket,sess,"From: %s %s",fromname,fromaddr);
		else
			s=sockprintf(socket,sess,"From: %s <%s>",fromname,fromaddr);
	}
	if(!s)
		return(0);

	if(msg->from_org!=NULL || msg->from_net.type==NET_NONE)
		if(!sockprintf(socket,sess,"Organization: %s"
			,msg->from_org==NULL ? scfg.sys_name : msg->from_org))
			return(0);

	if(!sockprintf(socket,sess,"Subject: %s",msg->subj))
		return(0);

	if((p=smb_get_hfield(msg,RFC822TO,NULL))!=NULL)
		s=sockprintf(socket,sess,"To: %s",p);	/* use original RFC822 header field */
	else {
		if(strchr(msg->to,'@')!=NULL || msg->to_net.addr==NULL)
			s=sockprintf(socket,sess,"To: %s",msg->to);	/* Avoid double-@ */
		else if(msg->to_net.type==NET_INTERNET || msg->to_net.type==NET_QWK) {
			if(strchr((char*)msg->to_net.addr,'<')!=NULL)
				s=sockprintf(socket,sess,"To: %s",(char*)msg->to_net.addr);
			else
				s=sockprintf(socket,sess,"To: \"%s\" <%s>",msg->to,(char*)msg->to_net.addr);
		} else if(msg->to_net.type==NET_FIDO) {
			s=sockprintf(socket,sess,"To: \"%s\" (%s)",msg->to, smb_faddrtoa((fidoaddr_t*)msg->to_net.addr, NULL));
		} else {
			usermailaddr(&scfg,toaddr,msg->to);
			s=sockprintf(socket,sess,"To: \"%s\" <%s>",msg->to,toaddr);
		}
	}
	if(!s)
		return(0);
	if((p=smb_get_hfield(msg,SMB_CARBONCOPY,NULL))!=NULL)
		if(!sockprintf(socket,sess,"CC: %s",p))
			return(0);
	np=NULL;
	if((p=smb_get_hfield(msg,RFC822REPLYTO,NULL))==NULL) {
		np=msg->replyto;
		if(msg->replyto_net.type==NET_INTERNET)
			p=msg->replyto_net.addr;
	}
	if(p!=NULL) {
		if(np!=NULL)
			s=sockprintf(socket,sess,"Reply-To: \"%s\" <%s>",np,p);
		else 
			s=sockprintf(socket,sess,"Reply-To: %s",p);
	}
	if(!s)
		return(0);
	if(!sockprintf(socket,sess,"Message-ID: %s",get_msgid(&scfg,INVALID_SUB,msg,msgid,sizeof(msgid))))
		return(0);
	if(msg->reply_id!=NULL)
		if(!sockprintf(socket,sess,"In-Reply-To: %s",msg->reply_id))
			return(0);

	originator_info(socket, sess, msg);

	/* Include all possible FidoNet header fields here */
	for(i=0;i<msg->total_hfields;i++) {
		switch(msg->hfield[i].type) {
			case FIDOCTRL:
			case FIDOAREA:	
			case FIDOSEENBY:
			case FIDOPATH:
			case FIDOMSGID:
			case FIDOREPLYID:
			case FIDOPID:
			case FIDOFLAGS:
			case FIDOTID:
				if(!sockprintf(socket,sess, "%s: %s", smb_hfieldtype(msg->hfield[i].type), (char*)msg->hfield_dat[i]))
					return(0);
				break;
		}
	}
	for(i=0;i<msg->total_hfields;i++) { 
		if(msg->hfield[i].type==RFC822HEADER) { 
			if(strnicmp((char*)msg->hfield_dat[i],"Content-Type:",13)==0)
				content_type=msg->hfield_dat[i];
			if(!sockprintf(socket,sess,"%s",(char*)msg->hfield_dat[i]))
				return(0);
        }
    }
	/* Default MIME Content-Type for non-Internet messages */
	if(msg->from_net.type!=NET_INTERNET && content_type==NULL && startup->default_charset[0]) {
		/* No content-type specified, so assume IBM code-page 437 (full ex-ASCII) */
		sockprintf(socket,sess,"Content-Type: text/plain; charset=%s", startup->default_charset);
		sockprintf(socket,sess,"Content-Transfer-Encoding: 8bit");
	}

	if(strListCount(file_list)) {	/* File attachments */
        mimeheaders(socket,sess,mime_boundary);
        sockprintf(socket,sess,"");
        mimeblurb(socket,sess,mime_boundary);
        sockprintf(socket,sess,"");
        mimetextpartheader(socket, sess,mime_boundary, startup->default_charset);
	}
	if(!sockprintf(socket,sess,""))	/* Header Terminator */
		return(0);

	/* MESSAGE BODY */
	lines=0;
	np=msgtxt;
	while(*np && lines<maxlines) {
		len=0;
		while(len<MAX_LINE_LEN && *(np+len)!=0 && *(np+len)!='\n')
			len++;

		tlen=len;
		while(tlen && *(np+(tlen-1))<=' ') /* Takes care of '\r' or spaces */
			tlen--;

		if(!sockprintf(socket,sess, "%s%.*s", *np=='.' ? ".":"", tlen, np))
			break;
		lines++;
		if(*(np+len)=='\r')
			len++;
		if(*(np+len)=='\n')
			len++;
		np+=len;
		/* release time-slices every x lines */
		if(startup->lines_per_yield
			&& !(lines%startup->lines_per_yield))	
			YIELD();
	}
	if(file_list!=NULL) {
		for(i=0;file_list[i];i++) { 
			sockprintf(socket,sess,"");
			lprintf(LOG_INFO,"%04u MIME Encoding and sending %s",socket,file_list[i]);
			if(!mimeattach(socket,sess,mime_boundary,file_list[i]))
				lprintf(LOG_ERR,"%04u !ERROR opening/encoding/sending %s",socket,file_list[i]);
			else {
				endmime(socket,sess,mime_boundary);
				if(msg->hdr.auxattr&MSG_KILLFILE)
					if(remove(file_list[i])!=0)
						lprintf(LOG_WARNING,"%04u !ERROR %d removing %s",socket,errno,file_list[i]);
			}
		}
	}
    sockprintf(socket,sess,".");	/* End of text */
	return(lines);
}

static ulong sockmsgtxt(SOCKET socket, CRYPT_SESSION sess, smbmsg_t* msg, char* msgtxt, ulong maxlines)
{
	char		filepath[MAX_PATH+1];
	ulong		retval;
	char*		boundary=NULL;
	unsigned	i;
	str_list_t	file_list=NULL;
	str_list_t	split;

	if(msg->hdr.auxattr&MSG_FILEATTACH) {

		boundary = mimegetboundary();
		file_list = strListInit();

		/* Parse header fields */
		for(i=0;i<msg->total_hfields;i++)
	        if(msg->hfield[i].type==FILEATTACH) 
				strListPush(&file_list,(char*)msg->hfield_dat[i]);

		/* Parse subject (if necessary) */
		if(!strListCount(file_list)) {	/* filename(s) stored in subject */
			split=strListSplitCopy(NULL,msg->subj," ");
			if(split!=NULL) {
				for(i=0;split[i];i++) {
					if(msg->idx.to!=0)
						SAFEPRINTF3(filepath,"%sfile/%04u.in/%s"
							,scfg.data_dir,msg->idx.to,getfname(truncsp(split[i])));
					else
						SAFEPRINTF3(filepath,"%sfile/%04u.out/%s"
							,scfg.data_dir,msg->idx.from,getfname(truncsp(split[i])));
					strListPush(&file_list,filepath);
				}
				strListFree(&split);
			}
		}
    }

	retval = sockmimetext(socket,sess,msg,msgtxt,maxlines,file_list,boundary);

	strListFree(&file_list);

	if(boundary!=NULL)
		free(boundary);

	return(retval);
}

static u_long resolve_ip(const char *inaddr)
{
	char*		p;
	char*		addr;
	char		buf[128];
	HOSTENT*	host;

	SAFECOPY(buf,inaddr);
	addr=buf;
	if(*addr=='[' && *(p=lastchar(addr))==']') { /* Support [ip_address] notation */
		addr++;
		*p=0;
	}

	if(*addr==0)
		return((u_long)INADDR_NONE);

	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit((uchar)*p))
			break;
	if(!(*p))
		return(inet_addr(addr));

	if((host=gethostbyname(inaddr))==NULL)
		return((u_long)INADDR_NONE);

	return(*((ulong*)host->h_addr_list[0]));
}

/****************************************************************************/
/* Consecutive failed login (possible password hack) attempt tracking		*/
/****************************************************************************/
/* Counter is global so it is tracked between multiple connections.			*/
/* Failed consecutive login attempts > 10 will generate a hacklog entry	and	*/
/* immediately disconnect (after the usual failed-login delay).				*/
/* A failed login from a different host resets the counter.					*/
/* A successful login from the same host resets the counter.				*/
/****************************************************************************/

static void badlogin(SOCKET sock, CRYPT_SESSION sess, const char* prot, const char* resp, char* user, char* passwd, char* host, union xp_sockaddr* addr)
{
	char	reason[128];
	char	ip[INET6_ADDRSTRLEN];
	ulong	count;
	
	if(user == NULL)
		user = "<unspecified>";

	if(addr!=NULL) {
		SAFEPRINTF(reason,"%s LOGIN", prot);
		count=loginFailure(startup->login_attempt_list, addr, prot, user, passwd);
		if(startup->login_attempt.hack_threshold && count>=startup->login_attempt.hack_threshold)
			hacklog(&scfg, reason, user, passwd, host, addr);
		inet_addrtop(addr, ip, sizeof(ip));
		if(startup->login_attempt.filter_threshold && count>=startup->login_attempt.filter_threshold)
			filter_ip(&scfg, (char*)prot, "- TOO MANY CONSECUTIVE FAILED LOGIN ATTEMPTS"
				,host, ip, user, /* fname: */NULL);
	}

	mswait(startup->login_attempt.delay);
	sockprintf(sock,sess,(char*)resp);
}

static void pop3_thread(void* arg)
{
	char*		p;
	char		str[128];
	char		buf[512];
	char		host_name[128];
	char		host_ip[INET6_ADDRSTRLEN];
	char		username[128];
	char		password[128];
	char		challenge[256];
	uchar		digest[MD5_DIGEST_SIZE];
	char*		response="";
	char*		msgtxt;
	int			i;
	int			rd;
	BOOL		activity=TRUE;
	BOOL		apop=FALSE;
	uint32_t	l;
	long		lm_mode = 0;
	ulong		lines;
	ulong		lines_sent;
	ulong		login_attempts;
	uint32_t	msgs;
	uint32_t	msgnum;
	ulong		bytes;
	SOCKET		socket;
	smb_t		smb;
	smbmsg_t	msg;
	user_t		user;
	client_t	client;
	mail_t*		mail;
	pop3_t		pop3=*(pop3_t*)arg;
	login_attempt_t attempted;
	CRYPT_SESSION	session = -1;
	BOOL nodelay=TRUE;
	ulong nb = 0;

	SetThreadName("sbbs/pop3");
	thread_up(TRUE /* setuid */);

	free(arg);

	socket=pop3.socket;

	if(startup->options&MAIL_OPT_DEBUG_POP3)
		lprintf(LOG_DEBUG,"%04d POP3 session thread started", socket);

#ifdef _WIN32
	if(startup->pop3_sound[0] && !(startup->options&MAIL_OPT_MUTE)) 
		PlaySound(startup->pop3_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

	inet_addrtop(&pop3.client_addr, host_ip, sizeof(host_ip));

	if(startup->options&MAIL_OPT_DEBUG_POP3)
		lprintf(LOG_INFO,"%04d POP3 connection accepted from: %s port %u"
			,socket, host_ip, inet_addrport(&pop3.client_addr));

	if(getnameinfo(&pop3.client_addr.addr, pop3.client_addr_len, host_name, sizeof(host_name), NULL, 0, (startup->options&MAIL_OPT_NO_HOST_LOOKUP)?NI_NUMERICHOST:0)!=0)
		SAFECOPY(host_name, "<no name>");

	if(!(startup->options&MAIL_OPT_NO_HOST_LOOKUP) && (startup->options&MAIL_OPT_DEBUG_POP3))
		lprintf(LOG_INFO,"%04d POP3 Hostname: %s", socket, host_name);

	if (pop3.tls_port) {
		if (get_ssl_cert(&scfg, NULL) == -1) {
			lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to get TLS certificate", socket, host_ip);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_SSL_SERVER) != CRYPT_OK) {
			lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to create TLS session", socket, host_ip);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (cryptSetAttribute(session, CRYPT_SESSINFO_SSL_OPTIONS, CRYPT_SSLOPTION_DISABLE_CERTVERIFY) != CRYPT_OK) {
			lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to disable certificate verification", socket, host_ip);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (cryptSetAttribute(session, CRYPT_SESSINFO_PRIVATEKEY, scfg.tls_certificate) != CRYPT_OK) {
			lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to set private key", socket, host_ip);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		nodelay = TRUE;
		setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));
		nb=0;
		ioctlsocket(socket,FIONBIO,&nb);
		if ((rd = cryptSetAttribute(session, CRYPT_SESSINFO_NETWORKSOCKET, socket)) != CRYPT_OK) {
			lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to set session socket (%d)", socket, host_ip, rd);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (cryptSetAttribute(session, CRYPT_SESSINFO_ACTIVE, 1) != CRYPT_OK) {
			lprintf(LOG_WARNING, "%04d !POP3 [%s] Unable to set session active", socket, host_ip);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (startup->max_inactivity) {
			if (cryptSetAttribute(session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity) != CRYPT_OK) {
				lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to set max inactivity", socket, host_ip);
				mail_close_socket(socket);
				thread_down();
				return;
			}
		}
	}

	ulong banned = loginBanned(&scfg, startup->login_attempt_list, socket, host_name, startup->login_attempt, &attempted);
	if(banned || trashcan(&scfg,host_ip,"ip")) {
		if(banned) {
			char ban_duration[128];
			lprintf(LOG_NOTICE, "%04d !POP3 TEMPORARY BAN of %s (%u login attempts, last: %s) - remaining: %s"
				,socket, host_ip, attempted.count-attempted.dupes, attempted.user, seconds_to_str(banned, ban_duration));
		}
		else
			lprintf(LOG_NOTICE,"%04d !POP3 CLIENT IP ADDRESS BLOCKED: %s",socket, host_ip);
		sockprintf(socket,session,"-ERR Access denied.");
		if (session != -1)
			cryptDestroySession(session);
		mail_close_socket(socket);
		thread_down();
		return;
	}

	if(trashcan(&scfg,host_name,"host")) {
		lprintf(LOG_NOTICE,"%04d !POP3 CLIENT HOSTNAME BLOCKED: %s"
			,socket, host_name);
		sockprintf(socket,session,"-ERR Access denied.");
		if (session != -1)
			cryptDestroySession(session);
		mail_close_socket(socket);
		thread_down();
		return;
	}

	protected_uint32_adjust(&active_clients, 1);
	update_clients();

	/* Initialize client display */
	client.size=sizeof(client);
	client.time=time32(NULL);
	SAFECOPY(client.addr,host_ip);
	SAFECOPY(client.host,host_name);
	client.port=inet_addrport(&pop3.client_addr);
	client.protocol="POP3";
	client.user=STR_UNKNOWN_USER;
	client_on(socket,&client,FALSE /* update */);

	SAFEPRINTF(str,"POP3: %s", host_ip);
	status(str);

	if(startup->login_attempt.throttle
		&& (login_attempts=loginAttempts(startup->login_attempt_list, &pop3.client_addr)) > 1) {
		lprintf(LOG_DEBUG,"%04d POP3 Throttling suspicious connection from: %s (%u login attempts)"
			,socket, host_ip, login_attempts);
		mswait(login_attempts*startup->login_attempt.throttle);
	}

	mail=NULL;

	do {
		memset(&smb,0,sizeof(smb));
		memset(&msg,0,sizeof(msg));
		memset(&user,0,sizeof(user));
		password[0]=0;

		srand((unsigned int)(time(NULL) ^ (time_t)GetCurrentThreadId()));	/* seed random number generator */
		rand();	/* throw-away first result */
		safe_snprintf(challenge,sizeof(challenge),"<%x%x%lx%lx@%.128s>"
			,rand(),socket,(ulong)time(NULL),clock(),startup->host_name);

		sockprintf(socket,session,"+OK Synchronet POP3 Server %s-%s Ready %s"
			,revision,PLATFORM_DESC,challenge);

		/* Requires USER or APOP command first */
		for(i=5;i;i--) {
			if(!sockgetrsp(socket,session,NULL,buf,sizeof(buf)))
				break;
			if(!strnicmp(buf,"USER ",5))
				break;
			else if(!strnicmp(buf,"APOP ",5)) {
				apop=TRUE;
				break;
			}
			else if (!stricmp(buf, "CAPA")) {
				// Capabilities
				sockprintf(socket,session, "+OK Capability list follows");
				sockprintf(socket,session, "TOP\r\nUSER\r\nPIPELINING\r\nUIDL\r\nIMPLEMENTATION Synchronet POP3 Server %s-%s\r\n%s.", revision, PLATFORM_DESC, (session != -1 || get_ssl_cert(&scfg, NULL) == -1) ? "" : "STLS\r\n");
				i++;
			}
			else if (!stricmp(buf, "STLS")) {
				if (get_ssl_cert(&scfg, NULL) == -1) {
					sockprintf(socket,session,"-ERR STLS command not supported");
					continue;
				}
				sockprintf(socket,session,"+OK Begin TLS negotiation");
				if (cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_SSL_SERVER) != CRYPT_OK) {
					lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to create TLS session", socket, host_ip);
					buf[0] = 0;
					break;
				}
				if (cryptSetAttribute(session, CRYPT_SESSINFO_SSL_OPTIONS, CRYPT_SSLOPTION_DISABLE_CERTVERIFY) != CRYPT_OK) {
					lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to disable certificate verification", socket, host_ip);
					buf[0] = 0;
					break;
				}
				if (cryptSetAttribute(session, CRYPT_SESSINFO_PRIVATEKEY, scfg.tls_certificate) != CRYPT_OK) {
					lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to set private key", socket, host_ip);
					buf[0] = 0;
					break;
				}
				nodelay = TRUE;
				setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));
				nb=0;
				ioctlsocket(socket,FIONBIO,&nb);
				if ((rd = cryptSetAttribute(session, CRYPT_SESSINFO_NETWORKSOCKET, socket)) != CRYPT_OK) {
					lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to set session socket (%d)", socket, host_ip, rd);
					buf[0] = 0;
					break;
				}
				if (cryptSetAttribute(session, CRYPT_SESSINFO_ACTIVE, 1) != CRYPT_OK) {
					lprintf(LOG_WARNING, "%04d !POP3 [%s] Unable to set session active", socket, host_ip);
					buf[0] = 0;
					break;
				}
				if (startup->max_inactivity) {
					if (cryptSetAttribute(session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity) != CRYPT_OK) {
						lprintf(LOG_ERR, "%04d !POP3 [%s] Unable to set max inactivity", socket, host_ip);
						buf[0] = 0;
						break;
					}
				}
				i++;
			}
			else
				sockprintf(socket,session,"-ERR USER, APOP, CAPA, or STLS command expected");
		}
		if(!i || buf[0]==0)	/* no USER or APOP command received */
			break;

		p=buf+5;
		SKIP_WHITESPACE(p);
		if(apop) {
			if((response=strrchr(p,' '))!=NULL)
				*(response++)=0;
			else
				response=p;
		}
		SAFECOPY(username,p);
		if((p = strstr(username, NO_SPAM)) != NULL) {
			*p = 0;
			lm_mode = LM_NOSPAM;
		} else
			lm_mode = 0;
		if(!apop) {
			sockprintf(socket,session,"+OK");
			if(!sockgetrsp(socket,session,"PASS ",buf,sizeof(buf))) {
				sockprintf(socket,session,"-ERR PASS command expected");
				break;
			}
			p=buf+5;
			SKIP_WHITESPACE(p);
			SAFECOPY(password,p);
		}
		user.number=matchuser(&scfg,username,FALSE /*sysop_alias*/);
		if(!user.number) {
			if(scfg.sys_misc&SM_ECHO_PW)
				lprintf(LOG_NOTICE,"%04d !POP3 [%s] UNKNOWN USER: '%s' (password: %s)"
					,socket, host_ip, username, password);
			else
				lprintf(LOG_NOTICE,"%04d !POP3 [%s] UNKNOWN USER: '%s'"
					,socket, host_ip, username);
			badlogin(socket, session, client.protocol, pop_err, username, password, host_name, &pop3.client_addr);
			break;
		}
		if((i=getuserdat(&scfg, &user))!=0) {
			lprintf(LOG_ERR,"%04d !POP3 [%s] ERROR %d getting data on user (%s)"
				,socket, host_ip, i, username);
			badlogin(socket, session, client.protocol, pop_err, NULL, NULL, NULL, NULL);
			break;
		}
		if(user.misc&(DELETED|INACTIVE)) {
			lprintf(LOG_NOTICE,"%04d !POP3 [%s] DELETED or INACTIVE user #%u (%s)"
				,socket, host_ip, user.number, username);
			badlogin(socket, session, client.protocol, pop_err, NULL, NULL, NULL, NULL);
			break;
		}
		if(apop) {
			strlwr(user.pass);	/* this is case-sensitive, so convert to lowercase */
			strcat(challenge,user.pass);
			MD5_calc(digest,challenge,strlen(challenge));
			MD5_hex((BYTE*)str,digest);
			if(strcmp(str,response)) {
				lprintf(LOG_NOTICE,"%04d !POP3 [%s] FAILED APOP authentication: %s"
					,socket, host_ip, username);
#if 0
				lprintf(LOG_DEBUG,"%04d !POP3 digest data: %s",socket,challenge);
				lprintf(LOG_DEBUG,"%04d !POP3 calc digest: %s",socket,str);
				lprintf(LOG_DEBUG,"%04d !POP3 resp digest: %s",socket,response);
#endif
				badlogin(socket, session, client.protocol, pop_err, username, response, host_name, &pop3.client_addr);
				break;
			}
		} else if(stricmp(password,user.pass)) {
			if(scfg.sys_misc&SM_ECHO_PW)
				lprintf(LOG_NOTICE,"%04d !POP3 [%s] FAILED Password attempt for user %s: '%s' expected '%s'"
					,socket, host_ip, username, password, user.pass);
			else
				lprintf(LOG_NOTICE,"%04d !POP3 [%s] FAILED Password attempt for user %s"
					,socket, host_ip, username);
			badlogin(socket, session, client.protocol, pop_err, username, password, host_name, &pop3.client_addr);
			break;
		}

		if(user.pass[0])
			loginSuccess(startup->login_attempt_list, &pop3.client_addr);

		putuserrec(&scfg,user.number,U_COMP,LEN_COMP,host_name);
		putuserrec(&scfg,user.number,U_IPADDR,LEN_IPADDR,host_ip);

		/* Update client display */
		client.user=user.alias;
		client_on(socket,&client,TRUE /* update */);
		activity=FALSE;

		if(startup->options&MAIL_OPT_DEBUG_POP3)		
			lprintf(LOG_INFO,"%04d POP3 [%s] %s logged in %s", socket, host_ip, user.alias, apop ? "via APOP":"");
		SAFEPRINTF(str,"POP3: %s",user.alias);
		status(str);

		SAFEPRINTF(smb.file,"%smail",scfg.data_dir);
		if(smb_islocked(&smb)) {
			lprintf(LOG_WARNING,"%04d !POP3 <%s> MAIL BASE LOCKED: %s",socket, user.alias, smb.last_error);
			sockprintf(socket,session,"-ERR database locked, try again later");
			break;
		}
		smb.retry_time=scfg.smb_retry_time;
		smb.subnum=INVALID_SUB;
		if((i=smb_open(&smb))!=SMB_SUCCESS) {
			lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) opening %s", socket, user.alias, i, smb.last_error,smb.file);
			sockprintf(socket,session,"-ERR %d opening %s",i,smb.file);
			break;
		}

		mail=loadmail(&smb,&msgs,user.number,MAIL_YOUR,lm_mode);

		for(l=bytes=0;l<msgs;l++) {
			msg.hdr.number=mail[l].number;
			if((i=smb_getmsgidx(&smb,&msg))!=SMB_SUCCESS) {
				lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) getting message index"
					,socket, user.alias, i, smb.last_error);
				break;
			}
			if((i=smb_lockmsghdr(&smb,&msg))!=SMB_SUCCESS) {
				lprintf(LOG_WARNING,"%04d !POP3 <%s> ERROR %d (%s) locking message header #%lu"
					,socket, user.alias, i, smb.last_error, msg.hdr.number);
				break; 
			}
			i=smb_getmsghdr(&smb,&msg);
			smb_unlockmsghdr(&smb,&msg);
			if(i!=0) {
				lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) line %u, msg #%lu"
					,socket, user.alias, i, smb.last_error, __LINE__, msg.hdr.number);
				break;
			}
			bytes+=smb_getmsgtxtlen(&msg);
			smb_freemsgmem(&msg);
		}

		if(l<msgs) {
			sockprintf(socket,session,"-ERR message #%d: %d (%s)"
				,mail[l].number,i,smb.last_error);
			break;
		}

		sockprintf(socket,session,"+OK %lu messages (%lu bytes)",msgs,bytes);

		while(1) {	/* TRANSACTION STATE */
			rd = sockreadline(socket, session, buf, sizeof(buf));
			if(rd<0) 
				break;
			truncsp(buf);
			if(startup->options&MAIL_OPT_DEBUG_POP3)
				lprintf(LOG_DEBUG,"%04d POP3 RX%s: %s", socket,session != -1 ? "S" : "", buf);
			if(!stricmp(buf, "NOOP")) {
				sockprintf(socket,session,"+OK");
				continue;
			}
			if(!stricmp(buf, "QUIT")) {
				sockprintf(socket,session,"+OK");
				break;
			}
			if(!stricmp(buf, "STAT")) {
				sockprintf(socket,session,"+OK %lu %lu",msgs,bytes);
				continue;
			}
			if(!stricmp(buf, "RSET")) {
				if((i=smb_locksmbhdr(&smb))!=SMB_SUCCESS) {
					lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) locking message base"
						,socket, user.alias, i, smb.last_error);
					sockprintf(socket,session,"-ERR %d locking message base",i);
					continue;
				}
				for(l=0;l<msgs;l++) {
					msg.hdr.number=mail[l].number;
					if((i=smb_getmsgidx(&smb,&msg))!=SMB_SUCCESS) {
						lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) getting message index"
							,socket, user.alias, i, smb.last_error);
						break;
					}
					if((i=smb_lockmsghdr(&smb,&msg))!=SMB_SUCCESS) {
						lprintf(LOG_WARNING,"%04d !POP3 <%s> ERROR %d (%s) locking message header #%lu"
							,socket, user.alias, i, smb.last_error, msg.hdr.number);
						break; 
					}
					if((i=smb_getmsghdr(&smb,&msg))!=SMB_SUCCESS) {
						smb_unlockmsghdr(&smb,&msg);
						lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) line %u, msg #%lu"
							,socket, user.alias, i, smb.last_error, __LINE__, msg.hdr.number);
						break;
					}
					msg.hdr.attr=mail[l].attr;
					if((i=smb_putmsg(&smb,&msg))!=SMB_SUCCESS)
						lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) updating message index"
							,socket, user.alias, i, smb.last_error);
					smb_unlockmsghdr(&smb,&msg);
					smb_freemsgmem(&msg);
				}
				smb_unlocksmbhdr(&smb);

				if(l<msgs)
					sockprintf(socket,session,"-ERR %d messages reset (ERROR: %d)",l,i);
				else
					sockprintf(socket,session,"+OK %lu messages (%lu bytes)",msgs,bytes);
				continue;
			}
			if(!strnicmp(buf, "LIST",4) || !strnicmp(buf,"UIDL",4)) {
				p=buf+4;
				SKIP_WHITESPACE(p);
				if(isdigit((uchar)*p)) {
					msgnum=strtoul(p, NULL, 10);
					if(msgnum<1 || msgnum>msgs) {
						lprintf(LOG_NOTICE,"%04d !POP3 <%s> INVALID message #%" PRIu32
							,socket, user.alias, msgnum);
						sockprintf(socket,session,"-ERR no such message");
						continue;
					}
					msg.hdr.number=mail[msgnum-1].number;
					if((i=smb_getmsgidx(&smb,&msg))!=SMB_SUCCESS) {
						lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) getting message index"
							,socket, user.alias, i, smb.last_error);
						sockprintf(socket,session,"-ERR %d getting message index",i);
						break;
					}
					if(msg.idx.attr&MSG_DELETE) {
						lprintf(LOG_NOTICE,"%04d !POP3 <%s> ATTEMPT to list DELETED message"
							,socket, user.alias);
						sockprintf(socket,session,"-ERR message deleted");
						continue;
					}
					if((i=smb_lockmsghdr(&smb,&msg))!=SMB_SUCCESS) {
						lprintf(LOG_WARNING,"%04d !POP3 <%s> ERROR %d (%s) locking message header #%lu"
							,socket, user.alias, i, smb.last_error, msg.hdr.number);
						sockprintf(socket,session,"-ERR %d locking message header",i);
						continue; 
					}
					i=smb_getmsghdr(&smb,&msg);
					smb_unlockmsghdr(&smb,&msg);
					if(i!=0) {
						smb_freemsgmem(&msg);
						lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) line %u, msg #%lu"
							,socket, user.alias, i, smb.last_error, __LINE__, msg.hdr.number);
						sockprintf(socket,session,"-ERR %d getting message header",i);
						continue;
					}
					if(!strnicmp(buf, "LIST",4)) {
						sockprintf(socket,session,"+OK %" PRIu32 " %lu",msgnum,smb_getmsgtxtlen(&msg));
					} else /* UIDL */
						sockprintf(socket,session,"+OK %" PRIu32 " %lu",msgnum,msg.hdr.number);

					smb_freemsgmem(&msg);
					continue;
				}
				/* List ALL messages */
				sockprintf(socket,session,"+OK %lu messages (%lu bytes)",msgs,bytes);
				for(l=0;l<msgs;l++) {
					msg.hdr.number=mail[l].number;
					if((i=smb_getmsgidx(&smb,&msg))!=SMB_SUCCESS) {
						lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) getting message index"
							,socket, user.alias, i, smb.last_error);
						break;
					}
					if(msg.idx.attr&MSG_DELETE) 
						continue;
					if((i=smb_lockmsghdr(&smb,&msg))!=SMB_SUCCESS) {
						lprintf(LOG_WARNING,"%04d !POP3 <%s> ERROR %d (%s) locking message header #%lu"
							,socket, user.alias, i, smb.last_error, msg.hdr.number);
						break; 
					}
					i=smb_getmsghdr(&smb,&msg);
					smb_unlockmsghdr(&smb,&msg);
					if(i!=0) {
						smb_freemsgmem(&msg);
						lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) line %u, msg #%lu"
							,socket, user.alias, i, smb.last_error, __LINE__, msg.hdr.number);
						break;
					}
					if(!strnicmp(buf, "LIST",4)) {
						sockprintf(socket,session,"%lu %lu",l+1,smb_getmsgtxtlen(&msg));
					} else /* UIDL */
						sockprintf(socket,session,"%lu %lu",l+1,msg.hdr.number);

					smb_freemsgmem(&msg);
				}			
				sockprintf(socket,session,".");
				continue;
			}
			activity=TRUE;
			if(!strnicmp(buf, "RETR ",5) || !strnicmp(buf,"TOP ",4)) {
				SAFEPRINTF(str,"POP3: %s", user.alias);
				status(str);

				lines=-1;
				p=buf+4;
				SKIP_WHITESPACE(p);
				msgnum=strtoul(p, NULL, 10);

				if(!strnicmp(buf,"TOP ",4)) {
					SKIP_DIGIT(p);
					SKIP_WHITESPACE(p);
					lines=atol(p);
				}
				if(msgnum<1 || msgnum>msgs) {
					lprintf(LOG_NOTICE,"%04d !POP3 <%s> ATTEMPTED to retrieve an INVALID message #%" PRIu32
						,socket, user.alias, msgnum);
					sockprintf(socket,session,"-ERR no such message");
					continue;
				}
				msg.hdr.number=mail[msgnum-1].number;

				lprintf(LOG_INFO,"%04d POP3 <%s> retrieving message #%ld with command: %s"
					,socket, user.alias, msg.hdr.number, buf);

				if((i=smb_getmsgidx(&smb,&msg))!=SMB_SUCCESS) {
					lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) getting message index"
						,socket, user.alias, i, smb.last_error);
					sockprintf(socket,session,"-ERR %d getting message index",i);
					continue;
				}
				if(msg.idx.attr&MSG_DELETE) {
					lprintf(LOG_NOTICE,"%04d !POP3 <%s> ATTEMPT to retrieve DELETED message"
						,socket, user.alias);
					sockprintf(socket,session,"-ERR message deleted");
					continue;
				}
				if((i=smb_lockmsghdr(&smb,&msg))!=SMB_SUCCESS) {
					lprintf(LOG_WARNING,"%04d !POP3 <%s> ERROR %d (%s) locking message header #%lu"
						,socket, user.alias, i, smb.last_error, msg.hdr.number);
					sockprintf(socket,session,"-ERR %d locking message header",i);
					continue; 
				}
				i=smb_getmsghdr(&smb,&msg);
				smb_unlockmsghdr(&smb,&msg);
				if(i!=0) {
					lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) line %u, msg #%lu"
						,socket, user.alias, i, smb.last_error, __LINE__, msg.hdr.number);
					sockprintf(socket,session,"-ERR %d getting message header",i);
					continue;
				}

				if((msgtxt=smb_getmsgtxt(&smb,&msg,GETMSGTXT_ALL))==NULL) {
					smb_freemsgmem(&msg);
					lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR (%s) retrieving message %lu text"
						,socket, user.alias, smb.last_error, msg.hdr.number);
					sockprintf(socket,session,"-ERR retrieving message text");
					continue;
				}

				remove_ctrl_a(msgtxt, msgtxt);

				if(lines > 0					/* Works around BlackBerry mail server */
					&& lines >= strlen(msgtxt))	/* which requests the number of bytes (instead of lines) using TOP */
					lines=-1;					

				sockprintf(socket,session,"+OK message follows");
				lprintf(LOG_DEBUG,"%04d POP3 <%s> sending message text (%u bytes)"
					,socket, user.alias, strlen(msgtxt));
				lines_sent=sockmsgtxt(socket,session,&msg,msgtxt,lines);
				/* if(startup->options&MAIL_OPT_DEBUG_POP3) */
				if(lines!=-1 && lines_sent<lines)	/* could send *more* lines */
					lprintf(LOG_WARNING,"%04d !POP3 <%s> ERROR sending message text (sent %ld of %ld lines)"
						,socket, user.alias, lines_sent, lines);
				else {
					lprintf(LOG_DEBUG,"%04d POP3 <%s> message transfer complete (%lu lines)"
						,socket, user.alias, lines_sent);

					if((i=smb_locksmbhdr(&smb))!=SMB_SUCCESS) {
						lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) locking message base"
							,socket, user.alias, i, smb.last_error);
					} else {
						if((i=smb_getmsgidx(&smb,&msg))!=SMB_SUCCESS) {
							lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) getting message index"
								,socket, user.alias, i, smb.last_error);
						} else {
							msg.hdr.attr|=MSG_READ;
							msg.hdr.netattr|=MSG_SENT;

							if((i=smb_lockmsghdr(&smb,&msg))!=SMB_SUCCESS) 
								lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) locking message header #%lu"
									,socket, user.alias, i, smb.last_error, msg.hdr.number);
							if((i=smb_putmsg(&smb,&msg))!=SMB_SUCCESS)
								lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) marking message #%lu as read"
									,socket, user.alias, i, smb.last_error, msg.hdr.number);
							smb_unlockmsghdr(&smb,&msg);
						}
						smb_unlocksmbhdr(&smb);
					}
				}
				smb_freemsgmem(&msg);
				smb_freemsgtxt(msgtxt);
				continue;
			}
			if(!strnicmp(buf, "DELE ",5)) {
				p=buf+5;
				SKIP_WHITESPACE(p);
				msgnum=strtoul(p, NULL, 10);

				if(msgnum<1 || msgnum>msgs) {
					lprintf(LOG_NOTICE,"%04d !POP3 <%s> ATTEMPTED to delete an INVALID message #%" PRIu32
						,socket, user.alias, msgnum);
					sockprintf(socket,session,"-ERR no such message");
					continue;
				}
				msg.hdr.number=mail[msgnum-1].number;

				lprintf(LOG_INFO,"%04d POP3 <%s> deleting message #%ld"
					,socket, user.alias, msg.hdr.number);

				if((i=smb_locksmbhdr(&smb))!=SMB_SUCCESS) {
					lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) locking message base"
						,socket, user.alias, i, smb.last_error);
					sockprintf(socket,session,"-ERR %d locking message base",i);
					continue;
				}
				if((i=smb_getmsgidx(&smb,&msg))!=SMB_SUCCESS) {
					smb_unlocksmbhdr(&smb);
					lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) getting message index"
						,socket, user.alias, i, smb.last_error);
					sockprintf(socket,session,"-ERR %d getting message index",i);
					continue;
				}
				if((i=smb_lockmsghdr(&smb,&msg))!=SMB_SUCCESS) {
					smb_unlocksmbhdr(&smb);
					lprintf(LOG_WARNING,"%04d !POP3 <%s> ERROR %d (%s) locking message header #%lu"
						,socket, user.alias, i, smb.last_error, msg.hdr.number);
					sockprintf(socket,session,"-ERR %d locking message header",i);
					continue; 
				}
				if((i=smb_getmsghdr(&smb,&msg))!=SMB_SUCCESS) {
					smb_unlockmsghdr(&smb,&msg);
					smb_unlocksmbhdr(&smb);
					lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) line %u, msg #%lu"
						,socket, user.alias, i, smb.last_error, __LINE__, msg.hdr.number);
					sockprintf(socket,session,"-ERR %d getting message header",i);
					continue;
				}
				msg.hdr.attr|=MSG_DELETE;

				if((i=smb_putmsg(&smb,&msg))==SMB_SUCCESS && msg.hdr.auxattr&MSG_FILEATTACH)
					delfattach(&scfg,&msg);
				smb_unlockmsghdr(&smb,&msg);
				smb_unlocksmbhdr(&smb);
				smb_freemsgmem(&msg);
				if(i!=SMB_SUCCESS) {
					lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR %d (%s) marking message as read"
						, socket, user.alias, i, smb.last_error);
					sockprintf(socket,session,"-ERR %d marking message for deletion",i);
					continue;
				}
				sockprintf(socket,session,"+OK");
				if(startup->options&MAIL_OPT_DEBUG_POP3)
					lprintf(LOG_INFO,"%04d POP3 <%s> message deleted", socket, user.alias);
				continue;
			}
			lprintf(LOG_NOTICE,"%04d !POP3 <%s> UNSUPPORTED COMMAND: '%s'"
				,socket, user.alias, buf);
			sockprintf(socket,session,"-ERR UNSUPPORTED COMMAND: %s",buf);
		}
		if(user.number) {
			if(!logoutuserdat(&scfg,&user,time(NULL),client.time))
				lprintf(LOG_ERR,"%04d !POP3 <%s> ERROR in logoutuserdat", socket, user.alias);
		}

	} while(0);

	if(activity) {
		if(user.number)
			lprintf(LOG_INFO,"%04d POP3 <%s> logged out from port %u on %s [%s]"
				,socket, user.alias, inet_addrport(&pop3.client_addr), host_name, host_ip);
		else
			lprintf(LOG_INFO,"%04d POP3 client disconnected from port %u on %s [%s]"
				,socket, inet_addrport(&pop3.client_addr), host_name, host_ip);
	}

	status(STATUS_WFC);

	/* Free up resources here */
	if(mail!=NULL)
		freemail(mail);

	smb_freemsgmem(&msg);
	smb_close(&smb);

	protected_uint32_adjust(&active_clients, -1);
	update_clients();
	client_off(socket);

	{ 
		int32_t remain = thread_down();
		if(startup->options&MAIL_OPT_DEBUG_POP3)
			lprintf(LOG_DEBUG,"%04d POP3 session thread terminated (%u threads remain, %lu clients served)"
				,socket, remain, ++stats.pop3_served);
	}

	if (session != -1)
		cryptDestroySession(session);
	/* Must be last */
	mail_close_socket(socket);
}

static ulong rblchk(SOCKET sock, union xp_sockaddr *addr, const char* rbl_addr)
{
	char		name[256];
	DWORD		mail_addr;
	HOSTENT*	host;
	struct in_addr dnsbl_result;
	unsigned char	*addr6;

	switch(addr->addr.sa_family) {
		case AF_INET:
			mail_addr=ntohl(addr->in.sin_addr.s_addr);
			safe_snprintf(name,sizeof(name),"%ld.%ld.%ld.%ld.%.128s"
				,mail_addr&0xff
				,(mail_addr>>8)&0xff
				,(mail_addr>>16)&0xff
				,(mail_addr>>24)&0xff
				,rbl_addr
				);
			break;
		case AF_INET6:
			addr6 = (unsigned char *)&addr->in6.sin6_addr;
			safe_snprintf(name,sizeof(name),"%1x.%1x.%1x.%1x.%1x.%1x.%1x.%1x."
											"%1x.%1x.%1x.%1x.%1x.%1x.%1x.%1x."
											"%1x.%1x.%1x.%1x.%1x.%1x.%1x.%1x."
											"%1x.%1x.%1x.%1x.%1x.%1x.%1x.%1x.%.128s"
				,addr6[15]&0x0f
				,addr6[15]>>4
				,addr6[14]&0x0f
				,addr6[14]>>4
				,addr6[13]&0x0f
				,addr6[13]>>4
				,addr6[12]&0x0f
				,addr6[12]>>4
				,addr6[11]&0x0f
				,addr6[11]>>4
				,addr6[10]&0x0f
				,addr6[10]>>4
				,addr6[9]&0x0f
				,addr6[9]>>4
				,addr6[8]&0x0f
				,addr6[8]>>4
				,addr6[7]&0x0f
				,addr6[7]>>4
				,addr6[6]&0x0f
				,addr6[6]>>4
				,addr6[5]&0x0f
				,addr6[5]>>4
				,addr6[4]&0x0f
				,addr6[4]>>4
				,addr6[3]&0x0f
				,addr6[3]>>4
				,addr6[2]&0x0f
				,addr6[2]>>4
				,addr6[1]&0x0f
				,addr6[1]>>4
				,addr6[0]&0x0f
				,addr6[0]>>4
				,rbl_addr
				);
			break;
		default:
			return 0;
	}
	lprintf(LOG_DEBUG,"%04d SMTP DNSBL Query: %s",sock,name);

	if((host=gethostbyname(name))==NULL)
		return(0);

	dnsbl_result.s_addr = *((ulong*)host->h_addr_list[0]);
	lprintf(LOG_INFO,"%04d SMTP DNSBL Query: %s resolved to: %s"
		,sock,name,inet_ntoa(dnsbl_result));

	return(dnsbl_result.s_addr);
}

static ulong dns_blacklisted(SOCKET sock, union xp_sockaddr *addr, char* host_name, char* list, char* dnsbl_ip)
{
	char	fname[MAX_PATH+1];
	char	str[256];
	char*	p;
	char*	tp;
	FILE*	fp;
	ulong	found=0;
	char	ip[INET6_ADDRSTRLEN];

	SAFEPRINTF(fname,"%sdnsbl_exempt.cfg",scfg.ctrl_dir);
	inet_addrtop(addr, ip, sizeof(ip));
	if(findstr(ip,fname))
		return(FALSE);
	if(findstr(host_name,fname))
		return(FALSE);

	SAFEPRINTF(fname,"%sdns_blacklist.cfg", scfg.ctrl_dir);
	if((fp=fopen(fname,"r"))==NULL)
		return(FALSE);

	while(!feof(fp) && !found) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		truncsp(str);

		p=str;
		SKIP_WHITESPACE(p);
		if(*p==';' || *p==0) /* comment or blank line */
			continue;

		sprintf(list,"%.100s",p);

		/* terminate */
		tp = p;
		FIND_WHITESPACE(tp);
		*tp=0;	

		found = rblchk(sock, addr, p);
	}
	fclose(fp);
	if(found)
		strcpy(dnsbl_ip, ip);

	return(found);
}


static BOOL chk_email_addr(SOCKET socket, char* p, char* host_name, char* host_ip
						   ,char* to, char* from, char* source)
{
	char	addr[64];
	char	tmp[128];

	SKIP_WHITESPACE(p);
	if(*p=='<') p++;		/* Skip '<' */
	SAFECOPY(addr,p);
	truncstr(addr,">( ");

	if(!trashcan(&scfg,addr,"email"))
		return(TRUE);

	lprintf(LOG_NOTICE,"%04d !SMTP BLOCKED %s e-mail address: %s"
		,socket, source, addr);
	SAFEPRINTF2(tmp,"Blocked %s e-mail address: %s", source, addr);
	spamlog(&scfg, "SMTP", "REFUSED", tmp, host_name, host_ip, to, from);

	return(FALSE);
}

static BOOL email_addr_is_exempt(const char* addr)
{
	char fname[MAX_PATH+1];
	char netmail[128];
	char* p;

	if(*addr==0 || strcmp(addr,"<>")==0)
		return FALSE;
	SAFEPRINTF(fname,"%sdnsbl_exempt.cfg",scfg.ctrl_dir);
	if(findstr((char*)addr,fname))
		return TRUE;
	SAFECOPY(netmail, addr);
	if(*(p=netmail)=='<')
		p++;
	truncstr(p,">");
	return userdatdupe(&scfg, 0, U_NETMAIL, LEN_NETMAIL, p, /* del */FALSE, /* next */FALSE, NULL, NULL);
}

static void exempt_email_addr(const char* comment
							  ,const char* fromname, const char* fromext, const char* fromaddr
							  ,const char* toaddr)
{
	char	fname[MAX_PATH+1];
	char	to[128];
	char	tmp[128];
	FILE*	fp;

	SAFEPRINTF(to,"<%s>",toaddr);
	if(!email_addr_is_exempt(to)) {
		SAFEPRINTF(fname,"%sdnsbl_exempt.cfg",scfg.ctrl_dir);
		if((fp=fopen(fname,"a"))==NULL)
			lprintf(LOG_ERR,"0000 !Error opening file: %s", fname);
		else {
			lprintf(LOG_INFO,"0000 %s: %s", comment, to);
			fprintf(fp,"\n;%s from \"%s\""
				,comment, fromname);
			if(fromext!=NULL)
				fprintf(fp,"%s",fromext);
			fprintf(fp," %s on %s\n%s\n"
				,fromaddr, timestr(&scfg,time32(NULL),tmp), to);
			fclose(fp);
		}
	}
}

static void signal_smtp_sem(void)
{
	int file;

	if(scfg.smtpmail_sem[0]==0) 
		return; /* do nothing */

	if((file=open(scfg.smtpmail_sem,O_WRONLY|O_CREAT|O_TRUNC,DEFFILEMODE))!=-1)
		close(file);
}

/*****************************************************************************/
/* Returns command line generated from instr with %c replacements            */
/*****************************************************************************/
static char* mailcmdstr(char* instr, char* msgpath, char* newpath, char* logpath
						,char* lstpath, char* errpath
						,char* host, char* ip, uint usernum
						,char* rcpt_addr
						,char* sender, char* sender_addr, char* reverse_path, char* cmd)
{
	char	str[1024];
    int		i,j,len;

    len=strlen(instr);
    for(i=j=0;i<len;i++) {
        if(instr[i]=='%') {
            i++;
            cmd[j]=0;
            switch(toupper(instr[i])) {
				case 'D':
					strcat(cmd,logpath);
					break;
				case 'E':
					strcat(cmd,errpath);
					break;
				case 'H':
					strcat(cmd,host);
					break;
				case 'I':
					strcat(cmd,ip);
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
				case 'N':
					strcat(cmd,newpath);
					break;
                case 'O':   /* SysOp */
                    strcat(cmd,scfg.sys_op);
                    break;
                case 'Q':   /* QWK ID */
                    strcat(cmd,scfg.sys_id);
                    break;
				case 'R':	/* reverse path */
					strcat(cmd,reverse_path);
					break;
				case 'S':	/* sender name */
					strcat(cmd,sender);
					break;
				case 'T':	/* recipient */
					strcat(cmd,rcpt_addr);
					break;
				case 'A':	/* sender address */
					strcat(cmd,sender_addr);
					break;
                case 'V':   /* Synchronet Version */
                    SAFEPRINTF2(str,"%s%c",VERSION,REVISION);
					strcat(cmd,str);
                    break;
                case 'Z':
                    strcat(cmd,scfg.text_dir);
                    break;
                case '!':   /* EXEC Directory */
                    strcat(cmd,scfg.exec_dir);
                    break;
                case '@':   /* EXEC Directory for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
                    strcat(cmd,scfg.exec_dir);
#endif
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
				case 'U':	/* User number */
					SAFEPRINTF(str,"%u",usernum);
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
#ifdef JAVASCRIPT

typedef struct {
	SOCKET		sock;
	const char*	log_prefix;
	const char*	proc_name;
} private_t;

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char	line[64];
	char	file[MAX_PATH+1];
	char*	warning;
	private_t*	p;
	jsrefcount	rc;
	int		log_level;

	if((p=(private_t*)JS_GetContextPrivate(cx))==NULL)
		return;

	if(report==NULL) {
		lprintf(LOG_ERR,"%04d %s %s !JavaScript: %s"
			, p->sock, p->log_prefix, p->proc_name, message);
		return;
    }

	if(report->filename)
		SAFEPRINTF(file," %s",report->filename);
	else
		file[0]=0;

	if(report->lineno)
		SAFEPRINTF(line," line %u",report->lineno);
	else
		line[0]=0;

	if(JSREPORT_IS_WARNING(report->flags)) {
		if(JSREPORT_IS_STRICT(report->flags))
			warning="strict warning";
		else
			warning="warning";
		log_level=LOG_WARNING;
	} else {
		log_level=LOG_ERR;
		warning="";
	}

	rc=JS_SUSPENDREQUEST(cx);
	lprintf(log_level,"%04d %s %s !JavaScript %s%s%s: %s"
		,p->sock, p->log_prefix, p->proc_name
		,warning ,file, line, message);
	JS_RESUMEREQUEST(cx, rc);
}

static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    uintN		i=0;
	int32		level=LOG_INFO;
	private_t*	p;
	jsrefcount	rc;
	char		*lstr=NULL;
	size_t		lstr_sz=0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(JSVAL_IS_NUMBER(argv[i])) {
		if(!JS_ValueToInt32(cx,argv[i++],&level))
			return JS_FALSE;
	}

	for(; i<argc; i++) {
		JSVALUE_TO_RASTRING(cx, argv[i], lstr, &lstr_sz, NULL);
		HANDLE_PENDING(cx, lstr);
		if(lstr==NULL)
			return(JS_TRUE);
		rc=JS_SUSPENDREQUEST(cx);
		lprintf(level,"%04d %s %s %s"
			,p->sock,p->log_prefix,p->proc_name,lstr);
		JS_SET_RVAL(cx, arglist, argv[i]);
		JS_RESUMEREQUEST(cx, rc);
	}

	if(lstr)
		free(lstr);

    return(JS_TRUE);
}

static JSBool
js_alert(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	jsrefcount	rc;
	char		*line;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], line, NULL);
	if(line==NULL)
	    return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	lprintf(LOG_ERR,"%04d %s %s %s"
		,p->sock, p->log_prefix, p->proc_name, line);
	free(line);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, argv[0]);

    return(JS_TRUE);
}


static JSFunctionSpec js_global_functions[] = {
	{"write",			js_log,				0},
	{"writeln",			js_log,				0},
	{"print",			js_log,				0},
	{"log",				js_log,				0},
	{"alert",			js_alert,			1},
    {0}
};

static BOOL
js_mailproc(SOCKET sock, client_t* client, user_t* user, struct mailproc* mailproc
			,char* cmdline
			,char* msgtxt_fname, char* newtxt_fname, char* logtxt_fname
			,char* rcpt_addr
			,char* rcptlst_fname, char* proc_err_fname
			,char* sender, char* sender_addr, char* reverse_path, char* hello_name
			,int32* result
			,JSRuntime**	js_runtime
			,JSContext**	js_cx
			,JSObject**		js_glob
			,const char*	log_prefix
)
{
	char*		p;
	char		fname[MAX_PATH+1];
	char		path[MAX_PATH+1];
	char		arg[MAX_PATH+1];
	BOOL		success=FALSE;
	JSObject*	js_scope=NULL;
	JSObject*	argv;
	jsuint		argc;
	JSObject*	js_script;
	js_callback_t	js_callback;
	jsval		val;
	jsval		rval=JSVAL_VOID;
	private_t	priv;

	ZERO_VAR(js_callback);

	SAFECOPY(fname,cmdline);
	truncstr(fname," \t");
	if(getfext(fname)==NULL) /* No extension specified, assume '.js' */
		strcat(fname,".js");

	SAFECOPY(path,fname);
	if(getfname(path)==path) { /* No path specified, assume mods or exec dir */
		SAFEPRINTF2(path,"%s%s",scfg.mods_dir,fname);
		if(scfg.mods_dir[0]==0 || !fexist(path))
			SAFEPRINTF2(path,"%s%s",scfg.exec_dir,fname);
	}

	*result = 0;
	do {
		if(*js_runtime==NULL) {
			lprintf(LOG_DEBUG,"%04d %s JavaScript: Creating runtime: %lu bytes\n"
				,sock, log_prefix, startup->js.max_bytes);

			if((*js_runtime = jsrt_GetNew(startup->js.max_bytes, 1000, __FILE__, __LINE__))==NULL)
				return FALSE;
		}

		if(*js_cx==NULL) {
			lprintf(LOG_DEBUG,"%04d %s JavaScript: Initializing context (stack: %lu bytes)\n"
				,sock, log_prefix, startup->js.cx_stack);

			if((*js_cx = JS_NewContext(*js_runtime, startup->js.cx_stack))==NULL)
				return FALSE;
		}
		JS_BEGINREQUEST(*js_cx);

		JS_SetErrorReporter(*js_cx, js_ErrorReporter);

		priv.sock=sock;
		priv.log_prefix=log_prefix;
		priv.proc_name=mailproc->name;
		JS_SetContextPrivate(*js_cx, &priv);

		if(*js_glob==NULL) {
			/* Global Objects (including system, js, client, Socket, MsgBase, File, User, etc. */
			if(!js_CreateCommonObjects(*js_cx, &scfg, &scfg, NULL
						,uptime, startup->host_name, SOCKLIB_DESC	/* system */
						,&js_callback								/* js */
						,&startup->js
						,client, sock, -1							/* client */
						,&js_server_props							/* server */
						,js_glob
				))
				break;

			if(!JS_DefineFunctions(*js_cx, *js_glob, js_global_functions))
				break;

			/* Area and "user" Objects */
			if(!js_CreateUserObjects(*js_cx, *js_glob, &scfg, user, client, NULL, NULL)) 
				break;

			/* Mailproc "API" filenames */
			JS_DefineProperty(*js_cx, *js_glob, "message_text_filename"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,msgtxt_fname))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "new_message_text_filename"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,newtxt_fname))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "log_text_filename"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,logtxt_fname))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "recipient_address"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,rcpt_addr))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "recipient_list_filename"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,rcptlst_fname))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "processing_error_filename"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,proc_err_fname))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "sender_name"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,sender))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "sender_address"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,sender_addr))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "reverse_path"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,reverse_path))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "hello_name"
				,STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,hello_name))
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

		}

		if((js_scope=JS_NewObject(*js_cx, NULL, NULL, *js_glob))==NULL)
			break;

		/* Convert command-line to argv/argc */
		argv=JS_NewArrayObject(*js_cx, 0, NULL);
		JS_DefineProperty(*js_cx, js_scope, "argv", OBJECT_TO_JSVAL(argv)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

		p=cmdline;
		FIND_WHITESPACE(p); 
		SKIP_WHITESPACE(p);
		for(argc=0;*p;argc++) {
			SAFECOPY(arg,p);
			truncstr(arg," \t");
			val=STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx,arg));
			if(!JS_SetElement(*js_cx, argv, argc, &val))
				break;
			FIND_WHITESPACE(p);
			SKIP_WHITESPACE(p);
		}
		JS_DefineProperty(*js_cx, js_scope, "argc", INT_TO_JSVAL(argc)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

		if(*mailproc->eval!=0) {
			lprintf(LOG_DEBUG,"%04d %s Evaluating: %s"
				,sock, log_prefix, mailproc->eval);
			js_script=JS_CompileScript(*js_cx, js_scope, mailproc->eval, strlen(mailproc->eval), NULL, 1);
		} else {
			lprintf(LOG_DEBUG,"%04d %s Executing: %s"
				,sock, log_prefix, cmdline);
			if((js_script=JS_CompileFile(*js_cx, js_scope, path)) != NULL)
				js_PrepareToExecute(*js_cx, js_scope, path, /* startup_dir: */NULL, js_scope);
		}
		if(js_script==NULL)
			break;

		/* ToDo: Set operational callback */
		success=JS_ExecuteScript(*js_cx, js_scope, js_script, &rval);

		JS_GetProperty(*js_cx, js_scope, "exit_code", &rval);

		if(rval!=JSVAL_VOID && JSVAL_IS_NUMBER(rval))
			JS_ValueToInt32(*js_cx,rval,result);

		js_EvalOnExit(*js_cx, js_scope, &js_callback);

		JS_ReportPendingException(*js_cx);

		JS_ClearScope(*js_cx, js_scope);

		JS_GC(*js_cx);

	} while(0);

	JS_ENDREQUEST(*js_cx);

	return(success);
}

void js_cleanup(JSRuntime* js_runtime, JSContext* js_cx, JSObject** js_glob)
{
	if(js_cx!=NULL) {
		JS_BEGINREQUEST(js_cx);
		JS_RemoveObjectRoot(js_cx, js_glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
	}
	if(js_runtime!=NULL)
		jsrt_Release(js_runtime);
}
#endif

static char* get_header_field(char* buf, char* name, size_t maxlen)
{
	char*	p;
	size_t	len;

	if(buf[0]<=' ')	/* folded header */
		return NULL;

	if((p=strchr(buf,':'))==NULL)
		return NULL;

	len = p-buf;
	if(len >= maxlen)
		len = maxlen-1;
	sprintf(name,"%.*s",(int)len,buf);
	truncsp(name);

	p++;	/* skip colon */
	SKIP_WHITESPACE(p);
	return p;
}

static int parse_header_field(char* buf, smbmsg_t* msg, ushort* type)
{
	char*	p;
	char*	tp;
	char	field[128];
	int		len;
	ushort	nettype;

	if(buf[0]<=' ' && *type!=UNKNOWN) {	/* folded header, append to previous */
		p=buf;
		truncsp(p);
		if(*type==RFC822HEADER || *type==SMTPRECEIVED)
			smb_hfield_append_str(msg,*type,"\r\n");
		else { /* Unfold other common header field types (e.g. Subject, From, To) */
			smb_hfield_append_str(msg,*type," ");
			SKIP_WHITESPACE(p);
		}
		return smb_hfield_append_str(msg, *type, p);
	}

	if((p=strchr(buf,':'))==NULL)
		return smb_hfield_str(msg, *type=RFC822HEADER, buf);

	len=(ulong)p-(ulong)buf;
	if(len>sizeof(field)-1)
		len=sizeof(field)-1;
	sprintf(field,"%.*s",len,buf);
	truncsp(field);

	p++;	/* skip colon */
	SKIP_WHITESPACE(p);
	truncsp(p);

	if(!stricmp(field, "TO"))
		return smb_hfield_str(msg, *type=RFC822TO, p);

	if(!stricmp(field, "REPLY-TO")) {
		smb_hfield_str(msg, *type=RFC822REPLYTO, p);
		if((tp=strrchr(p,'<'))!=NULL)  {
			tp++;
			truncstr(tp,">");
			p=tp;
		}
		nettype=NET_INTERNET;
		smb_hfield(msg, REPLYTONETTYPE, sizeof(nettype), &nettype);
		return smb_hfield_str(msg, *type=REPLYTONETADDR, p);
	}
	if(!stricmp(field, "FROM"))
		return smb_hfield_str(msg, *type=RFC822FROM, p);

	if(!stricmp(field, "ORGANIZATION"))
		return smb_hfield_str(msg, *type=SENDERORG, p);

	if(!stricmp(field, "DATE")) {
		msg->hdr.when_written=rfc822date(p);
		*type=UNKNOWN;
		return SMB_SUCCESS;
	}
	if(!stricmp(field, "MESSAGE-ID"))
		return smb_hfield_str(msg, *type=RFC822MSGID, p);

	if(!stricmp(field, "IN-REPLY-TO"))
		return smb_hfield_str(msg, *type=RFC822REPLYID, p);

	if(!stricmp(field, "CC"))
		return smb_hfield_str(msg, *type=SMB_CARBONCOPY, p);

	if(!stricmp(field, "RECEIVED"))
		return smb_hfield_str(msg, *type=SMTPRECEIVED, p);

	if(!stricmp(field, "RETURN-PATH")) {
		*type=UNKNOWN;
		return SMB_SUCCESS;	/* Ignore existing "Return-Path" header fields */
	}

	/* Fall-through */
	return smb_hfield_str(msg, *type=RFC822HEADER, buf);
}

static int chk_received_hdr(SOCKET socket,const char *buf,IN_ADDR *dnsbl_result, char *dnsbl, char *dnsbl_ip)
{
	char		host_name[128];
	char		*fromstr;
	char		ip[16];
	char		*p;
	char		*p2;
	char		*last;
	union xp_sockaddr addr;
	struct addrinfo ai,*res;

	fromstr=strdup(buf);
	if(fromstr==NULL)
		return(0);
	strlwr(fromstr);
	do {
		p=strstr(fromstr,"from ");
		if(p==NULL)
			break;
		p+=4;
		SKIP_WHITESPACE(p);
		if(*p==0)
			break;
		p2=host_name;
		for(;*p && !isspace((unsigned char)*p) && p2<host_name+126;p++)  {
			*p2++=*p;
		}
		*p2=0;
		p=strtok_r(fromstr,"[",&last);
		if(p==NULL)
			break;
		p=strtok_r(NULL,"]",&last);
		if(p==NULL)
			break;
		if(strnicmp("IPv6:", p, 5)) {
			p+=5;
			SKIP_WHITESPACE(p);
			memset(&ai, 0, sizeof(ai));
			ai.ai_flags = AI_NUMERICHOST|AI_NUMERICSERV|AI_PASSIVE;
			if(getaddrinfo(p, NULL, &ai, &res)!=0)
				break;
			if(res->ai_family == AF_INET6) {
				memcpy(&addr, res->ai_addr, res->ai_addrlen);
				freeaddrinfo(res);
			} else {
				freeaddrinfo(res);
				break;
			}
		}
		else {
			strncpy(ip,p,16);
			ip[15]=0;
			addr.in.sin_family=AF_INET;
			addr.in.sin_addr.s_addr=inet_addr(ip);
			lprintf(LOG_DEBUG,"%04d SMTP DNSBL checking received header address %s [%s]",socket,host_name,ip);
		}

		if((dnsbl_result->s_addr=dns_blacklisted(socket,&addr,host_name,dnsbl,dnsbl_ip))!=0)
				lprintf(LOG_NOTICE,"%04d SMTP BLACKLISTED SERVER on %s: %s [%s] = %s"
					,socket, dnsbl, host_name, ip, inet_ntoa(*dnsbl_result));
	} while(0);
	free(fromstr);
	return(dnsbl_result->s_addr);
}

static void strip_char(char* str, char ch)
{
	char* src;
	char* p;
	char* tmp = strdup(str);

	if(tmp == NULL)
		return;
	p=tmp;
	for(src = str; *src; src++) {
		if(*src != ch)
			*(p++) = *src;
	}
	*p=0;
	strcpy(str, tmp);
	free(tmp);
}

static void parse_mail_address(char* p
							   ,char* name, size_t name_len
							   ,char* addr, size_t addr_len)
{
	char*	tp;
	char	tmp[128];

	SKIP_WHITESPACE(p);

	/* Get the address */
	if((tp=strrchr(p,'<'))!=NULL)
		tp++;
	else
		tp=p;
	SKIP_WHITESPACE(tp);
	sprintf(addr,"%.*s",(int)addr_len,tp);
	truncstr(addr,">( ");

	SAFECOPY(tmp,p);
	p=tmp;
	/* Get the "name" (if possible) */
	if((tp=strchr(p,'"'))!=NULL) {	/* name in quotes? */
		p=tp+1;
		tp=strrchr(p,'"');
	} else if((tp=strchr(p,'('))!=NULL) {	/* name in parenthesis? */
		p=tp+1;
		tp=strrchr(p,')');
	} else if(*p=='<') {					/* address in brackets? */
		p++;
		tp=strrchr(p,'>');
	} else									/* name, then address in brackets */
		tp=strchr(p,'<');
	if(tp) *tp=0;
	sprintf(name,"%.*s",(int)name_len,p);
	truncsp(name);
	strip_char(name, '\\');
}

/* Decode quoted-printable content-transfer-encoded text */
/* Ignores (strips) unsupported ctrl chars and non-ASCII chars */
/* Does not enforce 76 char line length limit */
static char* qp_decode(char* buf)
{
	uchar*	p=(uchar*)buf;
	uchar*	dest=p;

	for(;;p++) {
		if(*p==0) {
			*dest++='\r';
			*dest++='\n';
			break;
		}
		if(*p==' ' || (*p>='!' && *p<='~' && *p!='=') || *p=='\t')
			*dest++=*p;
		else if(*p=='=') {
			p++;
			if(*p==0) 	/* soft link break */
				break;
			if(isxdigit(*p) && isxdigit(*(p+1))) {
				char hex[3];
				hex[0]=*p;
				hex[1]=*(p+1);
				hex[2]=0;
				/* ToDo: what about encoded NULs and the like? */
				*dest++=(uchar)strtoul(hex,NULL,16);
				p++;
			} else {	/* bad encoding */
				*dest++='=';
				*dest++=*p;
			}
		}
	}
	*dest=0;
	return buf;
}

static BOOL checktag(scfg_t *scfg, char *tag, uint usernum)
{
	char	fname[MAX_PATH+1];

	if(tag==NULL)
		return(FALSE);
	SAFEPRINTF2(fname,"%suser/%04d.smtpblock",scfg->data_dir,usernum);
	return(findstr(tag, fname));
}

static BOOL smtp_splittag(char *in, char **name, char **tag)
{
	char	*last;

	if(in==NULL)
		return(FALSE);

	*name=strtok_r(in, "#", &last);
	if(*name) {
		*tag=strtok_r(NULL, "", &last);
		return(TRUE);
	}
	return(FALSE);
}

static uint smtp_matchuser(scfg_t *scfg, char *str, BOOL aliases, BOOL datdupe)
{
	char	*user=strdup(str);
	char	*name;
	char	*tag=NULL;
	uint	usernum=0;

	if(!user)
		return(0);

	if(!smtp_splittag(user, &name, &tag))
		goto end;

	if(datdupe)
		usernum=userdatdupe(scfg, 0, U_NAME, LEN_NAME, name, /* del */FALSE, /* next */FALSE, NULL, NULL);
	else
		usernum=matchuser(scfg, name, aliases);

	if(!usernum)
		goto end;

	if(checktag(scfg, tag, usernum))
		usernum=UINT_MAX;

end:
	free(user);
	return(usernum);
}

#define WITH_ESMTP	(1<<0)
#define WITH_AUTH	(1<<1)
#define WITH_TLS	(1<<2)

char *with_clauses[] = {
	"SMTP",			// No WITH_*
	"ESMTP",		// WITH_ESMTP
	"SMTP",			// WITH_AUTH
	"ESMTPA",		// WITH_ESMTP | WITH_AUTH
	"SMTP",			// WITH_TLS
	"ESMTPS",		// WITH_ESMTP | WITH_TLS
	"SMTP",			// WITH_TLS | WITH_AUTH
	"ESMTPSA"		// WITH_TLS | WITH_AUTH | WITH_ESMTP
};

static void smtp_thread(void* arg)
{
	int			i,j;
	int			rd;
	char		str[512];
	char		tmp[128];
	char		path[MAX_PATH+1];
	char		value[INI_MAX_VALUE_LEN];
	str_list_t	sec_list;
	char*		section;
	char		buf[1024],*p,*tp,*cp;
	char		hdrfield[512];
	char		alias_buf[128];
	char		name_alias_buf[128];
	char		reverse_path[128];
	char		date[64];
	char		qwkid[32];
	char		rcpt_to[128];
	char		rcpt_name[128];
	char		rcpt_addr[128];
	char		sender[128];
	char		sender_ext[128];
	char		sender_addr[128];
	char		hello_name[128];
	char		user_name[128];
	char		user_pass[128];
	char		relay_list[MAX_PATH+1];
	char		domain_list[MAX_PATH+1];
	char		spam_bait[MAX_PATH+1];
	BOOL		spam_bait_result=FALSE;
	char		spam_block[MAX_PATH+1];
	char		spam_block_exemptions[MAX_PATH+1];
	BOOL		spam_block_exempt=FALSE;
	char		host_name[128];
	char		host_ip[INET6_ADDRSTRLEN];
	char		server_ip[INET6_ADDRSTRLEN];
	char		dnsbl[256];
	char		dnsbl_ip[INET6_ADDRSTRLEN];
	char*		telegram_buf;
	char*		msgbuf;
	char		challenge[256];
	char		response[128];
	char		secret[64];
	char		md5_data[384];
	uchar		digest[MD5_DIGEST_SIZE];
	char		dest_host[128];
	char*		errmsg;
	ushort		dest_port;
	socklen_t	addr_len;
	ushort		hfield_type;
	ushort		nettype;
	ushort		agent;
	uint		usernum;
	ulong		lines=0;
	ulong		hdr_lines=0;
	ulong		hdr_len=0;
	ulong		length;
	ulong		badcmds=0;
	ulong		login_attempts;
	ulong		waiting;
	BOOL		esmtp=FALSE;
	BOOL		telegram=FALSE;
	BOOL		forward=FALSE;
	BOOL		no_forward=FALSE;
	BOOL		auth_login;
	BOOL		routed=FALSE;
	BOOL		dnsbl_recvhdr;
	BOOL		msg_handled;
	uint		subnum=INVALID_SUB;
	FILE*		msgtxt=NULL;
	char		msgtxt_fname[MAX_PATH+1];
	char		newtxt_fname[MAX_PATH+1];
	char		logtxt_fname[MAX_PATH+1];
	FILE*		rcptlst;
	char		rcptlst_fname[MAX_PATH+1];
	ushort		rcpt_count=0;
	FILE*		proc_out;
	char		proc_err_fname[MAX_PATH+1];
	char		session_id[MAX_PATH+1];
	FILE*		spy=NULL;
	SOCKET		socket;
	int			smb_error;
	smb_t		smb;
	smb_t		spam;
	smbmsg_t	msg;
	smbmsg_t	newmsg;
	user_t		user;
	user_t		relay_user;
	node_t		node;
	client_t	client;
	smtp_t		smtp=*(smtp_t*)arg;
	union xp_sockaddr	server_addr;
	IN_ADDR		dnsbl_result;
	BOOL*		mailproc_to_match;
	int			mailproc_match;
	JSRuntime*	js_runtime=NULL;
	JSContext*	js_cx=NULL;
	JSObject*	js_glob=NULL;
	int32		js_result;
	struct mailproc*	mailproc;
	login_attempt_t attempted;
	int session = -1;
	BOOL nodelay=TRUE;
	ulong nb = 0;
	unsigned	with_val;

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

	enum {
			 ENCODING_NONE
			,ENCODING_BASE64
			,ENCODING_QUOTED_PRINTABLE
	} content_encoding = ENCODING_NONE;

	SetThreadName("sbbs/smtp");
	thread_up(TRUE /* setuid */);

	free(arg);

	socket=smtp.socket;

	lprintf(LOG_DEBUG,"%04d SMTP Session thread started", socket);

#ifdef _WIN32
	if(startup->inbound_sound[0] && !(startup->options&MAIL_OPT_MUTE)) 
		PlaySound(startup->inbound_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

	addr_len=sizeof(server_addr);

	if(smtp.tls_port) {
		if (get_ssl_cert(&scfg, NULL) == -1) {
			lprintf(LOG_ERR, "%04d !SMTP Unable to get certificate", socket);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_SSL_SERVER) != CRYPT_OK) {
			lprintf(LOG_ERR, "%04d !SMTP Unable to create TLS session", socket);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (cryptSetAttribute(session, CRYPT_SESSINFO_SSL_OPTIONS, CRYPT_SSLOPTION_DISABLE_CERTVERIFY) != CRYPT_OK) {
			lprintf(LOG_ERR, "%04d !SMTP Unable to disable certificate verification", socket);
			cryptDestroySession(session);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (cryptSetAttribute(session, CRYPT_SESSINFO_PRIVATEKEY, scfg.tls_certificate) != CRYPT_OK) {
			lprintf(LOG_ERR, "%04d !SMTP Unable to set private key", socket);
			cryptDestroySession(session);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		nodelay = TRUE;
		setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));
		nb=0;
		ioctlsocket(socket,FIONBIO,&nb);
		if ((rd = cryptSetAttribute(session, CRYPT_SESSINFO_NETWORKSOCKET, socket)) != CRYPT_OK) {
			lprintf(LOG_ERR, "%04d !SMTP Unable to set network socket", socket);
			cryptDestroySession(session);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (cryptSetAttribute(session, CRYPT_SESSINFO_ACTIVE, 1) != CRYPT_OK) {
			lprintf(LOG_ERR, "%04d !SMTP Unable to set session active", socket);
			cryptDestroySession(session);
			mail_close_socket(socket);
			thread_down();
			return;
		}
		if (startup->max_inactivity) {
			if (cryptSetAttribute(session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity) != CRYPT_OK) {
				lprintf(LOG_ERR, "%04d !SMTP Unable to set max inactivity", socket);
				cryptDestroySession(session);
				mail_close_socket(socket);
				thread_down();
				return;
			}
		}
	}

	if((i=getsockname(socket, &server_addr.addr, &addr_len))!=0) {
		lprintf(LOG_CRIT,"%04d !SMTP ERROR %d (%d) getting address/port"
			,socket, i, ERROR_VALUE);
		sockprintf(socket,session,sys_error);
		if (session != -1)
			cryptDestroySession(session);
		mail_close_socket(socket);
		thread_down();
		return;
	}

	if((mailproc_to_match=malloc(sizeof(BOOL)*mailproc_count))==NULL) {
		lprintf(LOG_CRIT,"%04d !SMTP ERROR allocating memory for mailproc_to_match", socket);
		sockprintf(socket,session,sys_error);
		if (session != -1)
			cryptDestroySession(session);
		mail_close_socket(socket);
		thread_down();
		return;
	} 
	memset(mailproc_to_match,FALSE,sizeof(BOOL)*mailproc_count);

	memset(&smb,0,sizeof(smb));
	memset(&msg,0,sizeof(msg));
	memset(&spam,0,sizeof(spam));
	memset(&user,0,sizeof(user));
	memset(&relay_user,0,sizeof(relay_user));

	inet_addrtop(&smtp.client_addr,host_ip,sizeof(host_ip));

	lprintf(LOG_INFO,"%04d SMTP Connection accepted on port %u from: %s port %u"
		,socket, inet_addrport(&server_addr), host_ip, inet_addrport(&smtp.client_addr));

	if(getnameinfo(&smtp.client_addr.addr, smtp.client_addr_len, host_name, sizeof(host_name), NULL, 0, (startup->options&MAIL_OPT_NO_HOST_LOOKUP)?NI_NUMERICHOST:0)!=0)
		SAFECOPY(host_name, "<no name>");

	if(!(startup->options&MAIL_OPT_NO_HOST_LOOKUP))
		lprintf(LOG_INFO,"%04d SMTP Hostname: %s", socket, host_name);

	protected_uint32_adjust(&active_clients, 1);
	update_clients();

	SAFECOPY(hello_name,host_name);

	SAFEPRINTF(spam_bait,"%sspambait.cfg",scfg.ctrl_dir);
	SAFEPRINTF(spam_block,"%sspamblock.cfg",scfg.ctrl_dir);
	SAFEPRINTF(spam_block_exemptions,"%sspamblock_exempt.cfg",scfg.ctrl_dir);

	inet_addrtop(&server_addr,server_ip,sizeof(server_ip));

	if(strcmp(server_ip, host_ip)==0) {
		/* local connection */
		dnsbl_result.s_addr=0;
	} else {
		ulong banned = loginBanned(&scfg, startup->login_attempt_list, socket, host_name, startup->login_attempt, &attempted);
		if(banned) {
			char ban_duration[128];
			lprintf(LOG_NOTICE, "%04d !TEMPORARY BAN of %s (%u login attempts, last: %s) - remaining: %s"
				,socket, host_ip, attempted.count-attempted.dupes, attempted.user, seconds_to_str(banned, ban_duration));
			if (session != -1)
				cryptDestroySession(session);
			mail_close_socket(socket);
			thread_down();
			protected_uint32_adjust(&active_clients, -1);
			update_clients();
			free(mailproc_to_match);
			return;
		}

		spam_block_exempt = findstr(host_ip,spam_block_exemptions) || findstr(host_name,spam_block_exemptions);
		if(trashcan(&scfg,host_ip,"ip") 
			|| (findstr(host_ip,spam_block) && !spam_block_exempt)) {
			lprintf(LOG_NOTICE,"%04d !SMTP CLIENT IP ADDRESS BLOCKED: %s (%u total)"
				,socket, host_ip, ++stats.sessions_refused);
			sockprintf(socket,session,"550 CLIENT IP ADDRESS BLOCKED: %s", host_ip);
			if (session != -1)
				cryptDestroySession(session);
			mail_close_socket(socket);
			thread_down();
			protected_uint32_adjust(&active_clients, -1);
			update_clients();
			free(mailproc_to_match);
			return;
		}

		if(trashcan(&scfg,host_name,"host") 
			|| (findstr(host_name,spam_block) && !spam_block_exempt)) {
			lprintf(LOG_NOTICE,"%04d !SMTP CLIENT HOSTNAME BLOCKED: %s (%u total)"
				,socket, host_name, ++stats.sessions_refused);
			sockprintf(socket,session,"550 CLIENT HOSTNAME BLOCKED: %s", host_name);
			if (session != -1)
				cryptDestroySession(session);
			mail_close_socket(socket);
			thread_down();
			protected_uint32_adjust(&active_clients, -1);
			update_clients();
			free(mailproc_to_match);
			return;
		}

		/*  SPAM Filters (mail-abuse.org) */
		dnsbl_result.s_addr = dns_blacklisted(socket,&smtp.client_addr,host_name,dnsbl,dnsbl_ip);
		if(dnsbl_result.s_addr) {
			lprintf(LOG_NOTICE,"%04d SMTP BLACKLISTED SERVER on %s: %s [%s] = %s"
				,socket, dnsbl, host_name, dnsbl_ip, inet_ntoa(dnsbl_result));
			if(startup->options&MAIL_OPT_DNSBL_REFUSE) {
				SAFEPRINTF2(str,"Listed on %s as %s", dnsbl, inet_ntoa(dnsbl_result));
				spamlog(&scfg, "SMTP", "SESSION REFUSED", str, host_name, dnsbl_ip, NULL, NULL);
				sockprintf(socket,session
					,"550 Mail from %s refused due to listing at %s"
					,dnsbl_ip, dnsbl);
				if (session != -1)
					cryptDestroySession(session);
				mail_close_socket(socket);
				lprintf(LOG_NOTICE,"%04d !SMTP REFUSED SESSION from blacklisted server (%u total)"
					,socket, ++stats.sessions_refused);
				thread_down();
				protected_uint32_adjust(&active_clients, -1);
				update_clients();
				free(mailproc_to_match);
				return;
			}
		}
	}

	SAFEPRINTF(smb.file,"%smail",scfg.data_dir);
	if(smb_islocked(&smb)) {
		lprintf(LOG_CRIT,"%04d !SMTP MAIL BASE LOCKED: %s"
			,socket, smb.last_error);
		sockprintf(socket,session,sys_unavail);
		if (session != -1)
			cryptDestroySession(session);
		mail_close_socket(socket);
		thread_down();
		protected_uint32_adjust(&active_clients, -1);
		update_clients();
		free(mailproc_to_match);
		return;
	}
	SAFEPRINTF(spam.file,"%sspam",scfg.data_dir);
	spam.retry_time=scfg.smb_retry_time;
	spam.subnum=INVALID_SUB;

	srand((unsigned int)(time(NULL) ^ (time_t)GetCurrentThreadId()));	/* seed random number generator */
	rand();	/* throw-away first result */
	SAFEPRINTF4(session_id,"%x%x%x%lx",getpid(),socket,rand(),clock());
	lprintf(LOG_DEBUG,"%04d SMTP Session ID=%s", socket, session_id);
	SAFEPRINTF2(msgtxt_fname,"%sSBBS_SMTP.%s.msg", scfg.temp_dir, session_id);
	SAFEPRINTF2(newtxt_fname,"%sSBBS_SMTP.%s.new", scfg.temp_dir, session_id);
	SAFEPRINTF2(logtxt_fname,"%sSBBS_SMTP.%s.log", scfg.temp_dir, session_id);
	SAFEPRINTF2(rcptlst_fname,"%sSBBS_SMTP.%s.lst", scfg.temp_dir, session_id);
	rcptlst=fopen(rcptlst_fname,"w+");
	if(rcptlst==NULL) {
		lprintf(LOG_CRIT,"%04d !SMTP ERROR %d creating recipient list: %s"
			,socket, errno, rcptlst_fname);
		sockprintf(socket,session,sys_error);
		if (session != -1)
			cryptDestroySession(session);
		mail_close_socket(socket);
		thread_down();
		protected_uint32_adjust(&active_clients, -1);
		update_clients();
		free(mailproc_to_match);
		return;
	}

	if(trashcan(&scfg,host_name,"smtpspy") 
		|| trashcan(&scfg,host_ip,"smtpspy")) {
		SAFEPRINTF(str,"%ssmtpspy.txt", scfg.logs_dir);
		spy=fopen(str,"a");
	}

	/* Initialize client display */
	client.size=sizeof(client);
	client.time=time32(NULL);
	SAFECOPY(client.addr,host_ip);
	SAFECOPY(client.host,host_name);
	client.port=inet_addrport(&smtp.client_addr);
	client.protocol="SMTP";
	client.user=STR_UNKNOWN_USER;
	client_on(socket,&client,FALSE /* update */);

	SAFEPRINTF(str,"SMTP: %s",host_ip);
	status(str);

	if(startup->login_attempt.throttle
		&& (login_attempts=loginAttempts(startup->login_attempt_list, &smtp.client_addr)) > 1) {
		lprintf(LOG_DEBUG,"%04d SMTP Throttling suspicious connection from: %s (%u login attempts)"
			,socket, host_ip, login_attempts);
		mswait(login_attempts*startup->login_attempt.throttle);
	}

	/* SMTP session active: */

	sockprintf(socket,session,"220 %s Synchronet SMTP Server %s-%s Ready"
		,startup->host_name,revision,PLATFORM_DESC);
	while(1) {
		rd = sockreadline(socket, session, buf, sizeof(buf));
		if(rd<0) 
			break;
		truncsp(buf);
		if(spy!=NULL)
			fprintf(spy,"%s\n",buf);
		if(relay_user.number==0 && dnsbl_result.s_addr && startup->options&MAIL_OPT_DNSBL_THROTTLE)
			mswait(DNSBL_THROTTLE_VALUE);
		if(state>=SMTP_STATE_DATA_HEADER) {
			if(!strcmp(buf,".")) {

				state=SMTP_STATE_HELO;	/* RESET state machine here in case of error */
				cmd=SMTP_CMD_NONE;

				if(msgtxt==NULL) {
					lprintf(LOG_ERR,"%04d !SMTP NO MESSAGE TEXT FILE POINTER?", socket);
					sockprintf(socket,session,"554 No message text");
					continue;
				}

				if(ftell(msgtxt)<1) {
					lprintf(LOG_ERR,"%04d !SMTP INVALID MESSAGE LENGTH: %ld (%lu lines)"
						, socket, ftell(msgtxt), lines);
					sockprintf(socket,session,"554 No message text");
					continue;
				}

				lprintf(LOG_INFO,"%04d SMTP End of message (body: %lu lines, %lu bytes, header: %lu lines, %lu bytes)"
					, socket, lines, ftell(msgtxt)-hdr_len, hdr_lines, hdr_len);

				if(!socket_check(socket, NULL, NULL, 0)) {
					lprintf(LOG_WARNING,"%04d !SMTP sender disconnected (premature evacuation)", socket);
					continue;
				}

				stats.msgs_received++;

				/* Twit-listing (sender's name and e-mail addresses) here */
				SAFEPRINTF(path,"%stwitlist.cfg",scfg.ctrl_dir);
				if(fexist(path) && (findstr(sender,path) || findstr(sender_addr,path))) {
					lprintf(LOG_NOTICE,"%04d !SMTP FILTERING TWIT-LISTED SENDER: %s <%s> (%u total)"
						,socket, sender, sender_addr, ++stats.msgs_refused);
					SAFEPRINTF2(tmp,"Twit-listed sender: %s <%s>", sender, sender_addr);
					spamlog(&scfg, "SMTP", "REFUSED", tmp, host_name, host_ip, rcpt_addr, reverse_path);
					sockprintf(socket,session, "554 Sender not allowed.");
					continue;
				}

				if(telegram==TRUE) {		/* Telegram */
					const char* head="\1n\1h\1cInstant Message\1n from \1h\1y";
					const char* tail="\1n:\r\n\1h";
					struct addrinfo ai;
					struct addrinfo *res,*cur;
					BOOL matched=FALSE;

					rewind(msgtxt);
					length=filelength(fileno(msgtxt));
					
					p=strchr(sender_addr,'@');
					memset(&ai, 0, sizeof(ai));
					ai.ai_flags = AI_PASSIVE;
					ai.ai_family = smtp.client_addr.addr.sa_family;
					if(getaddrinfo(p+1, NULL, &ai, &res) != 0)
						p=NULL;
					else {
						for(cur=res; cur; cur=cur->ai_next) {
							char cur_ip[INET6_ADDRSTRLEN];

							if(inet_addrtop((void *)cur->ai_addr, cur_ip, sizeof(cur_ip))) {
								if(strcmp(host_ip, cur_ip)==0)
									matched=TRUE;
							}
						}
						freeaddrinfo(res);
						if(!matched)
							p=NULL;
					}
					if(p==NULL)
						/* Append real IP and hostname if different */
						safe_snprintf(str,sizeof(str),"%s%s\r\n\1w[\1n%s\1h] (\1n%s\1h)%s"
							,head,sender_addr,host_ip,host_name,tail);
					else
						safe_snprintf(str,sizeof(str),"%s%s%s",head,sender_addr,tail);
					
					if((telegram_buf=(char*)malloc(length+strlen(str)+1))==NULL) {
						lprintf(LOG_CRIT,"%04d !SMTP ERROR allocating %lu bytes of memory for telegram from %s"
							,socket,length+strlen(str)+1,sender_addr);
						sockprintf(socket,session, insuf_stor);
						continue; 
					}
					strcpy(telegram_buf,str);	/* can't use SAFECOPY here */
					if(fread(telegram_buf+strlen(str),1,length,msgtxt)!=length) {
						lprintf(LOG_ERR,"%04d !SMTP ERROR reading %lu bytes from telegram file"
							,socket,length);
						sockprintf(socket,session, insuf_stor);
						free(telegram_buf);
						continue; 
					}
					telegram_buf[length+strlen(str)]=0;	/* Need ASCIIZ */

					/* Send telegram to users */
					sec_list=iniReadSectionList(rcptlst,NULL);	/* Each section is a recipient */
					for(rcpt_count=0; sec_list!=NULL
						&& sec_list[rcpt_count]!=NULL 
						&& (startup->max_recipients==0 || rcpt_count<startup->max_recipients); rcpt_count++) {

						section=sec_list[rcpt_count];

						SAFECOPY(rcpt_to,iniReadString(rcptlst,section	,smb_hfieldtype(RECIPIENT),"unknown",value));
						usernum=iniReadInteger(rcptlst,section				,smb_hfieldtype(RECIPIENTEXT),0);
						SAFECOPY(rcpt_addr,iniReadString(rcptlst,section	,smb_hfieldtype(RECIPIENTNETADDR),rcpt_to,value));

						if((i=putsmsg(&scfg,usernum,telegram_buf))==0)
							lprintf(LOG_INFO,"%04d SMTP Created telegram (%ld/%u bytes) from %s to %s <%s>"
								,socket, length, strlen(telegram_buf), sender_addr, rcpt_to, rcpt_addr);
						else
							lprintf(LOG_ERR,"%04d !SMTP ERROR %d creating telegram from %s to %s <%s>"
								,socket, i, sender_addr, rcpt_to, rcpt_addr);
					}
					iniFreeStringList(sec_list);
					free(telegram_buf);
					sockprintf(socket,session,ok_rsp);
					telegram=FALSE;
					continue;
				}

				fclose(msgtxt), msgtxt=NULL;
				fclose(rcptlst), rcptlst=NULL;

				/* External Mail Processing here */
				mailproc=NULL;
				msg_handled=FALSE;
				if(mailproc_count) {
					SAFEPRINTF2(proc_err_fname,"%sSBBS_SMTP.%s.err", scfg.temp_dir, session_id);
					remove(proc_err_fname);

					for(i=0;i<mailproc_count && !msg_handled;i++) {
						struct mailproc* mp=&mailproc_list[i];
						if(mp->disabled)
							continue;

						if(!mp->process_dnsbl && dnsbl_result.s_addr)
							continue;

						if(!mp->process_spam && spam_bait_result)
							continue;

						if(!chk_ar(&scfg,mp->ar,&relay_user,&client))
							continue;

						if(mp->to!=NULL && !mailproc_to_match[i])
							continue;

						if(mp->from!=NULL 
							&& !findstr_in_list(sender_addr, mp->from))
							continue;

						mailcmdstr(mp->cmdline
							,msgtxt_fname, newtxt_fname, logtxt_fname
							,rcptlst_fname, proc_err_fname
							,host_name, host_ip, relay_user.number
							,rcpt_addr
							,sender, sender_addr, reverse_path, str);
						lprintf(LOG_INFO,"%04d SMTP Executing external mail processor: %s"
							,socket, mp->name);

						if(mp->native) {
							lprintf(LOG_DEBUG,"%04d SMTP Executing external command: %s"
								,socket, str);
							if((j=system(str))!=0) {
								lprintf(LOG_NOTICE,"%04d SMTP system(%s) returned %d (errno: %d)"
									,socket, str, j, errno);
								if(mp->ignore_on_error) {
									lprintf(LOG_WARNING,"%04d !SMTP IGNORED MAIL due to mail processor (%s) error: %d"
										,socket, mp->name, j);
									msg_handled=TRUE;
								}
							}
						} else {  /* JavaScript */
							if(!js_mailproc(socket, &client, &relay_user
								,mp
								,str /* cmdline */
								,msgtxt_fname, newtxt_fname, logtxt_fname
								,rcpt_addr
								,rcptlst_fname, proc_err_fname
								,sender, sender_addr, reverse_path, hello_name, &js_result
								,&js_runtime, &js_cx, &js_glob
								,"SMTP") || js_result!=0) {
#if 0 /* calling exit() in a script causes js_mailproc to return FALSE */
								lprintf(LOG_NOTICE,"%04d !SMTP JavaScript mailproc command (%s) failed (returned: %d)"
									,socket, str, js_result);
								if(mailproc->ignore_on_error) {
									lprintf(LOG_WARNING,"%04d !SMTP IGNORED MAIL due to mail processor (%s) failure"
										,socket, mailproc->name);
									msg_handled=TRUE;
								}
#endif
							}
						}
						/* Log debug output (file) from mailproc: */
						if(flength(logtxt_fname) > 0 && (proc_out=fopen(logtxt_fname,"r"))!=NULL) {
							while(!feof(proc_out)) {
								if(!fgets(str,sizeof(str),proc_out))
									break;
								truncsp(str);
								lprintf(LOG_DEBUG,"%04d SMTP External mail processor (%s) debug: %s"
									,socket, mp->name, str);
							}
							fclose(proc_out);
						}
						remove(logtxt_fname);
						if(!mp->passthru || flength(proc_err_fname)>0 || !fexist(msgtxt_fname) || !fexist(rcptlst_fname)) {
							mailproc=mp;
							msg_handled=TRUE;
							break;
						}
					}
					if(flength(proc_err_fname)>0 
						&& (proc_out=fopen(proc_err_fname,"r"))!=NULL) {
						lprintf(LOG_WARNING,"%04d !SMTP External mail processor (%s) created: %s"
								,socket, mailproc->name, proc_err_fname);
						while(!feof(proc_out)) {
							int n;
							if(!fgets(str,sizeof(str),proc_out))
								break;
							truncsp(str);
							lprintf(LOG_WARNING,"%04d !SMTP External mail processor (%s) error: %s"
								,socket, mailproc->name, str);
							n=atoi(str);
							if(n>=100 && n<1000)
								sockprintf(socket,session,"%s", str);
							else
								sockprintf(socket,session,"554%c%s"
									,ftell(proc_out)<filelength(fileno(proc_out)) ? '-' : ' '
									,str);
						}
						fclose(proc_out);
						msg_handled=TRUE;
					}
					else if(!fexist(msgtxt_fname) || !fexist(rcptlst_fname)) {
						lprintf(LOG_NOTICE,"%04d SMTP External mail processor (%s) removed %s file"
							,socket
							,mailproc->name
							,fexist(msgtxt_fname)==FALSE ? "message text" : "recipient list");
						sockprintf(socket,session,ok_rsp);
						msg_handled=TRUE;
					}
					else if(msg_handled)
						sockprintf(socket,session,ok_rsp);
					remove(proc_err_fname);	/* Remove error file here */
				}

				/* Re-open files */
				/* We must do this before continuing for handled msgs */
				/* to prevent freopen(NULL) and orphaned temp files */
				if((rcptlst=fopen(rcptlst_fname,fexist(rcptlst_fname) ? "r":"w+"))==NULL) {
					lprintf(LOG_ERR,"%04d !SMTP ERROR %d re-opening recipient list: %s"
						,socket, errno, rcptlst_fname);
					if(!msg_handled)
						sockprintf(socket,session,sys_error);
					continue;
				}
			
				if(!msg_handled && subnum==INVALID_SUB && iniReadSectionCount(rcptlst,NULL) < 1) {
					lprintf(LOG_DEBUG,"%04d SMTP No recipients in recipient list file (message handled by external mail processor?)"
						,socket);
					sockprintf(socket,session,ok_rsp);
					msg_handled=TRUE;
				}
				if(msg_handled) {
					if(mailproc!=NULL)
						lprintf(LOG_NOTICE,"%04d SMTP Message handled by external mail processor (%s, %u total)"
							,socket, mailproc->name, ++mailproc->handled);
					continue;
				}

				/* If mailproc has written new message text to .new file, use that instead of .msg */
				if(flength(newtxt_fname) > 0) {
					remove(msgtxt_fname);
					SAFECOPY(msgtxt_fname, newtxt_fname);
				} else
					remove(newtxt_fname);

				if((msgtxt=fopen(msgtxt_fname,"rb"))==NULL) {
					lprintf(LOG_ERR,"%04d !SMTP ERROR %d re-opening message file: %s"
						,socket, errno, msgtxt_fname);
					sockprintf(socket,session,sys_error);
					continue;
				}

				/* Initialize message header */
				smb_freemsgmem(&msg);
				memset(&msg,0,sizeof(smbmsg_t));		

				/* Parse message header here */
				hfield_type=UNKNOWN;
				smb_error=SMB_SUCCESS; /* no SMB error */
				errmsg=insuf_stor;
				while(!feof(msgtxt)) {
					char field[32];

					if(!fgets(buf,sizeof(buf),msgtxt))
						break;
					truncsp(buf);
					if(buf[0]==0)	/* blank line marks end of header */
						break;

					if((p=get_header_field(buf, field, sizeof(field)))!=NULL) {
						if(stricmp(field, "SUBJECT")==0) {
							/* SPAM Filtering/Logging */
							if(relay_user.number==0) {
								if(trashcan(&scfg,p,"subject")) {
									lprintf(LOG_NOTICE,"%04d !SMTP BLOCKED SUBJECT (%s) from: %s (%u total)"
										,socket, p, reverse_path, ++stats.msgs_refused);
									SAFEPRINTF2(tmp,"Blocked subject (%s) from: %s"
										,p, reverse_path);
									spamlog(&scfg, "SMTP", "REFUSED"
										,tmp, host_name, host_ip, rcpt_addr, reverse_path);
									errmsg="554 Subject not allowed.";
									smb_error=SMB_FAILURE;
									break;
								}
								if(dnsbl_result.s_addr && startup->dnsbl_tag[0] && !(startup->options&MAIL_OPT_DNSBL_IGNORE)) {
									safe_snprintf(str,sizeof(str),"%.*s: %.*s"
										,(int)sizeof(str)/2, startup->dnsbl_tag
										,(int)sizeof(str)/2, p);
									p=str;
									lprintf(LOG_NOTICE,"%04d SMTP TAGGED MAIL SUBJECT from blacklisted server with: %s"
										,socket, startup->dnsbl_tag);
									msg.hdr.attr |= MSG_SPAM;
								}
							}
							smb_hfield_str(&msg, hfield_type=SUBJECT, p);
							continue;
						}
						if(relay_user.number==0	&& stricmp(field, "FROM")==0
							&& !chk_email_addr(socket,p,host_name,host_ip,rcpt_addr,reverse_path,"FROM")) {
							errmsg="554 Sender not allowed.";
							smb_error=SMB_FAILURE;
							break;
						}
						if(relay_user.number==0 && stricmp(field, "TO")==0 && !spam_bait_result
							&& !chk_email_addr(socket,p,host_name,host_ip,rcpt_addr,reverse_path,"TO")) {
							errmsg="550 Unknown user.";
							smb_error=SMB_FAILURE;
							break;
						}
						if(stricmp(field, "X-Spam-Flag") == 0 && stricmp(p, "Yes") == 0)
							msg.hdr.attr |= MSG_SPAM;	/* e.g. flagged by SpamAssasin */
					}
					if((smb_error=parse_header_field((char*)buf,&msg,&hfield_type))!=SMB_SUCCESS) {
						if(smb_error==SMB_ERR_HDR_LEN)
							lprintf(LOG_WARNING,"%04d !SMTP MESSAGE HEADER EXCEEDS %u BYTES"
								,socket, SMB_MAX_HDR_LEN);
						else
							lprintf(LOG_ERR,"%04d !SMTP ERROR %d adding header field: %s"
								,socket, smb_error, buf);
						break;
					}
				}
				if(smb_error!=SMB_SUCCESS) {	/* SMB Error */
					sockprintf(socket,session, errmsg);
					stats.msgs_refused++;
					continue;
				}
				if((p=smb_get_hfield(&msg, RFC822TO, NULL))!=NULL) {
					parse_mail_address(p
						,rcpt_name	,sizeof(rcpt_name)-1
						,rcpt_addr	,sizeof(rcpt_addr)-1);
				}
				if((p=smb_get_hfield(&msg, RFC822FROM, NULL))!=NULL) {
					parse_mail_address(p 
						,sender		,sizeof(sender)-1
						,sender_addr,sizeof(sender_addr)-1);
				}
				dnsbl_recvhdr=FALSE;
				if(startup->options&MAIL_OPT_DNSBL_CHKRECVHDRS)  {
					for(i=0;!dnsbl_result.s_addr && i<msg.total_hfields;i++)  {
						if(msg.hfield[i].type == SMTPRECEIVED)  {
							if(chk_received_hdr(socket,msg.hfield_dat[i],&dnsbl_result,dnsbl,dnsbl_ip)) {
								dnsbl_recvhdr=TRUE;
								break;
							}
						}
					}
				}
				if(relay_user.number==0 && dnsbl_result.s_addr && !(startup->options&MAIL_OPT_DNSBL_IGNORE)) {
					msg.hdr.attr |= MSG_SPAM;
					/* tag message as spam */
					if(startup->dnsbl_hdr[0]) {
						safe_snprintf(str,sizeof(str),"%s: %s is listed on %s as %s"
							,startup->dnsbl_hdr, dnsbl_ip
							,dnsbl, inet_ntoa(dnsbl_result));
						smb_hfield_str(&msg, RFC822HEADER, str);
						lprintf(LOG_NOTICE,"%04d SMTP TAGGED MAIL HEADER from blacklisted server with: %s"
							,socket, startup->dnsbl_hdr);
					}
					if(startup->dnsbl_hdr[0] || startup->dnsbl_tag[0]) {
						SAFEPRINTF2(str,"Listed on %s as %s", dnsbl, inet_ntoa(dnsbl_result));
						spamlog(&scfg, "SMTP", "TAGGED", str, host_name, dnsbl_ip, rcpt_addr, reverse_path);
					}
				}
				if(dnsbl_recvhdr)			/* DNSBL-listed IP found in Received header? */
					dnsbl_result.s_addr=0;	/* Reset DNSBL look-up result between messages */
				
				if((scfg.sys_misc&SM_DELREADM)
					|| ((startup->options&MAIL_OPT_KILL_READ_SPAM) && (msg.hdr.attr&MSG_SPAM)))
					msg.hdr.attr |= MSG_KILLREAD;

				if(sender[0]==0) {
					lprintf(LOG_WARNING,"%04d !SMTP MISSING mail header 'FROM' field (%u total)"
						,socket, ++stats.msgs_refused);
					sockprintf(socket,session, "554 Mail header missing 'FROM' field");
					subnum=INVALID_SUB;
					continue;
				}
				if(relay_user.number) {
					SAFEPRINTF(str,"%u",relay_user.number);
					smb_hfield_str(&msg, SENDEREXT, str);
				}
				if(relay_user.number && subnum!=INVALID_SUB) {
					nettype=NET_NONE;
					smb_hfield_str(&msg, SENDER, relay_user.alias);
				} else {
					nettype=NET_INTERNET;
					smb_hfield_str(&msg, SENDER, sender);
					smb_hfield(&msg, SENDERNETTYPE, sizeof(nettype), &nettype);
					smb_hfield_str(&msg, SENDERNETADDR, sender_addr);
				}
				smb_hfield_str(&msg, SMTPREVERSEPATH, reverse_path);
				if(msg.subj==NULL)
					smb_hfield(&msg, SUBJECT, 0, NULL);

				length=filelength(fileno(msgtxt))-ftell(msgtxt);

				if(startup->max_msg_size && length>startup->max_msg_size) {
					lprintf(LOG_WARNING,"%04d !SMTP Message size (%lu) exceeds maximum: %lu bytes"
						,socket,length,startup->max_msg_size);
					sockprintf(socket,session, "552 Message size (%lu) exceeds maximum: %lu bytes"
						,length,startup->max_msg_size);
					subnum=INVALID_SUB;
					stats.msgs_refused++;
					continue;
				}

				if((msgbuf=(char*)malloc(length+1))==NULL) {
					lprintf(LOG_CRIT,"%04d !SMTP ERROR allocating %d bytes of memory"
						,socket,length+1);
					sockprintf(socket,session, insuf_stor);
					subnum=INVALID_SUB;
					continue;
				}
				fread(msgbuf,length,1,msgtxt);
				msgbuf[length]=0;	/* ASCIIZ */

				/* Do external JavaScript processing here? */

				if(subnum!=INVALID_SUB) {	/* Message Base */
					uint reason;
					if(relay_user.number==0)
						memset(&relay_user,0,sizeof(relay_user));

					if(!can_user_post(&scfg,subnum,&relay_user,&client,&reason)) {
						lprintf(LOG_WARNING,"%04d !SMTP %s (user #%u) cannot post on %s (reason: %u)"
							,socket, sender_addr, relay_user.number
							,scfg.sub[subnum]->sname, reason + 1);
						sockprintf(socket,session,"550 Insufficient access");
						subnum=INVALID_SUB;
						stats.msgs_refused++;
						continue;
					}

					if(rcpt_name[0]==0)
						strcpy(rcpt_name,"All");
					smb_hfield_str(&msg, RECIPIENT, rcpt_name);

					smb.subnum=subnum;
					if((i=savemsg(&scfg, &smb, &msg, &client, startup->host_name, msgbuf))!=SMB_SUCCESS) {
						lprintf(LOG_WARNING,"%04d !SMTP ERROR %d (%s) saving message"
							,socket,i,smb.last_error);
						sockprintf(socket,session, "452 ERROR %d (%s) saving message"
							,i,smb.last_error);
					} else {
						lprintf(LOG_INFO,"%04d SMTP %s posted a message on %s"
							,socket, sender_addr, scfg.sub[subnum]->sname);
						sockprintf(socket,session,ok_rsp);
						if(relay_user.number != 0)
							user_posted_msg(&scfg, &relay_user, 1);
						signal_smtp_sem();
					}
					free(msgbuf);
					smb_close(&smb);
					subnum=INVALID_SUB;
					continue;
				}

				/* Create/check hashes of known SPAM */
				{
					hash_t**	hashes;
					BOOL		is_spam=spam_bait_result;
					long		sources=SMB_HASH_SOURCE_SPAM;

					if((dnsbl_recvhdr || dnsbl_result.s_addr) && startup->options&MAIL_OPT_DNSBL_SPAMHASH)
						is_spam=TRUE;

					lprintf(LOG_DEBUG,"%04d SMTP Calculating message hashes (sources=%lx, msglen=%u)"
						,socket, sources, strlen(msgbuf));
					if((hashes=smb_msghashes(&msg, (uchar*)msgbuf, sources)) != NULL) {
						hash_t	found;

						for(i=0;hashes[i];i++)
							lprintf(LOG_DEBUG,"%04d SMTP Message %s crc32=%lx flags=%lx length=%u"
								,socket, smb_hashsourcetype(hashes[i]->source)
								,hashes[i]->crc32, hashes[i]->flags, hashes[i]->length);

						if((i=smb_findhash(&spam, hashes, &found, sources, /* Mark: */TRUE))==SMB_SUCCESS) {
							SAFEPRINTF3(str,"%s (%s) found in SPAM database (added on %s)"
								,smb_hashsourcetype(found.source)
								,smb_hashsource(&msg,found.source)
								,timestr(&scfg,found.time,tmp)
								);
							lprintf(LOG_NOTICE,"%04d SMTP Message %s", socket, str);
							if(!is_spam) {
								spamlog(&scfg, "SMTP", "IGNORED"
									,str, host_name, host_ip, rcpt_addr, reverse_path);
								is_spam=TRUE;
							}
						} else if(i!=SMB_ERR_NOT_FOUND)
							lprintf(LOG_ERR,"%04d !SMTP ERROR %d (%s) opening SPAM database"
								,socket, i, spam.last_error);
						
						if(is_spam) {
							size_t	n,total=0;
							for(n=0;hashes[n]!=NULL;n++)
								if(!(hashes[n]->flags&SMB_HASH_MARKED)) {
									lprintf(LOG_INFO,"%04d SMTP Adding message %s (%s) to SPAM database"
										,socket
										,smb_hashsourcetype(hashes[n]->source)
										,smb_hashsource(&msg,hashes[n]->source)
										);
									total++;
								}
							if(total) {
								lprintf(LOG_DEBUG,"%04d SMTP Adding %u message hashes to SPAM database", socket, total);
								smb_addhashes(&spam, hashes, /* skip_marked: */TRUE);
							}
							if(i!=SMB_SUCCESS && !spam_bait_result && (dnsbl_recvhdr || dnsbl_result.s_addr))
								is_spam=FALSE;
						}
						smb_close_hash(&spam);

						smb_freehashes(hashes);
					} else
						lprintf(LOG_ERR,"%04d SMTP smb_msghashes returned NULL", socket);

					if(is_spam || ((startup->options&MAIL_OPT_DNSBL_IGNORE) && (dnsbl_recvhdr || dnsbl_result.s_addr))) {
						free(msgbuf);
						if(is_spam)
							lprintf(LOG_NOTICE,"%04d !SMTP IGNORED SPAM MESSAGE (%u total)"
								,socket, ++stats.msgs_ignored);
						else {
							SAFEPRINTF2(str,"Listed on %s as %s", dnsbl, inet_ntoa(dnsbl_result));
							lprintf(LOG_NOTICE,"%04d !SMTP IGNORED MAIL from server: %s (%u total)"
								,socket, str, ++stats.msgs_ignored);
							spamlog(&scfg, "SMTP", "IGNORED"
								,str, host_name, dnsbl_ip, rcpt_addr, reverse_path);
						}
						/* pretend we received it */
						sockprintf(socket,session,ok_rsp);
						subnum=INVALID_SUB;
						continue;
					}
				}

				/* E-mail */
				smb.subnum=INVALID_SUB;
				/* creates message data, but no header or index records (since msg.to==NULL) */
				i=savemsg(&scfg, &smb, &msg, &client, startup->host_name, msgbuf);
				free(msgbuf);
				if(i!=SMB_SUCCESS) {
					smb_close(&smb);
					lprintf(LOG_CRIT,"%04d !SMTP ERROR %d (%s) saving message"
						,socket,i,smb.last_error);
					sockprintf(socket,session, "452 ERROR %d (%s) saving message"
						,i,smb.last_error);
					continue;
				}

				lprintf(LOG_DEBUG,"%04d SMTP Recipient name: '%s'", socket, rcpt_name);

				sec_list=iniReadSectionList(rcptlst,NULL);	/* Each section is a recipient */
				for(rcpt_count=0; sec_list!=NULL
					&& sec_list[rcpt_count]!=NULL 
					&& (startup->max_recipients==0 || rcpt_count<startup->max_recipients); rcpt_count++) {
				
					section=sec_list[rcpt_count];

					SAFECOPY(rcpt_to,iniReadString(rcptlst,section	,smb_hfieldtype(RECIPIENT),"unknown",value));
					usernum=iniReadInteger(rcptlst,section				,smb_hfieldtype(RECIPIENTEXT),0);
					agent=iniReadShortInt(rcptlst,section				,smb_hfieldtype(RECIPIENTAGENT),AGENT_PERSON);
					nettype=iniReadShortInt(rcptlst,section				,smb_hfieldtype(RECIPIENTNETTYPE),NET_NONE);
					SAFEPRINTF(str,"#%u",usernum);
					SAFECOPY(rcpt_addr,iniReadString(rcptlst,section	,smb_hfieldtype(RECIPIENTNETADDR),str,value));

					if(nettype==NET_NONE /* Local destination */ && usernum==0) {
						lprintf(LOG_ERR,"%04d !SMTP can't deliver mail to user #0"
							,socket);
						break;
					}

					if((i=smb_copymsgmem(&smb,&newmsg,&msg))!=SMB_SUCCESS) {
						lprintf(LOG_ERR,"%04d !SMTP ERROR %d (%s) copying message"
							,socket, i, smb.last_error);
						break;
					}

					with_val = 0;
					if (esmtp)
						with_val |= WITH_ESMTP;
					if (auth_login)
						with_val |= WITH_AUTH;
					if (session != -1)
						with_val |= WITH_TLS;

					snprintf(hdrfield,sizeof(hdrfield),
						"from %s (%s [%s%s])\r\n"
						"          by %s [%s%s] (%s %s-%s) with %s\r\n"
						"          for %s; %s\r\n"
						"          (envelope-from %s)"
						,host_name,hello_name
						,smtp.client_addr.addr.sa_family==AF_INET6?"IPv6: ":""
						,host_ip
						,startup->host_name
						,server_addr.addr.sa_family==AF_INET6?"IPv6: ":""
						,server_ip
						,server_name
						,revision,PLATFORM_DESC
						,with_clauses[with_val]
						,rcpt_to,msgdate(msg.hdr.when_imported,date)
						,reverse_path);
					smb_hfield_add_str(&newmsg, SMTPRECEIVED, hdrfield, /* insert: */TRUE);

					if(nettype == NET_FIDO) {
						char* tp = strchr(rcpt_name, '@');
						if(tp != NULL)
							*tp = 0;
					}
					smb_hfield_str(&newmsg, RECIPIENT, rcpt_name);

					if(usernum && nettype!=NET_INTERNET) {	/* Local destination or QWKnet routed */
						/* This is required for fixsmb to be able to rebuild the index */
						SAFEPRINTF(str,"%u",usernum);
						smb_hfield_str(&newmsg, RECIPIENTEXT, str);
					}
					if(nettype!=NET_NONE) {
						smb_hfield(&newmsg, RECIPIENTNETTYPE, sizeof(nettype), &nettype);
						smb_hfield_netaddr(&newmsg, RECIPIENTNETADDR, rcpt_addr, &nettype);
					}
					if(agent!=newmsg.to_agent)
						smb_hfield(&newmsg, RECIPIENTAGENT, sizeof(agent), &agent);

					i=smb_addmsghdr(&smb,&newmsg,smb_storage_mode(&scfg, &smb));
					smb_freemsgmem(&newmsg);
					if(i!=SMB_SUCCESS) {
						lprintf(LOG_ERR,"%04d !SMTP ERROR %d (%s) adding message header"
							,socket, i, smb.last_error);
						break;
					}
					sender_ext[0]=0;
					if(msg.from_ext!=NULL)
						SAFEPRINTF(sender_ext," #%s",msg.from_ext);
					lprintf(LOG_INFO,"%04d SMTP Created message #%ld from %s%s [%s] to %s [%s]"
						,socket, newmsg.hdr.number, sender, sender_ext, smb_netaddrstr(&msg.from_net,tmp), rcpt_name, rcpt_addr);
					if(relay_user.number!=0)
						user_sent_email(&scfg, &relay_user, 1, usernum==1);

					if(nettype == NET_FIDO && scfg.netmail_sem[0])
						ftouch(mailcmdstr(scfg.netmail_sem
							,msgtxt_fname, newtxt_fname, logtxt_fname
							,rcptlst_fname, proc_err_fname
							,host_name, host_ip, relay_user.number
							,rcpt_addr
							,sender, sender_addr, reverse_path, str));

					if(!(startup->options&MAIL_OPT_NO_NOTIFY) && usernum) {
						if(newmsg.idx.to)
							for(i=1;i<=scfg.sys_nodes;i++) {
								getnodedat(&scfg, i, &node, 0);
								if(node.useron==usernum
									&& (node.status==NODE_INUSE || node.status==NODE_QUIET))
									break;
							}
						if(!newmsg.idx.to || i<=scfg.sys_nodes) {
							p=sender_addr;
							if(stricmp(sender, sender_addr) == 0) {
								if((p = strchr(sender_addr, '@')) == NULL)
									p = sender_addr;
								else
									p++;
							}
							safe_snprintf(str,sizeof(str)
								,startup->newmail_notice
								,timestr(&scfg,newmsg.hdr.when_imported.time,tmp)
								,sender, p);
							if(!newmsg.idx.to) 	/* Forwarding */
								sprintf(str+strlen(str), startup->forward_notice, rcpt_addr);
							putsmsg(&scfg, usernum, str);
						}
					}
				}
				iniFreeStringList(sec_list);
				if(rcpt_count<1) {
					smb_freemsg_dfields(&smb,&msg,SMB_ALL_REFS);
					sockprintf(socket,session, insuf_stor);
				}
				else {
					if(rcpt_count>1)
						smb_incmsg_dfields(&smb,&msg,(ushort)(rcpt_count-1));
					sockprintf(socket,session,ok_rsp);
					signal_smtp_sem();
				}
#if 0 /* This shouldn't be necessary here */
				smb_close_da(&smb);
#endif
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
				if(msgtxt!=NULL) {
					switch(content_encoding) {
						case ENCODING_BASE64:
							{
								char	decode_buf[sizeof(buf)];

								if(b64_decode(decode_buf, sizeof(decode_buf), p, strlen(p))<0)
									fprintf(msgtxt,"\r\n!Base64 decode error: %s\r\n", p);
								else
									fputs(decode_buf, msgtxt);
							}
							break;
						case ENCODING_QUOTED_PRINTABLE:
							fputs(qp_decode(p), msgtxt);
							break;
						default:
							fprintf(msgtxt, "%s\r\n", p);
							break;
					}
				}
				lines++;
				/* release time-slices every x lines */
				if(startup->lines_per_yield &&
					!(lines%startup->lines_per_yield))	
					YIELD();
				continue;
			}
			/* RFC822 Header parsing */
			strip_char(buf, '\r');	/* There should be no bare carriage returns in header fields */
			if(startup->options&MAIL_OPT_DEBUG_RX_HEADER)
				lprintf(LOG_DEBUG,"%04d SMTP %s",socket, buf);

			{
				char field[32];

				if((p=get_header_field(buf, field, sizeof(field)))!=NULL) {
					if(stricmp(field, "FROM")==0) {
						parse_mail_address(p
							,sender,		sizeof(sender)-1
							,sender_addr,	sizeof(sender_addr)-1);
					}
					else if(stricmp(field,"CONTENT-TRANSFER-ENCODING")==0) {
						lprintf(LOG_INFO,"%04d SMTP %s = %s", socket, field, p);
						if(stricmp(p,"base64")==0)
							content_encoding=ENCODING_BASE64;
						else if(stricmp(p,"quoted-printable")==0)
							content_encoding=ENCODING_QUOTED_PRINTABLE;
						else {	/* Other (e.g. 7bit, 8bit, binary) */
							content_encoding=ENCODING_NONE;
							if(msgtxt!=NULL) 
								fprintf(msgtxt, "%s\r\n", buf);
						}
						hdr_lines++;
						continue;
					}
				}
			}

			if(msgtxt!=NULL) 
				fprintf(msgtxt, "%s\r\n", buf);
			hdr_lines++;
			continue;
		}
		strip_ctrl(buf, buf);
		lprintf(LOG_DEBUG,"%04d SMTP RX: %s", socket, buf);
		if(!strnicmp(buf,"HELO",4)) {
			p=buf+4;
			SKIP_WHITESPACE(p);
			SAFECOPY(hello_name,p);
			sockprintf(socket,session,"250 %s",startup->host_name);
			esmtp=FALSE;
			state=SMTP_STATE_HELO;
			cmd=SMTP_CMD_NONE;
			telegram=FALSE;
			subnum=INVALID_SUB;
			continue;
		}
		if(!strnicmp(buf,"EHLO",4)) {
			p=buf+4;
			SKIP_WHITESPACE(p);
			SAFECOPY(hello_name,p);
			sockprintf(socket,session,"250-%s",startup->host_name);
			sockprintf(socket,session,"250-AUTH PLAIN LOGIN CRAM-MD5");
			sockprintf(socket,session,"250-SEND");
			sockprintf(socket,session,"250-SOML");
			sockprintf(socket,session,"250-SAML");
			sockprintf(socket,session,"250-8BITMIME");
			if (session != -1)
				sockprintf(socket,session,"250-STARTTLS");
			if (startup->max_msg_size)
				sockprintf(socket,session,"250-SIZE %lu", startup->max_msg_size);
			sockprintf(socket,session,ok_rsp);
			esmtp=TRUE;
			state=SMTP_STATE_HELO;
			cmd=SMTP_CMD_NONE;
			telegram=FALSE;
			subnum=INVALID_SUB;
			continue;
		}
		ZERO_VAR(user_pass);
		if((auth_login=(stricmp(buf,"AUTH LOGIN")==0))==TRUE 
			|| strnicmp(buf,"AUTH PLAIN",10)==0) {
			if(auth_login) {
				sockprintf(socket,session,"334 VXNlcm5hbWU6");	/* Base64-encoded "Username:" */
				if((rd=sockreadline(socket, session, buf, sizeof(buf)))<1) {
					lprintf(LOG_WARNING,"%04d !SMTP missing AUTH LOGIN username argument", socket);
					badlogin(socket, session, client.protocol, badarg_rsp, NULL, NULL, host_name, &smtp.client_addr);
					continue;
				}
				if(startup->options&MAIL_OPT_DEBUG_RX_RSP) 
					lprintf(LOG_DEBUG,"%04d RX: %s",socket,buf);
				if(b64_decode(user_name,sizeof(user_name),buf,rd)<1) {
					lprintf(LOG_WARNING,"%04d !SMTP bad AUTH LOGIN username argument", socket);
					badlogin(socket, session, client.protocol, badarg_rsp, NULL, NULL, host_name, &smtp.client_addr);
					continue;
				}
				sockprintf(socket,session,"334 UGFzc3dvcmQ6");	/* Base64-encoded "Password:" */
				if((rd=sockreadline(socket, session, buf, sizeof(buf)))<1) {
					lprintf(LOG_WARNING,"%04d !SMTP missing AUTH LOGIN password argument", socket);
					badlogin(socket, session, client.protocol, badarg_rsp, user_name, NULL, host_name, &smtp.client_addr);
					continue;
				}
				if(startup->options&MAIL_OPT_DEBUG_RX_RSP) 
					lprintf(LOG_DEBUG,"%04d RX: %s",socket,buf);
				if(b64_decode(user_pass,sizeof(user_pass),buf,rd)<1) {
					lprintf(LOG_WARNING,"%04d !SMTP bad AUTH LOGIN password argument", socket);
					badlogin(socket, session, client.protocol, badarg_rsp, user_name, NULL, host_name, &smtp.client_addr);
					continue;
				}
			} else {	/* AUTH PLAIN b64(<username>\0<user-id>\0<password>) */
				p=buf+10;
				SKIP_WHITESPACE(p);
				if(*p==0) {
					lprintf(LOG_WARNING,"%04d !SMTP missing AUTH PLAIN argument", socket);
					badlogin(socket, session, client.protocol, badarg_rsp, NULL, NULL, host_name, &smtp.client_addr);
					continue;
				}
				ZERO_VAR(tmp);
				if(b64_decode(tmp,sizeof(tmp),p,strlen(p))<1) {
					lprintf(LOG_WARNING,"%04d !SMTP bad AUTH PLAIN argument", socket);
					badlogin(socket, session, client.protocol, badarg_rsp, NULL, NULL, host_name, &smtp.client_addr);
					continue;
				}
				p=tmp;
				while(*p) p++;	/* skip username */
				p++;			/* skip NULL */
				if(*p==0) {
					lprintf(LOG_WARNING,"%04d !SMTP missing AUTH PLAIN user-id argument", socket);
					badlogin(socket, session, client.protocol, badarg_rsp, NULL, NULL, host_name, &smtp.client_addr);
					continue;
				}
				SAFECOPY(user_name,p);
				while(*p) p++;	/* skip user-id */
				p++;			/* skip NULL */
				if(*p==0) {
					lprintf(LOG_WARNING,"%04d !SMTP missing AUTH PLAIN password argument", socket);
					badlogin(socket, session, client.protocol, badarg_rsp, user_name, NULL, host_name, &smtp.client_addr);
					continue;
				}
				SAFECOPY(user_pass,p);
			}

			if((relay_user.number=matchuser(&scfg,user_name,FALSE))==0) {
				if(scfg.sys_misc&SM_ECHO_PW)
					lprintf(LOG_WARNING,"%04d !SMTP UNKNOWN USER: '%s' (password: %s)"
						,socket, user_name, user_pass);
				else
					lprintf(LOG_WARNING,"%04d !SMTP UNKNOWN USER: '%s'"
						,socket, user_name);
				badlogin(socket, session, client.protocol, badauth_rsp, user_name, user_pass, host_name, &smtp.client_addr);
				break;
			}
			if((i=getuserdat(&scfg, &relay_user))!=0) {
				lprintf(LOG_ERR,"%04d !SMTP ERROR %d getting data on user (%s)"
					,socket, i, user_name);
				badlogin(socket, session, client.protocol, badauth_rsp, NULL, NULL, NULL, NULL);
				break;
			}
			if(relay_user.misc&(DELETED|INACTIVE)) {
				lprintf(LOG_WARNING,"%04d !SMTP DELETED or INACTIVE user #%u (%s)"
					,socket, relay_user.number, user_name);
				badlogin(socket, session, client.protocol, badauth_rsp, NULL, NULL, NULL, NULL);
				break;
			}
			if(stricmp(user_pass,relay_user.pass)) {
				if(scfg.sys_misc&SM_ECHO_PW)
					lprintf(LOG_WARNING,"%04d !SMTP FAILED Password attempt for user %s: '%s' expected '%s'"
						,socket, user_name, user_pass, relay_user.pass);
				else
					lprintf(LOG_WARNING,"%04d !SMTP FAILED Password attempt for user %s"
						,socket, user_name);
				badlogin(socket, session, client.protocol, badauth_rsp, user_name, user_pass, host_name, &smtp.client_addr);
				break;
			}

			if(relay_user.pass[0])
				loginSuccess(startup->login_attempt_list, &smtp.client_addr);

			/* Update client display */
			client.user=relay_user.alias;
			client_on(socket,&client,TRUE /* update */);

			lprintf(LOG_INFO,"%04d SMTP %s authenticated using %s authentication"
				,socket,relay_user.alias,auth_login ? "LOGIN" : "PLAIN");
			sockprintf(socket,session,auth_ok);
			continue;
		}
		if(!stricmp(buf,"AUTH CRAM-MD5")) {
			safe_snprintf(challenge,sizeof(challenge),"<%x%x%lx%lx@%s>"
				,rand(),socket,(ulong)time(NULL),clock(),startup->host_name);
#if 0
			lprintf(LOG_DEBUG,"%04d SMTP CRAM-MD5 challenge: %s"
				,socket,challenge);
#endif
			b64_encode(str,sizeof(str),challenge,0);
			sockprintf(socket,session,"334 %s",str);
			if((rd=sockreadline(socket, session, buf, sizeof(buf)))<1) {
				lprintf(LOG_WARNING,"%04d !SMTP missing AUTH CRAM-MD5 response", socket);
				sockprintf(socket,session,badarg_rsp);
				continue;
			}
			if(startup->options&MAIL_OPT_DEBUG_RX_RSP) 
				lprintf(LOG_DEBUG,"%04d RX: %s",socket,buf);

			if(b64_decode(response,sizeof(response),buf,rd)<1) {
				lprintf(LOG_WARNING,"%04d !SMTP bad AUTH CRAM-MD5 response", socket);
				sockprintf(socket,session,badarg_rsp);
				continue;
			}
#if 0
			lprintf(LOG_DEBUG,"%04d SMTP CRAM-MD5 response: %s"
				,socket,response);
#endif
			if((p=strrchr(response,' '))!=NULL)
				*(p++)=0;
			else
				p=response;
			SAFECOPY(user_name,response);
			if((relay_user.number=matchuser(&scfg,user_name,FALSE))==0) {
				lprintf(LOG_WARNING,"%04d !SMTP UNKNOWN USER: '%s'"
					,socket, user_name);
				badlogin(socket, session, client.protocol, badauth_rsp, user_name, NULL, host_name, &smtp.client_addr);
				break;
			}
			if((i=getuserdat(&scfg, &relay_user))!=0) {
				lprintf(LOG_ERR,"%04d !SMTP ERROR %d getting data on user (%s)"
					,socket, i, user_name);
				badlogin(socket, session, client.protocol, badauth_rsp, NULL, NULL, NULL, NULL);
				break;
			}
			if(relay_user.misc&(DELETED|INACTIVE)) {
				lprintf(LOG_WARNING,"%04d !SMTP DELETED or INACTIVE user #%u (%s)"
					,socket, relay_user.number, user_name);
				badlogin(socket, session, client.protocol, badauth_rsp, NULL, NULL, NULL, NULL);
				break;
			}
			/* Calculate correct response */
			memset(secret,0,sizeof(secret));
			SAFECOPY(secret,relay_user.pass);
			strlwr(secret);	/* this is case sensitive, so convert to lowercase first */
			for(i=0;i<sizeof(secret);i++)
				md5_data[i]=secret[i]^0x36;	/* ipad */
			strcpy(md5_data+i,challenge);
			MD5_calc(digest,md5_data,sizeof(secret)+strlen(challenge));
			for(i=0;i<sizeof(secret);i++)
				md5_data[i]=secret[i]^0x5c;	/* opad */
			memcpy(md5_data+i,digest,sizeof(digest));
			MD5_calc(digest,md5_data,sizeof(secret)+sizeof(digest));
			MD5_hex((BYTE*)str,digest);
			if(strcmp(p,str)) {
				lprintf(LOG_WARNING,"%04d !SMTP %s FAILED CRAM-MD5 authentication"
					,socket,relay_user.alias);
#if 0
				lprintf(LOG_DEBUG,"%04d !SMTP calc digest: %s"
					,socket,str);
				lprintf(LOG_DEBUG,"%04d !SMTP resp digest: %s"
					,socket,p);
#endif
				badlogin(socket, session, client.protocol, badauth_rsp, user_name, p, host_name, &smtp.client_addr);
				break;
			}

			if(relay_user.pass[0])
				loginSuccess(startup->login_attempt_list, &smtp.client_addr);

			/* Update client display */
			client.user=relay_user.alias;
			client_on(socket,&client,TRUE /* update */);

			lprintf(LOG_INFO,"%04d SMTP %s authenticated using CRAM-MD5 authentication"
				,socket,relay_user.alias);
			sockprintf(socket,session,auth_ok);
			continue;
		}
		if(!strnicmp(buf,"AUTH",4)) {
			sockprintf(socket,session,"504 Unrecognized authentication type.");
			continue;
		}
		if(!stricmp(buf,"QUIT")) {
			sockprintf(socket,session,"221 %s Service closing transmission channel",startup->host_name);
			break;
		} 
		if(!stricmp(buf,"NOOP")) {
			sockprintf(socket,session, ok_rsp);
			badcmds=0;
			continue;
		}
		if(state<SMTP_STATE_HELO) {
			/* RFC 821 4.1.1 "The first command in a session must be the HELO command." */
			lprintf(LOG_WARNING,"%04d !SMTP MISSING 'HELO' command (Received: '%s')",socket, buf);
			sockprintf(socket,session, badseq_rsp);
			continue;
		}
		if(!stricmp(buf,"TURN")) {
			sockprintf(socket,session,"502 command not supported");
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
			spam_bait_result=FALSE;

			/* reset recipient list */
			if((rcptlst=freopen(rcptlst_fname,"w+",rcptlst))==NULL) {
				lprintf(LOG_ERR,"%04d !SMTP ERROR %d re-opening %s"
					,socket, errno, rcptlst_fname);
				sockprintf(socket,session,sys_error);
				break;
			}
			rcpt_count=0;
			content_encoding=ENCODING_NONE;

			memset(mailproc_to_match,FALSE,sizeof(BOOL)*mailproc_count);

			sockprintf(socket,session,ok_rsp);
			badcmds=0;
			lprintf(LOG_INFO,"%04d SMTP Session reset",socket);
			continue;
		}
		if(!strnicmp(buf,"MAIL FROM:",10)
			|| !strnicmp(buf,"SEND FROM:",10)	/* Send a Message (Telegram) to a local ONLINE user */
			|| !strnicmp(buf,"SOML FROM:",10)	/* Send OR Mail a Message to a local user */
			|| !strnicmp(buf,"SAML FROM:",10)	/* Send AND Mail a Message to a local user */
			) {
			p=buf+10;
			if(relay_user.number==0
				&& !chk_email_addr(socket,p,host_name,host_ip,NULL,NULL,"REVERSE PATH")) {
				sockprintf(socket,session, "554 Sender not allowed.");
				stats.msgs_refused++;
				break;
			}
			SKIP_WHITESPACE(p);
			SAFECOPY(reverse_path,p);
			if((p=strchr(reverse_path,' '))!=NULL)	/* Truncate "<user@domain> KEYWORD=VALUE" to just "<user@domain>" per RFC 1869 */
				*p=0;

			/* If MAIL FROM address is in dnsbl_exempt.cfg, clear DNSBL results */
			if(dnsbl_result.s_addr && email_addr_is_exempt(reverse_path)) {
				lprintf(LOG_INFO,"%04d SMTP Ignoring DNSBL results for exempt sender: %s"
					,socket,reverse_path);
				dnsbl_result.s_addr=0;
			}

			/* Update client display */
			if(relay_user.number==0) {
				client.user=reverse_path;
				client_on(socket,&client,TRUE /* update */);
			}

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
			if((rcptlst=freopen(rcptlst_fname,"w+",rcptlst))==NULL) {
				lprintf(LOG_ERR,"%04d !SMTP ERROR %d re-opening %s"
					,socket, errno, rcptlst_fname);
				sockprintf(socket,session,sys_error);
				break;
			}
			rcpt_count=0;
			content_encoding=ENCODING_NONE;
			memset(mailproc_to_match,FALSE,sizeof(BOOL)*mailproc_count);
			sockprintf(socket,session,ok_rsp);
			badcmds=0;
			continue;
		}

#if 0	/* No one uses this command */
		if(!strnicmp(buf,"VRFY",4)) {
			p=buf+4;
			SKIP_WHITESPACE(p);
			if(*p==0) {
				sockprintf(socket,session,"550 No user specified.");
				continue;
			}
#endif

		/* Add to Recipient list */
		if(!strnicmp(buf,"RCPT TO:",8)) {

			if(state<SMTP_STATE_MAIL_FROM) {
				lprintf(LOG_WARNING,"%04d !SMTP MISSING 'MAIL' command",socket);
				sockprintf(socket,session, badseq_rsp);
				continue;
			}

			p=buf+8;
			SKIP_WHITESPACE(p);
			SAFECOPY(rcpt_to,p);
			SAFECOPY(str,p);
			p=strrchr(str,'<');
			if(p==NULL)
				p=str;
			else
				p++;

			truncstr(str,">");	/* was truncating at space too */

			routed=FALSE;
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

			if(*p==0) {
				lprintf(LOG_NOTICE,"%04d !SMTP NO RECIPIENT SPECIFIED"
					,socket);
				sockprintf(socket,session, "500 No recipient specified");
				continue;
			}

			rcpt_name[0]=0;
			SAFECOPY(rcpt_addr,p);

			/* Check recipient counter */
			if(startup->max_recipients) {
				if(rcpt_count>=startup->max_recipients) {
					lprintf(LOG_NOTICE,"%04d !SMTP MAXIMUM RECIPIENTS (%d) REACHED"
						,socket, startup->max_recipients);
					SAFEPRINTF(tmp,"Maximum recipient count (%d)",startup->max_recipients);
					spamlog(&scfg, "SMTP", "REFUSED", tmp
						,host_name, host_ip, rcpt_addr, reverse_path);
					sockprintf(socket,session, "452 Too many recipients");
					stats.msgs_refused++;
					continue;
				}
				if(relay_user.number!=0 && !(relay_user.exempt&FLAG('M'))
					&& rcpt_count+(waiting=getmail(&scfg,relay_user.number,/* sent: */TRUE, /* SPAM: */FALSE)) > startup->max_recipients) {
					lprintf(LOG_NOTICE,"%04d !SMTP MAXIMUM PENDING SENT EMAILS (%u) REACHED for User #%u (%s)"
						,socket, waiting, relay_user.number, relay_user.alias);
					sockprintf(socket,session, "452 Too many pending emails sent");
					stats.msgs_refused++;
					continue;
				}
			}

			if(relay_user.number && (relay_user.etoday+rcpt_count) >= scfg.level_emailperday[relay_user.level]
				&& !(relay_user.exempt&FLAG('M'))) {
				lprintf(LOG_NOTICE,"%04d !SMTP EMAILS PER DAY LIMIT (%u) REACHED FOR USER #%u (%s)"
					,socket, scfg.level_emailperday[relay_user.level], relay_user.number, relay_user.alias);
				SAFEPRINTF2(tmp,"Maximum emails per day (%u) for %s"
					,scfg.level_emailperday[relay_user.level], relay_user.alias);
				spamlog(&scfg, "SMTP", "REFUSED", tmp
					,host_name, host_ip, rcpt_addr, reverse_path);
				sockprintf(socket,session, "452 Too many emails today");
				stats.msgs_refused++;
				continue;
			}
				
			/* Check for SPAM bait recipient */
			if((spam_bait_result=findstr(rcpt_addr,spam_bait))==TRUE) {
				char	reason[256];
				SAFEPRINTF(reason,"SPAM BAIT (%s) taken", rcpt_addr);
				lprintf(LOG_NOTICE,"%04d SMTP %s by: %s"
					,socket, reason, reverse_path);
				if(relay_user.number==0) {
					strcpy(tmp,"IGNORED");
					if(dnsbl_result.s_addr==0						/* Don't double-filter */
						&& !spam_block_exempt)	{ 
						lprintf(LOG_NOTICE,"%04d !BLOCKING IP ADDRESS: %s in %s", socket, host_ip, spam_block);
						filter_ip(&scfg, "SMTP", reason, host_name, host_ip, reverse_path, spam_block);
						strcat(tmp," and BLOCKED");
					}
					spamlog(&scfg, "SMTP", tmp, "Attempted recipient in SPAM BAIT list"
						,host_name, host_ip, rcpt_addr, reverse_path);
					dnsbl_result.s_addr=0;
				}
				sockprintf(socket,session,ok_rsp);
				state=SMTP_STATE_RCPT_TO;
				continue;
			}

			/* Check for blocked recipients */
			if(relay_user.number==0
				&& !chk_email_addr(socket,rcpt_addr,host_name,host_ip,rcpt_addr,reverse_path,"RECIPIENT")) {
				sockprintf(socket,session, "550 Unknown User: %s", rcpt_to);
				stats.msgs_refused++;
				continue;
			}

			if(relay_user.number==0 && dnsbl_result.s_addr && startup->options&MAIL_OPT_DNSBL_BADUSER) {
				lprintf(LOG_NOTICE,"%04d !SMTP REFUSED MAIL from blacklisted server (%u total)"
					,socket, ++stats.sessions_refused);
				SAFEPRINTF2(str,"Listed on %s as %s", dnsbl, inet_ntoa(dnsbl_result));
				spamlog(&scfg, "SMTP", "REFUSED", str, host_name, host_ip, rcpt_addr, reverse_path);
				sockprintf(socket,session
					,"550 Mail from %s refused due to listing at %s"
					,host_ip, dnsbl);
				break;
			}

			if(spy==NULL 
				&& (trashcan(&scfg,reverse_path,"smtpspy")
					|| trashcan(&scfg,rcpt_addr,"smtpspy"))) {
				SAFEPRINTF(path,"%ssmtpspy.txt", scfg.logs_dir);
				spy=fopen(path,"a");
			}

			/* Check for full address aliases */
			p=alias(&scfg,p,alias_buf);
			if(p==alias_buf) 
				lprintf(LOG_DEBUG,"%04d SMTP ADDRESS ALIAS: %s (for %s)"
					,socket,p,rcpt_addr);

			tp=strrchr(p,'@');
			if(cmd==SMTP_CMD_MAIL && tp!=NULL) {
				/* RELAY */
				dest_port=inet_addrport(&server_addr);
				SAFECOPY(dest_host,tp+1);
				if(relay_user.number && scfg.total_faddrs) {
					char* ftn_tld = strstr(dest_host, FIDO_TLD);
					if(ftn_tld != NULL && ftn_tld[strlen(FIDO_TLD)] == 0) {
						fidoaddr_t faddr = scfg.faddr[0];
						faddr.point = 0;
						if((sscanf(dest_host,"p%hu.f%hu.n%hu.z%hu.fidonet"
							,&faddr.point
							,&faddr.node
							,&faddr.net
							,&faddr.zone)==4
							||
							sscanf(dest_host,"f%hu.n%hu.z%hu.fidonet"
							,&faddr.node
							,&faddr.net
							,&faddr.zone)==3
							) && faddr.zone) {

							lprintf(LOG_INFO,"%04d SMTP %s relaying to FidoNet address: %s (%s)"
								,socket, relay_user.alias, tp+1, smb_faddrtoa(&faddr, NULL));

							fprintf(rcptlst,"[%u]\n",rcpt_count++);
							fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENT),rcpt_addr);
							fprintf(rcptlst,"%s=%u\n",smb_hfieldtype(RECIPIENTNETTYPE),NET_FIDO);
							fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENTNETADDR),smb_faddrtoa(&faddr,NULL));

							sockprintf(socket,session,ok_rsp);
							state=SMTP_STATE_RCPT_TO;
							continue;
						}
					}
				}
				cp=strrchr(dest_host,':');
				if(cp!=NULL) {
					*cp=0;
					dest_port=atoi(cp+1);
				}
				SAFEPRINTF(domain_list,"%sdomains.cfg",scfg.ctrl_dir);
				if((stricmp(dest_host,scfg.sys_inetaddr)!=0
						&& stricmp(dest_host,startup->host_name)!=0
						&& findstr(dest_host,domain_list)==FALSE)
					|| dest_port!=inet_addrport(&server_addr)) {

					SAFEPRINTF(relay_list,"%srelay.cfg",scfg.ctrl_dir);
					if(relay_user.number==0 /* not authenticated, search for IP */
						&& startup->options&MAIL_OPT_SMTP_AUTH_VIA_IP) { 
						relay_user.number=userdatdupe(&scfg, 0, U_IPADDR, LEN_IPADDR, host_ip, /* del */FALSE, /* next */FALSE, NULL, NULL);
						if(relay_user.number) {
							getuserdat(&scfg,&relay_user);
							if(relay_user.laston < time(NULL)-(60*60))	/* logon in past hour? */
								relay_user.number=0;
						}
					} else
						getuserdat(&scfg,&relay_user);
					if(p!=alias_buf /* forced relay by alias */ &&
						(!(startup->options&MAIL_OPT_ALLOW_RELAY)
							|| relay_user.number==0
							|| relay_user.rest&(FLAG('G')|FLAG('M'))) &&
						!findstr(host_name,relay_list) && 
						!findstr(host_ip,relay_list)) {
						lprintf(LOG_WARNING,"%04d !SMTP ILLEGAL RELAY ATTEMPT from %s [%s] to %s"
							,socket, reverse_path, host_ip, p);
						SAFEPRINTF(tmp,"Relay attempt to: %s", p);
						spamlog(&scfg, "SMTP", "REFUSED", tmp, host_name, host_ip, rcpt_addr, reverse_path);
						if(startup->options&MAIL_OPT_ALLOW_RELAY)
							sockprintf(socket,session, "553 Relaying through this server "
							"requires authentication.  "
							"Please authenticate before sending.");
						else {
							sockprintf(socket,session, "550 Relay not allowed.");
							stats.msgs_refused++;
						}
						break;
					}

					if(relay_user.number==0)
						SAFECOPY(relay_user.alias,"Unknown User");

					lprintf(LOG_INFO,"%04d SMTP %s relaying to external mail service: %s"
						,socket, relay_user.alias, tp+1);

					fprintf(rcptlst,"[%u]\n",rcpt_count++);
					fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENT),rcpt_addr);
					fprintf(rcptlst,"%s=%u\n",smb_hfieldtype(RECIPIENTNETTYPE),NET_INTERNET);
					fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENTNETADDR),p);

					sockprintf(socket,session,ok_rsp);
					state=SMTP_STATE_RCPT_TO;
					continue;
				}
			}
			if(tp!=NULL)
				*tp=0;	/* truncate at '@' */

			tp=strchr(p,'!');	/* Routed QWKnet mail in <qwkid!user@host> format */
			if(tp!=NULL) {
				*(tp++)=0;
				SKIP_CHAR(tp,'"');				/* Skip '"' */
				truncstr(tp,"\"");				/* Strip '"' */
				SAFECOPY(rcpt_addr,tp);
				routed=TRUE;
			}

			FIND_ALPHANUMERIC(p);				/* Skip '<' or '"' */
			truncstr(p,"\"");

			p=alias(&scfg,p,name_alias_buf);
			if(p==name_alias_buf) 
				lprintf(LOG_DEBUG,"%04d SMTP NAME ALIAS: %s (for %s)"
					,socket,p,rcpt_addr);
		
			/* Check if message is to be processed by an external mail processor */
			for(i=0;i<mailproc_count;i++) {

				if(!mailproc_list[i].process_dnsbl && dnsbl_result.s_addr)
					continue;

				if(!mailproc_list[i].process_spam && spam_bait_result)
					continue;

				if(!chk_ar(&scfg,mailproc_list[i].ar,&relay_user,&client))
					continue;

				if(findstr_in_list(p, mailproc_list[i].to) || findstr_in_list(rcpt_addr, mailproc_list[i].to)) {
					mailproc_to_match[i]=TRUE;
					break;
				}
			}
			mailproc_match=i;

			if(!strnicmp(p,"sub:",4)) {		/* Post on a sub-board */
				p+=4;
				for(i=0;i<scfg.total_subs;i++)
					if(!stricmp(p,scfg.sub[i]->code))
						break;
				if(i>=scfg.total_subs) {
					lprintf(LOG_NOTICE,"%04d !SMTP UNKNOWN SUB-BOARD: %s", socket, p);
					sockprintf(socket,session, "550 Unknown sub-board: %s", p);
					continue;
				}
				subnum=i;
				sockprintf(socket,session,ok_rsp);
				state=SMTP_STATE_RCPT_TO;
				rcpt_count++;
				continue;
			}

			/* destined for a (non-passthru) external mail processor */
			if(mailproc_match<mailproc_count) {
				fprintf(rcptlst,"[%u]\n",rcpt_count++);
				fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENT),rcpt_addr);
#if 0	/* should we fall-through to the sysop account? */
				fprintf(rcptlst,"%s=%u\n",smb_hfieldtype(RECIPIENTEXT),1);
#endif
				lprintf(LOG_INFO,"%04d SMTP Routing mail for %s to External Mail Processor: %s"
					,socket, rcpt_addr, mailproc_list[mailproc_match].name);
				sockprintf(socket,session,ok_rsp);
				state=SMTP_STATE_RCPT_TO;
				continue;
			}

			usernum=0;	/* unknown user at this point */

			if(routed) {
				SAFECOPY(qwkid,p);
				truncstr(qwkid,"/");
				/* Search QWKnet hub-IDs for route destination */
				for(i=0;i<scfg.total_qhubs;i++) {
					if(!stricmp(qwkid,scfg.qhub[i]->id))
						break;
				}
				if(i<scfg.total_qhubs) {	/* found matching QWKnet Hub */

					lprintf(LOG_INFO,"%04d SMTP Routing mail for %s <%s> to QWKnet Hub: %s"
						,socket, rcpt_addr, p, scfg.qhub[i]->id);

					fprintf(rcptlst,"[%u]\n",rcpt_count++);
					fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENT),rcpt_addr);
					fprintf(rcptlst,"%s=%u\n",smb_hfieldtype(RECIPIENTNETTYPE),NET_QWK);
					fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENTNETADDR),p);

					sockprintf(socket,session,ok_rsp);
					state=SMTP_STATE_RCPT_TO;
					continue;
				}
			}

			if((p==alias_buf || p==name_alias_buf || startup->options&MAIL_OPT_ALLOW_RX_BY_NUMBER)
				&& isdigit((uchar)*p)) {
				usernum=atoi(p);			/* RX by user number */
				/* verify usernum */
				username(&scfg,usernum,str);
				if(!str[0] || !stricmp(str,"DELETED USER"))
					usernum=0;
				p=str;
			} else {
				/* RX by "user alias", "user.alias" or "user_alias" */
				usernum=smtp_matchuser(&scfg,p,startup->options&MAIL_OPT_ALLOW_SYSOP_ALIASES,FALSE);	

				if(!usernum) { /* RX by "real name", "real.name", or "sysop.alias" */
					
					/* convert "user.name" to "user name" */
					SAFECOPY(rcpt_name,p);
					for(tp=rcpt_name;*tp;tp++)	
						if(*tp=='.') *tp=' ';

					if(!stricmp(p,scfg.sys_op) || !stricmp(rcpt_name,scfg.sys_op))
						usernum=1;			/* RX by "sysop.alias" */

					if(!usernum && scfg.msg_misc&MM_REALNAME)	/* RX by "real name" */
						usernum=smtp_matchuser(&scfg, p, FALSE, TRUE);	

					if(!usernum && scfg.msg_misc&MM_REALNAME)	/* RX by "real.name" */
						usernum=smtp_matchuser(&scfg, rcpt_name, FALSE, TRUE);	
				}
			}
			if(!usernum && startup->default_user[0]) {
				usernum=matchuser(&scfg,startup->default_user,TRUE /* sysop_alias */);
				if(usernum)
					lprintf(LOG_INFO,"%04d SMTP Forwarding mail for UNKNOWN USER to default user: '%s' #%u"
						,socket,startup->default_user,usernum);
				else
					lprintf(LOG_WARNING,"%04d !SMTP UNKNOWN DEFAULT USER: '%s'"
						,socket,startup->default_user);
			}

			if(usernum==UINT_MAX) {
				lprintf(LOG_INFO,"%04d SMTP Blocked tag: %s", socket, rcpt_to);
				sockprintf(socket,session, "550 Unknown User: %s", rcpt_to);
				continue;
			}
			if(!usernum) {
				lprintf(LOG_WARNING,"%04d !SMTP UNKNOWN USER: '%s'", socket, rcpt_to);
				sockprintf(socket,session, "550 Unknown User: %s", rcpt_to);
				continue;
			}
			user.number=usernum;
			if((i=getuserdat(&scfg, &user))!=0) {
				lprintf(LOG_ERR,"%04d !SMTP ERROR %d getting data on user #%u (%s)"
					,socket, i, usernum, p);
				sockprintf(socket,session, "550 Unknown User: %s", rcpt_to);
				continue;
			}
			if(user.misc&(DELETED|INACTIVE)) {
				lprintf(LOG_WARNING,"%04d !SMTP DELETED or INACTIVE user #%u (%s)"
					,socket, usernum, p);
				sockprintf(socket,session, "550 Unknown User: %s", rcpt_to);
				continue;
			}
			if(cmd==SMTP_CMD_MAIL) {
				if((user.rest&FLAG('M')) && relay_user.number==0) {
					lprintf(LOG_NOTICE,"%04d !SMTP M-restricted user #u (%s) cannot receive unauthenticated SMTP mail"
						,socket, user.number, user.alias);
					sockprintf(socket,session, "550 Closed mailbox: %s", rcpt_to);
					stats.msgs_refused++;
					continue;
				}
				if(startup->max_msgs_waiting && !(user.exempt&FLAG('W')) 
					&& (waiting=getmail(&scfg, user.number, /* sent: */FALSE, /* spam: */FALSE)) > startup->max_msgs_waiting) {
					lprintf(LOG_NOTICE,"%04d !SMTP User #%u (%s) mailbox (%u msgs) exceeds the maximum (%u) msgs waiting"
						,socket, user.number, user.alias, waiting, startup->max_msgs_waiting);
					sockprintf(socket,session, "450 Mailbox full: %s", rcpt_to);
					stats.msgs_refused++;
					continue;
				}
			}
			else if(cmd==SMTP_CMD_SEND) { /* Check if user online */
				for(i=0;i<scfg.sys_nodes;i++) {
					getnodedat(&scfg, i+1, &node, 0);
					if(node.status==NODE_INUSE && node.useron==user.number
						&& !(node.misc&NODE_POFF))
						break;
				}
				if(i>=scfg.sys_nodes) {
					lprintf(LOG_WARNING,"%04d !Attempt to send telegram to unavailable user #%u (%s)"
						,socket, user.number, user.alias);
					sockprintf(socket,session,"450 User unavailable");
					continue;
				}
			}
			if(cmd!=SMTP_CMD_MAIL)
				telegram=TRUE;

			fprintf(rcptlst,"[%u]\n",rcpt_count++);
			fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENT),rcpt_addr);
			fprintf(rcptlst,"%s=%u\n",smb_hfieldtype(RECIPIENTEXT),user.number);

			/* Forward to Internet */
			tp=strrchr(user.netmail,'@');
			if(!telegram
				&& !routed
				&& !no_forward
				&& scfg.sys_misc&SM_FWDTONET 
				&& (user.misc&NETMAIL || forward)
				&& tp!=NULL && smb_netaddr_type(user.netmail)==NET_INTERNET 
				&& !strstr(tp,scfg.sys_inetaddr)) {
				lprintf(LOG_INFO,"%04d SMTP Forwarding to: %s"
					,socket, user.netmail);
				fprintf(rcptlst,"%s=%u\n",smb_hfieldtype(RECIPIENTNETTYPE),NET_INTERNET);
				fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENTNETADDR),user.netmail);
				sockprintf(socket,session,"251 User not local; will forward to %s", user.netmail);
			} else { /* Local (no-forward) */
				if(routed) { /* QWKnet */
					fprintf(rcptlst,"%s=%u\n",smb_hfieldtype(RECIPIENTNETTYPE),NET_QWK);
					fprintf(rcptlst,"%s=%s\n",smb_hfieldtype(RECIPIENTNETADDR),user.alias);
				}						
				sockprintf(socket,session,ok_rsp);
			}
			state=SMTP_STATE_RCPT_TO;
			continue;
		}
		/* Message Data (header and body) */
		if(!strnicmp(buf,"DATA",4)) {
			if(state<SMTP_STATE_RCPT_TO) {
				lprintf(LOG_WARNING,"%04d !SMTP MISSING 'RCPT TO' command", socket);
				sockprintf(socket,session, badseq_rsp);
				continue;
			}
			if(msgtxt!=NULL) {
				fclose(msgtxt), msgtxt=NULL;
			}
			remove(msgtxt_fname);
			if((msgtxt=fopen(msgtxt_fname,"w+b"))==NULL) {
				lprintf(LOG_ERR,"%04d !SMTP ERROR %d opening %s"
					,socket, errno, msgtxt_fname);
				sockprintf(socket,session, insuf_stor);
				continue;
			}
			/* These vars are potentially over-written by parsing an RFC822 header */
			/* get sender_addr */
			p=strrchr(reverse_path,'<');
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

			sockprintf(socket,session, "354 send the mail data, end with <CRLF>.<CRLF>");
			if(telegram)
				state=SMTP_STATE_DATA_BODY;	/* No RFC headers in Telegrams */
			else
				state=SMTP_STATE_DATA_HEADER;
			lprintf(LOG_INFO,"%04d SMTP Receiving %s message from: %s to %s"
				,socket, telegram ? "telegram":"mail", reverse_path, rcpt_addr);
			hdr_lines=0;
			continue;
		}
		if(session == -1 && !stricmp(buf,"STARTTLS")) {
			if (get_ssl_cert(&scfg, NULL) == -1) {
				lprintf(LOG_ERR, "%04d !SMTP Unable to get certificate", socket);
				sockprintf(socket, session, "454 TLS not available");
				continue;
			}
			if (cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_SSL_SERVER) != CRYPT_OK) {
				lprintf(LOG_ERR, "%04d !SMTP Unable to create TLS session", socket);
				sockprintf(socket, session, "454 TLS not available");
				continue;
			}
			if (cryptSetAttribute(session, CRYPT_SESSINFO_SSL_OPTIONS, CRYPT_SSLOPTION_DISABLE_CERTVERIFY) != CRYPT_OK) {
				lprintf(LOG_ERR, "%04d !SMTP Unable to disable certificate verification", socket);
				cryptDestroySession(session);
				session = -1;
				sockprintf(socket, session, "454 TLS not available");
				continue;
			}
			if (cryptSetAttribute(session, CRYPT_SESSINFO_PRIVATEKEY, scfg.tls_certificate) != CRYPT_OK) {
				lprintf(LOG_ERR, "%04d !SMTP Unable to set private key", socket);
				cryptDestroySession(session);
				session = -1;
				sockprintf(socket, session, "454 TLS not available");
				continue;
			}
			nodelay = TRUE;
			setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));
			nb=0;
			ioctlsocket(socket,FIONBIO,&nb);
			if ((rd = cryptSetAttribute(session, CRYPT_SESSINFO_NETWORKSOCKET, socket)) != CRYPT_OK) {
				lprintf(LOG_ERR, "%04d !SMTP Unable to set network socket", socket);
				cryptDestroySession(session);
				session = -1;
				sockprintf(socket, session, "454 TLS not available");
				continue;
			}
			sockprintf(socket, -1, "220 Ready to start TLS");
			if (cryptSetAttribute(session, CRYPT_SESSINFO_ACTIVE, 1) != CRYPT_OK) {
				lprintf(LOG_ERR, "%04d !SMTP Unable to set session active", socket);
				cryptDestroySession(session);
				mail_close_socket(socket);
				thread_down();
				protected_uint32_adjust(&active_clients, -1);
				update_clients();
				free(mailproc_to_match);
				break;
			}
			if (startup->max_inactivity) {
				if (cryptSetAttribute(session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity) != CRYPT_OK) {
					lprintf(LOG_ERR, "%04d !SMTP Unable to set max inactivity", socket);
					cryptDestroySession(session);
					mail_close_socket(socket);
					thread_down();
					protected_uint32_adjust(&active_clients, -1);
					update_clients();
					free(mailproc_to_match);
					break;
				}
			}
			continue;
		}
		sockprintf(socket,session,"500 Syntax error");
		lprintf(LOG_WARNING,"%04d !SMTP UNSUPPORTED COMMAND: '%s'", socket, buf);
		if(++badcmds>9) {
			lprintf(LOG_WARNING,"%04d !TOO MANY INVALID COMMANDS (%u)",socket,badcmds);
			break;
		}
	}

	/* Free up resources here */
	smb_freemsgmem(&msg);

	if(msgtxt!=NULL)
		fclose(msgtxt);
	if(!(startup->options&MAIL_OPT_DEBUG_RX_BODY))
		remove(msgtxt_fname);
	if(rcptlst!=NULL)
		fclose(rcptlst);
	remove(rcptlst_fname);
	if(spy!=NULL)
		fclose(spy);
	js_cleanup(js_runtime, js_cx, &js_glob);

	status(STATUS_WFC);

	protected_uint32_adjust(&active_clients, -1);
	update_clients();
	client_off(socket);

	{
		int32_t remain = thread_down();
		lprintf(LOG_INFO,"%04d SMTP Session thread terminated (%u threads remain, %lu clients served)"
			,socket, remain, ++stats.smtp_served);
	}
	free(mailproc_to_match);

	if (session != -1)
		cryptDestroySession(session);
	/* Must be last */
	mail_close_socket(socket);
}

BOOL bounce(SOCKET sock, smb_t* smb, smbmsg_t* msg, char* err, BOOL immediate)
{
	char		str[128];
	char		attempts[64];
	char		msgid[256];
	int			i;
	ushort		agent=AGENT_SMTPSYSMSG;
	smbmsg_t	newmsg;

	msg->hdr.delivery_attempts++;
	lprintf(LOG_WARNING,"%04d !SEND Delivery attempt #%u FAILED (%s) for message #%lu from %s to %s"
		,sock
		,msg->hdr.delivery_attempts
		,err
		,msg->hdr.number
		,msg->from
		,msg->to_net.addr);

	if((i=smb_updatemsg(smb,msg))!=SMB_SUCCESS) {
		lprintf(LOG_ERR,"%04d !SEND BOUNCE ERROR %d (%s) incrementing delivery attempt counter"
			,sock, i, smb->last_error);
		return(FALSE);
	}

	if(!immediate && msg->hdr.delivery_attempts < startup->max_delivery_attempts)
		return(TRUE);

	newmsg=*msg;
	/* Mark original message as deleted */
	msg->hdr.attr|=MSG_DELETE;

	i=smb_updatemsg(smb,msg);
	if(msg->hdr.auxattr&MSG_FILEATTACH)
		delfattach(&scfg,msg);
	if(i!=SMB_SUCCESS) {
		lprintf(LOG_ERR,"%04d !SEND BOUNCE ERROR %d (%s) deleting message"
			,sock, i, smb->last_error);
		return(FALSE);
	}

	if(msg->from_agent==AGENT_SMTPSYSMSG	/* don't bounce 'bounce messages' */
		|| (msg->hdr.attr&MSG_NOREPLY)
		|| (msg->idx.from==0 && msg->from_net.type==NET_NONE)
		|| (msg->reverse_path!=NULL && *msg->reverse_path==0)) {
		lprintf(LOG_WARNING,"%04d !SEND Deleted undeliverable message from %s", sock, msg->from);
		return(TRUE);
	}
	
	newmsg.hfield=NULL;
	newmsg.hfield_dat=NULL;
	newmsg.total_hfields=0;
	newmsg.hdr.delivery_attempts=0;
	char* reverse_path = msg->reverse_path==NULL ? msg->from : msg->reverse_path;

	lprintf(LOG_WARNING,"%04d !SEND Bouncing message back to %s", sock, reverse_path);

	SAFEPRINTF(str,"Delivery failure: %s",newmsg.subj);
	smb_hfield_str(&newmsg, SUBJECT, str);
	smb_hfield_str(&newmsg, RECIPIENT, reverse_path);
	if(msg->from_agent==AGENT_PERSON) {

		if(newmsg.from_ext!=NULL) { /* Back to sender */
			smb_hfield_str(&newmsg, RECIPIENTEXT, newmsg.from_ext);
			newmsg.from_ext=NULL;	/* Clear the sender extension */
		}

		if((newmsg.from_net.type==NET_QWK || newmsg.from_net.type==NET_INTERNET)
			&& newmsg.reverse_path!=NULL) {
			smb_hfield(&newmsg, RECIPIENTNETTYPE, sizeof(newmsg.from_net.type), &newmsg.from_net.type);
			smb_hfield_str(&newmsg, RECIPIENTNETADDR, newmsg.reverse_path);
		}
	} else {
		smb_hfield(&newmsg, RECIPIENTAGENT, sizeof(msg->from_agent), &msg->from_agent);
	}
	newmsg.hdr.attr|=MSG_NOREPLY;
	newmsg.hdr.attr&=~MSG_READ;
	if(scfg.sys_misc&SM_DELREADM)
		newmsg.hdr.attr |= MSG_KILLREAD;

	strcpy(str,"Mail Delivery Subsystem");
	smb_hfield_str(&newmsg, SENDER, str);
	smb_hfield(&newmsg, SENDERAGENT, sizeof(agent), &agent);
	smb_hfield_str(&newmsg, RFC822MSGID, get_msgid(&scfg, INVALID_SUB, &newmsg, msgid, sizeof(msgid)));
	
	/* Put error message in subject for now */
	if(msg->hdr.delivery_attempts>1)
		SAFEPRINTF(attempts,"after %u attempts", msg->hdr.delivery_attempts);
	else
		attempts[0]=0;
	SAFEPRINTF2(str,"%s reporting delivery failure of message %s"
		,startup->host_name, attempts);
	smb_hfield_str(&newmsg, SMB_COMMENT, str);
	SAFEPRINTF2(str,"from %s to %s\r\n"
		,msg->from
		,(char*)msg->to_net.addr);
	smb_hfield_str(&newmsg, SMB_COMMENT, str);
	strcpy(str,"Reason:");
	smb_hfield_str(&newmsg, SMB_COMMENT, str);
	smb_hfield_str(&newmsg, SMB_COMMENT, err);
	smb_hfield_str(&newmsg, SMB_COMMENT, "\r\nOriginal message text follows:");

	if((i=smb_addmsghdr(smb,&newmsg,smb_storage_mode(&scfg, smb)))!=SMB_SUCCESS)
		lprintf(LOG_ERR,"%04d !BOUNCE ERROR %d (%s) adding message header"
			,sock,i,smb->last_error);
	else {
		lprintf(LOG_WARNING,"%04d !SEND Delivery failure notification (message #%ld) created for %s"
			,sock, newmsg.hdr.number, reverse_path);
		if((i=smb_incmsg_dfields(smb,&newmsg,1))!=SMB_SUCCESS)
			lprintf(LOG_ERR,"%04d !SEND BOUNCE ERROR %d (%s) incrementing data allocation units"
				,sock, i,smb->last_error);
	}

	newmsg.dfield=NULL;				/* Don't double-free the data fields */
	newmsg.hdr.total_dfields=0;
	smb_freemsgmem(&newmsg);

	return(TRUE);
}

static int remove_msg_intransit(smb_t* smb, smbmsg_t* msg)
{
	int i;

	if((i=smb_lockmsghdr(smb,msg))!=SMB_SUCCESS) {
		lprintf(LOG_WARNING,"0000 !SEND ERROR %d (%s) locking message header #%lu"
			,i, smb->last_error, msg->idx.number);
		return(i);
	}
	msg->hdr.netattr&=~MSG_INTRANSIT;
	i=smb_putmsghdr(smb,msg);
	smb_unlockmsghdr(smb,msg);
	
	if(i!=0)
		lprintf(LOG_ERR,"0000 !SEND ERROR %d (%s) writing message header #%lu"
			,i, smb->last_error, msg->idx.number);

	return(i);
}

void get_dns_server(char* dns_server, size_t len)
{
	str_list_t	list;
	size_t		count;

	sprintf(dns_server,"%.*s",(int)len,startup->dns_server);
	if(!isalnum(dns_server[0])) {
		if((list=getNameServerList())!=NULL) {
			if((count=strListCount(list))>0) {
				sprintf(dns_server,"%.*s",(int)len,list[xp_random(count)]);
				lprintf(LOG_DEBUG,"0000 SEND using auto-detected DNS server address: %s"
					,dns_server);
			}
			freeNameServerList(list);
		}
	}
}

static BOOL sendmail_open_socket(SOCKET *sock, smb_t *smb, 	smbmsg_t *msg)
{
	int i;
	SOCKADDR_IN	addr;

	if (*sock != INVALID_SOCKET)
		mail_close_socket(*sock);

	if((*sock=socket(AF_INET, SOCK_STREAM, IPPROTO_IP))==INVALID_SOCKET) {
		remove_msg_intransit(smb,msg);
		lprintf(LOG_ERR,"0000 !SEND ERROR %d opening socket", ERROR_VALUE);
		return FALSE;
	}
	mail_open_socket(*sock,"smtp|sendmail");

	if(startup->connect_timeout) {	/* Use non-blocking socket */
		long nbio=1;
		if((i=ioctlsocket(*sock, FIONBIO, &nbio))!=0) {
			remove_msg_intransit(smb,msg);
			lprintf(LOG_ERR,"%04d !SEND ERROR %d (%d) disabling blocking on socket"
				,*sock, i, ERROR_VALUE);
			return FALSE;
		}
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = htonl(startup->outgoing4.s_addr);
	addr.sin_family = AF_INET;

	i=bind(*sock,(struct sockaddr *)&addr, sizeof(addr));
	if(i!=0) {
		remove_msg_intransit(smb,msg);
		lprintf(LOG_ERR,"%04d !SEND ERROR %d (%d) binding socket", *sock, i, ERROR_VALUE);
		return FALSE;
	}
	return TRUE;
}

static SOCKET sendmail_negotiate(CRYPT_SESSION *session, smb_t *smb, smbmsg_t *msg, const char *mx, const char *mx2, const char *server, link_list_t *failed_server_list, ushort port)
{
	int i;
	int tls_retry;
	SOCKET sock;
	list_node_t*	node;
	ulong		ip_addr;
	union xp_sockaddr	server_addr;
	char		server_ip[INET6_ADDRSTRLEN];
	BOOL		success;
	BOOL nodelay=TRUE;
	ulong nb = 0;
	int status;
	char *estr;
	char		buf[512];
	char		err[1024];

	strcpy(err,"UNKNOWN ERROR");

	for (tls_retry = 0; tls_retry < 2; tls_retry++) {
		if (*session != -1) {
			cryptDestroySession(*session);
			*session = -1;
		}
		if (!sendmail_open_socket(&sock, smb, msg))
			continue;

		success=FALSE;
		for(i=0;i<2 && !success;i++) {
			if(i) {
				if(startup->options&MAIL_OPT_RELAY_TX || !mx2[0])
					break;
				lprintf(LOG_DEBUG,"%04d SEND reverting to second MX: %s", sock, mx2);
				server=mx2;	/* Give second mx record a try */
			}

			lprintf(LOG_DEBUG,"%04d SEND resolving SMTP hostname: %s", sock, server);
			ip_addr=resolve_ip(server);
			if(ip_addr==INADDR_NONE) {
				SAFEPRINTF(err, "Error resolving hostname %s", server);
				lprintf(LOG_WARNING,"%04d !SEND failure resolving hostname: %s", sock, server);
				continue;
			}

			memset(&server_addr,0,sizeof(server_addr));
			server_addr.in.sin_addr.s_addr = ip_addr;
			server_addr.in.sin_family = AF_INET;
			server_addr.in.sin_port = htons(port);
			inet_addrtop(&server_addr,server_ip,sizeof(server_ip));

			if((node=listFindNode(failed_server_list,&server_addr,sizeof(server_addr))) != NULL) {
				SAFEPRINTF4(err, "Error %ld connecting to port %u on %s [%s]", node->tag, inet_addrport(&server_addr), server, server_ip);
				lprintf(LOG_INFO,"%04d SEND skipping failed SMTP server: %s", sock, err);
				continue;
			}

			if((server==mx || server==mx2) 
				&& ((ip_addr&0xff)==127 || ip_addr==0)) {
				SAFEPRINTF2(err,"Bad IP address (%s) for MX server: %s"
					,server_ip,server);
				lprintf(LOG_INFO, "%04d SEND %s", err);
				continue;
			}

			lprintf(LOG_INFO,"%04d SEND connecting to port %u on %s [%s]"
				,sock
				,inet_addrport(&server_addr)
				,server,server_ip);
			if((i=nonblocking_connect(sock, (struct sockaddr *)&server_addr, xp_sockaddr_len(&server_addr), startup->connect_timeout))!=0) {
				SAFEPRINTF2(err,"ERROR %d connecting to SMTP server: %s"
					,i, server);
				lprintf(LOG_INFO,"%04d !SEND %s" ,sock, err);
				listAddNodeData(failed_server_list,&server_addr,sizeof(server_addr),i,NULL);
				continue;
			}

			lprintf(LOG_DEBUG,"%04d SEND connected to %s",sock,server);

			/* HELO */
			if(!sockgetrsp(sock,*session,"220",buf,sizeof(buf))) {
				SAFEPRINTF3(err,badrsp_err,server,buf,"220");
				lprintf(LOG_INFO, "%04d SEND %s", sock, err);
				continue;
			}
			success=TRUE;
		}
		if(!success) {	/* Failed to connect to an MX, so bounce */
			remove_msg_intransit(smb,msg);
			bounce(sock /* Should be zero? */, smb,msg,err,/* immediate: */FALSE);	
			mail_close_socket(sock);
			return INVALID_SOCKET;
		}

		sockprintf(sock,*session,"EHLO %s",startup->host_name);
		switch (sockgetrsp_opt(sock,*session,"250", "STARTTLS", buf, sizeof(buf))) {
			case -1:
				if(startup->options&MAIL_OPT_RELAY_TX 
					&& (startup->options&MAIL_OPT_RELAY_AUTH_MASK)!=0) {	/* Requires ESMTP */
					SAFEPRINTF3(err,badrsp_err,server,buf,"250");
					remove_msg_intransit(smb,msg);
					bounce(sock, smb,msg,err,/* immediate: */buf[0]=='5');
					mail_close_socket(sock);
					return INVALID_SOCKET;
				}
				sockprintf(sock,*session,"HELO %s",startup->host_name);
				if(!sockgetrsp(sock,*session,"250",buf,sizeof(buf))) {
					SAFEPRINTF3(err,badrsp_err,server,buf,"250");
					remove_msg_intransit(smb,msg);
					bounce(sock, smb,msg,err,/* immediate: */buf[0]=='5');
					mail_close_socket(sock);
					return INVALID_SOCKET;
				}
				return sock;
			case 0:
				return sock;
			case 1:
				if ((!tls_retry) && get_ssl_cert(&scfg, NULL) != -1) {
					sockprintf(sock, *session, "STARTTLS");
					if (sockgetrsp(sock, *session, "220", buf, sizeof(buf))) {
						if ((status=cryptCreateSession(session, CRYPT_UNUSED, CRYPT_SESSION_SSL)) != CRYPT_OK) {
							SAFEPRINTF2(err, "ERROR %d creating TLS session to SMTP server: %s", status, server);
							lprintf(LOG_INFO,"%04d !SEND %s", sock, err);
							continue;
						}
						if ((status=cryptSetAttribute(*session, CRYPT_SESSINFO_SSL_OPTIONS, CRYPT_SSLOPTION_DISABLE_CERTVERIFY)) != CRYPT_OK) {
							SAFEPRINTF2(err, "ERROR %d disabling certificate validation with SMTP server: %s", status, server);
							lprintf(LOG_INFO,"%04d !SEND %s" ,sock, err);
							continue;
						}
						if ((status=cryptSetAttribute(*session, CRYPT_OPTION_CERT_COMPLIANCELEVEL, CRYPT_COMPLIANCELEVEL_OBLIVIOUS)) != CRYPT_OK) {
							SAFEPRINTF2(err, "ERROR %d setting oblivious certificate compliance level with SMTP server: %s", status, server);
							lprintf(LOG_INFO,"%04d !SEND %s" ,sock, err);
							continue;
						}
						if ((status=cryptSetAttribute(*session, CRYPT_SESSINFO_PRIVATEKEY, scfg.tls_certificate)) != CRYPT_OK) {
							SAFEPRINTF2(err, "ERROR %d setting private key with SMTP server: %s", status, server);
							lprintf(LOG_INFO,"%04d !SEND %s", sock, err);
							continue;
						}
						nodelay = TRUE;
						setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));
						nb=0;
						ioctlsocket(sock,FIONBIO,&nb);
						if ((status=cryptSetAttribute(*session, CRYPT_SESSINFO_NETWORKSOCKET, sock)) != CRYPT_OK) {
							SAFEPRINTF2(err, "ERROR %d setting network socket with SMTP server: %s", status, server);
							lprintf(LOG_INFO,"%04d !SEND %s", sock, err);
							continue;
						}
						if ((status=cryptSetAttribute(*session, CRYPT_SESSINFO_ACTIVE, 1)) != CRYPT_OK) {
							estr = get_crypt_error(*session);
							SAFEPRINTF3(err, "ERROR %d (%s) activating TLS session with SMTP server: %s", status, estr ? estr : "<unknown>", server);
							if (estr)
								free_crypt_attrstr(estr);
							lprintf(LOG_INFO,"%04d !SEND %s", sock, err);
							continue;
						}
						if (startup->max_inactivity) {
							if ((status=cryptSetAttribute(*session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity)) != CRYPT_OK) {
								estr = get_crypt_error(*session);
								SAFEPRINTF3(err, "ERROR %d (%s) setting max inactivity with SMTP server: %s", status, estr ? estr : "<unknown>", server);
								if (estr)
									free_crypt_attrstr(estr);
								lprintf(LOG_INFO,"%04d !SEND %s", sock, err);
								continue;
							}
						}
						sockprintf(sock,*session,"EHLO %s",startup->host_name);
						if(!sockgetrsp(sock,*session,"250",buf,sizeof(buf))) {
							SAFEPRINTF3(err,badrsp_err,server,buf,"220");
							lprintf(LOG_INFO, "%04d SEND %s", sock, err);
							continue;
						}
					}
				}
				return sock;
		}
	}
	remove_msg_intransit(smb,msg);
	bounce(sock, smb,msg,err,/* immediate: */FALSE);	
	if (*session != -1) {
		cryptDestroySession(*session);
		*session = -1;
	}
	mail_close_socket(sock);
	return INVALID_SOCKET;
}

/* TODO: IPv6 etc. */
#ifdef __BORLANDC__
#pragma argsused
#endif
static void sendmail_thread(void* arg)
{
	int			i;
	char		to[128];
	char		mx[128];
	char		mx2[128];
	char		err[1024];
	char		buf[512];
	char		str[128];
	char		resp[512];
	char		toaddr[256];
	char		fromext[128];
	char		fromaddr[256];
	char		challenge[256];
	char		secret[64];
	char		md5_data[384];
	uchar		digest[MD5_DIGEST_SIZE];
	char		numeric_ip[16];
	char		domain_list[MAX_PATH+1];
	char		dns_server[16];
	char*		server;
	char*		msgtxt=NULL;
	char*		p;
	char*		tp;
	ushort		port;
	ulong		last_msg=0;
	ulong		dns;
	ulong		lines;
	ulong		bytes;
	BOOL		first_cycle=TRUE;
	SOCKET		sock=INVALID_SOCKET;
	time_t		last_scan=0;
	smb_t		smb;
	smbmsg_t	msg;
	mail_t*		mail;
	uint32_t	msgs;
	uint32_t	u;
	size_t		len;
	BOOL		sending_locally=FALSE;
	link_list_t	failed_server_list;
	CRYPT_SESSION session = -1;

	SetThreadName("sbbs/sendMail");
	thread_up(TRUE /* setuid */);

	terminate_sendmail=FALSE;

	lprintf(LOG_INFO,"SendMail thread started");

	memset(&msg,0,sizeof(msg));
	memset(&smb,0,sizeof(smb));

	listInit(&failed_server_list, /* flags: */0);

	while((!terminated) && !terminate_sendmail) {
		YIELD();
		if(startup->options&MAIL_OPT_NO_SENDMAIL) {
			sem_trywait_block(&sendmail_wakeup_sem,1000);
			continue;
		}

		if(active_sendmail!=0)
			active_sendmail=0, update_clients();

		listFreeNodes(&failed_server_list);

		smb_close(&smb);

		if (session != -1) {
			cryptDestroySession(session);
			session = -1;
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

		/* Don't delay on first loop */
		if(first_cycle)
			first_cycle=FALSE;
		else
			sem_trywait_block(&sendmail_wakeup_sem,startup->sem_chk_freq*1000);

		SAFEPRINTF(smb.file,"%smail",scfg.data_dir);
		smb.retry_time=scfg.smb_retry_time;
		smb.subnum=INVALID_SUB;
		if((i=smb_open(&smb))!=SMB_SUCCESS) 
			continue;
		if((i=smb_locksmbhdr(&smb))!=SMB_SUCCESS)
			continue;
		i=smb_getstatus(&smb);
		smb_unlocksmbhdr(&smb);
		if(i!=0)
			continue;
		if(smb.status.last_msg==last_msg && time(NULL)-last_scan<startup->rescan_frequency)
			continue;
		lprintf(LOG_DEBUG, "0000 SEND last_msg=%u, smb.status.last_msg=%u, elapsed=%u"
			,last_msg, smb.status.last_msg, time(NULL)-last_scan);
		last_msg=smb.status.last_msg;
		last_scan=time(NULL);
		mail=loadmail(&smb,&msgs,/* to network */0,MAIL_YOUR,0);
		for(u=0; u<msgs; u++) {
			if(active_sendmail!=0)
				active_sendmail=0, update_clients();

			if(terminated || terminate_sendmail)	/* server stopped */
				break;

			if (session != -1) {
				cryptDestroySession(session);
				session = -1;
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

			msg.hdr.number=mail[u].number;
			if((i=smb_getmsgidx(&smb,&msg))!=SMB_SUCCESS) {
				lprintf(LOG_ERR,"0000 !SEND ERROR %d (%s) getting message index #%lu"
					,i, smb.last_error, mail[u].number);
				break;
			}
			if((i=smb_lockmsghdr(&smb,&msg))!=SMB_SUCCESS) {
				lprintf(LOG_WARNING,"0000 !SEND ERROR %d (%s) locking message header #%lu"
					,i, smb.last_error, msg.idx.number);
				continue;
			}
			if((i=smb_getmsghdr(&smb,&msg))!=SMB_SUCCESS) {
				smb_unlockmsghdr(&smb,&msg);
				lprintf(LOG_ERR,"0000 !SEND ERROR %d (%s) line %u, msg #%lu"
					,i, smb.last_error, __LINE__, msg.idx.number);
				continue; 
			}
			if(msg.hdr.attr&MSG_DELETE || msg.to_net.type!=NET_INTERNET || msg.to_net.addr==NULL) {
				smb_unlockmsghdr(&smb,&msg);
				continue;
			}

			if(!(startup->options&MAIL_OPT_SEND_INTRANSIT) && msg.hdr.netattr&MSG_INTRANSIT) {
				smb_unlockmsghdr(&smb,&msg);
				lprintf(LOG_NOTICE,"0000 SEND Message #%lu from %s to %s - in transit"
					,msg.hdr.number, msg.from, msg.to_net.addr);
				continue;
			}
			msg.hdr.netattr|=MSG_INTRANSIT;	/* Prevent another sendmail thread from sending this msg */
			smb_putmsghdr(&smb,&msg);
			smb_unlockmsghdr(&smb,&msg);

			active_sendmail=1, update_clients();

			fromext[0]=0;
			if(msg.from_ext)
				SAFEPRINTF(fromext," #%s", msg.from_ext);
			if(msg.from_net.type==NET_INTERNET && msg.reverse_path!=NULL)
				SAFECOPY(fromaddr,msg.reverse_path);
			else 
				usermailaddr(&scfg,fromaddr,msg.from);
			truncstr(fromaddr," ");

			lprintf(LOG_INFO,"0000 SEND Message #%lu (%u of %u) from %s%s %s to %s [%s]"
				,msg.hdr.number, u+1, msgs, msg.from, fromext, fromaddr
				,msg.to, msg.to_net.addr);
			SAFEPRINTF2(str,"Sending (%u of %u)", u+1, msgs);
			status(str);
#ifdef _WIN32
			if(startup->outbound_sound[0] && !(startup->options&MAIL_OPT_MUTE)) 
				PlaySound(startup->outbound_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

			lprintf(LOG_DEBUG,"0000 SEND getting message text");
			if((msgtxt=smb_getmsgtxt(&smb,&msg,GETMSGTXT_ALL))==NULL) {
				remove_msg_intransit(&smb,&msg);
				lprintf(LOG_ERR,"0000 !SEND ERROR (%s) retrieving message text",smb.last_error);
				continue;
			}

			remove_ctrl_a(msgtxt, msgtxt);

			port=0;
			mx2[0]=0;

			sending_locally=FALSE;
			/* Check if this is a local email ToDo */
			SAFECOPY(to,(char*)msg.to_net.addr);
			truncstr(to,"> ");

			p=strrchr(to,'@');
			if(p==NULL) {
				remove_msg_intransit(&smb,&msg);
				lprintf(LOG_WARNING,"0000 !SEND INVALID destination address: %s", to);
				SAFEPRINTF(err,"Invalid destination address: %s", to);
				bounce(0, &smb,&msg,err, /* immediate: */TRUE);
				continue;
			}
			p++;
			SAFEPRINTF(domain_list,"%sdomains.cfg",scfg.ctrl_dir);
			if(stricmp(p,scfg.sys_inetaddr)==0
					|| stricmp(p,startup->host_name)==0
					|| findstr(p,domain_list)) {
				/* This is a local message... no need to send to remote */
				port = startup->smtp_port;
				/* TODO: IPv6 */
				if(startup->outgoing4.s_addr==0)
					server="127.0.0.1";
				else {
					SAFEPRINTF4(numeric_ip, "%u.%u.%u.%u"
							, startup->outgoing4.s_addr >> 24
							, (startup->outgoing4.s_addr >> 16) & 0xff
							, (startup->outgoing4.s_addr >> 8) & 0xff
							, startup->outgoing4.s_addr & 0xff);
					server = numeric_ip;
				}
				sending_locally=TRUE;
			}
			else {
				if(startup->options&MAIL_OPT_RELAY_TX) { 
					server=startup->relay_server;
					port=startup->relay_port;
				} else {
					server=p;
					tp=strrchr(p,':');	/* non-standard SMTP port */
					if(tp!=NULL) {
						*tp=0;
						port=atoi(tp+1);
					}
					if(port==0) {	/* No port specified, use MX look-up */
						get_dns_server(dns_server,sizeof(dns_server));
						if((dns=resolve_ip(dns_server))==INADDR_NONE) {
							remove_msg_intransit(&smb,&msg);
							lprintf(LOG_WARNING,"0000 !SEND INVALID DNS server address: %s"
								,dns_server);
							continue;
						}
						lprintf(LOG_DEBUG,"0000 SEND getting MX records for %s from %s",p,dns_server);
						if((i=dns_getmx(p, mx, mx2, INADDR_ANY, dns
							,startup->options&MAIL_OPT_USE_TCP_DNS ? TRUE : FALSE
							,TIMEOUT_THREAD_WAIT/2))!=0) {
							remove_msg_intransit(&smb,&msg);
							lprintf(LOG_WARNING,"0000 !SEND ERROR %d obtaining MX records for %s from %s"
								,i,p,dns_server);
							SAFEPRINTF2(err,"Error %d obtaining MX record for %s",i,p);
							bounce(0, &smb,&msg,err, /* immediate: */FALSE);
							continue;
						}
						server=mx;
					}
				}
			}
			if(!port)
				port=IPPORT_SMTP;

			sock = sendmail_negotiate(&session, &smb, &msg, mx, mx2, server, &failed_server_list, port);
			if (sock == INVALID_SOCKET)
				continue;

			/* AUTH */
			if(startup->options&MAIL_OPT_RELAY_TX 
				&& (startup->options&MAIL_OPT_RELAY_AUTH_MASK)!=0 && !sending_locally) {

				if((startup->options&MAIL_OPT_RELAY_AUTH_MASK)==MAIL_OPT_RELAY_AUTH_PLAIN) {
					/* Build the buffer: <username>\0<user-id>\0<password */
					len=safe_snprintf(buf,sizeof(buf),"%s%c%s%c%s"
						,startup->relay_user
						,0
						,startup->relay_user
						,0
						,startup->relay_pass);
					b64_encode(resp,sizeof(resp),buf,len);
					sockprintf(sock,session,"AUTH PLAIN %s",resp);
				} else {
					switch(startup->options&MAIL_OPT_RELAY_AUTH_MASK) {
						case MAIL_OPT_RELAY_AUTH_LOGIN:
							p="LOGIN";
							break;
						case MAIL_OPT_RELAY_AUTH_CRAM_MD5:
							p="CRAM-MD5";
							break;
						default:
							p="<unknown>";
							break;
					}
					sockprintf(sock,session,"AUTH %s",p);
					if(!sockgetrsp(sock,session,"334",buf,sizeof(buf))) {
						SAFEPRINTF3(err,badrsp_err,server,buf,"334 Username/Challenge");
						bounce(sock, &smb,&msg,err,/* immediate: */buf[0]=='5');
						continue;
					}
					switch(startup->options&MAIL_OPT_RELAY_AUTH_MASK) {
						case MAIL_OPT_RELAY_AUTH_LOGIN:
							b64_encode(p=resp,sizeof(resp),startup->relay_user,0);
							break;
						case MAIL_OPT_RELAY_AUTH_CRAM_MD5:
							p=buf;
							FIND_WHITESPACE(p);
							SKIP_WHITESPACE(p);
							b64_decode(challenge,sizeof(challenge),p,0);

							/* Calculate response */
							memset(secret,0,sizeof(secret));
							SAFECOPY(secret,startup->relay_pass);
							for(i=0;i<sizeof(secret);i++)
								md5_data[i]=secret[i]^0x36;	/* ipad */
							strcpy(md5_data+i,challenge);
							MD5_calc(digest,md5_data,sizeof(secret)+strlen(challenge));
							for(i=0;i<sizeof(secret);i++)
								md5_data[i]=secret[i]^0x5c;	/* opad */
							memcpy(md5_data+i,digest,sizeof(digest));
							MD5_calc(digest,md5_data,sizeof(secret)+sizeof(digest));
							
							safe_snprintf(buf,sizeof(buf),"%s %s",startup->relay_user,MD5_hex((BYTE*)str,digest));
							b64_encode(p=resp,sizeof(resp),buf,0);
							break;
						default:
							p="<unknown>";
							break;
					}
					sockprintf(sock,session,"%s",p);
					if((startup->options&MAIL_OPT_RELAY_AUTH_MASK)!=MAIL_OPT_RELAY_AUTH_CRAM_MD5) {
						if(!sockgetrsp(sock,session,"334",buf,sizeof(buf))) {
							SAFEPRINTF3(err,badrsp_err,server,buf,"334 Password");
							bounce(sock, &smb,&msg,err,/* immediate: */buf[0]=='5');
							continue;
						}
						switch(startup->options&MAIL_OPT_RELAY_AUTH_MASK) {
							case MAIL_OPT_RELAY_AUTH_LOGIN:
								b64_encode(p=buf,sizeof(buf),startup->relay_pass,0);
								break;
							default:
								p="<unknown>";
								break;
						}
						sockprintf(sock,session,"%s",p);
					}
				}
				if(!sockgetrsp(sock,session,"235",buf,sizeof(buf))) {
					SAFEPRINTF3(err,badrsp_err,server,buf,"235");
					bounce(sock, &smb,&msg,err,/* immediate: */buf[0]=='5');
					continue;
				}
			}

			/* MAIL */
			if(fromaddr[0]=='<')
				sockprintf(sock,session,"MAIL FROM: %s",fromaddr);
			else
				sockprintf(sock,session,"MAIL FROM: <%s>",fromaddr);
			if(!sockgetrsp(sock,session,"250", buf, sizeof(buf))) {
				remove_msg_intransit(&smb,&msg);
				SAFEPRINTF3(err,badrsp_err,server,buf,"250");
				bounce(sock, &smb,&msg,err,/* immediate: */buf[0]=='5');
				continue;
			}
			/* RCPT */
			if(msg.forward_path!=NULL) {
				SAFECOPY(toaddr,msg.forward_path);
			} else {
				if((p=strrchr((char*)msg.to_net.addr,'<'))!=NULL)
					p++;
				else
					p=(char*)msg.to_net.addr;
				SAFECOPY(toaddr,p);
				truncstr(toaddr,"> ");
				if((p=strrchr(toaddr,'@'))!=NULL && (tp=strrchr(toaddr,':'))!=NULL
					&& tp > p)
					*tp=0;	/* Remove ":port" designation from envelope */
			}
			sockprintf(sock,session,"RCPT TO: <%s>", toaddr);
			if(!sockgetrsp(sock,session,"25", buf, sizeof(buf))) {
				remove_msg_intransit(&smb,&msg);
				SAFEPRINTF3(err,badrsp_err,server,buf,"25*");
				bounce(sock, &smb,&msg,err,/* immediate: */buf[0]=='5');
				continue;
			}
			/* DATA */
			sockprintf(sock,session,"DATA");
			if(!sockgetrsp(sock,session,"354", buf, sizeof(buf))) {
				remove_msg_intransit(&smb,&msg);
				SAFEPRINTF3(err,badrsp_err,server,buf,"354");
				bounce(sock, &smb,&msg,err,/* immediate: */buf[0]=='5');
				continue;
			}
			bytes=strlen(msgtxt);
			lprintf(LOG_DEBUG,"%04d SEND sending message text (%u bytes) begin"
				,sock, bytes);
			lines=sockmsgtxt(sock,session,&msg,msgtxt,session);
			lprintf(LOG_DEBUG,"%04d SEND send of message text (%u bytes, %u lines) complete, waiting for acknowledgment (250)"
				,sock, bytes, lines);
			if(!sockgetrsp(sock,session,"250", buf, sizeof(buf))) {
				/* Wait doublely-long for the acknowledgment */
				if(buf[0] || !sockgetrsp(sock,session,"250", buf, sizeof(buf))) {
					remove_msg_intransit(&smb,&msg);
					SAFEPRINTF3(err,badrsp_err,server,buf,"250");
					bounce(sock, &smb,&msg,err,/* immediate: */buf[0]=='5');
					continue;
				}
			}
			lprintf(LOG_INFO,"%04d SEND message transfer complete (%u bytes, %lu lines)", sock, bytes, lines);

			/* Now lets mark this message for deletion without corrupting the index */
			msg.hdr.attr|=MSG_DELETE;
			msg.hdr.netattr&=~MSG_INTRANSIT;
			if((i=smb_updatemsg(&smb,&msg))!=SMB_SUCCESS)
				lprintf(LOG_ERR,"%04d !SEND ERROR %d (%s) deleting message #%lu"
					,sock, i, smb.last_error, msg.hdr.number);
			if(msg.hdr.auxattr&MSG_FILEATTACH)
				delfattach(&scfg,&msg);

			if(msg.from_agent==AGENT_PERSON && !(startup->options&MAIL_OPT_NO_AUTO_EXEMPT))
				exempt_email_addr("SEND Auto-exempting",msg.from,fromext,fromaddr,toaddr);

			/* QUIT */
			sockprintf(sock,session,"QUIT");
			sockgetrsp(sock,session,"221", buf, sizeof(buf));
			if (session != -1) {
				cryptDestroySession(session);
				session = -1;
			}
			mail_close_socket(sock);
			sock=INVALID_SOCKET;
		}				
		status(STATUS_WFC);
		/* Free up resources here */
		if(mail!=NULL)
			freemail(mail);
	}
	if (session != -1)
		cryptDestroySession(session);
	if(sock!=INVALID_SOCKET)
		mail_close_socket(sock);

	listFree(&failed_server_list);

	smb_freemsgtxt(msgtxt);
	smb_freemsgmem(&msg);
	smb_close(&smb);

	if(active_sendmail!=0)
		active_sendmail=0, update_clients();

	{
		int32_t remain = thread_down();
		lprintf(LOG_DEBUG,"0000 SendMail thread terminated (%u threads remain)", remain);
	}

	sendmail_running=FALSE;
}

void DLLCALL mail_terminate(void)
{
  	lprintf(LOG_INFO,"Mail Server terminate");
	terminate_server=TRUE;
}

static void cleanup(int code)
{
	int					i;

	if(protected_uint32_value(thread_count) > 1) {
		lprintf(LOG_DEBUG,"#### Waiting for %d child threads to terminate", protected_uint32_value(thread_count)-1);
		while(protected_uint32_value(thread_count) > 1) {
			mswait(100);
		}
	}

	free_cfg(&scfg);

	semfile_list_free(&recycle_semfiles);
	semfile_list_free(&shutdown_semfiles);

	if(mailproc_list!=NULL) {
		for(i=0;i<mailproc_count;i++) {
			if(mailproc_list[i].ar!=NULL && mailproc_list[i].ar!=nular)
				free(mailproc_list[i].ar);
			strListFree(&mailproc_list[i].to);
			strListFree(&mailproc_list[i].from);
		}
		FREE_AND_NULL(mailproc_list);
	}

	/* Check a if(mail_set!=NULL) check be performed here? */
	xpms_destroy(mail_set, mail_close_socket_cb, NULL);
	mail_set=NULL;
	terminated=TRUE;

	update_clients();	/* active_clients is destroyed below */

	if(protected_uint32_value(active_clients))
		lprintf(LOG_WARNING,"#### !Mail Server terminating with %ld active clients", protected_uint32_value(active_clients));
	else
		protected_uint32_destroy(active_clients);

#ifdef _WINSOCKAPI_	
	if(WSAInitialized && WSACleanup()!=0) 
		lprintf(LOG_ERR,"0000 !WSACleanup ERROR %d",ERROR_VALUE);
#endif

	thread_down();
	status("Down");
	if(terminate_server || code) {
		char str[1024];
		sprintf(str,"%lu connections served", stats.connections_served);
		if(stats.connections_refused)
			sprintf(str+strlen(str),", %lu refused", stats.connections_refused);
		if(stats.connections_ignored)
			sprintf(str+strlen(str),", %lu ignored", stats.connections_refused);
		if(stats.sessions_refused)
			sprintf(str+strlen(str),", %lu sessions refused", stats.sessions_refused);
		sprintf(str+strlen(str),", %lu messages received", stats.msgs_received);
		if(stats.msgs_refused)
			sprintf(str+strlen(str),", %lu refused", stats.msgs_refused);
		if(stats.msgs_ignored)
			sprintf(str+strlen(str),", %lu ignored", stats.msgs_ignored);
		if(stats.errors)
			sprintf(str+strlen(str),", %lu errors", stats.errors);
		if(stats.crit_errors)
			sprintf(str+strlen(str),", %lu critcal", stats.crit_errors);

		lprintf(LOG_INFO,"#### Mail Server thread terminated (%s)",str);
	}
	if(startup!=NULL && startup->terminated!=NULL)
		startup->terminated(startup->cbdata,code);
}

const char* DLLCALL mail_ver(void)
{
	static char ver[256];
	char compiler[32];

	DESCRIBE_COMPILER(compiler);

	sscanf("$Revision$", "%*s %s", revision);

	sprintf(ver,"%s %s%s  SMBLIB %s  "
		"Compiled %s %s with %s"
		,server_name
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
	char*			p;
	char			path[MAX_PATH+1];
	char			mailproc_ini[MAX_PATH+1];
	char			str[256];
	char			error[256];
	char			compiler[32];
	union xp_sockaddr	client_addr;
	socklen_t		client_addr_len;
	char			host_ip[INET6_ADDRSTRLEN];
	SOCKET			client_socket;
	int				i;
	ulong			l;
	time_t			t;
	time_t			start;
	time_t			initialized=0;
	pop3_t*			pop3;
	smtp_t*			smtp;
	FILE*			fp;
	str_list_t		sec_list;
	void			*cbdata;
	CRYPT_SESSION	session = -1;

	mail_ver();

	startup=(mail_startup_t*)arg;

#ifdef _THREAD_SUID_BROKEN
	if(thread_suid_broken)
		startup->seteuid(TRUE);
#endif

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

	ZERO_VAR(js_server_props);
	SAFEPRINTF2(js_server_props.version,"%s %s",server_name,revision);
	js_server_props.version_detail=mail_ver();
	js_server_props.clients=&active_clients.value;
	js_server_props.options=&startup->options;
	js_server_props.interfaces=&startup->interfaces;

	uptime=0;
	memset(&stats,0,sizeof(stats));
	startup->recycle_now=FALSE;
	startup->shutdown_now=FALSE;
	terminate_server=FALSE;

	SetThreadName("sbbs/mailServer");
	protected_uint32_init(&thread_count, 0);

	do {
		/* Setup intelligent defaults */
		if(startup->relay_port==0)				startup->relay_port=IPPORT_SMTP;
		if(startup->submission_port==0)			startup->submission_port=IPPORT_SUBMISSION;
		if(startup->submissions_port==0)		startup->submissions_port=IPPORT_SUBMISSIONS;
		if(startup->smtp_port==0)				startup->smtp_port=IPPORT_SMTP;
		if(startup->pop3_port==0)				startup->pop3_port=IPPORT_POP3;
		if(startup->pop3s_port==0)				startup->pop3s_port=IPPORT_POP3S;
		if(startup->rescan_frequency==0)		startup->rescan_frequency=MAIL_DEFAULT_RESCAN_FREQUENCY;
		if(startup->max_delivery_attempts==0)	startup->max_delivery_attempts=MAIL_DEFAULT_MAX_DELIVERY_ATTEMPTS;
		if(startup->max_inactivity==0) 			startup->max_inactivity=MAIL_DEFAULT_MAX_INACTIVITY; /* seconds */
		if(startup->sem_chk_freq==0)			startup->sem_chk_freq=DEFAULT_SEM_CHK_FREQ;
		if(startup->js.max_bytes==0)			startup->js.max_bytes=JAVASCRIPT_MAX_BYTES;
		if(startup->js.cx_stack==0)				startup->js.cx_stack=JAVASCRIPT_CONTEXT_STACK;

		protected_uint32_adjust(&thread_count,1);
		thread_up(FALSE /* setuid */);

		status("Initializing");

		memset(&scfg, 0, sizeof(scfg));

		lprintf(LOG_INFO,"%s Revision %s%s"
			,server_name
			,revision
#ifdef _DEBUG
			," Debug"
#else
			,""
#endif
			);

		DESCRIBE_COMPILER(compiler);

		lprintf(LOG_INFO,"Compiled %s %s with %s", __DATE__, __TIME__, compiler);

		lprintf(LOG_DEBUG,"SMBLIB %s (format %x.%02x)",smb_lib_ver(),smb_ver()>>8,smb_ver()&0xff);

		sbbs_srand();

		if(!winsock_startup()) {
			cleanup(1);
			return;
		}

		t=time(NULL);
		lprintf(LOG_INFO,"Initializing on %.24s with options: %lx"
			,ctime_r(&t,str),startup->options);

		if(chdir(startup->ctrl_dir)!=0)
			lprintf(LOG_ERR,"!ERROR %d changing directory to: %s", errno, startup->ctrl_dir);

		/* Initial configuration and load from CNF files */
		SAFECOPY(scfg.ctrl_dir,startup->ctrl_dir);
		lprintf(LOG_INFO,"Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size=sizeof(scfg);
		SAFECOPY(error,UNKNOWN_LOAD_ERROR);
		if(!load_cfg(&scfg, NULL, TRUE, error)) {
			lprintf(LOG_CRIT,"!ERROR %s",error);
			lprintf(LOG_CRIT,"!Failed to load configuration files");
			cleanup(1);
			return;
		}

		if(startup->temp_dir[0])
			SAFECOPY(scfg.temp_dir,startup->temp_dir);
		else
			SAFECOPY(scfg.temp_dir,"../temp");
	   	prep_dir(scfg.ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));
		MKDIR(scfg.temp_dir);
		lprintf(LOG_DEBUG,"Temporary file directory: %s", scfg.temp_dir);
		if(!isdir(scfg.temp_dir)) {
			lprintf(LOG_CRIT,"!Invalid temp directory: %s", scfg.temp_dir);
			cleanup(1);
			return;
		}

		/* Parse the mailproc[.host].ini */
		mailproc_list=NULL;
		mailproc_count=0;
		iniFileName(mailproc_ini,sizeof(mailproc_ini),scfg.ctrl_dir,"mailproc.ini");
		if((fp=iniOpenFile(mailproc_ini, /* create? */FALSE))!=NULL) {
			lprintf(LOG_DEBUG,"Reading %s",mailproc_ini);
			sec_list = iniReadSectionList(fp,/* prefix */NULL);
			if((mailproc_count=strListCount(sec_list))!=0
				&& (mailproc_list=malloc(mailproc_count*sizeof(struct mailproc)))!=NULL) {
				char buf[INI_MAX_VALUE_LEN+1];
				for(i=0;i<mailproc_count;i++) {
					memset(&mailproc_list[i],0,sizeof(struct mailproc));
					SAFECOPY(mailproc_list[i].name,sec_list[i]);
					SAFECOPY(mailproc_list[i].cmdline,
						iniReadString(fp,sec_list[i],"Command",sec_list[i],buf));
					SAFECOPY(mailproc_list[i].eval,
						iniReadString(fp,sec_list[i],"Eval","",buf));
					mailproc_list[i].to =
						iniReadStringList(fp,sec_list[i],"To",",",NULL);
					mailproc_list[i].from =
						iniReadStringList(fp,sec_list[i],"From",",",NULL);
					mailproc_list[i].passthru =
						iniReadBool(fp,sec_list[i],"PassThru",TRUE);
					mailproc_list[i].native =
						iniReadBool(fp,sec_list[i],"Native",FALSE);
					mailproc_list[i].disabled = 
						iniReadBool(fp,sec_list[i],"Disabled",FALSE);
					mailproc_list[i].ignore_on_error = 
						iniReadBool(fp,sec_list[i],"IgnoreOnError",FALSE);
					mailproc_list[i].process_spam =
						iniReadBool(fp,sec_list[i],"ProcessSPAM",TRUE);
					mailproc_list[i].process_dnsbl =
						iniReadBool(fp,sec_list[i],"ProcessDNSBL",TRUE);
					mailproc_list[i].ar = 
						arstr(NULL,iniReadString(fp,sec_list[i],"AccessRequirements","",buf),&scfg);
				}
			}
			iniFreeStringList(sec_list);
			iniCloseFile(fp);
		}

		if(startup->host_name[0]==0)
			SAFECOPY(startup->host_name,scfg.sys_inetaddr);

		if((t=checktime())!=0) {   /* Check binary time */
			lprintf(LOG_ERR,"!TIME PROBLEM (%ld)",t);
		}

		if(uptime==0)
			uptime=time(NULL);	/* this must be done *after* setting the timezone */

		if(startup->max_clients==0) {
			startup->max_clients=scfg.sys_nodes;
			if(startup->max_clients<10)
				startup->max_clients=10;
		}

		lprintf(LOG_DEBUG,"Maximum clients: %u",startup->max_clients);

		lprintf(LOG_DEBUG,"Maximum inactivity: %u seconds",startup->max_inactivity);

		protected_uint32_init(&active_clients, 0);
		update_clients();

		/* open a socket and wait for a client */
		mail_set = xpms_create(startup->bind_retry_count, startup->bind_retry_delay, lprintf);
		if(mail_set == NULL) {
			lprintf(LOG_CRIT,"!ERROR creating mail server socket set", ERROR_VALUE);
			cleanup(1);
			return;
		}
		terminated=FALSE;
		if(!xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->interfaces
			, startup->smtp_port, "SMTP Transfer Agent", mail_open_socket, startup->seteuid, "smtp"))
			lprintf(LOG_INFO,"SMTP No extra interfaces listening");

		if(startup->options&MAIL_OPT_USE_SUBMISSION_PORT) {
			xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->interfaces
				, startup->submission_port, "SMTP Submission Agent", mail_open_socket, startup->seteuid, "submission");
		}

		if(startup->options&MAIL_OPT_TLS_SUBMISSION) {
			xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->interfaces, startup->submissions_port
				, "SMTPS Submission Agent", mail_open_socket, startup->seteuid, "submissions");
		}

		if(startup->options&MAIL_OPT_ALLOW_POP3) {
			/* open a socket and wait for a client */
			if(!xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->pop3_interfaces, startup->pop3_port
				, "POP3 Server", mail_open_socket, startup->seteuid, "pop3"))
				lprintf(LOG_INFO,"POP3 No extra interfaces listening");
		}

		if(startup->options&MAIL_OPT_TLS_POP3) {
			/* open a socket and wait for a client */
			if(!xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->pop3_interfaces
				, startup->pop3s_port, "POP3S Server", mail_open_socket, startup->seteuid, "pop3s"))
				lprintf(LOG_INFO,"POP3S No extra interfaces listening");
		}

		sem_init(&sendmail_wakeup_sem,0,0);

		if(!(startup->options&MAIL_OPT_NO_SENDMAIL)) {
			sendmail_running=TRUE;
			protected_uint32_adjust(&thread_count,1);
			_beginthread(sendmail_thread, 0, NULL);
		}

		status(STATUS_WFC);

		/* Setup recycle/shutdown semaphore file lists */
		shutdown_semfiles=semfile_list_init(scfg.ctrl_dir,"shutdown","mail");
		recycle_semfiles=semfile_list_init(scfg.ctrl_dir,"recycle","mail");
		semfile_list_add(&recycle_semfiles,startup->ini_fname);
		SAFEPRINTF(path,"%smailsrvr.rec",scfg.ctrl_dir);	/* legacy */
		semfile_list_add(&recycle_semfiles,path);
		semfile_list_add(&recycle_semfiles,mailproc_ini);
		if(!initialized) {
			semfile_list_check(&initialized,recycle_semfiles);
			semfile_list_check(&initialized,shutdown_semfiles);
		}

		/* signal caller that we've started up successfully */
		if(startup->started!=NULL)
    		startup->started(startup->cbdata);

		lprintf(LOG_INFO,"Mail Server thread started");

		while(!terminated && !terminate_server) {
			YIELD();

			if(protected_uint32_value(thread_count) <= (unsigned int)(1+(sendmail_running?1:0))) {
				if(!(startup->options&MAIL_OPT_NO_RECYCLE)) {
					if((p=semfile_list_check(&initialized,recycle_semfiles))!=NULL) {
						lprintf(LOG_INFO,"Recycle semaphore file (%s) detected",p);
						break;
					}
					if(startup->recycle_now==TRUE) {
						lprintf(LOG_NOTICE,"Recycle semaphore signaled");
						startup->recycle_now=FALSE;
						break;
					}
				}
				if(((p=semfile_list_check(&initialized,shutdown_semfiles))!=NULL
						&& lprintf(LOG_INFO,"Shutdown semaphore file (%s) detected",p))
					|| (startup->shutdown_now==TRUE
						&& lprintf(LOG_INFO,"Shutdown semaphore signaled"))) {
					startup->shutdown_now=FALSE;
					terminate_server=TRUE;
					break;
				}
			}

			/* now wait for connection */
			client_addr_len = sizeof(client_addr);
			client_socket = xpms_accept(mail_set,&client_addr, &client_addr_len, startup->sem_chk_freq*1000, &cbdata);
			if(client_socket != INVALID_SOCKET) {
				if(startup->socket_open!=NULL)
					startup->socket_open(startup->cbdata,TRUE);
				stats.sockets++;

				inet_addrtop(&client_addr, host_ip, sizeof(host_ip));
				if(trashcan(&scfg,host_ip,"ip-silent")) {
					mail_close_socket(client_socket);
					stats.connections_ignored++;
					continue;
				}

				if(protected_uint32_value(active_clients)>=startup->max_clients) {
					lprintf(LOG_WARNING,"%04d %s !MAXIMUM CLIENTS (%u) reached, access denied (%u total)"
						,client_socket, (char *)cbdata, startup->max_clients, ++stats.connections_refused);
					sockprintf(client_socket,session,"-ERR Maximum active clients reached, please try again later.");
					mswait(3000);
					mail_close_socket(client_socket);
					continue;
				}

				l=1;

				if((i=ioctlsocket(client_socket, FIONBIO, &l))!=0) {
					lprintf(LOG_CRIT,"%04d %s !ERROR %d (%d) disabling blocking on socket"
						,client_socket, (char *)cbdata, i, ERROR_VALUE);
					sockprintf(client_socket,session,"-ERR System error, please try again later.");
					mswait(3000);
					mail_close_socket(client_socket);
					continue;
				}

				if(strcmp((char *)cbdata, "pop3") && strcmp((char *)cbdata, "pop3s")) { /* Not POP3 */
					if((smtp=malloc(sizeof(smtp_t)))==NULL) {
						lprintf(LOG_CRIT,"%04d SMTP !ERROR allocating %u bytes of memory for smtp_t"
							,client_socket, sizeof(smtp_t));
						mail_close_socket(client_socket);
						continue;
					}

					smtp->socket=client_socket;
					memcpy(&smtp->client_addr, &client_addr, client_addr_len);
					smtp->client_addr_len=client_addr_len;
					smtp->tls_port = (strcmp((char *)cbdata, "submissions") == 0);
					protected_uint32_adjust(&thread_count,1);
					_beginthread(smtp_thread, 0, smtp);
					stats.connections_served++;
				}
				else {
					if((pop3=malloc(sizeof(pop3_t)))==NULL) {
						lprintf(LOG_CRIT,"%04d !POP3 ERROR allocating %u bytes of memory for pop3_t"
							,client_socket,sizeof(pop3_t));
						sockprintf(client_socket,session,"-ERR System error, please try again later.");
						mswait(3000);
						mail_close_socket(client_socket);
						continue;
					}

					pop3->socket=client_socket;
					memcpy(&pop3->client_addr, &client_addr, client_addr_len);
					pop3->client_addr_len=client_addr_len;
					pop3->tls_port = (strcmp((char *)cbdata, "pop3s") == 0);
					protected_uint32_adjust(&thread_count,1);
					_beginthread(pop3_thread, 0, pop3);
					stats.connections_served++;
				}
			}
		}

		if(protected_uint32_value(active_clients)) {
			lprintf(LOG_DEBUG,"Waiting for %d active clients to disconnect..."
				, protected_uint32_value(active_clients));
			start=time(NULL);
			while(protected_uint32_value(active_clients)) {
				if(startup->max_inactivity && time(NULL)-start>startup->max_inactivity) {
					lprintf(LOG_WARNING,"!TIMEOUT (%u seconds) waiting for %d active clients"
						, startup->max_inactivity, protected_uint32_value(active_clients));
					break;
				}
				mswait(100);
			}
		}

		if(sendmail_running) {
			terminate_sendmail=TRUE;
			sem_post(&sendmail_wakeup_sem);
			mswait(100);
		}
		if(sendmail_running) {
			lprintf(LOG_DEBUG,"Waiting for SendMail thread to terminate...");
			start=time(NULL);
			while(sendmail_running) {
				if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
					lprintf(LOG_WARNING,"!TIMEOUT waiting for sendmail thread to terminate");
					break;
				}
				mswait(500);
			}
		}
		if(!sendmail_running) {
			while(sem_destroy(&sendmail_wakeup_sem)==-1 && errno==EBUSY) {
				mswait(1);
				sem_post(&sendmail_wakeup_sem);
			}
		}

		cleanup(0);

		if(!terminate_server) {
			lprintf(LOG_INFO,"Recycling server...");
			mswait(2000);
			if(startup->recycle!=NULL)
				startup->recycle(startup->cbdata);
		}

	} while(!terminate_server);

	protected_uint32_destroy(thread_count);
}
