/* $Id$ */

#include <stdlib.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "sockwrap.h"

#include "bbslist.h"
#include "telnet_io.h"
#include "conn.h"
#include "uifcinit.h"

static int	con_type=CONN_TYPE_UNKNOWN;
SOCKET conn_socket=INVALID_SOCKET;
char *conn_types[]={"Unknown","RLogin","Telnet","Raw",NULL};

int conn_recv(char *buffer, size_t buflen, unsigned timeout)
{
	int		rd;
	int		avail;
	BYTE	*p;
	BOOL	data_waiting;
	static BYTE	*telnet_buf=NULL;
	static size_t tbsize=0;

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

int conn_connect(char *addr, int port, char *ruser, char *passwd, char *syspass, int conn_type, int speed)
{
	HOSTENT *ent;
	SOCKADDR_IN	saddr;
	char	*p;
	unsigned int	neta;

	init_uifc(TRUE, TRUE);
	con_type=conn_type;
	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;
	if(!(*p))
		neta=inet_addr(addr);
	else {
		uifc.pop("Lookup up host");
		if((ent=gethostbyname(addr))==NULL) {
			char str[LIST_ADDR_MAX+17];

			uifc.pop(NULL);
			sprintf(str,"Cannot resolve %s!",addr);
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
	saddr.sin_port   = htons((WORD)port);

	if(connect(conn_socket, (struct sockaddr *)&saddr, sizeof(saddr))) {
		char str[LIST_ADDR_MAX+20];

		conn_close();
		uifc.pop(NULL);
		sprintf(str,"Cannot connect to %s!",addr);
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
			if(speed) {
				char	sbuf[30];
				sprintf(sbuf, "ansi-bbs/%d", speed);

				conn_send(sbuf, strlen(sbuf)+1,1000);
			}
			else
				conn_send("ansi-bbs/115200",15,1000);
			break;
	}
	uifc.pop(NULL);

	return(0);
}

int conn_close(void)
{
	return(closesocket(conn_socket));
}
