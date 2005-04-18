#include <stdlib.h>		       /* CPU-Computer Player */
#include <string.h>		       /* DAT-Smurf Combat Data */
#include <stdio.h>
#include <math.h>
#include "OpenDoor.h"		       /* BBS-System Player */

int             thisuserno, inuserno, noplayers, cyc, userhp, bbsexit;
char           *smurf_onl_player[] = {"GRANDPA SMURF", "PAPA SMURF", "REBEL SMURF",
    "SHADOW SMURF", "SWAP SMURF", "FIREBIRD SMURF",
"CONTRACT SMURF", "GOVERNMENT SMURF", "THIEF SMURF"};
int             smurf_onl_weapon[] = {20, 19, 1, 3, 0, 0, 0, 0, 0};
int             smurf_onl_armor[] = {20, 19, 0, 3, 0, 0, 0, 0, 0};
float           smurf_onl_gold[] = {100, 200, 300, 400, 0, 0, 0, 0, 0};
float           smurf_onl_bank[] = {1000000, 1000000, 10000, 1000, 0, 0, 0, 0, 0};
int             smurf_onl_level[] = {40, 35, 10, 2, 0, 0, 0, 0, 0};
int             smurf_onl_win[] = {30, 10, 5, 0, 0, 0, 0, 0, 0};
int             smurf_onl_lose[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int             smurf_onl_hp[] = {30, 10, 5, 0, 0, 0, 0, 0, 0};
float           smurf_onl_exp[] = {157450, 14640, 615, 60, 0, 0, 0, 0, 0};
int             smurf_onl_turns[] = {5, 5, 5, 5, 0, 0, 0, 0, 0};
int             smurf_onl_cpu[] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
int             smurf_onl_pimp[] = {10, 5, 0, 0, 0, 0, 0, 0, 0};	/* Pimping Level */
int             smurf_onl_pimp2[] = {10, 10, 0, 0, 0, 0, 0, 0, 0};	/* Pimping Quantity */
char           *smurf_smurfette[] = {"Femme Fatale", "Cassie", "None", "None", "", "", "", "", ""};
int             smurf_ettelevel[] = {11, 10, 1, 1, 0, 0, 0, 0, 0};
int             smurf_confineno[] = {11, 10, 0, 0, 0, 0, 0, 0, 0};
char           *smurf_confine[] = {"Ropes", "Wooden Cage", "Basement", "Infirmary", "Catacombs", "Jail House", "Prison", "Pyramid", "Dungeon", "Fortress", "Underworld"};
float           smurf_dat_cprices[] = {0, 1000, 10000, 50000, 100000, 500000, 1000000, 2000000, 5000000, 10000000, 100000000};
char           *smurf_dat_weapon[] = {"Bad Words", "Dandelion", "Sun Flower Seeds", "Fountain Pen", "Rubber Band", "Stick", "Auto H2O Gun", "H2O Uzi", "Kitchen Knife",
    "Metal PVC", "Baseball Bat", "Ice-T Album", "Satanic Rock CD", ".45 Auto", ".357 Magnum", "Shotgun", "Uzi", "Barrel Mach. Gun", "18", "Holy Water",
"Holy Cross"};
char           *smurf_dat_armor[] = {"Middle Finger", "Smurf Cap", "No Fear Clothesline", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
"Scapular/Inverted Star Pendant"};
float           smurf_dat_prices[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
float           smurf_dat_exp[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 1, 2, 3, 4, 5, 6, 7, 8, 9, 30, 1, 2, 3, 4, 5, 6, 7, 8, 9,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int             smurf_onl_hxst[] = {255, 255, 255, 255, 255, 255, 255, 255, 255};
int             smurf_onl_hxst1[] = {255, 255, 0, 255, 255, 255, 255, 255, 255};
int             smurf_onl_hxst2[] = {255, 255, 1, 255, 255, 255, 255, 255, 255};

int             smurf_onl_str[] = {255, 255, 255, 255, 255, 255, 255, 255, 255};	/* strength */
int             smurf_onl_dex[] = {255, 255, 255, 255, 255, 255, 255, 255, 255};	/* dexterity */
int             smurf_onl_int[] = {255, 255, 255, 255, 255, 255, 255, 255, 255};	/* intelligence */
int             smurf_onl_con[] = {255, 255, 255, 255, 255, 255, 255, 255, 255};	/* constitution */
int             smurf_onl_bra[] = {255, 255, 255, 255, 255, 255, 255, 255, 255};	/* bravery */
int             smurf_onl_chr[] = {255, 255, 255, 255, 255, 255, 255, 255, 255};	/* charisma */
#define SD_GREEN  0x0a
#define SD_CYAN   0x0b
#define SD_RED    0x0c
#define SD_VIOLET 0x0d
#define SD_YELLOW 0x0e
#define SD_WHITE  0x0f
void            itemlist(void);
void            displaystatus(void);
void            checkplayer(void);
void            titlescreen(void);
void            loadplayer(void);
void            checkplayer(void);
void            commandcycle(void);
void            displaymenu(void);
void            conflist(void);
void            userlist(void);
/* #define TUN thisuserno #define IUN inuserno #define BBSPLAYER
 * smurf_onl_player /* System Players */
#define BBSWEAPON smurf_onl_weapon     /* Sys Weapon */
#define BBSARMOR  smurf_onl_armor      /* Sys Armor */
#define BBSGOLD   smurf_onl_gold       /* Sys Gold */
#define BBSBANK   smurf_onl_bank       /* Sys Gold */
#define BBSLEVEL  smurf_onl_level      /* Sys Level */
#define BBSWIN    smurf_onl_win	       /* Sys Wins */
#define BBSLOSE   smurf_onl_lose       /* Sys Loss */
#define BBSHP     smurf_onl_hp	       /* Sys Hit Points */
#define BBSEXP    smurf_onl_exp	       /* Sys Experience */
#define BBSHOST   smurf_onl_hxst       /* Sys Hostages - Smurfettes */
#define BBSHOST1  smurf_onl_hxst1      /* Sys Hostages - Player Hostage1 */
#define BBSHOST2  smurf_onl_hxst2      /* Sys Hostages - Player Hostage2 */
#define BBSCONF   smurf_confineno      /* Sys Confine Number */
#define SMURFETTE smurf_smurfette      /* Sys Smurfette Name */
#define ETTELVL   smurf_ettelevel      /* Sys Smurfette Level */
#define CONFINE   smurf_confine	       /* Dat Confine Listing */
#define BBSSTR    smurf_onl_str	       /* Sys Strength */
#define BBSDEX    smurf_onl_dex	       /* Sys Dexterity */
#define BBSCON    smurf_onl_con	       /* Sys Constitution */
#define BBSINT    smurf_onl_int	       /* Sys Intelligence */
#define BBSBRA    smurf_onl_bra	       /* Sys Bravery */
#define BBSCHR    smurf_onl_chr	       /* Sys Charisma */

#define DATWEAPON smurf_dat_weapon     /* Smurf Combat Data Weapon Names */
#define DATARMOR  smurf_dat_armor      /* Smurf Combat Data Armor Names */
#define DATPRICES smurf_dat_prices     /* Smurf Combat Data Prices */
#define DATEXP    smurf_onl_exp	       /* Smurf Combat Data Experience */
*/
