/* Functions to parse ini (initialization / configuration) files */

/* $Id: ini_file.h,v 1.60 2020/04/03 18:41:45 rswindell Exp $ */
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

#if !defined(NO_SOCKET_SUPPORT)
	#include "sockwrap.h"	/* inet_addr, SOCKET */
#endif
#include "genwrap.h"
#include "str_list.h"	/* strList_t */

#define INI_MAX_VALUE_LEN	1024		/* Maximum value length, includes '\0' */
#define ROOT_SECTION		NULL

typedef struct {
	ulong		bit;
	const char*	name;
} ini_bitdesc_t;

typedef struct {
	int		key_len;
	char*	key_prefix;
	char*	section_separator;
	char*	value_separator;
	char*	bit_separator;
	char*	literal_separator;
} ini_style_t;

#if defined(__cplusplus)
extern "C" {
#endif

/* Read all section names and return as an allocated string list */
/* Optionally (if prefix!=NULL), returns a subset of section names */
DLLEXPORT str_list_t 	iniReadSectionList(FILE*, const char* prefix);
/* Returns number (count) of sections */
DLLEXPORT size_t 		iniReadSectionCount(FILE*, const char* prefix);
/* Read all key names and return as an allocated string list */
DLLEXPORT str_list_t 	iniReadKeyList(FILE*, const char* section);
/* Read all key and value pairs and return as a named string list */
DLLEXPORT named_string_t** 
						iniReadNamedStringList(FILE*, const char* section);

/* Return the supported Log Levels in a string list - for *LogLevel macros */
DLLEXPORT str_list_t 	iniLogLevelStringList(void);

/* Return the unparsed/converted value */
DLLEXPORT char* 		iniReadValue(FILE*, const char* section, const char* key
							,const char* deflt, char* value);
DLLEXPORT char* 		iniReadExistingValue(FILE*, const char* section, const char* key
							,const char* deflt, char* value);

/* These functions read a single key of the specified type */
DLLEXPORT char* 		iniReadString(FILE*, const char* section, const char* key
							,const char* deflt, char* value);
/* If the key doesn't exist, iniReadExistingString just returns NULL */
DLLEXPORT char* 		iniReadExistingString(FILE*, const char* section, const char* key
							,const char* deflt, char* value);
DLLEXPORT str_list_t 	iniReadStringList(FILE*, const char* section, const char* key
							,const char* sep, const char* deflt);
DLLEXPORT long 			iniReadInteger(FILE*, const char* section, const char* key
							,long deflt);
DLLEXPORT ushort 		iniReadShortInt(FILE*, const char* section, const char* key
							,ushort deflt);
DLLEXPORT ulong 		iniReadLongInt(FILE*, const char* section, const char* key
							,ulong deflt);
DLLEXPORT int64_t 		iniReadBytes(FILE*, const char* section, const char* key
							,ulong unit, int64_t deflt);
DLLEXPORT double 		iniReadDuration(FILE*, const char* section, const char* key
							,double deflt);
DLLEXPORT double 		iniReadFloat(FILE*, const char* section, const char* key
							,double deflt);
DLLEXPORT BOOL 			iniReadBool(FILE*, const char* section, const char* key
							,BOOL deflt);
DLLEXPORT time_t 		iniReadDateTime(FILE*, const char* section, const char* key
							,time_t deflt);
DLLEXPORT unsigned 		iniReadEnum(FILE*, const char* section, const char* key
							,str_list_t names, unsigned deflt);
DLLEXPORT unsigned* 	iniReadEnumList(FILE*, const char* section, const char* key
							,str_list_t names, unsigned* count, const char* sep, const char* deflt);
DLLEXPORT int* 			iniReadIntList(FILE*, const char* section, const char* key
							,unsigned* count, const char* sep, const char* deflt);
DLLEXPORT long 			iniReadNamedInt(FILE*, const char* section, const char* key
							,named_long_t*, long deflt);
DLLEXPORT ulong 		iniReadNamedLongInt(FILE*, const char* section, const char* key
							,named_ulong_t*, ulong deflt);
DLLEXPORT double 		iniReadNamedFloat(FILE*, const char* section, const char* key
							,named_double_t*, double deflt);
DLLEXPORT ulong 		iniReadBitField(FILE*, const char* section, const char* key
							,ini_bitdesc_t* bitdesc, ulong deflt);
#define		iniReadLogLevel(f,s,k,d) iniReadEnum(f,s,k,iniLogLevelStringList(),d)

/* Free string list returned from iniRead*List functions */
DLLEXPORT void* 		iniFreeStringList(str_list_t list);

/* Free named string list returned from iniReadNamedStringList */
DLLEXPORT void* 		iniFreeNamedStringList(named_string_t** list);


/* File I/O Functions */
DLLEXPORT char* 		iniFileName(char* dest, size_t maxlen, const char* dir, const char* fname);
DLLEXPORT FILE* 		iniOpenFile(const char* fname, BOOL create);
DLLEXPORT str_list_t 	iniReadFile(FILE*);
DLLEXPORT BOOL 			iniWriteFile(FILE*, const str_list_t);
DLLEXPORT BOOL 			iniCloseFile(FILE*);

/* StringList functions */
DLLEXPORT str_list_t 	iniGetSectionList(str_list_t list, const char* prefix);
DLLEXPORT size_t 		iniGetSectionCount(str_list_t list, const char* prefix);
DLLEXPORT str_list_t 	iniGetKeyList(str_list_t list, const char* section);
DLLEXPORT named_string_t** 
						iniGetNamedStringList(str_list_t list, const char* section);

/* Return the unparsed value (string literals not supported): */
DLLEXPORT char* 		iniGetValue(str_list_t, const char* section, const char* key
							,const char* deflt, char* value /* may be NULL */);
DLLEXPORT char* 		iniGetExistingValue(str_list_t, const char* section, const char* key
							,const char* deflt, char* value /* may be NULL */);

/* Return the string value (potentially string literals separated by colon rather than equal): */
DLLEXPORT char* 		iniGetString(str_list_t, const char* section, const char* key
							,const char* deflt, char* value /* may be NULL */);
/* If the key doesn't exist, iniGetExistingString just returns NULL */
DLLEXPORT char* 		iniGetExistingString(str_list_t, const char* section, const char* key
							,const char* deflt, char* value /* may be NULL */);
DLLEXPORT str_list_t 	iniGetStringList(str_list_t, const char* section, const char* key
							,const char* sep, const char* deflt);
DLLEXPORT long 			iniGetInteger(str_list_t, const char* section, const char* key
							,long deflt);
DLLEXPORT ushort 		iniGetShortInt(str_list_t, const char* section, const char* key
							,ushort deflt);
DLLEXPORT ulong 		iniGetLongInt(str_list_t, const char* section, const char* key
							,ulong deflt);
DLLEXPORT int64_t 		iniGetBytes(str_list_t, const char* section, const char* key
							,ulong unit, int64_t deflt);
DLLEXPORT double 		iniGetDuration(str_list_t, const char* section, const char* key
							,double deflt);
DLLEXPORT double 		iniGetFloat(str_list_t, const char* section, const char* key
							,double deflt);
DLLEXPORT BOOL 			iniGetBool(str_list_t, const char* section, const char* key
							,BOOL deflt);
DLLEXPORT time_t 		iniGetDateTime(str_list_t, const char* section, const char* key
							,time_t deflt);
DLLEXPORT unsigned 		iniGetEnum(str_list_t, const char* section, const char* key
							,str_list_t names, unsigned deflt);
DLLEXPORT unsigned* 	iniGetEnumList(str_list_t, const char* section, const char* key
							,str_list_t names, unsigned* count, const char* sep, const char* deflt);
DLLEXPORT int* 			iniGetIntList(str_list_t, const char* section, const char* key
							,unsigned* count, const char* sep, const char* deflt);
DLLEXPORT long 			iniGetNamedInt(str_list_t, const char* section, const char* key
							,named_long_t*, long deflt);
DLLEXPORT ulong 		iniGetNamedLongInt(str_list_t, const char* section, const char* key
							,named_ulong_t*, ulong deflt);
DLLEXPORT double 		iniGetNamedFloat(str_list_t, const char* section, const char* key
							,named_double_t*, double deflt);
DLLEXPORT ulong 		iniGetBitField(str_list_t, const char* section, const char* key
							,ini_bitdesc_t* bitdesc, ulong deflt);
DLLEXPORT str_list_t 	iniGetSection(str_list_t, const char *section);
#define		iniGetLogLevel(l,s,k,d) iniGetEnum(l,s,k,iniLogLevelStringList(),d)

#if !defined(NO_SOCKET_SUPPORT)
DLLEXPORT ulong 		iniReadIpAddress(FILE*, const char* section, const char* key
							,ulong deflt);
DLLEXPORT ulong 		iniGetIpAddress(str_list_t, const char* section, const char* key
							,ulong deflt);
DLLEXPORT char* 		iniSetIpAddress(str_list_t*, const char* section, const char* key, ulong value
							,ini_style_t*);
DLLEXPORT struct in6_addr
						iniReadIp6Address(FILE*, const char* section, const char* key
							,struct in6_addr deflt);
DLLEXPORT struct in6_addr
						iniGetIp6Address(str_list_t, const char* section, const char* key
							,struct in6_addr deflt);
DLLEXPORT char* 		iniSetIp6Address(str_list_t*, const char* section, const char* key, struct in6_addr value
							,ini_style_t*);
DLLEXPORT int 			iniGetSocketOptions(str_list_t, const char* section
							,SOCKET sock, char* error, size_t errlen);
#endif

DLLEXPORT void 			iniSetDefaultStyle(ini_style_t);

DLLEXPORT char* 		iniSetString(str_list_t*, const char* section, const char* key, const char* value
							,ini_style_t*);
DLLEXPORT char* 		iniSetStringLiteral(str_list_t*, const char* section, const char* key, const char* value
							,ini_style_t*);
DLLEXPORT char* 		iniSetValue(str_list_t*, const char* section, const char* key, const char* value
							,ini_style_t*);
DLLEXPORT char* 		iniSetInteger(str_list_t*, const char* section, const char* key, long value
							,ini_style_t*);
DLLEXPORT char* 		iniSetShortInt(str_list_t*, const char* section, const char* key, ushort value
							,ini_style_t*);
DLLEXPORT char* 		iniSetLongInt(str_list_t*, const char* section, const char* key, ulong value
							,ini_style_t*);
DLLEXPORT char* 		iniSetBytes(str_list_t*, const char* section, const char* key, ulong unit, int64_t value
							,ini_style_t*);
DLLEXPORT char* 		iniSetDuration(str_list_t*, const char* section, const char* key, double value
							,ini_style_t*);
DLLEXPORT char* 		iniSetHexInt(str_list_t*, const char* section, const char* key, ulong value
							,ini_style_t*);
DLLEXPORT char* 		iniSetFloat(str_list_t*, const char* section, const char* key, double value
							,ini_style_t*);
DLLEXPORT char* 		iniSetBool(str_list_t*, const char* section, const char* key, BOOL value
							,ini_style_t*);
DLLEXPORT char* 		iniSetDateTime(str_list_t*, const char* section, const char* key, BOOL include_time, time_t
							,ini_style_t*);
DLLEXPORT char* 		iniSetEnum(str_list_t*, const char* section, const char* key, str_list_t names
							,unsigned value, ini_style_t*);
DLLEXPORT char* 		iniSetEnumList(str_list_t*, const char* section, const char* key
							,const char* sep, str_list_t names, unsigned* values, unsigned count, ini_style_t*);
DLLEXPORT char* 		iniSetNamedInt(str_list_t*, const char* section, const char* key, named_long_t*
							,long value, ini_style_t*);
DLLEXPORT char* 		iniSetNamedHexInt(str_list_t*, const char* section, const char* key, named_ulong_t*
							,ulong value, ini_style_t*);
DLLEXPORT char* 		iniSetNamedLongInt(str_list_t*, const char* section, const char* key, named_ulong_t*
							,ulong value, ini_style_t*);
DLLEXPORT char* 		iniSetNamedFloat(str_list_t*, const char* section, const char* key, named_double_t*
							,double value, ini_style_t*);
DLLEXPORT char* 		iniSetBitField(str_list_t*, const char* section, const char* key, ini_bitdesc_t*, ulong value
							,ini_style_t*);
DLLEXPORT char* 		iniSetStringList(str_list_t*, const char* section, const char* key
							,const char* sep, str_list_t value, ini_style_t*);
#define		iniSetLogLevel(l,s,k,v,style) iniSetEnum(l,s,k,iniLogLevelStringList(),v,style)
DLLEXPORT char*			iniSetIntList(str_list_t*, const char* section, const char* key
							,const char* sep, int* value, unsigned count, ini_style_t*);

DLLEXPORT size_t 		iniAddSection(str_list_t*, const char* section
							,ini_style_t*);

DLLEXPORT size_t 		iniAppendSection(str_list_t*, const char* section
							,ini_style_t*);

DLLEXPORT size_t 		iniAppendSectionWithKeys(str_list_t*, const char* section, const str_list_t keys
							,ini_style_t*);

DLLEXPORT BOOL 			iniSectionExists(str_list_t, const char* section);
DLLEXPORT BOOL 			iniKeyExists(str_list_t, const char* section, const char* key);
DLLEXPORT BOOL 			iniValueExists(str_list_t, const char* section, const char* key);
DLLEXPORT char* 		iniPopKey(str_list_t*, const char* section, const char* key, char* value);
DLLEXPORT char* 		iniPopString(str_list_t*, const char* section, const char* key, char* value);
DLLEXPORT BOOL 			iniRemoveKey(str_list_t*, const char* section, const char* key);
DLLEXPORT BOOL 			iniRemoveValue(str_list_t*, const char* section, const char* key);
DLLEXPORT BOOL 			iniRemoveSection(str_list_t*, const char* section);
DLLEXPORT BOOL 			iniRemoveSections(str_list_t*, const char* prefix);
DLLEXPORT BOOL 			iniRenameSection(str_list_t*, const char* section, const char* newname);

DLLEXPORT BOOL 			iniHasInclude(const str_list_t);

/*
 * Too handy to leave internal
 */
DLLEXPORT unsigned*		parseEnumList(const char* values, const char* sep, str_list_t names, unsigned* count);
DLLEXPORT int*			parseIntList(const char* values, const char* sep, unsigned* count);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
