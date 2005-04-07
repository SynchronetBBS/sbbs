/* This file contains the functions that are used in every game, but must
   be modified for each game.
*/
#define EXTDRIVER
#include "gac_fc.h"
#undef EXTDRIVER

#include "dirwrap.h"

void SetProgVariables(void)
{
 
	strcpy(od_control.od_prog_name, "Tournament FreeCell v2.0");
	strcpy(od_control.od_prog_version, "Version 2.0");
	strcpy(od_control.od_prog_copyright, "Copyright 2005 GAC/Vagabond Software");
	strcpy(currentversion, "2.0");
	// ibbsgame title can ONLY be 6 characters long!!!
	strcpy(ibbsgametitle, "gac_fc");
	strcpy(ibbsid, "fc");
	// Set the validation code for this Game
	strcpy(valcode, "OEQH5EB5TC" );
	// Set the executable file name
	#ifdef ODPLAT_NIX
	strcpy(executable, "gac_fc");
	#else
	strcpy(executable, "gac_fc.exe");
	#endif
	return;
}



// Configuration for parameters unique to this game
void PrivateConfig(char *keyword, char *options)
{
	if(stricmp(keyword, "MAX_GAMES") == 0)
	{
		maxGames=atoi(options);
	}
	
	return;
}




// this game's unique initialization
void InitGamePlay( void )
{
	
	// test for a answer to ctrl-e ============================================
	od_clear_keybuffer();

	od_putch(5); // ctrl-e
	od_sleep(500);
	if(od_get_key(0))
	{
		while(od_get_key(0));
		od_printf("\r\n`Bright red flashing`*** ATTENTION ***`Bright white`\r\n");
		od_printf("\r\n%s uses Ctrl-E (ENQ) for the 'club' card "
			"symbol.", od_control.od_prog_name);
		od_printf("\r\nYour terminal responded to this control character with an "
			"answerback string.");
		od_printf("\r\nYou will need to disable all Ctrl-E (ENQ) answerback "
			"strings (Including \r\nCompuserve Quick B transfers) if you wish to "
			"toggle card symbols on.\r\n\r\n");
		symbols=0;
		od_get_key(TRUE);
	}

}

// this game's unique exit requirements
void PrivateExit( void )
{
	return;
}

// This game's unique player initialization
void PrivatePlayerInit( void )    
{
	player.money = 0;
	player.gamesWon = 0;
	player.games = 0;
	player.gamesToday = 0;
	player.savedGame = FALSE;
	
	return;
}

// This game's unique player update (daily)
void PrivatePlayerUpdate( void)
{
	// check to see if they have gotten their extra money today
	// check to see if they have played today
		if (player.laston < currentday) player.gamesToday = 0;
	
	return;
}



// ===========================================================================
// This part of the program will create the config files and setup the game
// All pretty format and very easy to use
// ===========================================================================
void Config( void )
{
	char chMenuChoice, logging, Crash, Hold, EraseOnSend;
		//, EraseOnReceive, IgnoreZonePoint;
	char sysop[85], bbs[85], reg[25], bbsdir[129], disable[25], max_games[5];
		// , tot_decks[5], min_bet[15],
		// max_bet[15], default_bet[15], start_money[15], daily_money[15], 
	char tourlength[6], leagueid[4];
		//, bbsnumber[6],
	char thisaddress[16], netmail[128], temp[10], inbound[128], tot_nodes[5];
	FILE *config;
	INT16 more = TRUE;

	//Init the strings
	sysop[0]='\0';
	bbs[0]='\0';
	reg[0]='\0';
	bbsdir[0]='\0';
	disable[0]='\0';
	max_games[0]='\0';
	tourlength[0]='\0';
	leagueid[0]='\0';
	thisaddress[0]='\0';
	netmail[0]='\0';
	inbound[0]='\0';
	strcpy(tot_nodes, "1");

	chMenuChoice = 'Y';
	logging = 'Y';
	Crash = 'N';
	Hold = 'N';
	EraseOnSend = 'Y';


	// Force local mode for configuration
	od_control.od_force_local = TRUE;
	// Ignore Copyright
	od_control.od_nocopyright = TRUE;
	// Registration Information!
		strcpy(od_registered_to, "Gregory Campbell");
		od_registration_key = 583073792L;
	// init the open doors stuff
	od_init();
	// disable the status line
	od_set_statusline(STATUS_NONE);
	od_control.od_status_on = FALSE;
	window(1,1,25,80);
	od_clr_scr();

	od_control.od_help_text2 = (char *) "Copyright G.A.C. Computer Services - [F1]=User Info [F10]=Disable Status Line";
	// Set the border I want for the boxes
	od_control.od_box_chars[BOX_UPPERLEFT] = (unsigned char) 218;
	od_control.od_box_chars[BOX_TOP] = (unsigned char) 196;
	od_control.od_box_chars[BOX_UPPERRIGHT] = (unsigned char) 183;
	od_control.od_box_chars[BOX_LEFT] = (unsigned char) 179;
	od_control.od_box_chars[BOX_LOWERLEFT] = (unsigned char) 212;
	od_control.od_box_chars[BOX_LOWERRIGHT] = (unsigned char) 188;
	od_control.od_box_chars[BOX_BOTTOM] = (unsigned char) 205;
	od_control.od_box_chars[BOX_RIGHT] = (unsigned char) 186;


	// Read in the information from the Config file (if it exists)
	config = fopen(od_control.od_config_filename, "rt");

	if (config != NULL)
	{
			fscanf( config, "%*[^ ] %[^\n]\n", sysop);
	fscanf( config, "%*[^ ] %[^\n]\n", bbs);
		fscanf( config, "%*[^ ] %[^\n]\n", reg);
		fscanf( config, "%*[^ ] %[^\n]\n", bbsdir);
		fscanf( config, "%*[^ ] %[^\n]\n", max_games);
		fscanf( config, "%*[^ ] %[^\n]\n", tourlength);
		fscanf( config, "%*[^ ] %[^\n]\n", leagueid);
		fscanf( config, "%*[^ ] %[^\n]\n", thisaddress);
		fscanf( config, "%*[^ ] %[^\n]\n", netmail);
		fscanf( config, "%[^\n]\n", disable);
		if (stricmp( disable, "DisableLogging") == 0)
			logging = 'N';
		else
			logging = 'Y';

		fscanf( config, "%*[^ ] %[^\n]\n", temp);
		if (stricmp( temp, "YES") == 0)
			Crash = 'Y';
		else
			Crash = 'N';

		fscanf( config, "%*[^ ] %[^\n]\n", temp);
		if (stricmp( temp, "YES") == 0)
			Hold = 'Y';
		else
			Hold = 'N';

		fscanf( config, "%*[^ ] %[^\n]\n", temp);
		if (stricmp( temp, "NO") == 0)
			EraseOnSend = 'N';
		else
			EraseOnSend = 'Y';

		fscanf( config, "%*[^ ] %[^\n]\n", inbound);
		fscanf( config, "%*[^ ] %[^\n]\n", tot_nodes);
		fclose(config);
	}
	else
	{
		strcpy(sysop, "No Name");
		strcpy(bbs, "No Board");
		strcpy(reg, "0000000");
		strcpy(bbsdir, "C:\\BBS\\");
		strcpy(max_games, "5");
		strcpy(tourlength, "0");
		strcpy(leagueid, "00");
		strcpy(thisaddress, "1:1/0");
		strcpy(netmail, "C:\\FD\\NETMAIL\\");
		strcpy(inbound, "C:\\FD\\inbound\\");
		strcpy(tot_nodes, "1");

		logging = 'Y';

		Crash = 'N';
		Hold = 'N';
		EraseOnSend = 'Y';
	}

	// Show the config screen, get user info etc...
	 //for(;;)
	 while (more == TRUE)
	 {
			//  Clear the screen */
			od_clr_scr();
			//  Display main menu. */
	 od_printf("`bright white`                    %s `dark cyan`Configuration\n\r", od_control.od_prog_name);
	 // od_printf("`dark cyan`Programmed by `bright cyan`Gregory Campbell\n\r");
	 od_printf("`dark cyan`                    %s\n\r", od_control.od_prog_copyright);
	 od_printf("`dark red`");
	 if(od_control.user_ansi || od_control.user_avatar)
	 {
			od_repeat((unsigned char)196, 79);
	 }
	 else
	 {
			od_repeat('-', 79);
	 }
	 od_printf("\n\r`dark cyan`");
	 od_printf("`dark cyan`  [`bright cyan`A`dark cyan`] %-30s`bright cyan`%s\n\r", "SysOp Name: ", sysop);
	 od_printf("`dark cyan`  [`bright cyan`B`dark cyan`] %-30s`bright cyan`%s\n\r", "BBS Name: ", bbs);
	 od_printf("`dark cyan`  [`bright cyan`C`dark cyan`] %-30s`bright cyan`%s\n\r", "Registration Code: ", reg);
	 od_printf("`dark cyan`  [`bright cyan`D`dark cyan`] %-30s`bright cyan`%-40.40s\n\r", "Dropfile Directory: ", bbsdir);
	 od_printf("`dark cyan`  [`bright cyan`E`dark cyan`] %-30s`bright cyan`%s\n\r", "Use Logging: ", ( (logging=='Y' ) ? "Yes":"No"));
	 od_printf("`dark cyan`  [`bright cyan`F`dark cyan`] %-30s`bright cyan`%s\n\r", "Total Nodes: ", tot_nodes);
	 od_printf("`dark cyan`  [`bright cyan`G`dark cyan`] %-30s`bright cyan`%s\n\r", "Maximum Games per Day: ", max_games);
	 od_printf("`dark cyan`  [`bright cyan`H`dark cyan`] %-30s`bright cyan`%s\n\r", "Tournament Length: ", tourlength);
	 od_printf("`dark cyan`  [`bright cyan`I`dark cyan`] %-30s`bright cyan`%s\n\r", "Inter-BBS League ID: ", leagueid);
	 od_printf("`dark cyan`  [`bright cyan`J`dark cyan`] %-30s`bright cyan`%s\n\r", "NetMail Address: ", thisaddress);
	 od_printf("`dark cyan`  [`bright cyan`K`dark cyan`] %-30s`bright cyan`%-40.40s\n\r", "Inbound File Directory: ", inbound);
	 od_printf("`dark cyan`  [`bright cyan`L`dark cyan`] %-30s`bright cyan`%-40.40s\n\r", "NetMail Directory (*.MSG): ", netmail);
	 od_printf("`dark cyan`  [`bright cyan`M`dark cyan`] Advanced Inter-BBS Options\n\r");

	 od_printf("`dark cyan`  [`bright cyan`Q`dark cyan`] Quit Configuration\n\r");
	 od_printf("`bright white`Press the key corresponding to the option of your choice: `dark cyan`" );
	 //  Get the user's choice from the main menu. This choice may only be */
	 chMenuChoice = od_get_answer("ABCDEFGHIJKLMQ");

			// Make it prettier
			if(od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
			{
				// Only for ANSI, RIP or AVATAR
				od_draw_box(5, 12, 75, 16);
				od_set_cursor(14,7);
			}
			else
				od_printf("\n\r");

			//  Perform the appropriate action based on the user's choice */
			switch(chMenuChoice)
			{
	 case 'A':
				od_printf("`bright cyan`Enter the SysOp Name: `bright white`");
						od_input_str(sysop, 80, 32, 127);
			break;

	 case 'B':
		od_printf("`bright cyan`Enter the BBS Name: `bright white`");
					od_input_str(bbs, 80, 32, 127);
		break;

	 case 'C':
		od_printf("`bright cyan`Enter your Registration Code: `bright white`");
					od_input_str(reg, 20, 32, 127);
		break;

	 case 'D':
		od_printf("`bright cyan`Enter the Directory to your drop files: `bright white`");
						if(od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
						{
							// Only for ANSI, RIP or AVATAR
							od_set_cursor(15,7);
						}
						else
							od_printf("\n\r");
						od_input_str(bbsdir, 128, 32, 127);
						backslash(bbsdir);

			break;

	 case 'E':
		 if (logging == 'Y')
			logging = 'N';
		 else
			logging = 'Y';
		 break;

	 case 'F':
			od_printf("`bright cyan`Enter the Total Number of Nodes: `bright white`");
						od_input_str(tot_nodes, 4, 48, 57);
			break;
	 case 'G':
			od_printf("`bright cyan`Enter the Maximum number of games a user can play per day: `bright white`");
						od_input_str(max_games, 4, 48, 57);
			break;
	 case 'H':
			od_printf("`bright cyan`Enter the length of the Tournament in days (0 disables reset): `bright white`");
						od_input_str(tourlength, 5, 48, 57);
			break;
	 case 'I':
			od_printf("`bright cyan`Enter the Inter-BBS League ID: `bright white`");
						od_input_str(leagueid, 3, 32, 127);
			break;
	 case 'J':
			od_printf("`bright cyan`Enter your BBS Fidonet Address: `bright white`");
						od_input_str(thisaddress, 15, 32, 127);
			break;
	 case 'K':
			od_printf("`bright cyan`Enter the path to your inbound file directory: `bright white`");
						if(od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
						{
							// Only for ANSI, RIP or AVATAR
							od_set_cursor(15,7);
						}
						else
							od_printf("\n\r");

						od_input_str(inbound, 127, 32, 127);
						backslash(inbound);

			break;
	 case 'L':
			od_printf("`bright cyan`Enter the path to your netmail directory: `bright white`");
						if(od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
						{
							// Only for ANSI, RIP or AVATAR
							od_set_cursor(15,7);
						}
						else
							od_printf("\n\r");

						od_input_str(netmail, 127, 32, 127);
						if (netmail[strlen(netmail)-1] != '\\') strcat(netmail, "\\");

			break;

	 case 'M':


		more = TRUE;
		// Show the advanced Inter-BBS config screen
		while (more == TRUE)
		 {
				//  Clear the screen */
				od_clr_scr();
				//  Display main menu. */
				od_printf("`bright cyan`                    %s `cyan`Configuration\n\r", od_control.od_prog_name);
			 od_printf("`dark cyan`                    %s\n\r", od_control.od_prog_copyright);
			 od_printf("`dark red`");
			 if(od_control.user_ansi || od_control.user_avatar)
			 {
					od_repeat((unsigned char)196, 79);
			 }
			else
			 {
					od_repeat('-', 79);
			 }
			od_printf("\n\r`dark cyan`");
		 od_printf("`dark cyan`  [`bright cyan`A`dark cyan`] %-35s`bright cyan`%s\n\r", "Crash Mail ALL Messages: ", ( (Crash=='Y' ) ? "Yes":"No"));
		 od_printf("`dark cyan`  [`bright cyan`B`dark cyan`] %-35s`bright cyan`%s\n\r", "Hold ALL Messages: ", ( (Hold=='Y' ) ? "Yes":"No"));
		 od_printf("`dark cyan`  [`bright cyan`C`dark cyan`] %-35s`bright cyan`%s\n\r", "Erase Messages After Sending: ", ( (EraseOnSend=='Y' ) ? "Yes":"No"));

		 od_printf("`dark cyan`  [`bright cyan`Q`dark cyan`] Quit Back to Main Configuration\n\r\n\r");
		 od_printf("`bright white`Press the key corresponding to the option of your choice: `dark cyan`" );
		 //  Get the user's choice from the main menu. This choice may only be */
		 chMenuChoice = od_get_answer("ABCQ");

			//  Perform the appropriate action based on the user's choice */
			switch(chMenuChoice)
			{
				case 'A':
					if (Crash == 'Y')
						Crash = 'N';
					else
						Crash = 'Y';
					break;

				case 'B':
					if (Hold == 'Y')
						Hold = 'N';
					else
						Hold = 'Y';
					break;

				case 'C':
					if (EraseOnSend == 'Y')
						EraseOnSend = 'N';
					else
						EraseOnSend = 'Y';
					break;
				case 'Q':
					more = FALSE;
					break;

			}
		}
		more = TRUE;
		break;

		case 'Q':
		 od_printf("`cyan`Save any changes (`bright cyan`Y`cyan`/`bright cyan`n`cyan`)? ");
			//  Get user's response */
			if (od_get_answer("YN\r") != 'N')
			{
		config = fopen( od_control.od_config_filename, "wt" );

		fprintf(config, "SysopName %s\n", sysop);
		fprintf(config, "SystemName %s\n", bbs);
		fprintf(config, "RegistrationKey %s\n", reg);
		fprintf(config, "BBSDir %s\n", bbsdir);
		fprintf(config, "MaxGames %s\n", max_games);
		fprintf(config, "Length %s\n", tourlength);
		fprintf(config, "League %s\n", leagueid);
		fprintf(config, "ThisBBSAddress %s\n", thisaddress);
		fprintf(config, "NetMailDir %s\n", netmail);

		if (logging == 'N')
			fprintf(config, "DisableLogging\n");
		else
			fprintf(config, ";DisableLogging\n");



		if (Crash == 'N')
			fprintf(config, "Crash No\n");
		else
			fprintf(config, "Crash Yes\n");

		if (Hold == 'N')
			fprintf(config, "Hold No\n");
		else
			fprintf(config, "Hold Yes\n");

		if (EraseOnSend == 'N')
			fprintf(config, "EraseOnSend No\n");
		else
			fprintf(config, "EraseOnSend Yes\n");

		fprintf(config, "InboundDir %s\n", inbound);

		fprintf(config, "TotalNodes %s\n", tot_nodes);

		fclose(config);
			}
			more = FALSE;
			break;

			}
	 }
	return;
}
// ===========================================================================
// The main menu
void Title( void )
{
	char str[120];
	INT16 more = TRUE;

	od_clr_scr();
	od_printf("`Cyan`%s\r\n", od_control.od_prog_name);
	od_printf("`CYAN`%s\r\n", od_control.od_prog_copyright);

	while(1)
	{

	// Check to see if the user has any mail waiting.
	CheckMail();

		od_clr_scr();
		g_send_file("gac_fc_1");
		if (od_control.user_ansi) od_set_cursor(1,1);

		od_printf("`CYAN`%s\r\n", od_control.od_prog_copyright);
		od_printf("`Cyan`%s\r\n\n\r", od_control.od_prog_name);
		od_printf("     `bright cyan`C`bright blue`)ompeting BBSs\r\n");
		od_printf("     `bright cyan`I`bright blue`)nformation and Instructions\r\n");
		od_printf("     `bright cyan`P`bright blue`)lay the Game\r\n");
		od_printf("     `bright cyan`L`bright blue`)ist Tournament Players\r\n");
		// od_printf("     `bright cyan`P`bright blue`)layers in On-Line Game\r\n");
		od_printf("     `bright cyan`S`bright blue`)end a message\r\n");
		od_printf("     `bright cyan`V`bright blue`)iew Scores\r\n");
		sprintf(str,"\n\r     `bright red`Q`bright blue`)uit to `bright cyan`%s\r\n", od_control.system_name);
		od_printf(str);
		od_printf("\r\n     `Bright Yellow`Which: ``");
		ShowUnreg();

		switch(od_get_answer("CILPRSTVQ|!"))
		{
			case 'C':
			{
				ListBBSs();
				gac_pause();
				break;
			}
			case 'I':
			{
				more = TRUE;
				while (more == TRUE)
				{
					// Place a cool ANSI next to the menu. if the user has ANSI
					od_clr_scr();
					g_send_file("gac_fc_9");
					if (od_control.user_ansi) od_printf("\n\r");

					od_printf("     `bright cyan`I`bright blue`)nformation on %s\r\n", od_control.od_prog_name);
					od_printf("     `bright cyan`L`bright blue`)ist Instructions\r\n");
					od_printf("     `bright cyan`R`bright blue`)ules of the Tournament\r\n");
					// od_printf("     `bright cyan`T`bright blue`)oggle Card Symbols\r\n");
					od_printf("\n\r     `bright red`Q`bright blue`)uit\r\n");
					od_printf("\r\n     `Bright Yellow`Which [`bright white`Q`bright yellow`]: ``");
					ShowUnreg();

					switch(od_get_answer("ILRTQ\r"))
					{
						case 'I':
							od_clr_scr();
							Information();
							gac_pause();
							break;
						case 'L':
							od_clr_scr();
							od_printf("`Bright Cyan`");
							od_send_file(ibbsgametitle);
							gac_pause();
							break;
						case 'R':
							od_clr_scr();
							g_send_file("gac_fc_7");
							if (od_control.user_ansi) od_printf("\n\r\n\r");
							else od_printf("`Bright cyan`The following are in effect for this Tournament:\n\r\n\r");
							od_printf("`Cyan`\r\n          Tournament Length : `Bright Cyan`%d days",tourneylength);
							od_printf("`Cyan`\r\n          Current Day       : `Bright Cyan`%d", currentday);
							od_printf("`Cyan`\r\n          Maximum Games/Day : `Bright Cyan`%d",maxGames);
							gac_pause();
							break;
						/*
						case 'T':
							symbols=!symbols;
							od_printf("`Bright White`\r\nCard symbols now: `Cyan`%s\r\n",symbols ? "ON":"OFF");
							gac_pause();
							break;
						*/
						default :
							more = FALSE;
							break;
					}
				}
				break;
			}
			case 'P':
				od_clr_scr();
				FreeCell();
				WritePlayerInfo(&player, player.account, TRUE);
				break;
			case 'L':
				ShowPlayersList(ALL);
				gac_pause();
				break;
			case 'S':
				SendIBBSMsg(FALSE);
				break;
			case 'V':
				ViewScores();
				break;
			case 'Q':
				od_clr_scr();
				// Place a cool ANSI next to the menu. if the user has ANSI
				g_send_file("gac_fc_8");
				if (od_control.user_ansi || od_control.user_rip)
				{
#ifdef KEEP_REG
					if (registered == RK_REGISTERED)
					{
						od_set_cursor(8,6);
						od_printf("`bright cyan`REGISTERED `cyan`Thank you for playing and registering `Bright cyan`%s\n\r", od_control.od_prog_name);
					}
					else
					{
						od_set_cursor(8,(16 - strlen(od_control.sysop_name)/2));
						od_printf("`Bright red`UNREGISTERED!!!`cyan` Please encourage `Bright cyan`%s `cyan` to register!", od_control.sysop_name);
						od_sleep(3000);
					}
#else
					od_printf("`bright cyan`FREEWARE `cyan`Thank you for playing `Bright cyan`%s\n\r", od_control.od_prog_name);
#endif
					 od_set_cursor(10,0);
					 gac_pause();
				}
				else
				{
					od_printf("\n\r\n\r");
#ifdef KEEP_REG
					if (registered == RK_REGISTERED)
						od_printf("`bright cyan`REGISTERED `cyan`Thank you for playing and registering `Bright cyan`%s\n\r", od_control.od_prog_name);
					else
						od_printf("`Bright red`UNREGISTERED!!!`cyan` Please encourage `Bright cyan`%s `cyan` to register!", od_control.sysop_name);
#else
					od_printf("`bright cyan`FREEWARE `cyan`Thank you for playing `Bright cyan`%s\n\r", od_control.od_prog_name);
#endif
					od_sleep(3000);
				}
				return;
			}
		}
}

#define PRIVATE_NUM_KEYWORDS       2
// This programs config options
#define KEYWORD_MAX_GAMES    0
#define KEYWORD_LENGTH  1

char *apszPrivateKeyWord[PRIVATE_NUM_KEYWORDS] = {
					// These can be changed for each application
					"Max_Games",
					 "Length",
				   };

tIBResult PrivateReadConfig(tIBInfo *pInfo, char *pszConfigFile)
   {
   // Set defaults for our variables (if desired)
   
   // Process configuration file 
   if(!ProcessConfigFile(pszConfigFile, PRIVATE_NUM_KEYWORDS, apszPrivateKeyWord,
			 ProcessPrivateConfigLine, (void *)pInfo))
	  {
	  return(eFileOpenError);
	  }
   
   // else 
   return(eSuccess);
   }

void ProcessPrivateConfigLine(INT16 nKeyword, char *pszParameter, void *pCallbackData)
   {
   tIBInfo *pInfo = (tIBInfo *)pCallbackData;
   tOtherNode *paNewNodeArray;

   switch(nKeyword)
	  {
	//   This programs specific config options, these will override the .CFG
	  case KEYWORD_MAX_GAMES:
	 maxGames=atoi(pszParameter);            
	 break;

	  case KEYWORD_LENGTH:
	 tourneylength = atoi(pszParameter);          
	 break;


	  }
   }

#ifdef __BIG_ENDIAN__
void player_le(struct player *pl)
{
	pl->bbs=LE_SHORT(pl->bbs);
	pl->laston=LE_SHORT(pl->laston);
	pl->money=LE_LONG(pl->money);
	pl->account=LE_SHORT(pl->account);
	pl->gamesWon=LE_SHORT(pl->gamesWon);
	pl->games=LE_SHORT(pl->games);
	pl->gamesToday=LE_SHORT(pl->gamesToday);
}
#endif

void player_write(struct player *pl, FILE *f)
{
	player_le(pl);
	fwrite(pl->names, sizeof(pl->names), 1, f);
	fwrite(pl->real_names, sizeof(pl->real_names), 1, f);
	fwrite(&(pl->bbs), sizeof(pl->bbs), 1, f);
	fwrite(&(pl->laston), sizeof(pl->laston), 1, f);
	fwrite(&(pl->money), sizeof(pl->money), 1, f);
	fwrite(&(pl->account), sizeof(pl->account), 1, f);
	fwrite(pl->column, sizeof(pl->column), 1, f);
	fwrite(pl->freeCell, sizeof(pl->freeCell), 1, f);
	fwrite(pl->homeCell, sizeof(pl->homeCell), 1, f);
	fwrite(&(pl->gamesWon), sizeof(pl->gamesWon), 1, f);
	fwrite(&(pl->games), sizeof(pl->games), 1, f);
	fwrite(&(pl->gamesToday), sizeof(pl->gamesToday), 1, f);
	fwrite(&(pl->savedGame), sizeof(pl->savedGame), 1, f);
	fwrite(pl->unused, sizeof(pl->unused), 1, f);
	player_le(pl);
}

void player_read(struct player *pl, FILE *f)
{
	fread(pl->names, sizeof(pl->names), 1, f);
	fread(pl->real_names, sizeof(pl->real_names), 1, f);
	fread(&(pl->bbs), sizeof(pl->bbs), 1, f);
	fread(&(pl->laston), sizeof(pl->laston), 1, f);
	fread(&(pl->money), sizeof(pl->money), 1, f);
	fread(&(pl->account), sizeof(pl->account), 1, f);
	fread(pl->column, sizeof(pl->column), 1, f);
	fread(pl->freeCell, sizeof(pl->freeCell), 1, f);
	fread(pl->homeCell, sizeof(pl->homeCell), 1, f);
	fread(&(pl->gamesWon), sizeof(pl->gamesWon), 1, f);
	fread(&(pl->games), sizeof(pl->games), 1, f);
	fread(&(pl->gamesToday), sizeof(pl->gamesToday), 1, f);
	fread(&(pl->savedGame), sizeof(pl->savedGame), 1, f);
	fread(pl->unused, sizeof(pl->unused), 1, f);
	player_le(pl);
}
