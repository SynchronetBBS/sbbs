/* SBBS.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#ifndef _SBBS_H
#define _SBBS_H

/****************************/
/* Standard library headers */
/****************************/

#include <io.h>
#ifdef __TURBOC__
	#include <dir.h>
	#include <alloc.h>
#else
	#include <malloc.h>
#endif

#include <dos.h>
#include <mem.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <conio.h>
#include <ctype.h>
#include <fcntl.h>
#include <share.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <sys\stat.h>

#ifdef __OS2__
	#define INCL_BASE
    #include <os2.h>
#else
	#include <bios.h>
#endif

#define GLOBAL extern	/* turn vars.c and scfgvars.c files into headers */
#ifdef SBBS
#include "vars.c"
#endif
#include "smblib.h"
#include "ars_defs.h"
#include "scfgvars.c"
#include "scfglib.h"
#include "riolib.h"
#include "riodefs.h"

/***********************************************/
/* Prototypes of functions used ONLY with SBBS */
/***********************************************/


void lputc(int);
int  lclatr(int);
uint lkbrd(int);
int  lclaes(void);
void lclxy(int,int);
int  lclwx(void);
int  lclwy(void);
long lputs(char FAR16 *);

#ifndef __FLAT__

/* LCLOLL.ASM */
int  lclini(int);

#endif

/* COMIO.C */
void putcom(char *str);			/* Send string to com port */
void putcomch(char ch);			/* Send single char to com port */
void mdmcmd(char *str); 		/* Send translated modem command to COM */
char getmdmstr(char *str, int sec);  /* Returns chars  */
void hangup(void);				/* Hangup modem							*/
void offhook(void);				/* Takes phone offhook					*/
void checkline(void);			/* Checks if remote user has hungup		*/
void comini(void);				/* initializes com port and sets baud   */
void setrate(void); 			/* sets baud rate for current connect	*/

/* DATIO.C */
void getuserdat(user_t *user); 	/* Fill userdat struct with user data   */
void putuserdat(user_t user);	/* Put userdat struct into user file	*/
void getuserrec(int usernumber, int start, char length, char *str);
void putuserrec(int usernumber, int start, char length, char *str);
ulong adjustuserrec(int usernumber, int start, char length, long adj);
void subtract_cdt(long amt);
void getrec(char *instr,int start,int length,char *outstr);
								/* Retrieve a record from a string		*/
void putrec(char *outstr,int start,int length,char *instr);
								/* Place a record into a string			*/
uint matchuser(char *str);		/* Checks for a username match		*/
uint finduser(char *str);
uint lastuser(void);
char getage(char *birthdate);

int 	sub_op(uint subnum);
ulong	getlastmsg(uint subnum, ulong *ptr, time_t *t);
time_t	getmsgtime(uint subnum, ulong ptr);
ulong	getmsgnum(uint subnum, time_t t);
ulong getposts(uint subnum);

int  getfiles(uint dirnum);
int  dir_op(uint dirnum);
int  getuserxfers(int fromuser, int destuser, char *fname);
void rmuserxfers(int fromuser, int destuser, char *fname);
char *username(int usernumber, char *str);
uint gettotalfiles(uint dirnum);
void getnodeext(uint number, char *str);
void putnodeext(uint number, char *str);
void getnodedat(uint number, node_t *node, char lock);
void putnodedat(uint number, node_t node);
void putusername(int number, char *name);
void nodesync(void);
void getmsgptrs(void);
void putmsgptrs(void);
void getusrsubs(void);
void getusrdirs(void);
uint userdatdupe(uint usernumber, uint offset, uint datlen, char *dat
	, char del);
void gettimeleft(void);

/* FILIO.C */
char addfiledat(file_t *f);
void getfileixb(file_t *f);
void getfiledat(file_t *f);
void putfiledat(file_t f);
char uploadfile(file_t *f);
void downloadfile(file_t f);
void removefiledat(file_t f);
void fileinfo(file_t f);
void openfile(file_t f);
void closefile(file_t f);
void update_uldate(file_t f);
void putextdesc(uint dirnum, ulong datoffset, char *ext);
void getextdesc(uint dirnum, ulong datoffset, char *ext);
void viewfilecontents(file_t f);

/* STR.C */
void userlist(char subonly);
char gettmplt(char *outstr, char *template, int mode);
void sif(char *fname, char *answers, long len);	/* Synchronet Inteface File */
void sof(char *fname, char *answers, long len);
void create_sif_dat(char *siffile, char *datfile);
void read_sif_dat(char *siffile, char *datfile);
void printnodedat(uint number, node_t node);
void reports(void);
char inputnstime(time_t *dt);
char chkpass(char *pass, user_t user);
char *cmdstr(char *instr, char *fpath, char *fspec, char *outstr);
void getcomputer(char *computer);
void subinfo(uint subnum);
void dirinfo(uint dirnum);
char trashcan(char *insearch, char *name);
#ifdef SBBS
char *decrypt(ulong [],char *);
#endif

/* MSGIO.C */
void automsg(void);
char writemsg(char *str, char *top, char *title, int mode, int subnum
	,char *dest);
char putmsg(char HUGE16 *str, int mode);
char msgabort(void);
void email(int usernumber, char *top, char *title, char mode);
void forwardmail(smbmsg_t *msg, ushort usernum);
char postmsg(uint subnum, smbmsg_t *msg, int wm_mode);
void removeline(char *str, char *str2, char num, char skip);
ulong msgeditor(char *buf, char *top, char *title);
void editfile(char *path);
void getsmsg(int usernumber);
void putsmsg(int usernumber, char *strin);
void getnmsg(void);
void putnmsg(int num, char *strin);
int loadmsg(smbmsg_t *msg, ulong number);
char HUGE16 *loadmsgtxt(smbmsg_t msg, int tails);
ushort chmsgattr(ushort attr);
void show_msgattr(ushort attr);
void show_msghdr(smbmsg_t msg);
void show_msg(smbmsg_t msg, int mode);
void msgtotxt(smbmsg_t msg, char *str, int header, int tails);
void quotemsg(smbmsg_t msg, int tails);
void putmsg_fp(FILE *fp, long length, int mode);
void editmsg(smbmsg_t *msg, uint subnum);
void remove_re(char *str);
void remove_ctrl_a(char *instr);

/* MAIL.C */

ulong loadmail(mail_t **mail, uint usernumber, int which, int mode);
int getmail(int usernumber, char sent);
int delmail(uint usernumber,int which);
void delfattach(uint to, char *title);
void telluser(smbmsg_t msg);
void delallmail(uint usernumber);
void readmail(uint usernumber, int sent);

void bulkmail(uchar *ar);

/* CONIO.C */
int  bputs(char *str);				/* BBS puts function */
int  rputs(char *str);				/* BBS raw puts function */
int  lprintf(char *fmt, ...);		/* local printf */
int  bprintf(char *fmt, ...);		/* BBS printf function */
int  rprintf(char *fmt, ...);		/* BBS raw printf function */
int  getstr(char *str,int length, long mode);
void outchar(char ch);			/* Output a char - check echo and emu.  */
char r0dentch(char ch);			/* Converts CH to r0dent CH				*/
char getkey(long mode); 		 /* Waits for a key hit local or remote  */
void ungetkey(char ch);			/* Places 'ch' into the input buffer    */
char inkey(int mode);				/* Returns key if one has been hit		*/
char yesno(char *str);
char noyes(char *str);
void printfile(char *str, int mode);
void printtail(char *str, int lines, int mode);
void menu(char *code);
long getkeys(char *str, ulong max);
long getnum(ulong max);
void center(char *str);
int  uselect(int add, int n, char *title, char *item, char *ar);
void statusline(void);
void pause(void);
void waitforsysop(char on);
void riosync(char abortable);
char validattr(char a);
char stripattr(char *str);
void redrwstr(char *strin, int i, int l, char mode);
void mnemonics(char *str);
void attr(int atr);				/* Change local and remote text attributes */
void ctrl_a(char x);			/* Peforms the Ctrl-Ax attribute changes */
int  atcodes(char *code);
char *ansi(char atr);			/* Returns ansi escape sequence for atr */
int  whos_online(char listself);/* Lists active nodes, returns active nodes */

/* LOGONOFF.C */
int login(char *str, char *pw);
char logon(void);
void logout(void);
void logoff(void);
void newuser(void);					/* Get new user							*/
void backout(void);

/* BBSIO.C */
char scanposts(uint subnum, char newscan, char *find);	/* Scan sub-board */
int  searchsub(uint subnum, char *search);	/* Search for string on sub */
int  searchsub_toyou(uint subnum);
char text_sec(void);						/* Text sections */

/* CHAT.C */
void localchat(void);
void chatsection(void);
void nodepage(void);
void nodemsg(void);
void guruchat(char *line, char *guru, int gurunum);
char guruexp(char **ptrptr, char *line);
void localguru(char *guru, int gurunum);
void packchatpass(char *pass, node_t *node);
char *unpackchatpass(char *pass, node_t node);
void sysop_page(void);
void privchat(void);

/* MAIN.C */
void printstatslog(uint node);
ulong logonstats(void);
void logoffstats(void);
void getstats(char node, stats_t *stats);

/* MISC.C */
void bail(int code);			/* Exit 								*/
char *ultoac(ulong l,char *str);
int  bstrlen(char *str);
int  nopen(char *str, int access);
FILE *fnopen(int *file, char *str,int access);
char *sectostr(uint sec, char *str);		/* seconds to HH:MM:SS */
void truncsp(char *str);		/* Truncates white spaces off end of str */
ulong ahtoul(char *str);	/* Converts ASCII hex to ulong */
int  pstrcmp(char **str1, char **str2);  /* Compares pointers to pointers */
int  strsame(char *str1, char *str2);	/* Compares number of same chars */
void errormsg(int line, char *file, char action, char *object, ulong access);
int  mv(char *src, char *dest, char copy); /* fast file move/copy function */
char chksyspass(int local);
char *timestr(time_t *intime);  /* ASCII representation of time_t */
char *zonestr(short zone);
void dv_pause(void);
void secwait(int sec);
time_t ftimetounix(struct ftime f);
struct ftime unixtoftime(time_t unix);
time_t dstrtounix(char *str);	/* ASCII date (MM/DD/YY) to unix conversion */
char *unixtodstr(time_t unix, char *str); /* Unix time to ASCII date */
char *hexplus(uint num, char *str); 	/* Hex plus for 3 digits up to 9000 */
uint hptoi(char *str);
int chk_ar(char *str, user_t user); /* checks access requirements */
ushort crc16(char *str);
ulong  _crc32(char *str);
void ucrc16(uchar ch, ushort *rcrc);
int kbd_state(void);

#ifndef __FLAT__
long lread(int file, char HUGE16 *buf, long bytes);   /* Reads >32k files */
long lfread(char huge *buf, long bytes, FILE *fp);
long lwrite(int file, char HUGE16 *buf, long bytes);   /* Writes >32k files */
#endif

/* XFER.C */
int listfile(char *fname, char HUGE16 *buf, uint dirnum
	, char *search, char letter, ulong datoffset);
int  listfiles(uint dirnum, char *filespec, int tofile, char mode);
int  listfileinfo(uint dirnum, char *filespec, char mode);
void batchmenu(void);
void temp_xfer(void);
void batch_add_list(char *list);
void notdownloaded(ulong size, time_t start, time_t end);
int viewfile(file_t f, int ext);

/* XFERMISC.C */
void seqwait(uint devnum);
void listfiletofile(char *fname, char HUGE16 *buf, uint dirnum, int file);
void upload(uint dirnum);
char bulkupload(uint dirnum);
void extract(uint dirnum);
char *getfilespec(char *str);
int  delfiles(char *path, char *spec);
char findfile(uint dirnum, char *filename);
char fexist(char *filespec);
long flength(char *filespec);
long fdate(char *filespec);
long fdate_dir(char *filespec);
char *padfname(char *filename, char *str);
char *unpadfname(char *filename, char *str);
char filematch(char *filename, char *filespec);
int  protocol(char *cmdline, int cd);
void autohangup(void);
char checkprotlog(file_t f);
char checkfname(char *fname);
void viewfiles(uint dirnum, char *fspec);
char addtobatdl(file_t f);
int  fnamecmp_a(char **str1, char **str2);	 /* for use with resort() */
int  fnamecmp_d(char **str1, char **str2);
int  fdatecmp_a(uchar **buf1, uchar **buf2);
int  fdatecmp_d(uchar **buf1, uchar **buf2);
void resort(uint dirnum);
int  create_batchup_lst(void);
int  create_batchdn_lst(void);
int  create_bimodem_pth(void);
void batch_upload(void);
void batch_download(int xfrprot);
void start_batch_download(void);
ulong create_filelist(char *name, char mode);

/* XTRN.C */
int  external(char *cmdline,char mode); /* Runs external program */
void xtrndat(char *name, char *dropdir, uchar type, ulong tleft);
char xtrn_sec(void);					/* The external program section  */
void exec_xtrn(uint xtrnnum);			/* Executes online external program */
void user_event(char event);			/* Executes user event(s) */
char xtrn_access(uint xnum);			/* Does useron have access to xtrn? */
char *temp_cmd(void);					/* Returns temp file command line */

/* LOGIO.C */
void logentry(char *code,char *entry);
void log(char *str);				/* Writes 'str' to node log */
void logch(char ch, char comma);	/* Writes 'ch' to node log */
void logline(char *code,char *str); /* Writes 'str' on it's own line in log */
void logofflist(void);              /* List of users logon activity */
void errorlog(char *text);			/* Logs errors to ERROR.LOG and NODE.LOG */
#if DEBUG
void dlog(int line, char *file, char *text);	/* Debug log file */
#endif

/* QWK.C */
void	qwk_sec(void);
char	pack_qwk(char *packet, ulong *msgcnt, int prepack);
void	unpack_qwk(char *packet,uint hubnum);


/* FIDO.C */
void	netmail(char *into, char *subj, char mode);
void	qwktonetmail(FILE *rep, char *block, char *into, uchar fromhub);

void inetmail(char *into, char *subj, char mode);
void qnetmail(char *into, char *subj, char mode);

/* USEREDIT.C */
void useredit(int usernumber, int local);
void maindflts(user_t user);
void purgeuser(int usernumber);

/* INITDATA.C */
void initdata(void);

/* VER.C */
void ver(void);

/* MAIN_SEC.C */
void main_sec(void);
void scanallsubs(char mode);
void scansubs(char mode);
void new_scan_cfg(ulong misc);
void new_scan_ptr_cfg(void);

/* XFER_SEC.C */
void xfer_sec(void);
void scanalldirs(char mode);
void scandirs(char mode);

/* MSWAIT.OBJ */

void mswait(int msecs);

#ifdef __OS2__
#define beep(f,d) DosBeep(f,d)
#else
void beep(int freq, int dur);
#endif

void backslash(char *str);
void backslashcolon(char *str);
void getlines(void);
void strip_ctrl(char *str);
void strip_exascii(char *str);

char wfc_events(time_t lastnodechk);
void catsyslog(int crash);
char waitforcall(void);
void quicklogonstuff(void);
char terminal(void);
void startup(int argc, char *argv[]);

void crack_attempt(void);

/*********************/
/* OS specific stuff */
/*********************/

#ifdef __FLAT__

#define lread(f,b,l) read(f,b,l)
#define lfread(b,l,f) fread(b,l,f)
#define lwrite(f,b,l) write(f,b,l)
#define lfwrite(b,l,f) fwrite(b,l,f)

#endif

#if (__BORLANDC__ > 0x0410)
#define _chmod(p,f,a) _rtl_chmod(p,f,a) 	// _chmod obsolete in 4.x
#endif

#if defined(__OS2__)
	#define lclini(x)	window(1,1,80,x)
	#define lclaes()	conaes()
    #define nosound() ;
#elif defined(__WIN32__)
	#define lclini(x)	window(1,1,80,x)
	#define lclxy(x,y)	gotoxy(x,y)
	#define lclwy() 	wherey()
	#define lclwx() 	wherex()
	#define lputs		cputs
    #define nosound() ;
#elif defined(__FLAT__)
    #define mswait(x) delay(x)
#endif

#ifndef __OS2__
	#define outcon(ch) lputc(ch)
#endif


#endif /* Don't add anything after this #endif statement */
