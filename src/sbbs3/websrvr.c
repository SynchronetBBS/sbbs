/* websrvr.c */

/* Synchronet Web Server */

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

#if defined(__unix__)
	#include <sys/wait.h>		/* waitpid() */
	#include <sys/types.h>
	#include <signal.h>			/* kill() */
#endif

#ifndef JAVASCRIPT
#define JAVASCRIPT
#endif

#include "sbbs.h"
#include "sockwrap.h"		/* sendfilesocket() */
#include "websrvr.h"

static const char*	server_name="Synchronet Web Server";
static const char*	newline="\r\n";
static const char*	http_scheme="http://";
static const size_t	http_scheme_len=7;

extern const uchar* nular;

#define TIMEOUT_THREAD_WAIT		60		/* Seconds */
#define MAX_MIME_TYPES			128
#define MAX_REQUEST_LINE		1024

static scfg_t	scfg;
static BOOL		scfg_reloaded=TRUE;
static uint 	http_threads_running=0;
static ulong	active_clients=0;
static ulong	sockets=0;
static BOOL		recycle_server=FALSE;
static uint		thread_count=0;
static SOCKET	server_socket=INVALID_SOCKET;
static ulong	mime_count=0;
static char		revision[16];
static char		root_dir[MAX_PATH+1];
static char		error_dir[MAX_PATH+1];
static time_t	uptime=0;
static DWORD	served=0;
static web_startup_t* startup=NULL;

typedef struct  {
	char	*val;
	void	*next;
} linked_list;

typedef struct  {
	BOOL		method;
	char		virtual_path[MAX_PATH+1];
	char		physical_path[MAX_PATH+1];
	BOOL		parsed_headers;
	BOOL    	expect_go_ahead;
	time_t		if_modified_since;
	BOOL		keep_alive;
	char		ars[256];
	char    	auth[128];				/* UserID:Password */
	char		host[128];				/* The requested host. (virtual hosts) */
	int			send_location;
	const char*	mime_type;

	/* CGI parameters */
	char		query_str[MAX_REQUEST_LINE];

	linked_list*	cgi_env;
	linked_list*	dynamic_heads;
	char		status[MAX_REQUEST_LINE+1];
	char *		post_data;
	size_t		post_len;
	int			dynamic;

	/* Dynamically (sever-side JS) generated HTML parameters */
	FILE*	fp;

} http_request_t;

typedef struct  {
	SOCKET			socket;
	SOCKADDR_IN		addr;
	http_request_t	req;
	char			host_ip[64];
	char			host_name[128];	/* Resolved remote host */
	int				http_ver;       /* HTTP version.  0 = HTTP/0.9, 1=HTTP/1.0, 2=HTTP/1.1 */
	BOOL			finished;		/* Do not accept any more imput from client */
	user_t			user;

	/* JavaScript parameters */
	JSRuntime*		js_runtime;
	JSContext*		js_cx;
	JSObject*		js_glob;
	JSObject*		js_query;
	JSObject*		js_header;
	JSObject*		js_request;
} http_session_t;

typedef struct {
	char	ext[16];
	char	type[128];
} mime_types_t;

static mime_types_t		mime_types[MAX_MIME_TYPES];

enum { 
	 HTTP_0_9
	,HTTP_1_0
	,HTTP_1_1
};
static char* http_vers[] = {
	 ""
	,"HTTP/1.0"
	,"HTTP/1.1"
	,NULL	/* terminator */
};

enum { 
	 HTTP_HEAD
	,HTTP_GET
};

static char* methods[] = {
	 "HEAD"
	,"GET"
	,"POST"
	,NULL	/* terminator */
};

enum { 
	 IS_STATIC
	,IS_CGI
	,IS_JS
	,IS_SSJS
};

enum { 
	 HEAD_DATE
	,HEAD_HOST
	,HEAD_IFMODIFIED
	,HEAD_LENGTH
	,HEAD_TYPE
	,HEAD_AUTH
	,HEAD_CONNECTION
	,HEAD_WWWAUTH
	,HEAD_STATUS
	,HEAD_ALLOW
	,HEAD_EXPIRES
	,HEAD_LASTMODIFIED
	,HEAD_LOCATION
	,HEAD_PRAGMA
	,HEAD_SERVER
};

static struct {
	int		id;
	char*	text;
} headers[] = {
	{ HEAD_DATE,			"Date"					},
	{ HEAD_HOST,			"Host"					},
	{ HEAD_IFMODIFIED,		"If-Modified-Since"		},
	{ HEAD_LENGTH,			"Content-Length"		},
	{ HEAD_TYPE,			"Content-Type"			},
	{ HEAD_AUTH,			"Authorization"			},
	{ HEAD_CONNECTION,		"Connection"			},
	{ HEAD_WWWAUTH,			"WWW-Authenticate"		},
	{ HEAD_STATUS,			"Status"				},
	{ HEAD_ALLOW,			"Allow"					},
	{ HEAD_EXPIRES,			"Expires"				},
	{ HEAD_LASTMODIFIED,	"Last-Modified"			},
	{ HEAD_LOCATION,		"Location"				},
	{ HEAD_PRAGMA,			"Pragma"				},
	{ HEAD_SERVER,			"Server"				},
	{ -1,					NULL /* terminator */	},
};

enum  {
	NO_LOCATION
	,MOVED_TEMP
	,MOVED_PERM
	,MOVED_STAT
};

static char	*days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static char	*months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static const char * base64alphabet = 
 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

static DWORD monthdays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

static void respond(http_session_t * session);
static BOOL js_setup(http_session_t* session);

static time_t time_gm( struct tm* ti )  {
	time_t t;

	t=(ti->tm_year-70)*365;
	t+=(ti->tm_year-69)/4;
	t+=monthdays[ti->tm_mon];
	if(ti->tm_mon >= 2 && ti->tm_year+1900%400 ? (ti->tm_year+1900%100 ? (ti->tm_year+1900%4 ? 0:1):0):1)
		++t;
	t += ti->tm_mday - 1;
	t = t * 24 + ti->tm_hour;
	t = t * 60 + ti->tm_min;
	t = t * 60 + ti->tm_sec;

	return t;
}

static int lprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    if(startup==NULL || startup->lputs==NULL)
        return(0);

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

static void status(char* str)
{
	if(startup!=NULL && startup->status!=NULL)
	    startup->status(str);
}

static void update_clients(void)
{
	if(startup!=NULL && startup->clients!=NULL)
		startup->clients(active_clients);
}

#if 0	/* These will be used later */
static void client_on(SOCKET sock, client_t* client, BOOL update)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(TRUE,sock,client,update);
}

static void client_off(SOCKET sock)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(FALSE,sock,NULL,FALSE);
}
#endif

static void thread_up(BOOL setuid)
{
	thread_count++;
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(TRUE, setuid);
}

static void thread_down(void)
{
	if(thread_count>0)
		thread_count--;
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(FALSE, FALSE);
}

static linked_list *add_list(linked_list *list,const char *value)  {
	linked_list*	entry;

	entry=malloc(sizeof(linked_list));
	if(entry==NULL)  {
		lprintf("Could not allocate memory for \"%s\" in linked list.",&value);
		return(list);
	}
	entry->val=malloc(strlen(value)+1);
	if(entry->val==NULL)  {
		FREE_AND_NULL(entry);
		lprintf("Could not allocate memory for \"%s\" in linked list.",&value);
		return(list);
	}
	strcpy(entry->val,value);
	entry->next=list;
	return(entry);
}

static void add_env(http_session_t *session, const char *name,const char *value)  {
	char	newname[129];
	char	fullname[387];
	char	*p;
	
	if(name==NULL || value==NULL)  {
		lprintf("%04d Attempt to set NULL env variable", session->socket);
		return;
	}
	SAFECOPY(newname,name);

	for(p=newname;*p;p++)  {
		*p=toupper(*p);
		if(*p=='-')
			*p='_';
	}

	sprintf(fullname,"%s=%s",newname,value);
	session->req.cgi_env=add_list(session->req.cgi_env,fullname);
}

static void init_enviro(http_session_t *session)  {
	char	str[128];

	add_env(session,"SERVER_SOFTWARE",VERSION_NOTICE);
	sprintf(str,"%d",startup->port);
	add_env(session,"SERVER_PORT",str);
	add_env(session,"GATEWAY_INTERFACE","CGI/1.1");
	if(!strcmp(session->host_name,"<no name>"))
		add_env(session,"REMOTE_HOST",session->host_name);
	add_env(session,"REMOTE_ADDR",session->host_ip);
}

static int sockprint(SOCKET sock, const char *str)  {
	int len;
	int	result;
	int written=0;

	if(startup->options&WEB_OPT_DEBUG_TX)
		lprintf("%04d TX: %s", sock, str);
	if(sock==INVALID_SOCKET)
		return(0);
	len=strlen(str);
	while((result=sendsocket(sock,str+written,len-written))>0)  {
		written+=result;
	}
	if(written != len) {
		lprintf("%04d !ERROR %d sending on socket",sock,ERROR_VALUE);
		return(0);
	}
	return(len);
}

static int sockprinttwo(SOCKET sock, const char *head, const char *value, BOOL colon)  {
	int total=0;

	if(sock==INVALID_SOCKET)
		return(0);
	total+=sockprint(sock,head);
	if(colon)  {
		total+=sockprint(sock,": ");
	}
	else  {
		total+=sockprint(sock," ");
	}
	
	total+=sockprint(sock,value);
	total+=sockprint(sock,newline);

	if(sock==INVALID_SOCKET)
		return(0);
	return(total);
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
    len=vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
	if(startup->options&WEB_OPT_DEBUG_TX)
		lprintf("%04d TX: %s", sock, sbuf);
	strcat(sbuf,newline);
	len+=2;
    va_end(argptr);

	if(sock==INVALID_SOCKET) {
		lprintf("!INVALID SOCKET in call to sockprintf");
		return(0);
	}

	/* Check socket for writability (using select) */
	tv.tv_sec=60;
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

static char *cleanpath(char *target, char *path, size_t size)  {
	char	*out;
	char	*p;
	char	*p2;
	
	out=target;
	*out=0;

	if(*path != '/' && *path != '\\')  {
		p=getcwd(target,size);
		if(p==NULL || strlen(p)+strlen(path)>=size)
			return(NULL);
		out=strrchr(target,'\0');
		*(out++)='/';
		*out=0;
		out--;
	}
	strncat(target,path,size-1);
	
	for(;*out;out++)  {
		while(*out=='/' || *out=='\\')  {
			if(*(out+1)=='/' || *(out+1)=='\\')
				memmove(out,out+1,strlen(out));
			else if(*(out+1)=='.' && (*(out+2)=='/' || *(out+2)=='\\'))
				memmove(out,out+2,strlen(out)-1);
			else if(*(out+1)=='.' && *(out+2)=='.' && (*(out+3)=='/' || *(out+3)=='\\'))  {
				*out=0;
				p=strrchr(target,'/');
				p2=strrchr(target,'\\');
				if(p2>p)
					p=p2;
				memmove(p,out+3,strlen(out+3)+1);
				out=p;
			}
			else  {
				out++;
			}
		}
	}
	return(target);
}

static int getmonth(char *mon)
{
	int	i;
	for(i=0;i<12;i++)
		if(!stricmp(mon,months[i]))
			return(i);

	return 0;
}

static time_t decode_date(char *date)
{
	struct	tm	ti;
	char	*token;
	time_t	t;

	ti.tm_sec=0;		/* seconds (0 - 60) */
	ti.tm_min=0;		/* minutes (0 - 59) */
	ti.tm_hour=0;		/* hours (0 - 23) */
	ti.tm_mday=1;		/* day of month (1 - 31) */
	ti.tm_mon=0;		/* month of year (0 - 11) */
	ti.tm_year=0;		/* year - 1900 */
	ti.tm_isdst=0;		/* is summer time in effect? */

#if 0	/* non-standard */
	ti.tm_zone="UTC";	/* abbreviation of timezone name */
	ti.tm_gmtoff=0;		/* offset from UTC in seconds */
#endif

	token=strtok(date,",");
	/* This probobly only needs to be 9, but the extra one is for luck. */
	if(strlen(date)>15) {
		/* asctime() */
		/* Toss away week day */
		token=strtok(date," ");
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_mon=getmonth(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_mday=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		ti.tm_hour=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		ti.tm_min=atoi(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_sec=atoi(token);
		token=strtok(NULL,"");
		if(token==NULL)
			return(0);
		ti.tm_year=atoi(token)-1900;
	}
	else  {
		/* RFC 1123 or RFC 850 */
		token=strtok(NULL," -");
		if(token==NULL)
			return(0);
		ti.tm_mday=atoi(token);
		token=strtok(NULL," -");
		if(token==NULL)
			return(0);
		ti.tm_mon=getmonth(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_year=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		ti.tm_hour=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		ti.tm_min=atoi(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_sec=atoi(token);
		if(ti.tm_year>1900)
			ti.tm_year -= 1900;
	}

	t=time_gm(&ti);
	return(t);
}

static SOCKET open_socket(int type)
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
	}
	return(sock);
}


static int close_socket(SOCKET sock)
{
	int		result;

	if(sock==INVALID_SOCKET)
		return(-1);

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	if(startup!=NULL && startup->socket_open!=NULL) {
		startup->socket_open(FALSE);
	}
	sockets--;
	if(result!=0) {
		if(ERROR_VALUE!=ENOTSOCK)
			lprintf("%04d !ERROR %d closing socket",sock, ERROR_VALUE);
	}

	return(result);
}

static void close_request(http_session_t * session)
{
	linked_list	*p;
	while(session->req.dynamic_heads != NULL)  {
		FREE_AND_NULL(session->req.dynamic_heads->val);
		p=session->req.dynamic_heads->next;
		FREE_AND_NULL(session->req.dynamic_heads);
		session->req.dynamic_heads=p;
	}
	while(session->req.cgi_env != NULL)  {
		FREE_AND_NULL(session->req.cgi_env->val);
		p=session->req.cgi_env->next;
		FREE_AND_NULL(session->req.cgi_env);
		session->req.cgi_env=p;
	}
	FREE_AND_NULL(session->req.post_data);
	if(!session->req.keep_alive || session->socket==INVALID_SOCKET) {
		close_socket(session->socket);
		session->socket=INVALID_SOCKET;
		session->finished=TRUE;
	}
}

static int get_header_type(char *header)
{
	int i;
	for(i=0; headers[i].text!=NULL; i++) {
		if(!stricmp(header,headers[i].text)) {
			return(headers[i].id);
		}
	}
	return(-1);
}

static char *get_header(int id) 
{
	int i;
	if(headers[id].id==id)
		return(headers[id].text);

	for(i=0;headers[i].text!=NULL;i++) {
		if(headers[i].id==id) {
			return(headers[i].text);
		}
	}
	return(NULL);
}

static const char* unknown_mime_type="application/octet-stream";

static const char* get_mime_type(char *ext)
{
	uint i;

	if(ext==NULL)
		return(unknown_mime_type);

	for(i=0;i<mime_count;i++)
		if(!stricmp(ext+1,mime_types[i].ext))
			return(mime_types[i].type);

	return(unknown_mime_type);
}

static BOOL send_headers(http_session_t *session, const char *status)
{
	int		ret;
	BOOL	send_file=TRUE;
	time_t	ti;
	char	status_line[MAX_REQUEST_LINE];
	struct stat	stats;
	struct tm	tm;
	linked_list	*p;

	SAFECOPY(status_line,status);
	ret=stat(session->req.physical_path,&stats);
	if(!ret && (stats.st_mtime < session->req.if_modified_since) && !session->req.dynamic) {
		SAFECOPY(status_line,"304 Not Modified");
		ret=-1;
		send_file=FALSE;
	}
	if(session->req.send_location==MOVED_PERM)  {
		SAFECOPY(status_line,"301 Moved Permanently");
		ret=-1;
		send_file=FALSE;
	}
	if(session->req.send_location==MOVED_TEMP)  {
		SAFECOPY(status_line,"302 Moved Temporarily");
		ret=-1;
		send_file=FALSE;
	}
	/* Status-Line */
	sockprinttwo(session->socket,http_vers[session->http_ver],status_line,FALSE);

	/* General Headers */
	ti=time(NULL);
	if(gmtime_r(&ti,&tm)==NULL)
		memset(&tm,0,sizeof(tm));
	sprintf(status_line,"%s: %s, %02d %s %04d %02d:%02d:%02d GMT",get_header(HEAD_DATE),days[tm.tm_wday],tm.tm_mday,months[tm.tm_mon],tm.tm_year+1900,tm.tm_hour,tm.tm_min,tm.tm_sec);
	sockprint(session->socket,status_line);
	sockprint(session->socket,newline);
	if(session->req.keep_alive)
		sockprinttwo(session->socket,get_header(HEAD_CONNECTION),"Keep-Alive",TRUE);
	else
		sockprinttwo(session->socket,get_header(HEAD_CONNECTION),"Close",TRUE);

	/* Response Headers */
	sockprinttwo(session->socket,get_header(HEAD_SERVER),VERSION_NOTICE,TRUE);
	
	/* Entity Headers */
	if(session->req.dynamic)
		sockprinttwo(session->socket,get_header(HEAD_ALLOW),"GET, HEAD, POST",TRUE);
	else
		sockprinttwo(session->socket,get_header(HEAD_ALLOW),"GET, HEAD",TRUE);

	if(session->req.send_location) {
		sockprinttwo(session->socket,get_header(HEAD_LOCATION),(session->req.virtual_path),TRUE);
	}
	if(session->req.keep_alive) {
		if(ret)  {
			sockprinttwo(session->socket,get_header(HEAD_LENGTH),"0",TRUE);
		}
		else  {
			sprintf(status_line,"%s: %d",get_header(HEAD_LENGTH),(int)stats.st_size);
			sockprint(session->socket,status_line);
			sockprint(session->socket,newline);
		}
	}

	if(!ret && !session->req.dynamic)  {
		send_file=sockprinttwo(session->socket,get_header(HEAD_TYPE)
			,session->req.mime_type,TRUE);

		gmtime_r(&stats.st_mtime,&tm);
		sprintf(status_line,"%s: %s, %02d %s %04d %02d:%02d:%02d GMT"
			,get_header(HEAD_LASTMODIFIED)
			,days[tm.tm_wday],tm.tm_mday,months[tm.tm_mon]
			,tm.tm_year+1900,tm.tm_hour,tm.tm_min,tm.tm_sec);
		sockprint(session->socket,status_line);
		sockprint(session->socket,newline);
	} 

	if(session->req.dynamic)  {
		/* Dynamic headers */
		/* Set up environment */
		p=session->req.dynamic_heads;
		while(p != NULL)  {
			sockprint(session->socket,p->val);
			sockprint(session->socket,newline);
			p=p->next;
		}
	}

	sockprint(session->socket,newline);
	return(send_file);
}

static void sock_sendfile(SOCKET socket,char *path)
{
	int		file;
	long	offset=0;

	if(startup->options&WEB_OPT_DEBUG_TX)
		lprintf("%04d Sending %s",socket,path);
	if((file=open(path,O_RDONLY|O_BINARY))==-1)
		lprintf("%04d !ERROR %d opening %s",socket,errno,path);
	else {
		if(sendfilesocket(socket, file, &offset, 0) < 1)
			lprintf("%04d !ERROR %d sending %s"
				, socket, errno, path);
		close(file);
	}
}

static void send_error(http_session_t * session, char *message)
{
	char	error_code[4];

	lprintf("%04d !ERROR %s",session->socket,message);
	session->req.keep_alive=FALSE;
	session->req.send_location=NO_LOCATION;
	SAFECOPY(error_code,message);
	sprintf(session->req.physical_path,"%s/%s.html",error_dir,error_code);
	if(session->http_ver > HTTP_0_9)  {
		session->req.mime_type=get_mime_type(strrchr(session->req.physical_path,'.'));
		send_headers(session,message);
	}
	if(fexist(session->req.physical_path))
		sock_sendfile(session->socket,session->req.physical_path);
	else {
		lprintf("%04d Error message file %s doesn't exist.",session->socket,session->req.physical_path);
		sockprintf(session->socket,"<HTML><HEAD><TITLE>%s Error</TITLE></HEAD><BODY><H1>%s Error</H1><BR><H3>In addition, \
			I can't seem to find the %s error file</H3><br>please notify <a href=\"mailto:SysOp@%s\">\
			The SysOp</a></BODY></HTML>"
			,error_code,error_code,error_code,scfg.sys_inetaddr);
	}
	close_request(session);
}

static BOOL check_ars(http_session_t * session)
{
	char	*username;
	char	*password;
	uchar	*ar;
	BOOL	authorized;

	if(session->req.auth[0]==0) {
		if(startup->options&WEB_OPT_DEBUG_RX)
			lprintf("%04d !No authentication information",session->socket);
		return(FALSE);
	}

	username=strtok(session->req.auth,":");
	password=strtok(NULL,":");
	/* Require a password */
	if(password==NULL)
		password="";
	session->user.number=matchuser(&scfg, username, FALSE);
	if(session->user.number==0) {
		if(scfg.sys_misc&SM_ECHO_PW)
			lprintf("%04d !UNKNOWN USER: %s, Password: %s"
				,session->socket,username,password);
		else
			lprintf("%04d !UNKNOWN USER: %s"
				,session->socket,username);
		return(FALSE);
	}
	getuserdat(&scfg, &session->user);
	if(session->user.pass[0] && stricmp(session->user.pass,password)) {
		/* Should go to the hack log? */
		if(scfg.sys_misc&SM_ECHO_PW)
			lprintf("%04d !PASSWORD FAILURE for user %s: '%s' expected '%s'"
				,session->socket,username,password,session->user.pass);
		else
			lprintf("%04d !PASSWORD FAILURE for user %s"
				,session->socket,username);
		session->user.number=0;
		return(FALSE);
	}
	ar = arstr(NULL,session->req.ars,&scfg);
	authorized=chk_ar(&scfg,ar,&session->user);
	if(ar!=NULL && ar!=nular)
		free(ar);

	if(authorized)  {
		if(session->req.dynamic==IS_CGI)  {
			add_env(session,"AUTH_TYPE","Basic");
			/* Should use real name if set to do so somewhere ToDo */
			add_env(session,"REMOTE_USER",session->user.alias);
		}
		if(session->req.dynamic==IS_SSJS)  {
			if(!js_CreateUserObjects(session->js_cx, session->js_glob, &scfg, &session->user
				,NULL /* ftp index file */, NULL /* subscan */)) 
				lprintf("%04d !JavaScript ERROR creating user objects",session->socket);
		}

		return(TRUE);
	}

	/* Should go to the hack log? */
	lprintf("%04d !AUTHORIZATION FAILURE for user %s, ARS: %s"
		,session->socket,username,session->req.ars);

	return(FALSE);
}

static void b64_decode(uchar *p)
{
	uchar	*read;
	uchar	*write;
	int		bits=0;
	int		working=0;
	char *	i;

	write=p;
	read=write;
	for(;*read;read++) {
		working<<=6;
		i=strchr(base64alphabet,(char)*read);
		if(i==NULL) {
			break;
		}
		if(*i=='=') i=(char*)base64alphabet; /* pad char */
		working |= (i-base64alphabet);
		bits+=6;
		if(bits>8) {
			*(write++)=(uchar)((working&(0xFF<<(bits-8)))>>(bits-8));
			bits-=8;
		}
	}
	*write=0;
	return;
}

static BOOL read_mime_types(char* fname)
{
	char	str[1024];
	char *	ext;
	char *	type;
	FILE*	mime_config;

	if((mime_config=fopen(fname,"r"))==NULL)
		return(FALSE);

	while (!feof(mime_config)&&mime_count<MAX_MIME_TYPES) {
		if(fgets(str,sizeof(str),mime_config)!=NULL) {
			truncsp(str);
			ext=strtok(str," \t");
			if(ext!=NULL) {
				while(*ext && *ext<=' ') ext++;
				if(*ext!=';') {
					type=strtok(NULL," \t");
					if(type!=NULL) {
						while(*type && *type<=' ') type++;
						if(strlen(ext)>0 && strlen(type)>0) {
							SAFECOPY((mime_types[mime_count]).ext,ext);
							SAFECOPY((mime_types[mime_count++]).type,type);
						}
					}
				}
			}
		}
	}
	fclose(mime_config);
	lprintf("Loaded %d mime types",mime_count);
	return(mime_count>0);
}

static int sockreadline(http_session_t * session, char *buf, size_t length)
{
	char	ch;
	DWORD	i;
#if 0
	BOOL	rd;
	time_t	start;

	start=time(NULL);
	for(i=0;TRUE;) {
		if(!socket_check(session->socket,&rd)) {
			session->req.keep_alive=FALSE;
			close_request(session);
			session->socket=INVALID_SOCKET;
			return(-1);
		}

		if(!rd) {
			if(time(NULL)-start>startup->max_inactivity) {
				session->req.keep_alive=FALSE;
				close_request(session);
				session->socket=INVALID_SOCKET;
				return(-1);        /* time-out */
			}
			mswait(1);
			continue;       /* no data */
		}

		if(recv(session->socket, &ch, 1, 0)!=1)
			break;

		if(ch=='\n')
			break;

		if(i<length)
			buf[i++]=ch;
	}
#else
	for(i=0;TRUE;) {
		if(recv(session->socket, &ch, 1, 0)!=1)  {
			session->req.keep_alive=FALSE;
			close_request(session);
			session->socket=INVALID_SOCKET;
			return(-1);        /* time-out */
		}

		if(ch=='\n')
			break;

		if(i<length)
			buf[i++]=ch;
	}
#endif

	/* Terminate at length if longer */
	if(i>length)
		i=length;
		
	if(i>0 && buf[i-1]=='\r')
		buf[--i]=0;
	else
		buf[i]=0;

	if(startup->options&WEB_OPT_DEBUG_RX)
		lprintf("%04d RX: %s",session->socket,buf);
	return(i);
}

static int pipereadline(int pipe, char *buf, size_t length)
{
	char	ch;
	DWORD	i;
	time_t	start;

	start=time(NULL);
	for(i=0;TRUE;) {
		if(time(NULL)-start>startup->max_cgi_inactivity) {
			return(-1);
		}
		
		if(read(pipe, &ch, 1)==1)  {
			start=time(NULL);
			
			if(ch=='\n')
				break;

			if(i<length)
				buf[i++]=ch;
		}
	}

	/* Terminate at length if longer */
	if(i>length)
		i=length;
		
	if(i>0 && buf[i-1]=='\r')
		buf[--i]=0;
	else
		buf[i]=0;

	return(i);
}

int recvbufsocket(int sock, char *buf, long count)
{
	int		rd=0;
	int		i;
	time_t	start;

	if(count<1) {
		errno=ERANGE;
		return(0);
	}

	/* ToDo Timeout here too? */
	while(rd<count && socket_check(sock,NULL))  {
		i=read(sock,buf,count-rd);
		if(i<=0)  {
			*buf=0;
			return(0);
		}

		rd+=i;
		start=time(NULL);
	}

	if(rd==count)  {
		return(rd);
	}

	*buf=0;
	return(0);
}

static void unescape(char *p)
{
	char *	dst;
	char	code[3];
	
	dst=p;
	for(;*p;p++) {
		if(*p=='%' && isxdigit(*(p+1)) && isxdigit(*(p+2))) {
			sprintf(code,"%.2s",p+1);
			*(dst++)=(char)strtol(code,NULL,16);
			p+=2;
		}
		else  {
			if(*p=='+')  {
				*(dst++)=' ';
			}
			else  {
				*(dst++)=*p;
			}
		}
	}
	*(dst)=0;
}

static void js_parse_post(http_session_t * session)  {
	char		*p;
	char		*key;
	char		*value;
	JSString*	js_str;

	if(session->req.post_data != NULL)  {
		p=session->req.post_data;
		while((key=strtok(p,"="))!=NULL)  {
			p=NULL;
			if(key != NULL)  {
				value=strtok(NULL,"&");
				if(value != NULL)  {
					unescape(value);
					unescape(key);
					if((js_str=JS_NewStringCopyZ(session->js_cx, value))==NULL)
						return;
					JS_DefineProperty(session->js_cx, session->js_query, key, STRING_TO_JSVAL(js_str)
						,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
				}
			}
		}
	}
}

static void js_add_header(http_session_t * session, char *key, char *value)  {
	JSString*	js_str;

	if((js_str=JS_NewStringCopyZ(session->js_cx, value))==NULL)
		return;
	JS_DefineProperty(session->js_cx, session->js_header, key, STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
}

static BOOL parse_headers(http_session_t * session)
{
	char	req_line[MAX_REQUEST_LINE];
	char	next_char[2];
	char	*value;
	char	*p;
	int		i;
	size_t	content_len=0;
	char	env_name[128];

	while(sockreadline(session,req_line,sizeof(req_line))>0) {
		/* Multi-line headers */
		while((recvfrom(session->socket,next_char,1,MSG_PEEK,NULL,0)>0) 
			&& (next_char[0]=='\t' || next_char[0]==' ')) {
			i=strlen(req_line);
			sockreadline(session,req_line+i,sizeof(req_line)-i);
		}
		strtok(req_line,":");
		if((value=strtok(NULL,""))!=NULL) {
			i=get_header_type(req_line);
			while(*value && *value<=' ') value++;
			if(session->req.dynamic==IS_SSJS)
				js_add_header(session,req_line,value);
			switch(i) {
				case HEAD_AUTH:
					strtok(value," ");
					p=strtok(NULL," ");
					if(p==NULL)
						break;
					while(*p && *p<' ') p++;
					b64_decode(p);
					SAFECOPY(session->req.auth,p);
					break;
				case HEAD_LENGTH:
					if(session->req.dynamic==IS_CGI)
						add_env(session,"CONTENT_LENGTH",value);
					content_len=atoi(value);
					break;
				case HEAD_TYPE:
					if(session->req.dynamic==IS_CGI)
						add_env(session,"CONTENT_TYPE",value);
					break;
				case HEAD_IFMODIFIED:
					session->req.if_modified_since=decode_date(value);
					break;
				case HEAD_CONNECTION:
					if(!stricmp(value,"Keep-Alive")) {
						session->req.keep_alive=TRUE;
					}
					if(!stricmp(value,"Close")) {
						session->req.keep_alive=FALSE;
					}
					break;
				case HEAD_HOST:
					if(session->req.host[0]==0) {
						SAFECOPY(session->req.host,value);
						if(startup->options&WEB_OPT_DEBUG_RX)
							lprintf("%04d Grabbing from virtual host: %s"
								,session->socket,value);
					}
					break;
				default:
					break;
			}
			if(session->req.dynamic==IS_CGI)  {
				sprintf(env_name,"HTTP_%s",req_line);
				add_env(session,env_name,value);
			}
		}
	}
	if(content_len)  {
		if((session->req.post_data=malloc(content_len+1)) != NULL)  {
			recvbufsocket(session->socket,session->req.post_data,content_len);
			session->req.post_len=content_len;
			if(session->req.dynamic==IS_SSJS)  {
				js_parse_post(session);
			}
		}
		else  {
			lprintf("%04d !ERROR Allocating %d bytes of memory",session->socket,content_len);
			return(FALSE);
		}
	}
	if(session->req.dynamic==IS_CGI)
		add_env(session,"SERVER_NAME",session->req.host[0] ? session->req.host : startup->host_name );
	return TRUE;
}

static int get_version(char *p)
{
	int		i;
	if(p==NULL)
		return(0);
	while(*p && *p<' ') p++;
	if(*p==0)
		return(0);
	for(i=1;http_vers[i]!=NULL;i++) {
		if(!stricmp(p,http_vers[i])) {
			return(i);
		}
	}
	return(i-1);
}

static void js_parse_query(http_session_t * session, char *p)  {
	char	*key;
	char	*value;
	JSString*	js_str;
	
	while((key=strtok(p,"="))!=NULL)  {
		p=NULL;
		if(key != NULL)  {
			value=strtok(NULL,"&");
			if(value != NULL)  {
				unescape(value);
				unescape(key);
				if((js_str=JS_NewStringCopyZ(session->js_cx, value))==NULL)
					return;
				JS_DefineProperty(session->js_cx, session->js_query, key, STRING_TO_JSVAL(js_str)
					,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
			}
		}
	}
}

static int is_dynamic(http_session_t * session,char *req)  {
	char*	p;
	int		i=0;
	char	path[MAX_PATH+1];

	if((p=strrchr(req,'.'))==NULL)  {
		return(IS_STATIC);
	}

	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT) && stricmp(p,startup->ssjs_ext)==0)  {
		lprintf("Setting up JavaScript support");
	
		if(!js_setup(session)) {
			lprintf("%04d !ERROR setting up JavaScript support", session->socket);
			send_error(session,"500 Internal Server Error");
			return(IS_STATIC);
		}
	
		sprintf(path,"%s/SBBS_SSJS.%d.html",startup->cgi_temp_dir,session->socket);
		if((session->req.fp=fopen(path,"w"))==NULL) {
			lprintf("%04d !ERROR %d opening/creating %s", session->socket, errno, path);
			send_error(session,"500 Internal Server Error");
			return(IS_STATIC);
		}
		return(IS_SSJS);
	}

	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT) && stricmp(p,startup->js_ext)==0)
		return(IS_JS);

	if(!(startup->options&WEB_OPT_NO_CGI)) {
		for(i=0;i<10&&startup->cgi_ext[i][0];i++)  {
			if(!(startup->options&BBS_OPT_NO_JAVASCRIPT) && stricmp(p,startup->cgi_ext[i])==0)  {
				init_enviro(session);
				return(IS_CGI);
			}
		}
	}

	return(IS_STATIC);
}

static char *get_request(http_session_t * session, char *req_line)
{
	char *	p;
	char *  retval;
	int		offset;

	while(*req_line && *req_line<' ') req_line++;
	SAFECOPY(session->req.virtual_path,req_line);
	strtok(session->req.virtual_path," \t");
	retval=strtok(NULL," \t");
	strtok(session->req.virtual_path,"?");
	p=strtok(NULL,"");
	session->req.dynamic=is_dynamic(session,session->req.virtual_path);
	if(session->req.dynamic)
		SAFECOPY(session->req.query_str,p);
	if(p!=NULL)  {
		switch(session->req.dynamic) {
			case IS_CGI:
				add_env(session,"QUERY_STRING",p);
				break;
			case IS_SSJS:
				js_parse_query(session,p);
				break;
		}
	}
	
	unescape(session->req.virtual_path);
	SAFECOPY(session->req.physical_path,session->req.virtual_path);
	if(!strnicmp(session->req.physical_path,http_scheme,http_scheme_len)) {
		/* Set HOST value... ignore HOST header */
		SAFECOPY(session->req.host,session->req.physical_path+http_scheme_len);
		strtok(session->req.physical_path,"/");
		p=strtok(NULL,"/");
		if(p==NULL) {
			/* Do not allow host values larger than 128 bytes */
			session->req.host[0]=0;
			p=session->req.physical_path+http_scheme_len;
		}
		offset=p-session->req.physical_path;
		memmove(session->req.physical_path
			,session->req.physical_path+offset
			,strlen(session->req.physical_path+offset)+1	/* move '\0' terminator too */
			);
	}
	return(retval);
}

static char *get_method(http_session_t * session, char *req_line)
{
	int i;

	for(i=0;methods[i]!=NULL;i++) {
		if(!strnicmp(req_line,methods[i],strlen(methods[i]))) {
			session->req.method=i;
			if(strlen(req_line)<strlen(methods[i])+2) {
				send_error(session,"400 Bad Request");
				return(NULL);
			}
			return(req_line+strlen(methods[i])+1);
		}
	}
	if(req_line!=NULL && *req_line>=' ')
		send_error(session,"501 Not Implemented");
	return(NULL);
}

static BOOL get_req(http_session_t * session)
{
	char	req_line[MAX_REQUEST_LINE];
	char *	p;
	
	if(sockreadline(session,req_line,sizeof(req_line))>0) {
		if(startup->options&WEB_OPT_DEBUG_RX)
			lprintf("%04d Got request line: %s",session->socket,req_line);
		p=NULL;
		p=get_method(session,req_line);
		if(p!=NULL) {
			p=get_request(session,p);
			session->http_ver=get_version(p);
			if(session->http_ver>=HTTP_1_1)
				session->req.keep_alive=TRUE;
			if(session->req.dynamic==IS_CGI)  {
				add_env(session,"REQUEST_METHOD",methods[session->req.method]);
				add_env(session,"SERVER_PROTOCOL",session->http_ver ? 
					http_vers[session->http_ver] : "HTTP/0.9");
			}
			return(TRUE);
		}
	}
	session->req.keep_alive=FALSE;
	close_request(session);
	return FALSE;
}

static BOOL check_request(http_session_t * session)
{
	char	path[MAX_PATH+1];
	char	str[MAX_PATH+1];
	char	last_ch;
	char*	last_slash;
	FILE*	file;
	int		i;
	
	if(!(startup->options&WEB_OPT_VIRTUAL_HOSTS))
		session->req.host[0]=0;
	if(session->req.host[0]) {
		sprintf(str,"%s/%s",root_dir,session->req.host);
		if(isdir(str))
			sprintf(str,"%s/%s%s",root_dir,session->req.host,session->req.physical_path);
	} else
		sprintf(str,"%s%s",root_dir,session->req.physical_path);
	
	if(cleanpath(path,str,sizeof(session->req.physical_path))==NULL) {
		send_error(session,"404 Not Found");
		return(FALSE);
	}
	if(!fexist(path)) {
		last_ch=*lastchar(path);
		if(last_ch!='/' && last_ch!='\\')  {
			strcat(path,"/");
		}
		last_ch=*lastchar(session->req.virtual_path);
		if(last_ch!='/' && last_ch!='\\')  {
			strcat(session->req.virtual_path,"/");
		}
		last_slash=strrchr(path,'/');
		last_slash++;
		for(i=0;i<4 && startup->index_file_name[i][0];i++)  {
			*last_slash=0;
			strcat(path,startup->index_file_name[i]);
			if(fexist(path))
				break;
		}
		strcat(session->req.virtual_path,startup->index_file_name[i]);
		session->req.send_location=MOVED_PERM;
	}
	if(strnicmp(path,root_dir,strlen(root_dir))) {
		session->req.keep_alive=FALSE;
		send_error(session,"400 Bad Request");
		return(FALSE);
	}
	if(!fexist(path)) {
		send_error(session,"404 Not Found");
		return(FALSE);
	}
	SAFECOPY(session->req.physical_path,path);
	if(session->req.dynamic==IS_CGI)  {
		add_env(session,"PATH_TRANSLATED",path);
		add_env(session,"SCRIPT_NAME",session->req.virtual_path);
	}
	SAFECOPY(str,session->req.virtual_path);
	last_slash=strrchr(str,'/');
	if(last_slash!=NULL)
		*(last_slash+1)=0;
	if(session->req.dynamic==IS_CGI)
		add_env(session,"PATH_INFO",str);

	/* Set default ARS to a 0-length string */
	session->req.ars[0]=0;
	/* Walk up from root_dir checking for access.ars */
	SAFECOPY(str,path);
	last_slash=str+strlen(root_dir)-1;
	/* Loop while there's more /s in path*/
	while((last_slash=strchr(last_slash+1,'/'))!=NULL) {
		/* Terminate the path after the slash */
		*(last_slash+1)=0;
		strcat(str,"access.ars");
		if(fexist(str)) {
			if(!strcmp(path,str)) {
				send_error(session,"403 Forbidden");
				return(FALSE);
			}
			/* Read access.ars file */
			if((file=fopen(str,"r"))!=NULL) {
				fgets(session->req.ars,sizeof(session->req.ars),file);
				fclose(file);
			}
			else  {
				/* If cannot open access.ars, only allow sysop access */
				SAFECOPY(session->req.ars,"LEVEL 90");
				break;
			}
			/* Truncate at \r or \n - can use last_slash since I'm done with it.*/
			truncsp(session->req.ars);
		}
		SAFECOPY(str,path);
	}
	
	if(session->req.ars[0] && !(check_ars(session))) {
		/* No authentication provided */
		sprintf(str,"401 Unauthorized%s%s: Basic realm=\"%s\""
			,newline,get_header(HEAD_WWWAUTH),scfg.sys_name);
		send_error(session,str);
		return(FALSE);
	}
	return(TRUE);
}

static BOOL exec_cgi(http_session_t *session)  
{
	char	cmdline[MAX_PATH+256];
#ifdef __unix__
	/* ToDo: Damn, that's WAY too many variables */
	int		i=0;
	int		status=0;
	pid_t	child=0;
	int		in_pipe[2];
	int		out_pipe[2];
	int		err_pipe[2];
	struct timeval tv={0,0};
	fd_set	read_set;
	fd_set	write_set;
	int		high_fd=0;
	char	buf[MAX_REQUEST_LINE+1];
	size_t	post_offset=0;
	BOOL	done_parsing_headers=FALSE;
	BOOL	done_reading=FALSE;
	char	cgi_status[MAX_REQUEST_LINE+1];
	char	header[MAX_REQUEST_LINE+1];
	char	*directive=NULL;
	char	*value=NULL;
	BOOL	done_wait=FALSE;
	BOOL	got_valid_headers=FALSE;
	time_t	start;
#endif

	SAFECOPY(cmdline,session->req.physical_path);
	
#ifdef __unix__

	lprintf("Executing %s",cmdline);

	/* ToDo: Should only do this if the Content-Length header was NOT sent */
	session->req.keep_alive=FALSE;

	/* Set up I/O pipes */
	if(pipe(in_pipe)!=0) {
		lprintf("%04d Can't create in_pipe",session->socket,buf);
		return(FALSE);
	}
    fcntl(in_pipe[0],F_SETFL,fcntl(in_pipe[0],F_GETFL)|O_NONBLOCK);
    fcntl(in_pipe[1],F_SETFL,fcntl(in_pipe[1],F_GETFL)|O_NONBLOCK);

	if(pipe(out_pipe)!=0) {
		lprintf("%04d Can't create out_pipe",session->socket,buf);
		return(FALSE);
	}
    fcntl(out_pipe[0],F_SETFL,fcntl(out_pipe[0],F_GETFL)|O_NONBLOCK);
    fcntl(out_pipe[1],F_SETFL,fcntl(out_pipe[1],F_GETFL)|O_NONBLOCK);


	if(pipe(err_pipe)!=0) {
		lprintf("%04d Can't create err_pipe",session->socket,buf);
		return(FALSE);
	}
    fcntl(err_pipe[0],F_SETFL,fcntl(err_pipe[0],F_GETFL)|O_NONBLOCK);
    fcntl(err_pipe[1],F_SETFL,fcntl(err_pipe[1],F_GETFL)|O_NONBLOCK);

	if((child=fork())==0)  {
		/* Set up environment */
		while(session->req.cgi_env != NULL)  {
			putenv(session->req.cgi_env->val);
			session->req.cgi_env=session->req.cgi_env->next;
		}
		
		/* Set up STDIO */
		close(in_pipe[1]);		/* close write-end of pipe */
		dup2(in_pipe[0],0);		/* redirect stdin */
		close(in_pipe[0]);		/* close excess file descriptor */
		close(out_pipe[0]);		/* close read-end of pipe */
		dup2(out_pipe[1],1);	/* stdout */
		close(out_pipe[1]);		/* close excess file descriptor */
		close(err_pipe[0]);		/* close read-end of pipe */
		dup2(err_pipe[1],2);	/* stderr */
		close(err_pipe[1]);		/* close excess file descriptor */

		/* Execute command */
		execl(cmdline,cmdline,NULL);
		lprintf("FAILED! execl()");
		exit(EXIT_FAILURE); /* Should never happen */
	}
	close(in_pipe[0]);		/* close excess file descriptor */
	close(out_pipe[1]);		/* close excess file descriptor */
	close(err_pipe[1]);		/* close excess file descriptor */

	start=time(NULL);

	if(child==0)  {
		close(in_pipe[1]);		/* close write-end of pipe */
		close(out_pipe[0]);		/* close read-end of pipe */
		close(err_pipe[0]);		/* close read-end of pipe */
		return(FALSE);
	}

	post_offset+=write(in_pipe[1],
		session->req.post_data+post_offset,
		session->req.post_len-post_offset);

	high_fd=out_pipe[0];
	if(err_pipe[0]>high_fd)
		high_fd=err_pipe[0];
	if(in_pipe[1]>high_fd)
		high_fd=in_pipe[1];
	if(session->socket>high_fd)
		high_fd=session->socket;

	/* ToDo: Magically set done_parsing_headers for nph-* scripts */
	cgi_status[0]=0;
	while(!done_reading)  {
		if(!done_wait)
			done_wait = (waitpid(child,&status,WNOHANG)==child);

		tv.tv_sec=startup->max_cgi_inactivity;
		tv.tv_usec=0;

		FD_ZERO(&read_set);
		FD_SET(out_pipe[0],&read_set);
		FD_SET(err_pipe[0],&read_set);
		FD_SET(session->socket,&read_set);
		FD_ZERO(&write_set);
		if(post_offset < session->req.post_len)
			FD_SET(in_pipe[1],&write_set);

		if(!done_reading && (select(high_fd+1,&read_set,&write_set,NULL,&tv)>0))  {
			if(FD_ISSET(session->socket,&read_set))  {
				done_reading=TRUE;
			}
			if(FD_ISSET(in_pipe[1],&write_set))  {
				if(post_offset < session->req.post_len)  {
					i=post_offset;
					post_offset+=write(in_pipe[1],
						session->req.post_data+post_offset,
						session->req.post_len-post_offset);
					if(post_offset>=session->req.post_len)
						close(in_pipe[1]);
					if(i!=post_offset)  {
						start=time(NULL);
					}
					if(i<0)  {
						done_reading=TRUE;
					}
				}
			}
			if(FD_ISSET(out_pipe[0],&read_set))  {
				if(done_parsing_headers && got_valid_headers)  {
					i=read(out_pipe[0],buf,MAX_REQUEST_LINE);
					if(i>0)  {
						start=time(NULL);
						write(session->socket,buf,i);
					}
					if(i<=0)  {
						done_reading=TRUE;
					}
				}
				else  {
					/* This is the tricky part */
					i=pipereadline(out_pipe[0],buf,MAX_REQUEST_LINE);
					if(i<0)  {
						done_reading=TRUE;
						got_valid_headers=FALSE;
					}
					if(i>0)  {
						start=time(NULL);
					}

					if(!done_parsing_headers && *buf)  {
						SAFECOPY(header,buf);
						directive=strtok(header,":");
						if(directive != NULL)  {
							value=strtok(NULL,"");
							i=get_header_type(directive);
							switch (i)  {
								case HEAD_LOCATION:
									got_valid_headers=TRUE;
									if(*value=='/')  {
										unescape(value);
										SAFECOPY(session->req.virtual_path,value);
										session->req.send_location=MOVED_STAT;
										if(cgi_status[0]==0)
											SAFECOPY(cgi_status,"302 Moved Temporarily");
									} else  {
										SAFECOPY(session->req.virtual_path,value);
										session->req.send_location=MOVED_STAT;
										if(cgi_status[0]==0)
											SAFECOPY(cgi_status,"302 Moved Temporarily");
									}
									break;
								case HEAD_STATUS:
									SAFECOPY(cgi_status,value);
									break;
								case HEAD_TYPE:
									got_valid_headers=TRUE;
								default:
									session->req.dynamic_heads=add_list(session->req.dynamic_heads,buf);
							}
						}
					}
					else  {
						if(got_valid_headers)  {
							session->req.dynamic=IS_CGI;
							if(cgi_status[0]==0)
								SAFECOPY(cgi_status,"200 OK");
							send_headers(session,cgi_status);
						}
						done_parsing_headers=TRUE;
					}
				}
			}
			if(FD_ISSET(err_pipe[0],&read_set))  {
				i=read(err_pipe[0],buf,MAX_REQUEST_LINE);
				buf[i]=0;
				if(i>0)  {
					start=time(NULL);
				}
				if(i<0)  {
					done_reading=TRUE;
				}
			}
		}
		else  {
			if(!done_wait)
				done_wait = (waitpid(child,&status,WNOHANG)==child);

			if((time(NULL)-start) >= startup->max_cgi_inactivity)  {
				/* timeout */
				lprintf("%04d CGI Script %s Timed out",session->socket,cmdline);
				if(!done_wait)  {
					kill(child,SIGTERM);
					mswait(1000);
					done_wait = (waitpid(child,&status,WNOHANG)==child);
					if(!done_wait)  {
						kill(child,SIGKILL);
						done_wait = (waitpid(child,&status,0)==child);
					}
				}
			}
			if(done_wait)
				done_reading=TRUE;
		}
	}
	if(!done_wait)  {
		lprintf("%04d CGI Script %s still alive on client exit",session->socket,cmdline);
		kill(child,SIGTERM);
		mswait(1000);
		done_wait = (waitpid(child,&status,WNOHANG)==child);
		if(!done_wait)  {
			kill(child,SIGKILL);
			done_wait = (waitpid(child,&status,0)==child);
		}
	}
			
	close(in_pipe[1]);		/* close write-end of pipe */
	close(out_pipe[0]);		/* close read-end of pipe */
	close(err_pipe[0]);		/* close read-end of pipe */
	if(!done_parsing_headers || !got_valid_headers)
		return(FALSE);
	return(TRUE);
#else
	/* Win32 exec_cgi() */
	return(FALSE);
#endif
}

/********************/
/* JavaScript stuff */
/********************/

JSObject* DLLCALL js_CreateHttpReplyObject(JSContext* cx, JSObject* parent, http_session_t *session)
{
	JSObject*	reply;
	JSObject*	headers;
	jsval		val;
	JSString*	js_str;
	
	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,parent,"http_reply",&val) && val!=JSVAL_VOID)  {
		reply = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx,reply);
	}
	else
		reply = JS_DefineObject(cx, parent, "http_reply", NULL
									, NULL, JSPROP_ENUMERATE);

	if((js_str=JS_NewStringCopyZ(cx, "200 OK"))==NULL)
		return(FALSE);
	JS_DefineProperty(cx, reply, "status", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE);
		
	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,reply,"header",&val) && val!=JSVAL_VOID)  {
		headers = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx,headers);
	}
	else
		headers = JS_DefineObject(cx, reply, "header", NULL
									, NULL, JSPROP_ENUMERATE);

	if((js_str=JS_NewStringCopyZ(cx, "text/html"))==NULL)
		return(FALSE);
	JS_DefineProperty(cx, headers, "Content-Type", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE);

	return(reply);
}

JSObject* DLLCALL js_CreateHttpRequestObject(JSContext* cx, JSObject* parent, http_session_t *session)
{
	JSObject*	request;
	JSObject*	query;
/*	JSObject*	cookie; */
	JSObject*	headers;
	JSString*	js_str;
	jsval		val;

	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,parent,"http_request",&val) && val!=JSVAL_VOID)  {
		request = JSVAL_TO_OBJECT(val);
	}
	else
		request = JS_DefineObject(cx, parent, "http_request", NULL
									, NULL, JSPROP_ENUMERATE);

	if((js_str=JS_NewStringCopyZ(session->js_cx, methods[session->req.method]))==NULL)
		return(FALSE);
	JS_DefineProperty(session->js_cx, request, "method", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(session->js_cx, session->req.virtual_path))==NULL)
		return(FALSE);
	JS_DefineProperty(session->js_cx, request, "virtual_path", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,request,"query",&val) && val!=JSVAL_VOID)  {
		query = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx,query);
	}
	else
		query = JS_DefineObject(cx, request, "query", NULL
									, NULL, JSPROP_ENUMERATE);

	
	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,request,"header",&val) && val!=JSVAL_VOID)  {
		headers = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx,headers);
	}
	else
		headers = JS_DefineObject(cx, request, "header", NULL
									, NULL, JSPROP_ENUMERATE);

	session->js_query=query;
	session->js_header=headers;
	session->js_request=request;
	return(request);
}

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char	line[64];
	char	file[MAX_PATH+1];
	char*	warning;
	http_session_t* session;

	if((session=(http_session_t*)JS_GetContextPrivate(cx))==NULL)
		return;
	
	if(report==NULL) {
		lprintf("!JavaScript: %s", message);
		if(session->req.fp!=NULL)
			fprintf(session->req.fp,"!JavaScript: %s", message);
		return;
    }

	if(report->filename)
		sprintf(file," %s",report->filename);
	else
		file[0]=0;

	if(report->lineno)
		sprintf(line," line %u",report->lineno);
	else
		line[0]=0;

	if(JSREPORT_IS_WARNING(report->flags)) {
		if(JSREPORT_IS_STRICT(report->flags))
			warning="strict warning";
		else
			warning="warning";
	} else
		warning="";

	lprintf("!JavaScript %s%s%s: %s",warning,file,line,message);
	if(session->req.fp!=NULL)
		fprintf(session->req.fp,"!JavaScript %s%s%s: %s",warning,file,line,message);
}

static JSBool
js_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i;
    JSString *	str;
	http_session_t* session;

	if((session=(http_session_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(session->req.fp==NULL)
		return(JS_FALSE);

    for(i=0; i<argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
			continue;
		fprintf(session->req.fp,"%s",JS_GetStringBytes(str));
	}

	return(JS_TRUE);
}

static JSBool
js_writeln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i;
    JSString *	str;
	http_session_t* session;

	if((session=(http_session_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(session->req.fp==NULL)
		return(JS_FALSE);

    for (i=0; i<argc;i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
			continue;
		fprintf(session->req.fp,"%s",JS_GetStringBytes(str));
	}

	fprintf(session->req.fp,"\n");

	return(JS_TRUE);
}

static JSFunctionSpec js_global_functions[] = {
	{"write",           js_write,           1},		/* write to HTML file */
	{"writeln",         js_writeln,         1},		/* write line to HTML file */
	{0}
};

static JSContext* 
js_initcx(JSRuntime* runtime, SOCKET sock, JSObject** glob, http_session_t *session)
{
	char		ver[256];
	JSContext*	js_cx;
	JSObject*	js_glob;
	JSObject*	server;
	JSString*	js_str;
	jsval		val;
	BOOL		success=FALSE;

	lprintf("%04d JavaScript: Initializing context (stack: %lu bytes)"
		,sock,JAVASCRIPT_CONTEXT_STACK);

    if((js_cx = JS_NewContext(runtime, JAVASCRIPT_CONTEXT_STACK))==NULL)
		return(NULL);

	lprintf("%04d JavaScript: Context created",sock);

    JS_SetErrorReporter(js_cx, js_ErrorReporter);

	do {

		lprintf("%04d JavaScript: Initializing Global object",sock);
		if((js_glob=js_CreateGlobalObject(js_cx, &scfg))==NULL) 
			break;

		if (!JS_DefineFunctions(js_cx, js_glob, js_global_functions)) 
			break;

		lprintf("%04d JavaScript: Initializing System object",sock);
		if(js_CreateSystemObject(js_cx, js_glob, &scfg, uptime, startup->host_name)==NULL) 
			break;

		if((server=JS_DefineObject(js_cx, js_glob, "server", NULL,NULL,0))==NULL)
			break;

		sprintf(ver,"%s %s",server_name,revision);
		if((js_str=JS_NewStringCopyZ(js_cx, ver))==NULL)
			break;
		val = STRING_TO_JSVAL(js_str);
		if(!JS_SetProperty(js_cx, server, "version", &val))
			break;

		if((js_str=JS_NewStringCopyZ(js_cx, web_ver()))==NULL)
			break;
		val = STRING_TO_JSVAL(js_str);
		if(!JS_SetProperty(js_cx, server, "version_detail", &val))
			break;

		if(glob!=NULL)
			*glob=js_glob;

		success=TRUE;

	} while(0);

	if(!success) {
		JS_DestroyContext(js_cx);
		session->js_cx=NULL;
		return(NULL);
	}

	return(js_cx);
}

static BOOL js_setup(http_session_t* session)
{
	JSObject*	argv;

	if(session->js_runtime == NULL) {
		lprintf("%04d JavaScript: Creating runtime: %lu bytes"
			,session->socket,startup->js_max_bytes);

		if((session->js_runtime=JS_NewRuntime(startup->js_max_bytes))==NULL) {
			lprintf("%04d !ERROR creating JavaScript runtime",session->socket);
			send_error(session,"500 Error creating JavaScript runtime");
			return(FALSE);
		}
	}

	if(session->js_cx==NULL) {	/* Context not yet created, create it now */
		if(((session->js_cx=js_initcx(session->js_runtime, session->socket
			,&session->js_glob, session))==NULL)) {
			lprintf("%04d !ERROR initializing JavaScript context",session->socket);
			send_error(session,"500 Error initializing JavaScript context");
			return(FALSE);
		}
		if(js_CreateUserClass(session->js_cx, session->js_glob, &scfg)==NULL) 
			lprintf("%04d !JavaScript ERROR creating user class",session->socket);

		if(js_CreateFileClass(session->js_cx, session->js_glob)==NULL) 
			lprintf("%04d !JavaScript ERROR creating File class",session->socket);

		if(js_CreateSocketClass(session->js_cx, session->js_glob)==NULL)
			lprintf("%04d !JavaScript ERROR creating Socket class",session->socket);

		if(js_CreateMsgBaseClass(session->js_cx, session->js_glob, &scfg)==NULL)
			lprintf("%04d !JavaScript ERROR creating MsgBase class",session->socket);

#if 0
		if(js_CreateClientObject(session->js_cx, session->js_glob, "client", &client
			,session->socket)==NULL) 
			lprintf("%04d !JavaScript ERROR creating client object",session->socket);
#endif

		argv=JS_NewArrayObject(session->js_cx, 0, NULL);

		JS_DefineProperty(session->js_cx, session->js_glob, "argv", OBJECT_TO_JSVAL(argv)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
		JS_DefineProperty(session->js_cx, session->js_glob, "argc", INT_TO_JSVAL(0)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
	}

	lprintf("     JavaScript: Initializing HttpRequest object");
	if(js_CreateHttpRequestObject(session->js_cx, session->js_glob, session)==NULL) {
		lprintf("%04d !ERROR initializing JavaScript HttpRequest object",session->socket);
		send_error(session,"500 Error initializing JavaScript HttpRequest object");
		return(FALSE);
	}

	lprintf("     JavaScript: Initializing HttpReply object");
	if(js_CreateHttpReplyObject(session->js_cx, session->js_glob, session)==NULL) {
		lprintf("%04d !ERROR initializing JavaScript HttpReply object",session->socket);
		send_error(session,"500 Error initializing JavaScript HttpReply object");
		return(FALSE);
	}

	JS_SetContextPrivate(session->js_cx, session);

	return(TRUE);
}

static BOOL exec_ssjs(http_session_t* session)  {
	JSString*	js_str;
	JSScript*	js_script;
	jsval		rval;
	jsval		val;
	JSObject*	reply;
	JSIdArray*	heads;
	JSObject*	headers;
	char		path[MAX_PATH+1];
	char		str[MAX_REQUEST_LINE+1];
	int			i;

	if((js_str=JS_NewStringCopyZ(session->js_cx, session->req.physical_path))==NULL)
		return(FALSE);
	JS_DefineProperty(session->js_cx, session->js_request, "real_path", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(session->js_cx, session->req.ars))==NULL)
		return(FALSE);
	JS_DefineProperty(session->js_cx, session->js_request, "ars", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(session->js_cx, session->req.host))==NULL)
		return(FALSE);
	JS_DefineProperty(session->js_cx, session->js_request, "host", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(session->js_cx, http_vers[session->http_ver]))==NULL)
		return(FALSE);
	JS_DefineProperty(session->js_cx, session->js_request, "http_ver", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(session->js_cx, session->host_ip))==NULL)
		return(FALSE);
	JS_DefineProperty(session->js_cx, session->js_request, "remote_ip", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(session->js_cx, session->host_name))==NULL)
		return(FALSE);
	JS_DefineProperty(session->js_cx, session->js_request, "remote_host", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	do {
		/* RUN SCRIPT */
		JS_ClearPendingException(session->js_cx);



		if((js_script=JS_CompileFile(session->js_cx, session->js_glob
			,session->req.physical_path))==NULL) {
			lprintf("%04d !JavaScript FAILED to compile script (%s)"
				,session->socket,session->req.physical_path);
			return(FALSE);
		}

		JS_ExecuteScript(session->js_cx, session->js_glob, js_script, &rval);

	} while(0);

	/* Read http_reply object */
	JS_GetProperty(session->js_cx,session->js_glob,"http_reply",&val);
	reply = JSVAL_TO_OBJECT(val);
	JS_GetProperty(session->js_cx,reply,"status",&val);
	SAFECOPY(session->req.status,JS_GetStringBytes(JSVAL_TO_STRING(val)));
	JS_GetProperty(session->js_cx,reply,"header",&val);
	headers = JSVAL_TO_OBJECT(val);
	heads=JS_Enumerate(session->js_cx,headers);
	for(i=0;i<heads->length;i++)  {
		JS_IdToValue(session->js_cx,heads->vector[i],&val);
		js_str=JSVAL_TO_STRING(val);
		JS_GetProperty(session->js_cx,headers,JS_GetStringBytes(js_str),&val);
		snprintf(str,MAX_REQUEST_LINE+1,"%s: %s",JS_GetStringBytes(js_str),JS_GetStringBytes(JSVAL_TO_STRING(val)));
		session->req.dynamic_heads=add_list(session->req.dynamic_heads,str);
	}
	JS_DestroyIdArray(session->js_cx, heads);

	/* Free up temporary resources here */

	if(js_script!=NULL) 
		JS_DestroyScript(session->js_cx, js_script);
	if(session->req.fp!=NULL)
		fclose(session->req.fp);

	SAFECOPY(session->req.physical_path, path);
	session->req.dynamic=IS_SSJS;
	
	return(TRUE);
}

static void respond(http_session_t * session)
{
	BOOL		send_file=TRUE;

	if(session->req.dynamic==IS_CGI)  {
		if(!exec_cgi(session))  {
			send_error(session,"500 Internal Server Error");
			return;
		}
		close_request(session);
		return;
	}

	if(session->req.dynamic==IS_SSJS) {	/* Server-Side JavaScript */
		if(!exec_ssjs(session))  {
			send_error(session,"500 Internal Server Error");
			return;
		}
		sprintf(session->req.physical_path,"%s/SBBS_SSJS.%d.html",startup->cgi_temp_dir,session->socket);
	}

	if(session->http_ver > HTTP_0_9)  {
		session->req.mime_type=get_mime_type(strrchr(session->req.physical_path,'.'));
		send_file=send_headers(session,session->req.status);
	}
	if(send_file)  {
		lprintf("%04d Sending file",session->socket);
		sock_sendfile(session->socket,session->req.physical_path);
		lprintf("%04d Sent file",session->socket);
	}
	close_request(session);
}

void http_session_thread(void* arg)
{
	int				i;
	char*			host_name;
	HOSTENT*		host;
	SOCKET			socket;
	http_session_t	session=*(http_session_t*)arg;	/* copies arg BEFORE it's freed */

	free(arg);	

	socket=session.socket;
	lprintf("%04d Session thread started", session.socket);

	thread_up(FALSE /* setuid */);
	session.finished=FALSE;

	if(startup->options&BBS_OPT_NO_HOST_LOOKUP)
		host=NULL;
	else
		host=gethostbyaddr ((char *)&session.addr.sin_addr
			,sizeof(session.addr.sin_addr),AF_INET);

	if(host!=NULL && host->h_name!=NULL)
		host_name=host->h_name;
	else
		host_name="<no name>";

	if(!(startup->options&BBS_OPT_NO_HOST_LOOKUP))  {
		lprintf("%04d Hostname: %s", session.socket, host_name);
		SAFECOPY(session.host_name,host_name);
		for(i=0;host!=NULL && host->h_aliases!=NULL 
			&& host->h_aliases[i]!=NULL;i++)
			lprintf("%04d HostAlias: %s", session.socket, host->h_aliases[i]);
		if(trashcan(&scfg,host_name,"host")) {
			close_socket(session.socket);
			lprintf("%04d !CLIENT BLOCKED in host.can: %s", session.socket, host_name);
			thread_down();
			lprintf("%04d Free()ing session",socket);
			return;
		}

	}

	/* host_ip wasn't defined in http_session_thread */
	if(trashcan(&scfg,session.host_ip,"ip")) {
		close_socket(session.socket);
		lprintf("%04d !CLIENT BLOCKED in ip.can: %s", session.socket, session.host_ip);
		thread_down();
		lprintf("%04d Free()ing session",socket);
		return;
	}

	active_clients++;
	update_clients();

	while(!session.finished && server_socket!=INVALID_SOCKET) {
	    memset(&(session.req), 0, sizeof(session.req));
		SAFECOPY(session.req.status,"200 OK");
		if(get_req(&session)) {
			if((session.http_ver<HTTP_1_0)||parse_headers(&session)) {
				if(check_request(&session)) {
					respond(&session);
				}
			}
		}
	}

	if(session.js_cx!=NULL) {
		lprintf("%04d JavaScript: Destroying context",socket);
		JS_DestroyContext(session.js_cx);	/* Free Context */
		session.js_cx=NULL;
	}

	if(session.js_runtime!=NULL) {
		lprintf("%04d JavaScript: Destroying runtime",socket);
		JS_DestroyRuntime(session.js_runtime);
	}

	active_clients--;
	update_clients();
	/* client_off(session.socket); */

	thread_down();
	lprintf("%04d Session thread terminated (%u clients, %u threads remain, %lu served)"
		,socket, active_clients, thread_count, served);

}

void DLLCALL web_terminate(void)
{
	recycle_server=FALSE;
	if(server_socket!=INVALID_SOCKET) {
    	lprintf("%04d Web Terminate: closing socket",server_socket);
		close_socket(server_socket);
		server_socket=INVALID_SOCKET;
    }
}

static void cleanup(int code)
{
	free_cfg(&scfg);

	if(server_socket!=INVALID_SOCKET) {
		close_socket(server_socket);
		server_socket=INVALID_SOCKET;
	}

	update_clients();

#ifdef _WINSOCKAPI_
	if(WSAInitialized && WSACleanup()!=0) 
		lprintf("0000 !WSACleanup ERROR %d",ERROR_VALUE);
#endif

	thread_down();
	status("Down");
    lprintf("#### Web Server thread terminated (%u threads remain, %lu clients served)"
		,thread_count, served);
	if(startup!=NULL && startup->terminated!=NULL)
		startup->terminated(code);
}

const char* DLLCALL web_ver(void)
{
	static char ver[256];
	char compiler[32];

	DESCRIBE_COMPILER(compiler);

	sscanf("$Revision$", "%*s %s", revision);

	sprintf(ver,"%s %s%s  "
		"Compiled %s %s with %s"
		,server_name
		,revision
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		,__DATE__, __TIME__, compiler);

	return(ver);
}

void DLLCALL web_server(void* arg)
{
	int				i;
	int				result;
	time_t			start;
	WORD			host_port;
	char			host_ip[32];
	char			path[MAX_PATH+1];
	char			logstr[256];
	SOCKADDR_IN		server_addr={0};
	SOCKADDR_IN		client_addr;
	socklen_t		client_addr_len;
	SOCKET			client_socket;
	SOCKET			high_socket_set;
	fd_set			socket_set;
	time_t			t;
	time_t			initialized=0;
	char*			p;
	char			compiler[32];
	http_session_t *	session;
	struct timeval tv;
	startup=(web_startup_t*)arg;

	web_ver();	/* get CVS revision */

    if(startup==NULL) {
    	sbbs_beep(100,500);
    	fprintf(stderr, "No startup structure passed!\n");
    	return;
    }

	if(startup->size!=sizeof(web_startup_t)) {	/* verify size */
		sbbs_beep(100,500);
		sbbs_beep(300,500);
		sbbs_beep(100,500);
		fprintf(stderr, "Invalid startup structure!\n");
		return;
	}

	/* Setup intelligent defaults */
	if(startup->port==0)					startup->port=IPPORT_HTTP;
	if(startup->index_file_name[0][0]==0)	SAFECOPY(startup->index_file_name[0],"index.html");
	if(startup->root_dir[0]==0)				SAFECOPY(startup->root_dir,"../html");
	if(startup->error_dir[0]==0)			SAFECOPY(startup->error_dir,"../html/error");
	if(startup->max_inactivity==0) 			startup->max_inactivity=120; /* seconds */
	if(startup->js_max_bytes==0)			startup->js_max_bytes=JAVASCRIPT_MAX_BYTES;
	if(startup->cgi_ext[0][0]==0)			SAFECOPY(startup->cgi_ext[0],"cgi");
	if(startup->ssjs_ext[0]==0)				SAFECOPY(startup->ssjs_ext,"ssjs");

	/* Copy html directories */
	SAFECOPY(root_dir,startup->root_dir);
	SAFECOPY(error_dir,startup->error_dir);

	/* Change to absolute path */
	prep_dir(startup->ctrl_dir, root_dir, sizeof(root_dir));
	prep_dir(startup->ctrl_dir, error_dir, sizeof(error_dir));

	/* Trim off trailing slash/backslash */
	if(*(p=lastchar(root_dir))==BACKSLASH)	*p=0;
	if(*(p=lastchar(error_dir))==BACKSLASH)	*p=0;

	uptime=0;
	served=0;
	startup->recycle_now=FALSE;
	recycle_server=TRUE;
	do {

		thread_up(FALSE /* setuid */);

		status("Initializing");

		memset(&scfg, 0, sizeof(scfg));

		lprintf("%s Revision %s%s"
			,server_name
			,revision
#ifdef _DEBUG
			," Debug"
#else
			,""
#endif
			);

		DESCRIBE_COMPILER(compiler);

		lprintf("Compiled %s %s with %s", __DATE__, __TIME__, compiler);

		srand(time(NULL));	/* Seed random number generator */
		sbbs_random(10);	/* Throw away first number */

		if(!winsock_startup()) {
			cleanup(1);
			return;
		}

		t=time(NULL);
		lprintf("Initializing on %.24s with options: %lx"
			,CTIME_R(&t,logstr),startup->options);

		lprintf("Root HTML directory: %s", root_dir);
		lprintf("Error HTML directory: %s", error_dir);

		/* Initial configuration and load from CNF files */
		SAFECOPY(scfg.ctrl_dir,startup->ctrl_dir);
		lprintf("Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size=sizeof(scfg);
		SAFECOPY(logstr,UNKNOWN_LOAD_ERROR);
		if(!load_cfg(&scfg, NULL, TRUE, logstr)) {
			lprintf("!ERROR %s",logstr);
			lprintf("!FAILED to load configuration files");
			cleanup(1);
			return;
		}
		scfg_reloaded=TRUE;

		sprintf(path,"%smime_types.cfg",scfg.ctrl_dir);
		if(!read_mime_types(path)) {
			lprintf("!ERROR %d reading %s", errno,path);
			cleanup(1);
			return;
		}

		if(startup->host_name[0]==0)
			SAFECOPY(startup->host_name,scfg.sys_inetaddr);

		if(!(scfg.sys_misc&SM_LOCAL_TZ) && !(startup->options&BBS_OPT_LOCAL_TIMEZONE)) {
			if(putenv("TZ=UTC0"))
				lprintf("!putenv() FAILED");
			tzset();
		}

		if(uptime==0)
			uptime=time(NULL);	/* this must be done *after* setting the timezone */

		active_clients=0;
		update_clients();

		/* open a socket and wait for a client */

		server_socket = open_socket(SOCK_STREAM);

		if(server_socket == INVALID_SOCKET) {
			lprintf("!ERROR %d creating HTTP socket", ERROR_VALUE);
			cleanup(1);
			return;
		}
		
		lprintf("Web Server socket %d opened",server_socket);

		/*****************************/
		/* Listen for incoming calls */
		/*****************************/
		memset(&server_addr, 0, sizeof(server_addr));

		server_addr.sin_addr.s_addr = htonl(startup->interface_addr);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port   = htons(startup->port);

		if(startup->seteuid!=NULL)
			startup->seteuid(FALSE);
		result = bind(server_socket,(struct sockaddr *)&server_addr,sizeof(server_addr));
		if(startup->seteuid!=NULL)
			startup->seteuid(TRUE);
		if(result != 0) {
			lprintf("!ERROR %d (%d) binding HTTP socket to port %d"
				,result, ERROR_VALUE,startup->port);
			lprintf(BIND_FAILURE_HELP);
			cleanup(1);
			return;
		}

		result = listen(server_socket, 64);

		if(result != 0) {
			lprintf("!ERROR %d (%d) listening on HTTP socket", result, ERROR_VALUE);
			cleanup(1);
			return;
		}
		lprintf("Web Server listening on port %d",startup->port);
		status("Listening");

		/* signal caller that we've started up successfully */
		if(startup->started!=NULL)
    		startup->started();

		lprintf("Web Server thread started");

		sprintf(path,"%swebsrvr.rec",scfg.ctrl_dir);

		while(server_socket!=INVALID_SOCKET) {

			/* check for re-cycle semaphores */
			if(!(startup->options&BBS_OPT_NO_RECYCLE)) {
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

			tv.tv_sec=2;
			tv.tv_usec=0;

			if((i=select(high_socket_set,&socket_set,NULL,NULL,&tv))<1) {
				if(i==0) {
					mswait(1);
					continue;
				}
				if(ERROR_VALUE==EINTR)
					lprintf("Web Server listening interrupted");
				else if(ERROR_VALUE == ENOTSOCK)
            		lprintf("Web Server socket closed");
				else
					lprintf("!ERROR %d selecting socket",ERROR_VALUE);
				break;
			}

			if(server_socket==INVALID_SOCKET)	/* terminated */
				break;

			client_addr_len = sizeof(client_addr);

			if(server_socket!=INVALID_SOCKET
				&& FD_ISSET(server_socket,&socket_set)) {
				client_socket = accept(server_socket, (struct sockaddr *)&client_addr
	        		,&client_addr_len);
			}
			else {
				lprintf("!NO SOCKETS set by select");
				continue;
			}

			if(client_socket == INVALID_SOCKET)	{
				if(ERROR_VALUE == ENOTSOCK || ERROR_VALUE == EINTR || ERROR_VALUE == EINVAL) {
            		lprintf("Web Server socket closed");
					break;
				}
				lprintf("!ERROR %d accepting connection", ERROR_VALUE);
				break;
			}

			SAFECOPY(host_ip,inet_ntoa(client_addr.sin_addr));

			if(trashcan(&scfg,host_ip,"ip-silent")) {
				close_socket(client_socket);
				continue;
			}

			host_port=ntohs(client_addr.sin_port);

			lprintf("%04d HTTP connection accepted from: %s port %u"
				,client_socket
				,host_ip, host_port);

			if(startup->socket_open!=NULL)
				startup->socket_open(TRUE);
	
			if((session=malloc(sizeof(http_session_t)))==NULL) {
				lprintf("%04d !ERROR allocating %u bytes of memory for service_client"
					,client_socket, sizeof(http_session_t));
				mswait(3000);
				close_socket(client_socket);
				continue;
			}

			memset(session, 0, sizeof(http_session_t));
			SAFECOPY(session->host_ip,host_ip);
			session->addr=client_addr;
   			session->socket=client_socket;

			_beginthread(http_session_thread, 0, session);
			served++;
		}

#if 0	/* this is handled in cleanup() */
		/* Close all open sockets  */
		lprintf("Closing Server Socket %d", server_socket);
		close_socket(server_socket);
		server_socket=INVALID_SOCKET;
#endif

		/* Wait for all node threads to terminate */
		if(http_threads_running) {
			lprintf("Waiting for %d connection threads to terminate...", http_threads_running);
			start=time(NULL);
			while(http_threads_running) {
				if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
					lprintf("!TIMEOUT waiting for %d http thread(s) to "
            			"terminate", http_threads_running);
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
