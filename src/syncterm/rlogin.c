#include <genwrap.h>
#include <sockwrap.h>

#include "uifcinit.h"
#include "bbslist.h"

static SOCKET	rlogin_socket=INVALID_SOCKET;

int rlogin_recv(char *buffer, size_t buflen)
{
	int	r;
	int	avail;
	int rd;

	if(!socket_check(rlogin_socket, NULL, NULL, 0))
		return(-1);

	if(!ioctlsocket(rlogin_socket,FIONREAD,(void *)&avail) && avail)
		r=recv(rlogin_socket,buffer,avail<buflen?avail:buflen,0);
	else
		return(0);

	if(r==-1 && (errno==EAGAIN || errno==EINTR || errno==0))	/* WTF? */
		r=0;
	return(r);
}

int rlogin_send(char *buffer, size_t buflen, unsigned int timeout)
{
	int sent=0;
	int	ret;
	int	i;

	while(sent<buflen) {
		if(!socket_check(rlogin_socket, NULL, &i, timeout))
			return(-1);
		if(!i)
			return(-1);
		ret=send(rlogin_socket,buffer+sent,buflen-sent,0);
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

int rlogin_connect(char *addr, int port, char *ruser, char *passwd, int bedumb)
{
	HOSTENT *ent;
	SOCKADDR_IN	saddr;
	char	nil=0;
	char	*p;
	unsigned int	neta;
	int	i;

	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;
	if(!(*p))
		neta=inet_addr(addr);
	else {
		if((ent=gethostbyname(addr))==NULL) {
			char str[LIST_ADDR_MAX+17];

			sprintf(str,"Cannot resolve %s!",addr);
			uifcmsg(str,	"`Cannot Resolve Host`\n\n"
							"The system is unable to resolve the hostname... double check the spelling.\n"
							"If it's not an issue with your DNS settings, the issue is probobly\n"
							"with the DNS settings of the system you are trying to contact.");
			return(-1);
		}
		neta=*((unsigned int*)ent->h_addr_list[0]);
	}
	rlogin_socket=socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
	if(rlogin_socket==INVALID_SOCKET) {
		uifcmsg("Cannot create socket!",	"`Unable to create socket`\n\n"
											"Your system is either dangerously low on resources, or there"
											"is a problem with your TCP/IP stack.");
		return(-1);
	}
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_addr.s_addr = neta;
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons(port);
	
	if(connect(rlogin_socket, (struct sockaddr *)&saddr, sizeof(saddr))) {
		char str[LIST_ADDR_MAX+20];

		rlogin_close();
		sprintf(str,"Cannot connect to %s!",addr);
		uifcmsg(str,	"`Unable to connect`\n\n"
						"Cannot connect to the remost system... it is down or unreachable.");
		return(-1);
	}

	if(!bedumb) {
		rlogin_send("",1,1000);
		rlogin_send(passwd,strlen(passwd)+1,1000);
		rlogin_send(ruser,strlen(ruser)+1,1000);
		rlogin_send("ansi-bbs/9600",14,1000);
	}
	return(0);
}

int rlogin_close(void)
{
	return(closesocket(rlogin_socket));
}
