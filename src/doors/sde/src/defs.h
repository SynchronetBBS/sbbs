/* Copyright 2004 by Darryl Perry
 * 
 * This file is part of Space Dynasty Elite.
 * 
 * Space Dynasty Elite is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Foobar; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DEFS_H_
#define _DEFS_H_

char insurtxt[9][25]={"peace","mild insurgency","violent demonstrations",
					"riots","political crime","terrorism","guerrilla warfare",
					"revolutionary warfare","under coup"};



char diptxt[4][21]={"Neutral",
					"Minor Alliance",
					"Major Alliance",
					"Enemy"};

char cvstr3[5][25]={"Your dynasty.",
					"One of your generals.",
					"One of your carriers.",
					"Insurge your people.",
					"At your embasy."};

char cvstr2[5][16]={"Spy on ",
					"Assassinate ",
					"Sabotage ",
					"Disrupt ",
					"Plant a bomb "};
char cvstr1[5][16]={"Spy mission...",
					"Assassination...",
					"Sabotage...",
					"Insurgency...",
					"Plant a bomb..."};

long cvamt[5]={7500,10000,12500,100000,1000000};
long mktfood=7680;
char ttxt2[512];
#endif

