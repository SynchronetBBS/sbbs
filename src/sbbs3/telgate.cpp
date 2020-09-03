/* Synchronet telnet gateway routines */

/* $Id: telgate.cpp,v 1.46 2019/08/04 17:49:51 deuce Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

void sbbs_t::telnet_gate(char* destaddr, ulong mode, char* client_user_name, char* server_user_name, char* term_type)
{
	char*	p;
	uchar	buf[512];
	int		i;
	int		rd;
	uint	attempts;
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
	if(ip_addr==INADDR_NONE) {
		lprintf(LOG_NOTICE,"!TELGATE Failed to resolve address: %s",destaddr);
		bprintf("!Failed to resolve address: %s\r\n",destaddr);
		return;
	}

    if((remote_socket = open_socket(PF_INET, SOCK_STREAM, client.protocol)) == INVALID_SOCKET) {
		errormsg(WHERE,ERR_OPEN,"socket",0);
		return;
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = htonl(startup->outgoing4.s_addr);
	addr.sin_family = AF_INET;

	if((i=bind(remote_socket, (struct sockaddr *) &addr, sizeof (addr)))!=0) {
		lprintf(LOG_NOTICE,"!TELGATE ERROR %d (%d) binding to socket %d",i, ERROR_VALUE, remote_socket);
		bprintf("!ERROR %d (%d) binding to socket\r\n",i, ERROR_VALUE);
		close_socket(remote_socket);
		return;
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	if((i=connect(remote_socket, (struct sockaddr *)&addr, sizeof(addr)))!=0) {
		lprintf(LOG_NOTICE,"!TELGATE ERROR %d (%d) connecting to server: %s"
			,i,ERROR_VALUE, destaddr);
		bprintf("!ERROR %d (%d) connecting to server: %s\r\n"
			,i,ERROR_VALUE, destaddr);
		close_socket(remote_socket);
		return;
	}

	l=1;

	if((i = ioctlsocket(remote_socket, FIONBIO, &l))!=0) {
		lprintf(LOG_NOTICE,"!TELGATE ERROR %d (%d) disabling socket blocking"
			,i, ERROR_VALUE);
		close_socket(remote_socket);
		return;
	}

	lprintf(LOG_INFO,"Node %d %s gate to %s port %u on socket %d"
		,cfg.node_num
		,mode&TG_RLOGIN ? "RLogin" : "Telnet"
		,destaddr,port,remote_socket);

	if(!(mode&TG_CTRLKEYS))
		console|=CON_RAW_IN;

	if(mode&TG_RLOGIN) {
		if (client_user_name == NULL)
			client_user_name = (mode&TG_RLOGINSWAP) ? useron.name : useron.alias;
		if (server_user_name == NULL)
			server_user_name = (mode&TG_RLOGINSWAP) ? useron.alias : useron.name;
		p=(char*)buf;
		*(p++)=0;
		p+=sprintf(p,"%s",client_user_name);
		p++;	// Add NULL
		p+=sprintf(p,"%s",server_user_name);
		p++;	// Add NULL
		if(term_type!=NULL)
			p+=sprintf(p,"%s",term_type);
		else
			p+=sprintf(p,"%s/%lu",terminal, cur_rate);
		p++;	// Add NULL
		l=p-(char*)buf;
		sendsocket(remote_socket,(char*)buf,l);
		mode|=TG_NOLF;	/* Send LF (to remote host) when Telnet client sends CRLF (when not in binary mode) */
	}

	/* This is required for gating to Unix telnetd */
	if(mode&TG_NOTERMTYPE)
		request_telnet_opt(TELNET_DONT,TELNET_TERM_TYPE, 3000);	// Re-negotiation of terminal type

	/* Text/NVT mode by default */
	request_telnet_opt(TELNET_DONT,TELNET_BINARY_TX, 3000);

	if(!(telnet_mode&TELNET_MODE_OFF) && (mode&TG_PASSTHRU))
		telnet_mode|=TELNET_MODE_GATE;	// Pass-through telnet commands

	while(online) {
		if(!(mode&TG_NOCHKTIME))
			gettimeleft();
		rd=RingBufRead(&inbuf,buf,sizeof(buf)-1);
		if(rd) {
#if 0
			if(memchr(buf,TELNET_IAC,rd)) {
				char dump[2048];
				dump[0];
				p=dump;
				for(int i=0;i<rd;i++)
					p+=sprintf(p,"%u ",buf[i]);
				lprintf(LOG_DEBUG,"Node %d Telnet cmd from client: %s", cfg.node_num, dump);
			}
#endif
			if(telnet_remote_option[TELNET_BINARY_TX]!=TELNET_WILL) {
				if(*buf==CTRL_CLOSE_BRACKET) {
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
				else if(*buf<' ' && (mode&TG_CTRLKEYS))
					handle_ctrlkey(*buf, K_NONE);
				gotline=false;
				if((mode&TG_LINEMODE) && buf[0]!='\r') {
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
				if((mode&TG_CRLF) && buf[rd-1]=='\r')
					buf[rd++]='\n';
				else if((mode&TG_NOLF) && buf[rd-1]=='\n')
					rd--;
				if(!gotline && (mode&TG_ECHO) && rd) {
					RingBufWrite(&outbuf,buf,rd);
				}
			} /* Not Telnet Binary mode */
			if(rd > 0) {
				for(attempts=0;attempts<60 && online; attempts++) /* added retry loop here, Jan-20-2003 */
				{
					if((i=sendsocket(remote_socket,(char*)buf,rd))>=0)
						break;
					if(ERROR_VALUE!=EWOULDBLOCK)
						break;
					mswait(500);
				} 
				if(i<0) {
					lprintf(LOG_NOTICE,"!TELGATE ERROR %d sending on socket %d",ERROR_VALUE,remote_socket);
					break;
				}
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
				YIELD();
				continue;
			}
			lprintf(LOG_NOTICE,"!TELGATE ERROR %d receiving on socket %d",ERROR_VALUE,remote_socket);
			break;
		}
		if(!rd) {
			lprintf(LOG_INFO,"Node %d Telnet gate disconnected",cfg.node_num);
			break;
		}
#if 0
		if(memchr(buf,TELNET_IAC,rd)) {
			char dump[2048];
			dump[0];
			p=dump;
			for(int i=0;i<rd;i++)
				p+=sprintf(p,"%u ",buf[i]);
			lprintf(LOG_DEBUG,"Node %d Telnet cmd from server: %s", cfg.node_num, dump);
		}
#endif
		RingBufWrite(&outbuf,buf,rd);
	}
	console&=~CON_RAW_IN;
	telnet_mode&=~TELNET_MODE_GATE;

	/* Disable Telnet Terminal Echo */
	request_telnet_opt(TELNET_WILL,TELNET_ECHO);

	close_socket(remote_socket);

	lprintf(LOG_INFO,"Node %d Telnet gate to %s finished",cfg.node_num,destaddr);
}
