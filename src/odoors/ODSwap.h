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
 *        File: ODSwap.h
 *
 * Description: Provides memory swapping, spawning and certain other low-
 *              level assembly routines needed by OpenDoors. This file is only
 *              applicable when building the MS-DOS version of OpenDoors.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Nov 25, 1995  6.00  BP   Created.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 */

#ifndef _INC_ODSWAP
#define _INC_ODSWAP

#ifdef ODPLAT_DOS

/* Data types. */
typedef struct _vector
{
   char number;                        /* vector number */
   char flag;                          /* 0-CURRENT, 1-IRET, 2-free, 3-end */
   unsigned int vseg;                  /* vector segment */
   unsigned int voff;                  /* vector offset */
} VECTOR;

/* Global variables. */
extern char **environ;

/* Public functions. */
int _chkems(char *, int *);
int _create(char *, int *);
int _dskspace(int, unsigned int *, unsigned int *);
int _getcd(int, char *);
int _getdrv(void);
int _getems(int, int *);
int _getrc(void);
void _getvect(int, unsigned int *, unsigned int *);
int _restmap(char *);
int _savemap(char *);
void _setdrvcd(int, char * );
void _setvect(VECTOR *);
int _xsize(unsigned int, long *, long *);
int _xspawn(char *, char *, char *, VECTOR *, int, int, char *, int);

#endif /* ODPLAT_DOS */


#endif /* _INC_ODSWAP */
