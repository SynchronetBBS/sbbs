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

#include "net_gui.h"

// The waiting room is implemented in syncdoom.c (it owns the client socket I/O
// to draw the player list and read the start/cancel keys).
extern void sd_waitroom_run(void);

void NET_WaitForLaunch(void)
{
    sd_waitroom_run();
}
