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

// These are also in GAC_FC.H
// total number of columns
#define MAX_COLUMNS 8
// Max Column rows is 52/8 + 13 = 20
#define MAX_COL_ROWS 17

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
    // cards are numbered 1-52
    // enough room to store all cards in any column
    char column[MAX_COLUMNS][MAX_COL_ROWS];
    char freeCell[4];    // store one card in each free cell
    char homeCell[4];    // store one card in each home cell

    INT16 gamesWon;   // number of games user has won
    INT16 games;      // number of games user has played total
    INT16 gamesToday; // number of games user has played today (sysop max)
    char savedGame;  // did the player save a game?

    char unused[20]; // some unused space so we can add to it later on...

};

EXT struct player  player, opponent;

// Use some number defines (record length is record + unused - 2 account)
#define RECORD_LENGTH 253
#define RECORD_UNUSED 0L

#ifdef __BIG_ENDIAN__
void player_le(struct player *);
#else
#define player_le(x)
#endif
void player_read(struct player *, FILE *);
void player_write(struct player *, FILE *);

#endif

