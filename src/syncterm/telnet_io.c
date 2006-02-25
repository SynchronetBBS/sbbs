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

extern FILE*	log_fp;
int	telnet_log_level;

static int lprintf(int level, const char *fmt, ...)
{
	char sbuf[1024];
	va_list argptr;

	if(log_fp==NULL || level > telnet_log_level)
		return 0;

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(fprintf(log_fp, "%s\n", sbuf));
}

void putcom(BYTE* buf, size_t len)
{
	char	str[128];
	char*	p=str;
	size_t i;

	for(i=0;i<len;i++)
		p+=sprintf(p,"%02X ", buf[i]);

	lprintf(LOG_DEBUG,"TX: %s", str);
	send(conn_socket, buf, len, 0);
}

static void send_telnet_cmd(uchar cmd, uchar opt)
{
	char buf[16];
	
	if(cmd<TELNET_WILL) {
		lprintf(LOG_INFO,"TX Telnet command: %s"
			,telnet_cmd_desc(cmd));
		sprintf(buf,"%c%c",TELNET_IAC,cmd);
		putcom(buf,2);
	} else {
		lprintf(LOG_INFO,"TX Telnet command: %s %s"
			,telnet_cmd_desc(cmd), telnet_opt_desc(opt));
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

    if(!telnet_cmdlen	&& first_iac==NULL) {
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
						int len=sprintf(buf,"%c%c%c%cANSI%c%c"
							,TELNET_IAC,TELNET_SB
							,TELNET_TERM_TYPE,TELNET_TERM_IS
							,TELNET_IAC,TELNET_SE);
						lprintf(LOG_INFO,"TX Telnet command: Terminal Type is ANSI");
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

				lprintf(LOG_INFO,"RX Telnet command: %s %s"
					,telnet_cmd_desc(command),telnet_opt_desc(option));

				if(command==TELNET_DO || command==TELNET_DONT) {	/* local options */
					if(telnet_local_option[option]!=command) {
						switch(option) {
							case TELNET_BINARY_TX:
							case TELNET_ECHO:
							case TELNET_TERM_TYPE:
							case TELNET_SUP_GA:
							case TELNET_NEGOTIATE_WINDOW_SIZE:
								telnet_local_option[option]=command;
								send_telnet_cmd(telnet_opt_ack(command),option);
								break;
							default: /* unsupported local options */
								if(command==TELNET_DO) /* NAK */
									send_telnet_cmd(telnet_opt_nak(command),option);
								break;
						}
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
						lprintf(LOG_INFO,"TX Telnet command: Window Size is %u x %u"
							,term.width, term.height);
						putcom(buf,9);
					}

				} else { /* WILL/WONT (remote options) */ 
					if(telnet_remote_option[option]!=command) {	

						switch(option) {
							case TELNET_BINARY_TX:
							case TELNET_ECHO:
							case TELNET_TERM_TYPE:
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
