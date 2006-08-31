/* $Id$ */

#include <stdlib.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "sockwrap.h"

#include "bbslist.h"
#include "telnet_io.h"
#include "conn.h"
#include "uifcinit.h"

#ifdef USE_CRYPTLIB
#include "st_crypt.h"
#endif

static int	con_type=CONN_TYPE_UNKNOWN;
SOCKET conn_socket=INVALID_SOCKET;
char *conn_types[]={"Unknown","RLogin","Telnet","Raw"
#ifdef USE_CRYPTLIB
	,"SSH"
#endif
,NULL};
int conn_ports[]={0,513,23,0
#ifdef USE_CRYPTLIB
	,22
#endif
};
#ifdef USE_CRYPTLIB
CRYPT_SESSION	ssh_session;
int				ssh_active=FALSE;
#endif

#if defined(__BORLANDC__)
	#pragma argsused
#endif
BOOL conn_is_connected(void)
{
#ifdef USE_CRYPTLIB
	if(con_type==CONN_TYPE_SSH) {
		if(!ssh_active)
			return(FALSE);
	}
#endif
	return socket_check(conn_socket,NULL,NULL,0);
}

int conn_recv(char *buffer, size_t buflen, unsigned timeout)
{
	int		rd;
	int		avail;
	BYTE	*p;
	BOOL	data_waiting;
	static BYTE	*telnet_buf=NULL;
	static size_t tbsize=0;

#ifdef USE_CRYPTLIB
	if(con_type==CONN_TYPE_SSH) {
		int	status;
		status=cl.PopData(ssh_session, buffer, buflen, &rd);
		if(cryptStatusError(status)) {
			char	str[1024];

			if(status==CRYPT_ERROR_COMPLETE) {	/* connection closed */
				ssh_active=FALSE;
				return(-1);
			}
			sprintf(str,"Error %d recieving data",status);
			uifcmsg("Error recieving data",str);
			return(-1);
		}
		return(rd);
	}
#endif	/* USE_CRYPTLIB */

	if(con_type == CONN_TYPE_TELNET) {
		if(tbsize < buflen) {
			p=(BYTE *)realloc(telnet_buf, buflen);
			if(p != NULL) {
				telnet_buf=p;
				tbsize = buflen;
			}
		}
	}

	while(1) {

		if(!socket_check(conn_socket, &data_waiting, NULL, timeout))
			return(-1);

		if(!data_waiting)
			return(0);

		if(!ioctlsocket(conn_socket,FIONREAD,(void *)&avail) && avail)
			rd=recv(conn_socket,buffer,avail<(int)buflen?avail:buflen,0);
		else
			return(0);
		if(con_type == CONN_TYPE_TELNET) {
			if(rd>0) {
				if(telnet_interpret(buffer, rd, telnet_buf, &rd)==telnet_buf) {
					if(rd==0)	/* all bytes removed */
						continue;
					memcpy(buffer, telnet_buf, rd);
				}
			}
		}
		break;
	}
	return(rd);
}

int conn_send(char *buffer, size_t buflen, unsigned int timeout)
{
	int sent=0;
	int	ret;
	int	i;
	BYTE *sendbuf;
	BYTE *p;
	static BYTE *outbuf=NULL;
	static size_t obsize=0;

#ifdef USE_CRYPTLIB
	if(con_type==CONN_TYPE_SSH) {
		int status;

		sent=0;
		while(sent<(int)buflen) {
			status=cl.PushData(ssh_session, buffer+sent, buflen-sent, &i);
			if(cryptStatusError(status)) {
				char	str[1024];

				if(status==CRYPT_ERROR_COMPLETE) {	/* connection closed */
					ssh_active=FALSE;
					return(-1);
				}
				sprintf(str,"Error %d sending data",status);
				uifcmsg("Error sending data",str);
				return(-1);
			}
			sent+=i;
		}
		cl.FlushData(ssh_session);
		return(0);
	}
#endif

	if(con_type == CONN_TYPE_TELNET) {
		if(obsize < buflen*2) {
			p=realloc(outbuf, buflen*2);
			if(p!=NULL) {
				outbuf=p;
				obsize = 2 * buflen;
			}
			else
				return(-1);
		}
		sendbuf=telnet_expand(buffer, buflen, outbuf, &buflen);
	}
	else
		sendbuf = buffer;

	while(sent<(int)buflen) {
		if(!socket_check(conn_socket, NULL, &i, timeout))
			return(-1);

		if(!i)
			return(-1);

		ret=send(conn_socket,sendbuf+sent,buflen-sent,0);
		if(ret==-1) {
			switch(errno) {
				case EAGAIN:
				case ENOBUFS:
					SLEEP(1);
					break;
				default:
					return(-1);
			}
		}
		else
			sent+=ret;
	}
	return(0);
}

int conn_connect(struct bbslist *bbs)
{
	HOSTENT *ent;
	SOCKADDR_IN	saddr;
	char	*p;
	unsigned int	neta;
	char	*ruser;
	char	*passwd;

	init_uifc(TRUE, TRUE);

	con_type=bbs->conn_type;
#ifdef USE_CRYPTLIB
	if(con_type==CONN_TYPE_SSH) {
		if(!crypt_loaded)
			init_crypt();
		if(!crypt_loaded) {
			uifcmsg("Cannot load cryptlib - SSH inoperative",	"`Cannot load cryptlib`\n\n"
						"Cannot laod the file "
#ifdef _WIN32
						"cl32.dll"
#else
						"libcl.so"
#endif
						"\nThis file is required for SSH functionality.\n\n"
						"The newest version is always available from:\n"
						"http://www.cs.auckland.ac.nz/~pgut001/cryptlib/"
						);
			return(-1);
		}
	}
#endif

	ruser=bbs->user;
	passwd=bbs->password;
	if(con_type==CONN_TYPE_RLOGIN && bbs->reversed) {
		passwd=bbs->user;
		ruser=bbs->password;
	}
	for(p=bbs->addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;

	if(!(*p))
		neta=inet_addr(bbs->addr);
	else {
		uifc.pop("Lookup up host");
		if((ent=gethostbyname(bbs->addr))==NULL) {
			char str[LIST_ADDR_MAX+17];

			uifc.pop(NULL);
			sprintf(str,"Cannot resolve %s!",bbs->addr);
			uifcmsg(str,	"`Cannot Resolve Host`\n\n"
							"The system is unable to resolve the hostname... double check the spelling.\n"
							"If it's not an issue with your DNS settings, the issue is probobly\n"
							"with the DNS settings of the system you are trying to contact.");
			return(-1);
		}
		neta=*((unsigned int*)ent->h_addr_list[0]);
		uifc.pop(NULL);
	}
	uifc.pop("Connecting...");

	conn_socket=socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
	if(conn_socket==INVALID_SOCKET) {
		uifc.pop(NULL);
		uifcmsg("Cannot create socket!",	"`Unable to create socket`\n\n"
											"Your system is either dangerously low on resources, or there"
											"is a problem with your TCP/IP stack.");
		return(-1);
	}
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_addr.s_addr = neta;
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons((WORD)bbs->port);

	if(connect(conn_socket, (struct sockaddr *)&saddr, sizeof(saddr))) {
		char str[LIST_ADDR_MAX+20];

		conn_close();
		uifc.pop(NULL);
		sprintf(str,"Cannot connect to %s!",bbs->addr);
		uifcmsg(str,	"`Unable to connect`\n\n"
						"Cannot connect to the remote system... it is down or unreachable.");
		return(-1);
	}

	switch(con_type) {
		case CONN_TYPE_TELNET:
			memset(telnet_local_option,0,sizeof(telnet_local_option));
			memset(telnet_remote_option,0,sizeof(telnet_remote_option));
			break;

		case CONN_TYPE_RLOGIN:
			conn_send("",1,1000);
			conn_send(passwd,strlen(passwd)+1,1000);
			conn_send(ruser,strlen(ruser)+1,1000);
			if(bbs->bpsrate) {
				char	sbuf[30];
				sprintf(sbuf, "ansi-bbs/%d", bbs->bpsrate);

				conn_send(sbuf, strlen(sbuf)+1,1000);
			}
			else
				conn_send("ansi-bbs/115200",15,1000);
			break;
#ifdef USE_CRYPTLIB
		case CONN_TYPE_SSH: {
			int off=1;
			int status;

			ssh_active=FALSE;
			status=cl.CreateSession(&ssh_session, CRYPT_UNUSED, CRYPT_SESSION_SSH);
			if(cryptStatusError(status)) {
				char	str[1024];
				sprintf(str,"Error %d creating session",status);
				uifcmsg("Error creating session",str);
				return(-1);
			}

			/* we need to disable Nagle on the socket. */
			setsockopt(conn_socket, IPPROTO_TCP, TCP_NODELAY, ( char * )&off, sizeof ( off ) );

			/* Pass socket to cryptlib */
			status=cl.SetAttribute(ssh_session, CRYPT_SESSINFO_NETWORKSOCKET, conn_socket);
			if(cryptStatusError(status)) {
				char	str[1024];
				sprintf(str,"Error %d passing socket",status);
				uifcmsg("Error passing socket",str);
				return(-1);
			}

			/* Add username/password */
			status=cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_USERNAME, ruser, strlen(ruser));
			if(cryptStatusError(status)) {
				char	str[1024];
				sprintf(str,"Error %d setting username",status);
				uifcmsg("Error setting username",str);
				return(-1);
			}
			status=cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_PASSWORD, passwd, strlen(passwd));
			if(cryptStatusError(status)) {
				char	str[1024];
				sprintf(str,"Error %d setting password",status);
				uifcmsg("Error setting password",str);
				return(-1);
			}

			/* Activate the session */
			status=cl.SetAttribute(ssh_session, CRYPT_SESSINFO_ACTIVE, 1);
			if(cryptStatusError(status)) {
				char	str[1024];
				sprintf(str,"Error %d activating session",status);
				uifcmsg("Error activating session",str);
				return(-1);
			}

			ssh_active=TRUE;
			break;
		}
#endif
	}
	uifc.pop(NULL);

	return(0);
}

int conn_close(void)
{
#ifdef USE_CRYPTLIB
	if(con_type==CONN_TYPE_SSH) {
		cl.DestroySession(ssh_session);
		ssh_active=FALSE;
		return(0);
	}
#endif
	return(closesocket(conn_socket));
}
