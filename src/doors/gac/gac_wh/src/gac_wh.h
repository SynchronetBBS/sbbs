/* 
	This program uses the GAME SDK written to produce high quality Inter-BBS
	small games fast.  Best suited towards Card Games, Board Games etc...
	(Maybe even arcade games!)

	This file includes information specific to GAC_FC!

*/

#ifndef _GACFC_H
#define _GACFC_H

#include "gamesdk.h"

void WaHoo( void );
INT16 PlayWaHoo( void );
INT16 WaHooInit( void );
void InitWaHooBoard( void );
void SetupWaHooBoard( void );
void DeleteWaHoo( char playerNumber, char pieceNumber);
void ShowWaHoo( char playerNumber, char pieceNumber);
INT16 MoveWaHoo( char playerNumber, char pieceNumber, INT16 dieroll);
#define START -5
void SmallDieRoll( INT16 die, INT16 x, INT16 y);
void DeleteDieRoll( INT16 x, INT16 y);

// defines for gameOver variable
#define LOST  5
#define WON   6
#define SAVED 7

// an empty spot
#define EMPTY -1

struct space
{
	char x;        // x Location of this space on the board
	char y;        // y location of this space on the board 
};

// Total spaces around the outside of the board
#define TOTAL_SPACES 48
// These are also defined in GAMESTRU.H
#define MAX_PLAYERS 4
#define MAX_PIECES  4
EXT struct space board[TOTAL_SPACES];               // setup of the board
EXT struct space homeLocations[MAX_PLAYERS][MAX_PIECES];  // where home is
EXT struct space startLocations[MAX_PLAYERS][MAX_PIECES]; // where start is
EXT struct space centerLocation;                         // where center is
// define constants for which home space the player's piece is in
#define HOME_1 TOTAL_SPACES+1
#define HOME_2 TOTAL_SPACES+2
#define HOME_3 TOTAL_SPACES+3
#define HOME_4 TOTAL_SPACES+4
#define CENTER TOTAL_SPACES+5

// define constants for where the players pieces start on the board and
// where they enter their homes (refers to the index into the board structure)
#define START_1 4
//#define START_2 16
//#define START_3 28
//#define START_4 40
#define ENTER_HOME_1 2
//#define ENTER_HOME_2 14
//#define ENTER_HOME_3 26
//#define ENTER_HOME_4 38
#define CORNER_1 8
#define DISTANCE 12


// define the maximum games a player can play/day.
EXT INT16 maxGames;

/* Don't add anything after this #endif */
#endif

