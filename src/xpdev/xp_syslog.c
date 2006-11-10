#include <stdarg.h>
#ifdef __unix__
#include <syslog.h>
#endif

#include "xp_syslog.h"
#include "sockwrap.h"
#include "gen_defs.h"

static SOCKET syslog_sock=INVALID_SOCKET;
static char log_host[1025]=
static int log_opts=LOG_CONS;
#ifdef __unix__
"";
#else
"localhost";
#endif

static char syslog_tag[33]="undefined";
static int default_facility=LOG_USER;

static u_long resolve_ip(char *addr)
{
	HOSTENT*        host;
	char*           p;

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

void xp_set_loghost(const char *hostname)
{
	SAFECOPY(log_host, hostname);
}

void xp_openlog(const char *ident, int logopt, int facility)
{
	int		sock;
	SOCKADDR_IN	syslog_addr;
	socklen_t	syslog_len;

#ifdef __unix__
	if(!log_host[0]) {
		openlog(ident,logopt,facility);
		return;
	}
#endif

	memset(&syslog_addr, 0, sizeof(syslog_addr));
	syslog_addr.sin_addr.s_addr=INADDR_ANY;
	syslog_addr.sin_family=AF_INET;
	syslog_addr.sin_port=htons(SYSLOG_PORT);

	syslog_sock=socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(syslog_sock==INVALID_SOCKET)
		return;
	if(bind(syslog_sock, (struct sockaddr *)&syslog_addr, sizeof(syslog_addr))) {
		syslog_addr.sin_port=0;
		if(bind(syslog_sock, (struct sockaddr *)&syslog_addr, sizeof(syslog_addr)))
			return;
	}
	syslog_addr.sin_addr.s_addr=resolve_ip(log_host);
	syslog_addr.sin_port=htons(SYSLOG_PORT);
	if(connect(syslog_sock, (struct sockaddr *)&syslog_addr, sizeof(syslog_addr))) {
		closesocket(syslog_sock);
		syslog_sock=INVALID_SOCKET;
		return;
	}

	log_opts=logopt;

	SAFECOPY(syslog_tag, ident);
}

void xp_vsyslog(int priority, const char *message, va_list va)
{
	int orig_errno=ERROR_VALUE;
	time_t	now;
	char	timestr[32];
	char	msg_to_send[1025];
	char	format[1025];
	char	*msg_end;
	char	*p;
	int		i;

#ifdef __unix__
	if(!log_host[0]) {
		vsyslog(priority, message, va);
		return;
	}
#endif

	/* Ensure we can even send this thing */
	if(syslog_sock==INVALID_SOCKET && !(log_opts & LOG_CONS) && !(log_opts & LOG_PERROR))
		return;

	/* Check for valid priority */
	if(priority & ~(LOG_PRIMASK|LOG_FACMASK)) {
		i=priority;
		/* Fix to something legal */
		priority &= (LOG_PRIMASK|LOG_FACMASK);
		/* Ooooo... re-entrant! */
		syslog(priority,"Unknown priority %x",i);
	}

	/* Set default facility if needed */
	if(!(priority & LOG_FACMASK))
		priority |= default_facility;

	time(&now);
	ctime_r(&now, timestr);
	/* Remove trailing \n */
	timestr[24]=0;

	snprintf(format, sizeof(format), "<%d>%.15s %.32s: %s", priority, timestr+4 /* Remove day of week */, syslog_tag, message);
	format[sizeof(format)-1]=0;
	vsnprintf(msg_to_send, sizeof(msg_to_send), format, va);
	msg_to_send[sizeof(msg_to_send)-1]=0;

	/* Clean out invalid chars (as per RFC3164) */
	msg_end=msg_to_send+sizeof(msg_to_send)-1;
	for(p=msg_to_send; *p; p++) {
		if(*p<32) {
			i=strlen(p);
			memmove(p, p+1, i);
			*p='^';
			p++;
			*p|=0x40;
			if(p+i > msg_end)
				*msg_end=0;
			else
				*(p+i)=0;
		}
		else if(*p>126)
			*p=' ';
	}
	if(syslog_sock!=INVALID_SOCKET)
		sendsocket(syslog_sock, msg_to_send, strlen(msg_to_send));
	else if(log_opts & LOG_CONS)
		puts(msg_to_send);
	if(log_opts & LOG_PERROR) {
		fputs(stderr,msg_to_send);
		fputs(stderr,"\n");
	}
}

void xp_syslog(int priority, const char *message, ...)
{
	va_list va;

	va_start(va, message);
	vsyslog(priority, message, va);
	va_end(va);
}
