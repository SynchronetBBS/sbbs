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
 *        File: ODStat.h
 *
 * Description: Public functions provided by the odstat.c module, for status
 *              line functions shared among personalities.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Nov 13, 1995  6.00  BP   Created.
 *              Jan 12, 1996  6.00  BP   Added ODStatStartArrowUse(), etc.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 */

#ifndef _INC_ODSTAT
#define _INC_ODSTAT


/* Global working string available to all personalities for status line */
/* generation.                                                          */
extern char szStatusText[80];


/* Public status line function prototypes. */
void ODStatAddKey(WORD wKeyCode);
void ODStatRemoveKey(WORD wKeyCode);
void ODStatGetUserAge(char *pszAge);
void ODStatStartArrowUse(void);
void ODStatEndArrowUse(void);


#endif /* _INC_ODSTAT */
