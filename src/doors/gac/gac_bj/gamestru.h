/* This file contains information that is needed by the GAMESDK also.

    Specifically, the player structure, player and opponent def's and size.
*/

#include "gamesdk.h"	/* Needs PACK stuff */

#ifndef _GAMESTRU_H
#define _GAMESTRU_H

#ifdef  EXTDRIVER
#define     EXT /* */
#else
#define     EXT extern
#endif

// 1/97 changed structure to use platform independent types
// Structure to hold all info about a player (should be player)

#if defined(PRAGMA_PACK)
	#pragma pack(push,1)            /* Disk image structures must be packed */
#endif

struct player
{
    char names[21]; // The players handle //char
    char real_names[51];  // The real name //char
    INT16 bbs;  // bbs number for this player //int
    INT16 laston;  //last day on // int
    //long int money;  // money in their hands
    DWORD money; // unsigned long
    INT32 investment;  // money in an investment // long int
    INT16 inv_start; // day money was invested // int
//      long int bank;  // money in their bank
    INT16 account; // int
    char unused[98]; // space not used in the file //char
} _PACK;

#if defined(PRAGMA_PACK)
	#pragma pack(pop)       /* original packing */
#endif

EXT struct player  player, opponent;

// Use some number defines // was 186L
#define RECORD_LENGTH 186
#define RECORD_UNUSED 0L

#ifdef __BID_ENDIAN__
void player_le(struct player *);
#else
#define player_le(x)
#endif

#endif
