/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * Major Changelog:
 *
 * Jordan K. Hubbard
 * 17 Jan 1996
 *
 * Turned inside out. Now returns xfers as new file ids, not as a special
 * `state' of FTP_t
 *
 * Stephen J. Hurd
 * 23 Jan 2003
 *
 * Severly mangled for use in the Synchronet installer
 *
 * $Id$
 *
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftpio.h"

#define SUCCESS		 0
#define FAILURE		-1

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

/* How to see by a given code whether or not the connection has timed out */
#define FTP_TIMEOUT(code)	(FtpTimedOut || code == FTP_TIMED_OUT)

/* Internal routines - deal only with internal FTP_t type */
static FTP_t	ftp_new(void);
static int	ftp_read_method(void *n, char *buf, int nbytes);
static int	ftp_close_method(void *n);
static int	writes(int fd, char *s);
static __inline char *get_a_line(FTP_t ftp);
static int	get_a_number(FTP_t ftp, char **q);
static int	botch(char *func, char *botch_state);
static int	cmd(FTP_t ftp, const char *fmt, ...);
static int	ftp_login_session(FTP_t ftp, char *host, char *user, char *passwd, int port, int verbose);
static int	ftp_file_op(FTP_t ftp, char *operation, char *file, ftp_FILE **, char *mode, off_t *seekto);
static int	ftp_close(FTP_t ftp);
static int	get_url_info(char *url_in, char *host_ret, int *port_ret, char *name_ret);
static void	ftp_timeout(int sig);
static void	ftp_set_timeout(void);
static void	ftp_clear_timeout(void);
ftp_FILE *      ftpopen(void *cookie, int (*readfn)(void *, char *, int),
	             int (*writefn)(void *, const char *, int),
	             fpos_t (*seekfn)(void *, fpos_t, int), int (*closefn)(void *));
ftp_FILE *	ftpdopen(int filedes, const char *mode);

/* Global status variable - ick */
int FtpTimedOut;

/* FTP happy status codes */
#define FTP_GENERALLY_HAPPY	200
#define FTP_ASCII_HAPPY		FTP_GENERALLY_HAPPY
#define FTP_BINARY_HAPPY	FTP_GENERALLY_HAPPY
#define FTP_PORT_HAPPY		FTP_GENERALLY_HAPPY
#define FTP_HAPPY_COMMENT	220
#define FTP_QUIT_HAPPY		221
#define FTP_TRANSFER_HAPPY	226
#define FTP_PASSIVE_HAPPY	227
#define FTP_LPASSIVE_HAPPY	228
#define FTP_EPASSIVE_HAPPY	229
#define FTP_CHDIR_HAPPY		250

/* FTP unhappy status codes */
#define FTP_TIMED_OUT		421

/*
 * XXX
 * gross!  evil!  bad!  We really need an access primitive for cookie in stdio itself.
 * it's too convenient a hook to bury and it's already exported through funopen as it is, so...
 * XXX
 */
#define fcookie(fp)	((fp)->_cookie)

/* Check a return code with some lenience for back-dated garbage that might be in the buffer */
static int
check_code(FTP_t ftp, int var, int preferred)
{
    ftp->error = 0;
    while (1) {
	if (var == preferred)
	    return 0;
	else if (var == FTP_TRANSFER_HAPPY)	/* last operation succeeded */
	    var = get_a_number(ftp, NULL);
	else if (var == FTP_HAPPY_COMMENT)	/* chit-chat */
	    var = get_a_number(ftp, NULL);
	else if (var == FTP_GENERALLY_HAPPY)	/* general success code */
	    var = get_a_number(ftp, NULL);
	else {
	    ftp->error = var;
	    return 1;
	}
    }
}
    
int
ftpBinary(ftp_FILE *fp)
{
    FTP_t ftp = fcookie(fp);
    int i;

    if (ftp->is_binary)
	return SUCCESS;
    i = cmd(ftp, "TYPE I");
    if (i < 0 || check_code(ftp, i, FTP_BINARY_HAPPY))
	return i;
    ftp->is_binary = TRUE;
    return SUCCESS;
}

int
ftpChdir(ftp_FILE *fp, char *dir)
{
    int i;
    FTP_t ftp = fcookie(fp);

    i = cmd(ftp, "CWD %s", dir);
    if (i < 0 || check_code(ftp, i, FTP_CHDIR_HAPPY))
	return i;
    return SUCCESS;
}

int
ftpErrno(ftp_FILE *fp)
{
    FTP_t ftp = fcookie(fp);
    return ftp->error;
}

const char *
ftpErrString(int error)
{
    int	k;

    if (error == -1)
	return("connection in wrong state");
    if (error < 100)
	/* XXX soon UNIX errnos will catch up with FTP protocol errnos */
	return strerror(error);
    for (k = 0; k < ftpErrListLength; k++)
      if (ftpErrList[k].num == error)
	return(ftpErrList[k].string);
    return("Unknown error");
}

ftp_FILE *
ftpGet(ftp_FILE *fp, char *file, off_t *seekto)
{
    ftp_FILE *fp2;
    FTP_t ftp = fcookie(fp);

    ftpPassive(fp,TRUE);
    if (ftpBinary(fp) != SUCCESS)
	return NULL;

    if (ftp_file_op(ftp, "RETR", file, &fp2, "r", seekto) == SUCCESS)
	return fp2;
    return NULL;
}

/* Returns a standard ftp_FILE pointer type representing an open control connection */
ftp_FILE *
ftpLogin(char *host, char *user, char *passwd, int port, int verbose, int *retcode)
{
    FTP_t n;
    ftp_FILE *fp;

    if (retcode)
	*retcode = 0;

    n = ftp_new();
    fp = NULL;
    if (n && ftp_login_session(n, host, user, passwd, port, verbose) == SUCCESS) {
	fp = ftpopen(n, ftp_read_method, NULL, NULL, ftp_close_method);	/* BSD 4.4 function! */
	fp->_file = n->fd_ctrl;
    }
    if (retcode) {
	if (!n)
	    *retcode = (FtpTimedOut ? FTP_TIMED_OUT : -1);
	/* Poor attempt at mapping real errnos to FTP error codes */
	else switch(n->error) {
	    case EADDRNOTAVAIL:
		*retcode = FTP_TIMED_OUT;	/* Actually no such host, but we have no way of saying that. :-( */
		break;

            case ETIMEDOUT:
		*retcode = FTP_TIMED_OUT;
		break;

	    default:
		*retcode = n->error;
		break;
	}
    }
    return fp;
}

int
ftpPassive(ftp_FILE *fp, int st)
{
    FTP_t ftp = fcookie(fp);

    ftp->is_passive = !!st;	/* normalize "st" to zero or one */
    return SUCCESS;
}

ftp_FILE *
ftpGetURL(char *url, char *user, char *passwd, int *retcode)
{
    char host[255], name[255];
    int port;
    ftp_FILE *fp2;
    static ftp_FILE *fp = NULL;
    static char *prev_host;

    if (retcode)
	*retcode = 0;
    if (get_url_info(url, host, &port, name) == SUCCESS) {
	if (fp && prev_host) {
	    if (!strcmp(prev_host, host)) {
		/* Try to use cached connection */
		fp2 = ftpGet(fp, name, NULL);
		if (!fp2) {
		    /* Connection timed out or was no longer valid */
		    fp->closefn(fp);
		    free(prev_host);
		    prev_host = NULL;
		}
		else
		    return fp2;
	    }
	    else {
		/* It's a different host now, flush old */
		fp->closefn(fp);
		free(prev_host);
		prev_host = NULL;
	    }
	}
	fp = ftpLogin(host, user, passwd, port, 0, retcode);
	if (fp) {
	    fp2 = ftpGet(fp, name, NULL);
	    if (!fp2) {
		/* Connection timed out or was no longer valid */
		if (retcode)
		    *retcode = ftpErrno(fp);
		fp->closefn(fp);
		fp = NULL;
	    }
	    else
		prev_host = strdup(host);
	    return fp2;
	}
    }
    return NULL;
}

/* Internal workhorse function for dissecting URLs.  Takes a URL as the first argument and returns the
   result of such disection in the host, user, passwd, port and name variables. */
static int
get_url_info(char *url_in, char *host_ret, int *port_ret, char *name_ret)
{
    char *name, *host, *cp, url[BUFSIZ];
    int port;

    name = host = NULL;
    /* XXX add http:// here or somewhere reasonable at some point XXX */
    if (strncmp("ftp://", url_in, 6) != 0)
	return FAILURE;
    /* We like to stomp a lot on the URL string in dissecting it, so copy it first */
    strncpy(url, url_in, BUFSIZ);
    host = url + 6;
    if ((cp = index(host, ':')) != NULL) {
	*(cp++) = '\0';
	port = strtol(cp, 0, 0);
    }
    else
	port = 0;	/* use default */
    if (port_ret)
	*port_ret = port;
    
    if ((name = index(cp ? cp : host, '/')) != NULL)
	*(name++) = '\0';
    if (host_ret)
	strcpy(host_ret, host);
    if (name && name_ret)
	strcpy(name_ret, name);
    return SUCCESS;
}

static FTP_t
ftp_new(void)
{
    FTP_t ftp;

    ftp = (FTP_t)malloc(sizeof *ftp);
    if (!ftp)
	return NULL;
    memset(ftp, 0, sizeof *ftp);
    ftp->fd_ctrl = -1;
    ftp->con_state = init;
    ftp->is_binary = FALSE;
    ftp->is_passive = FALSE;
    ftp->is_verbose = FALSE;
    ftp->error = 0;
    return ftp;
}

static int
ftp_read_method(void *vp, char *buf, int nbytes)
{
    int i, fd;
    FTP_t n = (FTP_t)vp;

    fd = n->fd_ctrl;
    i = (fd >= 0) ? read(fd, buf, nbytes) : EOF;
    return i;
}

static int
ftp_close_method(void *n)
{
    int i;

    i = ftp_close((FTP_t)n);
    free(n);
    return i;
}

static void
ftp_timeout(int sig)
{
    FtpTimedOut = TRUE;
    /* Debug("ftp_pkg: ftp_timeout called - operation timed out"); */
}

static void
ftp_set_timeout(void)
{
    struct sigaction new;
    char *cp;
    int ival;

    FtpTimedOut = FALSE;
    sigemptyset(&new.sa_mask);
    new.sa_flags = 0;
    new.sa_handler = ftp_timeout;
    sigaction(SIGALRM, &new, NULL);
    cp = getenv("FTP_TIMEOUT");
    if (!cp || !(ival = atoi(cp)))
	ival = 120;
    alarm(ival);
}

static void
ftp_clear_timeout(void)
{
    struct sigaction new;

    alarm(0);
    sigemptyset(&new.sa_mask);
    new.sa_flags = 0;
    new.sa_handler = SIG_DFL;
    sigaction(SIGALRM, &new, NULL);
}

static int
writes(int fd, char *s)
{
    int n, i = strlen(s);

    ftp_set_timeout();
    n = write(fd, s, i);
    ftp_clear_timeout();
    if (FtpTimedOut || i != n)
	return TRUE;
    return FALSE;
}

static __inline char *
get_a_line(FTP_t ftp)
{
    static char buf[BUFSIZ];
    int i,j;

    /* Debug("ftp_pkg: trying to read a line from %d", ftp->fd_ctrl); */
    for(i = 0; i < BUFSIZ;) {
	ftp_set_timeout();
	j = read(ftp->fd_ctrl, buf + i, 1);
	ftp_clear_timeout();
	if (FtpTimedOut || j != 1)
	    return NULL;
	if (buf[i] == '\r' || buf[i] == '\n') {
	    if (!i)
		continue;
	    buf[i] = '\0';
	    if (ftp->is_verbose == TRUE)
		fprintf(stderr, "%s\n",buf+4);
	    return buf;
	}
	i++;
    }
    /* Debug("ftp_pkg: read string \"%s\" from %d", buf, ftp->fd_ctrl); */
    return buf;
}

static int
get_a_number(FTP_t ftp, char **q)
{
    char *p;
    int i = -1, j;

    while(1) {
	p = get_a_line(ftp);
	if (!p) {
	    ftp_close(ftp);
	    if (FtpTimedOut)
		return FTP_TIMED_OUT;
	    return FAILURE;
	}
	if (!(isdigit(p[0]) && isdigit(p[1]) && isdigit(p[2])))
	    continue;
	if (i == -1 && p[3] == '-') {
	    i = strtol(p, 0, 0);
	    continue;
	}
	if (p[3] != ' ' && p[3] != '\t')
	    continue;
	j = strtol(p, 0, 0);
	if (i == -1) {
	    if (q) *q = p+4;
	    /* Debug("ftp_pkg: read reply %d from server (%s)", j, p); */
	    return j;
	} else if (j == i) {
	    if (q) *q = p+4;
	    /* Debug("ftp_pkg: read reply %d from server (%s)", j, p); */
	    return j;
	}
    }
}

static int
ftp_close(FTP_t ftp)
{
    int i, rcode;

    rcode = FAILURE;
    if (ftp->con_state == isopen) {
	ftp->con_state = quit;
	/* If last operation timed out, don't try to quit - just close */
	if (ftp->error != FTP_TIMED_OUT)
	    i = cmd(ftp, "QUIT");
	else
	    i = FTP_QUIT_HAPPY;
	if (!check_code(ftp, i, FTP_QUIT_HAPPY))
	    rcode = SUCCESS;
	close(ftp->fd_ctrl);
	ftp->fd_ctrl = -1;
    }
    else if (ftp->con_state == quit)
	rcode = SUCCESS;
    return rcode;
}

static int
botch(char *func, char *botch_state)
{
    /* Debug("ftp_pkg: botch: %s(%s)", func, botch_state); */
    return FAILURE;
}

static int
cmd(FTP_t ftp, const char *fmt, ...)
{
    char p[BUFSIZ];
    int i;

    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(p, sizeof p, fmt, ap);
    va_end(ap);

    if (ftp->con_state == init)
	return botch("cmd", "open");

    strcat(p, "\r\n");
    if (ftp->is_verbose)
	fprintf(stderr, "Sending: %s", p);
    if (writes(ftp->fd_ctrl, p)) {
	if (FtpTimedOut)
	    return FTP_TIMED_OUT;
	return FAILURE;
    }
    while ((i = get_a_number(ftp, NULL)) == FTP_HAPPY_COMMENT);
    return i;
}

static int
ftp_login_session(FTP_t ftp, char *host, 
		  char *user, char *passwd, int port, int verbose)
{
    char pbuf[10];
    struct addrinfo	hints, *res, *res0;
    int			err;
    int 		s;
    int			i;

    if (ftp->con_state != init) {
	ftp_close(ftp);
	ftp->error = -1;
	return FAILURE;
    }

    if (!user)
	user = "ftp";

    if (!passwd)
	passwd = "setup@";

    if (!port)
	port = 21;

    snprintf(pbuf, sizeof(pbuf), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    err = getaddrinfo(host, pbuf, &hints, &res0);
    if (err) {
	ftp->error = 0;
	return FAILURE;
    }

    s = -1;
    for (res = res0; res; res = res->ai_next) {
	ftp->addrtype = res->ai_family;

	if ((s = socket(res->ai_family, res->ai_socktype,
			res->ai_protocol)) < 0)
	    continue;

	if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
	    (void)close(s);
	    s = -1;
	    continue;
	}

	break;
    }
    freeaddrinfo(res0);
    if (s < 0) {
	ftp->error = errno;
	return FAILURE;
    }

    ftp->fd_ctrl = s;
    ftp->con_state = isopen;
    ftp->is_verbose = verbose;

    i = cmd(ftp, "USER %s", user);
    if (i >= 300 && i < 400)
	i = cmd(ftp, "PASS %s", passwd);
    if (i >= 299 || i < 0) {
	ftp_close(ftp);
	if (i > 0)
	    ftp->error = i;
	return FAILURE;
    }
    return SUCCESS;
}

static int
ftp_file_op(FTP_t ftp, char *operation, char *file, ftp_FILE **fp, char *mode, off_t *seekto)
{
    int i,l,s;
    char *q;
    unsigned char addr[64];
    union sockaddr_cmn {
	struct sockaddr_in sin4;
	struct sockaddr_in6 sin6;
    } sin;
    char *cmdstr;

    if (!fp)
	return FAILURE;
    *fp = NULL;

    if (ftp->con_state != isopen)
	return botch("ftp_file_op", "open");

    if ((s = socket(ftp->addrtype, SOCK_STREAM, 0)) < 0) {
	ftp->error = errno;
	return FAILURE;
    }

    if (ftp->is_passive) {
        if (ftp->is_verbose)
	    fprintf(stderr, "Sending PASV\n");
	if (writes(ftp->fd_ctrl, "PASV\r\n")) {
	    ftp_close(ftp);
	    if (FtpTimedOut)
	    ftp->error = FTP_TIMED_OUT;
	    return FTP_TIMED_OUT;
	}
	i = get_a_number(ftp, &q);
	if (check_code(ftp, i, FTP_PASSIVE_HAPPY)) {
	    ftp_close(ftp);
	    return i;
	}
	cmdstr = "PASV";
	while (*q && !isdigit(*q))
	    q++;
	if (!*q) {
	    ftp_close(ftp);
	    return FAILURE;
	}
	q--;
	l = (ftp->addrtype == AF_INET ? 6 : 21);
	for (i = 0; i < l; i++) {
	    q++;
	    addr[i] = strtol(q, &q, 10);
	}

	sin.sin4.sin_family = ftp->addrtype;
	bcopy(addr, (char *)&sin.sin4.sin_addr, 4);
	bcopy(addr + 4, (char *)&sin.sin4.sin_port, 2);
	if (connect(s, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
	    (void)close(s);
	    return FAILURE;
	}

	if (seekto && *seekto) {
	    i = cmd(ftp, "REST %d", *seekto);
	    if (i < 0 || FTP_TIMEOUT(i)) {
		close(s);
		ftp->error = i;
		*seekto = (off_t)0;
		return i;
	    }
	}
	i = cmd(ftp, "%s %s", operation, file);
	if (i < 0 || i > 299) {
	    close(s);
	    ftp->error = i;
	    return i;
	}
	*fp = ftpdopen(s, mode);
    }
    else {
	int fd;

#ifdef IP_PORTRANGE
	int portrange;

	if (ftp->addrtype == AF_INET) {
		portrange = IP_PORTRANGE_HIGH;
		if (setsockopt(s, IPPROTO_IP, IP_PORTRANGE, (char *)
			       &portrange, sizeof(portrange)) < 0) {
			close(s);   
			return FAILURE;
		}
	}
#endif

	i = sizeof sin;
	getsockname(ftp->fd_ctrl, (struct sockaddr *)&sin, &i);
	sin.sin4.sin_port = 0;
	i = sizeof(struct sockaddr_in);
	if (bind(s, (struct sockaddr *)&sin, i) < 0) {
	    close(s);	
	    return FAILURE;
	}
	i = sizeof sin;
	getsockname(s,(struct sockaddr *)&sin,&i);
	if (listen(s, 1) < 0) {
	    close(s);	
	    return FAILURE;
	}
	if (sin.sin4.sin_family == AF_INET) {
            u_long a;
	    a = ntohl(sin.sin4.sin_addr.s_addr);
	    i = cmd(ftp, "PORT %d,%d,%d,%d,%d,%d",
		    (a                   >> 24) & 0xff,
		    (a                   >> 16) & 0xff,
		    (a                   >>  8) & 0xff,
		    a                           & 0xff,
		    (ntohs(sin.sin4.sin_port) >>  8) & 0xff,
		    ntohs(sin.sin4.sin_port)         & 0xff);
	    if (check_code(ftp, i, FTP_PORT_HAPPY)) {
		close(s);
		return i;
	    }
	}
	if (seekto && *seekto) {
	    i = cmd(ftp, "REST %d", *seekto);
	    if (i < 0 || FTP_TIMEOUT(i)) {
		close(s);
		ftp->error = i;
		return i;
	    }
	    else if (i != 350)
		*seekto = (off_t)0;
	}
	i = cmd(ftp, "%s %s", operation, file);
	if (i < 0 || i > 299) {
	    close(s);
	    ftp->error = i;
	    return FAILURE;
	}
	fd = accept(s, 0, 0);
	if (fd < 0) {
	    close(s);
	    ftp->error = 401;
	    return FAILURE;
	}
	close(s);
	*fp = ftpdopen(fd, mode);
    }
    if (*fp)
	return SUCCESS;
    else
	return FAILURE;
}

struct ftperr ftpErrList[] = {  
  { 110, "Restart marker reply" },
  { 120, "Service ready in a few minutes" },
  { 125, "Data connection already open; transfer starting" },
  { 150, "File status okay; about to open data connection" },
  { 200, "Command okay" },
  { 202, "Command not implemented, superfluous at this site" },
  { 211, "System status, or system help reply" },
  { 212, "Directory status" },
  { 213, "File status" },
  { 214, "Help message" },
  { 215, "Set system type" },
  { 220, "Service ready for new user" },
  { 221, "Service closing control connection" },
  { 225, "Data connection open; no transfer in progress" },
  { 226, "Requested file action successful" },
  { 227, "Entering Passive Mode" },
  { 229, "Entering Extended Passive Mode" },
  { 230, "User logged in, proceed" },
  { 250, "Requested file action okay, completed" },
  { 257, "File/directory created" },
  { 331, "User name okay, need password" },
  { 332, "Need account for login" },
  { 350, "Requested file action pending further information" },
  { 421, "Service not available, closing control connection" },
  { 425, "Can't open data connection" },
  { 426, "Connection closed; transfer aborted" },
  { 450, "File unavailable (e.g., file busy)" },
  { 451, "Requested action aborted: local error in processing" },
  { 452, "Insufficient storage space in system" },
  { 500, "Syntax error, command unrecognized" },
  { 501, "Syntax error in parameters or arguments" },
  { 502, "Command not implemented" },
  { 503, "Bad sequence of commands" },
  { 504, "Command not implemented for that parameter" },
  { 530, "Not logged in" },
  { 532, "Need account for storing files" },
  { 550, "File unavailable (e.g., file not found, no access)" },
  { 551, "Requested action aborted. Page type unknown" },
  { 552, "Exceeded storage allocation" },
  { 553, "File name not allowed" },
};
int const ftpErrListLength = sizeof(ftpErrList) / sizeof(*ftpErrList);

char *
ftpgets(char *str, int size, void *stream)
{
    static char buf[BUFSIZ];
    char *p=NULL;
    int i,j;

    /* Debug("ftp_pkg: trying to read a line from %d", ftp->fd_ctrl); */
    for(i = 0; i < BUFSIZ;) {
	ftp_set_timeout();
	j = read(((ftp_FILE *)stream)->_file, buf + i, 1);
	ftp_clear_timeout();
	if (FtpTimedOut || j != 1)  {
	    p=NULL;
	    i=BUFSIZ;
	}
	if (buf[i] == '\n') {
	    buf[i+1] = '\0';
	    p=buf;
	    i=BUFSIZ;
	}
	i++;
    }
    if(p!=NULL)  {
    	strcpy(str,p);
	return(str);
    }
    return(NULL);
}

ssize_t
ftpread(void *stream, void *buf, size_t nbytes)
{
	return(read(((ftp_FILE *)stream)->_file,buf,nbytes));
}

ftp_FILE *
ftpopen(void *cookie, int (*readfn)(void *, char *, int),
         int (*writefn)(void *, const char *, int),
         fpos_t (*seekfn)(void *, fpos_t, int), int (*closefn)(void *))
{
    ftp_FILE *f;
    
    f=malloc(sizeof(ftp_FILE));
    f->_cookie=cookie;
    f->readfn=readfn;
    f->writefn=writefn;
    f->seekfn=seekfn;
    f->closefn=closefn;
    f->gets=ftpgets;
	f->read=ftpread;
    return(f);
}

ftp_FILE *
ftpdopen(int filedes, const char *mode)
{
    ftp_FILE *f;
    
    f=malloc(sizeof(ftp_FILE));
    f->_cookie=NULL;
    f->_file=filedes;
    f->readfn=ftp_read_method;
    f->writefn=NULL;
    f->seekfn=NULL;
    f->closefn=ftp_close_method;
    f->gets=ftpgets;
	f->read=ftpread;
    return(f);
}
