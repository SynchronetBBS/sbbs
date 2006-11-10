#ifndef XP_SYSLOG_H_
#define XP_SYSLOG_H_
#include <stdarg.h>


/* priorities */
#define LOG_EMERG		0		/* system is unusable */
#define LOG_ALERT		1		/* action must be taken immediately */
#define LOG_CRIT		2		/* critical conditions */
#define LOG_ERR			3		/* error conditions */
#define LOG_WARNING		4		/* warning conditions */
#define LOG_NOTICE		5		/* normal but significant condition */
#define LOG_INFO		6		/* informational */
#define LOG_DEBUG		7		/* debug-level messages */

#define LOG_KERN		(0<<3)	/* kernel messages */
#define LOG_USER		(1<<3)	/* random user-level messages */
#define LOG_MAIL		(2<<3)	/* mail system */
#define LOG_DAEMON		(3<<3)	/* system daemons */
#define LOG_AUTH		(4<<3)	/* authorization messages */
#define LOG_SYSLOG		(5<<3)	/* messages generated internally by syslogd */
#define LOG_LPR			(6<<3)	/* line printer subsystem */
#define LOG_NEWS		(7<<3)	/* network news subsystem */
#define LOG_UUCP		(8<<3)	/* UUCP subsystem */
#define LOG_CRON		(9<<3)	/* clock daemon */
#define LOG_AUTHPRIV	(10<<3)	/* authorization messages (private) */
								/* Facility #10 clashes in DEC UNIX, where */
								/* it's defined as LOG_MEGASAFE for AdvFS  */
								/* event logging.                          */
#define LOG_FTP			(11<<3)	/* ftp daemon */
#define LOG_NTP			(12<<3)	/* NTP subsystem */
#define LOG_SECURITY	(13<<3)	/* security subsystems (firewalling, etc.) */
#define LOG_CONSOLE		(14<<3)	/* /dev/console output */

/* facility codes */
#define LOG_LOCAL0		(16<<3)	/* reserved for local use */
#define LOG_LOCAL1		(17<<3)	/* reserved for local use */
#define LOG_LOCAL2		(18<<3)	/* reserved for local use */
#define LOG_LOCAL3		(19<<3)	/* reserved for local use */
#define LOG_LOCAL4		(20<<3)	/* reserved for local use */
#define LOG_LOCAL5		(21<<3)	/* reserved for local use */
#define LOG_LOCAL6		(22<<3)	/* reserved for local use */
#define LOG_LOCAL7		(23<<3)	/* reserved for local use */

/* Masks to extract parts */
#define LOG_PRIMASK     0x07    /* mask to extract priority part */
#define LOG_FACMASK     0x03f8  /* mask to extract facility part */

/* Logopts for xp_openlog() */
#define LOG_CONS		0x02	/* log on the console if errors in sending */
#define LOG_PERROR		0x20	/* log to stderr as well */

/* Port number */
#define SYSLOG_PORT 514

void xp_set_loghost(const char *hostname);
void xp_openlog(const char *ident, int logopt, int facility);
void xp_vsyslog(int priority, const char *message, va_list va);
void xp_syslog(int priority, const char *message, ...);

#endif
