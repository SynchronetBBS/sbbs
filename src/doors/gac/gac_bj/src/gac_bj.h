/* 
	This program uses the GAME SDK written to produce high quality Inter-BBS
	small games fast.  Best suited towards Card Games, Board Games etc...
	(Maybe even arcade games!)

	This file includes information specific to GAC_BJ!

*/

#ifndef _GACBJ_H
#define _GACBJ_H

#ifdef  EXTDRIVER
#define     EXT /* */
#else
#define     EXT extern
#endif

#include "../gamesdk/gamesdk.h"
#include "gamestru.h"

#define user_level 60

// 1/97 changed decls?
#define uchar unsigned char
//#define uint unsigned int
// 3/05 genwrap gives us this
//#define uint WORD
#define ushort unsigned short
#define ulong unsigned long



// Defines for this game
#define MAX_DECKS       100
#define MAX_CARDS       10              /* maximum number of cards per hand */
#define MAX_HANDS       4               /* maximum number of hands per player */

#define DEBUG 0 // Set to 1 if you want debug mode...

#define J 11    /* jack */
#define Q 12    /* queen */
#define K 13    /* king */
#define A 14    /* ace */

#define H 0             /* heart */
#define D 1     /* diamond */
#define C 2             /* club */
#define S 3             /* spade */
									/* bits used in misc variable */
#define INPLAY          (1<<0)                  /* hand in play */

/* 4/05 removed EXT here... what was it supposed to be doing? */
enum {                              /* values for status bytes */
	 BET                                                    /* betting */
	,WAIT                                                   /* waiting for turn */
	,PLAY                                                   /* playing his hand */
	,SYNC_P                                                 /* In sync area - player */
	,SYNC_D                                                 /* In sync area - dealer */
	};

#if defined(PRAGMA_PACK)
	#pragma pack(push,1)            /* Disk image structures must be packed */
#endif

typedef struct _PACK { char value, suit; } card_t;

#if defined(PRAGMA_PACK)
	#pragma pack(pop)       /* original packing */
#endif

// These are mine
EXT char temp[20];

EXT char    lncntr;             /* Line counter */
// EXT uchar sys_nodes,
EXT uchar node_num;              /* Number of nodes on system */
														/* Current node number */
EXT uint user_number;           /* User's number */
EXT ulong user_cdt;         /* User's money */
EXT char com_port;          /* Number of COM port  */

EXT uchar   misc;
EXT uchar   curplayer;
EXT uchar   total_nodes;
EXT uchar   total_decks,sys_decks;
EXT INT16     cur_card;
EXT uchar   dc;
EXT card_t  dealer[MAX_CARDS];
EXT int     gamedab;                         /* file handle for data file */
EXT card_t  card[MAX_DECKS*52];
EXT card_t  bjplayer[MAX_HANDS][MAX_CARDS];
EXT int     hands;
EXT char    pc[MAX_HANDS];
EXT uchar   total_players;
//char  logit=0;
EXT WORD    node[MAX_NODES];   /* the usernumber in each node */
EXT char    status[MAX_NODES];
// ulong   money;
EXT ulong money;
EXT uint    bet[MAX_HANDS],ibet,min_bet,max_bet;
EXT char    tmp[81];
/*
EXT char    *UserSays="`Bright Magenta`%s `Magenta`says \"`Bright Magenta`%s`Magenta`\"\r\n";
EXT char    *UserWhispers="`Bright Magenta`%s `Magenta`whispers \"`Bright Cyan`%s`Magenta`\"\r\n";
EXT char    *ShoeStatus="\r\n`Bright White`Shoe: %u/%u\r\n";
*/

void play(void);
char *cardstr(card_t card);
char hand(card_t card[MAX_CARDS], char count);
void getgamedat(char lockit);
void putgamedat(void);
void getcarddat(void);
void putcarddat(void);
void shuffle(void);
void waitturn(void);
void nextplayer(void);
char lastplayer(void);
char firstplayer(void);
void getnodemsg(void);
void putnodemsg(char *msg,char nodenumber);
void putallnodemsg(char *msg);
void syncplayer(void);
void syncdealer(void);
//void moduserdat(void);
char *hit(void);
char *stand(void);
char *doubit(void);
char *split(void);
void open_gamedab(void);
void create_gamedab(void);
char *activity(char status_type);
void chat(void);
void list_bj_players(void);
char *joined(void);
char *left(void);
void strip_symbols(char *str);
void debug(void);       


// This programs configurations
EXT INT16 startmoney, dailymoney;


// 12/96 Show full sized cards
void DisplayCard( INT16 left, INT16 top, card_t card); // special one for BJ
void DisplayCardBack( INT16 left, INT16 top);
EXT char currentLine, currentTop;

void UpdateCursor(void);
#define SCROLL_VALUE 2
#define DEALER_X 55     // Where the initial two dealer cards are shown
#define DEALER_X2 45    // where we start showing all of the dealers cars at end
#define FINAL_Y 6       // the y for the cards in the final
/* Don't add anything after this #endif */

#endif

