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
//     Networking module which uses plain UDP sockets (via xpdev sockwrap),
//     replacing Chocolate Doom's SDL_net transport (net_sdl) so syncdoom
//     stays SDL-free and portable across the platforms Synchronet builds on.
//

#ifndef NET_UDP_H_
#define NET_UDP_H_

#include "net_defs.h"

extern net_module_t net_udp_module;

#endif /* #ifndef NET_UDP_H_ */
