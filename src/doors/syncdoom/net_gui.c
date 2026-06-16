//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2026 Rob Swindell / syncdoom
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Headless replacement for Chocolate Doom's net_gui (the SDL/textscreen
//     "waiting for players" screen).  syncdoom drives the pre-game lobby and
//     waiting room itself; here we only pump the client/server until the
//     server launches the game.  As the controlling player we auto-launch
//     once every expected slot is filled (the lobby may also call
//     NET_CL_LaunchGame() to start earlier).
//

#include <stdio.h>

#include "doomtype.h"
#include "i_system.h"
#include "i_timer.h"
#include "net_client.h"
#include "net_gui.h"
#include "net_server.h"

void NET_WaitForLaunch(void)
{
    boolean launched = false;
    int last_reported = -1;

    while (net_waiting_for_launch)
    {
        NET_CL_Run();
        NET_SV_Run();

        if (!net_client_connected)
        {
            I_Error("NET_WaitForLaunch: Lost connection to server");
        }

        if (net_client_received_wait_data)
        {
            // Report player count changes to stderr (logged by the BBS).

            if (net_client_wait_data.num_players != last_reported)
            {
                fprintf(stderr, "syncdoom: waiting for players (%d/%d)\n",
                        net_client_wait_data.num_players,
                        net_client_wait_data.max_players);
                last_reported = net_client_wait_data.num_players;
            }

            // The controlling player launches the game once every expected
            // slot is filled.

            if (!launched
             && net_client_wait_data.is_controller
             && net_client_wait_data.num_players
                    >= net_client_wait_data.max_players)
            {
                NET_CL_LaunchGame();
                launched = true;
            }
        }

        I_Sleep(50);
    }
}
