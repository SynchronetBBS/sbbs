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
#endif

#define JAVASCRIPT
#include "sbbs.h"
#include "sockwrap.h"		/* sendfilesocket() */
#include "threadwrap.h"		/* _beginthread() */
#include "websrvr.h"

static const char*	server_name="Synchronet Web Server";
static const char*	newline="\r\n";
static const char*	http_scheme="http://";
static const size_t	http_scheme_len=7;

extern const uchar* nular;

#define TIMEOUT_THREAD_WAIT		60		/* Seconds */
#define MAX_MIME_TYPES			128
#define MAX_REQUEST_LINE		1024
#define CGI_ENVIRON_BLOCK_SIZE	10
#define CGI_HEADS_BLOCK_SIZE	5		/* lines */
#define MAX_ENCODINGS			32		/* Becomes bit-flags in a DWORD */

static scfg_t	scfg;
static BOOL		scfg_reloaded=TRUE;
static uint 	http_threads_running=0;
static ulong	active_clients=0;
static ulong	sockets=0;
static BOOL		recycle_server=FALSE;
static uint		thread_count=0;
static SOCKET	server_socket=INVALID_SOCKET;
static ulong	mime_count=0;
static int		enc_count=0;
static char		revision[16];
static char		root_dir[MAX_PATH+1];
static char		error_dir[MAX_PATH+1];
static time_t	uptime=0;
static web_startup_t* startup=NULL;

typedef struct  {
	BOOL	method;
	char	virtual_path[MAX_PATH+1];
	char	physical_path[MAX_PATH+1];
	BOOL	parsed_headers;
	BOOL    expect_go_ahead;
	time_t	if_modified_since;
	BOOL	keep_alive;
	char	ars[256];
	char    auth[128];				/* UserID:Password */
	char	host[128];				/* The requested host. (virtual hosts) */
	int		send_location;
	DWORD	accept_encodings;
	int		encoded_as;
	const char*	mime_type;

	/* CGI parameters */
	char**	cgi_env;
	uint	cgi_env_size;
	uint	cgi_env_max_size;
	char**	cgi_heads;
	uint	cgi_heads_size;
	uint	cgi_heads_max_size;
	char	cgi_infile[MAX_PATH+1];
	char	cgi_location[MAX_REQUEST_LINE];
	char	cgi_status[MAX_REQUEST_LINE];
	BOOL	was_cgi;

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
	char			useragent[MAX_REQUEST_LINE];
	user_t			user;

	/* JavaScript parameters */
	JSRuntime*		js_runtime;
	JSContext*		js_cx;
	JSObject*		js_glob;

} http_session_t;

typedef struct {
	char	ext[16];
	char	type[128];
} mime_types_t;

typedef struct {
	char	name[64];
	char	encode[128];
	char	decode[128];
	char	file[128];
} encode_types_t;

static mime_types_t		mime_types[MAX_MIME_TYPES];
static encode_types_t	encode_types[MAX_ENCODINGS];

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
	 HEAD_ALLOW
	,HEAD_AUTH
	,HEAD_ENCODE
	,HEAD_LENGTH
	,HEAD_TYPE
	,HEAD_DATE
	,HEAD_EXPIRES
	,HEAD_FROM
	,HEAD_IFMODIFIED
	,HEAD_LASTMODIFIED
	,HEAD_LOCATION
	,HEAD_PRAGMA
	,HEAD_REFERER
	,HEAD_SERVER
	,HEAD_USERAGENT
	,HEAD_WWWAUTH
	,HEAD_CONNECTION
	,HEAD_HOST
	,HEAD_STATUS
	,HEAD_ACCEPT_ENCODING
};

static struct {
	int		id;
	char*	text;
} headers[] = {
	{ HEAD_ALLOW,			"Allow"					},
	{ HEAD_AUTH,			"Authorization"			},
	{ HEAD_ENCODE,			"Content-Encoding"		},
	{ HEAD_LENGTH,			"Content-Length"		},
	{ HEAD_TYPE,			"Content-Type"			},
	{ HEAD_DATE,			"Date"					},
	{ HEAD_EXPIRES,			"Expires"				},
	{ HEAD_FROM,			"From"					},
	{ HEAD_IFMODIFIED,		"If-Modified-Since"		},
	{ HEAD_LASTMODIFIED,	"Last-Modified"			},
	{ HEAD_LOCATION,		"Location"				},
	{ HEAD_PRAGMA,			"Pragma"				},
	{ HEAD_REFERER,			"Referer"				},
	{ HEAD_SERVER,			"Server"				},
	{ HEAD_USERAGENT,		"User-Agent"			},
	{ HEAD_WWWAUTH,			"WWW-Authenticate"		},
	{ HEAD_CONNECTION,		"Connection"			},
	{ HEAD_HOST,			"Host"					},
	{ HEAD_STATUS,			"Status"				},
	{ HEAD_ACCEPT_ENCODING,	"Accept-Encoding"		},
	{ -1,				NULL /* terminator */	},
};

enum  {
	NO_LOCATION
	,MOVED_TEMP
	,MOVED_PERM
};

static char	*days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static char	*months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static const char * base64alphabet = 
 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

static DWORD monthdays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

static void respond(http_session_t * session);

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

static void grow_enviro(http_session_t *session)  {
	char **new_cgi_env;

	new_cgi_env=session->req.cgi_env;
	new_cgi_env=realloc(session->req.cgi_env,
		sizeof(void*)*(CGI_ENVIRON_BLOCK_SIZE+session->req.cgi_env_max_size));
	if(new_cgi_env != NULL)  {
		session->req.cgi_env=new_cgi_env;
		memset(session->req.cgi_env+sizeof(void*)*session->req.cgi_env_max_size,0,
			sizeof(void*)*(CGI_ENVIRON_BLOCK_SIZE));
		session->req.cgi_env_max_size+=CGI_ENVIRON_BLOCK_SIZE;
	} else  {
		lprintf("%04d Cannot enlarge environment",session->socket);
	}
	return;
}

static void add_env(http_session_t *session, const char *name,const char *value)  {
	char	newname[128];
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
	if(session->req.cgi_env_size==session->req.cgi_env_max_size-1)
		grow_enviro(session);
		
	if(session->req.cgi_env_size<session->req.cgi_env_max_size)  {
		session->req.cgi_env[session->req.cgi_env_size]=
			malloc(sizeof(char)*(strlen(newname)+strlen(value)+2));
		if(session->req.cgi_env[session->req.cgi_env_size] != NULL)  {
			sprintf(session->req.cgi_env[session->req.cgi_env_size++],"%s=%s",
				newname,value);
			session->req.cgi_env[session->req.cgi_env_size]=NULL;
		}
	}
}

static void init_enviro(http_session_t *session)  {
	char	str[128];

	session->req.cgi_env=malloc(sizeof(void*)*CGI_ENVIRON_BLOCK_SIZE);
	if(session->req.cgi_env != NULL)  {
		memset(session->req.cgi_env,0,sizeof(void*)*CGI_ENVIRON_BLOCK_SIZE);
		session->req.cgi_env_max_size=CGI_ENVIRON_BLOCK_SIZE;
	} else  {
		lprintf("%04d Cannot create environment",session->socket);
		session->req.cgi_env_max_size=0;
	}
	/* Need a better way of doing this! */
	add_env(session,"SERVER_SOFTWARE",VERSION_NOTICE);
	sprintf(str,"%d",startup->port);
	add_env(session,"SERVER_PORT",str);
	add_env(session,"GATEWAY_INTERFACE","CGI/1.1");
	if(!strcmp(session->host_name,"<no name>"))
		add_env(session,"REMOTE_HOST",session->host_name);
	add_env(session,"REMOTE_ADDR",session->host_ip);
}

static void grow_heads(http_session_t *session)  {
	char **new_cgi_heads;

	new_cgi_heads=session->req.cgi_heads;
	new_cgi_heads=realloc(session->req.cgi_heads,
		sizeof(void*)*(CGI_HEADS_BLOCK_SIZE+session->req.cgi_heads_max_size));
	if(new_cgi_heads != NULL)  {
		session->req.cgi_heads=new_cgi_heads;
		memset(session->req.cgi_heads+sizeof(void*)*session->req.cgi_heads_max_size,0,
			sizeof(void*)*(CGI_HEADS_BLOCK_SIZE));
		session->req.cgi_heads_max_size+=CGI_HEADS_BLOCK_SIZE;
	} else  {
		lprintf("%04d Cannot enlarge headroom",session->socket);
	}
	return;
}

static void add_head(http_session_t *session, const char *value)  {
	if(value==NULL)  {
		lprintf("%04d Attempt to set NULL header", session->socket);
		return;
	}

	if(session->req.cgi_heads_size==session->req.cgi_heads_max_size-1)
		grow_heads(session);
		
	if(session->req.cgi_heads_size<session->req.cgi_heads_max_size)  {
		session->req.cgi_heads[session->req.cgi_heads_size]=
			malloc(sizeof(char)*(strlen(value)));
		if(session->req.cgi_heads[session->req.cgi_heads_size] != NULL)  {
			sprintf(session->req.cgi_heads[session->req.cgi_heads_size++],"%s",value);
			session->req.cgi_heads[session->req.cgi_heads_size]=NULL;
			lprintf("%04d Added header: %s",session->socket,value);
		}
	}
}

static void init_heads(http_session_t *session)  {
	session->req.cgi_heads=malloc(sizeof(void*)*CGI_HEADS_BLOCK_SIZE);
	if(session->req.cgi_heads != NULL)  {
		memset(session->req.cgi_heads,0,sizeof(void*)*CGI_HEADS_BLOCK_SIZE);
		session->req.cgi_heads_max_size=CGI_HEADS_BLOCK_SIZE;
	} else  {
		lprintf("%04d Cannot create headers",session->socket);
		session->req.cgi_heads_max_size=0;
	}
	return;
}

static BOOL is_cgi(http_session_t *session)  {

#ifdef __unix__
	/* NOTE: (From the FreeBSD man page) 
	   "Access() is a potential security hole and should never be used." */
	if(!access(session->req.physical_path,X_OK))
		return(TRUE);
#endif
	return(FALSE);	
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

/**	lprintf("Parsing date: %s",date); **/
	token=strtok(date,",");
	/* This probobly only needs to be 9, but the extra one is for luck. */
	if(strlen(date)>15) {
		/* asctime() */
		//lprintf("asctime() Date: %s",token);
		/* Toss away week day */
		token=strtok(date," ");
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		//lprintf("Month: %s",token);
		ti.tm_mon=getmonth(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		//lprintf("MDay: %s",token);
		ti.tm_mday=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		//lprintf("Hour: %s",token);
		ti.tm_hour=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		//lprintf("Minute: %s",token);
		ti.tm_min=atoi(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		//lprintf("Second: %s",token);
		ti.tm_sec=atoi(token);
		token=strtok(NULL,"");
		if(token==NULL)
			return(0);
		//lprintf("Year: %s",token);
		ti.tm_year=atoi(token)-1900;
	}
	else  {
		/* RFC 1123 or RFC 850 */
		//lprintf("RFC Date");
		token=strtok(NULL," -");
		if(token==NULL)
			return(0);
		//lprintf("MDay: %s",token);
		ti.tm_mday=atoi(token);
		token=strtok(NULL," -");
		if(token==NULL)
			return(0);
		//lprintf("Month: %s",token);
		ti.tm_mon=getmonth(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		//lprintf("Year: %s",token);
		ti.tm_year=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		//lprintf("Hour: %s",token);
		ti.tm_hour=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		//lprintf("Min: %s",token);
		ti.tm_min=atoi(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		//lprintf("Sec: %s",token);
		ti.tm_sec=atoi(token);
		if(ti.tm_year>1900)
			ti.tm_year -= 1900;
	}

	t=time_gm(&ti);
/**	lprintf("Parsed date as: %d",t); **/
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
	uint i;
	
	
	if(session->req.cgi_heads_size)  {
		for(i=0;i<session->req.cgi_heads_size;i++)
			FREE_AND_NULL(session->req.cgi_heads[i]);
		FREE_AND_NULL(session->req.cgi_heads);
		session->req.cgi_heads_size=0;
	}
	
	if(session->req.cgi_env_size)  {
		for(i=0;i<session->req.cgi_env_size;i++)
			FREE_AND_NULL(session->req.cgi_env[i]);
		FREE_AND_NULL(session->req.cgi_env);
		session->req.cgi_env_size=0;
	}

	if(!session->req.keep_alive) {
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

	lprintf("Getting mime-type for %s",ext);
	if(ext==NULL)
		return(unknown_mime_type);

	for(i=0;i<mime_count;i++)
		if(!stricmp(ext+1,mime_types[i].ext))
			return(mime_types[i].type);

	return(unknown_mime_type);
}

static BOOL send_headers(http_session_t *session, const char *status)
{
	int		ret,i;
	BOOL	send_file=TRUE;
	time_t	ti;
	char	status_line[MAX_REQUEST_LINE];
	struct stat	stats;
	struct tm	*t;

	SAFECOPY(status_line,status);
	ret=stat(session->req.physical_path,&stats);
	if(!ret && (stats.st_mtime < session->req.if_modified_since)) {
		SAFECOPY(status_line,"304 Not Modified");
		ret=-1;
		send_file=FALSE;
		lprintf("%04d Not modified",session->socket);
	}
	if(session->req.send_location==MOVED_PERM)  {
		SAFECOPY(status_line,"301 Moved Permanently");
		ret=-1;
		send_file=FALSE;
		lprintf("%04d Moved Permanently",session->socket);
	}
	if(session->req.send_location==MOVED_TEMP)  {
		SAFECOPY(status_line,"302 Moved Temporarily");
		ret=-1;
		send_file=FALSE;
		lprintf("%04d Moved Temporarily",session->socket);
	}
	/* Status-Line */
	sockprintf(session->socket,"%s %s",http_vers[session->http_ver],status_line);

	/* General Headers */
	ti=time(NULL);
	t=gmtime(&ti);
	sockprintf(session->socket,"%s: %s, %02d %s %04d %02d:%02d:%02d GMT",get_header(HEAD_DATE),days[t->tm_wday],t->tm_mday,months[t->tm_mon],t->tm_year+1900,t->tm_hour,t->tm_min,t->tm_sec);
	if(session->req.keep_alive)
		sockprintf(session->socket,"%s: %s",get_header(HEAD_CONNECTION),"Keep-Alive");
	else
		sockprintf(session->socket,"%s: %s",get_header(HEAD_CONNECTION),"Close");

	/* Response Headers */
	sockprintf(session->socket,"%s: %s",get_header(HEAD_SERVER),VERSION_NOTICE);
	

	/* Entity Headers */
	if(session->req.was_cgi)
		sockprintf(session->socket,"%s: %s",get_header(HEAD_ALLOW),"GET, HEAD, POST");
	else
		sockprintf(session->socket,"%s: %s",get_header(HEAD_ALLOW),"GET, HEAD");

	if(session->req.send_location) {
		if(!strnicmp(session->req.physical_path,http_scheme,http_scheme_len))  {
			lprintf("%04d Sending location as %s",session->socket,session->req.virtual_path);
			sockprintf(session->socket,"%s: %s",get_header(HEAD_LOCATION),(session->req.virtual_path));
		} else  {
			sockprintf(session->socket,"%s: %s",get_header(HEAD_LOCATION),(session->req.virtual_path));
			lprintf("%04d Sending location as %s",session->socket,session->req.virtual_path);
		}
	}
	if(!ret) {
		if(session->req.encoded_as>=0)
			sockprintf(session->socket,"%s: %s"
				,get_header(HEAD_ENCODE),encode_types[session->req.encoded_as].name);
		sockprintf(session->socket,"%s: %d",get_header(HEAD_LENGTH),stats.st_size);
		lprintf("%04d Sending stats for: %s",session->socket,session->req.physical_path);

		if(!session->req.was_cgi)
			sockprintf(session->socket,"%s: %s",get_header(HEAD_TYPE)
				,session->req.mime_type);

		t=gmtime(&stats.st_mtime);
		sockprintf(session->socket,"%s: %s, %02d %s %04d %02d:%02d:%02d GMT"
			,get_header(HEAD_LASTMODIFIED)
			,days[t->tm_wday],t->tm_mday,months[t->tm_mon]
			,t->tm_year+1900,t->tm_hour,t->tm_min,t->tm_sec);
	} else 
		sockprintf(session->socket,"%s: 0",get_header(HEAD_LENGTH));

	if(session->req.was_cgi)  {
		/* CGI-generated headers */
		for(i=0;session->req.cgi_heads[i]!=NULL;i++)  {
			sockprintf(session->socket,"%s",session->req.cgi_heads[i]);
			lprintf("%04d Sending header: %s",session->socket,session->req.cgi_heads[i]);
		}
	}

	/* This could probobly be combined with the CGI-generated headers - ToDo */
	for(i=0;i<(int)session->req.cgi_heads_size;i++)
		FREE_AND_NULL(session->req.cgi_heads[i]);
	FREE_AND_NULL(session->req.cgi_heads);
	session->req.cgi_heads_size=0;

	sendsocket(session->socket,newline,2);
	return(send_file);
}

static void sock_sendfile(SOCKET socket,char *path)
{
	int		file;

	lprintf("%04d Sending %s",socket,path);
	if((file=open(path,O_RDONLY|O_BINARY))==-1)
		lprintf("%04d !ERROR %d opening %s",socket,errno,path);
	else {
		if(sendfilesocket(socket, file, 0, 0) < 1)
			lprintf("%04d !ERROR %d sending %s"
				, socket, errno, path);
		close(file);
		/* sendfilesocket() does not close() */
	}
}

static int	get_best_encoding(http_session_t *session)  {
	/* Essentially, get the lowest set bit from accept_encodings */
	int	i=0;
	
	if(session->req.accept_encodings)  {
		for(i=0;i<enc_count;)  {
			if(session->req.accept_encodings&(1<<i))
				break;
			i++;
		}
		if(i==enc_count)
			return(-1);
		lprintf("%04d Best encoding is %s (%d)",session->socket,encode_types[i].name,i);
		return(i);
	}
	return(-1);
}

static BOOL apply_encoding(http_session_t *session)  {
	int				encoding=0;
	char			path[MAX_PATH+1];
	char			command[MAX_PATH+1];
	struct stat		orig_s;
	struct stat		enc_s;


	encoding=get_best_encoding(session);

	/* Hack for Mozilla 4.* browsers apparently broken Content-Encoding */
	if(!strncmp(session->useragent,"Mozilla/4",9))
		encoding=-1;
		
	session->req.mime_type=get_mime_type(strrchr(session->req.physical_path,'.'));
	
	if(encoding!=-1)  {

		lprintf("%04d Applying encoding %s to %s",session->socket,encode_types[encoding].name,session->req.physical_path);
		sprintf(path,encode_types[encoding].file,session->req.physical_path);
		lprintf("%04d Encoded file is: %s",session->socket,path);
		/* Check if has already been encoded */
		if(fexist(path))  {
			stat(session->req.physical_path,&orig_s);
			stat(path,&enc_s);
			if(orig_s.st_mtime>enc_s.st_mtime)  {
				/* Do encoding */
				sprintf(command,encode_types[encoding].encode,session->req.physical_path,session->req.physical_path);
				lprintf("%04d Encoding command: %s",session->socket,command);
				system(command);
			}
		}
		else  {
			/* Do encoding */
			sprintf(command,encode_types[encoding].encode,session->req.physical_path,session->req.physical_path);
			lprintf("%04d Encoding command: %s",session->socket,command);
			system(command);
		}
		if(fexist(path))  {
			SAFECOPY(session->req.physical_path,path);
			session->req.encoded_as=encoding;
			return(TRUE);
		}
		lprintf("%04d Encoding FAILED! %s",session->socket,encode_types[encoding].name);
		session->req.encoded_as=-1;
		return(FALSE);
	}
	session->req.encoded_as=encoding;
	return(TRUE);
}

static void send_error(http_session_t * session, char *message)
{
	char	error_code[4];

	lprintf("%04d !ERROR %s",session->socket,message);
	session->req.send_location=NO_LOCATION;
	SAFECOPY(error_code,message);
	sprintf(session->req.physical_path,"%s/%s.html",error_dir,error_code);
	if(session->http_ver > HTTP_0_9)  {
		apply_encoding(session);
		send_headers(session,message);
	}
	if(fexist(session->req.physical_path))
		sock_sendfile(session->socket,session->req.physical_path);
	else {
		lprintf("%04d Error message file %s doesn't exist.",session->socket,session->req.physical_path);
		/* It really needs to send some kind of default HTML error text here */
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
	lprintf("%04d User number: %d",session->socket,session->user.number);
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
		add_env(session,"AUTH_TYPE","Basic");
		/* Should use name if set to do so somewhere ToDo */
		add_env(session,"REMOTE_USER",session->user.alias);
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
	lprintf("B64 Decoding: %s",p);
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
	lprintf("B64 Decoded: %s",p);
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

static BOOL read_encode_types(char* fname)
{
	char	str[1024];
	char *	name;
	char *	encode;
	char *	decode;
	char *	file;
	FILE*	enc_config;

	if((enc_config=fopen(fname,"r"))==NULL)
		return(FALSE);

	memset(encode_types,0,sizeof(encode_types));
	while (!feof(enc_config)&&enc_count<MAX_ENCODINGS) {
		if(fgets(str,sizeof(str),enc_config)!=NULL) {
			truncsp(str);
			name=strtok(str,"\t");
			if(name!=NULL) {
				while(*name && *name<=' ') name++;
				if(*name!=';') {
					encode=strtok(NULL,"\t");
					if(encode != NULL)  {
						while(*encode=='\t')
							*(encode++)=0;
						decode=strtok(NULL,"\t");
						if(decode != NULL)  {
							while(*decode=='\t')
								*(decode++)=0;
							file=strtok(NULL,"\t");
							if(file != NULL)  {
								while(*file=='\t')
									*(file++)=0;
								SAFECOPY((encode_types[enc_count]).name,name);
								SAFECOPY((encode_types[enc_count]).encode,encode);
								SAFECOPY((encode_types[enc_count]).decode,decode);
								SAFECOPY((encode_types[enc_count++]).file,file);
							}
						}
					}
				}
			}
		}
	}
	fclose(enc_config);
	lprintf("Loaded %d encoding types",enc_count);
	return(enc_count>0);
}

static DWORD get_encodings(char *encodings)  {
	char *	value;
	char *	p;
	char	str[128];
	DWORD	parsed=0;
	int		i;
	
	lprintf("Parsing encodings: %s",encodings);
	value=strtok(encodings,",");
	while(value!=NULL)  {
		while(*value<' ') value++;
		SAFECOPY(str,value);
		if((p=strchr(str,';'))!=NULL)
			*p=0;
		for(i=0;i<enc_count;i++)  {
			if(!stricmp(encode_types[i].name,value))  {
				parsed |= (1<<i);
				lprintf("Accepts %s",encode_types[i].name);
			}
		}
		value=strtok(NULL,",");
	}
	lprintf("Accepts %d",parsed);
	return(parsed);
}

static int sockreadline(http_session_t * session, char *buf, size_t length)
{
	char	ch;
	DWORD	i;
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

	/* Terminate at length if longer */
	if(i>length)
		i=length;
		
	if(i>0 && buf[i-1]=='\r')
		buf[i-1]=0;
	else
		buf[i]=0;

	if(startup->options&WEB_OPT_DEBUG_RX)
		lprintf("%04d RX: %s",session->socket,buf);
	return(0);
}

static BOOL parse_headers(http_session_t * session)
{
	char	req_line[MAX_REQUEST_LINE];
	char	next_char[2];
	char	*value;
	char	*p;
	int		i;
	int		content_len=0;
	char	env_name[128];
	int		incgi;

	lprintf("%04d Parsing headers",session->socket);
	while(!sockreadline(session,req_line,sizeof(req_line))&&strlen(req_line)) {
		/* Check this... SHOULD append lines starting with spaces or horizontal tabs. */
		while((recvfrom(session->socket,next_char,1,MSG_PEEK,NULL,0)>0) 
			&& (next_char[0]=='\t' || next_char[0]==' ')) {
			i=strlen(req_line);
			sockreadline(session,req_line+i,sizeof(req_line)-i);
		}
		strtok(req_line,":");
		if((value=strtok(NULL,""))!=NULL) {
			i=get_header_type(req_line);
			while(*value && *value<=' ') value++;
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
				case HEAD_ENCODE:
					/* Not implemented  - POST */
					break;
				case HEAD_LENGTH:
					add_env(session,"CONTENT_LENGTH",value);
					content_len=atoi(value);
					break;
				case HEAD_TYPE:
					add_env(session,"CONTENT_TYPE",value);
					break;
				case HEAD_DATE:
					/* Not implemented  - Who really cares what time the client thinks it is? */
					break;
				case HEAD_FROM:
					/* Not implemented  - usefull for logging?  Does ANYONE send this? */
					break;
				case HEAD_IFMODIFIED:
					session->req.if_modified_since=decode_date(value);
					break;
				case HEAD_REFERER:
					/* Not implemented  - usefull for logging/CGI */
					break;
				case HEAD_USERAGENT:
					/* usefull for logging/CGI */
					SAFECOPY(session->useragent,value);
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
						lprintf("%04d Grabbing from virtual host: %s"
							,session->socket,value);
					}
					break;
				case HEAD_ACCEPT_ENCODING:
					session->req.accept_encodings=get_encodings(value);
					break;
				default:
					break;
			}
			sprintf(env_name,"HTTP_%s",req_line);
			add_env(session,env_name,value);
		}
	}
	if(content_len)  {
		sprintf(session->req.cgi_infile,"%s/SBBS_CGI.%d",startup->cgi_temp_dir,session->socket);
		if((incgi=open(session->req.cgi_infile,O_WRONLY|O_CREAT|O_TRUNC,S_IREAD|S_IWRITE))>0)  {
			lprintf("%04d Created %s",session->socket,session->req.cgi_infile);
			recvfilesocket(session->socket,incgi,NULL,content_len);
			lprintf("Closing file: %04d",incgi);
			close(incgi);
		} else  {
			lprintf("%04d !ERROR %d creating %s",session->socket,errno,session->req.cgi_infile);
#ifdef __unix__
			SAFECOPY(session->req.cgi_infile,"/dev/null");
#else
			SAFECOPY(session->req.cgi_infile,"NUL");
#endif
		}
	}
	else  {
#ifdef __unix__
		SAFECOPY(session->req.cgi_infile,"/dev/null");
#else
		SAFECOPY(session->req.cgi_infile,"NUL");
#endif
	}
	add_env(session,"SERVER_NAME",session->req.host[0] ? session->req.host : startup->host_name );
	lprintf("%04d Done parsing headers",session->socket);
	return TRUE;
}

static void unescape(char *p)
{
	char *	dst;
	char	code[3];
	
	dst=p;
	for(;*p;p++) {
		if(*p=='%' && isxdigit(*p) && isxdigit(*p)) {
			sprintf(code,"%.2s",p);
			*(dst++)=(char)strtol(p+1,NULL,16);
			p+=2;
		}
		else  {
			*(dst++)=*p;
		}
	}
	*(dst)=0;
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
			lprintf("Got version: %s (%d)",http_vers[i],i);
			return(i);
		}
	}
	lprintf("Got version: %s (%d)",http_vers[i-1],i-1);
	return(i-1);
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
	add_env(session,"QUERY_STRING",p);
	unescape(session->req.virtual_path);
	SAFECOPY(session->req.physical_path,session->req.virtual_path);
	if(!strnicmp(session->req.physical_path,http_scheme,http_scheme_len)) {
		/* Set HOST value... ignore HOST header */
		SAFECOPY(session->req.host,session->req.physical_path+http_scheme_len);
		strtok(session->req.physical_path,"/");
		p=strtok(NULL,"/");
		if(p==NULL) {
			/* Do not allow host values larger than 128 bytes just to be anal */
			session->req.host[0]=0;
			p=session->req.physical_path+http_scheme_len;
		}
		offset=p-session->req.physical_path;
		p=session->req.physical_path;
		do { *p=*((p++)+offset); } while(*p);
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
			add_env(session,"REQUEST_METHOD",methods[i]);
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
	
	if(!sockreadline(session,req_line,sizeof(req_line))) {
		lprintf("%04d Got request line: %s",session->socket,req_line);
		p=NULL;
		p=get_method(session,req_line);
		if(p!=NULL) {
			lprintf("%04d Method: %s"
				,session->socket,methods[session->req.method]);
			p=get_request(session,p);
			lprintf("%04d Request: %s"
				,session->socket,session->req.virtual_path);
			session->http_ver=get_version(p);
			if(session->http_ver>=HTTP_1_1)
				session->req.keep_alive=TRUE;
			add_env(session,"SERVER_PROTOCOL",session->http_ver ? 
				http_vers[session->http_ver] : "HTTP/0.9");
			lprintf("%04d Version: %s"
				,session->socket,http_vers[session->http_ver]);
			return(TRUE);
		}
	}
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
	
	lprintf("%04d Validating request: %s"
		,session->socket,session->req.physical_path);
	if(!(startup->options&WEB_OPT_VIRTUAL_HOSTS))
		session->req.host[0]=0;
	if(session->req.host[0]) {
		sprintf(str,"%s/%s",root_dir,session->req.host);
		if(isdir(str))
			sprintf(str,"%s/%s%s",root_dir,session->req.host,session->req.physical_path);
	} else
		sprintf(str,"%s%s",root_dir,session->req.physical_path);
	
	if(FULLPATH(path,str,sizeof(session->req.physical_path))==NULL) {
		send_error(session,"404 Not Found");
		return(FALSE);
	}
	if(!fexist(path)) {
		lprintf("%04d Requested file doesn't exist: %s", session->socket, path);
		last_ch=*lastchar(path);
		if(last_ch!='/' && last_ch!='\\')  {
			strcat(path,"/");
		}
		last_ch=*lastchar(session->req.virtual_path);
		if(last_ch!='/' && last_ch!='\\')  {
			strcat(session->req.virtual_path,"/");
		}
		strcat(path,startup->index_file_name);
		strcat(session->req.virtual_path,startup->index_file_name);
		session->req.send_location=MOVED_PERM;
	}
	if(strnicmp(path,root_dir,strlen(root_dir))) {
		lprintf("%04d path = '%s'",session->socket,path);
		lprintf("%04d root_dir = '%s'",session->socket,root_dir);
		send_error(session,"400 Bad Request");
		session->req.keep_alive=FALSE;
		return(FALSE);
	}
	if(!fexist(path)) {
		send_error(session,"404 Not Found");
		return(FALSE);
	}
	SAFECOPY(session->req.physical_path,path);
	add_env(session,"PATH_TRANSLATED",path);
	add_env(session,"SCRIPT_NAME",session->req.virtual_path);

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
	lprintf("%04d Validated as %s",session->socket,session->req.physical_path);
	return(TRUE);
}

static BOOL parse_cgi_headers(http_session_t *session,FILE *output)  {
	char	str[MAX_REQUEST_LINE];
	char	header[MAX_REQUEST_LINE];
	char	*directive;
	char	*value;
	int		i;

	if(fgets(str,MAX_REQUEST_LINE,output)==NULL)
		str[0]=0;
	truncsp(str);
	while(str[0])  {
		SAFECOPY(header,str);
		directive=strtok(header,":");
		if(directive != NULL)  {
			value=strtok(NULL,"");
			i=get_header_type(directive);
			switch (i)  {
				case HEAD_TYPE:
					add_head(session,str);
					break;
				case HEAD_LOCATION:
					SAFECOPY(session->req.cgi_location,value);
					if(*value=='/')  {
						unescape(value);
						SAFECOPY(session->req.virtual_path,value);
						SAFECOPY(session->req.physical_path,value);
						if(check_request(session)) {
							respond(session);
						}
					} else  {
						SAFECOPY(session->req.virtual_path,value);
						session->req.send_location=MOVED_TEMP;
						send_headers(session,"302 Moved Temporarily");
					}
					return(FALSE);
				case HEAD_STATUS:
					SAFECOPY(session->req.cgi_status,value);
					break;
				default:
					add_head(session,str);
			}
		}
		if(fgets(str,MAX_REQUEST_LINE,output)==NULL)
			str[0]=0;
		truncsp(str);
	}
	lprintf("%04d Done with da headers",session->socket);
	return(TRUE);
}

static void send_cgi_response(http_session_t *session,FILE *output)  {
	long		filepos;
	struct stat	stats;
	int			send_out;
	char *		buf;

	/* Support nph-* scripts ToDo */
	if(!parse_cgi_headers(session,output))
		return;

	filepos=ftell(output);
	fstat(fileno(output),&stats);
	sprintf(session->req.physical_path,"%s/SBBS_CGI.%d.send",startup->cgi_temp_dir,session->socket);
	if((send_out=open(session->req.physical_path,O_WRONLY|O_CREAT|O_TRUNC,S_IREAD|S_IWRITE))>0)  {
		lprintf("%04d Created %s",session->socket,session->req.cgi_infile);
		if((buf=malloc(stats.st_size-filepos))==NULL)  {
			send_error(session,"500 Internal Server Error");
			return;
		}
		fread(buf,stats.st_size-filepos,1,output);
		write(send_out,buf,stats.st_size-filepos);
		free(buf);
		lprintf("Closing file: %04d",send_out);
		close(send_out);
	} else  {
		lprintf("%04d !ERROR %d creating %s",session->socket,errno,session->req.physical_path);
		send_error(session,"500 Internal Server Error");
		return;
	}
	
	lprintf("%04d Sending CGI response",session->socket);
	respond(session);
}

static BOOL exec_cgi(http_session_t *session)  
{
	char	cmdline[MAX_PATH+256];
	char	*args[4];
	char	*comspec;
#ifdef __unix__
	int		i;
	int		status;
	char	outpath[MAX_PATH+1];
	FILE	*output;
	pid_t	child;
#endif

	SAFECOPY(cmdline,session->req.physical_path);
	sprintf(cmdline,"%s < %s > %s/SBBS_CGI.%d.out"
		,session->req.physical_path,session->req.cgi_infile
		,startup->cgi_temp_dir,session->socket);
	sprintf(session->req.physical_path,"%s/SBBS_CGI.%d.out",startup->cgi_temp_dir
		,session->socket);
	
	comspec=getenv(  
#ifdef __unix__
		"SHELL"
#else
		"COMSPEC"
#endif
		);
	if(comspec==NULL) {
		lprintf("%04d Could not get comspec",session->socket);
		return(FALSE);
	}

	args[0]=comspec;
	args[1]="-c";
	args[2]=cmdline;
	args[3]=NULL;

#ifdef __unix__

	lprintf("Executing %s -c %s",comspec,cmdline);
	session->req.cgi_env[session->req.cgi_env_size]=NULL;
	if((child=fork())==0)  {
		execve(comspec,args,session->req.cgi_env);
		lprintf("FAILED! execve()");
		exit(EXIT_FAILURE); /* Should never happen */
	}
	/* Free() the environment */
	for(i=0;i<session->req.cgi_env_size;i++)
		FREE_AND_NULL(session->req.cgi_env[i]);
	FREE_AND_NULL(session->req.cgi_env);
	session->req.cgi_env_size=0;
	
	if(child>0)  {
		waitpid(child,&status,0);
		lprintf("%04d Child exited",session->socket);
//		unlink(session->req.cgi_infile);
		if(WIFEXITED(status))  {
			/* Parse headers */
			output=fopen(session->req.physical_path,"r");
			if(output!=NULL)  {
				session->req.was_cgi=TRUE;
				SAFECOPY(outpath,session->req.physical_path);
				lprintf("%04d Opened response file: %s",session->socket,session->req.physical_path);
				send_cgi_response(session,output);
				lprintf("%04d Sent response file.",session->socket);
				fclose(output);
//				unlink(outpath);
//				unlink(session->req.physical_path);
				return(WEXITSTATUS(status)==EXIT_SUCCESS);
			}
			else  {
				lprintf("%04d !ERROR %d opening response file: %s"
					,session->socket,errno,session->req.physical_path);
				return(FALSE);
			}
		}
		return(FALSE);
	}
	return(FALSE);
#else
	sprintf(cmdline,"%s /C %s < %s > %s",comspec,cmdline,session->req.cgi_infile,session->req.physical_path);
	system(cmdline);
//	unlink(session->req.cgi_infile);
	return(TRUE);
#endif
}

/********************/
/* JavaScript stuff */
/********************/

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
js_initcx(JSRuntime* runtime, SOCKET sock, JSObject** glob)
{
	char		ver[256];
	JSContext*	js_cx;
	JSObject*	js_glob;
	JSObject*	server;
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
		val = STRING_TO_JSVAL(JS_NewStringCopyZ(js_cx, ver));
		if(!JS_SetProperty(js_cx, server, "version", &val))
			break;

		val = STRING_TO_JSVAL(JS_NewStringCopyZ(js_cx, web_ver()));
		if(!JS_SetProperty(js_cx, server, "version_detail", &val))
			break;

		if(glob!=NULL)
			*glob=js_glob;

		success=TRUE;

	} while(0);

	if(!success) {
		JS_DestroyContext(js_cx);
		return(NULL);
	}

	return(js_cx);
}

BOOL js_setup(http_session_t* session)
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
			,&session->js_glob))==NULL)) {
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

		if(!js_CreateUserObjects(session->js_cx, session->js_glob, &scfg, &session->user
			,NULL /* ftp index file */, NULL /* subscan */)) 
			lprintf("%04d !JavaScript ERROR creating user objects",session->socket);
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

	JS_SetContextPrivate(session->js_cx, session);

	return(TRUE);
}


BOOL is_js(http_session_t * session)
{
	char* p;

	if((p=strrchr(session->req.physical_path,'.'))==NULL)
		return(FALSE);

	if(stricmp(p,startup->js_ext)==0)
		return(TRUE);

	return(FALSE);
}

static void respond(http_session_t * session)
{
	char		path[MAX_PATH+1];
	BOOL		send_file=TRUE;
	BOOL		success=FALSE;
	JSScript*	js_script;
	jsval		rval;


	if(is_cgi(session))  {
		if(!exec_cgi(session))  {
			send_error(session,"500 Internal Server Error");
			return;
		}
		close_request(session);
		return;
	}

	if(is_js(session)) {	/* Server-Side JavaScript */

		if(!js_setup(session)) {
			lprintf("%04d !ERROR setting up JavaScript support", session->socket);
			send_error(session,"500 Internal Server Error");
			return;
		}

		sprintf(path,"%s/SBBS_SSJS.%d.html",startup->cgi_temp_dir,session->socket);
		if((session->req.fp=fopen(path,"w"))==NULL) {
			lprintf("%04d !ERROR %d opening/creating %s", session->socket, errno, path);
			send_error(session,"500 Internal Server Error");
			return;
		}

		do {

			/* RUN SCRIPT */
			if((js_script=JS_CompileFile(session->js_cx, session->js_glob
				,session->req.physical_path))==NULL) {
				lprintf("%04d !JavaScript FAILED to compile script (%s)"
					,session->socket,session->req.physical_path);
				break;
			}

			if((success=JS_ExecuteScript(session->js_cx, session->js_glob, js_script, &rval))!=TRUE) {
				lprintf("%04d !JavaScript FAILED to execute script (%s)"
					,session->socket,session->req.physical_path);
				break;
			}

		} while(0);

		/* Free up temporary resources here */

		if(js_script!=NULL) 
			JS_DestroyScript(session->js_cx, js_script);
		if(session->req.fp!=NULL)
			fclose(session->req.fp);

		if(!success) {
			send_error(session,"500 Internal Server Error");
			return;
		}
		SAFECOPY(session->req.physical_path, path);
	}

	if(session->http_ver > HTTP_0_9)  {
		apply_encoding(session);
		send_file=send_headers(session,"200 OK");
	}
	if(send_file)
		sock_sendfile(session->socket,session->req.physical_path);
	close_request(session);
}

void http_session_thread(void* arg)
{
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
	}

	/* host_ip wasn't defined in http_session_thread */
	if(trashcan(&scfg,session.host_ip,"ip")) {
		close_socket(session.socket);
		lprintf("%04d !CLIENT BLOCKED in ip.can: %s", session.socket, session.host_ip);
		thread_down();
		lprintf("%04d Free()ing session",socket);
		return;
	}

	if(trashcan(&scfg,host_name,"host")) {
		close_socket(session.socket);
		lprintf("%04d !CLIENT BLOCKED in host.can: %s", session.socket, host_name);
		thread_down();
		lprintf("%04d Free()ing session",socket);
		return;
	}

	active_clients++;
	update_clients();

	while(!session.finished && server_socket!=INVALID_SOCKET) {
	    memset(&(session.req), 0, sizeof(session.req));
		init_enviro(&session);
		init_heads(&session);
		if(get_req(&session)) {
			lprintf("%04d Got request %s method %d version %d"
				,session.socket
				,session.req.virtual_path,session.req.method,session.http_ver);
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
	}

	if(session.js_runtime!=NULL) {
		lprintf("%04d JavaScript: Destroying runtime",socket);
		JS_DestroyRuntime(session.js_runtime);
	}

	active_clients--;
	update_clients();
//	client_off(session.socket);

	thread_down();
	lprintf("%04d Session thread terminated (%u clients, %u threads remain)"
		,socket, active_clients, thread_count);

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
    lprintf("#### Web Server thread terminated (%u threads remain)", thread_count);
	if(startup!=NULL && startup->terminated!=NULL)
		startup->terminated(code);
}

const char* DLLCALL web_ver(void)
{
	static char ver[256];
	char compiler[32];

	DESCRIBE_COMPILER(compiler);

	sscanf("$Revision$" + 11, "%s", revision);

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
	if(startup->index_file_name[0]==0)		SAFECOPY(startup->index_file_name,"index.html");
	if(startup->root_dir[0]==0)				SAFECOPY(startup->root_dir,"../html");
	if(startup->error_dir[0]==0)			SAFECOPY(startup->error_dir,"../html/error");
	if(startup->max_inactivity==0) 			startup->max_inactivity=120; /* seconds */
	if(startup->js_max_bytes==0)			startup->js_max_bytes=JAVASCRIPT_MAX_BYTES;

	/* Copy html directories */
	SAFECOPY(root_dir,startup->root_dir);
	SAFECOPY(error_dir,startup->error_dir);

	/* Change to absolute path */
	prep_dir(startup->ctrl_dir, root_dir);
	prep_dir(startup->ctrl_dir, error_dir);

	/* Trim off trailing slash/backslash */
	if(*(p=lastchar(root_dir))==BACKSLASH)	*p=0;
	if(*(p=lastchar(error_dir))==BACKSLASH)	*p=0;

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
			,ctime(&t),startup->options);

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

		sprintf(path,"%sweb_encode.cfg",scfg.ctrl_dir);
		read_encode_types(path);

		if(startup->host_name[0]==0)
			SAFECOPY(startup->host_name,scfg.sys_inetaddr);

		if(!(scfg.sys_misc&SM_LOCAL_TZ) && !(startup->options&BBS_OPT_LOCAL_TIMEZONE)) {
			if(putenv("TZ=UTC0"))
				lprintf("!putenv() FAILED");
			tzset();
		}

		if(uptime==0)
			uptime=time(NULL);

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

		result = listen(server_socket, 16);

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

		while(server_socket!=INVALID_SOCKET) {

			/* check for re-cycle semaphores */
			if(!(startup->options&BBS_OPT_NO_RECYCLE)) {
				sprintf(path,"%swebsrvr.rec",scfg.ctrl_dir);
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

			client_addr_len = sizeof(client_addr);

			if(FD_ISSET(server_socket,&socket_set)) 
				client_socket = accept(server_socket, (struct sockaddr *)&client_addr
	        		,&client_addr_len);
			else {
				lprintf("!NO SOCKETS set by select");
				continue;
			}

			if(client_socket == INVALID_SOCKET)	{
				if(ERROR_VALUE == ENOTSOCK || ERROR_VALUE == EINTR) {
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

