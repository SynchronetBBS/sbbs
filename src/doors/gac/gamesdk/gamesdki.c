#include "gamesdk.h"



// ===========================================================================
// Process Outbound information (before the info is actually sent)
// ===========================================================================
void OutProcess( void )
{
	struct player_list *current_player;
	INT16 outaccount=0;
	#ifndef GSDK_OVERLAY
	static tIBResult returned;
	#endif
//  static char buffer[16000];
	char filename[128];
//  unsigned int bufsize = 16000;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  OutProcess()\n");
	fclose(gac_debug);
	#endif

	od_printf("`Bright cyan`Making Outbound Inter-BBS Packages\n\r\n\r");
	od_printf("`cyan`Reading list of players\n\r");
	GetPlayersList(TRUE);
	// Check if a player.dat file exists
	sprintf(filename, "%splayer.dat", doorpath);

	// Init the current players list and the outgoing account number
	current_player = list;
	outaccount = 0;
	// Loop for every user and process the ones from this BBS
	while (current_player != NULL)
	{
		// Check if the player is on this board, if so send their stats out to be updated
		if (current_player->bbs == thisbbs || current_player->bbs == 0)
		{
			// Get a players information
			GetPlayerInfo( &player, current_player->account, TRUE);
			// Write the players information to the outgoing BBS file
			od_printf("`cyan`Writing `bright cyan`%s`cyan`'s Information to OUT%c%s_p.dat file\n\r", player.names, PATH_DELIM, ibbsgametitle);
			// check to see if the player's bbs number is 0, if so set it to thisbbs
			if (player.bbs == 0)
			{
				player.bbs = thisbbs;
				WritePlayerInfo( &player, current_player->account, TRUE);
			}

			WritePlayerInfo( &player, outaccount, FALSE);
			outaccount +=1;
		}
		current_player = current_player->next;
	}

	return;
// Send the created file to all the other systems
// OutBound();

/*
	sprintf(InterBBSInfo.szProgName, "%s League %s", ibbsgametitle, league);
	// Make the name of the file...
	// sprintf(filename, "%sPLAYER.OUT", doorpath);
	sprintf(filename, "%sOUT%c%s_p.dat", doorpath, PATH_DELIM, ibbsgametitle);
	// open the file
	if ( access( filename, 00) != 0)
	{
	 od_printf("No player information to send\n\r");
	}
	else
	{
		od_printf("\n\r`cyan`Sending OUT%c%s_p.dat file to all systems\n\r", PATH_DELIM, ibbsgametitle);
		if( (file=nopen(filename,O_RDONLY))==-1)
		{
			// couldn't open the file
			od_printf("`bright red`ERROR: Couldn't open the OUT%c%s_p.dat file\n\r", PATH_DELIM, ibbsgametitle);
			od_log_write("ERROR: Couldn't open the OUT/game_p.dat file\n\r");
			return;
		}
		lseek(file, 0L, SEEK_SET);
		// Send the PLAYER.OUT file to all systems
		// Send the OUT\%s_P.DAT
		while ( (nbytesread = read( file, buffer,  bufsize)) != 0 )
		{
			returned = IBSendAll(&InterBBSInfo, &buffer, nbytesread, thisbbs, TRUE, FALSE );
			// quit early if there was a problem
			if (returned != eSuccess)
			{
			 ErrorDetect(returned);
			 close(file);
			 break;
			}
		}
		close(file);
		remove(filename);
		ErrorDetect(returned);
	}

	// *** Check for and send any message files to all of the other systems.

	od_printf("\n\r`Bright cyan`Checking for Outbound Inter-BBS Message Files\n\r");
	// Send the created file to all the other systems
	sprintf(InterBBSInfo.szProgName, "%s League %s MSG", ibbsgametitle, league);
	for (i=0;i<InterBBSInfo.nTotalSystems; i++)
	{
		// Make the name of the file...
		// sprintf(filename, "%sOUT_MSGS.%d", doorpath, InterBBSInfo.paOtherSystem[i].szNumber);
		sprintf(filename, "%sOUT%c%s_M.%d", doorpath, PATH_DELIM, ibbsgametitle, InterBBSInfo.paOtherSystem[i].szNumber);
		if (access(filename, 00) != 0) continue;

		od_printf("`cyan`Sending `bright cyan`OUT%c%s_M.%d`cyan` file to `bright cyan`%s\n\r", PATH_DELIM, ibbsgametitle, InterBBSInfo.paOtherSystem[i].szNumber, InterBBSInfo.paOtherSystem[i].szSystemName);
		// open the file
		if( (file=nopen(filename,O_RDONLY))==-1)
		{
			// couldn't open the file
			od_printf("`bright red`ERROR: Couldn't open the OUT%c%s_M.%d file\n\r", PATH_DELIM, ibbsgametitle, i);
			od_log_write("ERROR: Couldn't open an OUT/GAME_M.# file\n\r");
			return;
		}
		lseek(file, 0L, SEEK_SET);
		// Send the file to that system
		while ( (nbytesread = read( file, buffer,  bufsize)) != 0 )
		{
			returned = IBSend(&InterBBSInfo, InterBBSInfo.paOtherSystem[i].szAddress, &buffer, nbytesread, TRUE, FALSE );
			// quit early if there was a problem
			if (returned != eSuccess)
			{
			 ErrorDetect(returned);
			 close(file);
			 break;
			}
		}
		close(file);
		remove(filename);
		ErrorDetect(returned);
	}
*/
//  return;
}



// ===========================================================================
// Process inbound Inter-BBS packets.  Inbound will unpack the file and
// take care of routing.  This function is just supposed to process the
// info in the IN\ directory...
// ===========================================================================
void InProcess( void )
{

	struct player_list *current;
//  static char buffer[16000];
	char filename[128], handle[21], realname[51];
//  unsigned int bufsize = 16000;
	int file, playerfile;
	INT16 newaccount = 0, found = FALSE, i, records, bbs;
	#ifndef GSDK_OVERLAY
	static tIBResult returned;
	#endif
	INT16 msg_length;
	
	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  InProcess()\n");
	fclose(gac_debug);
	#endif

	// 12/96
	ProcessRoute();
	// check for incoming player information...
	// create the filename
	sprintf(filename, "%sIN%c%s_p.dat", doorpath, PATH_DELIM, ibbsgametitle);
	// open the file
	if (access(filename, 00) == 0)
	{
		 if((file=nopen(filename,O_WRONLY|O_CREAT|O_APPEND))==-1)
		 {
		od_printf("`bright red`ERROR: Couldn't create/open the IN%c%s_p.dat file\n\r", PATH_DELIM, ibbsgametitle);
		od_log_write("ERROR: Couldn't create/open the IN/game_p.dat file\n\r");
			return;
		 }

		 records = (int) (filelength(file)/RECORD_LENGTH);
		 close(file);

		// read in the player info from the inbound file, check for the users info
		// need to loop for each player in the IN/GAME_P.DAT (PLAYER.IN) file...
		for (i=0;i< records;i++)
		{
		// *** find the account for this user on this system ***
		// Get the players list into memory each time, in case we get two or more messages about a new player
		GetPlayersList(TRUE);

		// set the current equal to the start of the list
		current = list;
		newaccount = 0;
		found = FALSE;
		// Get this players information from the IN file
		GetPlayerInfo( &player, i, FALSE);

		// check for the users name
		while (current != NULL)
		{
			// Check if this is the user
			if (stricmp(current->real_names, player.real_names) == 0)
			{
				player.account = current->account;
				found = TRUE;
				// stop searching the rest of the list
				break;
			}
			current = current->next;
			// increment the account variable in case a player is not found
			newaccount += 1;
		}
		// Update the users account number to a new account if he/she was not found
		// and Write the players information to our database file
		if (found == FALSE)
		{
			player.account = newaccount;
			WritePlayerInfo( &player, player.account, TRUE);
			od_printf("`cyan`Success updating `bright cyan`%s`cyan`'s information\n\r", player.names);
		}
		else
		{
			// Get the accounts information and make sure this date is after the one in the file...
			// get the local information into opponent (remote is already in player)
			GetPlayerInfo( &opponent, player.account, TRUE);
			// Check to see if the player info is newer than the local (oppnent) info
			if (player.laston >= opponent.laston)
			{
				WritePlayerInfo( &player, player.account, TRUE);
				od_printf("`cyan`Success updating `bright cyan`%s`cyan`'s information\n\r", player.names);
			}
			else
			{
				od_printf("`cyan`Old information for `bright cyan`%s`cyan` ignored\n\r", player.names);
			}
		}
		} // end of for
		// close the file and remove it
		close(file);
		remove(filename);
	} // end of if

	// check for the existence of incoming message data for this board
	// create the filename
	sprintf(filename, "%sIN%c%s_M.%d", doorpath, PATH_DELIM, ibbsgametitle, thisbbs);
	// open the file
	if (access(filename, 00) == 0)
	{
		if((file=nopen(filename,O_RDWR|O_CREAT|O_APPEND))==-1)
		{
			// couldn't open the file
			od_printf("`bright red`ERROR: Couldn't create/open the IN%c%s_M.%d file\n\r", PATH_DELIM, ibbsgametitle, thisbbs);
			od_log_write("ERROR: Couldn't create/open the IN/GAME_M.<BBS> file\n\r");
			return;
		}

		// distribute the messages to the individual users

		lseek(file, 0L, SEEK_SET);

		// *** Read in the to players name, find that account number and write the from player and message text to the file
		// loop while we can read a players handle
		while (read( file, handle, 21L) > 0)
		{

			// Read in the players real name
			read( file, realname, 51L);
			// look for the player in the database
			current = list;
			found = FALSE;
			while (current != NULL)
			{
				if ( (stricmp(current->names, handle) == 0) &&( stricmp(current->real_names, realname) == 0) )
				{
					found = TRUE;
					break;
				}
				current = current->next;
			}

			// see if we found the player, if so write the message to their file
			if (found == TRUE)
			{
				// create the filename
				sprintf(filename, "%s%d.msg", doorpath, current->account);
				// open the file
				if((playerfile=nopen(filename,O_WRONLY|O_CREAT|O_APPEND))==-1)
				{
					// couldn't open the file
					od_printf("`bright red`ERROR: Couldn't create/open the Player's .msg file\n\r");
					od_log_write("ERROR: Couldn't create/open the Player's .msg file\n\r");
					continue;
				}

				od_printf("`cyan`Message for `bright cyan`%s`cyan` received.\n\r", handle);
				// read in the from player's handle and real name and text and write to the players file
				read(file, handle, 21L);
				read(file, realname, 51L);
				read(file, &bbs, 2L);
				bbs=LE_SHORT(bbs);
//              read(file, buffer, USER_MSG_LEN);
				read(file, &msg_length, 2L);
				msg_length=LE_SHORT(msg_length);
				read(file, msg_text, msg_length);
				write(playerfile, handle, 21L);
				write(playerfile, realname, 51L);
				bbs=LE_SHORT(bbs);
				write(playerfile, &bbs, 2L);
				bbs=LE_SHORT(bbs);
//              write(playerfile, buffer, USER_MSG_LEN);
				msg_length=LE_SHORT(msg_length);
				write(playerfile, &msg_length, 2L);
				msg_length=LE_SHORT(msg_length);
				write(playerfile, msg_text, msg_length);

				// close the player's .MSG file...
				close(playerfile);
			}

		}

		// close and remove the incoming.msg file
		close(file);
		// create the filename
		sprintf(filename, "%sIN%c%s_M.%d", doorpath, PATH_DELIM, ibbsgametitle, thisbbs);
		remove(filename);

	}

	return;

}


// ===========================================================================
// Process inbound Inter-BBS packets
// ===========================================================================
// This function is also responsible for updating a players description if
// they changed it...

// this function must look for and open an incoming GAME_R.DAT file and
// process it.  This function could also be the one to write it for sending
void ProcessRoute( void )
{
	int file;
	char filename[128];

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  ProcessRoute()\n");
	fclose(gac_debug);
	#endif

	sprintf(filename, "%sIN%c%s_r.dat", doorpath, PATH_DELIM, ibbsgametitle);
	if (access(filename, 00) != 0) return;

	// write the structure to a file in the out directory...
	if((file=nopen(filename,O_RDWR|O_CREAT))==-1)
		{
			// couldn't open the file
			od_printf("`bright red`ERROR: Couldn't create/open the IN%c%s_r.dat file\n\r", PATH_DELIM, ibbsgametitle);
			od_log_write("ERROR: Couldn't create/open the IN/game_r.dat file\n\r");
			return;
		}

	lseek(file, 0L, SEEK_SET);

	// Write the routing info to the file
	read(file, routing.datebuf, sizeof(routing.datebuf));
	read(file, routing.timebuf, sizeof(routing.timebuf));
	read(file, &(routing.first), sizeof(routing.first));
	routing.first=LE_LONG(routing.first);
	read(file, &(routing.second), sizeof(routing.second));
	routing.second=LE_LONG(routing.second);
	read(file, &(routing.routetime), sizeof(routing.routetime));
	routing.routetime=LE_LONG(routing.routetime);
	read(file, routing.version, sizeof(routing.version));
	read(file, &(routing.from_bbs), sizeof(routing.from_bbs));
	routing.from_bbs=LE_SHORT(routing.from_bbs);
	read(file, &(routing.registered), sizeof(routing.registered));
	routing.registered=LE_SHORT(routing.registered);
	close(file);
	// 12/96
	remove(filename);

	routing.second = time(NULL);  // gets the system time
	routing.routetime = difftime(routing.second, routing.first);

	od_printf("\n\r`Cyan`     - Origin `bright cyan`%s`cyan`      Routing time `bright cyan`%2.2ld:%2.2ld`cyan` (hours:min)\n\r", getbbsname(routing.from_bbs), routing.routetime/3600, (routing.routetime%3600)/60);
	od_printf("`cyan`       Packet created on `bright cyan`%s`cyan` at `bright cyan`%s`cyan`\n\r",routing.datebuf,routing.timebuf);
	od_printf("`cyan`       Remote Version `bright cyan`%s`cyan` (`bright cyan`%s`cyan`)\n\r", routing.version, (routing.registered == TRUE)?"Registered" : "UNREGISTERED");

	UpdateRouteFile();

	return;
}

void UpdateRouteFile( void )
{

//  char filename[128];
	FILE *warfile;
	INT16 i;
	char blankbuf[ROUTEINFOSIZE+1];


	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  UpdateRouteFile()\n");
	fclose(gac_debug);
	#endif

	sprintf(filename, "%srouteinf.dat", doorpath);

	if (access(filename, 00) != 0)
	{
		// init the file to zeros...
		// Open the WAR info file to read in information
		warfile = gopen(filename, O_BINARY, O_DENYALL);
		if (warfile == NULL)
		{
			od_printf("`BRIGHT RED`ERROR OPENING %s\n\r", filename);
			sprintf(filename, "ERROR Opening: routeinf.dat file");
			od_log_write(filename);
			return;
		}
		// Set SEEK to start of file
		fseek(warfile, 0L, SEEK_SET);

		memset(blankbuf, '0', ROUTEINFOSIZE);
		blankbuf[ROUTEINFOSIZE]=0;
		//setmem(blankbuf, ROUTEINFOSIZE, '0');
//od_printf("Blankbuf - %s\n\r", blankbuf);
//od_get_key(TRUE);
		// write enough zeros for 256 BBSs
		for (i=0; i<=256; i++)
		{
			fwrite(blankbuf, ROUTEINFOSIZE, 1, warfile);
		}

		fclose(warfile);
	}
	// Open the WAR info file to read in information
	warfile = gopen(filename, O_BINARY, O_DENYALL);
	if (warfile == NULL)
	{
		od_printf("`BRIGHT RED`ERROR OPENING %s\n\r", filename);
		sprintf(filename, "ERROR Opening: routeinf.dat file");
		od_log_write(filename);
		return;
	}
	// Set SEEK to start of file
	fseek(warfile, 0L, SEEK_SET);

	// seek to the correct record
	fseek(warfile, ROUTEINFOSIZE*routing.from_bbs, SEEK_SET);

	// Write the information
	// 12/96
	fwrite(routing.datebuf, sizeof(routing.datebuf), 1,warfile);
	fwrite(routing.timebuf, sizeof(routing.timebuf), 1,warfile);
	routing.first=LE_LONG(routing.first);
	fwrite(&(routing.first), sizeof(routing.first), 1,warfile);
	routing.first=LE_LONG(routing.first);
	routing.second=LE_LONG(routing.second);
	fwrite(&(routing.second), sizeof(routing.second), 1,warfile);
	routing.second=LE_LONG(routing.second);
	routing.routetime=LE_LONG(routing.routetime);
	fwrite(&(routing.routetime), sizeof(routing.routetime), 1,warfile);
	routing.routetime=LE_LONG(routing.routetime);
	fwrite(routing.version, sizeof(routing.version), 1,warfile);
	routing.from_bbs=LE_SHORT(routing.from_bbs);
	fwrite(&(routing.from_bbs), sizeof(routing.from_bbs), 1,warfile);
	routing.from_bbs=LE_SHORT(routing.from_bbs);
	routing.registered=LE_SHORT(routing.registered);
	fwrite(&(routing.registered), sizeof(routing.registered), 1,warfile);
	routing.registered=LE_SHORT(routing.registered);

	fclose(warfile);
	return;
}

// GAC 10/96 modified the routine to not need a netmail message to process
// incoming information

void FAR16 Inbound( INT16 night )
{

	// 12/96 Tempnode 25 characters
	char tempnode[25]; //, tempadd[25];
	// 12/96 Added found fromBBS=FALSE;
	char foundFromBBS=FALSE;
	INT16 tempbbs, system; //, first=TRUE;
	#ifndef GSDK_OVERLAY
	static tIBResult returned;
	#endif
	FILE *config, *fromfile;
   int file;
//  unsigned attrib;
	char zipfile[15];
	INT16 i,j;
	INT16 value1, value2, value3;

	INT16 done=-1;

	char filename[256];

	glob_t	ffblk;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  Inbound()\n");
	fclose(gac_debug);
	#endif

//printf("In Inbound\n");
//getch();

	chdir(doorpath);

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

	od_clr_scr();
	od_printf("`Brigth cyan`Processing Incoming %s Inter-BBS Packages\n\r\n\r", od_control.od_prog_name);
	// Check for inbound messages
	od_printf("`bright Cyan`  - Searching for Incoming Player Information.\n\r\n\r");

	// Check for inbound player data messages
	sprintf(InterBBSInfo.szProgName, "%-.20s League %-.3s", ibbsgametitle, league);

	// check to see if this is cleanup processing and if there are files to find
//    if (night == CLEANUP)
//    {
	 od_printf("`bright cyan`     - Finding First Inbound File\n\r");
	 sprintf(filename, "%s%2.2s*.%.3s", inbounddir, ibbsid, league);
	 done = glob(filename, 0, NULL, &ffblk);
//    }

	// new PKUNZIP routines
	/*
	while (
		((returned = IBGet(&InterBBSInfo, msg_text, MSG_LENGTH-1 ) ) == eSuccess
		&& night != CLEANUP) ||
		(!done)
		)
	*/

//printf("Before While\n");
//getch();

	for(j=0; j<ffblk.gl_pathc; j++)
	{
		// copy the filename into our subject line (to avoid changing all
		// references)
		sprintf(MessageHeader.szSubject, "%s%s", inbounddir, ffblk.gl_pathv[j]);
		od_printf("`cyan`     - Extracting frombbs.dat\n\r");
		// unzip the FROMBBS.DAT file from the incoming file into the in dir.
		#ifdef ODPLAT_NIX
		sprintf(filename, "unzip -Lo %s frombbs.dat > /dev/null", MessageHeader.szSubject);
		#else
		sprintf(filename, "PKUNZIP.EXE -o %s frombbs.dat >NUL", MessageHeader.szSubject);
		#endif
		if (od_spawn(filename) == FALSE)
		{
			#ifdef ODPLAT_NIX
			od_printf("`bright red`ERROR: unzip execution error.\n\r");
			od_log_write("ERROR: unzip execution error.");
			#else
			od_printf("`bright red`ERROR: PKUNZIP execution error.\n\r");
			od_log_write("ERROR: PKUNZIP execution error.");
			#endif
			// remove(MessageHeader.szSubject);
		}

		// get the from bbs's node number from our data file...
		sprintf(filename, "%sfrombbs.dat", doorpath);
		fromfile = fopen(filename, "rb");

		if (file == -1 )
		{
			od_printf("`BRIGHT RED`ERROR OPENING %s\n\r", filename);
			sprintf(prompt, "ERROR Opening: frombbs.dat file");
			od_log_write(prompt);

			// return;
			// 12/96 commented out
			// night = CLEANUP; // a problem finding the FROMBBS.DAT file
			foundFromBBS=FALSE;
		}
		else
		{
			fscanf(fromfile, "%s\r\n", tempnode);
			fclose(fromfile);
			// 12/96
			foundFromBBS = TRUE;
		}

	//if (night != CLEANUP)
	//{
		// build a net/node string and search to determine which BBS number
		// the file came from.
		// Removed on 10/96
		// sprintf(tempnode, "%d/%d", MessageHeader.wOrigNet, MessageHeader.wOrigNode);

		// default to not sending information to ourself
		tempbbs = thisbbs;
		for (system=0; system<InterBBSInfo.nTotalSystems ;system++)
		{
			if (strstr(InterBBSInfo.paOtherSystem[system].szAddress, tempnode) != NULL)
			{
				tempbbs = InterBBSInfo.paOtherSystem[system].szNumber;
				// end early
				system = InterBBSInfo.nTotalSystems+1;
			}
		}

		// Unzip the incoming file
		od_printf("`bright cyan`     Processing inbound data from `bright cyan`%s\n\r", getbbsname(tempbbs));
	/*
	}
	else
	{
		od_printf("`bright cyan`     Processing orphaned inbound data\n\r");
		strcpy(MessageHeader.szSubject, ffblk.gl_pathv[j]);
	}
	*/

		od_printf("`bright cyan`     - Unzipping data\n\r");
		// unzip it into the door directory (forces overwriting)

//      sprintf(filename, "-o %s >NUL", MessageHeader.szSubject);
//      if (spawnlp(P_WAIT,"PKUNZIP.EXE", filename, NULL) == -1)

		// had to add a check to make sure the file exists, if not then add
		// the inbound path to the file and make it work that way...
		if (access(MessageHeader.szSubject, 00) != 0)
		{

			for (i = 0; i<12; i++)
			{
				// GAC 10/96 Modified this to concant the correct filename to the end
				// of the inbound path.
				zipfile[i] = MessageHeader.szSubject[strlen(MessageHeader.szSubject) - 9 - strlen(league) + i];
//                zipfile[i] = MessageHeader.szSubject[i];
				//exit early if we find the end of the string
				if (MessageHeader.szSubject[strlen(MessageHeader.szSubject) - 9 - strlen(league)  + i] == '\0')
//                if (MessageHeader.szSubject[i] == '\0')
					i = 13;
			}
			zipfile[12] = '\0';
			strcpy(MessageHeader.szSubject, inbounddir);
			strcat(MessageHeader.szSubject, zipfile);
			if (access(MessageHeader.szSubject, 00) != 0)
			{
				od_printf("`bright red`     ERROR: Opening %s\r\n", MessageHeader.szSubject);
				od_log_write("ERROR: Opening");
				od_log_write(MessageHeader.szSubject);
			}
		}

		// unpack anything with a .DAT extension or an extension of our bbs number
		// into the IN directory for processing... GAME*.DAT GAME*.<thisbbs>
		#ifdef ODPLAT_NIX
		sprintf(filename, "unzip -Lo %s %s*.dat %s*.%d IN%c > /dev/null",
			 MessageHeader.szSubject, ibbsgametitle, ibbsgametitle, thisbbs, PATH_DELIM);
		#else
		sprintf(filename, "PKUNZIP.EXE -o %s %s*.dat %s*.%d IN%c >NUL",
			 MessageHeader.szSubject, ibbsgametitle, ibbsgametitle, thisbbs, PATH_DELIM);
		#endif
		if (od_spawn(filename) == FALSE)
		{
			#ifdef ODPLAT_NIX
			od_printf("`bright red`ERROR: unzip execution error.\n\r");
			od_log_write("ERROR: unzip execution error.");
			#else
			od_printf("`bright red`ERROR: PKUNZIP execution error.\n\r");
			od_log_write("ERROR: PKUNZIP execution error.");
			#endif
			remove(MessageHeader.szSubject);
		}

		// if the file is from the first bbs, see if there are any exe files in it
		if (tempbbs == 1 || night == CLEANUP)
		{
			// only want to get .EXEs from BBS 1 (and we are 1)
			if (thisbbs != 1) 
			{
				#ifdef ODPLAT_NIX
				sprintf(filename, "unzip -Lo %s *.exe in%c > /dev/null", MessageHeader.szSubject, PATH_DELIM);
				#else
				sprintf(filename, "PKUNZIP.EXE -o %s *.exe in%c >NUL", MessageHeader.szSubject, PATH_DELIM);
				#endif

				if (od_spawn(filename) == FALSE)
				{
					#ifdef ODPLAT_NIX
					od_printf("`bright red`ERROR: unzip execution error.\n\r");
					od_log_write("ERROR: unzip execution error.");
					#else
					od_printf("`bright red`ERROR: PKUNZIP execution error.\n\r");
					od_log_write("ERROR: PKUNZIP execution error.");
					#endif
					remove(MessageHeader.szSubject);
				}
			}

			// unpack all other file updates from BBS 1 into our directory
			if (thisbbs != 1) {
				#ifdef ODPLAY_NIX
				sprintf(filename, "unzip -Lo %s -x*.exe -x%s.%d -x%s*.dat >NUL",
				  MessageHeader.szSubject, ibbsgametitle, thisbbs, ibbsgametitle);
				#else
				sprintf(filename, "PKUNZIP.EXE -o %s -x*.exe -x%s.%d -x%s*.dat >NUL",
				  MessageHeader.szSubject, ibbsgametitle, thisbbs, ibbsgametitle);
				#endif
			}

			if (od_spawn(filename) == FALSE)
			{
				#ifdef ODPLAT_NIX
				od_printf("`bright red`ERROR: unzip execution error.\n\r");
				od_log_write("ERROR: unzip execution error.");
				#else
				od_printf("`bright red`ERROR: PKUNZIP execution error.\n\r");
				od_log_write("ERROR: PKUNZIP execution error.");
				#endif
				remove(MessageHeader.szSubject);
			}
			

		}

		// copy the file to OUTBOUND\TEMPFILE.ZIP
/*
		od_printf("`bright cyan`     - Copying file for routing\n\r");
		sprintf(filename, "%sOUTBOUND%ctempfile.zip", doorpath, PATH_DELIM);
		if (access(MessageHeader.szSubject, 00) == 0)
			copyfile(MessageHeader.szSubject, filename);
*/
//      remove(MessageHeader.szSubject);
/*
		sprintf(filename, "OUTBOUND%ctempfile.zip", PATH_DELIM);
		if (copyfile(MessageHeader.szSubject, filename)==-1)
		{
			od_printf("`bright red`ERROR: Copying file for routing.\n\r");
			od_log_write("ERROR: Copying file for routing.");
			remove(MessageHeader.szSubject);
			od_exit(-1, FALSE);
		}
*/

		/////// process the information for this bbs
		InProcess();

		// send the information to the other systems if needed
		// 12/96 Modified to only route if a FROMBBS.dat exists
		if (night != CLEANUP && foundFromBBS == TRUE) 
			SendIBBSData(TRUE, tempbbs);
		else
		{
			od_printf("`bright cyan`     - Will not route information without frombbs.dat file.\n\r");
		}

		// remove the file...
		remove(MessageHeader.szSubject);
		// end of new while loop, to only process one message at a time

		// find the next file if we are doing cleanup
		// if (night == CLEANUP)

	}
	done = TRUE;

	// check for a reset file
	sprintf(filename, "%sIN%c%s_n.dat", doorpath, PATH_DELIM, ibbsgametitle);

	if (access(filename, 00) == 0)
	{
		config = fopen(filename, "rb");
		fread(&value1, 2, 1, config);
		value1=LE_SHORT(value1);
		fread(&value2, 2, 1, config);
		value2=LE_SHORT(value2);
		fread(&value3, 2, 1, config);
		value3=LE_SHORT(value3);

		fclose(config);
		remove(filename);
		if (value1 == 4 && value2 == 18 && value3 == 72)
		{
			od_printf("`bright cyan`     - Forced reset message found\n\r");
			od_log_write("Game Reset from remote system");
			// Set the Tournament Length to a negative number to insure that the system is reset properly
			tourneylength = -5;
			CheckDate();
		}
	}

	od_printf("\n\r");

	return;
}

// ===========================================================================
// This functions resets all games in the Inter-BBS Tournament
// ===========================================================================
void FAR16 ResetIBBS( void )
{

	FILE *config;
	INT16 value;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  ResetIBBS()\n");
	fclose(gac_debug);
	#endif

	// check for and create OUT directory
	strcpy(filename, doorpath);
	strcat(filename, "OUT");
	if (access(filename, 00) != 0)
		MKDIR(filename);

	// check if this bbs is number 1
	if (thisbbs > 1)
	{
		od_printf("`bright red`You are not the League Coordinator (BBS #1).\n\r");
		od_printf("`cyan`You can not reset the Inter-BBS Tournament.\n\r\n\r");
		od_printf("`cyan`You may reset your local game, if you want.\n\r");
		od_printf("`bright cyan`NOTE: Your players information will not be deleted from the other BBSs\n\r");
		od_printf("`cyan`Do you wish to reset your local game (y/N)? ");
		if (od_get_answer("YN\r\r") == 'Y')
		{
			od_printf("\n\r`bright cyan`Resetting the local game\n\r");
			od_log_write("Resetting local game");

		}
		else return;
	}
	else if (thisbbs == 1)
	{
		od_printf("`cyan`Are you sure you want to reset the league (y/N)? ");
		if (od_get_answer("YN\r\r") != 'Y')
		return;

		od_printf("`cyan`     - Resetting the Inter-BBS League\n\r");
		od_log_write("Resetting the League");

		// open and create a file called RESET.DAT in the OUT directory
		sprintf(filename, "%sOUT%c%s_n.dat", doorpath, PATH_DELIM, ibbsgametitle);
		config = fopen(filename, "wb");
//      fprintf(config, "041872");
		value = LE_SHORT(4);
		fwrite(&value, 2, 1, config);
		value = LE_SHORT(18);
		fwrite(&value, 2, 1, config);
		value = LE_SHORT(72);
		fwrite(&value, 2, 1, config);
		fclose(config);

		// check for new inbound
		Inbound(FALSE);
		// create the outbound packets
		Outbound(FALSE);
	}
	else // a local game being reset
	{
		od_printf("`cyan`     - Resetting the local game\n\r");
		od_log_write("Resetting local game");
	}

	tourneylength = -5;
	CheckDate();
	return;

}

