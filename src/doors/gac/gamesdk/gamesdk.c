#include "gamesdk.h"
/* This file includes all of the functions which are used UNMODIFIED in the
   smaller Inter-BBS games (BJ, FreeCell, etc...)

   // 1/97 Modified code to allow compile and running of a Win_32 version!
   // need to use command line parameters...
*/


/* The main() or WinMain() function: program execution begins here. */
#ifdef ODPLAT_WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpszCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
//void main(int argc, char **argv)
{
	char filename[128];
	char temp[10];
	int online;

	// moved up to the top 1/97
	#ifdef ODPLAT_WIN32
		od_control.od_cmd_show = nCmdShow;
		(void)hInstance; // ignore unused parameters
		(void)hPrevInstance;
	#endif
	
	#ifdef GAC_DEBUG
	strcpy(gac_debugfile, "gac_bugs.log");
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "***  Entering Program ***\n");
	fclose(gac_debug);
	#endif

	// set the stack size if compiling for DOS
	#ifdef ODPLAT_DOS
	_stklen = 64000;
	#endif

	// Set program's name, to be written to the OpenDoors log file
	// 12/96
	SetProgVariables();

	// LEAVE THESE ALONE =======================================================
	// Ignore Copyright
	od_control.od_nocopyright = TRUE;
	// Registration Information!
	strcpy(od_registered_to, "Gregory Campbell");
	od_registration_key = 583073792L;
	
	// Set a variable to the doors path
	#ifndef ODPLAT_WIN32
	strcpy(doorpath, "");
	strncat(doorpath, argv[0], (strlen(argv[0]) - strlen(executable)) );
	#else
	doorpath[0]=0;
	doorpath[1]=PATH_DELIM;
	doorpath[2]=0;
	#endif
	
	// Set the config file name
	#ifndef ODPLAT_WIN32
	strcpy(configfile, "");
	#ifdef ODPLAT_NIX
	strncat(configfile, argv[0], (strlen(argv[0])) );
	strcat(configfile, ".");
	#else
	strncat(configfile, argv[0], (strlen(argv[0]) - 3) );
	#endif
	strcat(configfile, "cfg");
	#else
	strcpy(configfile, "");
	strncat(configfile, executable, (strlen(executable) - 3) );
	strcat(configfile, "CFG");
	#endif

	od_control.od_config_filename = configfile;

	// 12/96 Set some new options for OpenDoors 6.0
	#ifdef ODPLAT_WIN32
		od_parse_cmd_line(lpszCmdLine);
	#else
		od_parse_cmd_line(argc, argv);
	#endif
	
	// Set the name of the log file
	#ifndef ODPLAT_WIN32
	strcpy(od_control.od_logfile_name, "");
	//strncat(od_control.od_logfile_name, argv[0], (strlen(argv[0]) - 7) );
	strcat(od_control.od_logfile_name, "gac" );
	sprintf(temp, "%d", od_control.od_node);
	strcat(od_control.od_logfile_name, temp);
	strcat(od_control.od_logfile_name, ".log");
	#else
	strcpy(od_control.od_logfile_name, "");
	strncat(od_control.od_logfile_name, executable, (strlen(executable) - 7) );
	sprintf(temp, "%d", od_control.od_node);
	strcat(od_control.od_logfile_name, temp);
	strcat(od_control.od_logfile_name, ".log");
	#endif

	// check for and create in directory
	strcpy(filename, doorpath);
	strcat(filename, "IN");
	if (access(filename, 00) != 0)
		MKDIR(filename);

	// check for and create in directory
	strcpy(filename, doorpath);
	strcat(filename, "OUT");
	if (access(filename, 00) != 0)
		MKDIR(filename);

	// check for and create in directory
	strcpy(filename, doorpath);
	strcat(filename, "OUTBOUND");
	if (access(filename, 00) != 0)
		MKDIR(filename);

	sprintf(filename, "%sfrombbs.dat", doorpath);
	if (access(filename, 00) == 0) remove(filename);

	// Check to see if any parameters where used ================================
	#ifndef ODPLAT_WIN32
	if (argc == 1)
		command_line();
	#else
	if (stricmp( lpszCmdLine, "") == 0)
		od_parse_cmd_line("/?");
	#endif
	
	#ifndef ODPLAT_WIN32
	else if (argv[2] && stricmp(argv[2], "binkley") == 0)  // 12/96 removed argc == 3 &&
	#else
	else if (strstr(strupr(lpszCmdLine), "BINKLEY") != NULL)
	#endif        
		binkley = TRUE;
	else binkley = FALSE;


	// Check for argv[1] as config
	#ifndef ODPLAT_WIN32
	if (stricmp(argv[1], "config") == 0)
	#else
	if (strstr(strupr(lpszCmdLine), "CONFIG") != NULL)
	#endif        
	{
		// call the config function
		od_control.od_disable|=DIS_NAME_PROMPT;

		Config();
		od_log_write("Configuring Program");
		// Clean up OpenDoors and exit
		od_exit(0, FALSE);
	}

	#ifndef ODPLAT_WIN32
	else if (stricmp(argv[1], "route") == 0)
	#else
	else if (strstr(strupr(lpszCmdLine), "ROUTE") != NULL)
	#endif        
	{
		od_control.od_force_local = TRUE;
		od_control.od_disable|=DIS_NAME_PROMPT;

		// Init the BlackJack game
		InitGame(FALSE);
		InitIBBS();
		od_control.od_clear_on_exit = FALSE;

		if (CheckMaintenance() == TRUE)
			 Maintain();

		MakeRouteReport();
		od_log_write("Creating routing report");
		// Clean up OpenDoors and exit
		od_exit(FALSE, 0);
	}
	#ifndef ODPLAT_WIN32
	else if (stricmp(argv[1], "bulletins") == 0)
	#else
	else if (strstr(strupr(lpszCmdLine), "BULLETINS") != NULL)
	#endif        
	{
		od_control.od_force_local = TRUE;
		od_control.od_disable|=DIS_NAME_PROMPT;

		// Init the BlackJack game
		InitGame(FALSE);
		InitIBBS();
		od_control.od_clear_on_exit = FALSE;

		if (CheckMaintenance() == TRUE)
			  Maintain();

		MakeBulletins(FALSE);
		od_log_write("Creating top 15 bulletins");
		// Clean up OpenDoors and exit
		od_exit(FALSE, 0);
	}
	#ifndef ODPLAT_WIN32
	else if (stricmp(argv[1], "inbound") == 0)
	#else
	else if (strstr(strupr(lpszCmdLine), "INBOUND") != NULL)
	#endif        
	{
		od_control.od_force_local = TRUE;
		od_control.od_disable|=DIS_NAME_PROMPT;

		// Init the BlackJack game
		InitGame(FALSE);
		InitIBBS();
		od_control.od_clear_on_exit = FALSE;

		if (CheckMaintenance() == TRUE)
		{
			  Maintain();
		}

		od_log_write("Receiving inbound information");
		Inbound(FALSE);
		// Clean up OpenDoors and exit
		od_exit(FALSE, 0);
	}
	
	#ifndef ODPLAT_WIN32
	else if (stricmp(argv[1], "outbound") == 0)
	#else
	else if (strstr(strupr(lpszCmdLine), "OUTBOUND") != NULL)
	#endif        
	{
		od_control.od_force_local = TRUE;
		od_control.od_disable|=DIS_NAME_PROMPT;
		// Init the BlackJack game
		InitGame(FALSE);
		InitIBBS();
		od_control.od_clear_on_exit = FALSE;

		if (CheckMaintenance() == TRUE)
			 Maintain();

		od_log_write("Sending Outbound information");
		Outbound(FALSE);
		// Clean up OpenDoors and exit
		od_exit(FALSE, 0);
	}
	#ifndef ODPLAT_WIN32
	else if (stricmp(argv[1], "sendall") == 0)
	#else
	else if (strstr(strupr(lpszCmdLine), "SENDALL") != NULL)
	#endif        
	{
		od_control.od_force_local = TRUE;
		od_control.od_disable|=DIS_NAME_PROMPT;
		// Init the BlackJack game
		InitGame(FALSE);
		InitIBBS();
		od_control.od_clear_on_exit = TRUE;
		SendAll();
		// Clean up OpenDoors and exit
		od_exit(FALSE, 0);
	}
	#ifndef ODPLAT_WIN32
	else if (stricmp(argv[1], "maintain") == 0)
	#else
	else if (strstr(strupr(lpszCmdLine), "MAINTAIN") != NULL)
	#endif
	{
		od_control.od_force_local = TRUE;
		od_control.od_disable|=DIS_NAME_PROMPT;
		// Init the BlackJack game
		InitGame(FALSE);
		InitIBBS();
		od_control.od_clear_on_exit = FALSE;
		od_log_write("Running Maintenance");
		Maintain();
		// Clean up OpenDoors and exit
		od_exit(FALSE, 0);
	}
	#ifndef ODPLAT_WIN32
	else if (stricmp(argv[1], "reset") == 0)
	#else
	else if (strstr(strupr(lpszCmdLine), "RESET") != NULL)
	#endif        
	{
		od_control.od_force_local = TRUE;
		od_control.od_disable|=DIS_NAME_PROMPT;
		// Init the BlackJack game
		InitGame(FALSE);
		InitIBBS();
		od_control.od_clear_on_exit = FALSE;

		if (CheckMaintenance() == TRUE)
			 Maintain();

		ResetIBBS();
		// Clean up OpenDoors and exit
		od_exit(FALSE, 0);
	}
   // 1/97
	#ifndef ODPLAT_WIN32
	else if (stricmp(argv[1], "delete") == 0)
	#else
	else if (strstr(strupr(lpszCmdLine), "DELETE") != NULL)
	#endif        
	{
		od_control.od_force_local = TRUE;
		od_control.od_disable|=DIS_NAME_PROMPT;
		// Init the BlackJack game
		InitGame(FALSE);
		InitIBBS();
		od_control.od_clear_on_exit = FALSE;

		DeleteIBBSPlayer();
		// Clean up OpenDoors and exit
		od_exit(FALSE, 0);
	}

	// 12/96 moved this down here
	od_control.od_disable|=DIS_NAME_PROMPT;
	// Init the game
	InitGame(TRUE);
	// Get the interbbs information needed.
	InitIBBS();

	// 12/96
	// See if this is a local logon
	if (od_control.od_force_local == TRUE)
	{
		od_clr_scr();
		od_printf("\n\r\n\r`cyan`Running `bright cyan`%s`cyan` in LOCAL mode.\n\r\n\r", od_control.od_prog_name);
		od_printf("`cyan`Please enter your name: `bright cyan`");
		od_input_str(od_control.user_name, 35, 32, 127);
		// 12/96 if blank, exit
		if (stricmp(od_control.user_name, "               ") <=0)
			od_exit(0,FALSE);
	}


	// check if we need to do maintenance.
	if (CheckMaintenance() == TRUE)
	{
		od_printf("Please wait, doing daily maintenance...\n\r");
		Maintain();
	}

	// Need to check for the user's name in the gacplayr.dat file and get the account number...
	CheckPlayer();

	// Need to open a node usage file and write the user's name to the file...
	sprintf(filename, "%sONLINE.%d", doorpath,od_control.od_node);
	if (access(filename, 00) == 0) remove(filename);
	online = nopen(filename, O_RDWR|O_CREAT);
	lseek(online,0L,SEEK_SET);
	write(online,player.names,36);
	close(online);

	// Check to see if the user has any mail waiting.
	CheckMail();

	
	// for BJ?
	/* 12/96
	// Set the user_number = account;
	user_number = player.account + 1;
	// Set com_port for use here
	com_port = od_control.port + 1;
	*/

	// Do routines for starting game play...check files and options
	InitGamePlay();

	// call the main menu
	Title();

	// exit gracefully
	od_exit(0,FALSE);

	return(0);
}

// 1/97
// Allow the sysop to remove a player from their system
void DeleteIBBSPlayer( void )
{
   char response;
   void *instruct_win;
   char input[21];

   g_clr_scr();
 while (1)
 {
	do
	{
		response = PromptBox("`cyan`Do you want to view the list of players (`bright cyan`y`cyan`/`bright cyan`N`cyan`) or `bright cyan`q`cyan`)uit",
		 "", "YNQ\r\n", FALSE);

		if (response == 'Q') return;
		else if (response == 'Y')
		{
			ShowPlayersList(ALL);
			gac_pause();
		}

			// for ANSI users
			if (od_control.user_ansi || od_control.user_rip)
			{
				instruct_win = od_window_create(5,10,75,13,"Make your choice",0x09,0x0b,0x00,0);
				od_set_attrib(0x03);
				od_set_cursor(11,7);
				// Ask for the name of a user or let them list the players
				od_printf("Enter the handle of the person you want to delete:`bright cyan`");
				// od_set_attrib(0x03);
				od_set_cursor(12,7);
				od_input_str(input, 20, 32, 127);
				od_window_remove(instruct_win);
			}
			else
			{
				od_save_screen(screen);
				// Ask for the name of a user or let them list the players
				od_printf("Enter the handle of the person you want to delete:`bright cyan`\n\r");
				od_input_str(input, 20, 32, 127);
				od_restore_screen(screen);
			}

			// Check for that user, if found the info is in TEMP_PLAYER
		} while(CheckPlayerName(input) != 0);

	  sprintf(prompt, "Handle: %s,  Real Name: %s", temp_player.names, temp_player.real_names);
		response = PromptBox(prompt,
		 "`cyan`Do you want to delete this player (`bright cyan`y`cyan`/`bright cyan`N`cyan`)",
		 "YN\r\n", FALSE);

	  if (response == 'Y')
	  {
		 GetPlayerInfo(&opponent, temp_player.account, TRUE);
		 strcpy(opponent.names, "Deleted");
		 WritePlayerInfo(&opponent, temp_player.account, TRUE);
	  }

 }  
  
}


// ===========================================================================
// Get extra information from the config file, basically the registration code
// ===========================================================================
void MyConfig(char *keyword, char *options)
{
	if (stricmp(keyword, "REGISTRATIONKEY") == 0)
	{
		strcpy(regcode, options);
	}
	else if(stricmp(keyword, "TOTAL_NODES") == 0)
	{
		sys_nodes = atoi(options);
	}
	else if(stricmp(keyword, "LENGTH") == 0)
	{
		tourneylength = atoi(options);
	}
	// The interbbs Options
	else if(stricmp(keyword, "LEAGUE") == 0)
	{
		strcpy(league, options);
	}
/* No longer needed...
	else if(stricmp(keyword, "BBSNUMBER") == 0)
	{
		thisbbs = atoi(options);
	}
*/
	else if(stricmp(keyword, "ThisBBSAddress") == 0)
	{
		strcpy(ThisBBSAddress, options);
	}
	else if(stricmp(keyword, "NETMAILDIR") == 0)
	{
		strcpy(netmaildir, options);
	}
	else if(stricmp(keyword, "INBOUNDDIR") == 0)
	{
		strcpy(inbounddir, options);
	}

	else if(stricmp(keyword, "CRASH") == 0)
	{
		if(stricmp(options, "Yes") == 0)
		{
			InterBBSInfo.bCrash = TRUE;
		}
		else if(stricmp(options, "No") == 0)
		{
			InterBBSInfo.bCrash = FALSE;
		}
	}
	else if(stricmp(keyword, "Hold") == 0)
	{
		if(stricmp(options, "Yes") == 0)
		{
			InterBBSInfo.bHold = TRUE;
		}
		else if(stricmp(options, "No") == 0)
		{
			InterBBSInfo.bHold = FALSE;
		}
	}
	else if(stricmp(keyword, "EraseOnSend") == 0)
	{
		if(stricmp(options, "Yes") == 0)
		{
			InterBBSInfo.bEraseOnSend = TRUE;
		}
		else if(stricmp(options, "No") == 0)
		{
			InterBBSInfo.bEraseOnSend = FALSE;
		}
	}
/* No longer used...
	else if(stricmp(keyword, "EraseOnReceive") == 0)
	{
		if(stricmp(options, "Yes") == 0)
		{
			InterBBSInfo.bEraseOnReceive = TRUE;
		}
		else if(stricmp(options, "No") == 0)
		{
			InterBBSInfo.bEraseOnReceive = FALSE;
		}
	}
	else if(stricmp(keyword, "IgnoreZonePoint") == 0)
	{
		if(stricmp(options, "Yes") == 0)
		{
			InterBBSInfo.bIgnoreZonePoint = TRUE;
		}
		else if(stricmp(options, "No") == 0)
		{
			InterBBSInfo.bIgnoreZonePoint = FALSE;
		}
	}
*/

	// call the games individual extra config
	PrivateConfig(keyword, options);

	return;
}


// This function is executed before ANY sort of exit...
void BeforeExitFunction( void)
{
	char filename[128];
	struct player_list *tail;
	struct bbs_list *bbstail;

	// The games unique exit requirements
	PrivateExit();

	// Update the player to the database ONLY if a user is playing the game
	if (refused == FALSE)
	{
		WritePlayerInfo(&player, player.account, TRUE);
	}

	// Need to remove the node usage file
	sprintf(filename, "%sONLINE.%d", doorpath,      od_control.od_node);
	if (access(filename, 00) == 0) remove(filename);
	// Check for and remove a CHATLINE file if there
	sprintf(filename, "%sCHATLINE.%d", doorpath,    od_control.od_node);
	if (access(filename, 00) == 0) remove(filename);

	// Need to free memory for the linked lists

	// Deallocate the memory for the previous list (if any
	// Since we allocated mem, we need to free it
	// Loop to free each link...

	tail = list;
	while ( tail != NULL)
	{
	tail = list->next;
		free(list);
		list = tail;
	}

	// Deallocate the memory for the previous list (if any
	// Since we allocated mem, we need to free it
	// Loop to free each link...
	bbstail = bbslist;
	while ( bbstail != NULL)
	{
	bbstail = bbslist->next;
		free(bbslist);
		bbslist = bbstail;
	}

	// fcloseall();

	return;

}


// ===========================================================================
// This checks to see if the player is in the database, if they are not, then they are added.
// If they are in it on another BBS, then they are told not to play here and exited.
// ===========================================================================
void CheckPlayer( void )
{
	struct player_list *current;
	char log[80], choice, handle[21];
	INT16 newaccount = 0, good = FALSE;

	// Get the players list into memory
	GetPlayersList(TRUE);

	// set the current equal to the start of the list
	current = list;

	// check for the users name
	while (current != NULL)
	{
		// increment the account variable in case a player is not found
		// 1/97 was newaccount += 1;
		if (current->account >= newaccount) 
			newaccount = current->account+1;

		// Check if this is the user
		if (stricmp(current->real_names, od_control.user_name) == 0)
		{
			// Player is on this BBS, let them play
			if (current->bbs == thisbbs || current->bbs == 0 )
         // 1/97 can turn off dupe checking
         // 4/97 || checkForDupes == FALSE)
			{
				// Get the players information
				GetPlayerInfo(&player, current->account, TRUE);
				// 12/96
				PrivatePlayerUpdate();
				/*
				// check to see if they have gotten their extra money today
				if ((dailymoney != 0L) && (player.laston < currentday)) player.money += dailymoney;
				*/
				player.laston = currentday;
				// The following is need to keep a new player from corrupting the PLAYER.DAT file
				refused = FALSE;
				// check to see if the player's bbs number is 0, if so set it to thisbbs
				if (player.bbs == 0) player.bbs = thisbbs;

				WritePlayerInfo(&player, player.account, TRUE);
				return;
			}
			// Player is already playing on another BBS
         // 4/97 fixed dupe player checking
			else if (checkForDupes == TRUE)
			{
				od_clr_scr();
				// Place a cool ANSI next to the menu. if the user has ANSI
				g_send_file("gac_1");

				if(od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
				{
					// Only for ANSI, RIP or AVATAR
					// Create a background Window for the registration information
					od_printf("`bright blue`");
					od_draw_box(5, 8, 75, 13);
					od_set_cursor(9,7);
					od_printf("`bright red`WARNING! `cyan`Duplicate player name found in the Inter-BBS League `bright cyan`%s", league);
					od_set_cursor(10,7);
					od_printf("`bright cyan`%s`cyan` was already found in the database on", od_control.user_name);
					od_set_cursor(11,7);
					od_printf("`bright cyan`%s`cyan`.  If you are not playing on that BBS, then", getbbsname(current->bbs));
					od_set_cursor(12,7);
					od_printf("`cyan`you need to have the sysop change your name to play this game.");
					od_set_cursor(13,20);
					od_printf("`bright blue`< `bright cyan`Press any key to return to the BBS `bright blue`>");
					od_get_key(TRUE);
				}
				else
				{
					od_printf("`bright red`WARNING! `cyan`Duplicate player name found in the Inter-BBS League `bright cyan`%s\n\r", league);
					od_printf("`bright cyan`%s`cyan` was already found in the database on \n\r", od_control.user_name);
					od_printf("`bright cyan`%s`cyan`.  If you are not playing on that BBS,\n\r", getbbsname(current->bbs));
					od_printf("`cyan`then you need to have the sysop change your name to play this game.\n\r");
					gac_pause();
				}

				sprintf(log, "%s was found playing on BBS %s and rejected from playing here", od_control.user_name, getbbsname(current->bbs));
				od_log_write(log);
				od_exit(0, FALSE);
			}
		}
		current = current->next;
	}

	od_clr_scr();
	// Place a cool ANSI next to the menu. if the user has ANSI
	g_send_file("gac_1");

	// User was not found, get a handle, show them the rules and let them play!
	if(od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
	{
		// Only for ANSI, RIP or AVATAR
		// Create a background Window for the registration information
		od_printf("`bright blue`");
		od_draw_box(5, 8, 75, 13);
		od_set_cursor(9,7);
		od_printf("`bright cyan`WELCOME!  `cyan`You were not found in the player database.");
		od_set_cursor(10,7);
		od_printf("`cyan`Would you like to join the Tournament (`bright cyan`Y`cyan`/`bright cyan`n`cyan`)? : `bright yellow`");
		choice = od_get_answer("YN\r");
		od_printf("%c", choice);
		if (choice == 'N')
		{
			od_set_cursor(12,7);
			od_printf("`cyan`Come back when you are ready to play %s.", od_control.od_prog_name);
			od_set_cursor(13,20);
			od_printf("`bright blue`< `bright cyan`Press any key to return to the BBS `bright blue`>");
			od_get_key(TRUE);
			// Exit back to the BBS
			od_exit(0, FALSE);
		}
		else
		{
			good = FALSE;
			while (good == FALSE)
			{
				od_set_cursor(11,7);
				od_printf("`cyan`Please enter the handle you want to use:                       `bright cyan`");
				od_set_cursor(11, 48);
				od_input_str(handle, 20, 32, 127);
				// check for dupe handles
				if (CheckHandle(handle) == 0)
					good = TRUE;
			}

			od_set_cursor(12,7);
			od_printf("`cyan`Would you like to read the instructions (`bright cyan`Y`cyan`/`bright cyan`n`cyan`)? : `bright yellow`");
			choice = od_get_answer("YN\r");
			od_printf("%c", choice);
		}
	}
	else
	{
		od_printf("`bright blue`");
		od_printf("`bright cyan`WELCOME!  `cyan`You were not found in the player database.\n\r");
		od_printf("`cyan`Would you like to join the Tournament (`bright cyan`Y`cyan`/`bright cyan`n`cyan`)? : `bright yellow`");
		choice = od_get_answer("YN\r");
		od_printf("%c\n\r", choice);
		if (choice == 'N')
		{
			od_printf("`cyan`Come back when you are ready to play %s.\n\r", od_control.od_prog_name);
			gac_pause();
			// Exit back to the BBS
			od_exit(0, FALSE);
		}
		else
		{
			good = FALSE;
			while (good == FALSE)
			{
				od_printf("\n\r`cyan`Please enter the handle you want to use: `bright cyan`");
				od_input_str(handle, 20, 32, 127);
				// check for dupe handles
				if (CheckHandle(handle) == 0)
					good = TRUE;
			}

			od_printf("\n\r`cyan`Would you like to read the instructions (`bright cyan`Y`cyan`/`bright cyan`n`cyan`)? : `bright yellow`");
			choice = od_get_answer("YN\r");
			od_printf("%c\n\r", choice);
		}
	}

	if (choice != 'N')
	{
		// display the rules...
		od_clr_scr();
		od_printf("`Bright Cyan`");
		g_send_file(ibbsgametitle); // 12/96
		gac_pause();
	}

	// Set the default user information, write it to the file and let them continue...
	// copy the handle to the current player structure
   memset(&player, 0, sizeof(struct player));
	strcpy(player.names, handle);
	strcpy(player.real_names, od_control.user_name);
	player.bbs = thisbbs;
	player.laston = currentday;
	player.account = newaccount;

	PrivatePlayerInit();
	/*
	player.money =   (ulong) startmoney;
	player.investment = 0L;
	player.inv_start = 0;
	*/
	
	// Add the player to the database
	WritePlayerInfo(&player, player.account, TRUE);
	// Get the players list again
	GetPlayersList(TRUE);

	// The following is need to keep a new player from corrupting the PLAYER.DAT file
	refused = FALSE;

	sprintf(log, "%s joining the tournament\n", od_control.user_name);
	od_log_write(log);

	// The following is need to keep a new player from corrupting the PLAYER.DAT file
	refused = FALSE;
	return;
}


// ===========================================================================
// This function writes the current information for the player and stores it in the structure. Clean Up...
// If you pass FALSE for local and a bbs number, it will write to that file...
// Now if you pass false for local, then it will create a OUT\\GAME_P.DAT (PLAYER.OUT) file...
// ===========================================================================
// modified to write entire structure at once

void WritePlayerInfo( struct player *bjaccount, INT16 account_num, INT16 local)
{
	FILE *config;
	char bjfile[128];
	long int i;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  WritePlayerInfo()\n");
	fclose(gac_debug);
	#endif
	// ***** Write in the players information to the PLAYER.DAT file

	// write the current date to the file
	sprintf(bjfile, "%scurrent.day", doorpath);

	// Open the file to write in information
	config = gopen(bjfile, O_BINARY, O_DENYALL);
	if (config == NULL)
	{
		od_printf("ERROR OPENING %s to write current day.\n\r", bjfile);
		sprintf(bjfile, "ERROR Opening: %s to write current day", bjfile);
		od_log_write(bjfile);
	}
	else
	{
		// set pointer to start of file
		fseek(config, 0L, SEEK_SET);
		// Write the current day to the start of the file!
		currentday=LE_SHORT(currentday);
		fwrite( &currentday, sizeof( INT16 ), 1, config);
		currentday=LE_SHORT(currentday);
		fclose(config);
	}


	// Build the path to the info file
	if (local == TRUE) sprintf(bjfile, "%splayer.dat", doorpath);
	// else sprintf(bjfile, "%s%stbjp.%3.3d", doorpath, league, bbs);
	// else sprintf(bjfile, "%splayer.out", doorpath);
	else sprintf(bjfile, "%sOUT%c%s_p.dat", doorpath, PATH_DELIM, ibbsgametitle);

	config = gopen(bjfile, O_BINARY, O_DENYALL);
	if (config == NULL)
	{
		od_printf("`bright red`ERROR OPENING %s\n\r", bjfile);
		sprintf(bjfile, "ERROR Opening: %s", bjfile);
		od_log_write(bjfile);
		od_exit(1, FALSE);
	}

	// Set SEEK to start of player information.
	fseek(config, 0L, SEEK_SET);


	// Jump to the beginning of this Users Information
	for (i=0;i< account_num; i++)
	{
		// Seek past a user record...
		fseek(config, RECORD_LENGTH, SEEK_CUR);
	}

	// write the structure to the file
	player_write(bjaccount, config);
/*
	// Write user handle
	fwrite(bjaccount->names, sizeof( char ), 21, config);
	// Write user real name
	fwrite(bjaccount->real_names, sizeof( char ), 51, config);
	// BBS Number
	fwrite	(&bjaccount->bbs, sizeof( INT16 ), 1, config);
	// Last time on
	fwrite(&bjaccount->laston, sizeof( INT16 ), 1, config);
	// money
	fwrite(&bjaccount->money, sizeof( long int ), 1, config);
	// money investments
	fwrite(&bjaccount->investment, sizeof( long int ), 1, config);
	// Investment start time
	fwrite(&bjaccount->inv_start, sizeof( INT16 ), 1, config);
	// NOT USED
	for (i=1;i<=RECORD_UNUSED;i++)
	{
		temp_char = 'x';
	fwrite(&temp_char, sizeof( char ), 1, config);
	}
*/    

	// Close the file
	fclose(config);

	return;
}


// ===========================================================================
// This function grabs the infromation for the player and stores it in the structure.
// If you pass FALSE for local, and a BBS number, it can read the info from that file...
// ===========================================================================
// modified to read structure in at once...

INT16 GetPlayerInfo( struct player *bjaccount, INT16 account_num, INT16 local)
{
	FILE *config;
	char bjfile[128];
	INT16 i;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  GetPlayerInfo()\n");
	fclose(gac_debug);
	#endif
	// ***** Read in the players infromation from the PLAYER.DAT file
	// read in the date from the current.day file
	sprintf(bjfile, "%scurrent.day", doorpath);

	// Open the file to read in information
	config = gopen(bjfile, O_BINARY, O_RDONLY);
	if (config == NULL)
	{
		od_printf("ERROR OPENING %s current day set to 0.\n\r", bjfile);
		sprintf(bjfile, "ERROR Opening: %s, current day set to 0", bjfile);
		od_log_write(bjfile);
		currentday = 0;
	}
	else
	{
		// set pointer to start of file
		fseek(config, 0L, SEEK_SET);
		// Read in the current day from the current date file!
		fread( &currentday, sizeof( INT16 ), 1, config);
		currentday=LE_SHORT(currentday);
		fclose(config);
	}

	// ***** Read in the players infromation from the PLAYER.DAT file
	if (local == TRUE) sprintf(bjfile, "%splayer.dat", doorpath);
	//else sprintf(bjfile, "%s%stbjp.%3.3d", doorpath, league, bbs);
	// else sprintf(bjfile, "%splayer.in", doorpath);
	else sprintf(bjfile, "%sIN%c%s_p.dat", doorpath, PATH_DELIM, ibbsgametitle);

	// store the account number in the structure
	bjaccount->account = account_num;

	if (access(bjfile, 00) != 0)
	{
		// if the file does not exist, return...without error
		od_printf("No player.dat file found\n\r");
		return(-1);

	}
	// Open the file to read in information
	config = gopen(bjfile, O_BINARY, O_RDONLY);
	if (config == NULL)
	{
		printf("ERROR OPENING %s\n\r", bjfile);
		sprintf(bjfile, "ERROR Opening: %s", bjfile);
		od_log_write(bjfile);
		od_exit(1, FALSE);
		//exit(1);
	}
	// set pointer to start of file
	fseek(config, 0L, SEEK_SET);

	// Jump to the beginning of this Users Information
	for (i=0;i < account_num; i++)
	{
		// Seek past a user record...
		if (fseek(config, RECORD_LENGTH, SEEK_CUR) != 0)
		{
			fclose(config);
			od_printf("`Bright red`%d is an Invalid Account Number!`dark cyan` List players for account numbers.\n\r", account_num);
			return(-1);
		}
	}
	// read in the sctructure all at once
	player_read(bjaccount, config);
/*
	// Write user handle
	fread(bjaccount->names, sizeof( char ), 21, config);
	// Write user real name
	fread(bjaccount->real_names, sizeof( char ), 51, config);
	// BBS Number
	fread(&bjaccount->bbs, sizeof( INT16 ), 1, config);
	// Last time on
	fread(&bjaccount->laston, sizeof( INT16 ), 1, config);
	// money
	fread(&bjaccount->money, sizeof( long int ), 1, config);
	// money investments
	fread(&bjaccount->investment, sizeof( long int ), 1, config);
	// Investment start time
	fread(&bjaccount->inv_start, sizeof( INT16 ), 1, config);
	// 100 characters in the file are NOT USED
		fseek(config, RECORD_UNUSED, SEEK_CUR);
	
*/    
	// Close the file
	fclose(config);

	return(0);
}





// ===========================================================================
// My random function...
// ===========================================================================
INT16 g_rand( INT16 max )
{
	float rand_num = 0;
   // 1/97 changed calculation
//  rand_num = ((float) rand()/ (float)RAND_MAX ) * max;
	rand_num = ((float) rand()/ (float)RAND_MAX ) * (float) (max+1);
   if (rand_num > max)
	  rand_num = max;

	return( (int) rand_num);
}


// ===========================================================================
// Initialize the BJ game
// ===========================================================================
void InitGame( INT16 pause)
{

		INT16 i;
	char regstring[100];
	time_t now;
	long int diff;
	INT16 extra_delay;

	// Set the linked lists to null
	list = NULL;
	bbslist = NULL;
	
	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  InitGame()\n");
	fclose(gac_debug);
	#endif

	// Ignore Copyright
	od_control.od_nocopyright = TRUE;
	// Make the default rip screen be 23x80
	od_control.od_default_rip_win = FALSE;
	// Registration Information!
	strcpy(od_registered_to, "Gregory Campbell");
	od_registration_key = 583073792L;
	// Include OpenDoors Config File
	od_control.od_config_file = INCLUDE_CONFIG_FILE;
	// Point to custom function
	od_control.od_config_function = MyConfig;
	// Include the OpenDoors multiple personality system, which allows
	// the system operator to set the sysop statusline function key set
	 // to mimic the BBS software of their choice.
	 od_control.od_mps = INCLUDE_MPS;
	 // Include the OpenDoors log file system, which will record when the
	 // door runs, and major activites that the user performs.
	//#ifndef ODPLAT_WIN32
	od_control.od_logfile = INCLUDE_LOGFILE;
	//#endif
	 // Set function to be called before program exits.
	 od_control.od_before_exit = BeforeExitFunction;

	// Seed the random-number generator with current time so that the numbers will be different every time we run.
	srand( (unsigned)time( NULL ) );


	// Init the OpenDoors information
	od_init();
	// Ignore Copyright
	od_control.od_nocopyright = TRUE;
	// Make the default rip screen be 23x80
	od_control.od_default_rip_win = FALSE;
	od_control.od_help_text2 = (char *) "Copyright Vagabond Software - [F1]=User Info [F10]=Disable Status Line";
	// Set the border I want for the boxes
	od_control.od_box_chars[BOX_UPPERLEFT] = (unsigned char) 218;
	od_control.od_box_chars[BOX_TOP] = (unsigned char) 196;
	od_control.od_box_chars[BOX_UPPERRIGHT] = (unsigned char) 183;
	od_control.od_box_chars[BOX_LEFT] = (unsigned char) 179;
	od_control.od_box_chars[BOX_LOWERLEFT] = (unsigned char) 212;
	od_control.od_box_chars[BOX_LOWERRIGHT] = (unsigned char) 188;
	od_control.od_box_chars[BOX_BOTTOM] = (unsigned char) 205;
	od_control.od_box_chars[BOX_RIGHT] = (unsigned char) 186;

	// do not use a color control character
	od_control.od_color_char = '\0';

	od_clr_scr();
	// Create the RegString
	strcpy(regstring, od_control.sysop_name);
	strcat(regstring, od_control.system_name);
#ifdef KEEP_REG
	// Call the registration checking function
	#ifndef ODPLAT_WIN32
	RegKeyValidate(regstring, regcode, valcode, "Gregory Campbell", 272457219, &registered);
	// 4/97 temp fix for registration system in 32 bit versions
	#else
	if (strlen(regcode) == 20)
		registered = RK_REGISTERED;
	#endif
	od_clr_scr();
	if (registered == RK_REGISTERED)
	{
		od_printf("`bright white`%s\n\r", od_control.od_prog_name);
		od_printf("`green`Registered to `bright green`%s `green`of `bright green`%s`green`.\n\r\n\r", od_control.sysop_name, od_control.system_name);
		od_printf("`green`Programmed by `bright green`Gregory Campbell.\n\r");
                od_printf("`green`Maintained by `bright green`Vagabond Software.\n\r");
		od_printf("`green`%s.  All rights reserved.\n\r", od_control.od_prog_copyright);
		od_sleep(1000);
	}
	else
	{
		od_printf("`bright white`%s\n\r", od_control.od_prog_name);
		od_printf("`red`UNREGISTERED `green`help `bright green`%s `green`of `bright green`%s`green` register!\n\r\n\r", od_control.sysop_name, od_control.system_name);
		od_printf("`green`Programmed by `bright green`Gregory Campbell.\n\r");
                od_printf("`green`Maintained by `bright green`Vagabond Software.\n\r");
		od_printf("`green`%s.  All rights reserved.\n\r", od_control.od_prog_copyright);
		if (pause == TRUE)
		{
			/*
			od_printf("\n\r\n\r`red`Unregistered Pause ");
			for (i=0; i<10; i++)
			{
				od_sleep(1000);
				od_printf(".");
			}
			*/


			now = time(NULL);
			UpdateTime();
			diff = difftime(now, first_time);
			extra_delay = (int) ((diff-3888000L)/86400L);

			od_printf("\n\r`green`This program has been used `bright green`%ld`green` times in the last `bright green`%ld`green` days.\n\r", total_uses, (long int) (diff/86400L));
			if (extra_delay <= 0)
			{
				od_printf("\n\r`green`Unregistered Pause `bright red`   ");
				for (i=5; i>0; i--)
				{
					od_sleep(1000);
					od_printf("\b\b\b   \b\b\b");
					od_printf("%3d", i);
				}
				od_printf("\n\r");
			}
			else
			{
				od_printf("Please help `bright green`%s`green` register `bright green`%s`green`!", od_control.sysop_name, od_control.od_prog_name);
				od_printf("\n\r`green`Unregistered Pause (1 second added for each day over 45) `bright red`   ");
				for (i=5+extra_delay; i>0; i--)
				{
					od_sleep(1000);
					od_printf("\b\b\b   \b\b\b");
					od_printf("%3d", i);
				}
				od_printf("\n\r");
			}






		}
		else
		{
			od_sleep(1000);
		}
	}
#else
	od_printf("`bright white`%s\n\r", od_control.od_prog_name);
	od_printf("`green`Freeware Version.\n\r\n\r");
	od_printf("`green`Programmed by `bright green`Gregory Campbell.\n\r");
        od_printf("`green`Maintained by `bright green`Vagabond Software.\n\r");
	od_printf("`green`%s.  All rights reserved.\n\r", od_control.od_prog_copyright);
	od_sleep(1000);
#endif

	// Needs to be set to keep users from corrupting the PLAYER.DAT if they are not in the Tournament
	refused = TRUE;

	return;
}

// ===========================================================================
// This function checks for the correct number of command line parameters
// ===========================================================================
void command_line( void )
{
	// Check fo the proper number of command line parameters.
		printf( "The required command line parameters are:\n\n");
		printf( "  1 - Node Number (-N x), CONFIG, INBOUND, MAINTAIN, ROUTE\n");
		printf( "           SENDALL, RESET or BULLETINS\n");
		//printf( "  2 - LOCAL or Path to drop file if different from %s.cfg path (optional)\n", ibbsgametitle);
		//printf( "    - May also be BINKLEY  (e.g. %s OUTBOUND BINKLEY)\n\n", executable);
		printf( "  2 - Must be BINKLEY if you are using Binkley\n");
		printf( "Optional command line parameters are:\n");
		printf( "  -L - Run in Local mode\n");
		printf( "  -D - Specifiy dropfile path if different from %s.cfg\n", ibbsgametitle);
		printf( "  -? - View all other optional parameters\n\n");
		printf( "%s\n", od_control.od_prog_copyright);
		printf( "(972) 226-0485 (Voice)\n");
		printf( "Fidonet:  Bryan Turner@1:124/7013)\n");
		printf( "Telnet:   telnet://pegasus.bbs.us (Pegasus Flight BBS)\n");
		printf( "Internet: v@vbsoft.org\n");
		printf( "WWW:      http://www.vbsoft.org\n");
		printf( "This program is now FREEWARE!");

		exit(1);

	return;
}





// ===========================================================================
// This function gives the pause key prompt...
// ===========================================================================
void gac_pause( void)
{

		// Should add code to show a wonderful little RIP click box if RIP enabled...
		od_repeat( ' ', 25);
		od_printf("`bright blue`< `bright cyan`Press any key to continue`bright blue` >");
		od_get_key(TRUE);
}


// ===========================================================================
// This function reads the target nodes player name from the online.[node] file
// ===========================================================================
char *getname( INT16 node) // Get's the players name if given the node number
{
	char filename[128];
	#ifndef GSDK_OVERLAY
	static char username[36];
	#endif
	int online;

	strcpy(username, "Unknown");

	sprintf(filename, "%sONLINE.%d", doorpath, node);
	online = nopen(filename, O_RDONLY);
	if (online > 0)
	{
		lseek(online, 0L, SEEK_SET);
		read(online, username, 36);
		close(online);
	}

	return(username);
}





// ===========================================================================
// Shows an unregistered remark at the bottom of the screen...
// ===========================================================================
void ShowUnreg( void )
{

	//Show an Unregistered item
#ifdef KEEP_REG
	if ((registered != RK_REGISTERED) )
	{
		if (od_control.user_ansi)
		{
			od_set_cursor(25,1);
			od_clr_line();
		}
		else od_printf("\n\r\n\r");
		od_printf("`bright red`UNREGISTERED Evaluation Version!  Please register.");
	}
#endif
	return;
}


// ===========================================================================
// Get's a menu option processing the correct information
// ===========================================================================
char GetMenuChoice( void )
{

	char choice;

	choice = '\0';
	while (choice == '\0')
	{
		choice = od_get_key(FALSE);
		od_sleep(50);
	}
	choice = toupper(choice);

	return( choice );
}

// ===========================================================================
// This displays information about the Game
// ===========================================================================
void Information( void )
{

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  Information()\n");
	fclose(gac_debug);
	#endif
	od_clr_scr();
	od_printf("`bright blue`");
	if(od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
			{
					// Only for ANSI, RIP or AVATAR
					// Create a background Window for the registration information
					od_draw_box(5, 1, 75, 13);
					od_set_cursor(2,7);
					od_printf("`bright white`%s\n\r", od_control.od_prog_name);
					od_set_cursor(3,7);
#ifdef KEEP_REG
					if (registered == RK_REGISTERED)
					{
					od_printf("`cyan`Registered to `bright cyan`%s `cyan`of `bright cyan`%s`cyan`.\n\r\n\r", od_control.sysop_name, od_control.system_name);
			}
					else
					{
						od_printf("`red`UNREGISTERED `cyan`help `bright cyan`%s `cyan`of `bright cyan`%s`cyan` register!\n\r\n\r", od_control.sysop_name, od_control.system_name);
					}
#else
					od_printf("`cyan`Freeware Version.\n\r\n\r");
#endif
					od_set_cursor(4,7);
					od_printf("`cyan`%s.\n\r", od_control.od_prog_copyright);

					od_set_cursor(5,7);
					od_printf("`bright cyan`Commercial Quality at a Postcard Price.\n\r");

				od_set_cursor(7,7);
					od_printf("`bright cyan`Creation Team:\n\r");
				od_set_cursor(8,7);
					od_printf("`cyan`Lead Programmer:      `bright cyan`Gregory Campbell\n\r");
				od_set_cursor(9,7);
					od_printf("`cyan`Artwork:              `bright cyan`Gregory Campbell\n\r");
				od_set_cursor(11,7);
					od_printf("`cyan`Thank you to all the beta testers, users and friends.\n\r");

					od_set_cursor(15, 1);
		}
			else
			{
			od_printf("`bright white`%s\n\r", od_control.od_prog_name);
#ifdef KEEP_REG
				if (registered == RK_REGISTERED)
				{
					od_printf("`cyan`Registered to `bright cyan`%s `cyan`of `bright cyan`%s`cyan`.\n\r\n\r", od_control.sysop_name, od_control.system_name);
				}
				else
				{
					od_printf("`red`UNREGISTERED `cyan`help `bright cyan`%s `cyan`of `bright cyan`%s`cyan` register!\n\r\n\r", od_control.sysop_name, od_control.system_name);
				}
#else
				od_printf("`cyan`Freeware Version.\n\r\n\r");
#endif
				od_printf("`cyan`%s.\n\r",od_control.od_prog_copyright);
				od_printf("`bright cyan`Commercial Quality at a Postcard Price.\n\r\n\r");
				od_printf("`bright cyan`Creation Team:\n\r");
				od_printf("`cyan`Lead Programmer:      `bright cyan`Gregory Campbell.\n\r");
				od_printf("`cyan`Artwork:              `bright cyan`Gregory Campbell.\n\r\n\r");
				od_printf("`cyan`Thank you to all the beta testers, users and friends.\n\r");
				od_printf("\n\r\n\r");

			}
	return;
}

// ===========================================================================
//  Network open function. Opens all files DENYALL and retries LOOP_NOPEN
//  number of times if the attempted file is already open or denying access
//  for some other reason.      All files are opened in BINARY mode.                    */
// ===========================================================================

int nopen(char *str, int access)
{
	char count=0;
	int file,share;

if(access&O_DENYNONE) share=SH_DENYNO;
else if(access==O_RDONLY) share=SH_DENYWR;
else share=SH_DENYRW;
while(((file=sopen(str,O_BINARY|access,share,S_IWRITE|S_IREAD))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN)
	if(count>10)
		od_sleep(50);
if(count>(LOOP_NOPEN/2) && count<=LOOP_NOPEN)
	od_printf("`bright red`\r\nNOPEN COLLISION - File: %s Count: %d\r\n"
		,str,count);
if(file==-1 && errno==EACCES)
	od_printf("`Bright Red`\r\nNOPEN: ACCESS DENIED\r\n");
return(file);
}





// ===========================================================================
// This function grabs all the player numbers and names and puts it into a linked list
// also grabs all the info for the NPC characters and places it in the listing...
// 1/97 ignores player's with a handle of "Deleted".
// ===========================================================================
void GetPlayersList( INT16 byplayer )
{
	struct player_list *tail;
	FILE *config;
	char bjfile[128], tempnames[21];
	INT16 account = 0;
   char ignored=FALSE; // 1/97
   
	
	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  GetPlayersList()\n");
	fclose(gac_debug);
	#endif

	// Deallocate the memory for the previous list (if any
	// Since we allocated mem, we need to free it
	// Loop to free each link...
	tail = list;
	while ( tail != NULL)
	{
		tail = list->next;
		free(list);
		list = tail;
	}




		// ***** Read in the players infromation from the PLAYER.DAT file
		sprintf(bjfile, "%scurrent.day", doorpath);

		// Check to see if the file exists
		if (access(bjfile, 00) != 0 )
		{
			// No file found, so set the current day to zero
			currentday = 0;
		}
		else
		{
			// Open the player file to read in information (try 10 times to gain access)
			config = gopen(bjfile, O_BINARY, O_RDONLY);
			// Check for failure to open the file in time
			if ( config == NULL)
			{
				od_printf("`Bright Red`ERROR OPENING %s\n\r", bjfile);
				sprintf(bjfile, "ERROR Opening: %s\n", bjfile);
				od_log_write(bjfile);
				currentday=0;
			}
			else
			{
				// Set SEEK to start of file
				fseek(config, 0L, SEEK_SET);
				fread(&currentday, sizeof(INT16 ), 1, config);
				currentday=LE_SHORT(currentday);
				fclose(config);
			}
		}

		sprintf(bjfile, "%splayer.dat", doorpath);

		// Check to see if the file exists
		if (access(bjfile, 00) != 0 )
		{
			 list = NULL; // set the head to null...
			 return;
		}

		// Open the player file to read in information (try 10 times to gain access)
		config = gopen(bjfile, O_BINARY, O_RDONLY);
		// Check for failure to open the file in time
		if ( config == NULL)
		{
			od_printf("`Bright Red`ERROR OPENING %s\n\r", bjfile);
			sprintf(bjfile, "ERROR Opening: %s\n", bjfile);
			od_log_write(bjfile);
			od_exit(1, FALSE);
		}
		else
		{
			// Set SEEK to start of file
			fseek(config, 0L, SEEK_SET);

			// Allocate space for a structure
			list = malloc( sizeof(struct player_list) );
			if( list == NULL )
			{
				// Not enough memory
				od_printf( "`RED`Insufficient memory available to load Player Listing.\n\r" );
				od_log_write("Insufficient memory for Player Listing");
				return;
			}
			else
			{
				// Read in the handle
//              fread(  list->names, sizeof(char), 21, config);
				// 1/97 check if handle is "Deleted", if so ignore info
				// Read in the handle
				ignored = TRUE; // 1/97
				do 
				{
					if (fread(  tempnames, sizeof(char), 21, config) != 21)
					{
						// no good players in the file
						ignored = FALSE;
						free(list);
						list = NULL;
						fclose(config);
						return;
					}
		  
					if (stricmp(tempnames, "Deleted") == 0)
					{
						// seek past the rest of the record
						fseek(config, (RECORD_LENGTH - 21L), SEEK_CUR);
						ignored = TRUE;
						account += 1;
					}
					else
					{
						strcpy(list->names, tempnames);
						// Read in the real name
						fread( list->real_names, sizeof( char ), 51, config);
						// Read in the bbs number
						fread( &list->bbs, sizeof( INT16 ), 1, config);
						list->bbs=LE_SHORT(list->bbs);
						// skip the LastOn integer
						fseek(config, 2L, SEEK_CUR);
						// read in the players wealth
						fread( &list->wealth, sizeof( long int), 1, config);
						list->wealth=LE_LONG(list->wealth);	

						list->account = account;
						account += 1;
						// next must equal null
						list->next = NULL;
						// Set the tail to the same location as head
						// Seek past a user record...
						fseek(config, (RECORD_LENGTH - 80L), SEEK_CUR);
						ignored = FALSE;
					}

			   } while (ignored == TRUE );
			}
	}

		// Loop to read in remainder of file...
		while ( fread(  tempnames, sizeof(char), 21, config) == 21)
		{
		 // 1/97
		 if (stricmp(tempnames, "Deleted") == 0)
		 {
			// seek past the rest of the record
			fseek(config, (RECORD_LENGTH - 21L), SEEK_CUR);
			ignored = TRUE;
			account += 1;
			continue;
		 }
		 else
		 {
	
			// copy player handle
			strcpy(temp_player.names, tempnames);
			// Read in the real name
			fread( temp_player.real_names, sizeof(char), 51, config);
			// Read in the bbs number
			fread( &temp_player.bbs, sizeof( INT16 ), 1, config);
			temp_player.bbs=LE_SHORT(temp_player.bbs);
			// skip the LastOn integer
			fseek(config, 2L, SEEK_CUR);
			// read in the players wealth
			fread( &temp_player.wealth, sizeof( long int), 1, config);
			temp_player.wealth=LE_LONG(temp_player.wealth);

			temp_player.account = account;
			account += 1;
			// Seek past a user record
			fseek(config, (RECORD_LENGTH - 80L), SEEK_CUR);

			// Sort the information into the list
			SortPlayersList( &temp_player, byplayer);
		 }
		}

	// Close the file
	fclose(config);

	return;
}




// ===========================================================================
//  Network open function. Opens all files DENYALL and retries LOOP_GOPEN
//  number of times if the attempted file is already open or denying access
//  for some other reason.      All files are opened in the mode passed.                        */
// ===========================================================================
FILE *gopen(char *str, int mode, int access )
{
	char count=0, option[5];
	int file, share, mode2;

	// set mode2 to be O_CREAT
	mode2 = O_CREAT;
	if ( access == O_DENYNONE ) share=SH_DENYNO;
	else if (access == O_RDONLY) share=SH_DENYWR;
	else
	{
		 share=SH_DENYRW;
		 mode2 = O_CREAT|O_RDWR;
	}

	while ( ((file=sopen(str,mode|mode2, share, S_IWRITE|S_IREAD))==-1) && errno==EACCES && count++<LOOP_GOPEN)

	if (count>10)
			od_sleep(50);

	if (count>(LOOP_GOPEN/2) && count<=LOOP_GOPEN)
		od_printf("\n\r`BRIGHT RED`OPEN COLLISION - File: %s Count: %d\n\r",str,count);

	if (file==-1 && errno==EACCES)
		od_printf("\n\r`BRIGHT RED`NOPEN: ACCESS DENIED\n\r");


	// Set the file option for the conversion from INT16 to *FILE;
	if (access == O_DENYALL) strcpy(option, "r+");
	else strcpy(option, "r");

	if ((mode == O_BINARY) || (mode == (O_BINARY|O_APPEND))) strcat(option, "b");
	else strcat(option, "t");

	return(fdopen( file, option));
}



// ===========================================================================
// Show the list of players to the user in a pretty format DOES NOT pause at end of list
// Pass the BBS number of the list to be displayed
// ===========================================================================
void ShowPlayersList( INT16 bbs )
{
	struct player_list *current;
	struct player listplayer;
	INT16 i;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  ShowPlayerslist()\n");
	fclose(gac_debug);
	#endif


	// Get the players list
	GetPlayersList(TRUE);
	// Set line number to start
	i = 0;
	// Set current equal to the start
	current = list;

	// Clear the screen
	od_clr_scr();

	if (bbs == ALL)
	{
		// Show the cool ANSI
		g_send_file("gac_4");
		if (od_control.user_ansi) od_printf("\n\r");

		if (!od_control.user_ansi)
			od_printf("\n\r`bright white`Listing of all Players in this Tournament.\n\r");
	}
	else if (bbs == TOP)
	{
		// Place a cool ANSI next to the menu. if the user has ANSI
		g_send_file("gac_12");
		if (od_control.user_ansi) od_printf("\n\r");

		if (!od_control.user_ansi)
			od_printf("\n\r\n\r`Bright white`Inter-BBS Top Player Scores Listing\n\r");

	}
	else if (bbs == thisbbs)
	{
		// Place a cool ANSI next to the menu. if the user has ANSI
		g_send_file("gac_10");
		if (od_control.user_ansi) od_printf("\n\r");

		if (!od_control.user_ansi)
			od_printf("\n\r\n\r`Bright white`Player Scores Listing for this BBS\n\r");
	}
	else
	{
		// Print Heading
		g_send_file("gac_10");
		if (od_control.user_ansi) od_printf("\n\r");

		if (!od_control.user_ansi)
		od_printf("`bright white`Listing of all Players on `bright cyan`%s.\n\r", getbbsname(bbs));
	}


	od_printf("  `bright white`%-5s %-20s %-10s %-35s\n\r", "Rank",  "Player's Handle",  "  Wealth", "     BBS Name");

	od_printf("`dark cyan`");
	if(od_control.user_ansi || od_control.user_avatar)
	{
		 od_repeat((unsigned char)196, 79);
	}
	else
	{
	od_repeat('-', 79);
	}
	// skip a line
	od_printf("\n\r");

	// Loop to display the list the Players
	while (current != NULL)
	{
		// print the user info if it's for the BBS requested
		if ((current->bbs == bbs ) || (bbs == ALL) || (bbs == TOP)) 
			GetPlayerInfo(&listplayer, current->account, TRUE);
		else
		{
				// Increase the current pointer by one
				current = current->next;
				continue;
		}
		// Print the information
		// 12/96 removed investment
		od_printf("  `bright white`%3d   `cyan`%-20s `bright blue`$`cyan`%10lu   `bright blue`%-35.35s\n\r", i+1,  listplayer.names,
				(listplayer.money), getbbsname(listplayer.bbs));
		// Increment line number
		i++;

		// Increase the current pointer by one
		current = current->next;
		// check for a full page and pause if necessary
		if ((i%15 == 0) && (i != 0))
		{
			gac_pause();

			od_clr_scr();

			if (bbs == ALL)
			{
				// Show the cool ANSI
				g_send_file("gac_4");
				od_printf("\n\r");
				if (!od_control.user_ansi)
					od_printf("`bright white`Listing of all Players in this Tournament.\n\r");
			}
			else if (bbs == TOP)
			{
				// Return since we only want to show 15 players
				return;
			}
			else if (bbs == thisbbs)
			{
				// Place a cool ANSI next to the menu. if the user has ANSI
				g_send_file("gac_10");
      		if (od_control.user_ansi) od_printf("\n\r");

				if (!od_control.user_ansi)
					od_printf("\n\r\n\r`Bright white`Player Scores Listing for this BBS\n\r");
			}
			else
			{
				// Print Heading
				g_send_file("gac_10");
      		if (od_control.user_ansi) od_printf("\n\r");

				if (!od_control.user_ansi)
				od_printf("`bright white`Listing of all Players on `Bright cyan`%s.\n\r", getbbsname(bbs));
			}

			od_printf("  `bright white`%-5s %-20s %-10s %-35s\n\r", "Rank",  "Player's Handle",  "  Wealth", "     BBS Name");
			od_printf("`dark cyan`");
			if(od_control.user_ansi || od_control.user_avatar)
			{
				od_repeat((unsigned char)196, 79);
			}
			else
			{
				od_repeat('-', 79);
			}
			// skip a line
			od_printf("\n\r");
		}
	}

	return;
}



// ===========================================================================
// Simply put, this function will sort the players list according to maximum money for a better display
// void SortPlayersList( INT16 byplayer )
// ===========================================================================
void SortPlayersList( struct player_list *sortplayer, INT16 byplayer )
{

	struct player_list *cur;
	struct player_list *prev;
	struct player_list *newitem;

	INT16 done = FALSE;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  SortPlayersList()\n");
	fclose(gac_debug);
	#endif

	// set previous to null since beginning of list
	prev = NULL;
	// set the current equal to the start
	cur = list;

	// Set done to false, becomes true when spot in list is found
	done = FALSE;
	// advance prev and cur as long as the account money is less than the current money

	while ( ( cur != NULL ) && ( done == FALSE ) )
	{
		// loop to determine if account money is greater than all other moneys
		if (( sortplayer->wealth < cur->wealth) && (byplayer == TRUE))
		{
			prev = cur;
			cur = cur->next;
		}
		else if (( sortplayer->bbs < cur->bbs) && (byplayer == FALSE))
		{
			prev = cur;
			cur = cur->next;
		}
		else
		{
			// found where temp_address goes
			done = TRUE;
		}

	}

	// create a new node which newitem points to, and insert the new data
	newitem = malloc( sizeof(struct player_list) );
	if( newitem == NULL )
	{
		// Not enough memory
		od_printf( "`bright red`Insufficient memory available to load player file.\n" );
		od_log_write("Insufficient memory to load player file");
		return;
	}

	// Set newitem equal to the account sent
	strcpy(newitem->names, sortplayer->names);
	strcpy(newitem->real_names, sortplayer->real_names);
	newitem->bbs = sortplayer->bbs;
	newitem->wealth = sortplayer->wealth;
	newitem->account = sortplayer->account;

	// connect the new node to the list
	if ( prev == NULL)
	{
		// insert new node at beginning
		newitem->next = list;
		list = newitem;
	}
	else
	{
		// insert new nodes between nodes
		newitem->next = cur;
		prev->next = newitem;
	}

	return;
}


// ===========================================================================
// This function should retrieve a BBS name from the Inter-BBB file
// ===========================================================================
char *getbbsname( INT16 bbs)
{
	#ifndef GSDK_OVERLAY
	static char bbsname[41];
	#endif
	INT16 iCurrentSystem;

	if (bbs == thisbbs)
	{
		strcpy(bbsname, od_control.system_name);
		return( bbsname);
	}
	else
	{
		// Search the InterBBS function for the information
		//Init the Inter-BBS structures
//      InitIBBS();

		// Loop for each system in other systems array
		for(iCurrentSystem = 0; iCurrentSystem < InterBBSInfo.nTotalSystems; ++iCurrentSystem)
		{
			// Find the bbs number
			if (InterBBSInfo.paOtherSystem[iCurrentSystem].szNumber == bbs)
			{
				// Copy the current BBS name to the string
				strcpy(bbsname, InterBBSInfo.paOtherSystem[iCurrentSystem].szSystemName);
				return( bbsname);
			}
		 }
	}

	// Create a Generic Name in case it was not found
	sprintf(bbsname, "BBS %d (Unknown)", bbs);
	return( bbsname);
}


// ===========================================================================
// Lets the user view a listing of the BBS.  Shows total or average scores
// ===========================================================================
void ViewBBSList( INT16 average )
{

	struct bbs_list *currbbs;
	INT16 i;


	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  ViewBBSList()\n");
	fclose(gac_debug);
	#endif


	// Get the players list sorted by BBSs
	GetPlayersList( FALSE );
	// Develop the top BBS's.
	MakeTopBBS(average);

	od_clr_scr();
	// Place a cool ANSI next to the menu. if the user has ANSI
	g_send_file("gac_13");
	if (od_control.user_ansi) od_printf("\n\r");

	if (!od_control.user_ansi) od_printf("\n\r\n\r`Bright white`Participating BBSs Listing\n\r");

	if (average == TRUE) od_printf("  `bright white`%-5s %-35s %-15s %-15s\n\r", "Rank", "BBS Name",  "Total Players",  "Average Wealth");
	else od_printf("  `bright white`%-5s %-35s %-15s %-15s\n\r", "Rank", "BBS Name",  "Total Players",  "Total Wealth");

	od_printf("`dark cyan`");
	if(od_control.user_ansi || od_control.user_avatar)
	{
		 od_repeat((unsigned char)196, 79);
	}
	else
	{
	od_repeat('-', 79);
	}
	// skip a line
	od_printf("\n\r");

	// check to see if this is a local only game
	if (thisbbs == 0)
	{
		od_printf("\n\r`bright cyan`This Tournament Game has NOT been setup for Inter-BBS\n\r");
		return;
	}
	i = 0;
	// set the start of the list of top BBSs
	currbbs = bbslist;

	// Loop to display the list the Players
	while (currbbs != NULL)
	{


		// print info for the BBS
		od_printf( "  `bright white`%3d   `cyan`%-35s %10ld        `bright blue`$`cyan`%10lu\n\r", i+1,  getbbsname(currbbs->bbs),
				currbbs->players, currbbs->wealth);
		// Increment line number
		i++;
		// Increase the current pointer by one
		currbbs = currbbs->next;



		// check for a full page and pause if necessary
		if ((i%15 == 0) && (i != 0))
		{
			gac_pause();

			od_clr_scr();
			// Place a cool ANSI next to the menu. if the user has ANSI
			g_send_file("gac_13");
   		if (od_control.user_ansi) od_printf("\n\r");

			if (!od_control.user_ansi) od_printf("\n\r\n\r`Bright white`Participating BBSs Listing\n\r");

			if (average == TRUE) od_printf("  `bright white`%-5s %-35s %-15s %-15s\n\r", "Rank", "BBS Name",  "Total Players",  "Average Wealth");
			else od_printf("  `bright white`%-5s %-35s %-15s %-15s\n\r", "Rank", "BBS Name",  "Total Players",  "Total Wealth");


			od_printf("`dark cyan`");
			if(od_control.user_ansi || od_control.user_avatar)
			{
				 od_repeat((unsigned char)196, 79);
			}
			else
			{
				od_repeat('-', 79);
			}
			// skip a line
			od_printf("\n\r");
		}
	}
	return;
}



// ===========================================================================
// Checks to see if the Tournament is over, and starts a new one.
// ===========================================================================
void CheckDate( void )
{

	char bjfile[128];
	int dayfile;
	INT16 i =0;
	struct player_list *current;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  CheckDate()\n");
	fclose(gac_debug);
	#endif
	
	
	
	if ((currentday>tourneylength) && (tourneylength != 0))
	{
		od_log_write("Tournament end, game automatically reset.");
		od_printf("`Bright Cyan`The %s game has ended, the game will be reset.\n\r\n\r", od_control.od_prog_name);

		// Make new Bulletins
		MakeBulletins(TRUE);

		// Remove any .MSG files that may be here
		GetPlayersList(TRUE);
		od_printf("`Cyan`Removing player message files..");
		i = 0;
		current = list;
		while (current !=NULL)
		{
			od_printf(".");
			// remove the file
			sprintf(bjfile, "%s%d.msg", doorpath, i);
			if (access(bjfile, 00) == 0) remove(bjfile);
			i++;
			current = current->next;
		}
		od_printf("\n\r");

		od_printf("\n\r`Bright cyan`Removing Outbound Inter-BBS Message Files..");
		for (i=0;i<InterBBSInfo.nTotalSystems; i++)
		{
			od_printf(".");
			// Make the name of the file...
			// sprintf(bjfile, "%sOUT_MSGS.%d", doorpath, i);
			sprintf(bjfile, "%sOUT%c%s_M.%d", doorpath, PATH_DELIM, ibbsgametitle, i);
			if (access(bjfile, 00) == 0)
				remove(bjfile);
		}
		od_printf("\n\r");

		// remove the PLAYER.DAT file
		sprintf(bjfile, "%splayer.dat", doorpath);
		if (access(bjfile, 00) == 0) remove(bjfile);
		od_printf("`Cyan`Removing player data file...\n\r");
		// remove the CURRENT.DAY file
		sprintf(bjfile, "%scurrent.day", doorpath);
		if (access(bjfile, 00) == 0) remove(bjfile);
		od_printf("`Cyan`Resetting the current day file...\n\r");
		// set the current day equal to zero
		currentday = 0;
		if((dayfile=nopen(bjfile,O_WRONLY|O_CREAT))==-1)
		{
		od_printf("\n\r`bright red`Error creating current.day\r\n");
		od_log_write("Error creating current.day");
		}
		currentday=LE_SHORT(currentday);
		write(dayfile,&currentday,sizeof(INT16));
		currentday=LE_SHORT(currentday);
		close(dayfile);

		// Get the new players listing (which is empty)
		GetPlayersList(TRUE);

		od_printf("`Bright Cyan`FINISHED resetting.\n\r");

	}
	return;
}


// ===========================================================================
// Makes bulletins of the top 15 players and BBSs...
// ===========================================================================
void MakeBulletins( INT16 last )
{

	FILE *playerAns, *playerAsc, *bbsAns, *bbsAsc;
	char playerAnsfile[128], playerAscfile[128], bbsAnsfile[128], bbsAscfile[128];
	INT16 i;
	struct player_list *current;
	struct player listplayer;
	struct bbs_list *currbbs;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  MakeBulletins()\n");
	fclose(gac_debug);
	#endif

	//Create a BJ score file of the TOP ten BBSs and Players.

	// InitIBBS();
	// Get the listing of players
	GetPlayersList(TRUE);
	// Set account to start
	i = 0;
	od_printf("`Bright cyan`Creating Top 15 Bulletins for the Tournament.\n\r\n\r");
	// Make paths to the score files
	if (last == TRUE)
	{
		sprintf(bbsAscfile, "%slst15bbs.asc", doorpath);
		sprintf(playerAscfile, "%slst15ply.asc", doorpath);
		sprintf(bbsAnsfile, "%slst15bbs.ans", doorpath);
		sprintf(playerAnsfile, "%slst15ply.ans", doorpath);
	}
	else
	{
		sprintf(bbsAscfile, "%stop15bbs.asc", doorpath);
		sprintf(playerAscfile, "%stop15ply.asc", doorpath);
		sprintf(bbsAnsfile, "%stop15bbs.ans", doorpath);
		sprintf(playerAnsfile, "%stop15ply.ans", doorpath);
	}
	od_printf("`Cyan`Removing old files...\n\r");
	// Remove the old top 15 bulletins
	remove(bbsAnsfile);
	remove(playerAnsfile);
	remove(bbsAscfile);
	remove(playerAscfile);
	od_printf("`Cyan`Opening new player score bulletins ...\n\r");
	playerAsc = fopen(playerAscfile, "w+b");
	playerAns = fopen(playerAnsfile, "w+b");
	od_printf("`Cyan`Writing player score files ..");

	// print the titles
	if (last == TRUE)
	{
// 4/97                fputs(szPlylstANS, playerAns);
                fputs(szPlycurANS, playerAns);
                fprintf(playerAns, "\r\n     *** Top 15 Players of the Last %s Game ***\r\n\r\n", od_control.od_prog_name);
                fprintf(playerAsc, "\r\n\r\n     *** Top 15 Players of the Last %s Game ***\r\n\r\n", od_control.od_prog_name);
                fprintf(playerAsc, "\r\n\r\n     *** Top 15 Players of the Last %s Game ***\r\n\r\n", od_control.od_prog_name);
	}
	else
	{
		fputs(szPlycurANS, playerAns);
                fprintf(playerAns, "\r\n     *** Top 15 Players of the Current %s Game ***\r\n\r\n", od_control.od_prog_name);
                fprintf(playerAsc, "\r\n\r\n     *** Top 15 Players of the Current %s Game ***\r\n\r\n", od_control.od_prog_name);
	}

	fprintf(playerAns, "\r\n  \x1B[0;1;37m%-5s %-20s %-10s %-35s\r\n", "Rank",  "Player's Handle",  "  Wealth", "     BBS Name");
	fprintf(playerAsc, "  %-5s %-20s %-10s %-35s\r\n", "Rank",  "Player's Handle",  "  Wealth", "     BBS Name");

	fprintf(playerAns, "\x1B[0;36m");
	for (i = 1; i<= 79; i++)
	{
		fprintf(playerAns, "%c", 196);
		fprintf(playerAsc, "-");
	}
	// skip a line
	fprintf(playerAns, "\r\n");
	fprintf(playerAsc, "\r\n");

	i = 0;
	// Set current equal to the start
	current = list;

	// Loop to display the list the Players
	while ((current != NULL) && (i<15))
	{

		// print the user info if it's for the BBS requested
		GetPlayerInfo(&listplayer, current->account, TRUE);

		// 12/96 removed investment
		fprintf(playerAns, "  \x1B[0;1;37m%3d   \x1B[0;36m%-20s \x1B[0;1;34m$\x1B[0;36m%10lu   \x1B[0;34m%-35.35s\r\n", i+1,  listplayer.names,
					(listplayer.money), getbbsname(listplayer.bbs));
		fprintf(playerAsc, "  %3d   %-20s $%10lu   %-35.35s\r\n", i+1,  listplayer.names,
					(listplayer.money), getbbsname(listplayer.bbs));
		// Increment line number
		i++;
		od_printf("`Cyan`.");

		// Increase the current pointer by one
		current = current->next;

	}
		od_printf("`Cyan`\n\r");

		// close the files
		fclose(playerAns);
		fclose(playerAsc);


	// *** Make the TOP 15 BBS Bulletins
	// Get the players list sorted by BBSs
	GetPlayersList( FALSE );
	od_printf("`Cyan`Calculating top BBSs...\n\r");

	// Develop the top 15 bbss.
	MakeTopBBS(TRUE);

	od_printf("`Cyan`Opening new BBS score files...\n\r");

	bbsAns = fopen(bbsAnsfile, "w+b");
	bbsAsc = fopen(bbsAscfile, "w+b");

	od_printf("`Cyan`Writing new BBS score files ..");

	// print the titles
	if (last == TRUE)
	{
// 4/97                fputs(szBbslstANS, bbsAns);
                fputs(szBbscurANS, bbsAns);
                fprintf(bbsAns, "\r\n     *** Top 15 BBSs of the Last %s Game ***\r\n\r\n", od_control.od_prog_name);
                fprintf(bbsAsc, "\r\n\r\n     *** Top 15 BBSs of the Last %s Game ***\r\n\r\n", od_control.od_prog_name);
	}
	else
	{
		fputs(szBbscurANS, bbsAns);
                fprintf(bbsAns, "\r\n     *** Top 15 BBSs of the Current %s Game ***\r\n\r\n", od_control.od_prog_name);
                fprintf(bbsAsc, "\r\n\r\n     *** Top 15 BBSs of the Current %s Game ***\r\n\r\n", od_control.od_prog_name);
	}

	fprintf(bbsAns, "\r\n  \x1B[0;1;37m%-5s %-35s %-15s %-15s\r\n", "Rank", "BBS Name",  "Total Players",  "Average Wealth");
	fprintf(bbsAsc, "  %-5s %-35s %-15s %-15s\r\n", "Rank", "BBS Name",  "Total Players",  "Average Wealth");

	fprintf(bbsAns, "\x1B[0;36m");
	for (i = 1; i<= 79; i++)
	{
		fprintf(bbsAns, "%c", (unsigned char)196);
		fprintf(bbsAsc, "-");
	}
	// skip a line
	fprintf(bbsAns, "\r\n");
	fprintf(bbsAsc, "\r\n");

	i = 0;
	// set the start of the list of top BBSs
	currbbs = bbslist;

	// Loop to display the list the Players
	while ((currbbs != NULL) && (i<15))
	{
		// print info for the BBS
		fprintf(bbsAns, "  \x1B[0;37m%3d   \x1B[0;36m%-35s %10ld        \x1B[0;34m$\x1B[0;36m%10ld \r\n", i+1,  getbbsname(currbbs->bbs),
					currbbs->players, currbbs->wealth);
		fprintf(bbsAsc, "  %3d   %-35s %10ld        $%10ld \r\n", i+1,  getbbsname(currbbs->bbs),
					currbbs->players, currbbs->wealth);
		// Increment line number
		i++;
		od_printf("`Cyan`.");
		// Increase the current pointer by one
		currbbs = currbbs->next;

	}
	od_printf("`Cyan`\n\r");
	// close the files
	fclose(bbsAns);
	fclose(bbsAsc);

	od_printf("`Cyan`Closing all files...\n\r");
	od_printf("`Cyan`All finished creating score files\n\r");

	return;
}

// ===========================================================================
// This gets a list of the BBSs listed in order of average or total wealth
// ===========================================================================
void    MakeTopBBS( INT16 average )
{

	struct bbs_list *tail;
	struct player_list *currplayer;
	INT16  currentbbs=0, iCurrentSystem=0;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  MakeTopBBS()\n");
	fclose(gac_debug);
	#endif


	// Deallocate the memory for the previous list (if any
	// Since we allocated mem, we need to free it
	// Loop to free each link...
	tail = bbslist;
	while ( tail != NULL)
	{
		tail = bbslist->next;
		free(bbslist);
		bbslist = tail;
	}


	// Allocate space for a structure
	bbslist = malloc( sizeof(struct bbs_list) );
	if( bbslist == NULL )
	{
		// Not enough memory
		od_printf( "`RED`Insufficient memory available to Sort BBS Listing.\n\r" );
		// Pause execution so user hits key
		od_get_key(TRUE);
		return;
	}
	else
	{
		// do the first BBS in the interbbs list
		iCurrentSystem = 0;
		// init the first bbs to the bbslist linked list
		bbslist->wealth = 0L;
		bbslist->players = 0L;
		bbslist->bbs = InterBBSInfo.paOtherSystem[iCurrentSystem].szNumber;
		currentbbs = InterBBSInfo.paOtherSystem[iCurrentSystem].szNumber;

		// set the current player to the start of the listing
		currplayer = list;
		// Loop for all players on this BBS
		while (currplayer != NULL )
		{
			if (currplayer->bbs == currentbbs)
			{
				bbslist->wealth += currplayer->wealth;
				bbslist->players += 1L;
			}
			currplayer = currplayer->next;
		}

		if (average == TRUE)
		{
			// Make the wealth an average wealth
			if (bbslist->players == 0) bbslist->wealth = 0L;
			else bbslist->wealth = bbslist->wealth/bbslist->players;
		}

		// next must equal null
		bbslist->next = NULL;
	}

	// Loop to do the remainder of the BBSs...
	for(iCurrentSystem = 1; iCurrentSystem < InterBBSInfo.nTotalSystems; ++iCurrentSystem)
	{
		// set the current player to the start of the listing
		currplayer = list;

		// init the bbs to the bbslist linked list
		temp_bbs.wealth = 0L;
		temp_bbs.players = 0L;
		temp_bbs.bbs = InterBBSInfo.paOtherSystem[iCurrentSystem].szNumber;
		currentbbs = InterBBSInfo.paOtherSystem[iCurrentSystem].szNumber;

		// Loop for all players on this BBS
		while (currplayer != NULL )
		{
			if (currplayer->bbs == currentbbs)
			{
				temp_bbs.wealth += currplayer->wealth;
				temp_bbs.players += 1L;
			}
			currplayer = currplayer->next;
		}
		if (average == TRUE)
		{
			// Make the wealth an average wealth
			if (temp_bbs.players == 0) temp_bbs.wealth = 0L;
			else temp_bbs.wealth = temp_bbs.wealth/temp_bbs.players;
		}

		// next must equal null
//      temp_bbs.next = NULL;
		// Sort the information into the list
		SortBBSList( &temp_bbs );
		}

	return;
}



// ===========================================================================
// Simply put, this function will sort the players list according to maximum money for a better display
// ===========================================================================
void SortBBSList(struct bbs_list *sortbbs)
{

	struct bbs_list *cur;
	struct bbs_list *prev;
	struct bbs_list *newitem;

	INT16 done = FALSE;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  SortBBSList()\n");
	fclose(gac_debug);
	#endif


	// set previous to null since beginning of list
	prev = NULL;
	// set the current equal to the start
	cur = bbslist;

	// Set done tofalse, becomse true when spot in list is found
	done = FALSE;
	// advance prev and cur as long as the account money is less than the current money

	while ( ( cur != NULL ) && ( done == FALSE ) )
	{
		// loop to determine if account money is greater than all other moneys
		if (( sortbbs->wealth < cur->wealth))
		{
			prev = cur;
			cur = cur->next;
		}
		else
		{
			// found where temp_address goes
			done = TRUE;
		}

	}

	// create a new node which newitem points to, and insert the new data
	newitem = malloc( sizeof(struct bbs_list) );
	if( newitem == NULL )
	{
		// Not enough memory
		od_printf( "`bright red`Insufficient memory available to sort BBSs.\n" );

		od_log_write("Insufficient memory to sort BBSs");
		// Pause execution so user hits key
		// gac_pause();
		return;
	}

	// Set newitem equal to the account sent
	newitem->bbs = sortbbs->bbs;
	newitem->wealth = sortbbs->wealth;
	newitem->players = sortbbs->players;

	// connect the new node to the list
	if ( prev == NULL)
	{
		// insert new node at beginning
		newitem->next = bbslist;
		bbslist = newitem;
	}
	else
	{
		// insert new nodes between nodes
		newitem->next = cur;
		prev->next = newitem;
	}
	return;
}






// ===========================================================================
// This function checks to see if the user entered a correct handle...
// ===========================================================================
INT16 CheckHandle(char *input)
{
	struct player_list *current;
	char   tempprompt[128];


	// check to make sure another player on this BBS does not have the handle...

	// check for a blank input
	if (stricmp(input, "                    ") <= 0 ) return(-1);

   // 1/97 don't allow a player to be named deleted
   if (stricmp(input, "Deleted") == 0) return(-1);

	// Get the players list (Upadtye the info...)
	GetPlayersList(TRUE);
	// Set current equal to the start
	current = list;
	// Loop to process the list of Players
	while (current != NULL)
	{
		if ((stricmp(input, current->names) == 0) && (current->bbs == thisbbs))
		{

			sprintf(tempprompt, "`bright cyan`%s`cyan` is already in use by a player on this BBS.", input);
			PromptBox(tempprompt, "Press any key to continue", "ANY", FALSE);
			return(-1);
		}
		current = current->next;
	}

	return(0);
}


// sends an extra \r\n if a RIP user is detected...
void g_clr_scr(void)
{
	od_clr_scr();
	// force screen clearing even if it is turned off for the user
	if (!od_control.user_attribute & 0x02) od_disp_emu("\xc", TRUE);
	// send an extra line for RIP users to avoid losing the top line
	if (od_control.user_rip)
	{
		// display the RIP border that has buttons to click ?
		g_send_file("gac_keys");
		od_printf("``\n\r\n\r");
	}

	return;
}


// This function will take the prompt (1 line) and the possibles responses as parameters
// It will then create a box to prompt the user for the required info...
// If you pass "ANY" as the third parameter, then the user can hit any key
// if bottom == TRUE, the box is at the bottom of the screen
char PromptBox( char prompt1[200], char prompt2[200], char responses[20], INT16 bottom )
{

	void *instruct_win;   // pop up window
	char response;              // the user's response

	#ifdef GAC_DEBUG
	gac_debug = myopen(gac_debugfile, "a", SH_DENYRW);
	fprintf(gac_debug, "  PromptBox()\n");
	fclose(gac_debug);
	#endif

	// for ANSI users
	if (od_control.user_ansi || od_control.user_rip)
	{
			if (bottom == FALSE) instruct_win = od_window_create(5,10,75,13,"Make your choice",0x09,0x0b,0x00,0);
			else instruct_win = od_window_create(5,20,75,23,"Make your choice",0x09,0x0b,0x00,0);
			od_set_attrib(0x03);
			if (bottom == FALSE) od_set_cursor(11,7);
			else od_set_cursor(21,7);
			od_printf(prompt1);
			od_set_attrib(0x03);
			if (bottom == FALSE) od_set_cursor(12,7);
			else od_set_cursor(22,7);
			od_printf(prompt2);
			// response = od_get_answer(responses);
			// want the players to still be notified when they are deciding and to loop until they pick a correct choice
			response = '~';  // use a bogus character to start with (the '\0' character could be found)
			if (stricmp(responses, "ANY") != 0)
			{
				while (strchr(responses, response) == NULL)
					response = GetMenuChoice();
			}
			else
			{
				response = GetMenuChoice();
			}
			od_window_remove(instruct_win);
	}
	else
	{
			od_save_screen(screen);
			od_printf(prompt1);
			od_printf(prompt2);
			if (stricmp(responses, "ANY") != 0)
				response = od_get_answer(responses);
			else
				response = od_get_key(TRUE);
			od_restore_screen(screen);
	}

	return (response);
}

void PromptString( char prompt1[200], char *response, INT16 max_len, char minchar, char maxchar, INT16 bottom )
{

	void *instruct_win;   // pop up window
//  static char response[80];           // the user's response

	#ifdef GAC_DEBUG
	gac_debug = myopen(gac_debugfile, "a", SH_DENYRW);
	fprintf(gac_debug, "  PromptString()\n");
	fclose(gac_debug);
	#endif

	// for ANSI users
	if (od_control.user_ansi || od_control.user_rip)
	{
			if (bottom == FALSE) instruct_win = od_window_create(5,10,75,13,"Answer",0x09,0x0b,0x00,0);
			else instruct_win = od_window_create(5,20,75,23,"Answer",0x09,0x0b,0x00,0);
			od_set_attrib(0x03);
			if (bottom == FALSE) od_set_cursor(11,7);
			else od_set_cursor(21,7);
			od_printf(prompt1);
			od_set_attrib(0x03);
			if (bottom == FALSE) od_set_cursor(12,7);
			else od_set_cursor(22,7);
			od_input_str(response, max_len, minchar, maxchar);
			
			od_window_remove(instruct_win);
	}
	else
	{
			od_save_screen(screen);
			od_printf(prompt1);
			od_input_str(response, max_len, minchar, maxchar);
			od_restore_screen(screen);
	}

	return;
}

#ifdef OPEN_DOOR_6
tODEditMenuResult EditQuit( void *unused )
{
	char choice='\0';

	choice = EditorHelp();

	switch (toupper(choice))
	{
		case 'A':
			memset(&msg_text, '\0', sizeof(msg_text));
			return(EDIT_MENU_EXIT_EDITOR);
		case 'C':
			od_save_screen( screen);
			ColorHelp();
			choice = '\0';
			return(EDIT_MENU_DO_NOTHING);
		case 'S':
			return(EDIT_MENU_EXIT_EDITOR);
//              case 'Y':
//                  od_save_screen( screen);
//                  PlayersInfo( &player);
//                  od_restore_screen( screen);
//                    return(EDIT_MENU_DO_NOTHING);
//                  break;
		default:
			return(EDIT_MENU_DO_NOTHING);
	}

//  return(EDIT_MENU_DO_NOTHING);

}

#endif

// ===========================================================================
// This function will allow a user to send a message to another user on another BBS
// If reply is TRUE, then the temp_player structure must be filled out as necessary
// if FALSE, the user will be prompted for the destination.
// ===========================================================================
void SendIBBSMsg( INT16 reply )
{

	void *instruct_win;   // pop up window
	char input[21], filename[128], response;
	int playerfile;

	INT16 more, msg_length;
	// 12/96
	#ifdef OPEN_DOOR_6
	tODEditOptions myEditOptions;
	#endif
	char *p, *m;    //12/96

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  SendIBBSMsg()\n");
	fclose(gac_debug);
	#endif


	od_clr_scr();
	g_send_file("gac_14");
	if (od_control.user_ansi) od_printf("\n\r");

	// Possibly show a neat ANSI border all the way around the screen?
	if (reply == FALSE)
	{
		do
		{
			response = PromptBox("`cyan`Do you want to view the list of players (`bright cyan`y`cyan`/`bright cyan`N`cyan`) or `bright cyan`q`cyan`)uit", "", "QYN\n\r", FALSE);
			if (response == 'Q') return;
			else if (response == 'Y')
			{
				ShowPlayersList(ALL);
				gac_pause();
			}

			// for ANSI users
			if (od_control.user_ansi || od_control.user_rip)
			{
				instruct_win = od_window_create(5,10,75,13,"Make your choice",0x09,0x0b,0x00,0);
				od_set_attrib(0x03);
				od_set_cursor(11,7);
				// Ask for the name of a user or let them list the players
				od_printf("Enter the name of the person you want to send mail to:`bright cyan`");
				// od_set_attrib(0x03);
				od_set_cursor(12,7);
				od_input_str(input, 20, 32, 127);
				od_window_remove(instruct_win);
			}
			else
			{
				od_save_screen(screen);
				// Ask for the name of a user or let them list the players
				od_printf("Enter the name of the person you want to send mail to:`bright cyan`\n\r");
				od_input_str(input, 20, 32, 127);
				od_restore_screen(screen);
			}

			// Check for that user, if found the info is in TEMP_PLAYER
		} while(CheckPlayerName(input) != 0);
		// init the message buffer to all null characters plus a \r\n
		memset(msg_text, '\0', sizeof(msg_text));

	}
	
	// GAC 12/96 Added ability to quote incoming messages...
	// If true, let them quote a message
	else if (reply == TRUE  && 
		PromptBox("Do you want to quote the original message (`bright cyan`y`cyan`/`bright cyan`N`cyan`)?", "", "YN\n\r", FALSE) == 'Y')
	{
		// incoming message is in the buffer WITHOUT padding, so we need
		// to copy the buffer to a temp_buffer and then copy the lines
		// back into the main buffer where the lines will point to...

		// copy the message text to the temp buffer
		memcpy(screen, msg_text, sizeof(msg_text));
		memset(&msg_text, '\0', sizeof(msg_text));
		// set a pointer to the start of the temp buffer
		p = &screen[0];
		m = &msg_text[0];

		// place each line from the temp buffer to the main buffer preceeded
		// by a >
		more=TRUE;
		do
		{
			m[0] = '>';
			m++;
			while (1)
			{
				// see if we are at the end of the line
				// if ( (p[0] == '\r' && p[1] == '\n') || (p[0] == '\n' && p[1] == '\r'))
				if ( p[0] == '\n')
				{
					// at end of the string...
					m[0] = '\n';
					m++;
					p++;
					break;
				}
				else if (p[0] == '\0')
				{
					m[0] = '\0';
					more = FALSE;
					break;
				}
				else
				{
					m[0] = p[0];
					m++;
					p++; // go to next letter
				}
			}
		}   while (more == TRUE);
	}
	else
	{
		// init the message buffer to all null characters plus a \r\n
		memset(msg_text, '\0', sizeof(msg_text));
		// strcpy(msg_text, "\r\n");
	}


	// If true, let them edit a message
	// msg_text[0] = '\0';
	// 12/96


	od_clr_scr();
	g_send_file("gac_14");
	if (od_control.user_ansi) od_printf("\n\r");

	if (od_control.user_ansi)
	{
		od_set_cursor(7,7);
		od_printf("`cyan`To: `bright cyan`%s`cyan` on `bright cyan`%s`cyan`\n\r\n\r", temp_player.names, getbbsname(temp_player.bbs));
		od_set_cursor(8,1);

		// 12/96 using new OD6.0 multiline_edit function
		#ifdef OPEN_DOOR_6
			memset(&myEditOptions, 0, sizeof(myEditOptions));
			// not working
			myEditOptions.TextFormat = FORMAT_LINE_BREAKS;
			od_printf("`bright cyan`");
			od_draw_box(4,9,76,23);
			od_printf("`cyan`");
			myEditOptions.nAreaLeft = 5;
			myEditOptions.nAreaRight = 75;
			myEditOptions.nAreaTop = 10;
			myEditOptions.nAreaBottom = 22;
			myEditOptions.pfMenuCallback = EditQuit;
			od_multiline_edit(msg_text, sizeof(msg_text), &myEditOptions);
			if (msg_text[0] == '\0') return;
		#else
		// init the pointers
		for (cur_line=0; cur_line < MAX_EDIT_LINES; cur_line++)
		{
			od_set_cursor(23-MAX_EDIT_LINES+cur_line+1, 1);
			od_printf("`bright cyan`%2d`cyan`-", cur_line+1);
			lines[cur_line] = &msg_text[(EDIT_LINE_WIDTH+2)*cur_line];
			lines[cur_line][0] = '\0';
		}
		// create the format string for editing
		strcpy(templine, "*");
		for (cur_line = 0; cur_line<EDIT_LINE_WIDTH; cur_line++)
		{
			strcat(templine, "*");
		}
		cur_line = 0;
		more = TRUE;
		while (more == TRUE)
		{

			edit_return = od_edit_str(lines[cur_line], templine,
						23-MAX_EDIT_LINES+cur_line+1, 5, 0x03, 0x21, 176, EDIT_FLAG_FIELD_MODE|EDIT_FLAG_EDIT_STRING|EDIT_FLAG_AUTO_DELETE|EDIT_FLAG_ALLOW_CANCEL);
			od_set_cursor(23-MAX_EDIT_LINES+cur_line+1, 1);
			od_printf("`bright cyan`%2d`cyan`- %-70s", cur_line+1, lines[cur_line]);
			// check for saving or aborting
			if (lines[cur_line][0] == '/')
			{
				choice = lines[cur_line][1];
				lines[cur_line][0] = '\0';
			}
			else
			{
				// change line number for editing
				if ((edit_return == EDIT_RETURN_ACCEPT || edit_return == EDIT_RETURN_NEXT) && cur_line < (MAX_EDIT_LINES-1) )
					cur_line++;
				else if (edit_return == EDIT_RETURN_PREVIOUS && cur_line > 0)
					cur_line--;
				else if ((edit_return == EDIT_RETURN_ACCEPT || edit_return == EDIT_RETURN_NEXT) && cur_line == (MAX_EDIT_LINES-1))
					choice = EditorHelp();
			}

			if (choice == '?')
				choice = EditorHelp();
			switch (toupper(choice))
			{
				case 'A':
					return;
				case 'C':
					od_save_screen( screen);
					ColorHelp();
					choice = '\0';
					break;
				case 'D':
					od_set_cursor(23-MAX_EDIT_LINES+cur_line+1, 1);
					od_printf("`bright cyan`%2d`cyan`- %-70s", cur_line+1, lines[cur_line]);
					if (cur_line < (MAX_EDIT_LINES-1)) cur_line++;
					break;
				case 'U':
					od_set_cursor(23-MAX_EDIT_LINES+cur_line+1, 1);
					od_printf("`bright cyan`%2d`cyan`- %-70s", cur_line+1, lines[cur_line]);
					if (cur_line > 0) cur_line--;
					break;
				case 'S':
					more = FALSE;
					break;
//              case 'Y':
//                  od_save_screen( screen);
//                  PlayersInfo( &player);
//                  od_restore_screen( screen);
//                  break;
				default:
					choice = '\0';
					break;
			}
		}

		// concantanate the lines to one string!
		// copy the buffer contents into a single string, instead of
		// having them seperated (start at second string)
		strcat(msg_text, "\r\n");
		for (cur_line=1; cur_line<MAX_EDIT_LINES; cur_line++)
		{
			strcpy(templine, lines[cur_line]);
			strcat(templine, "\r\n");
			strcat(msg_text, templine);
		}
		#endif

	}
	else
	{
		od_printf("\n\rUse Ctrl-Z, Ctrl-K, or Escape to save message, Ctrl-C to abort (15 line Max)\n\r");
		od_printf("`cyan`To: `bright cyan`%s`cyan` on `bright cyan`%s`cyan`\n\r\n\r", temp_player.names, getbbsname(temp_player.bbs));
		if (gac_lined(75, 15) == 0) return;
	}

	// Send it to another BBS, or write it to this BBS
	if (temp_player.bbs == thisbbs)
	{
		// write the info to the players [ACCOUNT].MSG file
		// create the filename
		sprintf(filename, "%s%d.msg", doorpath, temp_player.account);
		// open the file
		if((playerfile=nopen(filename,O_WRONLY|O_CREAT|O_APPEND))==-1)
		{
			// couldn't open the file
			od_printf("`bright red`ERROR: Couldn't create/open the Player's %d.msg file\n\r", temp_player.account);
			od_log_write("ERROR: Couldn't create/open the Player's .msg file\n\r");
			return;
		}
	}
	else
	{
		//write the message to a file for the BBS we are going to
		// create the filename
		// sprintf(filename, "%sOUT_MSGS.%d", doorpath, temp_player.bbs);
		sprintf(filename, "%sOUT%c%s_M.%d", doorpath, PATH_DELIM, ibbsgametitle, temp_player.bbs);
		// open the file
		if((playerfile=nopen(filename,O_WRONLY|O_CREAT|O_APPEND))==-1)
		{
			// couldn't open the file
			od_printf("`bright red`ERROR: Couldn't create/open the OUT%c%s_M.%d file\n\r", PATH_DELIM, ibbsgametitle, temp_player.bbs);
			od_log_write("ERROR: Couldn't create/open the GAME_M.[BBS] file\n\r");
			return;
		}
		// write the "to" player's handle and real_name
		write(playerfile, temp_player.names, 21L);
		write(playerfile, temp_player.real_names, 51L);

	}

	// write the "from" players handle, real_name, bbs and message
	write(playerfile, player.names, 21L);
	write(playerfile, player.real_names, 51L);
	player.bbs=LE_SHORT(player.bbs);
	write(playerfile, &player.bbs, 2L);
	player.bbs=LE_SHORT(player.bbs);
//  write(playerfile, msg_text, USER_MSG_LEN);
	msg_length = strlen(msg_text);
	msg_length=LE_SHORT(msg_length);
	write(playerfile, &msg_length, 2L);
	msg_length=LE_SHORT(msg_length);
	write(playerfile, msg_text, msg_length);

	// close the player's .MSG file...
	close(playerfile);

	return;
}

// For ANSI users only!
char EditorHelp( void )
{
	char input1[80], format[200], choice;
	INT16 popup;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  EditorHelp()\n");
	fclose(gac_debug);
	#endif


	if (od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
	{
		// Only for ANSI, RIP or AVATAR
		// save the screen to redisplay
//      od_save_screen( screen);
		// Create a popup window that provides help and the user can pick from it!
		strcpy(input1, "Editor Help (Use NumPad or Arrows)");
		// create the menu string...
		sprintf(format, " /^S Save Message | /^A Abort Message | /^C Color Help | /^? Display Help ");
		popup = od_popup_menu( input1, format ,1 , 1, 1 , MENU_ALLOW_CANCEL);
		// check to see if a selection was made, if so update choice.
		if (popup != POPUP_ESCAPE)
		{
			// set options for use
			strcpy(input1, "SAC?");
			choice = input1[popup - 1];
		}
		else choice = '\0';
		//od_window_remove(input_window);
//      od_restore_screen( screen);
	}
/*
	else
	{
		// For ASCII callers
		od_save_screen( screen);
		od_printf("`Bright Green`Editor Help:\n\r");
		od_printf("     `Bright Blue`/A `DarkBlue`- Abort Message\n\r");
		od_printf("     `Bright Blue`/C`DarkBlue` - Color Help\n\r");
		od_printf("     `Bright Blue`/S`DarkBlue` - Save Message\n\r");
		od_printf("     `Bright Blue`/? `DarkBlue`- Display Help\n\r");
		od_restore_screen( screen);
	}
*/
	return(choice);
}


void ColorHelp( void )
{
	void *input_window;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  ColorHelp()\n");
	fclose(gac_debug);
	#endif

				// save the screen to redisplay
				od_save_screen( screen);


				if(od_control.user_ansi || od_control.user_avatar || od_control.user_rip )
				{
					// Only for ANSI, RIP or AVATAR
					od_clr_scr();
					// Create a background Window for the color help screen
					input_window = od_window_create(3, 2, 77, 19, "Color Help Screen", 0x02, 0x0a, 0x02, 0);
					// Change the color delimiter so I can use the `
					od_control.od_colour_delimiter = '~';

					od_set_cursor(3,5);
					od_printf("   ~Bright Green~If you want to use ~Red~C~Blue~O~Bright White~L~Yellow~O~Cyan~R~MAGENTA~S~Bright Green~ in your messages, just enclose the ");
					od_set_cursor(4,5);
					od_printf(" ~bright green~following color names in back-quotes (e.g. `red`) within your message.");
					od_set_cursor(5,5);
					od_printf("  ~bright green~To use the background colors or modifiers, use the following format");
					od_set_cursor(6, 5);
					od_printf("          ~Bright green~`[Modifier] [Foreground] on [Modifier] [Background]`.");

					// Change the color delimiter back
					od_control.od_colour_delimiter = '`';

					od_set_cursor(9,12);
					od_printf("`bright green`%-25s", "Foreground Colors");
					od_set_cursor(9,47);
					od_printf("`bright green`%-25s", "Background Colors");
					od_set_cursor(10,5);
					od_printf("`bright Black`%-10s`Blue`%-10s`Green`%-10s``", "  Black", "  Blue", "  Green");
					od_set_cursor(10,40);
					od_printf("`white on black`%-10s`white on blue`%-10s`white on green`%-10s``", "  Black", "  Blue", "  Green");
					od_set_cursor(11,5);
					od_printf("`Cyan`%-10s`red`%-10s`Magenta`%-10s``", "  Cyan", "  Red", "  Magenta");
					od_set_cursor(11,40);
					od_printf("`white on Cyan`%-10s`white on red`%-10s`white on Magenta`%-10s``", "  Cyan", "  Red", "  Magenta");
					od_set_cursor(12,5);
					od_printf("`brown`%-10s`white`%-10s`yellow`%-10s``", "  Brown", "  White", "  Yellow");
					od_set_cursor(12,40);
					od_printf("`white on brown`%-10s`black on white`%-10s``", "  Brown", "  White");

					od_set_cursor(14,12);
					od_printf("`bright green`Color Modifiers");
					od_set_cursor(15,5);
					od_printf("  `Bright green`Bright    `Dark green`Dark    `Green on Flashing`Flashing``");

					// Change the color delimiter so I can use the `
					od_control.od_colour_delimiter = '~';
					od_set_cursor(14,49);
					od_printf("~bright green~%-20s", "Some Examples");
					od_set_cursor(15,40);
					od_printf("~blue on grey~`Blue on grey`~~ ~bright red on blue~`Bright red on blue`~~");
					od_set_cursor(16,40);
					od_printf("  ~bright white on flashing red~`Bright white on flashing red`~~");
					// Change the color delimiter back
					od_control.od_colour_delimiter = '`';

										od_set_cursor(18,22);
					od_printf("`dark green`< `bright green`Press any key to continue`dark green` >");
					od_get_key(TRUE);


					od_window_remove(input_window);
				}
				else
				{
					// For ASCII callers
					od_printf("`Bright Green`Color Help Screen\n\r `Dark Green`(Note: you may not be able to see colors, but others will)\n\r");

					od_printf("   ~Bright Green~If you want to use ~Red~C~Blue~O~Bright White~L~Yellow~O~Cyan~R~MAGENTA~S~Bright Green~ in your messages, just enclose the \n\r");
					od_printf(" ~bright green~following color names in back-quotes (e.g. `red`) within your message.\n\r");
					od_printf("  ~bright green~To use the background colors or modifiers, use the following format\n\r");
					od_printf("          ~Bright green~`[Modifier] [Foreground] on [Modifier] [Background]`.\n\r");

					// Change the color delimiter back
					od_control.od_colour_delimiter = '`';

					od_printf("`bright green`%-25s", "Foreground Colors\n\r");
					od_printf("`bright Black`%-10s`Blue`%-10s`Green`%-10s``\n\r", "  Black", "  Blue", "  Green");
					od_printf("`Cyan`%-10s`red`%-10s`Magenta`%-10s``\n\r", "  Cyan", "  Red", "  Magenta");
					od_printf("`brown`%-10s`white`%-10s`yellow`%-10s``\n\r", "  Brown", "  White", "  Yellow");


					od_printf("`bright green`%-25s", "Background Colors");
					od_printf("`white on black`%-10s`white on blue`%-10s`white on green`%-10s``\n\r", "  Black", "  Blue", "  Green");
					od_printf("`white on Cyan`%-10s`white on red`%-10s`white on Magenta`%-10s``\n\r", "  Cyan", "  Red", "  Magenta");
					od_printf("`white on brown`%-10s`black on white`%-10s``\n\r", "  Brown", "  White");

					od_printf("`bright green`Color Modifiers");
					od_printf("  `Bright green`Bright    `Dark green`Dark    `Green on Flashing`Flashing``\n\r");

					// Change the color delimiter so I can use the `
					od_control.od_colour_delimiter = '~';
					od_printf("~bright green~%-20s", "Some Examples");
					od_printf("~blue on grey~`Blue on grey`~~ ~bright red on blue~`Bright red on blue`~~\n\r");
					od_printf("  ~bright white on flashing red~`Bright white on flashing red`~~\n\r");
					// Change the color delimiter back
					od_control.od_colour_delimiter = '`';
					gac_pause();

				}

					// restores the screen
					od_restore_screen( screen);


	return;
}




// ===========================================================================
// This function will accept the players input, do a search for the name, assign the info
// to the temp_player and will return a -1 if not found;
// ===========================================================================
INT16 CheckPlayerName( char *input)
{
	char choice = '\0';
	struct player_list *current;
	char tempprompt[200];

	// check for info in the player listing
	// Get the players list (Upadtye the info...)
	GetPlayersList(TRUE);
	// Set current equal to the start
	current = list;
	// Loop to process the list the Players
	while (current != NULL)
	{
		if (g_strstr(current->names, input) != NULL)
		{
			// Ask user if this is the person they meant
			sprintf(tempprompt, "`cyan`Do you mean `bright cyan`%s `cyan`on `bright cyan`%s`cyan` (`bright cyan`Y`cyan`/`bright cyan`n`cyan`) ?", current->names, getbbsname(current->bbs));

			choice = PromptBox(tempprompt, "", "YN\r\n", FALSE);
			if (choice != 'N')
			{
				// Next get the info from the player.dat file for this user
				strcpy(temp_player.names, current->names);
				strcpy(temp_player.real_names, current->real_names);
				temp_player.account = current->account;
				temp_player.bbs = current->bbs;
				temp_player.wealth = current->wealth;

				return(0);
			}
		}

		// Increase the current pointer by one
		current = current->next;
	}

	//Let the user know that the input was a bad node number

	sprintf(tempprompt, "`Bright cyan`%s`cyan` is an Invalid Name!  List the players for valid names.", input);
	PromptBox(tempprompt, "Press any key to continue", "ANY", FALSE);
	return(-1);
}


// ===========================================================================
// This function will do a non-case sensitive string comparison
// Specifically it is used for partial names...
// ===========================================================================
char *g_strstr(char *name, char *input)
{
	char newname[21], newinput[21];
	INT16 i = 0;

	strcpy(newname, name);
	strcpy(newinput, input);

	while (newname[i] != '\0')
	{
		newname[i] = toupper(newname[i]);
		i++;
	}
	i = 0;
	while (newinput[i] != '\0')
	{
		newinput[i] = toupper(newinput[i]);
		i++;
	}

	return( strstr(newname, newinput) );
}




// ===========================================================================
// This function checks to see if the user has any mail waiting, and will allow
// him/her to respond to the mail if wanted.
// ===========================================================================
void CheckMail( void )
{

	char filename[128], handle[21], oldname[128];
// static char buffer[USER_MSG_LEN];
	char response, templine[80];
	int file;
	INT16 i, y, msg_length;
	struct player_list *current;

	char line;  // 12/96

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  CheckMail()\n");
	fclose(gac_debug);
	#endif


	// create the filename
	sprintf(filename, "%s%d.tmp", doorpath, player.account);
	sprintf(oldname, "%s%d.msg", doorpath, player.account);

	// see if the file exists, if it does not then return
	if (access(oldname, 00) != 0) return;

	// Rename the message file, so a player can read it and still receive mail...
	// (dirty, but simple solution)
	if (access(filename, 00) == 0) remove(filename);
	rename(oldname, filename);

	// open the file
	if((file=nopen(filename,O_RDONLY|O_DENYNONE))==-1)
	{
		// couldn't open the file
		od_printf("`bright red`ERROR: Couldn't open the Player's .msg file\n\r");
		od_log_write("ERROR: Couldn't open the Player's .msg file\n\r");
		return;
	}

	// loop as long as we are able to read a handle from the file
	while (read( file, temp_player.names, 21L) != 0)
	{
		// read in the from player's real name and text
		read(file, temp_player.real_names, 51L);
		read(file, &temp_player.bbs, 2L);
		temp_player.bbs=LE_SHORT(temp_player.bbs);
//      read(file, buffer, USER_MSG_LEN);
		read(file, &msg_length, 2L);
		msg_length=LE_SHORT(msg_length);
		// 12/96 Clear message buffer before placing more text there.
		memset(msg_text, '\0', sizeof(msg_text));
		read(file, msg_text, msg_length);


		od_clr_scr();
		// Display the text
		g_send_file("gac_15");

		if (od_control.user_ansi)
		{
			od_set_cursor(6,7);
			od_printf("`cyan`From: `bright cyan`%s`cyan` on `bright cyan`%s`cyan`\n\r\n\r", temp_player.names, getbbsname(temp_player.bbs));
			od_set_cursor(8,1);
		}
		else
		{
			od_printf("`cyan`From: `bright cyan`%s`cyan` on `bright cyan`%s`cyan`\n\r\n\r", temp_player.names, getbbsname(temp_player.bbs));
		}

		// print the buffer to the screen
		y = 0;
		// 12/96 Allow more than MAX_EDIT_LINES to be shown
		line=0;
		for (i=0;i<USER_MSG_LEN;i++)
		{
			if (msg_text[i] == '\0')
				break;
			// 12/96 else if (msg_text[i] == '\r' || msg_text[i] == '\n')
			else if (msg_text[i] == '\r' || msg_text[i] == '\n')
			{
				templine[y] = '\0';
				strcat(templine, "\r\n");
				// 12/96 i++;
				od_printf(templine);
				line++; // 12/96
				y=0;
				templine[y] = '\0';
			}
			else
			{
				templine[y] = msg_text[i];
				y++;
			}

			if (line >=MAX_EDIT_LINES)
			{
				gac_pause();
				line=0;
				od_clr_scr();
				// Display the text
				g_send_file("gac_15");

				if (od_control.user_ansi)
				{
					od_set_cursor(6,7);
					od_printf("`cyan`From: `bright cyan`%s`cyan` on `bright cyan`%s`cyan` <`bright cyan`Continued`cyan`>\n\r\n\r", temp_player.names, getbbsname(temp_player.bbs));
					od_set_cursor(8,1);
				}
				else
				{
					od_printf("`cyan`From: `bright cyan`%s`cyan` on `bright cyan`%s`cyan`\n\r\n\r", temp_player.names, getbbsname(temp_player.bbs));
				}
			}
		}
		gac_pause();
		
		// Ask the user if they want to respond
		response = PromptBox("`cyan`Do you want to reply to this message (`bright cyan`y`cyan`/`bright cyan`N`cyan`) ?", "", "YN\n\r", FALSE);
		if (response == 'Y')
		{
			// If the user that the message is from is on this BBS, then we need to obtain
			// the account number...
			if (temp_player.bbs == thisbbs)
			{
				// In case the player is not on this system, just continue

				// copy the handle to a holding variable, since the GetPlayersList uses the temp_player structure
				strcpy(handle, temp_player.names);
				// set temp_player.names = to nothing so we can detect a match
				temp_player.names[0] = '\0';
				// check for info in the player listing
				// Get the players list (Upadtye the info...)
				GetPlayersList(TRUE);
				// Set current equal to the start
				current = list;
				// Loop to process the list the Players
				while (current != NULL)
				{
					if (stricmp(current->names, handle) == 0)
					{
						// Next get the info from the player.dat file for this user
						strcpy(temp_player.names, current->names);
						strcpy(temp_player.real_names, current->real_names);
						temp_player.account = current->account;
						temp_player.bbs = current->bbs;
						temp_player.wealth = current->wealth;
					}
					// Increase the current pointer by one
					current = current->next;
				}
				// make sure a match was found
				if (stricmp(temp_player.names, handle) != 0)
				{
					PromptBox("The user was not found in the database, unable to respond.", "Press any key to continue", "ANY", FALSE);
					continue;
				}
			}
			SendIBBSMsg(TRUE);
		}
	}

	// close and remove the .MSG file
	close(file);
	remove(filename);
	return;
}

INT16 CheckMaintenance( void )
{
	FILE *warfile;
	char curdate[9], maintdate[9];
	time_t	now;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  CheckMaintenance()\n");
	fclose(gac_debug);
	#endif


	// get the current date as a string
	now=time(NULL);
	strftime(curdate,sizeof(curdate),"%m/%d/%y",localtime(&now));
	//_strdate(curdate);

	// make the filename to store the date in
	sprintf(filename, "%s%s_d.dat", doorpath, ibbsgametitle);

	warfile = fopen(filename, "rb");
	if (warfile == NULL)
	{
		od_printf("`BRIGHT RED`ERROR %s does not exist\n\r", filename);
		sprintf(prompt, "ERROR: %s_d.dat does not exist file", ibbsgametitle);
		od_log_write(prompt);

		warfile = fopen(filename, "wb");
		if (warfile == NULL)
		{
			od_printf("`BRIGHT RED`ERROR CREATING %s\n\r", filename);
			sprintf(prompt, "ERROR Creating: %s_d.dat file", ibbsgametitle);
			od_log_write(prompt);
			return(TRUE);
		}
		fprintf(warfile, "%s\r\n", curdate);
		fclose(warfile);
		return(TRUE);
	}
	fscanf(warfile, "%s\r\n", maintdate);
	fclose(warfile);

	if (stricmp(curdate, maintdate) != 0) return(TRUE);
	else return(FALSE);

}




// Used for TIMEUPDATE ONLY
void UpdateTime( void )
{
	FILE *chat;
//  char filename[128];

	time_t times;
	long int uses;


	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  UpdateTime()\n");
	fclose(gac_debug);
	#endif

		// If the Gypsy.dat file exists, read in the current balances
	sprintf(filename, "%s%s_s.dat", doorpath, ibbsgametitle);
	if ( access(filename, 00) == 0)
	{
		chat = gopen(filename, O_BINARY, O_RDONLY);
			fread(&times, sizeof(time_t), 1, chat);
			times=LE_LONG(times);
			fread(&uses, sizeof(long int), 1, chat);
			uses=LE_LONG(uses);
			fclose(chat);
	}
	else
	{
			times = time(NULL);
			uses = 0;

	}

	if (uses == 0) times=time(NULL);
	uses += 1;

	//copy to global variables for other uses
	total_uses = uses;
	first_time = times;


	// Update the Gypsy stats in the file
	sprintf(filename, "%s%s_s.dat", doorpath, ibbsgametitle);
	chat = gopen(filename, O_BINARY, O_DENYALL);
	times=LE_LONG(times);
	fwrite(&times, sizeof(time_t), 1, chat);
	times=LE_LONG(times);
	uses=LE_LONG(uses);
	fwrite(&uses, sizeof(long int), 1, chat);
	uses=LE_LONG(uses);
	fclose(chat);

	return;
}

// This function allows me to send a file simply
// it will send the individual file if available, otherwise it will send
// the one in the archive xxxxANSI.ART, xxxxASC.ART, or xxxxRIP.ART files

INT16 g_send_file( char *filename)
{
	char sendfile[128];
	INT16 sent=FALSE;


	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  g_send_file()\n");
	fclose(gac_debug);
	#endif


	if (od_control.user_rip)
	{
		sprintf(sendfile, "%s%s.rip", doorpath, filename);
		if ((sent = od_send_file(sendfile)) == FALSE)
		{
			// attempt to send one from our archive
			sent = SendArchive(filename, RIP_FILE);
		}
	}

	if (od_control.user_ansi && sent==FALSE)
	{
		sprintf(sendfile, "%s%s.ans", doorpath, filename);
		if ((sent = od_send_file(sendfile)) == FALSE)
		{
			// attempt to send one from our archive
			sent = SendArchive(filename, ANS_FILE);
		}
	}

	if (sent == FALSE)
	{
		sprintf(sendfile, "%s%s.asc", doorpath, filename);
		if ((sent = od_send_file(sendfile)) == FALSE)
		{
			// attempt to send one from our archive
			sent = SendArchive(filename, ASC_FILE);
		}
	}

//  sprintf(sendfile, "%s%s", doorpath, filename);
//  sent = od_send_file(sendfile);

	// pause slightly and ignore keyboard to stop ctrl-e
	// answerbacks to the ANSIs...
/*
	if (answered == TRUE)
	{
		od_sleep(500);
		if(od_get_key(FALSE) != '\0')
		{
			while(od_get_key(FALSE) != '\0');
		}
	}
*/
	return( sent);


}

// will attempt to open a ART archive and send the file requested.
// returns TRUE if sent successfully
INT16 SendArchive(char *file, INT16 type)
{

		char archive[128], line[261], keyword[15];
	FILE *arcfile;
	INT16 found=FALSE, done=FALSE, encrypted=FALSE;
	char *p;

	if (type == RIP_FILE)
		sprintf(archive, "%s%4.4srip.art", doorpath, executable);
	else if (type == ANS_FILE)
		sprintf(archive, "%s%4.4sans.art", doorpath, executable);
	else if (type == ASC_FILE)
		sprintf(archive, "%s%4.4sasc.art", doorpath, executable);

//od_printf("%s\n\r", archive);
//od_get_key(TRUE);


	// open the file and search for the string #@file then display everything
	// until we get to #@ again

	if (access(archive, 00) != 0) return(FALSE);

	arcfile = myopen(archive, "rb", SH_DENYWR);
	if (arcfile == NULL) return(FALSE);

	fseek(arcfile, 0, SEEK_SET);
	// read one line at a time
	found=FALSE;
	done = FALSE;
/*  OLD UNENCRYPTED WAY  
	sprintf(keyword, "@#%s", file);
	while (fscanf( arcfile, "%[^\n\r]\r\n", line) == 1 && found == FALSE)
	{
		// look for our keyword...  2+(strlen(file)
		if (strnicmp(line, keyword, 2+strlen(file)) == 0) found = TRUE;
	}
*/
	// NEW Encrypted way...
	sprintf(keyword, "%s", file);
	while (found == FALSE)
	{
		if (fscanf( arcfile, "%[^\r\n]\r\n", line) != 1)
			break;
		// look for a word from our prompt line and the starting @#
		p = &line[2];
		if (strnicmp(line, "@#", 2) == 0)
		{
			if (strnicmp(p, keyword, strlen(keyword)) == 0) {
				found = TRUE;
				break;
			}
			HelpDecrypt(p);

			// if there are blanks on the end remove them...
/*
			while (p[strlen(p)-1] == ' ')
			{
				p[strlen(p)-1] = '\0';
			}
*/
			if (p[0] == '\0')
				found = FALSE;
			else if (stricmp(p , keyword) == 0 ) {
				found = TRUE;
				encrypted = TRUE;
			}
			
		}
	}

	// display the file...
	if (found == TRUE)
	{
		//od_disp_emu(line, TRUE);
		//if (type == ASC_FILE) od_printf("\r\n");

		// display one line at a time until done
		while (fscanf( arcfile, "%[^\r\n]\r\n", line) == 1 && done == FALSE)
		{
			// look for the end
			if (strnicmp(line, "@#", 2) == 0) 
				done = TRUE;
			else
			{
				// display the line
				// od_disp_emu(line, TRUE);
				if(encrypted)
					HelpDecrypt(line);
				od_disp_emu(line, TRUE);
				//if (type == ASC_FILE)
					od_printf("\r\n");

				//allow user to exit out early
				if (od_get_key(FALSE) != 0)
					done = TRUE;
//od_printf("%s\n\r", line);
//od_get_key(TRUE);
			}
		}
//    od_printf("\n\r");
	}

	fclose(arcfile);

	return(found);
}



void HelpDecrypt( char *line)
{
	char *curp;
	
	curp = &line[0];
	while (curp[0] != '\0')
	{

				if (curp[0] > 0 )
						curp[0] -= 128;
				else
						curp[0] += 128; //should be 127
		curp++;

	}

	swab(line, line, sizeof(line));
	strcpy(line, strrev(line));
			
	// if there are blanks on the end remove them...

	while (curp[strlen(curp)-1] == ' ')
	{
		curp[strlen(curp)-1] = '\0';
	}

	return;
}

/*

// will attempt to open a ART archive and send the file requested.
// returns TRUE if sent successfully
INT16 SendArchive(char *file, INT16 type)
{

	char archive[128], line[260], keyword[15];
	FILE *arcfile;
	INT16 found=FALSE, done=FALSE;

	if (type == RIP_FILE)
		sprintf(archive, "%s%4.4srip.art", doorpath, executable);
	else if (type == ANS_FILE)
		sprintf(archive, "%s%4.4sans.art", doorpath, executable);
	else if (type == ASC_FILE)
		sprintf(archive, "%s%4.4sasc.art", doorpath, executable);

//od_printf("%s\n\r", archive);
//od_get_key(TRUE);


	// open the file and search for the string #@file then display everything
	// until we get to #@ again

	if (access(archive, 00) != 0) return(FALSE);

	arcfile = fopen(archive, "rb");
	if (arcfile == NULL) return(FALSE);

	fseek(arcfile, 0, SEEK_SET);
	// read one line at a time
	found=FALSE;
	done = FALSE;
	sprintf(keyword, "@#%s", file);
//od_printf("%s\n\r", keyword);
//od_get_key(TRUE);
	while (fscanf( arcfile, "%[^\r\n]\r\n", line) == 1 && found == FALSE)
	{
		// look for our keyword...  2+(strlen(file)
		if (strnicmp(line, keyword, 2+strlen(file)) == 0) found = TRUE;
//od_printf("%s\n\r", line);
//od_get_key(TRUE);

	}

	// display first line already read in
	if (found == TRUE)
	{
		od_disp_emu(line, TRUE);
		if (type == ASC_FILE) od_printf("\r\n");

		// display one line at a time until done
		while (fscanf( arcfile, "%[^\r\n]\r\n", line) == 1 && done == FALSE)
		{
			// look for our keyword...
			if (strnicmp(line, "@#", 2) == 0) done = TRUE;
			else
			{
				// display the line
				od_disp_emu(line, TRUE);
				if (type == ASC_FILE) od_printf("\r\n");
//od_printf("%s\n\r", line);
//od_get_key(TRUE);
			}
		}
//    od_printf("\n\r");
	}

	fclose(arcfile);

	return(found);
}
*/


// ===========================================================================
// Simply outputs the list of Participating BBSs to the screen
// (good for asking users to pick a BBS number.
void ListBBSs( void )
{
	INT16 i, iCurrentSystem;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  ListBBSs()\n");
	fclose(gac_debug);
	#endif
	
	
	od_clr_scr();
	g_send_file("gac_3");
	if (od_control.user_ansi) od_printf("\n\r");

	// check to see if this is a local only game
	if (thisbbs == 0)
	{
		od_printf("\n\r`bright cyan`This Tournament Game has NOT been setup for Inter-BBS\n\r");
		return;
	}

	// show the BBSs in the Tournament
	i=1;
	for(iCurrentSystem = 0; iCurrentSystem < InterBBSInfo.nTotalSystems; ++iCurrentSystem)
	{
		od_printf("   `bright cyan`%3d`cyan`) - `bright white`%-25.25s  ", i, InterBBSInfo.paOtherSystem[iCurrentSystem].szSystemName);
		i++;
		iCurrentSystem++;
		if (iCurrentSystem < InterBBSInfo.nTotalSystems)
		{
			od_printf("`bright cyan`%3d`cyan`) - `bright white`%-25.25s\n\r", i, InterBBSInfo.paOtherSystem[iCurrentSystem].szSystemName);
		}

		// check for a full page, if so pause and redisplay title
		if (i%24 == 0)
		{
			gac_pause();
			od_clr_scr();
			g_send_file("gac_3");
   		if (od_control.user_ansi) od_printf("\n\r");

		}

		i++;
	}
	// make sure we start on a fresh line
	od_printf("\n\r");

	return;
}

// Menu for displaying score listings =========================================
void ViewScores (void )
{
	char inputbbs[6];

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  ViewScores()\n");
	fclose(gac_debug);
	#endif


	while (1)
	{
		od_clr_scr();
		// Place a cool ANSI next to the menu. if the user has ANSI
		g_send_file("gac_6");
		if (od_control.user_ansi) od_printf("\n\r");

		od_printf("          `bright cyan`1`bright blue`) All player scores for this tournament\r\n");
		od_printf("          `bright cyan`2`bright blue`) Player scores on this BBS\r\n");
		od_printf("          `bright cyan`3`bright blue`) Top 15 players on all BBS's for this tournament\r\n");
		od_printf("          `bright cyan`4`bright blue`) Average BBSs scores for this tournament\r\n");
		od_printf("          `bright cyan`5`bright blue`) Total BBSs scores for this tournament\r\n");
		od_printf("          `bright cyan`6`bright blue`) Scores for all players on a BBS\n\r\n\r");
		od_printf("          `bright cyan`L`bright blue`)ast tournaments top 15 BBSs and top 15 players\r\n");
		od_printf("\n\r          `bright red`Q`bright blue`)uit to main menu\r\n");
		od_printf("\r\n          `Bright Yellow`Which [`bright white`Q`bright yellow`]: ``");

		switch(od_get_answer("123456QL\r"))
		{
			case 'L':
				od_clr_scr();
				if (g_send_file("lst15bbs") == FALSE)
					od_printf("`Bright cyan`\n\r\n\rThere is no record of the top BBSs for the previous tournament.\n\r\n\r");
      		if (od_control.user_ansi) od_printf("\n\r");

				gac_pause();
				od_clr_scr();
				if (g_send_file("lst15ply") == FALSE)
					od_printf("`bright cyan`\n\r\n\rThere is no record of the top players for the previous tournament.\n\r\n\r");
      		if (od_control.user_ansi) od_printf("\n\r");

				gac_pause();
			break;
			case '1':
				ShowPlayersList( ALL );
				gac_pause();
				break;
			case '2':
				ShowPlayersList( thisbbs);
				gac_pause();
				break;
			case '3':
				ShowPlayersList( TOP);
				gac_pause();
				break;
			case '4':
				ViewBBSList(TRUE);
				gac_pause();
				break;
			case '5':
				ViewBBSList(FALSE);
				gac_pause();
				break;
			case '6':
				ListBBSs();
				// check to see if this is a local only game
				if (thisbbs == 0) {
					gac_pause();
					break;
				}
				// display a list of the BBSs
				od_printf("`cyan`Enter the BBS number you wish to see scores for: `bright cyan`");
				od_input_str(inputbbs, 5, '0', '9');
				// see if user hit enter
				if (atoi(inputbbs) == 0) break;
				ShowPlayersList(InterBBSInfo.paOtherSystem[atoi(inputbbs)-1].szNumber);
				gac_pause();
				break;
			case 'Q':
				return;
			case '\r':
				return;
		}
	}

}

// My new file open function, pass same parameters as in fsopen, but I loop
// to determine if we can gain access.  This function ignores the share paramter
// if SHARE.EXE is not loaded.
FILE *myopen(char *str, char *mode, int share )
{
	FILE *returnhandle;
	INT16 count=0;

	// see if the file exists, if it does not and we are trying to read and
	// write, we will create a new file by changing the mode!
	if ((access(str, 00) != 0) && strnicmp(mode, "r+", 2) == 0)
	{
		if (stricmp(mode, "r+b") == 0) strcpy(mode, "w+b");
		else if (stricmp(mode, "r+t") == 0) strcpy(mode, "w+t");
	}

	while ( ((returnhandle=_fsopen(str,mode,share))==NULL) && count++<LOOP_GOPEN)
	{
		#ifndef ODPLAT_WIN32
		if (count%10 == 0)
			od_sleep(50);
		#endif
	}

	if (returnhandle==NULL)
		od_printf("\n\r`BRIGHT RED`FILE OPEN: ACCESS DENIED\n\r");

	return(returnhandle);
}
