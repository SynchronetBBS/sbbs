/* Copyright (C), 2007 by Stephen Hurd */

/* $Id$ */

#ifdef __unix__

#include <unistd.h>		/* _POSIX_VDISABLE - needed when termios.h is broken */
#include <signal.h>		// kill()
#include <sys/wait.h>	// WEXITSTATUS

#define TTYDEFCHARS		// needed for ttydefchars definition

#if defined(__FreeBSD__)
	#include <libutil.h>	// forkpty()
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DARWIN__)
	#include <util.h>
#elif defined(__linux__)
	#include <pty.h>
#elif defined(__QNX__)
#if 0
	#include <unix.h>
#else
	#define NEEDS_FORKPTY
#endif
#endif

	#ifdef NEEDS_FORKPTY
	#include <grp.h>
	#endif

	#include <termios.h>

/*
 * Control Character Defaults
 */
#ifndef CTRL
	#define CTRL(x)	(x&037)
#endif
#ifndef CEOF
	#define	CEOF		CTRL('d')
#endif
#ifndef CEOL
	#define	CEOL		0xff		/* XXX avoid _POSIX_VDISABLE */
#endif
#ifndef CERASE
	#define	CERASE		0177
#endif
#ifndef CERASE2
	#define	CERASE2		CTRL('h')
#endif
#ifndef CINTR
	#define	CINTR		CTRL('c')
#endif
#ifndef CSTATUS
	#define	CSTATUS		CTRL('t')
#endif
#ifndef CKILL
	#define	CKILL		CTRL('u')
#endif
#ifndef CMIN
	#define	CMIN		1
#endif
#ifndef CQUIT
	#define	CQUIT		034		/* FS, ^\ */
#endif
#ifndef CSUSP
	#define	CSUSP		CTRL('z')
#endif
#ifndef CTIME
	#define	CTIME		0
#endif
#ifndef CDSUSP
	#define	CDSUSP		CTRL('y')
#endif
#ifndef CSTART
	#define	CSTART		CTRL('q')
#endif
#ifndef CSTOP
	#define	CSTOP		CTRL('s')
#endif
#ifndef CLNEXT
	#define	CLNEXT		CTRL('v')
#endif
#ifndef CDISCARD
	#define	CDISCARD 	CTRL('o')
#endif
#ifndef CWERASE
	#define	CWERASE 	CTRL('w')
#endif
#ifndef CREPRINT
	#define	CREPRINT 	CTRL('r')
#endif
#ifndef CEOT
	#define	CEOT		CEOF
#endif
/* compat */
#ifndef CBRK
	#define	CBRK		CEOL
#endif
#ifndef CRPRNT
	#define CRPRNT		CREPRINT
#endif
#ifndef CFLUSH
	#define	CFLUSH		CDISCARD
#endif

#ifndef TTYDEF_IFLAG
	#ifndef IMAXBEL
		#define TTYDEF_IFLAG    (BRKINT | ICRNL | IXON | IXANY)
	#else
		#define TTYDEF_IFLAG    (BRKINT | ICRNL | IMAXBEL | IXON | IXANY)
	#endif
#endif
#ifndef TTYDEF_OFLAG
	#define TTYDEF_OFLAG    (OPOST | ONLCR)
#endif
#ifndef TTYDEF_LFLAG
	#define TTYDEF_LFLAG    (ECHO | ICANON | ISIG | IEXTEN | ECHOE|ECHOKE|ECHOCTL)
#endif
#ifndef TTYDEF_CFLAG
	#define TTYDEF_CFLAG    (CREAD | CS8 | HUPCL)
#endif
#if defined(__QNX__) || defined(__solaris__) || defined(__NetBSD__) || defined(__HAIKU__)
	static cc_t     ttydefchars[NCCS] = {
        CEOF,   CEOL,   CEOL,   CERASE, CWERASE, CKILL, CREPRINT,
        CERASE2, CINTR, CQUIT,  CSUSP,  CDSUSP, CSTART, CSTOP,  CLNEXT,
        CDISCARD, CMIN, CTIME,  CSTATUS
#ifndef __solaris__
	, _POSIX_VDISABLE
#endif
	};
#endif

#include <stdlib.h>
#include <xpprintf.h>

#include "bbslist.h"
#include "conn.h"
#include "uifcinit.h"
#include "ciolib.h"
#include "syncterm.h"
#include "fonts.h"
extern int default_font;

#ifdef NEEDS_CFMAKERAW
void
cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
}
#endif

#ifdef NEEDS_FORKPTY
static int login_tty(int fd)
{
	(void) setsid();
	if (!isatty(fd))
		return (-1);
	(void) dup2(fd, 0);
	(void) dup2(fd, 1);
	(void) dup2(fd, 2);
	if (fd > 2)
		(void) close(fd);
	return (0);
}

#ifdef NEEDS_DAEMON
/****************************************************************************/
/* Daemonizes the process                                                   */
/****************************************************************************/
int
daemon(int nochdir, int noclose)
{
    int fd;

    switch (fork()) {
    case -1:
        return (-1);
    case 0:
        break;
    default:
        _exit(0);
    }

    if (setsid() == -1)
        return (-1);

    if (!nochdir)
        (void)chdir("/");

    if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
        (void)dup2(fd, STDIN_FILENO);
        (void)dup2(fd, STDOUT_FILENO);
        (void)dup2(fd, STDERR_FILENO);
        if (fd > 2)
            (void)close(fd);
    }
    return (0);
}
#endif

static int openpty(int *amaster, int *aslave, char *name, struct termios *termp, struct winsize *winp)
{
	char line[] = "/dev/ptyXX";
	const char *cp1, *cp2;
	int master, slave, ttygid;
	struct group *gr;

	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = -1;

	for (cp1 = "pqrsPQRS"; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp2 = "0123456789abcdefghijklmnopqrstuv"; *cp2; cp2++) {
			line[5] = 'p';
			line[9] = *cp2;
			if ((master = open(line, O_RDWR, 0)) == -1) {
				if (errno == ENOENT)
					break; /* try the next pty group */
			} else {
				line[5] = 't';
				(void) chown(line, getuid(), ttygid);
				(void) chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
				/* Hrm... SunOS doesn't seem to have revoke
				(void) revoke(line); */
				if ((slave = open(line, O_RDWR, 0)) != -1) {
					*amaster = master;
					*aslave = slave;
					if (name)
						strcpy(name, line);
					if (termp)
						(void) tcsetattr(slave,
							TCSAFLUSH, termp);
					if (winp)
						(void) ioctl(slave, TIOCSWINSZ,
							(char *)winp);
					return (0);
				}
				(void) close(master);
			}
		}
	}
	errno = ENOENT;	/* out of ptys */
	return (-1);
}

static int forkpty(int *amaster, char *name, struct termios *termp, struct winsize *winp)
{
	int master, slave, pid;

	if (openpty(&master, &slave, name, termp, winp) == -1)
		return (-1);
	switch (pid = FORK()) {
	case -1:
		return (-1);
	case 0:
		/*
		 * child
		 */
		(void) close(master);
		login_tty(slave);
		return (0);
	}
	/*
	 * parent
	 */
	*amaster = master;
	(void) close(slave);
	return (pid);
}
#endif /* NEED_FORKPTY */

int master;
int child_pid;
static int status;

#ifdef __BORLANDC__
#pragma argsused
#endif
void pty_input_thread(void *args)
{
	fd_set	rds;
	int		rd;
	int	buffered;
	size_t	buffer;
	int i;

	SetThreadName("PTY Input");
	conn_api.input_thread_running=1;
	while(master != -1 && !conn_api.terminate) {
		if((i=waitpid(child_pid, &status, WNOHANG)))
			break;
		FD_ZERO(&rds);
		FD_SET(master, &rds);
#ifdef __linux__
		{
			struct timeval tv;
			tv.tv_sec=0;
			tv.tv_usec=500000;
			rd=select(master+1, &rds, NULL, NULL, &tv);
		}
#else
		rd=select(master+1, &rds, NULL, NULL, NULL);
#endif
		if(rd==-1) {
			if(errno==EBADF)
				break;
			rd=0;
		}
		if(rd==1) {
			rd=read(master, conn_api.rd_buf, conn_api.rd_buf_size);
			if(rd < 0)
				continue;
		}
		buffered=0;
		while(buffered < rd) {
			pthread_mutex_lock(&(conn_inbuf.mutex));
			buffer=conn_buf_wait_free(&conn_inbuf, rd-buffered, 100);
			buffered+=conn_buf_put(&conn_inbuf, conn_api.rd_buf+buffered, buffer);
			pthread_mutex_unlock(&(conn_inbuf.mutex));
		}
	}
	conn_api.input_thread_running=0;
}

#ifdef __BORLANDC__
#pragma argsused
#endif
void pty_output_thread(void *args)
{
	fd_set	wds;
	int		wr;
	int		ret;
	int	sent;

	SetThreadName("PTY Output");
	conn_api.output_thread_running=1;
	while(master != -1 && !conn_api.terminate) {
		if(waitpid(child_pid, &status, WNOHANG))
			break;
		pthread_mutex_lock(&(conn_outbuf.mutex));
		ret=0;
		wr=conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if(wr) {
			wr=conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent=0;
			while(sent < wr) {
				FD_ZERO(&wds);
				FD_SET(master, &wds);
#ifdef __linux__
				{
					struct timeval tv;
					tv.tv_sec=0;
					tv.tv_usec=500000;
					ret=select(master+1, NULL, &wds, NULL, &tv);
				}
#else
				ret=select(master+1, NULL, &wds, NULL, NULL);
#endif
				if(ret==-1) {
					if(errno==EBADF)
						break;
					ret=0;
				}
				if(ret==1) {
					ret=write(master, conn_api.wr_buf+sent, wr-sent);
					if(ret==-1)
						continue;
					sent+=ret;
				}
			}
		}
		else
			pthread_mutex_unlock(&(conn_outbuf.mutex));
		if(ret==-1)
			break;
	}
	conn_api.output_thread_running=0;
}

int pty_connect(struct bbslist *bbs)
{
	struct winsize ws;
	struct termios ts;
	struct text_info ti;
	char	*termcap;

	/* Init ti */
	ts.c_iflag = TTYDEF_IFLAG;
	ts.c_oflag = TTYDEF_OFLAG;
	ts.c_lflag = TTYDEF_LFLAG;
	ts.c_cflag = TTYDEF_CFLAG;
	memcpy(ts.c_cc,ttydefchars,sizeof(ts.c_cc));
	cfsetspeed(&ts, 115200);

	/* Horrible way to determine the screen size */
	textmode(screen_to_ciolib(bbs->screen_mode));

	gettextinfo(&ti);
	if(ti.screenwidth < 80)
		ws.ws_col=40;
	else {
		if(ti.screenwidth < 132)
			ws.ws_col=80;
		else
			ws.ws_col=132;
	}
	ws.ws_row=ti.screenheight;
	if(!bbs->nostatus)
		ws.ws_row--;
	if(ws.ws_row<24)
		ws.ws_row=24;

	child_pid = forkpty(&master, NULL, &ts, &ws);
	switch(child_pid) {
	case -1:
		load_font_files();
		textmode(ti.currmode);
		settitle("SyncTERM");
		return(-1);
	case 0:		/* Child */
		setenv("TERM",settings.TERM,1);
		termcap=xp_asprintf("syncterm|SyncTERM"
			":am:da:mi:ms:ND:ut"
			":co#%d:it#8:li#%d"
			"%s"
			":@7=\\E[K:AL=\\E[%%dL:DL=\\E[%%dM"
			":DO=\\E[%%dB:F1=\\E[23~:F2=\\E[24~:IC=\\E[%%d@"
			":ic=\\E[@"
			":LE=\\E[%%dD:RA=\\E[7l:RI=\\E[%%dC:SA=\\E[?7h:SF=\\E[%%dS"
			":SR=\\E[%%dT:UP=\\E[%%dA"
			":ac=-\\030.^Y0\\333`\\004a\\260f\\370g\\361h\\261i\\025j\\331k\\277l\\332m\\300n\\305q\\304t\\303u\\264v\\301w\\302x\\263y\\363z\\362~\\371"
			":al=\\E[L:bl=^G:bt=\\E[Z:cb=\\E[1K:cd=\\E[J:ce=\\E[K"
			":ch=\\E[%%i%%dG:cl=\\E[2J:cm=\\E[%%i%%d;%%dH:cr=^M:cs=\\E[%%i%%d;%%dr"
			":dc=\\E[P:dl=\\E[M:do=^J:ec=\\E[%%dX:ho=\\E[H"
			":k1=\\EOP:k2=\\EOQ:k3=\\EOR:k4=\\EOS:k5=\\EOt:k6=\\E[17~"
			":k7=\\E[18~:k8=\\E[19~:k9=\\E[20~:k;=\\E[21~:kD=\\177:kI=\\E[@"
			":kN=\\E[U:kP=\\E[V:kb=^H:kd=\\E[B:kh=\\E[H:kl=\\E[D:kr=\\E[C"
			":ku=\\E[A:le=\\E[D:ll=\\E[255H:mb=\\E[5m:md=\\E[1m:me=\\E[0m"
			":nd=\\E[C:nw=^M^J:DC=\\E[%%dP"
			":r1=\\E[?7h\\E[?25h\\E[?31l\\E[?32l\\E[?33l\\E[*r\\E[ D\\E[0m\\E[?s"
			":rc=\\E[u"
			":sc=\\E[s:sf=\\E[S:so=\\E[0;1;7m:se=\\E[m:sr=\\E[T:ta=^I:up=\\E[A"
			":ve=\\E[?25h:vi=\\E[?25l:",ws.ws_col,ws.ws_row
				,cio_api.options & CONIO_OPT_PALETTE_SETTING ? ":Co#256:pa#32762:AB=\\E[48;5;%dm:AF=\\E[38;5;%dm" : ":Co#8:pa#64:AB=\\E[4%dm:AF=\\E[3%dm");
		setenv("TERMCAP",termcap,1);
		xp_asprintf_free(termcap);
		termcap=xp_asprintf("%d",ws.ws_col);
		setenv("COLUMNS",termcap,1);
		xp_asprintf_free(termcap);
		termcap=xp_asprintf("%d",ws.ws_row);
		setenv("LINES",termcap,1);
		xp_asprintf_free(termcap);
		if(bbs->addr[0])
			execl("/bin/sh", "/bin/sh", "-c", bbs->addr, (char *)0);
		else
			execl(getenv("SHELL"), getenv("SHELL"), (char *)0);
		exit(1);
	}

	if(!create_conn_buf(&conn_inbuf, BUFFER_SIZE)) {
		load_font_files();
		textmode(ti.currmode);
		settitle("SyncTERM");
		return(-1);
	}
	if(!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		load_font_files();
		textmode(ti.currmode);
		settitle("SyncTERM");
		return(-1);
	}
	if(!(conn_api.rd_buf=(unsigned char *)malloc(BUFFER_SIZE))) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		load_font_files();
		textmode(ti.currmode);
		settitle("SyncTERM");
		return(-1);
	}
	conn_api.rd_buf_size=BUFFER_SIZE;
	if(!(conn_api.wr_buf=(unsigned char *)malloc(BUFFER_SIZE))) {
		free(conn_api.rd_buf);
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		load_font_files();
		textmode(ti.currmode);
		settitle("SyncTERM");
		return(-1);
	}
	conn_api.wr_buf_size=BUFFER_SIZE;

	_beginthread(pty_output_thread, 0, NULL);
	_beginthread(pty_input_thread, 0, NULL);

	return(0);
}

int pty_close(void)
{
	time_t	start;
	char garbage[1024];

	conn_api.terminate=1;
	start=time(NULL);
	kill(child_pid, SIGHUP);
	while(waitpid(child_pid, &status, WNOHANG)==0) {
		/* Wait for 10 seconds */
		if(time(NULL)-start >= 10)
			break;
		SLEEP(1);
	}
	kill(child_pid, SIGKILL);
	waitpid(child_pid, &status, 0);

	while(conn_api.input_thread_running || conn_api.output_thread_running) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	return(0);
}

#endif	/* __unix__ */
