/*
 * Mystic Draw 1.5 : A nice ANSI Drawing tool Copyright(C) 1996 by Mike
 * Krueger (mkrueger@inf.fu-berlin.de) Updated 2002 by Stephen Hurd
 * (deuce@lordlegacy.org)
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include <stdio.h>
#include <gen_defs.h>
#include <genwrap.h>
#include <ciolib.h>
#include <ctype.h>
#include <string.h>

#include <stdlib.h>
#include <sys/types.h>

#include <dirwrap.h>
#include "xpendian.h"

#include "homedir.h"
#include "block.h"
#include "config.h"
#include "crt.h"
#include "draw.h"
#include "effekt.h"
#include "fonts.h"
#include "help.h"
#include "key.h"
#include "load.h"
#include "syncdraw.h"
#include "menue.h"
#include "miscfunctions.h"
#include "options.h"
#include "sauce.h"
#include "save.h"
#include "tabulator.h"

unsigned char   Screen[4][MaxLines + 1][160];
unsigned char   tabs[80];
unsigned char   CursorPos[80], cpos = 1;
int             CursorX = 0, CursorY = 0, FirstLine = 0, x, y, LastLine = 0;
unsigned char   Attribute = 7, FontMode = FALSE, Undo = FALSE, FontTyped = FALSE,
                InsertMode = FALSE, EliteMode = FALSE, SaveSauce = FALSE,
                ActiveCharset = 1, ActivePage = 1;
unsigned char   FullScreen = FALSE;
unsigned short  DrawMode = 0;
char           *stri[20];
int				Dragging=0;

unsigned char   SelectAnsi[4096] = {
	218, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 191, 8, 179,
	8, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0,
	27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0,
	27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 83, 27, 69, 27, 76, 27, 69, 27, 67,
	27, 84, 27, 32, 27, 79, 27, 85, 27, 84, 27, 76, 27, 73, 27, 78, 27, 69, 27, 32,
	27, 70, 27, 79, 27, 78, 27, 84, 27, 32, 27, 83, 27, 84, 27, 89, 27, 76, 27, 69,
	27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0,
	27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0,
	27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 0, 27, 179, 8, 195,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 180, 8, 179,
	8, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 65, 11, 218, 2, 196, 2, 196, 2, 196, 2, 196, 2, 196,
	2, 191, 2, 32, 11, 66, 11, 213, 2, 205, 2, 205, 2, 205, 2, 205, 2, 205, 2, 184,
	2, 32, 2, 67, 11, 218, 2, 196, 2, 196, 2, 196, 2, 196, 2, 196, 2, 191, 2, 32,
	2, 68, 11, 213, 2, 205, 2, 205, 2, 205, 2, 205, 2, 205, 2, 184, 2, 32, 2, 69,
	11, 214, 2, 196, 2, 196, 2, 196, 2, 196, 2, 196, 2, 191, 2, 32, 2, 70, 11, 201,
	2, 205, 2, 205, 2, 205, 2, 205, 2, 205, 2, 184, 2, 32, 2, 71, 11, 214, 2, 196,
	2, 196, 2, 196, 2, 196, 2, 196, 2, 191, 2, 32, 2, 72, 11, 201, 2, 205, 2, 205,
	2, 205, 2, 205, 2, 205, 2, 184, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 32, 11, 179, 2, 32, 2, 218, 2, 196, 2, 191, 2, 32,
	2, 179, 2, 32, 11, 32, 11, 179, 2, 32, 2, 218, 2, 196, 2, 191, 2, 32, 2, 179,
	2, 32, 2, 32, 11, 179, 2, 32, 2, 213, 2, 205, 2, 184, 2, 32, 2, 179, 2, 32,
	2, 32, 11, 179, 2, 32, 2, 213, 2, 205, 2, 184, 2, 32, 2, 179, 2, 32, 2, 32,
	11, 186, 2, 32, 2, 218, 2, 196, 2, 183, 2, 32, 2, 179, 2, 32, 2, 32, 11, 186,
	2, 32, 2, 218, 2, 196, 2, 183, 2, 32, 2, 179, 2, 32, 2, 32, 11, 186, 2, 32,
	2, 213, 2, 205, 2, 187, 2, 32, 2, 179, 2, 32, 2, 32, 11, 186, 2, 32, 2, 213,
	2, 205, 2, 187, 2, 32, 2, 179, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 32, 11, 179, 2, 32, 2, 192, 2, 196, 2, 217, 2, 32,
	2, 179, 2, 32, 11, 32, 11, 179, 2, 32, 2, 212, 2, 205, 2, 190, 2, 32, 2, 179,
	2, 32, 2, 32, 11, 179, 2, 32, 2, 192, 2, 196, 2, 217, 2, 32, 2, 179, 2, 32,
	2, 32, 11, 179, 2, 32, 2, 212, 2, 205, 2, 190, 2, 32, 2, 179, 2, 32, 2, 32,
	11, 186, 2, 32, 2, 192, 2, 196, 2, 189, 2, 32, 2, 179, 2, 32, 2, 32, 11, 186,
	2, 32, 2, 212, 2, 205, 2, 188, 2, 32, 2, 179, 2, 32, 2, 32, 11, 186, 2, 32,
	2, 192, 2, 196, 2, 189, 2, 32, 2, 179, 2, 32, 2, 32, 11, 186, 2, 32, 2, 212,
	2, 205, 2, 188, 2, 32, 2, 179, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 32, 11, 179, 2, 32, 2, 218, 2, 196, 2, 191, 2, 32,
	2, 179, 2, 32, 11, 32, 11, 179, 2, 32, 2, 218, 2, 196, 2, 191, 2, 32, 2, 179,
	2, 32, 2, 32, 11, 179, 2, 32, 2, 213, 2, 205, 2, 184, 2, 32, 2, 179, 2, 32,
	2, 32, 11, 179, 2, 32, 2, 213, 2, 205, 2, 184, 2, 32, 2, 179, 2, 32, 2, 32,
	11, 186, 2, 32, 2, 218, 2, 196, 2, 183, 2, 32, 2, 179, 2, 32, 2, 32, 11, 186,
	2, 32, 2, 218, 2, 196, 2, 183, 2, 32, 2, 179, 2, 32, 2, 32, 11, 186, 2, 32,
	2, 213, 2, 205, 2, 187, 2, 32, 2, 179, 2, 32, 2, 32, 11, 186, 2, 32, 2, 213,
	2, 205, 2, 187, 2, 32, 2, 179, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 32, 11, 192, 2, 196, 2, 217, 2, 32, 2, 192, 2, 196,
	2, 217, 2, 32, 11, 32, 11, 192, 2, 196, 2, 217, 2, 32, 2, 192, 2, 196, 2, 217,
	2, 32, 2, 32, 11, 212, 2, 205, 2, 190, 2, 32, 2, 212, 2, 205, 2, 190, 2, 32,
	2, 32, 11, 212, 2, 205, 2, 190, 2, 32, 2, 212, 2, 205, 2, 190, 2, 32, 2, 32,
	11, 211, 2, 196, 2, 217, 2, 32, 2, 211, 2, 196, 2, 217, 2, 32, 2, 32, 11, 211,
	2, 196, 2, 217, 2, 32, 2, 211, 2, 196, 2, 217, 2, 32, 2, 32, 11, 200, 2, 205,
	2, 190, 2, 32, 2, 200, 2, 205, 2, 190, 2, 32, 2, 32, 11, 200, 2, 205, 2, 190,
	2, 32, 2, 200, 2, 205, 2, 190, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 73, 11, 218, 2, 196, 2, 196, 2, 196, 2, 196, 2, 196,
	2, 183, 2, 32, 11, 74, 11, 213, 2, 205, 2, 205, 2, 205, 2, 205, 2, 205, 2, 187,
	2, 32, 2, 75, 11, 218, 2, 196, 2, 196, 2, 196, 2, 196, 2, 196, 2, 183, 2, 32,
	2, 76, 11, 213, 2, 205, 2, 205, 2, 205, 2, 205, 2, 205, 2, 187, 2, 32, 2, 77,
	11, 214, 2, 196, 2, 196, 2, 196, 2, 196, 2, 196, 2, 183, 2, 32, 2, 78, 11, 201,
	2, 205, 2, 205, 2, 205, 2, 205, 2, 205, 2, 187, 2, 32, 2, 79, 11, 214, 2, 196,
	2, 196, 2, 196, 2, 196, 2, 196, 2, 183, 2, 32, 2, 80, 11, 201, 2, 205, 2, 205,
	2, 205, 2, 205, 2, 205, 2, 187, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 32, 11, 179, 2, 32, 2, 214, 2, 196, 2, 191, 2, 32,
	2, 186, 2, 32, 11, 32, 11, 179, 2, 32, 2, 214, 2, 196, 2, 191, 2, 32, 2, 186,
	2, 32, 2, 32, 11, 179, 2, 32, 2, 201, 2, 205, 2, 184, 2, 32, 2, 186, 2, 32,
	2, 32, 11, 179, 2, 32, 2, 201, 2, 205, 2, 184, 2, 32, 2, 186, 2, 32, 2, 32,
	11, 186, 2, 32, 2, 214, 2, 196, 2, 183, 2, 32, 2, 186, 2, 32, 2, 32, 11, 186,
	2, 32, 2, 214, 2, 196, 2, 183, 2, 32, 2, 186, 2, 32, 2, 32, 11, 186, 2, 32,
	2, 201, 2, 205, 2, 187, 2, 32, 2, 186, 2, 32, 2, 32, 2, 186, 2, 32, 2, 201,
	2, 205, 2, 187, 2, 32, 2, 186, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 32, 11, 179, 2, 32, 2, 211, 2, 196, 2, 217, 2, 32,
	2, 186, 2, 32, 11, 32, 11, 179, 2, 32, 2, 200, 2, 205, 2, 190, 2, 32, 2, 186,
	2, 32, 2, 32, 11, 179, 2, 32, 2, 211, 2, 196, 2, 217, 2, 32, 2, 186, 2, 32,
	2, 32, 11, 179, 2, 32, 2, 200, 2, 205, 2, 190, 2, 32, 2, 186, 2, 32, 2, 32,
	11, 186, 2, 32, 2, 211, 2, 196, 2, 189, 2, 32, 2, 186, 2, 32, 2, 32, 11, 186,
	2, 32, 2, 200, 2, 205, 2, 188, 2, 32, 2, 186, 2, 32, 2, 32, 11, 186, 2, 32,
	2, 211, 2, 196, 2, 189, 2, 32, 2, 186, 2, 32, 2, 32, 2, 186, 2, 32, 2, 200,
	2, 205, 2, 188, 2, 32, 2, 186, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 32, 11, 179, 2, 32, 2, 214, 2, 196, 2, 191, 2, 32,
	2, 186, 2, 32, 11, 32, 11, 179, 2, 32, 2, 214, 2, 196, 2, 191, 2, 32, 2, 186,
	2, 32, 2, 32, 11, 179, 2, 32, 2, 201, 2, 205, 2, 184, 2, 32, 2, 186, 2, 32,
	2, 32, 11, 179, 2, 32, 2, 201, 2, 205, 2, 184, 2, 32, 2, 186, 2, 32, 2, 32,
	11, 186, 2, 32, 2, 214, 2, 196, 2, 183, 2, 32, 2, 186, 2, 32, 2, 32, 2, 186,
	2, 32, 2, 214, 2, 196, 2, 183, 2, 32, 2, 186, 2, 32, 2, 32, 2, 186, 2, 32,
	2, 201, 2, 205, 2, 187, 2, 32, 2, 186, 2, 32, 2, 32, 2, 186, 2, 32, 2, 201,
	2, 205, 2, 187, 2, 32, 2, 186, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 32, 15, 32, 15, 32, 2, 32, 11, 192, 2, 196, 2, 189, 2, 32, 2, 192, 2, 196,
	2, 189, 2, 32, 11, 32, 11, 192, 2, 196, 2, 189, 2, 32, 2, 192, 2, 196, 2, 189,
	2, 32, 2, 32, 11, 212, 2, 205, 2, 188, 2, 32, 2, 212, 2, 205, 2, 188, 2, 32,
	2, 32, 2, 212, 2, 205, 2, 188, 2, 32, 2, 212, 2, 205, 2, 188, 2, 32, 2, 32,
	11, 211, 2, 196, 2, 189, 2, 32, 2, 211, 2, 196, 2, 189, 2, 32, 2, 32, 2, 211,
	2, 196, 2, 189, 2, 32, 2, 211, 2, 196, 2, 189, 2, 32, 2, 32, 2, 200, 2, 205,
	2, 188, 2, 32, 2, 200, 2, 205, 2, 188, 2, 32, 2, 32, 2, 200, 2, 205, 2, 188,
	2, 32, 2, 200, 2, 205, 2, 188, 2, 32, 2, 32, 2, 32, 2, 32, 9, 179, 8, 179,
	8, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 179, 8, 179,
	8, 32, 7, 32, 7, 32, 2, 81, 11, 220, 2, 220, 2, 220, 2, 220, 2, 220, 2, 220,
	2, 220, 2, 32, 11, 82, 11, 219, 2, 223, 2, 223, 2, 223, 2, 223, 2, 223, 2, 219,
	2, 32, 2, 83, 11, 222, 2, 223, 2, 223, 2, 223, 2, 223, 2, 223, 2, 221, 2, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 179, 8, 179,
	8, 32, 7, 32, 7, 32, 2, 32, 11, 219, 2, 32, 2, 220, 2, 220, 2, 220, 2, 32,
	2, 219, 2, 32, 11, 32, 11, 219, 2, 32, 2, 219, 2, 223, 2, 219, 2, 32, 2, 219,
	2, 32, 2, 32, 11, 222, 2, 32, 2, 220, 2, 220, 2, 220, 2, 32, 2, 221, 2, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 179, 8, 179,
	8, 32, 7, 32, 7, 32, 2, 32, 11, 219, 2, 32, 2, 219, 2, 220, 2, 219, 2, 32,
	2, 219, 2, 32, 11, 32, 11, 219, 2, 32, 2, 223, 2, 223, 2, 223, 2, 32, 2, 219,
	2, 32, 2, 32, 2, 222, 2, 32, 2, 223, 2, 223, 2, 223, 2, 32, 2, 221, 2, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 179, 8, 179,
	8, 32, 7, 32, 7, 32, 2, 32, 2, 219, 2, 32, 2, 220, 2, 220, 2, 220, 2, 32,
	2, 219, 2, 32, 11, 32, 11, 219, 2, 32, 2, 219, 2, 223, 2, 219, 2, 32, 2, 219,
	2, 32, 2, 32, 2, 222, 2, 32, 2, 220, 2, 220, 2, 220, 2, 32, 2, 221, 2, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 179, 8, 179,
	8, 32, 7, 32, 7, 32, 2, 32, 2, 219, 2, 220, 2, 219, 2, 32, 2, 219, 2, 220,
	2, 219, 2, 32, 11, 32, 11, 223, 2, 223, 2, 223, 2, 32, 2, 223, 2, 223, 2, 223,
	2, 32, 2, 32, 2, 222, 2, 220, 2, 221, 2, 32, 2, 222, 2, 220, 2, 221, 2, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 179, 8, 179,
	8, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 179, 8, 195,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 180, 8, 179,
	8, 32, 15, 32, 15, 85, 9, 115, 9, 101, 9, 32, 9, 67, 9, 117, 9, 114, 9, 115,
	9, 111, 9, 114, 9, 32, 9, 75, 9, 101, 9, 121, 9, 115, 9, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 91, 9, 69, 7, 78,
	7, 84, 7, 69, 7, 82, 7, 93, 9, 32, 9, 83, 9, 101, 9, 108, 9, 101, 9, 99,
	9, 116, 9, 115, 9, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 91, 9, 69, 7, 83, 7, 67, 7, 93, 9, 32, 9, 65,
	9, 98, 9, 111, 9, 114, 9, 116, 9, 115, 9, 32, 14, 32, 2, 32, 2, 179, 8, 192,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196,
	8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 196, 8, 217, 8, 177,
	42, 223, 2, 32, 2, 220, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 220,
	8, 32, 8, 32, 8, 32, 40, 220, 42, 32, 42, 32, 42, 32, 42, 219, 2, 223, 2, 32,
	2, 220, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 254, 8, 220, 8, 223, 8, 223,
	8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223,
8, 223, 8, 223, 8, 254, 8, 223, 8, 223, 8, 223, 8, 223, 8};

char           *NCharSet[12] = {
	"",
	"Ú¿ÀÙÄ³Ã´ÁÂ",
	"É»È¼ÍºÌ¹ÊË",
	"Õ¸Ô¾Í³ÆµÏÑ",
	"ÅÎØ×Ÿé›œ™ï",
	"°±²ÛßÜÝÞþú",
	"÷ù­¨‘’ðž63‡",
	"“”•¢§–—£š",
	"®¯òó©ªýö«¬",
	"ãñôõêäøûü",
	"àáâåæçëìíî",
	"ˆ‰Š‚Œ‹¡¦"
};

char           *CharSet[12];

char            EliteTable[256];

void
ShowScreen(int a1, int a2)
{
	int   c;
	struct			text_info	ti;

	gettextinfo(&ti);
	c=FullScreen?1:2;
	puttext(1,c+a1,80,ti.screenheight-a2,Screen[ActivePage][FirstLine+a1]);
}

void
Statusline(void)
{
	char		s[162];
	int	cx,cy;
	int	i,a;

	if (FullScreen)
		return;
	cx=CursorX+1;
	cy=CursorY+FirstLine+1;
	s[0]='(';
	s[1]=2;
	s[2]='0'+cx/10;
	s[3]=2;
	s[4]='0'+cx%10;
	s[5]=2;
	s[6]=',';
	s[7]=2;
	s[8]='0'+cy/1000;
	s[9]=2;
	s[10]='0'+(cy/100)%10;
	s[11]=2;
	s[12]='0'+(cy/10)%10;
	s[13]=2;
	s[14]='0'+cy%10;
	s[15]=2;
	s[16]=')';
	s[17]=2;
	s[18]=' ';
	s[19]=2;
	s[20]=' ';
	s[21]=2;
	s[22]='C';
	s[23]=Attribute;
	s[24]='o';
	s[25]=Attribute;
	s[26]='l';
	s[27]=Attribute;
	s[28]='o';
	s[29]=Attribute;
	s[30]='r';
	s[31]=Attribute;
	s[32]=' ';
	s[33]=2;
	s[34]=' ';
	s[35]=2;
	if(EliteMode & 1)
		s[36]='e';
	else
		s[36]=' ';
	s[37]=8;
	s[38]=' ';
	s[39]=8;
	if(InsertMode & 1) {
		s[40]='I';
		s[42]='n';
		s[44]='s';
	}
	else {
		s[40]=' ';
		s[42]=' ';
		s[44]=' ';
	}
	s[41]=14;
	s[43]=14;
	s[45]=14;
	s[46]=' ';
	s[47]=14;
	s[48]='P';
	s[49]=2;
	s[50]='a';
	s[51]=2;
	s[52]='g';
	s[53]=2;
	s[54]='e';
	s[55]=2;
	s[56]=':';
	s[57]=2;
	s[58]='0'+ActivePage;
	s[59]=2;
	s[60]=' ';
	s[61]=2;
	s[62]=' ';
	s[63]=2;
	s[64]='S';
	s[65]=9;
	s[66]='e';
	s[67]=9;
	s[68]='t';
	s[69]=9;
	s[70]=':';
	s[71]=9;
	s[72]=' ';
	s[73]=9;
	if(ActiveCharset>=10) {
		s[74]='0'+ActiveCharset/10;
		s[76]='0'+ActiveCharset%10;
	}
	else {
		s[74]='0'+ActiveCharset%10;
		s[76]=' ';
	}
	s[75]=9;
	s[77]=9;
	i=78;
	for(a=1;a<=10;a++) {
		s[i++]=' ';
		s[i++]=64;
		if(a>=10) {
			s[i++]='0'+a/10;
			s[i++]=64;
		}
		s[i++]='0'+a%10;
		s[i++]=64;
		s[i++]='=';
		s[i++]=64;
		s[i++]=CharSet[ActiveCharset][a-1];
		s[i++]=64+15;
	}
	while(i<160) {
		s[i++]=' ';
		s[i++]=64;
	}
	puttext(1,1,80,1,s);
}

void
CursorCheck(void)
{
	struct			text_info	ti;

	gettextinfo(&ti);
	if (CursorY < 0) {
		CursorY = 0;
		FirstLine--;
	}
	if (FullScreen) {
		if (CursorY > ti.screenheight - 1) {
			CursorY = ti.screenheight - 1;
			FirstLine++;
		}
	} else if (CursorY > ti.screenheight - 2) {
		CursorY = ti.screenheight - 2;
		FirstLine++;
	}
	if (FullScreen) {
		if (FirstLine + ti.screenheight > MaxLines)
			FirstLine = MaxLines - ti.screenheight;
	} else if (FirstLine + ti.screenheight - 1 > MaxLines)
		FirstLine = MaxLines - ti.screenheight + 1;
	if (FirstLine < 0)
		FirstLine = 0;
	if (CursorX < 0)
		CursorX = 0;
	if (CursorX > 79)
		CursorX = 79;
}

void
CursorHandling(int ch)
{
	struct			text_info	ti;
	struct			mouse_event	me;

	gettextinfo(&ti);
	switch (ch) {
	case CIO_KEY_MOUSE:
		getmouse(&me);
		if(me.endy>(FullScreen?0:1)) {
			if(!Dragging) {
				if(me.event==CIOLIB_BUTTON_1_CLICK) {
					CursorY=me.endy-2;
					CursorX=me.endx-1;
				}
				if(me.event==CIOLIB_BUTTON_1_DRAG_MOVE) {
					CursorY=me.endy-2;
					CursorX=me.endx-1;
				}
				if(me.event==CIOLIB_BUTTON_1_DRAG_START) {
					CursorY=me.endy-2;
					CursorX=me.endx-1;
				}
				if(me.event==CIOLIB_BUTTON_1_DRAG_END) {
					CursorY=me.endy-2;
					CursorX=me.endx-1;
				}
			}
			else {
				CursorY=me.endy-2;
				CursorX=me.endx-1;
			}
		}
		ungetmouse(&me);
		break;
	case CIO_KEY_DOWN:
		CursorY++;
		break;
	case CIO_KEY_UP:
		CursorY--;
		break;
	case CIO_KEY_LEFT:
		CursorX--;
		break;
	case CIO_KEY_RIGHT:
		CursorX++;
		break;
	case CIO_KEY_NPAGE:
		FirstLine += ti.screenheight - 1;
		if (FullScreen)
			CursorY = ti.screenheight - 1;
		else
			CursorY = ti.screenheight - 2;
		break;
	case CIO_KEY_PPAGE:
		FirstLine -= ti.screenheight - 1;
		CursorY = 0;
	case CIO_KEY_ALT_F(1):	/* SHIFT + F1 */
		ActiveCharset = 1;
		break;
	case CIO_KEY_ALT_F(2):	/* SHIFT + F2 */
		ActiveCharset = 2;
		break;
	case CIO_KEY_ALT_F(3):	/* SHIFT + F3 */
		ActiveCharset = 3;
		break;
	case CIO_KEY_ALT_F(4):	/* SHIFT + F4 */
		ActiveCharset = 4;
		break;
	case CIO_KEY_ALT_F(5):	/* SHIFT + F5 */
		ActiveCharset = 5;
		break;
	case CIO_KEY_ALT_F(6):	/* SHIFT + F6 */
		ActiveCharset = 6;
		break;
	case CIO_KEY_ALT_F(7):	/* SHIFT + F7 */
		ActiveCharset = 7;
		break;
	case CIO_KEY_ALT_F(8):	/* SHIFT + F8 */
		ActiveCharset = 8;
		break;
	case CIO_KEY_ALT_F(9):	/* SHIFT + F9 */
		ActiveCharset = 9;
		break;
	case CIO_KEY_ALT_F(10):	/* SHIFT + F10 */
		ActiveCharset = 10;
		break;
	case CIO_KEY_HOME:		/* POS 1  */
		CursorX = 0;
		break;
	case CIO_KEY_END:		/* END */
		CursorX = 79;
		break;
	}
}

void
putchacter(unsigned char c)
{
	int             a;

	if (InsertMode)
		for (a = 159; a >= CursorX * 2; a--)
			Screen[ActivePage][CursorY + FirstLine][a] = Screen[ActivePage][CursorY + FirstLine][a - 2];
	Screen[ActivePage][CursorY + FirstLine][CursorX * 2] = c;
	Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 1] = Attribute;
	CursorX++;
	if (CursorY + FirstLine + 1 >= LastLine)
		LastLine = CursorY + FirstLine + 1;
}

void
ShowCharacter(int num, unsigned char *c)
{
	int             a, b, i=0;
	char			buf[30*12*2];
	int				xoff,yoff;

	Openfont(num);
	if(*c>=127)
		*c='A';
	if(*c<=' ')
		*c='A';
	MaxX=0;
	(*c)--;
	while((MaxX == 0 || MaxY==0 || MaxX > 30 || MaxY > 12) && i < 128) {
		(*c)++;
		if(*c>=127)
			*c='!';
		i++;
		ReadFonts(*c);
	}
	memset(buf,0,sizeof(buf));
	if(MaxX && MaxY) {
		if (MaxX > 30)
			MaxX = 30;
		if (MaxY > 12)
			MaxY = 12;
		for (a = 1; a <= MaxY; a++) {
			yoff=((a-1)+(6-(MaxY/2)))*30;
			for (b = 1; b <= MaxX; b++) {
				xoff=(b-1+(15-((MaxX)/2)));
				switch (TDFont.FontType) {
				case 0:
				case 1:
					buf[(yoff+xoff)*2]=Chars[b][a];
					buf[(yoff+xoff)*2+1]=7;
					break;
				case 2:
					buf[(yoff+xoff)*2]=Chars[b*2][a];
					buf[(yoff+xoff)*2+1]=Chars[b*2-1][a];;
					break;
				}
			}
		}
	}
	puttext(31,6,60,17,buf);
};

int
SelectCharSet(void)
{
	unsigned short  a, y, z;
	int             ch,i;
	struct			mouse_event	me;
	char			buf[14*12*2];

	a = ActiveCharset;
	DrawBox(34, 6, 47, 18);
	do {
		i=0;
		for (y = 1; y <= 11; y++) {
			buf[i++]=' ';
			buf[i++]=2;
			for (z = 0; z < 10; z++) {
				buf[i++]=CharSet[y][z];
				buf[i++]=(y == a)?32 + 15:2;
			}
			buf[i++]=' ';
			buf[i++]=2;
		}
		puttext(35,7,46,17,buf);
		ch = newgetch();
		switch (ch) {
		case CIO_KEY_MOUSE:
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
			if(me.endx>=36 && me.endx<=45
					&& me.endy>=7 && me.endy<=17)
				a=me.endy-6;
				if(me.event==CIOLIB_BUTTON_1_CLICK)
					ch=13;
			break;
		case CIO_KEY_UP:
			a--;
			if (a == 0)
				a = 1;
			break;
		case CIO_KEY_DOWN:
			a++;
			if (a == 12)
				a = 11;
			break;
		case CIO_KEY_NPAGE:
			a = 11;
			break;
		case CIO_KEY_PPAGE:
			a = 1;
			break;
		}
	} while ((ch != 13) && (ch != 27));
	if (ch == 13) {
		return a;
	} else
		return ActiveCharset;
}

int
SelectFont(void)
{
	unsigned		char c='a';
	char            Fnts[2000][16];
	FILE           *fp;
	long            b;
	int				a, y;
	int             ch;
	char            FontFile[255];
	struct			mouse_event	me;
	char			tline[32];
	char			tlines[32];
	char			bline[32];
	char			blines[32];
	char			buf[11*16*2];

	memset(buf,0,sizeof(buf));
	sprintf(FontFile, "%s%s", homedir(), "allfont.fnt");
	DrawBox(10, 6, 27, 18);
	DrawBox(30, 5, 61, 18);
	fp = fopen(FontFile, "rb");
	if (fp != NULL) {
		fread(&Header.sign, 1, 10, fp);
		fread(&Header.NumberofFonts, 2, 1, fp);
		Header.NumberofFonts=LE_SHORT(Header.NumberofFonts);
		for (b = 0; b <= Header.NumberofFonts; b++) {
			Openfont(b);
			memcpy(&Fnts[b + 1], &FontRec.FontName, 16);
		}
		fclose(fp);
	}
	b = 0;
	a = 1;
	ShowCharacter(a + b - 1, &c);
	gettext(11,6,26,6,tline);
	memcpy(tlines,tline,sizeof(tline));
	tlines[8]=24; /* Up arrow */
	tlines[22]=24; /* Up arrow */
	gettext(11,18,26,18,bline);
	memcpy(blines,bline,sizeof(tline));
	blines[8]=25; /* Down arrow */
	blines[22]=25; /* Down arrow */
	do {
		if(b > 0)
			puttext(11,6,26,6,tlines);
		else
			puttext(11,6,26,6,tline);
		if(b < Header.NumberofFonts - 11)
			puttext(11,18,26,18,blines);
		else
			puttext(11,18,26,18,bline);

		for (y = 1; y <= 11; y++) {
			if (y <= Header.NumberofFonts) {
				bufprintf(&buf[(y-1)*16*2],(y==a)?32+15:2,"%-*.*s",16,16,Fnts[y + b]+1);
			}
		}
		puttext(11,7,26,17,buf);
		ch = newgetch();
		switch (ch) {
		case CIO_KEY_MOUSE:
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
			if(me.endx>=11 && me.endx<=26
					&& me.endy>=7 && me.endy<=17) {
				a=me.endy-6;
				if(me.event==CIOLIB_BUTTON_1_CLICK)
					ch=13;
			}
			if(me.endx>=11 && me.endx<=26
					&& me.endy==6 && me.event==CIOLIB_BUTTON_1_CLICK) {
				/* Scroll Up */
				if(b>0) {
					b-=5;
					a+=5;
					if(a>11)
						a=11;
				}
			}
			if(me.endx>=11 && me.endx<=26
					&& me.endy==18 && me.event==CIOLIB_BUTTON_1_CLICK) {
				/* Scroll Down */
				if(b<Header.NumberofFonts - 11) {
					b+=5;
					a-=5;
					if(a<1)
						a=1;
				}
			}
			break;
		case CIO_KEY_LEFT:
			c--;
			break;
		case CIO_KEY_RIGHT:
			c++;
			break;
		case CIO_KEY_UP:
			a--;
			break;
		case CIO_KEY_DOWN:
			a++;
			break;
		case CIO_KEY_NPAGE:
			a += 10;
			if (a > 11) {
				b += a-11;
				a = 11;
			}
			break;
		case CIO_KEY_PPAGE:
			a -= 10;
			if (a < 1) {
				b += a-1;
				a = 1;
			}
			break;
		default:
			if(ch<=255)
				c=ch;
		}
		if (a==12 && b==Header.NumberofFonts - 11) {
			a=1;
			b=0;
		}
		if (a==0 && b==0) {
			if(Header.NumberofFonts>=11) {
				a=11;
				b=Header.NumberofFonts-11;
			}
			else {
				a=Header.NumberofFonts;
			}
		}
		if (a < 1) {
			b=b+(a-1);
			a=1;
		}
		if (a > Header.NumberofFonts) {
			a = Header.NumberofFonts;
			b=0;
		}
		if (a > 11) {
			b+=(a-11);
			a = 11;
		}
		if (b < 0)
			b = 0;
		if ((b + 11 > Header.NumberofFonts) & (Header.NumberofFonts > 11)) {
			b = Header.NumberofFonts - 11;
		}
		ShowCharacter(a + b - 1, &c);
	} while ((ch != 27) & (ch != 13));
	if (ch == 13) {
		return (a + b - 1);
	} else
		return 0;
}
void
DrawFontCharakter(char c)
{
	int             a, b;
	struct			text_info	ti;

	gettextinfo(&ti);
	ReadFonts(c);
	if ((CursorX + MaxX + TDFont.Spaces > 80) | (MaxX == 0) | (MaxY + FirstLine + CursorY > MaxLines)) {
		goto end;
	}
	FontTyped = TRUE;
	if (MaxY + CursorY > ti.screenheight - 1) {
		FirstLine += MaxY + CursorY - ti.screenheight + 2;
		CursorY -= MaxY + CursorY - ti.screenheight + 2;
	}
	for (a = 1; a <= MaxX; a++)
		for (b = 1; b <= MaxY; b++) {
			switch (TDFont.FontType) {
			case 0:
			case 1:
				Screen[ActivePage][CursorY + FirstLine + b - 1][(CursorX + a - 1) * 2] = Chars[a][b];
				Screen[ActivePage][CursorY + FirstLine + b - 1][(CursorX + a - 1) * 2 + 1] = Attribute;
				break;
			case 2:
				Screen[ActivePage][CursorY + FirstLine + b - 1][(CursorX + a - 1) * 2] = Chars[a * 2][b];
				Screen[ActivePage][CursorY + FirstLine + b - 1][(CursorX + a - 1) * 2 + 1] = Chars[a * 2 - 1][b];
				break;
			}
		}
	cpos++;
	CursorPos[cpos] = MaxX + TDFont.Spaces;
	CursorX += MaxX + TDFont.Spaces - 1;
	if (MaxY + CursorY + FirstLine > LastLine)
		LastLine = MaxY + CursorY + FirstLine;
end:
	return;
}

void
setunderground(int Underg, int b)
{
	int             x, y;
	struct			text_info	ti;

	gettextinfo(&ti);
	for (x = 1; x <= 7; x++)
		for (y = 1; y < 6; y++)
			SelectAnsi[(4 + x + (b % 8) * 9) * 2 + (3 + y + (b / 8) * 6) * 160 + 1] = Underg;
	puttext(1,1,80,ti.screenheight>24?25:ti.screenheight,SelectAnsi);
}

unsigned char
asciitable(void)
{
	int             y, ch, x = 32;
	struct			mouse_event	me;
	char			buf[512+32];

	DrawBox(30, 5, 47, 23);
	do {
		bufprintf(buf,7,"hex :%02x  dec:%03d", x, x);
		for (y = 0; y <= 255; y++) {
			buf[y*2+32]=y;
			buf[y*2+32+1]=(y==x)?27:3;
		}
		puttext(31,6,46,22,buf);
		ch = newgetch();
		switch (ch) {
		case CIO_KEY_MOUSE:
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				return(0);
			if (me.endx > 30 && me.endx < 47 &&
			    me.endy > 6 && me.endy < 23) {
				x = (me.endx - 31) + (me.endy - 7) * 16;
				if (me.event == CIOLIB_BUTTON_1_CLICK)
					return x;
			}
			break;
		case CIO_KEY_LEFT:
			if (x > 0)
				x--;
			else
				x = 255;
			break;
		case CIO_KEY_RIGHT:
			if (x < 255)
				x++;
			else
				x = 0;
			break;
		case CIO_KEY_DOWN:
			if (x <= 239)
				x += 16;
			break;
		case CIO_KEY_UP:
			if (x >= 16)
				x -= 16;
			break;
		case CIO_KEY_HOME:
			if(x)
				x = (x / 16)*16;
			break;
		case CIO_KEY_END:
			if(x)
				x = (x / 16)*16;
			x += 15;
			break;
		case CIO_KEY_PPAGE:
			if(x)
				x=x % 16;
			break;
		case CIO_KEY_NPAGE:
			if(x)
				x=x % 16;
			x+=240;
			break;
		default:
			return(ch);
		}
	} while (ch != 27 && ch != 13);
	if (ch == 13 && x != 127)
		return x;
	return 0;
}


void
SelectOutline(void)
{
	int             ch, b;
	int				col,row;
	unsigned int	i;
	struct			mouse_event	me;

	b = Outline;
	do {
		setunderground(18, b);
		ch = newgetch();
		setunderground(2, b);
		if ((ch >= 'A') & (ch <= 'S')) {
			Outline = ch - 'A';
			ch = 27;
		}
		if ((ch >= 'a') & (ch <= 's')) {
			Outline = ch - 'a';
			ch = 27;
		}
		switch (ch) {
		case CIO_KEY_MOUSE:
			row=col=0;
			getmouse(&me);
			i=(me.endx-6)%9;
			if(i<=6)
				col=((me.endx-5)/9)+1;
			if(col>8)
				col=0;
			i=(me.endy-5)%6;
			if(i<=4)
				row=((me.endy-4)/6)+1;
			if(row>3)
				row=0;
			if((row-1)*8+(col-1)>18)
				row=col=0;
			if(row && col)
				b=(row-1)*8+(col-1);
			if(me.event==CIOLIB_BUTTON_1_CLICK && row && col) {
				Outline=b;
				ch=27;
			}
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
			break;
		case CIO_KEY_LEFT:
			if (b > 0)
				b--;
			else
				b = 18;
			break;
		case CIO_KEY_RIGHT:
			if (b < 18)
				b++;
			else
				b = 0;
			break;
		case CIO_KEY_DOWN:
			if ((b < 16) & (b < 11))
				b += 8;
			break;
		case CIO_KEY_UP:
			if (b > 7)
				b -= 8;
			break;
		case 13:
			Outline = b;
			ch = 27;
			break;
		}
	} while (ch != 27);
}

int
main(int argnum, char *args[])
{
	int             a, b;
	int             fg, bg;
	struct			mouse_event	me;

	_wscroll=0;

	initciolib(CIOLIB_MODE_AUTO);
	settitle("SyncDraw");

	CharSet[0] = NCharSet[0];
	CharSet[1] = NCharSet[1];
	CharSet[2] = NCharSet[2];
	CharSet[3] = NCharSet[3];
	CharSet[4] = NCharSet[4];
	CharSet[5] = NCharSet[5];
	CharSet[6] = NCharSet[6];
	CharSet[7] = NCharSet[7];
	CharSet[8] = NCharSet[8];
	CharSet[9] = NCharSet[9];
	CharSet[10] = NCharSet[10];
	CharSet[11] = NCharSet[11];
	CharSet[12] = NCharSet[12];

	for (a = 0; a < 4; a++)
		for (y = 0; y <= MaxLines; y++) {
			for (x = 0; x < 80; x++) {
				Screen[a][y][x << 1] = 32;
				Screen[a][y][(x << 1) + 1] = 7;
			}
		}
	loadconfig();
	if (argnum > 1) {
		printf("Loading file :%s\n", args[1]);
		LoadFile(args[1]);
	}
	ciomouse_setevents(0);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_MOUSE_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_3_CLICK);

	do {
		Statusline();
		ShowScreen(0, 0);
		if (FullScreen)
			gotoxy(CursorX + 1, CursorY + 1);
		else
			gotoxy(CursorX + 1, CursorY + 2);
		/* *** */
		a = newgetch();

		CursorHandling(a);
		if(a==CIO_KEY_MOUSE) {
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_1_CLICK) {
				if(me.endy==1 && !FullScreen) {
					if(me.endx>=12 && me.endx<=16)
						Attribute=QuickColor();
					if(me.endx>=18 && me.endx<=20)
						EliteMode=!EliteMode;
					if(me.endx>=25 && me.endx<=31)
						ActivePage=3-ActivePage;
					if(me.endx>=37 && me.endx<=80)
						ActiveCharset = SelectCharSet();
				}
			}
			if(me.event==CIOLIB_BUTTON_1_DRAG_START)
				blockmode();
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				a=27;;
		}
		switch (a) {
		case CIO_KEY_MOUSE:
			/* already handled */
			break;
		case 0x2c00:	/* ALT+Z - Blink on\off */
			Attribute ^= 128;
			break;
		case 0x2500:	/* ALT+P - ASCII Chart */
			putchacter(asciitable());
			break;
		case 0x7800:	/* ALT-1 */
			DelCol();
			break;
		case 0x7900:	/* ALT-2 */
			InsCol();
			break;
		case 0x1300:	/* ALT+R UNDO */
			UndoLast();
			break;
		case 0x2200:	/* ALT-G */
			global();
			break;
		case 0x1600:	/* ALT+U PiCKUP COLOR */
			Attribute = Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 1];
			break;
		case 0x1100:	/* ALT-W */
			SelectOutline();
			break;
		case 0x1700:	/* ALT+I InsLine */
			InsLine();
			break;
		case 0x1500:	/* ALT+Y - DelLine */
			DelLine();
			break;
		case 0x1200:	/* ALT+E - Elite */
			EliteMode = !EliteMode;
			break;
		case 0x2d00:	/* ALT+X - Exit */
			if(exitprg()==-1)
				return(0);
			break;
		case 0x1e00:	/* ALT+A - Color */
			Attribute = SelectColor();
			break;
		case 0x3100:	/* ALT+N - Font Mode */
			FontMode = !FontMode;
			Undo = 0;
			SaveScreen();
			break;
		case 0x1f00:	/* ALT-S */
			save();
			break;
		case 0x2e00:	/* ALT+C - ClearScreen */
			ClearScreen();
			break;
		case 0x2600:	/* ALT-L */
			load();
			break;
		case 0x1900:	/* ALT+P - SetPage */
			SetPage();
			break;
		case 0x1400:	/* ALT-T */
			tabsetup();
			break;
		case 0x2300:	/* alt+h help */
			help();
			break;
		case 0x8200:	/* alt+- draw mode */
			drawmode();
			break;
		case 0xa500:/* ALT+TAB */
			CursorX = tabback(CursorX);
			break;
		case 0x3000:	/* ALT-B */
			blockmode();
			break;
		case 0x2000:	/* ALT+D Draw Line Mode */
			drawline();
			break;
		case 0x3200:	/* ALT-M */
			select_effekt();
			break;
		case 0x2100:	/* ALT+F - Select Font */
			a = SelectFont();
			if (a > 0) {
				SFont = a;
				Openfont(SFont);
			}
			/* ActiveCharset=SelectCharSet(); */
			break;
		case 28:	/* PRiNT SCREEN - Toggle fullscreen on/off */
			FullScreen = !FullScreen;
			break;
		case 27:	/* ESC - Menue Mode  */
			if(menuemode()==-1)
				return(0);
			break;
		case 1:	/* Next FG Colour */
			fg = ((Attribute & 128) >> 3) | (Attribute & 15);
			bg = (Attribute & 112) >> 4;
			fg -= 1;
			Attribute = ((fg & 16) << 3) | (fg & 15) | ((bg & 7) << 4);
			break;
		case 4:	/* Prev FG Colour */
			fg = ((Attribute & 128) >> 3) | (Attribute & 15);
			bg = (Attribute & 112) >> 4;
			fg += 1;
			Attribute = ((fg & 16) << 3) | (fg & 15) | ((bg & 7) << 4);
			break;
		case 26:	/* Next BG Colour */
			fg = ((Attribute & 128) >> 3) | (Attribute & 15);
			bg = (Attribute & 112) >> 4;
			bg -= 1;
			Attribute = ((fg & 16) << 3) | (fg & 15) | ((bg & 7) << 4);
			break;
		case 3:	/* Prev BG Colour */
			fg = ((Attribute & 128) >> 3) | (Attribute & 15);
			bg = (Attribute & 112) >> 4;
			bg += 1;
			Attribute = ((fg & 16) << 3) | (fg & 15) | ((bg & 7) << 4);
			break;
		case 19:
			SauceSet();
			break;
		case 9:	/* TAB */
			CursorX = tabforward(CursorX);
			break;
		case 13:	/* RETURN */
			CursorX = 0;
			if ((FontMode) & (FontTyped)) {
				CursorY += MaxY;
				Undo = FALSE;
				SaveScreen();
				FontTyped = FALSE;
			} else
				CursorY++;
			cpos = 0;
			break;
		case 127:	/* Entf */
			for (a = CursorX * 2; a <= 157; a++)
				Screen[ActivePage][CursorY + FirstLine][a] = Screen[ActivePage][CursorY + FirstLine][a + 2];
			Screen[ActivePage][CursorY + FirstLine][159] = 7;
			Screen[ActivePage][CursorY + FirstLine][158] = 32;
			break;
		case CIO_KEY_IC:	/* Insert */
			InsertMode = !InsertMode;
			break;

		case 8:	/* CIO_KEY_BACKSPACE */
			if (CursorX > 0) {
				if ((FontMode) & (FontTyped) & (cpos > 0)) {
					CursorX -= CursorPos[cpos] - 1;
					for (a = 0; a <= CursorPos[cpos]; a++)
						for (b = 0; b <= MaxY; b++) {
							Screen[ActivePage][CursorY + FirstLine + b][(CursorX + a) * 2] = Screen[UNDOPage][CursorY + FirstLine + b][(CursorX + a) * 2];
							Screen[ActivePage][CursorY + FirstLine + b][(CursorX + a) * 2 + 1] = Screen[UNDOPage][CursorY + FirstLine + b][(CursorX + a) * 2 + 1];
						}
					cpos--;
				} else {
					cpos = 0;
					CursorX--;
					if (InsertMode) {
						for (a = CursorX * 2; a <= 156; a++)
							Screen[ActivePage][CursorY + FirstLine][a] = Screen[ActivePage][CursorY + FirstLine][a + 2];
						Screen[ActivePage][CursorY + FirstLine][159] = Attribute;
						Screen[ActivePage][CursorY + FirstLine][158] = 32;
					} else {
						Screen[ActivePage][CursorY + FirstLine][CursorX * 2] = 32;
						Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 1] = Attribute;
					}
				}
			}
			break;
		default:
			if (a <= 255) {
				if ((FontMode) & (a > 32) & (a < 127)) {
					DrawFontCharakter(a);
				} else {
					if ((FontMode) & (FontTyped)) {
						cpos++;
						CursorPos[cpos] = 2;
					}
					if (EliteMode)
						putchacter(translate(a));
					else
						putchacter(a);
				}
			}
		}
		for (b = 1; b <= 10; b++)
		if (a == CIO_KEY_F(b)) {
			putchacter(CharSet[ActiveCharset][b - 1]);
		}
		CursorCheck();
	} while (1 == 1);
}
