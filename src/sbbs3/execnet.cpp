/* execnet.cpp */

/* Synchronet command shell/module TCP/IP Network functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2001 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "cmdshell.h"

#define TIMEOUT_SOCK_READLINE	30	/* seconds */
#define TIMEOUT_SOCK_LISTEN		30	/* seconds */
#define TIMEOUT_FTP_RESPONSE	300	/* seconds */

int sbbs_t::exec_net(csi_t* csi)
{
	char	str[512],rsp[512],buf[1025],ch,*p,**pp,**pp1,**pp2;
	ushort	w;
	uint 	i;
	BOOL	rd;
	long	*lp,*lp1,*lp2;
	time_t	start;

	switch(*(csi->ip++)) {	/* sub-op-code stored as next byte */
		case CS_SOCKET_OPEN:
			lp=getintvar(csi,*(long *)csi->ip);
			csi->ip+=4;
			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;
			if(csi->sockets>=MAX_SOCKETS)
				return(0);
			if(lp!=NULL) {

				SOCKET sock=open_socket(SOCK_STREAM);
				if(sock!=INVALID_SOCKET) {

					SOCKADDR_IN	addr;

					memset(&addr,0,sizeof(addr));
					addr.sin_addr.s_addr = htonl(startup->telnet_interface);
					addr.sin_family = AF_INET;

					if((i=bind(sock, (struct sockaddr *) &addr, sizeof (addr)))!=0) {
						csi->socket_error=ERROR_VALUE;
						close_socket(sock);
						return(0);
					}

					*lp=sock;

					for(i=0;i<csi->sockets;i++)
						if(!csi->socket[i])
							break;
					csi->socket[i]=*lp;
					if(i==csi->sockets)
						csi->sockets++;
					csi->logic=LOGIC_TRUE; 
				} 
			}
			return(0);
		case CS_SOCKET_CLOSE:
			lp=getintvar(csi,*(long *)csi->ip);
			csi->ip+=4;
			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;
			if(lp && *lp) {
				csi->logic=close_socket((SOCKET)*lp);
				csi->socket_error=ERROR_VALUE;
				for(i=0;i<csi->sockets;i++)
					if(csi->socket[i]==*lp)
						csi->socket[i]=0; 
				*lp=0;
			}
			return(0);
		case CS_SOCKET_CHECK:
			lp=getintvar(csi,*(long *)csi->ip);
			csi->ip+=4;
			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(lp==NULL || *lp==INVALID_SOCKET) 
				return(0);

			if(socket_check(*lp,NULL)==TRUE)
				csi->logic=LOGIC_TRUE;
			else
				csi->socket_error=ERROR_VALUE;
				
			return(0);
		case CS_SOCKET_CONNECT:
			lp=getintvar(csi,*(long *)csi->ip);		/* socket */
			csi->ip+=4;

			pp=getstrvar(csi,*(long *)csi->ip);		/* address */
			csi->ip+=4;

			w=*(ushort *)csi->ip;					/* port */
			csi->ip+=2;

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp || !*lp || !pp || !*pp || !w)
				return(0);

			ulong ip_addr;

			if((ip_addr=resolve_ip(*pp))==INADDR_NONE)
				return(0);

			SOCKADDR_IN	addr;

			memset(&addr,0,sizeof(addr));
			addr.sin_addr.s_addr = ip_addr;
			addr.sin_family = AF_INET;
			addr.sin_port   = htons(w);

			if((i=connect(*lp, (struct sockaddr *)&addr, sizeof(addr)))!=0) {
				csi->socket_error=ERROR_VALUE;
				return(0);
			}
			csi->logic=LOGIC_TRUE;
			return(0);
		case CS_SOCKET_ACCEPT:
			lp1=getintvar(csi,*(long *)csi->ip);		/* socket */
			csi->ip+=4;
			csi->socket_error=0;
			/* TODO */
			return(0);
		case CS_SOCKET_NREAD:
			lp1=getintvar(csi,*(long *)csi->ip);		/* socket */
			csi->ip+=4;
			lp2=getintvar(csi,*(long *)csi->ip);		/* var */
			csi->ip+=4;

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp1 || !lp2)
				return(0);

			if(ioctlsocket(*lp1, FIONREAD, (ulong*)lp2)==0) 
				csi->logic=LOGIC_TRUE;
			else
				csi->socket_error=ERROR_VALUE;
			return(0);
		case CS_SOCKET_PEEK:
		case CS_SOCKET_READ:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			pp=getstrvar(csi,*(long *)csi->ip);			/* buffer */
			csi->ip+=4;
			w=*(ushort *)csi->ip;						/* length */
			csi->ip+=2;					

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp || !pp)
				return(0);

			if(w<1 || w>sizeof(buf)-1)
				w=sizeof(buf)-1;

			if((i=recv(*lp,buf,w
				,*(csi->ip-13)==CS_SOCKET_PEEK ? MSG_PEEK : 0))>0) {
				csi->logic=LOGIC_TRUE;
				buf[i]=0;
				if(csi->etx) {
					p=strchr(buf,csi->etx);
					if(p) *p=0; 
				}
				*pp=copystrvar(csi,*pp,buf); 
			} else
				csi->socket_error=ERROR_VALUE;
			return(0);
		case CS_SOCKET_READLINE:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			pp=getstrvar(csi,*(long *)csi->ip);			/* buffer */
			csi->ip+=4;
			w=*(ushort *)csi->ip;						/* length */
			csi->ip+=2;					

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp || !pp)
				return(0);

			if(w<1 || w>sizeof(buf)-1)
				w=sizeof(buf)-1;

			start=time(NULL);
			for(i=0;i<w;) {

				if(!online)
					return(1);

				if(!socket_check(*lp,&rd))
					return(0);

				if(!rd) {
					if(time(NULL)-start>TIMEOUT_SOCK_READLINE) {
						lprintf("!socket_readline: timeout (%d) exceeded"
							,TIMEOUT_SOCK_READLINE);
						return(0);
					}
					mswait(1);
					continue;
				}

				if(recv(*lp, &ch, 1, 0)!=1) {
					csi->socket_error=ERROR_VALUE;
					return(0);
				}

				if(ch=='\n' && i>=1) 
					break;

				buf[i++]=ch;
			}
			if(i>0 && buf[i-1]=='\r')
				buf[i-1]=0;
			else
				buf[i]=0;

			if(csi->etx) {
				p=strchr(buf,csi->etx);
				if(p) *p=0; 
			}
			*pp=copystrvar(csi,*pp,buf); 
			csi->logic=LOGIC_TRUE;
			return(0);
		case CS_SOCKET_WRITE:	
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			pp=getstrvar(csi,*(long *)csi->ip);			/* buffer */
			csi->ip+=4;

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp || !pp || !(*pp))
				return(0);

			if(sendsocket(*lp,*pp,strlen(*pp))>0)
				csi->logic=LOGIC_TRUE;
			else
				csi->socket_error=ERROR_VALUE;
			return(0);

		/* FTP Functions */
		case CS_FTP_LOGIN:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			pp1=getstrvar(csi,*(long *)csi->ip);		/* username */
			csi->ip+=4;
			pp2=getstrvar(csi,*(long *)csi->ip);		/* password */
			csi->ip+=4;

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp || !pp1 || !pp2)
				return(0);

			if(!ftp_cmd(csi,*lp,NULL,rsp))
				return(0);

			if(atoi(rsp)!=220)
				return(0);

			sprintf(str,"USER %s",*pp1);

			if(!ftp_cmd(csi,*lp,str,rsp))
				return(0);
			
			if(atoi(rsp)==331) { /* Password needed */
				sprintf(str,"PASS %s",*pp2);
				if(!ftp_cmd(csi,*lp,str,rsp))
					return(0);
			}

			if(atoi(rsp)==230)	/* Login successful */
				csi->logic=LOGIC_TRUE;
			return(0);

		case CS_FTP_LOGOUT:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp)
				return(0);

			if(!ftp_cmd(csi,*lp,"QUIT",rsp))
				return(0);

			if(atoi(rsp)==221)	/* Logout successful */
				csi->logic=LOGIC_TRUE;
			return(0);

		case CS_FTP_PWD:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;
			if(!lp)
				return(0);

			if(!ftp_cmd(csi,*lp,"PWD",rsp))
				return(0);

			if(atoi(rsp)==257)	/* pathname */
				csi->logic=LOGIC_TRUE;
			return(0);

		case CS_FTP_CWD:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			pp=getstrvar(csi,*(long *)csi->ip);			/* path */
			csi->ip+=4;

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;
			if(!lp || !pp)
				return(0);
			
			sprintf(str,"CWD %s",*pp);
			if(!ftp_cmd(csi,*lp,str,rsp))
				return(0);

			if(atoi(rsp)==250)
				csi->logic=LOGIC_TRUE;

			return(0);

		case CS_FTP_DIR:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			pp=getstrvar(csi,*(long *)csi->ip);			/* path */
			csi->ip+=4;

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp || !pp)
				return(0);

			if(ftp_get(csi,*lp,*pp,NULL /* unused */, true /* DIR */)==true)
				csi->logic=LOGIC_TRUE;

			return(0);

		case CS_FTP_DELETE:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			pp=getstrvar(csi,*(long *)csi->ip);			/* path */
			csi->ip+=4;

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;
			if(!lp || !pp)
				return(0);
			
			sprintf(str,"DELE %s",*pp);
			if(!ftp_cmd(csi,*lp,str,rsp))
				return(0);

			if(atoi(rsp)==250)
				csi->logic=LOGIC_TRUE;

			return(0);


		case CS_FTP_GET:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			pp1=getstrvar(csi,*(long *)csi->ip);		/* src path */
			csi->ip+=4;
			pp2=getstrvar(csi,*(long *)csi->ip);		/* dest path */
			csi->ip+=4;

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp || !pp1 || !pp2)
				return(0);

			if(ftp_get(csi,*lp,*pp1,*pp2)==true)
				csi->logic=LOGIC_TRUE;
			
			return(0);

		case CS_FTP_PUT:
			lp=getintvar(csi,*(long *)csi->ip);			/* socket */
			csi->ip+=4;
			pp1=getstrvar(csi,*(long *)csi->ip);		/* src path */
			csi->ip+=4;
			pp2=getstrvar(csi,*(long *)csi->ip);		/* dest path */
			csi->ip+=4;

			csi->logic=LOGIC_FALSE;
			csi->socket_error=0;

			if(!lp || !pp1 || !pp2)
				return(0);

			if(ftp_put(csi,*lp,*pp1,*pp2)==true)
				csi->logic=LOGIC_TRUE;
			
			return(0);


		default:
			errormsg(WHERE,ERR_CHK,"net sub-instruction",*(csi->ip-1));
			return(0); 
	}
}

/* FTP Command/Response function */
bool sbbs_t::ftp_cmd(csi_t* csi, SOCKET sock, char* cmdsrc, char* rsp)
{
	char	cmd[512];
	int		len;
	BOOL	data_avail;
	time_t	start;

	if(cmdsrc!=NULL) {
		sprintf(cmd,"%s\r\n",cmdsrc);

		if(csi->ftp_mode&CS_FTP_ECHO_CMD)
			bputs(cmd);

		len=strlen(cmd);
		if(sendsocket(sock,cmd,len)!=len) {
			csi->socket_error=ERROR_VALUE;
			return(FALSE);
		}
	}

	if(rsp!=NULL) {

		int		rd;
		char	ch;

		while(1) {
			rd=0;

			start=time(NULL);
			while(rd<500) {

				if(!online)
					return(FALSE);

				if(!socket_check(sock,&data_avail))
					return(FALSE);

				if(!data_avail) {
					if(time(NULL)-start>TIMEOUT_FTP_RESPONSE) {
						lprintf("!ftp_cmd: TIMEOUT_FTP_RESPONSE (%d) exceeded"
							,TIMEOUT_FTP_RESPONSE);
						return(FALSE);
					}
					mswait(1);
					continue;
				}

				if(recv(sock, &ch, 1, 0)!=1) {
					csi->socket_error=ERROR_VALUE;
					return(FALSE);
				}

				if(ch=='\n' && rd>=1) 
					break;

				rsp[rd++]=ch;
			}
			rsp[rd-1]=0;
			if(csi->ftp_mode&CS_FTP_ECHO_RSP)
				bprintf("%s\r\n",rsp);
			if(rsp[0]!=' ' && rsp[3]!='-')
				break;
		}
	}		

	return(TRUE);
}

SOCKET sbbs_t::ftp_data_sock(csi_t* csi, SOCKET ctrl_sock, SOCKADDR_IN* addr)
{
	char		cmd[512];
	char		rsp[512];
	char*		p;
	socklen_t	addr_len;
	SOCKET		data_sock;
	int			ip_b[4];
	int			port_b[2];
	union {
		DWORD	dw;
		BYTE	b[sizeof(DWORD)];
	} ip_addr;
	union {
		WORD	w;
		BYTE	b[sizeof(WORD)];
	} port;

	if(csi->ftp_mode&CS_FTP_ASCII)
		strcpy(cmd,"TYPE A");
	else	/* BINARY */
		strcpy(cmd,"TYPE I");
	if(!ftp_cmd(csi,ctrl_sock,cmd,rsp) 
		|| atoi(rsp)!=200) {
		return(INVALID_SOCKET);
	}

	if((data_sock=open_socket(SOCK_STREAM))==INVALID_SOCKET) {
		csi->socket_error=ERROR_VALUE;
		return(INVALID_SOCKET);
	}

	memset(addr,0,sizeof(SOCKADDR_IN));
	addr->sin_addr.s_addr = htonl(startup->telnet_interface);
	addr->sin_family = AF_INET;

	if(bind(data_sock, (struct sockaddr *)addr,sizeof(SOCKADDR_IN))!= 0) {
		csi->socket_error=ERROR_VALUE;
		close_socket(data_sock);
		return(INVALID_SOCKET);
	}
	
	if(csi->ftp_mode&CS_FTP_PASV) {

		if(!ftp_cmd(csi,ctrl_sock,"PASV",rsp) 
			|| atoi(rsp)!=227 /* PASV response */) {
			close_socket(data_sock);
			return(INVALID_SOCKET);
		}

		p=strchr(rsp,'(');
		if(p==NULL) {
			close_socket(data_sock);
			return(INVALID_SOCKET);
		}
		p++;
		if(sscanf(p,"%u,%u,%u,%u,%u,%u"
			,&ip_b[0],&ip_b[1],&ip_b[2],&ip_b[3]
			,&port_b[0],&port_b[1])!=6) {
			close_socket(data_sock);
			return(INVALID_SOCKET);
		}

		ip_addr.b[0]=ip_b[0];	ip_addr.b[1]=ip_b[1];
		ip_addr.b[2]=ip_b[2];	ip_addr.b[3]=ip_b[3];
		port.b[0]=port_b[0];	port.b[1]=port_b[1];

		addr->sin_addr.s_addr=ip_addr.dw;
		addr->sin_port=port.w;

	} else {	/* Normal (Active) FTP */

		addr_len=sizeof(SOCKADDR_IN);
		if(getsockname(data_sock, (struct sockaddr *)addr,&addr_len)!=0) {
			csi->socket_error=ERROR_VALUE;
			close_socket(data_sock);
			return(INVALID_SOCKET);
		} 

		SOCKADDR_IN ctrl_addr;
		addr_len=sizeof(ctrl_addr);
		if(getsockname(ctrl_sock, (struct sockaddr *)&ctrl_addr,&addr_len)!=0) {
			csi->socket_error=ERROR_VALUE;
			close_socket(data_sock);
			return(INVALID_SOCKET);
		} 

		if(listen(data_sock, 1)!= 0) {
			csi->socket_error=ERROR_VALUE;
			close_socket(data_sock);
			return(INVALID_SOCKET);
		}

		ip_addr.dw=ntohl(ctrl_addr.sin_addr.s_addr);
		port.w=ntohs(addr->sin_port);
		sprintf(cmd,"PORT %u,%u,%u,%u,%u,%u"
			,ip_addr.b[3]
			,ip_addr.b[2]
			,ip_addr.b[1]
			,ip_addr.b[0]
			,port.b[1]
			,port.b[0]
			);

		if(!ftp_cmd(csi,ctrl_sock,cmd,rsp) 
			|| atoi(rsp)!=200 /* PORT response */) {
			close_socket(data_sock);
			return(INVALID_SOCKET);
		}

	}

	return(data_sock);
}

bool sbbs_t::ftp_get(csi_t* csi, SOCKET ctrl_sock, char* src, char* dest, bool dir)
{
	char		cmd[512];
	char		rsp[512];
	char		buf[4097];
	int			rd;
	int			result;
	BOOL		data_avail;
	ulong		total=0;
	SOCKET		data_sock;
	SOCKADDR_IN	addr;
	socklen_t	addr_len;
	FILE*		fp=NULL;
	struct timeval	tv;
	fd_set			socket_set;

	if((data_sock=ftp_data_sock(csi, ctrl_sock, &addr))==INVALID_SOCKET)
		return(false);

	if(csi->ftp_mode&CS_FTP_PASV) {

#if 0	// Debug
		bprintf("Connecting to %s:%hd\r\n"
			,inet_ntoa(addr.sin_addr)
			,ntohs(addr.sin_port));
#endif

		if(connect(data_sock,(struct sockaddr *)&addr,sizeof(addr))!=0) {
			csi->socket_error=ERROR_VALUE;
			close_socket(data_sock);
			return(false);
		}
	}

	if(dir)
		sprintf(cmd,"LIST %s",src);
	else
		sprintf(cmd,"RETR %s",src);

	if(!ftp_cmd(csi,ctrl_sock,cmd,rsp) 
		|| atoi(rsp)!=150 /* Open data connection */) {
		close_socket(data_sock);
		return(false);
	}

	if(!(csi->ftp_mode&CS_FTP_PASV)) {	/* Normal (Active) FTP */

		/* Setup for select() */
		tv.tv_sec=TIMEOUT_SOCK_LISTEN;
		tv.tv_usec=0;

		FD_ZERO(&socket_set);
		FD_SET(data_sock,&socket_set);

		result=select(data_sock+1,&socket_set,NULL,NULL,&tv);
		if(result<1) {
			csi->socket_error=ERROR_VALUE;
			closesocket(data_sock);
			return(false);
		}

		SOCKET accept_sock;

		addr_len=sizeof(addr);
		if((accept_sock=accept_socket(data_sock,(struct sockaddr*)&addr,&addr_len))
			==INVALID_SOCKET) {
			csi->socket_error=ERROR_VALUE;
			closesocket(data_sock);
			return(false);
		}

		close_socket(data_sock);
		data_sock=accept_sock;
	}

	if(!dir)
		if((fp=fopen(dest,"wb"))==NULL) {
			close_socket(data_sock);
			return(false);
		}

	while(online) {

		if(!socket_check(ctrl_sock,NULL))
			break; /* Control connection lost */

		if(!socket_check(data_sock,&data_avail))
			break; /* Data connection lost */

		if(!data_avail) {
			mswait(1);
			continue;
		}

		if((rd=recv(data_sock, buf, sizeof(buf)-1, 0))<1)
			break;

		if(dir) {
			buf[rd]=0;
			bputs(buf);
		} else
			fwrite(buf,1,rd,fp);

		total+=rd;
		
		if(!dir && csi->ftp_mode&CS_FTP_HASH)
			outchar('#');
	}

	if(!dir && csi->ftp_mode&CS_FTP_HASH) {
		CRLF;
	}

	if(fp!=NULL)
		fclose(fp);

	close_socket(data_sock);

	if(!ftp_cmd(csi,ctrl_sock,NULL,rsp) 
		|| atoi(rsp)!=226 /* Download complete */)
		return(false);

	bprintf("ftp: %lu bytes received.\r\n", total);

	return(true);
}

bool sbbs_t::ftp_put(csi_t* csi, SOCKET ctrl_sock, char* src, char* dest)
{
	char		cmd[512];
	char		rsp[512];
	char		buf[4097];
	int			rd;
	int			result;
	ulong		total=0;
	SOCKET		data_sock;
	SOCKADDR_IN	addr;
	socklen_t	addr_len;
	FILE*		fp=NULL;
	bool		error=false;
	struct timeval	tv;
	fd_set			socket_set;

	if((data_sock=ftp_data_sock(csi, ctrl_sock, &addr))==INVALID_SOCKET)
		return(false);

	if(csi->ftp_mode&CS_FTP_PASV) {

#if 0	// Debug
		bprintf("Connecting to %s:%hd\r\n"
			,inet_ntoa(addr.sin_addr)
			,ntohs(addr.sin_port));
#endif

		if(connect(data_sock,(struct sockaddr *)&addr,sizeof(addr))!=0) {
			csi->socket_error=ERROR_VALUE;
			close_socket(data_sock);
			return(false);
		}
	}

	if((fp=fopen(src,"rb"))==NULL) {
		close_socket(data_sock);
		return(false);
	}

	sprintf(cmd,"STOR %s",dest);

	if(!ftp_cmd(csi,ctrl_sock,cmd,rsp) 
		|| atoi(rsp)!=150 /* Open data connection */) {
		close_socket(data_sock);
		return(false);
	}

	if(!(csi->ftp_mode&CS_FTP_PASV)) {	/* Normal (Active) FTP */

		/* Setup for select() */
		tv.tv_sec=TIMEOUT_SOCK_LISTEN;
		tv.tv_usec=0;

		FD_ZERO(&socket_set);
		FD_SET(data_sock,&socket_set);

		result=select(data_sock+1,&socket_set,NULL,NULL,&tv);
		if(result<1) {
			csi->socket_error=ERROR_VALUE;
			closesocket(data_sock);
			return(false);
		}

		SOCKET accept_sock;

		addr_len=sizeof(addr);
		if((accept_sock=accept_socket(data_sock,(struct sockaddr*)&addr,&addr_len))
			==INVALID_SOCKET) {
			csi->socket_error=ERROR_VALUE;
			closesocket(data_sock);
			return(false);
		}

		close_socket(data_sock);
		data_sock=accept_sock;
	}

	while(online && !feof(fp)) {

		rd=fread(buf,sizeof(char),sizeof(buf),fp);
		if(rd<1) /* EOF or READ error */
			break;

		if(!socket_check(ctrl_sock,NULL))
			break; /* Control connection lost */

		if(sendsocket(data_sock,buf,rd)<1) {
			error=true;
			break;
		}

		total+=rd;
		
		if(csi->ftp_mode&CS_FTP_HASH)
			outchar('#');
	}

	if(csi->ftp_mode&CS_FTP_HASH) {
		CRLF;
	}

	fclose(fp);

	close_socket(data_sock);

	if(!ftp_cmd(csi,ctrl_sock,NULL,rsp) 
		|| atoi(rsp)!=226 /* Upload complete */)
		return(false);

	if(!error)
		bprintf("ftp: %lu bytes sent.\r\n", total);

	return(!error);
}
