/* Synchronet telnet gateway routines */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "telnet.h"

struct TelnetProxy
{
	SOCKET sock;
	uint8_t	local_option[0x100]{};
	uint8_t	remote_option[0x100]{};
	sbbs_t* sbbs;
	size_t cmdlen = 0;
	uint8_t cmd[64];
	uint8_t outbuf[1024];

	TelnetProxy(SOCKET sock_, sbbs_t* sbbs_)
		: sock{sock_}, sbbs{sbbs_} {}

	void send(uint8_t cmd, uint8_t opt)
	{
		char buf[16];

		if (cmd < TELNET_WILL) {
			if(sbbs->startup->options&BBS_OPT_DEBUG_TELNET)
				sbbs->lprintf(LOG_DEBUG, "%s: %s", __FUNCTION__, telnet_cmd_desc(cmd));
			sprintf(buf, "%c%c", TELNET_IAC, cmd);
			if (::sendsocket(sock, buf, 2) != 2)
				lprintf(LOG_WARNING, "%s: Failed to send command: %s", __FUNCTION__, telnet_cmd_desc(cmd));
		}
		else {
			if(sbbs->startup->options&BBS_OPT_DEBUG_TELNET)
				sbbs->lprintf(LOG_DEBUG, "%s: %s %s", __FUNCTION__, telnet_cmd_desc(cmd), telnet_opt_desc(opt));
			sprintf(buf, "%c%c%c", TELNET_IAC, cmd, opt);
			if (::sendsocket(sock, buf, 3) != 3)
				sbbs->lprintf(LOG_WARNING, "%s: Failed to send command: %s %s", __FUNCTION__, telnet_cmd_desc(cmd), telnet_opt_desc(opt));
		}
	}

	void request_opt(uint8_t cmd, uint8_t opt)
	{
		if ((cmd == TELNET_DO) || (cmd == TELNET_DONT)) { /* remote option */
			if (remote_option[opt] == telnet_opt_ack(cmd))
				return;                           /* already set in this mode, do nothing */
			remote_option[opt] = telnet_opt_ack(cmd);
		}
		else {                                            /* local option */
			if (local_option[opt] == telnet_opt_ack(cmd))
				return;                           /* already set in this mode, do nothing */
			local_option[opt] = telnet_opt_ack(cmd);
		}
		send(cmd, opt);
	}

	uint8_t* recv(uint8_t *inbuf, size_t inlen, ssize_t& outlen)
	{
		uint8_t  command;
		uint8_t  option;
		uint8_t *first_int = NULL;

		if (inlen < 1) {
			outlen = 0;
			return inbuf; /* no length? No interpretation */
		}

		first_int = (uint8_t *)memchr(inbuf, TELNET_IAC, inlen);
		if ((cmdlen == 0) && (first_int == NULL)) {
			outlen = inlen;
			return inbuf; /* no interpretation needed */
		}

		if (cmdlen == 0 /* If we haven't returned and cmdlen==0 then first_int is not NULL */) {
			outlen = first_int - inbuf;
			memcpy(outbuf, inbuf, outlen);
		}
		else {
			outlen = 0;
		}

		for (size_t i = outlen; i < inlen; i++) {
			if (remote_option[TELNET_BINARY_TX] != TELNET_WILL) {
				if ((cmdlen == 1) && (cmd[0] == '\r')) {
					outbuf[outlen++] = '\r';
					if ((inbuf[i] != 0) && (inbuf[i] != TELNET_IAC))
						outbuf[outlen++] = inbuf[i];
					cmdlen = 0;
					if (inbuf[i] != TELNET_IAC)
						continue;
				}
				if ((inbuf[i] == '\r') && (cmdlen == 0)) {
					cmd[cmdlen++] = '\r';
					continue;
				}
			}

			if ((inbuf[i] == TELNET_IAC) && (cmdlen == 1)) { /* escaped 255 */
				cmdlen = 0;
				outbuf[outlen++] = TELNET_IAC;
				continue;
			}
			if ((inbuf[i] == TELNET_IAC) || cmdlen) {
				if (cmdlen < sizeof(cmd))
					cmd[cmdlen++] = inbuf[i];

				command = cmd[1];
				option = cmd[2];

				if ((cmdlen >= 2) && (command == TELNET_SB)) {
					if ((inbuf[i] == TELNET_SE)
						&& (cmd[cmdlen - 2] == TELNET_IAC)) {
											/* sub-option terminated */
						if ((option == TELNET_TERM_TYPE) && (cmd[3] == TELNET_TERM_SEND)) {
							char        buf[32];
							int         len = snprintf(buf, sizeof buf, "%c%c%c%c%s%c%c",
									TELNET_IAC, TELNET_SB,
									TELNET_TERM_TYPE, TELNET_TERM_IS,
									sbbs->term_type(),
									TELNET_IAC, TELNET_SE);

							if (::sendsocket(sock, buf, len) != len)
								lprintf(LOG_WARNING, "%s: Failed to send TERM_TYPE command", __FUNCTION__);
							request_opt(TELNET_WILL, TELNET_NEGOTIATE_WINDOW_SIZE);
						}
						cmdlen = 0;
					}
				}
				else if ((cmdlen == 2) && (inbuf[i] < TELNET_WILL)) {
					cmdlen = 0;
				}
				else if (cmdlen >= 3) { /* telnet option negotiation */
					if(sbbs->startup->options&BBS_OPT_DEBUG_TELNET)
						sbbs->lprintf(LOG_DEBUG, "%s: %s %s", __FUNCTION__, telnet_cmd_desc(command), telnet_opt_desc(option));

					if ((command == TELNET_DO) || (command == TELNET_DONT)) { /* local options */
						if (local_option[option] != command) {
							switch (option) {
								case TELNET_BINARY_TX:
								case TELNET_TERM_TYPE:
								case TELNET_SUP_GA:
								case TELNET_NEGOTIATE_WINDOW_SIZE:
									local_option[option] = command;
									send(telnet_opt_ack(command), option);
									break;
								default: // unsupported local options
									if (command == TELNET_DO) /* NAK */
										send(telnet_opt_nak(command),
											option);
									break;
							}
						}

						if ((command == TELNET_DO) && (option == TELNET_NEGOTIATE_WINDOW_SIZE)) {
							uint8_t buf[32];

							buf[0] = TELNET_IAC;
							buf[1] = TELNET_SB;
							buf[2] = TELNET_NEGOTIATE_WINDOW_SIZE;
							buf[3] = (sbbs->cols >> 8) & 0xff;
							buf[4] = sbbs->cols & 0xff;
							buf[5] = (sbbs->rows >> 8) & 0xff;
							buf[6] = sbbs->rows & 0xff;
							buf[7] = TELNET_IAC;
							buf[8] = TELNET_SE;
							if (sbbs->startup->options&BBS_OPT_DEBUG_TELNET)
								sbbs->lprintf(LOG_DEBUG, "%s: Window Size is %u x %u", __FUNCTION__, sbbs->cols, sbbs->rows);
							if (::sendsocket(sock, (char *)buf, 9) != 9)
								lprintf(LOG_WARNING, "%s: Failed to send Window Size command", __FUNCTION__);
						}
					}
					else { /* WILL/WONT (remote options) */
						if (remote_option[option] != command) {
							switch (option) {
								case TELNET_BINARY_TX:
								case TELNET_ECHO:
								case TELNET_TERM_TYPE:
								case TELNET_SUP_GA:
								case TELNET_NEGOTIATE_WINDOW_SIZE:
									remote_option[option] = command;
									send(telnet_opt_ack(command), option);
									break;
								default: // unsupported remote options
									if (command == TELNET_WILL) /* NAK */
										send(telnet_opt_nak(command), option);
									break;
							}
						}
					}

					cmdlen = 0;
				}
			}
			else {
				outbuf[outlen++] = inbuf[i];
			}
		}
		return outbuf;
	}
}; // struct TelnetProxy

/*****************************************************************************/
// Expands Sole-LF to CR/LF
/*****************************************************************************/
static uint8_t* expand_lf(uint8_t* inbuf, size_t inlen, uint8_t* outbuf, size_t& outlen)
{
	size_t i, j;

	if(outlen < 1 || outbuf == nullptr)
		return nullptr;
	for(i = j = 0; i < inlen && j < (outlen - 1); ++i) {
		if(inbuf[i] == '\n' && (i == 0 || inbuf[i - 1] != '\r'))
			outbuf[j++] = '\r';
		outbuf[j++] = inbuf[i];
	}
	outlen = j;
    return outbuf;
}

bool sbbs_t::telnet_gate(char* destaddr, uint mode, unsigned timeout, str_list_t send_strings, char* client_user_name, char* server_user_name, char* term_type)
{
	char*	p;
	uchar	buf[512];
	int		i;
	ssize_t	rd;
	uint	attempts;
	u_long	l;
	bool	gotline;
	ushort	port;
	uint	ip_addr;
	uint	save_console;
	SOCKET	remote_socket;
	SOCKADDR_IN	addr;
	TelnetProxy* proxy = nullptr;

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
		return false;
	}

    if((remote_socket = open_socket(PF_INET, SOCK_STREAM, client.protocol)) == INVALID_SOCKET) {
		errormsg(WHERE,ERR_OPEN,"socket",0);
		return false;
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = htonl(startup->outgoing4.s_addr);
	addr.sin_family = AF_INET;

	if((i=bind(remote_socket, (struct sockaddr *) &addr, sizeof (addr)))!=0) {
		lprintf(LOG_NOTICE,"!TELGATE ERROR %d (%d) binding to socket %d",i, ERROR_VALUE, remote_socket);
		bprintf("!ERROR %d (%d) binding to socket\r\n",i, ERROR_VALUE);
		close_socket(remote_socket);
		return false;
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	l=1;
	if((i = ioctlsocket(remote_socket, FIONBIO, &l))!=0) {
		lprintf(LOG_NOTICE,"!TELGATE ERROR %d (%d) disabling socket blocking"
			,i, ERROR_VALUE);
		close_socket(remote_socket);
		return false;
	}

	lprintf(LOG_INFO,"Node %d %s gate to %s port %u on socket %d"
		,cfg.node_num
		,mode&TG_RLOGIN ? "RLogin" : "Telnet"
		,destaddr,port,remote_socket);

	if((i=nonblocking_connect(remote_socket, (struct sockaddr *)&addr, sizeof(addr), timeout))!=0) {
		lprintf(LOG_NOTICE,"!TELGATE ERROR %d (%d) connecting to server: %s"
			,i,ERROR_VALUE, destaddr);
		bprintf("!ERROR %d (%d) connecting to server: %s\r\n"
			,i,ERROR_VALUE, destaddr);
		close_socket(remote_socket);
		return false;
	}

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
			p+=sprintf(p,"%s/%u",terminal, cur_rate);
		p++;	// Add NULL
		l=p-(char*)buf;
		if(sendsocket(remote_socket,(char*)buf,l) != (ssize_t)l)
			lprintf(LOG_WARNING, "Error %d sending %lu bytes to server: %s", ERROR_VALUE, l, destaddr);
		mode|=TG_NOLF;	/* Send LF (to remote host) when Telnet client sends CRLF (when not in binary mode) */
	} else if(!(mode & TG_RAW)) {
		proxy = new TelnetProxy(remote_socket, this);
		if(!(mode & TG_ECHO))
			proxy->request_opt(TELNET_DO, TELNET_ECHO);
	}

	/* This is required for gating to Unix telnetd */
	if(mode&TG_NOTERMTYPE)
		request_telnet_opt(TELNET_DONT,TELNET_TERM_TYPE, 3000);	// Re-negotiation of terminal type

	/* Text/NVT mode by default */
	request_telnet_opt(TELNET_DONT,TELNET_BINARY_TX, 3000);
	if(!(telnet_mode&TELNET_MODE_OFF) && (mode&TG_PASSTHRU))
		telnet_mode|=TELNET_MODE_GATE;	// Pass-through telnet commands

	if(send_strings != NULL) {
		for(i = 0; send_strings[i] != NULL; ++i)
			sendsocket(remote_socket, send_strings[i], strlen(send_strings[i]));
	}

	while(online) {
		if(!(mode&TG_NOCHKTIME))
			gettimeleft();
		rd=RingBufRead(&inbuf,buf,sizeof(buf)-1);
		if(rd) {
			if(telnet_remote_option[TELNET_BINARY_TX]!=TELNET_WILL) {
				if(*buf==CTRL_CLOSE_BRACKET) {
					save_console=console;
					console&=~CON_RAW_IN;	// Allow Ctrl-U/Ctrl-P
					CRLF;
					while(online) {
						sync();
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
									? text[On]:text[Off]);
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
					*buf = '\0';
					rd=getstr((char*)buf,sizeof(buf)-1,l);
					if(!rd)
						continue;
					SAFECAT(buf,crlf);
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
					sync();
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
		uint8_t* p = buf;
		if(!(mode & (TG_RLOGIN | TG_PASSTHRU | TG_RAW)))
			p = proxy->recv(buf, rd, rd);
		if(mode & TG_EXPANDLF) {
			uint8_t expanded_buf[sizeof(buf) * 2];
			size_t expanded_len = sizeof(expanded_buf);
			expand_lf(p, rd, expanded_buf, expanded_len);
			RingBufWrite(&outbuf, expanded_buf, expanded_len);
		} else
			RingBufWrite(&outbuf,p,rd);
	}
	console&=~CON_RAW_IN;
	telnet_mode&=~TELNET_MODE_GATE;

	/* Disable Telnet Terminal Echo */
	request_telnet_opt(TELNET_WILL,TELNET_ECHO);

	close_socket(remote_socket);

	lprintf(LOG_INFO,"Node %d Telnet gate to %s finished",cfg.node_num,destaddr);
	return true;
}
