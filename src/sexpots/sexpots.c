/* sexpots.c */

/* Synchronet External Plain Old Telephone System (POTS) support */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2007 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/* ANSI C */
#include <stdio.h>

/* Windows */
#include <winsock.h>

/* xpdev lib */
#include "dirwrap.h"
#include "datewrap.h"
#include "sockwrap.h"
#include "ini_file.h"

/* comio lib */
#include "comio.h"

/* sbbs */
#include "telnet.h"

/* constants */
#define MDM_TILDE_DELAY			500	/* milliseconds */

/* global vars */
char	termtype[TELNET_TERM_MAXLEN+1];
char	termspeed[TELNET_TERM_MAXLEN+1];	/* "tx,rx", max length not defined */
char	revision[16];

char	mdm_init[INI_MAX_VALUE_LEN];
char	mdm_autoans[INI_MAX_VALUE_LEN];
char	mdm_cleanup[INI_MAX_VALUE_LEN];
BOOL	mdm_null=FALSE;
int		mdm_timeout=5;			/* seconds */

char	com_dev[MAX_PATH+1];
HANDLE	com_handle=INVALID_HANDLE_VALUE;
BOOL	com_handle_passed=FALSE;
BOOL	com_alreadyconnected=FALSE;
BOOL	com_hangup=TRUE;
int		dcd_timeout=10;	/* seconds */

BOOL	terminated=FALSE;
BOOL	terminate_after_one_call=FALSE;

SOCKET	sock=INVALID_SOCKET;
char	host[MAX_PATH+1];
ushort	port;

/* stats */
ulong	total_calls=0;
ulong	bytes_sent=0;
ulong	bytes_received=0;

/* .ini over-rideable */
int		log_level=LOG_INFO;
BOOL	pause_on_exit=FALSE;
BOOL	tcp_nodelay=TRUE;

/* telnet stuff */
BOOL	telnet=TRUE;
BOOL	debug_telnet=FALSE;
uchar	telnet_local_option[0x100];
uchar	telnet_remote_option[0x100];
BYTE	telnet_cmd[64];
int		telnet_cmdlen;

/****************************************************************************/
/****************************************************************************/
int usage(const char* fname)
{
	printf("usage: %s [ini file] [options]\n"
		"\nOptions:"
		"\n-null                 No 'AT commands' sent to modem"
		"\n-com <device>         Specify communications port device"
		"\n-live [handle]        Communications port is already open/connected"
		"\n-nohangup             Do not hangup (drop DTR) after call"
		"\n-host <addr | name>   Specify TCP server hostname or IP address"
		"\n-port <number>        Specify TCP port number"
		,getfname(fname));

	return 0;
}

/****************************************************************************/
/****************************************************************************/
int lputs(int level, const char* str)
{
	time_t		t;
	struct tm	tm;
	char		tstr[32];

	if(level>log_level)
		return 0;

	t=time(NULL);
	if(localtime_r(&t,&tm)==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm.tm_mon+1,tm.tm_mday
			,tm.tm_hour,tm.tm_min,tm.tm_sec);

	return printf("%s %s\n", tstr, str);
}

/****************************************************************************/
/****************************************************************************/
int lprintf(int level, char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(lputs(level,sbuf));
}

#ifdef _WINSOCKAPI_

/* Note: Don't call WSACleanup() or TCP session will close! */
WSADATA WSAData;	

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		lprintf(LOG_INFO,"%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		return(TRUE);
	}

    lprintf(LOG_ERR,"WinSock startup ERROR %d", status);
	return(FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)	

#endif

/****************************************************************************/
/****************************************************************************/
BOOL modem_send(COM_HANDLE com_handle, const char* str)
{
	const char* p;

	lprintf(LOG_INFO,"Modem Command: %s", str);
	for(p=str; *p; p++) {
		if(*p=='~') {
			SLEEP(MDM_TILDE_DELAY);
			continue;
		}
		if(!comWriteByte(com_handle,*p))
			return FALSE;
	}
	SLEEP(100);
	comPurgeInput(com_handle);
	return comWriteByte(com_handle, '\r');
}

/****************************************************************************/
/****************************************************************************/
BOOL modem_response(COM_HANDLE com_handle, char *str, size_t maxlen)
{
	BYTE	ch;
	size_t	len=0;
	time_t	start;

	lprintf(LOG_DEBUG,"Waiting for Modem Response ...");
	start=time(NULL);
	while(1){
		if(time(NULL)-start >= mdm_timeout) {
			lprintf(LOG_WARNING,"Modem Response TIMEOUT (%lu seconds)", mdm_timeout);
			return FALSE;
		}
		if(len >= maxlen) {
			lprintf(LOG_WARNING,"Modem Response too long (%u >= %u)"
				,len, maxlen);
			return FALSE;
		}
		if(!comReadByte(com_handle, &ch)) {
			YIELD();
			continue;
		}
		if(ch<' ' && len==0)	/* ignore prepended control chars */
			continue;

		if(ch=='\r') {
			while(comReadByte(com_handle,&ch))	/* eat trailing ctrl chars (e.g. 'LF') */
#if 0
				lprintf(LOG_DEBUG, "eating ch=%02X", ch)
#endif
				;
			break;
		}
		str[len++]=ch;
	}
	str[len]=0;

	return TRUE;
}

/****************************************************************************/
/****************************************************************************/
BOOL modem_command(COM_HANDLE com_handle, const char* cmd)
{
	char resp[128];

	if(!modem_send(com_handle, cmd)) {
		lprintf(LOG_ERR,"ERROR %u sending modem command", COM_ERROR_VALUE);
		return FALSE;
	}

	if(!modem_response(com_handle, resp, sizeof(resp)))
		return FALSE;

	lprintf(LOG_INFO,"Modem Response: %s", resp);

	return TRUE;
}

/****************************************************************************/
/****************************************************************************/
void close_socket(SOCKET* sock)
{
	if(*sock != INVALID_SOCKET) {
		shutdown(*sock,SHUT_RDWR);	/* required on Unix */
		closesocket(*sock);
		*sock=INVALID_SOCKET;
	}
}

/****************************************************************************/
/****************************************************************************/
void cleanup(void)
{
	lprintf(LOG_INFO,"Cleaning up ...");


	if(com_handle!=COM_HANDLE_INVALID) {
		if(!mdm_null && mdm_cleanup[0])
			modem_command(com_handle, mdm_cleanup);
		if(!com_handle_passed)
			comClose(com_handle);
	}

	close_socket(&sock);

#ifdef _WINSOCKAPI_
	WSACleanup();
#endif

	lprintf(LOG_INFO,"Done (handled %lu calls).", total_calls);
	if(pause_on_exit) {
		printf("Hit enter to continue...");
		getchar();
	}
}

/****************************************************************************/
/****************************************************************************/
BOOL wait_for_call(HANDLE com_handle)
{
	char		str[128];
	int			rd;
	BOOL		result=TRUE;
	DWORD		events=0;

	if(com_alreadyconnected)
		return TRUE;

	if(!mdm_null && mdm_init[0]) {
		lprintf(LOG_INFO,"Initializing modem:");
		if(!modem_command(com_handle, mdm_init))
			return FALSE;
	}

	if(!mdm_null && mdm_autoans[0]) {
		lprintf(LOG_INFO,"Setting modem to auto-answer:");
		if(!modem_command(com_handle, mdm_autoans))
			return FALSE;
	}

	lprintf(LOG_INFO,"Waiting for incoming call (Carrier Detect) on %s ...", com_dev);
	while(1) {
		if(terminated)
			return FALSE;
		if(comGetModemStatus(com_handle)&COM_DCD)
			break;
		if((rd=comReadBuf(com_handle, str, sizeof(str)-1, 250)) > 0) {
			str[rd]=0;
			truncsp(str);
			lprintf(LOG_INFO, "Modem Message: %s", str);
		}
	}

	return TRUE;
}

/****************************************************************************/
/****************************************************************************/
u_long resolve_ip(const char *addr)
{
	HOSTENT*	host;
	const char*	p;

	if(*addr==0)
		return((u_long)INADDR_NONE);

	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;
	if(!(*p))
		return(inet_addr(addr));
	if((host=gethostbyname(addr))==NULL) 
		return((u_long)INADDR_NONE);
	return(*((ulong*)host->h_addr_list[0]));
}

/****************************************************************************/
/****************************************************************************/
SOCKET connect_socket(const char* host, ushort port)
{
	SOCKET		sock;
	SOCKADDR_IN	addr;

	if((sock=socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
		lprintf(LOG_ERR,"ERROR %u creating socket", ERROR_VALUE);
		return INVALID_SOCKET;
	}

	lprintf(LOG_DEBUG,"Setting TCP_NODELAY to %d",tcp_nodelay);
	setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char*)&tcp_nodelay,sizeof(tcp_nodelay));

	ZERO_VAR(addr);
	addr.sin_addr.s_addr = resolve_ip(host);
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	lprintf(LOG_INFO,"Connecting to %s port %u", host, port);
	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
		lprintf(LOG_INFO,"Connected from %s to %s port %u on socket %u", com_dev, host, port, sock);
		return sock;
	}

	lprintf(LOG_ERR,"SOCKET ERROR %u connecting to %s port %u", ERROR_VALUE, host, port);
	closesocket(sock);

	return INVALID_SOCKET;
}

BOOL call_terminated;
BOOL input_thread_terminated;

/****************************************************************************/
/****************************************************************************/
void input_thread(void* arg)
{
	BYTE		ch;

	lprintf(LOG_DEBUG,"Input thread started");
	while(!call_terminated) {
		if(!comReadByte(com_handle, &ch)) {
			YIELD();
			continue;
		}
		if(telnet && ch==TELNET_IAC)
			sendsocket(sock, &ch, sizeof(ch));	/* escape Telnet IAC char (255) when in telnet mode */
		sendsocket(sock, &ch, sizeof(ch));
		bytes_received++;
	}
	lprintf(LOG_DEBUG,"Input thread terminated");
	input_thread_terminated=TRUE;
}

static void send_telnet_cmd(uchar cmd, uchar opt)
{
	char buf[16];
	
	if(cmd<TELNET_WILL) {
		if(debug_telnet)
			lprintf(LOG_INFO,"TX Telnet command: %s"
				,telnet_cmd_desc(cmd));
		sprintf(buf,"%c%c",TELNET_IAC,cmd);
		sendsocket(sock,buf,2);
	} else {
		if(debug_telnet)
			lprintf(LOG_INFO,"TX Telnet command: %s %s"
				,telnet_cmd_desc(cmd), telnet_opt_desc(opt));
		sprintf(buf,"%c%c%c",TELNET_IAC,cmd,opt);
		sendsocket(sock,buf,3);
	}
}

void request_telnet_opt(uchar cmd, uchar opt)
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

/****************************************************************************/
/****************************************************************************/
BYTE* telnet_interpret(BYTE* inbuf, int inlen, BYTE* outbuf, int *outlen)
{
	BYTE	command;
	BYTE	option;
	BYTE*   first_iac;
	int 	i;

	if(inlen<1) {
		*outlen=0;
		return(inbuf);	/* no length? No interpretation */
	}

    first_iac=(BYTE*)memchr(inbuf, TELNET_IAC, inlen);

    if(!telnet_cmdlen && first_iac==NULL) {
        *outlen=inlen;
        return(inbuf);	/* no interpretation needed */
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
						int len=sprintf(buf,"%c%c%c%c%s%c%c"
							,TELNET_IAC,TELNET_SB
							,TELNET_TERM_TYPE,TELNET_TERM_IS
							,termtype
							,TELNET_IAC,TELNET_SE);
						if(debug_telnet)
							lprintf(LOG_INFO,"TX Telnet command: Terminal Type is %s", termtype);
						sendsocket(sock,buf,len);
						request_telnet_opt(TELNET_WILL, TELNET_TERM_SPEED);
					} else if(option==TELNET_TERM_SPEED && telnet_cmd[3]==TELNET_TERM_SEND) {
						BYTE buf[32];
						int len=sprintf(buf,"%c%c%c%c%s%c%c"
							,TELNET_IAC,TELNET_SB
							,TELNET_TERM_SPEED,TELNET_TERM_IS
							,termspeed
							,TELNET_IAC,TELNET_SE);
						if(debug_telnet)
							lprintf(LOG_INFO,"TX Telnet command: Terminal Speed is %s", termspeed);
						sendsocket(sock,buf,len);
						request_telnet_opt(TELNET_WILL, TELNET_TERM_SPEED);
					}

					telnet_cmdlen=0;
				}
			}
            else if(telnet_cmdlen==2 && inbuf[i]<TELNET_WILL) {
                telnet_cmdlen=0;
            }
            else if(telnet_cmdlen>=3) {	/* telnet option negotiation */

				if(debug_telnet)
					lprintf(LOG_INFO,"RX Telnet command: %s %s"
						,telnet_cmd_desc(command),telnet_opt_desc(option));

				if(command==TELNET_DO || command==TELNET_DONT) {	/* local options */
					if(telnet_local_option[option]!=command) {
						switch(option) {
							case TELNET_BINARY_TX:
							case TELNET_ECHO:
							case TELNET_TERM_TYPE:
							case TELNET_SUP_GA:
								telnet_local_option[option]=command;
								send_telnet_cmd(telnet_opt_ack(command),option);
								break;
							default: /* unsupported local options */
								if(command==TELNET_DO) /* NAK */
									send_telnet_cmd(telnet_opt_nak(command),option);
								break;
						}
					}
				} else { /* WILL/WONT (remote options) */ 
					if(telnet_remote_option[option]!=command) {	

						switch(option) {
							case TELNET_BINARY_TX:
							case TELNET_ECHO:
							case TELNET_TERM_TYPE:
							case TELNET_SUP_GA:
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

/****************************************************************************/
/****************************************************************************/
BOOL handle_call(void)
{
	BYTE		buf[4096];
	BYTE		telnet_buf[sizeof(buf)];
	BYTE*		p;
	int			result;
	int			rd;
	int			wr;
	fd_set		socket_set;
	struct		timeval tv = {0, 0};

	bytes_sent=0;
	bytes_received=0;
	call_terminated=FALSE;

	/* Reset Telnet state information */
	telnet_cmdlen=0;
	ZERO_VAR(telnet_local_option);
	ZERO_VAR(telnet_remote_option);

	input_thread_terminated=FALSE;
	_beginthread(input_thread, 0, NULL);

	while(!terminated) {

		if((comGetModemStatus(com_handle)&COM_DCD) == 0) {
			lprintf(LOG_WARNING,"Loss of Carrier Detect (DCD) detected");
			break;
		}
#if 0
		if(comReadByte(com_handle, &ch)) {
			lprintf(LOG_DEBUG,"read byte: %c", ch);
			send(sock, &ch, sizeof(ch), 0);
		}
#endif

		FD_ZERO(&socket_set);
		FD_SET(sock,&socket_set);
		if((result = select(sock+1,&socket_set,NULL,NULL,&tv)) == 0) {
			YIELD();
			continue;
		}
		if(result == SOCKET_ERROR) {
			lprintf(LOG_ERR,"SOCKET ERROR %u on select", ERROR_VALUE);
			break;
		}

		rd=recv(sock,buf,sizeof(buf),0);

		if(rd < 1) {
			if(rd==0) {
				lprintf(LOG_WARNING,"Socket Disconnected");
				break;
			}
			lprintf(LOG_ERR,"SOCKET RECV ERROR %d",ERROR_VALUE);
			continue;
		}

		if(telnet)
			p=telnet_interpret(buf,rd,telnet_buf,&rd);
		else
			p=buf;

		if((wr=comWriteBuf(com_handle, p, rd)) != COM_ERROR)
			bytes_sent += wr;
	}

	call_terminated=TRUE;	/* terminate input_thread() */
	while(!input_thread_terminated) {
		YIELD();
	}

	lprintf(LOG_INFO,"Bytes sent-to, received-from %s: %lu, %lu"
		, com_dev, bytes_sent, bytes_received);

	return TRUE;
}

/****************************************************************************/
/****************************************************************************/
BOOL hangup_call(HANDLE com_handle)
{
	time_t start;

	if((comGetModemStatus(com_handle)&COM_DCD)==0)	/* DCD already low */
		return TRUE;

	lprintf(LOG_INFO,"Dropping DTR");
	if(!comLowerDTR(com_handle))
		return FALSE;

	lprintf(LOG_INFO,"Waiting for loss of Carrier Detect (DCD)");
	start=time(NULL);
	while(time(NULL)-start <= dcd_timeout) {
		if((comGetModemStatus(com_handle)&COM_DCD) == 0)
			return TRUE;
		SLEEP(1000); 
	}
	lprintf(LOG_ERR,"TIMEOUT waiting for DCD to drop");

	return FALSE;
}

/****************************************************************************/
/****************************************************************************/
void break_handler(int type)
{
	lprintf(LOG_NOTICE,"-> Terminated Locally (signal: %d)",type);
	terminated=TRUE;
}

#if defined(_WIN32)
BOOL WINAPI ControlHandler(DWORD CtrlType)
{
	break_handler((int)CtrlType);
	return TRUE;
}
#endif

void parse_ini_file(const char* ini_fname)
{
	FILE* fp;

	if((fp=fopen(ini_fname,"r"))!=NULL)
		lprintf(LOG_INFO,"Reading %s",ini_fname);

	/* Root section */
	pause_on_exit			= iniReadBool(fp,ROOT_SECTION,"PauseOnExit",FALSE);
	log_level				= iniReadLogLevel(fp,ROOT_SECTION,"LogLevel",log_level);

	if(iniReadBool(fp,ROOT_SECTION,"Debug",FALSE))
		log_level=LOG_DEBUG;
	
	/* [COM] Section */
	iniReadString(fp, "COM", "Device", "COM1", com_dev);
	iniReadString(fp, "COM", "ModemInit", "AT&F", mdm_init);
	iniReadString(fp, "COM", "ModemAutoAnswer", "ATS0=1", mdm_autoans);
	iniReadString(fp, "COM", "ModemCleanup", "ATS0=0", mdm_cleanup);
	com_hangup	= iniReadBool(fp, "COM", "Hangup", com_hangup);
	mdm_null	= iniReadBool(fp, "COM", "NullModem", mdm_null);
	mdm_timeout = iniReadInteger(fp, "COM", "ModemTimeout", mdm_timeout);
	dcd_timeout = iniReadInteger(fp, "COM", "DCDTimeout", dcd_timeout);

	/* [TCP] Section */
	iniReadString(fp, "TCP", "Host", "localhost", host);
	iniReadString(fp, "TCP", "TelnetTermType", termtype, termtype);
	iniReadString(fp, "TCP", "TelnetTermSpeed", "28800,28800", termspeed);
	port					= iniReadShortInt(fp, "TCP", "Port", IPPORT_TELNET);
	tcp_nodelay				= iniReadBool(fp,"TCP","NODELAY", tcp_nodelay);
	telnet					= iniReadBool(fp,"TCP","Telnet", telnet);
	debug_telnet			= iniReadBool(fp,"TCP","DebugTelnet", debug_telnet);

	if(fp!=NULL)
		fclose(fp);
}

/****************************************************************************/
/****************************************************************************/
int main(int argc, char** argv)
{
	int		argn;
	char*	arg;
	char*	p;
	char	path[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	char	ini_fname[MAX_PATH+1];
	char	banner[128];

	/*******************************/
	/* Generate and display banner */
	/*******************************/
	sscanf("$Revision$", "%*s %s", revision);

	sprintf(banner,"\nSynchronet External POTS<->TCP Driver v%s-%s"
		" Copyright %s Rob Swindell\n"
		,revision
		,PLATFORM_DESC
		,__DATE__+7
		);

	fprintf(stdout,"%s\n", banner);

	/******************/
	/* Read .ini file */
	/******************/
	/* Generate path/sexpots[.host].ini from path/sexpots[.exe] */
	SAFECOPY(path,argv[0]);
	p=getfname(path);
	SAFECOPY(fname,p);
	*p=0;
	if((p=getfext(fname))!=NULL) 
		*p=0;
	SAFECOPY(termtype,fname);
	strcat(fname,".ini");

	iniFileName(ini_fname,sizeof(ini_fname),path,fname);
	parse_ini_file(ini_fname);

	/**********************/
	/* Parse command-line */
	/**********************/

	for(argn=1; argn<argc; argn++) {
		arg=argv[argn];
		if(*arg!='-') {	/* .ini file specified */
			arg++;
			if(!fexist(arg)) {
				perror(arg);
				return usage(argv[0]);
			}
			parse_ini_file(arg);
			continue;
		}
		while(*arg=='-') 
			arg++;
			if(stricmp(arg,"null")==0)
				mdm_null=TRUE;
			else if(stricmp(arg,"com")==0 && argc > argn+1)
				SAFECOPY(com_dev, argv[++argn]);
			else if(stricmp(arg,"host")==0 && argc > argn+1)
				SAFECOPY(host, argv[++argn]);
			else if(stricmp(arg,"port")==0 && argc > argn+1)
				port = (ushort)strtol(argv[++argn], NULL, 0);
			else if(stricmp(arg,"live")==0) {
				if(argc > argn+1 &&
					(com_handle = (HANDLE)strtol(argv[argn+1], NULL, 0)) != 0) {
					argn++;
					com_handle_passed=TRUE;
				}
				com_alreadyconnected=TRUE;
				terminate_after_one_call=TRUE;
				mdm_null=TRUE;
			}
			else if(stricmp(arg,"nohangup")==0) {
				com_hangup=FALSE;
			}
			else if(stricmp(arg,"help")==0 || *arg=='?')
				return usage(argv[0]);
			else {
				fprintf(stderr,"Invalid option: %s\n", arg);
				return usage(argv[0]);
			}
	}

#if defined(_WIN32)
	SetConsoleCtrlHandler(ControlHandler, TRUE /* Add */);

	/* Convert "1" to "COM1" for Windows */
	{
		int i;

		if((i=atoi(com_dev)) != 0)
			SAFEPRINTF(com_dev, "COM%d", i);
	}

#endif

	/************************************/
	/* Inititalize WinSock and COM Port */
	/************************************/

	lprintf(LOG_INFO,"TCP Host: %s", host);
	lprintf(LOG_INFO,"TCP Port: %u", port);
	
	if(!com_handle_passed) {
		lprintf(LOG_INFO,"Opening Communications Device: %s", com_dev);
		if((com_handle=comOpen(com_dev)) == COM_HANDLE_INVALID) {
			lprintf(LOG_ERR,"ERROR %u opening %s", COM_ERROR_VALUE, com_dev);
			return -1;
		}
	}
	lprintf(LOG_INFO,"%s set to %ld bps DTE rate", com_dev, comGetBaudRate(com_handle));

	if(!winsock_startup())
		return -1;

	/* Install clean-up callback */
	atexit(cleanup);

	/***************************/
	/* Initialization Complete */
	/***************************/

	/* Main service loop: */
	while(wait_for_call(com_handle)) {
		comWriteByte(com_handle,'\r');
		comWriteString(com_handle, banner);
		if((sock=connect_socket(host, port)) == INVALID_SOCKET) {
				comWriteString(com_handle,"\7\r\n!ERROR connecting to TCP port\r\n");
		} else {
			handle_call();
			close_socket(&sock);
			total_calls++;
			lprintf(LOG_INFO,"Call completed (%lu total)", total_calls);
		}
		if(com_hangup && !hangup_call(com_handle))
			return -1;
		if(terminate_after_one_call)
			break;
	}

	return 0;
}
