/* telgate.cpp */

/* Synchronet telnet gateway routines */

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
#include "telnet.h" 

void sbbs_t::telnet_gate(char* destaddr, ulong mode)
{
	char*	p;
	uchar	buf[512];
	int		i;
	int		rd;
	ulong	l;
	bool	gotline;
	ushort	port;
	ulong	ip_addr;
	ulong	save_console;
	SOCKET	remote_socket;
	SOCKADDR_IN	addr;

	if(mode&TG_RLOGIN)
		port=513;
	else
		port=IPPORT_TELNET;

	p=strchr(destaddr,':');
	if(p!=NULL) {
		*p=0;
		port=atoi(p+1);
	}

	ip_addr=resolve_ip(destaddr);
	if(!ip_addr) {
		lprintf("!Failed to resolve address: %s",destaddr);
		bprintf("!Failed to resolve address: %s\r\n",destaddr);
		return;
	}

    if((remote_socket = open_socket(SOCK_STREAM)) == INVALID_SOCKET) {
		errormsg(WHERE,ERR_OPEN,"socket",0);
		return;
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = htonl(startup->telnet_interface);
	addr.sin_family = AF_INET;

	if((i=bind(remote_socket, (struct sockaddr *) &addr, sizeof (addr)))!=0) {
		lprintf("!ERROR %d (%d) binding to socket %d",i, ERROR_VALUE, remote_socket);
		bprintf("!ERROR %d (%d) binding to socket\r\n",i, ERROR_VALUE);
		close_socket(remote_socket);
		return;
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	if((i=connect(remote_socket, (struct sockaddr *)&addr, sizeof(addr)))!=0) {
		lprintf("!ERROR %d (%d) connecting to server: %s"
			,i,ERROR_VALUE, destaddr);
		bprintf("!ERROR %d (%d) connecting to server: %s\r\n"
			,i,ERROR_VALUE, destaddr);
		close_socket(remote_socket);
		return;
	}

	l=1;

	if((i = ioctlsocket(remote_socket, FIONBIO, &l))!=0) {
		lprintf("!ERROR %d (%d) disabling socket blocking"
			,i, ERROR_VALUE);
		close_socket(remote_socket);
		return;
	}

	lprintf("Node %d %s gate to %s port %d on socket %d"
		,cfg.node_num
		,mode&TG_RLOGIN ? "RLogin" : "Telnet"
		,destaddr,port,remote_socket);

	if(mode&(TG_PASSTHRU|TG_RLOGIN))
		telnet_mode|=TELNET_MODE_GATE;	// Pass-through telnet commands

	if(!(mode&TG_CTRLKEYS))
		console|=CON_RAW_IN;

	if(mode&TG_RLOGIN) {
		p=(char*)buf;
		*(p++)=0;
		strcpy(p,useron.alias);
		p+=strlen(p)+1;
		strcpy(p,useron.name);
		p+=strlen(p)+1;
		strcpy(p,"vt100/57600");
		p+=strlen(p)+1;
		l=p-(char*)buf;
		sendsocket(remote_socket,(char*)buf,l);
	}

	while(online) {
		gettimeleft();
		rd=RingBufRead(&inbuf,buf,sizeof(buf));
		if(rd) {
			if(!(telnet_mode&TELNET_MODE_BIN_RX)) {
				if(*buf==0x1d) { // ^]
					save_console=console;
					console&=~CON_RAW_IN;	// Allow Ctrl-U/Ctrl-P
					CRLF;
					while(online) {
						SYNC;
						mnemonics("\1n\r\n\1h\1bTelnet Gate: \1y~D\1wisconnect, "
							"\1y~E\1wcho toggle, \1y~L\1wist Users, \1y~P\1wrivate message, "
							"\1y~Q\1wuit: ");
						switch(getkeys("DELPQ",0)) {
							case 'D':
								closesocket(remote_socket);
								break;
							case 'E':
								mode^=TG_ECHO;
								bprintf(text[EchoIsNow]
									,mode&TG_ECHO
									? text[ON]:text[OFF]);
								continue;
							case 'L':
								whos_online(true);
								continue;
							case 'P':
								nodemsg();
								continue;
						}
						break;
					}
					attr(LIGHTGRAY);
					console=save_console;
				}
				gotline=false;
				if(mode&TG_LINEMODE && buf[0]!='\r') {
					ungetkey(buf[0]);
					l=K_CHAT;
					if(!(mode&TG_ECHO))
						l|=K_NOECHO;
					rd=getstr((char*)buf,sizeof(buf)-1,l);
					if(!rd)
						continue;
					strcat((char*)buf,crlf);
					rd+=2;
					gotline=true;
				}
				if(mode&TG_CRLF && buf[rd-1]=='\r')
					buf[rd++]='\n';
				if(!gotline && mode&TG_ECHO) {
					RingBufWrite(&outbuf,buf,rd);
					sem_post(&output_sem);
				}
			}
			if((i=sendsocket(remote_socket,(char*)buf,rd))<0) {
				lprintf("!TELGATE ERROR %d sending on socket %d",ERROR_VALUE,remote_socket);
				break;
			}
		}
		rd=recv(remote_socket,(char*)buf,sizeof(buf),0);
		if(rd<0) {
			if(ERROR_VALUE==EWOULDBLOCK) {
				if(mode&TG_NODESYNC) {
					SYNC;
				} else {
					// Check if the node has been interrupted
					getnodedat(cfg.node_num,&thisnode,0);
					if(thisnode.misc&NODE_INTR)
						break;
				}
				mswait(1);
				continue;
			}
			lprintf("!TELGATE ERROR %d receiving on socket %d",ERROR_VALUE,remote_socket);
			break;
		}
		if(!rd) {
			lprintf("Node %d Telnet gate disconnected",cfg.node_num);
			break;
		}
		RingBufWrite(&outbuf,buf,rd);
		sem_post(&output_sem);
	}
	console&=~CON_RAW_IN;
	telnet_mode&=~TELNET_MODE_GATE;

	/* Disable Telnet Terminal Echo */
	send_telnet_cmd(TELNET_WILL,TELNET_ECHO);

	close_socket(remote_socket);

	lprintf("Node %d Telnet gate to %s finished",cfg.node_num,destaddr);
}

