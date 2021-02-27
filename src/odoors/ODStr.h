/* OpenDoors Online Software Programming Toolkit
 * (C) Copyright 1991 - 1999 by Brian Pirie.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *        File: ODStr.h
 *
 * Description: Functions used to maipulate strings
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Aug 10, 2003  6.23  SH   Initial rev.
 */

#include "OpenDoor.h"

#ifndef _INC_ODSTR
#define _INC_ODSTR

#ifdef ODPLAT_NIX
#define strnicmp      strncasecmp
#define stricmp       strcasecmp
#define strlwr(x)	od_strlwr(x)
#define strupr(x)	od_strupr(x)
char *od_strlwr(char *str);
char *od_strupr(char *str);
#endif

#endif
