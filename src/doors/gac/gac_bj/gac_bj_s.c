/* This file contains the functions that are used in every game, but must
   be modified for each game.
*/
#define EXTDRIVER
#include "gac_bj.h"
#undef EXTDRIVER

extern char    ShoeStatus[];
// extern uchar   symbols;

void SetProgVariables(void)
{

	strcpy(od_control.od_prog_name, "Tournament Blackjack v3.0");
	strcpy(od_control.od_prog_version, "Version 3.0");
	strcpy(od_control.od_prog_copyright, "Copyright 2005 G.A.C./Vagabond Software");
	strcpy(currentversion, "3.0");
	// ibbsgame title can ONLY be 6 characters long!!!
	strcpy(ibbsgametitle, "gac_bj");
	strcpy(ibbsid, "bj");
	// Set the validation code for this Game
	strcpy(valcode, "2KZ5RBAB8Y" );
	// Set the executable file name
	#ifdef ODPLAT_NIX
	strcpy(executable, "gac_bj");
	#else
	strcpy(executable, "GAC_BJ.EXE");
	#endif
	return;
}



// Configuration for parameters unique to this game
void PrivateConfig(char *keyword, char *options)
{

	// This games parameters
	if(stricmp(keyword, "TOTAL_DECKS") == 0)
	{
		total_decks=sys_decks=atoi(options);
	}
	else if(stricmp(keyword, "MIN_BET") == 0)
	{
		min_bet = atoi(options);
	}
	else if(stricmp(keyword, "MAX_BET") == 0)
	{
		max_bet = atoi(options);
	}
	else if(stricmp(keyword, "DEFAULT_BET") == 0)
	{
		ibet = atoi(options);
	}
	else if(stricmp(keyword, "START_money") == 0)
	{
		startmoney = atoi(options);
	}
	else if(stricmp(keyword, "Daily_money") == 0)
	{
		dailymoney = atoi(options);
	}

}



// ===========================================================================
// This part of the program will create the config files and setup the game
// All pretty format and very easy to use
// ===========================================================================
void Config( void )
{
	char chMenuChoice, logging, Crash, Hold, EraseOnSend;
		//, EraseOnReceive, IgnoreZonePoint;
	char sysop[85], bbs[85], reg[25], bbsdir[129], disable[25], tot_nodes[5], tot_decks[5], min_bet[15],
		max_bet[15], default_bet[15], start_money[15], daily_money[15], tourlength[6], leagueid[4];
		//, bbsnumber[6],
	char thisaddress[16], netmail[128], temp[10], inbound[128];
	FILE *config;
	INT16 more = TRUE;

	//Init the strings
	sysop[0]='\0';
	bbs[0]='\0';
	reg[0]='\0';
	bbsdir[0]='\0';
	disable[0]='\0';
	tot_nodes[0]='\0';
	tot_decks[0]='\0';
	min_bet[0]='\0';
	max_bet[0]='\0';
	default_bet[0]='\0';
	start_money[0]='\0';
	daily_money[0]='\0';
	tourlength[0]='\0';
	leagueid[0]='\0';
	// bbsnumber[0]='\0';
	thisaddress[0]='\0';
	netmail[0]='\0';
	inbound[0]='\0';

	chMenuChoice = 'Y';
	logging = 'Y';
	Crash = 'N';
	Hold = 'N';
	EraseOnSend = 'Y';
	// EraseOnReceive = 'Y';
	// IgnoreZonePoint = 'Y';


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
	g_clr_scr();

	od_control.od_help_text2 = (char *) "Copyright Vagabond Software  - [F1]=User Info [F10]=Disable Status Line";
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
		fscanf( config, "%*[^ ] %[^\n]\n", tot_nodes);
		fscanf( config, "%*[^ ] %[^\n]\n", tot_decks);
		fscanf( config, "%*[^ ] %[^\n]\n", min_bet);
		fscanf( config, "%*[^ ] %[^\n]\n", max_bet);
		fscanf( config, "%*[^ ] %[^\n]\n", default_bet);
		fscanf( config, "%*[^ ] %[^\n]\n", start_money);
		fscanf( config, "%*[^ ] %[^\n]\n", daily_money);
		fscanf( config, "%*[^ ] %[^\n]\n", tourlength);
		fscanf( config, "%*[^ ] %[^\n]\n", leagueid);
		fscanf( config, "%*[^ ] %*[^\n]\n"); // skip BBS number
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

		fscanf( config, "%*[^ ] %[^\n]\n", temp); // skip eraseonrec

		fscanf( config, "%*[^ ] %*[^\n]\n", temp); // skip zonepoint

		fscanf( config, "%*[^ ] %[^\n]\n", inbound);
		fclose(config);
	}
	else
	{
		strcpy(sysop, "No Name");
		strcpy(bbs, "No Board");
		strcpy(reg, "0000000");
		strcpy(bbsdir, "C:\\BBS\\");
		strcpy(tot_nodes, "2");
		strcpy(tot_decks, "10");
		strcpy(min_bet, "100");
		strcpy(max_bet, "10000");
		strcpy(default_bet, "200");
		strcpy(start_money, "10000");
		strcpy(daily_money, "500");
		strcpy(tourlength, "0");
		strcpy(leagueid, "000");
		// strcpy(bbsnumber, "0");
		strcpy(thisaddress, "1:1/0");
		strcpy(netmail, "C:\\FD\\NETMAIL\\");
		strcpy(inbound, "C:\\FD\\inbound\\");

		logging = 'Y';

		Crash = 'N';
		Hold = 'N';
		EraseOnSend = 'Y';
		// EraseOnReceive = 'Y';
		// IgnoreZonePoint = 'Y';
	}

	// Show the config screen, get user info etc...
	 //for(;;)
	 while (more == TRUE)
	 {
			//  Clear the screen */
			g_clr_scr();
			//  Display main menu. */
	 od_printf("`bright white`                    %s `dark cyan`Configuration\n\r", od_control.od_prog_name);
	 // od_printf("`dark cyan`Programmed by `bright cyan`Gregory Campbell\n\r");
	 od_printf("`dark cyan`                  %s\n\r", od_control.od_prog_copyright);
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
	 //od_printf("`dark cyan`  [`bright cyan`E`dark cyan`] %s Directory: `bright cyan`%s\n\r", od_control.od_prog_name, doordir);
	 od_printf("`dark cyan`  [`bright cyan`F`dark cyan`] %-30s`bright cyan`%s\n\r", "Use Logging: ", ( (logging=='Y' ) ? "Yes":"No"));
	 od_printf("`dark cyan`  [`bright cyan`G`dark cyan`] %-30s`bright cyan`%s\n\r", "Total Number of Nodes: ", tot_nodes);
	 od_printf("`dark cyan`  [`bright cyan`H`dark cyan`] %-30s`bright cyan`%s\n\r", "Total Number of Decks: ", tot_decks);
	 od_printf("`dark cyan`  [`bright cyan`I`dark cyan`] %-30s`bright cyan`%s\n\r", "Minimum Bet: ", min_bet);
	 od_printf("`dark cyan`  [`bright cyan`J`dark cyan`] %-30s`bright cyan`%s\n\r", "Maximum Bet: ", max_bet);
	 od_printf("`dark cyan`  [`bright cyan`K`dark cyan`] %-30s`bright cyan`%s\n\r", "Default Bet: ", default_bet);
	 od_printf("`dark cyan`  [`bright cyan`L`dark cyan`] %-30s`bright cyan`%s\n\r", "Starting Money: ", start_money);
	 od_printf("`dark cyan`  [`bright cyan`M`dark cyan`] %-30s`bright cyan`%s\n\r", "Daily Money: ", daily_money);
	 od_printf("`dark cyan`  [`bright cyan`N`dark cyan`] %-30s`bright cyan`%s\n\r", "Tournament Length: ", tourlength);
	 od_printf("`dark cyan`  [`bright cyan`O`dark cyan`] %-30s`bright cyan`%s\n\r", "Inter-BBS League ID: ", leagueid);
	 // od_printf("`dark cyan`  [`bright cyan`P`dark cyan`] %-30s`bright cyan`%s\n\r", "This BBS's Inter-BBS Number: ", bbsnumber);
	 od_printf("`dark cyan`  [`bright cyan`P`dark cyan`] %-30s`bright cyan`%s\n\r", "NetMail Address: ", thisaddress);
	 od_printf("`dark cyan`  [`bright cyan`R`dark cyan`] %-30s`bright cyan`%-40.40s\n\r", "Inbound File Directory: ", inbound);
	 od_printf("`dark cyan`  [`bright cyan`S`dark cyan`] %-30s`bright cyan`%-40.40s\n\r", "NetMail Directory (*.msg): ", netmail);
	 od_printf("`dark cyan`  [`bright cyan`T`dark cyan`] Advanced Inter-BBS Options\n\r");

	 od_printf("`dark cyan`  [`bright cyan`Q`dark cyan`] Quit Configuration\n\r");
	 od_printf("`bright white`Press the key corresponding to the option of your choice: `dark cyan`" );
	 //  Get the user's choice from the main menu. This choice may only be */
	 chMenuChoice = od_get_answer("ABCDFGHIJKLMNOPQRST");

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

	 case 'F':
		 if (logging == 'Y')
			logging = 'N';
		 else
			logging = 'Y';
		 break;

	 case 'G':
			od_printf("`bright cyan`Enter the Total Number of Nodes: `bright white`");
						od_input_str(tot_nodes, 4, 48, 57);
			break;
	 case 'H':
			od_printf("`bright cyan`Enter the Total Number of Decks of Cards to Use: `bright white`");
						od_input_str(tot_decks, 4, 48, 57);
			break;
	 case 'I':
			od_printf("`bright cyan`Enter the Mimimum Bet a Player Can Make: `bright white`");
						od_input_str(min_bet, 14, 48, 57);
			break;
	 case 'J':
			od_printf("`bright cyan`Enter the Maximum Bet a Player Can Make: `bright white`");
						od_input_str(max_bet, 14, 48, 57);
			break;
	 case 'K':
			od_printf("`bright cyan`Enter the Default Bet for the Player to Make: `bright white`");
						od_input_str(default_bet, 14, 48, 57);
			break;
	 case 'L':
			od_printf("`bright cyan`Enter the Amount of dollars the player starts with (<=30,000): `bright white`");
				od_set_cursor(15,7);
						od_input_str(start_money, 14, 48, 57);
			break;
	 case 'M':
			od_printf("`bright cyan`Enter the Amount of dollars the player gets each day (<=30,000): `bright white`");
				od_set_cursor(15,7);
						od_input_str(daily_money, 14, 48, 57);
			break;
	 case 'N':
			od_printf("`bright cyan`Enter the length of the Tournament in days (0 disables reset): `bright white`");
						od_input_str(tourlength, 5, 48, 57);
			break;
	 case 'O':
			od_printf("`bright cyan`Enter the Inter-BBS League ID: `bright white`");
						od_input_str(leagueid, 3, 32, 127);
			break;
	 case 'P':
			od_printf("`bright cyan`Enter your BBS Fidonet Address: `bright white`");
						od_input_str(thisaddress, 15, 32, 127);
			break;
	 case 'R':
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
	 case 'S':
			od_printf("`bright cyan`Enter the path to your netmail directory: `bright white`");
						if(od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
						{
							// Only for ANSI, RIP or AVATAR
							od_set_cursor(15,7);
						}
						else
							od_printf("\n\r");

						od_input_str(netmail, 127, 32, 127);
						backslash(netmail);

			break;

	 case 'T':


		more = TRUE;
		// Show the advanced Inter-BBS config screen
		while (more == TRUE)
		 {
				//  Clear the screen */
				g_clr_scr();
				//  Display main menu. */
				od_printf("`bright cyan`                    %s `cyan`Configuration\n\r", od_control.od_prog_name);
			 od_printf("`dark cyan`                 %s\n\r", od_control.od_prog_copyright);
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
		 // od_printf("`dark cyan`  [`bright cyan`D`dark cyan`] %-35s`bright cyan`%s\n\r", "Erase Messages After Importing: ", ( (EraseOnReceive=='Y' ) ? "Yes":"No"));
		 // od_printf("`dark cyan`  [`bright cyan`E`dark cyan`] %-50s`bright cyan`%s\n\r\n\r", "Ignore the Zone and Point Fields for greatest compatibility: ", ( (IgnoreZonePoint=='Y' ) ? "Yes":"No"));

		 od_printf("`dark cyan`  [`bright cyan`Q`dark cyan`] Quit Back to Main Configuration\n\r\n\r");
		 od_printf("`bright white`Press the key corresponding to the option of your choice: `dark cyan`" );
		 //  Get the user's choice from the main menu. This choice may only be */
		 chMenuChoice = od_get_answer("ABCDEQ");

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
				/*
				case 'D':
					if (EraseOnReceive == 'Y')
						EraseOnReceive = 'N';
					else
						EraseOnReceive = 'Y';
					break;

				case 'E':
					if (IgnoreZonePoint == 'Y')
						IgnoreZonePoint = 'N';
					else
						IgnoreZonePoint = 'Y';
					break;
				*/
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
		fprintf(config, "Total_Nodes %s\n", tot_nodes);
		fprintf(config, "Total_Decks %s\n", tot_decks);
		fprintf(config, "Min_Bet %s\n", min_bet);
		fprintf(config, "Max_Bet %s\n", max_bet);
		fprintf(config, "Default_Bet %s\n", default_bet);
		fprintf(config, "Start_money %s\n", start_money);
		fprintf(config, "Daily_money %s\n", daily_money);
		fprintf(config, "Length %s\n", tourlength);
		fprintf(config, "League %s\n", leagueid);
		fprintf(config, "NOTUSED NOTUSED\n");
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

		// if (EraseOnReceive == 'N')
			fprintf(config, "NOTUSED NOTUSED\n");

		// if (IgnoreZonePoint == 'N')
			fprintf(config, "NOTUSED NOTUSED\n");

		fprintf(config, "InboundDir %s\n", inbound);
				fclose(config);
			}
			more = FALSE;
			break;

			}
	 }
	return;
}

// this game's unique initialization
void InitGamePlay( void )
{
	
	symbols=1;
	node_num = od_control.od_node;
	// Set the user_number = account;
	user_number = player.account + 1;
	// Set com_port for use here
	com_port = od_control.port + 1;
	
	// Check for correct configuration options ============================
	if(!total_decks || total_decks>MAX_DECKS)
	{
		od_printf("Invalid number of decks in gac_bj.cfg\r\n");
		od_get_key(TRUE); //Pause
		od_exit(1,FALSE);
	}
	if(!max_bet)
	{
		od_printf("Invalid max bet in gac_bj.cfg\r\n");
		od_get_key(TRUE); //Pause
		od_exit(1,FALSE);
	}
	if(min_bet>max_bet)
	{
		od_printf("Invalid min bet in gac_bj.cfg\r\n");
		od_get_key(TRUE); //Pause
		od_exit(1,FALSE);
	}
	if(ibet>max_bet || ibet<min_bet)
	{
		od_printf("Invalid default bet in gac_bj.cfg\r\n");
		od_get_key(TRUE); //Pause
		od_exit(1,FALSE);
	}

	// Game Startup Procedure ============================================
	// check for the card data file
	if(access("gac_bj.crd", 00) != 0)
	{
		cur_card=0;
		dc=0;
		memset(dealer,0,sizeof(dealer));
		memset(card,0,sizeof(card));
		putcarddat();
	}
	else
	{
		getcarddat();
		if(total_decks!=sys_decks)
		{
			remove("gac_bj.crd");
			total_decks=sys_decks;
			putcarddat();
		}
	}

	// check for the data file
	if(access("gac_bj.dat", 00) != 0)
	{         // File's not there
		create_gamedab();
	}

	open_gamedab();

	getgamedat(0);
	// check if total nodes changed
	if(total_nodes!=sys_nodes)
	{
		close(gamedab);
		total_nodes=sys_nodes;
		create_gamedab();
		open_gamedab();
	}



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
	if (refused == FALSE)        
	{
		// Make sure the player is logged out of the game...
		getgamedat(1);
		node[node_num-1]=0;
		putgamedat();
	}

	return;
}

// This game's unique player initialization
void PrivatePlayerInit( void )    
{
	player.money =   (ulong) startmoney;
	player.investment = 0L;
	player.inv_start = 0;
	return;
}

// This game's unique player update (daily)
void PrivatePlayerUpdate( void)
{
	// check to see if they have gotten their extra money today
	if ((dailymoney != 0L) && (player.laston < currentday)) player.money += dailymoney;
	return;
}



// ===========================================================================
// The main menu
void Title( void )
{
	char str[120];
	INT16 more = TRUE;

	g_clr_scr();
	od_printf("`CYAN`%s\r\n", od_control.od_prog_copyright);
	od_printf("`Cyan`%s\r\n", od_control.od_prog_name);

	getgamedat(1);
	node[node_num-1]=0;
	putgamedat();


	while(1)
	{

	// Check to see if the user has any mail waiting.
	CheckMail();

		g_clr_scr();
		g_send_file("gac_1");
		if (od_control.user_ansi) od_set_cursor(1,1);

		od_printf("`CYAN`%s\r\n", od_control.od_prog_copyright);
		od_printf("`Cyan`%s\r\n\n\r", od_control.od_prog_name);
		od_printf("     `bright cyan`C`bright blue`)ompeting BBSs\r\n");
		od_printf("     `bright cyan`I`bright blue`)nformation and Instructions\r\n");
		od_printf("     `bright cyan`J`bright blue`)oin the Game\r\n");
		od_printf("     `bright cyan`L`bright blue`)ist Tournament Players\r\n");
		od_printf("     `bright cyan`P`bright blue`)layers in On-Line Game\r\n");
		od_printf("     `bright cyan`S`bright blue`)end a message\r\n");
		od_printf("     `bright cyan`V`bright blue`)iew Scores\r\n");
		sprintf(str,"\n\r     `bright red`Q`bright blue`)uit to `bright cyan`%s\r\n", od_control.system_name);
		od_printf(str);
		od_printf("\r\n     `Bright Yellow`Which: ``");
		ShowUnreg();

		switch(od_get_answer("CIJLPRSTVQ|!"))
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
					g_clr_scr();
					g_send_file("gac_9");
					if (od_control.user_ansi) od_printf("\n\r");

					od_printf("     `bright cyan`I`bright blue`)nformation on %s\r\n", od_control.od_prog_name);
					od_printf("     `bright cyan`L`bright blue`)ist Instructions\r\n");
					od_printf("     `bright cyan`R`bright blue`)ules of the Tournament\r\n");
					od_printf("     `bright cyan`T`bright blue`)oggle Card Symbols\r\n");
					od_printf("\n\r     `bright red`Q`bright blue`)uit\r\n");
					od_printf("\r\n     `Bright Yellow`Which [`bright white`Q`bright yellow`]: ``");
					ShowUnreg();

					switch(od_get_answer("ILRTQ\r"))
					{
						case 'I':
							g_clr_scr();
							Information();
							gac_pause();
							break;
						case 'L':
							g_clr_scr();
							od_printf("`Bright Cyan`");
							g_send_file(ibbsgametitle); // 12/96
							gac_pause();
							break;
						case 'R':
							g_clr_scr();
							g_send_file("gac_7");
							if (od_control.user_ansi) od_printf("\n\r\n\r");
							else od_printf("`Bright cyan`The following are in effect for this Tournament:\n\r\n\r");
							od_printf("`Cyan`\r\n          Tournament Length : `Bright Cyan`%d days",tourneylength);
							od_printf("`Cyan`\r\n          Current Day       : `Bright Cyan`%d", currentday);
							od_printf("`Cyan`\r\n          Minimum bet       : $`Bright Cyan`%u",min_bet);
							od_printf("`Cyan`\r\n          Maximum bet       : $`Bright Cyan`%u",max_bet);
							od_printf("`Cyan`\r\n          Starting Money    : $`Bright Cyan`%u", startmoney);
							od_printf("`Cyan`\r\n          Daily Money Added : $`Bright Cyan`%u", dailymoney);
							od_printf("`Cyan`\r\n          Card decks in shoe: `Bright white`%u\r\n\n\r",sys_decks);
							gac_pause();
							break;
						case 'T':
							symbols=!symbols;
							od_printf("`Bright White`\r\nCard symbols now: `Cyan`%s\r\n",symbols ? "ON":"OFF");
							gac_pause();
							break;
						default :
							more = FALSE;
							break;
					}
				}
				break;
			}
			case 'J':
				g_clr_scr();
				play();
				WritePlayerInfo(&player, player.account, TRUE);
				break;
			case 'L':
				ShowPlayersList(ALL);
				gac_pause();
				break;
			case 'P':
				g_clr_scr();
				g_send_file("gac_5");
				if (od_control.user_ansi) od_printf("\n\r");
				list_bj_players();
				od_printf(ShoeStatus,cur_card,total_decks*52);
				gac_pause();
				break;
			case 'S':
				SendIBBSMsg(FALSE);
				break;
			case 'V':
				ViewScores();
				break;
			case 'Q':
				g_clr_scr();
				// Place a cool ANSI next to the menu. if the user has ANSI
				g_send_file("gac_8");
				if (od_control.user_ansi || od_control.user_rip)
				{
#ifdef KEEP_REG
					if (registered == RK_REGISTERED)
					{
						od_set_cursor(8,6);
						od_printf("`bright cyan`REGISTERED `cyan`Thank you for playing and registering `Bright cyan`Tournament Blackjack");
					}
					else
					{
						od_set_cursor(8,(16 - strlen(od_control.sysop_name)/2));
						od_printf("`Bright red`UNREGISTERED!!!`cyan` Please encourage `Bright cyan`%s `cyan` to register!", od_control.sysop_name);
						od_sleep(3000);
					}
#else
					od_printf("`bright cyan`FREEWARE `cyan`Thank you for playing `Bright cyan`Tournament Blackjack");
#endif
					 od_set_cursor(10,0);
					 gac_pause();
				}
				else
				{
					od_printf("\n\r\n\r");
#ifdef KEEP_REG
					if (registered == RK_REGISTERED)
						od_printf("`bright cyan`REGISTERED `cyan`Thank you for playing and registering `Bright cyan`Tournament Blackjack");
					else
						od_printf("`Bright red`UNREGISTERED!!!`cyan` Please encourage `Bright cyan`%s `cyan` to register!", od_control.sysop_name);
#else
					od_printf("`bright cyan`FREEWARE `cyan`Thank you for playing `Bright cyan`Tournament Blackjack");
#endif
					od_sleep(3000);
				}
				return;
			}
		}
}


#define PRIVATE_NUM_KEYWORDS       7
// This programs config options
#define KEYWORD_TOTAL_DECKS    0
#define KEYWORD_MIN_BET  1
#define KEYWORD_MAX_BET   2
#define KEYWORD_DEFAULT_BET  3
#define KEYWORD_START_MONEY   4
#define KEYWORD_DAILY_MONEY  5
#define KEYWORD_LENGTH  6

char *apszPrivateKeyWord[PRIVATE_NUM_KEYWORDS] = {
					// These can be changed for each application
					"Total_Decks",
					"Min_bet",
					"Max_bet",
					"Default_bet",
					"Start_Money",
					"Daily_Money",
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
   switch(nKeyword)
	  {
	//   This programs specific config options, these will override the .CFG

	  case KEYWORD_TOTAL_DECKS:
	 total_decks=sys_decks=atoi(pszParameter);            
	 break;

	  case KEYWORD_MIN_BET:
	 min_bet = atoi(pszParameter);                
	 break;

	  case KEYWORD_MAX_BET:
	 max_bet = atoi(pszParameter);                
	 break;

	  case KEYWORD_DEFAULT_BET:
	 ibet = atoi(pszParameter);                
	 break;
		
	  case KEYWORD_START_MONEY:
	 startmoney = atoi(pszParameter);
	 break;

	  case KEYWORD_DAILY_MONEY:
	 dailymoney = atoi(pszParameter);             
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
	pl->investment=LE_LONG(pl->investment);
	pl->inv_start=LE_SHORT(pl->inv_start);
	pl->account=LE_SHORT(pl->account);
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
	fwrite(&(pl->investment), sizeof(pl->investment), 1, f);
	fwrite(&(pl->inv_start), sizeof(pl->inv_start), 1, f);
	fwrite(&(pl->account), sizeof(pl->account), 1, f);
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
	fread(&(pl->investment), sizeof(pl->investment), 1, f);
	fread(&(pl->inv_start), sizeof(pl->inv_start), 1, f);
	fread(&(pl->account), sizeof(pl->account), 1, f);
	fread(pl->unused, sizeof(pl->unused), 1, f);
	player_le(pl);
}

