#include "datewrap.h"
#include "gamesdk.h"

// ===========================================================================
// Process Outbound Inter-BBS packets AND do maintenance for the day.
// ===========================================================================

// ===========================================================================
// Process the maintenance for the day.
// ===========================================================================
void Maintain( void )
{
	char file[128];
	FILE *config, *warfile;
	char curdate[9];
	time_t now;
	
	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  Maintain()\n");
	fclose(gac_debug);
	#endif

//printf("In Maintain\n");
//getch();

	Inbound(TRUE);
//printf("Inbound\n");
//getch();


	// Update the current day
	sprintf(file, "%scurrent.day", doorpath);

	// Open the file to read in information
	config = gopen(file, O_BINARY, O_DENYALL);
	if (config == NULL)
	{
		od_printf("`bright red`ERROR OPENING %s to update current day.\n\r", file);
		sprintf(file, "ERROR Opening: %s to update current day", file);
		od_log_write(file);
	}
	else
	{
		// set pointer to start of file
		fseek(config, 0L, SEEK_SET);
		if (fread( &currentday, sizeof( INT16 ), 1, config) != 1) currentday = 1;
		else currentday = LE_SHORT(currentday)+1;
		od_printf("`cyan`Advancing current day to `bright cyan`%d`cyan`.\n\r", currentday);
		// Write the current day to the start of the file!

		fseek(config, 0L, SEEK_SET);
		currentday=LE_SHORT(currentday);
		fwrite( &currentday, sizeof( INT16 ), 1, config);
		currentday=LE_SHORT(currentday);
		fclose(config);
	}

	// Create the name of the interbbs file
	sprintf(file, "%sinterbbs.cfg", doorpath);
	// Send the outbound Inter-BBS info, if in a league
	if ( access(file, 00) == 0)
	{
		 Outbound(TRUE);
	}

	// Check to see if we are over the expiration date
	CheckDate();

	now=time(NULL);
	strftime(curdate,sizeof(curdate),"%m/%d/%y",localtime(&now));
	//_strdate(curdate);

	// make the filename to store the date in
	sprintf(filename, "%s%s_d.dat", doorpath, ibbsgametitle);

	warfile = fopen(filename, "wb");
	if (warfile == NULL)
	{
		od_printf("`BRIGHT RED`ERROR CREATING %s\n\r", filename);
		sprintf(prompt, "ERROR Creating: %s_d.dat file", ibbsgametitle);
		od_log_write(prompt);
		return;
	}
	fprintf(warfile, "%s\r\n", curdate);
	fclose(warfile);

	return;
}


// added functionality to send zipped files
void FAR16 Outbound( INT16 night )
{

	char filename[128], filename2[128];


	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  Outbound()\n");
	fclose(gac_debug);
	#endif


	// check for and create OUT directory
	strcpy(filename, doorpath);
	strcat(filename, "OUT");
	if (access(filename, 00) != 0)
		MKDIR(filename);

	strcpy(filename, doorpath);
	strcat(filename, "OUTBOUND");
	if (access(filename, 00) != 0)
		MKDIR(filename);

	chdir(doorpath);

	od_printf("`Bright cyan`Making Outbound Inter-BBS Packages\n\r\n\r");

	if (night == TRUE)
	{
		od_printf("`Cyan`     - Creating outbound player information.\n\r");

		OutProcess(); // have the program create an out packet
		// make the routing time message
		MakeRouteTime();
	}
	else
	{
		od_printf("`Cyan`     - Will ONLY send message information in OUTBOUND mode...\n\r");

		// make the routing time message ONLY if there is info to send...
		sprintf(filename, "OUT%c%s_M.*", PATH_DELIM, ibbsgametitle);
		if (access(filename, 00) == 0)
			MakeRouteTime();
	}

	if (thisbbs != 1)
	{
		// cheating files...
		sprintf(filename, "playinfo.dat");
		sprintf(filename2, "OUT%cplayinfo.dat", PATH_DELIM);
		if (access(filename, 00) == 0)
			rename(filename, filename2);

		sprintf(filename, "newdata.dat");
		sprintf(filename2, "OUT%cnewdata.dat", PATH_DELIM);
		if (access(filename, 00) == 0)
			rename(filename, filename2);
	}
	else if (night == TRUE)
	{
		sprintf(filename, "interbbs.cfg");
		sprintf(filename2, "OUT%cinterbbs.cfg", PATH_DELIM);
		if (access(filename, 00) == 0)
			copyfile(filename, filename2);

	}

	// Send the Inter-BBS Data to the other systems
	SendIBBSData(FALSE, thisbbs);

	return;
}

// this function must write the routing information in the structure to the
// GAME_R.DAT file...
void MakeRouteTime(void)
{

	int file;
	char filename[128];
	time_t	now;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  MakeRouteTime()\n");
	fclose(gac_debug);
	#endif
	
	now=time(NULL);
	strftime(routing.datebuf,sizeof(routing.datebuf),"%m/%d/%y",localtime(&now));
	// _strdate(routing.datebuf);
	strftime(routing.timebuf,sizeof(routing.timebuf),"%H:%M:%S",localtime(&now));
	//_strtime(routing.timebuf);

	routing.first = time(NULL);  // gets the system time

	od_printf("`Cyan`     - Creating routing time information...\n\r");
	od_printf("`cyan`       Current Date: `bright cyan`%s`cyan`  Time: `bright cyan`%s`cyan`\n\r",routing.datebuf,routing.timebuf);

#ifdef KEEP_REG
	if (registered == RK_REGISTERED) routing.registered = TRUE;
	else routing.registered = FALSE;
#else
	routing.registered=TRUE;
#endif

	strcpy(routing.version, currentversion);

	routing.from_bbs = (int) thisbbs;

	sprintf(filename, "%sOUT%c%s_r.dat", doorpath, PATH_DELIM, ibbsgametitle);
	// write the structure to a file in the out directory...
	if((file=nopen(filename,O_RDWR|O_CREAT))==-1)
		{
			// couldn't open the file
			od_printf("`bright red`ERROR: Couldn't create/open the OUT%c%s_r.dat file\n\r", PATH_DELIM, ibbsgametitle);
			od_log_write("ERROR: Couldn't create/open the OUT/game_r.dat file\n\r");
			return;
		}

		lseek(file, 0L, SEEK_SET);

		// Write the routing info to the file
		read(file, routing.datebuf, sizeof(routing.datebuf));
		read(file, routing.timebuf, sizeof(routing.timebuf));
		routing.first=LE_LONG(routing.first);
		read(file, &(routing.first), sizeof(routing.first));
		routing.first=LE_LONG(routing.first);
		routing.second=LE_LONG(routing.second);
		read(file, &(routing.second), sizeof(routing.second));
		routing.second=LE_LONG(routing.second);
		routing.routetime=LE_LONG(routing.routetime);
		read(file, &(routing.routetime), sizeof(routing.routetime));
		routing.routetime=LE_LONG(routing.routetime);
		read(file, routing.version, sizeof(routing.version));
		routing.from_bbs=LE_SHORT(routing.from_bbs);
		read(file, &(routing.from_bbs), sizeof(routing.from_bbs));
		routing.from_bbs=LE_SHORT(routing.from_bbs);
		routing.registered=LE_SHORT(routing.registered);
		read(file, &(routing.registered), sizeof(routing.registered));
		routing.registered=LE_SHORT(routing.registered);
		close(file);


	return;
}

// This function sends the data files to all other systems, it may
// be called from the INBOUND routine to hub data or the OUTBOUND routine
void SendIBBSData(char inbound, INT16 frombbs)
{

	char sendname[128], commandline[128];
	INT16 first = TRUE;
	struct  time t;
	#ifndef GSDK_OVERLAY
	static tIBResult returned;
   #endif
	INT16 system;
	FILE *fromfile;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  SendIBBSData()\n");
	fclose(gac_debug);
	#endif

	// change to the INTRLORD directory
	chdir(doorpath);
	// Send the created file to all the other systems
	sprintf(InterBBSInfo.szProgName, "%-.20s League %-.3s", ibbsgametitle, league);
	strcpy(InterBBSInfo.szFromUserName, InterBBSInfo.szProgName);
	strcpy(InterBBSInfo.szToUserName, InterBBSInfo.szProgName);

	// GAC 10/96 Added the file FROMBBS.DAT to outbound files so we
	// could route w/o netmail

	// make the filename to store the date in
	sprintf(filename, "%sfrombbs.dat", doorpath);

	fromfile = fopen(filename, "wb");
	if (fromfile == NULL)
	{
		od_printf("`BRIGHT RED`ERROR OPENING %s\n\r", filename);
		sprintf(prompt, "ERROR Opening: frombbs.dat file");
		od_log_write(prompt);
		// return;
	}
	fprintf(fromfile, "%s\r\n", ThisBBSAddress);
	fclose(fromfile);

//  sprintf(filename, "%sOUTBOUND%cTEMPFILE.ZIP", doorpath, PATH_DELIM);
	sprintf(filename, "OUTBOUND%ctempfile.zip", PATH_DELIM);
	if (inbound == FALSE)
	{
		sprintf(commandline,"OUT%c*.*",PATH_DELIM);
		if(fexist(commandline)) {
			od_printf("`Cyan`     - Zipping outbound data files...\n\r");
//  if (inbound == TRUE)    sprintf(commandline, "PKZIP.EXE -ex -m %s IN%c*.* >NUL", filename, PATH_DELIM);
			#ifdef ODPLAT_NIX
			sprintf(commandline, "zip -9 -m %s OUT%c*.* > /dev/null", filename, PATH_DELIM);
			#else
			sprintf(commandline, "PKZIP.EXE -ex -m %s OUT%c*.* >NUL", filename, PATH_DELIM);
			#endif
			if (od_spawn(commandline) == FALSE)
//      sprintf(commandline, "%sOUT%c*.*", doorpath, PATH_DELIM);
//      if (spawnlp(P_WAIT,"PKZIP.EXE", "-ex", "-m", filename, commandline, ">NUL", NULL) == -1)
			{
				#ifdef ODPLAT_NIX
				od_printf("`bright red`ERROR: zip execution error.\n\r");
				od_log_write("ERROR: zip execution error.");
				#else
				od_printf("`bright red`ERROR: PKZIP execution error.\n\r");
				od_log_write("ERROR: PKZIP execution error.");
				#endif
				od_exit(-1, FALSE);
			}
		}

		if (access(filename, 00) == 0 && fexist("frombbs.dat"))
		{
			#ifdef ODPLAT_NIX
			sprintf(commandline, "zip -9 -u %s frombbs.dat > /dev/null", filename);
			#else
			sprintf(commandline, "PKZIP.EXE -ex -u %s frombbs.dat >NUL", filename);
			#endif
			if (od_spawn(commandline) == FALSE)
			{
				#ifdef ODPLAT_NIX
				od_printf("`bright red`ERROR: zip execution error.\n\r");
				od_log_write("ERROR: zip execution error.");
				#else
				od_printf("`bright red`ERROR: PKZIP execution error.\n\r");
				od_log_write("ERROR: PKZIP execution error.");
				#endif
				od_exit(-1, FALSE);
			}
		}


	}
	// GAC 10/96 Add FROMBBS.DAT to routed file
	else
	{
		// 12/96 Modified to use .szSubject
		if (access(MessageHeader.szSubject, 00) == 0)
		{
			od_printf("`Cyan`     - Updating outbound data file for routing...\n\r");
		 // 1/97 changed filename to MessageHeader.szSubject
			#ifdef ODPLAT_NIX
			sprintf(commandline, "zip -9 -u %s frombbs.dat > /dev/null", MessageHeader.szSubject);
			#else
			sprintf(commandline, "PKZIP.EXE -ex -u %s frombbs.dat >NUL", MessageHeader.szSubject);
			#endif
			if (od_spawn(commandline) == FALSE)
			{
				#ifdef ODPLAT_NIX
				od_printf("`bright red`ERROR: zip execution error.\n\r");
				od_log_write("ERROR: zip execution error.");
				#else
				od_printf("`bright red`ERROR: PKZIP execution error.\n\r");
				od_log_write("ERROR: PKZIP execution error.");
				#endif
				od_exit(-1, FALSE);
			}
		}

	}

	// use the normal routing method or we are the hub of the wagon wheel
	if (wagon_wheel == FALSE || (int) thisbbs == InterBBSInfo.paOtherSystem[0].szNumber)
	{
		// send the info to all the systems that have sendto == TRUE and are not
		// the system it came from...
		for (system=0;system<InterBBSInfo.nTotalSystems;system++)
		{
			
			if ( (
					InterBBSInfo.paOtherSystem[system].szNumber != frombbs
					&& InterBBSInfo.paOtherSystem[system].szSendTo == TRUE
					) || (
					((int) thisbbs == InterBBSInfo.paOtherSystem[0].szNumber)
					&& (wagon_wheel == TRUE)
					&& (thisbbs != InterBBSInfo.paOtherSystem[system].szNumber)
					&& (frombbs != InterBBSInfo.paOtherSystem[system].szNumber)
					))
			{

				if (first == TRUE && inbound == TRUE)
				{
					od_printf("`bright cyan`     - Copying file for routing\n\r");
					if (access(MessageHeader.szSubject, 00) == 0)
						copyfile(MessageHeader.szSubject, filename);
					remove(MessageHeader.szSubject);
					first = FALSE;
				}

				// make sure a file exists to send...
				if (access(filename, 00) != 0)
				{
					od_printf("`bright cyan`     - No Data to Send\n\r");
					return;
				}

				od_printf("`cyan`     - Sending information to `bright cyan`%s\r\n", InterBBSInfo.paOtherSystem[system].szSystemName);
				// create a new filename for this system
				gettime(&t);
				// 12/96 Modified to have to BBS in filename
				// sprintf(sendname, "%sOUTBOUND%c%2.2s%02d%02d%02d.%s", doorpath, PATH_DELIM, ibbsid, t.ti_min, t.ti_sec, t.ti_hund, league);
				sprintf(sendname, "%sOUTBOUND%c%2.2s%02d%02d%02d.%s", doorpath, PATH_DELIM, ibbsid, InterBBSInfo.paOtherSystem[system].szNumber, t.ti_sec, t.ti_hund, league);
			#ifdef ODPLAT_DOS
			delay(100); // slow down 100th of a second
			#endif
				// copy the temp filename to this filename
				if (access(filename, 00) == 0)
					copyfile(filename, sendname);

				// 12/96 modified to have to BBS in filename
				if (binkley == TRUE) sprintf(sendname, "^%sOUTBOUND%c%2.2s%02d%02d%02d.%s", doorpath, PATH_DELIM, ibbsid, InterBBSInfo.paOtherSystem[system].szNumber, t.ti_sec, t.ti_hund, league);

/*
				if (copyfile(filename, sendmail)==-1)
				{
					od_printf("`bright red`ERROR: Copying file.\n\r");
					od_log_write("ERROR: Copying File.");
					od_exit(-1, FALSE);
				}
*/
				// set the subject to this file...delete on send, and toggel file mode
				strcpy(InterBBSInfo.szSubject,sendname);
				InterBBSInfo.bFileAttach = TRUE;
				InterBBSInfo.bDeleteOnSend = TRUE;
				// 10/96 back to blank message
				msg_text[0] = '\0';
				// sprintf(msg_text, "Message must be processed by %s\r\n", od_control.od_prog_name);

				// actually make the message to transfer the file
				returned = IBSend(&InterBBSInfo, InterBBSInfo.paOtherSystem[system].szAddress, msg_text, strlen(msg_text), FALSE, FALSE );
				// quit early if there was a problem
				if (returned != eSuccess)
				{
				 ErrorDetect(returned);
				}
			}
		}
	}
	// not the hub of the wagon wheel, only send normal outbound information to the hub
	else if (inbound == FALSE)
	{
		// make sure a file exists to send...
		if (access(filename, 00) != 0)
		{
			od_printf("`bright cyan`     - No Data to Send\n\r");
			return;
		}

		od_printf("`cyan`     - Sending information to `bright cyan`%s\r\n", InterBBSInfo.paOtherSystem[0].szSystemName);
		// create a new filename for this system
		gettime(&t);
		// 12/96 Modified to have to BBS in filename and use ibbsid
		sprintf(sendname, "%sOUTBOUND%c%2.2s%02d%02d%02d.%s", doorpath, PATH_DELIM, ibbsid, InterBBSInfo.paOtherSystem[system].szNumber, t.ti_sec, t.ti_hund, league);
	  #ifdef ODPLAT_DOS
		delay(100); // slow the system down a bit
	  #endif
		if (access(filename, 00) == 0)
			copyfile(filename, sendname);
/*
		if (copyfile(filename, sendname)==-1)
		{
			od_printf("`bright red`ERROR: Copying file.\n\r");
			od_log_write("ERROR: Copying File.");
			od_exit(-1, FALSE);
		}
*/
		// 12/96 Modified to have to BBS in filename and use ibbs id
		if (binkley == TRUE) sprintf(sendname, "^%sOUTBOUND%c%2.2s%02d%02d%02d.%s", doorpath, PATH_DELIM, ibbsid, InterBBSInfo.paOtherSystem[system].szNumber, t.ti_sec, t.ti_hund, league);

		// set the subject to this file...delete on send, and toggel file mode
		strcpy(InterBBSInfo.szSubject,sendname);
		InterBBSInfo.bFileAttach = TRUE;
		InterBBSInfo.bDeleteOnSend = TRUE;
		// 10/96 Back to blank message
		msg_text[0] = '\0';
		// sprintf(msg_text, "Message must be processed by %s\r\n", od_control.od_prog_name);
		// send the info only to the first BBS, where it will be passed along
		returned = IBSend(&InterBBSInfo, InterBBSInfo.paOtherSystem[0].szAddress, msg_text, strlen(msg_text), FALSE, FALSE );
	}
	else
	{
		// inbound is true, but we are not echoing data, so we must delete the
		// file...
		remove(MessageHeader.szSubject);
	}

	// detect problems
	ErrorDetect(returned);

	// erase our temporary filename
	remove(filename);

	return;
}



void MakeRouteReport( void )
{

//  char filename[128];
	FILE *warfile;
	INT16 i, iCurrentSystem;
	char blankbuf[ROUTEINFOSIZE+1];
	FILE *bbsAns, *bbsAsc;
	char bbsAnsfile[128], bbsAscfile[128];
//  struct s_bbs_list *currbbs;
	// 12/96
	INT16 current_bbs;
	
	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  MakeRouteReport()\n");
	fclose(gac_debug);
	#endif



	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  MakeRouteReport()\n");
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

		// setmem(blankbuf, ROUTEINFOSIZE, '0');
		memset(blankbuf, '0', ROUTEINFOSIZE);
		blankbuf[ROUTEINFOSIZE]=0;
		// write enough zeros for 256 BBSs
		for (i=0; i<=256; i++)
		{
			fwrite(blankbuf, ROUTEINFOSIZE, 1, warfile);
		}

		fclose(warfile);
	}

	od_printf("`Bright cyan`Creating Routing Bulletins.\n\r\n\r");
	// Make paths to the score files
	sprintf(bbsAscfile, "%sbbstimes.asc", doorpath);
	sprintf(bbsAnsfile, "%sbbstimes.ans", doorpath);

	od_printf("`Cyan`Removing old files...\n\r");
	// Remove the old top 15 bulletins
	if (access(bbsAnsfile, 00 ) == 0) remove(bbsAnsfile);
	if (access(bbsAscfile, 00 ) == 0) remove(bbsAscfile);

	// Develop the bbs list.
//  MakeTopBBS(FALSE);

	od_printf("`Cyan`Opening new bulletins ...\n\r");

	// Open the WAR info file to read in information
	warfile = gopen(filename, O_BINARY, O_DENYALL);
	if (warfile == NULL)
	{
		od_printf("`BRIGHT RED`ERROR OPENING %s\n\r", filename);
		sprintf(filename, "ERROR Opening: routeinf.dat file");
		od_log_write(filename);
		return;
	}
	bbsAns = fopen(bbsAnsfile, "w+b");
	bbsAsc = fopen(bbsAscfile, "w+b");

	od_printf("`Cyan`Writing new bulletins ..");

//  fputs( szBbslstANS, bbsAns);
        fprintf(bbsAns, "\r\n\r\n                \x1B[0;1;37m*** \x1B[0;1;36mRouting Times for %s \x1B[0;1;37m***\r\n\r\n", od_control.od_prog_name);

	// 12/96
        fprintf(bbsAsc, "\r\n\r\n                *** Routing Times for %s ***\r\n\r\n", od_control.od_prog_name);


//      fprintf(bbsAns, "\r\n  \x1B[0;1;37m%-5s %-25s %-15s %-10s %-15s\r\n", "Rank", "BBS Name",  "Total Players",  "Ave Kills", "Ave Experience");
	fprintf(bbsAns, "\r\n \x1B[0;1;37m%-3s %-25s %-23s     %s\r\n", " #", "BBS Name", "Route Time (Packet Date)", "Version");
//      fprintf(bbsAsc, "  %-5s %-25s %-15s %-10s %-15s\r\n", "Rank", "BBS Name",  "Total Players",  "Ave Kills", "Ave Experience");
	fprintf(bbsAsc, " %-3s %-25s %-23s     %s\r\n", " #", "BBS Name",  "Route Time (Packet Date)", "Version");

	fprintf(bbsAns, "\x1B[0;36m");
	for (i = 1; i<= 79; i++)
	{
		fprintf(bbsAns, "%c", (unsigned char)196);
		fprintf(bbsAsc, "-");
	}
	// skip a line
	fprintf(bbsAns, "\r\n");
	fprintf(bbsAsc, "\r\n");

	// set the start of the list of top BBSs
//  currbbs = s_bbslist;

	// loop for each BBS and create a line of information
//  while ((currbbs != NULL))
//  {
	for(iCurrentSystem = 0; iCurrentSystem < InterBBSInfo.nTotalSystems; ++iCurrentSystem)
	{
		// read in information for the current bbs...
//      routing.from_bbs = currbbs->bbs;
		current_bbs = (int) InterBBSInfo.paOtherSystem[iCurrentSystem].szNumber;

		// Set SEEK to start of file
		fseek(warfile, 0L, SEEK_SET);
		// seek to the correct record
		fseek(warfile, ROUTEINFOSIZE*current_bbs, SEEK_CUR);

		// 12/96
		fread(routing.datebuf, sizeof(routing.datebuf), 1,warfile);
		fread(routing.timebuf, sizeof(routing.timebuf), 1,warfile);
		fread(&(routing.first), sizeof(routing.first), 1,warfile);
		routing.first=LE_LONG(routing.first);
		fread(&(routing.second), sizeof(routing.second), 1,warfile);
		routing.second=LE_LONG(routing.second);
		fread(&(routing.routetime), sizeof(routing.routetime), 1,warfile);
		routing.routetime=LE_LONG(routing.routetime);
		fread(routing.version, sizeof(routing.version), 1,warfile);
		fread(&(routing.from_bbs), sizeof(routing.from_bbs), 1,warfile);
		routing.from_bbs=LE_SHORT(routing.from_bbs);
		fread(&(routing.registered), sizeof(routing.registered), 1,warfile);
		routing.registered=LE_SHORT(routing.registered);

		// find out if the data is blank...
		// routing.datebuf[9]='\0';
		// 12/96
		if (routing.from_bbs != current_bbs && current_bbs != (int) thisbbs)
		{
			// print info for the BBS
			fprintf(bbsAns, "\x1B[1;37m%3d  \x1B[0;36m%-25.25s \x1B[0;37m%s\r\n",
			 current_bbs,  getbbsname(current_bbs), "No data available");

			fprintf(bbsAsc, "%3d  %-25.25s %s\r\n",
			 current_bbs,  getbbsname(current_bbs), "No data available");
		}
		else if (current_bbs == (int) thisbbs)
		{
			// print info for the BBS
			fprintf(bbsAns, "\x1B[1;37m%3d  \x1B[0;36m%-25.25s \x1B[0;37m%-25.25s   \x1B[0;1;36m%5.5s \x1B[0;34m(\x1B[0;1;34m%s\x1B[0;34m)\r\n",
#ifdef KEEP_REG
			 current_bbs,  getbbsname(current_bbs), "Created Report", currentversion, (registered ==RK_REGISTERED)?"Reg'd" : "UNReg'd");
#else
			 current_bbs,  getbbsname(current_bbs), "Created Report", currentversion, "Free");
#endif

			fprintf(bbsAsc, "%3d  %-25.25s %-25.25s   %5.5s (%s)\r\n",
#ifdef KEEP_REG
			 current_bbs,  getbbsname(current_bbs), "Created Report", currentversion, (registered ==RK_REGISTERED)?"Reg'd" : "UNReg'd");
#else
			 current_bbs,  getbbsname(current_bbs), "Created Report", currentversion, "Free");
#endif
		}
		else
		{
			// print info for the BBS
			fprintf(bbsAns, "\x1B[1;37m%3d  \x1B[0;36m%-25.25s \x1B[0;1;35m%2.2ld:%2.2ld \x1B[0;1;37m(%8.8s %8.8s)   \x1B[0;1;36m%5.5s \x1B[0;34m(\x1B[0;1;34m%s\x1B[0;34m)\r\n",
			 routing.from_bbs,  getbbsname(routing.from_bbs),
			 routing.routetime/3600, (routing.routetime%3600)/60,
			 routing.datebuf, routing.timebuf,
#ifdef KEEP_REG
			 routing.version, (routing.registered == TRUE)?"Reg'd" : "UNReg'd");
#else
			 routing.version, "Free");
#endif

			fprintf(bbsAsc, "%3d  %-25.25s %2.2ld:%2.2ld (%8.8s %8.8s)   %5.5s (%s)\r\n",
			 routing.from_bbs,  getbbsname(routing.from_bbs),
			 routing.routetime/3600, (routing.routetime%3600)/60,
			 routing.datebuf, routing.timebuf,
#ifdef KEEP_REG
			 routing.version, (routing.registered == TRUE)?"Reg'd" : "UNReg'd");
#else
			 routing.version, "Free");
#endif
		}
//      od_printf("\n\r`Cyan`     - Origin `bright cyan`%s`cyan`      Routing time `bright cyan`%2.2ld:%2.2ld`cyan` (hours:min)\n\r", getbbsname(routing.from_bbs), routing.routetime/3600, (routing.routetime%3600)/60);
//      od_printf("`cyan`       Packet created on `bright cyan`%s`cyan` at `bright cyan`%s`cyan`\n\r",routing.datebuf,routing.timebuf);
//      od_printf("`cyan`       Remote Version `bright cyan`%s`cyan` (`bright cyan`%s`cyan`)\n\r", routing.remoteversion, (msg.gems)?"Registered" : "UNREGISTERED");

		od_printf("`Cyan`.");
		// Increase the current pointer by one
//      currbbs = currbbs->next;

	}
	od_printf("`Cyan`\n\r");

	od_printf("`Cyan`Closing all files...\n\r");
	od_printf("`Cyan`All finished creating score files\n\r");

	// close the files
	fclose(bbsAns);
	fclose(bbsAsc);
	fclose(warfile);
	return;
}


// this function should perform a very quick copy without any problems...
INT16 copyfile(char *fromfile, char *tofile)
{
	FILE *to, *from;
	int bytesread;
	char *buffer;

	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  CopyFile()\n");
	fclose(gac_debug);
	#endif

//  char buffer[5000];

	if ((buffer = (char *) malloc(BUF_SIZE)) == NULL)
	{
		od_printf("`bright red`ERROR: Unable to allocate memory for copy buffer\r\n");
		od_log_write("ERROR: Unable to allocate memory for copy buffer");
		od_exit(-1, FALSE);
	}

	to = fopen(tofile, "wb");
	if (to == NULL)
	{
		od_printf("`bright red`ERROR: Opening file: %s.\n\r", tofile);
		od_log_write("ERROR: Opening to file for copying");
		free(buffer);
		return(-1);
	}

	from = fopen(fromfile, "rb");
	if (from == NULL)
	{
		od_printf("`bright red`ERROR: Opening file: %s.\n\r", fromfile);
		od_log_write("ERROR: Opening from file for copying");
		fclose(to);
		free(buffer);
		return(-1);
	}

	fseek(to, 0L, SEEK_SET);
	fseek(from, 0L, SEEK_SET);

	while ((bytesread=fread( buffer, sizeof(char), BUF_SIZE, from)) != 0)
	{
		fwrite(buffer, sizeof(char), bytesread, to);
	}

	fclose(to);
	fclose(from);
	free(buffer);

	return(0);
}


// ===========================================================================
// This functions sends a message to every SysOp in the League
// ===========================================================================
void SendAll( void )
{
	#ifndef GSDK_OVERLAY
	static tIBResult returned;
	#endif
	// InitIBBS();
	tODEditOptions myEditOptions;
   char choice='\0';
	
	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  SendAll()\n");
	fclose(gac_debug);
	#endif

	g_clr_scr();

	InterBBSInfo.szSubject[0] = '\0';

	od_printf("`cyan`Do you wish to attach a file (`bright cyan`y`cyan`/`bright cyan`N`cyan`)?\n\r");
	choice = od_get_answer("YN\n\r");

	if (choice  == 'Y')
	{
		InterBBSInfo.bFileAttach = TRUE;
		InterBBSInfo.bDeleteOnSend = FALSE;

		// attach a file to the message
		od_printf("\n\r`cyan`Enter FULL path to File: `bright cyan`");
		// use command_line to store filename, and create full path
		od_input_str(InterBBSInfo.szSubject, 70, 32, 128);
		// check for no entry
		if (stricmp(InterBBSInfo.szSubject, "          ") < 0) return;
		// check for a valid file
		if (access(InterBBSInfo.szSubject, 00) != 0)
		{
			od_printf("\n\r%s does not exist!\n\r", InterBBSInfo.szSubject);
			gac_pause();
			return;
		}
	}
	else
	{
		InterBBSInfo.szSubject[0] = '\0';
		od_printf("\n\r`cyan`Enter Subject: `bright cyan`");
		od_input_str(InterBBSInfo.szSubject, 70, 32, 128);
		// check for no subject
	}

	if (stricmp(InterBBSInfo.szSubject, "              ") < 0)
	{
	 od_printf("\n\r`bright red`Aborted!\n\r\n\r");
	 return;
	}

	// init the message buffer to all null characters plus a \r\n
	memset(msg_text, '\0', sizeof(msg_text));

	memset(&myEditOptions, 0, sizeof(myEditOptions));
	od_printf("\n\r\n\r`bright cyan`Use [CTRL]-[Z] or [ESC] for pop-up menu (Save, or Abort)`cyan`\n\r");
	
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
	if (msg_text[0] == '\0')
	{
	 od_printf("\n\r\n\r`bright red`Aborted!\n\r\n\r");
	 gac_pause();
	 return;
	}

/*
   // allow user to enter message using internal editor
	od_printf("\n\r`bright cyan`Use [ESC] or [CTRL]-[Z] to Save, [CTRL]-[C] to Abort`cyan`\n\r");
	if (gac_lined(75, 45) == 0)
	{
	 g_clr_scr();
	 od_printf("\n\r\n\r`bright red`Aborted!\n\r\n\r");
	 gac_pause();
	 return;
	}
*/

	g_clr_scr();
	od_printf("`cyan`Sending the message to ALL league members\n\r");
	od_log_write("Sending Message to all League Members");

	strcpy(InterBBSInfo.szToUserName, "Sysop");
	strcpy(InterBBSInfo.szFromUserName, "Sysop");
	// Send the message
	returned = IBSendAll(&InterBBSInfo, &msg_text, (int) strlen(msg_text),
		thisbbs, FALSE, FALSE);
	ErrorDetect( returned);
	gac_pause();

	return;

}



// ===========================================================================
// Checks for Inter-BBS related errors
// ===========================================================================
void ErrorDetect( tIBResult returned)
{

	// Show the final line of information
	if (returned == eSuccess) od_printf("`bright cyan`Finished\r\n");
	else if (returned == eNoMoreMessages) od_printf("`bright cyan`No more messages to process\n\r");
	else if (returned == eGeneralFailure) od_printf("`Bright red`IBBS ERROR: General Failure\n\r");
	else if (returned == eBadParameter) od_printf("`Bright red`IBBS ERROR: Bad Parameter\n\r");
	else if (returned == eNoMemory) od_printf("`Bright red`IBBS ERROR: Not enough memory\n\r");
	else if (returned == eMissingDir) od_printf("`Bright red`IBBS ERROR: Missing NetMail Directory\n\r");
	else if (returned == eFileOpenError) od_printf("`Bright red`IBBS ERROR: File Open Error\n\r");

	// Log errors to the log file
	if (returned == eGeneralFailure) od_log_write("IBBS ERROR: General Failure");
	else if (returned == eBadParameter) od_log_write("IBBS ERROR: Bad Parameter");
	else if (returned == eNoMemory) od_log_write("IBBS ERROR: Not enough memory");
	else if (returned == eMissingDir) od_log_write("IBBS ERROR: Missing NetMail Directory");
	else if (returned == eFileOpenError) od_log_write("IBBS ERROR: File Open Error\n\r");


	return;
}

// ===========================================================================
// This function will initialize the InterBBS information
// ===========================================================================
void FAR16 InitIBBS( void )
{
	FILE *ibbsfile;
	char found=FALSE;
	char templine[81];
	int i;
//  char ibbsconfig[128];


	#ifdef GAC_DEBUG
	gac_debug = fopen(gac_debugfile, "a");
	fprintf(gac_debug, "  InitIBBS()\n");
	fclose(gac_debug);
	#endif

	// Init the variables
	strcpy(InterBBSInfo.szThisNodeAddress, ThisBBSAddress);
	sprintf(InterBBSInfo.szProgName, "%s League %s", ibbsgametitle, league);
	strcpy(InterBBSInfo.szNetmailDir, netmaildir);
	InterBBSInfo.nTotalSystems = 0;

	// search the INTERBBS.CFG and find the line that has our node number
	// on it and find our linknumber...
	thisbbs = 0;
	found = FALSE;    
	sprintf(filename, "%sinterbbs.cfg", doorpath);
	ibbsfile = fopen(filename, "rb");
	if (ibbsfile == NULL)
	{
		// couldn't open the file, but let them play anyway
		od_printf("`bright red`Unable to open %s, forcing local game.\n\r", filename);
		thisbbs = 0;
	}
	else
	{
		while (fscanf(  ibbsfile, "%[^\n]\n", templine) != EOF && thisbbs == 0)
		{
         // 4/97 Added strupr to perform comparison correctly
         strupr(templine);
			i=strlen(templine);
			while(templine[i-1]=='\r' || templine[i-1]=='\n')
				templine[i-1]=0;
			if (templine[0] == ';') continue;  // don't look at comments                
			else if (found == TRUE)
			{
				// get this bbs's number from the linknumber line
				if (strstr(templine, "LINKNUMBER") != NULL) 
				{
					sscanf(templine, "%*s %hd", &thisbbs);
					break;
				}
			}
			else
			{
				// see if we have found the LinkWith ThisBBSAddress line
				if (strstr(templine, "LINKWITH") != NULL && strstr(templine, ThisBBSAddress) != NULL) 
               found = TRUE;
			}
		
		}
	
		fclose( ibbsfile );
	}
	// let the system read in all of the information
	sprintf(filename, "%sinterbbs.cfg", doorpath);
	IBReadConfig(&InterBBSInfo, filename);
	return;
}
