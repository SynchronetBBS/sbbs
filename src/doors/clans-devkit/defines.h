#ifndef THE_CLANS__DEFINES___H
#define THE_CLANS__DEFINES___H 1

#define VERSION                 "v0.96b1"

#define NTRUE			581 					// "True" used for reg info
#define NFALSE			0
// results of fighting
#define FT_RAN			0
#define FT_WON			1
#define FT_LOST 		2

#define MAX_ITEMSTAKEN	2		// allow this many items to be stolen in combat


#define MAX_SPELLS              40
#define NUM_ATTRIBUTES          6

#define TRUE                    1
#define FALSE                   0

#define MAIL_OTHER              0           // type of mailers
#define MAIL_BINKLEY            1

#define MAX_TOKEN_CHARS         32
#define MAX_MEMBERS             20
#define MAX_USERS               500
#define MAX_TOPUSERS            20

#define SPECIAL_CODE    '%'

#define STR_YESNO	" |0A(|0BYes|0C/no|0A) |0F"
#define STR_NOYES	" |0A(|0Cyes/|0BNo|0A) |0F"
#define YES 		1
#define NO			0

#define MAX_PCLASSES	15

// INI Specific data
#define MAX_NPCFILES	32
#define MAX_SPELLFILES	8
#define MAX_RACEFILES	8
#define MAX_CLASSFILES	8
#define MAX_VILLFILES	8
#define MAX_ITEMFILES	8
#define MAX_CLANCOMBAT	20

#define MAX_LEVELS      100
#define MAX_ITEMS_HELD	30

#define MAX_BUILDINGS 32

#define MAX_ITEMS		60
#define I_ITEM			0
#define I_WEAPON		1
#define I_ARMOR 		2
#define I_SHIELD		3
#define I_SCROLL		4
#define I_BOOK			5
#define I_OTHER 		6

// display types for GetStringChoice
#define DT_WIDE     0
#define DT_LONG 		1

// XOR Values
#define XOR_VILLAGE   (char)9
#define XOR_GAME      (char)9
#define XOR_USER      (char)9
#define XOR_PC        (char)9
#define XOR_MSG       (char)69
#define XOR_ITEMS     (char)23
#define XOR_ALLIES    (char)0xA3
#define XOR_TRADE     (char)0x2B
#define XOR_IBBS      (char)0x8A
#define XOR_PACKET    (char)0x47
#define XOR_IPS       (char)0x94
#define XOR_TRAVEL    (char)0x3B
#define XOR_ULIST     (char)0xCE
#define XOR_DISBAND   (char)0x79

#define ATTR_AGILITY      0
#define ATTR_DEXTERITY    1
#define ATTR_STRENGTH     2
#define ATTR_WISDOM       3
#define ATTR_ARMORSTR     4
#define ATTR_CHARISMA     5

#define SF_HEAL                 1
#define SF_DAMAGE               2
#define SF_MODIFY               4
#define SF_INCAPACITATE         8
#define SF_RAISEUNDEAD          16
#define SF_BANISHUNDEAD         32

#define MAX_ALLIES		5						// player may be in 5 alliances
#define MAX_ALLIANCES 16
#define MAX_ALLIANCEMEMBERS 20
#define MAX_ALLIANCEITEMS	30

/* interBBS stuff */
#define WS_STAYING              0           /* they're staying */
#define WS_LEAVING              1           /* they're leaving */
#define WS_GONE                 2           /* they're leaving */

// Village types
#define V_ALL         0
#define V_WOODLAND		1
#define V_COASTAL     2
#define V_WASTELAND 	3

// gov't system
#define GS_DEMOCRACY	0
#define GS_DICTATOR 	1

#define NUM_BUILDINGTYPES	10

#define B_BARRACKS		0
#define B_WALL			1
#define B_TOWER 		2
#define B_STEELMILL 	3
#define B_STABLES		4
#define B_AGENCY		5
#define B_SECURITY		6
#define B_GYM			7
#define B_DEVELOPERS	8
#define B_BUSINESS		9


#define MAX_QUESTS		64
#define MAX_OPTIONS 	16
#define MAX_QUOTES		64
#define MAX_TOPICS		10


#define NPCS_NOTHERE	0	// status of NPC, not here -- i.e. not in town
#define NPCS_HERE		1

#define MAX_CHATSPERDAY 4
#define MAX_MONSTERS	255 					// Number of monsters in .MON file


/* IBBS Stuff */
#define MAX_IBBSNODES   50


#define WN_NONE 		0						// not a wanderer
#define WN_CHURCH		1
#define WN_STREET		2
#define WN_MARKET		3
#define WN_TOWNHALL 5
#define WN_THALL		6
#define WN_MINE 		8

#define MAX_NPCS		16

// empire owner types
#define EO_VILLAGE		0
#define EO_ALLIANCE 	1
#define EO_CLAN 		2


#define MAX_PAWNLEVEL	5
#define MAX_WIZARDLEVEL 5
#define MAX_MARKETLEVEL 5
#define MAX_CHURCHLEVEL 5
#define MAX_THALLLEVEL	4

#define MAX_PACKETAGE   2       // number of days before a packet is old
#define MAX_BACKUPAGE   4       // number of days before a backup packet is old

#define MQ_AVERAGE      0
#define MQ_GOOD         1
#define MQ_VERYGOOD     2
#define MQ_EXCELLENT    3

// uncomment for Turbo C++
// typedef char BOOL;
#ifdef __unix__
# define BOOL char			// Stops OpenDoor.h from re-defining it.
# define FAR
# define _INT16	short
# define _STRCASECMP strcasecmp
#elif defined(_MSC_VER)
# define _INT16 short int
# ifndef FAR
#  define FAR
# endif
# define _STRCASECMP stricmp
#else
# define FAR far
# define _INT16 int
# define _STRCASECMP stricmp
#endif

typedef char __BOOL;

#ifdef __GNUC__
# define PACKED __attribute__ ((packed))
#elif defined(_MSC_VER)
# pragma warning(disable:4103)
# pragma pack(1)
#endif

#ifndef PACKED
# define PACKED
#endif

#ifdef _MSC_VER
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# define delay(x) Sleep(x)
# define sleep(x) Sleep((x * 1000))
# define RANDOM(x) ((x) == 0 ? 0 : (rand() % (x)))
#endif

enum Status { Dead, Unconscious, RanAway, Here };
enum PlayerStats { stAgility, stDexterity, stStrength, stWisdom, stArmorStr };
enum action { acAttack, acRun, acCast, acRead, acSkip, acNone };
typedef _INT16 action;

_INT16	_argc;
char	**_argv;

#define MAX_FILENAME_LEN        13

#endif /* THE_CLANS__DEFINES___H */
