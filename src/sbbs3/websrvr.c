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

#include "sbbs.h"
#include "sockwrap.h"		/* sendfilesocket() */
#include "threadwrap.h"		/* _beginthread() */
#include "websrvr.h"

static const char* server_name="Synchronet Web Server";
static const char* newline="\r\n";
static const char* unknown_mime_type="application/octet-stream";

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
static web_startup_t* startup=NULL;

typedef struct  {
	BOOL	method;
	char	request[MAX_PATH+1];
	BOOL	parsed_headers;
	BOOL    expect_go_ahead;
	time_t	if_modified_since;
	BOOL	keep_alive;
	char	ars[256];
	char    auth[128];				/* UserID:Password */
	BOOL	send_location;
} http_request_t;

typedef struct  {
	SOCKET			socket;
	SOCKADDR_IN		addr;
	http_request_t	req;
	char			host[128];		/* What's this for? */
	int				http_ver;       /* HTTP version.  0 = HTTP/0.9, 1=HTTP/1.0, 2=HTTP/1.1 */
	BOOL			finished;		/* Do not accept any more imput from client */
} http_session_t;

typedef struct {
	char	ext[16];
	char	type[128];
} mime_types_t;

static mime_types_t	mime_types[MAX_MIME_TYPES];

enum { 
	 HTTP_0_9
	,HTTP_1_0
};
static char* http_vers[] = {
	 ""
	,"HTTP/1.0"
	,NULL	/* terminator */
};

enum { 
	 HTTP_HEAD
	,HTTP_GET
};
static char* methods[] = {
	 "HEAD"
	,"GET"
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
};

static struct {
	int		id;
	char*	text;
} headers[] = {
	{ HEAD_ALLOW,		"Allow"					},
	{ HEAD_AUTH,		"Authorization"			},
	{ HEAD_ENCODE,		"Content-Encoding"		},
	{ HEAD_LENGTH,		"Content-Length"		},
	{ HEAD_TYPE,		"Content-Type"			},
	{ HEAD_DATE,		"Date"					},
	{ HEAD_EXPIRES,		"Expires"				},
	{ HEAD_FROM,		"From"					},
	{ HEAD_IFMODIFIED,	"If-Modified-Since"		},
	{ HEAD_LASTMODIFIED,"Last-Modified"			},
	{ HEAD_LOCATION,	"Location"				},
	{ HEAD_PRAGMA,		"Pragma"				},
	{ HEAD_REFERER,		"Referer"				},
	{ HEAD_SERVER,		"Server"				},
	{ HEAD_USERAGENT,	"User-Agent"			},
	{ HEAD_WWWAUTH,		"WWW-Authenticate"		},
	{ HEAD_CONNECTION,	"Connection"			},
	{ HEAD_HOST,		"Host"					},
	{ -1,				NULL /* terminator */	},
};

static char	*days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static char	*months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static const char * base64alphabet = 
 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

static DWORD monthdays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

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
		lprintf("%04d !ERROR %d (%d) selecting socket for send"
			,sock, result, ERROR_VALUE, sock);
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
		lprintf("asctime() Date: %s",token);
		/* Toss away week day */
		token=strtok(date," ");
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		lprintf("Month: %s",token);
		ti.tm_mon=getmonth(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		lprintf("MDay: %s",token);
		ti.tm_mday=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		lprintf("Hour: %s",token);
		ti.tm_hour=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		lprintf("Minute: %s",token);
		ti.tm_min=atoi(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		lprintf("Second: %s",token);
		ti.tm_sec=atoi(token);
		token=strtok(NULL,"");
		if(token==NULL)
			return(0);
		lprintf("Year: %s",token);
		ti.tm_year=atoi(token)-1900;
	}
	else  {
		/* RFC 1123 or RFC 850 */
		lprintf("RFC Date");
		token=strtok(NULL," -");
		if(token==NULL)
			return(0);
		lprintf("MDay: %s",token);
		ti.tm_mday=atoi(token);
		token=strtok(NULL," -");
		if(token==NULL)
			return(0);
		lprintf("Month: %s",token);
		ti.tm_mon=getmonth(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		lprintf("Year: %s",token);
		ti.tm_year=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		lprintf("Hour: %s",token);
		ti.tm_hour=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		lprintf("Min: %s",token);
		ti.tm_min=atoi(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		lprintf("Sec: %s",token);
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
	if(!session->req.keep_alive) {
		close_socket(session->socket);
		session->finished=TRUE;
	}
}

static int get_header_type(char *header)
{
	int		i;
	for(i=0; headers[i].text!=NULL; i++) {
		if(!stricmp(header,headers[i].text)) {
			return(headers[i].id);
		}
	}
	return(-1);
}

static char *get_header(int id) 
{
	int	i;

	for(i=0;headers[i].text!=NULL;i++) {
		if(headers[i].id==id) {
			return(headers[i].text);
		}
	}
	return(NULL);
}

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

BOOL send_headers(http_session_t *session, const char *status)
{
	int		ret;
	BOOL	send_file=TRUE;
	size_t	location_offset;
	time_t	ti;
	char	status_line[MAX_REQUEST_LINE];
	struct stat	stats;
	struct tm	*t;

	SAFECOPY(status_line,status);
	ret=stat(session->req.request,&stats);
	if(!ret && (stats.st_mtime < session->req.if_modified_since)) {
		SAFECOPY(status_line,"304 Not Modified");
		ret=-1;
		send_file=FALSE;
		lprintf("%04d Not modified",session->socket);
	}
	if(session->req.send_location)  {
		SAFECOPY(status_line,"301 Moved Permanently");
		ret=-1;
		send_file=FALSE;
		lprintf("%04d Moved Permanently",session->socket);
	}
	/* Status-Line */
	sockprintf(session->socket,"%s %s",http_vers[session->http_ver],status_line);

	/* General Headers */
	ti=time(NULL);
	t=gmtime(&ti);
	sockprintf(session->socket,"%s: %s, %02d %s %04d %02d:%02d:%02d GMT",get_header(HEAD_DATE),days[t->tm_wday],t->tm_mday,months[t->tm_mon],t->tm_year+1900,t->tm_hour,t->tm_min,t->tm_sec);
	if(session->req.keep_alive)
		sockprintf(session->socket,"%s: %s",get_header(HEAD_CONNECTION),"Keep-Alive");

	/* Response Headers */
	sockprintf(session->socket,"%s: %s",get_header(HEAD_SERVER),VERSION_NOTICE);
	

	/* Entity Headers */
	/* Should be dynamic to allow POST for CGIs */
	sockprintf(session->socket,"%s: %s",get_header(HEAD_ALLOW),"GET, HEAD");
	if(session->req.send_location) {
		location_offset=strlen(root_dir);
		if(session->host[0])
			location_offset+=strlen(session->host)+1;
		sockprintf(session->socket,"%s: %s",get_header(HEAD_LOCATION),session->req.request+location_offset);
	}
	if(!ret) {
		sockprintf(session->socket,"%s: %d",get_header(HEAD_LENGTH),stats.st_size);
		
		sockprintf(session->socket,"%s: %s",get_header(HEAD_TYPE),get_mime_type(strrchr(session->req.request,'.')));
		t=gmtime(&stats.st_mtime);
		sockprintf(session->socket,"%s: %s, %02d %s %04d %02d:%02d:%02d GMT",get_header(HEAD_LASTMODIFIED),days[t->tm_wday],t->tm_mday,months[t->tm_mon],t->tm_year+1900,t->tm_hour,t->tm_min,t->tm_sec);
	}
	sendsocket(session->socket,newline,2);
	return(send_file);
}

static void sock_sendfile(SOCKET socket,char *path)
{
	int		file;

	lprintf("%04d Sending %s",socket,path);
	if((file=open(path,O_RDONLY|O_RDONLY))==-1)
		lprintf("%04d !ERROR %d opening %s",socket,errno,path);
	else {
		sendfilesocket(socket, file, 0, 0);
		close(file);
	}
}

static void send_error(char *message, http_session_t * session)
{
	char	error_path[MAX_PATH+1];
	char	error_code[4];

	session->req.send_location=FALSE;
	if(session->http_ver > HTTP_0_9)
		send_headers(session,message);
	SAFECOPY(error_code,message);
	sprintf(error_path,"%s%s.html",error_dir,error_code);
	sock_sendfile(session->socket,error_path);
	close_request(session);
}

static BOOL check_ars(char *ars,http_session_t * session)
{
	char	*username;
	char	*password;
	uchar	*ar;
	user_t	user;
	BOOL	authorized;

	if(session->req.auth[0]==0)
		return(FALSE);

	username=strtok(session->req.auth,":");
	password=strtok(NULL,":");
	/* Require a password */
	if(password==NULL)
		return(FALSE);
	user.number=matchuser(&scfg, username, FALSE);
	lprintf("%04d User number: %d",session->socket,user.number);
	getuserdat(&scfg, &user);
	if(strnicmp(user.pass,password,LEN_PASS)) {
		/* Should go to the hack log? */
		lprintf("%04d Incorrect password for: %s Password: %s Should be: "
			,session->socket,username,password,user.pass);
		return(FALSE);
	}
	ar = arstr(NULL,session->req.ars,&scfg);
	authorized=chk_ar(&scfg,ar,&user);
	if(ar!=NULL && ar!=nular)
		free(ar);

	if(authorized)
		return(TRUE);

	/* Should go to the hack log? */
	lprintf("%04d Failed ARS Auth: %s Password: %s ARS: %s"
		,session->socket,username,password,ars);

	return(FALSE);
}

static void b64_decode (uchar *p)
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

static int sockreadline(SOCKET socket, time_t timeout, char *buf, size_t length)
{
	char	ch;
	DWORD	i;
	BOOL	rd;
	time_t	start;

	start=time(NULL);
	for(i=0;TRUE;) {
		if(!socket_check(socket,&rd)) {
			close_socket(socket);
			return(-1);
		}

		if(!rd) {
			if(time(NULL)-start>timeout) {
				close_socket(socket);
				return(-1);        /* time-out */
			}
			mswait(1);
			continue;       /* no data */
		}

		if(recv(socket, &ch, 1, 0)!=1)
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

	lprintf("%04d RX: %s",socket,buf);
	return(0);
}

static BOOL parse_headers(http_session_t * session)
{
	char	req_line[MAX_REQUEST_LINE];
	char	next_char[2];
	char	*value;
	char	*p;
	int		i;

	lprintf("%04d Parsing headers",session->socket);
	while(!sockreadline(session->socket,TIMEOUT_THREAD_WAIT,req_line,sizeof(req_line))&&strlen(req_line)) {
		/* Check this... SHOULD append lines starting with spaces or horizontal tabs. */
		while((recvfrom(session->socket,next_char,1,MSG_PEEK,NULL,0)>0) && (next_char[0]=='\t' || next_char[0]==' ')) {
			i=strlen(req_line);
			sockreadline(session->socket,TIMEOUT_THREAD_WAIT,req_line+i,sizeof(req_line)-i);
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
					/* Not implemented  - POST */
					break;
				case HEAD_TYPE:
					/* Not implemented  - POST */
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
					/* Not implemented  - usefull for logging/CGI */
					break;
				case HEAD_CONNECTION:
					if(!stricmp(value,"Keep-Alive")) {
						session->req.keep_alive=TRUE;
					}
					break;
				case HEAD_HOST:
					if(session->host[0]==0) {
						SAFECOPY(session->host,value);
						lprintf("%04d Grabbing from virtual host: %s"
							,session->socket,value);
					}
				default:
					/* Should store for HTTP_* env variables in CGI */
					break;
			}
		}
	}
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

static char *get_request(char *req_line, http_session_t * session)
{
	char *	p;
	char *  retval;
	int		offset;

	while(*req_line && *req_line<' ') req_line++;
	SAFECOPY(session->req.request,req_line);
	strtok(session->req.request," \t");
	retval=strtok(NULL," \t");
	unescape(session->req.request);
	if(!strnicmp(session->req.request,"http://",7)) {
		/* Set HOST value... ignore HOST header */
		SAFECOPY(session->host,session->req.request+7);
		strtok(session->req.request,"/");
		p=strtok(NULL,"/");
		if(p==NULL) {
			/* Do not allow host values larger than 128 bytes just to be anal */
			session->host[0]=0;
			p=session->req.request+7;
		}
		offset=p-session->req.request;
		p=session->req.request;
		do { *p=*((p++)+offset); } while(*p);
	}
	return(retval);
}

static char *get_method(char *req_line, http_session_t * session)
{
	int i;

	for(i=0;methods[i]!=NULL;i++) {
		if(!strnicmp(req_line,methods[i],strlen(methods[i]))) {
			session->req.method=i;
			if(strlen(req_line)<strlen(methods[i])+2) {
				send_error("400 Bad Request",session);
				return(NULL);
			}
			return(req_line+strlen(methods[i])+1);
		}
	}
	send_error("501 Not Implemented",session);
	return(NULL);
}

static BOOL get_req(http_session_t * session)
{
	char	req_line[MAX_REQUEST_LINE];
	char *	p;
	
	if(!sockreadline(session->socket,TIMEOUT_THREAD_WAIT,req_line,sizeof(req_line))) {
		lprintf("%04d Got request line: %s",session->socket,req_line);
		p=get_method(req_line,session);
		if(p!=NULL) {
			lprintf("%04d Method: %s"
				,session->socket,methods[session->req.method]);
			p=get_request(p,session);
			lprintf("%04d Request: %s"
				,session->socket,session->req.request);
			session->http_ver=get_version(p);
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
	char	*last_slash;
	FILE*	file;
	
	lprintf("%04d Validating request: %s"
		,session->socket,session->req.request);
	if(!(startup->options&WEB_OPT_VIRTUAL_HOSTS))
		session->host[0]=0;
	if(session->host[0]) {
		sprintf(path,"%s%s",root_dir,session->host);
		if(isdir(path))
			sprintf(path,"%s%s%s",root_dir,session->host,session->req.request);
	} else
		sprintf(path,"%s%s",root_dir,session->req.request);
	
	if(FULLPATH(session->req.request,path,sizeof(session->req.request))==NULL) {
		send_error("404 Not Found",session);
		return(FALSE);
	}
	if(!strcmp(path,session->req.request))
		session->req.send_location=TRUE;
	if(!fexist(path)) {
		backslash(path);
		strcat(path,startup->index_file_name);
		session->req.send_location=TRUE;
	}
	if(strnicmp(session->req.request,root_dir,strlen(root_dir))) {
		lprintf("%04d request = '%s'",session->socket,session->req.request);
		lprintf("%04d root_dir = '%s'",session->socket,root_dir);
		send_error("400 Bad Request",session);
		session->req.keep_alive=FALSE;
		return(FALSE);
	}
	if(!fexist(path)) {
		send_error("404 Not Found",session);
		return(FALSE);
	}		
	SAFECOPY(session->req.request,path);

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
				send_error("403 Forbidden",session);
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
	
	if(session->req.ars[0] && !(check_ars(session->req.ars,session))) {
		/* No authentication provided */
		sprintf(str,"401 Unauthorized%s%s: Basic realm=\"%s\""
			,newline,get_header(HEAD_WWWAUTH),scfg.sys_name);
		send_error(str,session);
		return(FALSE);
	}
	
	return(TRUE);
}

static void respond(http_session_t * session)
{
	BOOL	send_file=TRUE;

	if(session->http_ver > HTTP_0_9)
		send_file=send_headers(session,"200 OK");
	if(send_file)
		sock_sendfile(session->socket,session->req.request);
	close_request(session);
}

void http_session_thread(void* arg)
{
	char			host_ip[64];
	char*			host_name;
	HOSTENT*		host;
	http_session_t	session=*(http_session_t*)arg;

	free(arg);	/* now we don't need to worry about freeing the session */

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

	if(!(startup->options&BBS_OPT_NO_HOST_LOOKUP))
		lprintf("%04d Hostname: %s", session.socket, host_name);

	if(trashcan(&scfg,host_ip,"ip")) {
		close_socket(session.socket);
		lprintf("%04d !CLIENT BLOCKED in ip.can: %s", session.socket, host_ip);
		thread_down();
		return;
	}

	if(trashcan(&scfg,host_name,"host")) {
		close_socket(session.socket);
		lprintf("%04d !CLIENT BLOCKED in host.can: %s", session.socket, host_name);
		thread_down();
		return;
	}

	while(!session.finished && server_socket!=INVALID_SOCKET) {
	    memset(&(session.req), 0, sizeof(session.req));
		if(get_req(&session)) {
			lprintf("%04d Got request %s method %d version %d"
				,session.socket
				,session.req.request,session.req.method,session.http_ver);
			if((session.http_ver<HTTP_1_0)||parse_headers(&session)) {
				if(check_request(&session)) {
					respond(&session);
				}
			}
		}
	}
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
	char 			host_ip[32];
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

	/* Copy html directories */
	SAFECOPY(root_dir,startup->root_dir);
	SAFECOPY(error_dir,startup->error_dir);

	/* Change to absolute path */
	prep_dir(scfg.ctrl_dir, root_dir);
	prep_dir(scfg.ctrl_dir, error_dir);

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

		/* Initial configuration and load from CNF files */
		SAFECOPY(scfg.ctrl_dir,startup->ctrl_dir);
		lprintf("Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size=sizeof(scfg);
		SAFECOPY(logstr,UNKNOWN_LOAD_ERROR);
		if(!load_cfg(&scfg, NULL, FALSE, logstr)) {
			lprintf("!ERROR %s",logstr);
			lprintf("!FAILED to load configuration files");
			cleanup(1);
			return;
		}
		scfg_reloaded=TRUE;

		sprintf(path,"%smime_types.cfg",scfg.ctrl_dir);
		if(!read_mime_types(path)) {
			lprintf("!ERROR %d reading %s", ERROR_VALUE,path);
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

		result = listen(server_socket, 1);

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

		lprintf("Web Server thread started.");

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

			strcpy(host_ip,inet_ntoa(client_addr.sin_addr));
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

