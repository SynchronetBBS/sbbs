/* This file contains information that is needed by the GAMESDK also.

    Specifically, the player structure, player and opponent def's and size.
*/


#ifndef _GAMESTRU_H
#define _GAMESTRU_H

#include <stdio.h>

#ifdef  EXTDRIVER
#define     EXT /* */
#else
#define     EXT extern
#endif

// These are also defined in GAC_WH.H
#define MAX_PLAYERS 4
#define MAX_PIECES  4

// Structure to hold all info about a player (should be player)
struct player
{
 // Variables used by the GAME SDK (Must be in the following order in
 // the strucutre for the Lists to work correctly...

    char names[21]; // The players handle
    char real_names[51];  // The real name
    INT16 bbs;  // bbs number for this player
    INT16 laston;  //last day on
    DWORD money; // the score for the player (I know...)
    INT16 account;  // not stored in the files...

 // Variables used for this game
    // enough room to store the player positions on the board (4pieces/player)
    char locations[MAX_PLAYERS][MAX_PIECES];
    char playerColor; // what color is the player...

    INT16 gamesWon;   // number of games user has won
    INT16 games;      // number of games user has played total
    INT16 gamesToday; // number of games user has played today (sysop max)
    char savedGame;  // did the player save a game?


    char unused[20]; // some unused space so we can add to it later on...

};

EXT struct player  player, opponent;

// Use some number defines (old record length is record + unused - 2 account)
#define RECORD_LENGTH 126
#define RECORD_UNUSED 0L

#ifdef __BIG_ENDIAN__
void player_le(struct player *);
#else
#define player_le(x)
#endif
void player_read(struct player *, FILE *);
void player_write(struct player *, FILE *);

#endif

