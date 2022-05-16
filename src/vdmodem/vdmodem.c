/* Synchronet Virtual DOS Modem for Windows */

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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <process.h>

#include "genwrap.h"
#include "dirwrap.h"
#include "gen_defs.h"
#include "sockwrap.h"
#include "telnet.h"
#include "ini_file.h"

#define TITLE "Synchronet Virtual DOS Modem for Windows"
#define VERSION "0.0"

SOCKET sock = INVALID_SOCKET;
SOCKET listening_sock = INVALID_SOCKET;
HANDLE hangup_event = INVALID_HANDLE_VALUE;	// e.g. program drops DTR
HANDLE hungup_event = INVALID_HANDLE_VALUE;	// e.g. ATH0
HANDLE carrier_event = INVALID_HANDLE_VALUE;
HANDLE rdslot = INVALID_HANDLE_VALUE;
HANDLE wrslot = INVALID_HANDLE_VALUE;
str_list_t ini;
char ini_fname[MAX_PATH + 1];
union xp_sockaddr listening_interface;
enum {
	 RAW
	,TELNET
} mode;
char* modeNames[] = {"raw", "telnet", NULL};
struct {
	uint8_t	local_option[0x100];
	uint8_t	remote_option[0x100];
	uint cmdlen;
	uchar cmd[64];
} telnet;

#define XTRN_IO_BUF_LEN 10000
#define RING_DELAY 6000 /* US standard is 6 seconds */

struct {
	int node_num;
	uint16_t port;
	bool listen;
	bool debug;
	bool terminate_on_disconnect;
	ulong data_rate;
	bool server_echo;
	enum {
		 ADDRESS_FAMILY_UNSPEC
		,ADDRESS_FAMILY_INET
		,ADDRESS_FAMILY_INET6
	} address_family;
} cfg;
char* addrFamilyNames[] = {"unspec", "ipv4", "ipv6", NULL};

static void dprintf(const char *fmt, ...)
{
	char buf[1024] = "SBBSVDM: ";
	va_list argptr;

    va_start(argptr,fmt);
	size_t offset = strlen(buf);
    _vsnprintf(buf + offset, sizeof(buf) - offset, fmt, argptr);
	TERMINATE(buf);
    va_end(argptr);
    OutputDebugString(buf);
}

void usage(void)
{
	fprintf(stderr, "usage:\n");
	exit(EXIT_SUCCESS);
}

const char* supported_cmds = "ADEHIMOQSVXZ&";
const char* string_cmds = "D";
#define MAX_SAVES 20
struct modem {
	enum {
		 INIT
		,A
		,AT
	} cmdstate;
	char cr;
	char lf;
	char bs;
	char esc;
	char save[MAX_SAVES][INI_MAX_VALUE_LEN];
	char last[INI_MAX_VALUE_LEN];
	bool echo_off;
	bool numeric_mode;
	bool offhook;
	bool online; // false means "command mode"
	bool ringing;
	ulong ringcount;
	ulong auto_answer;
	ulong dial_wait;
	ulong guard_time;
	ulong esc_count;
	ulong ext_results;
	ulong quiet;
	uint8_t buf[128];
	size_t buflen;
};

void newcmd(struct modem* modem)
{
	modem->cmdstate = INIT;
	modem->buflen = 0;
}

ulong guard_time(struct modem* modem)
{
	return modem->guard_time * 20;
}

ulong count_esc(struct modem* modem, uint8_t* buf, size_t rd)
{
	if(modem->esc < 0 || modem->esc > 127)
		return 0;

	ulong count = 0;
	for(size_t i = 0; i < rd; i++) {
		if(buf[i] == modem->esc)
			count++;
	}
	return count;
}

// Basic Hayes modem responses
enum modem_response {
	OK				= 0,
	CONNECT			= 1,
	RING			= 2,
	NO_CARRIER		= 3,
	ERROR			= 4,
	CONNECT_1200	= 5,
	NO_DIAL_TONE	= 6,
	BUSY			= 7,
	NO_ANSWER		= 8,
	RESERVED1		= 9,
	CONNECT_2400	= 10,
	RESERVED2		= 11,
	RESERVED3		= 12,
	CONNECT_9600	= 13
};

const char* response_str[] = {
	"OK",
	"CONNECT",
	"RING",
	"NO CARRIER",
	"ERROR",
	"CONNECT 1200",
	"NO DIAL TONE",
	"BUSY",
	"NO ANSWER",
	"RESERVED1",
	"CONNECT 2400",
	"RESERVED2",
	"RESERVED3",
	"CONNECT 9600"
};

char* response(struct modem* modem, enum modem_response code)
{
	static char str[128];

	if(modem->quiet)
		return "";
	if(modem->numeric_mode)
		sprintf(str, "%u%c", code, modem->cr);
	else
		sprintf(str, "%c%c%s%c%c", modem->cr, modem->lf, response_str[code], modem->cr, modem->lf);
	return str;
}

char* ok(struct modem* modem)
{
	return response(modem, OK);
}

char* error(struct modem* modem)
{
	return response(modem, ERROR);
}

char* connect_result(struct modem* modem)
{
	return response(modem, modem->ext_results ? CONNECT_9600 : CONNECT);
}

char* connected(struct modem* modem)
{
	modem->online = true;
	modem->ringing = false;
	ResetEvent(hungup_event);
	SetEvent(carrier_event);
	return connect_result(modem);
}

void disconnect(struct modem* modem)
{
	modem->online = false;
	shutdown(sock, SD_SEND);
	closesocket(sock);
	sock = INVALID_SOCKET;
	SetEvent(hungup_event);
	SetEvent(carrier_event);
}

bool kbhit()
{
	unsigned long waiting = 0;
	if(!GetMailslotInfo(
		rdslot,				// mailslot handle
 		NULL,				// address of maximum message size
		NULL,				// address of size of next message
		&waiting,			// address of number of messages
 		NULL				// address of read time-out
		))
		return false;
	return waiting != 0;
}

void init(struct modem* modem)
{
	const char* section = "modem";
	memset(modem, 0, sizeof(*modem));
	modem->auto_answer = iniGetBool(ini, section, "auto_answer", FALSE);
	modem->echo_off = !iniGetBool(ini, section, "echo", TRUE);
	modem->quiet = iniGetBool(ini, section, "quiet", FALSE);
	modem->numeric_mode = iniGetBool(ini, section, "numeric", FALSE);
	modem->cr = (char)iniGetInteger(ini, section, "cr", '\r');
	modem->lf = (char)iniGetInteger(ini, section, "lf", '\n');
	modem->bs = (char)iniGetInteger(ini, section, "bs", '\b');
	modem->esc = (char)iniGetInteger(ini, section, "esc", '+');
	modem->ext_results = iniGetInteger(ini, section, "ext_results", 4);
	modem->dial_wait = iniGetInteger(ini, section, "dial_wait", 60);
	modem->guard_time = iniGetInteger(ini, section, "guard_time", 50);
}

bool write_cfg(struct modem* modem)
{
	dprintf(__FUNCTION__);
	const char* section = "modem";
	iniSetBool(&ini, section, "auto_answer", modem->auto_answer, /* style: */NULL);
	bool result = false;
	FILE* fp = iniOpenFile(ini_fname, /* create: */TRUE);
	if(fp != NULL) {
		if(iniWriteFile(fp, ini)) {
			dprintf("Wrote config to: %s", ini_fname);
			result = true;
		}
		iniCloseFile(fp);
	}
	return result;
}

bool write_save(struct modem* modem, ulong savnum)
{
	char key[128];
	str_list_t ini;

	if(savnum >= MAX_SAVES)
		return false;
	SAFEPRINTF(key, "save%lu", savnum);
	FILE* fp = iniOpenFile(ini_fname, /* create: */TRUE);
	if(fp == NULL)
		return false;
	ini = iniReadFile(fp);
	iniSetString(&ini, "modem", key, modem->save[savnum], /* style: */NULL);
	bool result = iniWriteFile(fp, ini);
	iniCloseFile(fp);
	return result;
}

const char* protocol(enum mode mode)
{
	switch(mode) {
	case TELNET: return "Telnet";
	}
	return "Raw";
}

int address_family()
{
	switch(cfg.address_family) {
		case ADDRESS_FAMILY_INET:	return PF_INET;
		case ADDRESS_FAMILY_INET6:	return PF_INET6;
	}
	return PF_UNSPEC;
}

int putcom(char* buf, size_t len)
{
	return send(sock, buf, len, /* flags: */0);
}

static void send_telnet_cmd(uchar cmd, uchar opt)
{
	char buf[16];
	
	if(cmd<TELNET_WILL) {
		dprintf("TELNET TX: %s"
			,telnet_cmd_desc(cmd));
		sprintf(buf,"%c%c",TELNET_IAC,cmd);
		putcom(buf,2);
	} else {
		dprintf("TELNET TX: %s %s"
			,telnet_cmd_desc(cmd), telnet_opt_desc(opt));
		sprintf(buf,"%c%c%c",TELNET_IAC,cmd,opt);
		putcom(buf,3);
	}
}

void request_telnet_opt(uchar cmd, uchar opt)
{
	if(cmd==TELNET_DO || cmd==TELNET_DONT) {	/* remote option */
		if(telnet.remote_option[opt]==telnet_opt_ack(cmd))
			return;	/* already set in this mode, do nothing */
		telnet.remote_option[opt]=telnet_opt_ack(cmd);
	} else {	/* local option */
		if(telnet.local_option[opt]==telnet_opt_ack(cmd))
			return;	/* already set in this mode, do nothing */
		telnet.local_option[opt]=telnet_opt_ack(cmd);
	}
	send_telnet_cmd(cmd,opt);
}

BYTE* telnet_interpret(BYTE* inbuf, size_t inlen, BYTE* outbuf, size_t *outlen)
{
	BYTE	command;
	BYTE	option;
	BYTE*   first_cr=NULL;
	BYTE*   first_int=NULL;
	size_t 	i;

	if(inlen<1) {
		*outlen=0;
		return(inbuf);	/* no length? No interpretation */
	}

    first_int=(BYTE*)memchr(inbuf, TELNET_IAC, inlen);
	if(telnet.remote_option[TELNET_BINARY_TX]!=TELNET_WILL) {
		first_cr=(BYTE*)memchr(inbuf, '\r', inlen);
		if(first_cr) {
			if(first_int==NULL || first_cr < first_int)
				first_int=first_cr;
		}
	}

    if(telnet.cmdlen==0 && first_int==NULL) {
        *outlen=inlen;
        return(inbuf);	/* no interpretation needed */
    }

    if(telnet.cmdlen==0 /* If we haven't returned and telnet.cmdlen==0 then first_int is not NULL */  ) {
   		*outlen=first_int-inbuf;
	    memcpy(outbuf, inbuf, *outlen);
    } else
    	*outlen=0;

    for(i=*outlen;i<inlen;i++) {
		if(telnet.remote_option[TELNET_BINARY_TX]!=TELNET_WILL) {
			if(telnet.cmdlen==1 && telnet.cmd[0]=='\r') {
            	outbuf[(*outlen)++]='\r';
				if(inbuf[i]!=0 && inbuf[i]!=TELNET_IAC)
	            	outbuf[(*outlen)++]=inbuf[i];
				telnet.cmdlen=0;
				if(inbuf[i]!=TELNET_IAC)
					continue;
			}
			if(inbuf[i]=='\r' && telnet.cmdlen==0) {
				telnet.cmd[telnet.cmdlen++]='\r';
				continue;
			}
		}

        if(inbuf[i]==TELNET_IAC && telnet.cmdlen==1) { /* escaped 255 */
            telnet.cmdlen=0;
            outbuf[(*outlen)++]=TELNET_IAC;
            continue;
        }
        if(inbuf[i]==TELNET_IAC || telnet.cmdlen) {

			if(telnet.cmdlen<sizeof(telnet.cmd))
				telnet.cmd[telnet.cmdlen++]=inbuf[i];

			command	= telnet.cmd[1];
			option	= telnet.cmd[2];

			if(telnet.cmdlen>=2 && command==TELNET_SB) {
				if(inbuf[i]==TELNET_SE 
					&& telnet.cmd[telnet.cmdlen-2]==TELNET_IAC) {
					telnet.cmdlen=0;
				}
			}
            else if(telnet.cmdlen==2 && inbuf[i]<TELNET_WILL) {
                telnet.cmdlen=0;
            }
            else if(telnet.cmdlen>=3) {	/* telnet option negotiation */

				dprintf("TELNET RX: %s %s"
					,telnet_cmd_desc(command),telnet_opt_desc(option));

				if(command==TELNET_DO || command==TELNET_DONT) {	/* local options */
					if(telnet.local_option[option]!=command) {
						switch(option) {
							case TELNET_BINARY_TX:
							case TELNET_ECHO:
							case TELNET_TERM_TYPE:
							case TELNET_SUP_GA:
							case TELNET_NEGOTIATE_WINDOW_SIZE:
								telnet.local_option[option]=command;
								send_telnet_cmd(telnet_opt_ack(command),option);
								break;
							default: /* unsupported local options */
								if(command==TELNET_DO) /* NAK */
									send_telnet_cmd(telnet_opt_nak(command),option);
								break;
						}
					}
				} else { /* WILL/WONT (remote options) */ 
					if(telnet.remote_option[option]!=command) {	

						switch(option) {
							case TELNET_BINARY_TX:
							case TELNET_ECHO:
							case TELNET_TERM_TYPE:
							case TELNET_SUP_GA:
							case TELNET_NEGOTIATE_WINDOW_SIZE:
								telnet.remote_option[option]=command;
								send_telnet_cmd(telnet_opt_ack(command),option);
								break;
							default: /* unsupported remote options */
								if(command==TELNET_WILL) /* NAK */
									send_telnet_cmd(telnet_opt_nak(command),option);
								break;
						}
					}
				}

                telnet.cmdlen=0;

            }
        } else
        	outbuf[(*outlen)++]=inbuf[i];
    }
    return(outbuf);
}

// Significant portions copies from syncterm/conn.c
char* dial(struct modem* modem, const char* number)
{
	struct addrinfo	hints;
	struct addrinfo	*res=NULL;
	char host[128];
	char portnum[16];
	uint16_t port = cfg.port;

	dprintf("dial(%s)", number);
	if(stricmp(number, "L") == 0)
		number = modem->last;
	else {
		if(toupper(*number) == 'S' && IS_DIGIT(number[1])) {
			char* p;
			ulong val = strtoul(number+1, &p, 10);
			if(val < MAX_SAVES && *p == '\0')
				number = modem->save[val];
		}
		SAFECOPY(modem->last, number);
	}
	if(strncmp(number, "raw:", 4) == 0) {
		mode = RAW;
		number += 4;
	}
	else if(strncmp(number, "telnet:", 7) == 0) {
		mode = TELNET;
		number += 7;
	}
	SKIP_WHITESPACE(number);
	SAFECOPY(host, number);
	char* p = strrchr(host, ':');
	char* b = strrchr(host, ']'); 
	if(p != NULL && p > b) {
		port = (uint16_t)strtol(p, &p, 10);
		*p = 0;
	}
	dprintf("Connecting to port %hu at host '%s' via %s", port, host, protocol(mode));
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags=PF_UNSPEC;
	hints.ai_family=address_family();
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=IPPROTO_TCP;
	hints.ai_flags=AI_NUMERICSERV;
#ifdef AI_ADDRCONFIG
	hints.ai_flags|=AI_ADDRCONFIG;
#endif
	dprintf("%s %d calling getaddrinfo", __FILE__, __LINE__);
	SAFEPRINTF(portnum, "%hu", port);
	int result = getaddrinfo(host, portnum, &hints, &res);
	if(result != 0) {
		dprintf("getaddrinfo(%s, %s) returned %d", host, portnum, result);
		return response(modem, NO_ANSWER);
	}

	int				nonblock;
	struct addrinfo	*cur;
	for(cur=res; cur && sock == INVALID_SOCKET; cur=cur->ai_next) {
		if(sock==INVALID_SOCKET) {
			sock=socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
			if(sock==INVALID_SOCKET) {
				dprintf("Error %ld creating socket", WSAGetLastError());
				return response(modem, NO_DIAL_TONE);
			}
			/* Set to non-blocking for the connect */
			nonblock=-1;
			ioctlsocket(sock, FIONBIO, &nonblock);
		}

		dprintf("%s %d calling connect", __FILE__, __LINE__);
		if(connect(sock, cur->ai_addr, cur->ai_addrlen)) {
			switch(ERROR_VALUE) {
				case EINPROGRESS:
				case EINTR:
				case EAGAIN:
				case EWOULDBLOCK:
					for(;sock!=INVALID_SOCKET;) {
						if (socket_writable(sock, 1000)) {
							if (socket_recvdone(sock, 0)) {
								closesocket(sock);
								sock=INVALID_SOCKET;
								continue;
							}
							else {
								goto connected;
							}
						}
						else {
							if (kbhit()) {
								dprintf("%s %d kbhit", __FILE__, __LINE__);
								closesocket(sock);
								sock = INVALID_SOCKET;
								return response(modem, NO_CARRIER);
							}
						}
					}

connected:
					break;
				default:
					closesocket(sock);
					sock=INVALID_SOCKET;
					continue;
			}
		}
	}
	if (sock == INVALID_SOCKET) {
		dprintf("%s %d invalid hostname?", __FILE__, __LINE__);
		return response(modem, NO_ANSWER);
	}

	freeaddrinfo(res);
	res=NULL;
	nonblock=0;
	ioctlsocket(sock, FIONBIO, &nonblock);
	if(socket_recvdone(sock, 0)) {
		dprintf("%s %d socket_recvdone", __FILE__, __LINE__);
		return response(modem, NO_CARRIER);
	}

	dprintf("%s %d connected!", __FILE__, __LINE__);
	int keepalives = TRUE;
	setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepalives, sizeof(keepalives));
	ZERO_VAR(telnet);
	return connected(modem);
}

char* answer(struct modem* modem)
{
	if(listening_sock == INVALID_SOCKET)
		return response(modem, NO_DIAL_TONE);

	fd_set fds = {0};
	FD_SET(listening_sock, &fds);
	struct timeval tv = { 0, 0 };
	if(select(/* ignored: */0, &fds, NULL, NULL, &tv) != 1)
		return response(modem, NO_CARRIER);

	union xp_sockaddr addr;
	socklen_t addrlen = sizeof(addr);
	sock = accept(listening_sock, (SOCKADDR*)&addr, &addrlen);
	if(sock == INVALID_SOCKET) {
		dprintf("accept returned %d (errno=%ld)", sock, WSAGetLastError());
		return response(modem, NO_CARRIER);
	}
	char tmp[256];
	dprintf("Connection accepted from TCP port %hu at %s", inet_addrport(&addr), inet_addrtop(&addr, tmp, sizeof(tmp)));

	if(mode == TELNET) {
		ZERO_VAR(telnet);
		if(cfg.server_echo) {
			/* Disable Telnet Terminal Echo */
			request_telnet_opt(TELNET_WILL,TELNET_ECHO);
		}
		/* Will suppress Go Ahead */
		request_telnet_opt(TELNET_WILL,TELNET_SUP_GA);
	}
	return connected(modem);
}

char* atmodem_exec(struct modem* modem)
{
	static char respbuf[128];
	char* resp = ok(modem);
	modem->buf[modem->buflen] = '\0';
	for(char* p = modem->buf; *p != '\0';) {
		char ch = toupper(*p);
		p++;
		if(strchr(supported_cmds, ch) == NULL)
			return error(modem);
		if(strchr(string_cmds, ch) == NULL) {
			if(ch == '&') {
				ch = toupper(*p);
				ulong val = strtoul(p + 1, &p, 10); // unused
				switch(ch) {
					case 'W':
						resp = write_cfg(modem) ? ok(modem) : error(modem);
						break;
					case 'Z':
						if(val >= MAX_SAVES)
							return error(modem);
						if(*p == '=') {
							p++;
							if(strcmp(p, "L") == 0)
								p = modem->last;
							SAFECOPY(modem->save[val], p);
							return write_save(modem, val) ? ok(modem) : error(modem);
						}
						if(*p == '?' || strcmp(p, "L?") == 0) {
							if(strcmp(p, "L?") == 0)
								p = modem->last;
							else
								p = modem->save[val];
							sprintf(respbuf, "%c%s%c%c%s", modem->lf, p, modem->cr, modem->lf, ok(modem));
							return respbuf;
						}
				}
				continue;
			}
			// Numeric argument commands
			ulong val = strtoul(p, &p, 10);
			switch(ch) {
				case 'A':
					return answer(modem);
				case 'E':
					modem->echo_off = !val;
					break;
				case 'H':
					modem->offhook = val;
					modem->ringing = false;
					if(!modem->offhook) {
						if(sock != INVALID_SOCKET) {
							disconnect(modem);
						}
					}
					break;
				case 'I':
					sprintf(respbuf, "\r\n" TITLE " v" VERSION " Copyright %s Rob Swindell\r\n", &__DATE__[7]);
					return respbuf;
				case 'O':
					if(sock == INVALID_SOCKET)
						return error(modem);
					modem->online = true;
					return connect_result(modem);
					break;
				case 'V':
					modem->numeric_mode = !val;
					resp = ok(modem); // Use the new verbal/numeric mode in response (ala USRobotics)
					break;
				case 'Q':
					modem->quiet = val;
					resp = ok(modem); // Use the new quiet/verbose mode in response (ala USRobotics)
					break;
				case 'S':
					if(*p == '=') {
						ulong sreg = val;
						ulong val = strtoul(p + 1, &p, 10);
						dprintf("S%lu = %lu", sreg, val);
						switch(sreg) {
							case 0:
								if(val && listening_sock == INVALID_SOCKET) {
									dprintf("Can't enable auto-answer when not in listening mode");
									return error(modem);
								}
								modem->auto_answer = val;
								break;
							case 1:
								modem->ringcount = val;
								break;
							case 2:
								modem->esc = (char)val;
								break;
							case 3:
								modem->cr = (char)val;
								break;
							case 4:
								modem->lf = (char)val;
								break;
							case 5:
								modem->bs = (char)val;
								break;
							case 7:
								modem->dial_wait = val;
								break;
							case 12:
								modem->guard_time = val;
								break;
						}
					} else if(*p == '?') {
						switch(val) {
							case 0:
								val = modem->auto_answer;
								break;
							case 1:
								val = modem->ringcount;
								break;
							case 2:
								val = modem->esc;
								break;
							case 3:
								val = modem->cr;
								break;
							case 4:
								val = modem->lf;
								break;
							case 5:
								val = modem->bs;
								break;
							case 7:
								val = modem->dial_wait;
								break;
							case 12:
								val = modem->guard_time;
								break;
							default:
								val = 0;
								break;
						}
						sprintf(respbuf, "%c%03lu%c%c%s", modem->lf, val, modem->cr, modem->lf, ok(modem));
						return respbuf;
					} else
						return error(modem);
					break;
				case 'X':
					modem->ext_results = val;
					break;
				case 'Z':
					init(modem);
					break;
			}
		} else { // string argument commands
			switch(ch) {
				case 'D':
					if(sock != INVALID_SOCKET) {
						dprintf("Can't dial: Already connected");
						return error(modem);
					}
					if(*p == 'T' /* tone */|| *p == 'P' /* pulse */)
						p++;
					return dial(modem, p);
			}
		}
	}
	return resp;
}

char* atmodem_parsech(struct modem* modem, uint8_t ch)
{
	switch(modem->cmdstate) {
		case INIT:
			if(toupper(ch) == 'A')
				modem->cmdstate = A;
			break;
		case A:
			if(toupper(ch) == 'T')
				modem->cmdstate = AT;
			else
				newcmd(modem);
			break;
		case AT:
			if(ch == modem->cr) {
				char* retval = atmodem_exec(modem);
				newcmd(modem);
				return retval;
			} else if(ch == modem->bs) {
				if(modem->buflen)
					modem->buflen--;
			} else {
				if(modem->buflen >= sizeof(modem->buf))
					return error(modem);
				if(ch != ' ')
					modem->buf[modem->buflen++] = ch;
			}
			break;
	}
	return NULL;
}

char* atmodem_parse(struct modem* modem, uint8_t* buf, size_t len)
{
	for(size_t i = 0; i < len; i++) {
		char* resp = atmodem_parsech(modem, buf[i]);
		if(resp != NULL)
			return resp;
	}
	return NULL;
}

BOOL vdd_write(HANDLE* slot, uint8_t* buf, size_t buflen)
{
	if(*slot == INVALID_HANDLE_VALUE) {
		char path[MAX_PATH + 1];
		sprintf(path, "\\\\.\\mailslot\\sbbsexec\\wr%d", cfg.node_num);
		*slot = CreateFile(path
			,GENERIC_WRITE
			,FILE_SHARE_READ
			,NULL
			,OPEN_EXISTING
			,FILE_ATTRIBUTE_NORMAL
			,(HANDLE) NULL);
		if(*slot == INVALID_HANDLE_VALUE) {
			dprintf("!ERROR %u (%s) opening '%s'", GetLastError(), strerror(GetLastError()), path);
			return FALSE;
		}
	}
	DWORD wr = 0;
	BOOL result = WriteFile(*slot, buf, buflen, &wr, /* LPOVERLAPPED */NULL);
	if(wr != buflen)
		dprintf("WriteFile wrote %ld instead of %ld", wr, buflen);
	return result;
}

BOOL vdd_writestr(HANDLE* slot, char* str)
{
	return vdd_write(slot, str, strlen(str));
}

void listen_thread(void* arg)
{
	struct modem* modem = (struct modem*)arg;

	for(;;) {
		fd_set fds = {0};
		FD_SET(listening_sock, &fds);
		struct timeval tv = { 1, 0 };
		if(select(/* ignored: */0, &fds, NULL, NULL, &tv) == 1) {
			if(sock != INVALID_SOCKET) {	// In-use
				SOCKADDR_IN addr;
				socklen_t addrlen = sizeof(addr);
				SOCKET newsock = accept(listening_sock, (SOCKADDR*)&addr, &addrlen);
				if(newsock != INVALID_SOCKET) {
					char* busy_notice = "\r\nSorry, not available right now\r\n";
					send(newsock, busy_notice, strlen(busy_notice), /* flags: */0);
					shutdown(newsock, SD_SEND);
					closesocket(newsock);
				}
				continue;
			}
			if(!modem->offhook && !modem->online)
				modem->ringing = true;
		}
	}
}

int main(int argc, char** argv)
{
	int argn = 1;
	char tmp[256];
	char path[MAX_PATH + 1];
	char fullmodemline[MAX_PATH + 1];
	uint8_t buf[XTRN_IO_BUF_LEN];
	uint8_t telnet_buf[sizeof(buf) * 2];
	size_t rx_buflen = sizeof(buf);
	ULONGLONG rx_delay = 0;
	WSADATA WSAData;
	int	result;

	fprintf(stderr, TITLE " v" VERSION " Copyright %s Rob Swindell\n", &__DATE__[7]);
    if((result = WSAStartup(MAKEWORD(1,1), &WSAData)) == 0)
		dprintf("%s %s",WSAData.szDescription, WSAData.szSystemStatus);
	else {
		fprintf(stderr,"!WinSock startup ERROR %d", result);
		return EXIT_FAILURE;
	}

	// Default configuration values
	cfg.server_echo = TRUE;
	cfg.port = IPPORT_TELNET;
	cfg.address_family = ADDRESS_FAMILY_INET;

	ini = strListInit();
	SAFECOPY(ini_fname, argv[0]);
	char* ext = getfext(ini_fname);
	if(ext != NULL)
		*ext = 0;
	SAFECAT(ini_fname, ".ini");
	fprintf(stderr, "ini_fname = '%s'\n", ini_fname);
	FILE* fp = iniOpenFile(ini_fname, /* create: */false);
	if(fp != NULL) {
		ini = iniReadFile(fp);
		iniCloseFile(fp);
		mode = iniGetEnum(ini, ROOT_SECTION, "mode", modeNames, mode);
		cfg.port = iniGetShortInt(ini, ROOT_SECTION, "port", cfg.port);
		cfg.node_num = iniGetInteger(ini, ROOT_SECTION, "node", cfg.node_num);
		cfg.listen = iniGetBool(ini, ROOT_SECTION, "listen", cfg.listen);
		cfg.debug = iniGetBool(ini, ROOT_SECTION, "debug", cfg.debug);
		cfg.server_echo = iniGetBool(ini, ROOT_SECTION, "server_echo", cfg.server_echo);
		cfg.data_rate = iniGetLongInt(ini, ROOT_SECTION, "rate", cfg.data_rate);
		cfg.address_family = iniGetEnum(ini, ROOT_SECTION, "address_family", addrFamilyNames, cfg.address_family);
	}

	for(; argn < argc; argn++) {
		char* arg = argv[argn];
		if(*arg != '-')
			break;
		while(*arg == '-')
			arg++;
		if(strcmp(arg, "telnet") == 0) {
			mode = TELNET;
			continue;
		}
		if(strcmp(arg, "raw") == 0) {
			mode = RAW;
			continue;
		}
		switch(*arg) {
			case '4':
				cfg.address_family = ADDRESS_FAMILY_INET;
				break;
			case '6':
				cfg.address_family = ADDRESS_FAMILY_INET6;
				break;
			case 'l':
				cfg.listen = true;
				arg++;
				if(*arg != '\0') {
					listening_interface.addr.sa_family = address_family();
					if(inet_ptoaddr(arg, &listening_interface, sizeof(listening_interface)) == NULL) {
						fprintf(stderr, "!Error parsing network address: %s", arg);
						return EXIT_FAILURE;
					}
				}
				break;
			case 'p':
				cfg.port = atoi(arg + 1);
				break;
			case 'd':
				cfg.debug = true;
				break;
			case 'h':
				sock = strtoul(arg + 1, NULL, 10);
				break;
			case 'r':
				cfg.data_rate = strtoul(arg + 1, NULL, 10);
				break;
			case 'B':
				rx_buflen = min(strtoul(arg + 1, NULL, 10), sizeof(buf));
			case 'R':
				rx_delay = strtoul(arg + 1, NULL, 10);
				break;
			default:
				usage();
				break;
		}
	}
	if(argn >= argc) {
		usage();
	}

	struct modem modem = {0};
	init(&modem);

	if(cfg.listen) {
		if(sock != INVALID_SOCKET)
			listening_sock = sock;
		else {
			listening_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
			if(listening_sock == INVALID_SOCKET) {
				fprintf(stderr, "Error %ld creating socket\n", WSAGetLastError());
				return EXIT_FAILURE;
			}
		}
		listening_interface.addr.sa_family = address_family();
		inet_setaddrport(&listening_interface, cfg.port);
		result = bind(listening_sock, &listening_interface.addr, xp_sockaddr_len(&listening_interface));
		if(result != 0) {
			fprintf(stderr, "Error %d binding socket\n", WSAGetLastError());
			return EXIT_FAILURE;
		}
		if(listen(listening_sock, /* backlog: */1) != 0) {
			fprintf(stderr, "Error %d listening on socket\n", WSAGetLastError());
			return EXIT_FAILURE;
		}
		fprintf(stderr, "Listening on TCP port %u at %s\n"
			,inet_addrport(&listening_interface), inet_addrtop(&listening_interface, tmp, sizeof(tmp)));

		_beginthread(listen_thread, /* stack_size: */0, &modem);
	} else {
		if(sock != INVALID_SOCKET)
			connected(&modem);
	}

	const char* dropfile = "dosxtrn.env";
	fp = fopen(dropfile, "w");
	if(fp == NULL) {
		perror(dropfile);
		return EXIT_FAILURE;
	}
	char* dospgm = argv[argn];
	for(; argn < argc; argn++) {
		fprintf(fp, "%s ", argv[argn]);
	}
	fputc('\n', fp);
	fclose(fp);

	while(1) {
		sprintf(path, "\\\\.\\mailslot\\sbbsexec\\rd%d", cfg.node_num);
		rdslot = CreateMailslot(path
			,sizeof(buf)/2			// Maximum message size (0=unlimited)
			,0						// Read time-out
			,NULL);                 // Security
		if(rdslot!=INVALID_HANDLE_VALUE)
			break;
		if(cfg.node_num == 0xff) {
			fprintf(stderr, "Error %ld creating '%s'\n", GetLastError(), path);
			return EXIT_FAILURE;
		}
		++cfg.node_num;
	}

	sprintf(path, "sbbsexec_carrier%d", cfg.node_num);
	carrier_event = CreateEvent(
		 NULL	// pointer to security attributes
		,FALSE	// flag for manual-reset event
		,FALSE  // flag for initial state
		,path	// pointer to event-object name
		);
	if(carrier_event == NULL) {
		fprintf(stderr, "Error %ld creating '%s'\n", GetLastError(), path);
		return EXIT_FAILURE;
	}

	sprintf(path, "sbbsexec_hangup%d", cfg.node_num);
	hangup_event = CreateEvent(
		 NULL	// pointer to security attributes
		,FALSE	// flag for manual-reset event
		,FALSE  // flag for initial state (DTR = high)
		,path	// pointer to event-object name
		);
	if(hangup_event == NULL) {
		fprintf(stderr, "Error %ld creating '%s'\n", GetLastError(), path);
		return EXIT_FAILURE;
	}

	sprintf(path, "sbbsexec_hungup%d", cfg.node_num);
	hungup_event = CreateEvent(
		 NULL	// pointer to security attributes
		,TRUE	// flag for manual-reset event
		,TRUE   // flag for initial state (DCD = low)
		,path	// pointer to event-object name
		);
	if(hungup_event == NULL) {
		fprintf(stderr, "Error %ld creating '%s'\n", GetLastError(), path);
		return EXIT_FAILURE;
	}

    STARTUPINFO startup_info={0};
    startup_info.cb=sizeof(startup_info);

	BOOL x64 = FALSE;
	IsWow64Process(GetCurrentProcess(), &x64);
	sprintf(fullmodemline, "dosxtrn.exe %s %s %u svdm.ini", dropfile, x64 ? "x64" : "NT", cfg.node_num);

	PROCESS_INFORMATION process_info;
    if(!CreateProcess(
		NULL,			// pointer to name of executable module
		fullmodemline,  	// pointer to command line string
		NULL,  			// process security attributes
		NULL,   		// thread security attributes
		FALSE, 			// handle inheritance flag
		0, //CREATE_NEW_CONSOLE/*|CREATE_SEPARATE_WOW_VDM*/, // creation flags
        NULL,			// pointer to new environment block
		NULL		,	// pointer to current directory name
		&startup_info,  // pointer to STARTUPINFO
		&process_info  	// pointer to PROCESS_INFORMATION
		)) {
        fprintf(stderr, "Error %ld executing '%s'", GetLastError(), fullmodemline);
		return EXIT_FAILURE;
	}
	printf("Executed '%s' successfully\n", fullmodemline);

	CloseHandle(process_info.hThread);

	if(cfg.data_rate > 0) {
		rx_buflen = max(cfg.data_rate / 100, 1);
		rx_delay = (ULONGLONG) (1000 * ((double)rx_buflen / cfg.data_rate));
	}

	ULONGLONG lastring = 0;
	ULONGLONG lasttx = 0;
	ULONGLONG lastrx = 0;
	int largest_recv = 0;

	while(WaitForSingleObject(process_info.hProcess,0) != WAIT_OBJECT_0) {
		ULONGLONG now = xp_timer64();
		if(modem.online) {
			fd_set fds = {0};
			FD_SET(sock, &fds);
			struct timeval tv = { 0, 0 };
			if(now - lastrx >= rx_delay && select(/* ignored: */0, &fds, NULL, NULL, &tv) == 1) {
				dprintf("select returned 1");
				int rd = recv(sock, buf, rx_buflen, /* flags: */0);
				dprintf("recv returned %d", rd);
				if(rd <= 0) {
					int error = WSAGetLastError();
					if(rd == 0 || error == WSAECONNRESET) {
						dprintf("Connection reset (error %d, %s) detected on socket %ld", error, strerror(error), sock);
						disconnect(&modem);
						vdd_writestr(&wrslot, response(&modem, NO_CARRIER));
						continue;
					}
					dprintf("Socket error %ld on recv", error);
					continue;
				}
				if(rd > largest_recv)
					largest_recv = rd;
				uint8_t* p = buf;
				size_t len = rd;
				if(mode == TELNET) {
					p = telnet_interpret(buf, rd, telnet_buf, &len);
				}
				vdd_write(&wrslot, p, len);
				lastrx = now;
			}
			if(WaitForSingleObject(hangup_event, 0) == WAIT_OBJECT_0) {
				dprintf("hangup_event signaled");
				disconnect(&modem);
				vdd_writestr(&wrslot, response(&modem, NO_CARRIER));
			}
		} else {
			if(modem.ringing) {
				if(modem.ringcount < 1)
					dprintf("Incoming connection");
				if(now - lastring > RING_DELAY) {
					dprintf("RING");
					vdd_writestr(&wrslot, response(&modem, RING));
					lastring = now;
					modem.ringcount++;
					if(modem.auto_answer > 0 && modem.ringcount >= modem.auto_answer) {
						vdd_writestr(&wrslot, answer(&modem));
					}
				}
			}
			if(cfg.terminate_on_disconnect) {
				dprintf("Terminating process on disconnect");
				TerminateProcess(process_info.hProcess, 2112);
			}
		}

		size_t rd = 0;
		size_t len = sizeof(buf);

		while(rd<len) {
			unsigned long waiting = 0;
			unsigned long msglen = 0;

			GetMailslotInfo(
				rdslot,				// mailslot handle
 				NULL,				// address of maximum message size
				NULL,				// address of size of next message
				&waiting,			// address of number of messages
 				NULL				// address of read time-out
				);
			if(!waiting)
				break;
			if(ReadFile(rdslot, buf+rd, len-rd, &msglen, NULL)==FALSE || msglen<1)
				break;
			rd+=msglen;
		}
		if(rd) {
			if(modem.online) {
				if(modem.esc_count) {
					if(modem.esc_count >= 3)
						modem.esc_count = 0;
					else  {
						if(now - lasttx < guard_time(&modem))
							if(*buf == modem.esc)
								modem.esc_count += count_esc(&modem, buf, rd);
							else
								modem.esc_count = 0;
					}
				} else {
					if(now - lasttx > guard_time(&modem))
						modem.esc_count = count_esc(&modem, buf, rd);
				}
				size_t len = rd;
				uint8_t* p = buf;
				if(mode == TELNET) {
					len = telnet_expand(buf, rd, telnet_buf, sizeof(telnet_buf)
						,telnet.local_option[TELNET_BINARY_TX] != TELNET_WILL // expand_cr
						,&p);
					if(len != rd)
						dprintf("Telnet expanded %d bytes to %d", rd, len);
				}
				int wr = send(sock, p, len, /* flags: */0);
				if(wr != len)
					dprintf("Sent %d instead of %d", wr, len);
				else if(cfg.debug)
					dprintf("TX: %d bytes", wr);
			} else { // Command mode
				dprintf("RX command: '%.*s'\n", rd, buf);
				if(!modem.echo_off)
					vdd_write(&wrslot, buf, rd);
				char* response = atmodem_parse(&modem, buf, rd);
				if(response != NULL) {
					vdd_writestr(&wrslot, response);
					SKIP_WHITESPACE(response);
					dprintf("Modem response: %s", response);
				}
			}
			lasttx = now;
		} else {
			if(modem.online && modem.esc_count == 3 && now - lasttx >= guard_time(&modem)) {
				dprintf("Entering command mode");
				modem.online = false;
				modem.esc_count = 0;
				vdd_writestr(&wrslot, ok(&modem));
			}
		}
	}

	int retval = EXIT_SUCCESS;
	fp = fopen("DOSXTRN.RET", "r");
	if(fp == NULL) {
		perror("DOSXTRN.RET");
	} else {
		if(fscanf(fp, "%d", &retval) != 1) {
			fprintf(stderr, "Error reading return value from DOSXTRN.REG");
			retval = EXIT_FAILURE;
		}
		fclose(fp);
		if(retval == -1) {
			fprintf(stderr, "DOSXTRN failed to execute '%s': ", dospgm);
			fp = fopen("DOSXTRN.ERR", "r");
			if(fp == NULL) {
				perror("DOSXTRN.ERR");
			} else {
				char errstr[256] = "";
				int errval = 0;
				if(fscanf(fp, "%d\n", &errval) == 1) {
					fgets(errstr, sizeof(errstr), fp);
					truncsp(errstr);
					fprintf(stderr, "Error %d (%s)\n", errval, errstr);
				} else
					fprintf(stderr, "Failed to parse DOSXTRN.ERR\n");
				fclose(fp);
			}
		}
	}

	if(cfg.debug) {
		printf("rx_delay: %lld\n", rx_delay);
		printf("rx_buflen: %ld\n", rx_buflen);
		printf("largest recv: %d\n", largest_recv);
	}
	return retval;
}
