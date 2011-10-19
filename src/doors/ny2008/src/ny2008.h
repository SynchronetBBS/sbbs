//                        0 - Critical Door error (no fossil, etc.)
//                        1 - Carrier lost, user off-line
//                        2 - Sysop terminated call, user off-line
//                        3 - User time used up, user STILL ON-LINE
//                        4 - Keyboard inactivity timeout, user off-line
//                       10 - User chose to return to BBS
//                       11 - User chose to logoff, user off-line
//                       12 - Critical RAVote error

#ifndef __unix__
#include <process.h>
#include <dos.h>
#endif
#include <math.h>
#if defined(__WATCOMC__)
#define date    dosdate_t
#define getdate(x)      _dos_getdate(x)
#else
#include <dirent.h>
#endif

#ifdef __TURBOC__
#include <dir.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

/* OpenDoors *MUST* be included first */
#ifndef __BEEN_HERE__

//I never did an OS2 port .. although i started on it
#ifndef __OS2__
#include <OpenDoor.h>                   // Must be included in all doors
#else
#include <OpenDoor.h>                   // Must be included in all doors
#endif

#endif
#define __BEEN_HERE__

#if !defined(__unix__) && !defined(_WIN32)
#include <share.h>
#include <ciolib.h>
#include <io.h>
#else
#include <dirwrap.h>
#include <genwrap.h>
#include <filewrap.h>
#include <ciolib.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>

#if defined(__TURBOC__) && !defined(_WIN32)
#include <locking.h>
#endif

#ifdef __WATCOMC__
#include <malloc.h>
#endif


#include <limits.h>

#include "filename.h"
#include "ibbsny.h"
#include "structs.h"
#include "const.h"

/*#define NO_MAIL_FLAG          0x0001
#define NY2008_FALG2          0x0002
#define NY2008_FALG3          0x0004
#define NY2008_FALG4          0x0008
#define NY2008_FALG5          0x0010
#define NY2008_FALG6          0x0020*/


char    entry_menu(void);               // Menu draw...
//char    central_park(void);
//char    drugs(void);
//char    money(void);
//char    evil_stuff(void);
//char    food(void);
//char    guns(void);
//char    get_laid(void);
//char    healing(void);
//char    mail(void);

char    callmenu(const char allowed[],menu_t menu,INT16 menu_line,char figst);
void DisplayBestIB(void);

void    read_IGMs(void);

DWORD   get_val(DWORD def,DWORD max);
void    EnterStreet(void);
void    ListPlayers(void);
void    ListPlayersA(void);
void    ListPlayersS(sex_type psex);
void    DisplayStats(void);
void    Maintanance(void);
void    drug_ops(void);
void    money_ops(void);
void    get_laid_ops(void);
void    food_ops(void);
void    guns_ops(void);
void    healing_ops(void);
void    newz_ops(void);
void    mail_ops(void);
void    wrt_sts(void);
void    exit_ops(void);
void    SortScrFile(INT16 usr, INT16 max);
void    ChangeOnlineRanks(void);
void    WhosOnline(void);
INT16     askifuser(char handle[25]);
char    ny_get_answer(const char *string);
void    fig_ker(void);
void    trim(char *numstr);

void    dobadwords(char *s);

void    print_drug(drug_type drug);
//void  print_drug(drug_type drug,INT16 len);

void    print_arm(weapon arm);

void    points_raise(DWORD raise);
INT16     CheckForHandle(char handle[25]);
void    Die(INT16 diecode); //die of reason 1=drugs 2=hunger 3=std
void    ny_kernel(void);
//char    rest(void);
void    rest_ops(void);
void    get_line(char beg[],char line[],char ovr[],INT16 wrap);
void    read_mail(void);
void    illness(void);
void    illness(desease ill, INT16 inf, INT16 rape=FALSE);
void    news_post(const char line[], const char name1[], const char name2[], INT16 flag);
void    print_disease(desease ill);
INT16     seereg(char bbsname[]);
void    get_bbsname(char bbsname[]);
//void  cap_names(char name[]);
void    change_info(void);
void    dump(void);
INT16     strzcmp(const char str1[], const char str2[]);
void    del(char *file);
//void  checknodes(void);

void WaitForEnter(void);
INT16 ReadOrAddCurrentUser(void);
void WriteCurrentUser(void);
INT32 randomf(INT32 max);
//FILE *ExculsiveFileOpen(char *pszFileName, char *pszMode);
FILE *ShareFileOpen(const char *pszFileName, const char *pszMode);
size_t ny_fread(void *ptr, size_t size, size_t n, FILE*stream);
size_t ny_fwrite(const void *ptr, size_t size, size_t n, FILE*stream);
void CustomConfigFunction(char *pszKeyword, char *pszOptions);
void strzcpy(char dest[],const char src[], INT16 beg,INT16 end);
double DrgPtsCoef(void);
void DisplayBest(void);
void AddBestPlayer(void);
void MakeFiles(void);
char *D_Num(INT16 num);
char *D_Num(INT32 num);
char *D_Num(DWORD num);
void heal_wounds(void);
void take_drug(void);
void money_plus(DWORD howmuch);
void money_minus(DWORD howmuch);
void disp_fig_stats(void);
void SortScrFileB(INT16 usr);
void points_loose(DWORD lost);
void CrashRecovery(void);
void IGM(char exenam[]);
void IGM_ops(void);
void CreateDropFile(INT16 all);
void ny_disp_emu(const char line[]);
void nyr_disp_emu(char line[]);
void ny_clr_scr();
//void print_gun_prices(void);
void ny_disp_emu(const char line[],INT16 min);
void ny_disp_emu_file(FILE *ans_phile,FILE *asc_phile,char line[],INT16 min);
char *ny_un_emu(char line[]);
char *ny_un_emu(char line[],char out[]);
void scr_save(void);
void scr_res(void);
void ch_game_d(void);
void ch_flag_d(void);
//void strim(char s[]);
void forrest_IGM(void);
void call_IGM(char exenamr[]);
void read_fight_IGMs(void);
FILE *ShareFileOpenAR(const char *pszFileName, const char *pszMode);
void UserStatus(void);
void ny_pers(unsigned char message);
void time_slice(void);
INT16 copyfile(const char *file1,const char *file2);
INT16 ny_remove(const char *pszFileName);
void game_events(void);
void ny_stat_line(INT16 line,INT16 before,INT16 after);
void ny_read_stat_line(INT16 line,char *string, FILE *phile);
void read_ibmail(void);
char *LocationOf(char *address);
INT16 ibbs_bbs_list(void);
void ibbs_bbs_name(INT16 bbs,INT16 sex,INT16 nochoice,char nameI[],INT16 *dbn,INT16 *pn);
//char ibbs_m(void);
void ibbs_ops(void);
void AddBestPlayerIB(void);
void AddBestPlayerInIB(char *name, DWORD points);
/*int findf(char *pathname, struct ffblk *ffblk, INT16 attrib);
INT16 fnext(struct ffblk *ffblk);
INT16 strwcmp(char *format,char *string);*/

/*typedef struct {
        INT16             first_enemy[LEVELS],
                        last_enemy[LEVELS];
        } enemy_idx;*/


/*typedef struct {
        char    sender[25];
        char    senderI[36];
        sex_type sender_sex;
        char    node_s[NODE_ADDRESS_CHARS + 1];
        char    node_r[NODE_ADDRESS_CHARS + 1];
        char    recver[25];
        char    recverI[36];
        INT16     quote_length;
        INT16     length;
        INT16     flirt;
        desease ill;
        INT16     inf;
        INT16     deleted;
        } ibbs_mail_idx_type;*/

