#ifndef _STRUCTS_H_
#define _STRUCTS_H_

#include "OpenDoor.h"
#include "const.h"

#ifndef tBool
typedef INT16 tBool;
#endif

typedef unsigned char tIBResult;
enum
{
    eSuccess,
    eNoMoreMessages,
    eGeneralFailure,
    eBadParameter,
    eNoMemory,
    eMissingDir,
    eFileOpenError
};

typedef unsigned char weapon;
enum {HANDS,
              PEPPER,
              KNIFE,
              CHAIN,
              GUN,
              RIFLE,
              LASER_GUN,
              SHOTGUN,
              MACHINEGUN,
              GRANADE_LAUNCHER,
              BLASTER,
              A_BOMB,
              SHARP_STICK,
              SCREWDRIVER,
              HAMMER,
              LEAD_PIPE,
              COLT,
              ELEPHANT_GUN,
              NAILGUN,
              ASSAULT_RIFLE,
              PROTON_GUN,
              NEUTRON_PHASER,
              ULTRASOUND_GUN};
typedef unsigned char drug_type;
enum {POT,HASH,LSD,COKE,PCP,HEROIN};
typedef unsigned char sex_type;
enum {MALE,FEMALE};
typedef unsigned char guy_type;
enum {HEADBANGER,HIPPIE,BIG_FAT_DUDE,CRACK_ADDICT,PUNK};
typedef unsigned char desease;
enum {NONE,CRAPS,HERPES,SYPHILIS,AIDS};
typedef unsigned char guy_status;
 enum {ALIVE,UNCONCIOUS,DEAD};
typedef unsigned char hotel_type;
 enum {NOWHERE,MOTEL,REG_HOTEL,EXP_HOTEL};
typedef unsigned char menu_t;
 enum {CENTRAL_PARK,CENTRAL_PARK_IB,EVIL_STUFF,BANK,HEALING,FOOD,
              DRUGS,ARMS,SEX,MAIL,REST,P_FIG,C_FIG,S_FIG,ENTRY_1,ENTRY_2,
              ONLINE,NEWZ,LIST,CONSIOUS,ATTACK,WIN,MAINT_RUN,WEAPONS,COPS,
              NEW,NATION,OTHER,NEW_NAME,NEW_WIN,NEW_LOOSE,TEN_BEST,BUSTED,
              ASS_KICKED,ASS_KICKED_P,ASS_KICKED_O,COLORS_HELP,CH_DRUG,
              LIST_IB_SYS,IBBS_MENU,HITMEN,TEN_BEST_IBBS,END};

#if defined(_WIN32) || defined(__BORLANDC__)
        #define PRAGMA_PACK
#endif

#if defined(PRAGMA_PACK)
        #define _PACK
#else
        #define _PACK __attribute__ ((packed))
#endif

#if defined(PRAGMA_PACK)
        #pragma pack(push,1)    /* Disk image structures must be packed */
#endif

typedef struct _PACK {
	char szAddress[NODE_ADDRESS_CHARS + 1];
	char szSystemName[SYSTEM_NAME_CHARS + 1];
	char szLocation[LOCATION_CHARS + 1];
}
tOtherNode;

typedef struct _PACK {
	char szThisNodeAddress[NODE_ADDRESS_CHARS + 1];
	char szProgName[PROG_NAME_CHARS + 1];
	char szNetmailDir[PATH_CHARS + 1];
	tBool bCrash;
	tBool bHold;
	tBool bEraseOnSend;
	tBool bEraseOnReceive;
	INT16 nTotalSystems;
	tOtherNode *paOtherSystem;
}
tIBInfo;

typedef struct _PACK {
	char szFromUserName[36];
	char szToUserName[36];
	char szSubject[72];
	char szDateTime[20]; 		/* "DD Mon YY  HH:MM:SS" */
	WORD wTimesRead;
	WORD wDestNode;
	WORD wOrigNode;
	WORD wCost;					/* Lowest unit of originator's currency */
	WORD wOrigNet;
	WORD wDestNet;
	WORD wDestZone;
	WORD wOrigZone;
	WORD wDestPoint;
	WORD wOrigPoint;
	WORD wReplyTo;
	WORD wAttribute;
	WORD wNextReply;
}
tMessageHeader;

typedef struct _PACK {
	WORD wZone;
	WORD wNet;
	WORD wNode;
	WORD wPoint;
}
tFidoNode;

typedef struct _PACK {
	char    sender[25];
	char    senderI[36];
	sex_type sender_sex;
	char    n1;
	char    recver[25];
	char    recverI[36];
	INT32   location;
	INT16   afterquote;
	INT16   length;
	INT16   flirt;
	desease ill;
	char    n2;
	INT16   inf;
	INT16   deleted;
}
mail_idx_type;

//defined in interbbs.h changed enum to char (as ibbs_mail_type_1)
typedef struct _PACK {
	char    sender[25];
	char    senderI[36];
	sex_type sender_sex;
	char    node_s[NODE_ADDRESS_CHARS + 1];
	char    node_r[NODE_ADDRESS_CHARS + 1];
	char    recver[25];
	char    recverI[36];
	char    quote_length;
	char    length;
	INT16   flirt;
	desease ill;
	INT16   inf;
	char    deleted;
	char	lines[20][81];
	//	char	quote_lines[10][81];
}
ibbs_mail_type;


typedef struct _PACK {
	char            name[25];
	guy_type        nation;
	char            n1;
	INT16           level;
	DWORD           points;
	guy_status      alive;
	char            n2;
	sex_type        sex;
	char            n3;
	INT16           user_num,
	online;
}
scr_rec;

typedef struct _PACK {
	char            name[25];
	char            nameI[36];
	guy_type        nation;
	INT16           level;
	DWORD           points;
	sex_type        sex;
}
ibbs_scr_rec;

typedef struct _PACK {
	char            name[25];
	char            nameI[36];
	guy_type        nation;
	INT16           level;
	DWORD           points;
	sex_type        sex;
	char    	node[NODE_ADDRESS_CHARS + 1];
}
ibbs_scr_spy_rec;




typedef struct _PACK {
	char    	node[NODE_ADDRESS_CHARS + 1];
	DWORD           hi_points;
}
ibbs_bbs_rec;

typedef struct _PACK {
	char    	node[NODE_ADDRESS_CHARS + 1];
	DWORD           hi_points;
	INT16		players;
	INT16		player_list; //number of file with player list
}
ibbs_bbs_spy_rec;


typedef struct _PACK {
	INT16		action;
	char            name_s[25];
	char            name_sI[36];
	char            name_r[25];
	char            name_rI[36];
	char		node_s[NODE_ADDRESS_CHARS + 1];
	//	DWORD   money;
	char		data[10];
}
ibbs_act_rec;

/*actions:
  0=name change name_sI change to name_s
  1=spy on name_rI
*/


typedef struct _PACK {
	char            name[25];
	DWORD           points;
}
best_rec_type;

typedef struct _PACK {
	char            name[25];
	DWORD           points;
	char 		location[NODE_ADDRESS_CHARS + 1];
}
ibbs_best_rec_type;


typedef struct _PACK {
	char            name[36];
	INT32           hitpoints,
	strength,
	defense;
	weapon          arm;
	char            n1;
}
enemy;

typedef struct _PACK {
	char            tagline[80], name[25], name2[36];
	INT16             flag;
}
newzfile_t;
// flag settings//
// 0 - system news
// 1 - user announcment (name)
// 2 - user BUSTED (name) for (tagline)
// 3 - user newz (name)
// 4 - user kicked ass (name kicked name2's ass)
// 5 - user had his ass kicked (name was beat by name2)
// 6 - user announcment (name from name2(cut down to 35 chars))

//Player User File structure (beta6+)
typedef struct _PACK {
	//character fields
	char            bbsname[36],     //the BBS name of the user
	name[25],        //the name of the character
	say_win[41],     //what the user says when he wins
	say_loose[41];   // "    "    "   "    "   "  looses
	//integer records
	INT16           rank,            //user rank
	days_not_on,     //days the user gas been inactive
	strength,        //attacking strenght of the user
	defense,         //defensive strenght
	condoms,         //condoms user has
	since_got_laid,  //days since the user last got laid
	drug_hits,       //the hist that the user has
	drug_days_since; //if addicted how long the user
	//has not used the drug

	//long type records
	INT32           hitpoints,       //users hitpoints
	maxhitpoints;    //maximum of the users hitpoints

	//DWORD type record
	DWORD           points,          //users points
	money,           //money in hand
	bank;            //money in bank

	//unsigned char type records used as values
	unsigned char   level,           //user level
	turns,           //fight the user has left today
	hunger,          // % of hunger
	sex_today,       //sex turns left today
	std_percent,     // % of current std
	drug_addiction,  // % of drug addiction
	drug_high,       // % of how "high" the player is
	hotel_paid_fer,  //for how many more days the hotel
	//is paid for
	days_in_hospital;//how many days has the use been
	//in hospital

	/*enumerated types stored as char!!! (not int)*/
	guy_status      alive;           //user: alive, unconsious, or dead
	sex_type        sex;             //user sex: Male, Female
	guy_type        nation;          //what is he:
	//punk, headbanger ...
	weapon          arm;             //players weapon
	desease         std;             //current player std
	drug_type       drug;            //current player drug type
	hotel_type      rest_where;      //where the user is staying lately

	/*added values BETA 9*/
	char            unhq;            //# of unhq bombings allowed per day
	//default=1
	char            poison;          //number of poisoning of watr allowed
	//per day, default=1
	/*added values v0.10*/
	unsigned char   rocks,           //Rocks, usable in fights
	throwing_ability,//Throwing ability 0-100
	punch_ability,   //Punching ability 0-100
	kick_ability;    //Kicking ability 0-100
	char            InterBBSMoves;

	/*reserved for future use 3 bytes reset to 0*/
	char            res1;
	INT16		res2;
}
user_rec;

/*
typedef struct _PACK {
	char            bbsname[36],
			name[25];
	guy_type        nation;
	INT16           level,
			rank;
	DWORD           points,
			money,
			bank;
	guy_status      alive;
	INT16           days_not_on;
	sex_type        sex;
	char            say_win[41],
			say_loose[41];
	INT16           turns,
			hunger,
			strength,
			defense;
	INT32           hitpoints,
			maxhitpoints;
	weapon          arm;
	INT16           condoms;
	INT16           since_got_laid;
	INT16           sex_today;
	desease         std;
	INT16           std_percent;
	drug_type       drug;
	INT16           drug_hits,
			drug_addiction,
			drug_high,
			drug_days_since;
	hotel_type      rest_where;
	unsigned char   hotel_paid_fer,
			days_in_hospital;
	INT16           res1,
			res2,
			res3,
			res4,
			res5;
	} user_rec;*/

typedef struct _PACK {
	INT16		first_enemy[LEVELS],
	last_enemy[LEVELS];
}
enemy_idx;

#if 0
/* From nyedit.h */





//Player User File structure (beta6+)
typedef struct _PACK {
	//character files
	char		bbsname[36],     //the BBS name of the user
	name[25],        //the name of the character
	say_win[41],     //what the user says when he wins
	say_loose[41];   // "    "    "   "    "   "  looses
	//integer records
	INT16		rank,	         //user rank
	days_not_on,     //days the user has been inactive
	strength,        //attacking strenght of the user
	defense,         //defensive strenght
	condoms,         //condoms user has
	since_got_laid,  //days since the user last got laid
	drug_hits,       //the hist that the user has
	drug_days_since; //if addicted how long the user
	//has not used the drug

	//long type records
	INT32		hitpoints,       //users hitpoints
	maxhitpoints;    //maximum of the users hitpoints

	//DWORD type record
	DWORD           points,          //users points
	money,           //money in hand
	bank;            //money in bank

	//unsigned char type records used as values
	unsigned char	level,           //user level
	turns,           //fight the user has left today
	hunger,          // % of hunger
	sex_today,       //sex turns left today
	std_percent,     // % of current std
	drug_addiction,  // % of drug addiction
	drug_high,       // % of how "high" the player is
	hotel_paid_fer,  //for how many more days the hotel
	//is paid for
	days_in_hospital;//how many days has the use been
	//in hospital

	/*enumerated types stored as char!!! (not int)*/
	guy_status	alive;           //user: alive, unconsious, or dead
	sex_type	sex;             //user sex: Male, Female
	guy_type	nation;          //what is he:
	//punk, headbanger ...
	weapon		arm;	         //players weapon
	desease		std;		 //current player std
	drug_type	drug;            //current player drug type
	hotel_type	rest_where;      //where the user is staying lately

	/*added values BETA 9*/
	char		unhq;		 //# of unhq bombings allowed per day
	//default=1
	char		poison;		 //number of poisoning of watr allowed
	//per day, default=1

	/*added values v0.10*/
	unsigned char   rocks,           //Rocks, usable in fights
	throwing_ability,//Throwing ability 0-100
	punch_ability,   //Punching ability 0-100
	kick_ability;    //Kicking ability 0-100

	/*reserved for future use 4 bytes reset to 0*/
	INT16		res1,
	res2;
}
user_rec;


/*typedef struct _PACK {
	char		bbsname[36],
			name[25];
	guy_type	nation;
	INT16		level,
			rank;
	INT32           points,
			money,
			bank;
	guy_status	alive;
	INT16		days_not_on;
	sex_type	sex;
	char		say_win[41],
			say_loose[41];
	INT16		turns,
			hunger,
			strength,
			defense;
	INT32		hitpoints,
			maxhitpoints;
	weapon		arm;
	INT16		condoms;
	INT16		since_got_laid;
	INT16		sex_today;
	desease		std;
	INT16		std_percent;
	drug_type	drug;
	INT16		drug_hits,
			drug_addiction,
			drug_high,
			drug_days_since;
	hotel_type	rest_where;
	unsigned char	hotel_paid_fer,
			days_in_hospital;
	} user_rec;*/

typedef struct _PACK {
	char		name[25];
	guy_type	nation;
	char		n1;
	INT16		level;
	DWORD           points;
	guy_status	alive;
	char		n2;
	sex_type	sex;
	char		n3;
	INT16		user_num,
	online;
}
scr_rec;

//typedef enum                {HANDS,PEPPER,KNIFE,CHAIN,GUN,RIFLE,LASER_GUN,SHOTGUN,MACHINEGUN,GRANADE_LAUNCHER,BLASTER,A_BOMB} weapon;
typedef unsigned char weapon;
 enum {HANDS,
              PEPPER,
              KNIFE,
              CHAIN,
              GUN,
              RIFLE,
              LASER_GUN,
              SHOTGUN,
              MACHINEGUN,
              GRANADE_LAUNCHER,
              BLASTER,
              A_BOMB,
              SHARP_STICK,
              SCREWDRIVER,
              HAMMER,
              LEAD_PIPE,
              COLT,
              ELEPHANT_GUN,
              NAILGUN,
              ASSAULT_RIFLE,
              PROTON_GUN,
              NEUTRON_PHASER,
              ULTRASOUND_GUN};

/*DWORD 	gun_price[A_BOMB+1]={0,    50,    100,  200,  500,1000, 2000,     5000,   10000,     20000,           50000,  250000};
*/
typedef unsigned char drig_type;
 enum                 {POT,HASH,LSD,COKE,PCP,HEROIN};
/*int 	drug_price[HEROIN+1]={10, 25,  50, 100, 150,200};
*/
typedef unsigned char sex_type;
 enum {MALE,FEMALE};
typedef unsigned char guy_type;
 enum {HEADBANGER,HIPPIE,BIG_FAT_DUDE,CRACK_ADDICT,PUNK};
typedef unsigned char desease;
 enum {NONE,CRAPS,HERPES,SYPHILIS,AIDS};
typedef unsigned char guy_status;
 enum {ALIVE,UNCONCIOUS,DEAD};
typedef unsigned char hotel_type;
 enum {NOWHERE,MOTEL,REG_HOTEL,EXP_HOTEL};


#endif

#if defined(PRAGMA_PACK)
#pragma pack(pop)               /* original packing */
#endif

#endif
