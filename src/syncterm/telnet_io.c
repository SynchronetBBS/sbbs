/* $Id$ */

#include <stdlib.h>
#include <string.h>

#include "term.h"

#include "genwrap.h"
#include "sockwrap.h"
#include "telnet.h"
#include "gen_defs.h"
#include "bbslist.h"
#include "conn.h"
#include "uifcinit.h"

#define TELNET_TERM_MAXLEN	40

uint	telnet_cmdlen=0;
uchar	telnet_cmd[64];
char	terminal[TELNET_TERM_MAXLEN+1];
uchar	telnet_local_option[0x100];
uchar	telnet_remote_option[0x100];

#define putcom(buf,len)	send(conn_socket, buf, len, 0)

static void send_telnet_cmd(uchar cmd, uchar opt)
{
	char buf[16];
	
	if(cmd<TELNET_WILL) {
		sprintf(buf,"%c%c",TELNET_IAC,cmd);
		putcom(buf,2);
	} else {
		sprintf(buf,"%c%c%c",TELNET_IAC,cmd,opt);
		putcom(buf,3);
	}
}

static void request_telnet_opt(uchar cmd, uchar opt)
{
	if(cmd==TELNET_DO || cmd==TELNET_DONT) {	/* remote option */
		if(telnet_remote_option[opt]==telnet_opt_ack(cmd))
			return;	/* already set in this mode, do nothing */
		telnet_remote_option[opt]=telnet_opt_ack(cmd);
	} else {	/* local option */
		if(telnet_local_option[opt]==telnet_opt_ack(cmd))
			return;	/* already set in this mode, do nothing */
		telnet_local_option[opt]=telnet_opt_ack(cmd);
	}
	send_telnet_cmd(cmd,opt);
}

static BYTE* telnet_interpret(BYTE* inbuf, int inlen, BYTE* outbuf, int *outlen)
{
	BYTE	command;
	BYTE	option;
	BYTE*   first_iac;
	int 	i;

	if(inlen<1) {
		*outlen=0;
		return(inbuf);	// no length? No interpretation
	}

    first_iac=(BYTE*)memchr(inbuf, TELNET_IAC, inlen);

    if(!telnet_cmdlen	&& first_iac==NULL) {
        *outlen=inlen;
        return(inbuf);	// no interpretation needed
    }

    if(first_iac!=NULL) {
   		*outlen=first_iac-inbuf;
	    memcpy(outbuf, inbuf, *outlen);
    } else
    	*outlen=0;

    for(i=*outlen;i<inlen;i++) {
        if(inbuf[i]==TELNET_IAC && telnet_cmdlen==1) { /* escaped 255 */
            telnet_cmdlen=0;
            outbuf[(*outlen)++]=TELNET_IAC;
            continue;
        }
        if(inbuf[i]==TELNET_IAC || telnet_cmdlen) {

			if(telnet_cmdlen<sizeof(telnet_cmd))
				telnet_cmd[telnet_cmdlen++]=inbuf[i];

			command	= telnet_cmd[1];
			option	= telnet_cmd[2];

			if(telnet_cmdlen>=2 && command==TELNET_SB) {
				if(inbuf[i]==TELNET_SE 
					&& telnet_cmd[telnet_cmdlen-2]==TELNET_IAC) {
					/* sub-option terminated */
					if(option==TELNET_TERM_TYPE && telnet_cmd[3]==TELNET_TERM_SEND) {
						BYTE buf[32];
						int len=sprintf(buf,"%c%c%c%cANSI%c%c"
							,TELNET_IAC,TELNET_SB
							,TELNET_TERM_TYPE,TELNET_TERM_IS
							,TELNET_IAC,TELNET_SE);
						putcom(buf,len);
						request_telnet_opt(TELNET_WILL, TELNET_NEGOTIATE_WINDOW_SIZE);
					}
					telnet_cmdlen=0;
				}
			}
            else if(telnet_cmdlen==2 && inbuf[i]<TELNET_WILL) {
                telnet_cmdlen=0;
            }
            else if(telnet_cmdlen>=3) {	/* telnet option negotiation */
				if(command==TELNET_DO || command==TELNET_DONT) {	/* local options */
					if(telnet_local_option[option]!=command) {
						telnet_local_option[option]=command;
						send_telnet_cmd(telnet_opt_ack(command),option);
					}

					if(command==TELNET_DO && option==TELNET_NEGOTIATE_WINDOW_SIZE) {
						BYTE buf[32];
						buf[0]=TELNET_IAC;
						buf[1]=TELNET_SB;
						buf[2]=TELNET_NEGOTIATE_WINDOW_SIZE;
						buf[3]=(term.width>>8)&0xff;
						buf[4]=term.width&0xff;
						buf[5]=(term.height>>8)&0xff;
						buf[6]=term.height&0xff;
						buf[7]=TELNET_IAC;
						buf[8]=TELNET_SE;
						putcom(buf,9);
					}

				} else { /* WILL/WONT (remote options) */ 
					if(telnet_remote_option[option]!=command) {	

						switch(option) {
							case TELNET_BINARY_TX:
							case TELNET_ECHO:
							case TELNET_TERM_TYPE:
							case TELNET_TERM_SPEED:
							case TELNET_SUP_GA:
							case TELNET_NEGOTIATE_WINDOW_SIZE:
								telnet_remote_option[option]=command;
								send_telnet_cmd(telnet_opt_ack(command),option);
								break;
							default: /* unsupported remote options */
								if(command==TELNET_WILL) /* NAK */
									send_telnet_cmd(telnet_opt_nak(command),option);
								break;
						}
					}
				}

                telnet_cmdlen=0;

            }
        } else
        	outbuf[(*outlen)++]=inbuf[i];
    }
    return(outbuf);
}

int telnet_recv(char *buffer, size_t buflen)
{
	int	r;
	int	avail;
	int rd;
	BYTE *inbuf;

	if(!socket_check(conn_socket, NULL, NULL, 0))
		return(-1);

	if((inbuf=(BYTE *)malloc(buflen))==NULL)
		return(-1);

	if(!ioctlsocket(conn_socket,FIONREAD,(void *)&avail) && avail)
		r=recv(conn_socket,inbuf,avail<(int)buflen?avail:buflen,0);
	else {
		free(inbuf);
		return(0);
	}

	if(r==-1 && (errno==EAGAIN || errno==EINTR || errno==0))	/* WTF? */
		r=0;
	if(r) {
		if(telnet_interpret(inbuf, r, buffer, &r)==inbuf)
			memcpy(buffer, inbuf, r);
	}

	free(inbuf);
	return(r);
}

BYTE* telnet_expand(BYTE* inbuf, size_t inlen, BYTE* outbuf, size_t *newlen)
{
	BYTE*   first_iac;
	ulong	i,outlen;

    first_iac=(BYTE*)memchr(inbuf, TELNET_IAC, inlen);

	if(first_iac==NULL) {	/* Nothing to expand */
		*newlen=inlen;
		return(inbuf);
	}

	outlen=first_iac-inbuf;
	memcpy(outbuf, inbuf, outlen);

    for(i=outlen;i<inlen;i++) {
		if(inbuf[i]==TELNET_IAC)
			outbuf[outlen++]=TELNET_IAC;
		outbuf[outlen++]=inbuf[i];
	}
    *newlen=outlen;
    return(outbuf);
}

int telnet_send(char *buffer, size_t buflen, unsigned int timeout)
{
	int sent=0;
	int	ret;
	int	i;
	BYTE *outbuf;
	BYTE *sendbuf;

	if((outbuf=(BYTE *)malloc(buflen*2))==NULL)
		return(-1);
	sendbuf=telnet_expand(buffer, buflen, outbuf, &buflen);
	while(sent<(int)buflen) {
		if(!socket_check(conn_socket, NULL, &i, timeout)) {
			free(outbuf);
			return(-1);
		}
		if(!i) {
			free(outbuf);
			return(-1);
		}			
		ret=send(conn_socket,sendbuf+sent,buflen-sent,0);
		if(ret==-1) {
			switch(errno) {
				case EAGAIN:
				case ENOBUFS:
					SLEEP(1);
					break;
				default:
					free(outbuf);
					return(-1);
			}
		}
		else
			sent+=ret;
	}
	free(outbuf);
	return(0);
}

int telnet_close(void)
{
	return(closesocket(conn_socket));
}

int telnet_connect(char *addr, int port, char *ruser, char *passwd)
{
	HOSTENT *ent;
	SOCKADDR_IN	saddr;
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
	conn_socket=socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
	if(conn_socket==INVALID_SOCKET) {
		uifcmsg("Cannot create socket!",	"`Unable to create socket`\n\n"
											"Your system is either dangerously low on resources, or there"
											"is a problem with your TCP/IP stack.");
		return(-1);
	}
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_addr.s_addr = neta;
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons(port);
	
	if(connect(conn_socket, (struct sockaddr *)&saddr, sizeof(saddr))) {
		char str[LIST_ADDR_MAX+20];

		telnet_close();
		sprintf(str,"Cannot connect to %s!",addr);
		uifcmsg(str,	"`Unable to connect`\n\n"
						"Cannot connect to the remote system... it is down or unreachable.");
		return(-1);
	}

	return(0);
}
