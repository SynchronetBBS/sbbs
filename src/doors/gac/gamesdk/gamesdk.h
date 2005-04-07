/* This file contains all of the variables and protypes used by the
   GAMESDKx.C files.

   Game Designer will still have to modify the IBBSCFG.C, and GAC_ID_S.C files

   Include ALL GAMESDK*.C files in your project as well as ALL ART\*.C files
   
   This GAME SDK is intended to make creating small Inter-BBS games fast
   and easy.

   All of these files should reside in the GAC_CS\GAMESDK directory
*/

#ifndef GAMESDK_H
#define GAMESDK_H

#ifdef  EXTDRIVER
#define     EXT /* */
#else
#define     EXT extern
#endif

// Define GAC_DEBUG if you want detailed debug log file created
// #define GAC_DEBUG 1

#include <stdio.h>

#define OPEN_DOOR_6 // needed for some of my libraries to still work with IGMs

#include "OpenDoor.h"          // Include the OpenDoors Library!
#include "xpendian.h"

#include "gamestru.h"   // found in the game's own directory
#ifdef KEEP_REG
#include "regkey.h"          // Include the RegKey Library!
#endif
#include "display.h"                 // To display internally imbedded ANS,AVT,RIP,ASC files
#include "netmail.h"  //The Inter-BBS Kit
#include "gregedit.h"  // needed for editor


#define MSG_LENGTH 3750 //needed for editor
#define USER_MSG_LEN 1126
EXT char msg_text[MSG_LENGTH];  // The text buffer
#define MAX_EDIT_LINES 15
#define EDIT_LINE_WIDTH 70


#define BUF_SIZE 5000   // used for fast file copies
// The following are no longer needed since we use file based Inter-BBS
// EXT char buffer[BUF_SIZE];
// EXT unsigned INT16 bufsize;

// the structure for the message header files
EXT tMessageHeader MessageHeader;

// artwork for the score listings
#include "art/bbscur.h"
//#include "art/bbslst.h"
#include "art/plycur.h"
//#include "art/plylst.h"

#include "gen_defs.h"
#include "genwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
//#include <share.h>
//#include <io.h>
//#include <time.h>
//#include <alloc.h>
//#include <errno.h>
/* Dunno what these do... */
//#include <dos.h>
//#include <dir.h>
#include <ctype.h>
#include <stdio.h>
#include <ciolib.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>

// Useful defines
#define MAX_NODES 259
#define ALL -5
#define TOP -4

// some number defines for use by the text editors                                  /* Control characters */
#ifndef STX
#define STX     0x02                            /* Start of text                        ^B      */
#endif
#ifndef ETX
#define ETX     0x03                            /* End of text                          ^C      */
#endif
#ifndef BS
#define BS              0x08                            /* Back space                           ^H      */
#endif
#ifndef TAB
#define TAB     0x09                            /* Horizontal tabulation        ^I      */
#endif
#ifndef LF
#define LF              0x0a                            /* Line feed                            ^J      */
#endif
#ifndef FF
#define FF              0x0c                            /* Form feed                            ^L      */
#endif
#ifndef CR
#define CR              0x0d                            /* Carriage return                      ^M      */
#endif
#ifndef ESC
#define ESC     0x1b                            /* Escape                                       ^[      */
#endif
#ifndef SP
#define SP      0x20                /* Space                        */
#endif


#define DCD    0x8000           /* Data carrier detect bit in msr                       */
#define LOOP_NOPEN   50         /* Retries before file access denied        */
#define LOOP_GOPEN   50         /* Retries before file access denied        */

// These must be modified for each game and are in the file
// GAMES_S.C
void SetProgVariables( void ); // Sets program name, copyright, etc...
void PrivateConfig(char *keyword, char *options);   // use private config options
void Config( void ); // The configuration routine..
void InitGamePlay( void ); // Do some startup maintenance
void PrivateExit( void );
void PrivatePlayerInit( void );
void PrivatePlayerUpdate( void);
void Title( void ); // main program menu

void ProcessPrivateConfigLine(INT16 nKeyword, char *pszParameter, void *pCallbackData);
tIBResult PrivateReadConfig(tIBInfo *pInfo, char *pszConfigFile);

// Structure to keep track of the list of players
struct player_list
{
	INT16 account;

	char names[21];
	char real_names[51];
	INT16 bbs;
	//long int wealth;
	unsigned long wealth;
	struct player_list *next;
};

struct bbs_list
{
	INT16 bbs;
	// long int wealth
	unsigned long wealth;
	long int players;
	struct bbs_list *next;
};


// These should not need to be touched
/// IN GAMESDK.C
void DeleteIBBSPlayer( void );   // allows a SysOp to delete a player

void MyConfig(char *keyword, char *options); // Read the config file correctly...
void CheckPlayer( void ); // Checks if the player is in the database for this BBS, either lets them play, logs them out, or starts new
void BeforeExitFunction( void); // Executed before an exit...

// Get the players info, pass structure and account number, returns -1 if bad number
INT16 GetPlayerInfo( struct player *account, INT16 account_num, INT16 local);     
// Write updated information to PLAYER.DAT
void WritePlayerInfo( struct player *account, INT16 account_num, INT16 local);  
INT16 g_rand( INT16 ); // My random function...
void InitGame( INT16 pause); // The initialization function...
void command_line( void ); // Command line parameters
void gac_pause( void); // My Pause function
char *getname( INT16 node); // Get's the players name if given the node number
void ShowUnreg( void );  // Shows an unregistered item
char GetMenuChoice( void ); // get's a menu choice...and processes information
void Information(void);  // Function to display info on copyright, programmers...etc...
int  nopen(char *str, int access);
void GetPlayersList( INT16 byplayer );            // Get a list of all players in memory for spying, etc...
FILE *gopen(char *str, int mode, int access);  // My network open function, send it file name, open and access modes.
void ShowPlayersList( INT16 bbs );                        // Show the list of all players in memory local or all displayed
void SortPlayersList( struct player_list *temp_player, INT16 byplayer ); // Sort players according to money
char *getbbsname( INT16 bbs); // This function should retrieve a BBS name from the Inter-BBB file
void ViewBBSList( INT16 average );  // Shows the list of BBSs
void CheckDate( void ); // checks for a possible reset
void MakeBulletins( INT16 last); // Make the displayable bulletins (last = TRUE indicates new tourney
void MakeTopBBS(INT16 average);         // Get a list of top BBSs into memory (average = TRUE means average wealth)
void SortBBSList( struct bbs_list *temp_bbs ); // Sort bbss according to wealth
INT16 CheckHandle(char *input); // Checks to make sure only one player on each BBS has the same handle
void g_clr_scr(void);
char PromptBox( char prompt1[200], char prompt2[200], char responses[20], INT16 bottom );
void SendIBBSMsg( INT16 reply); // The function to allow a user to send a msg to other users.
#ifdef OPEN_DOOR_6
tODEditMenuResult EditQuit( void *unused );
#endif
char EditorHelp( void );
void ColorHelp( void );
INT16 CheckPlayerName( char *input); // Check to see if the name given exists
char *g_strstr(char *name, char *input); // perform a non case sensitive partial string comparison
void CheckMail( void );
INT16 CheckMaintenance( void );
void UpdateTime( void );
#define RIP_FILE -3
#define ANS_FILE -4
#define ASC_FILE -5
INT16 g_send_file( char *filename);
INT16 SendArchive(char *file, INT16 type);
void HelpDecrypt( char *line);
void ListBBSs( void ); // Lists the BBS in the Tournament League
void ViewScores (void );
FILE *myopen(char *str, char *mode, int share );


/// IN MSWAIT.OBJ
void mswait(INT16 ms);


/// IN GAMESDKI.C
void Inbound( INT16 night );
void ResetIBBS( void ); // This allows the League coordinator to reset the league instantly
void ProcessRoute( void );
void UpdateRouteFile( void );
void OutProcess( void );
void InProcess( void );


/// IN GAMESDKO.C
void Maintain( void ); // The Nightly maintenance
void Outbound( INT16 night );
void MakeRouteTime(void);
void MakeRouteReport( void );
void SendIBBSData(char inbound, INT16 frombbs);
INT16 copyfile(char *fromfile, char *tofile);
void SendAll( void ); // Let SysOps send a message to each other...
void ErrorDetect( tIBResult returned); // Test for errors and write to the log file and the screen
void InitIBBS( void ); //This function gathers the interbbs information for use by several parts of the program...
void PromptString( char prompt1[200], char *response, INT16 max_len, char minchar, char maxchar, INT16 bottom );


//void SendConfig( void ); // Send the INTERBBS.CFG file to all systems listed


EXT char filename[128];
EXT char inbounddir[128];
EXT char prompt[120];
#define CLEANUP 3


EXT char binkley;
EXT char currentversion[6];
EXT char wagon_wheel;
EXT char checkForDupes;

struct routeinfo
{
	// used for ROUTETIME
	char datebuf[9];
	char timebuf[9];
	time_t first, second, routetime;
	char version[6];
	INT16 from_bbs;
	INT16 registered;
};

// 12/96 #define ROUTEINFOSIZE 34
#define ROUTEINFOSIZE 40

#ifdef __BID_ENDIAN__
void routeinfo_le(struct routeinfo *);
#else
#define	routeinfo_le(x)
#endif

EXT struct routeinfo routing;

// some variables for controlling everything.
EXT char doorpath[128], configfile[128], valcode[21], regcode[21], league[4], ThisBBSAddress[24], netmaildir[128];
EXT char executable[22]; // 2/97 Must be larger for RPG_ENG
EXT char ibbsgametitle[20];
EXT char ibbsid[4];
EXT char screen[4005];
#ifdef KEEP_REG
EXT RKVALID registered;
#endif
// Interbbs structure variable
EXT tIBInfo InterBBSInfo;
EXT unsigned long lwLastMsgSent;  // record the last created message...
EXT INT16 currentday, thisbbs, tourneylength, refused;

EXT unsigned char sys_nodes;
EXT char symbols;

// time variables
EXT long int total_uses;
EXT time_t first_time;


// Start of the Linked list...
EXT struct player_list *list, temp_player;

// Start of the Linked list...
EXT struct bbs_list *bbslist, temp_bbs;

// a couple of defines used in processing info...
#define OUTGOING -5
#define INCOMING -10


// For debug logs
EXT char gac_debugfile[128];
EXT FILE *gac_debug;

// 4/97 Added support to overlay gamesdk games
#define GSDK_OVERLAY
// from GAMESDK.C
EXT char username[36];
EXT char bbsname[41];
// from GAMESDKI.C
EXT tIBResult returned;

#ifndef O_DENYALL
#define O_DENYALL	1
#endif



#endif

