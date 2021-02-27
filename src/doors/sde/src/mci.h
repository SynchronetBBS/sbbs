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

#ifndef _MCI_H_
#define _MCI_H_

void text(char *lnum, ...);
void text1(char *lnum, char *ttxt);
void runmci(char *ttxt,int showit);
char *strrepl(char *Str, size_t BufSiz, char *OldStr, char *NewStr);
void mci(char *strng, ...);
void mcistr(char *ostr,char *strng, ...);
void center(char *strng, ...);
int mcistrlen(char *ttxt);

#endif
