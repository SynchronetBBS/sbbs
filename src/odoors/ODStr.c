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
 *        File: ODStr.c
 *
 * Description: Functions used to mainuplate strings
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#include "OpenDoor.h"

#ifdef ODPLAT_NIX
#include <string.h>

int
strlwr(char *str)
{
  size_t i;
  for(i=0;i<strlen(str);i++)
  {
    if(str[i]>64 && str[i]<=91) str[i]=str[i]|32;
  }
  return 0;
}

int
strupr(char *str)
{
  size_t i;
  for(i=0;i<strlen(str);i++)
  {
    if(str[i]>96 && str[i]<123) str[i] &= 223;
  }
  return 0;
}
#endif
