#include <signal.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>
#define MAXDIR  PATH_MAX
#define MAXPATH PATH_MAX

// ToDo - Tune this better.
#define KERN_SKIP   8

// ToDo This is totally arbitrary.
#define MAX_STRING_LEN  1024

struct __attribute__((packed)) date {
	short da_year;
	short da_mon;
	short da_day;
} PACKED;

// ToDo needs some kind of REAL randomisation.
#define randomize   srandomdev
#define RANDOM      unix_random

#define strnicmp    strncasecmp
#define stricmp     strcasecmp

struct ffblk {
	char lfn_magic[6];        /* LFN: the magic "LFN32" signature */
	short lfn_handle;         /* LFN: the handle used by findfirst/findnext */
	unsigned short lfn_ctime; /* LFN: file creation time */
	unsigned short lfn_cdate; /* LFN: file creation date */
	unsigned short lfn_atime; /* LFN: file last access time (usually 0) */
	unsigned short lfn_adate; /* LFN: file last access date */
	char ff_reserved[5];      /* used to hold the state of the search */
	unsigned char ff_attrib;  /* actual attributes of the file found */
	unsigned short ff_ftime;  /* hours:5, minutes:6, (seconds/2):5 */
	unsigned short ff_fdate;  /* (year-1980):7, month:4, day:5 */
	off_t ff_fsize;           /* size of file */
	char ff_name[MAXPATH];    /* name of file as ASCIIZ string */
	DIR *dir_handle;
	char pathname[MAXPATH];
};
#define FA_RDONLY   1
#define FA_HIDDEN   2
#define FA_SYSTEM   4
#define FA_LABEL    8
#define FA_DIREC    16
#define FA_ARCH     32


void srandomdev();
off_t filelength(int file);
int strrev(char *str);
int strlwr(char *str);
int strupr(char *str);
int findfirst(char *pathname, struct ffblk *fblk, int attrib);
int findnext(struct ffblk *fblk);
int getdate(struct date *getme);
int strset(char *str, char ch);
int lock(int file, off_t offset, off_t length);
int unlock(int file, off_t offset, off_t length);
long unix_random(long maxnum);
int gettime(struct tm *getme);
#ifdef NEED_GETCH
char getch(void);
#endif

#define SH_DENYWR   1
#define SH_DENYRW   2
#define O_BINARY    0

FILE * _fsopen(char *pathname, char *mode, int flags);

void delay(unsigned msec);
