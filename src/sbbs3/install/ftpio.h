#ifndef _FTP_H_INCLUDE
#define _FTP_H_INCLUDE

#include <sys/types.h>
#include <sys/cdefs.h>
#include <stdio.h>
#include <time.h>

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
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
 * $FreeBSD: src/lib/libftpio/ftpio.h,v 1.17 2002/03/25 13:49:48 phk Exp $
 */


typedef struct {
	void	*_cookie;
	int		_file;
	void	*read;
	int (*readfn)(void *, char *, int);
	int (*writefn)(void *, const char *, int);
	fpos_t (*seekfn)(void *, fpos_t, int);
	int (*closefn)(void *);
	char* (*gets)(char *,int,void *);
} ftp_file_t;
#define ftp_FILE ftp_file_t

/* Internal housekeeping data structure for FTP sessions */
typedef struct {
    enum { init, isopen, quit } con_state;
    int		fd_ctrl;
    int		addrtype;
    char	*host;
    char	*file;
    int		error;
    int		is_binary;
    int		is_passive;
    int		is_verbose;
} *FTP_t;

/* Structure we use to match FTP error codes with readable strings */
struct ftperr {
  const int	num;
  const char	*string;
};

__BEGIN_DECLS
extern struct	ftperr ftpErrList[];
extern int	const ftpErrListLength;

/* Exported routines - deal only with ftp_FILE* type */
extern ftp_FILE	*ftpLogin(char *host, char *user, char *passwd,	int port, int verbose, int *retcode);
extern int	ftpChdir(ftp_FILE *fp, char *dir);
extern int	ftpErrno(ftp_FILE *fp);
extern off_t	ftpGetSize(ftp_FILE *fp, char *file);
extern ftp_FILE	*ftpGet(ftp_FILE *fp, char *file, off_t *seekto);
extern ftp_FILE	*ftpPut(ftp_FILE *fp, char *file);
extern int	ftpAscii(ftp_FILE *fp);
extern int	ftpBinary(ftp_FILE *fp);
extern int	ftpPassive(ftp_FILE *fp, int status);
extern void	ftpVerbose(ftp_FILE *fp, int status);
extern ftp_FILE	*ftpGetURL(char	*url, char *user, char *passwd,	int *retcode);
extern ftp_FILE	*ftpPutURL(char	*url, char *user, char *passwd,	int *retcode);
extern time_t	ftpGetModtime(ftp_FILE *fp, char *s);
extern const	char *ftpErrString(int error);
__END_DECLS

#endif	/* _FTP_H_INCLUDE */
