/* Functions to parse ini (initialization / configuration) files */

/* $Id$ */
// vi: tabstop=4

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

#ifndef _INI_FILE_H
#define _INI_FILE_H

#include "genwrap.h"
#include "str_list.h"	/* strList_t */
#if !defined(NO_SOCKET_SUPPORT)
	#include "sockwrap.h"	/* inet_addr, SOCKET */
#endif

#define INI_MAX_VALUE_LEN	1024		/* Maximum value length, includes '\0' */
#define ROOT_SECTION		NULL

typedef struct {
	ulong		bit;
	const char*	name;
} ini_bitdesc_t;

typedef struct {
	int			key_len;
	const char* key_prefix;
	const char* section_separator;
	const char* value_separator;
	const char*	bit_separator;
	const char* literal_separator;
} ini_style_t;

#if defined(__cplusplus)
extern "C" {
#endif

/* Read all section names and return as an allocated string list */
/* Optionally (if prefix!=NULL), returns a subset of section names */
DLLEXPORT str_list_t DLLCALL	iniReadSectionList(FILE*, const char* prefix);
/* Returns number (count) of sections */
DLLEXPORT size_t DLLCALL		iniReadSectionCount(FILE*, const char* prefix);
/* Read all key names and return as an allocated string list */
DLLEXPORT str_list_t DLLCALL	iniReadKeyList(FILE*, const char* section);
/* Read all key and value pairs and return as a named string list */
DLLEXPORT named_string_t** DLLCALL
			iniReadNamedStringList(FILE*, const char* section);

/* Return the supported Log Levels in a string list - for *LogLevel macros */
DLLEXPORT str_list_t DLLCALL	iniLogLevelStringList(void);

/* These functions read a single key of the specified type */
DLLEXPORT char* DLLCALL		iniReadString(FILE*, const char* section, const char* key
					,const char* deflt, char* value);
/* If the key doesn't exist, iniReadExistingString just returns NULL */
DLLEXPORT char* DLLCALL		iniReadExistingString(FILE*, const char* section, const char* key
					,const char* deflt, char* value);
DLLEXPORT str_list_t DLLCALL	iniReadStringList(FILE*, const char* section, const char* key
					,const char* sep, const char* deflt);
DLLEXPORT long DLLCALL		iniReadInteger(FILE*, const char* section, const char* key
					,long deflt);
DLLEXPORT ushort DLLCALL		iniReadShortInt(FILE*, const char* section, const char* key
					,ushort deflt);
DLLEXPORT ulong DLLCALL		iniReadLongInt(FILE*, const char* section, const char* key
					,ulong deflt);
DLLEXPORT int64_t DLLCALL		iniReadBytes(FILE*, const char* section, const char* key
					,ulong unit, int64_t deflt);
DLLEXPORT double DLLCALL		iniReadDuration(FILE*, const char* section, const char* key
					,double deflt);
DLLEXPORT double DLLCALL		iniReadFloat(FILE*, const char* section, const char* key
					,double deflt);
DLLEXPORT BOOL DLLCALL		iniReadBool(FILE*, const char* section, const char* key
					,BOOL deflt);
DLLEXPORT time_t DLLCALL		iniReadDateTime(FILE*, const char* section, const char* key
					,time_t deflt);
DLLEXPORT unsigned DLLCALL	iniReadEnum(FILE*, const char* section, const char* key
					,str_list_t names, unsigned deflt);
DLLEXPORT unsigned* DLLCALL	iniReadEnumList(FILE*, const char* section, const char* key
					,str_list_t names, unsigned* count, const char* sep, const char* deflt);
DLLEXPORT long DLLCALL		iniReadNamedInt(FILE*, const char* section, const char* key
					,named_long_t*, long deflt);
DLLEXPORT ulong DLLCALL		iniReadNamedLongInt(FILE*, const char* section, const char* key
					,named_ulong_t*, ulong deflt);
DLLEXPORT double DLLCALL		iniReadNamedFloat(FILE*, const char* section, const char* key
					,named_double_t*, double deflt);
DLLEXPORT ulong DLLCALL		iniReadBitField(FILE*, const char* section, const char* key
					,ini_bitdesc_t* bitdesc, ulong deflt);
#define		iniReadLogLevel(f,s,k,d) iniReadEnum(f,s,k,iniLogLevelStringList(),d)

/* Free string list returned from iniRead*List functions */
DLLEXPORT void* DLLCALL		iniFreeStringList(str_list_t list);

/* Free named string list returned from iniReadNamedStringList */
DLLEXPORT void* DLLCALL		iniFreeNamedStringList(named_string_t** list);


/* File I/O Functions */
DLLEXPORT char* DLLCALL		iniFileName(char* dest, size_t maxlen, const char* dir, const char* fname);
DLLEXPORT FILE* DLLCALL		iniOpenFile(const char* fname, BOOL create);
DLLEXPORT str_list_t DLLCALL	iniReadFile(FILE*);
DLLEXPORT BOOL DLLCALL		iniWriteFile(FILE*, const str_list_t);
DLLEXPORT BOOL DLLCALL		iniCloseFile(FILE*);

/* StringList functions */
DLLEXPORT str_list_t DLLCALL	iniGetSectionList(str_list_t list, const char* prefix);
DLLEXPORT size_t DLLCALL		iniGetSectionCount(str_list_t list, const char* prefix);
DLLEXPORT str_list_t DLLCALL	iniGetKeyList(str_list_t list, const char* section);
DLLEXPORT named_string_t** DLLCALL
			iniGetNamedStringList(str_list_t list, const char* section);

DLLEXPORT char* DLLCALL		iniGetString(str_list_t, const char* section, const char* key
					,const char* deflt, char* value /* may be NULL */);
/* If the key doesn't exist, iniGetExistingString just returns NULL */
DLLEXPORT char* DLLCALL		iniGetExistingString(str_list_t, const char* section, const char* key
					,const char* deflt, char* value /* may be NULL */);
DLLEXPORT str_list_t DLLCALL	iniGetStringList(str_list_t, const char* section, const char* key
					,const char* sep, const char* deflt);
DLLEXPORT long DLLCALL		iniGetInteger(str_list_t, const char* section, const char* key
					,long deflt);
DLLEXPORT ushort DLLCALL		iniGetShortInt(str_list_t, const char* section, const char* key
					,ushort deflt);
DLLEXPORT ulong DLLCALL		iniGetLongInt(str_list_t, const char* section, const char* key
					,ulong deflt);
DLLEXPORT int64_t DLLCALL	iniGetBytes(str_list_t, const char* section, const char* key
					,ulong unit, int64_t deflt);
DLLEXPORT double DLLCALL	iniGetDuration(str_list_t, const char* section, const char* key
					,double deflt);
DLLEXPORT double DLLCALL	iniGetFloat(str_list_t, const char* section, const char* key
					,double deflt);
DLLEXPORT BOOL DLLCALL		iniGetBool(str_list_t, const char* section, const char* key
					,BOOL deflt);
DLLEXPORT time_t DLLCALL		iniGetDateTime(str_list_t, const char* section, const char* key
					,time_t deflt);
DLLEXPORT unsigned DLLCALL	iniGetEnum(str_list_t, const char* section, const char* key
					,str_list_t names, unsigned deflt);
DLLEXPORT unsigned* DLLCALL	iniGetEnumList(str_list_t, const char* section, const char* key
					,str_list_t names, unsigned* count, const char* sep, const char* deflt);
DLLEXPORT long DLLCALL		iniGetNamedInt(str_list_t, const char* section, const char* key
					,named_long_t*, long deflt);
DLLEXPORT ulong DLLCALL		iniGetNamedLongInt(str_list_t, const char* section, const char* key
					,named_ulong_t*, ulong deflt);
DLLEXPORT double DLLCALL		iniGetNamedFloat(str_list_t, const char* section, const char* key
					,named_double_t*, double deflt);
DLLEXPORT ulong DLLCALL		iniGetBitField(str_list_t, const char* section, const char* key
					,ini_bitdesc_t* bitdesc, ulong deflt);
DLLEXPORT str_list_t DLLCALL	iniGetSection(str_list_t, const char *section);
#define		iniGetLogLevel(l,s,k,d) iniGetEnum(l,s,k,iniLogLevelStringList(),d)

#if !defined(NO_SOCKET_SUPPORT)
DLLEXPORT ulong DLLCALL		iniReadIpAddress(FILE*, const char* section, const char* key
					,ulong deflt);
DLLEXPORT ulong DLLCALL		iniGetIpAddress(str_list_t, const char* section, const char* key
					,ulong deflt);
DLLEXPORT char* DLLCALL		iniSetIpAddress(str_list_t*, const char* section, const char* key, ulong value
					,ini_style_t*);
DLLEXPORT struct in6_addr DLLCALL	iniReadIp6Address(FILE*, const char* section, const char* key
					,struct in6_addr deflt);
DLLEXPORT struct in6_addr DLLCALL		iniGetIp6Address(str_list_t, const char* section, const char* key
					,struct in6_addr deflt);
DLLEXPORT char* DLLCALL		iniSetIp6Address(str_list_t*, const char* section, const char* key, struct in6_addr value
					,ini_style_t*);
DLLEXPORT int DLLCALL			iniGetSocketOptions(str_list_t, const char* section
					,SOCKET sock, char* error, size_t errlen);
#endif

DLLEXPORT void DLLCALL		iniSetDefaultStyle(ini_style_t);

DLLEXPORT char* DLLCALL		iniSetString(str_list_t*, const char* section, const char* key, const char* value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetStringLiteral(str_list_t*, const char* section, const char* key, const char* value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetInteger(str_list_t*, const char* section, const char* key, long value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetShortInt(str_list_t*, const char* section, const char* key, ushort value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetLongInt(str_list_t*, const char* section, const char* key, ulong value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetBytes(str_list_t*, const char* section, const char* key, ulong unit, int64_t value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetDuration(str_list_t*, const char* section, const char* key, double value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetHexInt(str_list_t*, const char* section, const char* key, ulong value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetFloat(str_list_t*, const char* section, const char* key, double value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetBool(str_list_t*, const char* section, const char* key, BOOL value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetDateTime(str_list_t*, const char* section, const char* key, BOOL include_time, time_t
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetEnum(str_list_t*, const char* section, const char* key, str_list_t names
					,unsigned value, ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetEnumList(str_list_t*, const char* section, const char* key 
					,const char* sep, str_list_t names, unsigned* values, unsigned count, ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetNamedInt(str_list_t*, const char* section, const char* key, named_long_t*
					,long value, ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetNamedHexInt(str_list_t*, const char* section, const char* key, named_ulong_t*
					,ulong value, ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetNamedLongInt(str_list_t*, const char* section, const char* key, named_ulong_t*
					,ulong value, ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetNamedFloat(str_list_t*, const char* section, const char* key, named_double_t*
					,double value, ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetBitField(str_list_t*, const char* section, const char* key, ini_bitdesc_t*, ulong value
					,ini_style_t*);
DLLEXPORT char* DLLCALL		iniSetStringList(str_list_t*, const char* section, const char* key
					,const char* sep, str_list_t value, ini_style_t*);
#define		iniSetLogLevel(l,s,k,v,style) iniSetEnum(l,s,k,iniLogLevelStringList(),v,style)

DLLEXPORT size_t DLLCALL		iniAddSection(str_list_t*, const char* section
					,ini_style_t*);

DLLEXPORT size_t DLLCALL		iniAppendSection(str_list_t*, const char* section
					,ini_style_t*);

DLLEXPORT BOOL DLLCALL		iniSectionExists(str_list_t, const char* section);
DLLEXPORT BOOL DLLCALL		iniKeyExists(str_list_t, const char* section, const char* key);
DLLEXPORT BOOL DLLCALL		iniValueExists(str_list_t, const char* section, const char* key);
DLLEXPORT char* DLLCALL		iniPopKey(str_list_t*, const char* section, const char* key, char* value);
DLLEXPORT BOOL DLLCALL		iniRemoveKey(str_list_t*, const char* section, const char* key);
DLLEXPORT BOOL DLLCALL		iniRemoveValue(str_list_t*, const char* section, const char* key);
DLLEXPORT BOOL DLLCALL		iniRemoveSection(str_list_t*, const char* section);
DLLEXPORT BOOL DLLCALL		iniRemoveSections(str_list_t*, const char* prefex);
DLLEXPORT BOOL DLLCALL		iniRenameSection(str_list_t*, const char* section, const char* newname);

/*
 * Too handy to leave internal
 */
DLLEXPORT unsigned* DLLCALL parseEnumList(const char* values, const char* sep, str_list_t names, unsigned* count);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
