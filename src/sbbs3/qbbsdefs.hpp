/* QuickBBS Structure Definitions, Source: STRUCT.290 from QuickBBS v2.90 */

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

#ifndef QBBSDEFS_H_
#define QBBSDEFS_H_

#include "pascal_types.hpp"

namespace QBBS {

using Pascal::String;
using Pascal::Byte;
using Pascal::Word;
using Pascal::Boolean;
using Pascal::Integer;
using Pascal::LongInt;
using Pascal::LongWord;

static const int USER_NAME_LEN			= 35;

static const int USER_ATTRIB_DELETED	= (1<<0);
static const int USER_ATTRIB_CLRSCRN	= (1<<1);
static const int USER_ATTRIB_MORE		= (1<<2);
static const int USER_ATTRIB_ANSI		= (1<<3);
static const int USER_ATTRIB_NO_KILL	= (1<<4);
static const int USER_ATTRIB_IGN_DL_HRS	= (1<<5);
static const int USER_ATTRIB_ANSI_FSED	= (1<<6);
static const int USER_ATTRIB_FEMALE		= (1<<7);

static const int USER_ATTRIB2_GUEST		= (1<<0);
static const int USER_ATTRIB2_SSR		= (1<<1);
static const int USER_ATTRIB2_DIRTY		= (1<<7);

#pragma pack(push,1)	// Disk image structures must be packed

struct sysinfo {
	LongInt	CallCount;
	String<USER_NAME_LEN> LastCallerName; // Just "LastCaller" in QuickBBS v2.75
	String<USER_NAME_LEN> LastCallerAlias; // Not present in QuickBBS v2.75
	Byte ExtraSpace[92];
};

struct timelog {
	String<8> StartDate;	// MM-DD-YY
	Word BusyPerHour[24];
	Word BusyPerDay[7];
};

struct user {
	String<USER_NAME_LEN> Name;
	String<25> Location; // "City"
	// End of commonality with RemoteAccess
	Byte	Reserved; // Reserved, should always be 0 (used to be the password string length)
	Byte	Language;
	LongInt	PwdCrc;	// Was PascalString<15> Password in QuickBBS v2.75
	Word	PwdChangeData,
			ExpireData; // Number of days since 1/1/1900
	LongInt	HighMsgRead;
	Byte Attrib2;
	Byte ExtraSpace;
	String<12> DataPhone;
	String<12> HomePhone;
	String<5> LastTime;
	String<8> LastDate;
	Byte Attrib;
	LongWord Flags; // Byte[4]
	Word	Credit,
			Pending,
			TimesPosted,
			ObsoleteField, // Was HighMsgRead in QuickBBS v2.75
			SecLvl,
			Times,
			Ups,
			Downs,
			UpK,
			DownK,
			TodayK;
	Integer	Elapsed,
			ScreenLength; // "Len" in QuickBBS STRUCT.*, "ScreenLength" in RemoteAccess STUCT.* (at different offset)
	Word	CombinedPtr; // Record number in COMBINED.BBS, Note:  0 signifies no combined record assigned
	Word	AliasPtr; // Record number in ALIAS.BBS, Note:  0 signifies no alias record assigned
	LongInt	Birthday; // Number of days since 1/1/1600
};

struct exitinfo {
	Word		BaudRate;
	sysinfo		SysInfo;
	timelog		TimeLogInfo;
	user		UserInfo;
	Byte		EventInfo[19]; // *NO LONGER* used, It should be initialized to NULL
	Boolean		NetMailEntered;
	Boolean		EchoMailEntered;
	String<5>	LoginTime;
	String<8>	LoginDate;
	Integer		TimeLimit; // TmLimit
	LongInt		LoginSec;
	// End of commonality with RemoteAccess v2.52 EXITINFO.BBS definition (STRUCT.252)
	LongInt		Credit;
	Integer		UserRecNum;
	Word		ReadThru;
	Integer		PageTimes;
	Integer		DownLimit;
	Boolean		WantChat;
	Byte		GosubLevel;
	String<8>	GoSubDataType[20];
	String<8>	Menu;
	// End of QuickBBS v2.75 EXITINFO.BBS definition (STRUCT.275)
	Boolean		ScreenClear,
				MorePrompts,
				GraphicsMode,
				ExternEdit;
	Integer		ScreenLength;
	Boolean		MNP_Connect;
	String<48>	ChatReason;
	Boolean		ExternLogoff;
	Boolean		ANSI_Capable;
	// End of Synchronet v3.19b EXITINFO.BBS generation
	Byte		CurrentLanguage;
	Boolean		RIP_Active;
	Byte		ExtraSpace[199];
};

#pragma pack(pop)		// original packing

} // namespace QBBS

#endif // Don't add anything after this line
