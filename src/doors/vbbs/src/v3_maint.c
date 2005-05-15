/*****************************************************************************
 *
 * File ..................: v3_maint.c
 * Purpose ...............: Daily maint
 *
 *****************************************************************************/

#include "vbbs.h"

#include "v3_defs.h"
#include "vbbs_db.h"
#include "vbbsutil.h"
#include "v3_io.h"

#include "v3_maint.h"

static void compact_players()
{
	char cur_path[_MAX_PATH];
	char new_path[_MAX_PATH];
	tUserRecord plyr;
	int  p;

	// make all the paths
	sprintf(cur_path,"%susers.vb3",gamedir);
	sprintf(new_path,"%susers.new",gamedir);
	// get the player count
	int total_players = countplyrs();
	// open the current file and new file
	FILE* fcur = fopen(cur_path, "rb");
	FILE* fnew = fopen(new_path, "wb");
	// abort if we couldn't open the files
	if (!fcur || !fnew)
		return;
	// new player count
	s16 players = 0;
	// read each player, and write them if they survive
	for (p=0; p<total_players; p++)
	{
		// grab the player
		fread(&plyr, sizeof(tUserRecord), 1, fcur);
		// check for deletion
		if (plyr.status != DELETED)
		{
			// add him to the count
			plyr.plyrnum = players++;
			// write them to the new file
			fwrite(&plyr, sizeof(tUserRecord), 1, fnew);
		}
	}
	// close both files
	fclose(fcur);
	fclose(fnew);
	// rid ourselves of the old one
	remove(cur_path);
	rename(new_path, cur_path);
}

static void validate_players()
{
	tUserRecord plyr;
	int	p;

	// get the player count
	int total_players = countplyrs();
	// read each player, and write them if they survive
	for (p=0; p<total_players; p++)
	{
		// grab the player
		readplyr(&plyr, p);

		// check number of modems
		if (plyr.mspeed.low < 0)		plyr.mspeed.low = 0;
		if (plyr.mspeed.high < 0)		plyr.mspeed.high = 0;
		if (plyr.mspeed.digital < 0)	plyr.mspeed.digital = 0;
		// check funds
		if (plyr.funds < 0.0f) 			plyr.funds = 0.0f;

		// write them to the new file
		saveplyr(&plyr, p);
	}
}

void v3_maint_daily()
{
	// get today
	u32 today = vbbs_util_getday();
	// get last maint
	record sys_rec;
	gSystemDB_read(&sys_rec);
	// check for maint
	if (sys_rec.last_maint != today)
	{
		// compact all the players
		compact_players();
		// check for errors
		validate_players();
		// update sys record
		gSystemDB_update(&sys_rec);
		sys_rec.last_maint = today;
		gSystemDB_commit(&sys_rec);
	}
}
