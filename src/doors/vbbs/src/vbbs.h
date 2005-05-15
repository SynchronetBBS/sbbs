#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "OpenDoor.h"

/* OpenDoors target platform. */
#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#define VBBS_WIN32
#elif defined(__GNUC__)
#define VBBS_LINUX
#else
#define VBBS_DOS
#endif /* !WIN32 */

#if defined(VBBS_DOS)
	#include <dos.h>
	#include <io.h>
	#include <share.h>
#endif
#if defined(VBBS_LINUX)
	#include <unistd.h>
	#include <termios.h>
	#include <limits.h>
	#define _MAX_PATH PATH_MAX
#endif

#include "OpenDoor.h"

#define VBBS_VERSION "v3.2 [0009]"

/* setup cross platform types */
typedef CHAR	s8;
typedef BYTE	u8;
typedef	INT16	s16;
typedef	WORD	u16;
typedef	INT32	s32;
typedef	DWORD	u32;

#define int8   s8
#define uint8  u8
#define int16  s16
#define uint16 u16
#define int32  s32
#define uint32 u32

/* random number functions */
void vbbs_randomize(void);
int rand_num(int num);

#define MAX_USERS                 255
#define FILE_ACCESS_MAX_WAIT      20
#define	OPEN			  		  0
#define USED			  		  1
#define	DEAD			  		  2
#define	DELETED			  			3
#define UNREAD			  			2
#define INUSE			  			1
#define BLANK			  			0
#define MYNET			  			0
#define ACT_NOTHING_HAPPENS       	0
#define ACT_ADD_FREE_USERS        	1
#define ACT_ADD_PAY_USERS         	2
#define ACT_SUB_FREE_USERS        	3
#define ACT_SUB_PAY_USERS         	4
#define ACT_ADD_FUNDS             	5
#define ACT_SUB_FUNDS             	6
#define ACT_UPGRADE_CPU           	7
#define ACT_ADD_ACTIONS           	8
#define ACT_FREE_2_PAY            	9
#define ACT_UPGRADE_HD           	10
#define ACT_DOWNGRADE_CPU        	11
#define ACT_DOWNGRADE_HD         	12
#define ACT_DOWNGRADE_MODEM      	13
#define ACT_PHONELINE_DAMAGE     	14
#define	HACK_BBS_SW		  			1
#define	HACK_USERBASE		  		2
#define	HACK_SECURITY		  		3
#define	HACK_VIRUS		  			4
#define	HACK_FAKECLUE		  		5
#define	HACK_BURGLERY		  		6
#define MAX_MODEM 		 			10
#define MAX_HDD						20

#pragma pack(1)

typedef struct
{
	s16 low, high, digital;
} modemspeed;

typedef struct
{
	s16 yr, mn, dy;
} dt;

typedef struct
{
	char 	realname[36];
	char	sysopname[36];
	char	bbsname[36];
	char	reserved_char[256];
	u16		reserved_int1;
	s16		plyrnum;
	u16		status;
	u16		expert;
	u16		reserved_int2;
	u16		reserved_int3;
	u16		callid;
	u16		lastbill;
	s16		cpu;
	s16		hd_size;
	u16		msgs_waiting;
	u16		skill_lev;
	u16		insurance;
	u16		rapsheet;
	u16		bbs_sw;
	u16		net_lev;
	u16		charge_lev;
	u16		hacktoday;
	u16		hackin[5];
	u16		breakincnt;
	u16		damagedby;
	s16		employee_actions;
	u16		employees;
	s16 	pacbell_ordered;
	s16		pacbell_installed;
	s16		pacbell_broke;
	s16		security;
	s16		reserved_int[12];
	float 	education[2];
	float	funds;
	float	reserved_fl[3];
	u32		skore_before;
	u32		reserved_long2;
	u32		skore_now;
	s32		actions;
	u32		users_p;
	u32		users_f;
	u32		reserved_long[1];
	modemspeed	mspeed;
	time_t 	laston;
} tUserRecord;

typedef struct
{
	s16 	towho, tosys, from, fromsys, hacktype;
	double	otherdata;
	char	hackmsg[4][81];
} hackrec;

typedef struct
{
	s32 	skore;
	s16	recno;
} tUserIdx;

typedef struct
{
   char  modemname[20];
   s32 cost;
   s16 req;
} modemtype;

typedef struct
{
	char  	cpuname[41];
	s32   	cost;
} cputype;

typedef struct
{
	char	hdsize[31];
	s32		cost;
	s16		req;
} hdtype;

typedef struct
{
    char	bbsname[31];
    s32  	cost;
    s16   	req;
    s32 	numlines;
} bbstype;

typedef struct
{
    s16 	classes_available,
			average_hours,
			apch,cost;
} classtype;

typedef struct
{
	s16  fsys;
	s16  tosys;
	s16  from;
	s16  to;
	s16  letternum;
	s16  item;
	s16  amount;
	s16  deleted;
	s16  idx;
	char   subject[81];
	time_t date;
} mailrec;

#pragma pack()

#ifdef ODPLAT_WIN32
void init(LPSTR lpszCmdLine, int nCmdShow);
#else
void init(int argc, char *argv[]);
#endif
void vbbs_main(void);
void BeforeExitFunction(void);
int ReadOrAddCurrentUser(void);
void WriteCurrentUser(void);
void finddate(s32 *tday);
void q2bbs(void);
char onekey(char *list);
void employ(void);
void actions(void);
void virus_det(void);
void bank(void);
void charge(void);
void readmail(void);
void prob_mail(void);
void topten(void);
void vbbsscan(void);
void status(void);
void users_online(void);
void UseActions(int numacts);
void advance(void);
void ans_chat(void);
void not_enough(void);
void bbslist(void);
void change_title(void);
void change_handle(void);
void toptenrecord(int i);
char *transform(s32 inum);
void viruslist(void);
int vlcount(void);
void personal(void);
void init_users_dat(void);
void inspect(void);
int	findBBS(void);
int GetUser(int record);
void sort_users(void);
long filesize(FILE *stream);
int rankplyr(int plyrno);
void get_time_diff(void);
void learn(void);
void use_skill(void);
void security_menu(void);
int check_first(void);

void VBBS3_Report(tUserRecord* pl, int bInspect);

