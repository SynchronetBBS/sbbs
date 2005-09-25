/**
 * mrelayd.c -- a relay daemon
 * --
 * $Id$
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * "The Java Telnet Applet" is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* mrelayd.c (c) 1996,1997 Marcus Meissner <marcus@mud.de> */

/* Compile with: cc -o relayd relayd.c
 *           or: gcc -o relayd relayd.c
 * Solaris: (g)cc -o relayd relayd.c -lsocket -lnsl
 */

/* this program expects the string:
 * "relay <hostname> <port>" or "relay <hostname>"
 * after connecting. It will then try to connect to the specified host
 * if failures occur, it will terminate the connection.
 */

/* adjust this to a reasonable limit */
#define MAXUSERS  120

/* message printed if all slots are used ... */
#define FAILMESSAGE "Sorry, all slots are full.\r\n"

/* string printed before connection */
#define RELAYHEADER "Relayd $Revision$ (c) Marcus Meissner\r\n"

/* the tcp port this demons is listening on ... */
#define LISTENPORT  31415

/* default connect port (telnet) */
#define DEFAULTPORT 23

/* default buffersize */
#define DEFAULTSIZE	2000

#include <stdio.h>
#ifdef _WIN32
#include <winsock.h>
#include <signal.h>
#define ioctl ioctlsocket
#define read(a,b,c) recv(a,b,c,0)
#define write(a,b,c) send(a,b,c,0)
#define close _lclose
#define EINTR WSAEINTR
#define perror xperror
void xperror(char *s) {
	fprintf(stderr,"%s: %d\n",s,GetLastError());
}
#else
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#endif
#include <memory.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>

#if defined(sun) && defined(__GNUC__)
int socket(int,int,int);
int shutdown(int,int);
int close(int);
int bind(int,struct sockaddr*,int);
int listen(int,int);
void bzero(char*,int);
int select(int,fd_set *,fd_set*,fd_set*,struct timeval*);
int accept(int,struct sockaddr*,int*);
int connect(int,struct sockaddr*,int);
int recvfrom(int,char*,int,int,struct sockaddr*,int*);
/*void perror(char*); SLOWLARIS HASS*/
/*int sendto(int,char*,int,int,struct sockaddr*,int); SLOWLARIS HASS*/
#endif

#ifdef hpux
/* redefinition... to avoid prototype in <time.h> */
#define FD_CAST int
#endif

#ifdef sgi
void bzero(void*,int);
#endif

#ifndef FD_CAST
#define FD_CAST fd_set
#endif

extern int errno;

struct relay {
	char	*inbuf,*outbuf;
	int	infd,outfd,incur,outcur,insize,outsize;
	struct	sockaddr_in	inaddr,outaddr;
	int	state;
#define STATE_ACCEPTED		0
#define STATE_OK		1
	int	flags;
#define FLAG_EOF_USER		1
#define FLAG_EOF_TARGET		2
#define FLAG_CLOSED_TARGET	4
#define FLAG_CLOSED_USER	8
} *relays = NULL;
int nrofrelays = 0;

void*
xmalloc(int size) {
	void*x;

	x=malloc(size);
	if (!x && size) {
		fprintf(stderr,"Out of memory, exiting.\n");
		exit(1);
	}
	return x;
}

void*
xrealloc(void *y,int size) {
	void*x;

	x=realloc(y,size);
	if (!x && size) {
		fprintf(stderr,"Out of memory, exiting.\n");
		exit(1);
	}
	return x;
}

static int
fd_make_nonblocking(int fd) {
  int     isnonblock=0;

#ifdef FIONBIO
  if (!isnonblock) {
    int     b;
    b=1;
    if (-1==ioctl(fd,FIONBIO,&b)) {
      perror("ioctl FIONBIO");
    } else
      isnonblock=1;
  }
#endif
#ifdef O_NDELAY
  if (!isnonblock) {
    int     flags;

    if (-1==(flags=fcntl(fd,F_GETFL))) {
      perror("fcntl F_GETFL");
    } else {
      flags|=O_NDELAY;
      if (-1==fcntl(fd,F_SETFL,flags)) {
        perror("fcntl F_SETFL  O_NDELAY");
      } else
        isnonblock=1;
    }
  }
#endif
#ifdef O_NONBLOCK
  if (!isnonblock) {
    int     flags;

    if (-1==(flags=fcntl(fd,F_GETFL))) {
      perror("fcntl F_GETFL");
    } else {
      flags|=O_NONBLOCK;
      if (-1==fcntl(fd,F_SETFL,flags)) {
        perror("fcntl F_SETFL  O_NONBLOCK");
      } else
        isnonblock=1;
    }
  }
#endif
  return isnonblock;
}

void
clean_connection(struct relay *relay) {
  if (!relay) return;
  if (relay->outfd>=0) {
    if (-1==close(relay->outfd))
      perror("close");
    relay->outfd=-1;
  }
  if (relay->infd>=0) {
    if (-1==close(relay->infd))
      perror("close");
    relay->infd=-1;
  }
  free(relay->outbuf);free(relay->inbuf);
  memcpy(relay,relay+1,sizeof(struct relay)*(nrofrelays-(relay-relays)-1));
  relays = xrealloc(relays,sizeof(struct relay)*(--nrofrelays));
}

void
main(argc,argv)
int argc;
char  **argv;
{
  int i,j,res;
  int acfd;
  struct  sockaddr_in acsa;
  char	readbuf[1000],relaystring[1000];
  struct  in_addr targetaddr;
#ifdef _WIN32 
  {
	WSADATA wsad;
	
	WSAStartup(0x0101,&wsad);
  }
#else
  close(0);
  close(1);
#endif
#ifdef SIGPIPE
  signal(SIGPIPE,SIG_IGN);
#endif
#ifdef SIGHUP
  signal(SIGHUP,SIG_IGN); /* don't terminate on session detach */
#endif
  strcpy(relaystring,FAILMESSAGE);
  if (-1==(acfd=socket(PF_INET,SOCK_STREAM,0))) {
    perror("socket(accept_socket)");
    exit(1);
  }

  acsa.sin_family=AF_INET;
  acsa.sin_port=htons(LISTENPORT);
  acsa.sin_addr.s_addr=INADDR_ANY;
#ifdef SO_REUSEADDR
  {
    int reuseit=1;
    if (-1==setsockopt(acfd,SOL_SOCKET,SO_REUSEADDR,(char*)&reuseit,sizeof(reuseit)))
	perror("setsockopt SOL_SOCKET SO_REUSEADDR");
  }
#endif
  if (-1==bind(acfd,(struct sockaddr*)&acsa,sizeof(struct sockaddr_in))) {
    perror("bind");
    exit(1);
  }
  /* 5 is usual the maximum anyway */
  if (-1==listen(acfd,5)) {
    perror("listen");
    exit(1);
  }
  while (1) {
    fd_set  readfds,writefds;
    int width;

    width=3;
    if (acfd>=width)
      width=acfd+1;
restart_select:
    FD_ZERO(&readfds);FD_ZERO(&writefds);
    FD_SET(acfd,&readfds);
    for (i=nrofrelays;i--;) {
      struct relay *relay = relays+i;

      /* both sides closed? -> clean */
      if ((relay->flags & (FLAG_CLOSED_TARGET|FLAG_CLOSED_USER)) ==
      	(FLAG_CLOSED_TARGET|FLAG_CLOSED_USER)
      ) {
      	clean_connection(relay);
	continue;
      }
      /* transmitted all stuff left to user? -> close */
      if ((relay->flags&(FLAG_CLOSED_TARGET|FLAG_EOF_TARGET))&&(!relay->outcur)) {
	clean_connection(relay);
	continue;
      }
      /* transmitted all stuff left to target? -> close */
      if ((relay->flags&(FLAG_CLOSED_USER|FLAG_EOF_USER))&&(!relay->incur)) {
	clean_connection(relay);
	continue;
      }

      if (relay->outfd>=0) {
        /*need to do that... else it will cause load 1*/
        if (relay->incur)
          FD_SET(relay->outfd,&writefds);
	if (!(relay->flags & FLAG_EOF_TARGET))
	    FD_SET(relay->outfd,&readfds);
        if (relay->outfd>=width)
          width=relay->outfd+1;
      }
      if (relay->infd>=0) {
        /*need to do that... else it will cause load 1*/
        if (relay->outcur)
          FD_SET(relay->infd,&writefds);
	if (!(relay->flags & FLAG_EOF_USER))
          FD_SET(relay->infd,&readfds);
        if (relay->infd>=width)
          width=relay->infd+1;
      }
    }
    if (-1==select(   width,
          (FD_CAST*)&readfds,
          (FD_CAST*)&writefds,
          NULL,/*no exceptfds.*/
          0)
    ) {
      if (errno!=EINTR)
        perror("select");
      else
        goto  restart_select;
    }
    if (FD_ISSET(acfd,&readfds)) {
      int afd;
      int aclen;
      struct  sockaddr_in conaddr;
      struct relay *relay = NULL;

      aclen=sizeof(struct sockaddr_in);
      if (-1==(afd=accept(acfd,(struct sockaddr*)&conaddr,&aclen)))
        perror("accept");
      if (relays)
	  relays=(struct relay*)xrealloc(relays,sizeof(struct relay)*(nrofrelays+1));
      else
	  relays=(struct relay*)xmalloc(sizeof(struct relay));
      nrofrelays++;
      relay = relays+(nrofrelays-1);
      relay->inbuf	= xmalloc(DEFAULTSIZE);
      relay->outbuf	= xmalloc(DEFAULTSIZE);
      relay->insize	= DEFAULTSIZE;
      relay->outsize	= DEFAULTSIZE;
      relay->flags	= 0;
      relay->incur	= 0;
      relay->outcur	= 0;
      relay->infd	= afd;
      relay->outfd	= -1;
      relay->state	= STATE_ACCEPTED;
      memcpy(&relay->inaddr,&conaddr,sizeof(struct sockaddr_in));
      if (nrofrelays>=MAXUSERS) {
        strcpy(relay->outbuf,relaystring);
	relay->outcur = strlen(relaystring)+1;
	relay->state = STATE_OK;
	relay->flags = FLAG_CLOSED_TARGET;
      }
#ifdef SO_LINGER
      {
	struct linger sol;
	sol.l_linger = 5;
	sol.l_onoff = 1;
	if (-1==setsockopt(acfd,SOL_SOCKET,SO_LINGER,(char*)&sol,sizeof(sol)))
	    perror("setsockopt SOL_SOCKET SO_LINGER");
      }
#endif
    }
    for (i=nrofrelays;i--;) {
      struct relay *relay = relays+i;

      if ((relay->infd>=0) && FD_ISSET(relay->infd,&readfds)) {
        do {
          if (-1==(res=read(relay->infd,readbuf,1000))) {
            if (errno==EINTR)
              break;
            /* user side has broken the connection */
	    close(relay->infd);relay->infd=-1;
	    relay->flags |= FLAG_CLOSED_USER;
            break;
          }
          break;
        } while (1);
        if (res==0) {
	  /* we read the End Of File marker. but we still have to write 
	   * the rest of the text
	   */
	  relay->flags |= FLAG_EOF_USER;
        }
        if (res>0) {
          readbuf[res]='\0';
          while (relay->incur+res>=relay->insize) {
            relay->inbuf=xrealloc(relay->inbuf,relay->insize*2);
            relay->insize*=2;
          }
          memcpy(relay->inbuf+relay->incur,readbuf,res+1);
          relay->incur+=res;
        }
        if (	(relay->outfd==-1) &&
		(relay->state==STATE_ACCEPTED) &&
		memchr(relay->inbuf,'\n',relay->incur)
        ) {
          char  sendbuf[200];
          struct  hostent *hp;
          char  *s,*nextchar,*tmp;
          int port;

          s = memchr(relay->inbuf,'\n',relay->incur);
          if (!s)
            continue;
          *s='\0';
          nextchar=s+1;
          if ((s=memchr(relay->inbuf,'\r',(s-relay->inbuf))))
            *s='\0';


	  relay->state = STATE_OK;

          tmp = (char*)xmalloc(strlen(relay->inbuf));
          if (2!=sscanf(relay->inbuf,"relay %s %d",
            tmp,&port
          )) {
            if (!sscanf(relay->inbuf,"relay %s",tmp)) {

              free(tmp);
              /* we avoid telling potential hackers how to use this relay */
	      sprintf(relay->outbuf,"550 Bad syntax. Go away.\n",tmp);
	      relay->outcur = strlen(relay->outbuf);
	      relay->flags = FLAG_CLOSED_TARGET;
              continue;
            } else
              port = DEFAULTPORT;
          }
          hp=gethostbyname(tmp);
          if (!hp) {/* not found */
	    sprintf(relay->outbuf,"No hostentry for '%s'!\n",tmp);
            free(tmp);
	    relay->outcur = strlen(relay->outbuf);
	    relay->flags = FLAG_CLOSED_TARGET;
            continue;
          }
          memcpy(&targetaddr,hp->h_addr_list[0],sizeof(struct in_addr));
          relay->outaddr.sin_family=AF_INET;
          relay->outaddr.sin_port=htons(port);
          memcpy(&(relay->outaddr.sin_addr),&targetaddr,4);
          strcpy(sendbuf,RELAYHEADER);
          relay->outcur=strlen(sendbuf);
          memcpy(relay->outbuf,sendbuf,strlen(sendbuf)+1);
          if (-1==(relay->outfd=socket(PF_INET,SOCK_STREAM,0)))
            perror("socket(connect_socket)");
#ifndef _WIN32
          (void)fd_make_nonblocking(relay->outfd);
#endif
          if (  (-1==connect( relay->outfd,
              (struct sockaddr*)&(relay->outaddr),
              sizeof(struct sockaddr_in))
#ifdef _WIN32
				) && (WSAGetLastError()!=WSAEINPROGRESS)
#else
				) && (errno!=EINPROGRESS)
#endif
          ) {
            sprintf(readbuf,"Connect to %s failed: %s\n",tmp,strerror(errno));
            perror("connect");
	    close(relay->outfd);relay->outfd=-1;
	    relay->state = STATE_OK;
	    relay->flags |= FLAG_CLOSED_TARGET;
	    strcpy(relay->outbuf,readbuf);
	    relay->outsize = strlen(readbuf)+1;
            free(tmp);
            continue;
          }
          free(tmp);
#ifdef SEND_REMOTEIP
          /* only useful if you want to tell the
           * remotemud the _real_ host the caller
           * is calling from
           */
          tmphp=gethostbyaddr(
            (char*)(&(conaddr.sin_addr)),
            sizeof(struct in_addr),
            AF_INET
          );
          if (!tmphp) {
            sprintf(sendbuf,"remoteip %s %s\n",
              inet_ntoa(conaddr.sin_addr),
              inet_ntoa(conaddr.sin_addr)
            );
          } else {
            sprintf(sendbuf,"remoteip %s %s\n",
              inet_ntoa(conaddr.sin_addr),
              tmphp->h_name
            );
          }
          memcpy(relay->inbuf,sendbuf,strlen(sendbuf)+1);
          relay->incur=strlen(sendbuf);
#else
          relay->inbuf[0]='\0';
          relay->incur=0;
#endif
        }
      }
      if ((relay->outfd>=0) && FD_ISSET(relay->outfd,&readfds)) {
        do {
          if (-1==(res=read(relay->outfd,readbuf,1000))) {
            if (errno==EINTR)
              continue;
            /* the mudside has broken the
             * connection. we still have
             * to transmit the rest of
             * the text
             */
            close(relay->outfd);relay->outfd=-1;
	    relay->flags |= FLAG_CLOSED_TARGET;
            break;
          }
          break;
        } while (1);
        if (res==0) {
	  /* we read the End Of File marker. but we still have to write 
	   * the rest of the text
	   */
	  relay->flags |= FLAG_EOF_TARGET;
	}
        if (res>0) {
          /* 0 is not automagically appended. */
          readbuf[res]='\0';
          while (relay->outcur+res>=relay->outsize) {
            relay->outbuf=xrealloc(relay->outbuf,relay->outsize*2);
            relay->outsize*=2;
          }
          memcpy(relay->outbuf+relay->outcur,readbuf,res+1);
          relay->outcur+=res;
        }
      }
      if ((relay->infd>=0) && FD_ISSET(relay->infd,&writefds)) {
        j=relay->outcur;
        if (-1==(res=write(relay->infd,relay->outbuf,j))) {
          if (errno!=EINTR) {
	    close(relay->infd);relay->infd=-1;
	    relay->flags |= FLAG_CLOSED_USER;
          }
        }
        if (res>0) {
          memcpy(relay->outbuf,relay->outbuf+res,relay->outcur-res);
          relay->outcur-=res;
        }
      }
      if ((relay->outfd>=0) && FD_ISSET(relay->outfd,&writefds)) {
        j=relay->incur;
        if (-1==(res=write(relay->outfd,relay->inbuf,j))) {
          if (errno!=EINTR) {
            close(relay->outfd);relay->outfd=-1;
	    relay->flags |= FLAG_CLOSED_TARGET;
          }
        }
        if (res>0) {
          memcpy(relay->inbuf,relay->inbuf+res,relay->incur-res);
          relay->incur-=res;
        }
      }
    }
  }
}
