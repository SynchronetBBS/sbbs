//                        0 - Critical Door error (no fossil, etc.)
//                        1 - Carrier lost, user off-line
//                        2 - Sysop terminated call, user off-line
//                        3 - User time used up, user STILL ON-LINE
//                        4 - Keyboard inactivity timeout, user off-line
//                       10 - User chose to return to BBS
//                       11 - User chose to logoff, user off-line
//                       12 - Critical RAVote error


//#include <process.h>
#include <math.h>
#ifndef __unix__
#include <dir.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
//#include <ctype.h>
//#include <share.h>
//#include <locking.h>
//#include <fcntl.h>
//#include <sys\stat.h>

#include <OpenDoor.h>                   // Must be included in all doors
#include <dirwrap.h>
#include <ciolib.h>
#include <filewrap.h>
#include <genwrap.h>

#include "filename.h"
#include "const.h"
#include "structs.h"

char   	entry_menu(void);               // Menu draw...
void	DisplayStats(void);
void	print_drug(drug_type drug);
void	print_arm(weapon arm);
void    ny_kernel(void);
void	print_disease(desease ill);

void WaitForEnter(void);
void WriteCurrentUser(INT16 user_num);
//FILE *ExculsiveFileOpen(char *pszFileName, char *pszMode);
void CustomConfigFunction(char *pszKeyword, char *pszOptions);
void strzcpy(char dest[],const char src[], INT16 beg,INT16 end);
void	cap_names(char name[]);
void	dump(void);
void    ny_disp_emu(const char line[]);
void	get_bbsname(char bbsname[]);
INT16	seereg(char bbsname[]);
FILE *ShareFileOpen(const char *pszFileName, const char *pszMode);
size_t ny_fread(void *ptr, size_t size, size_t n, FILE*stream);
size_t ny_fwrite(const void *ptr, size_t size, size_t n, FILE*stream);
//void ChangeOnlineRanks(void);
void SortScrFile(INT16 usr);
void SortScrFileB(INT16 usr);
void ch_game_d(void);
void ch_flag_d(void);
void set_points(DWORD raise);

#include "structs.h"
