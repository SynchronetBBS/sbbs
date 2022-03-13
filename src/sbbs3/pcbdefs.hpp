/* PCBoard Structure Definitions, Source: SYS.C and USERSYS.H v15.3 */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef PCBDEFS_HPP_
#define PCBDEFS_HPP_

#include "endian.hpp"

namespace PCBoard {

static const uint16_t Version{1530};

class abool { // "ASCII Bool" craziness
	uint16_t value{False};
public:
	static const uint16_t True = LE_INT(0x312D);	// "-1"
	static const uint16_t False = LE_INT(0x3020);	// " 0"
	void operator = (int nval) {
		value = nval ? True : False;
	}
	bool operator = (const abool&) {
		return value ==  True;
	}
};

// Unterminated space-padded ASCII string
template <size_t size>
class string {
	char value[size]{};
public:
	void operator = (const char* s) {
		memset(value, ' ', size);
		memcpy(value, s, min(size, strlen(s)));
	}
};

// All multi-byte integer data types are all implicitly little-endian (x86),
// so provide automatic byte-swap (on big-endian systems) during assignment operations.

using Int16 = LittleEndInt<int16_t>;
using Int32 = LittleEndInt<int32_t>;
using UInt16 = LittleEndInt<uint16_t>;
using UInt32 = LittleEndInt<uint32_t>;

static const int USER_FLAG_DIRTY		= (1<<0);
static const int USER_FLAG_MSGCLEAR		= (1<<1);
static const int USER_FLAG_HASMAIL		= (1<<2);
static const int USER_FLAG_NOFSE		= (1<<3);	// DontAskFSE
static const int USER_FLAG_SCROLLMSG	= (1<<4);	// ScrollMsgBody
static const int USER_FLAG_SHORTHDR		= (1<<5);	// ShortHeader
static const int USER_FLAG_WIDEEDITOR	= (1<<6);

#pragma pack(push,1)	// Disk image structures must be packed

struct sys {
	abool    Screen;
	abool    PrintLog;
	abool    PageBell;
	abool    Alarm;
	char     SysopFlag{' '};
	abool    ErrorCorrected;
	char     GraphicsMode;
	char     UserNetStatus{'A'};
	string<5> ModemSpeed;         /* rate to open com port  */
	string<5> CarrierSpeed;       /* actual speed of caller */
	UInt16   UserRecNo;
	string<15> FirstName;
	string<12> Password;
	UInt16   LogonMinute;
	Int16    TimeUsed;
	string<5> LogonTime;
	Int16    PwrdTimeAllowed;
	Int16    MaxKBytesAllowed;
	char     Conference;
	char     ConfJoined[5];
	char     ConfScanned[5];
	Int16    ConfAddTime;
	Int16    CreditMinutes;
	char     MultiLangExt[4];
	string<25> Name;
	Int16    MinutesLeft;
	uint8_t  NodeNum;	// Binary node number or 0
	string<5> EventTime;
	abool    EventActive;
	abool    EventSlide;
	char     MemorizeNum[4];
	char     ComPortNumber{'1'};	// ASCII decimal ('0' = none)
	char     PackFlag{' '};
	uint8_t  Reserve;
	int8_t   UseAnsi; // bool
	/* used to have 8 bytes for date of last event run and 2 bytes for the time */
	Int16    Country;
	Int16    CodePage;
	char     YesChar;
	char     NoChar;
	int8_t   Language;
	char     Reserve2[3];   /* the rest of the date/time used to be used */
	int8_t   RemoteDOS; // bool
	int8_t   RunningEvent;  //bool, was EventUpComing
	int8_t   StopUploads; // bool
	UInt16   Conference2;
};

struct userSysHeader {
	UInt16 Version;           /* PCBoard version number */
	Int32  RecNo;             /* Record number from USER's file */
	UInt16 SizeOfRec;         /* Size of "fixed" user record */
	UInt16 NumOfAreas;        /* Number of conference areas (Main=1 thru 65535) */
	UInt16 NumOfBitFields;    /* Number of Bit Map fields for conferences */
	UInt16 SizeOfBitFields;   /* Size of each Bit Map field */
	char   AppName[15];       /* Name of the Third Party Application (if any) */
	UInt16 AppVersion;        /* Version number for the application (if any) */
	UInt16 AppSizeOfRec;      /* Size of a "fixed length" record (if any) */
	UInt16 AppSizeOfConfRec;  /* Size of each conference record (if any) */
	Int32  AppRecOffset;      /* Offset of AppRec into USERS.INF record (if any) */
	int8_t Updated;           /* TRUE if the USERS.SYS file has been updated */
};

struct userSysFixed {
	// The strings in this struct *are* NUL-terminated
	char           Name[26];           /* Name (NULL terminated) */
	char           City[25];           /* City (NULL terminated) */
	char           Password[13];       /* Password (NULL terminated) */
	char           BusDataPhone[14];   /* Business or Data Phone (NULL terminated) */
	char           HomeVoicePhone[14]; /* Home or Voice Phone (NULL terminated) */
	UInt16         LastDateOn;         /* Julian date for the Last Date On */
	char           LastTimeOn[6];      /* Last Time On (NULL Terminated) */
	int8_t         ExpertMode;         /* 1=Expert, 0=Novice */
	char           Protocol;           /* Protocol (A thru Z) */
	uint8_t        PackedFlags;        /* Bit packed flags */
	UInt16         DateLastDirRead;    /* Date for Last DIR Scan (most recent file) */ // DOS format
	Int16          SecurityLevel;      /* Security Level */
	UInt16         NumTimesOn;         /* Number of times the caller has connected */
	uint8_t        PageLen;            /* Page Length when display data on the screen */
	UInt16         NumUploads;         /* Total number of FILES uploaded */
	UInt16         NumDownloads;       /* Total number of FILES downloaded */
	UInt32         DailyDnldBytes;     /* Number of BYTES downloaded so far today (was "long") */
	char           UserComment[31];    /* Comment field #1 (NULL terminated) */
	char           SysopComment[31];   /* Comment field #1 (NULL terminated) */
	Int16          ElapsedTimeOn;      /* Number of minutes online */
	UInt16         RegExpDate;         /* Julian date for Registration Expiration Date */
	Int16          ExpSecurityLevel;   /* Expired Security Level */
	UInt16         LastConference;     /* Number of the conference the caller was in */
	UInt32         ulTotDnldBytes;     /* Total number of BYTES downloaded */
	UInt32         ulTotUpldBytes;     /* Total number of BYTES uploaded */
	int8_t         DeleteFlag;         /* 1=delete this record, 0=keep */
	Int32          RecNum;             /* Record Number in USERS.INF file */
	uint8_t        Flags;
	char           Reserved[8];        /* Bytes 390-397 from the USERS file */
	UInt32         MsgsRead;           /* Number of messages the user has read in PCB */
	UInt32         MsgsLeft;           /* Number of messages the user has left in PCB */
};

struct usersys {
	userSysHeader	hdr;
	userSysFixed	fixed;
};

#pragma pack(pop)		// original packing

} // namespace PCBoard

#endif // Don't add anything after this line
