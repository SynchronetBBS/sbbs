/* Synchronet CP437 <-> UTF-8 translation table */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "cp437_utf8_tbl.h"
#include "stdio.h"	// NULL

// Want UTF-8 encondings of terminatl control characters?
#if defined USE_UTF8_FOR_TERM_CTRL_CHARS
#	define UTF8_TERM_CTRL_CHAR_SEQ(x) x
#else
#	define UTF8_TERM_CTRL_CHAR_SEQ(x) NULL
#endif

// CP437 character to/from UNICODE/UTF-8 sequence conversion
// The CP437 character value is the index into the table
// If the pointer at that index is NULL, no translation is needed (1:1 mapping)
const char* cp437_utf8_tbl[] =
{
	// 0x00:
	NULL,
	// 0x01:
	"\xE2\x98\xBA", // 263A
	// 0x02:
	"\xE2\x98\xBB", // 263B
	// 0x03:
	"\xE2\x99\xA5", // 2665
	// 0x04:
	"\xE2\x99\xA6", // 2666
	// 0x05:
	"\xE2\x99\xA3", // 2663
	// 0x06:
	"\xE2\x99\xA0", // 2660
	// 0x07: '\a'
	UTF8_TERM_CTRL_CHAR_SEQ("\xE2\x80\xA2"), // 2022
	// 0x08: '\b'
	UTF8_TERM_CTRL_CHAR_SEQ("\xE2\x97\x98"), // 25D8
	// 0x09: '\t'
	UTF8_TERM_CTRL_CHAR_SEQ("\xE2\x97\x8B"), // 25CB
	// 0x0A: '\n'
	UTF8_TERM_CTRL_CHAR_SEQ("\xE2\x97\x99"), // 25D9
	// 0x0B:
	"\xE2\x99\x82", // 2642
	// 0x0C: '\f'
	UTF8_TERM_CTRL_CHAR_SEQ("\xE2\x99\x80"), // 2640
	// 0x0D: '\r'
	UTF8_TERM_CTRL_CHAR_SEQ("\xE2\x99\xAA"), // 266A
	// 0x0E:
	"\xE2\x99\xAB", // 266B
	// 0x0F:
	"\xE2\x98\xBC", // 263C
	// 0x10:
	"\xE2\x96\xBA", // 25BA
	// 0x11:
	"\xE2\x97\x84", // 25C4
	// 0x12:
	"\xE2\x86\x95", // 2195
	// 0x13:
	"\xE2\x80\xBC", // 203C
	// 0x14:
	"\xC2\xB6", // 00B6
	// 0x15:
	"\xC2\xA7", // 00A7
	// 0x16:
	"\xE2\x96\xAC", // 25AC
	// 0x17:
	"\xE2\x86\xA8", // 21A8
	// 0x18:
	"\xE2\x86\x91", // 2191
	// 0x19:
	"\xE2\x86\x93", // 2193
	// 0x1A:
	"\xE2\x86\x92", // 2192
	// 0x1B:
	"\xE2\x86\x90", // 2190
	// 0x1C:
	"\xE2\x88\x9F", // 221F
	// 0x1D:
	"\xE2\x86\x94", // 2194
	// 0x1E:
	"\xE2\x96\xB2", // 25B2
	// 0x1F:
	"\xE2\x96\xBC", // 25BC
	// 0x20-0x7F	(1:1 with US-ASCII and CP437)
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	// 0x80:
	"\xC3\x87",	// 00C7
	// 0x81:
	"\xC3\xBC",	// 00FC
	// 0x82:
	"\xC3\xA9", // 00E9
	// 0x83:
	"\xC3\xA2", // 00E2
	// 0x84:
	"\xC3\xA4", // 00E4
	// 0x85:
	"\xC3\xA0", // 00E0
	// 0x86:
	"\xC3\xA5", // 00E5
	// 0x87:
	"\xC3\xA7", // 00E7
	// 0x88:
	"\xC3\xAA", // 00EA
	// 0x89:
	"\xC3\xAB", // 00EB
	// 0x8A:
	"\xC3\xA8", // 00E8
	// 0x8B:
	"\xC3\xAF", // 00EF
	// 0x8C:
	"\xC3\xAE", // 00EE
	// 0x8D:
	"\xC3\xAC", // 00EC
	// 0x8E:
	"\xC3\x84", // 00C4
	// 0x8F:
	"\xC3\x85", // 00C5
	// 0x90:
	"\xC3\x89", // 00C9
	// 0x91:
	"\xC3\xA6", // 00E6
	// 0x92:
	"\xC3\x86", // 00C6
	// 0x93:
	"\xC3\xB4", // 00F4
	// 0x94:
	"\xC3\xB6", // 00F6
	// 0x95:
	"\xC3\xB2", // 00F2
	// 0x96:
	"\xC3\xB8", // 00F8
	// 0x97:
	"\xC3\xB9", // 00F9
	// 0x98:
	"\xC3\xBF", // 00FF
	// 0x99:
	"\xC3\x96", // 00D6
	// 0x9A:
	"\xC3\x9C", // 00DC
	// 0x9B:
	"\xC2\xA2", // 00A2
	// 0x9C:
	"\xC2\xA3", // 00A3
	// 0x9D:
	"\xC2\xA5", // 00A5
	// 0x9E:
	"\xC2\xA7", // 20A7
	// 0x9F:
	"\xC6\x92", // 0192
	// 0xA0:
	"\xC3\xA1", // 00E1
	// 0xA1:
	"\xC3\xAD", // 00ED
	// 0xA2:
	"\xC3\xB3", // 00F3
	// 0xA3:
	"\xC3\xBA", // 00FA
	// 0xA4:
	"\xC3\xB1", // 00F1
	// 0xA5:
	"\xC3\x91", // 00D1
	// 0xA6:
	"\xC2\xAA", // 00AA
	// 0xA7:
	"\xC2\xBA", // 00BA
	// 0xA8:
	"\xC2\xBF", // 00BF
	// 0xA9:
	"\xE2\x8C\x90", // 2310
	// 0xAA:
	"\xC2\xAC", // 00AC
	// 0xAB:
	"\xC2\xBD", // 00BD
	// 0xAC:
	"\xC2\xBC", // 00BC
	// 0xAD:
	"\xC2\xA1", // 00A1
	// 0xAE:
	"\xC2\xAB", // 00AB
	// 0xAF:
	"\xC2\xBB", // 00BB
	// 0xB0:
	"\xE2\x96\x91", // 2591
	// 0xB1:
	"\xE2\x96\x92", // 2592
	// 0xB2:
	"\xE2\x96\x93", // 2593
	// 0xB3:
	"\xE2\x94\x82", // 2502
	// 0xB4:
	"\xE2\x94\xA4", // 2524
	// 0xB5:
	"\xE2\x95\xA1", // 2561
	// 0xB6:
	"\xE2\x95\xA2", // 2562
	// 0xB7:
	"\xE2\x95\x96", // 2556
	// 0xB8:
	"\xE2\x95\x95", // 2555
	// 0xB9:
	"\xE2\x95\xA3", // 2563
	// 0xBA:
	"\xE2\x95\x91", // 2551
	// 0xBB:
	"\xE2\x95\x97", // 2557
	// 0xBC:
	"\xE2\x95\x9D", // 255D
	// 0xBD:
	"\xE2\x95\x9C", // 255C
	// 0xBE:
	"\xE2\x95\x9B", // 255B
	// 0xBF:
	"\xE2\x94\x90", // 2510
	// 0xC0:
	"\xE2\x94\x94", // 2514
	// 0xC1:
	"\xE2\x94\xB4", // 2534
	// 0xC2:
	"\xE2\x94\xAC", // 252C
	// 0xC3:
	"\xE2\x94\x9C", // 251C
	// 0xC4:
	"\xE2\x94\x80", // 2500
	// 0xC5:
	"\xE2\x94\xBC", // 253C
	// 0xC6:
	"\xE2\x95\x9E", // 255E
	// 0xC7:
	"\xE2\x95\x9F", // 255F
	// 0xC8:
	"\xE2\x95\x9A", // 255A
	// 0xC9:
	"\xE2\x95\x94", // 2554
	// 0xCA:
	"\xE2\x95\xA9", // 2569
	// 0xCB:
	"\xE2\x95\xA6", // 2566
	// 0xCC:
	"\xE2\x95\xA0", // 2560
	// 0xCD:
	"\xE2\x95\x90", // 2550
	// 0xCE:
	"\xE2\x95\xAC", // 256C
	// 0xCF:
	"\xE2\x95\xA7", // 2567
	// 0xD0:
	"\xE2\x95\xA8", // 2568
	// 0xD1:
	"\xE2\x95\xA4", // 2564
	// 0xD2:
	"\xE2\x95\xA5", // 2565
	// 0xD3:
	"\xE2\x95\x99", // 2559
	// 0xD4:
	"\xE2\x95\x98", // 2558
	// 0xD5:
	"\xE2\x95\x92", // 2552
	// 0xD6:
	"\xE2\x95\x93", // 2553
	// 0xD7:
	"\xE2\x95\xAB", // 256B
	// 0xD8:
	"\xE2\x95\xAA", // 256A
	// 0xD9:
	"\xE2\x94\x98", // 2518
	// 0xDA:
	"\xE2\x94\x8C", // 250C
	// 0xDB:
	"\xE2\x96\x88", // 2588
	// 0xDC:
	"\xE2\x96\x84", // 2584
	// 0xDD:
	"\xE2\x96\x8C", // 258C
	// 0xDE:
	"\xE2\x96\x90", // 2590
	// 0xDF:
	"\xE2\x96\x80", // 2580
	// 0xE0:
	"\xCE\xB1", // 03B1
	// 0xE1:
	"\xC3\x9F", // 00DF
	// 0xE2:
	"\xCE\x93", // 0393
	// 0xE3:
	"\xCF\x80", // 03C0
	// 0xE4:
	"\xCE\xA3", // 03A3
	// 0xE5:
	"\xCF\x83", // 03C3
	// 0xE6:
	"\xC2\xBF", // 00B5
	// 0xE7:
	"\xCF\x84", // 03C4
	// 0xE8:
	"\xCE\xA6", // 03A6
	// 0xE9:
	"\xCE\x98", // 0398
	// 0xEA:
	"\xCE\xA9", // 03A9
	// 0xEB:
	"\xCE\xB4", // 03B4
	// 0xEC:
	"\xE2\x88\x9E", // 221E
	// 0xED:
	"\xCF\x86", // 03C6
	// 0xEE:
	"\xCE\xB5", // 03B5
	// 0xEF:
	"\xE2\x88\xA9", // 2229
	// 0xF0:
	"\xE2\x89\xA1", // 2261
	// 0xF1:
	"\xC2\xB1", // 00B1
	// 0xF2:
	"\xE2\x89\xA5", // 2265
	// 0xF3:
	"\xE2\x89\xA4", // 2264
	// 0xF4:
	"\xE2\x8C\xA0", // 2320
	// 0xF5:
	"\xE2\x8C\xA1", // 2321
	// 0xF6:
	"\xC3\xB7", // 00F7
	// 0xF7:
	"\xE2\x89\x88", // 2248
	// 0xF8:
	"\xC2\xB0", // 00B0
	// 0xF9:
	"\xE2\x88\x99", // 2219
	// 0xFA:
	"\xC2\xB7", // 00B7
	// 0xFB:
	"\xE2\x88\x9A", // 221A
	// 0xFC:
	"\xE2\x81\xBF", // 207F
	// 0xFD:
	"\xC2\xB2", // 00B2
	// 0xFE:
	"\xE2\x96\xA0", // 25A0
	// 0xFF:
	"\xC2\xA0" // 00A0
};
