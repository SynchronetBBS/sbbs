/**
 * relayd.c -- a relay daemon (using one targethost/port)
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

/* relayd.c (c) 1996,1997 Marcus Meissner <marcus@mud.de> */

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
#define RELAYHEADER "Relayd (c) Marcus Meissner\r\n"

#include <stdio.h>
#ifdef _WIN32
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <winsock.h>
#include <io.h>
#define ioctl ioctlsocket
#define write(h,buf,size) send(h,buf,size,0)
#define read(a,b,c) recv(a,b,c,0)
#define close _lclose
#define EINPROGRESS WSAEWOULDBLOCK
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
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

char  *inbuf[MAXUSERS],*outbuf[MAXUSERS];
int insize[MAXUSERS],outsize[MAXUSERS];
int incur[MAXUSERS],outcur[MAXUSERS];
int outfd[MAXUSERS],infd[MAXUSERS];
struct  sockaddr_in inaddr[MAXUSERS],outaddr[MAXUSERS];

#ifdef _WIN32
#define perror xperror
void xperror(char *str) {
	fprintf(stderr,"%s: %d\n",str,GetLastError());
}
#endif

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
clean_connection(i)
int i;
{
  if (outfd[i]>=0) {
    if (-1==close(outfd[i]))
      perror("close");
    outfd[i]=-1;
  }
  if (infd[i]>=0) {
    if (-1==close(infd[i]))
      perror("close");
    infd[i]=-1;
  }
  incur[i]=outcur[i]=0;
  outbuf[i][0]=inbuf[i][0]='\0';
}

void
usage() {
   fprintf(stderr,"Usage: relayd <listenport> <targethost> [<targetport>]\n");
}

void
main(argc,argv)
int argc;
char  **argv;
{
  int i,j,res;
  int acfd;
  struct  sockaddr_in acsa;
  char  readbuf[1000],relaystring[1000];
  struct  in_addr targetaddr;
  struct  hostent *hp;
  char *targethost;
  int  port,targetport;
  
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
  switch (argc) {
  default:
  case 1:
  case 2:
    usage();
    exit(1);
  case 3:
    if (!sscanf(argv[1],"%d",&port)) {
      usage();
      exit(1);
    }
    targethost = argv[2];
    targetport = 23;
    break;
  case 4:
    if (!sscanf(argv[1],"%d",&port)) {
      usage();exit(1);
    }
    if (!sscanf(argv[3],"%d",&targetport)) {
      usage();exit(1);
    }
    targethost=argv[2];
    break;
  }
  strcpy(relaystring,FAILMESSAGE);
  if (-1==(acfd=socket(PF_INET,SOCK_STREAM,0))) {
    perror("socket(accept_socket)");
    exit(1);
  }
  for (i=MAXUSERS;i--;) {
    inbuf[i]=malloc(10000*sizeof(char));
    outbuf[i]=malloc(10000*sizeof(char));
    inbuf[i][0]='\0';outbuf[i][0]='\0';

    insize[i]=10000;incur[i]=0;

    outsize[i]=10000;outcur[i]=0;
    outfd[i]=infd[i]=-1;
  }

  acsa.sin_family=AF_INET;
  acsa.sin_port=htons(port);
  acsa.sin_addr.s_addr=INADDR_ANY;
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
    for (i=MAXUSERS;i--;) {
      if (outfd[i]>=0) {
        /*need to do that... else it will cause load 1*/
        if (incur[i])
          FD_SET(outfd[i],&writefds);
        FD_SET(outfd[i],&readfds);
        if (outfd[i]>=width)
          width=outfd[i]+1;
      }
      if (infd[i]>=0) {
        /*need to do that... else it will cause load 1*/
        if (outcur[i])
          FD_SET(infd[i],&writefds);
        FD_SET(infd[i],&readfds);
        if (infd[i]>=width)
          width=infd[i]+1;
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

      aclen=sizeof(struct sockaddr_in);
      if (-1==(afd=accept(acfd,(struct sockaddr*)&conaddr,&aclen)))
        perror("accept");
      for (i=MAXUSERS;i--;)
        if ((infd[i]==-1) && (outfd[i]==-1))
          break;
      if (i==-1) {
        write(afd,relaystring,strlen(relaystring));
        close(afd);
      } else {
        char  sendbuf[200];
        infd[i]=afd;
        memcpy(&inaddr[i],&conaddr,sizeof(struct sockaddr_in));
        /* outfd setting delayed until we get
         * to the first line
         */
        hp=gethostbyname(targethost);
        if (!hp) {/* not found */
          clean_connection(i);
          continue;
        }
        memcpy(&targetaddr,hp->h_addr_list[0],sizeof(struct in_addr));
        outaddr[i].sin_family=AF_INET;
        outaddr[i].sin_port=htons(targetport);
        memcpy(&(outaddr[i].sin_addr),&targetaddr,4);
        strcpy(sendbuf,RELAYHEADER);
        outcur[i]=strlen(sendbuf);
        memcpy(outbuf[i],sendbuf,strlen(sendbuf)+1);
        if (-1==(outfd[i]=socket(PF_INET,SOCK_STREAM,0)))
          perror("socket(connect_socket)");
#ifndef _WIN32
        (void)fd_make_nonblocking(outfd[i]);
#endif        
		if (  (-1==connect( outfd[i],
            (struct sockaddr*)&outaddr[i],
            sizeof(struct sockaddr_in))
#ifdef _WIN32
        ) && (WSAGetLastError()!=WSAEWOULDBLOCK)
#else  
        ) && (errno!=EINPROGRESS)
#endif
        ) {
#ifdef _WIN32
			sprintf(readbuf,"Connect to %s failed: %d\n",targethost,GetLastError());
#else
          sprintf(readbuf,"Connect to %s failed.\n",targethost);
#endif
          perror("connect");
          if (-1==write(infd[i],readbuf,strlen(readbuf)))
            perror("write");
          clean_connection(i);
          continue;
        }
        inbuf[i][0]='\0';
        incur[i]=0;
      }
    }
    for (i=MAXUSERS;i--;) {
      if ((infd[i]>=0) && FD_ISSET(infd[i],&readfds)) {
        do {
          if (-1==(res=read(infd[i],readbuf,1000))) {
            if (errno==EINTR)
              continue;
            /* user side has broken the connection */
            clean_connection(i);
            break;
          }
          break;
        } while (1);
        if (res==0) /* EOF */
          clean_connection(i);
        if (res>0) {
          readbuf[res]='\0';
          while (incur[i]+res>=insize[i]) {
            inbuf[i]=realloc(inbuf[i],insize[i]*2);
            insize[i]*=2;
          }
          memcpy(inbuf[i]+incur[i],readbuf,res+1);
          incur[i]+=res;
        }
      }
      if ((outfd[i]>=0) && FD_ISSET(outfd[i],&readfds)) {
        do {
          if (-1==(res=read(outfd[i],readbuf,1000))) {
            if (errno==EINTR)
              continue;
            /* the mudside has broken the
             * connection. we still have
             * to transmit the rest of
             * the text
             */
            outfd[i]=-1;
            break;
          }
          break;
        } while (1);
        if (res==0)
          clean_connection(i);
        if (res>0) {
          /* 0 is not automagically appended. */
          readbuf[res]='\0';
          while (outcur[i]+res>=outsize[i]) {
            outbuf[i]=realloc(outbuf[i],outsize[i]*2);
            outsize[i]*=2;
          }
          memcpy(outbuf[i]+outcur[i],readbuf,res+1);
          outcur[i]+=res;
        }
      }
      if ((infd[i]>=0) && FD_ISSET(infd[i],&writefds)) {
        j=outcur[i];
        if (-1==(res=write(infd[i],outbuf[i],j))) {
          if (errno!=EINTR) {
            clean_connection(i);
          }
        }
        if (res>0) {
          memcpy(outbuf[i],outbuf[i]+res,outcur[i]-res);
          outcur[i]-=res;
        }
      }
      if ((outfd[i]>=0) && FD_ISSET(outfd[i],&writefds)) {
        j=incur[i];
        if (-1==(res=write(outfd[i],inbuf[i],j))) {
          if (errno!=EINTR) {
            outfd[i]=-1;
            /*clean_connection(i);*/
          }
        }
        if (res>0) {
          memcpy(inbuf[i],inbuf[i]+res,incur[i]-res);
          incur[i]-=res;
        }
      }
    }
  }
}
