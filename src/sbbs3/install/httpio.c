#include <stdio.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "sockwrap.h"

#define MAX_SCHEME_LEN	10
#define MAX_HOST_LEN	128
#define MAX_INACTIVITY		30	/* Seconds */
#define MAX_HEADER		1024

u_long resolve_ip(char *addr)
{
	struct hostent*	host;
	char*		p;

	if(*addr==0)
		return(INADDR_NONE);

	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;
	if(!(*p))
		return(inet_addr(addr));
	if((host=gethostbyname(addr))==NULL) 
		return(INADDR_NONE);
	return(*((u_long*)host->h_addr_list[0]));
}

int sockreadline(int sock, char *buf, size_t length, char *error)
{
	char	ch;
	int		i;
	int		rd;
#if 0
	time_t	start;

	start=time(NULL);
	for(i=0;TRUE;) {
		if(!socket_check(sock,&rd,NULL,1000))  {
			if(error != NULL)
				strcpy(error, "Socket error");
			return(-1);
		}

		if(!rd) {
			if(time(NULL)-start>MAX_INACTIVITY)  {
				if(error != NULL)
					strcpy(error,"Timed out");
				return(-1);        /* time-out */
			}
			continue;       /* no data */
		}

		if(recv(sock, &ch, 1, 0)!=1)
			break;

		if(ch=='\n')
			break;

		if(i<length)
			buf[i++]=ch;
	}
#else
	for(i=0;1;) {
		if(!socket_check(sock,&rd,NULL,60000) || !rd || recv(sock, &ch, 1, 0)!=1)  {
			if(error != NULL)
				strcpy(error,"Timed out");
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

	return(i);
}

/* Error will hold the error message on a failed return... */
/* Must be at least 128 bytes long                         */
int http_get_fd(char *URL, size_t *len, char *error)
{
	char 	host[MAX_HOST_LEN+1];
	ulong	ip_addr;
	int		s=-1;
	int		port=80;
	char	*p;
	char	*path;
	struct sockaddr_in addr;
	char	header[MAX_HEADER+1];

	if(len != NULL)
		*len=0;
	if(error != NULL)
		*error=0;
	if(strncasecmp(URL, "http://", 7))  {
		if(error!=NULL)
			strcpy(error, "URL is not http");
		return(-1);
	}
	strncpy(host, URL+7, MAX_HOST_LEN);
	host[MAX_HOST_LEN]=0;
	if((p=strchr(host,'/'))==NULL)  {
		if(error!=NULL)
			strcpy(error, "Host too long or no path info found");
		return(-1);
	}
	*p=0;
	if((p=strchr(host,':'))!=NULL)  {
		*p=0;
		port=atoi(p+1);
	}
	if((ip_addr=resolve_ip(host))==INADDR_NONE)  {
		if(error!=NULL)
			strcpy(error, "Unable to resolve host");
		return(-1);
	}
		if((path=strchr(URL+7, '/'))==NULL)  {
		if(error!=NULL)
			strcpy(error, "Path info not found");
		return(-1);
	}
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)  {
		if(error!=NULL)
			strcpy(error, "Unable to create socket");
		return(-1);
	}
	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);
	if (connect(s, (struct sockaddr*)&addr, sizeof (addr)) < 0) {
		close(s);
		if(error!=NULL)
			strcpy(error, "Unable to connect");
		return(-1);
	}
	if(error!=NULL)
		strcpy(error,"Unable to send request");
	if(write(s, "GET ", 4)!=4)  {
		close(s);
		return(-1);
	}
	if(write(s, path, strlen(path))!=strlen(path))  {
		close(s);
		return(-1);
	}
	if(write(s, " HTTP/1.0\r\nHost: ", 17)!=17)  {
		close(s);
		return(-1);
	}	
	if(write(s, host, strlen(host))!=strlen(host))  {
		close(s);
		return(-1);
	}	
	if(write(s, "\r\nConnection: close\r\n\r\n",23)!=23)  {
		close(s);
		return(-1);
	}
	if(error!=NULL)
		*error=0;
	if(sockreadline(s, header, sizeof(header), error) < 0)  {
		close(s);
		return(-1);
	}
	if(atoi(header+9)!=200)  {
		if(error != NULL)  {
			strncpy(error, header, 128);
			error[127]=0;
		}
		close(s);
		return(-1);
	}
	while(header[0])  {
		if(sockreadline(s, header, sizeof(header), error) < 0)  {
			close(s);
			return(-1);
		}
		if(len != NULL && strncasecmp(header,"content-length:",15)==0)  {
			*len=atol(header+15);
		}
	}
	return(s);
}
