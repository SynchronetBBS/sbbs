/*

The Clans BBS Door Game
Copyright (C) 1997-2002 Allen Ussher

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
 * Maintenance ADT -- all maintenance functions should go here.
 *
 */
#ifdef __unix__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "misc.h"
#include "maint.h"
#include "video.h"
#include "system.h"
#include "user.h"
#include "ibbs.h"
#include "alliance.h"
#include "trades.h"
#include "village.h"
#include "pawn.h"
#include "npc.h"
#include "mail.h"
#include "game.h"

extern struct game Game;
extern struct system System;
extern struct ibbs IBBS;

void Maintenance(void)
/*
 * Daily maintenance is run.
 *
 *  If this is an IBBS game AND maint was already run, it will not run
 *  again.  Otherwise, Maintenance() will ALWAYS run.
 *
 * PreCondition   System is initialized (excluding Door_Init).
 *
 */
{
	DisplayStr("|09Maintenance running\n");

	// if IBBS game AND maintenance already run, don't run it again
	// otherwise, run it again
	// REP: remove FALSE so cheating not allowed
	if ((DaysBetween(Game.Data->szTodaysDate, System.szTodaysDate) <= 0) &&
#ifdef PRELAB
			Game.Data->InterBBS && FALSE)
#else
			Game.Data->InterBBS)
#endif
	{
		System_Error("Maintenance already run today.\n");
	}

	// update today's date
	strcpy(Game.Data->szTodaysDate, System.szTodaysDate);


	// if the game has not yet begun, skip reset of maintenance
	if (Game.Data->GameState == 1 || Game.Data->GameState == 2) {
		DisplayStr("* Game currently not in progress, maintenance skipped.\n");
		return;
	}


	System_Maint();

	Village_Maint();

	IBBS_Maint();

	User_Maint();

	Mail_Maint();

	NPC_Maint();

	Trades_Maint();

	PS_Maint();

	// remove disbanding user names
	unlink("disband.dat");

	Alliance_Maint();

	Game_Write();

	DisplayStr("Done.\n");

}
