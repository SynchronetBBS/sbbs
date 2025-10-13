/* 
	This program uses the GAME SDK written to produce high quality Inter-BBS
	small games fast.  Best suited towards Card Games, Board Games etc...
	(Maybe even arcade games!)

	This file includes information specific to GAC_FC!

*/

#ifndef _GACFC_H
#define _GACFC_H

#include "gamesdk.h"
// include the player structure
#include "gamestru.h"


void FreeCell( void );
INT16 PlayFreeCell( void );
void CheckHomeCells( void );
void MoveFreeCard( char location1, char location2, char topCard, char bottomCard);
INT16 CheckFreeCell( void );
INT16 GetLastFreeCard( INT16 column );
INT16 CheckFreeCard( char topCard, char bottomCard, char movingTo);

INT16 FreeCellInit( void );
void DealFreeCell( void );
void DeleteSmallCard( INT16 left, INT16 top);
// 12/96 Show full sized cards
void DisplaySmallCard( INT16 left, INT16 top, INT16 card); // special one for BJ
void DisplaySmallCardBack( INT16 left, INT16 top);
void FreeShowScore( void );
INT16 FreeTotalMoves( char toColumn );
INT16 MoveFreeColumn( char col1, char col2);
char FreeKeys( char input);
char FreeDefinedKeys( char input);

// defines for the suits
#define HEARTS   0
#define CLUBS    1
#define DIAMONDS 2
#define SPADES   3

// defines for gameOver variable
#define LOST  5
#define WON   6
#define SAVED 7

// an empty spot has a 0 in it, cards are 1-52
#define EMPTY 0    // was 0

// define where the columns and rows start
#define COLUMN_X 2
#define ROW_Y    3

// define an amount of offset of x and y for columns
// may need to have the columns angle...from top left to bottom right
#define X_OFFSET 8
#define Y_OFFSET 1

// x distance between FREE and HOME cell cards
#define FREE_OFFSET 1
// define where the free cells and home cells start
#define FREE_X   67
#define FREE_Y   5
#define HOME_X   67
#define HOME_Y   14

// the next defines are used by the CheckFreeCard to determine where the
// card is going...
#define HOME_CELL  -1
#define FREE_CELL  -2
#define COLUMN     -3
#define GOING_HOME -4

// how big are the cards
#define CARD_X 7
#define CARD_Y 5

// these are also in GAMESTRU.H
// total number of columns
#define MAX_COLUMNS 8
// Max Column rows is 52/8 + 13 = 20
#define MAX_COL_ROWS 17


#define HOME_BONUS 1
#define WIN_BONUS  100

// define the maximum games a player can play/day.
EXT INT16 maxGames;



/* Don't add anything after this #endif */
#endif

