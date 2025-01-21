/* Synchronet user data fields */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _USERFIELDS_H
#define _USERFIELDS_H

// User data field indexes
// Do not insert new fields into following enum, add to the end
enum user_field {
	USER_ID,
	USER_ALIAS,
	USER_NAME,
	USER_HANDLE,
	USER_NOTE,
	USER_IPADDR,
	USER_HOST,
	USER_NETMAIL,
	USER_ADDRESS,
	USER_LOCATION,
	USER_ZIPCODE,
	USER_PHONE,
	USER_BIRTH,
	USER_GENDER,
	USER_COMMENT,
	USER_CONNECTION,

	// Bit-fields:
	USER_MISC,
	USER_QWK,
	USER_CHAT,

	// Settings:
	USER_ROWS,
	USER_COLS,
	USER_XEDIT,
	USER_SHELL,
	USER_TMPEXT,
	USER_PROT,
	USER_CURSUB,
	USER_CURDIR,
	USER_CURXTRN,

	// Date/times:
	USER_LOGONTIME,
	USER_NS_TIME,
	USER_LASTON,
	USER_FIRSTON,

	// Counting stats:
	USER_LOGONS,
	USER_LTODAY,
	USER_TIMEON,
	USER_TTODAY,
	USER_TLAST,
	USER_POSTS,
	USER_EMAILS,
	USER_FBACKS,
	USER_ETODAY,
	USER_PTODAY,

	// File xfer stats:
	USER_ULB,
	USER_ULS,
	USER_DLB,
	USER_DLS,
	USER_DLCPS,

	// Security:
	USER_PASS,
	USER_PWMOD,
	USER_LEVEL,
	USER_FLAGS1,
	USER_FLAGS2,
	USER_FLAGS3,
	USER_FLAGS4,
	USER_EXEMPT,
	USER_REST,
	USER_CDT,
	USER_FREECDT,
	USER_MIN,
	USER_TEXTRA,
	USER_EXPIRE,
	USER_LEECH,

	// Misc:
	USER_MAIL,
	USER_LANG,
	USER_DELDATE,

	// Last:
	USER_FIELD_COUNT
};

#endif
